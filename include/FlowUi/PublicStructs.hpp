#pragma once

#include <cstddef>
#include <cstdint>
#include <string>
#include <filesystem>
#include <vector>

#include <clay.h>


namespace FlowUi {

/** @addtogroup flowui_config
 * @{
 */

/** @brief Presentation mode requested for the Vulkan swapchain. */
enum class PresentMode { Fifo, Mailbox, Immediate };

/** @brief Multisample anti-aliasing sample count. */
enum class MSAA { x1 = 1, x2 = 2, x4 = 4, x8 = 8 };

/** @brief Window cursor visibility and capture mode. */
enum class CursorMode : uint8_t { Normal = 0, Hidden = 1, Disabled = 2 };

/** @brief Stable concrete font id consumed by Clay text configuration. */
using FontId = uint16_t;

/** @brief Stable logical font family id returned by FontManager. */
using FontFamilyId = uint32_t;

/** @brief Font style requested when resolving a family face. */
enum class FontStyle : uint8_t {
	Normal,
	Italic,
};

/** @brief One font face source inside a family. */
struct FontFaceCreateInfo {
	/** @brief Source .arfont, .ttf, .otf, or future supported font file path. */
	std::filesystem::path path{};
	/** @brief Bake/load size in pixels per em. */
	float pixelSize = 18.0f;
	/** @brief CSS-style font weight, where 400 is regular and 700 is bold. */
	uint32_t weight = 400;
	/** @brief Font style represented by this face. */
	FontStyle style = FontStyle::Normal;
	/** @brief Optional registered face name. Empty means infer from source metadata/path. */
	std::string name{};
};

/** @brief Font family creation data with one or more concrete faces. */
struct FontFamilyCreateInfo {
	/** @brief Logical family name used by string-based resolution. */
	std::string name = "Default";
	/** @brief Concrete faces initially registered for this family. */
	std::vector<FontFaceCreateInfo> faces{
		FontFaceCreateInfo{
			.path = "assets/fonts/Inter.arfont",
			.pixelSize = 18.0f,
			.weight = 400,
			.style = FontStyle::Normal,
		},
	};
};

/** @brief Cursor shape requested by FlowUi UI code. */
enum class CursorType : uint8_t {
	Default,
	Arrow,
	IBeam,
	Crosshair,
	PointingHand,
	ResizeHorizontal, 
	ResizeVertical,
	ResizeDiagonalTL, 
	ResizeDiagonalTR, 
	ResizeAll, 
	NotAllowed,
	Wait,
	Progress,
	Grab,
	Grabbing,
	Custom,
};

/** @brief Window input behavior toggles. */
struct WindowInputConfig {
	/** @brief Cursor mode applied to the window backend. */
	CursorMode cursorMode = CursorMode::Normal;
	/** @brief Brief goes here. */
	bool stickyKeys = false;
	/** @brief Brief goes here. */
	bool stickyMouseButtons = false;
	/** @brief Brief goes here. */
	bool lockKeyMods = false;
	/** @brief Request raw mouse motion when supported by the backend. */
	bool rawMouseMotion = false;
};

/** @brief Window creation and runtime window defaults. */
struct WindowConfig {
	/** @brief Initial window width in screen coordinates. */
	int width = 1280;
	/** @brief Initial window height in screen coordinates. */
	int height = 720;
	/** @brief Initial window title. */
	std::string title = "FlowUi App";
	/** @brief Whether the window can be resized. */
	bool resizable = true;
	/** @brief Whether the window starts maximized. */
	bool maximized = false;
	/** @brief Whether the window starts fullscreen. */
	bool fullscreen = false;
	/** @brief Whether the window backend should prefer high-DPI framebuffer behavior. */
	bool highDPI = true;
	/** @brief Window input configuration. */
	WindowInputConfig input{};
};

/** @brief Vulkan runtime configuration. */
struct VulkanConfig {
	/** @brief Enable Vulkan validation layers when available. */
	bool enableValidation = true;
	/** @brief Enable Vulkan debug utils when available. */
	bool enableDebugUtils = true;
	/** @brief Preferred swapchain presentation mode. */
	PresentMode presentMode = PresentMode::Fifo;
	/** @brief Prefer a discrete GPU when one is available. */
	bool preferDiscreteGPU = true;
	/** @brief Requested MSAA level. No-op for now. */
	MSAA msaa = MSAA::x1; //No-op For now
	/** @brief Request an sRGB swapchain format when available. */
	bool srgbBackbuffer = true;
	/** @brief Number of frames held in flight by the renderer. */
	uint32_t framesInFlight = 2;
};

/** @brief UI runtime configuration. */
struct UiConfig {
	/** @brief Input field rendering configuration. */
	struct InputManagerConfig {
		/** @brief Caret width in pixels. */
		float caretWidthPx = 2.0f;
		/** @brief Extra caret height above the text bounds. */
		float caretHeightOverflowTopPx = 1.0f;
		/** @brief Extra caret height below the text bounds. */
		float caretHeightOverflowBottomPx = 1.0f;
		/** @brief Caret color. */
		Clay_Color caretColor = Clay_Color{255.0f, 255.0f, 255.0f, 255.0f};
		/** @brief Selection highlight rectangle color. */
		Clay_Color highlightBoxColor = Clay_Color{66.0f, 133.0f, 244.0f, 150.0f};
		/** @brief Text color used for selected text. */
		Clay_Color highlightedTextColor = Clay_Color{255.0f, 255.0f, 255.0f, 255.0f};
	};

