#pragma once

#include <cstddef>
#include <cstdint>
#include <string>
#include <filesystem>


namespace FlowUi {

enum class PresentMode { Fifo, Mailbox, Immediate };
enum class MSAA { x1 = 1, x2 = 2, x4 = 4, x8 = 8 };

struct WindowConfig {
	int width = 1280;
	int height = 720;
	std::string title = "FlowUi App";
	bool resizable = true;
	bool maximized = false;
	bool fullscreen = false;
	bool highDPI = true;
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
	float dpi = 96.0f;
	float uiScale = 1.0f;
	float fontScale = 1.0f;
	size_t stringArenaSize = 256 * 1024;
	// 0 means "use Clay_MinMemorySize()".
	size_t clayArenaCapacityBytes = 0;

	std::filesystem::path defaultFontPath = "assets/fonts/FacultyGlyphic-Regular.arfont";
	float defaultFontPx = 18.0f;

	uint32_t fontAtlasSize = 2048;
	uint32_t iconAtlasSize = 1024;
};

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

struct AppConfig {
	WindowConfig window{};
	VulkanConfig vk{};
	UiConfig ui{};
};

} // namespace FlowUi
