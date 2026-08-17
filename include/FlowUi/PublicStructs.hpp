#pragma once

#include <cstddef>
#include <cstdint>
#include <string>
#include <filesystem>
#include <vector>

#include <clay.h>

#include "FlowUi/TextureHandle.hpp"
#include "FlowUi/WindowId.hpp"
#include "managers/structs/FontManagerStructs.hpp"
#include "managers/structs/ShortcutManagerStructs.hpp"

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

/**
 * @brief Configures low-level window input behavior.
 *
 * These options are forwarded to the window backend during window creation and
 * whenever the input configuration is updated at runtime.
 */
struct WindowInputConfig {
	/** @brief Cursor visibility and capture mode applied to the window. */
	CursorMode cursorMode = CursorMode::Normal;

	/**
	 * @brief Keep key press events available until they are polled.
	 *
	 * Useful when the app wants to avoid missing short key presses between
	 * input polling points.
	 */
	bool stickyKeys = false;

	/**
	 * @brief Keep mouse button press events available until they are polled.
	 *
	 * Useful when the app wants to avoid missing short mouse clicks between
	 * input polling points.
	 */
	bool stickyMouseButtons = false;

	/**
	 * @brief Preserve lock-key modifier state in reported input events.
	 *
	 * When supported by the backend, events include Caps Lock and Num Lock
	 * modifier bits while those locks are active.
	 */
	bool lockKeyMods = false;

	/**
	 * @brief Request unaccelerated mouse motion when the backend supports it.
	 *
	 * Unsupported backends or platforms disable this option automatically.
	 */
	bool rawMouseMotion = false;
};

/**
 * @brief Describes a native application window.
 *
 * Width, height, title, and input settings also provide the initial runtime
 * defaults used by FlowUi managers. AppConfig::window configures the semantic
 * main window. Explicit secondary windows inherit the application's Vulkan and
 * UI configuration and supply only this native-window configuration.
 */
struct WindowConfig {
	/** @brief Initial window width in screen coordinates.
	 *
	 * Must be a positive integer greater than 0.
	 */
	int width = 1280;

	/** @brief Initial window height in screen coordinates.
	 *
	 * Must be a positive integer greater than 0.
	 */
	int height = 720;

	/** @brief Initial native window title. */
	std::string title = "FlowUi App";

	/** @brief Allow the user or window manager to resize the window. */
	bool resizable = true;

	/** @brief Create the window with native title bar and borders. */
	bool decorated = true;

	/** @brief Create the window in a maximized state when supported. */
	bool maximized = false;

	/** @brief Create the window on the primary monitor in fullscreen mode.
	 *
	 * @warning The GLFW backend creates a true fullscreen window without
	 * normal title-bar controls, so users may not have a visible close or
	 * minimize button. Provide an app-level exit path, such as a key chord
	 * or menu action, before enabling fullscreen by default.
	 */
	bool fullscreen = false;

	/** @brief Prefer high-DPI framebuffer behavior for capable backends. */
	bool highDPI = true;

	/** @brief Initial low-level input behavior for the window. */
	WindowInputConfig input{};
};

/**
 * @brief Configures Vulkan device, swapchain, and frame scheduling defaults.
 *
 * These options are consumed during renderer initialization.
 */
struct VulkanConfig {
	/** @brief Enable Vulkan validation layers when they are available. */
	bool enableValidation = true;

	/** @brief Enable Vulkan debug utils messenger support when available. */
	bool enableDebugUtils = true;

	/** @brief Preferred swapchain presentation mode. */
	PresentMode presentMode = PresentMode::Fifo;

	/** @brief Prefer a discrete GPU when selecting a physical device. */
	bool preferDiscreteGPU = true;

	/**
	 * @brief Requested multisample anti-aliasing level.
	 *
	 * @note Reserved. The current renderer uses one sample regardless of this value.
	 */
	MSAA msaa = MSAA::x1;

	/** @brief Prefer an sRGB swapchain format when the surface supports one. */
	bool srgbBackbuffer = true;

	/** @brief Number of frames the renderer may keep in flight. */
	uint32_t framesInFlight = 2;
};

