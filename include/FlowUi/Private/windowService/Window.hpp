#pragma once

#include "FlowUi/Private/utility/EnumFlags.hpp"

#include <algorithm>
#include <array>
#include <memory>
#include <stdexcept>
#include <string>
#include <string_view>
#include <thread>
#include <vector>

#include "FlowUi/Public/configurationStructs/InputConfig.hpp"
#include "FlowUi/Public/configurationStructs/WindowConfig.hpp"
#include "FlowUi/Public/errorManagement/ErrorCodeTypes.hpp"
#include "FlowUi/Public/errorManagement/ErrorEvents.hpp"
#include "FlowUi/Public/errorManagement/ErrorSites.hpp"
#include "FlowUi/Public/errorManagement/ErrorValue.hpp"
#include "FlowUi/Public/errorManagement/Exception.hpp"
#include "FlowUi/Public/managerStructs/InputTypes.hpp"
#include "IWindow.hpp"
#include "FlowUi/Private/inputService/InputQueue.hpp"

#ifndef GLFW_INCLUDE_VULKAN
#define GLFW_INCLUDE_VULKAN
#endif
#include <GLFW/glfw3.h>

namespace FlowUi::detail {

inline void glfwErrorCallback(int code, const char* description) noexcept {
	reportErrorEvent(ErrorEventView{
		.error = makeError(
			ErrorCode::None, ErrorSite::WindowBackendDiagnostic,
			0u, 0u, static_cast<std::uint32_t>(code)),
		.nativeMessage = description ? std::string_view{description} : std::string_view{},
		.kind = ErrorEventKind::BackendDiagnostic,
	});
}

inline int toGlfwCursorMode(FlowUi::CursorMode cursorMode) {
	switch (cursorMode) {
		case FlowUi::CursorMode::Hidden:
			return GLFW_CURSOR_HIDDEN;
		case FlowUi::CursorMode::Disabled:
			return GLFW_CURSOR_DISABLED;
		case FlowUi::CursorMode::Normal:
		default:
			return GLFW_CURSOR_NORMAL;
	}
}

inline int toGlfwCursorShape(FlowUi::CursorType cursorType) {
	switch (cursorType) {
		case FlowUi::CursorType::IBeam:
			return GLFW_IBEAM_CURSOR;
		case FlowUi::CursorType::Crosshair:
			return GLFW_CROSSHAIR_CURSOR;
		case FlowUi::CursorType::PointingHand:
			return GLFW_HAND_CURSOR;
		case FlowUi::CursorType::ResizeHorizontal:
#ifdef GLFW_RESIZE_EW_CURSOR
			return GLFW_RESIZE_EW_CURSOR;
#else
			return GLFW_HRESIZE_CURSOR;
#endif
		case FlowUi::CursorType::ResizeVertical:
#ifdef GLFW_RESIZE_NS_CURSOR
			return GLFW_RESIZE_NS_CURSOR;
#else
			return GLFW_VRESIZE_CURSOR;
#endif
		case FlowUi::CursorType::ResizeDiagonalTL:
#ifdef GLFW_RESIZE_NWSE_CURSOR
			return GLFW_RESIZE_NWSE_CURSOR;
#else
			return GLFW_HRESIZE_CURSOR;
#endif
		case FlowUi::CursorType::ResizeDiagonalTR:
#ifdef GLFW_RESIZE_NESW_CURSOR
			return GLFW_RESIZE_NESW_CURSOR;
#else
			return GLFW_HRESIZE_CURSOR;
#endif
		case FlowUi::CursorType::ResizeAll:
#ifdef GLFW_RESIZE_ALL_CURSOR
			return GLFW_RESIZE_ALL_CURSOR;
#else
			return GLFW_CROSSHAIR_CURSOR;
#endif
		case FlowUi::CursorType::NotAllowed:
#ifdef GLFW_NOT_ALLOWED_CURSOR
			return GLFW_NOT_ALLOWED_CURSOR;
#else
			return GLFW_ARROW_CURSOR;
#endif
		case FlowUi::CursorType::Wait:
			return GLFW_ARROW_CURSOR; // Placeholder mapping
		case FlowUi::CursorType::Progress:
			return GLFW_ARROW_CURSOR; // Placeholder mapping
		case FlowUi::CursorType::Grab:
			return GLFW_HAND_CURSOR; // Placeholder mapping
		case FlowUi::CursorType::Grabbing:
			return GLFW_HAND_CURSOR; // Placeholder mapping
		case FlowUi::CursorType::Custom:
			return GLFW_ARROW_CURSOR; // Placeholder mapping
		case FlowUi::CursorType::Default:
		case FlowUi::CursorType::Arrow:
		default:
			return GLFW_ARROW_CURSOR;
	}
}

struct GlfwLibrary {
	GlfwLibrary() {
		if (refCount++ == 0) {
			glfwSetErrorCallback(glfwErrorCallback);
			if (!glfwInit()) {
				refCount = 0;
				throw FlowUiException(makeError(ErrorCode::PlatformInitializationFailed, ErrorSite::WindowBackendInitialize));
			}
		}
	}

