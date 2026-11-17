#pragma once

#include <concepts>
#include <cstddef>
#include <cstdint>
#include <functional>
#include <memory>
#include <span>
#include <stdexcept>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "FlowUi/Private/ApiBoundary.hpp"
#include "FlowUi/Public/BuildConfig.hpp"
#include "FlowUi/Public/Color.hpp"
#include "FlowUi/Public/configurationStructs/AppConfig.hpp"
#include "FlowUi/Public/elementAuthoring/ElementConcepts.hpp"
#include "FlowUi/Public/errorManagement/Result.hpp"
#include "FlowUi/Public/identificationTypes/ElementID.hpp"
#include "FlowUi/Public/identificationTypes/ResourceKey.hpp"
#include "FlowUi/Public/managerStructs/DevSnapshotTypes.hpp"
#include "FlowUi/Public/managerStructs/WindowOperationTypes.hpp"
#include "FlowUi/Public/managerStructs/WindowTypes.hpp"
#include "clay.h"
#include "devSystems/DevAppIntegration.hpp"

namespace FlowUi {

struct FontManager;

/** @addtogroup flowui_app
 * @{
 */

class UiManager;
template <FlowElement Element>
class ElementBuilder;
class ImageManager;
class ThemeManager;
class ElementManager;
class ActionManager;
#if FLOW_UI_AGENTIC_WORKFLOW
namespace devSystems::agentic {
class AgenticWorkflowService;
}
#endif

#if FLOWUI_INCLUDE_ICON_MANAGER
class IconManager;
#endif
#if FLOWUI_PUBLIC_VULKAN_INTEROP
class ViewPortManager;
#endif

/**
 * @brief Main FlowUi application object and owner of runtime managers.
 *
 * Create an initialized app with makeApplication(). Use it on its platform
 * thread and complete beginFrame(), endFrame(), drawFrame() for one window
 * before beginning another. No-argument window APIs select mainWindowId().
 * Manager references are borrowed from the app or the selected window and must
 * not be used after their owner is destroyed or replaced by move assignment.
 * App Result/Status operations contain operational failures and preserve their
 * FlowUiError. Ordinary builders, context access and action invocation instead
 * use direct values/references and propagate exceptions while poisoning a failed
 * composition frame. runFrame() owns build cancellation and callback recovery.
 *
 * @see @ref md_docs_2tutorials_2quick__start "Quick Start"
 * @see @ref md_docs_2concepts_2frame__lifecycle "Frame Lifecycle"
 * @see @ref md_docs_2concepts_2managers "Managers"
 */
class App {
public:
	/**
	 * @brief Construct an empty App handle without initializing a runtime.
	 *
	 * An empty handle can receive a later move assignment; makeApplication() creates an initialized application. Checked operations on an empty or moved-from App return AppUnavailable; hasWindow() is false and mainWindowId() is invalid.
	 * @see FlowUi::makeApplication()
	 *
	 * @par Example
	 * @code{.cpp}
	 * FlowUi::App emptyHandle{};
	 * @endcode
	 */
	App() noexcept;
	/**
	 * @brief Transfer the runtime and manager ownership; leave the source empty.
	 * @pre Perform ownership transfer while the App is quiescent, on its platform thread. Reacquire borrowed manager references through the destination after the move. Move assignment destroys the destination runtime first.
	 *
	 * @par Example
	 * @code{.cpp}
	 * FlowUi::App destination(std::move(source));
	 * @endcode
	 */
	App(App&&) noexcept;
	/**
	 * @brief Release the destination runtime and transfer ownership from the source.
	 * @pre Perform ownership transfer while the App is quiescent, on its platform thread. Reacquire borrowed manager references through the destination after the move. Move assignment destroys the destination runtime first.
	 *
	 * @par Example
	 * @code{.cpp}
	 * destination = std::move(source);
	 * @endcode
	 */
	App& operator=(App&&) noexcept;
	/**
	 * @brief Copying App owners is disabled; move the owner instead.
	 */
	App(const App&) = delete;
	/**
	 * @brief Copying App owners is disabled; move the owner instead.
	 */
	App& operator=(const App&) = delete;
	/**
	 * @brief Destroy the app and owned runtime resources.
	 *
	 * @par Example
	 * @code{.cpp}
	 * {
	 *     FlowUi::App scopedApplication = std::move(application);
	 * } // The App destructor releases its runtime here.
	 * @endcode
	 */
	~App() noexcept;

	/**
	 * @brief Return the stable identity of the semantic main window.
	 *
	 * No-argument window and frame functions operate on this window.
	 *
	 * @return Stable main-window identity.
	 *
	 * @par Example
	 * @code{.cpp}
	 * const FlowUi::WindowId mainWindow = application.mainWindowId();
	 * @endcode
	 */
	[[nodiscard]] WindowId mainWindowId() const noexcept;

	/**
	 * @brief Route a caller-owned FlowUi error through this App's configured reporting
	 * sink and/or compact default writer. Reporting does not resolve the error or
	 * alter application control flow.
	 *
	 * @par Example
	 * @code{.cpp}
	 * const auto error = FlowUi::makeError(
	 *     FlowUi::ErrorCode::ColorInvalid, FlowUi::ErrorSite::ColorParse);
	 * application.reportError(error);
	 * @endcode
	 */
	void reportError(FlowUiError error) const noexcept;

	/**
	 * @brief Create and fully initialize a secondary window.
	 *
	 * The new window uses the app's Vulkan and UI configuration together with
	 * the supplied native-window configuration.
	 *
	 * @param config Native-window configuration for the new window.
	 * @return Stable window identity, or an application, configuration, frame-phase,
	 * platform-thread, native-window, or rendering-resource error.
	 * @pre Call on the platform thread with no active window frame.
	 *
	 * @note Check success before continuing or reading a value. This noexcept App boundary returns operational failures as FlowUiError and preserves their attribution. Caller argument construction occurs before entry.
	 *
	 * @par Example
	 * @code{.cpp}
	 * FlowUi::WindowConfig configuration{};
	 * configuration.title = "Inspector";
	 * configuration.width = 640;
	 * configuration.height = 480;
	 * auto result = application.createWindow(configuration);
	 * if (!result) {
	 *     throw FlowUi::FlowUiException(result.error());
	 * }
	 * const FlowUi::WindowId createdWindow = *result;
	 * @endcode
	 */
	[[nodiscard]] Result<WindowId> createWindow(const WindowConfig& config) noexcept;
	/**
	 * @brief Create a secondary window using the main configuration and an automatically suffixed title.
	 * @return New identity, or the same structured failures as createWindow(const WindowConfig&).
	 *
	 * @note Check success before continuing or reading a value. This noexcept App boundary returns operational failures as FlowUiError and preserves their attribution. Caller argument construction occurs before entry.
	 *
	 * @par Example
	 * @code{.cpp}
	 * auto result = application.createWindowLikeMain();
	 * if (!result) {
	 *     throw FlowUi::FlowUiException(result.error());
	 * }
	 * const FlowUi::WindowId createdWindow = *result;
	 * @endcode
	 */
	[[nodiscard]] Result<WindowId> createWindowLikeMain() noexcept;
	/**
	 * @brief Apply overrides to the original main-window configuration and create a secondary window.
	 * @param overrides Values to replace; an absent title receives a unique window suffix.
	 * @return New identity, or the same structured failures as createWindow(const WindowConfig&).
	 *
	 * @note Check success before continuing or reading a value. This noexcept App boundary returns operational failures as FlowUiError and preserves their attribution. Caller argument construction occurs before entry.
	 *
	 * @par Example
	 * @code{.cpp}
	 * FlowUi::WindowConfigOverrides overrides{};
	 * overrides.title = "Inspector";
	 * overrides.width = 640;
	 * overrides.height = 480;
	 * auto result = application.createWindow(overrides);
	 * if (!result) {
	 *     throw FlowUi::FlowUiException(result.error());
	 * }
	 * const FlowUi::WindowId createdWindow = *result;
	 * @endcode
	 */
	[[nodiscard]] Result<WindowId> createWindow(const WindowConfigOverrides& overrides) noexcept;
	/**
	 * @brief Create a secondary window with an explicit title and optional dimensions.
	 * @param title Copied title for the new native window.
	 * @param width Width override; zero inherits the main configuration.
	 * @param height Height override; zero inherits the main configuration.
	 * @return New identity, or a structured creation/configuration error.
	 *
	 * @note Check success before continuing or reading a value. This noexcept App boundary returns operational failures as FlowUiError and preserves their attribution. Caller argument construction occurs before entry.
	 *
	 * @par Example
	 * @code{.cpp}
	 * auto result = application.createWindow("Inspector", 640, 480);
	 * if (!result) {
	 *     throw FlowUi::FlowUiException(result.error());
	 * }
	 * const FlowUi::WindowId createdWindow = *result;
	 * @endcode
	 */
	[[nodiscard]] Result<WindowId> createWindow(std::string_view title, int width = 0,
												int height = 0) noexcept;
	/**
	 * @brief Create a secondary window and retain its synchronous UI callback.
	 * @param overrides Main-configuration overrides for the new window.
	 * @param buildUi Nonempty callback retained until replacement, removal, or window destruction.
	 * Captured references must outlive registration and any active invocation.
	 * @param flags Managed dispatch and close behavior.
	 * @return New identity or a creation/registration error. Callback ownership
	 * and registration capacity are prepared before native window creation.
	 *
	 * @note Check success before continuing or reading a value. This noexcept App boundary returns operational failures as FlowUiError and preserves their attribution. Caller argument construction occurs before entry.
	 *
	 * @par Example
	 * @code{.cpp}
	 * const FlowUi::UiBuildCallback buildUi =
	 *     [](FlowUi::UiManager& ui, FlowUi::WindowId) {
	 *         ui.createElement(FlowUi::kButton, "inspector/action")
	 *             .setParameters({.label = "Inspector"}).draw();
	 *     };
	 * FlowUi::WindowConfigOverrides overrides{};
	 * overrides.title = "Inspector";
	 * overrides.width = 640;
	 * overrides.height = 480;
	 * auto result = application.createWindow(overrides, buildUi, FlowUi::ManagedWindowFlags{.autoDestroyOnClose = true});
	 * if (!result) {
	 *     throw FlowUi::FlowUiException(result.error());
	 * }
	 * const FlowUi::WindowId createdWindow = *result;
	 * @endcode
	 */
	[[nodiscard]] Result<WindowId> createWindow(const WindowConfigOverrides& overrides,
												const UiBuildCallback& buildUi,
												ManagedWindowFlags flags = {}) noexcept;
	/**
	 * Attach or replace the managed UI callback for a secondary window.
	 *
	 * The semantic main window is not eligible because its frame remains under
	 * the no-argument frame API's control.
	 * @param id Registered secondary window.
	 * @param buildUi Nonempty retained build callback. References captured by it
	 * must remain valid while registered and during active invocations.
	 * @param flags Dispatch and close behavior for this registration.
	 * @return Success, or an app, identity, configuration, thread, or phase error.
	 *
	 * @note Check success before continuing or reading a value. This noexcept App boundary returns operational failures as FlowUiError and preserves their attribution. Caller argument construction occurs before entry.
	 *
	 * @par Example
	 * @code{.cpp}
	 * const FlowUi::UiBuildCallback buildUi =
	 *     [](FlowUi::UiManager& ui, FlowUi::WindowId) {
	 *         ui.createElement(FlowUi::kButton, "inspector/action")
	 *             .setParameters({.label = "Inspector"}).draw();
	 *     };
	 * const auto status = application.setWindowUiCallback(window, buildUi, FlowUi::ManagedWindowFlags{.autoDestroyOnClose = true});
	 * if (!status) {
	 *     throw FlowUi::FlowUiException(status.error());
	 * }
	 * @endcode
	 */
	[[nodiscard]] Status setWindowUiCallback(WindowId id, const UiBuildCallback& buildUi,
											 ManagedWindowFlags flags = {}) noexcept;
	/**
	 * Return a managed window to explicit/manual frame control on the platform thread.
	 * May remove the current callback or one belonging to a stopped/closing window.
	 * Unknown identities are successful no-ops.
	 * @return Success when removed or already absent; an unavailable-App, platform-thread, or prohibited-phase error otherwise.
	 *
	 * @note Check success before continuing or reading a value. This noexcept App boundary returns operational failures as FlowUiError and preserves their attribution. Caller argument construction occurs before entry.
	 *
	 * @par Example
	 * @code{.cpp}
	 * const auto status = application.removeWindowUiCallback(window);
	 * if (!status) {
	 *     throw FlowUi::FlowUiException(status.error());
	 * }
	 * @endcode
	 */
	[[nodiscard]] Status removeWindowUiCallback(WindowId id) noexcept;
	/**
	 * Execute all managed secondary-window frame triads in registration order.
	 *
	 * Dispatch is synchronous on the platform thread. Closed windows with
	 * autoDestroyOnClose enabled are drained and destroyed before returning.
	 * @return Success, or a structured frame/dispatch error after canceling the active frame.
	 *
	 * @note Check success before continuing or reading a value. This noexcept App boundary returns operational failures as FlowUiError and preserves their attribution. Caller argument construction occurs before entry.
	 *
	 * @par Example
	 * @code{.cpp}
	 * const auto status = application.dispatchManagedWindows();
	 * if (!status) {
	 *     throw FlowUi::FlowUiException(status.error());
	 * }
	 * @endcode
	 */
	[[nodiscard]] Status dispatchManagedWindows() noexcept;

