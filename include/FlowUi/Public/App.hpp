#pragma once

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
 * Window operations requiring a registered ID throw FlowUiException for an
 * unavailable app or invalid ID unless their Result/Status documents recovery.
 *
 * @see @ref md_docs_2tutorials_2quick__start "Quick Start"
 * @see @ref md_docs_2concepts_2frame__lifecycle "Frame Lifecycle"
 * @see @ref md_docs_2concepts_2managers "Managers"
 */
class App {
public:
	/** @brief Construct an empty app handle.
	 *
	 * Default constructor for the App class.
	 *
	 * @warning Do not manually construct App; use makeApplication() instead.
	 * @see FlowUi::makeApplication()
	 */
	App();
	/** @brief Transfer the runtime and manager ownership; leave the source empty. */
	App(App&&) noexcept;
	/** @brief Release the destination runtime and transfer ownership from the source. */
	App& operator=(App&&) noexcept;
	App(const App&) = delete;
	App& operator=(const App&) = delete;
	/** @brief Destroy the app and owned runtime resources. */
	~App();

	/** @brief Return the stable identity of the semantic main window.
	 *
	 * No-argument window and frame functions operate on this window.
	 *
	 * @return Stable main-window identity.
	 */
	[[nodiscard]] WindowId mainWindowId() const noexcept;

	/**
	 * Route a caller-owned FlowUi error through this App's configured reporting
	 * sink and/or compact default writer. Reporting does not resolve the error or
	 * alter application control flow.
	 */
	void reportError(FlowUiError error) const noexcept;

	/** @brief Create and fully initialize a secondary window.
	 *
	 * The new window uses the app's Vulkan and UI configuration together with
	 * the supplied native-window configuration.
	 *
	 * @param config Native-window configuration for the new window.
	 * @return Stable window identity, or an application, configuration, frame-phase,
	 * platform-thread, native-window, or rendering-resource error.
	 * @pre Call on the platform thread with no active window frame.
	 */
	[[nodiscard]] Result<WindowId> createWindow(const WindowConfig& config);
	/** @brief Create a secondary window using the main configuration and an automatically suffixed title.
	 * @return New identity, or the same structured failures as createWindow(const WindowConfig&).
	 */
	[[nodiscard]] Result<WindowId> createWindowLikeMain();
	/** @brief Apply overrides to the original main-window configuration and create a secondary window.
	 * @param overrides Values to replace; an absent title receives a unique window suffix.
	 * @return New identity, or the same structured failures as createWindow(const WindowConfig&).
	 */
	[[nodiscard]] Result<WindowId> createWindow(const WindowConfigOverrides& overrides);
	/** @brief Create a secondary window with an explicit title and optional dimensions.
	 * @param title Copied title for the new native window.
	 * @param width Width override; zero inherits the main configuration.
	 * @param height Height override; zero inherits the main configuration.
	 * @return New identity, or a structured creation/configuration error.
	 */
	[[nodiscard]] Result<WindowId> createWindow(std::string_view title, int width = 0,
												int height = 0);
	/** @brief Create a secondary window and retain its synchronous UI callback.
	 * @param overrides Main-configuration overrides for the new window.
	 * @param buildUi Nonempty callback retained until replacement, removal, or window destruction.
	 * Captured references must outlive registration and any active invocation.
	 * @param flags Managed dispatch and close behavior.
	 * @return New identity or a creation/registration error; failed registration
	 * destroys the newly created window.
	 */
	[[nodiscard]] Result<WindowId> createWindow(const WindowConfigOverrides& overrides,
												UiBuildCallback buildUi,
												ManagedWindowFlags flags = {});
	/** Attach or replace the managed UI callback for a secondary window.
	 *
	 * The semantic main window is not eligible because its frame remains under
	 * the no-argument frame API's control.
	 * @param id Registered secondary window.
	 * @param buildUi Nonempty retained build callback. References captured by it
	 * must remain valid while registered and during active invocations.
	 * @param flags Dispatch and close behavior for this registration.
	 * @return Success, or an app, identity, configuration, thread, or phase error.
	 */
	[[nodiscard]] Status setWindowUiCallback(WindowId id, UiBuildCallback buildUi,
											 ManagedWindowFlags flags = {});
	/** Return a managed window to explicit/manual frame control. */
	void removeWindowUiCallback(WindowId id);
	/** Execute all managed secondary-window frame triads in registration order.
	 *
	 * Dispatch is synchronous on the platform thread. Closed windows with
	 * autoDestroyOnClose enabled are drained and destroyed before returning.
	 * @return Success, or a local frame/dispatch error. User callback exceptions
	 * propagate after the active window frame is canceled.
	 */
	[[nodiscard]] Status dispatchManagedWindows();

