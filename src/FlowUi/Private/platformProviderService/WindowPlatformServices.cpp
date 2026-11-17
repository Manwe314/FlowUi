#include "FlowUi/Private/platformProviderService/WindowPlatformServices.hpp"

#include "FlowUi/Private/ApiBoundary.hpp"

#include <utility>

namespace FlowUi::detail::platform {

void WindowPlatformServices::bind(ActivePlatformProviderService& service,
								  ActivePlatformProviderService::WindowState& state) noexcept {
	service_ = &service;
	state_ = &state;
}

void WindowPlatformServices::detach() noexcept {
	state_ = nullptr;
	service_ = nullptr;
}

bool WindowPlatformServices::hasClipboardAccess() const noexcept {
	return service_ && state_ && service_->hasClipboardAccess(*state_);
}

Status WindowPlatformServices::setClipboardText(std::string_view text) const noexcept try {
	if (!service_ || !state_)
		return unexpectedError(makeError(ErrorCode::ObjectNotInitialized, ErrorSite::WindowWriteClipboard));
	return service_->setClipboardText(*state_, text);
} catch (...) {
	return unexpectedError(detail::currentExceptionError(ErrorSite::WindowWriteClipboard));
}

Result<std::string> WindowPlatformServices::clipboardText() const noexcept try {
	if (!service_ || !state_)
		return unexpectedError(makeError(ErrorCode::ObjectNotInitialized, ErrorSite::WindowReadClipboard));
	return service_->clipboardText(*state_);
} catch (...) {
	return unexpectedError(detail::currentExceptionError(ErrorSite::WindowReadClipboard));
}

Status WindowPlatformServices::setCursor(CursorType cursor) const noexcept try {
	if (!service_ || !state_)
		return unexpectedError(makeError(ErrorCode::ObjectNotInitialized, ErrorSite::WindowSetCursor));
	return service_->setCursor(*state_, cursor);
} catch (...) {
	return unexpectedError(detail::currentExceptionError(ErrorSite::WindowSetCursor));
}

} // namespace FlowUi::detail::platform