	/**
	 * Execute the main window frame under a library-owned error boundary.
	 * Poll events, begin the frame, invoke buildUi, finish and draw the frame,
	 * then dispatch managed windows. Any build failure cancels the active frame.
	 * @return Success or the original FlowUiError; foreign callback exceptions
	 * become ForeignExceptionObserved and allocation failures become AllocationFailed.
	 *
	 * @note Check success before continuing or reading a value. This noexcept App boundary returns operational failures as FlowUiError and preserves their attribution. Caller argument construction occurs before entry.
	 *
	 * @par Example
	 * @code{.cpp}
	 * const FlowUi::UiBuildCallback buildUi =
	 *     [](FlowUi::UiManager& ui, FlowUi::WindowId) {
	 *         ui.createElement(FlowUi::kButton, "inspector/action")
	 *             .setParameters({.label = "Inspector"}).draw();
	 *     };
	 * const auto status = application.runFrame(buildUi);
	 * if (!status) {
	 *     throw FlowUi::FlowUiException(status.error());
	 * }
	 * @endcode
	 */
	[[nodiscard]] Status runFrame(const UiBuildCallback& buildUi) noexcept;
	/**
	 * Execute a registered window frame without polling native events.
	 * Invoke pollEvents() once before processing explicit window frames.
	 * The callback and all frame phases execute inside the result boundary.
	 *
	 * @note Check success before continuing or reading a value. This noexcept App boundary returns operational failures as FlowUiError and preserves their attribution. Caller argument construction occurs before entry.
	 *
	 * @par Example
	 * @code{.cpp}
	 * const FlowUi::UiBuildCallback buildUi =
	 *     [](FlowUi::UiManager& ui, FlowUi::WindowId) {
	 *         ui.createElement(FlowUi::kButton, "inspector/action")
	 *             .setParameters({.label = "Inspector"}).draw();
	 *     };
	 * const auto status = application.runFrame(window, buildUi);
	 * if (!status) {
	 *     throw FlowUi::FlowUiException(status.error());
	 * }
	 * @endcode
	 */
	[[nodiscard]] Status runFrame(WindowId id, const UiBuildCallback& buildUi) noexcept;

	/**
	 * Retain a callable after constructing its owning callback inside the API boundary.
	 * A returned Status is resolved by managed frame dispatch.
	 * @note Only an exact Status return is interpreted as a recoverable callback result. Other callback return values are ignored. Captured data must remain alive for every retained callback invocation.
	 *
	 * @note Check success before continuing or reading a value. This noexcept App boundary returns operational failures as FlowUiError and preserves their attribution. Caller argument construction occurs before entry.
	 *
	 * @par Example
	 * @code{.cpp}
	 * FlowUi::WindowConfigOverrides overrides{};
	 * overrides.title = "Inspector";
	 * overrides.width = 640;
	 * overrides.height = 480;
	 * auto buildUi = [](FlowUi::UiManager& ui, FlowUi::WindowId) -> FlowUi::Status {
	 *     ui.createElement(FlowUi::kButton, "inspector/action")
	 *         .setParameters({.label = "Inspector"}).draw();
	 *     return ui.frameStatus();
	 * };
	 * auto result = application.createWindow(overrides, buildUi);
	 * if (!result) {
	 *     throw FlowUi::FlowUiException(result.error());
	 * }
	 * const FlowUi::WindowId createdWindow = *result;
	 * @endcode
	 */
	template <typename Callback>
		requires (!std::same_as<std::remove_cvref_t<Callback>, UiBuildCallback> &&
				  std::invocable<Callback&, UiManager&, WindowId>)
	[[nodiscard]] Result<WindowId> createWindow(const WindowConfigOverrides& overrides,
		Callback&& buildUi, ManagedWindowFlags flags = {}) noexcept {
		return detail::catchResult<Result<WindowId>>(ErrorSite::AppCreateWindow, [&] {
			const UiBuildCallback callback = ownBuildCallback(std::forward<Callback>(buildUi));
			return createWindow(overrides, callback, flags);
		});
	}
	/**
	 * Retain a callable while containing callback allocation or copying failures.
	 * @note Only an exact Status return is interpreted as a recoverable callback result. Other callback return values are ignored. Captured data must remain alive for every retained callback invocation.
	 *
	 * @note Check success before continuing or reading a value. This noexcept App boundary returns operational failures as FlowUiError and preserves their attribution. Caller argument construction occurs before entry.
	 *
	 * @par Example
	 * @code{.cpp}
	 * auto buildUi = [](FlowUi::UiManager& ui, FlowUi::WindowId) -> FlowUi::Status {
	 *     ui.createElement(FlowUi::kButton, "managed/action")
	 *         .setParameters({.label = "Managed window"}).draw();
	 *     return ui.frameStatus();
	 * };
	 * const auto status = application.setWindowUiCallback(window, buildUi, FlowUi::ManagedWindowFlags{.autoDestroyOnClose = true});
	 * if (!status) {
	 *     throw FlowUi::FlowUiException(status.error());
	 * }
	 * @endcode
	 */
	template <typename Callback>
		requires (!std::same_as<std::remove_cvref_t<Callback>, UiBuildCallback> &&
				  std::invocable<Callback&, UiManager&, WindowId>)
	[[nodiscard]] Status setWindowUiCallback(WindowId id, Callback&& buildUi,
		ManagedWindowFlags flags = {}) noexcept {
		return detail::catchResult<Status>(ErrorSite::AppFrameDispatch, [&] {
			const UiBuildCallback callback = ownBuildCallback(std::forward<Callback>(buildUi));
			return setWindowUiCallback(id, callback, flags);
		});
	}
	/**
	 * Construct the main-window build callback inside the result boundary.
	 * A callback returning Status propagates its failure through runFrame().
	 * @note Only an exact Status return is interpreted as a recoverable callback result. Other callback return values are ignored. Captured data must remain alive for every retained callback invocation.
	 *
	 * @note Check success before continuing or reading a value. This noexcept App boundary returns operational failures as FlowUiError and preserves their attribution. Caller argument construction occurs before entry.
	 *
	 * @par Example
	 * @code{.cpp}
	 * auto buildUi = [](FlowUi::UiManager& ui, FlowUi::WindowId) -> FlowUi::Status {
	 *     ui.createElement(FlowUi::kButton, "main/action")
	 *         .setParameters({.label = "Save"}).draw();
	 *     return ui.frameStatus();
	 * };
	 * const auto status = application.runFrame(buildUi);
	 * if (!status) {
	 *     throw FlowUi::FlowUiException(status.error());
	 * }
	 * @endcode
	 */
	template <typename Callback>
		requires (!std::same_as<std::remove_cvref_t<Callback>, UiBuildCallback> &&
				  std::invocable<Callback&, UiManager&, WindowId>)
	[[nodiscard]] Status runFrame(Callback&& buildUi) noexcept {
		return detail::catchResult<Status>(ErrorSite::AppFrameDispatch, [&] {
			const UiBuildCallback callback = ownBuildCallback(std::forward<Callback>(buildUi));
			return runFrame(callback);
		});
	}
	/**
	 * Construct an explicit-window build callback inside the result boundary.
	 * @note Only an exact Status return is interpreted as a recoverable callback result. Other callback return values are ignored. Captured data must remain alive for every retained callback invocation.
	 *
	 * @note Check success before continuing or reading a value. This noexcept App boundary returns operational failures as FlowUiError and preserves their attribution. Caller argument construction occurs before entry.
	 *
	 * @par Example
	 * @code{.cpp}
	 * auto buildUi = [](FlowUi::UiManager& ui, FlowUi::WindowId) -> FlowUi::Status {
	 *     ui.createElement(FlowUi::kButton, "main/action")
	 *         .setParameters({.label = "Save"}).draw();
	 *     return ui.frameStatus();
	 * };
	 * const auto status = application.runFrame(window, buildUi);
	 * if (!status) {
	 *     throw FlowUi::FlowUiException(status.error());
	 * }
	 * @endcode
	 */
	template <typename Callback>
		requires (!std::same_as<std::remove_cvref_t<Callback>, UiBuildCallback> &&
				  std::invocable<Callback&, UiManager&, WindowId>)
	[[nodiscard]] Status runFrame(WindowId id, Callback&& buildUi) noexcept {
		return detail::catchResult<Status>(ErrorSite::AppFrameDispatch, [&] {
			const UiBuildCallback callback = ownBuildCallback(std::forward<Callback>(buildUi));
			return runFrame(id, callback);
		});
	}

	/**
	 * @brief Create a managed window drawing one element with retained parameters.
	 * @tparam Element Flow element definition to draw as the window root.
	 * @param overrides Window-configuration overrides.
	 * @param element Element tag copied into the retained callback.
	 * @param params Parameters copied into that callback; any borrowed members
	 * must outlive their use by the managed window.
	 * @param localName Root element name; borrowed name storage must remain valid.
	 * @return New window identity or a structured creation/registration error.
	 *
	 * @note Check success before continuing or reading a value. This noexcept App boundary returns operational failures as FlowUiError and preserves their attribution. Caller argument construction occurs before entry.
	 *
	 * @par Example
	 * @code{.cpp}
	 * FlowUi::WindowConfigOverrides overrides{};
	 * overrides.title = "Inspector";
	 * overrides.width = 640;
	 * overrides.height = 480;
	 * auto result = application.createWindow(overrides, FlowUi::kButton, FlowUi::ButtonParameters{.label = "Inspector"});
	 * if (!result) {
	 *     throw FlowUi::FlowUiException(result.error());
	 * }
	 * const FlowUi::WindowId createdWindow = *result;
	 * @endcode
	 */
	template <FlowElement Element>
	[[nodiscard]] Result<WindowId>
	createWindow(const WindowConfigOverrides& overrides, const Element& element,
				 const ParametersOf<Element>& params,
				 LocalElementName localName = LocalElementName{"window/root-element"}) noexcept;

	/**
	 * Create a managed element window with parameters constructed inside the error boundary.
	 *
	 * @note Check success before continuing or reading a value. This noexcept App boundary returns operational failures as FlowUiError and preserves their attribution. Caller argument construction occurs before entry.
	 *
	 * @par Example
	 * @code{.cpp}
	 * FlowUi::WindowConfigOverrides overrides{};
	 * overrides.title = "Inspector";
	 * overrides.width = 640;
	 * overrides.height = 480;
	 * auto result = application.createWindow(overrides, FlowUi::kButton);
	 * if (!result) {
	 *     throw FlowUi::FlowUiException(result.error());
	 * }
	 * const FlowUi::WindowId createdWindow = *result;
	 * @endcode
	 */
	template <FlowElement Element>
	[[nodiscard]] Result<WindowId> createWindow(const WindowConfigOverrides& overrides,
		const Element& element) noexcept;

