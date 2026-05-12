#pragma once

#include <string>
#include <vector>
#include <string_view>
#include <vulkan/vulkan.h>

#include "FlowUi/PublicStructs.hpp"

/** @addtogroup flowui_app
 * @{
 */

/** @brief Interface implemented by platform window backends. */
struct IWindowBackend {
	/** @brief Destroy the backend. */
	virtual ~IWindowBackend() = default;

	/** @brief Poll platform events. */
	virtual void pollEvents() = 0;
	/** @brief Return true when the window should close. */
	virtual bool shouldClose() const = 0;

	/** @brief Return Vulkan instance extensions required by the backend. */
	virtual std::vector<const char*> requiredInstanceExtensions() const = 0;
	/** @brief Create a Vulkan surface for the window. */
	virtual VkSurfaceKHR createSurface(VkInstance instance) = 0;

	/** @brief Return the window extent in screen coordinates. */
	virtual VkExtent2D windowExtent() const = 0;
	/** @brief Return the framebuffer extent in pixels. */
	virtual VkExtent2D framebufferExtent() const = 0;
	/** @brief Set the window title. */
	virtual void setTitle(std::string_view title) = 0;
	/** @brief Apply window input configuration. */
	virtual void setInputConfig(const FlowUi::WindowInputConfig& config) = 0;
	/** @brief Return the current input configuration. */
	virtual FlowUi::WindowInputConfig getInputConfig() const = 0;
	/** @brief Set the platform cursor shape. */
	virtual void setCursorType(FlowUi::CursorType cursorType) = 0;
	/** @brief Return true if raw mouse motion is supported. */
	virtual bool supportsRawMouseMotion() const = 0;
	/** @brief Set clipboard text. */
	virtual void setClipboardText(std::string_view text) = 0;
	/** @brief Get clipboard text. */
	virtual std::string getClipboardText() const = 0;

	/** @brief Return the native platform window handle. */
	virtual void* nativeHandle() const = 0;
};

/** @} */