	~GlfwLibrary() {
		if (--refCount == 0) {
			glfwTerminate();
		}
	}

	inline static int refCount = 0;
};

class GlfwWindowBackend final : public IWindowBackend {

private:
	struct WindowedState {
		int position_x = 0;
		int position_y = 0;
		int width = 1280;
		int height = 720;
		bool maximized = false;
	};

	[[nodiscard]] Status validate_window_operation() const noexcept {
		if (std::this_thread::get_id() != platform_thread_) {
			return unexpectedError(makeError(ErrorCode::WrongThread, ErrorSite::WindowOperation));
		}
		if (!window) {
			return unexpectedError(
				makeError(ErrorCode::ObjectNotInitialized, ErrorSite::WindowOperation));
		}
		return {};
	}

	[[nodiscard]] static Status native_operation_result() noexcept {
		const int native_error = glfwGetError(nullptr);
		if (native_error != GLFW_NO_ERROR) {
			return unexpectedError(makeError(ErrorCode::WindowOperationFailed,
											 ErrorSite::WindowOperation, 0u, 0u,
											 static_cast<std::uint32_t>(native_error)));
		}
		return {};
	}

	void remember_windowed_geometry() noexcept {
		if (!window || window_flags_.has_flag(WindowFlag::fullscreen_transition) ||
			window_flags_.has_flag(WindowFlag::fullscreen_restore_available) ||
			window_flags_.has_flag(WindowFlag::windowed_restore_pending) || glfwGetWindowMonitor(window) ||
			glfwGetWindowAttrib(window, GLFW_MAXIMIZED) == GLFW_TRUE ||
			glfwGetWindowAttrib(window, GLFW_ICONIFIED) == GLFW_TRUE) {
			return;
		}
		glfwGetWindowPos(window, &windowed_state_.position_x, &windowed_state_.position_y);
		glfwGetWindowSize(window, &windowed_state_.width, &windowed_state_.height);
	}

