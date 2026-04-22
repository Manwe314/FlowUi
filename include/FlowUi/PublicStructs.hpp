#pragma once

#include <cstddef>
#include <cstdint>
#include <string>
#include <filesystem>

#include <clay.h>


namespace FlowUi {

// application configuration stuff

enum class PresentMode { Fifo, Mailbox, Immediate };
enum class MSAA { x1 = 1, x2 = 2, x4 = 4, x8 = 8 };
enum class CursorMode : uint8_t { Normal = 0, Hidden = 1, Disabled = 2 };
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

struct WindowInputConfig {
	CursorMode cursorMode = CursorMode::Normal;
	bool stickyKeys = false;
	bool stickyMouseButtons = false;
	bool lockKeyMods = false;
	bool rawMouseMotion = false;
};

struct WindowConfig {
	int width = 1280;
	int height = 720;
	std::string title = "FlowUi App";
	bool resizable = true;
	bool maximized = false;
	bool fullscreen = false;
	bool highDPI = true;
	WindowInputConfig input{};
};

struct VulkanConfig {
	bool enableValidation = true;
	bool enableDebugUtils = true;
	PresentMode presentMode = PresentMode::Fifo;
	bool preferDiscreteGPU = true;
	MSAA msaa = MSAA::x1;
	bool srgbBackbuffer = true;
	uint32_t framesInFlight = 2;
};

struct UiConfig {
	struct InputManagerConfig {
		float caretWidthPx = 2.0f;
		float caretHeightOverflowTopPx = 1.0f;
		float caretHeightOverflowBottomPx = 1.0f;
		Clay_Color caretColor = Clay_Color{255.0f, 255.0f, 255.0f, 255.0f};
		Clay_Color highlightBoxColor = Clay_Color{66.0f, 133.0f, 244.0f, 150.0f};
		Clay_Color highlightedTextColor = Clay_Color{255.0f, 255.0f, 255.0f, 255.0f};
	};

	float dpi = 96.0f;
	float uiScale = 1.0f;
	float fontScale = 1.0f;
	size_t stringArenaSize = 256 * 1024;
	// 0 means "use Clay_MinMemorySize()".
	size_t clayArenaCapacityBytes = 0;

	std::filesystem::path defaultFontPath = "assets/fonts/Inter.arfont";
	float defaultFontPx = 18.0f;

	uint32_t fontAtlasSize = 2048;
	uint32_t iconAtlasSize = 1024;
	InputManagerConfig inputManager{};
};

struct SvgManagerConfig {
	uint32_t atlasSize = 2048;
	uint32_t maxAtlasPages = 10;
	uint32_t sizeBucketStep = 8;
	uint32_t atlasPadding = 1;
};

enum class DevShortcutTrigger : uint8_t {
	Press = 0,
	Release = 1,
	Down = 2,
};

struct DevShortcutChord {
	// Uses platform key codes consumed by FlowUi's ShortcutManager backend.
	int key = 68; // Default: 'D'
	bool ctrl = true;
	bool shift = true;
	bool alt = false;
	bool super = false;
	DevShortcutTrigger trigger = DevShortcutTrigger::Press;
};

struct DevToolsConfig {
	bool enabled = false;
	bool panelOpenByDefault = false;
	float panelWidthPx = 420.0f;
	bool useShortcutManagerForPanelToggle = true;
	DevShortcutChord panelToggleChord{};
	bool excludeInternalDevElementsFromCapture = true;
	std::filesystem::path overridesPath = ".flowui/overrides.v1.json";
	bool autoSave = true;
};

struct AppConfig {
	WindowConfig window{};
	VulkanConfig vk{};
	UiConfig ui{};
	SvgManagerConfig svgManager{};
	DevToolsConfig dev{};
};

// texture handling things

enum class TextureFitMode : uint8_t {
	Stretch = 0,
	Contain = 1,
	Cover = 2,
	None = 3,
};

enum class TextureSamplingMode : uint8_t {
	Linear = 0,
	Nearest = 1,
};

struct TextureRef {
	uint32_t id = 0;

	float uv0x = 0.0f;
	float uv0y = 0.0f;
	float uv1x = 1.0f;
	float uv1y = 1.0f;

	TextureFitMode fitMode = TextureFitMode::Contain;
	// Stored in V1 but intentionally not applied by renderer yet.
	TextureSamplingMode samplingMode = TextureSamplingMode::Linear;
	bool tintEnabled = false;

	int32_t sourceWidth = 0;
	int32_t sourceHeight = 0;
};


} // namespace FlowUi
