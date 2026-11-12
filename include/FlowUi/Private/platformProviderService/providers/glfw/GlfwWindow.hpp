#pragma once

#include "FlowUi/Private/platformProviderService/providers/glfw/GlfwRuntime.hpp"
#include "FlowUi/Private/platformProviderService/providers/glfw/GlfwVulkanSurface.hpp"
#include "FlowUi/Private/utility/EnumFlags.hpp"
#include "FlowUi/Public/configurationStructs/WindowConfig.hpp"
#include "FlowUi/Public/managerStructs/WindowOperationTypes.hpp"
#include <algorithm>
#include <cstdint>
#include <new>
#include <string>
#include <string_view>
#include <thread>
#include <vulkan/vulkan.h>

namespace FlowUi::detail::platform {
/** Nonmovable native window and retained native window geometry. */
class GlfwWindow {
private:
	struct WindowedState {
		int positionX = 0;
		int positionY = 0;
		int width = 1280;
		int height = 720;
		bool maximized = false;
	};

	[[nodiscard]] Status validateWindowOperation() const noexcept {
		if (owner_ && std::this_thread::get_id() != platformThread_) {
			return unexpectedError(makeError(ErrorCode::WrongThread, ErrorSite::WindowOperation));
		}
		if (!window) {
			return unexpectedError(
				makeError(ErrorCode::ObjectNotInitialized, ErrorSite::WindowOperation));
		}
		return {};
	}

	[[nodiscard]] static Status nativeOperationResult() noexcept {
		const int nativeError = glfwGetError(nullptr);
		if (nativeError != GLFW_NO_ERROR) {
			return unexpectedError(makeError(ErrorCode::WindowOperationFailed,
											 ErrorSite::WindowOperation, 0u, 0u,
											 static_cast<std::uint32_t>(nativeError)));
		}
		return {};
	}

	void rememberWindowedGeometry() noexcept {
		if (!window || windowFlags_.has_flag(WindowFlag::fullscreenTransition) ||
			windowFlags_.has_flag(WindowFlag::fullscreenRestoreAvailable) ||
			windowFlags_.has_flag(WindowFlag::windowedRestorePending) ||
			glfwGetWindowMonitor(window) ||
			glfwGetWindowAttrib(window, GLFW_MAXIMIZED) == GLFW_TRUE ||
			glfwGetWindowAttrib(window, GLFW_ICONIFIED) == GLFW_TRUE) {
			return;
		}
		glfwGetWindowPos(window, &windowedState_.positionX, &windowedState_.positionY);
		glfwGetWindowSize(window, &windowedState_.width, &windowedState_.height);
	}

	void completeWindowedRestore() noexcept {
		if (!window || !windowFlags_.has_flag(WindowFlag::windowedRestorePending) ||
			windowFlags_.has_flag(WindowFlag::fullscreenTransition) ||
			windowFlags_.has_flag(WindowFlag::restoringWindowedGeometry))
			return;
		glfwGetError(nullptr);
		const bool fullscreen = glfwGetWindowMonitor(window) != nullptr;
		const bool maximized = glfwGetWindowAttrib(window, GLFW_MAXIMIZED) == GLFW_TRUE;
		if (const auto status = nativeOperationResult(); !status) {
			windowFlags_.set_flag(WindowFlag::windowedRestorePending, false);
			reportErrorEvent(
				ErrorEventView{.error = status.error(), .kind = ErrorEventKind::Reported});
			return;
		}
		if (fullscreen || maximized)
			return;
		// Some desktops mark initially fullscreen windows maximized. Their later
		// unmaximize notification can replace the geometry passed to SetWindowMonitor.
		// Apply our retained rectangle after that native state transition completes.
		glfwGetError(nullptr);
		windowFlags_.set_flag(WindowFlag::restoringWindowedGeometry, true);
		glfwSetWindowPos(window, windowedState_.positionX, windowedState_.positionY);
		glfwSetWindowSize(window, windowedState_.width, windowedState_.height);
		windowFlags_.set_flag(WindowFlag::restoringWindowedGeometry, false);
		if (const auto status = nativeOperationResult(); !status) {
			windowFlags_.set_flag(WindowFlag::windowedRestorePending, false);
			reportErrorEvent(
				ErrorEventView{.error = status.error(), .kind = ErrorEventKind::Reported});
			return;
		}
		int restoredX = 0;
		int restoredY = 0;
		int restoredWidth = 0;
		int restoredHeight = 0;
		glfwGetWindowPos(window, &restoredX, &restoredY);
		glfwGetWindowSize(window, &restoredWidth, &restoredHeight);
		if (const auto status = nativeOperationResult(); !status) {
			windowFlags_.set_flag(WindowFlag::windowedRestorePending, false);
			reportErrorEvent(
				ErrorEventView{.error = status.error(), .kind = ErrorEventKind::Reported});
			return;
		}
		windowFlags_.set_flag(
			WindowFlag::windowedRestorePending,
			restoredX != windowedState_.positionX || restoredY != windowedState_.positionY ||
				restoredWidth != windowedState_.width || restoredHeight != windowedState_.height);
	}