	void complete_windowed_restore() noexcept {
		if (!window || !window_flags_.has_flag(WindowFlag::windowed_restore_pending) ||
			window_flags_.has_flag(WindowFlag::fullscreen_transition) ||
			window_flags_.has_flag(WindowFlag::restoring_windowed_geometry))
			return;
		glfwGetError(nullptr);
		const bool fullscreen = glfwGetWindowMonitor(window) != nullptr;
		const bool maximized = glfwGetWindowAttrib(window, GLFW_MAXIMIZED) == GLFW_TRUE;
		if (const auto status = native_operation_result(); !status) {
			window_flags_.set_flag(WindowFlag::windowed_restore_pending, false);
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
		window_flags_.set_flag(WindowFlag::restoring_windowed_geometry, true);
		glfwSetWindowPos(window, windowed_state_.position_x, windowed_state_.position_y);
		glfwSetWindowSize(window, windowed_state_.width, windowed_state_.height);
		window_flags_.set_flag(WindowFlag::restoring_windowed_geometry, false);
		if (const auto status = native_operation_result(); !status) {
			window_flags_.set_flag(WindowFlag::windowed_restore_pending, false);
			reportErrorEvent(
				ErrorEventView{.error = status.error(), .kind = ErrorEventKind::Reported});
			return;
		}
		int restored_x = 0;
		int restored_y = 0;
		int restored_width = 0;
		int restored_height = 0;
		glfwGetWindowPos(window, &restored_x, &restored_y);
		glfwGetWindowSize(window, &restored_width, &restored_height);
		if (const auto status = native_operation_result(); !status) {
			window_flags_.set_flag(WindowFlag::windowed_restore_pending, false);
			reportErrorEvent(
				ErrorEventView{.error = status.error(), .kind = ErrorEventKind::Reported});
			return;
		}
		window_flags_.set_flag(WindowFlag::windowed_restore_pending,
			restored_x != windowed_state_.position_x || restored_y != windowed_state_.position_y ||
			restored_width != windowed_state_.width || restored_height != windowed_state_.height);
	}

	[[nodiscard]] static std::int64_t monitor_overlap(const WindowedState& state,
													  GLFWmonitor* monitor) noexcept {
		int monitor_x = 0;
		int monitor_y = 0;
		glfwGetMonitorPos(monitor, &monitor_x, &monitor_y);
		const GLFWvidmode* mode = glfwGetVideoMode(monitor);
		if (!mode)
			return 0;
		const std::int64_t overlap_width = std::max<std::int64_t>(
			0, std::min<std::int64_t>(static_cast<std::int64_t>(state.position_x) + state.width,
									  static_cast<std::int64_t>(monitor_x) + mode->width) -
				   std::max(state.position_x, monitor_x));
		const std::int64_t overlap_height = std::max<std::int64_t>(
			0, std::min<std::int64_t>(static_cast<std::int64_t>(state.position_y) + state.height,
									  static_cast<std::int64_t>(monitor_y) + mode->height) -
				   std::max(state.position_y, monitor_y));
		return overlap_width * overlap_height;
	}

	[[nodiscard]] static GLFWmonitor* select_monitor(const WindowedState& state) noexcept {
		GLFWmonitor* selected_monitor = glfwGetPrimaryMonitor();
		std::int64_t greatest_overlap = 0;
		int monitor_count = 0;
		GLFWmonitor** monitors = glfwGetMonitors(&monitor_count);
		for (int monitor_index = 0; monitor_index < monitor_count; ++monitor_index) {
			const std::int64_t overlap = monitor_overlap(state, monitors[monitor_index]);
			if (overlap > greatest_overlap) {
				greatest_overlap = overlap;
				selected_monitor = monitors[monitor_index];
			}
		}
		return selected_monitor;
	}

	static void center_on_monitor(WindowedState& state, GLFWmonitor* monitor) noexcept {
		int work_x = 0;
		int work_y = 0;
		int work_width = 0;
		int work_height = 0;
		glfwGetMonitorWorkarea(monitor, &work_x, &work_y, &work_width, &work_height);
		if (work_width <= 0 || work_height <= 0)
			return;
		state.width = std::min(state.width, work_width);
		state.height = std::min(state.height, work_height);
		state.position_x = work_x + (work_width - state.width) / 2;
		state.position_y = work_y + (work_height - state.height) / 2;
	}

	GLFWcursor* acquireStandardCursor(FlowUi::CursorType cursorType) {
		const std::size_t cursorIndex = static_cast<std::size_t>(cursorType);
		if (cursorIndex >= standardCursors_.size()) {
			return nullptr;
		}

		GLFWcursor*& cachedCursor = standardCursors_[cursorIndex];
		if (cachedCursor) {
			return cachedCursor;
		}

		cachedCursor = glfwCreateStandardCursor(toGlfwCursorShape(cursorType));
		return cachedCursor;
	}

	void destroyStandardCursors() {
		for (GLFWcursor*& cursor : standardCursors_) {
			if (!cursor) {
				continue;
			}
			glfwDestroyCursor(cursor);
			cursor = nullptr;
		}
	}

	void installCallbacks() {
		glfwSetWindowPosCallback(window, [](GLFWwindow* native_window, int, int) noexcept {
			auto* backend =
				static_cast<GlfwWindowBackend*>(glfwGetWindowUserPointer(native_window));
			if (backend) {
				if (backend->window_flags_.has_flag(WindowFlag::windowed_restore_pending))
					backend->complete_windowed_restore();
				else
					backend->remember_windowed_geometry();
			}
		});
		glfwSetWindowSizeCallback(window, [](GLFWwindow* native_window, int, int) noexcept {
			auto* backend =
				static_cast<GlfwWindowBackend*>(glfwGetWindowUserPointer(native_window));
			if (backend) {
				if (backend->window_flags_.has_flag(WindowFlag::windowed_restore_pending))
					backend->complete_windowed_restore();
				else
					backend->remember_windowed_geometry();
			}
		});
		glfwSetWindowMaximizeCallback(window, [](GLFWwindow* native_window, int) noexcept {
			auto* backend =
				static_cast<GlfwWindowBackend*>(glfwGetWindowUserPointer(native_window));
			if (backend)
				backend->complete_windowed_restore();
		});
		glfwSetCursorPosCallback(window, [](GLFWwindow* win, double x, double y) noexcept {
			auto* self = static_cast<GlfwWindowBackend*>(glfwGetWindowUserPointer(win));
			if (self && self->input) {
				self->input->setMousePos(static_cast<float>(x), static_cast<float>(y));
			}
		});

		glfwSetMouseButtonCallback(window, [](GLFWwindow* win, int button, int action, int mods) noexcept {
			(void)mods;
			auto* self = static_cast<GlfwWindowBackend*>(glfwGetWindowUserPointer(win));
			if (self && self->input) {
				self->input->pushMouseButton(button, action != GLFW_RELEASE);
			}
		});

		glfwSetScrollCallback(window, [](GLFWwindow* win, double dx, double dy) noexcept {
			auto* self = static_cast<GlfwWindowBackend*>(glfwGetWindowUserPointer(win));
			if (self && self->input) {
				self->input->pushScroll(static_cast<float>(dx), static_cast<float>(dy));
			}
		});

		glfwSetKeyCallback(window, [](GLFWwindow* win, int key, int scancode, int action, int mods) noexcept {
			(void)scancode;
			(void)mods;
			auto* self = static_cast<GlfwWindowBackend*>(glfwGetWindowUserPointer(win));
			if (self && self->input) {
				self->input->pushKey(key, action != GLFW_RELEASE);
#if FLOW_UI_DEV_MODE
				if (action == GLFW_PRESS)
					self->input->note_dev_shortcut_press(
						key,
						((mods & GLFW_MOD_CONTROL) ? 1 : 0) | ((mods & GLFW_MOD_SHIFT) ? 2 : 0) |
							((mods & GLFW_MOD_ALT) ? 4 : 0) | ((mods & GLFW_MOD_SUPER) ? 8 : 0));
#endif
			}
		});

		glfwSetCharCallback(window, [](GLFWwindow* win, unsigned int codepoint) noexcept {
			auto* self = static_cast<GlfwWindowBackend*>(glfwGetWindowUserPointer(win));
			if (self && self->input) {
				self->input->pushChar(static_cast<char32_t>(codepoint));
			}
		});

		glfwSetWindowFocusCallback(window, [](GLFWwindow* win, int focused) noexcept {
			auto* self = static_cast<GlfwWindowBackend*>(glfwGetWindowUserPointer(win));
			if (self && self->input)
				self->input->setWindowFocused(focused == GLFW_TRUE);
			if (!self || !self->input || focused == GLFW_TRUE) {
				return;
			}
			self->input->clearKeyboardState();
			self->input->clearMouseButtonsState();
		});
	}

	GlfwLibrary library;
	std::array<GLFWcursor*, static_cast<std::size_t>(FlowUi::CursorType::Custom) + 1u> standardCursors_{};
	WindowedState windowed_state_{};
	WindowedState fullscreen_restore_state_{};
	std::thread::id platform_thread_ = std::this_thread::get_id();
	GLFWwindow* window = nullptr;
	FlowUi::CursorType currentCursorType_ = FlowUi::CursorType::Default;
	enum class WindowFlag : std::uint8_t {
		fullscreen_restore_available = 1u << 0u,
		fullscreen_transition = 1u << 1u,
		windowed_restore_pending = 1u << 2u,
		restoring_windowed_geometry = 1u << 3u,
	};
	EnumFlags<WindowFlag> window_flags_{};

public:
	explicit GlfwWindowBackend(const FlowUi::WindowConfig& config, InputQueue* inputQueue)
		: input(inputQueue) {
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
				throw FlowUiException(makeError(ErrorCode::PlatformMonitorUnavailable, ErrorSite::WindowMonitorSelect));
			}
			const GLFWvidmode* mode = glfwGetVideoMode(monitor);
			if (!mode) {
				throw FlowUiException(makeError(ErrorCode::PlatformMonitorUnavailable,
												ErrorSite::WindowMonitorSelect));
			}
			width = mode->width;
			height = mode->height;
		}

