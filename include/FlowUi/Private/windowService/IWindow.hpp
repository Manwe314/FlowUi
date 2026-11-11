#pragma once

#include <string>
#include <vector>
#include <string_view>
#include <vulkan/vulkan.h>

#include "FlowUi/Public/configurationStructs/InputConfig.hpp"
#include "FlowUi/Public/errorManagement/Result.hpp"
#include "FlowUi/Public/managerStructs/InputTypes.hpp"
#include "FlowUi/Public/managerStructs/WindowOperationTypes.hpp"

namespace FlowUi::detail {

struct IWindowBackend {
	virtual ~IWindowBackend() = default;

	virtual void refreshInputSnapshot() = 0;
	virtual void detachCallbacks() noexcept = 0;
	virtual bool shouldClose() const = 0;
	virtual void setShouldClose(int value) = 0;

	/** Report only native operations implemented by this backend. */
	[[nodiscard]] virtual WindowOperationCapabilities
	window_operation_capabilities() const noexcept {
		return {};
	}
	/** Request closure without destroying native or rendering resources inline. */
	[[nodiscard]] virtual Status request_window_close() noexcept {
		return unexpectedError(
			makeError(ErrorCode::WindowOperationUnsupported, ErrorSite::WindowOperation));
	}
	/** Request native minimization. */
	[[nodiscard]] virtual Status minimize_window() noexcept {
		return unexpectedError(
			makeError(ErrorCode::WindowOperationUnsupported, ErrorSite::WindowOperation));
	}
	/** Toggle native maximization; fullscreen windows must first leave fullscreen. */
	[[nodiscard]] virtual Status toggle_window_maximized() noexcept {
		return unexpectedError(
			makeError(ErrorCode::WindowOperationUnsupported, ErrorSite::WindowOperation));
	}
	/** Toggle fullscreen with retained windowed restoration state. */
	[[nodiscard]] virtual Status toggle_window_fullscreen() noexcept {
		return unexpectedError(
			makeError(ErrorCode::WindowOperationUnsupported, ErrorSite::WindowOperation));
	}

	virtual std::vector<const char*> requiredInstanceExtensions() const = 0;
	virtual VkSurfaceKHR createSurface(VkInstance instance) = 0;

	virtual VkExtent2D windowExtent() const = 0;
	virtual VkExtent2D framebufferExtent() const = 0;
	virtual void setTitle(std::string_view title) = 0;
	virtual void setInputConfig(const FlowUi::WindowInputConfig& config) = 0;
	virtual FlowUi::WindowInputConfig getInputConfig() const = 0;
	virtual void setCursorType(FlowUi::CursorType cursorType) = 0;
	virtual bool supportsRawMouseMotion() const = 0;
	virtual void setClipboardText(std::string_view text) = 0;
	virtual std::string getClipboardText() const = 0;

	virtual void* nativeHandle() const = 0;
};

} // namespace FlowUi::detail