	[[nodiscard]] static std::int64_t monitorOverlap(const WindowedState& state,
													 GLFWmonitor* monitor) noexcept {
		int monitorX = 0;
		int monitorY = 0;
		glfwGetMonitorPos(monitor, &monitorX, &monitorY);
		const GLFWvidmode* mode = glfwGetVideoMode(monitor);
		if (!mode)
			return 0;
		const std::int64_t overlapWidth = std::max<std::int64_t>(
			0, std::min<std::int64_t>(static_cast<std::int64_t>(state.positionX) + state.width,
									  static_cast<std::int64_t>(monitorX) + mode->width) -
				   std::max(state.positionX, monitorX));
		const std::int64_t overlapHeight = std::max<std::int64_t>(
			0, std::min<std::int64_t>(static_cast<std::int64_t>(state.positionY) + state.height,
									  static_cast<std::int64_t>(monitorY) + mode->height) -
				   std::max(state.positionY, monitorY));
		return overlapWidth * overlapHeight;
	}

	[[nodiscard]] static GLFWmonitor* selectMonitor(const WindowedState& state) noexcept {
		GLFWmonitor* selectedMonitor = glfwGetPrimaryMonitor();
		std::int64_t greatestOverlap = 0;
		int monitorCount = 0;
		GLFWmonitor** monitors = glfwGetMonitors(&monitorCount);
		for (int monitorIndex = 0; monitorIndex < monitorCount; ++monitorIndex) {
			const std::int64_t overlap = monitorOverlap(state, monitors[monitorIndex]);
			if (overlap > greatestOverlap) {
				greatestOverlap = overlap;
				selectedMonitor = monitors[monitorIndex];
			}
		}
		return selectedMonitor;
	}

	static void centerOnMonitor(WindowedState& state, GLFWmonitor* monitor) noexcept {
		int workX = 0;
		int workY = 0;
		int workWidth = 0;
		int workHeight = 0;
		glfwGetMonitorWorkarea(monitor, &workX, &workY, &workWidth, &workHeight);
		if (workWidth <= 0 || workHeight <= 0)
			return;
		state.width = std::min(state.width, workWidth);
		state.height = std::min(state.height, workHeight);
		state.positionX = workX + (workWidth - state.width) / 2;
		state.positionY = workY + (workHeight - state.height) / 2;
	}