		window = glfwCreateWindow(width, height, config.title.c_str(), monitor, nullptr);
		if (!window) {
			throw FlowUiException(makeError(ErrorCode::WindowCreationFailed, ErrorSite::WindowCreate));
		}
		windowed_state_.width = std::max(1, config.width);
		windowed_state_.height = std::max(1, config.height);
		windowed_state_.maximized = config.maximized;
		if (config.fullscreen || config.maximized) {
			if (GLFWmonitor* fallback_monitor = glfwGetPrimaryMonitor()) {
				center_on_monitor(windowed_state_, fallback_monitor);
			}
		} else {
			remember_windowed_geometry();
		}
		if (config.fullscreen) {
			fullscreen_restore_state_ = windowed_state_;
			window_flags_.set_flag(WindowFlag::fullscreen_restore_available, true);
		}

		glfwSetWindowUserPointer(window, this);
		installCallbacks();
		setInputConfig(config.input);
		setCursorType(FlowUi::CursorType::Arrow);
	}

	~GlfwWindowBackend() override {
		detachCallbacks();
		destroyStandardCursors();
		if (window) {
			glfwDestroyWindow(window);
			window = nullptr;
		}
	}

	void detachCallbacks() noexcept override {
		input = nullptr;
		if (!window) return;
		glfwSetWindowUserPointer(window, nullptr);
		glfwSetWindowPosCallback(window, nullptr);
		glfwSetWindowSizeCallback(window, nullptr);
		glfwSetWindowMaximizeCallback(window, nullptr);
		glfwSetCursorPosCallback(window, nullptr);
		glfwSetMouseButtonCallback(window, nullptr);
		glfwSetScrollCallback(window, nullptr);
		glfwSetKeyCallback(window, nullptr);
		glfwSetCharCallback(window, nullptr);
		glfwSetWindowFocusCallback(window, nullptr);
	}