/**
 * @brief Configures input field editing visuals.
 *
 * These values control caret and selection rendering for FlowUi text input
 * fields; they do not affect low-level window input behavior.
 */
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

/**
 * @brief Configures FlowUi layout, text, and UI resource defaults.
 *
 * These values are consumed during app and UI manager initialization. They affect
 * Clay layout memory, transient per-frame UI storage, text scaling, and the
 * default font resources used by text rendering.
 */
struct UiConfig {
	/**
	 * @brief Logical dots-per-inch used for point-to-pixel conversion.
	 *
	 * Text sizes are specified in points and converted to pixels as dpi / 72.
	 * Values below 1 are clamped to 1 during initialization.
	 *
	 * For example, a 12 pt text size at the default 96 dpi is laid out as
	 * 12 * (96 / 72) = 16 px before fontScale is applied.
	 */
	float dpi = 96.0f;

	/**
	 * @brief Global layout scale applied to UI coordinates and input.
	 *
	 * Values above 1 make the logical layout space smaller relative to the
	 * native window size, producing larger rendered UI.
	 */
	float uiScale = 1.0f;

	/**
	 * @brief Global multiplier applied after DPI font scaling.
	 *
	 * If the configured value is zero or negative, FlowUi falls back to the
	 * DPI-derived point-to-pixel scale.
	 */
	float fontScale = 1.0f;

	/**
	 * @brief Byte capacity of each transient UI arena.
	 *
	 * FlowUi creates one arena per frame in flight and resets the active arena at
	 * the beginning of each UI frame. These arenas store temporary Clay strings,
	 * element ids, and texture references created during UI construction.
	 * 
	 * @note The Size set here is for ONE arena. Total Size used will be stringArenaSize * framesInFlight
	 *
	 * @see FlowUi::VulkanConfig::framesInFlight
	 */
	size_t stringArenaSize = 256 * 1024;

	/**
	 * @brief Byte capacity of the persistent Clay layout arena.
	 *
	 * A value of 0 uses Clay_MinMemorySize(). Non-zero values are clamped up to
	 * at least Clay_MinMemorySize() before Clay_Initialize is called.
	 */
	size_t clayArenaCapacityBytes = 0;

	/**
	 * @brief Default font family loaded during app startup.
	 *
	 * This family is registered first, so family id 0 is the fallback family used
	 * by default text rendering. If loading fails, FlowUi attempts built-in
	 * fallback font candidates.
	 *
	 * @see FlowUi::FontFamilyCreateInfo
	 */
	FontFamilyCreateInfo defaultFontFamily{};

	/**
	 * @brief Width and height in pixels of each font atlas page.
	 *
	 * The font manager uploads each loaded font face into square atlas layers of
	 * this size. The value must be non-zero and large enough for the source
	 * .arfont atlas or runtime-baked glyph atlas.
	 */
	uint32_t fontAtlasSize = 2048;

	/**
	 * @brief Reserved legacy icon atlas size setting.
	 *
	 * @note This value is currently unused. Configure icon atlas sizing through
	 * AppConfig::iconManager instead.
	 *
	 * @see FlowUi::IconManagerConfig
	 */
	uint32_t iconAtlasSize = 1024;

	/**
	 * @brief Input field caret and selection rendering configuration.
	 *
	 * @see FlowUi::InputManagerConfig
	 */
	InputManagerConfig inputManager{};

	/** @brief Built-in shortcut registrations installed for this UI manager. */
	ShortcutManagerConfig shortcuts{};
};

/**
 * @brief Configures SVG icon rasterization, caching, and atlas storage.
 *
 * Icons are rasterized on demand at the size required by the current frame,
 * then cached in atlas pages for reuse by later frames.
 */
struct IconManagerConfig {
	/** @brief Width and height in pixels of each icon atlas page. */
	uint32_t atlasSize = 2048;

	/**
	 * @brief Maximum number of icon atlas pages the manager may allocate.
	 *
	 * More pages allow more cached icon variants to stay resident, at the cost of
	 * additional GPU memory.
	 */
	uint32_t maxAtlasPages = 10;

