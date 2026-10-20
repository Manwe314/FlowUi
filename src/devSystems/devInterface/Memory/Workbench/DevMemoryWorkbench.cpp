#include "devSystems/devInterface/Memory/Workbench/DevMemoryWorkbench.hpp"
#if FLOW_UI_DEV_MODE
#include "FSEL/RadioChoice.hpp"
#include "devSystems/devInterface/Memory/Analysis/DevMemoryUi.hpp"
#include "devSystems/devInterface/Memory/Workbench/DevMemoryHistoryViewport.hpp"
#include "devSystems/devInterface/Memory/Workbench/DevMemoryWorkbenchControls.hpp"
#include "devSystems/devInterface/Permanents/Backend/DevTheme.hpp"
#include "devSystems/devMonitoringAndReporting/DevMonitoringAndReporting.hpp"
#include "managers/UiManager.hpp"
#include <GLFW/glfw3.h>
#include <algorithm>
#include <array>
#include <cstdio>
#include <utility>
namespace FlowUi::devSystems::interface_elements {
namespace {
constexpr std::array<Clay_Color, 6> colors = {Flow_Color("#33465F"), Flow_Color("#49B9FF"),
											  Flow_Color("#BA8CFF"), Flow_Color("#FFD166"),
											  Flow_Color("#FF6B75"), interface_theme::kTextMuted};
template <class Context>
void text(Context& context, std::string_view label,
		  Clay_Color color = interface_theme::kTextSecondary, uint16_t size = 11) {
	Clay_TextElementConfig config{};
	config.fontSize = size;
	config.textColor = color;
	config.wrapMode = CLAY_TEXT_WRAP_WORDS;
	CLAY_TEXT(context.uiManager.toClayString(label), CLAY_TEXT_CONFIG(config));
}
Clay_ElementDeclaration column() {
	Clay_ElementDeclaration declaration{};
	declaration.layout.sizing = {.width = CLAY_SIZING_PERCENT(1), .height = CLAY_SIZING_FIT(0)};
	declaration.layout.layoutDirection = CLAY_TOP_TO_BOTTOM;
	declaration.layout.childGap = 4;
	return declaration;
}
Clay_ElementDeclaration row(float height = 24) {
	auto declaration = column();
	declaration.layout.layoutDirection = CLAY_LEFT_TO_RIGHT;
	declaration.layout.sizing.height = CLAY_SIZING_FIXED(height);
	declaration.layout.childAlignment.y = CLAY_ALIGN_Y_CENTER;
	declaration.layout.childGap = 6;
	return declaration;
}
template <class Context>
void button(Context& context, DevMemoryWorkbenchState& state, uint32_t action, std::string label,
			uint64_t identity = 0) {
	context.uiManager
		.createElement(
			kDevMemoryControl,
			Keyed("memory-control",
				  identity ? (identity | (uint64_t{1} << 63)) ^ (uint64_t(action) << 56) : action))
		.setParameters({.state = &state,
						.label = std::move(label),
						.action = action,
						.identity = identity,
						.selection = &context.params.interfaceState->memory_selection})
		.setDevInternalCapture(true)
		.draw();
}
void band(Clay_ElementId identity, float begin, float end, float height, Clay_Color color) {
	if (end <= begin)
		return;
	Clay_ElementDeclaration declaration{};
	declaration.layout.sizing = {.width = CLAY_SIZING_PERCENT(std::clamp(end - begin, 0.0f, 1.0f)),
								 .height = CLAY_SIZING_FIXED(height)};
	declaration.backgroundColor = color;
	declaration.border = {.color = interface_theme::kDepth0Keel, .width = {0, 1, 0, 0, 0}};
	CLAY(identity, declaration) {}
}
} // namespace
void DevMemoryWorkbench::buildElement(BuildContext& context) {
	if (!context.params.app || !context.params.interfaceState)
		return;
	auto& state = context.params.interfaceState->memory_workbench;
	const auto& selection = context.params.interfaceState->memory_selection;
	auto& reporting = context.params.app->devMonitoring().memoryReporting();
	const auto now = memory_now_ns();
	update_memory_session(state.analysis, reporting);
	if (state.analysis.previous_dataset != state.analysis.dataset) {
		state.analysis.previous_dataset = state.analysis.dataset;
		state.held_data.reset();
		state.selected_ns = state.range_begin_ns = state.range_end_ns = 0;
		state.view_begin_ns = state.view_end_ns = 0;
		state.following = state.analysis.dataset == 0;
		state.fit_pending = !state.following;
		state.next_live_ns = state.next_history_ns = 0;
		state.analysis.last_refresh_ns = 0;
	}
	const auto history_action = state.analysis.ui_action >= 200 && state.analysis.ui_action < 300
									? std::exchange(state.analysis.ui_action, 0)
									: 0;
	if (history_action == 204)
		state.history_settings_open = !state.history_settings_open;
	if (history_action == 206)
		state.history_settings_open = false;
	if (history_action == 205 && state.lane_count < 3) {
		state.lane_settings[state.lane_count] = {};
		++state.lane_count;
		state.next_history_ns = 0;
	}
	if (history_action >= 210 && history_action < 210 + state.lane_count && state.lane_count > 1) {
		const auto removed = history_action - 210;
		for (size_t lane_index = removed; lane_index + 1 < state.lane_count; ++lane_index)
			state.lane_settings[lane_index] = state.lane_settings[lane_index + 1];
		--state.lane_count;
		state.next_history_ns = 0;
	}
	const auto previous_width = Clay_GetElementData(context.clayID()).boundingBox.width;
	const bool compact = previous_width > 0 && previous_width < 640;
	const auto action = std::exchange(state.pending_action, 0);
	if (action == 1)
		state.units_choice = state.decimal ? 0 : 1;
	const bool units_changed = state.decimal != (state.units_choice == 1);
	state.decimal = state.units_choice == 1;
	if (action == 2) {
		state.following = !state.following;
		if (!state.following)
			hold_memory_history(state, reporting);
		if (state.following) {
			state.held_data.reset();
			state.selected_ns = state.range_begin_ns = state.range_end_ns = 0;
			state.next_live_ns = 0;
		}
		state.next_history_ns = 0;
	}
	if (action == 3) {
		hold_memory_history(state, reporting);
		state.following = false;
		state.fit_pending = true;
		state.next_history_ns = 0;
	}
	if (action == 4 || action == 5) {
		hold_memory_history(state, reporting);
		zoom_memory_view(state, action == 4 ? 2 : .5);
	}
	if (action == 6)
		state.expanded = !state.expanded;
	if (action == 7) {
		state.range_begin_ns = 0;
		state.range_end_ns = 0;
		state.selected_ns = 0;
		state.next_live_ns = 0;
	}
	const bool scope_changed =
		state.scope != selection.selected_node || state.window != selection.window_scope;
	if (scope_changed) {
		state.focus = 0;
		state.lane_settings[0].source_index = 0;
		state.held_data.reset();
		state.next_live_ns = 0;
		state.next_history_ns = 0;
	}
	if (now >= state.next_live_ns || units_changed) {
		const auto* saved = selected_memory_dataset(state.analysis);
		resolve_memory_tracks(state, selection, saved ? saved->live : reporting.read_live());
		const auto live_primary =
			std::ranges::find_if(state.tracks, [](const auto& track) { return track.available; });
		state.live_summary = live_primary == state.tracks.end()
								 ? "Live readings unavailable"
								 : "Live: " + memory_track_reading(*live_primary, state.decimal);
		if (state.selected_ns)
			resolve_memory_history_tracks(state, reporting, state.selected_ns);
		state.next_live_ns = now + 200'000'000;
	}
#if FLOWUI_PUBLIC_VULKAN_INTEROP
	auto* controller = context.uiManager.memory_history_controller();
#else
	constexpr auto* controller = static_cast<DevMemoryHistoryController*>(nullptr);
#endif
	std::string track_tooltip;
	auto root = column();
	root.layout.sizing = {.width = CLAY_SIZING_PERCENT(1), .height = CLAY_SIZING_PERCENT(1)};
	root.layout.padding = {12, 12, 12, 12};
	root.layout.childGap = 10;
	root.backgroundColor = interface_theme::kDepth0Keel;
	root.clip = {.horizontal = true, .vertical = true};
	const auto root_scroll = Clay_GetScrollContainerData(context.clayID());
	if (root_scroll.found && root_scroll.scrollPosition)
		root.clip.childOffset = *root_scroll.scrollPosition;
	CLAY(context.clayID(), root) {
		auto header = row(32);
		header.backgroundColor = interface_theme::kDepth2Ink;
		header.layout.padding = {8, 8, 2, 2};
		CLAY(context.clayID("header"), header) {
			const auto* node = memory_node(selection.selected_node);
			text(context, node ? node->label : "Overview", interface_theme::kTextCanvas, 13);
			auto spacer = row(1);
			spacer.layout.sizing.width = CLAY_SIZING_GROW(0);
			CLAY(context.clayID("header-spacer"), spacer) {}
			for (uint64_t choice = 0; choice < 2; ++choice) {
				FSEL::RadioChoiceParameters parameters{};
				parameters.choiceValue = choice;
				parameters.selectedValue = &state.units_choice;
				parameters.style.sizing = {.width = CLAY_SIZING_FIT(0),
										   .height = CLAY_SIZING_FIXED(24)};
				parameters.style.padding = Clay_Padding{8, 8, 2, 2};
				context.uiManager.createElement(FSEL::kRadioChoice, Keyed("units", choice))
					.setParameters(std::move(parameters))
					.setDevInternalCapture(true)
					.construct();
				text(context, choice ? "Decimal" : "Binary", interface_theme::kTextCanvas);
				context.uiManager.drawConstructed();
			}
		}
		auto distribution = column();
		distribution.layout.sizing.height = CLAY_SIZING_FIT(0);
		CLAY(context.clayID("distribution"), distribution) {
			text(context,
				 state.selected_ns ? "Distribution · historical sampled values"
				 : selected_memory_dataset(state.analysis) ? "Distribution · saved observation"
														   : "Distribution · live accounting views",
				 interface_theme::kTextCanvas, 12);
			CLAY(context.clayID("legend"), row(18)) {
				constexpr std::array<std::string_view, 5> labels = {"Backing", "Active", "Reusable",
																	"Padding / reserve", "Retired"};
				for (size_t entry = 0; entry < labels.size(); ++entry) {
					auto swatch = row(8);
					swatch.layout.sizing.width = CLAY_SIZING_FIXED(8);
					swatch.backgroundColor = colors[entry];
					CLAY(context.clayID(Keyed("legend-swatch", entry)), swatch) {}
					text(context, labels[entry], interface_theme::kTextSecondary, 10);
				}
			}
			auto gauge = row(30);
			gauge.layout.childGap = 0;
			CLAY(context.clayID("gauge"), gauge) {
				for (int tick = 0; tick < 3; ++tick) {
					auto cell = column();
					cell.layout.sizing.width = CLAY_SIZING_PERCENT(1.0f / 3);
					cell.layout.childAlignment.x = tick == 0   ? CLAY_ALIGN_X_LEFT
												   : tick == 1 ? CLAY_ALIGN_X_CENTER
															   : CLAY_ALIGN_X_RIGHT;
					cell.border = {.color = interface_theme::kBorderVisible,
								   .width = {1, uint16_t(tick == 2), 0, 1, 0}};
					CLAY(context.clayID(Keyed("gauge-tick", tick)), cell) {
						text(context,
							 memory_bytes(
								 uint64_t(static_cast<long double>(state.scale_bytes) * tick / 2),
								 state.decimal));
					}
				}
			}
			auto scroll = column();
			scroll.layout.sizing.height = CLAY_SIZING_FIT(0);
			scroll.layout.childGap = 8;
			scroll.clip = {.horizontal = true, .vertical = false};
			const auto scroll_data = Clay_GetScrollContainerData(context.clayID("tracks"));
			if (scroll_data.found && scroll_data.scrollPosition)
				scroll.clip.childOffset = *scroll_data.scrollPosition;
			CLAY(context.clayID("tracks"), scroll) {
				for (size_t group_begin = 0; group_begin < state.tracks.size();) {
					size_t group_end = group_begin + 1;
					while (group_end < state.tracks.size()) {
						const auto parent_identity = state.tracks[group_end].parent_identity;
						bool parent_present = false;
						for (size_t parent_index = group_begin; parent_index < group_end;
							 ++parent_index)
							parent_present |=
								parent_identity != 0 &&
								state.tracks[parent_index].identity == parent_identity;
						if (!parent_present)
							break;
						++group_end;
					}
					auto group = column();
					group.layout.childGap = 0;
					group.backgroundColor = interface_theme::kDepth2Ink;
					group.cornerRadius = CLAY_CORNER_RADIUS(4);
					group.border = {.color = interface_theme::kBorderPrimary,
									.width = {1, 1, 1, 1, 0}};
					CLAY(context.clayID(Keyed("track-group", uint32_t(group_begin))), group) {
						for (size_t index = group_begin; index < group_end; ++index) {
							const auto& track = state.tracks[index];
							auto track_row = column();
							const float track_height = compact ? 76 : 58;
							track_row.layout.sizing.height = CLAY_SIZING_FIXED(track_height);
							uint16_t depth = 0;
							auto parent_identity = track.parent_identity;
							for (size_t parent_index = index; parent_index > group_begin;) {
								--parent_index;
								if (parent_identity &&
									state.tracks[parent_index].identity == parent_identity) {
									++depth;
									parent_identity = state.tracks[parent_index].parent_identity;
								}
							}
							track_row.layout.padding = {uint16_t(8 + depth * 14), 8, 4, 4};
							track_row.border = {
								.color = interface_theme::kBorderPrimary,
								.width = {0, 0, uint16_t(index > group_begin), 0, 0}};
							const auto track_identity =
								track.identity ? track.identity : 1000 + index;
							context.uiManager
								.createElement(
									kDevMemoryControl,
									Keyed("memory-control", track_identity | (uint64_t{1} << 63)))
								.setParameters(
									{.surface = track_row,
									 .state = &state,
									 .identity = track_identity,
									 .selection = &context.params.interfaceState->memory_selection,
									 .whole_track = true})
								.setDevInternalCapture(true)
								.construct();
							{
								CLAY(context.clayID(Keyed("track-title", uint32_t(index))), row()) {
									text(context, track.title, interface_theme::kTextCanvas, 12);
									auto title_spacer = row(1);
									title_spacer.layout.sizing.width = CLAY_SIZING_GROW(0);
									CLAY(context.clayID(Keyed("track-spacer", uint32_t(index))),
										 title_spacer) {}
									if (memory_node(track.identity) &&
										track.identity != selection.selected_node)
										button(context, state, 9, "Select Scope", track.identity);
									if (!compact)
										text(context, memory_track_reading(track, state.decimal),
											 track.available ? interface_theme::kTextSecondary
															 : interface_theme::kStatusAmber);
								}
								if (compact)
									text(context, memory_track_reading(track, state.decimal),
										 track.available ? interface_theme::kTextSecondary
														 : interface_theme::kStatusAmber);

								if (track.available) {
									auto bar = row(18);
									bar.layout.childGap = 0;
									if (track.recipe == MemoryTrackRecipe::Independent)
										bar.layout.layoutDirection = CLAY_TOP_TO_BOTTOM;
									bar.backgroundColor = interface_theme::kDepth0Keel;
									CLAY(context.clayID(Keyed("track-bar", uint32_t(index))), bar) {
										const auto fraction = [&](uint64_t bytes) {
											return float(double(bytes) / state.scale_bytes);
										};
										const auto active = fraction(track.value.logicalLiveBytes),
												   backing =
													   fraction(track.value.backingAllocatedBytes);
										const auto previous_bar =
											Clay_GetElementData(
												context.clayID(Keyed("track-bar", uint32_t(index))))
												.boundingBox;
										const auto& input =
											context.uiManager.getCurrentFrameInput();
										if (input.mouseX >= previous_bar.x &&
											input.mouseX < previous_bar.x + previous_bar.width &&
											input.mouseY >= previous_bar.y &&
											input.mouseY < previous_bar.y + previous_bar.height) {
											track_tooltip =
												track.title + " · " +
												(state.selected_ns ? "Historical samples"
																   : "Live measurement") +
												"\n" + memory_track_reading(track, state.decimal);
											if (track.value.value_fields &
												uint16_t(MemoryValueField::Reusable))
												track_tooltip +=
													"\nReusable: " +
													memory_bytes(track.value.reusableBytes,
																 state.decimal);
											if (track.value.value_fields &
												uint16_t(MemoryValueField::LogicalLive))
												track_tooltip +=
													"\nExact active: " +
													std::to_string(track.value.logicalLiveBytes) +
													" bytes";
											if (track.value.value_fields &
												uint16_t(MemoryValueField::Backing))
												track_tooltip +=
													"\nExact backing: " +
													std::to_string(
														track.value.backingAllocatedBytes) +
													" bytes";
											if (track.reference_bytes)
												track_tooltip += "\nDefault reserve: " +
																 memory_bytes(track.reference_bytes,
																			  state.decimal);
											track_tooltip += "\nLatest report tick: " +
															 std::to_string(track.sampled_tick) +
															 "\n" + track.note;
										}
										band(context.clayID(Keyed("active", uint32_t(index))), 0,
											 active,
											 track.recipe == MemoryTrackRecipe::Independent ? 8
																							: 18,
											 colors[1]);
										if (track.recipe == MemoryTrackRecipe::Independent)
											band(context.clayID(
													 Keyed("independent-backing", uint32_t(index))),
												 0, backing, 8, colors[0]);
										else if (track.recipe != MemoryTrackRecipe::ActiveOnly) {
											const auto padding_end = std::min(
												backing, active + fraction(track.padding_bytes));
											const auto reusable_begin = std::max(
												padding_end,
												backing - fraction(track.value.reusableBytes));
											band(context.clayID(Keyed("padding", uint32_t(index))),
												 active, padding_end, 18, colors[3]);
											band(context.clayID(Keyed("backing", uint32_t(index))),
												 padding_end, reusable_begin, 18,
												 track.recipe == MemoryTrackRecipe::Resource
													 ? colors[4]
													 : colors[0]);
											band(context.clayID(Keyed("reusable", uint32_t(index))),
												 reusable_begin, backing, 18, colors[2]);
										}
									}
								}
							}
							context.uiManager.drawConstructed();
						}
					}
					group_begin = group_end;
				}
			}
		}
		auto history = column();
		history.layout.childGap = 0;
		history.backgroundColor = interface_theme::kDepth1Panel;
		history.border = {.color = interface_theme::kBorderPrimary, .width = {1, 1, 1, 1, 0}};
		CLAY(context.clayID("history"), history) {
			auto toolbar = row(28);
			toolbar.layout.padding = {8, 8, 2, 2};
			toolbar.backgroundColor = interface_theme::kDepth2Ink;
			CLAY(context.clayID("history-controls"), toolbar) {
				text(context, "History", interface_theme::kTextCanvas);
				if (!compact)
					text(context,
						 state.lanes.empty() ? "Selected scope" : state.lanes.front().label,
						 interface_theme::kTextSecondary, 10);
				auto spacer = row(1);
				spacer.layout.sizing.width = CLAY_SIZING_GROW(0);
				CLAY(context.clayID("history-spacer"), spacer) {}
				static constexpr auto windows = std::to_array<FSEL::ComboBoxOption>(
					{{0, "1s"}, {1, "5s"}, {2, "30s"}, {3, "All"}});
				FSEL::ComboBoxParameters window{};
				window.options = windows;
				window.selectedValue = &state.duration_choice;
				window.open = &state.history_window_open;
				window.fontSize = 11;
				window.sizing =
					Clay_Sizing{.width = CLAY_SIZING_FIXED(65), .height = CLAY_SIZING_FIXED(24)};
				context.uiManager.createElement(FSEL::kComboBox, "history-window")
					.setParameters(window)
					.setDevInternalCapture(true)
					.draw();
				button(context, state, 3, "Fit");
				auto follow = row(24);
				follow.layout.sizing.width = CLAY_SIZING_FIT(0);
				if (state.following) {
					follow.backgroundColor = interface_theme::kSelectedRow;
					follow.border = {.color = interface_theme::kAccentCurrent,
									 .width = {0, 0, 0, 1, 0}};
				}
				CLAY(context.clayID("history-follow"), follow) {
					button(context, state, 2, state.following ? "Live" : "Follow live");
				}
				memory_ui::button(context, state, 204, "Settings");
			}
			if (state.duration_choice != state.previous_duration_choice) {
				state.previous_duration_choice = state.duration_choice;
				state.duration_ns = state.duration_choice == 0	 ? 1'000'000'000ull
									: state.duration_choice == 1 ? 5'000'000'000ull
																 : 30'000'000'000ull;
				state.fit_pending = state.duration_choice == 3;
				if (!state.following && !state.fit_pending)
					state.view_begin_ns = std::max(state.retained_begin_ns,
												   state.view_end_ns > state.duration_ns
													   ? state.view_end_ns - state.duration_ns
													   : 0);
				state.next_history_ns = 0;
			}
			const auto popup_id = ::FlowUi::detail::element_id::resolveLocal(
				context.id, DevMemoryWorkbench::definitionId,
				LocalElementName{"history-settings"}.token);
			if (context.uiManager.popups().consumeDismissed(popup_id))
				state.history_settings_open = false;
			if (state.history_settings_open) {
				const auto anchor = ::FlowUi::detail::element_id::resolveLocal(
					context.id, DevMemoryControl::definitionId, Keyed("memory-action", 204).token);
				PopupRequest request{};
				request.anchor = PopupAnchor::element(anchor);
				request.placement = {
					PopupAttachmentPoint::BottomRight, PopupAttachmentPoint::TopRight, {0, 4}};
				request.expectedSize = Clay_Dimensions{340, float(68 + state.lane_count * 140)};
				request.outsidePress = PopupOutsidePressPolicy::DismissAndConsume;
				const auto frame =
					context.uiManager.popups().request(popup_id, request).value_or(PopupFrame{});
				if (frame.visible) {
					auto panel = column();
					panel.layout.sizing.width =
						CLAY_SIZING_FIXED(std::min(340.0f, std::max(260.0f, previous_width - 16)));
					panel.layout.padding = {10, 10, 10, 10};
					panel.layout.childGap = 8;
					panel.backgroundColor = interface_theme::kDepth3Elevated;
					panel.border = {.color = interface_theme::kBorderVisible,
									.width = {1, 1, 1, 1, 0}};
					panel.floating = frame.floating;
					CLAY(context.uiManager.toClayEID(popup_id), panel) {
						CLAY(context.clayID("lane-settings-title"), row(24)) {
							text(context, "History lanes", interface_theme::kTextCanvas, 12);
							memory_ui::button(context, state, 205, "+ Lane", state.lane_count < 3);
							memory_ui::button(context, state, 206, "Done");
						}
						static const auto scope_labels = [] {
							std::vector<std::string> labels;
							labels.reserve(memory_selector_nodes.size());
							for (const auto& node : memory_selector_nodes)
								labels.emplace_back(
									std::string(node.label) +
									(node.application_wide ? " · Application" : " · Window scope"));
							return labels;
						}();
						static const auto scopes = [] {
							std::vector<FSEL::ComboBoxOption> options;
							options.reserve(memory_selector_nodes.size() + 2);
							options.emplace_back(
								FSEL::ComboBoxOption{UINT64_MAX, "Follow selector"});
							options.emplace_back(FSEL::ComboBoxOption{0, "Overview"});
							for (size_t node_index = 0; node_index < memory_selector_nodes.size();
								 ++node_index)
								options.emplace_back(FSEL::ComboBoxOption{
									uint64_t(memory_selector_nodes[node_index].id),
									scope_labels[node_index]});
							return options;
						}();
						for (size_t lane_index = 0; lane_index < state.lane_count; ++lane_index) {
							auto& settings = state.lane_settings[lane_index];
							auto lane_panel = column();
							lane_panel.layout.childGap = 4;
							lane_panel.layout.padding = {6, 6, 6, 6};
							lane_panel.backgroundColor = interface_theme::kDepth1Panel;
							CLAY(context.clayID(Keyed("lane-settings", lane_index)), lane_panel) {
								CLAY(context.clayID(Keyed("lane-heading", lane_index)), row(22)) {
									text(context, "Lane " + std::to_string(lane_index + 1),
										 interface_theme::kTextCanvas);
									memory_ui::button(context, state, 210 + uint32_t(lane_index),
													  "Remove", state.lane_count > 1);
								}
								FSEL::ComboBoxParameters scope{};
								scope.options = scopes;
								scope.selectedValue = &settings.scope;
								scope.fontSize = 11;
								scope.popupMaxHeight = 240;
								scope.sizing = Clay_Sizing{.width = CLAY_SIZING_GROW(0),
														   .height = CLAY_SIZING_FIXED(26)};
								const auto old_scope = settings.scope;
								context.uiManager
									.createElement(FSEL::kComboBox, Keyed("lane-scope", lane_index))
									.setParameters(scope)
									.setDevInternalCapture(true)
									.draw();
								if (old_scope != settings.scope)
									settings.source_index = 0;
								if (state.source_options[lane_index].size() > 1) {
									scope.options = state.source_options[lane_index];
									scope.selectedValue = &settings.source_index;
									context.uiManager
										.createElement(FSEL::kComboBox,
													   Keyed("lane-source", lane_index))
										.setParameters(scope)
										.setDevInternalCapture(true)
										.draw();
								}
								CLAY(context.clayID(Keyed("lane-measurement", lane_index)),
									 row(24)) {
									for (uint64_t metric = 0; metric < 2; ++metric) {
										FSEL::RadioChoiceParameters radio{};
										radio.choiceValue = metric;
										radio.selectedValue = &settings.metric;
										radio.style.sizing = {.width = CLAY_SIZING_GROW(0),
															  .height = CLAY_SIZING_FIXED(24)};
										radio.style.padding = Clay_Padding{6, 6, 2, 2};
										context.uiManager
											.createElement(
												FSEL::kRadioChoice,
												Keyed("lane-metric", lane_index * 2 + metric))
											.setParameters(radio)
											.setDevInternalCapture(true)
											.construct();
										text(context,
											 metric ? "Objects / capacity" : "Active / backing",
											 interface_theme::kTextCanvas, 10);
										context.uiManager.drawConstructed();
									}
								}
							}
						}
					}
				}
			} else
				context.uiManager.popups().dismiss(popup_id);
			uint64_t settings_key = state.lane_count;
			for (const auto& settings : state.lane_settings)
				for (const auto value : {settings.scope, settings.source_index, settings.metric})
					settings_key = (settings_key ^ value) * 1099511628211ull;
			if (settings_key != state.history_settings_key) {
				state.history_settings_key = settings_key;
				state.history_scales = {1, 1, 1};
				state.next_history_ns = 0;
			}
			if (now >= state.next_history_ns) {
				const auto plot_width =
					Clay_GetElementData(context.clayID("history-image")).boundingBox.width;
				build_memory_history(state, reporting,
									 plot_width > 68 ? size_t(plot_width - 68) : 512);
				state.next_history_ns = now + 100'000'000ull;
			}
			auto plot = row();
			plot.layout.sizing.height =
				CLAY_SIZING_FIXED(state.lane_count == 1 ? 238 : 18 + float(state.lane_count) * 160);
			plot.clip = {.horizontal = true, .vertical = true};
			CLAY(context.clayID("history-plot"), plot) {
#if FLOWUI_PUBLIC_VULKAN_INTEROP
				if (controller)
					controller->draw(context.uiManager, context.clayID("history-image"), state,
									 reporting);
#endif
				if (!controller)
					text(context, "History viewport unavailable", interface_theme::kStatusAmber);
			}
			auto status = row(22);
			status.layout.padding = {8, 8, 2, 2};
			status.backgroundColor = interface_theme::kDepth2Ink;
			status.clip = {.horizontal = true, .vertical = true};
			CLAY(context.clayID("history-context"), status) {
				Clay_Color context_color = interface_theme::kTextSecondary;
				if (state.range_end_ns > state.range_begin_ns && !state.lanes.empty()) {
					const auto& range =
						state.lanes[std::min<size_t>(state.inspected_lane, state.lanes.size() - 1)]
							.range;
					if (range.incomplete || !range.valid)
						context_color = interface_theme::kStatusAmber;
					else if (range.backing_after < range.backing_before ||
							 range.active_after < range.active_before)
						context_color = interface_theme::kAccentSeaGlass;
					else if (range.backing_after > range.backing_before)
						context_color = interface_theme::kStatusAmber;
				}
				char retained_label[100];
				std::snprintf(retained_label, sizeof(retained_label), "Retained %.1fs (%s) · ",
							  double(state.retained_end_ns - state.retained_begin_ns) / 1e9,
							  state.following ? "live" : "held");
				text(context,
					 !state.viewport_error.empty() ? state.viewport_error
					 : !state.tooltip.empty()	   ? state.tooltip
					 : state.lanes.empty()
						 ? state.message
						 : std::string(retained_label) +
							   "Drag: range · Middle/right: pan · Scroll: zoom · Click: cursor",
					 context_color, 10);
			}
		}

		if (!track_tooltip.empty()) {
			const auto& input = context.uiManager.getCurrentFrameInput();
			auto tooltip = column();
			tooltip.layout.sizing.width = CLAY_SIZING_FIXED(320);
			tooltip.layout.padding = {8, 8, 6, 6};
			tooltip.backgroundColor = interface_theme::kDepth3Elevated;
			tooltip.border = {.color = interface_theme::kAccentSeaGlass, .width = {1, 1, 1, 1, 0}};
			tooltip.floating.attachTo = CLAY_ATTACH_TO_ROOT;
			const auto panel = Clay_GetElementData(context.clayID()).boundingBox;
			tooltip.floating.offset = {std::clamp(input.mouseX - 160, panel.x,
												  std::max(panel.x, panel.x + panel.width - 320)),
									   std::clamp(input.mouseY - 130, panel.y,
												  std::max(panel.y, panel.y + panel.height - 180))};
			tooltip.floating.zIndex = 100;
			tooltip.floating.pointerCaptureMode = CLAY_POINTER_CAPTURE_MODE_PASSTHROUGH;
			CLAY(context.clayID("track-tooltip"), tooltip) {
				text(context, track_tooltip, interface_theme::kTextCanvas);
			}
		}
	}
}
} // namespace FlowUi::devSystems::interface_elements
#endif