	/**
	 * @brief Create a managed window whose root builder is configured on every frame.
	 * @tparam Element Flow element definition to draw.
	 * @param overrides Window-configuration overrides.
	 * @param element Element tag copied into the retained callback.
	 * @param configurator Nonempty retained callback invoked before the root is drawn.
	 * Captured references must outlive registration and active callback execution.
	 * @param localName Root element name with storage valid for the window lifetime.
	 * @return New identity, or InvalidWindowConfiguration for an empty configurator,
	 * or a structured creation/registration error.
	 *
	 * @note Check success before continuing or reading a value. This noexcept App boundary returns operational failures as FlowUiError and preserves their attribution. Caller argument construction occurs before entry.
	 *
	 * @par Example
	 * @code{.cpp}
	 * FlowUi::WindowConfigOverrides overrides{};
	 * overrides.title = "Inspector";
	 * overrides.width = 640;
	 * overrides.height = 480;
	 * const std::function<void(FlowUi::ElementBuilder<FlowUi::Button>&, FlowUi::WindowId)> configure =
	 *     [](FlowUi::ElementBuilder<FlowUi::Button>& builder, FlowUi::WindowId) {
	 *         (void)builder.setParameters({.label = "Inspector"});
	 *     };
	 * auto result = application.createWindow(overrides, FlowUi::kButton, configure);
	 * if (!result) {
	 *     throw FlowUi::FlowUiException(result.error());
	 * }
	 * const FlowUi::WindowId createdWindow = *result;
	 * @endcode
	 */
	template <FlowElement Element>
	[[nodiscard]] Result<WindowId>
	createWindow(const WindowConfigOverrides& overrides, const Element& element,
				 const std::function<void(ElementBuilder<Element>&, WindowId)>& configurator,
				 LocalElementName localName = LocalElementName{"window/root-element"}) noexcept;

	/**
	 * Construct an owning element configurator inside the result boundary.
	 * A returned Status is resolved by managed frame dispatch.
	 * @note The adapter resolves an exact Status return before drawing the root. Other configurator return values are ignored. Borrowed captured data must outlive the managed window registration.
	 *
	 * @note Check success before continuing or reading a value. This noexcept App boundary returns operational failures as FlowUiError and preserves their attribution. Caller argument construction occurs before entry.
	 *
	 * @par Example
	 * @code{.cpp}
	 * FlowUi::WindowConfigOverrides overrides{};
	 * overrides.title = "Inspector";
	 * overrides.width = 640;
	 * overrides.height = 480;
	 * auto configure = [](FlowUi::ElementBuilder<FlowUi::Button>& builder, FlowUi::WindowId) -> FlowUi::Status {
	 *     (void)builder.setParameters({.label = "Inspector"});
	 *     return {};
	 * };
	 * auto result = application.createWindow(overrides, FlowUi::kButton, configure);
	 * if (!result) {
	 *     throw FlowUi::FlowUiException(result.error());
	 * }
	 * const FlowUi::WindowId createdWindow = *result;
	 * @endcode
	 */
	template <FlowElement Element, typename Configurator>
		requires (!std::same_as<std::remove_cvref_t<Configurator>,
							std::function<void(ElementBuilder<Element>&, WindowId)>> &&
				  std::invocable<Configurator&, ElementBuilder<Element>&, WindowId>)
	[[nodiscard]] Result<WindowId> createWindow(const WindowConfigOverrides& overrides,
		const Element& element, Configurator&& configurator,
		LocalElementName localName = LocalElementName{"window/root-element"}) noexcept {
		return detail::catchResult<Result<WindowId>>(ErrorSite::AppCreateWindow, [&] {
			const std::function<void(ElementBuilder<Element>&, WindowId)> callback =
				[configure = std::forward<Configurator>(configurator)](ElementBuilder<Element>& builder,
					WindowId id) mutable {
					if constexpr (std::same_as<std::invoke_result_t<std::decay_t<Configurator>&,
						ElementBuilder<Element>&, WindowId>, Status>)
						detail::requireApiSuccess(std::invoke(configure, builder, id));
					else
						(void)std::invoke(configure, builder, id);
				};
			return createWindow(overrides, element, callback, localName);
		});
	}

	/**
	 * @brief Create a managed window that reads shared root parameters each frame.
	 * @tparam Element Flow element definition to draw.
	 * @param overrides Window-configuration overrides.
	 * @param element Element tag copied into the retained callback.
	 * @param sharedParams Nonnull shared parameter object retained by the callback.
	 * Updates are observed on later synchronous builds; borrowed parameter members
	 * remain caller-owned.
	 * @param localName Root name with storage valid for the window lifetime.
	 * @return New identity, or InvalidWindowConfiguration for null shared state,
	 * or a structured creation/registration error.
	 *
	 * @note Check success before continuing or reading a value. This noexcept App boundary returns operational failures as FlowUiError and preserves their attribution. Caller argument construction occurs before entry.
	 *
	 * @par Example
	 * @code{.cpp}
	 * FlowUi::WindowConfigOverrides overrides{};
	 * overrides.title = "Shared inspector";
	 * auto parameters = std::make_shared<FlowUi::ButtonParameters>();
	 * parameters->label = "Shared inspector";
	 * auto result = application.createWindowWithState(overrides, FlowUi::kButton, parameters);
	 * if (!result) {
	 *     throw FlowUi::FlowUiException(result.error());
	 * }
	 * const FlowUi::WindowId createdWindow = *result;
	 * @endcode
	 */
	template <FlowElement Element>
	[[nodiscard]] Result<WindowId>
	createWindowWithState(const WindowConfigOverrides& overrides, const Element& element,
						  std::shared_ptr<ParametersOf<Element>> sharedParams,
						  LocalElementName localName = LocalElementName{"window/root-element"}) noexcept;
	/**
	 * @brief Destroy a secondary window after draining its outstanding work.
	 *
	 * The semantic main window cannot be explicitly destroyed. A failed secondary
	 * teardown remains registered and can be retried; frame and native-window
	 * operations reject it while closing. A stopped secondary window can be
	 * destroyed and replaced. Shared graphics failure requires whole-app teardown.
	 *
	 * @param id Secondary-window identity returned by createWindow().
	 * @return Success, or an unavailable-app, invalid-window, main-window,
	 * platform-thread, frame-phase, or resource-drain error.
	 *
	 * @note Check success before continuing or reading a value. This noexcept App boundary returns operational failures as FlowUiError and preserves their attribution. Caller argument construction occurs before entry.
	 *
	 * @par Example
	 * @code{.cpp}
	 * const auto status = application.destroyWindow(window);
	 * if (!status) {
	 *     throw FlowUi::FlowUiException(status.error());
	 * }
	 * @endcode
	 */
	[[nodiscard]] Status destroyWindow(WindowId id) noexcept;
	/**
	 * @brief Return whether an identity currently names a registered window.
	 *
	 * @param id Window identity to query.
	 * @retval true id identifies a current window.
	 * @retval false id is invalid, unknown, or was destroyed.
	 *
	 * @par Example
	 * @code{.cpp}
	 * const bool windowExists = application.hasWindow(window);
	 * @endcode
	 */
	[[nodiscard]] bool hasWindow(WindowId id) const noexcept;
	/**
	 * @brief Poll global platform events and advance app-shared maintenance.
	 *
	 * Multi-window applications should call this once per outer application-loop
	 * iteration before beginning explicit window frames. The function is global,
	 * not associated with one WindowId, and cannot run while a window frame
	 * triplet is active.
	 *
	 * Single-main-window applications normally do not call this directly because
	 * the no-argument beginFrame() overload performs the same polling first.
	 *
	 * @return Success, or an app, platform-thread, or frame-phase error.
	 *
	 * @note Check success before continuing or reading a value. This noexcept App boundary returns operational failures as FlowUiError and preserves their attribution. Caller argument construction occurs before entry.
	 *
	 * @par Example
	 * @code{.cpp}
	 * const auto status = application.pollEvents();
	 * if (!status) {
	 *     throw FlowUi::FlowUiException(status.error());
	 * }
	 * @endcode
	 */
	[[nodiscard]] Status pollEvents() noexcept;

	/**
	 * @brief Query whether the window backend requested shutdown.
	 *
	 * Use this function to drive the app lifecycle loop.
	 *
	 * @return True when shutdown was requested or rendering stopped, false otherwise,
	 * or an app/window access error.
	 *
	 * @note Check success before continuing or reading a value. This noexcept App boundary returns operational failures as FlowUiError and preserves their attribution. Caller argument construction occurs before entry.
	 *
	 * @par Example
	 * @code{.cpp}
	 * auto result = application.shouldClose();
	 * if (!result) {
	 *     throw FlowUi::FlowUiException(result.error());
	 * }
	 * const bool closeRequested = *result;
	 * @endcode
	 */
	[[nodiscard]] Result<bool> shouldClose() const noexcept;
	/**
	 * @brief Query whether a specific window requested shutdown.
	 *
	 * @param id Registered window identity.
	 * @return True when shutdown was requested or the window is Stopped/Closing, false otherwise, or an app/window/thread access error.
	 *
	 * @note Check success before continuing or reading a value. This noexcept App boundary returns operational failures as FlowUiError and preserves their attribution. Caller argument construction occurs before entry.
	 *
	 * @par Example
	 * @code{.cpp}
	 * auto result = application.shouldClose(window);
	 * if (!result) {
	 *     throw FlowUi::FlowUiException(result.error());
	 * }
	 * const bool closeRequested = *result;
	 * @endcode
	 */
	[[nodiscard]] Result<bool> shouldClose(WindowId id) const noexcept;

	/**
	 * @brief Set whether the window backend should request shutdown.
	 *
	 * The `value` argument updates the selected platform provider close flag. Zero
	 * clears the window close flag; a non-zero value sets it.
	 *
	 * @param value Zero clears the close flag; nonzero requests closing.
	 * @note Setting the close flag does not destroy a window inline. A Stopped/Closing window rejects this ordinary native-window operation; use shouldClose(), removeWindowUiCallback(), or secondary destroyWindow() for teardown.
	 *
	 * @note Check success before continuing or reading a value. This noexcept App boundary returns operational failures as FlowUiError and preserves their attribution. Caller argument construction occurs before entry.
	 *
	 * @par Example
	 * @code{.cpp}
	 * const auto status = application.setShouldClose(1);
	 * if (!status) {
	 *     throw FlowUi::FlowUiException(status.error());
	 * }
	 * @endcode
	 */
	[[nodiscard]] Status setShouldClose(int value) noexcept;
	/**
	 * @brief Set or clear a specific window's shutdown request.
	 *
	 * @param id Registered window identity.
	 * @param value Zero clears the request; a non-zero value sets it.
	 * @note Setting the close flag does not destroy a window inline. A Stopped/Closing window rejects this ordinary native-window operation; use shouldClose(), removeWindowUiCallback(), or secondary destroyWindow() for teardown.
	 *
	 * @note Check success before continuing or reading a value. This noexcept App boundary returns operational failures as FlowUiError and preserves their attribution. Caller argument construction occurs before entry.
	 *
	 * @par Example
	 * @code{.cpp}
	 * const auto status = application.setShouldClose(window, 1);
	 * if (!status) {
	 *     throw FlowUi::FlowUiException(status.error());
	 * }
	 * @endcode
	 */
	[[nodiscard]] Status setShouldClose(WindowId id, int value) noexcept;

