#include "devSystems/devInterface/Memory/Workbench/DevMemoryWorkbench.hpp"
#if FLOW_UI_DEV_MODE
#include "FSEL/RadioChoice.hpp"
#include "FSEL/Slider.hpp"
#include "devSystems/devInterface/Memory/Analysis/DevMemoryUi.hpp"
#include "devSystems/devInterface/Memory/Workbench/DevMemoryHistoryViewport.hpp"
#include "devSystems/devInterface/Memory/Workbench/DevMemoryWorkbenchControls.hpp"
#include "devSystems/devInterface/Permanents/Backend/DevTheme.hpp"
#include "devSystems/devMonitoringAndReporting/DevMonitoringAndReporting.hpp"
#include "managers/UiManager.hpp"
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
	if (history_action == 200 || history_action == 201) {
		hold_memory_history(state, reporting);
		pan_memory_view(state, history_action == 200 ? -1 : 1);
	}
	if (history_action == 202) {
		state.scale_mode = 0;
		state.next_history_ns = 0;
	}
	if (history_action == 203) {
		state.show_backing = !state.show_backing;
		state.next_history_ns = 0;
	}
	if (state.navigator != state.navigator_applied &&
		state.retained_end_ns > state.retained_begin_ns) {
		hold_memory_history(state, reporting);
		const auto width = std::min(state.view_end_ns > state.view_begin_ns
										? state.view_end_ns - state.view_begin_ns
										: state.duration_ns,
									state.retained_end_ns - state.retained_begin_ns);
		const auto available = state.retained_end_ns - state.retained_begin_ns - width;
		state.view_begin_ns =
			state.retained_begin_ns + uint64_t(available * std::clamp(state.navigator, 0.0, 1.0));
		state.view_end_ns = state.view_begin_ns + width;
		state.following = false;
		state.next_history_ns = 0;
		state.navigator_applied = state.navigator;
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
			state.selected_ns = 0;
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
		state.series_index = 0;
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
		history.layout.sizing.height = CLAY_SIZING_FIT(0);
		CLAY(context.clayID("history"), history) {
			CLAY(context.clayID("history-controls"), row()) {
				text(context, "History", interface_theme::kTextCanvas, 12);
				button(context, state, 2, state.following ? "Hold view" : "Follow latest");
				button(context, state, 3, "Fit time");
				memory_ui::button(context, state, 202, "Fit values");
			}
			CLAY(context.clayID("history-navigation"), row()) {
				memory_ui::button(context, state, 200, "Pan left");
				memory_ui::button(context, state, 201, "Pan right");
				button(context, state, 4, "Zoom out");
				button(context, state, 5, "Zoom in");
			}
			static constexpr auto windows =
				std::to_array<FSEL::ComboBoxOption>({{0, "1 second"},
													 {1, "5 seconds"},
													 {2, "30 seconds"},
													 {3, "All retained / capture"}});
			static constexpr auto gestures = std::to_array<FSEL::ComboBoxOption>(
				{{0, "Pan / click to inspect"}, {1, "Select range"}});
			static constexpr auto modes = std::to_array<FSEL::ComboBoxOption>(
				{{0, "Focused series"}, {1, "Compare up to 3"}});
			static constexpr auto metrics =
				std::to_array<FSEL::ComboBoxOption>({{0, "Active / backing bytes"},
													 {1, "Backing bytes"},
													 {2, "Objects / capacity count"}});
			static constexpr auto scales =
				std::to_array<FSEL::ComboBoxOption>({{0, "Auto: independent scales"},
													 {1, "Manual value scale"},
													 {2, "Shared comparison scale"}});
			memory_ui::choice(context, "history-window", "Time window", state.duration_choice,
							  windows);
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
			memory_ui::choice(context, "history-mode", "Chart presentation", state.history_mode,
							  modes);
			if (!state.lane_options.empty()) {
				if (!state.history_mode)
					memory_ui::choice(context, "history-series",
									  "Constituent measurement (independent views)",
									  state.series_index, state.lane_options);
				else
					for (size_t index = 0; index < state.comparison_indices.size(); ++index) {
						const auto identity = index == 0   ? LocalElementName{"compare-first"}
											  : index == 1 ? LocalElementName{"compare-second"}
														   : LocalElementName{"compare-third"};
						memory_ui::choice(context, identity, "Compare series",
										  state.comparison_indices[index], state.lane_options);
					}
			}
			memory_ui::choice(context, "history-metric", "Measurement", state.history_metric,
							  metrics);
			memory_ui::choice(context, "history-gesture", "Pointer mode", state.gesture_mode,
							  gestures);
			memory_ui::choice(context, "history-scale", "Value axis", state.scale_mode, scales);
			if (state.scale_mode == 1)
				memory_ui::number(context, "history-manual-scale",
								  "Axis maximum (bytes / native count)", state.manual_scale, 1.0,
								  9e18);
			memory_ui::button(context, state, 203,
							  state.show_backing ? "Backing / capacity outline: on"
												 : "Backing / capacity outline: off");
			const auto settings_key =
				state.series_index ^ (state.history_mode << 8) ^ (state.history_metric << 10) ^
				(state.scale_mode << 12) ^ (uint64_t(state.show_backing) << 14) ^
				(state.comparison_indices[0] << 16) ^ (state.comparison_indices[1] << 24) ^
				(state.comparison_indices[2] << 32) ^ uint64_t(state.manual_scale);
			if (settings_key != state.history_settings_key) {
				state.history_settings_key = settings_key;
				state.next_history_ns = 0;
			}
			if (now >= state.next_history_ns) {
				const auto plot_width =
					Clay_GetElementData(context.clayID("history-image")).boundingBox.width;
				build_memory_history(state, reporting, plot_width > 0 ? size_t(plot_width) : 512);
				state.next_history_ns = now + 500'000'000ull;
			}
			for (const auto& lane : state.lanes)
				memory_ui::text(context, lane.label, interface_theme::kTextCanvas, 11);
			auto plot = row();
			plot.layout.sizing.height =
				CLAY_SIZING_FIXED(state.lanes.size() > 1 ? float(state.lanes.size()) * 160
								  : state.expanded		 ? 340
														 : 220);
			plot.clip = {.horizontal = true, .vertical = true};
			CLAY(context.clayID("history-plot"), plot) {
				auto value_rulers = column();
				value_rulers.layout.sizing = {.width = CLAY_SIZING_FIXED(78),
											  .height = CLAY_SIZING_GROW(0)};
				value_rulers.layout.childGap = 0;
				CLAY(context.clayID("value-rulers"), value_rulers) {
					for (size_t lane_index = 0;
						 lane_index < std::max<size_t>(1, state.lanes.size()); ++lane_index) {
						auto ruler = column();
						ruler.layout.sizing.height =
							CLAY_SIZING_PERCENT(1.0f / std::max<size_t>(1, state.lanes.size()));
						ruler.layout.childGap = 0;
						ruler.layout.padding = {0, 4, 1, 1};
						CLAY(context.clayID(Keyed("value-ruler", lane_index)), ruler) {
							for (size_t tick = 0; tick < 5; ++tick) {
								auto cell = column();
								cell.layout.sizing.height =
									tick == 4 ? CLAY_SIZING_FIXED(14) : CLAY_SIZING_GROW(0);
								const auto maximum = lane_index < state.lanes.size()
														 ? state.lanes[lane_index].scale
														 : 1;
								CLAY(context.clayID(Keyed("value-tick", lane_index * 5 + tick)),
									 cell) {
									text(context,
										 state.history_metric == 2
											 ? std::to_string(
												   uint64_t(static_cast<long double>(maximum) *
															(4 - tick) / 4))
											 : memory_bytes(
												   uint64_t(static_cast<long double>(maximum) *
															(4 - tick) / 4),
												   state.decimal),
										 interface_theme::kTextSecondary, 10);
								}
							}
						}
					}
				}
#if FLOWUI_PUBLIC_VULKAN_INTEROP
				if (controller)
					controller->draw(context.uiManager, context.clayID("history-image"), state,
									 reporting);
#endif
				if (!state.viewport_error.empty())
					text(context, state.viewport_error, interface_theme::kStatusAmber);
				if (!controller)
					text(context, "History viewport unavailable in this build",
						 interface_theme::kStatusAmber);
			}
			if (state.retained_end_ns > state.retained_begin_ns) {
				text(context, "Retained time navigator", interface_theme::kTextMuted, 10);
				FSEL::SliderParameters navigator{};
				navigator.value = &state.navigator;
				navigator.minimum = 0;
				navigator.maximum = 1;
				navigator.length = std::max(120.0f, previous_width - 40);
				context.uiManager.createElement(FSEL::kSlider, "history-navigator")
					.setParameters(navigator)
					.setDevInternalCapture(true)
					.draw();
			}
			if (state.selected_ns || state.range_begin_ns)
				button(context, state, 7, "Clear selection");
			if (!state.lanes.empty()) {
				auto time_ruler = row(16);
				time_ruler.layout.padding.left = 84;
				time_ruler.layout.childGap = 0;
				CLAY(context.clayID("time-ruler"), time_ruler) {
					for (int tick = 0; tick < 3; ++tick) {
						auto cell = row(16);
						cell.layout.sizing.width = CLAY_SIZING_PERCENT(1.0f / 3);
						cell.layout.childAlignment.x = tick == 0   ? CLAY_ALIGN_X_LEFT
													   : tick == 1 ? CLAY_ALIGN_X_CENTER
																   : CLAY_ALIGN_X_RIGHT;
						char label[48];
						std::snprintf(label, sizeof(label), "%+.2f s",
									  -double(state.view_end_ns - state.view_begin_ns) / 1e9 *
										  (1 - double(tick) / 2));
						CLAY(context.clayID(Keyed("time-tick", tick)), cell) {
							text(context, label, interface_theme::kTextMuted, 10);
						}
					}
				}
			}
			text(context, "Time relative to visible interval end", interface_theme::kTextMuted, 10);
			text(context,
				 state.lanes.empty()
					 ? state.message
					 : "0 — " + memory_bytes(state.history_scale_bytes, state.decimal) + " · " +
						   std::to_string(double(state.view_end_ns - state.view_begin_ns) / 1e9) +
						   " s · " + state.message,
				 interface_theme::kTextMuted, 10);
			text(context, state.statistics_label, interface_theme::kTextMuted, 10);
			text(context,
				 state.tooltip.empty()
					 ? (state.following
							? "Ctrl + wheel to zoom · drag to pan · click to inspect"
							: "View held · collection continues · Ctrl + wheel zooms time")
					 : state.tooltip,
				 interface_theme::kTextSecondary, 10);
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