	/** @brief Logical dots-per-inch used for point-to-pixel conversion. */
	float dpi = 96.0f;
	/** @brief Global UI scale multiplier. */
	float uiScale = 1.0f;
	/** @brief Global font scale multiplier. */
	float fontScale = 1.0f;
	/** @brief Size of each transient string arena used during UI construction. */
	size_t stringArenaSize = 256 * 1024;
	/** @brief Clay arena size. 0 means use Clay_MinMemorySize(). */
	size_t clayArenaCapacityBytes = 0;

	/** @brief Default font family loaded during app startup. Family 0 is the fallback family. */
	FontFamilyCreateInfo defaultFontFamily{};

	/** @brief Font atlas texture size. */
	uint32_t fontAtlasSize = 2048;
	/** @brief Icon atlas texture size. No-op for now. */
	uint32_t iconAtlasSize = 1024; //No-op for now
	/** @brief Input field manager configuration. */
	InputManagerConfig inputManager{};
};

/** @brief Icon atlas and cache configuration. */
struct IconManagerConfig {
	/** @brief Atlas page width and height in pixels. */
	uint32_t atlasSize = 2048;
	/** @brief Maximum icon atlas page count. */
	uint32_t maxAtlasPages = 10;
	/** @brief Icon raster size bucket step in pixels. */
	uint32_t sizeBucketStep = 8;
	/** @brief Padding around atlas allocations in pixels. */
	uint32_t atlasPadding = 1;
};

/** @brief Developer panel shortcut trigger mode. */
enum class DevShortcutTrigger : uint8_t {
	Press = 0,
	Release = 1,
	Down = 2,
};

/** @brief Keyboard chord used by developer tooling. */
struct DevShortcutChord {
	/** @brief Platform key code consumed by FlowUi's ShortcutManager backend. */
	int key = 68; // Default: 'D'
	/** @brief Whether Ctrl is required. */
	bool ctrl = true;
	/** @brief Whether Shift is required. */
	bool shift = true;
	/** @brief Whether Alt is required. */
	bool alt = false;
	/** @brief Whether Super/Command is required. */
	bool super = false;
	/** @brief Shortcut trigger mode. */
	DevShortcutTrigger trigger = DevShortcutTrigger::Press;
};

/** @brief Developer tooling runtime configuration. */
struct DevToolsConfig {
	/** @brief Enable developer tooling at runtime. */
	bool enabled = false;
	/** @brief Open the developer panel on startup. */
	bool panelOpenByDefault = false;
	/** @brief Initial developer panel width in pixels. */
	float panelWidthPx = 420.0f;
	/** @brief Use ShortcutManager to toggle the developer panel. */
	bool useShortcutManagerForPanelToggle = true;
	/** @brief Shortcut used to toggle the developer panel. */
	DevShortcutChord panelToggleChord{};
	/** @brief Exclude internal developer UI elements from dev capture. */
	bool excludeInternalDevElementsFromCapture = true;
	/** @brief Path used for developer override export data. */
	std::filesystem::path overridesPath = ".flowui/overrides.v1.json";
	/** @brief Auto-save developer changes. No-op for now. */
	bool autoSave = true; //No-op for now
};

/** @brief Top-level FlowUi application configuration. */
struct AppConfig {
	/** @brief Window configuration. */
	WindowConfig window{};
	/** @brief Vulkan configuration. */
	VulkanConfig vk{};
	/** @brief UI runtime configuration. */
	UiConfig ui{};
	/** @brief Icon manager configuration. */
	IconManagerConfig iconManager{};
	/** @brief Developer tooling configuration. */
	DevToolsConfig dev{};
};

/** @brief Texture layout mode inside a target rectangle. */
enum class TextureFitMode : uint8_t {
	Stretch = 0,
	Contain = 1,
	Cover = 2,
	None = 3,
};

/** @brief Texture sampling mode. */
enum class TextureSamplingMode : uint8_t {
	Linear = 0,
	Nearest = 1,
};

/** @brief Renderer texture reference used by image, icon, and viewport APIs. */
struct TextureRef {
	/** @brief Texture registry slot id. */
	uint32_t id = 0;

	/** @brief Left U coordinate. */
	float uv0x = 0.0f;
	/** @brief Top V coordinate. */
	float uv0y = 0.0f;
	/** @brief Right U coordinate. */
	float uv1x = 1.0f;
	/** @brief Bottom V coordinate. */
	float uv1y = 1.0f;

	/** @brief Fit mode used when rendering the texture. */
	TextureFitMode fitMode = TextureFitMode::Contain;
	/** @brief Sampling mode. Stored in V1 but intentionally not applied by renderer yet. */
	TextureSamplingMode samplingMode = TextureSamplingMode::Linear;
	/** @brief Whether texture tinting is enabled. */
	bool tintEnabled = false;

	/** @brief Source texture width in pixels. */
	int32_t sourceWidth = 0;
	/** @brief Source texture height in pixels. */
	int32_t sourceHeight = 0;
};

/** @} */

} // namespace FlowUi
