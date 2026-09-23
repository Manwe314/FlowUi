#include "devSystems/devInterface/Performance/Selector/DevPerformanceSelector.hpp"
#if FLOW_UI_DEV_MODE
#include "FSEL/RadioChoice.hpp"
#include "devSystems/devInterface/Performance/Selector/DevPerformanceSelectorElements.hpp"
#include "devSystems/devInterface/Permanents/Backend/DevTheme.hpp"
#include "devSystems/devInterface/Permanents/Elements/DevSelectorSearch.hpp"
#include "devSystems/devInterface/Permanents/Elements/DevSelectorStyle.hpp"
#include "devSystems/devMonitoringAndReporting/DevMonitoringAndReporting.hpp"
#include "managers/UiManager.hpp"
#include <algorithm>

namespace FlowUi::devSystems::interface_elements {
namespace {
using Context = DevPerformanceSelector::BuildContext;

void draw_text(Context& context, std::string_view label, uint16_t size = 12,
			   Clay_Color color = interface_theme::kTextCanvas) {
	Clay_TextElementConfig text{};
	text.fontSize = size;
	text.textColor = color;
	text.wrapMode = CLAY_TEXT_WRAP_NONE;
	CLAY_TEXT(context.uiManager.toClayString(label), CLAY_TEXT_CONFIG(text));
}

void draw_section_title(Context& context, LocalElementName name, std::string_view label) {
	Clay_ElementDeclaration section{};
	section.layout.sizing = {.width = CLAY_SIZING_GROW(0), .height = CLAY_SIZING_FIXED(30)};
	section.layout.padding = Clay_Padding{12, 12, 0, 0};
	section.layout.childAlignment = {CLAY_ALIGN_X_LEFT, CLAY_ALIGN_Y_CENTER};
	section.backgroundColor = interface_theme::kDepth1Panel;
	section.border = {.color = interface_theme::kBorderPrimary,
					  .width = Clay_BorderWidth{0, 0, 0, 1, 0}};
	CLAY(context.clayID(name), section) {
		draw_text(context, label);
	}
}

void draw_choice(Context& context, LocalElementName group, uint64_t value, uint64_t& selection,
				 std::string_view label) {
	FSEL::RadioChoiceParameters parameters{};
	parameters.choiceValue = value;
	parameters.selectedValue = &selection;
	parameters.style = selector_choice_style();
	context.uiManager.createElement(FSEL::kRadioChoice, Keyed(group, value))
		.setParameters(parameters)
		.setDevInternalCapture(true)
		.construct();
	draw_text(context, label, 11,
			  selection == value ? interface_theme::kTextCanvas : interface_theme::kTextSecondary);
	context.uiManager.drawConstructed();
}

void draw_row(Context& context, LocalElementName group, uint64_t key,
			  DevPerformanceRowParameters parameters) {
	context.uiManager.createElement(kDevPerformanceRow, Keyed(group, key))
		.setParameters(parameters)
		.setDevInternalCapture(true)
		.draw();
}

void draw_scope(Context& context, DevPerformanceSelection& selection) {
	const auto windows =
		context.params.app ? context.params.app->devWindowSnapshot() : std::vector<DevWindowInfo>{};
	const auto threads =
		context.params.app
			? context.params.app->devMonitoring().timingReporting().cpuTrackSnapshot()
			: std::vector<TimingTrackDescriptor>{};
	auto& scope = selection.selected_scope;
	if (scope.id != 0) {
		const bool exists =
			scope.kind == DevPerformanceScopeKind::Window
				? std::ranges::any_of(windows,
									  [&](const auto& window) { return window.id == scope.id; })
				: std::ranges::any_of(threads,
									  [&](const auto& thread) { return thread.id == scope.id; });
		if (!exists)
			scope.id = 0;
	}

	draw_section_title(context, LocalElementName{"window-title"}, "Window Scope:");
	draw_row(context, LocalElementName{"window"}, 0,
			 {.label = "All Windows",
			  .scope_selection = &selection.selected_scope,
			  .expanded = &selection.windows_expanded});
	if (selection.windows_expanded) {
		for (const auto& window : windows) {
			const std::string label =
				window.title.empty() ? "Window " + std::to_string(window.id) : window.title;
			draw_row(context, LocalElementName{"window"}, window.id,
					 {.label = label,
					  .scope_selection = &selection.selected_scope,
					  .value = window.id,
					  .child = true});
		}
		if (windows.empty())
			draw_text(context, "No windows available", 11);
	}
	draw_section_title(context, LocalElementName{"thread-title"}, "Thread Selector:");
	draw_row(context, LocalElementName{"thread"}, 0,
			 {.label = "All Threads",
			  .scope_selection = &selection.selected_scope,
			  .expanded = &selection.threads_expanded,
			  .scope_kind = DevPerformanceScopeKind::Thread});
	if (selection.threads_expanded) {
		for (const auto& thread : threads) {
			const std::string label = thread.name == "flowui.platform" ? "Main Thread"
									  : thread.name.empty() ? "Thread " + std::to_string(thread.id)
															: thread.name;
			draw_row(context, LocalElementName{"thread"}, thread.id,
					 {.label = label,
					  .scope_selection = &selection.selected_scope,
					  .value = thread.id,
					  .scope_kind = DevPerformanceScopeKind::Thread,
					  .child = true});
		}
		if (threads.empty())
			draw_text(context, "No registered threads", 11);
	}
	draw_section_title(context, LocalElementName{"hardware-title"}, "Hardware Domain:");
	Clay_ElementDeclaration controls{};
	controls.layout.sizing = {.width = CLAY_SIZING_GROW(0), .height = CLAY_SIZING_FIXED(44)};
	controls.layout.padding = Clay_Padding{8, 8, 7, 7};
	controls.layout.childAlignment.y = CLAY_ALIGN_Y_CENTER;
	controls.backgroundColor = interface_theme::kDepth1Panel;
	CLAY(context.clayID("hardware-controls"), controls) {
		const auto choices = selector_choice_track();
		CLAY(context.clayID("hardware"), choices) {
			draw_choice(context, LocalElementName{"domain"}, 0, selection.hardware_domain, "Both");
			draw_choice(context, LocalElementName{"domain"}, 1, selection.hardware_domain,
						"CPU Only");
			draw_choice(context, LocalElementName{"domain"}, 2, selection.hardware_domain,
						"GPU Only");
		}
	}
	draw_section_title(context, LocalElementName{"category-title"}, "Category Filter:");
	for (size_t category_index = 0; category_index < performance_category_names.size();
		 ++category_index) {
		draw_row(context, LocalElementName{"filter"}, category_index,
				 {.label = performance_category_names[category_index],
				  .category_mask = &selection.category_mask,
				  .category_bit = timingCategoryBit(static_cast<TimingCategory>(category_index))});
	}
}

void draw_zones(Context& context, DevPerformanceSelection& selection) {
	auto descriptors =
		context.params.app
			? context.params.app->devMonitoring().timingReporting().descriptorSnapshot()
			: std::vector<TimingZoneDescriptor>{};
	std::ranges::sort(descriptors, [](const auto& left, const auto& right) {
		if (left.category != right.category)
			return left.category < right.category;
		if (left.name != right.name)
			return left.name < right.name;
		return left.typeId < right.typeId;
	});
	size_t matching_count = 0;
	for (size_t category_index = 0; category_index < performance_category_names.size();
		 ++category_index) {
		const auto category = static_cast<TimingCategory>(category_index);
		const auto category_name = performance_category_names[category_index];
		const bool category_matches =
			performance_zone_matches(category_name, category_name, selection.zone_search);
		const auto matches = [&](const TimingZoneDescriptor& descriptor) {
			return descriptor.category == category &&
				   (category_matches ||
					performance_zone_matches(descriptor.name,
											 performance_zone_label(descriptor.name),
											 selection.zone_search));
		};
		const size_t count = static_cast<size_t>(std::ranges::count_if(descriptors, matches));
		if (!selection.zone_search.empty() && count == 0)
			continue;
		matching_count += count;
		draw_row(
			context, LocalElementName{"category"}, category_index,
			{.label = category_name, .expanded = &selection.expanded_categories[category_index]});
		if (!selection.expanded_categories[category_index])
			continue;
		for (const auto& descriptor : descriptors) {
			if (!matches(descriptor))
				continue;
			const auto label = performance_zone_label(descriptor.name);
			draw_row(context, LocalElementName{"zone"}, descriptor.typeId,
					 {.label = label,
					  .selection = &selection.selected_zone,
					  .value = descriptor.typeId,
					  .child = true});
		}
		if (count == 0)
			draw_text(context, "No registered zones", 11);
	}
	if (!selection.zone_search.empty() && matching_count == 0)
		draw_text(context, "No matching zones", 12);
}
} // namespace

void DevPerformanceSelector::buildElement(BuildContext& context) {
	Clay_ElementDeclaration root{};
	root.layout.sizing = {.width = CLAY_SIZING_PERCENT(1), .height = CLAY_SIZING_PERCENT(1)};
	root.layout.layoutDirection = CLAY_TOP_TO_BOTTOM;
	root.backgroundColor = interface_theme::kDepth1Panel;
	CLAY(context.clayID(), root) {
		if (!context.params.interfaceState) {
			draw_text(context, "Performance selection unavailable");
		} else {
			auto& selection = context.params.interfaceState->performance_selection;
			Clay_ElementDeclaration header{};
			header.layout.sizing = {.width = CLAY_SIZING_GROW(0), .height = CLAY_SIZING_FIXED(42)};
			header.layout.padding = Clay_Padding{8, 8, 5, 5};
			header.layout.childGap = 6;
			header.layout.childAlignment.y = CLAY_ALIGN_Y_CENTER;
			header.backgroundColor = interface_theme::kDepth2Ink;
			header.border = {.color = interface_theme::kBorderPrimary,
							 .width = Clay_BorderWidth{0, 0, 0, 1, 0}};
			CLAY(context.clayID("header"), header) {
				draw_text(context, "Performance Scope:", 12);
				const auto toggle = selector_choice_track();
				CLAY(context.clayID("mode-toggle"), toggle) {
					draw_choice(context, LocalElementName{"mode"}, 0, selection.selector_mode,
								"Scope");
					draw_choice(context, LocalElementName{"mode"}, 1, selection.selector_mode,
								"Zone");
				}
			}
			Clay_ElementDeclaration main{};
			main.layout.sizing = {.width = CLAY_SIZING_GROW(0), .height = CLAY_SIZING_GROW(0)};
			main.layout.layoutDirection = CLAY_TOP_TO_BOTTOM;
			CLAY(context.clayID("main"), main) {
				if (selection.selector_mode == 1) {
					context.uiManager.createElement(kDevSelectorSearch, LocalElementName{"search"})
						.setParameters(DevSelectorSearchParameters{
							.query = &selection.zone_search, .placeholder = "Search zones..."})
						.setDevInternalCapture(true)
						.draw();
				}
				Clay_ElementDeclaration content{};
				content.layout.sizing = {.width = CLAY_SIZING_GROW(0),
										 .height = CLAY_SIZING_GROW(0)};
				content.layout.layoutDirection = CLAY_TOP_TO_BOTTOM;
				content.backgroundColor = interface_theme::kDepth0Keel;
				const auto scroll_id =
					context.clayID(selection.selector_mode == 0 ? LocalElementName{"scope-scroll"}
																: LocalElementName{"zone-scroll"});
				const auto scroll = Clay_GetScrollContainerData(scroll_id);
				content.clip = {.vertical = true,
								.childOffset = scroll.found && scroll.scrollPosition
												   ? *scroll.scrollPosition
												   : Clay_Vector2{}};
				CLAY(scroll_id, content) {
					Clay_ElementDeclaration rows{};
					rows.layout.sizing = {.width = CLAY_SIZING_GROW(0),
										  .height = CLAY_SIZING_FIT(0)};
					rows.layout.layoutDirection = CLAY_TOP_TO_BOTTOM;
					rows.layout.padding = Clay_Padding{0, 0, 0, 0};
					rows.layout.childGap = 0;
					CLAY(context.clayID("rows"), rows) {
						if (selection.selector_mode == 0)
							draw_scope(context, selection);
						else
							draw_zones(context, selection);
					}
				}
			}
		}
	}
}
} // namespace FlowUi::devSystems::interface_elements
#endif