	/**
	 * @brief Create a managed window drawing one element with retained parameters.
	 * @tparam Element Flow element definition to draw as the window root.
	 * @param overrides Window-configuration overrides.
	 * @param element Element tag copied into the retained callback.
	 * @param params Parameters moved into that callback; any borrowed members
	 * must outlive their use by the managed window.
	 * @param localName Root element name; borrowed name storage must remain valid.
	 * @return New window identity or a structured creation/registration error.
	 */
	template <FlowElement Element>
	[[nodiscard]] Result<WindowId>
	createWindow(const WindowConfigOverrides& overrides, const Element& element,
				 ParametersOf<Element> params = {},
				 LocalElementName localName = LocalElementName{"window/root-element"});

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
	 */
	template <FlowElement Element>
	[[nodiscard]] Result<WindowId>
	createWindow(const WindowConfigOverrides& overrides, const Element& element,
				 std::function<void(ElementBuilder<Element>&, WindowId)> configurator,
				 LocalElementName localName = LocalElementName{"window/root-element"});

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
	 */
	template <FlowElement Element>
	[[nodiscard]] Result<WindowId>
	createWindowWithState(const WindowConfigOverrides& overrides, const Element& element,
						  std::shared_ptr<ParametersOf<Element>> sharedParams,
						  LocalElementName localName = LocalElementName{"window/root-element"});
	/** @brief Destroy a secondary window after draining its outstanding work.
	 *
	 * The semantic main window cannot be explicitly destroyed.
	 *
	 * @param id Secondary-window identity returned by createWindow().
	 * @return Success, or an unavailable-app, invalid-window, main-window,
	 * platform-thread, frame-phase, or resource-drain error.
	 */
	[[nodiscard]] Status destroyWindow(WindowId id);
	/** @brief Return whether an identity currently names a registered window.
	 *
	 * @param id Window identity to query.
	 * @retval true id identifies a current window.
	 * @retval false id is invalid, unknown, or was destroyed.
	 */
	[[nodiscard]] bool hasWindow(WindowId id) const noexcept;
	/** @brief Poll global platform events and advance app-shared maintenance.
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
	 */
	[[nodiscard]] Status pollEvents();

	/** @brief Query whether the window backend requested shutdown.
	 *
	 * Use this function to drive the app lifecycle loop.
	 *
	 * Example:
	 * @code{.cpp}
	 * FlowUi::App application = FlowUi::makeApplication(config);
	 * while (!application.shouldClose()) {
	 * 	// Per-frame code here.
	 * }
	 * @endcode
	 *
	 * @retval true The window requested shutdown.
	 * @retval false The window has not requested shutdown.
	 */
	[[nodiscard]] bool shouldClose() const;
	/** @brief Query whether a specific window requested shutdown.
	 *
	 * @param id Registered window identity.
	 * @retval true The window requested shutdown.
	 * @retval false The window has not requested shutdown.
	 */
	[[nodiscard]] bool shouldClose(WindowId id) const;

	/** @brief Set whether the window backend should request shutdown.
	 *
	 * The `value` argument is passed to the GLFW built-in window backend. Zero
	 * clears the window close flag; a non-zero value sets it.
	 *
	 * Example:
	 * @code{.cpp}
	 * while (!application.shouldClose()) {
	 * 	if (error()) {
	 * 		application.setShouldClose(1);
	 * 	}
	 * }
	 * @endcode
	 * @param value Zero clears the close flag; nonzero requests closing.
	 */
	void setShouldClose(int value);
	/** @brief Set or clear a specific window's shutdown request.
	 *
	 * @param id Registered window identity.
	 * @param value Zero clears the request; a non-zero value sets it.
	 */
	void setShouldClose(WindowId id, int value);

