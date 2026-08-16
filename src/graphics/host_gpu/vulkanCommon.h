#ifndef EMULATOR_SRC_GRAPHICS_HOST_GPU_VULKANCOMMON_H_
#define EMULATOR_SRC_GRAPHICS_HOST_GPU_VULKANCOMMON_H_

// Force dynamic dispatch for Vulkan-Hpp regardless of whether <vulkan/vulkan.hpp>
// was already included by another header (e.g. via gpu_format.h). If vulkan.hpp
// was included before these macros were set, vulkan_hpp_macros.hpp would have
// defaulted VULKAN_HPP_DISPATCH_LOADER_DYNAMIC to 0 (static dispatch), causing
// the compiler to emit direct calls to vk* symbols that are not resolvable when
// only bundled Vulkan-Headers are available (no SDK import library).
// The #undef ensures we can safely redefine these macros even if they were
// already set by a prior include of vulkan_hpp_macros.hpp.
#ifdef VULKAN_HPP_DISPATCH_LOADER_DYNAMIC
#  undef VULKAN_HPP_DISPATCH_LOADER_DYNAMIC
#endif
#define VK_NO_PROTOTYPES
#define VULKAN_HPP_DISPATCH_LOADER_DYNAMIC    1
#define VULKAN_HPP_ENABLE_DYNAMIC_LOADER_TOOL 1
#define VULKAN_HPP_NO_CONSTRUCTORS
#define VULKAN_HPP_NO_EXCEPTIONS

#define VMA_STATIC_VULKAN_FUNCTIONS  0
#define VMA_DYNAMIC_VULKAN_FUNCTIONS 1

#include <cstdint>
#include <string>
#include <vector>
#include <vulkan/vulkan.hpp>
#include <vulkan/vulkan_hash.hpp>

namespace Libs::Graphics {

using VulkanMemoryBarrier = vk::MemoryBarrier;

template <typename Handle>
[[nodiscard]] void* VulkanHandleToPointer(Handle handle) {
	return reinterpret_cast<void*>(static_cast<typename Handle::CType>(handle));
}

std::string VulkanToString(vk::Result value);
std::string VulkanToString(vk::Format value);
std::string VulkanToString(vk::ImageLayout value);
std::string VulkanToString(vk::QueueFlags value);
vk::Format  VulkanFormat(uint32_t guest_format);
void        RequireVulkanSuccess(vk::Result result, const char* operation);

template <typename T, typename Enumerator>
[[nodiscard]] std::vector<T> EnumerateVulkan(const char* operation, Enumerator&& enumerate) {
	for (;;) {
		uint32_t count = 0;
		RequireVulkanSuccess(enumerate(&count, nullptr), operation);
		if (count == 0) {
			return {};
		}

		std::vector<T> values(count);
		const auto     result = enumerate(&count, values.data());
		if (result == vk::Result::eSuccess) {
			values.resize(count);
			return values;
		}
		if (result != vk::Result::eIncomplete) {
			RequireVulkanSuccess(result, operation);
		}
	}
}

} // namespace Libs::Graphics

#endif // EMULATOR_SRC_GRAPHICS_HOST_GPU_VULKANCOMMON_H_