	/**
	 * Report native operations supported by a live window on the platform thread.
	 *
	 * @note Check success before continuing or reading a value. This noexcept App boundary returns operational failures as FlowUiError and preserves their attribution. Caller argument construction occurs before entry.
	 *
	 * @par Example
	 * @code{.cpp}
	 * auto result = application.window_operation_capabilities(window);
	 * if (!result) {
	 *     throw FlowUi::FlowUiException(result.error());
	 * }
	 * const FlowUi::WindowOperationCapabilities capabilities = *result;
	 * @endcode
	 */
	[[nodiscard]] Result<WindowOperationCapabilities>
	window_operation_capabilities(WindowId window_id) const noexcept;
	/**
	 * Request closure of this window without destroying it during input dispatch.
	 *
	 * @note Check success before continuing or reading a value. This noexcept App boundary returns operational failures as FlowUiError and preserves their attribution. Caller argument construction occurs before entry.
	 *
	 * @par Example
	 * @code{.cpp}
	 * const auto status = application.request_window_close(window);
	 * if (!status) {
	 *     throw FlowUi::FlowUiException(status.error());
	 * }
	 * @endcode
	 */
	[[nodiscard]] Status request_window_close(WindowId window_id) noexcept;
	/**
	 * Minimize this window; requests during a frame run at the next event-poll boundary.
	 *
	 * @note Check success before continuing or reading a value. This noexcept App boundary returns operational failures as FlowUiError and preserves their attribution. Caller argument construction occurs before entry.
	 *
	 * @par Example
	 * @code{.cpp}
	 * const auto status = application.minimize_window(window);
	 * if (!status) {
	 *     throw FlowUi::FlowUiException(status.error());
	 * }
	 * @endcode
	 */
	[[nodiscard]] Status minimize_window(WindowId window_id) noexcept;
	/**
	 * Toggle observed maximized state; rejected while fullscreen. Frame requests are deferred.
	 *
	 * @note Check success before continuing or reading a value. This noexcept App boundary returns operational failures as FlowUiError and preserves their attribution. Caller argument construction occurs before entry.
	 *
	 * @par Example
	 * @code{.cpp}
	 * const auto status = application.toggle_window_maximized(window);
	 * if (!status) {
	 *     throw FlowUi::FlowUiException(status.error());
	 * }
	 * @endcode
	 */
	[[nodiscard]] Status toggle_window_maximized(WindowId window_id) noexcept;
	/**
	 * Toggle fullscreen and restore windowed geometry/state. Frame requests are deferred.
	 *
	 * @note Check success before continuing or reading a value. This noexcept App boundary returns operational failures as FlowUiError and preserves their attribution. Caller argument construction occurs before entry.
	 *
	 * @par Example
	 * @code{.cpp}
	 * const auto status = application.toggle_window_fullscreen(window);
	 * if (!status) {
	 *     throw FlowUi::FlowUiException(status.error());
	 * }
	 * @endcode
	 */
	[[nodiscard]] Status toggle_window_fullscreen(WindowId window_id) noexcept;

	/**
	 * @brief Poll events and begin a frame for the semantic main window.
	 *
	 * This convenience overload first performs the same global event polling and
	 * app-shared maintenance as pollEvents(), then begins the window returned by
	 * mainWindowId(). It preserves the classic single-window frame loop without a
	 * separate polling call.
	 *
	 * @pre The FlowUi::App instance is initialized and its window/UI systems are valid.
	 * @return Success when UI construction can begin, or a structured frame error.
	 * @post On success, the frame is active and its UI is ready for construction.
	 * @note Do not also call pollEvents() in the same loop iteration unless a
	 * second global event-pump pass is intentional.
	 * @see @ref md_docs_2concepts_2frame__lifecycle "Frame Lifecycle"
	 * @see @ref md_docs_2tutorials_2quick__start "Quick Start"
	 *
	 * @note Check success before continuing or reading a value. This noexcept App boundary returns operational failures as FlowUiError and preserves their attribution. Caller argument construction occurs before entry.
	 *
	 * @par Example
	 * @code{.cpp}
	 * const auto status = application.beginFrame();
	 * if (!status) {
	 *     throw FlowUi::FlowUiException(status.error());
	 * }
	 * @endcode
	 */
	[[nodiscard]] Status beginFrame() noexcept;
	/**
	 * @brief Begin one explicit window frame without polling global events.
	 *
	 * Use this overload after one pollEvents() call when driving multiple windows;
	 * `id` must identify a registered window.
	 * A window's beginFrame(id), endFrame(id), and drawFrame(id) triplet must finish
	 * before another window frame is begun.
	 *
	 * @pre pollEvents() was called at the cadence required by the application.
	 * @param id Registered window to build.
	 * @return Success or a structured app, identity, thread, or frame-phase error.
	 * @post On success, the selected UI manager is ready for construction.
	 *
	 * @note Check success before continuing or reading a value. This noexcept App boundary returns operational failures as FlowUiError and preserves their attribution. Caller argument construction occurs before entry.
	 *
	 * @par Example
	 * @code{.cpp}
	 * const auto status = application.beginFrame(window);
	 * if (!status) {
	 *     throw FlowUi::FlowUiException(status.error());
	 * }
	 * @endcode
	 */
	[[nodiscard]] Status beginFrame(WindowId id) noexcept;
	/**
	 * @brief End UI construction and produce render commands.
	 *
	 * Completes UI construction, produces render commands for this frame, and
	 * prepares viewport and icon manager resources.
	 *
	 * @pre FlowUi::App::beginFrame() was called and UI was built afterward.
	 * @return Success or a structured frame/resource/allocation error.
	 * @post On success, render commands are ready and viewport handles are sized.
	 * @note This function should be called exactly once per frame.
	 *
	 * @note Check success before continuing or reading a value. This noexcept App boundary returns operational failures as FlowUiError and preserves their attribution. Caller argument construction occurs before entry.
	 *
	 * @par Example
	 * @code{.cpp}
	 * const auto status = application.endFrame();
	 * if (!status) {
	 *     throw FlowUi::FlowUiException(status.error());
	 * }
	 * @endcode
	 */
	[[nodiscard]] Status endFrame() noexcept;
	/**
	 * @brief End UI construction for a specific active window frame.
	 *
	 * @param id Window identity passed to the matching beginFrame(id).
	 * @return Success or a structured identity, phase, frame/resource, or allocation error.
	 *
	 * @note Check success before continuing or reading a value. This noexcept App boundary returns operational failures as FlowUiError and preserves their attribution. Caller argument construction occurs before entry.
	 *
	 * @par Example
	 * @code{.cpp}
	 * const auto status = application.endFrame(window);
	 * if (!status) {
	 *     throw FlowUi::FlowUiException(status.error());
	 * }
	 * @endcode
	 */
	[[nodiscard]] Status endFrame(WindowId id) noexcept;
	/**
	 * @brief Submit the current frame for rendering and presentation.
	 *
	 * Takes the render commands output by endFrame(), builds GPU draw runs,
	 * renders them, and passes the output to the swapchain for presentation.
	 *
	 * @pre FlowUi::App::endFrame() was called successfully.
	 * @return Success or a structured render, presentation, allocation, or managed-dispatch error.
	 * @post On success, frame submission/presentation processing has completed.
	 * @note This function should be called exactly once per frame.
	 * @note After presenting the main window, this overload synchronously calls
	 * dispatchManagedWindows(). The WindowId overload never dispatches managed
	 * windows implicitly.
	 *
	 * @note Check success before continuing or reading a value. This noexcept App boundary returns operational failures as FlowUiError and preserves their attribution. Caller argument construction occurs before entry.
	 *
	 * @par Example
	 * @code{.cpp}
	 * const auto status = application.drawFrame();
	 * if (!status) {
	 *     throw FlowUi::FlowUiException(status.error());
	 * }
	 * @endcode
	 */
	[[nodiscard]] Status drawFrame() noexcept;
	/**
	 * @brief Submit and present a specific prepared window frame.
	 *
	 * @param id Window identity passed to the matching beginFrame(id) and
	 * endFrame(id).
	 * @return Success or a structured identity, phase, render, allocation, or presentation error.
	 *
	 * @note Check success before continuing or reading a value. This noexcept App boundary returns operational failures as FlowUiError and preserves their attribution. Caller argument construction occurs before entry.
	 *
	 * @par Example
	 * @code{.cpp}
	 * const auto status = application.drawFrame(window);
	 * if (!status) {
	 *     throw FlowUi::FlowUiException(status.error());
	 * }
	 * @endcode
	 */
	[[nodiscard]] Status drawFrame(WindowId id) noexcept;