	void refreshInputSnapshot() override {
		if (!window || !input) {
			return;
		}

		double mouseX = 0.0;
		double mouseY = 0.0;
		glfwGetCursorPos(window, &mouseX, &mouseY);
		input->setMousePos(static_cast<float>(mouseX), static_cast<float>(mouseY));
		input->setPointerInside(glfwGetWindowAttrib(window, GLFW_HOVERED) == GLFW_TRUE);

		for (int mouseButton = 0; mouseButton < static_cast<int>(FrameInput::kMouseButtonCount); ++mouseButton) {
			const int buttonState = glfwGetMouseButton(window, mouseButton);
			input->pushMouseButton(mouseButton, buttonState == GLFW_PRESS);
		}
	}

	bool shouldClose() const override {
		return window ? glfwWindowShouldClose(window) == GLFW_TRUE : true;
	}

	void setShouldClose(int value) override {
		if (window) {
			glfwSetWindowShouldClose(window, value);
		}
	}

	[[nodiscard]] WindowOperationCapabilities
	window_operation_capabilities() const noexcept override {
		if (!window)
			return {};
		return {.close_window = true,
				.minimize = true,
				.maximize_restore = true,
				.toggle_fullscreen = true};
	}

	[[nodiscard]] Status request_window_close() noexcept override {
		if (const auto status = validate_window_operation(); !status)
			return status;
		glfwGetError(nullptr);
		glfwSetWindowShouldClose(window, GLFW_TRUE);
		return native_operation_result();
	}

