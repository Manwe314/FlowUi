#include "FlowUi/Public/App.hpp"
#include "FlowUi/Public/errorManagement/Exception.hpp"
#include "FlowUi/Public/managers/UiManager.hpp"
#include "../TestHarness.hpp"

#include <vulkan/vulkan.h>

#include <cstdio>
#include <stdexcept>
#include <utility>

namespace {
VkResult nextFenceWait = VK_SUCCESS;
VkResult nextQueueSubmit = VK_SUCCESS;
VkResult nextCommandEnd = VK_SUCCESS;

struct ConditionalCopyFailure {
	bool* failCopy;
	bool* invoked;
	ConditionalCopyFailure(bool& failure, bool& invocation) noexcept
		: failCopy(&failure), invoked(&invocation) {}
	ConditionalCopyFailure(const ConditionalCopyFailure& source)
		: failCopy(source.failCopy), invoked(source.invoked) {
		if (*failCopy) throw 91;
	}
	void operator()(FlowUi::UiManager&, FlowUi::WindowId) const { *invoked = true; }
};

FlowUi::Result<FlowUi::App> application() {
	FlowUi::AppConfig config{};
	config.window.width = 240;
	config.window.height = 160;
	config.window.title = "FlowUi deterministic window recovery";
	config.vk.enableValidation = false;
	return FlowUi::makeApplication(config);
}

void verifyManagedPreparationAndClosingRetry(FlowUi::App& app) {
	using namespace FlowUi;
	const auto first = test::requireValue(app.createWindowLikeMain());
	bool failCopy = false;
	bool invoked = false;
	const UiBuildCallback callback = ConditionalCopyFailure(failCopy, invoked);
	failCopy = true;
	const auto rejected = app.createWindow(WindowConfigOverrides{}, callback);
	FLOWUI_CHECK(!rejected && rejected.error().code == ErrorCode::ForeignExceptionObserved);
	failCopy = false;
	const auto second = test::requireValue(app.createWindow(WindowConfigOverrides{}, callback));
	// Callback preparation failed before native creation consumed the next window identity.
	FLOWUI_CHECK(second == first + 1u);
	FLOWUI_CHECK(app.dispatchManagedWindows());
	FLOWUI_CHECK(invoked);

	nextFenceWait = VK_TIMEOUT;
	const auto interrupted = app.destroyWindow(second);
	FLOWUI_CHECK(!interrupted && interrupted.error().code == ErrorCode::VulkanNativeCallFailed);
	FLOWUI_CHECK(nextFenceWait == VK_SUCCESS);
	FLOWUI_CHECK(app.hasWindow(second));
	FLOWUI_CHECK(test::requireValue(app.shouldClose(second)));
	FLOWUI_CHECK(!app.ui(second));
	FLOWUI_CHECK(!app.beginFrame(second));
	FLOWUI_CHECK(app.removeWindowUiCallback(second));
	FLOWUI_CHECK(app.runFrame(first, [](UiManager&, WindowId) {}));
	FLOWUI_CHECK(app.destroyWindow(second));
	FLOWUI_CHECK(!app.hasWindow(second));
	FLOWUI_CHECK(app.destroyWindow(first));
}

void verifyCallbackErrorDoesNotEstablishDeviceLoss(FlowUi::App& app) {
	using namespace FlowUi;
	const auto spoofed = makeError(ErrorCode::VulkanDeviceLost, ErrorSite::AppSubmitFrame);
	const auto failed = app.runFrame([&](UiManager&, WindowId) { throw FlowUiException(spoofed); });
	FLOWUI_CHECK(!failed && failed.error().code == ErrorCode::VulkanDeviceLost);
	FLOWUI_CHECK(app.runFrame([](UiManager&, WindowId) {}));
}

void verifyRecordingFailureStopsOnlyItsWindow(FlowUi::App& app) {
	using namespace FlowUi;
	const auto second = test::requireValue(app.createWindowLikeMain());
	FLOWUI_CHECK(app.beginFrame(second));
	FLOWUI_CHECK(app.endFrame(second));
	nextCommandEnd = VK_ERROR_OUT_OF_HOST_MEMORY;
	const auto interrupted = app.drawFrame(second);
	FLOWUI_CHECK(!interrupted && interrupted.error().code == ErrorCode::AllocationFailed);
	FLOWUI_CHECK(nextCommandEnd == VK_SUCCESS);
	FLOWUI_CHECK(test::requireValue(app.shouldClose(second)));
	FLOWUI_CHECK(!app.beginFrame(second));
	FLOWUI_CHECK(app.runFrame([](UiManager&, WindowId) {}));
	FLOWUI_CHECK(app.destroyWindow(second));
	const auto replacement = test::requireValue(app.createWindowLikeMain());
	FLOWUI_CHECK(app.runFrame(replacement, [](UiManager&, WindowId) {}));
	FLOWUI_CHECK(app.destroyWindow(replacement));
}

void verifyNativeDeviceLossStopsSharedScope(FlowUi::App& app) {
	using namespace FlowUi;
	const auto second = test::requireValue(app.createWindowLikeMain());
	FLOWUI_CHECK(app.runFrame(second, [](UiManager&, WindowId) {}));
	nextFenceWait = VK_ERROR_DEVICE_LOST;
	const auto lost = app.destroyWindow(second);
	FLOWUI_CHECK(!lost && lost.error().code == ErrorCode::VulkanDeviceLost);
	FLOWUI_CHECK(nextFenceWait == VK_SUCCESS);
	FLOWUI_CHECK(test::requireValue(app.shouldClose()));
	FLOWUI_CHECK(test::requireValue(app.shouldClose(second)));
	FLOWUI_CHECK(!app.beginFrame());
	FLOWUI_CHECK(!app.createWindowLikeMain());
	FLOWUI_CHECK(!app.destroyWindow(second));
}

void verifyFailedSubmitNeverWaitsAnUnsubmittedFence(FlowUi::App& app) {
	using namespace FlowUi;
	const auto second = test::requireValue(app.createWindowLikeMain());
	FLOWUI_CHECK(app.beginFrame(second));
	FLOWUI_CHECK(app.endFrame(second));
	nextQueueSubmit = VK_ERROR_OUT_OF_HOST_MEMORY;
	const auto failed = app.drawFrame(second);
	FLOWUI_CHECK(!failed && failed.error().code == ErrorCode::AllocationFailed);
	FLOWUI_CHECK(nextQueueSubmit == VK_SUCCESS);
	FLOWUI_CHECK(test::requireValue(app.shouldClose()));
	FLOWUI_CHECK(test::requireValue(app.shouldClose(second)));
	// Scope shutdown drains the real device, then completes the conservatively
	// registered storage token. It must never wait the reset, unsubmitted fence.
}
} // namespace