	/**
	 * @brief Access the font manager.
	 *
	 * @return Borrowed reference wrapper, or AppUnavailable, for FlowUi::FontManager.
	 * @see @ref flowui_font_manager "Font Manager"
	 *
	 * @note Check success before continuing or reading a value. This noexcept App boundary returns operational failures as FlowUiError and preserves their attribution. Caller argument construction occurs before entry.
	 * @note The example first obtains a checked result, then a comfortable borrowed reference. The result/reference does not own or extend the manager lifetime. App-owned managers expire with App; window-owned managers also expire when that window is destroyed. Reacquire after moving/replacing the owning App.
	 *
	 * @par Example
	 * @code{.cpp}
	 * auto result = application.fonts();
	 * if (!result) {
	 *     throw FlowUi::FlowUiException(result.error());
	 * }
	 * FlowUi::FontManager& fonts = result->get();
	 * @endcode
	 */
	[[nodiscard]] Result<std::reference_wrapper<FontManager>> fonts() noexcept;
	/**
	 * @brief Access the font manager.
	 *
	 * @return Borrowed const reference wrapper, or AppUnavailable, for FlowUi::FontManager.
	 * @see @ref flowui_font_manager "Font Manager"
	 *
	 * @note Check success before continuing or reading a value. This noexcept App boundary returns operational failures as FlowUiError and preserves their attribution. Caller argument construction occurs before entry.
	 * @note The example first obtains a checked result, then a comfortable borrowed reference. The result/reference does not own or extend the manager lifetime. App-owned managers expire with App; window-owned managers also expire when that window is destroyed. Reacquire after moving/replacing the owning App.
	 *
	 * @par Example
	 * @code{.cpp}
	 * const FlowUi::App& readOnlyApplication = application;
	 * auto result = readOnlyApplication.fonts();
	 * if (!result) {
	 *     throw FlowUi::FlowUiException(result.error());
	 * }
	 * const FlowUi::FontManager& fonts = result->get();
	 * @endcode
	 */
	[[nodiscard]] Result<std::reference_wrapper<const FontManager>> fonts() const noexcept;
	/**
	 * @brief Access the image manager.
	 *
	 * @return Borrowed reference wrapper, or AppUnavailable, for FlowUi::ImageManager.
	 * @see @ref flowui_image_manager "Image Manager"
	 *
	 * @note Check success before continuing or reading a value. This noexcept App boundary returns operational failures as FlowUiError and preserves their attribution. Caller argument construction occurs before entry.
	 * @note The example first obtains a checked result, then a comfortable borrowed reference. The result/reference does not own or extend the manager lifetime. App-owned managers expire with App; window-owned managers also expire when that window is destroyed. Reacquire after moving/replacing the owning App.
	 *
	 * @par Example
	 * @code{.cpp}
	 * auto result = application.images();
	 * if (!result) {
	 *     throw FlowUi::FlowUiException(result.error());
	 * }
	 * FlowUi::ImageManager& images = result->get();
	 * @endcode
	 */
	[[nodiscard]] Result<std::reference_wrapper<ImageManager>> images() noexcept;
	/**
	 * @brief Access the image manager.
	 *
	 * @return Borrowed const reference wrapper, or AppUnavailable, for FlowUi::ImageManager.
	 * @see @ref flowui_image_manager "Image Manager"
	 *
	 * @note Check success before continuing or reading a value. This noexcept App boundary returns operational failures as FlowUiError and preserves their attribution. Caller argument construction occurs before entry.
	 * @note The example first obtains a checked result, then a comfortable borrowed reference. The result/reference does not own or extend the manager lifetime. App-owned managers expire with App; window-owned managers also expire when that window is destroyed. Reacquire after moving/replacing the owning App.
	 *
	 * @par Example
	 * @code{.cpp}
	 * const FlowUi::App& readOnlyApplication = application;
	 * auto result = readOnlyApplication.images();
	 * if (!result) {
	 *     throw FlowUi::FlowUiException(result.error());
	 * }
	 * const FlowUi::ImageManager& images = result->get();
	 * @endcode
	 */
	[[nodiscard]] Result<std::reference_wrapper<const ImageManager>> images() const noexcept;
	/**
	 * @brief Access the theme manager.
	 *
	 * @return Borrowed reference wrapper, or AppUnavailable, for FlowUi::ThemeManager.
	 *
	 * @note Check success before continuing or reading a value. This noexcept App boundary returns operational failures as FlowUiError and preserves their attribution. Caller argument construction occurs before entry.
	 * @note The example first obtains a checked result, then a comfortable borrowed reference. The result/reference does not own or extend the manager lifetime. App-owned managers expire with App; window-owned managers also expire when that window is destroyed. Reacquire after moving/replacing the owning App.
	 *
	 * @par Example
	 * @code{.cpp}
	 * auto result = application.themes();
	 * if (!result) {
	 *     throw FlowUi::FlowUiException(result.error());
	 * }
	 * FlowUi::ThemeManager& themes = result->get();
	 * @endcode
	 */
	[[nodiscard]] Result<std::reference_wrapper<ThemeManager>> themes() noexcept;
	/**
	 * @brief Access the theme manager.
	 *
	 * @return Borrowed const reference wrapper, or AppUnavailable, for FlowUi::ThemeManager.
	 *
	 * @note Check success before continuing or reading a value. This noexcept App boundary returns operational failures as FlowUiError and preserves their attribution. Caller argument construction occurs before entry.
	 * @note The example first obtains a checked result, then a comfortable borrowed reference. The result/reference does not own or extend the manager lifetime. App-owned managers expire with App; window-owned managers also expire when that window is destroyed. Reacquire after moving/replacing the owning App.
	 *
	 * @par Example
	 * @code{.cpp}
	 * const FlowUi::App& readOnlyApplication = application;
	 * auto result = readOnlyApplication.themes();
	 * if (!result) {
	 *     throw FlowUi::FlowUiException(result.error());
	 * }
	 * const FlowUi::ThemeManager& themes = result->get();
	 * @endcode
	 */
	[[nodiscard]] Result<std::reference_wrapper<const ThemeManager>> themes() const noexcept;
	/**
	 * @brief Access the app-wide Flow element state/resource manager.
	 *
	 * @return Borrowed reference wrapper, or AppUnavailable, for FlowUi::ElementManager owned by this app.
	 * @see @ref flowui_element_system "Element System"
	 *
	 * @note Check success before continuing or reading a value. This noexcept App boundary returns operational failures as FlowUiError and preserves their attribution. Caller argument construction occurs before entry.
	 * @note The example first obtains a checked result, then a comfortable borrowed reference. The result/reference does not own or extend the manager lifetime. App-owned managers expire with App; window-owned managers also expire when that window is destroyed. Reacquire after moving/replacing the owning App.
	 *
	 * @par Example
	 * @code{.cpp}
	 * auto result = application.elements();
	 * if (!result) {
	 *     throw FlowUi::FlowUiException(result.error());
	 * }
	 * FlowUi::ElementManager& elements = result->get();
	 * @endcode
	 */
	[[nodiscard]] Result<std::reference_wrapper<ElementManager>> elements() noexcept;
	/**
	 * @brief Access the app-wide Flow element state/resource manager.
	 *
	 * @return Borrowed const reference wrapper, or AppUnavailable, for FlowUi::ElementManager owned by this app.
	 * @see @ref flowui_element_system "Element System"
	 *
	 * @note Check success before continuing or reading a value. This noexcept App boundary returns operational failures as FlowUiError and preserves their attribution. Caller argument construction occurs before entry.
	 * @note The example first obtains a checked result, then a comfortable borrowed reference. The result/reference does not own or extend the manager lifetime. App-owned managers expire with App; window-owned managers also expire when that window is destroyed. Reacquire after moving/replacing the owning App.
	 *
	 * @par Example
	 * @code{.cpp}
	 * const FlowUi::App& readOnlyApplication = application;
	 * auto result = readOnlyApplication.elements();
	 * if (!result) {
	 *     throw FlowUi::FlowUiException(result.error());
	 * }
	 * const FlowUi::ElementManager& elements = result->get();
	 * @endcode
	 */
	[[nodiscard]] Result<std::reference_wrapper<const ElementManager>> elements() const noexcept;
#if FLOW_UI_AGENTIC_WORKFLOW
	/**
	 * App-owned evidence coordinator; mutations are admitted at application safe points.
	 *
	 * @note Check success before continuing or reading a value. This noexcept App boundary returns operational failures as FlowUiError and preserves their attribution. Caller argument construction occurs before entry.
	 * @note The example first obtains a checked result, then a comfortable borrowed reference. The result/reference does not own or extend the manager lifetime. App-owned managers expire with App; window-owned managers also expire when that window is destroyed. Reacquire after moving/replacing the owning App.
	 * @note Available only when FLOW_UI_AGENTIC_WORKFLOW is enabled in the library build.
	 *
	 * @par Example
	 * @code{.cpp}
	 * #if FLOW_UI_AGENTIC_WORKFLOW
	 * auto result = application.agentic_workflow();
	 * if (!result) {
	 *     throw FlowUi::FlowUiException(result.error());
	 * }
	 * FlowUi::devSystems::agentic::AgenticWorkflowService& workflow = result->get();
	 * #endif
	 * @endcode
	 */
	[[nodiscard]] Result<std::reference_wrapper<devSystems::agentic::AgenticWorkflowService>> agentic_workflow() noexcept;
	/**
	 * Inspect the application's agentic service without activating it.
	 *
	 * @note Check success before continuing or reading a value. This noexcept App boundary returns operational failures as FlowUiError and preserves their attribution. Caller argument construction occurs before entry.
	 * @note The example first obtains a checked result, then a comfortable borrowed reference. The result/reference does not own or extend the manager lifetime. App-owned managers expire with App; window-owned managers also expire when that window is destroyed. Reacquire after moving/replacing the owning App.
	 * @note Available only when FLOW_UI_AGENTIC_WORKFLOW is enabled in the library build.
	 *
	 * @par Example
	 * @code{.cpp}
	 * #if FLOW_UI_AGENTIC_WORKFLOW
	 * const FlowUi::App& readOnlyApplication = application;
	 * auto result = readOnlyApplication.agentic_workflow();
	 * if (!result) {
	 *     throw FlowUi::FlowUiException(result.error());
	 * }
	 * const FlowUi::devSystems::agentic::AgenticWorkflowService& workflow = result->get();
	 * #endif
	 * @endcode
	 */
	[[nodiscard]] Result<std::reference_wrapper<const devSystems::agentic::AgenticWorkflowService>> agentic_workflow() const noexcept;
#endif
	/**
	 * @brief Access the app-wide action manager.
	 *
	 * @note Check success before continuing or reading a value. This noexcept App boundary returns operational failures as FlowUiError and preserves their attribution. Caller argument construction occurs before entry.
	 * @note The example first obtains a checked result, then a comfortable borrowed reference. The result/reference does not own or extend the manager lifetime. App-owned managers expire with App; window-owned managers also expire when that window is destroyed. Reacquire after moving/replacing the owning App.
	 *
	 * @par Example
	 * @code{.cpp}
	 * auto result = application.actions();
	 * if (!result) {
	 *     throw FlowUi::FlowUiException(result.error());
	 * }
	 * FlowUi::ActionManager& actions = result->get();
	 * @endcode
	 */
	[[nodiscard]] Result<std::reference_wrapper<ActionManager>> actions() noexcept;
	/**
	 * @brief Access the immutable app-wide action manager.
	 *
	 * @note Check success before continuing or reading a value. This noexcept App boundary returns operational failures as FlowUiError and preserves their attribution. Caller argument construction occurs before entry.
	 * @note The example first obtains a checked result, then a comfortable borrowed reference. The result/reference does not own or extend the manager lifetime. App-owned managers expire with App; window-owned managers also expire when that window is destroyed. Reacquire after moving/replacing the owning App.
	 *
	 * @par Example
	 * @code{.cpp}
	 * const FlowUi::App& readOnlyApplication = application;
	 * auto result = readOnlyApplication.actions();
	 * if (!result) {
	 *     throw FlowUi::FlowUiException(result.error());
	 * }
	 * const FlowUi::ActionManager& actions = result->get();
	 * @endcode
	 */
	[[nodiscard]] Result<std::reference_wrapper<const ActionManager>> actions() const noexcept;
#if FLOW_UI_DEV_MODE
	/**
	 * Queue a manual developer-interface toggle, equivalent to its panel shortcut.
	 * @note Available only when FLOW_UI_DEV_MODE is enabled in the library build.
	 *
	 * @par Example
	 * @code{.cpp}
	 * #if FLOW_UI_DEV_MODE
	 * application.request_dev_interface_toggle();
	 * #endif
	 * @endcode
	 */
	void request_dev_interface_toggle() noexcept;
	/**
	 * Validate capture intent against this application's panel shortcut.
	 *
	 * @note Check success before continuing or reading a value. This noexcept App boundary returns operational failures as FlowUiError and preserves their attribution. Caller argument construction occurs before entry.
	 * @note Available only when FLOW_UI_DEV_MODE is enabled in the library build.
	 *
	 * @par Example
	 * @code{.cpp}
	 * #if FLOW_UI_DEV_MODE
	 * FlowUi::devSystems::PerformanceCaptureSettings settings{};
	 * const auto status = application.request_dev_performance_capture(settings);
	 * if (!status) {
	 *     throw FlowUi::FlowUiException(status.error());
	 * }
	 * #endif
	 * @endcode
	 */
	[[nodiscard]] Status request_dev_performance_capture(
		const devSystems::PerformanceCaptureSettings& settings) noexcept;
	/**
	 * Suspend developer-window dispatch and visibility while preserving its session.
	 *
	 * @note Check success before continuing or reading a value. This noexcept App boundary returns operational failures as FlowUiError and preserves their attribution. Caller argument construction occurs before entry.
	 * @note Available only when FLOW_UI_DEV_MODE is enabled in the library build.
	 *
	 * @par Example
	 * @code{.cpp}
	 * #if FLOW_UI_DEV_MODE
	 * const auto status = application.set_dev_window_suspended(window, true);
	 * if (!status) {
	 *     throw FlowUi::FlowUiException(status.error());
	 * }
	 * #endif
	 * @endcode
	 */
	[[nodiscard]] Status set_dev_window_suspended(WindowId window, bool suspended) noexcept;
	/**
	 * Poll a configured chord on focused native application windows before UI work.
	 *
	 * @note Check success before continuing or reading a value. This noexcept App boundary returns operational failures as FlowUiError and preserves their attribution. Caller argument construction occurs before entry.
	 * @note Available only when FLOW_UI_DEV_MODE is enabled in the library build.
	 *
	 * @par Example
	 * @code{.cpp}
	 * #if FLOW_UI_DEV_MODE
	 * FlowUi::DevShortcutChord chord{};
	 * auto result = application.dev_shortcut_down(chord);
	 * if (!result) {
	 *     throw FlowUi::FlowUiException(result.error());
	 * }
	 * const bool chordDown = *result;
	 * #endif
	 * @endcode
	 */
	[[nodiscard]] Result<bool> dev_shortcut_down(const DevShortcutChord& chord) const noexcept;
	/**
	 * Read a native press edge before UI dispatch; repeat events are excluded.
	 *
	 * @note Check success before continuing or reading a value. This noexcept App boundary returns operational failures as FlowUiError and preserves their attribution. Caller argument construction occurs before entry.
	 * @note Available only when FLOW_UI_DEV_MODE is enabled in the library build.
	 *
	 * @par Example
	 * @code{.cpp}
	 * #if FLOW_UI_DEV_MODE
	 * FlowUi::DevShortcutChord chord{};
	 * auto result = application.dev_shortcut_pressed(chord);
	 * if (!result) {
	 *     throw FlowUi::FlowUiException(result.error());
	 * }
	 * const bool chordPressed = *result;
	 * #endif
	 * @endcode
	 */
	[[nodiscard]] Result<bool> dev_shortcut_pressed(const DevShortcutChord& chord) const noexcept;
	/**
	 * Development-only access to timing monitoring and retained reports.
	 *
	 * @note Check success before continuing or reading a value. This noexcept App boundary returns operational failures as FlowUiError and preserves their attribution. Caller argument construction occurs before entry.
	 * @note The example first obtains a checked result, then a comfortable borrowed reference. The result/reference does not own or extend the manager lifetime. App-owned managers expire with App; window-owned managers also expire when that window is destroyed. Reacquire after moving/replacing the owning App.
	 * @note Available only when FLOW_UI_DEV_MODE is enabled in the library build.
	 *
	 * @par Example
	 * @code{.cpp}
	 * #if FLOW_UI_DEV_MODE
	 * auto result = application.devMonitoring();
	 * if (!result) {
	 *     throw FlowUi::FlowUiException(result.error());
	 * }
	 * FlowUi::devSystems::DevMonitoringAndReporting& monitoring = result->get();
	 * #endif
	 * @endcode
	 */
	[[nodiscard]] Result<std::reference_wrapper<devSystems::DevMonitoringAndReporting>> devMonitoring() noexcept;
	/**
	 * @brief Borrow immutable development timing, memory, and error reporting services.
	 *
	 * @note Check success before continuing or reading a value. This noexcept App boundary returns operational failures as FlowUiError and preserves their attribution. Caller argument construction occurs before entry.
	 * @note The example first obtains a checked result, then a comfortable borrowed reference. The result/reference does not own or extend the manager lifetime. App-owned managers expire with App; window-owned managers also expire when that window is destroyed. Reacquire after moving/replacing the owning App.
	 * @note Available only when FLOW_UI_DEV_MODE is enabled in the library build.
	 *
	 * @par Example
	 * @code{.cpp}
	 * #if FLOW_UI_DEV_MODE
	 * const FlowUi::App& readOnlyApplication = application;
	 * auto result = readOnlyApplication.devMonitoring();
	 * if (!result) {
	 *     throw FlowUi::FlowUiException(result.error());
	 * }
	 * const FlowUi::devSystems::DevMonitoringAndReporting& monitoring = result->get();
	 * #endif
	 * @endcode
	 */
	[[nodiscard]] Result<std::reference_wrapper<const devSystems::DevMonitoringAndReporting>> devMonitoring() const noexcept;
	/**
	 * Development-only schema catalogue, discovery, and publication service.
	 *
	 * @note Check success before continuing or reading a value. This noexcept App boundary returns operational failures as FlowUiError and preserves their attribution. Caller argument construction occurs before entry.
	 * @note The example first obtains a checked result, then a comfortable borrowed reference. The result/reference does not own or extend the manager lifetime. App-owned managers expire with App; window-owned managers also expire when that window is destroyed. Reacquire after moving/replacing the owning App.
	 * @note Available only when FLOW_UI_DEV_MODE is enabled in the library build.
	 *
	 * @par Example
	 * @code{.cpp}
	 * #if FLOW_UI_DEV_MODE
	 * auto result = application.devTooling();
	 * if (!result) {
	 *     throw FlowUi::FlowUiException(result.error());
	 * }
	 * FlowUi::devSystems::DevTooling& tooling = result->get();
	 * #endif
	 * @endcode
	 */
	[[nodiscard]] Result<std::reference_wrapper<devSystems::DevTooling>> devTooling() noexcept;
	/**
	 * @brief Borrow immutable development schema, discovery, and publication services.
	 *
	 * @note Check success before continuing or reading a value. This noexcept App boundary returns operational failures as FlowUiError and preserves their attribution. Caller argument construction occurs before entry.
	 * @note The example first obtains a checked result, then a comfortable borrowed reference. The result/reference does not own or extend the manager lifetime. App-owned managers expire with App; window-owned managers also expire when that window is destroyed. Reacquire after moving/replacing the owning App.
	 * @note Available only when FLOW_UI_DEV_MODE is enabled in the library build.
	 *
	 * @par Example
	 * @code{.cpp}
	 * #if FLOW_UI_DEV_MODE
	 * const FlowUi::App& readOnlyApplication = application;
	 * auto result = readOnlyApplication.devTooling();
	 * if (!result) {
	 *     throw FlowUi::FlowUiException(result.error());
	 * }
	 * const FlowUi::devSystems::DevTooling& tooling = result->get();
	 * #endif
	 * @endcode
	 */
	[[nodiscard]] Result<std::reference_wrapper<const devSystems::DevTooling>> devTooling() const noexcept;
	/**
	 * Snapshot registered windows in stable identity order for development UI.
	 *
	 * @note Check success before continuing or reading a value. This noexcept App boundary returns operational failures as FlowUiError and preserves their attribution. Caller argument construction occurs before entry.
	 * @note Available only when FLOW_UI_DEV_MODE is enabled in the library build.
	 *
	 * @par Example
	 * @code{.cpp}
	 * #if FLOW_UI_DEV_MODE
	 * auto result = application.devWindowSnapshot();
	 * if (!result) {
	 *     throw FlowUi::FlowUiException(result.error());
	 * }
	 * auto windows = std::move(*result);
	 * #endif
	 * @endcode
	 */
	[[nodiscard]] Result<std::vector<DevWindowInfo>> devWindowSnapshot() const noexcept;
	/**
	 * Observe window-local viewport metadata independently of public Vulkan interop.
	 *
	 * @note Check success before continuing or reading a value. This noexcept App boundary returns operational failures as FlowUiError and preserves their attribution. Caller argument construction occurs before entry.
	 * @note Available only when FLOW_UI_DEV_MODE is enabled in the library build.
	 *
	 * @par Example
	 * @code{.cpp}
	 * #if FLOW_UI_DEV_MODE
	 * auto result = application.devViewportSnapshot(window);
	 * if (!result) {
	 *     throw FlowUi::FlowUiException(result.error());
	 * }
	 * auto viewports = std::move(*result);
	 * #endif
	 * @endcode
	 */
	[[nodiscard]] Result<std::vector<DevViewportInfo>> devViewportSnapshot(WindowId window) const noexcept;
	/**
	 * Request one immutable GPU copy at the viewport's next natural source render.
	 *
	 * @note Check success before continuing or reading a value. This noexcept App boundary returns operational failures as FlowUiError and preserves their attribution. Caller argument construction occurs before entry.
	 * @note Available only when FLOW_UI_DEV_MODE is enabled in the library build.
	 *
	 * @par Example
	 * @code{.cpp}
	 * #if FLOW_UI_DEV_MODE
	 * const std::string_view key = "preview/scene";
	 * const auto status = application.request_dev_viewport_capture(window, key);
	 * if (!status) {
	 *     throw FlowUi::FlowUiException(status.error());
	 * }
	 * #endif
	 * @endcode
	 */
	[[nodiscard]] Status request_dev_viewport_capture(WindowId window,
													  std::string_view key) noexcept;
	/**
	 * Retire this tool's backend capture cache; submitted consumers retain their own leases.
	 * @note Available only when FLOW_UI_DEV_MODE is enabled in the library build.
	 *
	 * @par Example
	 * @code{.cpp}
	 * #if FLOW_UI_DEV_MODE
	 * const std::string_view key = "preview/scene";
	 * application.release_dev_viewport_capture(window, key);
	 * #endif
	 * @endcode
	 */
	void release_dev_viewport_capture(WindowId window, std::string_view key) noexcept;
	/**
	 * Own an independent binding of a completed immutable copy; survives source removal.
	 *
	 * @note Check success before continuing or reading a value. This noexcept App boundary returns operational failures as FlowUiError and preserves their attribution. Caller argument construction occurs before entry.
	 * @note Available only when FLOW_UI_DEV_MODE is enabled in the library build.
	 *
	 * @par Example
	 * @code{.cpp}
	 * #if FLOW_UI_DEV_MODE
	 * const std::string_view key = "preview/scene";
	 * auto result = application.retain_dev_viewport_capture(window, key, expectedCapture);
	 * if (!result) {
	 *     throw FlowUi::FlowUiException(result.error());
	 * }
	 * const FlowUi::TextureHandle retainedCapture = *result;
	 * #endif
	 * @endcode
	 */
	[[nodiscard]] Result<TextureHandle>
	retain_dev_viewport_capture(WindowId window, std::string_view key,
								TextureHandle expected_capture) noexcept;
	/**
	 * Release a binding returned by retain_dev_viewport_capture().
	 *
	 * @note Check success before continuing or reading a value. This noexcept App boundary returns operational failures as FlowUiError and preserves their attribution. Caller argument construction occurs before entry.
	 * @note Available only when FLOW_UI_DEV_MODE is enabled in the library build.
	 *
	 * @par Example
	 * @code{.cpp}
	 * #if FLOW_UI_DEV_MODE
	 * const auto status = application.release_dev_viewport_capture_texture(capture);
	 * if (!status) {
	 *     throw FlowUi::FlowUiException(status.error());
	 * }
	 * #endif
	 * @endcode
	 */
	[[nodiscard]] Status release_dev_viewport_capture_texture(TextureHandle texture) noexcept;
	/**
	 * Return the latest owned raw-command packet captured for development replay.
	 * @note Available only when FLOW_UI_DEV_MODE is enabled in the library build.
	 *
	 * @par Example
	 * @code{.cpp}
	 * #if FLOW_UI_DEV_MODE
	 * const auto replaySource = application.devUiReplaySource(window);
	 * #endif
	 * @endcode
	 */
	[[nodiscard]] devSystems::DevUiReplaySource devUiReplaySource(WindowId id) noexcept;
	/**
	 * Request capture of raw Clay commands owned by the supplied subtree IDs.
	 *
	 * @note Check success before continuing or reading a value. This noexcept App boundary returns operational failures as FlowUiError and preserves their attribution. Caller argument construction occurs before entry.
	 * @note Available only when FLOW_UI_DEV_MODE is enabled in the library build.
	 *
	 * @par Example
	 * @code{.cpp}
	 * #if FLOW_UI_DEV_MODE
	 * const auto status = application.requestDevUiReplay(window, requestKey, commandIds, rootBounds);
	 * if (!status) {
	 *     throw FlowUi::FlowUiException(status.error());
	 * }
	 * #endif
	 * @endcode
	 */
	[[nodiscard]] Status requestDevUiReplay(WindowId id, uint64_t requestKey,
							std::span<const uint32_t> clayCommandIds,
							Clay_BoundingBox sourceRootBounds) noexcept;
#endif
#if FLOWUI_INCLUDE_ICON_MANAGER
	/**
	 * @brief Access the icon manager.
	 *
	 * @note FlowUi must be built with FLOWUI_INCLUDE_ICON_MANAGER enabled to use IconManager.
	 *
	 * @return Borrowed reference wrapper, or AppUnavailable, for FlowUi::IconManager.
	 * @see @ref flowui_icon_manager "Icon Manager"
	 *
	 * @note Check success before continuing or reading a value. This noexcept App boundary returns operational failures as FlowUiError and preserves their attribution. Caller argument construction occurs before entry.
	 * @note The example first obtains a checked result, then a comfortable borrowed reference. The result/reference does not own or extend the manager lifetime. App-owned managers expire with App; window-owned managers also expire when that window is destroyed. Reacquire after moving/replacing the owning App.
	 * @note Available only when FLOWUI_INCLUDE_ICON_MANAGER is enabled in the library build.
	 *
	 * @par Example
	 * @code{.cpp}
	 * #if FLOWUI_INCLUDE_ICON_MANAGER
	 * auto result = application.icons();
	 * if (!result) {
	 *     throw FlowUi::FlowUiException(result.error());
	 * }
	 * FlowUi::IconManager& icons = result->get();
	 * #endif
	 * @endcode
	 */
	[[nodiscard]] Result<std::reference_wrapper<IconManager>> icons() noexcept;
	/**
	 * @brief Access the icon manager.
	 *
	 * @note FlowUi must be built with FLOWUI_INCLUDE_ICON_MANAGER enabled to use IconManager.
	 *
	 * @return Borrowed const reference wrapper, or AppUnavailable, for FlowUi::IconManager.
	 * @see @ref flowui_icon_manager "Icon Manager"
	 *
	 * @note Check success before continuing or reading a value. This noexcept App boundary returns operational failures as FlowUiError and preserves their attribution. Caller argument construction occurs before entry.
	 * @note The example first obtains a checked result, then a comfortable borrowed reference. The result/reference does not own or extend the manager lifetime. App-owned managers expire with App; window-owned managers also expire when that window is destroyed. Reacquire after moving/replacing the owning App.
	 * @note Available only when FLOWUI_INCLUDE_ICON_MANAGER is enabled in the library build.
	 *
	 * @par Example
	 * @code{.cpp}
	 * #if FLOWUI_INCLUDE_ICON_MANAGER
	 * const FlowUi::App& readOnlyApplication = application;
	 * auto result = readOnlyApplication.icons();
	 * if (!result) {
	 *     throw FlowUi::FlowUiException(result.error());
	 * }
	 * const FlowUi::IconManager& icons = result->get();
	 * #endif
	 * @endcode
	 */
	[[nodiscard]] Result<std::reference_wrapper<const IconManager>> icons() const noexcept;
#endif
#if FLOWUI_PUBLIC_VULKAN_INTEROP
	/**
	 * @brief Access the viewport manager.
	 *
	 * @note FlowUi must be built with FLOWUI_PUBLIC_VULKAN_INTEROP enabled to use ViewPortManager.
	 *
	 * @return Borrowed reference wrapper, or an unavailable-app/window access error, for FlowUi::ViewPortManager.
	 * @see @ref flowui_viewport_manager "ViewPort Manager"
	 *
	 * @note Check success before continuing or reading a value. This noexcept App boundary returns operational failures as FlowUiError and preserves their attribution. Caller argument construction occurs before entry.
	 * @note The example first obtains a checked result, then a comfortable borrowed reference. The result/reference does not own or extend the manager lifetime. App-owned managers expire with App; window-owned managers also expire when that window is destroyed. Reacquire after moving/replacing the owning App.
	 * @note Available only when FLOWUI_PUBLIC_VULKAN_INTEROP is enabled in the library build.
	 *
	 * @par Example
	 * @code{.cpp}
	 * #if FLOWUI_PUBLIC_VULKAN_INTEROP
	 * auto result = application.viewPorts();
	 * if (!result) {
	 *     throw FlowUi::FlowUiException(result.error());
	 * }
	 * FlowUi::ViewPortManager& viewports = result->get();
	 * #endif
	 * @endcode
	 */
	[[nodiscard]] Result<std::reference_wrapper<ViewPortManager>> viewPorts() noexcept;
	/**
	 * @brief Access the viewport manager owned by a specific window.
	 *
	 * @param id Registered window identity.
	 * @return Borrowed mutable viewport manager wrapper, or an app/window access error.
	 *
	 * @note Check success before continuing or reading a value. This noexcept App boundary returns operational failures as FlowUiError and preserves their attribution. Caller argument construction occurs before entry.
	 * @note The example first obtains a checked result, then a comfortable borrowed reference. The result/reference does not own or extend the manager lifetime. App-owned managers expire with App; window-owned managers also expire when that window is destroyed. Reacquire after moving/replacing the owning App.
	 * @note Available only when FLOWUI_PUBLIC_VULKAN_INTEROP is enabled in the library build.
	 *
	 * @par Example
	 * @code{.cpp}
	 * #if FLOWUI_PUBLIC_VULKAN_INTEROP
	 * auto result = application.viewPorts(window);
	 * if (!result) {
	 *     throw FlowUi::FlowUiException(result.error());
	 * }
	 * FlowUi::ViewPortManager& viewports = result->get();
	 * #endif
	 * @endcode
	 */
	[[nodiscard]] Result<std::reference_wrapper<ViewPortManager>> viewPorts(WindowId id) noexcept;
	/**
	 * @brief Access the viewport manager.
	 *
	 * @note FlowUi must be built with FLOWUI_PUBLIC_VULKAN_INTEROP enabled to use ViewPortManager.
	 *
	 * @return Borrowed const reference wrapper, or an unavailable-app/window access error, for FlowUi::ViewPortManager.
	 * @see @ref flowui_viewport_manager "ViewPort Manager"
	 *
	 * @note Check success before continuing or reading a value. This noexcept App boundary returns operational failures as FlowUiError and preserves their attribution. Caller argument construction occurs before entry.
	 * @note The example first obtains a checked result, then a comfortable borrowed reference. The result/reference does not own or extend the manager lifetime. App-owned managers expire with App; window-owned managers also expire when that window is destroyed. Reacquire after moving/replacing the owning App.
	 * @note Available only when FLOWUI_PUBLIC_VULKAN_INTEROP is enabled in the library build.
	 *
	 * @par Example
	 * @code{.cpp}
	 * #if FLOWUI_PUBLIC_VULKAN_INTEROP
	 * const FlowUi::App& readOnlyApplication = application;
	 * auto result = readOnlyApplication.viewPorts();
	 * if (!result) {
	 *     throw FlowUi::FlowUiException(result.error());
	 * }
	 * const FlowUi::ViewPortManager& viewports = result->get();
	 * #endif
	 * @endcode
	 */
	[[nodiscard]] Result<std::reference_wrapper<const ViewPortManager>> viewPorts() const noexcept;
	/**
	 * @brief Access the immutable viewport manager owned by a specific window.
	 *
	 * @param id Registered window identity.
	 * @return Borrowed immutable viewport manager wrapper, or an app/window access error.
	 *
	 * @note Check success before continuing or reading a value. This noexcept App boundary returns operational failures as FlowUiError and preserves their attribution. Caller argument construction occurs before entry.
	 * @note The example first obtains a checked result, then a comfortable borrowed reference. The result/reference does not own or extend the manager lifetime. App-owned managers expire with App; window-owned managers also expire when that window is destroyed. Reacquire after moving/replacing the owning App.
	 * @note Available only when FLOWUI_PUBLIC_VULKAN_INTEROP is enabled in the library build.
	 *
	 * @par Example
	 * @code{.cpp}
	 * #if FLOWUI_PUBLIC_VULKAN_INTEROP
	 * const FlowUi::App& readOnlyApplication = application;
	 * auto result = readOnlyApplication.viewPorts(window);
	 * if (!result) {
	 *     throw FlowUi::FlowUiException(result.error());
	 * }
	 * const FlowUi::ViewPortManager& viewports = result->get();
	 * #endif
	 * @endcode
	 */
	[[nodiscard]] Result<std::reference_wrapper<const ViewPortManager>> viewPorts(WindowId id) const noexcept;
#endif

