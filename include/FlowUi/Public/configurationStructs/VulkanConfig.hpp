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
 * Device, swapchain, and frame scheduling defaults consumed at renderer initialization.
 * Availability-dependent preferences may fall back; rendering uses one sample per pixel.
 */
struct VulkanConfig {
	/** Preferred presentation mode; unsupported modes fall back to FIFO (vertical synchronization). */
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

/** @} */

} // namespace FlowUi