	WindowedState windowedState_{};
	WindowedState fullscreenRestoreState_{};
	std::thread::id platformThread_{};
	GlfwRuntime* owner_ = nullptr;
	GLFWwindow* window = nullptr;
	enum class WindowFlag : std::uint8_t {
		fullscreenRestoreAvailable = 1u << 0u,
		fullscreenTransition = 1u << 1u,
		windowedRestorePending = 1u << 2u,
		restoringWindowedGeometry = 1u << 3u,
	};
	EnumFlags<WindowFlag> windowFlags_{};
	friend class GlfwEventBinding;

public:
	GlfwWindow() noexcept = default;
	~GlfwWindow() noexcept { shutdown(); }
	GlfwWindow(const GlfwWindow&) = delete;
	GlfwWindow& operator=(const GlfwWindow&) = delete;
	/** Acquire a native window from this exact runtime instance. */
	[[nodiscard]] Status initialize(GlfwRuntime& runtime, const WindowConfig& config) noexcept {
		if (const auto status = runtime.validate(); !status)
			return status;
		if (window)
			return unexpectedError(
				makeError(ErrorCode::ObjectAlreadyInitialized, ErrorSite::WindowCreate));
		if (config.width <= 0 || config.height <= 0)
			return unexpectedError(
				makeError(ErrorCode::InvalidWindowConfiguration, ErrorSite::WindowCreate));
		glfwGetError(nullptr);
		glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
		glfwWindowHint(GLFW_RESIZABLE, config.resizable ? GLFW_TRUE : GLFW_FALSE);
		glfwWindowHint(GLFW_DECORATED, config.decorated ? GLFW_TRUE : GLFW_FALSE);
		glfwWindowHint(GLFW_MAXIMIZED, config.maximized ? GLFW_TRUE : GLFW_FALSE);
#ifdef GLFW_SCALE_TO_MONITOR
		glfwWindowHint(GLFW_SCALE_TO_MONITOR, config.highDPI ? GLFW_TRUE : GLFW_FALSE);
#endif
#ifdef GLFW_COCOA_RETINA_FRAMEBUFFER
		glfwWindowHint(GLFW_COCOA_RETINA_FRAMEBUFFER, config.highDPI ? GLFW_TRUE : GLFW_FALSE);
#endif

		GLFWmonitor* monitor = nullptr;
		int width = std::max(1, config.width);
		int height = std::max(1, config.height);
		if (config.fullscreen) {
			monitor = glfwGetPrimaryMonitor();
			if (!monitor) {
				return unexpectedError(makeError(ErrorCode::PlatformMonitorUnavailable,
												 ErrorSite::WindowMonitorSelect));
			}
			const GLFWvidmode* mode = glfwGetVideoMode(monitor);
			if (!mode) {
				return unexpectedError(makeError(ErrorCode::PlatformMonitorUnavailable,
												 ErrorSite::WindowMonitorSelect));
			}
			width = mode->width;
			height = mode->height;
		}

		window = glfwCreateWindow(width, height, config.title.c_str(), monitor, nullptr);
		if (!window) {
			return unexpectedError(
				makeError(ErrorCode::WindowCreationFailed, ErrorSite::WindowCreate));
		}
		windowedState_.width = std::max(1, config.width);
		windowedState_.height = std::max(1, config.height);
		windowedState_.maximized = config.maximized;
		if (config.fullscreen || config.maximized) {
			if (GLFWmonitor* fallbackMonitor = glfwGetPrimaryMonitor()) {
				centerOnMonitor(windowedState_, fallbackMonitor);
			}
		} else {
			rememberWindowedGeometry();
		}
		if (config.fullscreen) {
			fullscreenRestoreState_ = windowedState_;
			windowFlags_.set_flag(WindowFlag::fullscreenRestoreAvailable, true);
		}

		owner_ = &runtime;
		platformThread_ = std::this_thread::get_id();
		++runtime.windowCount_;
		if (const auto status = GlfwRuntime::nativeResult(ErrorSite::WindowCreate); !status) {
			shutdown();
			return status;
		}
		return {};
	}
	/** Release this window after callbacks, input and the graphics surface are gone. */
	void shutdown() noexcept {
		if (!window)
			return;
		if (const auto status = validateWindowOperation(); !status) {
			reportErrorEvent(
				ErrorEventView{.error = status.error(), .kind = ErrorEventKind::Reported});
			return;
		}
		glfwDestroyWindow(window);
		window = nullptr;
		--owner_->windowCount_;
		owner_ = nullptr;
		windowFlags_ = {};
	}
	/** Observe native readiness. */
	[[nodiscard]] bool ready() const noexcept { return window != nullptr; }
	/** Borrow the owning runtime identity for typed integration validation. */
	[[nodiscard]] const GlfwRuntime* owner() const noexcept { return owner_; }
	/** Validate native ownership, readiness and platform thread. */
	[[nodiscard]] Status validate() const noexcept { return validateWindowOperation(); }
	/** Borrow the GLFW handle within the provider integration boundary. */
	[[nodiscard]] GLFWwindow* native() const noexcept { return window; }
	/** Observe the requested close flag. */
	[[nodiscard]] bool shouldClose() const noexcept {
		return !window || glfwWindowShouldClose(window) == GLFW_TRUE;
	}
	/** Set or clear the requested close flag. */
	void setShouldClose(bool value) noexcept {
		if (validate())
			glfwSetWindowShouldClose(window, value ? GLFW_TRUE : GLFW_FALSE);
	}
	/** Report the native operations implemented for this window. */
	[[nodiscard]] WindowOperationCapabilities windowOperationCapabilities() const noexcept {
		if (!window)
			return {};
		return {.close_window = true,
				.minimize = true,
				.maximize_restore = true,
				.toggle_fullscreen = true};
	}

