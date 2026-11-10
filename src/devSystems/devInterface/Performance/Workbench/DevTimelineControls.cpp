#include "devSystems/devInterface/Performance/Workbench/DevTimelineControls.hpp"
#if FLOW_UI_DEV_MODE
#include "devSystems/devInterface/Diagnostics/DevDiagnosticsUi.hpp"
#include <algorithm>
namespace FlowUi::devSystems::interface_elements {
namespace interaction = FSEL::detail::selectable_surface;
DevTimelineButtonResources::DevTimelineButtonResources([[maybe_unused]] App& app) {
#if FLOWUI_INCLUDE_ICON_MANAGER
	auto& icons = app.icons();
	interface_icons::registerDevInterfaceIcons(icons);
	for (size_t key_index = 0; key_index < keys.size(); ++key_index)
		textures[key_index] = icons.textureRef(keys[key_index]);
#endif
}
void DevTimelineButton::onPressed(InteractionContext& context) {
	interaction::onPressed(context, true);
}
void DevTimelineButton::runLogic(InteractionContext& context) {
	interaction::runLogic(context, true);
}
void DevTimelineButton::onReleased(InteractionContext& context) {
	if (interaction::onReleased(context, true) && context.params.timeline)
		context.params.timeline->pending = context.params.command;
}
void DevTimelineButton::onHovered(InteractionContext& context) {
	context.uiManager.requestCursor(CursorType::PointingHand, 4);
}
void DevTimelineButton::buildElement(BuildContext& context) {
	auto icon = context.resources().texture(context.params.icon_key);
	auto root = timeline_ui::row(24);
	root.layout.sizing.width = CLAY_SIZING_FIT(0);
	root.layout.padding = {4, 4, 0, 0};
	root.layout.childAlignment.x = CLAY_ALIGN_X_CENTER;
	root.backgroundColor = interface_theme::kDepth3Elevated;
	root.clip.horizontal = true;
	root.clip.scrollInputDisabled = true;
	if (context.uiManager.getPreviousFramesInteraction().isHovered(context.clayID()))
		root.border = {.color = interface_theme::kAccentSeaGlass,
					   .width = Clay_BorderWidth{1, 1, 1, 1, 0}};
	CLAY(context.clayID(), root) {
		if (icon.handle) {
			icon.tintEnabled = true;
			Clay_ElementDeclaration image{};
			image.layout.sizing = {.width = CLAY_SIZING_FIXED(14), .height = CLAY_SIZING_FIXED(14)};
			image.backgroundColor = interface_theme::kTextCanvas;
			image.image.imageData = context.uiManager.imageData(icon);
			CLAY(context.clayID("action-icon"), image) {}
		} else
			timeline_ui::text(context, context.params.label, interface_theme::kTextCanvas);
		if (!context.params.icon_key.empty() &&
			context.uiManager.getPreviousFramesInteraction().isHovered(context.clayID()))
			diagnostics_ui::pointer_tooltip(context, LocalElementName{"action-tooltip"},
											context.params.label);
	}
}
namespace timeline_ui {
Clay_Color frame_color(uint64_t metric) noexcept {
	const float time_ms = float(metric / 1e6);
	Clay_Color low{}, high{};
	float mix = 0;
	if (metric <= 16'667'000) {
		low = Flow_Color("#22B8C7");
		high = Flow_Color("#18B8A6");
		mix = time_ms / 16.667f;
	} else if (metric <= 33'300'000) {
		low = Flow_Color("#D9A633");
		high = Flow_Color("#F59E0B");
		mix = (time_ms - 16.667f) / (33.3f - 16.667f);
	} else {
		low = Flow_Color("#EF4444");
		high = Flow_Color("#FB7185");
		mix = std::min(1.0f, (time_ms - 33.3f) / 100);
	}
	return {low.r + (high.r - low.r) * mix, low.g + (high.g - low.g) * mix,
			low.b + (high.b - low.b) * mix, 255};
}
Clay_Color block_color(const TimelineBlockSlice& block, bool ghost) noexcept {
	Clay_Color color = block.domain == TimingSampleDomain::Gpu	   ? Flow_Color("#B58AE8")
					   : block.category == TimingCategory::Element ? Flow_Color("#8B7BD6")
					   : block.category == TimingCategory::Wait	   ? Flow_Color("#C69A43")
					   : block.category == TimingCategory::User	   ? Flow_Color("#39A976")
																   : Flow_Color("#238FA4");
	const uint64_t identity = block.type_id ^ block.invocation_id ^ (block.app_tick * 31);
	const float variation = .8f + float((identity ^ (identity >> 17)) % 7) * .055f;
	color.r = std::min(255.0f, color.r * variation);
	color.g = std::min(255.0f, color.g * variation);
	color.b = std::min(255.0f, color.b * variation);
	if (ghost) {
		color.r *= .25f;
		color.g *= .25f;
		color.b *= .25f;
	}
	return color;
}
} // namespace timeline_ui
} // namespace FlowUi::devSystems::interface_elements
#endif
