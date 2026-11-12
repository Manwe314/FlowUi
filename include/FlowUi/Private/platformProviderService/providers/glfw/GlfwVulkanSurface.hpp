#pragma once
#include "FlowUi/Public/errorManagement/ErrorCodeTypes.hpp"
#include "FlowUi/Public/errorManagement/ErrorSites.hpp"
#include "FlowUi/Public/errorManagement/Result.hpp"
#include <cstdint>
#ifndef GLFW_INCLUDE_VULKAN
#define GLFW_INCLUDE_VULKAN
#endif
#include <GLFW/glfw3.h>
#include <vulkan/vulkan.h>
namespace FlowUi::detail::platform {
/** Create the current renderer's presentation surface; graphics ownership destroys it. */
[[nodiscard]] inline Result<VkSurfaceKHR> createGlfwVulkanSurface(GLFWwindow* window,
																  VkInstance instance) noexcept {
	if (!window || instance == VK_NULL_HANDLE)
		return unexpectedError(
			makeError(ErrorCode::ObjectNotInitialized, ErrorSite::WindowSurfaceCreate));
	VkSurfaceKHR surface = VK_NULL_HANDLE;
	const VkResult result = glfwCreateWindowSurface(instance, window, nullptr, &surface);
	if (result != VK_SUCCESS || surface == VK_NULL_HANDLE)
		return unexpectedError(makeError(ErrorCode::PlatformSurfaceCreationFailed,
										 ErrorSite::WindowSurfaceCreate, 0u, 0u,
										 static_cast<std::uint32_t>(result)));
	return surface;
}
} // namespace FlowUi::detail::platform
