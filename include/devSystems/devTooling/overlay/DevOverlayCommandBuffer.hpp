#pragma once

#include "FlowUi/Public/BuildConfig.hpp"

#if FLOW_UI_DEV_MODE

#include <cstdint>
#include <string>
#include <vector>

#include "FlowUi/Private/renderer/UiRenderTypes.hpp"
#include "FlowUi/Public/managerStructs/FontTypes.hpp"

namespace FlowUi::devSystems::tooling {

/** @brief Renderer-only primitive recorded by the overlay generator. */
enum class DevOverlayPrimitiveKind : uint8_t {
	/** @brief Filled bounds rectangle. */
	FilledRect,
	/** @brief Outlined bounds rectangle. */
	StrokedRect,
	/** @brief Line segment. */
	Line,
	/** @brief Owned debug text. */
	TextLabel,
	/** @brief Textured inspection quad. */
	TextureQuad,
};

/** @brief Owned renderer-only debug primitive in UI layout coordinates. */
struct DevOverlayPrimitive {
	/** @brief Primitive shape or text/texture operation. */
	DevOverlayPrimitiveKind kind = DevOverlayPrimitiveKind::FilledRect;
	/** @brief Layout-space rectangle, or line endpoints encoded by the renderer. */
	RectF bounds{};
	/** @brief Packed RGBA debug color. */
	uint32_t colorRGBA = 0xFFFFFFFFu;
	/** @brief Per-corner radius in UI layout units. */
	float cornerRadius[4]{0.0f, 0.0f, 0.0f, 0.0f};
	/** @brief Per-edge stroke widths in UI layout units. */
	float borderWidth[4]{0.0f, 0.0f, 0.0f, 0.0f};
	/** @brief Owned text for a TextLabel primitive. */
	std::string textLabel{};
	/** @brief Concrete font used for text primitives. */
	FontId fontId = 0;
	/** @brief Text size in points. */
	float textSizePoints = 11.0f;
	/** @brief Additional text spacing consumed by the text layout service. */
	uint16_t letterSpacing = 0;
	/** @brief Renderer texture slot for a TextureQuad primitive. */
	uint32_t textureIndex = 0;
};

/** @brief Owned primitive and renderer-instance storage reused across overlay frames. */
struct DevOverlayCommandBuffer {
	/** @brief Authoring primitives generated from the inspection selection. */
	std::vector<DevOverlayPrimitive> primitives{};
	/** @brief Contiguous renderer instances produced from primitives. */
	std::vector<UiInstance> instances{};
	/** @brief Contiguous renderer draw runs referencing instances. */
	std::vector<UiRun> runs{};

	/** @brief Clear primitives, renderer instances, and draw runs while retaining vector capacity. */
	void clear() noexcept {
		primitives.clear();
		instances.clear();
		runs.clear();
	}
};

} // namespace FlowUi::devSystems::tooling

#endif