	/** Request closure without releasing resources inline. */
	[[nodiscard]] Status requestWindowClose() noexcept {
		if (const auto status = validateWindowOperation(); !status)
			return status;
		glfwGetError(nullptr);
		glfwSetWindowShouldClose(window, GLFW_TRUE);
		return nativeOperationResult();
	}

	/** Request native minimization on the platform thread. */
	[[nodiscard]] Status minimizeWindow() noexcept {
		if (const auto status = validateWindowOperation(); !status)
			return status;
		glfwGetError(nullptr);
		glfwIconifyWindow(window);
		return nativeOperationResult();
	}

	/** Toggle maximization while preserving windowed geometry. */
	[[nodiscard]] Status toggleWindowMaximized() noexcept {
		if (const auto status = validateWindowOperation(); !status)
			return status;
		if (windowFlags_.has_flag(WindowFlag::windowedRestorePending))
			completeWindowedRestore();
		if (windowFlags_.has_flag(WindowFlag::windowedRestorePending))
			return unexpectedError(
				makeError(ErrorCode::WindowOperationFailed, ErrorSite::WindowOperation));
		glfwGetError(nullptr);
		if (windowFlags_.has_flag(WindowFlag::fullscreenRestoreAvailable) ||
			glfwGetWindowMonitor(window)) {
			return unexpectedError(
				makeError(ErrorCode::WindowOperationUnsupported, ErrorSite::WindowOperation));
		}
		if (glfwGetWindowAttrib(window, GLFW_MAXIMIZED) == GLFW_TRUE) {
			windowedState_.maximized = false;
			windowFlags_.set_flag(WindowFlag::windowedRestorePending, true);
			windowFlags_.set_flag(WindowFlag::fullscreenTransition, true);
			glfwRestoreWindow(window);
			windowFlags_.set_flag(WindowFlag::fullscreenTransition, false);
			if (const auto status = nativeOperationResult(); !status) {
				windowFlags_.set_flag(WindowFlag::windowedRestorePending, false);
				return status;
			}
			completeWindowedRestore();
		} else {
			rememberWindowedGeometry();
			glfwMaximizeWindow(window);
		}
		return nativeOperationResult();
	}

