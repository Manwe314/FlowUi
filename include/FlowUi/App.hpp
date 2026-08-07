#pragma once

#include <memory>
#include <string>
#include <string_view>
#include <cstddef>
#include <cstdint>
#include <stdexcept>
#include <utility>

#include "FlowUi/BuildConfig.hpp"
#include "FlowUi/PublicStructs.hpp"
#include "FlowUi/ResourceKey.hpp"
#include "clay.h"

namespace FlowUi {

struct FontManager;

/** @addtogroup flowui_app
 * @{
 */

/** @brief ID type for element instances.
 *
 * Alias for a uint64_t hashed FlowUi element id.
 *
 * @see @ref flowui_element_system "Element System"
 */
using FlowElementId = uint64_t;

/** @brief ID type for element definitions.
 *
 * Alias for a uint64_t hashed FlowUi element definition id.
 *
 * @see @ref flowui_element_system "Element System"
 */
using FlowDefinitionId = uint64_t;

namespace detail {

constexpr uint64_t kFlowFnvOffsetBasis = 14695981039346656037ull;
constexpr uint64_t kFlowFnvPrime = 1099511628211ull;

constexpr uint64_t flowHashBytes(std::string_view text) noexcept {
	uint64_t hash = kFlowFnvOffsetBasis;
	for (const char c : text) {
		hash ^= static_cast<uint64_t>(static_cast<unsigned char>(c));
		hash *= kFlowFnvPrime;
	}
	return (hash == 0ull) ? 1ull : hash;
}

constexpr uint64_t flowMix64(uint64_t value) noexcept {
	value ^= value >> 30;
	value *= 0xbf58476d1ce4e5b9ull;
	value ^= value >> 27;
	value *= 0x94d049bb133111ebull;
	value ^= value >> 31;
	return (value == 0ull) ? 1ull : value;
}

} // namespace detail

/** @brief Hash an element name into a FlowUi element id.
 *
 * @param elementName String or string_view representation of the element name.
 * @return FlowElementId generated from the hashed string.
 */
constexpr FlowElementId toFlowId(std::string_view elementName) noexcept {
	return detail::flowHashBytes(elementName);
}

/** @brief Hash an element name into a FlowUi element id.
 *
 * @param elementName String literal representation of the element name.
 * @return FlowElementId generated from the hashed string.
 */
template <std::size_t N>
constexpr FlowElementId toFlowId(const char (&elementName)[N]) noexcept {
	return detail::flowHashBytes(std::string_view{elementName, N - 1});
}

/** @brief Hash a definition name into a FlowUi definition id.
 *
 * @param definitionName String or string_view representation of the element definition name.
 * @return FlowDefinitionId generated from the hashed string.
 */
constexpr FlowDefinitionId toFlowDefinitionId(std::string_view definitionName) noexcept {
	return detail::flowHashBytes(definitionName);
}

/** @brief Hash a string literal into a FlowUi definition id.
 *
 * @param definitionName String literal representation of the element definition name.
 * @return FlowDefinitionId generated from the hashed string.
 */
template <std::size_t N>
constexpr FlowDefinitionId toFlowDefinitionId(const char (&definitionName)[N]) noexcept {
	return detail::flowHashBytes(std::string_view{definitionName, N - 1});
}

/** @brief Create a stable child/index id from a root and numeric index.
 *
 * @param rootId FlowElementId to serve as the root.
 * @param index uint64_t value to mix with the root.
 * @return FlowElementId created by mixing the root id and index.
 */
constexpr FlowElementId createIndexedFlowId(FlowElementId rootId, uint64_t index) noexcept {
	const uint64_t mixedIndex = detail::flowMix64(index + 0x9e3779b97f4a7c15ull);
	return detail::flowMix64(rootId ^ mixedIndex);
}

/** @brief Create a stable child/index id from a root name and numeric index.
 *
 * @param rootName String or string_view to serve as the root.
 * @param index uint64_t value to mix with the root.
 * @return FlowElementId created by hashing the root name and mixing in the index.
 */
constexpr FlowElementId createIndexedFlowId(std::string_view rootName, uint64_t index) noexcept {
	return createIndexedFlowId(toFlowId(rootName), index);
}

/** @brief Create a stable child/index id from a root string literal and numeric index.
 *
 * @param rootName String literal to serve as the root.
 * @param index uint64_t value to mix with the root.
 * @return FlowElementId created by hashing the root name and mixing in the index.
 */
template <std::size_t N>
constexpr FlowElementId createIndexedFlowId(const char (&rootName)[N], uint64_t index) noexcept {
	return createIndexedFlowId(toFlowId(rootName), index);
}

/**
 * @brief Convert a \#RRGGBBAA color string into a Clay_Color.
 *
 * Example:
 * @code{.cpp}
 * Clay_Color color = FlowUi::Flow_Color("\#fff3e8ff");
 * @endcode
 *
 * @param hexRgba String view of a \#RRGGBBAA hex color code.
 * @return Clay_Color decoded from the hex color input.
 * @throws std::invalid_argument if the string is not valid \#RRGGBBAA.
 */
inline Clay_Color Flow_Color(std::string_view hexRgba)
{
	if (hexRgba.size() != 9 || hexRgba[0] != '#') {
		throw std::invalid_argument("Flow_Color expects #RRGGBBAA.");
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
		decodeHexByte(7),
	};
}

class UiManager;
class ImageManager;
class ThemeManager;
#if FLOWUI_INCLUDE_ICON_MANAGER
class IconManager;
#endif
#if FLOWUI_PUBLIC_VULKAN_INTEROP
class ViewPortManager;
#endif

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
	[[nodiscard]] WindowId createWindow(const WindowConfig& config);
	/** @brief Destroy a secondary window after draining its outstanding work.
	 *
	 * The semantic main window cannot be explicitly destroyed.
	 *
	 * @param id Secondary-window identity returned by createWindow().
	 * @throws std::invalid_argument if id is the main window or does not identify
	 * a registered window.
	 * @throws std::logic_error if another window frame triplet is active.
	 */
	void destroyWindow(WindowId id);
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
	void pollEvents();

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
	void beginFrame();
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
	void beginFrame(WindowId id);
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
	void endFrame();
	/** @brief End UI construction for a specific active window frame.
	 *
	 * @param id Window identity passed to the matching beginFrame(id).
	 */
	void endFrame(WindowId id);
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
	 */
	void drawFrame();
	/** @brief Submit and present a specific prepared window frame.
	 *
	 * @param id Window identity passed to the matching beginFrame(id) and
	 * endFrame(id).
	 */
	void drawFrame(WindowId id);

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

/** @brief Convenience macro for FlowUi::toFlowId().
 *
 * @see FlowUi::toFlowId()
 */
#define FLOW_ID(label) (::FlowUi::toFlowId(label))

/** @brief Convenience macro for FlowUi::toFlowDefinitionId().
 *
 * @see FlowUi::toFlowDefinitionId()
 */
#define FLOW_DEF_ID(label) (::FlowUi::toFlowDefinitionId(label))