	/** Report native operations supported by a live window on the platform thread. */
	[[nodiscard]] Result<WindowOperationCapabilities>
	window_operation_capabilities(WindowId window_id) const noexcept;
	/** Request closure of this window without destroying it during input dispatch. */
	[[nodiscard]] Status request_window_close(WindowId window_id) noexcept;
	/** Minimize this window; requests during a frame run at the next event-poll boundary. */
	[[nodiscard]] Status minimize_window(WindowId window_id) noexcept;
	/** Toggle observed maximized state; rejected while fullscreen. Frame requests are deferred. */
	[[nodiscard]] Status toggle_window_maximized(WindowId window_id) noexcept;
	/** Toggle fullscreen and restore windowed geometry/state. Frame requests are deferred. */
	[[nodiscard]] Status toggle_window_fullscreen(WindowId window_id) noexcept;

	/** @brief Poll events and begin a frame for the semantic main window.
	 *
	 * This convenience overload first performs the same global event polling and
	 * app-shared maintenance as pollEvents(), then begins the window returned by
	 * mainWindowId(). It preserves the classic single-window frame loop without a
	 * separate polling call.
	 *
	 * Example:
	 * @code{.cpp}
	 * while (!application.shouldClose()) {
	 * 	application.beginFrame();
	 * 	buildUi(application.ui());
	 * 	application.endFrame();
	 * 	application.drawFrame();
	 * }
	 * @endcode
	 *
	 * @pre The FlowUi::App instance is initialized and its window/UI systems are valid.
	 * @return Success when UI construction can begin, or a structured frame error.
	 * @post On success, the frame is active and its UI is ready for construction.
	 * @note Do not also call pollEvents() in the same loop iteration unless a
	 * second global event-pump pass is intentional.
	 * @see @ref md_docs_2concepts_2frame__lifecycle "Frame Lifecycle"
	 * @see @ref md_docs_2tutorials_2quick__start "Quick Start"
	 */
	[[nodiscard]] Status beginFrame();
	/** @brief Begin one explicit window frame without polling global events.
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
	 */
	[[nodiscard]] Status beginFrame(WindowId id);
	/** @brief End UI construction and produce render commands.
	 *
	 * Completes UI construction, produces render commands for this frame, and
	 * prepares viewport and icon manager resources.
	 *
	 * Example:
	 * @code{.cpp}
	 * buildUi(application);
	 * application.endFrame();
	 * application.drawFrame();
	 * @endcode
	 *
	 * @pre FlowUi::App::beginFrame() was called and UI was built afterward.
	 * @return Success or a structured local frame/resource error.
	 * @post On success, render commands are ready and viewport handles are sized.
	 * @note This function should be called exactly once per frame.
	 */
	[[nodiscard]] Status endFrame();
	/** @brief End UI construction for a specific active window frame.
	 *
	 * @param id Window identity passed to the matching beginFrame(id).
	 * @return Success or a structured identity, phase, or local frame/resource error.
	 */
	[[nodiscard]] Status endFrame(WindowId id);
	/** @brief Submit the current frame for rendering and presentation.
	 *
	 * Takes the render commands output by endFrame(), builds GPU draw runs,
	 * renders them, and passes the output to the swapchain for presentation.
	 *
	 * Example:
	 * @code{.cpp}
	 * application.endFrame();
	 * application.drawFrame();
	 * @endcode
	 *
	 * @pre FlowUi::App::endFrame() was called successfully.
	 * @return Success or a structured local render, presentation, or managed-dispatch error.
	 * @post On success, frame submission/presentation processing has completed.
	 * @note This function should be called exactly once per frame.
	 * @note After presenting the main window, this overload synchronously calls
	 * dispatchManagedWindows(). The WindowId overload never dispatches managed
	 * windows implicitly.
	 */
	[[nodiscard]] Status drawFrame();
	/** @brief Submit and present a specific prepared window frame.
	 *
	 * @param id Window identity passed to the matching beginFrame(id) and
	 * endFrame(id).
	 * @return Success or a structured local identity, phase, render, or presentation error.
	 */
	[[nodiscard]] Status drawFrame(WindowId id);

