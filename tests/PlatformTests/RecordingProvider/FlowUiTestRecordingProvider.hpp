#pragma once

#include "FlowUi/Private/platformProviderService/providers/glfw/GlfwPlatformProviderService.hpp"

#include <cstdint>

namespace FlowUi::test {

/** Counters shared by the library's selected coordinator and its test consumer. */
struct PlatformCallCounts {
	std::uint64_t nativeAcquisitions = 0;
	std::uint64_t nativeReleases = 0;
	std::uint64_t runtimeInitializations = 0;
	std::uint64_t eventPumps = 0;
	std::uint64_t windowInitializations = 0;
	std::uint64_t inputRefreshes = 0;
	std::uint64_t surfaceCreations = 0;
	std::uint64_t clipboardWrites = 0;
	std::uint64_t clipboardReads = 0;
	std::uint64_t cursorRequests = 0;
	std::uint64_t callbackQuiesces = 0;
	std::uint64_t windowShutdowns = 0;
	std::uint64_t runtimeShutdowns = 0;
};

/** Distinct compile-time coordinator which records actual core integration calls. */
class RecordingPlatformProvider {
public:
	using DefaultProvider = detail::platform::GlfwPlatformProviderService;
	using PlatformRuntime = DefaultProvider::PlatformRuntime;
	using PlatformWindow = DefaultProvider::PlatformWindow;
	using PlatformWindowInput = DefaultProvider::PlatformWindowInput;
	using EventBinding = DefaultProvider::EventBinding;
	using WindowState = DefaultProvider::WindowState;
	static constexpr auto contractRevision = DefaultProvider::contractRevision;
	inline static PlatformCallCounts calls{};
	inline static bool failAfterNativeInitialization = false;
	inline static bool failSurfaceCreation = false;
	RecordingPlatformProvider() noexcept = default;
	RecordingPlatformProvider(const RecordingPlatformProvider&) = delete;
	RecordingPlatformProvider& operator=(const RecordingPlatformProvider&) = delete;

	[[nodiscard]] Status initialize(const detail::platform::PlatformConfig& config) {
		++calls.runtimeInitializations;
		return service_.initialize(config);
	}
	[[nodiscard]] Status pollEvents() {
		++calls.eventPumps;
		return service_.pollEvents();
	}
	[[nodiscard]] Status initializeWindow(WindowState& state, WindowId id,
										  const WindowConfig& config, InputSink& sink) {
		++calls.windowInitializations;
		const auto status = service_.initializeWindow(state, id, config, sink);
		if (!status)
			return status;
		++calls.nativeAcquisitions;
		if (failAfterNativeInitialization) {
			failAfterNativeInitialization = false;
			shutdownWindow(state);
			return unexpectedError(
				makeError(ErrorCode::PlatformInitializationFailed, ErrorSite::WindowCreate));
		}
		return {};
	}
	[[nodiscard]] Status refreshInput(WindowState& state) {
		++calls.inputRefreshes;
		return service_.refreshInput(state);
	}
	[[nodiscard]] Result<VkSurfaceKHR> createSurface(WindowState& state, VkInstance instance) {
		++calls.surfaceCreations;
		if (failSurfaceCreation) {
			failSurfaceCreation = false;
			return unexpectedError(makeError(ErrorCode::PlatformSurfaceCreationFailed,
											 ErrorSite::WindowSurfaceCreate));
		}
		return service_.createSurface(state, instance);
	}
	[[nodiscard]] Status setClipboardText(WindowState& state, std::string_view text) {
		++calls.clipboardWrites;
		return service_.setClipboardText(state, text);
	}
	[[nodiscard]] Result<std::string> clipboardText(const WindowState& state) const {
		++calls.clipboardReads;
		return service_.clipboardText(state);
	}
	[[nodiscard]] Status setCursor(WindowState& state, CursorType cursor) {
		++calls.cursorRequests;
		return service_.setCursor(state, cursor);
	}
	void quiesceWindow(WindowState& state) noexcept {
		++calls.callbackQuiesces;
		service_.quiesceWindow(state);
	}
	void shutdownWindow(WindowState& state) noexcept {
		++calls.windowShutdowns;
		if (service_.nativeHandle(state))
			++calls.nativeReleases;
		service_.shutdownWindow(state);
	}
	void shutdown() noexcept {
		++calls.runtimeShutdowns;
		service_.shutdown();
	}
	[[nodiscard]] bool ready() const noexcept { return service_.ready(); }
	[[nodiscard]] bool windowReady(const WindowState& state) const noexcept {
		return service_.windowReady(state);
	}
	[[nodiscard]] Status validateWindowOwner(const WindowState& state) const noexcept {
		return service_.validateWindowOwner(state);
	}
	[[nodiscard]] Result<std::vector<const char*>> requiredInstanceExtensions() const {
		return service_.requiredInstanceExtensions();
	}
	void setShouldClose(WindowState& state, bool closed) noexcept {
		service_.setShouldClose(state, closed);
	}
	[[nodiscard]] Status setTitle(WindowState& state, std::string_view title) {
		return service_.setTitle(state, title);
	}
	[[nodiscard]] Status setInputConfig(WindowState& state, const WindowInputConfig& config) {
		return service_.setInputConfig(state, config);
	}
	[[nodiscard]] Status setVisible(WindowState& state, bool visible) noexcept {
		return service_.setVisible(state, visible);
	}
	[[nodiscard]] bool shouldClose(const WindowState& state) const noexcept {
		return service_.shouldClose(state);
	}
	[[nodiscard]] WindowOperationCapabilities
	windowOperationCapabilities(const WindowState& state) const noexcept {
		return service_.windowOperationCapabilities(state);
	}
	[[nodiscard]] VkExtent2D windowExtent(const WindowState& state) const noexcept {
		return service_.windowExtent(state);
	}
	[[nodiscard]] VkExtent2D framebufferExtent(const WindowState& state) const noexcept {
		return service_.framebufferExtent(state);
	}
	[[nodiscard]] void* nativeHandle(const WindowState& state) const noexcept {
		return service_.nativeHandle(state);
	}
	[[nodiscard]] WindowInputConfig inputConfig(const WindowState& state) const noexcept {
		return service_.inputConfig(state);
	}
	[[nodiscard]] bool supportsRawMouseMotion(const WindowState& state) const noexcept {
		return service_.supportsRawMouseMotion(state);
	}
	[[nodiscard]] bool hasClipboardAccess(const WindowState& state) const noexcept {
		return service_.hasClipboardAccess(state);
	}
	[[nodiscard]] bool isFocused(const WindowState& state) const noexcept {
		return service_.isFocused(state);
	}
	[[nodiscard]] bool isVisible(const WindowState& state) const noexcept {
		return service_.isVisible(state);
	}
	[[nodiscard]] bool isMinimized(const WindowState& state) const noexcept {
		return service_.isMinimized(state);
	}
	[[nodiscard]] Status requestWindowClose(WindowState& state) noexcept {
		return service_.requestWindowClose(state);
	}
	[[nodiscard]] Status minimizeWindow(WindowState& state) noexcept {
		return service_.minimizeWindow(state);
	}
	[[nodiscard]] Status toggleWindowMaximized(WindowState& state) noexcept {
		return service_.toggleWindowMaximized(state);
	}
	[[nodiscard]] Status toggleWindowFullscreen(WindowState& state) noexcept {
		return service_.toggleWindowFullscreen(state);
	}
	[[nodiscard]] Status requestFocus(WindowState& state) noexcept {
		return service_.requestFocus(state);
	}

private:
	DefaultProvider service_;
};

} // namespace FlowUi::test
