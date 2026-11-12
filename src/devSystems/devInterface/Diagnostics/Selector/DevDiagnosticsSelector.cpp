#include "devSystems/devInterface/Diagnostics/Selector/DevDiagnosticsSelector.hpp"
#include "FlowUi/Public/input/Key.hpp"

#if FLOW_UI_DEV_MODE

#include "FSEL/RadioChoice.hpp"
#include "devSystems/devInterface/Diagnostics/DevDiagnosticsModel.hpp"
#include "devSystems/devInterface/Diagnostics/DevDiagnosticsUi.hpp"
#include "devSystems/devInterface/Permanents/Elements/DevSelectorSearch.hpp"
#include "devSystems/devInterface/Permanents/Elements/DevSelectorStyle.hpp"
#include <algorithm>
#include <array>
#include <vector>

namespace FlowUi::devSystems::interface_elements {
namespace {
using Context = DevDiagnosticsSelector::BuildContext;
using Icon = diagnostics_icons::Icon;
namespace ui = diagnostics_ui;
enum Action : uint64_t {
	Filters = 1,
	Reset,
	ClearSearch,
	RefreshOrder,
	Domain,
	Group,
	DomainMask,
	ImpactMask,
	ResolutionMask
};

void activate(void* target, uint64_t action, uint64_t value) {
	auto& navigation = *static_cast<DevDiagnosticsSelector::State*>(target);
	auto& state = *navigation.diagnostics;
	if (action == Filters)
		state.filters_open = !state.filters_open;
	if (action == Reset)
		reset_diagnostics_filters(state);
	if (action == ClearSearch)
		state.filters.search.clear();
	if (action == RefreshOrder) {
		state.refresh_order = true;
		state.pending = DevDiagnosticsCommand::Refresh;
	}
	if (action == Domain) {
		state.domain_expanded[value] = !state.domain_expanded[value];
		navigation.focused_domain = value;
		navigation.domain_focus = true;
		navigation.keyboard_active = true;
	}
	if (action == Group) {
		const auto found = std::ranges::find(state.groups, value, &DevDiagnosticsGroup::identity);
		if (found != state.groups.end()) {
			select_diagnostics_group(state, found->key, found->mode);
			navigation.focused_identity = value;
			navigation.focused_domain = static_cast<uint64_t>(found->error.domain());
			navigation.domain_focus = false;
			navigation.keyboard_active = true;
		}
	}
	if (action == DomainMask)
		state.filters.domain_mask ^= uint32_t{1} << value;
	if (action == ImpactMask)
		state.filters.impact_mask ^= uint32_t{1} << value;
	if (action == ResolutionMask)
		state.filters.resolution_mask ^= uint32_t{1} << value;
	state.dirty = true;
	if (action != Group && action != Domain && action != Filters)
		state.refresh_order = true;
}

[[nodiscard]] Icon action_icon(uint64_t action) noexcept {
	if (action == Filters)
		return Icon::Filter;
	if (action == Reset || action == RefreshOrder)
		return Icon::Refresh;
	if (action == ClearSearch)
		return Icon::Close;
	return Icon::None;
}

template <class Identity>
void button_with_identity(Context& context, Identity identity, std::string label, uint64_t action,
						  uint64_t value, std::string tooltip, Icon icon) {
	if (action == DomainMask || action == ImpactMask || action == ResolutionMask) {
		const auto& filters = context.state().diagnostics->filters;
		const auto mask = action == DomainMask	 ? filters.domain_mask
						  : action == ImpactMask ? filters.impact_mask
												 : filters.resolution_mask;
		const bool included = (mask & (uint32_t{1} << value)) != 0;
		context.uiManager.createElement(ui::kControl, identity)
			.setParameters({.label = std::move(label),
							.tooltip = included ? "Included in the current filters"
												: "Excluded from the current filters",
							.icon = included ? Icon::Check : Icon::None,
							.activate = activate,
							.target = &context.state(),
							.action = action,
							.value = value,
							.selected = included})
			.setDevInternalCapture(true)
			.draw();
		return;
	}
	ui::button(context, identity, std::move(label), &context.state(), activate, action, value, true,
			   std::move(tooltip), icon == Icon::None ? action_icon(action) : icon);
}

void button(Context& context, LocalElementName identity, std::string label, uint64_t action,
			uint64_t value = 0, std::string tooltip = {}, Icon icon = Icon::None) {
	button_with_identity(context, identity, std::move(label), action, value, std::move(tooltip),
						 icon);
}

void button(Context& context, IndexedElementName identity, std::string label, uint64_t action,
			uint64_t value = 0, std::string tooltip = {}, Icon icon = Icon::None) {
	button_with_identity(context, identity, std::move(label), action, value, std::move(tooltip),
						 icon);
}

void choice(Context& context, uint64_t value, uint64_t& selection, std::string_view label) {
	FSEL::RadioChoiceParameters parameters{};
	parameters.choiceValue = value;
	parameters.selectedValue = &selection;
	parameters.style = selector_choice_style();
	context.uiManager.createElement(FSEL::kRadioChoice, Keyed("activity", value))
		.setParameters(parameters)
		.setDevInternalCapture(true)
		.construct();
	Clay_TextElementConfig configuration{};
	configuration.fontSize = 12;
	configuration.textColor =
		selection == value ? interface_theme::kTextCanvas : interface_theme::kTextSecondary;
	// Clay retains each single-word label's measured width plus the style's 14 px padding.
	configuration.wrapMode = CLAY_TEXT_WRAP_NONE;
	CLAY_TEXT(context.uiManager.toClayString(label), CLAY_TEXT_CONFIG(configuration));
	context.uiManager.drawConstructed();
}

[[nodiscard]] Clay_Color impact_color(ErrorImpact impact) noexcept {
	if (impact >= ErrorImpact::FrameCanceled)
		return interface_theme::kStatusRed;
	if (impact >= ErrorImpact::Degraded)
		return interface_theme::kStatusAmber;
	return interface_theme::kTextSecondary;
}

void filters(Context& context, DevDiagnosticsState& state) {
	const auto popup_id = context.childID("filters");
	if (context.uiManager.popups().consumeDismissed(popup_id)) {
		state.filters_open = false;
		return;
	}
	const auto viewport = Clay_GetLayoutDimensions();
	const float width = std::min(340.0f, std::max(180.0f, viewport.width - 20));
	const float height = std::min(520.0f, std::max(180.0f, viewport.height - 100));
	PopupRequest request{};
	request.anchor = PopupAnchor::element(context.childID("actions"));
	request.expectedSize = Clay_Dimensions{width, height};
	request.placement.offset.y = 4;
	const auto popup = context.uiManager.popups().request(popup_id, request);
	if (!popup) {
		state.message =
			"Could not open Diagnostics filters: " + std::string(popup.error().message());
		state.filters_open = false;
		return;
	}
	if (!popup->visible && !popup->measureOnly)
		return;
	auto panel = ui::column(0);
	panel.layout.sizing = {.width = CLAY_SIZING_FIXED(width), .height = CLAY_SIZING_FIXED(height)};
	panel.layout.padding = {10, 10, 8, 8};
	panel.backgroundColor = interface_theme::kDepth1Panel;
	panel.border = {.color = interface_theme::kBorderVisible, .width = {1, 1, 1, 1, 0}};
	panel.cornerRadius = CLAY_CORNER_RADIUS(4);
	panel.floating = popup->floating;
	CLAY(context.clayID("filters"), panel) {
		auto heading = ui::row(34);
		CLAY(context.clayID("filter-heading"), heading) {
			ui::text(context, "Advanced filters", interface_theme::kTextCanvas, 13);
			button(context, "close-filters", "Close", Filters, 0, {}, Icon::Close);
		}
		auto viewport_panel = ui::column(0);
		viewport_panel.layout.sizing.height = CLAY_SIZING_GROW(0);
		const auto scroll_id = context.clayID("filter-scroll");
		const auto scroll = Clay_GetScrollContainerData(scroll_id);
		viewport_panel.clip = {.vertical = true,
							   .childOffset = scroll.found && scroll.scrollPosition
												  ? *scroll.scrollPosition
												  : Clay_Vector2{}};
		CLAY(scroll_id, viewport_panel) {
			auto contents = ui::column(8);
			contents.layout.padding.right = 5;
			CLAY(context.clayID("filter-contents"), contents) {
				ui::text(context,
						 "Masks include checked values. All predicates apply to one retained "
						 "occurrence.",
						 interface_theme::kTextSecondary, 11);
				static constexpr std::array<FSEL::ComboBoxOption, 3> grouping = {
					{{0, "Contract: code + site"},
					 {1, "Native: code + site + native"},
					 {2, "Source: first retained step"}}};
				static constexpr std::array<FSEL::ComboBoxOption, 4> ordering = {
					{{0, "Impact"},
					 {1, "Most recent"},
					 {2, "Most retained occurrences"},
					 {3, "Name"}}};
				static constexpr std::array<FSEL::ComboBoxOption, 3> window_modes = {
					{{0, "All windows"},
					 {1, "Exact captured window ID"},
					 {2, "No window context"}}};
				static constexpr std::array<FSEL::ComboBoxOption, 3> source_modes = {
					{{0, "Any source"}, {1, "Exact descriptor ID"}, {2, "Source unavailable"}}};
				ui::choice(context, "grouping", "Group by", state.filters.grouping, grouping);
				ui::choice(context, "ordering", "Order", state.filters.order, ordering);
				ui::choice(context, "window-mode", "Window", state.filters.window_mode,
						   window_modes);
				if (state.filters.window_mode == 1) {
					ui::number(context, "window-id", "Captured window ID", state.filters.window, 1,
							   UINT64_MAX, static_cast<int16_t>(popup->floating.zIndex + 1));
					ui::number(context, "first-frame", "First window frame (0 = unbounded)",
							   state.filters.first_frame, 0, UINT64_MAX,
							   static_cast<int16_t>(popup->floating.zIndex + 1));
					ui::number(context, "last-frame", "Last window frame (0 = unbounded)",
							   state.filters.last_frame, 0, UINT64_MAX,
							   static_cast<int16_t>(popup->floating.zIndex + 1));
				}
				ui::choice(context, "source-mode", "Source", state.filters.source_mode,
						   source_modes);
				if (state.filters.source_mode == 1)
					ui::number(context, "source-id", "Captured source descriptor ID",
							   state.filters.source, 1, UINT64_MAX,
							   static_cast<int16_t>(popup->floating.zIndex + 1));
				ui::number(context, "first-tick",
						   "First application tick (inclusive, 0 = unbounded)",
						   state.filters.first_tick, 0, UINT64_MAX,
						   static_cast<int16_t>(popup->floating.zIndex + 1));
				ui::number(context, "last-tick", "Last application tick (inclusive, 0 = unbounded)",
						   state.filters.last_tick, 0, UINT64_MAX,
						   static_cast<int16_t>(popup->floating.zIndex + 1));
				if (state.filters.first_tick && state.filters.last_tick &&
					state.filters.first_tick > state.filters.last_tick)
					ui::text(context, "First tick exceeds last tick; no observations match.",
							 interface_theme::kStatusAmber, 11);
				ui::number(context, "ongoing-ticks", "Ongoing start window (application ticks)",
						   state.filters.ongoing_ticks, 1, UINT64_MAX,
						   static_cast<int16_t>(popup->floating.zIndex + 1));
				ui::number(context, "recent-ticks", "Recent start window (application ticks)",
						   state.filters.recent_ticks, 1, UINT64_MAX,
						   static_cast<int16_t>(popup->floating.zIndex + 1));
				ui::number(context, "histogram-ticks", "History strip range (application ticks)",
						   state.filters.histogram_ticks, 1, UINT64_MAX,
						   static_cast<int16_t>(popup->floating.zIndex + 1));
				ui::text(context, "Domains", interface_theme::kTextCanvas, 12);
				for (uint64_t domain = 0; domain <= static_cast<uint64_t>(ErrorDomain::Viewport);
					 ++domain)
					button(context, Keyed("domain-mask", domain),
						   diagnostics_domain_name(static_cast<ErrorDomain>(domain)), DomainMask,
						   domain);
				ui::text(context, "Contract impact", interface_theme::kTextCanvas, 12);
				for (uint64_t impact = 0; impact <= static_cast<uint64_t>(ErrorImpact::Fatal);
					 ++impact)
					button(context, Keyed("impact-mask", impact),
						   diagnostics_impact_name(static_cast<ErrorImpact>(impact)), ImpactMask,
						   impact);
				ui::text(context, "Recorded resolution", interface_theme::kTextCanvas, 12);
				for (uint64_t resolution = 0;
					 resolution <= static_cast<uint64_t>(ErrorResolution::Terminated); ++resolution)
					button(context, Keyed("resolution-mask", resolution),
						   diagnostics_resolution_name(static_cast<ErrorResolution>(resolution)),
						   ResolutionMask, resolution);
				button(context, "reset-filters-panel", "Reset filters", Reset);
			}
		}
	}
}

struct NavigationRow {
	uint64_t domain = 0, identity = 0;
};
void keyboard(Context& context, DevDiagnosticsState& state, std::span<const NavigationRow> rows) {
	auto& navigation = context.state();
	const auto& current = context.uiManager.getCurrentFrameInput();
	const auto& previous = context.uiManager.getPreviousFrameInput();
	const auto bounds = Clay_GetElementData(context.clayID()).boundingBox;
	if (current.mouseDown[0] && !previous.mouseDown[0] &&
		(current.mouseX < bounds.x || current.mouseY < bounds.y ||
		 current.mouseX >= bounds.x + bounds.width || current.mouseY >= bounds.y + bounds.height))
		navigation.keyboard_active = false;
	const auto pressed = [&](int key) { return current.keyDown[key] && !previous.keyDown[key]; };
	if (state.filters_open)
		return;
	if (rows.empty() || !navigation.keyboard_active ||
		context.uiManager.inputFields().hasPrimaryFieldFocus())
		return;
	const auto focused = context.uiManager.shortcuts().focusedElement().id;
	const auto focused_row = navigation.domain_focus
								 ? context.clayID(Keyed("domain", navigation.focused_domain)).id
								 : context.clayID(Keyed("issue", navigation.focused_identity)).id;
	if (focused != context.clayID().id && focused != focused_row)
		return;
	const auto found = std::ranges::find_if(rows, [&](const auto& row) {
		return navigation.domain_focus
				   ? row.identity == 0 && row.domain == navigation.focused_domain
				   : row.identity == navigation.focused_identity;
	});
	std::size_t index = found == rows.end() ? 0 : static_cast<std::size_t>(found - rows.begin());
	if (pressed(FlowUi::keyCode(FlowUi::Key::Up)) && index > 0)
		--index;
	if (pressed(FlowUi::keyCode(FlowUi::Key::Down)) && index + 1 < rows.size())
		++index;
	navigation.focused_domain = rows[index].domain;
	navigation.focused_identity = rows[index].identity;
	navigation.domain_focus = rows[index].identity == 0;
	if (pressed(FlowUi::keyCode(FlowUi::Key::Left))) {
		state.domain_expanded[navigation.focused_domain] = false;
		navigation.domain_focus = true;
	}
	if (pressed(FlowUi::keyCode(FlowUi::Key::Right)))
		state.domain_expanded[navigation.focused_domain] = true;
	if (pressed(FlowUi::keyCode(FlowUi::Key::Up)) || pressed(FlowUi::keyCode(FlowUi::Key::Down)) ||
		pressed(FlowUi::keyCode(FlowUi::Key::Left)) ||
		pressed(FlowUi::keyCode(FlowUi::Key::Right))) {
		context.uiManager.shortcuts().setFocusedElement(context.clayID());
		const auto target_id = navigation.domain_focus
								   ? context.clayID(Keyed("domain", navigation.focused_domain))
								   : context.clayID(Keyed("issue", navigation.focused_identity));
		const auto target = Clay_GetElementData(target_id);
		const auto viewport = Clay_GetElementData(context.clayID("scroll"));
		const auto scroll = Clay_GetScrollContainerData(context.clayID("scroll"));
		if (target.found && viewport.found && scroll.found && scroll.scrollPosition) {
			if (target.boundingBox.y < viewport.boundingBox.y)
				scroll.scrollPosition->y += viewport.boundingBox.y - target.boundingBox.y;
			if (target.boundingBox.y + target.boundingBox.height >
				viewport.boundingBox.y + viewport.boundingBox.height)
				scroll.scrollPosition->y -= target.boundingBox.y + target.boundingBox.height -
											viewport.boundingBox.y - viewport.boundingBox.height;
		}
	}
	if (pressed(FlowUi::keyCode(FlowUi::Key::Enter)))
		activate(&navigation, navigation.domain_focus ? Domain : Group,
				 navigation.domain_focus ? navigation.focused_domain : navigation.focused_identity);
}
} // namespace

void DevDiagnosticsSelector::buildElement(BuildContext& context) {
	auto root = ui::column(0);
	root.layout.sizing = {.width = CLAY_SIZING_PERCENT(1), .height = CLAY_SIZING_PERCENT(1)};
	root.backgroundColor = interface_theme::kDepth1Panel;
	CLAY(context.clayID(), root) {
		if (!context.params.app || !context.params.interfaceState) {
			ui::text(context, "Diagnostics reporting unavailable", interface_theme::kTextMuted);
		} else {
			auto& state = context.params.interfaceState->diagnostics;
			context.state().diagnostics = &state;
			const auto previous_filters = state.filters;
			auto header = ui::row(38);
			header.layout.padding = {12, 10, 0, 0};
			header.backgroundColor = interface_theme::kDepth2Ink;
			header.border = {.color = interface_theme::kBorderPrimary, .width = {0, 0, 0, 1, 0}};
			CLAY(context.clayID("header"), header) {
				ui::icon(context, "issues-icon", Icon::Issue, interface_theme::kTextSecondary, 14);
				ui::text(context, "Issues", interface_theme::kTextCanvas, 13);
				auto spacer = ui::column();
				spacer.layout.sizing.height = CLAY_SIZING_FIXED(1);
				CLAY(context.clayID("count-spacer"), spacer) {}
				ui::text(context,
						 state.groups.size() == state.total_groups
							 ? std::to_string(state.groups.size()) + " groups"
							 : std::to_string(state.groups.size()) + " of " +
								   std::to_string(state.total_groups) + " groups",
						 interface_theme::kTextSecondary, 11);
			}
			context.uiManager.createElement(kDevSelectorSearch, "search")
				.setParameters({.query = &state.filters.search,
								.placeholder = "Search name, code, site, source or ID...",
								.center_text = true})
				.setDevInternalCapture(true)
				.draw();
			auto controls = ui::column(6);
			controls.layout.padding = {8, 8, 6, 7};
			CLAY(context.clayID("controls"), controls) {
				constexpr std::array<std::string_view, 3> activity_labels{"All", "Ongoing",
																		  "Recent"};
				auto activity_clip = ui::column(0);
				activity_clip.layout.sizing.height = CLAY_SIZING_FIXED(28);
				activity_clip.clip = {.horizontal = true, .scrollInputDisabled = true};
				CLAY(context.clayID("activity-clip"), activity_clip) {
					auto track = selector_choice_track();
					CLAY(context.clayID("activity-track"), track) {
						for (size_t label_index = 0; label_index < activity_labels.size();
							 ++label_index)
							choice(context, label_index, state.filters.activity_view,
								   activity_labels[label_index]);
					}
				}
				auto actions = ui::row();
				CLAY(context.clayID("actions"), actions) {
					const auto count = diagnostics_filter_count(state.filters);
					button(context, "show-filters",
						   count ? "Filters (" + std::to_string(count) + ")" : "Filters...",
						   Filters);
					if (count)
						button(context, "reset-filters", "Reset", Reset);
					if (!state.filters.search.empty())
						button(context, "clear-search", "Clear", ClearSearch);
				}
				CLAY(context.clayID("refresh-status"), ui::row()) {
					button(
						context, "refresh-order", state.order_changed ? "Refresh order" : "Refresh",
						RefreshOrder, 0,
						"Read the latest retained diagnostics, or rebuild the held frozen view.");
					if (state.refresh_count)
						ui::text(context, "Refreshed #" + std::to_string(state.refresh_count),
								 interface_theme::kAccentSeaGlass, 10);
				}
			}
			auto content = ui::column(0);
			content.layout.sizing.height = CLAY_SIZING_GROW(0);
			content.backgroundColor = interface_theme::kDepth0Keel;
			const auto scroll_id = context.clayID("scroll");
			const auto scroll = Clay_GetScrollContainerData(scroll_id);
			if (scroll.found && scroll.scrollPosition)
				state.selector_scroll = scroll.scrollPosition->y;
			content.clip = {.vertical = true,
							.childOffset = scroll.found && scroll.scrollPosition
											   ? *scroll.scrollPosition
											   : Clay_Vector2{0, state.selector_scroll}};
			CLAY(scroll_id, content) {
				auto tree = ui::column(0);
				CLAY(context.clayID("tree"), tree) {
					std::vector<ErrorDomain> domains{};
					domains.reserve(21);
					for (const auto& group : state.groups)
						if (std::ranges::find(domains, group.error.domain()) == domains.end())
							domains.emplace_back(group.error.domain());
					std::ranges::sort(domains, [](auto left, auto right) {
						return diagnostics_domain_name(left) < diagnostics_domain_name(right);
					});
					std::vector<NavigationRow> navigation{};
					navigation.reserve(state.groups.size() + domains.size());
					for (const auto domain : domains) {
						const auto domain_index = static_cast<uint64_t>(domain);
						navigation.emplace_back(NavigationRow{domain_index, 0});
						const auto count =
							std::ranges::count_if(state.groups, [&](const auto& group) {
								return group.error.domain() == domain;
							});
						context.uiManager.createElement(ui::kControl, Keyed("domain", domain_index))
							.setParameters(
								{.label =
									 diagnostics_domain_name(domain) + "  " + std::to_string(count),
								 .tooltip =
									 "Domain totals count issue groups, not individual "
									 "occurrences. All counts describe retained reporting data.",
								 .icon = state.domain_expanded[domain_index] ? Icon::Expanded
																			 : Icon::Collapsed,
								 .activate = activate,
								 .target = &context.state(),
								 .action = Domain,
								 .value = domain_index,
								 .focused = context.state().keyboard_active &&
											context.state().domain_focus &&
											context.state().focused_domain == domain_index,
								 .keyboard_activation = false,
								 .domain_row = true})
							.setDevInternalCapture(true)
							.draw();
						if (!state.domain_expanded[domain_index])
							continue;
						for (const auto& group : state.groups) {
							if (group.error.domain() != domain)
								continue;
							navigation.emplace_back(NavigationRow{domain_index, group.identity});
							std::string secondary(group.error.siteDescriptor().name);
							if (group.mode == DevErrorGroupMode::Native)
								secondary += " · native " + std::to_string(group.key.nativeCode);
							if (group.mode == DevErrorGroupMode::Source) {
								const auto* source = diagnostics_source(state, group.key.sourceId);
								secondary += " · " + (source ? source->name
															 : std::string("Source unavailable"));
							}
							secondary += " · " + std::to_string(group.recent_count) + " / " +
										 std::to_string(state.filters.ongoing_ticks) + " ticks";
							const bool selected =
								state.selected_group && *state.selected_group == group.key &&
								state.selected_mode == static_cast<uint64_t>(group.mode);
							context.uiManager
								.createElement(ui::kControl, Keyed("issue", group.identity))
								.setParameters(
									{.label = std::string(group.error.descriptor().name),
									 .secondary = std::move(secondary),
									 .badge = "x" + std::to_string(group.count),
									 .tooltip =
										 std::string(group.error.descriptor().name) + "\n" +
										 std::string(group.error.siteDescriptor().name) + "\n" +
										 std::to_string(group.count) +
										 " retained occurrences in this scope. " +
										 (group.ongoing
											  ? "Seen within " +
													std::to_string(state.filters.ongoing_ticks) +
													" ticks"
											  : "Quiet in this window; this does not mean "
												"resolved.") +
										 " Observation coverage is not guaranteed.",
									 .icon = Icon::Issue,
									 .activate = activate,
									 .target = &context.state(),
									 .action = Group,
									 .value = group.identity,
									 .marker = impact_color(group.error.descriptor().impact),
									 .selected = selected,
									 .focused = context.state().keyboard_active &&
												!context.state().domain_focus &&
												context.state().focused_identity == group.identity,
									 .issue_row = true,
									 .keyboard_activation = false})
								.setDevInternalCapture(true)
								.draw();
						}
					}
					keyboard(context, state, navigation);
					if (state.groups.empty()) {
						auto empty = ui::column(8);
						empty.layout.padding = {12, 12, 15, 15};
						CLAY(context.clayID("empty"), empty) {
							ui::text(context,
									 state.snapshot.rows.empty() ? "No retained issues"
																 : "No retained issues match",
									 interface_theme::kTextCanvas);
							ui::text(
								context,
								state.snapshot.capture_config.level ==
										DevErrorCaptureLevel::Disabled
									? "Capture is Disabled. Retained reports remain inspectable; "
									  "enable capture in the header for future evidence."
									: "All totals describe retained reporting evidence. New "
									  "runtime failures appear here after the application consumes "
									  "them.",
								interface_theme::kTextSecondary, 12);
							if (!state.snapshot.rows.empty())
								button(context, "empty-reset", "Clear filters", Reset);
						}
					}
				}
			}
			if (state.filters_open)
				filters(context, state);
			auto footer = ui::column(4);
			footer.layout.padding = {10, 10, 9, 9};
			footer.backgroundColor = interface_theme::kDepth2Ink;
			footer.border = {.color = interface_theme::kBorderPrimary, .width = {0, 0, 1, 0, 0}};
			CLAY(context.clayID("footer"), footer) {
				ui::text(context,
						 std::to_string(state.snapshot.status.retainedOccurrences) +
							 " retained occurrences · tick " + std::to_string(state.anchor_tick),
						 interface_theme::kTextSecondary, 11);
				const auto& status = state.snapshot.status;
				ui::text(context,
						 "Global: " + std::to_string(status.evictedOccurrences) + " evicted · " +
							 std::to_string(status.quality.droppedEvents) + " dropped · " +
							 std::to_string(status.retainedBytes) + " retained bytes",
						 status.evictedOccurrences || status.quality.droppedEvents
							 ? interface_theme::kStatusAmber
							 : interface_theme::kTextMuted,
						 10);
				if (state.frozen)
					ui::text(context, "Frozen at tick " + std::to_string(state.anchor_tick),
							 interface_theme::kAccentSeaGlass, 11);
			}
			if (!(previous_filters == state.filters)) {
				state.dirty = true;
				state.refresh_order = true;
				state.history_page = 0;
				state.selected_bin = UINT64_MAX;
			}
		}
	}
}
} // namespace FlowUi::devSystems::interface_elements

#endif