	/**
	 * @brief Pixel tolerance for reusing cached icon raster sizes.
	 *
	 * FlowUi first looks for an existing cached raster of the same icon whose
	 * width and height are within this many pixels of the requested size. Larger
	 * cached rasters are preferred over smaller ones to preserve quality. If no
	 * cached raster is close enough, the SVG is rasterized at the exact requested
	 * size.
	 */
	uint32_t sizeBucketStep = 8;

	/** @brief Empty pixel padding reserved around each atlas allocation. */
	uint32_t atlasPadding = 1;
};

/** @brief Developer panel shortcut trigger mode. 
 * 
 * @see DevShortcutchord
*/
enum class DevShortcutTrigger : uint8_t {
	Press = 0,
	Release = 1,
	Down = 2,
};

/** @brief Keyboard chord used by developer tooling. 
 * 
 * Default Key Chord used to open DeV tool window
 * by default it is set to  ctrl + shift + D 
 * triggers on Press
*/
struct DevShortcutChord {
	/** @brief Platform key code consumed by FlowUi's ShortcutManager backend. */
	int key = 68;
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

/**
 * @brief Configures FlowUi developer tooling and capture behavior.
 *
 * These options are only used when developer mode support is compiled in.
 */
struct DevToolsConfig {
	/** @brief Enable developer tooling at runtime. */
	bool enabled = false;

	/** @brief Open the developer panel during UI manager initialization. */
	bool panelOpenByDefault = false;

	/** @brief Initial developer panel width in pixels. */
	float panelWidthPx = 420.0f;

	/**
	 * @brief Register the panel toggle chord with FlowUi's ShortcutManager.
	 *
	 * Disable this when the application wants to own panel toggling itself.
	 */
	bool useShortcutManagerForPanelToggle = true;

	/** @brief Keyboard chord used to toggle the developer panel. */
	DevShortcutChord panelToggleChord{};

	/**
	 * @brief Hide FlowUi's internal developer elements from captured UI data.
	 *
	 * @note Keeping this enabled is strongly recommended. Capturing the developer
	 * panel itself can add noisy internal elements to inspection/export data and
	 * make user-authored UI harder to reason about.
	 */
	bool excludeInternalDevElementsFromCapture = true;

	/**
	 * @brief File path used when exporting developer override data.
	 *
	 * Explicit developer exports write to this path.
	 */
	std::filesystem::path overridesPath = ".flowui/overrides.v1.json";

	/**
	 * @brief Request automatic saving of developer changes.
	 *
	 * @note Reserved. Developer data is currently written only by explicit export
	 * paths, so this value has no effect.
	 */
	bool autoSave = true;
};

/**
 * @brief Top-level configuration used to initialize a FlowUi app.
 *
 * This struct groups the subsystem-specific configuration blocks passed through
 * app startup.
 */
struct AppConfig {
	/**
	 * @brief Initial native-window configuration for the semantic main window.
	 *
	 * @see FlowUi::WindowConfig
	 */
	WindowConfig window{};

	/**
	 * @brief Vulkan device, swapchain, and frame scheduling defaults.
	 *
	 * @see FlowUi::VulkanConfig
	 */
	VulkanConfig vk{};

	/**
	 * @brief Layout, text, and UI resource defaults.
	 *
	 * @see FlowUi::UiConfig
	 */
	UiConfig ui{};

	/**
	 * @brief SVG icon rasterization and atlas cache defaults.
	 *
	 * @see FlowUi::IconManagerConfig
	 */
	IconManagerConfig iconManager{};

	/**
	 * @brief Developer tooling and capture defaults.
	 *
	 * @see FlowUi::DevToolsConfig
	 */
	DevToolsConfig dev{};
};

/** @brief Texture layout mode inside a target rectangle. */
enum class TextureFitMode : uint8_t {
	/** @brief Scale independently on each axis to fill the target rectangle. */
	Stretch = 0,
	/** @brief Fit the whole texture inside the target rectangle without cropping. */
	Contain = 1,
	/** @brief Fill the target rectangle while preserving aspect ratio; may crop. */
	Cover = 2,
	/** @brief Draw at source size without fit scaling. */
	None = 3,
};

/** @brief Texture sampling mode. */
enum class TextureSamplingMode : uint8_t {
	/** @brief Smoothly filter between neighboring texels. */
	Linear = 0,
	/** @brief Use the nearest texel for crisp pixel edges. */
	Nearest = 1,
};

/**
 * @brief Renderer texture reference returned by image, icon, and viewport managers.
 *
 * Treat this as a manager-owned handle with optional render settings. After
 * obtaining a TextureRef, application code may adjust fitMode, samplingMode, and
 * tintEnabled. The handle, UV coordinates, and source dimensions are populated by
 * FlowUi managers and should normally be left unchanged.
 */
struct TextureRef {
	/** @brief Manager-owned logical texture handle; do not edit manually. */
	TextureHandle handle{};

