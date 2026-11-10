#pragma once

#include <cstdint>

namespace FlowUi {

/** @addtogroup flowui_config
 * @{
 */

/** @brief Presentation mode requested for the Vulkan swapchain. */
enum class PresentMode : std::uint8_t {
	Fifo, /**< Queue presentation for vertical synchronization. */
	Mailbox, /**< Present the newest queued image at vertical synchronization when supported. */
	Immediate /**< Present without waiting for vertical synchronization when supported. */
};

/**
 * @brief Configures Vulkan device, swapchain, and frame scheduling defaults.
 *
 * These options are consumed during renderer initialization.
 * Rendering uses a fixed sample count of one (x1).
 */
struct VulkanConfig {
	/** @brief Preferred swapchain presentation mode. */
	PresentMode presentMode = PresentMode::Fifo;

	/** @brief Number of frame resource slots; app initialization clamps zero to one. */
	uint32_t framesInFlight = 2;
	/** @brief Enable Vulkan validation layers when they are available. */
	bool enableValidation = true;

	/** @brief Enable Vulkan debug utils messenger support when available. */
	bool enableDebugUtils = true;

	/** @brief Prefer a discrete GPU when selecting a physical device. */
	bool preferDiscreteGPU = true;

	/** @brief Prefer an sRGB swapchain format when the surface supports one. */
	bool srgbBackbuffer = true;
};

} // namespace FlowUi
