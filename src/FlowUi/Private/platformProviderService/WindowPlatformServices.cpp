#include "FlowUi/Private/platformProviderService/WindowPlatformServices.hpp"

#include "FlowUi/Private/ErrorReporting.hpp"

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

void WindowPlatformServices::setClipboardText(std::string_view text) const {
	if (!service_ || !state_)
		return;
	const auto status = service_->setClipboardText(*state_, text);
	if (!status)
		reportErrorEvent({.error = status.error(), .kind = ErrorEventKind::Reported});
}

std::string WindowPlatformServices::clipboardText() const {
	if (!service_ || !state_)
		return {};
	auto result = service_->clipboardText(*state_);
	if (!result) {
		reportErrorEvent({.error = result.error(), .kind = ErrorEventKind::Reported});
		return {};
	}
	return std::move(*result);
}

void WindowPlatformServices::setCursor(CursorType cursor) const {
	if (!service_ || !state_)
		return;
	const auto status = service_->setCursor(*state_, cursor);
	if (!status)
		reportErrorEvent({.error = status.error(), .kind = ErrorEventKind::Reported});
}

} // namespace FlowUi::detail::platform