	/**
	 * @brief Access the UI manager.
	 *
	 * @return Borrowed reference wrapper, or an unavailable-app/window access error, for FlowUi::UiManager.
	 * @see @ref flowui_ui_manager "UI Manager"
	 *
	 * @note Check success before continuing or reading a value. This noexcept App boundary returns operational failures as FlowUiError and preserves their attribution. Caller argument construction occurs before entry.
	 * @note The example first obtains a checked result, then a comfortable borrowed reference. The result/reference does not own or extend the manager lifetime. App-owned managers expire with App; window-owned managers also expire when that window is destroyed. Reacquire after moving/replacing the owning App.
	 *
	 * @par Example
	 * @code{.cpp}
	 * auto result = application.ui();
	 * if (!result) {
	 *     throw FlowUi::FlowUiException(result.error());
	 * }
	 * FlowUi::UiManager& ui = result->get();
	 * @endcode
	 */
	[[nodiscard]] Result<std::reference_wrapper<UiManager>> ui() noexcept;
	/**
	 * @brief Access the UI manager owned by a specific window.
	 *
	 * @param id Registered window identity.
	 * @return Borrowed mutable UI manager wrapper, or an app/window access error.
	 *
	 * @note Check success before continuing or reading a value. This noexcept App boundary returns operational failures as FlowUiError and preserves their attribution. Caller argument construction occurs before entry.
	 * @note The example first obtains a checked result, then a comfortable borrowed reference. The result/reference does not own or extend the manager lifetime. App-owned managers expire with App; window-owned managers also expire when that window is destroyed. Reacquire after moving/replacing the owning App.
	 *
	 * @par Example
	 * @code{.cpp}
	 * auto result = application.ui(window);
	 * if (!result) {
	 *     throw FlowUi::FlowUiException(result.error());
	 * }
	 * FlowUi::UiManager& ui = result->get();
	 * @endcode
	 */
	[[nodiscard]] Result<std::reference_wrapper<UiManager>> ui(WindowId id) noexcept;
	/**
	 * @brief Access the UI manager.
	 *
	 * @return Borrowed const reference wrapper, or an unavailable-app/window access error, for FlowUi::UiManager.
	 * @see @ref flowui_ui_manager "UI Manager"
	 *
	 * @note Check success before continuing or reading a value. This noexcept App boundary returns operational failures as FlowUiError and preserves their attribution. Caller argument construction occurs before entry.
	 * @note The example first obtains a checked result, then a comfortable borrowed reference. The result/reference does not own or extend the manager lifetime. App-owned managers expire with App; window-owned managers also expire when that window is destroyed. Reacquire after moving/replacing the owning App.
	 *
	 * @par Example
	 * @code{.cpp}
	 * const FlowUi::App& readOnlyApplication = application;
	 * auto result = readOnlyApplication.ui();
	 * if (!result) {
	 *     throw FlowUi::FlowUiException(result.error());
	 * }
	 * const FlowUi::UiManager& ui = result->get();
	 * @endcode
	 */
	[[nodiscard]] Result<std::reference_wrapper<const UiManager>> ui() const noexcept;
	/**
	 * @brief Access the immutable UI manager owned by a specific window.
	 *
	 * @param id Registered window identity.
	 * @return Borrowed immutable UI manager wrapper, or an app/window access error.
	 *
	 * @note Check success before continuing or reading a value. This noexcept App boundary returns operational failures as FlowUiError and preserves their attribution. Caller argument construction occurs before entry.
	 * @note The example first obtains a checked result, then a comfortable borrowed reference. The result/reference does not own or extend the manager lifetime. App-owned managers expire with App; window-owned managers also expire when that window is destroyed. Reacquire after moving/replacing the owning App.
	 *
	 * @par Example
	 * @code{.cpp}
	 * const FlowUi::App& readOnlyApplication = application;
	 * auto result = readOnlyApplication.ui(window);
	 * if (!result) {
	 *     throw FlowUi::FlowUiException(result.error());
	 * }
	 * const FlowUi::UiManager& ui = result->get();
	 * @endcode
	 */
	[[nodiscard]] Result<std::reference_wrapper<const UiManager>> ui(WindowId id) const noexcept;