	[[nodiscard]] Status minimize_window() noexcept override {
		if (const auto status = validate_window_operation(); !status)
			return status;
		glfwGetError(nullptr);
		glfwIconifyWindow(window);
		return native_operation_result();
	}

	[[nodiscard]] Status toggle_window_maximized() noexcept override {
		if (const auto status = validate_window_operation(); !status)
			return status;
		if (window_flags_.has_flag(WindowFlag::windowed_restore_pending))
			complete_windowed_restore();
		if (window_flags_.has_flag(WindowFlag::windowed_restore_pending))
			return unexpectedError(
				makeError(ErrorCode::WindowOperationFailed, ErrorSite::WindowOperation));
		glfwGetError(nullptr);
		if (window_flags_.has_flag(WindowFlag::fullscreen_restore_available) || glfwGetWindowMonitor(window)) {
			return unexpectedError(
				makeError(ErrorCode::WindowOperationUnsupported, ErrorSite::WindowOperation));
		}
		if (glfwGetWindowAttrib(window, GLFW_MAXIMIZED) == GLFW_TRUE) {
			windowed_state_.maximized = false;
			window_flags_.set_flag(WindowFlag::windowed_restore_pending, true);
			window_flags_.set_flag(WindowFlag::fullscreen_transition, true);
			glfwRestoreWindow(window);
			window_flags_.set_flag(WindowFlag::fullscreen_transition, false);
			if (const auto status = native_operation_result(); !status) {
				window_flags_.set_flag(WindowFlag::windowed_restore_pending, false);
				return status;
			}
			complete_windowed_restore();
		} else {
			remember_windowed_geometry();
			glfwMaximizeWindow(window);
		}
		return native_operation_result();
	}

