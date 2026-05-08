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
#include "clay.h"

struct FontManager;

namespace FlowUi {

/** @addtogroup flowui_app
 * @{
 */

/** @brief id for element instances.
 * 
 * an alias for uint64_t for hashed FlowUi elements
 * @see @ref flowui_element_system Element System
 */
using FlowElementId = uint64_t;

/** @brief id for element definitions. 
 * 
 * an alias for uint64_t for hashed FlowUi element Definitions
 * @see @ref flowui_element_system Element System
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
 * @param elementName string or string_view representation of Element name.
 * @return FlowElementId generated from hashed string.
 */
constexpr FlowElementId toFlowId(std::string_view elementName) noexcept {
	return detail::flowHashBytes(elementName);
}

/** @brief Hash an element name into a FlowUi element id. 
 * 
 * @param elementName string literal representation of element name.
 * @return FlowElementId generated from hashed string.
*/
template <std::size_t N>
constexpr FlowElementId toFlowId(const char (&elementName)[N]) noexcept {
	return detail::flowHashBytes(std::string_view{elementName, N - 1});
}

/** @brief Hash a definition name into a FlowUi definition id. 
 * 
 * @param definitionName string or string_view representation of element definition name.
 * @return FlowDefinitionId generated from hashed string.
*/
constexpr FlowDefinitionId toFlowDefinitionId(std::string_view definitionName) noexcept {
	return detail::flowHashBytes(definitionName);
}

/** @brief Hash a string literal into a FlowUi definition id.
 * 
 * @param definitionName string literal representation of element definition name.
 * @return FlowDefinitionId generated from hashed string.
 */
template <std::size_t N>
constexpr FlowDefinitionId toFlowDefinitionId(const char (&definitionName)[N]) noexcept {
	return detail::flowHashBytes(std::string_view{definitionName, N - 1});
}

/** @brief Create a stable child/index id from a root and numeric index. 
 * 
 * @param rootId FlowElementId to serve as the root
 * @param index uint64_t value to be mixed in with root
 * @return FlowElementId created by mixing root and index 
*/
constexpr FlowElementId createIndexedFlowId(FlowElementId rootId, uint64_t index) noexcept {
	const uint64_t mixedIndex = detail::flowMix64(index + 0x9e3779b97f4a7c15ull);
	return detail::flowMix64(rootId ^ mixedIndex);
}

/** @brief Create a stable child/index id from a root and numeric index. 
 * 
 * @param rootId string or String_view to serve as the root
 * @param index uint64_t value to be mixed in with root
 * @return FlowElementId created by mixing root and index 
*/
constexpr FlowElementId createIndexedFlowId(std::string_view rootName, uint64_t index) noexcept {
	return createIndexedFlowId(toFlowId(rootName), index);
}

/** @brief Create a stable child/index id from a root and numeric index. 
 * 
 * @param rootId StringLiteral to serve as the root
 * @param index uint64_t value to be mixed in with root
 * @return FlowElementId created by mixing root id and index 
*/
template <std::size_t N>
constexpr FlowElementId createIndexedFlowId(const char (&rootName)[N], uint64_t index) noexcept {
	return createIndexedFlowId(toFlowId(rootName), index);
}

/**
 * @brief Convert a \#RRGGBBAA color string into a Clay_Color.
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
#if FLOWUI_INCLUDE_ICON_MANAGER
class IconManager;
#endif
#if FLOWUI_PUBLIC_VULKAN_INTEROP
class ViewPortManager;
#endif

/** @brief Main FlowUi application object and owner of runtime managers. */
class App {
public:
	/** @brief Construct an empty app handle. 
	 * 
	 * Default constructor for App class
	 * 
	 * @warning Do not manually construct App use makeApplication() instead.
	 * @see makeApplication()
	 * 
	 */
	App();
	/** @brief Move constructor. 
	 * Default Move constructor
	*/
	App(App&&) noexcept;
	/** @brief Move assignment. 
	 * Default move assignment
	*/
	App& operator=(App&&) noexcept;
	App(const App&) = delete;
	App& operator=(const App&) = delete;
	/** @brief Destroy the app and owned runtime resources. 
	 * Cleans up App's resources
	*/
	~App();

	/** @brief query the window backend if it requests a shutdown.
	 * 
	 * use this function to set up the app lifecycle loop
	 * 
	 * Example:
	 * @code{.cpp}
	 * FlowUi::App application = makeApplication(config);
	 * while(!application.shouldClose())
	 * {
	 * 	//per Frame Code here
	 * }
	 * @endcode
	 * 
	 * 
	 * @retval true The window requested shutdown
	 * @retval false The window has NOT requested shutdown
	 */
	bool shouldClose() const;