	/**
	 * @brief Set the native window title.
	 *
	 * The initial window title is set from FlowUi::WindowConfig by default.
	 *
	 * @param title String view of the new title.
	 *
	 * @see FlowUi::WindowConfig
	 * @see @ref flowui_config "Config Structs"
	 *
	 * @note Check success before continuing or reading a value. This noexcept App boundary returns operational failures as FlowUiError and preserves their attribution. Caller argument construction occurs before entry.
	 *
	 * @par Example
	 * @code{.cpp}
	 * const auto status = application.setWindowTitle("Inspector — ready");
	 * if (!status) {
	 *     throw FlowUi::FlowUiException(status.error());
	 * }
	 * @endcode
	 */
	[[nodiscard]] Status setWindowTitle(std::string_view title) noexcept;
	/**
	 * @brief Set the native title of a specific window.
	 *
	 * @param id Registered window identity.
	 * @param title New native-window title.
	 *
	 * @note Check success before continuing or reading a value. This noexcept App boundary returns operational failures as FlowUiError and preserves their attribution. Caller argument construction occurs before entry.
	 *
	 * @par Example
	 * @code{.cpp}
	 * const auto status = application.setWindowTitle(window, "Inspector — ready");
	 * if (!status) {
	 *     throw FlowUi::FlowUiException(status.error());
	 * }
	 * @endcode
	 */
	[[nodiscard]] Status setWindowTitle(WindowId id, std::string_view title) noexcept;
	/**
	 * @brief Return the window size in screen coordinates.
	 *
	 * @return A (width, height) pair, or an app/window/thread access error.
	 *
	 * @note Check success before continuing or reading a value. This noexcept App boundary returns operational failures as FlowUiError and preserves their attribution. Caller argument construction occurs before entry.
	 *
	 * @par Example
	 * @code{.cpp}
	 * auto result = application.windowSize();
	 * if (!result) {
	 *     throw FlowUi::FlowUiException(result.error());
	 * }
	 * const auto [width, height] = *result;
	 * @endcode
	 */
	[[nodiscard]] Result<std::pair<int, int>> windowSize() const noexcept;
	/**
	 * @brief Return a specific window's size in screen coordinates.
	 *
	 * @param id Registered window identity.
	 * @return A (width, height) pair, or an app/window/thread access error.
	 *
	 * @note Check success before continuing or reading a value. This noexcept App boundary returns operational failures as FlowUiError and preserves their attribution. Caller argument construction occurs before entry.
	 *
	 * @par Example
	 * @code{.cpp}
	 * auto result = application.windowSize(window);
	 * if (!result) {
	 *     throw FlowUi::FlowUiException(result.error());
	 * }
	 * const auto [width, height] = *result;
	 * @endcode
	 */
	[[nodiscard]] Result<std::pair<int, int>> windowSize(WindowId id) const noexcept;
	/**
	 * @brief Return the framebuffer size in pixels.
	 *
	 * @return A (width, height) pair, or an app/window/thread access error.
	 *
	 * @note Check success before continuing or reading a value. This noexcept App boundary returns operational failures as FlowUiError and preserves their attribution. Caller argument construction occurs before entry.
	 *
	 * @par Example
	 * @code{.cpp}
	 * auto result = application.framebufferSize();
	 * if (!result) {
	 *     throw FlowUi::FlowUiException(result.error());
	 * }
	 * const auto [framebufferWidth, framebufferHeight] = *result;
	 * @endcode
	 */
	[[nodiscard]] Result<std::pair<int, int>> framebufferSize() const noexcept;
	/**
	 * @brief Return a specific window's framebuffer size in pixels.
	 *
	 * @param id Registered window identity.
	 * @return A (width, height) pair, or an app/window/thread access error.
	 *
	 * @note Check success before continuing or reading a value. This noexcept App boundary returns operational failures as FlowUiError and preserves their attribution. Caller argument construction occurs before entry.
	 *
	 * @par Example
	 * @code{.cpp}
	 * auto result = application.framebufferSize(window);
	 * if (!result) {
	 *     throw FlowUi::FlowUiException(result.error());
	 * }
	 * const auto [framebufferWidth, framebufferHeight] = *result;
	 * @endcode
	 */
	[[nodiscard]] Result<std::pair<int, int>> framebufferSize(WindowId id) const noexcept;
	/**
	 * @brief Apply window input configuration.
	 *
	 * The initial window input configuration is set from FlowUi::WindowConfig by default.
	 *
	 * @param config Const reference to the WindowInputConfig struct to activate.
	 *
	 * @see FlowUi::WindowConfig
	 * @see FlowUi::WindowInputConfig
	 * @see @ref flowui_config "Config Structs"
	 *
	 * @note Check success before continuing or reading a value. This noexcept App boundary returns operational failures as FlowUiError and preserves their attribution. Caller argument construction occurs before entry.
	 *
	 * @par Example
	 * @code{.cpp}
	 * FlowUi::WindowInputConfig input{};
	 * const auto status = application.setWindowInputConfig(input);
	 * if (!status) {
	 *     throw FlowUi::FlowUiException(status.error());
	 * }
	 * @endcode
	 */
	[[nodiscard]] Status setWindowInputConfig(const WindowInputConfig& config) noexcept;
	/**
	 * @brief Apply input configuration to a specific window.
	 *
	 * @param id Registered window identity.
	 * @param config Input configuration to apply.
	 *
	 * @note Check success before continuing or reading a value. This noexcept App boundary returns operational failures as FlowUiError and preserves their attribution. Caller argument construction occurs before entry.
	 *
	 * @par Example
	 * @code{.cpp}
	 * FlowUi::WindowInputConfig input{};
	 * const auto status = application.setWindowInputConfig(window, input);
	 * if (!status) {
	 *     throw FlowUi::FlowUiException(status.error());
	 * }
	 * @endcode
	 */
	[[nodiscard]] Status setWindowInputConfig(WindowId id, const WindowInputConfig& config) noexcept;
	/**
	 * @brief Return the current window input configuration.
	 *
	 * @return Active input configuration, or an app/window/thread access error.
	 *
	 * @see FlowUi::WindowInputConfig
	 * @see @ref flowui_config "Config Structs"
	 *
	 * @note Check success before continuing or reading a value. This noexcept App boundary returns operational failures as FlowUiError and preserves their attribution. Caller argument construction occurs before entry.
	 *
	 * @par Example
	 * @code{.cpp}
	 * auto result = application.windowInputConfig();
	 * if (!result) {
	 *     throw FlowUi::FlowUiException(result.error());
	 * }
	 * const FlowUi::WindowInputConfig inputConfiguration = *result;
	 * @endcode
	 */
	[[nodiscard]] Result<WindowInputConfig> windowInputConfig() const noexcept;
	/**
	 * @brief Return a specific window's current input configuration.
	 *
	 * @param id Registered window identity.
	 * @return Active input configuration, or an app/window/thread access error.
	 *
	 * @note Check success before continuing or reading a value. This noexcept App boundary returns operational failures as FlowUiError and preserves their attribution. Caller argument construction occurs before entry.
	 *
	 * @par Example
	 * @code{.cpp}
	 * auto result = application.windowInputConfig(window);
	 * if (!result) {
	 *     throw FlowUi::FlowUiException(result.error());
	 * }
	 * const FlowUi::WindowInputConfig inputConfiguration = *result;
	 * @endcode
	 */
	[[nodiscard]] Result<WindowInputConfig> windowInputConfig(WindowId id) const noexcept;
	/**
	 * @brief Return the backend native window handle.
	 *
	 * @return Borrowed native backend handle, or an app/window/thread access error.
	 *
	 * @note The concrete handle type depends on the active window backend.
	 *
	 * @note Check success before continuing or reading a value. This noexcept App boundary returns operational failures as FlowUiError and preserves their attribution. Caller argument construction occurs before entry.
	 *
	 * @par Example
	 * @code{.cpp}
	 * auto result = application.nativeWindowHandle();
	 * if (!result) {
	 *     throw FlowUi::FlowUiException(result.error());
	 * }
	 * void* nativeHandle = *result;
	 * @endcode
	 */
	[[nodiscard]] Result<void*> nativeWindowHandle() const noexcept;
	/**
	 * @brief Return a specific window's backend-native handle.
	 *
	 * @param id Registered window identity.
	 * @return Borrowed native backend handle, or an app/window/thread access error.
	 *
	 * @note Check success before continuing or reading a value. This noexcept App boundary returns operational failures as FlowUiError and preserves their attribution. Caller argument construction occurs before entry.
	 *
	 * @par Example
	 * @code{.cpp}
	 * auto result = application.nativeWindowHandle(window);
	 * if (!result) {
	 *     throw FlowUi::FlowUiException(result.error());
	 * }
	 * void* nativeHandle = *result;
	 * @endcode
	 */
	[[nodiscard]] Result<void*> nativeWindowHandle(WindowId id) const noexcept;
	/**
	 * @brief Return whether the window backend supports raw mouse motion.
	 *
	 * @return Raw-motion capability on success, or an app/window/thread access error.
	 *
	 * @note Check success before continuing or reading a value. This noexcept App boundary returns operational failures as FlowUiError and preserves their attribution. Caller argument construction occurs before entry.
	 *
	 * @par Example
	 * @code{.cpp}
	 * auto result = application.supportsRawMouseMotion();
	 * if (!result) {
	 *     throw FlowUi::FlowUiException(result.error());
	 * }
	 * const bool rawMotionSupported = *result;
	 * @endcode
	 */
	[[nodiscard]] Result<bool> supportsRawMouseMotion() const noexcept;
	/**
	 * @brief Return whether a specific window supports raw mouse motion.
	 *
	 * @param id Registered window identity.
	 *
	 * @note Check success before continuing or reading a value. This noexcept App boundary returns operational failures as FlowUiError and preserves their attribution. Caller argument construction occurs before entry.
	 *
	 * @par Example
	 * @code{.cpp}
	 * auto result = application.supportsRawMouseMotion(window);
	 * if (!result) {
	 *     throw FlowUi::FlowUiException(result.error());
	 * }
	 * const bool rawMotionSupported = *result;
	 * @endcode
	 */
	[[nodiscard]] Result<bool> supportsRawMouseMotion(WindowId id) const noexcept;
	/**
	 * @brief Set clipboard text through the window backend.
	 *
	 * @param text String view for the text to set in the system clipboard.
	 *
	 * @note Check success before continuing or reading a value. This noexcept App boundary returns operational failures as FlowUiError and preserves their attribution. Caller argument construction occurs before entry.
	 *
	 * @par Example
	 * @code{.cpp}
	 * const auto status = application.setClipboardText("Copied from FlowUi");
	 * if (!status) {
	 *     throw FlowUi::FlowUiException(status.error());
	 * }
	 * @endcode
	 */
	[[nodiscard]] Status setClipboardText(std::string_view text) noexcept;
	/**
	 * @brief Set clipboard text through a specific window backend.
	 *
	 * @param id Registered window identity.
	 * @param text Text to place in the system clipboard.
	 *
	 * @note Check success before continuing or reading a value. This noexcept App boundary returns operational failures as FlowUiError and preserves their attribution. Caller argument construction occurs before entry.
	 *
	 * @par Example
	 * @code{.cpp}
	 * const auto status = application.setClipboardText(window, "Copied from FlowUi");
	 * if (!status) {
	 *     throw FlowUi::FlowUiException(status.error());
	 * }
	 * @endcode
	 */
	[[nodiscard]] Status setClipboardText(WindowId id, std::string_view text) noexcept;
	/**
	 * @brief Read clipboard text through the window backend.
	 *
	 * @return Owned clipboard text, or a structured app/window/thread/clipboard error.
	 *
	 * @note Check success before continuing or reading a value. This noexcept App boundary returns operational failures as FlowUiError and preserves their attribution. Caller argument construction occurs before entry.
	 *
	 * @par Example
	 * @code{.cpp}
	 * auto result = application.clipboardText();
	 * if (!result) {
	 *     throw FlowUi::FlowUiException(result.error());
	 * }
	 * std::string text = std::move(*result);
	 * @endcode
	 */
	[[nodiscard]] Result<std::string> clipboardText() const noexcept;
	/**
	 * @brief Read clipboard text through a specific window backend.
	 *
	 * @param id Registered window identity.
	 * @return Owned clipboard text, or a structured app/window/thread/clipboard error.
	 *
	 * @note Check success before continuing or reading a value. This noexcept App boundary returns operational failures as FlowUiError and preserves their attribution. Caller argument construction occurs before entry.
	 *
	 * @par Example
	 * @code{.cpp}
	 * auto result = application.clipboardText(window);
	 * if (!result) {
	 *     throw FlowUi::FlowUiException(result.error());
	 * }
	 * std::string text = std::move(*result);
	 * @endcode
	 */
	[[nodiscard]] Result<std::string> clipboardText(WindowId id) const noexcept;

private:
#if FLOW_UI_DEV_MODE
	[[nodiscard]] Status advance_dev_capture() noexcept;
#endif
	struct Impl;
	std::unique_ptr<Impl> impl_;