	[[nodiscard]] Status toggle_window_fullscreen() noexcept override {
		if (const auto status = validate_window_operation(); !status)
			return status;
		if (window_flags_.has_flag(WindowFlag::windowed_restore_pending))
			complete_windowed_restore();
		if (window_flags_.has_flag(WindowFlag::windowed_restore_pending))
			return unexpectedError(
				makeError(ErrorCode::WindowOperationFailed, ErrorSite::WindowOperation));
		glfwGetError(nullptr);
		GLFWmonitor* current_monitor = glfwGetWindowMonitor(window);
		if (current_monitor || window_flags_.has_flag(WindowFlag::fullscreen_restore_available)) {
			// GLFW can detach a fullscreen window when its monitor disappears. The retained
			// snapshot still represents an unfinished fullscreen round trip in that case.
			WindowedState restored_state = fullscreen_restore_state_;
			GLFWmonitor* restore_monitor = select_monitor(restored_state);
			if (!restore_monitor) {
				return unexpectedError(makeError(ErrorCode::PlatformMonitorUnavailable,
												 ErrorSite::WindowMonitorSelect));
			}
			if (monitor_overlap(restored_state, restore_monitor) == 0) {
				center_on_monitor(restored_state, restore_monitor);
			}
			if (const auto status = native_operation_result(); !status)
				return status;
			window_flags_.set_flag(WindowFlag::fullscreen_transition, true);
			glfwSetWindowMonitor(window, nullptr, restored_state.position_x,
								 restored_state.position_y, restored_state.width,
								 restored_state.height, GLFW_DONT_CARE);
			window_flags_.set_flag(WindowFlag::fullscreen_transition, false);
			if (const auto status = native_operation_result(); !status)
				return status;
			if (glfwGetWindowMonitor(window)) {
				return unexpectedError(
					makeError(ErrorCode::WindowOperationFailed, ErrorSite::WindowOperation));
			}
			windowed_state_ = restored_state;
			window_flags_.set_flag(WindowFlag::windowed_restore_pending, !restored_state.maximized);
			window_flags_.set_flag(WindowFlag::fullscreen_transition, true);
			if (restored_state.maximized) {
				glfwMaximizeWindow(window);
			} else if (glfwGetWindowAttrib(window, GLFW_MAXIMIZED) == GLFW_TRUE) {
				glfwRestoreWindow(window);
			}
			window_flags_.set_flag(WindowFlag::fullscreen_transition, false);
			if (const auto status = native_operation_result(); !status) {
				window_flags_.set_flag(WindowFlag::windowed_restore_pending, false);
				return status;
			}
			window_flags_.set_flag(WindowFlag::fullscreen_restore_available, false);
			complete_windowed_restore();
			return {};
		}

		remember_windowed_geometry();
		WindowedState retained_state = windowed_state_;
		retained_state.maximized = glfwGetWindowAttrib(window, GLFW_MAXIMIZED) == GLFW_TRUE;
		GLFWmonitor* target_monitor = select_monitor(retained_state);
		if (!target_monitor) {
			return unexpectedError(
				makeError(ErrorCode::PlatformMonitorUnavailable, ErrorSite::WindowMonitorSelect));
		}
		const GLFWvidmode* target_mode = glfwGetVideoMode(target_monitor);
		if (!target_mode || target_mode->width <= 0 || target_mode->height <= 0) {
			return unexpectedError(
				makeError(ErrorCode::PlatformMonitorUnavailable, ErrorSite::WindowMonitorSelect));
		}
		if (const auto status = native_operation_result(); !status)
			return status;
		window_flags_.set_flag(WindowFlag::fullscreen_transition, true);
		glfwSetWindowMonitor(window, target_monitor, 0, 0, target_mode->width, target_mode->height,
							 target_mode->refreshRate);
		const auto transition_status = native_operation_result();
		if (!transition_status || glfwGetWindowMonitor(window) != target_monitor) {
			// Do not publish a fullscreen snapshot after a failed native transition.
			// A partially applied backend transition is returned to its windowed state.
			if (glfwGetWindowMonitor(window)) {
				glfwSetWindowMonitor(window, nullptr, retained_state.position_x,
									 retained_state.position_y, retained_state.width,
									 retained_state.height, GLFW_DONT_CARE);
				if (retained_state.maximized)
					glfwMaximizeWindow(window);
				const auto rollback_status = native_operation_result();
				if (!rollback_status || glfwGetWindowMonitor(window)) {
					// Preserve recovery data even when the failed transition cannot be
					// rolled back. The caller still receives the original failure.
					fullscreen_restore_state_ = retained_state;
					window_flags_.set_flag(WindowFlag::fullscreen_restore_available, true);
				}
			}
			window_flags_.set_flag(WindowFlag::fullscreen_transition, false);
			if (!transition_status)
				return transition_status;
			return unexpectedError(
				makeError(ErrorCode::WindowOperationFailed, ErrorSite::WindowOperation));
		}
		window_flags_.set_flag(WindowFlag::fullscreen_transition, false);
		fullscreen_restore_state_ = retained_state;
		window_flags_.set_flag(WindowFlag::fullscreen_restore_available, true);
		return {};
	}

	std::vector<const char*> requiredInstanceExtensions() const override {
		uint32_t count = 0;
		const char** extensions = glfwGetRequiredInstanceExtensions(&count);
		if (!extensions || count == 0) {
			throw FlowUiException(makeError(ErrorCode::PlatformRequiredExtensionMissing, ErrorSite::WindowRequiredExtensions));
		}
		return std::vector<const char*>(extensions, extensions + count);
	}

	VkSurfaceKHR createSurface(VkInstance instance) override {
		if (!window) {
			throw FlowUiException(makeError(ErrorCode::ObjectNotInitialized, ErrorSite::WindowSurfaceCreate));
		}
		VkSurfaceKHR surface = VK_NULL_HANDLE;
		const VkResult result = glfwCreateWindowSurface(instance, window, nullptr, &surface);
		if (result != VK_SUCCESS) {
			throw FlowUiException(makeError(
				ErrorCode::PlatformSurfaceCreationFailed, ErrorSite::WindowSurfaceCreate,
				0u,
				0u,
				static_cast<std::uint32_t>(result)));
		}
		return surface;
	}

