#pragma once

#include <functional>
#include <memory>
#include <string>
#include <string_view>
#include <cstddef>
#include <cstdint>
#include <stdexcept>
#include <utility>

#include "FlowUi/BuildConfig.hpp"
#include "FlowUi/ElementID.hpp"
#include "FlowUi/PublicStructs.hpp"
#include "FlowUi/ResourceKey.hpp"
#include "clay.h"
#include "managers/structs/FlowUiElementConcepts.hpp"

namespace FlowUi {

struct FontManager;

/** @addtogroup flowui_app
 * @{
 */

namespace detail {

constexpr Clay_Color decodeFlowColor(std::string_view hexRgba)
{
	if ((hexRgba.size() != 7 && hexRgba.size() != 9) || hexRgba[0] != '#') {
		throw std::invalid_argument("Flow_Color expects #RRGGBB or #RRGGBBAA.");
	}

	const auto decodeHexNibble = [](char c) -> uint8_t {
		if (c >= '0' && c <= '9') return static_cast<uint8_t>(c - '0');
		if (c >= 'a' && c <= 'f') return static_cast<uint8_t>(10 + (c - 'a'));
		if (c >= 'A' && c <= 'F') return static_cast<uint8_t>(10 + (c - 'A'));
		throw std::invalid_argument("Flow_Color received invalid hex digit.");
	};

	const auto decodeHexByte = [&](std::size_t index) -> float {
		const uint8_t hi = decodeHexNibble(hexRgba[index]);
		const uint8_t lo = decodeHexNibble(hexRgba[index + 1]);
		return static_cast<float>((hi << 4) | lo);
	};

	return Clay_Color{
		decodeHexByte(1),
		decodeHexByte(3),
		decodeHexByte(5),
		hexRgba.size() == 9 ? decodeHexByte(7) : 255.0f,
	};
}

} // namespace detail

/**
 * @brief Convert a literal \#RRGGBB or \#RRGGBBAA color into a Clay_Color at compile time.
 *
 * Example:
 * @code{.cpp}
 * Clay_Color color = FlowUi::Flow_Color("\#fff3e8ff");
 * Clay_Color opaque = FlowUi::Flow_Color("\#fff3e8");
 * @endcode
 *
 * Invalid literals are rejected during compilation. Six-digit RGB input receives
 * an implicit `ff` alpha channel.
 *
 * @param hexRgba String literal containing a \#RRGGBB or \#RRGGBBAA hex color code.
 * @return Clay_Color decoded from the hex color input.
 */
template <std::size_t Size>
consteval Clay_Color Flow_Color(const char (&hexRgba)[Size])
{
	return detail::decodeFlowColor(std::string_view{hexRgba, Size - 1u});
}

/**
 * @brief Convert a runtime \#RRGGBB or \#RRGGBBAA string into a Clay_Color.
 *
 * @throws std::invalid_argument if the string is not valid \#RRGGBB or \#RRGGBBAA.
 */
inline Clay_Color Flow_Color(std::string_view hexRgba)
{
	return detail::decodeFlowColor(hexRgba);
}

class UiManager;
template <FlowElement Element>
class ElementBuilder;
class ImageManager;
class ThemeManager;
class ElementManager;
class ActionManager;
#if FLOW_UI_DEV_MODE
namespace devSystems { class DevMonitoringAndReporting; class DevTooling; }
#endif
#if FLOWUI_INCLUDE_ICON_MANAGER
class IconManager;
#endif
#if FLOWUI_PUBLIC_VULKAN_INTEROP
class ViewPortManager;
#endif

/** Callback used to construct one managed window's UI each frame. */
using UiBuildCallback = std::function<void(UiManager&, WindowId)>;

/** Lifecycle policy for a window registered with a managed UI callback. */
struct ManagedWindowFlags {
	bool autoDestroyOnClose = true;
};

/**
 * @brief Main FlowUi application object and owner of runtime managers.
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
	/** @brief Move constructor. */
	App(App&&) noexcept;
	/** @brief Move assignment. */
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
	 * @return Stable identity of the created window.
	 * @throws std::logic_error if another window frame triplet is active.
	 * @throws std::runtime_error if the window or its rendering resources cannot
	 * be created on the selected device.
	 */
	[[nodiscard]] Result<WindowId> createWindow(const WindowConfig& config);
	/** Create a secondary window by cloning the main window configuration. */
	[[nodiscard]] Result<WindowId> createWindowLikeMain();
	/** Create a secondary window by applying partial overrides to the main config. */
	[[nodiscard]] Result<WindowId> createWindow(const WindowConfigOverrides& overrides);
	/** Create a secondary window with a concise title and optional dimensions. */
	[[nodiscard]] Result<WindowId> createWindow(
		std::string_view title,
		int width = 0,
		int height = 0);
	/** Create and register a synchronously managed secondary window. */
	[[nodiscard]] Result<WindowId> createWindow(
		const WindowConfigOverrides& overrides,
		UiBuildCallback buildUi,
		ManagedWindowFlags flags = {});
	/** Attach or replace the managed UI callback for a secondary window.
	 *
	 * The semantic main window is not eligible because its frame remains under
	 * the no-argument frame API's control.
	 */
	[[nodiscard]] Status setWindowUiCallback(
		WindowId id,
		UiBuildCallback buildUi,
		ManagedWindowFlags flags = {});
	/** Return a managed window to explicit/manual frame control. */
	void removeWindowUiCallback(WindowId id);
	/** Execute all managed secondary-window frame triads in registration order.
	 *
	 * Dispatch is synchronous on the platform thread. Closed windows with
	 * autoDestroyOnClose enabled are drained and destroyed before returning.
	 */
	[[nodiscard]] Status dispatchManagedWindows();

