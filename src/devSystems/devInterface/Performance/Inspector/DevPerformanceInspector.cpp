#include "devSystems/devInterface/Performance/Inspector/DevPerformanceInspector.hpp"
#if FLOW_UI_DEV_MODE
#include "FSEL/Button.hpp"
#include "FSEL/NumberInput.hpp"
#include "devSystems/devInterface/Performance/Inspector/DevPerformanceCapturePolicy.hpp"
#include "devSystems/devInterface/Performance/Workbench/DevTimelineControls.hpp"
#include "devSystems/devMonitoringAndReporting/DevMonitoringAndReporting.hpp"
#include "devSystems/devMonitoringAndReporting/reporting/DevPerformanceCapture.hpp"
#include <numeric>

namespace FlowUi::devSystems::interface_elements {
namespace {
using Context = DevPerformanceInspector::BuildContext;
constexpr auto toggle_disclosure = UiAction("flowui.performance.inspector.disclosure",
											[](bool& expanded) { expanded = !expanded; });

[[nodiscard]] Clay_ElementDeclaration column(uint16_t gap = 6) noexcept {
	Clay_ElementDeclaration declaration{};
	declaration.layout.sizing = {.width = CLAY_SIZING_PERCENT(1), .height = CLAY_SIZING_FIT(0)};
	declaration.layout.layoutDirection = CLAY_TOP_TO_BOTTOM;
	declaration.layout.childGap = gap;
	return declaration;
}
[[nodiscard]] Clay_ElementDeclaration row(float height = 28) noexcept {
	auto declaration = column();
	declaration.layout.layoutDirection = CLAY_LEFT_TO_RIGHT;
	declaration.layout.sizing.height = CLAY_SIZING_FIXED(height);
	declaration.layout.childAlignment.y = CLAY_ALIGN_Y_CENTER;
	return declaration;
}
template <class BuildContext>
void text(BuildContext& context, std::string_view value,
		  Clay_Color color = interface_theme::kTextCanvas, uint16_t size = 12,
		  Clay_TextAlignment alignment = CLAY_TEXT_ALIGN_LEFT) {
	Clay_TextElementConfig config{};
	config.fontSize = size;
	config.textColor = color;
	config.wrapMode = CLAY_TEXT_WRAP_WORDS;
	config.textAlignment = alignment;
	// Clay wraps at spaces. Add display-only breaks to long paths, symbols and numeric IDs.
	const auto parent = Clay_GetElementData(Clay_ElementId{.id = Clay_GetOpenElementId()});
	const float available_width = parent.found ? parent.boundingBox.width - 16 : 200;
	const size_t maximum_word_length = size_t(std::max(8.0f, available_width / (size * .8f)));
	std::string display_value;
	display_value.reserve(value.size() + value.size() / maximum_word_length);
	size_t word_length = 0;
	for (size_t byte_position = 0; byte_position < value.size(); ++byte_position) {
		const auto character = static_cast<unsigned char>(value[byte_position]);
		if (character == ' ' || character == '\n' || character == '\t')
			word_length = 0;
		else if ((character & 0xc0) != 0x80) {
			if (word_length == maximum_word_length) {
				display_value += '\n';
				word_length = 0;
			}
			++word_length;
		}
		display_value += static_cast<char>(character);
	}
	CLAY_TEXT(context.uiManager.toClayString(display_value), CLAY_TEXT_CONFIG(config));
}
[[nodiscard]] FSEL::ButtonParameters button_style(std::string_view label) {
	FSEL::ButtonParameters parameters;
	parameters.text = label;
	parameters.contentMode = FSEL::ButtonContentMode::TextOnly;
	parameters.sizing = {.width = CLAY_SIZING_GROW(0), .height = CLAY_SIZING_FIT(28)};
	parameters.padding = Clay_Padding{6, 6, 5, 5};
	parameters.labelFontSize = 11;
	parameters.labelWrapMode = CLAY_TEXT_WRAP_WORDS;
	parameters.labelAlignment = CLAY_TEXT_ALIGN_CENTER;
	parameters.cornerRadius = CLAY_CORNER_RADIUS(3);
	parameters.idleOverrides.backgroundColor = interface_theme::kDepth3Elevated;
	parameters.idleOverrides.labelColor = interface_theme::kTextCanvas;
	parameters.hoveredOverrides.backgroundColor = interface_theme::kHoverSurface;
	parameters.hoveredOverrides.labelColor = interface_theme::kTextCanvas;
	return parameters;
}
void disclosure(Context& context, LocalElementName name, std::string_view label, bool& expanded,
				bool header = false) {
	auto parameters = button_style(label);
	const auto owned_label = std::string(expanded ? "-  " : "+  ") + std::string(label);
	parameters.text = owned_label;
	if (header) {
		parameters.labelFontSize = 12;
		parameters.labelAlignment = CLAY_TEXT_ALIGN_LEFT;
		parameters.childAlignment = Clay_ChildAlignment{CLAY_ALIGN_X_LEFT, CLAY_ALIGN_Y_CENTER};
		parameters.padding = Clay_Padding{12, 12, 8, 8};
		parameters.cornerRadius = CLAY_CORNER_RADIUS(0);
		parameters.idleOverrides.backgroundColor = interface_theme::kDepth2Ink;
	}
	if (context.params.app)
		parameters.onActivate =
			ActionCall{context.params.app->actions().uiActions().make(toggle_disclosure, expanded)};
	context.uiManager.createElement(FSEL::kButton, name)
		.setParameters(parameters)
		.setDevInternalCapture(true)
		.draw();
}
template <class Name>
void navigation(Context& context, Name name, std::string_view label, size_t sample,
				TimelineAction action, bool enabled = true) {
	auto& timeline = context.params.interfaceState->performance_timeline;
	DevPerformanceInspectorActionParameters parameters;
	parameters.label = label;
	parameters.timeline = &timeline;
	parameters.command =
		TimelineCommand{{sample}, action, timeline.cards.size(), 0, timeline.snapshot_revision};
	parameters.enabled = enabled && sample < timeline.snapshot.blocks.size();
	context.uiManager.createElement(kDevPerformanceInspectorAction, name)
		.setParameters(parameters)
		.setDevInternalCapture(true)
		.draw();
}
void choice(Context& context, LocalElementName name, std::string_view label, uint64_t& value,
			std::span<const FSEL::ComboBoxOption> options) {
	auto field = column(3);
	field.layout.sizing.width = CLAY_SIZING_GROW(0);
	CLAY(context.clayID(Keyed("choice-field", name.token)), field) {
		text(context, label, interface_theme::kTextSecondary, 11);
		FSEL::ComboBoxParameters parameters;
		parameters.selectedValue = &value;
		parameters.options = options;
		parameters.fontSize = 11;
		parameters.labelAlignment = CLAY_TEXT_ALIGN_CENTER;
		parameters.sizing =
			Clay_Sizing{.width = CLAY_SIZING_GROW(0), .height = CLAY_SIZING_FIXED(28)};
		context.uiManager.createElement(FSEL::kComboBox, name)
			.setParameters(parameters)
			.setDevInternalCapture(true)
			.draw();
	}
}
void number(Context& context, LocalElementName name, std::string_view label, float& value,
			float minimum, float maximum, std::string_view unit) {
	auto field = column(3);
	field.layout.sizing.width = CLAY_SIZING_GROW(0);
	CLAY(context.clayID(Keyed("number-field", name.token)), field) {
		text(context, label, interface_theme::kTextSecondary, 11);
		CLAY(context.clayID(Keyed("number-row", name.token)), row()) {
			FSEL::NumberInputParameters<float> parameters;
			parameters.value = &value;
			parameters.minimum = minimum;
			parameters.maximum = maximum;
			parameters.step = .1f;
			parameters.fontSize = 12;
			parameters.format.notation = FSEL::NumericFloatNotation::Fixed;
			parameters.format.precision = 3;
			parameters.centerText = true;
			parameters.padding = Clay_Padding{6, 6, 2, 2};
			parameters.idleOverrides.backgroundColor = interface_theme::kDepth0Keel;
			parameters.idleOverrides.textColor = interface_theme::kTextCanvas;
			parameters.sizing =
				Clay_Sizing{.width = CLAY_SIZING_GROW(0), .height = CLAY_SIZING_FIXED(28)};
			context.uiManager.createElement(FSEL::kNumberInputFloat, name)
				.setParameters(parameters)
				.setDevInternalCapture(true)
				.draw();
			text(context, unit, interface_theme::kTextSecondary, 11);
		}
	}
}
void key_value(Context& context, uint64_t key, std::string_view label, std::string_view value,
			   bool narrow, Clay_Color color = interface_theme::kTextCanvas) {
	auto declaration = column(2);
	if (!narrow) {
		declaration.layout.layoutDirection = CLAY_LEFT_TO_RIGHT;
		declaration.layout.childGap = 8;
	}
	CLAY(context.clayID(Keyed("value-row", key)), declaration) {
		auto cell = column(0);
		if (!narrow)
			cell.layout.sizing.width = CLAY_SIZING_PERCENT(.4f);
		CLAY(context.clayID(Keyed("value-label", key)), cell) {
			text(context, label, interface_theme::kTextSecondary, 11);
		}
		CLAY(context.clayID(Keyed("value-content", key)), (narrow ? column(0) : [&] {
				 auto value_cell = column(0);
				 value_cell.layout.sizing.width = CLAY_SIZING_GROW(0);
				 return value_cell;
			 }())) {
			text(context, value, color, 12);
		}
	}
}
void subtitle(Context& context, uint64_t key, std::string_view title) {
	auto declaration = column();
	declaration.layout.padding = {0, 0, 8, 3};
	declaration.border = {.color = interface_theme::kBorderPrimary, .width = {.top = 1}};
	CLAY(context.clayID(Keyed("subtitle", key)), declaration) {
		text(context, title, interface_theme::kAccentSeaGlass, 12);
	}
}
void metric(Context& context, uint64_t key, std::string_view label, std::string_view value,
			std::string_view secondary, Clay_Color color = interface_theme::kTextCanvas) {
	auto tile = column(4);
	tile.layout.sizing.width = CLAY_SIZING_GROW(0);
	tile.backgroundColor = interface_theme::kDepth0Keel;
	tile.layout.padding = {8, 8, 8, 8};
	tile.cornerRadius = CLAY_CORNER_RADIUS(3);
	tile.layout.childAlignment.x = CLAY_ALIGN_X_CENTER;
	CLAY(context.clayID(Keyed("metric", key)), tile) {
		text(context, label, interface_theme::kTextSecondary, 11, CLAY_TEXT_ALIGN_CENTER);
		text(context, value, color, 15, CLAY_TEXT_ALIGN_CENTER);
		if (!secondary.empty())
			text(context, secondary, interface_theme::kTextSecondary, 10, CLAY_TEXT_ALIGN_CENTER);
	}
}
[[nodiscard]] std::string decimal(double value, std::string_view suffix = {}, int precision = 1) {
	char buffer[80]{};
	std::snprintf(buffer, sizeof(buffer), "%.*f", precision, value);
	return std::string(buffer) + std::string(suffix);
}
[[nodiscard]] std::string_view role_label(TimingZoneRole role) noexcept {
	switch (role) {
	case TimingZoneRole::Work:
		return "Work";
	case TimingZoneRole::Wait:
		return "Wait";
	case TimingZoneRole::Gap:
		return "Gap";
	case TimingZoneRole::GpuWork:
		return "GPU work";
	case TimingZoneRole::DevToolWork:
		return "Developer tool";
	}
	return "Unknown role";
}
[[nodiscard]] std::string quality_label(const TimelineBlockSlice& block) {
	if (block.synthetic_tick)
		return "Derived application tick grouping";
	const auto flags = block.details().quality_flags;
	std::string result = (flags & 1) ? "Completed" : "Completion unknown";
	if (block.domain == TimingSampleDomain::Cpu) {
		for (const auto& [flag, label] : std::array<std::pair<TimingRecordFlag, const char*>, 6>{
				 {{TimingRecordFlag::Canceled, " · canceled"},
				  {TimingRecordFlag::Incomplete, " · incomplete"},
				  {TimingRecordFlag::ClockAnomaly, " · clock anomaly"},
				  {TimingRecordFlag::DetailTruncated, " · detail truncated"},
				  {TimingRecordFlag::OutOfDate, " · out of date"},
				  {TimingRecordFlag::Exception, " · exception"}}})
			if (flags & timingRecordFlags(flag))
				result += label;
	} else {
		if (!block.cpu_clock_aligned)
			result += " · GPU local clock";
		if (flags & gpuTimingRecordFlags(GpuTimingRecordFlag::DetailTruncated))
			result += " · detail truncated";
	}
	return result;
}
[[nodiscard]] size_t find_sample(const DevTimelineState& timeline,
								 const PerformanceAnalysisSample& sample) noexcept {
	for (size_t block_position = 0; block_position < timeline.snapshot.blocks.size();
		 ++block_position)
		if (sample.matches(timeline.snapshot.blocks[block_position]))
			return block_position;
	return timeline_no_parent;
}
void build_capture(Context& context, bool narrow, const TimingReportingStatus& reporting_status) {
	auto& session = *context.params.interfaceState;
	auto& state = context.state();
	static constexpr std::array start_options{FSEL::ComboBoxOption{0, "Immediately"},
											  FSEL::ComboBoxOption{1, "Shortcut"}};
	static constexpr std::array end_options{FSEL::ComboBoxOption{0, "Manual reopen"},
											FSEL::ComboBoxOption{1, "Budget event"},
											FSEL::ComboBoxOption{2, "Duration"}};
	auto pair = column();
	if (!narrow)
		pair.layout.layoutDirection = CLAY_LEFT_TO_RIGHT;
	CLAY(context.clayID("capture-mode-fields"), pair) {
		choice(context, LocalElementName{"capture-start"}, "Start", session.capture_start_mode,
			   start_options);
		choice(context, LocalElementName{"capture-end"}, "Stop", session.capture_end_mode,
			   end_options);
	}
	if (session.capture_start_mode == 1) {
		static constexpr std::array key_options{
			FSEL::ComboBoxOption{65, "A"},	  FSEL::ComboBoxOption{66, "B"},
			FSEL::ComboBoxOption{67, "C"},	  FSEL::ComboBoxOption{68, "D"},
			FSEL::ComboBoxOption{69, "E"},	  FSEL::ComboBoxOption{70, "F"},
			FSEL::ComboBoxOption{71, "G"},	  FSEL::ComboBoxOption{72, "H"},
			FSEL::ComboBoxOption{73, "I"},	  FSEL::ComboBoxOption{74, "J"},
			FSEL::ComboBoxOption{75, "K"},	  FSEL::ComboBoxOption{76, "L"},
			FSEL::ComboBoxOption{77, "M"},	  FSEL::ComboBoxOption{78, "N"},
			FSEL::ComboBoxOption{79, "O"},	  FSEL::ComboBoxOption{80, "P"},
			FSEL::ComboBoxOption{81, "Q"},	  FSEL::ComboBoxOption{82, "R"},
			FSEL::ComboBoxOption{83, "S"},	  FSEL::ComboBoxOption{84, "T"},
			FSEL::ComboBoxOption{85, "U"},	  FSEL::ComboBoxOption{86, "V"},
			FSEL::ComboBoxOption{87, "W"},	  FSEL::ComboBoxOption{88, "X"},
			FSEL::ComboBoxOption{89, "Y"},	  FSEL::ComboBoxOption{90, "Z"},
			FSEL::ComboBoxOption{48, "0"},	  FSEL::ComboBoxOption{49, "1"},
			FSEL::ComboBoxOption{50, "2"},	  FSEL::ComboBoxOption{51, "3"},
			FSEL::ComboBoxOption{52, "4"},	  FSEL::ComboBoxOption{53, "5"},
			FSEL::ComboBoxOption{54, "6"},	  FSEL::ComboBoxOption{55, "7"},
			FSEL::ComboBoxOption{56, "8"},	  FSEL::ComboBoxOption{57, "9"},
			FSEL::ComboBoxOption{290, "F1"},  FSEL::ComboBoxOption{291, "F2"},
			FSEL::ComboBoxOption{292, "F3"},  FSEL::ComboBoxOption{293, "F4"},
			FSEL::ComboBoxOption{294, "F5"},  FSEL::ComboBoxOption{295, "F6"},
			FSEL::ComboBoxOption{296, "F7"},  FSEL::ComboBoxOption{297, "F8"},
			FSEL::ComboBoxOption{298, "F9"},  FSEL::ComboBoxOption{299, "F10"},
			FSEL::ComboBoxOption{300, "F11"}, FSEL::ComboBoxOption{301, "F12"},
			FSEL::ComboBoxOption{32, "Space"}};
		static constexpr std::array modifier_options{
			FSEL::ComboBoxOption{0, "No modifiers"},
			FSEL::ComboBoxOption{1, "Ctrl"},
			FSEL::ComboBoxOption{2, "Shift"},
			FSEL::ComboBoxOption{3, "Ctrl + Shift"},
			FSEL::ComboBoxOption{4, "Alt"},
			FSEL::ComboBoxOption{5, "Ctrl + Alt"},
			FSEL::ComboBoxOption{6, "Shift + Alt"},
			FSEL::ComboBoxOption{7, "Ctrl + Shift + Alt"},
			FSEL::ComboBoxOption{8, "Super"},
			FSEL::ComboBoxOption{9, "Ctrl + Super"},
			FSEL::ComboBoxOption{10, "Shift + Super"},
			FSEL::ComboBoxOption{11, "Ctrl + Shift + Super"},
			FSEL::ComboBoxOption{12, "Alt + Super"},
			FSEL::ComboBoxOption{13, "Ctrl + Alt + Super"},
			FSEL::ComboBoxOption{14, "Shift + Alt + Super"},
			FSEL::ComboBoxOption{15, "Ctrl + Shift + Alt + Super"}};
		CLAY(context.clayID("shortcut-fields"), pair) {
			choice(context, LocalElementName{"capture-modifiers"}, "Modifiers",
				   session.capture_modifiers, modifier_options);
			choice(context, LocalElementName{"capture-key"}, "Key", session.capture_key,
				   key_options);
		}
	}
	if (session.capture_end_mode == 2)
		number(context, LocalElementName{"capture-duration"}, "Duration",
			   session.capture_duration_seconds, .001f, 86400, "s");
	if (session.capture_end_mode == 1) {
		choice(context, LocalElementName{"capture-budget-window"}, "Budget source",
			   session.capture_budget_window, state.window_options);
		CLAY(context.clayID("capture-budget-fields"), pair) {
			number(context, LocalElementName{"capture-budget"}, "Threshold",
				   session.capture_budget_ms, .001f, 60000, "ms");
			number(context, LocalElementName{"capture-tail"}, "After event",
				   session.capture_tail_seconds, 0, 86400, "s");
		}
	}
	text(context,
		 session.capture_start_mode == 1
			 ? "Main Capture closes the interface and arms the shortcut."
			 : "Main Capture closes the interface and starts recording.",
		 interface_theme::kTextSecondary, 11);
	if (session.capture_end_mode == 0)
		text(context, "Reopen the interface to stop.", interface_theme::kTextSecondary, 11);
	const auto validation_error = performance_capture_policy_error(session, context.params.app);
	if (!validation_error.empty())
		text(context, validation_error, interface_theme::kStatusRed, 11);
	else if (!session.capture_error.empty())
		text(context, session.capture_error, interface_theme::kStatusRed, 11);
	if (session.capture_end_mode == 1 &&
		std::ranges::none_of(state.window_options, [&](const auto& option) {
			return option.value == session.capture_budget_window;
		}))
		text(context, "Budget window unavailable. Choose a new source.",
			 interface_theme::kStatusRed, 11);
	disclosure(context, LocalElementName{"capture-information"}, "Capture information",
			   state.capture_information);
	if (state.capture_information) {
		key_value(context, 1, "Capacity",
				  std::to_string(reporting_status.effectiveCapacity) + " ticks", narrow);
		key_value(context, 2, "Retained / overwritten",
				  std::to_string(reporting_status.retainedTickCount) + " / " +
					  std::to_string(reporting_status.capture_overwritten_ticks),
				  narrow);
		text(context,
			 "Wrapping retains the newest ticks. Settings above apply to the next capture.",
			 interface_theme::kTextSecondary, 11);
		if (context.params.app) {
			const auto& capture =
				context.params.app->devMonitoring().performance_capture().status();
			static constexpr std::array reasons{"Manual reopen", "Duration elapsed",
												"Budget event",	 "Shutdown",
												"Failure",		 "Ring capacity reached"};
			key_value(context, 3, "Last stop",
					  capture.generation ? reasons[size_t(capture.stop_reason)] : "No capture",
					  narrow);
			key_value(context, 4, "Recorded duration",
					  performance_duration(capture.settings.duration_ns), narrow);
			key_value(context, 5, "Recorded threshold",
					  performance_duration(capture.settings.budget_ns), narrow);
			key_value(context, 6, "Pending measurements",
					  std::to_string(capture.pending_measurements), narrow);
			if (capture.incomplete)
				text(context, "Incomplete capture: pending measurements did not finish.",
					 interface_theme::kStatusAmber, 11);
		}
	}
}

void build_rolling(Context& context, bool narrow) {
	auto& state = context.state();
	const auto& analysis = state.analysis;
	static constexpr std::array ranges{FSEL::ComboBoxOption{120, "Last 120 ticks"},
									   FSEL::ComboBoxOption{600, "Last 600 ticks"},
									   FSEL::ComboBoxOption{0, "All retained"}};
	auto pair = column();
	if (!narrow)
		pair.layout.layoutDirection = CLAY_LEFT_TO_RIGHT;
	CLAY(context.clayID("analysis-context"), pair) {
		choice(context, LocalElementName{"analysis-range"}, "Range", state.analysis_range, ranges);
		choice(context, LocalElementName{"analysis-source"}, "Timing source", state.analysis_source,
			   state.window_options);
	}
	number(context, LocalElementName{"analysis-target"}, "Analysis target",
		   state.analysis_target_ms, .001f, 60000, "ms");
	text(context, "Analysis only · display refreshes every 240 frames",
		 interface_theme::kTextSecondary, 10);
	if (!analysis.generation) {
		text(context, "Record a capture to see application timing statistics.",
			 interface_theme::kTextSecondary);
		return;
	}
	text(context,
		 "Retained capture #" + std::to_string(analysis.generation) + " · " +
			 std::to_string(analysis.complete_ticks) + " complete ticks · " +
			 decimal(double(analysis.coverage_ns) / 1e9, " s"),
		 interface_theme::kTextSecondary, 11);
	const auto& sorted = analysis.sorted_cadence;
	if (sorted.empty())
		text(context, "No complete timing intervals for this source.",
			 interface_theme::kStatusAmber, 11);
	else {
		const double target_ns =
			std::isfinite(state.analysis_target_ms) && state.analysis_target_ms > 0
				? double(state.analysis_target_ms) * 1e6
				: 16.6e6;
		const auto median = performance_percentile(sorted, .5);
		const auto p95 = performance_percentile(sorted, .95);
		const double mean = std::accumulate(sorted.begin(), sorted.end(), 0.0) / sorted.size();
		const auto above = sorted.end() - std::upper_bound(sorted.begin(), sorted.end(), target_ns);
		const auto color = median > target_ns		  ? interface_theme::kStatusRed
						   : median >= target_ns * .8 ? interface_theme::kStatusAmber
													  : interface_theme::kAccentSeaGlass;
		CLAY(context.clayID("rolling-metrics-first"), pair) {
			metric(context, 1, "Typical", performance_duration(median),
				   "Mean " + performance_duration(mean));
			metric(context, 2, "Tail · P95", performance_duration(p95),
				   "P99 " + performance_duration(performance_percentile(sorted, .99)));
		}
		CLAY(context.clayID("rolling-metrics-second"), pair) {
			metric(context, 3, "Worst", performance_duration(sorted.back()),
				   std::to_string(sorted.size()) + " intervals");
			metric(context, 4, "Above target", decimal(100.0 * above / sorted.size(), "%"),
				   performance_duration(std::abs(target_ns - median)) +
					   (median <= target_ns ? " headroom" : " overrun"),
				   color);
		}
		text(context, state.analysis_source ? "CPU frame duration" : "Application tick cadence",
			 interface_theme::kTextSecondary, 11);
		// Fixed-count downsampling keeps trend geometry cheap even for long captures.
		auto trend = row(44);
		trend.layout.childGap = 1;
		trend.layout.childAlignment.y = CLAY_ALIGN_Y_BOTTOM;
		trend.backgroundColor = interface_theme::kDepth0Keel;
		trend.layout.padding = {3, 3, 3, 3};
		const double maximum = std::max(double(sorted.back()), target_ns);
		CLAY(context.clayID("cadence-trend"), trend) {
			const size_t bucket_count = std::min(size_t{32}, analysis.cadence.size());
			for (size_t bucket = 0; bucket < bucket_count; ++bucket) {
				const size_t first = bucket * analysis.cadence.size() / bucket_count;
				const size_t end = (bucket + 1) * analysis.cadence.size() / bucket_count;
				const auto duration = *std::max_element(analysis.cadence.begin() + first,
														analysis.cadence.begin() + end);
				auto bar = column(0);
				bar.layout.sizing.width = CLAY_SIZING_GROW(0);
				bar.layout.sizing.height =
					CLAY_SIZING_FIXED(float(std::max(1.0, 38 * duration / maximum)));
				bar.backgroundColor = duration > target_ns ? interface_theme::kStatusRed
														   : interface_theme::kAccentCurrent;
				CLAY(context.clayID(Keyed("trend-bar", bucket)), bar) {}
			}
			auto target_line = row(1);
			target_line.backgroundColor = interface_theme::kAccentSignalBlue;
			target_line.layout.sizing.width = CLAY_SIZING_PERCENT(1);
			target_line.floating.attachTo = CLAY_ATTACH_TO_PARENT;
			target_line.floating.attachPoints = {CLAY_ATTACH_POINT_LEFT_BOTTOM,
												 CLAY_ATTACH_POINT_LEFT_BOTTOM};
			target_line.floating.offset.y = -float(3 + 38 * target_ns / maximum);
			target_line.floating.pointerCaptureMode = CLAY_POINTER_CAPTURE_MODE_PASSTHROUGH;
			CLAY(context.clayID("target-line"), target_line) {}
		}
		text(context, "Oldest to newest · bucket maximum · blue line: target",
			 interface_theme::kTextSecondary, 10);
		disclosure(context, LocalElementName{"distribution"}, "Timing distribution",
				   state.distribution_expanded);
		if (state.distribution_expanded) {
			key_value(context, 10, "Minimum", performance_duration(sorted.front()), narrow);
			key_value(context, 11, "Median", performance_duration(median), narrow);
			key_value(context, 12, "Mean", performance_duration(mean), narrow);
			key_value(context, 13, "P95", performance_duration(p95), narrow);
			key_value(context, 14, "P99", performance_duration(performance_percentile(sorted, .99)),
					  narrow);
			key_value(context, 15, "Maximum", performance_duration(sorted.back()), narrow);
			if (sorted.size() < 100)
				text(context,
					 "Limited population: tail percentiles may represent a single interval.",
					 interface_theme::kTextSecondary, 11);
		}
	}
	subtitle(context, 1, "Optimization opportunities");
	static constexpr std::array domains{FSEL::ComboBoxOption{0, "CPU"},
										FSEL::ComboBoxOption{1, "GPU"}};
	static constexpr std::array rankings{
		FSEL::ComboBoxOption{0, "Total recorded self"}, FSEL::ComboBoxOption{1, "Total inclusive"},
		FSEL::ComboBoxOption{2, "Mean per call"}, FSEL::ComboBoxOption{3, "P95 per call"},
		FSEL::ComboBoxOption{4, "Call count"}};
	static constexpr std::array gpu_rankings{
		FSEL::ComboBoxOption{1, "Total zone duration"}, FSEL::ComboBoxOption{2, "Mean per call"},
		FSEL::ComboBoxOption{3, "P95 per call"}, FSEL::ComboBoxOption{4, "Call count"}};
	static constexpr std::array roles{FSEL::ComboBoxOption{0, "Work + waits"},
									  FSEL::ComboBoxOption{1, "Work"},
									  FSEL::ComboBoxOption{2, "Waits / gaps"}};
	if (state.domain && !state.ranking)
		state.ranking = 1;
	CLAY(context.clayID("ranking-controls"), pair) {
		choice(context, LocalElementName{"ranking-domain"}, "Domain", state.domain, domains);
		choice(context, LocalElementName{"ranking-metric"}, "Rank by", state.ranking,
			   state.domain ? std::span<const FSEL::ComboBoxOption>{gpu_rankings}
							: std::span<const FSEL::ComboBoxOption>{rankings});
	}
	choice(context, LocalElementName{"ranking-role"}, "Roles", state.role_filter, roles);
	const auto score = [&](const PerformanceAnalysisZone& zone) -> double {
		switch (state.ranking) {
		case 0:
			return double(zone.self_ns);
		case 1:
			return double(zone.total_ns);
		case 2:
			return double(zone.total_ns) / zone.durations.size();
		case 3:
			return double(performance_percentile(zone.durations, .95));
		default:
			return double(zone.durations.size());
		}
	};
	auto& order = state.ranking_order;
	if (state.cached_ranking != state.ranking || state.cached_domain != state.domain ||
		state.cached_role != state.role_filter) {
		order.clear();
		order.reserve(analysis.zones.size());
		for (size_t zone_position = 0; zone_position < analysis.zones.size(); ++zone_position) {
			const auto& zone = analysis.zones[zone_position];
			const bool wait = zone.role == TimingZoneRole::Wait || zone.role == TimingZoneRole::Gap;
			if (size_t(zone.largest.domain) != state.domain || (state.role_filter == 1 && wait) ||
				(state.role_filter == 2 && !wait))
				continue;
			if (state.ranking == 0 && zone.self_count == 0)
				continue;
			order.emplace_back(zone_position);
		}
		std::ranges::stable_sort(order, [&](size_t left, size_t right) {
			return score(analysis.zones[left]) > score(analysis.zones[right]);
		});
		state.cached_ranking = state.ranking;
		state.cached_domain = state.domain;
		state.cached_role = state.role_filter;
	}
	const double total_score =
		std::accumulate(order.begin(), order.end(), 0.0, [&](double total, size_t index) {
			return total + score(analysis.zones[index]);
		});
	const auto& timeline = context.params.interfaceState->performance_timeline;
	for (size_t rank = 0; rank < std::min(order.size(), state.all_zones ? order.size() : size_t{5});
		 ++rank) {
		const auto& zone = analysis.zones[order[rank]];
		auto zone_row = column(4);
		zone_row.layout.padding = {8, 8, 8, 8};
		zone_row.backgroundColor = interface_theme::kDepth0Keel;
		zone_row.cornerRadius = CLAY_CORNER_RADIUS(3);
		CLAY(context.clayID(Keyed("ranked-zone", order[rank])), zone_row) {
			text(context, zone.label);
			text(context,
				 std::string(role_label(zone.role)) + " · Window " +
					 std::to_string(zone.largest.frame.window) +
					 (state.domain ? " · queue " : " · thread ") +
					 std::to_string(zone.largest.track),
				 interface_theme::kTextSecondary, 10);
			text(context,
				 state.ranking == 4 ? std::to_string(zone.durations.size()) + " calls"
									: performance_duration(score(zone)),
				 interface_theme::kAccentSeaGlass, 14);
			text(context,
				 performance_duration(double(zone.total_ns) / zone.durations.size()) + "/call · " +
					 (analysis.complete_ticks
						  ? decimal(double(zone.durations.size()) / analysis.complete_ticks,
									" calls/tick")
						  : "Calls/tick unavailable"),
				 interface_theme::kTextSecondary, 11);
			text(context,
				 total_score > 0 ? decimal(score(zone) * 100 / total_score, "% of this ranking")
								 : "Zero measured cost",
				 interface_theme::kTextSecondary, 10);
			auto rail = row(3);
			rail.backgroundColor = interface_theme::kBorderPrimary;
			CLAY(context.clayID(Keyed("ranking-rail", rank)), rail) {
				auto fill = rail;
				fill.layout.sizing.width = CLAY_SIZING_PERCENT(
					float(score(analysis.zones[order.front()]) > 0
							  ? score(zone) / score(analysis.zones[order.front()])
							  : 0));
				fill.backgroundColor = interface_theme::kAccentCurrent;
				CLAY(context.clayID(Keyed("ranking-fill", rank)), fill) {}
			}
			DevPerformanceInspectorActionParameters expand;
			expand.label =
				state.expanded_zone == order[rank] ? "Hide call details" : "Call details";
			expand.expanded_row = &state.expanded_zone;
			expand.row_identity = order[rank];
			context.uiManager
				.createElement(kDevPerformanceInspectorAction, Keyed("zone-details", order[rank]))
				.setParameters(expand)
				.setDevInternalCapture(true)
				.draw();
			if (state.expanded_zone == order[rank]) {
				key_value(context, 100 + order[rank] * 4, "Inclusive total",
						  performance_duration(zone.total_ns), narrow);
				key_value(context, 101 + order[rank] * 4, "P95 per call",
						  performance_duration(performance_percentile(zone.durations, .95)),
						  narrow);
				key_value(context, 102 + order[rank] * 4, "Call count",
						  std::to_string(zone.durations.size()), narrow);
				if (!state.domain)
					key_value(context, 103 + order[rank] * 4, "Self-eligible calls",
							  std::to_string(zone.self_count), narrow);
				const size_t sample = find_sample(timeline, zone.largest);
				navigation(context, Keyed("rank-inspect", order[rank]), "Inspect largest call",
						   sample, TimelineAction::Inspect);
				if (sample == timeline_no_parent)
					text(context, "Call hidden by timeline scope filters.",
						 interface_theme::kTextSecondary, 10);
			}
		}
	}
	if (order.empty())
		text(context, "No eligible zones in this population.", interface_theme::kTextSecondary, 11);
	if (order.size() > 5)
		disclosure(context, LocalElementName{"all-zones"},
				   "All zones (" + std::to_string(order.size()) + ")", state.all_zones);
	text(context,
		 state.domain
			 ? "GPU zone totals overlap across nesting and queues; they are not utilization."
		 : state.ranking == 0
			 ? "Recorded self includes waits and uninstrumented work. Threads may overlap."
			 : "Inclusive totals count nested work more than once.",
		 interface_theme::kTextSecondary, 10);
	if (analysis.excluded_intervals || analysis.excluded_samples || analysis.dropped_records)
		text(context,
			 "Timing quality · " + std::to_string(analysis.excluded_intervals) +
				 " incomplete intervals · " + std::to_string(analysis.excluded_samples) +
				 " excluded calls · " + std::to_string(analysis.dropped_records) +
				 " dropped records",
			 interface_theme::kStatusAmber, 11);
}

void build_selected(Context& context, bool narrow) {
	auto& inspector = context.state();
	auto& timeline = context.params.interfaceState->performance_timeline;
	if (timeline.inspected_sample >= timeline.snapshot.blocks.size() ||
		(inspector.analysis.generation &&
		 inspector.analysis.generation != timeline.capture_generation)) {
		text(context, "Select a zone in a timeline to inspect its cost and context.",
			 interface_theme::kTextSecondary);
		return;
	}
	const auto& block = timeline.snapshot.blocks[timeline.inspected_sample];
	const auto details = block.details();
	const bool eligible = performance_sample_valid(block);
	const bool self_valid =
		eligible && block.domain == TimingSampleDomain::Cpu && block.hierarchy_note.empty() &&
		!(details.quality_flags & timingRecordFlags(TimingRecordFlag::DetailTruncated)) &&
		!block.cpu_sample.empty() && block.cpu_sample.front().directChildNs <= block.duration_ns;
	text(context, block.label, interface_theme::kTextCanvas, 14);
	text(context,
		 std::string(block.domain == TimingSampleDomain::Cpu ? "CPU" : "GPU") + " · " +
			 std::string(performance_category_names[size_t(block.category)]) + " · " +
			 std::string(role_label(block.role)),
		 interface_theme::kAccentSeaGlass, 11);
	const auto window = std::ranges::find(context.params.interfaceState->capture_windows,
										  block.frame.window, &DevWindowInfo::id);
	text(context,
		 (window != context.params.interfaceState->capture_windows.end()
			  ? window->title
			  : "Window " + std::to_string(block.frame.window)) +
			 (block.domain == TimingSampleDomain::Cpu ? " · Thread " : " · Queue family ") +
			 std::to_string(block.track),
		 interface_theme::kTextSecondary, 11);
	text(context, quality_label(block),
		 eligible && block.hierarchy_note.empty() ? interface_theme::kTextSecondary
												  : interface_theme::kStatusAmber,
		 11);
	if (!block.hierarchy_note.empty())
		text(context, block.hierarchy_note, interface_theme::kStatusAmber, 11);
	auto pair = column();
	if (!narrow)
		pair.layout.layoutDirection = CLAY_LEFT_TO_RIGHT;
	CLAY(context.clayID("selected-costs"), pair) {
		metric(context, 10, block.synthetic_tick ? "Tick interval" : "Elapsed",
			   performance_duration(block.duration_ns), "Inclusive duration");
		if (block.domain == TimingSampleDomain::Cpu && !block.synthetic_tick)
			metric(context, 11, "Recorded self",
				   self_valid ? performance_duration(details.exclusive_ns) : "n/a",
				   "Includes waits / uninstrumented work");
	}
	if (self_valid && block.duration_ns)
		key_value(context, 30, "Self share",
				  decimal(100.0 * details.exclusive_ns / block.duration_ns, "%"), narrow);
	const bool parent_valid = block.parent < timeline.snapshot.blocks.size();
	if (parent_valid) {
		const auto& parent = timeline.snapshot.blocks[block.parent];
		if (eligible && performance_sample_valid(parent) && parent.duration_ns &&
			block.hierarchy_note.empty() && parent.domain == block.domain &&
			block.duration_ns <= parent.duration_ns)
			key_value(context, 31, "Parent share",
					  decimal(100.0 * block.duration_ns / parent.duration_ns, "%"), narrow);
	}
	CLAY(context.clayID("selected-navigation"), pair) {
		navigation(context, LocalElementName{"focus"}, "Focus in minor", timeline.inspected_sample,
				   TimelineAction::Open);
		navigation(context, LocalElementName{"reveal"}, "Reveal in major",
				   timeline.inspected_sample, TimelineAction::FitSelected, block.cpu_clock_aligned);
	}
	if (!block.cpu_clock_aligned)
		text(context, "GPU local clock: cannot reveal on the CPU-clock major timeline.",
			 interface_theme::kTextSecondary, 11);
	if (!block.synthetic_tick) {
		subtitle(context, 2, "Compared with other calls");
		const auto& analysis = inspector.analysis;
		const auto sample = std::ranges::find_if(
			analysis.samples, [&](const auto& candidate) { return candidate.matches(block); });
		if (sample == analysis.samples.end())
			text(context,
				 "Selected call is outside the analysis range or is not duration-eligible.",
				 interface_theme::kTextSecondary, 11);
		else {
			const auto population = std::ranges::find_if(analysis.zones, [&](const auto& zone) {
				return zone.largest.population() == sample->population();
			});
			if (population != analysis.zones.end()) {
				const auto median = performance_percentile(population->durations, .5);
				const auto count = population->durations.size();
				text(context,
					 "Same zone, window and " +
						 std::string(block.domain == TimingSampleDomain::Cpu ? "thread"
																			 : "device / queue") +
						 " · " + std::to_string(count) + " calls",
					 interface_theme::kTextSecondary, 11);
				key_value(
					context, 32, "Median / P95",
					performance_duration(median) + " / " +
						performance_duration(performance_percentile(population->durations, .95)),
					narrow);
				key_value(context, 33, "Selected / median",
						  median ? decimal(double(block.duration_ns) / median, "x", 2)
								 : "n/a (zero median)",
						  narrow);
				const auto rank = std::upper_bound(population->durations.begin(),
												   population->durations.end(), block.duration_ns) -
								  population->durations.begin();
				key_value(context, 34, "Percentile rank",
						  decimal(100.0 * rank / count, "% at or below selected"), narrow);
				key_value(context, 35, "Largest call",
						  performance_duration(population->largest.duration_ns), narrow);
				if (!population->largest.matches(block))
					navigation(context, LocalElementName{"largest-call"}, "Inspect largest call",
							   find_sample(timeline, population->largest), TimelineAction::Inspect);
				if (count < 100)
					text(context, "Limited population: tail values may represent one call.",
						 interface_theme::kTextSecondary, 10);
			}
		}
	}
	subtitle(context, 3, "Where this call spent time");
	if (parent_valid) {
		text(context, "Parent · " + std::string(timeline.snapshot.blocks[block.parent].label),
			 interface_theme::kTextSecondary, 11);
		navigation(context, LocalElementName{"parent"}, "Inspect parent", block.parent,
				   TimelineAction::Inspect);
	}
	std::vector<size_t> children;
	if (timeline.inspected_sample + 1 < timeline.snapshot.child_offsets.size()) {
		const auto first = timeline.snapshot.child_offsets[timeline.inspected_sample];
		const auto end = timeline.snapshot.child_offsets[timeline.inspected_sample + 1];
		children.reserve(end - first);
		for (size_t position = first; position < end; ++position)
			if (timeline.snapshot.children[position] < timeline.snapshot.blocks.size())
				children.emplace_back(timeline.snapshot.children[position]);
	}
	std::ranges::stable_sort(children, [&](size_t left, size_t right) {
		return timeline.snapshot.blocks[left].duration_ns >
			   timeline.snapshot.blocks[right].duration_ns;
	});
	if (self_valid)
		key_value(context, 36, "Synchronous child time",
				  performance_duration(block.duration_ns - details.exclusive_ns), narrow);
	text(context,
		 std::to_string(children.size()) + (block.domain == TimingSampleDomain::Cpu
												? " recorded direct children"
												: " instrumented GPU children (may overlap)"),
		 interface_theme::kTextSecondary, 11);
	if (block.domain == TimingSampleDomain::Cpu && !self_valid && !block.synthetic_tick)
		text(context, "Additive self/child accounting is unavailable for this sample.",
			 interface_theme::kStatusAmber, 11);
	for (size_t child_position = 0;
		 child_position <
		 std::min(children.size(), inspector.all_children ? children.size() : size_t{5});
		 ++child_position) {
		const size_t child_index = children[child_position];
		const auto& child = timeline.snapshot.blocks[child_index];
		CLAY(context.clayID(Keyed("child-detail", child_index)), column(3)) {
			text(context, child.label);
			text(context,
				 performance_duration(child.duration_ns) +
					 (block.duration_ns
						  ? " · " + decimal(100.0 * child.duration_ns / block.duration_ns,
											"% of elapsed")
						  : ""),
				 interface_theme::kTextSecondary, 11);
			navigation(context, Keyed("child", child_index), "Inspect child", child_index,
					   TimelineAction::Inspect);
		}
	}
	if (children.size() > 5)
		disclosure(context, LocalElementName{"all-children"},
				   "All children (" + std::to_string(children.size()) + ")",
				   inspector.all_children);
	if (!details.source_file.empty() || !details.source_function.empty()) {
		subtitle(context, 4, "Source");
		text(context,
			 details.source_function.empty() ? "Function unavailable" : details.source_function);
		const auto filename_position = details.source_file.find_last_of("/\\");
		text(context,
			 std::string(details.source_file.substr(
				 filename_position == std::string_view::npos ? 0 : filename_position + 1)) +
				 ":" + std::to_string(details.source_line),
			 interface_theme::kTextSecondary, 11);
		if (context.params.app) {
			DevPerformanceInspectorActionParameters parameters;
			parameters.label = "Copy source location";
			parameters.app = context.params.app;
			parameters.clipboard_text = std::string(details.source_file) + ":" +
										std::to_string(details.source_line) + "\n" +
										std::string(details.source_function);
			context.uiManager
				.createElement(kDevPerformanceInspectorAction, LocalElementName{"copy-source"})
				.setParameters(parameters)
				.setDevInternalCapture(true)
				.draw();
		}
	}
	subtitle(context, 5, "Timing context");
	key_value(context, 40, "App tick", std::to_string(block.app_tick), narrow);
	key_value(context, 41, "Window frame", std::to_string(block.frame.frameNumber), narrow);
	// Use the selected sample's containing parent, not an unrelated last minor card.
	if (parent_valid) {
		const auto& parent = timeline.snapshot.blocks[block.parent];
		const bool comparable =
			(block.cpu_clock_aligned && parent.cpu_clock_aligned) ||
			(!block.cpu_clock_aligned && !parent.cpu_clock_aligned &&
			 block.device_identity == parent.device_identity &&
			 block.queue_identity == parent.queue_identity &&
			 block.submission_serial == parent.submission_serial && block.frame == parent.frame);
		if (comparable) {
			const auto relative = std::string(block.start_ns >= parent.start_ns ? "+" : "-") +
								  performance_duration(block.start_ns >= parent.start_ns
														   ? block.start_ns - parent.start_ns
														   : parent.start_ns - block.start_ns);
			key_value(context, 42, "Offset from parent", relative, narrow);
			const auto start = std::max(block.start_ns, parent.start_ns);
			const auto end = std::min(timeline_end(block.start_ns, block.duration_ns),
									  timeline_end(parent.start_ns, parent.duration_ns));
			key_value(context, 43, "Within parent",
					  performance_duration(end > start ? end - start : 0), narrow);
		}
	}
	disclosure(context, LocalElementName{"technical-details"}, "Technical details",
			   inspector.technical_details);
	if (inspector.technical_details) {
		key_value(context, 50, "Descriptor ID", std::to_string(block.type_id), narrow);
		key_value(context, 51, "Invocation ID", std::to_string(block.invocation_id), narrow);
		key_value(context, 52, "Recorded parent ID", std::to_string(details.parent_invocation_id),
				  narrow);
		key_value(context, 53, "Entity IDs",
				  std::to_string(details.entity.primaryId) + " / " +
					  std::to_string(details.entity.secondaryId),
				  narrow);
		key_value(context, 54, "Window / frame IDs",
				  std::to_string(block.frame.window) + " / " +
					  std::to_string(block.frame.frameNumber),
				  narrow);
		key_value(context, 55, "Raw quality flags", std::to_string(details.quality_flags), narrow);
		key_value(context, 56, "Exact duration", std::to_string(block.duration_ns) + " ns", narrow);
		if (block.cpu_clock_aligned) {
			key_value(context, 57, "Absolute start", std::to_string(block.start_ns) + " ns",
					  narrow);
			key_value(context, 58, "Absolute end",
					  std::to_string(timeline_end(block.start_ns, block.duration_ns)) + " ns",
					  narrow);
		}
		if (!details.source_file.empty())
			key_value(context, 59, "Source path", details.source_file, true);
		if (block.domain == TimingSampleDomain::Gpu) {
			key_value(context, 60, "Submission / zone",
					  std::to_string(block.submission_serial) + " / " +
						  std::to_string(block.zone_index),
					  narrow);
			key_value(context, 61, "Parent zone index", std::to_string(block.parent_zone_index),
					  narrow);
			key_value(context, 62, "Device / queue IDs",
					  std::to_string(block.device_identity) + " / " +
						  std::to_string(block.queue_identity),
					  narrow);
			key_value(context, 63, "Device start / duration",
					  std::to_string(details.device_start_ticks) + " / " +
						  std::to_string(details.device_duration_ticks) + " ticks",
					  narrow);
			key_value(context, 64, "Tick period / valid bits",
					  decimal(details.timestamp_period_ns, " ns", 3) + " / " +
						  std::to_string(details.timestamp_valid_bits),
					  narrow);
			key_value(context, 65, "Calibration ID", std::to_string(details.calibration_id),
					  narrow);
			key_value(context, 66, "Calibration deviation",
					  performance_duration(details.calibration_deviation_ns), narrow);
			key_value(context, 67, "Timestamp stages",
					  std::to_string(details.begin_stage) + " -> " +
						  std::to_string(details.end_stage),
					  narrow);
		}
		if (context.params.app) {
			DevPerformanceInspectorActionParameters parameters;
			parameters.label = "Copy sample identity";
			parameters.app = context.params.app;
			parameters.clipboard_text = "Descriptor: " + std::to_string(block.type_id) +
										"\nInvocation: " + std::to_string(block.invocation_id) +
										"\nApp tick: " + std::to_string(block.app_tick) +
										"\nWindow: " + std::to_string(block.frame.window) +
										"\nFrame: " + std::to_string(block.frame.frameNumber);
			context.uiManager
				.createElement(kDevPerformanceInspectorAction, LocalElementName{"copy-sample"})
				.setParameters(parameters)
				.setDevInternalCapture(true)
				.draw();
		}
	}
}
} // namespace

void DevPerformanceInspectorAction::onPressed(InteractionContext& context) {
	FSEL::detail::selectable_surface::onPressed(context, context.params.enabled);
}
void DevPerformanceInspectorAction::runLogic(InteractionContext& context) {
	FSEL::detail::selectable_surface::runLogic(context, context.params.enabled);
}
void DevPerformanceInspectorAction::onHovered(InteractionContext& context) {
	if (context.params.enabled)
		context.uiManager.requestCursor(CursorType::PointingHand, 4);
}
void DevPerformanceInspectorAction::onReleased(InteractionContext& context) {
	if (!FSEL::detail::selectable_surface::onReleased(context, context.params.enabled))
		return;
	if (context.params.expanded_row)
		*context.params.expanded_row = *context.params.expanded_row == context.params.row_identity
										   ? timeline_no_parent
										   : context.params.row_identity;
	if (context.params.timeline &&
		context.params.command.revision == context.params.timeline->snapshot_revision)
		context.params.timeline->pending = context.params.command;
	if (context.params.app && !context.params.clipboard_text.empty())
		context.params.app->setClipboardText(context.params.clipboard_text);
}
void DevPerformanceInspectorAction::buildElement(BuildContext& context) {
	auto declaration = column(0);
	declaration.layout.sizing.width = CLAY_SIZING_GROW(0);
	declaration.layout.sizing.height = CLAY_SIZING_FIT(28);
	declaration.layout.padding = {6, 6, 5, 5};
	declaration.layout.childAlignment = {CLAY_ALIGN_X_CENTER, CLAY_ALIGN_Y_CENTER};
	declaration.cornerRadius = CLAY_CORNER_RADIUS(3);
	declaration.backgroundColor =
		context.uiManager.getPreviousFramesInteraction().isHovered(context.clayID()) &&
				context.params.enabled
			? interface_theme::kHoverSurface
			: interface_theme::kDepth3Elevated;
	CLAY(context.clayID(), declaration) {
		text(context, context.params.label,
			 context.params.enabled ? interface_theme::kTextCanvas
									: interface_theme::kTextSecondary,
			 11, CLAY_TEXT_ALIGN_CENTER);
	}
}

void DevPerformanceInspector::buildElement(BuildContext& context) {
	if (!context.params.interfaceState)
		return;
	auto& state = context.state();
	auto& timeline = context.params.interfaceState->performance_timeline;
	const auto status = context.params.app
							? context.params.app->devMonitoring().timingReporting().status()
							: TimingReportingStatus{};
	const auto generation = status.capture_generation;
	const bool context_changed = generation != state.analysis.generation ||
								 state.cached_range != state.analysis_range ||
								 state.cached_source != state.analysis_source;
	if (state.refresh_gate.advance(context_changed)) {
		if (context.params.app) {
			auto capture = context.params.app->devMonitoring().timingReporting().read_capture();
			state.analysis =
				analyze_performance(capture, state.analysis_range, state.analysis_source);
		}
		state.cached_ranking = UINT64_MAX;
		state.expanded_zone = timeline_no_parent;
		state.cached_range = state.analysis_range;
		state.cached_source = state.analysis_source;
	}
	state.window_labels.clear();
	state.window_options.clear();
	const auto live_windows =
		context.params.app ? context.params.app->devWindowSnapshot() : std::vector<DevWindowInfo>{};
	state.window_labels.reserve(live_windows.size() +
								context.params.interfaceState->capture_windows.size());
	state.window_options.reserve(state.window_labels.capacity() + 1);
	state.window_options.emplace_back(FSEL::ComboBoxOption{0, "Application tick cadence"});
	for (const auto& windows :
		 {std::span<const DevWindowInfo>{live_windows},
		  std::span<const DevWindowInfo>{context.params.interfaceState->capture_windows}})
		for (const auto& window : windows) {
			if (window.title == "FlowUi Developer Interface" ||
				std::ranges::any_of(state.window_options,
									[&](const auto& known) { return known.value == window.id; }))
				continue;
			state.window_labels.emplace_back(
				window.title.empty() ? "Window " + std::to_string(window.id) : window.title);
			state.window_options.emplace_back(
				FSEL::ComboBoxOption{window.id, state.window_labels.back()});
		}
	const auto previous_bounds = Clay_GetElementData(context.clayID());
	const float width = previous_bounds.found ? previous_bounds.boundingBox.width
											  : context.params.interfaceState->inspectorWidth;
	const bool narrow = width < 360;
	auto root = column(0);
	root.layout.sizing = {.width = CLAY_SIZING_PERCENT(1), .height = CLAY_SIZING_PERCENT(1)};
	root.backgroundColor = interface_theme::kDepth1Panel;
	root.clip.horizontal = false;
	root.clip.scrollInputDisabled = true;
	CLAY(context.clayID(), root) {
		auto title = row(36);
		title.layout.padding = {12, 12, 0, 0};
		title.backgroundColor = interface_theme::kDepth2Ink;
		CLAY(context.clayID("title"), title) {
			text(context, "Inspector", interface_theme::kTextCanvas, 14);
		}
		auto body = column(0);
		body.layout.sizing.height = CLAY_SIZING_GROW(0);
		body.clip = {.vertical = true};
		auto scroll = Clay_GetScrollContainerData(context.clayID("scroll-body"));
		if (scroll.found && scroll.scrollPosition) {
			if (state.previous_selection != timeline.inspected_sample ||
				state.selected_revision != timeline.snapshot_revision) {
				const auto selected = Clay_GetElementData(context.clayID("selected-section"));
				const auto viewport = Clay_GetElementData(context.clayID("scroll-body"));
				if (selected.found && viewport.found &&
					selected.boundingBox.y < viewport.boundingBox.y)
					scroll.scrollPosition->y += viewport.boundingBox.y - selected.boundingBox.y;
			}
			body.clip.childOffset = *scroll.scrollPosition;
		}
		state.previous_selection = timeline.inspected_sample;
		state.selected_revision = timeline.snapshot_revision;
		auto content = column(8);
		content.layout.padding = {12, 12, 10, 12};
		CLAY(context.clayID("scroll-body"), body) {
			CLAY(context.clayID("capture-section"), column(0)) {
				disclosure(context, LocalElementName{"capture-header"}, "Capture Settings",
						   state.capture_expanded, true);
				if (state.capture_expanded)
					CLAY(context.clayID("capture-content"), content) {
						build_capture(context, narrow, status);
					}
			}
			CLAY(context.clayID("rolling-section"), column(0)) {
				disclosure(context, LocalElementName{"rolling-header"}, "Rolling Stats",
						   state.rolling_expanded, true);
				if (state.rolling_expanded)
					CLAY(context.clayID("rolling-content"), content) {
						build_rolling(context, narrow);
					}
			}
			CLAY(context.clayID("selected-section"), column(0)) {
				disclosure(context, LocalElementName{"selected-header"}, "Selected Zone Details",
						   state.selected_expanded, true);
				if (state.selected_expanded) {
					// Details can only be resolved while their original capture generation is
					// leased.
					auto capture =
						context.params.app
							? context.params.app->devMonitoring().timingReporting().read_capture()
							: TimingCaptureReadView{};
					CLAY(context.clayID("selected-content"), content) {
						if (timeline.capture_generation &&
							capture.generation != timeline.capture_generation)
							text(context, "The selected sample is no longer retained.",
								 interface_theme::kTextSecondary);
						else
							build_selected(context, narrow);
					}
				}
			}
		}
	}
}
} // namespace FlowUi::devSystems::interface_elements
#endif
