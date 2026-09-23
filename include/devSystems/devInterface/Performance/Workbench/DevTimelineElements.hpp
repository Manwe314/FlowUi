#pragma once
#include "FlowUi/BuildConfig.hpp"
#if FLOW_UI_DEV_MODE
#include "FSEL/internal/SelectableSurfaceBehavior.hpp"
#include "devSystems/devInterface/Performance/Workbench/DevTimelineData.hpp"
#include "devSystems/devInterface/Permanents/Backend/DevTheme.hpp"
#include "managers/UiManager.hpp"
#include <cstdio>

namespace FlowUi::devSystems::interface_elements {
/** Shared state reference for the pinned header and two timeline card types. */
struct DevTimelineParameters {
	DevTimelineState* timeline = nullptr;
	DevPerformanceSelection* selection = nullptr;
	size_t card_index = 0;
};
/** A timeline control or sample; queues commands until the next Workbench build. */
struct DevTimelineButtonParameters {
	TimelineCommand command{};
	std::string label{};
	DevTimelineState* timeline = nullptr;
	Clay_Color color = interface_theme::kDepth3Elevated;
	float width = 0, height = 24;
	bool highlighted = false;
};
struct DevTimelineButton {
	using Parameters = DevTimelineButtonParameters;
	using State = FSEL::detail::selectable_surface::State;
	using BuildContext = ElementBuildContext<DevTimelineButton>;
	using InteractionContext = ElementInteractionContext<DevTimelineButton>;
	static constexpr FlowDefinitionID definitionId =
		DefinitionID("flowui.dev_interface.performance.timeline.button");
	static constexpr std::string_view debugName = "Timeline Control";
	static constexpr bool isDevInternal = true;
	static void onPressed(InteractionContext& context);
	static void onReleased(InteractionContext& context);
	static void onHovered(InteractionContext& context);
	static void runLogic(InteractionContext& context);
	static void buildElement(BuildContext& context);
};
inline constexpr DevTimelineButton kDevTimelineButton{};

namespace timeline_ui {
[[nodiscard]] inline std::string milliseconds(uint64_t duration) {
	char text[48];
	std::snprintf(text, sizeof(text), "%.3f ms", double(duration) / 1e6);
	return text;
}
[[nodiscard]] inline Clay_ElementDeclaration row(float height) noexcept {
	Clay_ElementDeclaration declaration{};
	declaration.layout.sizing = {.width = CLAY_SIZING_GROW(0), .height = CLAY_SIZING_FIXED(height)};
	declaration.layout.childAlignment.y = CLAY_ALIGN_Y_CENTER;
	declaration.layout.childGap = 4;
	return declaration;
}
template <class Context>
void text(Context& context, std::string_view value,
		  Clay_Color color = interface_theme::kTextSecondary) {
	Clay_TextElementConfig config{};
	config.fontSize = 11;
	config.textColor = color;
	config.wrapMode = CLAY_TEXT_WRAP_NONE;
	CLAY_TEXT(context.uiManager.toClayString(value), CLAY_TEXT_CONFIG(config));
}
template <class Context>
void button(Context& context, uint64_t key, std::string label, TimelineCommand command,
			float width = 0, Clay_Color color = interface_theme::kDepth3Elevated, float height = 24,
			bool highlighted = false) {
	context.uiManager.createElement(kDevTimelineButton, Keyed("control", key))
		.setParameters(DevTimelineButtonParameters{std::move(command), std::move(label),
												   context.params.timeline, color, width, height,
												   highlighted})
		.setDevInternalCapture(true)
		.draw();
}
[[nodiscard]] Clay_Color block_color(const TimelineBlockSlice& block, bool ghost) noexcept;
[[nodiscard]] Clay_Color frame_color(uint64_t metric) noexcept;
/** Draw bounded ruler ticks, preserving nanosecond subtraction before conversion. */
void ruler(UiManager& manager, Clay_ElementId id, uint64_t start, uint64_t duration, bool seconds);
/** Draw exact offsets, clipped samples and gaps for a hardware track. */
void lane(UiManager& manager, Clay_ElementId id, DevTimelineParameters parameters,
		  std::span<const size_t> blocks, uint64_t start, uint64_t duration, float width,
		  size_t chain_index, bool cluster_small = true);
} // namespace timeline_ui
} // namespace FlowUi::devSystems::interface_elements
#endif