	/** @brief Access the font manager.
	 *
	 * Example:
	 * @code{.cpp}
	 * FlowUi::App application = FlowUi::makeApplication(config);
	 * FlowUi::FontManager& fontManager = application.fonts();
	 * @endcode
	 *
	 * @return Reference to the FlowUi::FontManager.
	 * @see @ref flowui_font_manager "Font Manager"
	 */
	[[nodiscard]] FontManager& fonts();
	/** @brief Access the font manager.
	 *
	 * Example:
	 * @code{.cpp}
	 * FlowUi::App application = FlowUi::makeApplication(config);
	 * const FlowUi::FontManager& fontManager = application.fonts();
	 * @endcode
	 *
	 * @return Const reference to the FlowUi::FontManager.
	 * @see @ref flowui_font_manager "Font Manager"
	 */
	[[nodiscard]] const FontManager& fonts() const;
	/** @brief Access the image manager.
	 *
	 * Example:
	 * @code{.cpp}
	 * FlowUi::App application = FlowUi::makeApplication(config);
	 * FlowUi::ImageManager& imageManager = application.images();
	 * @endcode
	 *
	 * @return Reference to the FlowUi::ImageManager.
	 * @see @ref flowui_image_manager "Image Manager"
	 */
	[[nodiscard]] ImageManager& images();
	/** @brief Access the image manager.
	 *
	 * Example:
	 * @code{.cpp}
	 * FlowUi::App application = FlowUi::makeApplication(config);
	 * const FlowUi::ImageManager& imageManager = application.images();
	 * @endcode
	 *
	 * @return Const reference to the FlowUi::ImageManager.
	 * @see @ref flowui_image_manager "Image Manager"
	 */
	[[nodiscard]] const ImageManager& images() const;
	/** @brief Access the theme manager.
	 *
	 * Example:
	 * @code{.cpp}
	 * FlowUi::App application = FlowUi::makeApplication(config);
	 * FlowUi::ThemeManager& themeManager = application.themes();
	 * @endcode
	 *
	 * @return Reference to the FlowUi::ThemeManager.
	 */
	[[nodiscard]] ThemeManager& themes();
	/** @brief Access the theme manager.
	 *
	 * Example:
	 * @code{.cpp}
	 * FlowUi::App application = FlowUi::makeApplication(config);
	 * const FlowUi::ThemeManager& themeManager = application.themes();
	 * @endcode
	 *
	 * @return Const reference to the FlowUi::ThemeManager.
	 */
	[[nodiscard]] const ThemeManager& themes() const;
	/** @brief Access the app-wide Flow element state/resource manager.
	 *
	 * @return Reference to the FlowUi::ElementManager owned by this app.
	 * @see @ref flowui_element_system "Element System"
	 */
	[[nodiscard]] ElementManager& elements();
	/** @brief Access the app-wide Flow element state/resource manager.
	 *
	 * @return Const reference to the FlowUi::ElementManager owned by this app.
	 * @see @ref flowui_element_system "Element System"
	 */
	[[nodiscard]] const ElementManager& elements() const;
#if FLOW_UI_AGENTIC_WORKFLOW
	/** App-owned evidence coordinator; mutations are admitted at application safe points. */
	[[nodiscard]] devSystems::agentic::AgenticWorkflowService& agentic_workflow() noexcept;
	/** Inspect the application's agentic service without activating it. */
	[[nodiscard]] const devSystems::agentic::AgenticWorkflowService&
	agentic_workflow() const noexcept;
#endif
	/** @brief Access the app-wide action manager. */
	[[nodiscard]] ActionManager& actions();
	/** @brief Access the immutable app-wide action manager. */
	[[nodiscard]] const ActionManager& actions() const;
#if FLOW_UI_DEV_MODE
	/** Queue a manual developer-interface toggle, equivalent to its panel shortcut. */
	void request_dev_interface_toggle() noexcept;
	/** Validate capture intent against this application's panel shortcut. */
	[[nodiscard]] Status request_dev_performance_capture(
		const devSystems::PerformanceCaptureSettings& settings) noexcept;
	/** Suspend developer-window dispatch and visibility while preserving its session. */
	[[nodiscard]] Status set_dev_window_suspended(WindowId window, bool suspended);
	/** Poll a configured chord on focused native application windows before UI work. */
	[[nodiscard]] bool dev_shortcut_down(const DevShortcutChord& chord) const noexcept;
	/** Read a native press edge before UI dispatch; repeat events are excluded. */
	[[nodiscard]] bool dev_shortcut_pressed(const DevShortcutChord& chord) const noexcept;
	/** Development-only access to timing monitoring and retained reports. */
	[[nodiscard]] devSystems::DevMonitoringAndReporting& devMonitoring();
	/** @brief Borrow immutable development timing, memory, and error reporting services. */
	[[nodiscard]] const devSystems::DevMonitoringAndReporting& devMonitoring() const;
	/** Development-only schema catalogue, discovery, and publication service. */
	[[nodiscard]] devSystems::DevTooling& devTooling();
	/** @brief Borrow immutable development schema, discovery, and publication services. */
	[[nodiscard]] const devSystems::DevTooling& devTooling() const;
	/** Snapshot registered windows in stable identity order for development UI. */
	[[nodiscard]] std::vector<DevWindowInfo> devWindowSnapshot() const;
	/** Observe window-local viewport metadata independently of public Vulkan interop. */
	[[nodiscard]] std::vector<DevViewportInfo> devViewportSnapshot(WindowId window) const;
	/** Request one immutable GPU copy at the viewport's next natural source render. */
	[[nodiscard]] Status request_dev_viewport_capture(WindowId window,
													  std::string_view key) noexcept;
	/** Retire this tool's backend capture cache; submitted consumers retain their own leases. */
	void release_dev_viewport_capture(WindowId window, std::string_view key) noexcept;
	/** Own an independent binding of a completed immutable copy; survives source removal. */
	[[nodiscard]] Result<TextureHandle>
	retain_dev_viewport_capture(WindowId window, std::string_view key,
								TextureHandle expected_capture) noexcept;
	/** Release a binding returned by retain_dev_viewport_capture(). */
	[[nodiscard]] Status release_dev_viewport_capture_texture(TextureHandle texture) noexcept;
	/** Return the latest owned raw-command packet captured for development replay. */
	[[nodiscard]] devSystems::DevUiReplaySource devUiReplaySource(WindowId id) noexcept;
	/** Request capture of raw Clay commands owned by the supplied subtree IDs. */
	void requestDevUiReplay(WindowId id, uint64_t requestKey,
							std::span<const uint32_t> clayCommandIds,
							Clay_BoundingBox sourceRootBounds);
#endif
#if FLOWUI_INCLUDE_ICON_MANAGER
	/** @brief Access the icon manager.
	 *
	 * Example:
	 * @code{.cpp}
	 * FlowUi::App application = FlowUi::makeApplication(config);
	 * FlowUi::IconManager& iconManager = application.icons();
	 * @endcode
	 *
	 * @note FlowUi must be built with FLOWUI_INCLUDE_ICON_MANAGER enabled to use IconManager.
	 *
	 * @return Reference to the FlowUi::IconManager.
	 * @see @ref flowui_icon_manager "Icon Manager"
	 */
	[[nodiscard]] IconManager& icons();
	/** @brief Access the icon manager.
	 *
	 * Example:
	 * @code{.cpp}
	 * FlowUi::App application = FlowUi::makeApplication(config);
	 * const FlowUi::IconManager& iconManager = application.icons();
	 * @endcode
	 *
	 * @note FlowUi must be built with FLOWUI_INCLUDE_ICON_MANAGER enabled to use IconManager.
	 *
	 * @return Const reference to the FlowUi::IconManager.
	 * @see @ref flowui_icon_manager "Icon Manager"
	 */
	[[nodiscard]] const IconManager& icons() const;
#endif
#if FLOWUI_PUBLIC_VULKAN_INTEROP
	/** @brief Access the viewport manager.
	 *
	 * Example:
	 * @code{.cpp}
	 * FlowUi::App application = FlowUi::makeApplication(config);
	 * FlowUi::ViewPortManager& viewportManager = application.viewPorts();
	 * @endcode
	 *
	 * @note FlowUi must be built with FLOWUI_PUBLIC_VULKAN_INTEROP enabled to use ViewPortManager.
	 *
	 * @return Reference to the FlowUi::ViewPortManager.
	 * @see @ref flowui_viewport_manager "ViewPort Manager"
	 */
	[[nodiscard]] ViewPortManager& viewPorts();
	/** @brief Access the viewport manager owned by a specific window.
	 *
	 * @param id Registered window identity.
	 * @return Mutable viewport manager for that window.
	 */
	[[nodiscard]] ViewPortManager& viewPorts(WindowId id);
	/** @brief Access the viewport manager.
	 *
	 * Example:
	 * @code{.cpp}
	 * FlowUi::App application = FlowUi::makeApplication(config);
	 * const FlowUi::ViewPortManager& viewportManager = application.viewPorts();
	 * @endcode
	 *
	 * @note FlowUi must be built with FLOWUI_PUBLIC_VULKAN_INTEROP enabled to use ViewPortManager.
	 *
	 * @return Const reference to the FlowUi::ViewPortManager.
	 * @see @ref flowui_viewport_manager "ViewPort Manager"
	 */
	[[nodiscard]] const ViewPortManager& viewPorts() const;
	/** @brief Access the immutable viewport manager owned by a specific window.
	 *
	 * @param id Registered window identity.
	 * @return Immutable viewport manager for that window.
	 */
	[[nodiscard]] const ViewPortManager& viewPorts(WindowId id) const;
#endif

