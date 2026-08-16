#ifndef EMULATOR_INCLUDE_EMULATOR_GRAPHICS_GUEST_GPU_GPU_FORMAT_H_
#define EMULATOR_INCLUDE_EMULATOR_GRAPHICS_GUEST_GPU_GPU_FORMAT_H_

#include "common/common.h"
#include "graphics/host_gpu/vulkanCommon.h" // IWYU pragma: export

namespace Libs::Graphics::Prospero {

uint32_t NumBytesPerElement(uint32_t format);
uint32_t BlockCompressedBytesPerBlock(uint32_t format);
uint32_t RenderTargetBytesPerElement(uint32_t format);
bool     IsSupportedTextureFormat(uint32_t format);
bool     IsUintTextureFormat(uint32_t format);
bool     IsFmaskTextureFormat(uint32_t format);

// Returns the bytes-per-element of a Vulkan format (the standard packed
// texel size, NOT the block-compressed block size). Returns 0 for formats
// with no standard packed size (e.g. block-compressed or undefined).
// Used to guard format reinterpretation between mutable views.
uint32_t VulkanBytesPerElement(vk::Format format);

} // namespace Libs::Graphics::Prospero

#endif /* EMULATOR_INCLUDE_EMULATOR_GRAPHICS_GUEST_GPU_GPU_FORMAT_H_ */