	/** Toggle fullscreen and restore the retained windowed rectangle. */
	[[nodiscard]] Status toggleWindowFullscreen() noexcept {
		if (const auto status = validateWindowOperation(); !status)
			return status;
		if (windowFlags_.has_flag(WindowFlag::windowedRestorePending))
			completeWindowedRestore();
		if (windowFlags_.has_flag(WindowFlag::windowedRestorePending))
			return unexpectedError(
				makeError(ErrorCode::WindowOperationFailed, ErrorSite::WindowOperation));
		glfwGetError(nullptr);
		GLFWmonitor* currentMonitor = glfwGetWindowMonitor(window);
		if (currentMonitor || windowFlags_.has_flag(WindowFlag::fullscreenRestoreAvailable)) {
			// GLFW can detach a fullscreen window when its monitor disappears. The retained
			// snapshot still represents an unfinished fullscreen round trip in that case.
			WindowedState restoredState = fullscreenRestoreState_;
			GLFWmonitor* restoreMonitor = selectMonitor(restoredState);
			if (!restoreMonitor) {
				return unexpectedError(makeError(ErrorCode::PlatformMonitorUnavailable,
												 ErrorSite::WindowMonitorSelect));
			}
			if (monitorOverlap(restoredState, restoreMonitor) == 0) {
				centerOnMonitor(restoredState, restoreMonitor);
			}
			if (const auto status = nativeOperationResult(); !status)
				return status;
			windowFlags_.set_flag(WindowFlag::fullscreenTransition, true);
			glfwSetWindowMonitor(window, nullptr, restoredState.positionX, restoredState.positionY,
								 restoredState.width, restoredState.height, GLFW_DONT_CARE);
			windowFlags_.set_flag(WindowFlag::fullscreenTransition, false);
			if (const auto status = nativeOperationResult(); !status)
				return status;
			if (glfwGetWindowMonitor(window)) {
				return unexpectedError(
					makeError(ErrorCode::WindowOperationFailed, ErrorSite::WindowOperation));
			}
			windowedState_ = restoredState;
			windowFlags_.set_flag(WindowFlag::windowedRestorePending, !restoredState.maximized);
			windowFlags_.set_flag(WindowFlag::fullscreenTransition, true);
			if (restoredState.maximized) {
				glfwMaximizeWindow(window);
			} else if (glfwGetWindowAttrib(window, GLFW_MAXIMIZED) == GLFW_TRUE) {
				glfwRestoreWindow(window);
			}
			windowFlags_.set_flag(WindowFlag::fullscreenTransition, false);
			if (const auto status = nativeOperationResult(); !status) {
				windowFlags_.set_flag(WindowFlag::windowedRestorePending, false);
				return status;
			}
			windowFlags_.set_flag(WindowFlag::fullscreenRestoreAvailable, false);
			completeWindowedRestore();
			return {};
		}

		rememberWindowedGeometry();
		WindowedState retainedState = windowedState_;
		retainedState.maximized = glfwGetWindowAttrib(window, GLFW_MAXIMIZED) == GLFW_TRUE;
		GLFWmonitor* targetMonitor = selectMonitor(retainedState);
		if (!targetMonitor) {
			return unexpectedError(
				makeError(ErrorCode::PlatformMonitorUnavailable, ErrorSite::WindowMonitorSelect));
		}
		const GLFWvidmode* targetMode = glfwGetVideoMode(targetMonitor);
		if (!targetMode || targetMode->width <= 0 || targetMode->height <= 0) {
			return unexpectedError(
				makeError(ErrorCode::PlatformMonitorUnavailable, ErrorSite::WindowMonitorSelect));
		}
		if (const auto status = nativeOperationResult(); !status)
			return status;
		windowFlags_.set_flag(WindowFlag::fullscreenTransition, true);
		glfwSetWindowMonitor(window, targetMonitor, 0, 0, targetMode->width, targetMode->height,
							 targetMode->refreshRate);
		const auto transitionStatus = nativeOperationResult();
		if (!transitionStatus || glfwGetWindowMonitor(window) != targetMonitor) {
			// Do not publish a fullscreen snapshot after a failed native transition.
			// A partially applied backend transition is returned to its windowed state.
			if (glfwGetWindowMonitor(window)) {
				glfwSetWindowMonitor(window, nullptr, retainedState.positionX,
									 retainedState.positionY, retainedState.width,
									 retainedState.height, GLFW_DONT_CARE);
				if (retainedState.maximized)
					glfwMaximizeWindow(window);
				const auto rollbackStatus = nativeOperationResult();
				if (!rollbackStatus || glfwGetWindowMonitor(window)) {
					// Preserve recovery data even when the failed transition cannot be
					// rolled back. The caller still receives the original failure.
					fullscreenRestoreState_ = retainedState;
					windowFlags_.set_flag(WindowFlag::fullscreenRestoreAvailable, true);
				}
			}
			windowFlags_.set_flag(WindowFlag::fullscreenTransition, false);
			if (!transitionStatus)
				return transitionStatus;
			return unexpectedError(
				makeError(ErrorCode::WindowOperationFailed, ErrorSite::WindowOperation));
		}
		windowFlags_.set_flag(WindowFlag::fullscreenTransition, false);
		fullscreenRestoreState_ = retainedState;
		windowFlags_.set_flag(WindowFlag::fullscreenRestoreAvailable, true);
		return {};
	}
	/** Observe the logical native window extent. */
	[[nodiscard]] VkExtent2D windowExtent() const noexcept {
		if (!validate())
			return {};
		int width = 0;
		int height = 0;
		glfwGetWindowSize(window, &width, &height);
		VkExtent2D extent{};
		extent.width = static_cast<uint32_t>(std::max(0, width));
		extent.height = static_cast<uint32_t>(std::max(0, height));
		return extent;
	}