	VkExtent2D windowExtent() const override {
		int width = 0;
		int height = 0;
		glfwGetWindowSize(window, &width, &height);
		VkExtent2D extent{};
		extent.width = static_cast<uint32_t>(std::max(0, width));
		extent.height = static_cast<uint32_t>(std::max(0, height));
		return extent;
	}

	VkExtent2D framebufferExtent() const override {
		int width = 0;
		int height = 0;
		glfwGetFramebufferSize(window, &width, &height);
		VkExtent2D extent{};
		extent.width = static_cast<uint32_t>(std::max(0, width));
		extent.height = static_cast<uint32_t>(std::max(0, height));
		return extent;
	}

	void setTitle(std::string_view title) override {
		if (window) {
			std::string tmp(title);
			glfwSetWindowTitle(window, tmp.c_str());
		}
	}

	void setInputConfig(const FlowUi::WindowInputConfig& config) override {
		inputConfig_ = config;
		if (!window) {
			return;
		}

		glfwSetInputMode(window, GLFW_CURSOR, toGlfwCursorMode(config.cursorMode));
		glfwSetInputMode(window, GLFW_STICKY_KEYS, config.stickyKeys ? GLFW_TRUE : GLFW_FALSE);
		glfwSetInputMode(window, GLFW_STICKY_MOUSE_BUTTONS, config.stickyMouseButtons ? GLFW_TRUE : GLFW_FALSE);
#ifdef GLFW_LOCK_KEY_MODS
		glfwSetInputMode(window, GLFW_LOCK_KEY_MODS, config.lockKeyMods ? GLFW_TRUE : GLFW_FALSE);
#else
		inputConfig_.lockKeyMods = false;
#endif
#ifdef GLFW_RAW_MOUSE_MOTION
		if (glfwRawMouseMotionSupported() == GLFW_TRUE) {
			glfwSetInputMode(window, GLFW_RAW_MOUSE_MOTION, config.rawMouseMotion ? GLFW_TRUE : GLFW_FALSE);
			inputConfig_.rawMouseMotion = config.rawMouseMotion;
		} else {
			inputConfig_.rawMouseMotion = false;
		}
#else
		inputConfig_.rawMouseMotion = false;
#endif
	}

	FlowUi::WindowInputConfig getInputConfig() const override {
		return inputConfig_;
	}

	void setCursorType(FlowUi::CursorType cursorType) override {
		if (!window || cursorType == currentCursorType_) {
			return;
		}

		if (cursorType == FlowUi::CursorType::Default) {
			glfwSetCursor(window, nullptr);
			currentCursorType_ = cursorType;
			return;
		}

		GLFWcursor* cursorHandle = acquireStandardCursor(cursorType);
		glfwSetCursor(window, cursorHandle);
		currentCursorType_ = cursorType;
	}

	bool supportsRawMouseMotion() const override {
#ifdef GLFW_RAW_MOUSE_MOTION
		return glfwRawMouseMotionSupported() == GLFW_TRUE;
#else
		return false;
#endif
	}

	void setClipboardText(std::string_view text) override {
		if (!window) {
			return;
		}
		std::string copy(text);
		glfwSetClipboardString(window, copy.c_str());
	}

	std::string getClipboardText() const override {
		if (!window) {
			return {};
		}
		const char* clipboard = glfwGetClipboardString(window);
		if (!clipboard) {
			return {};
		}
		return std::string(clipboard);
	}

	void* nativeHandle() const override { return window; }
	InputQueue* input = nullptr;
	FlowUi::WindowInputConfig inputConfig_{};
};

inline std::unique_ptr<IWindowBackend> makeDefaultWindowBackend(
	const FlowUi::WindowConfig& config,
	FlowUi::detail::InputQueue* inputQueue)
{
	return std::make_unique<FlowUi::detail::GlfwWindowBackend>(config, inputQueue);
}

inline void pollWindowSystemEvents() {
	glfwPollEvents();
}

} // namespace FlowUi::detail
