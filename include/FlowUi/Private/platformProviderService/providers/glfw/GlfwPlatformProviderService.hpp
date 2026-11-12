#pragma once
#include "FlowUi/Private/platformProviderService/providers/glfw/GlfwEventBinding.hpp"
namespace FlowUi::detail::platform {
/** Stable native state; reverse destruction detaches callbacks before input/window release. */
class GlfwWindowState {
public:
	GlfwWindowState() noexcept = default;
	GlfwWindowState(const GlfwWindowState&) = delete;
	GlfwWindowState& operator=(const GlfwWindowState&) = delete;

private:
	friend class GlfwPlatformProviderService;
	GlfwWindow window_;
	GlfwWindowInput input_;
	GlfwEventBinding binding_;
};
/** Default complete native coordinator. Core never assembles its focused components. */
class GlfwPlatformProviderService {
public:
	using PlatformRuntime = GlfwRuntime;
	using PlatformWindow = GlfwWindow;
	using PlatformWindowInput = GlfwWindowInput;
	using EventBinding = GlfwEventBinding;
	using WindowState = GlfwWindowState;
	static constexpr std::uint32_t contractRevision = platformContractRevision;
	GlfwPlatformProviderService() noexcept = default;
	GlfwPlatformProviderService(const GlfwPlatformProviderService&) = delete;
	GlfwPlatformProviderService& operator=(const GlfwPlatformProviderService&) = delete;
	/** Acquire the shared platform runtime. */
	[[nodiscard]] Status initialize(const PlatformConfig& config) noexcept {
		return runtime_.initialize(config);
	}
	/** Pump native events once on the owner thread. */
	[[nodiscard]] Status pollEvents() noexcept { return runtime_.pollEvents(); }
	/** Release the runtime once every window has been shut down. */
	void shutdown() noexcept { runtime_.shutdown(); }
	/** Observe initialized runtime state. */
	[[nodiscard]] bool ready() const noexcept { return runtime_.ready(); }
	/** Observe a usable native suite belonging to this coordinator. */
	[[nodiscard]] bool windowReady(const WindowState& state) const noexcept {
		return state.window_.owner() == &runtime_ && state.window_.ready() &&
			   state.input_.ready() && state.binding_.attached();
	}
	/** Reject cross-runtime, detached and wrong-thread requests before mutation. */
	[[nodiscard]] Status validateWindowOwner(const WindowState& state) const noexcept {
		if (const auto status = runtime_.validate(); !status)
			return status;
		if (state.window_.owner() != &runtime_)
			return unexpectedError(makeError(state.window_.ready()
												 ? ErrorCode::InternalInvariantBroken
												 : ErrorCode::ObjectNotInitialized,
											 ErrorSite::WindowOperation));
		return state.window_.validate();
	}
	/** Acquire a complete native suite transactionally at its final address. */
	[[nodiscard]] Status initializeWindow(WindowState& state, WindowId target,
										  const WindowConfig& config, InputSink& sink) noexcept {
		if (const auto status = runtime_.validate(); !status)
			return status;
		if (state.window_.ready() || state.input_.ready() || state.binding_.attached())
			return unexpectedError(
				makeError(ErrorCode::ObjectAlreadyInitialized, ErrorSite::WindowCreate));
		if (target == InvalidWindowId)
			return unexpectedError(makeError(ErrorCode::InvalidWindowId, ErrorSite::WindowCreate));
		if (const auto status = state.window_.initialize(runtime_, config); !status)
			return status;
		if (const auto status = state.input_.initialize(state.window_, config.input); !status) {
			shutdownWindow(state);
			return status;
		}
		if (const auto status = state.binding_.attach(state.window_, state.input_, sink, target);
			!status) {
			shutdownWindow(state);
			return status;
		}
		return {};
	}
	/** Refresh this window's normalized input without a second native pump. */
	[[nodiscard]] Status refreshInput(WindowState& state) noexcept {
		if (const auto status = validateWindowOwner(state); !status)
			return status;
		return state.input_.refresh();
	}
	/** Stop native deliveries while semantic endpoints are still alive. */
	void quiesceWindow(WindowState& state) noexcept {
		if (!state.window_.ready())
			return;
		if (const auto status = validateWindowOwner(state); !status) {
			report(status);
			return;
		}
		state.binding_.detach();
	}
	/** Release native input and window after external surface destruction. */
	void shutdownWindow(WindowState& state) noexcept {
		if (!state.window_.ready())
			return;
		if (const auto status = validateWindowOwner(state); !status) {
			report(status);
			return;
		}
		quiesceWindow(state);
		state.input_.shutdown();
		state.window_.shutdown();
	}
	/** Borrow the renderer's required extension names until instance creation. */
	[[nodiscard]] Result<std::vector<const char*>> requiredInstanceExtensions() const {
		return runtime_.requiredInstanceExtensions();
	}
	/** Create the renderer's native Vulkan surface. */
	[[nodiscard]] Result<VkSurfaceKHR> createSurface(WindowState& state, VkInstance instance) {
		if (const auto status = validateWindowOwner(state); !status)
			return unexpectedError(status.error());
		return state.window_.createSurface(instance);
	}
	/** Forward requestWindowClose through validated concrete native state. */
	[[nodiscard]] Status requestWindowClose(WindowState& state) noexcept {
		if (const auto status = validateWindowOwner(state); !status)
			return status;
		return state.window_.requestWindowClose();
	}
	/** Forward minimizeWindow through validated concrete native state. */
	[[nodiscard]] Status minimizeWindow(WindowState& state) noexcept {
		if (const auto status = validateWindowOwner(state); !status)
			return status;
		return state.window_.minimizeWindow();
	}
	/** Forward toggleWindowMaximized through validated concrete native state. */
	[[nodiscard]] Status toggleWindowMaximized(WindowState& state) noexcept {
		if (const auto status = validateWindowOwner(state); !status)
			return status;
		return state.window_.toggleWindowMaximized();
	}
	/** Forward toggleWindowFullscreen through validated concrete native state. */
	[[nodiscard]] Status toggleWindowFullscreen(WindowState& state) noexcept {
		if (const auto status = validateWindowOwner(state); !status)
			return status;
		return state.window_.toggleWindowFullscreen();
	}
	/** Forward setTitle through validated concrete native state. */
	[[nodiscard]] Status setTitle(WindowState& state, std::string_view text) {
		if (const auto status = validateWindowOwner(state); !status)
			return status;
		return state.window_.setTitle(text);
	}
	/** Forward setInputConfig through validated concrete native state. */
	[[nodiscard]] Status setInputConfig(WindowState& state, const WindowInputConfig& config) {
		if (const auto status = validateWindowOwner(state); !status)
			return status;
		return state.input_.setInputConfig(config);
	}
	/** Forward setClipboardText through validated concrete native state. */
	[[nodiscard]] Status setClipboardText(WindowState& state, std::string_view text) {
		if (const auto status = validateWindowOwner(state); !status)
			return status;
		return state.input_.setClipboardText(text);
	}
	/** Forward setCursor through validated concrete native state. */
	[[nodiscard]] Status setCursor(WindowState& state, CursorType cursor) {
		if (const auto status = validateWindowOwner(state); !status)
			return status;
		return state.input_.setCursor(cursor);
	}
	/** Forward setVisible through validated concrete native state. */
	[[nodiscard]] Status setVisible(WindowState& state, bool visible) noexcept {
		if (const auto status = validateWindowOwner(state); !status)
			return status;
		return state.window_.setVisible(visible);
	}
	/** Forward requestFocus through validated concrete native state. */
	[[nodiscard]] Status requestFocus(WindowState& state) noexcept {
		if (const auto status = validateWindowOwner(state); !status)
			return status;
		return state.window_.requestFocus();
	}
	/** Observe shouldClose for this coordinator's native window. */
	[[nodiscard]] bool shouldClose(const WindowState& state) const noexcept {
		if (!validateWindowOwner(state))
			return true;
		return state.window_.shouldClose();
	}
	/** Observe windowOperationCapabilities for this coordinator's native window. */
	[[nodiscard]] WindowOperationCapabilities
	windowOperationCapabilities(const WindowState& state) const noexcept {
		if (!validateWindowOwner(state))
			return {};
		return state.window_.windowOperationCapabilities();
	}
	/** Observe windowExtent for this coordinator's native window. */
	[[nodiscard]] VkExtent2D windowExtent(const WindowState& state) const noexcept {
		if (!validateWindowOwner(state))
			return {};
		return state.window_.windowExtent();
	}
	/** Observe framebufferExtent for this coordinator's native window. */
	[[nodiscard]] VkExtent2D framebufferExtent(const WindowState& state) const noexcept {
		if (!validateWindowOwner(state))
			return {};
		return state.window_.framebufferExtent();
	}
	/** Observe inputConfig for this coordinator's native window. */
	[[nodiscard]] WindowInputConfig inputConfig(const WindowState& state) const noexcept {
		if (!validateWindowOwner(state))
			return {};
		return state.input_.inputConfig();
	}
	/** Observe supportsRawMouseMotion for this coordinator's native window. */
	[[nodiscard]] bool supportsRawMouseMotion(const WindowState& state) const noexcept {
		if (!validateWindowOwner(state))
			return false;
		return state.input_.supportsRawMouseMotion();
	}
	/** Observe hasClipboardAccess for this coordinator's native window. */
	[[nodiscard]] bool hasClipboardAccess(const WindowState& state) const noexcept {
		if (!validateWindowOwner(state))
			return false;
		return state.input_.hasClipboardAccess();
	}
	/** Observe isFocused for this coordinator's native window. */
	[[nodiscard]] bool isFocused(const WindowState& state) const noexcept {
		if (!validateWindowOwner(state))
			return false;
		return state.window_.isFocused();
	}
	/** Observe isVisible for this coordinator's native window. */
	[[nodiscard]] bool isVisible(const WindowState& state) const noexcept {
		if (!validateWindowOwner(state))
			return false;
		return state.window_.isVisible();
	}
	/** Observe isMinimized for this coordinator's native window. */
	[[nodiscard]] bool isMinimized(const WindowState& state) const noexcept {
		if (!validateWindowOwner(state))
			return false;
		return state.window_.isMinimized();
	}
	/** Set or clear closure while retaining native resource ownership. */
	void setShouldClose(WindowState& state, bool value) noexcept {
		if (const auto status = validateWindowOwner(state); !status) {
			report(status);
			return;
		}
		state.window_.setShouldClose(value);
	}
	/** Borrow the native interop handle; core must not interpret it. */
	[[nodiscard]] void* nativeHandle(const WindowState& state) const noexcept {
		return validateWindowOwner(state) ? state.window_.native() : nullptr;
	}
	/** Read synchronous clipboard text while retaining failure/empty distinction. */
	[[nodiscard]] Result<std::string> clipboardText(const WindowState& state) const {
		if (const auto status = validateWindowOwner(state); !status)
			return unexpectedError(status.error());
		return state.input_.clipboardText();
	}

private:
	static void report(const Status& status) noexcept {
		reportErrorEvent(ErrorEventView{.error = status.error(), .kind = ErrorEventKind::Reported});
	}
	GlfwRuntime runtime_;
};
} // namespace FlowUi::detail::platform
