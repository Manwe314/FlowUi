#pragma once

#include "FlowUi/BuildConfig.hpp"

#if FLOW_UI_DEV_MODE

#include <cstdint>
#include <string>
#include <vector>

#include "Ui/Vk_UiRenderer.hpp"
#include "managers/structs/FontManagerStructs.hpp"

namespace FlowUi::devSystems::tooling {

enum class DevOverlayPrimitiveKind : uint8_t {
	FilledRect,
	StrokedRect,
	Line,
	TextLabel,
	TextureQuad,
};

struct DevOverlayPrimitive {
	DevOverlayPrimitiveKind kind = DevOverlayPrimitiveKind::FilledRect;
	RectF bounds{};
	uint32_t colorRGBA = 0xFFFFFFFFu;
	float cornerRadius[4]{0.0f, 0.0f, 0.0f, 0.0f};
	float borderWidth[4]{0.0f, 0.0f, 0.0f, 0.0f};
	std::string textLabel{};
	FontId fontId = 0;
	float textSizePoints = 11.0f;
	uint16_t letterSpacing = 0;
	uint32_t textureIndex = 0;
};

struct DevOverlayCommandBuffer {
	std::vector<DevOverlayPrimitive> primitives{};
	std::vector<UiInstance> instances{};
	std::vector<UiRun> runs{};

	void clear() noexcept {
		primitives.clear();
		instances.clear();
		runs.clear();
	}
};

} // namespace FlowUi::devSystems::tooling

#endif
