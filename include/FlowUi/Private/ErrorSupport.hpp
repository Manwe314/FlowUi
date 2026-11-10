#pragma once

#include "FlowUi/Public/errorManagement/ErrorCodeTypes.hpp"

namespace FlowUi {

namespace detail {

[[nodiscard]] constexpr ErrorDomain errorDomainFromValue(ErrorCode code) noexcept {
	switch (static_cast<std::uint16_t>(code) >> 8u) {
	case 0x01:
		return ErrorDomain::Core;
	case 0x02:
		return ErrorDomain::App;
	case 0x03:
		return ErrorDomain::Platform;
	case 0x04:
		return ErrorDomain::Window;
	case 0x05:
		return ErrorDomain::Frame;
	case 0x06:
		return ErrorDomain::Vulkan;
	case 0x07:
		return ErrorDomain::Renderer;
	case 0x08:
		return ErrorDomain::Storage;
	case 0x09:
		return ErrorDomain::Resource;
	case 0x0a:
		return ErrorDomain::Asset;
	case 0x0b:
		return ErrorDomain::Font;
	case 0x0c:
		return ErrorDomain::Image;
	case 0x0d:
		return ErrorDomain::Icon;
	case 0x0e:
		return ErrorDomain::Element;
	case 0x0f:
		return ErrorDomain::Action;
	case 0x10:
		return ErrorDomain::Theme;
	case 0x11:
		return ErrorDomain::Input;
	case 0x12:
		return ErrorDomain::Popup;
	case 0x13:
		return ErrorDomain::Shortcut;
	case 0x14:
		return ErrorDomain::Viewport;
	case 0x15:
		return ErrorDomain::Agentic;
	default:
		return ErrorDomain::None;
	}
}

[[nodiscard]] constexpr ErrorDescriptor
localDescriptor(ErrorCode code, std::string_view name, std::string_view message,
				ErrorBoundary boundary = ErrorBoundary::DirectCaller) noexcept {
	return ErrorDescriptor{
		.code = code,
		.domain = errorDomainFromValue(code),
		.category = ErrorCategory::Local,
		.boundary = boundary,
		.resolvability = ErrorResolvability::CallerResolvable,
		.impact = ErrorImpact::OperationFailed,
		.resolutionPolicy = ErrorResolutionPolicy::CallerDefined,
		.defaultResolution = ErrorResolution::Rejected,
		.name = name,
		.message = message,
	};
}

} // namespace detail

} // namespace FlowUi