	/** @brief Access the UI manager.
	 *
	 * Example:
	 * @code{.cpp}
	 * FlowUi::App application = FlowUi::makeApplication(config);
	 * FlowUi::UiManager& uiManager = application.ui();
	 * @endcode
	 *
	 * @return Reference to the FlowUi::UiManager.
	 * @see @ref flowui_ui_manager "UI Manager"
	 */
	[[nodiscard]] UiManager& ui();
	/** @brief Access the UI manager owned by a specific window.
	 *
	 * @param id Registered window identity.
	 * @return Mutable UI manager for that window.
	 */
	[[nodiscard]] UiManager& ui(WindowId id);
	/** @brief Access the UI manager.
	 *
	 * Example:
	 * @code{.cpp}
	 * FlowUi::App application = FlowUi::makeApplication(config);
	 * const FlowUi::UiManager& uiManager = application.ui();
	 * @endcode
	 *
	 * @return Const reference to the FlowUi::UiManager.
	 * @see @ref flowui_ui_manager "UI Manager"
	 */
	[[nodiscard]] const UiManager& ui() const;
	/** @brief Access the immutable UI manager owned by a specific window.
	 *
	 * @param id Registered window identity.
	 * @return Immutable UI manager for that window.
	 */
	[[nodiscard]] const UiManager& ui(WindowId id) const;

