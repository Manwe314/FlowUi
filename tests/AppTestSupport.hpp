#pragma once

#include "FlowUi/Public/App.hpp"

#include <iostream>

namespace FlowUi::test {

/** Check native integration prerequisites before a runner consumes initialization errors. */
[[nodiscard]] inline int applicationEnvironmentStatus() {
	AppConfig config{};
	config.window.width = 160;
	config.window.height = 120;
	config.vk.enableValidation = false;
	const auto application = makeApplication(config);
	if (application)
		return 0;
	std::cerr << "Application integration prerequisites: " << application.error().message() << '\n';
	switch (application.error().code) {
	case ErrorCode::PlatformInitializationFailed:
	case ErrorCode::WindowCreationFailed:
	case ErrorCode::VulkanDeviceUnavailable:
	case ErrorCode::VulkanVersionUnsupported:
	case ErrorCode::VulkanExtensionMissing:
	case ErrorCode::VulkanFeatureMissing:
	case ErrorCode::WindowPresentationUnsupported:
		return 77;
	default:
		return 1;
	}
}

} // namespace FlowUi::test