extern "C" VKAPI_ATTR VkResult VKAPI_CALL __real_vkWaitForFences(
	VkDevice device, uint32_t fenceCount, const VkFence* fences, VkBool32 waitAll, uint64_t timeout);
extern "C" VKAPI_ATTR VkResult VKAPI_CALL __wrap_vkWaitForFences(
	VkDevice device, uint32_t fenceCount, const VkFence* fences, VkBool32 waitAll, uint64_t timeout) {
	if (nextFenceWait != VK_SUCCESS)
		return std::exchange(nextFenceWait, VK_SUCCESS);
	return __real_vkWaitForFences(device, fenceCount, fences, waitAll, timeout);
}
extern "C" VKAPI_ATTR VkResult VKAPI_CALL __real_vkQueueSubmit(
	VkQueue queue, uint32_t submitCount, const VkSubmitInfo* submissions, VkFence fence);
extern "C" VKAPI_ATTR VkResult VKAPI_CALL __wrap_vkQueueSubmit(
	VkQueue queue, uint32_t submitCount, const VkSubmitInfo* submissions, VkFence fence) {
	if (nextQueueSubmit != VK_SUCCESS)
		return std::exchange(nextQueueSubmit, VK_SUCCESS);
	return __real_vkQueueSubmit(queue, submitCount, submissions, fence);
}

extern "C" VKAPI_ATTR VkResult VKAPI_CALL __real_vkEndCommandBuffer(VkCommandBuffer command);
extern "C" VKAPI_ATTR VkResult VKAPI_CALL __wrap_vkEndCommandBuffer(VkCommandBuffer command) {
	if (nextCommandEnd != VK_SUCCESS)
		return std::exchange(nextCommandEnd, VK_SUCCESS);
	return __real_vkEndCommandBuffer(command);
}

int main() {
	try {
		auto initialized = application();
		if (!initialized) {
			switch (initialized.error().code) {
			case FlowUi::ErrorCode::PlatformInitializationFailed:
			case FlowUi::ErrorCode::VulkanDeviceUnavailable:
			case FlowUi::ErrorCode::VulkanVersionUnsupported:
			case FlowUi::ErrorCode::VulkanExtensionMissing:
			case FlowUi::ErrorCode::VulkanFeatureMissing:
			case FlowUi::ErrorCode::WindowPresentationUnsupported:
				return 77;
			default:
				return 1;
			}
		}
		{
			auto app = std::move(*initialized);
			const auto probe = app.createWindowLikeMain();
			if (!probe && probe.error().code == FlowUi::ErrorCode::WindowPresentationUnsupported)
				return 77;
			FLOWUI_CHECK(probe);
			FLOWUI_CHECK(app.destroyWindow(*probe));
			verifyManagedPreparationAndClosingRetry(app);
			verifyCallbackErrorDoesNotEstablishDeviceLoss(app);
			verifyRecordingFailureStopsOnlyItsWindow(app);
			verifyNativeDeviceLossStopsSharedScope(app);
		}
		{
			auto app = FlowUi::test::requireValue(application());
			verifyFailedSubmitNeverWaitsAnUnsubmittedFence(app);
		}
		return 0;
	} catch (const std::exception& error) {
		std::fprintf(stderr, "%s\n", error.what());
		return 2;
	}
}