	/** Observe the framebuffer extent used for presentation. */
	[[nodiscard]] VkExtent2D framebufferExtent() const noexcept {
		if (!validate())
			return {};
		int width = 0;
		int height = 0;
		glfwGetFramebufferSize(window, &width, &height);
		VkExtent2D extent{};
		extent.width = static_cast<uint32_t>(std::max(0, width));
		extent.height = static_cast<uint32_t>(std::max(0, height));
		return extent;
	}

	/** Change the native title immediately. */
	[[nodiscard]] Status setTitle(std::string_view title) {
		if (const auto status = validate(); !status)
			return status;
		try {
			const std::string terminatedTitle(title);
			glfwGetError(nullptr);
			glfwSetWindowTitle(window, terminatedTitle.c_str());
			return GlfwRuntime::nativeResult(ErrorSite::WindowSetTitle);
		} catch (const std::bad_alloc&) {
			return unexpectedError(
				makeError(ErrorCode::AllocationFailed, ErrorSite::WindowSetTitle));
		}
	}
	/** Request native visibility. */
	[[nodiscard]] Status setVisible(bool visible) noexcept {
		if (const auto status = validate(); !status)
			return status;
		glfwGetError(nullptr);
		if (visible)
			glfwShowWindow(window);
		else
			glfwHideWindow(window);
		return GlfwRuntime::nativeResult();
	}
	/** Request focus; OS acceptance remains observable separately. */
	[[nodiscard]] Status requestFocus() noexcept {
		if (const auto status = validate(); !status)
			return status;
		glfwGetError(nullptr);
		glfwFocusWindow(window);
		return GlfwRuntime::nativeResult();
	}
	/** Observe native focus. */
	[[nodiscard]] bool isFocused() const noexcept {
		return window && glfwGetWindowAttrib(window, GLFW_FOCUSED) == GLFW_TRUE;
	}
	/** Observe native visibility. */
	[[nodiscard]] bool isVisible() const noexcept {
		return window && glfwGetWindowAttrib(window, GLFW_VISIBLE) == GLFW_TRUE;
	}
	/** Observe native minimization. */
	[[nodiscard]] bool isMinimized() const noexcept {
		return window && glfwGetWindowAttrib(window, GLFW_ICONIFIED) == GLFW_TRUE;
	}
	/** Supply a Vulkan presentation surface for the current renderer. */
	[[nodiscard]] Result<VkSurfaceKHR> createSurface(VkInstance instance) noexcept {
		if (const auto status = validate(); !status)
			return unexpectedError(status.error());
		if (instance == VK_NULL_HANDLE)
			return unexpectedError(
				makeError(ErrorCode::ObjectNotInitialized, ErrorSite::WindowSurfaceCreate));
		return createGlfwVulkanSurface(window, instance);
	}
};
} // namespace FlowUi::detail::platform