	/** Adapt an owned user callback without discarding a returned Status.
	 * Construction may throw only inside the calling public API's error boundary.
	 */
	template <typename Callback>
	[[nodiscard]] static UiBuildCallback ownBuildCallback(Callback&& buildUi) {
		return [callback = std::forward<Callback>(buildUi)](UiManager& ui, WindowId id) mutable {
			if constexpr (std::same_as<std::invoke_result_t<std::decay_t<Callback>&, UiManager&, WindowId>, Status>)
				detail::requireApiSuccess(std::invoke(callback, ui, id));
			else
				(void)std::invoke(callback, ui, id);
		};
	}

	friend Result<App> makeApplication(const AppConfig& cfg) noexcept;
};

/**
 * @brief Create a running FlowUi application from configuration.
 *
 * Creates an App object, allocates its internal implementation, and initializes
 * the runtime managers.
 *
 * @param cfg Configuration struct used for initialization.
 * @return Sole owner of the initialized runtime and semantic main window, or a
 * structured configuration, allocation, platform, resource, or rendering error.
 * @note Runtime access and window/frame work must remain on the creating platform thread.
 *
 * @note Check success before continuing or reading a value. This noexcept App boundary returns operational failures as FlowUiError and preserves their attribution. Caller argument construction occurs before entry.
 *
 * @par Example
 * @code{.cpp}
 * FlowUi::AppConfig configuration{};
 * auto result = FlowUi::makeApplication(configuration);
 * if (!result) {
 *     throw FlowUi::FlowUiException(result.error());
 * }
 * FlowUi::App application = std::move(*result);
 * @endcode
 */
[[nodiscard]] Result<App> makeApplication(const AppConfig& cfg) noexcept;

/** @} */

} // namespace FlowUi
