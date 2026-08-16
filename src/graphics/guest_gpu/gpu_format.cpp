#include "graphics/guest_gpu/gpu_format.h"

#include "graphics/guest_gpu/gpu_defs.h"

#include <array>

namespace Libs::Graphics::Prospero {
namespace {

struct FormatInfo {
	uint32_t format;
	uint32_t bytes_per_element;
	uint32_t block_compressed_bytes_per_block;
	uint32_t render_target_bytes_per_element;
	bool     supported_sampled_texture_format;
	bool     unsigned_integer_texture_format;
};

constexpr FormatInfo kFormatInfo[] = {
    {GpuEnumValue(BufferFormat::k8UNorm), 1, 0, 1, true, false},
    {GpuEnumValue(BufferFormat::k8SNorm), 0, 0, 1, false, false},
    {GpuEnumValue(BufferFormat::k8UInt), 1, 0, 1, true, true},
    {GpuEnumValue(BufferFormat::k16UNorm), 2, 0, 2, true, false},
    {GpuEnumValue(BufferFormat::k16SNorm), 2, 0, 2, true, false},
    {GpuEnumValue(BufferFormat::k16UInt), 2, 0, 2, true, true},
    {GpuEnumValue(BufferFormat::k16SInt), 2, 0, 2, false, false},
    {GpuEnumValue(BufferFormat::k16Float), 2, 0, 2, true, false},
    {GpuEnumValue(BufferFormat::k8_8UNorm), 2, 0, 2, true, false},
    {GpuEnumValue(BufferFormat::k8_8SNorm), 2, 0, 2, true, false},
    {GpuEnumValue(BufferFormat::k8_8UInt), 2, 0, 2, true, true},
    {GpuEnumValue(BufferFormat::k8_8SInt), 2, 0, 2, false, false},
    {GpuEnumValue(BufferFormat::k32UInt), 4, 0, 4, true, true},
    {GpuEnumValue(BufferFormat::k32SInt), 4, 0, 4, false, false},
    {GpuEnumValue(BufferFormat::k32Float), 4, 0, 4, true, false},
    {GpuEnumValue(BufferFormat::k16_16UNorm), 4, 0, 4, true, false},
    {GpuEnumValue(BufferFormat::k16_16SNorm), 4, 0, 4, true, false},
    {GpuEnumValue(BufferFormat::k16_16UInt), 4, 0, 4, true, true},
    {GpuEnumValue(BufferFormat::k16_16SInt), 4, 0, 4, false, false},
    {GpuEnumValue(BufferFormat::k16_16Float), 4, 0, 4, true, false},
    {GpuEnumValue(BufferFormat::k11_11_10Float), 4, 0, 4, true, false},
    {GpuEnumValue(BufferFormat::k10_10_10_2UNorm), 4, 0, 4, true, false},
    {GpuEnumValue(BufferFormat::k10_10_10_2UInt), 4, 0, 4, true, true},
    {GpuEnumValue(BufferFormat::k8_8_8_8UNorm), 4, 0, 4, true, false},
    {GpuEnumValue(BufferFormat::k8_8_8_8SNorm), 4, 0, 4, true, false},
    {GpuEnumValue(BufferFormat::k8_8_8_8UInt), 4, 0, 4, true, true},
    {GpuEnumValue(BufferFormat::k8_8_8_8SInt), 4, 0, 4, false, false},
    {GpuEnumValue(BufferFormat::k32_32UInt), 8, 0, 8, true, true},
    {GpuEnumValue(BufferFormat::k32_32SInt), 8, 0, 8, false, false},
    {GpuEnumValue(BufferFormat::k32_32Float), 8, 0, 8, true, false},
    {GpuEnumValue(BufferFormat::k16_16_16_16UNorm), 8, 0, 8, true, false},
    {GpuEnumValue(BufferFormat::k16_16_16_16SNorm), 8, 0, 8, true, false},
    {GpuEnumValue(BufferFormat::k16_16_16_16UInt), 8, 0, 8, true, true},
    {GpuEnumValue(BufferFormat::k16_16_16_16SInt), 8, 0, 8, false, false},
    {GpuEnumValue(BufferFormat::k16_16_16_16Float), 8, 0, 8, true, false},
    {GpuEnumValue(BufferFormat::k32_32_32UInt), 12, 0, 12, true, true},
    {GpuEnumValue(BufferFormat::k32_32_32SInt), 12, 0, 12, false, false},
    {GpuEnumValue(BufferFormat::k32_32_32Float), 12, 0, 12, true, false},
    {GpuEnumValue(BufferFormat::k32_32_32_32UInt), 16, 0, 16, true, true},
    {GpuEnumValue(BufferFormat::k32_32_32_32SInt), 16, 0, 16, false, false},
    {GpuEnumValue(BufferFormat::k32_32_32_32Float), 16, 0, 16, true, false},
    {GpuEnumValue(BufferFormat::k8Srgb), 1, 0, 0, true, false},
    {GpuEnumValue(BufferFormat::k8_8Srgb), 2, 0, 0, true, false},
    {GpuEnumValue(BufferFormat::k8_8_8_8Srgb), 4, 0, 4, true, false},
    {GpuEnumValue(BufferFormat::k9_9_9_5Float), 4, 0, 0, true, false},
    {GpuEnumValue(BufferFormat::k5_6_5UNorm), 2, 0, 2, true, false},
    {GpuEnumValue(BufferFormat::k5_5_5_1UNorm), 2, 0, 2, true, false},
    {GpuEnumValue(BufferFormat::k4_4_4_4UNorm), 2, 0, 2, true, false},
    {GpuEnumValue(BufferFormat::kFmask8_S4_F4), 1, 0, 1, true, false},
    {GpuEnumValue(BufferFormat::kBc1UNorm), 0, 8, 0, true, false},
    {GpuEnumValue(BufferFormat::kBc1Srgb), 0, 8, 0, true, false},
    {GpuEnumValue(BufferFormat::kBc2UNorm), 0, 16, 0, true, false},
    {GpuEnumValue(BufferFormat::kBc2Srgb), 0, 16, 0, true, false},
    {GpuEnumValue(BufferFormat::kBc3UNorm), 0, 16, 0, true, false},
    {GpuEnumValue(BufferFormat::kBc3Srgb), 0, 16, 0, true, false},
    {GpuEnumValue(BufferFormat::kBc4UNorm), 0, 8, 0, true, false},
    {GpuEnumValue(BufferFormat::kBc4SNorm), 0, 8, 0, true, false},
    {GpuEnumValue(BufferFormat::kBc5UNorm), 0, 16, 0, true, false},
    {GpuEnumValue(BufferFormat::kBc5SNorm), 0, 16, 0, true, false},
    {GpuEnumValue(BufferFormat::kBc6UFloat), 0, 16, 0, true, false},
    {GpuEnumValue(BufferFormat::kBc6SFloat), 0, 16, 0, true, false},
    {GpuEnumValue(BufferFormat::kBc7UNorm), 0, 16, 0, true, false},
    {GpuEnumValue(BufferFormat::kBc7Srgb), 0, 16, 0, true, false},
    {97, 4, 0, 4, true, false},
    {37, 8, 0, 4, true, false},
};

constexpr auto MakeFormatInfoLookup() {
	constexpr uint32_t kMaxFormat = std::max(GpuEnumValue(BufferFormat::kBc7Srgb), 97u);
	std::array<const FormatInfo*, kMaxFormat + 1> lookup {};
	for (const auto& info: kFormatInfo) {
		lookup[info.format] = &info;
	}
	return lookup;
}

constexpr auto kFormatInfoLookup = MakeFormatInfoLookup();

const FormatInfo* FindFormatInfo(uint32_t format) {
	return format < kFormatInfoLookup.size() ? kFormatInfoLookup[format] : nullptr;
}

} // namespace

uint32_t NumBytesPerElement(uint32_t format) {
	const auto* info = FindFormatInfo(format);
	return info != nullptr ? info->bytes_per_element : 0;
}

uint32_t BlockCompressedBytesPerBlock(uint32_t format) {
	const auto* info = FindFormatInfo(format);
	return info != nullptr ? info->block_compressed_bytes_per_block : 0;
}

uint32_t RenderTargetBytesPerElement(uint32_t format) {
	const auto* info = FindFormatInfo(format);
	return info != nullptr ? info->render_target_bytes_per_element : 0;
}

bool IsSupportedTextureFormat(uint32_t format) {
	const auto* info = FindFormatInfo(format);
	return info != nullptr && info->supported_sampled_texture_format;
}

bool IsUintTextureFormat(uint32_t format) {
	const auto* info = FindFormatInfo(format);
	return info != nullptr && info->unsigned_integer_texture_format;
}

bool IsFmaskTextureFormat(uint32_t format) {
	return format == GpuEnumValue(BufferFormat::kFmask8_S4_F4);
}

// Returns the bytes-per-element of a Vulkan format (the standard packed texel
// size, NOT the block-compressed block size). Returns 0 for formats with no
// standard packed size (e.g. block-compressed or undefined).
// Used to guard format reinterpretation between mutable views.
uint32_t VulkanBytesPerElement(vk::Format format) {
	switch (format) {
		case vk::Format::eR4G4UnormPack8:
		case vk::Format::eR8Sint:
		case vk::Format::eR8Snorm:
		case vk::Format::eR8Srgb:
		case vk::Format::eR8Sscaled:
		case vk::Format::eR8Uint:
		case vk::Format::eR8Unorm:
		case vk::Format::eR8Uscaled: return 1;

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
		case vk::Format::eR8G8Uscaled: return 2;

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
		case vk::Format::eR8G8B8Uscaled: return 3;

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
		case vk::Format::eR8G8B8A8Uscaled: return 4;

		case vk::Format::eR16G16B16Sfloat:
		case vk::Format::eR16G16B16Sint:
		case vk::Format::eR16G16B16Snorm:
		case vk::Format::eR16G16B16Sscaled:
		case vk::Format::eR16G16B16Uint:
		case vk::Format::eR16G16B16Unorm:
		case vk::Format::eR16G16B16Uscaled: return 6;

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
		case vk::Format::eR64Uint: return 8;

		case vk::Format::eR32G32B32Sfloat:
		case vk::Format::eR32G32B32Sint:
		case vk::Format::eR32G32B32Uint: return 12;

		case vk::Format::eR32G32B32A32Sfloat:
		case vk::Format::eR32G32B32A32Sint:
		case vk::Format::eR32G32B32A32Uint:
		case vk::Format::eR64G64Sfloat:
		case vk::Format::eR64G64Sint:
		case vk::Format::eR64G64Uint: return 16;

		case vk::Format::eR64G64B64Sfloat:
		case vk::Format::eR64G64B64Sint:
		case vk::Format::eR64G64B64Uint: return 24;

		case vk::Format::eR64G64B64A64Sfloat:
		case vk::Format::eR64G64B64A64Sint:
		case vk::Format::eR64G64B64A64Uint: return 32;

		case vk::Format::eD16Unorm:
		case vk::Format::eS8Uint: return 2;
		case vk::Format::eX8D24UnormPack32:
		case vk::Format::eD24UnormS8Uint:
		case vk::Format::eD32Sfloat: return 4;
		case vk::Format::eD16UnormS8Uint:
		case vk::Format::eD32SfloatS8Uint: return 5;

		default: return 0;
	}
}

} // namespace Libs::Graphics::Prospero