	/** @brief Manager-owned left U coordinate; do not edit manually. */
	float uv0x = 0.0f;
	/** @brief Manager-owned top V coordinate; do not edit manually. */
	float uv0y = 0.0f;
	/** @brief Manager-owned right U coordinate; do not edit manually. */
	float uv1x = 1.0f;
	/** @brief Manager-owned bottom V coordinate; do not edit manually. */
	float uv1y = 1.0f;

	/**
	 * @brief Texture fit behavior inside the target rectangle.
	 *
	 * Application code may change this after obtaining the TextureRef.
	 *
	 * @see FlowUi::TextureFitMode
	 */
	TextureFitMode fitMode = TextureFitMode::Contain;

	/**
	 * @brief Requested texture filtering mode.
	 *
	 * @note Reserved. The current textured renderer does not apply this per-reference
	 * value; filtering comes from the sampler owned by the logical texture.
	 *
	 * @see FlowUi::TextureSamplingMode
	 */
	TextureSamplingMode samplingMode = TextureSamplingMode::Linear;

	/**
	 * @brief Enable multiplication by the image command color while rendering.
	 *
	 * Application code may change this after obtaining the TextureRef.
	 */
	bool tintEnabled = false;

	/** @brief Manager-owned source texture width in pixels; do not edit manually. */
	int32_t sourceWidth = 0;
	/** @brief Manager-owned source texture height in pixels; do not edit manually. */
	int32_t sourceHeight = 0;
};

/**
 * @brief Standard design system tokens for FlowUi built-in elements.
 */
struct FlowUiTheme {
	/** @brief Primary brand accent color. */
	Clay_Color primary            = { 0.0f, 122.0f / 255.0f, 204.0f / 255.0f, 1.0f };
	/** @brief Hover state color for primary elements. */
	Clay_Color primaryHover       = { 28.0f / 255.0f, 151.0f / 255.0f, 234.0f / 255.0f, 1.0f };
	/** @brief Active/pressed state color for primary elements. */
	Clay_Color primaryActive      = { 0.0f, 95.0f / 255.0f, 184.0f / 255.0f, 1.0f };
	/** @brief Content/text color rendered on top of primary color background. */
	Clay_Color onPrimary          = { 1.0f, 1.0f, 1.0f, 1.0f };

	/** @brief Main background surface color. */
	Clay_Color background         = { 30.0f / 255.0f, 30.0f / 255.0f, 30.0f / 255.0f, 1.0f };
	/** @brief Standard component surface color. */
	Clay_Color surface            = { 37.0f / 255.0f, 37.0f / 255.0f, 38.0f / 255.0f, 1.0f };
	/** @brief Header surface color. */
	Clay_Color surfaceHeader      = { 45.0f / 255.0f, 45.0f / 255.0f, 46.0f / 255.0f, 1.0f };
	/** @brief Hover state color for standard surface elements. */
	Clay_Color surfaceHover       = { 62.0f / 255.0f, 62.0f / 255.0f, 66.0f / 255.0f, 1.0f };
	/** @brief Selected state color for list items and cards. */
	Clay_Color surfaceSelected    = { 55.0f / 255.0f, 55.0f / 255.0f, 61.0f / 255.0f, 1.0f };

