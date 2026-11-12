#pragma once

#include "FlowUi/Private/platformProviderService/providers/glfw/GlfwWindowInput.hpp"

namespace FlowUi::detail::platform {
/** Sole owner of a native window's GLFW callback slots and user pointer. */
class GlfwEventBinding {
public:
	GlfwEventBinding() noexcept = default;
	~GlfwEventBinding() noexcept { detach(); }
	GlfwEventBinding(const GlfwEventBinding&) = delete;
	GlfwEventBinding& operator=(const GlfwEventBinding&) = delete;
	/** Attach exactly compatible initialized native endpoints to a stable semantic sink. */
	[[nodiscard]] Status attach(GlfwWindow& window, GlfwWindowInput& input, InputSink& sink,
								WindowId target) noexcept {
		if (window_)
			return unexpectedError(
				makeError(ErrorCode::ObjectAlreadyInitialized, ErrorSite::WindowPollInput));
		if (const auto status = window.validate(); !status)
			return status;
		if (input.window() != &window || !input.ready() ||
			glfwGetWindowUserPointer(window.native()))
			return unexpectedError(
				makeError(ErrorCode::InternalInvariantBroken, ErrorSite::WindowPollInput));
		window_ = &window;
		input_ = &input;
		input.sink_ = &sink;
		input.target_ = target;
		auto* native = window.native();
		glfwSetWindowUserPointer(native, this);
		glfwSetWindowPosCallback(native, [](GLFWwindow* native, int, int) noexcept {
			if (auto* binding = from(native))
				binding->geometryChanged();
		});
		glfwSetWindowSizeCallback(native, [](GLFWwindow* native, int, int) noexcept {
			if (auto* binding = from(native))
				binding->geometryChanged();
		});
		glfwSetWindowMaximizeCallback(native, [](GLFWwindow* native, int) noexcept {
			if (auto* binding = from(native))
				binding->window_->completeWindowedRestore();
		});
		glfwSetCursorPosCallback(
			native, [](GLFWwindow* native, double positionX, double positionY) noexcept {
				if (auto* binding = from(native))
					GlfwWindowInput::emit(binding->input_->sink_->pointerMove(
						{.device = 1u,
						 .target = binding->input_->target_,
						 .x = static_cast<float>(positionX),
						 .y = static_cast<float>(positionY),
						 .inside = glfwGetWindowAttrib(native, GLFW_HOVERED) == GLFW_TRUE}));
			});
		glfwSetCursorEnterCallback(native, [](GLFWwindow* native, int) noexcept {
			if (auto* binding = from(native))
				GlfwWindowInput::emit(binding->input_->refresh());
		});
		glfwSetMouseButtonCallback(
			native, [](GLFWwindow* native, int button, int action, int) noexcept {
				if (auto* binding = from(native))
					binding->input_->pointerButton(button, action != GLFW_RELEASE);
			});
		glfwSetScrollCallback(
			native, [](GLFWwindow* native, double horizontal, double vertical) noexcept {
				if (auto* binding = from(native))
					GlfwWindowInput::emit(
						binding->input_->sink_->scroll({.device = 1u,
														.target = binding->input_->target_,
														.x = static_cast<float>(horizontal),
														.y = static_cast<float>(vertical)}));
			});
		glfwSetKeyCallback(
			native, [](GLFWwindow* native, int key, int, int action, int modifiers) noexcept {
				if (auto* binding = from(native); binding && action != GLFW_REPEAT) {
					const auto modifierBits =
						static_cast<std::uint8_t>(((modifiers & GLFW_MOD_CONTROL) ? 1u : 0u) |
												  ((modifiers & GLFW_MOD_SHIFT) ? 2u : 0u) |
												  ((modifiers & GLFW_MOD_ALT) ? 4u : 0u) |
												  ((modifiers & GLFW_MOD_SUPER) ? 8u : 0u));
					binding->input_->key(key, action != GLFW_RELEASE, modifierBits);
				}
			});
		glfwSetCharCallback(native, [](GLFWwindow* native, unsigned int codepoint) noexcept {
			if (auto* binding = from(native))
				GlfwWindowInput::emit(
					binding->input_->sink_->text({.device = 1u,
												  .target = binding->input_->target_,
												  .codepoint = static_cast<char32_t>(codepoint)}));
		});
		glfwSetWindowFocusCallback(native, [](GLFWwindow* native, int focused) noexcept {
			if (auto* binding = from(native))
				binding->input_->focus(focused == GLFW_TRUE);
		});
		if (const auto status = GlfwRuntime::nativeResult(ErrorSite::WindowPollInput); !status) {
			detach();
			return status;
		}
		input.focus(window.isFocused());
		return {};
	}
	/** Detach callbacks before either endpoint or sink can be destroyed. */
	void detach() noexcept {
		if (!window_)
			return;
		if (const auto status = window_->validate(); !status) {
			GlfwWindowInput::emit(status);
			return;
		}
		auto* native = window_->native();
		if (native) {
			glfwSetWindowUserPointer(native, nullptr);
			glfwSetWindowPosCallback(native, nullptr);
			glfwSetWindowSizeCallback(native, nullptr);
			glfwSetWindowMaximizeCallback(native, nullptr);
			glfwSetCursorPosCallback(native, nullptr);
			glfwSetCursorEnterCallback(native, nullptr);
			glfwSetMouseButtonCallback(native, nullptr);
			glfwSetScrollCallback(native, nullptr);
			glfwSetKeyCallback(native, nullptr);
			glfwSetCharCallback(native, nullptr);
			glfwSetWindowFocusCallback(native, nullptr);
		}
		input_->sink_ = nullptr;
		input_ = nullptr;
		window_ = nullptr;
	}
	/** Observe live callback attachment. */
	[[nodiscard]] bool attached() const noexcept { return window_ != nullptr; }

private:
	[[nodiscard]] static GlfwEventBinding* from(GLFWwindow* native) noexcept {
		return static_cast<GlfwEventBinding*>(glfwGetWindowUserPointer(native));
	}
	void geometryChanged() noexcept {
		if (window_->windowFlags_.has_flag(GlfwWindow::WindowFlag::windowedRestorePending))
			window_->completeWindowedRestore();
		else
			window_->rememberWindowedGeometry();
	}
	GlfwWindow* window_ = nullptr;
	GlfwWindowInput* input_ = nullptr;
};
} // namespace FlowUi::detail::platform