	/** @brief Set the native window title.
	 *
	 * The initial window title is set from FlowUi::WindowConfig by default.
	 *
	 * @param title String view of the new title.
	 *
	 * @see FlowUi::WindowConfig
	 * @see @ref flowui_config "Config Structs"
	 */
	void setWindowTitle(std::string_view title);
	/** @brief Set the native title of a specific window.
	 *
	 * @param id Registered window identity.
	 * @param title New native-window title.
	 */
	void setWindowTitle(WindowId id, std::string_view title);
	/** @brief Return the window size in screen coordinates.
	 *
	 * @return std::pair<int, int> in the form (width, height).
	 */
	[[nodiscard]] std::pair<int, int> windowSize() const;
	/** @brief Return a specific window's size in screen coordinates.
	 *
	 * @param id Registered window identity.
	 * @return Pair in the form (width, height).
	 */
	[[nodiscard]] std::pair<int, int> windowSize(WindowId id) const;
	/** @brief Return the framebuffer size in pixels.
	 *
	 * @return std::pair<int, int> in the form (width, height).
	 */
	[[nodiscard]] std::pair<int, int> framebufferSize() const;
	/** @brief Return a specific window's framebuffer size in pixels.
	 *
	 * @param id Registered window identity.
	 * @return Pair in the form (width, height).
	 */
	[[nodiscard]] std::pair<int, int> framebufferSize(WindowId id) const;
	/** @brief Apply window input configuration.
	 *
	 * The initial window input configuration is set from FlowUi::WindowConfig by default.
	 *
	 * @param config Const reference to the WindowInputConfig struct to activate.
	 *
	 * @see FlowUi::WindowConfig
	 * @see FlowUi::WindowInputConfig
	 * @see @ref flowui_config "Config Structs"
	 */
	void setWindowInputConfig(const WindowInputConfig& config);
	/** @brief Apply input configuration to a specific window.
	 *
	 * @param id Registered window identity.
	 * @param config Input configuration to apply.
	 */
	void setWindowInputConfig(WindowId id, const WindowInputConfig& config);
	/** @brief Return the current window input configuration.
	 *
	 * @return WindowInputConfig currently active on the window backend.
	 *
	 * @see FlowUi::WindowInputConfig
	 * @see @ref flowui_config "Config Structs"
	 */
	[[nodiscard]] WindowInputConfig windowInputConfig() const;
	/** @brief Return a specific window's current input configuration.
	 *
	 * @param id Registered window identity.
	 * @return Active input configuration for that window.
	 */
	[[nodiscard]] WindowInputConfig windowInputConfig(WindowId id) const;
	/** @brief Return the backend native window handle.
	 *
	 * @return Native backend window handle, or nullptr if unavailable.
	 *
	 * @note The concrete handle type depends on the active window backend.
	 */
	[[nodiscard]] void* nativeWindowHandle() const;
	/** @brief Return a specific window's backend-native handle.
	 *
	 * @param id Registered window identity.
	 * @return Backend-native handle, or nullptr if unavailable.
	 */
	[[nodiscard]] void* nativeWindowHandle(WindowId id) const;
	/** @brief Return whether the window backend supports raw mouse motion.
	 *
	 * @retval false The window handle is not created or does not support raw mouse motion.
	 * @retval true The window handle supports raw mouse motion.
	 */
	[[nodiscard]] bool supportsRawMouseMotion() const;
	/** @brief Return whether a specific window supports raw mouse motion.
	 *
	 * @param id Registered window identity.
	 */
	[[nodiscard]] bool supportsRawMouseMotion(WindowId id) const;
	/** @brief Set clipboard text through the window backend.
	 *
	 * @param text String view for the text to set in the system clipboard.
	 *
	 */
	void setClipboardText(std::string_view text);
	/** @brief Set clipboard text through a specific window backend.
	 *
	 * @param id Registered window identity.
	 * @param text Text to place in the system clipboard.
	 */
	void setClipboardText(WindowId id, std::string_view text);
	/** @brief Read clipboard text through the window backend.
	 *
	 * @return std::string containing the current clipboard text.
	 */
	[[nodiscard]] std::string clipboardText() const;
	/** @brief Read clipboard text through a specific window backend.
	 *
	 * @param id Registered window identity.
	 * @return Current clipboard text.
	 */
	[[nodiscard]] std::string clipboardText(WindowId id) const;

private:
#if FLOW_UI_DEV_MODE
	[[nodiscard]] Status advance_dev_capture();
#endif
	struct Impl;
	std::unique_ptr<Impl> impl_;

	friend App makeApplication(const AppConfig& cfg);
};

/** @brief Create a running FlowUi application from configuration.
 *
 * Creates an App object, allocates its internal implementation, and initializes
 * the runtime managers.
 *
 * Example:
 * @code{.cpp}
 * FlowUi::AppConfig config{};
 * FlowUi::App application = FlowUi::makeApplication(config);
 * @endcode
 *
 * @param cfg Configuration struct used for initialization.
 * @return Sole owner of the initialized runtime and semantic main window.
 * @throws FlowUiException if configuration, capacity resolution, resource discovery,
 * native-window initialization, or a recoverable startup operation fails.
 * @throws std::bad_alloc if host allocation fails.
 * @note Runtime access and window/frame work must remain on the creating platform thread.
 */
[[nodiscard]] App makeApplication(const AppConfig& cfg);

/** @} */

} // namespace FlowUi