	template <FlowElement Element>
	[[nodiscard]] Result<WindowId> createWindow(
		const WindowConfigOverrides& overrides,
		const Element& element,
		ParametersOf<Element> params = {},
		LocalElementName localName = LocalElementName{"window/root-element"});

	template <FlowElement Element>
	[[nodiscard]] Result<WindowId> createWindow(
		const WindowConfigOverrides& overrides,
		const Element& element,
		std::function<void(ElementBuilder<Element>&, WindowId)> configurator,
		LocalElementName localName = LocalElementName{"window/root-element"});

	template <FlowElement Element>
	[[nodiscard]] Result<WindowId> createWindowWithState(
		const WindowConfigOverrides& overrides,
		const Element& element,
		std::shared_ptr<ParametersOf<Element>> sharedParams,
		LocalElementName localName = LocalElementName{"window/root-element"});
	/** @brief Destroy a secondary window after draining its outstanding work.
	 *
	 * The semantic main window cannot be explicitly destroyed.
	 *
	 * @param id Secondary-window identity returned by createWindow().
	 * @throws std::invalid_argument if id is the main window or does not identify
	 * a registered window.
	 * @throws std::logic_error if another window frame triplet is active.
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
	 * @throws std::logic_error if a window frame triplet is active or the call is
	 * made from outside the app's platform thread.
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
	bool shouldClose() const;
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
	 */
	void setShouldClose(int value);
	/** @brief Set or clear a specific window's shutdown request.
	 *
	 * @param id Registered window identity.
	 * @param value Zero clears the request; a non-zero value sets it.
	 */
	void setShouldClose(WindowId id, int value);

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
	 * @post The current frame is active and input/UI state is ready for frame logic/building.
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
	 * @post The selected window's UI manager is ready for frame construction.
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
	 * @post Render commands for this frame are ready and viewport handles are sized.
	 * @note This function should be called exactly once per frame.
	 */
	[[nodiscard]] Status endFrame();
	/** @brief End UI construction for a specific active window frame.
	 *
	 * @param id Window identity passed to the matching beginFrame(id).
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
	 * @post This frame is rendered and presented.
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
	FontManager& fonts();
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
	const FontManager& fonts() const;
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
	ImageManager& images();
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
	const ImageManager& images() const;
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
	ThemeManager& themes();
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
	const ThemeManager& themes() const;
	/** @brief Access the app-wide Flow element state/resource manager.
	 *
	 * @return Reference to the FlowUi::ElementManager owned by this app.
	 * @see @ref flowui_element_system "Element System"
	 */
	ElementManager& elements();
	/** @brief Access the app-wide Flow element state/resource manager.
	 *
	 * @return Const reference to the FlowUi::ElementManager owned by this app.
	 * @see @ref flowui_element_system "Element System"
	 */
	const ElementManager& elements() const;
	/** @brief Access the app-wide action manager. */
	ActionManager& actions();
	/** @brief Access the immutable app-wide action manager. */
	const ActionManager& actions() const;
#if FLOW_UI_DEV_MODE
	/** Development-only access to timing monitoring and retained reports. */
	devSystems::DevMonitoringAndReporting& devMonitoring();
	const devSystems::DevMonitoringAndReporting& devMonitoring() const;
	/** Development-only schema catalogue, discovery, and publication service. */
	devSystems::DevTooling& devTooling();
	const devSystems::DevTooling& devTooling() const;
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
	IconManager& icons();
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
	const IconManager& icons() const;
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
	ViewPortManager& viewPorts();
	/** @brief Access the viewport manager owned by a specific window.
	 *
	 * @param id Registered window identity.
	 * @return Mutable viewport manager for that window.
	 */
	ViewPortManager& viewPorts(WindowId id);
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
	const ViewPortManager& viewPorts() const;
	/** @brief Access the immutable viewport manager owned by a specific window.
	 *
	 * @param id Registered window identity.
	 * @return Immutable viewport manager for that window.
	 */
	const ViewPortManager& viewPorts(WindowId id) const;
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
	UiManager& ui();
	/** @brief Access the UI manager owned by a specific window.
	 *
	 * @param id Registered window identity.
	 * @return Mutable UI manager for that window.
	 */
	UiManager& ui(WindowId id);
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
	const UiManager& ui() const;
	/** @brief Access the immutable UI manager owned by a specific window.
	 *
	 * @param id Registered window identity.
	 * @return Immutable UI manager for that window.
	 */
	const UiManager& ui(WindowId id) const;

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
	std::pair<int,int> windowSize() const;
	/** @brief Return a specific window's size in screen coordinates.
	 *
	 * @param id Registered window identity.
	 * @return Pair in the form (width, height).
	 */
	[[nodiscard]] std::pair<int,int> windowSize(WindowId id) const;
	/** @brief Return the framebuffer size in pixels.
	 *
	 * @return std::pair<int, int> in the form (width, height).
	 */
	std::pair<int,int> framebufferSize() const;
	/** @brief Return a specific window's framebuffer size in pixels.
	 *
	 * @param id Registered window identity.
	 * @return Pair in the form (width, height).
	 */
	[[nodiscard]] std::pair<int,int> framebufferSize(WindowId id) const;
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
	WindowInputConfig windowInputConfig() const;
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
	void* nativeWindowHandle() const;
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
	bool supportsRawMouseMotion() const;
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
	std::string clipboardText() const;
	/** @brief Read clipboard text through a specific window backend.
	 *
	 * @param id Registered window identity.
	 * @return Current clipboard text.
	 */
	[[nodiscard]] std::string clipboardText(WindowId id) const;

private:
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
 * @return App object initialized with the passed config struct.
 */
App makeApplication(const AppConfig& cfg);

/** @} */

} // namespace FlowUi
