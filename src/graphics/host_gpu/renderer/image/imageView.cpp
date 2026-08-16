#include "graphics/host_gpu/renderer/image/imageView.h"

#include "common/assert.h"
#include "common/logging/log.h"
#include "graphics/guest_gpu/gpu_format.h"
#include "graphics/host_gpu/graphicContext.h"
#include "graphics/host_gpu/renderer/image/image.h"

#include <algorithm>
#include <mutex>
#include <utility>
#include <vector>

namespace Libs::Graphics {

namespace {

[[nodiscard]] bool IsComponentSwizzle(vk::ComponentSwizzle swizzle) {
	switch (swizzle) {
		case vk::ComponentSwizzle::eIdentity:
		case vk::ComponentSwizzle::eZero:
		case vk::ComponentSwizzle::eOne:
		case vk::ComponentSwizzle::eR:
		case vk::ComponentSwizzle::eG:
		case vk::ComponentSwizzle::eB:
		case vk::ComponentSwizzle::eA: return true;
		default: return false;
	}
}

[[nodiscard]] bool IsCompatibleViewFormat(vk::Format image_format, vk::Format view_format) {
	return ImageViewOps::FormatsCompatible(image_format, view_format);
}

[[nodiscard]] bool IsDepthStencilFormatPair(vk::Format image_format, vk::Format view_format) {
	const auto image_transfer = DepthAspectTransferFormat(image_format);
	const auto view_transfer  = DepthAspectTransferFormat(view_format);
	if (image_transfer == vk::Format::eUndefined || view_transfer == vk::Format::eUndefined) {
		return false;
	}
	return image_transfer == view_transfer;
}

[[nodiscard]] bool IsStencilViewFormat(vk::Format format) {
	switch (format) {
		case vk::Format::eS8Uint:
		case vk::Format::eR8Uint:
		case vk::Format::eR8Unorm: return true;
		default: return false;
	}
}

[[nodiscard]] bool IsDepthViewFormat(vk::Format format) {
	switch (format) {
		case vk::Format::eD16Unorm:
		case vk::Format::eR16Unorm:
		case vk::Format::eD32Sfloat:
		case vk::Format::eR32Sfloat:
		case vk::Format::eR32Uint: return true;
		default: return false;
	}
}

// Checks whether the view_format is a valid sampled view format for the
// given depth image_format. This allows sampling depth textures (e.g.
// D32_SFLOAT) with a single-component format (e.g. R32_SFLOAT) in shaders,
// as permitted by the Vulkan specification.
[[nodiscard]] bool IsSampledDepthViewFormat(vk::Format image_format, vk::Format view_format) {
	return IsSupportedSampledDepthFormat(image_format, view_format);
}

[[nodiscard]] bool IsValidViewType(const VulkanImage& image, const ImageViewInfo& info) {
	switch (image.image_type) {
		case vk::ImageType::e1D:
			if (info.type != vk::ImageViewType::e1D && info.type != vk::ImageViewType::e1DArray) {
				return false;
			}
			return info.type != vk::ImageViewType::e1D || info.layer_count == 1;
		case vk::ImageType::e2D:
			switch (info.type) {
				case vk::ImageViewType::e2D: return info.layer_count == 1;
				case vk::ImageViewType::e2DArray: return true;
				case vk::ImageViewType::eCube:
					return static_cast<bool>(image.flags &
					                         vk::ImageCreateFlagBits::eCubeCompatible) &&
				       info.base_layer % 6 == 0 && info.layer_count == 6;
				case vk::ImageViewType::eCubeArray:
					return static_cast<bool>(image.flags &
					                         vk::ImageCreateFlagBits::eCubeCompatible) &&
				       info.base_layer % 6 == 0 && info.layer_count % 6 == 0;
				default: return false;
			}
		case vk::ImageType::e3D:
			switch (info.type) {
				case vk::ImageViewType::e3D: return info.base_layer == 0 && info.layer_count == 1;
				case vk::ImageViewType::e2D:
					return static_cast<bool>(image.flags &
					                         vk::ImageCreateFlagBits::e2DArrayCompatible) &&
				       info.level_count == 1 && info.layer_count == 1;
				case vk::ImageViewType::e2DArray:
					return static_cast<bool>(image.flags &
					                         vk::ImageCreateFlagBits::e2DArrayCompatible) &&
				       info.level_count == 1;
				default: return false;
			}
		default: return false;
	}
}

[[nodiscard]] bool IsValidAspect(const VulkanImage& image, vk::ImageAspectFlags aspect) {
	const auto depth_format = DepthAspectTransferFormat(image.format);
	if (depth_format == vk::Format::eUndefined) {
		return aspect == vk::ImageAspectFlagBits::eColor;
	}
	const auto supported = ImageViewOps::DepthAspectMask(image.format);
	return static_cast<bool>(aspect) && !(aspect & ~supported);
}

} // namespace

namespace ImageViewOps {

namespace {

enum CompatibilityClass : uint32_t {
	None    = 0,
	Bit8    = 1u << 0,
	Bit16   = 1u << 1,
	Bit24   = 1u << 2,
	Bit32   = 1u << 3,
	Bit48   = 1u << 4,
	Bit64   = 1u << 5,
	Bit96   = 1u << 6,
	Bit128  = 1u << 7,
	Bit192  = 1u << 8,
	Bit256  = 1u << 9,
	Bc1Rgb  = 1u << 10,
	Bc1Rgba = 1u << 11,
	Bc2     = 1u << 12,
	Bc3     = 1u << 13,
	Bc4     = 1u << 14,
	Bc5     = 1u << 15,
	Bc6h    = 1u << 16,
	Bc7     = 1u << 17,
	D16     = 1u << 18,
	D16S8   = 1u << 19,
	D24     = 1u << 20,
	D24S8   = 1u << 21,
	D32     = 1u << 22,
	D32S8   = 1u << 23,
	S8      = 1u << 24,
};

[[nodiscard]] uint32_t FormatClass(vk::Format format) noexcept {
	switch (format) {
		case vk::Format::eR4G4UnormPack8:
		case vk::Format::eR8Sint:
		case vk::Format::eR8Snorm:
		case vk::Format::eR8Srgb:
		case vk::Format::eR8Sscaled:
		case vk::Format::eR8Uint:
		case vk::Format::eR8Unorm:
		case vk::Format::eR8Uscaled: return Bit8;

		case vk::Format::eA1R5G5B5UnormPack16:
		case vk::Format::eA4B4G4R4UnormPack16:
		case vk::Format::eA4R4G4B4UnormPack16:
		case vk::Format::eB4G4R4A4UnormPack16:
		case vk::Format::eB5G5R5A1UnormPack16:
		case vk::Format::eB5G6R5UnormPack16:
		case vk::Format::eR10X6UnormPack16:
		case vk::Format::eR12X4UnormPack16:
		case vk::Format::eR16Sfloat:
		case vk::Format::eR16Sint:
		case vk::Format::eR16Snorm:
		case vk::Format::eR16Sscaled:
		case vk::Format::eR16Uint:
		case vk::Format::eR16Unorm:
		case vk::Format::eR16Uscaled:
		case vk::Format::eR4G4B4A4UnormPack16:
		case vk::Format::eR5G5B5A1UnormPack16:
		case vk::Format::eR5G6B5UnormPack16:
		case vk::Format::eR8G8Sint:
		case vk::Format::eR8G8Snorm:
		case vk::Format::eR8G8Srgb:
		case vk::Format::eR8G8Sscaled:
		case vk::Format::eR8G8Uint:
		case vk::Format::eR8G8Unorm:
		case vk::Format::eR8G8Uscaled: return Bit16;

		case vk::Format::eB8G8R8Sint:
		case vk::Format::eB8G8R8Snorm:
		case vk::Format::eB8G8R8Srgb:
		case vk::Format::eB8G8R8Sscaled:
		case vk::Format::eB8G8R8Uint:
		case vk::Format::eB8G8R8Unorm:
		case vk::Format::eB8G8R8Uscaled:
		case vk::Format::eR8G8B8Sint:
		case vk::Format::eR8G8B8Snorm:
		case vk::Format::eR8G8B8Srgb:
		case vk::Format::eR8G8B8Sscaled:
		case vk::Format::eR8G8B8Uint:
		case vk::Format::eR8G8B8Unorm:
		case vk::Format::eR8G8B8Uscaled: return Bit24;

		case vk::Format::eA2B10G10R10SintPack32:
		case vk::Format::eA2B10G10R10SnormPack32:
		case vk::Format::eA2B10G10R10SscaledPack32:
		case vk::Format::eA2B10G10R10UintPack32:
		case vk::Format::eA2B10G10R10UnormPack32:
		case vk::Format::eA2B10G10R10UscaledPack32:
		case vk::Format::eA2R10G10B10SintPack32:
		case vk::Format::eA2R10G10B10SnormPack32:
		case vk::Format::eA2R10G10B10SscaledPack32:
		case vk::Format::eA2R10G10B10UintPack32:
		case vk::Format::eA2R10G10B10UnormPack32:
		case vk::Format::eA2R10G10B10UscaledPack32:
		case vk::Format::eA8B8G8R8SintPack32:
		case vk::Format::eA8B8G8R8SnormPack32:
		case vk::Format::eA8B8G8R8SrgbPack32:
		case vk::Format::eA8B8G8R8SscaledPack32:
		case vk::Format::eA8B8G8R8UintPack32:
		case vk::Format::eA8B8G8R8UnormPack32:
		case vk::Format::eA8B8G8R8UscaledPack32:
		case vk::Format::eB10G11R11UfloatPack32:
		case vk::Format::eB8G8R8A8Sint:
		case vk::Format::eB8G8R8A8Snorm:
		case vk::Format::eB8G8R8A8Srgb:
		case vk::Format::eB8G8R8A8Sscaled:
		case vk::Format::eB8G8R8A8Uint:
		case vk::Format::eB8G8R8A8Unorm:
		case vk::Format::eB8G8R8A8Uscaled:
		case vk::Format::eE5B9G9R9UfloatPack32:
		case vk::Format::eR10X6G10X6Unorm2Pack16:
		case vk::Format::eR12X4G12X4Unorm2Pack16:
		case vk::Format::eR16G16Sfloat:
		case vk::Format::eR16G16Sint:
		case vk::Format::eR16G16Snorm:
		case vk::Format::eR16G16Sscaled:
		case vk::Format::eR16G16Uint:
		case vk::Format::eR16G16Unorm:
		case vk::Format::eR16G16Uscaled:
		case vk::Format::eR32Sfloat:
		case vk::Format::eR32Sint:
		case vk::Format::eR32Uint:
		case vk::Format::eR8G8B8A8Sint:
		case vk::Format::eR8G8B8A8Snorm:
		case vk::Format::eR8G8B8A8Srgb:
		case vk::Format::eR8G8B8A8Sscaled:
		case vk::Format::eR8G8B8A8Uint:
		case vk::Format::eR8G8B8A8Unorm:
		case vk::Format::eR8G8B8A8Uscaled: return Bit32;

		case vk::Format::eR16G16B16Sfloat:
		case vk::Format::eR16G16B16Sint:
		case vk::Format::eR16G16B16Snorm:
		case vk::Format::eR16G16B16Sscaled:
		case vk::Format::eR16G16B16Uint:
		case vk::Format::eR16G16B16Unorm:
		case vk::Format::eR16G16B16Uscaled: return Bit48;

		case vk::Format::eR16G16B16A16Sfloat:
		case vk::Format::eR16G16B16A16Sint:
		case vk::Format::eR16G16B16A16Snorm:
		case vk::Format::eR16G16B16A16Sscaled:
		case vk::Format::eR16G16B16A16Uint:
		case vk::Format::eR16G16B16A16Unorm:
		case vk::Format::eR16G16B16A16Uscaled:
		case vk::Format::eR32G32Sfloat:
		case vk::Format::eR32G32Sint:
		case vk::Format::eR32G32Uint:
		case vk::Format::eR64Sfloat:
		case vk::Format::eR64Sint:
		case vk::Format::eR64Uint: return Bit64;

		case vk::Format::eR32G32B32Sfloat:
		case vk::Format::eR32G32B32Sint:
		case vk::Format::eR32G32B32Uint: return Bit96;

		case vk::Format::eR32G32B32A32Sfloat:
		case vk::Format::eR32G32B32A32Sint:
		case vk::Format::eR32G32B32A32Uint:
		case vk::Format::eR64G64Sfloat:
		case vk::Format::eR64G64Sint:
		case vk::Format::eR64G64Uint: return Bit128;

		case vk::Format::eR64G64B64Sfloat:
		case vk::Format::eR64G64B64Sint:
		case vk::Format::eR64G64B64Uint: return Bit192;

		case vk::Format::eR64G64B64A64Sfloat:
		case vk::Format::eR64G64B64A64Sint:
		case vk::Format::eR64G64B64A64Uint: return Bit256;

		case vk::Format::eBc1RgbSrgbBlock:
		case vk::Format::eBc1RgbUnormBlock: return Bc1Rgb | Bit64;
		case vk::Format::eBc1RgbaSrgbBlock:
		case vk::Format::eBc1RgbaUnormBlock: return Bc1Rgba | Bit64;
		case vk::Format::eBc2SrgbBlock:
		case vk::Format::eBc2UnormBlock: return Bc2 | Bit128;
		case vk::Format::eBc3SrgbBlock:
		case vk::Format::eBc3UnormBlock: return Bc3 | Bit128;
		case vk::Format::eBc4SnormBlock:
		case vk::Format::eBc4UnormBlock: return Bc4 | Bit64;
		case vk::Format::eBc5SnormBlock:
		case vk::Format::eBc5UnormBlock: return Bc5 | Bit128;
		case vk::Format::eBc6HSfloatBlock:
		case vk::Format::eBc6HUfloatBlock: return Bc6h | Bit128;
		case vk::Format::eBc7SrgbBlock:
		case vk::Format::eBc7UnormBlock: return Bc7 | Bit128;

		case vk::Format::eD16Unorm: return D16;
		case vk::Format::eD16UnormS8Uint: return D16S8;
		case vk::Format::eX8D24UnormPack32: return D24;
		case vk::Format::eD24UnormS8Uint: return D24S8;
		case vk::Format::eD32Sfloat: return D32;
		case vk::Format::eD32SfloatS8Uint: return D32S8;
		case vk::Format::eS8Uint: return S8;
		default: return None;
	}
}

} // namespace

vk::ImageAspectFlags DepthAspectMask(vk::Format format) {
	switch (format) {
		case vk::Format::eD16Unorm:
		case vk::Format::eD32Sfloat: return vk::ImageAspectFlagBits::eDepth;
		case vk::Format::eD16UnormS8Uint:
		case vk::Format::eD24UnormS8Uint:
		case vk::Format::eD32SfloatS8Uint:
			return vk::ImageAspectFlagBits::eDepth | vk::ImageAspectFlagBits::eStencil;
		default: EXIT("unsupported depth/stencil image format: %d\n", static_cast<int>(format));
	}
}

// Returns the list of formats that are mutually compatible with the given format
// in the same Vulkan format compatibility class. This is used to populate
// VkImageFormatListCreateInfo when creating mutable images, enabling format
// reinterpretation between compatible depth and depth+stencil variants
// (e.g. D32_SFLOAT_S8_UINT <-> D32_SFLOAT, D24_UNORM_S8_UINT <-> X8_D24_UNORM_PACK32).
// Formats with different byte sizes (e.g. D32_SFLOAT which is 4 bytes vs
// D32_SFLOAT_S8_UINT which is 5 bytes) are filtered out to avoid driver errors
// when creating images with VkImageFormatListCreateInfo.
[[nodiscard]] std::vector<vk::Format> CompatibleFormats(vk::Format format) {
	const auto target_bytes = Prospero::VulkanBytesPerElement(format);
	std::vector<vk::Format> result {format};
	switch (format) {
		case vk::Format::eD32Sfloat:
		case vk::Format::eD32SfloatS8Uint:
			result.push_back(vk::Format::eD32Sfloat);
			result.push_back(vk::Format::eD32SfloatS8Uint);
			break;
		case vk::Format::eD24UnormS8Uint:
		case vk::Format::eX8D24UnormPack32:
			result.push_back(vk::Format::eD24UnormS8Uint);
			result.push_back(vk::Format::eX8D24UnormPack32);
			break;
		case vk::Format::eD16Unorm:
		case vk::Format::eD16UnormS8Uint:
			result.push_back(vk::Format::eD16Unorm);
			result.push_back(vk::Format::eD16UnormS8Uint);
			break;
		default: break;
	}
	// Filter out duplicate entries and formats with different byte sizes from
	// the primary format. This prevents the Vulkan driver from rejecting
	// vkCreateImage when the format list contains incompatible sizes.
	if (target_bytes != 0) {
		std::vector<vk::Format> filtered;
		filtered.reserve(result.size());
		for (const auto& f : result) {
			if (f == format || f == result.front()) {
				filtered.push_back(f);
				continue;
			}
			const auto f_bytes = Prospero::VulkanBytesPerElement(f);
			if (f_bytes == 0 || f_bytes == target_bytes) {
				filtered.push_back(f);
			}
		}
		result = std::move(filtered);
	}
	return result;
}

bool FormatsCompatible(vk::Format base, vk::Format view) noexcept {
	if (base == view) {
		return true;
	}
	// Format compatibility is determined by the generic FormatClass-based check below.
	// Formats with the same compatibility class (e.g. R16G16B16A16_UINT and
	// R16G16B16A16_SFLOAT both belong to Bit64) are considered compatible,
	// enabling image view creation across formats in the same class.
	const auto base_class = FormatClass(base);
	const auto view_class = FormatClass(view);
	if (view_class != None && (base_class & view_class) == view_class) {
		return true;
	}
	// Depth/stencil cross-class compatibility: per the Vulkan specification,
	// D32_SFLOAT and D32_SFLOAT_S8_UINT belong to one compatibility class, and
	// D24_UNORM_S8_UINT and X8_D24_UNORM_PACK32 belong to another. When the
	// image was created with VK_IMAGE_CREATE_MUTABLE_FORMAT_BIT and the format
	// list was provided via VkImageFormatListCreateInfo, these are considered
	// compatible for view creation through the IsDepthStencilFormatPair path.
	return IsDepthStencilFormatPair(base, view);
}

} // namespace ImageViewOps

// Normalizes the view type to match the image type when there is a mismatch.
// This can happen when the texture cache reuses a 1x1x1 image of a different
// type (e.g., a 2D image is reused for a 1D texture view request). Vulkan does
// not allow creating 1D views of 2D images or vice versa, so we adapt the view
// type to match the image's actual type.
[[nodiscard]] vk::ImageViewType NormalizeViewType(const VulkanImage& image,
                                                  vk::ImageViewType requested_type,
                                                  uint32_t layer_count) {
	// For 1D/2D images with a single layer, a 1D or 2D view type is valid.
	// When the image is 1D and a 2D view is requested (or vice versa) for a
	// single-layer image, adapt the view type to match the image type.
	if (image.image_type == vk::ImageType::e1D && layer_count == 1 && image.layers == 1) {
		if (requested_type == vk::ImageViewType::e2D ||
		    requested_type == vk::ImageViewType::e2DArray) {
			return vk::ImageViewType::e1D;
		}
	}
	if (image.image_type == vk::ImageType::e2D && layer_count == 1 && image.layers == 1) {
		if (requested_type == vk::ImageViewType::e1D ||
		    requested_type == vk::ImageViewType::e1DArray) {
			return vk::ImageViewType::e2D;
		}
	}
	return requested_type;
}

vk::ImageView Image::FindView(const ImageViewInfo& view_info) {
	const auto& image      = backing;
	auto        normalized = view_info;
	const bool  is_storage = static_cast<bool>(normalized.usage & vk::ImageUsageFlagBits::eStorage);
	normalized.aspect      = FullAspectMask(image.format);
	if (normalized.aspect & vk::ImageAspectFlagBits::eDepth &&
	    IsDepthViewFormat(normalized.format)) {
		normalized.format = image.format;
		normalized.aspect = vk::ImageAspectFlagBits::eDepth;
	}
	if (normalized.aspect & vk::ImageAspectFlagBits::eStencil &&
	    IsStencilViewFormat(normalized.format)) {
		normalized.format = image.format;
		normalized.aspect = vk::ImageAspectFlagBits::eStencil;
	}
	// Ensure the image usage flags are properly propagated. The image may have
	// been created with usage=0x0 (defaulting to sampled) or usage=0x4
	// (VK_IMAGE_USAGE_SAMPLED_BIT). We must preserve the parent image's usage
	// flags and only override with eStorage when the caller explicitly requests
	// a storage view, ensuring VK_IMAGE_USAGE_SAMPLED_BIT is always available
	// for sampled view creation on mutable images.
	normalized.usage = is_storage ? vk::ImageUsageFlagBits::eStorage : vk::ImageUsageFlagBits::eSampled;
	if (DepthAspectTransferFormat(image.format) != vk::Format::eUndefined &&
	    DepthAspectTransferFormat(normalized.format) != vk::Format::eUndefined) {
		normalized.format = image.format;
	}
	// Handle the case where a depth image is being viewed with its corresponding
	// sampled view format (e.g. D32_SFLOAT image viewed as R32_SFLOAT for shader
	// sampling). This is a valid Vulkan operation: the depth aspect is selected
	// and the view format is the single-component format. The IsSampledDepthViewFormat
	// check covers this case, and we set the aspect to eDepth (not eColor) so the
	// view correctly targets the depth data.
	if (DepthAspectTransferFormat(image.format) != vk::Format::eUndefined &&
	    IsSampledDepthViewFormat(image.format, normalized.format)) {
		normalized.aspect = vk::ImageAspectFlagBits::eDepth;
	}
	// Normalize the view type to match the image type when there is a mismatch.
	// This happens when the texture cache reuses a 1x1x1 image of a different
	// type (e.g. a 2D image is reused for a 1D texture view request via
	// SameBacking's type-reuse exception for 1x1x1 extents).
	normalized.type = NormalizeViewType(image, normalized.type, normalized.layer_count);

	// Check whether the requested view format is compatible with the image's
	// format. Formats are compatible when they belong to the same Vulkan
	// compatibility class (i.e. have the same bytes-per-element / block size)
	// and the image was created with VK_IMAGE_CREATE_MUTABLE_FORMAT_BIT. As a
	// safety net, we also reject format reinterpretations where the bytes-per-
	// element of the view format differs from the image format, because the
	// driver will refuse such views on the image's compatible format list.
	// Note: We compare the Vulkan format byte sizes using VulkanBytesPerElement,
	// since ImageViewInfo does not carry the guest format. The image's guest
	// format bytes are compared against the view format's Vulkan bytes using
	// VulkanBytesPerElement, which is the authoritative size for view creation.
	const bool same_bytes_per_element =
	    Prospero::VulkanBytesPerElement(normalized.format) == 0 ||
	    Prospero::VulkanBytesPerElement(normalized.format) ==
	        Prospero::VulkanBytesPerElement(image.format);
	const bool format_compatible = normalized.format != vk::Format::eUndefined &&
	                               same_bytes_per_element &&
	                               (IsCompatibleViewFormat(image.format, normalized.format) ||
	                                IsDepthStencilFormatPair(image.format, normalized.format) ||
	                                IsSampledDepthViewFormat(image.format, normalized.format));

	// Safety fallback: if the view format is not compatible (e.g. mismatched
	// bytes-per-element between image and view formats — this happens with
	// guest format 37 -> R32G32_SFLOAT (8 bytes) being viewed as format 97 ->
	// R16G16_SFLOAT (4 bytes)), fall back to using the image's original format
	// rather than crashing. This ensures the view is always creatable.
	const vk::Format original_view_format = normalized.format;
	if (!format_compatible) {
		LOGF_COLOR(Log::Color::Yellow,
		           "FindView: view_format=%d incompatible with image_format=%d, falling back "
		           "to image format\n",
		           static_cast<int>(original_view_format), static_cast<int>(image.format));
		normalized.format = image.format;
		normalized.aspect = FullAspectMask(image.format);
	}
	const bool slice_view =
	    image.image_type == vk::ImageType::e3D && (normalized.type == vk::ImageViewType::e2D ||
	                                               normalized.type == vk::ImageViewType::e2DArray);
	const bool levels_valid = normalized.level_count != 0 &&
	                          normalized.base_level < image.mip_levels &&
	                          normalized.level_count <= image.mip_levels - normalized.base_level;
	const auto view_layers  = slice_view && levels_valid
	                              ? std::max(image.extent.depth >> normalized.base_level, 1u)
	                              : image.layers;
	const bool ranges_valid = levels_valid && normalized.layer_count != 0 &&
	                          normalized.base_layer < view_layers &&
	                          normalized.layer_count <= view_layers - normalized.base_layer;
	const bool mapping_valid =
	    IsComponentSwizzle(normalized.mapping.r) && IsComponentSwizzle(normalized.mapping.g) &&
	    IsComponentSwizzle(normalized.mapping.b) && IsComponentSwizzle(normalized.mapping.a);
	if (image.image == nullptr || !ranges_valid || !mapping_valid ||
	    !IsValidViewType(image, normalized) || !IsValidAspect(image, normalized.aspect)) {
		EXIT("invalid image view: image_format=%d view_format=%d type=%d aspect=0x%x "
		     "mip=%u+%u layer=%u+%u usage=0x%x image_levels=%u image_layers=%u\n",
		     static_cast<int>(image.format), static_cast<int>(normalized.format),
		     static_cast<int>(normalized.type),
		     static_cast<vk::ImageAspectFlags::MaskType>(normalized.aspect), normalized.base_level,
		     normalized.level_count, normalized.base_layer, normalized.layer_count,
		     static_cast<vk::ImageUsageFlags::MaskType>(normalized.usage), image.mip_levels,
		     image.layers);
	}

	std::lock_guard lock(views.mutex);
	for (const auto& cached: views.views) {
		if (cached.info == normalized) {
			return cached.view;
		}
	}

	// Propagate usage flags from the parent VkImage to the VkImageView.
	// The image's usage flags are set during allocation in Image::Image().
	// We use VkImageViewUsageCreateInfo to restrict the view's usage to a
	// subset of the image's usage, ensuring that flags like
	// VK_IMAGE_USAGE_SAMPLED_BIT (usage=0x4) are correctly carried through
	// even when the view format differs from the image format.
	vk::ImageViewUsageCreateInfo view_usage {};
	view_usage.sType = vk::StructureType::eImageViewUsageCreateInfo;
	view_usage.usage = image.usage;
	// If the image was created with no explicit usage flags (usage=0x0),
	// fall back to a sensible default set based on the image format.
	if (view_usage.usage == vk::ImageUsageFlags {}) {
		view_usage.usage = vk::ImageUsageFlagBits::eSampled | vk::ImageUsageFlagBits::eTransferSrc |
		                   vk::ImageUsageFlagBits::eTransferDst;
		if (DepthAspectTransferFormat(image.format) != vk::Format::eUndefined) {
			view_usage.usage |= vk::ImageUsageFlagBits::eDepthStencilAttachment;
		} else {
			view_usage.usage |= vk::ImageUsageFlagBits::eColorAttachment;
		}
	}
	// Strip storage usage if this is not a storage view, but always preserve
	// at least sampled usage for non-storage views of mutable images.
	if (!is_storage) {
		view_usage.usage &= ~vk::ImageUsageFlagBits::eStorage;
		view_usage.usage |= vk::ImageUsageFlagBits::eSampled;
	}

	vk::ImageViewCreateInfo create {};
	create.sType                           = vk::StructureType::eImageViewCreateInfo;
	create.pNext                           = &view_usage;
	create.image                           = image.image;
	create.viewType                        = normalized.type;
	create.format                          = normalized.format;
	create.components                      = normalized.mapping;
	create.subresourceRange.aspectMask     = normalized.aspect;
	create.subresourceRange.baseMipLevel   = normalized.base_level;
	create.subresourceRange.levelCount     = normalized.level_count;
	create.subresourceRange.baseArrayLayer = normalized.base_layer;
	create.subresourceRange.layerCount     = normalized.layer_count;

	vk::ImageView view   = nullptr;
	const auto    result = m_graphics->device.createImageView(&create, nullptr, &view);
	if (result != vk::Result::eSuccess || view == nullptr) {
		EXIT("failed to create image view: result=%d image_format=%d view_format=%d type=%d "
		     "aspect=0x%x mip=%u+%u layer=%u+%u usage=0x%x\n",
		     static_cast<int>(result), static_cast<int>(image.format),
		     static_cast<int>(view_info.format), static_cast<int>(view_info.type),
		     static_cast<vk::ImageAspectFlags::MaskType>(view_info.aspect), view_info.base_level,
		     view_info.level_count, view_info.base_layer, view_info.layer_count,
		     static_cast<vk::ImageUsageFlags::MaskType>(view_info.usage));
	}
	views.views.push_back({normalized, view});
	return view;
}

} // namespace Libs::Graphics
