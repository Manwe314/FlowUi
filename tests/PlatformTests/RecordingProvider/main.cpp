#include "FlowUi.hpp"
#include "FlowUi/Public/managers/UiManager.hpp"
#include "FlowUiTestRecordingProvider.hpp"

#include <cstdlib>
#include <iostream>
#include <utility>

int main() {
	using namespace FlowUi;
	using Provider = test::RecordingPlatformProvider;
	if (!std::getenv("DISPLAY") && !std::getenv("WAYLAND_DISPLAY")) {
		std::cout << "SKIP: no native display.\n";
		return 77;
	}
	Provider::calls = {};
	const auto fail = [](int code) {
		const auto& counts = Provider::calls;
		std::cerr << "Recording provider failure " << code
				  << " acquisitions=" << counts.nativeAcquisitions
				  << " releases=" << counts.nativeReleases
				  << " initializations=" << counts.windowInitializations
				  << " surfaces=" << counts.surfaceCreations << " pumps=" << counts.eventPumps
				  << " refreshes=" << counts.inputRefreshes
				  << " clipboard=" << counts.clipboardWrites << '/' << counts.clipboardReads
				  << " cursors=" << counts.cursorRequests
				  << " runtimeShutdowns=" << counts.runtimeShutdowns << '\n';
		return code;
	};
	AppConfig configuration;
	configuration.window.width = 320;
	configuration.window.height = 240;
	configuration.vk.enableValidation = false;
	configuration.resources.use_build_directory = false;
	{
		auto initialApplication = makeApplication(configuration);
		const auto initialHandle = initialApplication.nativeWindowHandle();
		auto application = std::move(initialApplication);
		if (application.nativeWindowHandle() != initialHandle)
			return fail(9);
		application.ui().setClipboardText("FlowUi selected recording provider");
		if (application.ui().clipboardText() != "FlowUi selected recording provider")
			return fail(1);
		const auto invalidWindow = application.createWindow(WindowConfigOverrides{.width = -1});
		if (invalidWindow || invalidWindow.error().code != ErrorCode::InvalidWindowConfiguration)
			return fail(10);
		Provider::failAfterNativeInitialization = true;
		const auto failedNative = application.createWindowLikeMain();
		if (failedNative || failedNative.error().code != ErrorCode::PlatformInitializationFailed)
			return fail(11);
		Provider::failSurfaceCreation = true;
		const auto failedSurface = application.createWindowLikeMain();
		if (failedSurface || failedSurface.error().code != ErrorCode::PlatformSurfaceCreationFailed)
			return fail(12);
		auto second = application.createWindow(WindowConfigOverrides{.width = 240, .height = 180});
		if (!second)
			return fail(2);
		auto third = application.createWindow(WindowConfigOverrides{.width = 220, .height = 160});
		if (!third || application.nativeWindowHandle() != initialHandle)
			return fail(13);
		if (!application.destroyWindow(*third))
			return fail(14);
		for (unsigned frame = 0; frame < 3; ++frame) {
			if (!application.pollEvents())
				return fail(8);
			if (!application.beginFrame(application.mainWindowId()))
				return fail(3);
			application.ui().requestCursor(CursorType::PointingHand, 10);
			if (!application.endFrame(application.mainWindowId()) ||
				!application.drawFrame(application.mainWindowId()))
				return fail(3);
			if (!application.beginFrame(*second) || !application.endFrame(*second) ||
				!application.drawFrame(*second))
				return fail(4);
		}
		if (!application.destroyWindow(*second))
			return fail(5);
		if (Provider::calls.runtimeInitializations != 1 ||
			Provider::calls.windowInitializations != 5 || Provider::calls.surfaceCreations != 4 ||
			Provider::calls.eventPumps != 3 || Provider::calls.inputRefreshes < 6 ||
			Provider::calls.clipboardWrites == 0 || Provider::calls.clipboardReads == 0 ||
			Provider::calls.cursorRequests == 0)
			return fail(6);
	}
	if (Provider::calls.nativeAcquisitions != 5 || Provider::calls.nativeReleases != 5 ||
		Provider::calls.runtimeShutdowns != 1)
		return fail(7);
	std::cout << "Custom coordinator handled runtime, native windows, input, Vulkan, managers and "
				 "teardown.\n";
	return 0;
}
