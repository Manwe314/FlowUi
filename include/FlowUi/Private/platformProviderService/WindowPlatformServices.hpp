#pragma once

#include "FlowUi/Private/platformProviderService/PlatformProviderSelection.hpp"
#include "FlowUi/Public/managerStructs/InputTypes.hpp"

#include <string>
#include <string_view>

namespace FlowUi::detail::platform {

/** Borrowed, per-window clipboard and cursor bridge used by UI managers. */
class WindowPlatformServices {
public:
	/** Bind only while both the coordinator and native window state remain alive. */
	void bind(ActivePlatformProviderService& service,
			  ActivePlatformProviderService::WindowState& state) noexcept;
	/** Stop manager access before native quiescence and UI storage destruction. */
	void detach() noexcept;
	/** Whether this attached window supports immediate clipboard reads and writes. */
	[[nodiscard]] bool hasClipboardAccess() const noexcept;
	/** Write text immediately, preserving provider failures. */
	[[nodiscard]] Status setClipboardText(std::string_view text) const noexcept;
	/** Read text immediately, preserving provider failures. */
	[[nodiscard]] Result<std::string> clipboardText() const noexcept;
	/** Apply the final cursor chosen by the frame's priority arbitration. */
	[[nodiscard]] Status setCursor(CursorType cursor) const noexcept;

private:
	ActivePlatformProviderService* service_ = nullptr;
	ActivePlatformProviderService::WindowState* state_ = nullptr;
};

} // namespace FlowUi::detail::platform