	/** @brief High-contrast primary text color. */
	Clay_Color textPrimary        = { 1.0f, 1.0f, 1.0f, 1.0f };
	/** @brief Muted secondary text color. */
	Clay_Color textSecondary      = { 204.0f / 255.0f, 204.0f / 255.0f, 204.0f / 255.0f, 1.0f };
	/** @brief Disabled text color. */
	Clay_Color textDisabled       = { 108.0f / 255.0f, 108.0f / 255.0f, 108.0f / 255.0f, 1.0f };
	/** @brief Hyperlink text color. */
	Clay_Color textLink           = { 79.0f / 255.0f, 193.0f / 255.0f, 255.0f / 255.0f, 1.0f };

	/** @brief Standard border color. */
	Clay_Color border             = { 69.0f / 255.0f, 69.0f / 255.0f, 69.0f / 255.0f, 1.0f };
	/** @brief Focused border color. */
	Clay_Color borderFocused      = { 0.0f, 122.0f / 255.0f, 204.0f / 255.0f, 1.0f };
	/** @brief Separator and divider line color. */
	Clay_Color divider            = { 45.0f / 255.0f, 45.0f / 255.0f, 46.0f / 255.0f, 1.0f };

	/** @brief Success status indicator color. */
	Clay_Color success            = { 78.0f / 255.0f, 201.0f / 255.0f, 176.0f / 255.0f, 1.0f };
	/** @brief Warning status indicator color. */
	Clay_Color warning            = { 206.0f / 255.0f, 145.0f / 255.0f, 120.0f / 255.0f, 1.0f };
	/** @brief Error/danger status indicator color. */
	Clay_Color danger             = { 241.0f / 255.0f, 76.0f / 255.0f, 76.0f / 255.0f, 1.0f };

	/** @brief Small corner radius token. */
	Clay_CornerRadius radiusSmall  = { 2.0f, 2.0f, 2.0f, 2.0f };
	/** @brief Medium corner radius token. */
	Clay_CornerRadius radiusMedium = { 4.0f, 4.0f, 4.0f, 4.0f };
	/** @brief Large corner radius token. */
	Clay_CornerRadius radiusLarge  = { 8.0f, 8.0f, 8.0f, 8.0f };
	/** @brief Pill / circular corner radius token. */
	Clay_CornerRadius radiusPill   = { 99.0f, 99.0f, 99.0f, 99.0f };

	/** @brief Default font family ID. */
	FontFamilyId defaultFontFamily = 0;
	/** @brief Small font size in pixels. */
	float fontSizeSmall            = 12.0f;
	/** @brief Medium font size in pixels. */
	float fontSizeMedium           = 14.0f;
	/** @brief Large font size in pixels. */
	float fontSizeLarge            = 18.0f;
	/** @brief Header font size in pixels. */
	float fontSizeHeader           = 24.0f;

	/** @brief Extra-small layout spacing in pixels. */
	uint16_t spacingXs             = 4;
	/** @brief Small layout spacing in pixels. */
	uint16_t spacingSm             = 8;
	/** @brief Medium layout spacing in pixels. */
	uint16_t spacingMd             = 16;
	/** @brief Large layout spacing in pixels. */
	uint16_t spacingLg             = 24;

	/** @brief Factory returning default dark theme instance. */
	static FlowUiTheme dark() { return FlowUiTheme{}; }
	/** @brief Factory returning default light theme instance. */
	static FlowUiTheme light() {
		FlowUiTheme t{};
		t.background      = { 1.0f, 1.0f, 1.0f, 1.0f };
		t.surface         = { 243.0f / 255.0f, 243.0f / 255.0f, 243.0f / 255.0f, 1.0f };
		t.surfaceHeader   = { 229.0f / 255.0f, 229.0f / 255.0f, 229.0f / 255.0f, 1.0f };
		t.surfaceHover    = { 232.0f / 255.0f, 232.0f / 255.0f, 232.0f / 255.0f, 1.0f };
		t.textPrimary     = { 0.0f, 0.0f, 0.0f, 1.0f };
		t.textSecondary   = { 51.0f / 255.0f, 51.0f / 255.0f, 51.0f / 255.0f, 1.0f };
		t.border          = { 204.0f / 255.0f, 204.0f / 255.0f, 204.0f / 255.0f, 1.0f };
		return t;
	}
};

/** @} */

} // namespace FlowUi