	/** @brief Begin a frame and prepare input/UI state. 
	 * 
	 * Polls window backend, prepares per frame UI resources and state
	 * This function updates the frame input snapshot, refreshes frame-dependent
 	 * state, and initializes the UI system using the current window dimensions and
 	 * UI scale configuration.
	 * 
	 * Example:
	 * @code{.cpp}
	 * application.beginFrame();
	 * buidlUI(application);
	 * application.endFrame();
	 * @endcode
	 * 
	 * @pre FlowUi::App variable is initialized and its window/Ui systems are valid
	 * @post The currant Frame is active and Input/UI is ready for Frame logic/build
	 * @note This function should be called Exactly ONCE per frame.
	 * 
	*/
	void beginFrame();
	/** @brief End UI construction and produce render commands.
	 * 
	 * makes Ui complete the frame and produce rendercommands for this frame
	 * prepares viewport managers resources and icon managers resources
	 * 
	 * Example:
	 * @code{.cpp}
	 * BuildUi(application);
	 * application.endFrame();
	 * application.drawFrame();
	 * @endcode
	 * 
	 * @pre FlowUi::App's beginFrame was called and Ui was built after.
	 * @post The render commands for this frame are ready, all viewport handles are sized.
	 * @note This fucntion should be called Exactly ONCE per frame
	 * 
	 * 
	 */
	void endFrame();
	/** @brief Submit the current frame for rendering/presentation. 
	 *  
	 * takes the rendercommands outputed by endFrame and builds GPU draw runs
	 * finally Renders all the runs generated and passes the output to Swapchain to be presented
	 * 
	 * Example:
	 * @code{.cpp}
	 * application.endFrame();
	 * application.drawFrame();
	 * @endcode
	 * 
	 * @pre FlowUi::App's endFrame was called successfuly.
	 * @post This Frame is rendered and output is Presented
	 * @note This function should be called Exactly ONCE per frame
	*/
	void drawFrame();

	/** @brief Access the font manager.
	 * 
	 * Example:
	 * @code{.cpp}
	 * FlowUi::App application = makeApplication(config);
	 * FlowUi::FontManager& fontManager = application.fonts();
	 * @endcode
	 * 
	 * 
	 * @return Referance to the FlowUi::fontManager
	 */
	FontManager& fonts();
	/** @brief Access the font manager. */
	const FontManager& fonts() const;
	/** @brief Access the image manager. */
	ImageManager& images();
	/** @brief Access the image manager. */
	const ImageManager& images() const;
#if FLOWUI_INCLUDE_ICON_MANAGER
	/** @brief Access the icon manager. */
	IconManager& icons();
	/** @brief Access the icon manager. */
	const IconManager& icons() const;
#endif
#if FLOWUI_PUBLIC_VULKAN_INTEROP
	/** @brief Access the viewport manager. */
	ViewPortManager& viewPorts();
	/** @brief Access the viewport manager. */
	const ViewPortManager& viewPorts() const;
#endif

	/** @brief Access the UI manager. */
	UiManager& ui();
	/** @brief Access the UI manager. */
	const UiManager& ui() const;

	/** @brief Set the native window title. */
	void setWindowTitle(std::string_view title);
	/** @brief Return the window size in screen coordinates. */
	std::pair<int,int> windowSize() const;
	/** @brief Return the framebuffer size in pixels. */
	std::pair<int,int> framebufferSize() const;
	/** @brief Apply window input configuration. */
	void setWindowInputConfig(const WindowInputConfig& config);
	/** @brief Return the backend native window handle, or nullptr if unavailable. */
	void* nativeWindowHandle() const;
	/** @brief Return the current window input configuration. */
	WindowInputConfig windowInputConfig() const;
	/** @brief Return whether the window backend supports raw mouse motion. */
	bool supportsRawMouseMotion() const;
	/** @brief Set clipboard text through the window backend. */
	void setClipboardText(std::string_view text);
	/** @brief Read clipboard text through the window backend. */
	std::string clipboardText() const;

private:
	struct Impl;
	std::unique_ptr<Impl> impl_;

	friend App makeApplication(const AppConfig& cfg);
};

/** @brief Create a running FlowUi application from configuration. 
 * creates an App object.
 * makes a unique pointer and assigns internal impl_ to it.
 * calls impl_'s init() function
 * @param cfg the Configguration struct
 * @return App object intialized with passed config struct.
 * @code{.cpp}
 * FlowUi::Appconfig config{};
 * FlowUi::App application = makeApplication(config);
 * @endcode
*/
App makeApplication(const AppConfig& cfg);

/** @} */

} // namespace FlowUi

/** @brief Convenience macro for FlowUi::toFlowId(). */
#define FLOW_ID(label) (::FlowUi::toFlowId(label))

/** @brief Convenience macro for FlowUi::toFlowDefinitionId(). */
#define FLOW_DEF_ID(label) (::FlowUi::toFlowDefinitionId(label))
