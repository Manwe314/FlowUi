#include "devSystems/devAgenticWorkflow/AgenticInteraction.hpp"
#include "FlowUi/Public/input/Key.hpp"
#if FLOW_UI_AGENTIC_WORKFLOW
#include "devSystems/devAgenticWorkflow/AgenticManifest.hpp"

#include <algorithm>
#include <cmath>
#include <limits>
#include <new>

namespace FlowUi::devSystems::agentic {
namespace {
constexpr std::uint64_t hard_maximum_frames = 65536;
constexpr std::uint64_t hard_maximum_bytes = 128u * 1024u * 1024u;
constexpr std::uint64_t hard_maximum_duration = 600'000'000'000ull;
constexpr std::uint32_t no_step = std::numeric_limits<std::uint32_t>::max();

[[nodiscard]] FlowUiError invalid() noexcept {
	return agentic_error(AgenticError::invalid_request);
}
[[nodiscard]] FlowUiError budget() noexcept {
	return agentic_error(AgenticError::budget_exhausted);
}
[[nodiscard]] bool
add_checked(std::uint64_t& value, std::uint64_t increment,
			std::uint64_t maximum = std::numeric_limits<std::uint64_t>::max()) noexcept {
	if (increment > maximum || value > maximum - increment)
		return false;
	value += increment;
	return true;
}
[[nodiscard]] bool finite_coordinate(double value) noexcept {
	return std::isfinite(value) && std::abs(value) <= std::numeric_limits<float>::max();
}
[[nodiscard]] bool scalar(char32_t value) noexcept {
	return value <= 0x10ffff && !(value >= 0xd800 && value <= 0xdfff);
}
[[nodiscard]] bool known_key(std::uint16_t value) noexcept {
	return FlowUi::keyFromCode(value).has_value();
}
[[nodiscard]] bool object_fields(const Json& value,
								 std::initializer_list<std::string_view> allowed) noexcept {
	if (value.kind != Json::Kind::object)
		return false;
	for (std::size_t index = 0; index < value.object.size(); ++index) {
		const auto& key = value.object[index].first;
		if (std::find(allowed.begin(), allowed.end(), key) == allowed.end())
			return false;
		for (std::size_t prior = 0; prior < index; ++prior)
			if (value.object[prior].first == key)
				return false;
	}
	return true;
}
[[nodiscard]] bool decimal_field(const Json& object, std::string_view key, std::uint64_t& result,
								 bool required = false) noexcept {
	const auto* member = object.find(key);
	if (!member)
		return !required;
	auto decoded = parse_decimal(*member);
	if (!decoded)
		return false;
	result = *decoded;
	return true;
}
[[nodiscard]] bool string_field(const Json& object, std::string_view key, std::string& result,
								std::size_t maximum = 1024) {
	const auto* member = object.find(key);
	if (!member)
		return true;
	if (member->kind != Json::Kind::string || member->text.size() > maximum)
		return false;
	result = member->text;
	return true;
}
[[nodiscard]] bool bool_field(const Json& object, std::string_view key, bool& result) noexcept {
	const auto* member = object.find(key);
	if (!member)
		return true;
	if (member->kind != Json::Kind::boolean)
		return false;
	result = member->boolean;
	return true;
}
[[nodiscard]] bool number_field(const Json& object, std::string_view key, double& result) noexcept {
	const auto* member = object.find(key);
	if (!member)
		return true;
	if (member->kind != Json::Kind::number)
		return false;
	result = member->number_or(std::numeric_limits<double>::quiet_NaN());
	return finite_coordinate(result);
}
[[nodiscard]] bool numeric_slot(const Json& value, std::uint64_t maximum,
								std::uint64_t& result) noexcept {
	if (value.kind != Json::Kind::number || value.text.empty())
		return false;
	std::uint64_t accumulator = 0;
	for (const char digit : value.text) {
		if (digit < '0' || digit > '9' || accumulator > maximum / 10)
			return false;
		accumulator *= 10;
		if (static_cast<unsigned>(digit - '0') > maximum - accumulator)
			return false;
		accumulator += static_cast<unsigned>(digit - '0');
	}
	result = accumulator;
	return true;
}
[[nodiscard]] bool decode_utf8(std::string_view text, std::vector<char32_t>& result,
							   std::uint64_t maximum) {
	result.clear();
	result.reserve(static_cast<std::size_t>(std::min<std::uint64_t>(text.size(), maximum)));
	for (std::size_t position = 0; position < text.size();) {
		const auto first = static_cast<unsigned char>(text[position++]);
		char32_t codepoint = first;
		std::uint32_t trailing = 0;
		char32_t minimum = 0;
		if (first >= 0xc2 && first <= 0xdf) {
			codepoint = first & 31;
			trailing = 1;
			minimum = 0x80;
		} else if (first >= 0xe0 && first <= 0xef) {
			codepoint = first & 15;
			trailing = 2;
			minimum = 0x800;
		} else if (first >= 0xf0 && first <= 0xf4) {
			codepoint = first & 7;
			trailing = 3;
			minimum = 0x10000;
		} else if (first >= 0x80)
			return false;
		if (trailing > text.size() - position)
			return false;
		for (std::uint32_t index = 0; index < trailing; ++index) {
			const auto next = static_cast<unsigned char>(text[position++]);
			if ((next & 0xc0) != 0x80)
				return false;
			codepoint = (codepoint << 6) | (next & 63);
		}
		if (codepoint < minimum || !scalar(codepoint) || result.size() >= maximum)
			return false;
		result.emplace_back(codepoint);
	}
	return true;
}
[[nodiscard]] std::string encode_utf8(std::span<const char32_t> text) {
	std::string result;
	result.reserve(text.size());
	for (const char32_t codepoint : text) {
		if (codepoint <= 0x7f)
			result.push_back(static_cast<char>(codepoint));
		else if (codepoint <= 0x7ff) {
			result.push_back(static_cast<char>(0xc0 | (codepoint >> 6)));
			result.push_back(static_cast<char>(0x80 | (codepoint & 63)));
		} else if (codepoint <= 0xffff) {
			result.push_back(static_cast<char>(0xe0 | (codepoint >> 12)));
			result.push_back(static_cast<char>(0x80 | ((codepoint >> 6) & 63)));
			result.push_back(static_cast<char>(0x80 | (codepoint & 63)));
		} else {
			result.push_back(static_cast<char>(0xf0 | (codepoint >> 18)));
			result.push_back(static_cast<char>(0x80 | ((codepoint >> 12) & 63)));
			result.push_back(static_cast<char>(0x80 | ((codepoint >> 6) & 63)));
			result.push_back(static_cast<char>(0x80 | (codepoint & 63)));
		}
	}
	return result;
}
[[nodiscard]] bool decode_input(const Json& value, AgenticInputSnapshot& result,
								bool flattened = false) {
	if (!object_fields(
			value,
			flattened ? std::initializer_list<std::string_view>{"offset", "window_frame", "pointer",
																"scroll", "pointer_inside",
																"window_focused", "buttons_down",
																"keys_down", "text"}
					  : std::initializer_list<std::string_view>{
							"pointer", "scroll", "pointer_inside", "window_focused", "buttons_down",
							"keys_down", "text"}))
		return false;
	if (const auto* pointer = value.find("pointer")) {
		if (!object_fields(*pointer, {"x", "y"}) ||
			!number_field(*pointer, "x", result.pointer_x) ||
			!number_field(*pointer, "y", result.pointer_y))
			return false;
	}
	if (const auto* scroll = value.find("scroll")) {
		if (!object_fields(*scroll, {"x", "y"}) || !number_field(*scroll, "x", result.scroll_x) ||
			!number_field(*scroll, "y", result.scroll_y))
			return false;
	}
	if (!bool_field(value, "pointer_inside", result.pointer_inside) ||
		!bool_field(value, "window_focused", result.window_focused))
		return false;
	if (const auto* text = value.find("text")) {
		if (text->kind != Json::Kind::string || !decode_utf8(text->text, result.text, 1048576))
			return false;
	}
	if (const auto* buttons = value.find("buttons_down")) {
		if (buttons->kind != Json::Kind::array ||
			buttons->array.size() > FrameInput::kMouseButtonCount)
			return false;
		result.buttons_down.reserve(buttons->array.size());
		for (const auto& button : buttons->array) {
			std::uint64_t slot = 0;
			if (!numeric_slot(button, FrameInput::kMouseButtonCount - 1, slot))
				return false;
			result.buttons_down.emplace_back(static_cast<std::uint8_t>(slot));
		}
	}
	if (const auto* keys = value.find("keys_down")) {
		if (keys->kind != Json::Kind::array || keys->array.size() > FrameInput::kKeyboardKeyCount)
			return false;
		result.keys_down.reserve(keys->array.size());
		for (const auto& key : keys->array) {
			std::uint64_t slot = 0;
			if (!numeric_slot(key, FrameInput::kKeyboardKeyCount - 1, slot))
				return false;
			result.keys_down.emplace_back(static_cast<std::uint16_t>(slot));
		}
	}
	return bool(validate_interaction_input(result, 1048576));
}
[[nodiscard]] bool decode_target(const Json& value, AgenticInteractionTarget& target) {
	if (!object_fields(value, {"window_id", "point", "clay_id", "clay_name", "flow_instance_id",
							   "flow_debug_name", "custom_id", "tree_generation", "layout_revision",
							   "custom_revision", "anchor"}))
		return false;
	if (!decimal_field(value, "window_id", target.window_id))
		return false;
	std::uint32_t selector_count = 0;
	if (const auto* point = value.find("point")) {
		if (!object_fields(*point, {"x", "y"}) || !point->find("x") || !point->find("y") ||
			!number_field(*point, "x", target.point.x) ||
			!number_field(*point, "y", target.point.y))
			return false;
		target.kind = AgenticTargetKind::coordinates;
		++selector_count;
	}
	if (value.find("clay_id")) {
		if (!decimal_field(value, "clay_id", target.clay_id, true) || !target.clay_id)
			return false;
		target.kind = AgenticTargetKind::clay_id;
		++selector_count;
	}
	if (value.find("clay_name")) {
		if (!string_field(value, "clay_name", target.clay_name) || target.clay_name.empty())
			return false;
		target.kind = AgenticTargetKind::clay_name;
		++selector_count;
	}
	if (value.find("flow_instance_id")) {
		if (!decimal_field(value, "flow_instance_id", target.flow_instance_id, true) ||
			!target.flow_instance_id)
			return false;
		target.kind = AgenticTargetKind::flow_instance;
		++selector_count;
	}
	if (value.find("flow_debug_name")) {
		if (!string_field(value, "flow_debug_name", target.flow_debug_name) ||
			target.flow_debug_name.empty())
			return false;
		target.kind = AgenticTargetKind::flow_debug;
		++selector_count;
	}
	if (value.find("custom_id")) {
		if (!string_field(value, "custom_id", target.custom_id) || target.custom_id.empty())
			return false;
		target.kind = AgenticTargetKind::custom_region;
		++selector_count;
	}
	for (auto [key, destination] : {std::pair{"tree_generation", &target.tree_generation},
									{"layout_revision", &target.layout_revision},
									{"custom_revision", &target.custom_revision}}) {
		if (const auto* member = value.find(key)) {
			auto parsed = parse_decimal(*member);
			if (!parsed)
				return false;
			*destination = *parsed;
		}
	}
	if (const auto* anchor = value.find("anchor")) {
		if (!object_fields(*anchor, {"x", "y"}) || !number_field(*anchor, "x", target.anchor_x) ||
			!number_field(*anchor, "y", target.anchor_y))
			return false;
	}
	return selector_count == 1 && target.anchor_x >= 0 && target.anchor_x <= 1 &&
		   target.anchor_y >= 0 && target.anchor_y <= 1;
}
constexpr std::pair<std::string_view, AgenticInteractionExpectation> expectation_names[] = {
	{"target_hovered", AgenticInteractionExpectation::target_hovered},
	{"target_pressed", AgenticInteractionExpectation::target_pressed},
	{"target_held", AgenticInteractionExpectation::target_held},
	{"target_released", AgenticInteractionExpectation::target_released},
	{"layout_completed", AgenticInteractionExpectation::layout_completed},
	{"frame_presented", AgenticInteractionExpectation::frame_presented},
	{"window_focused", AgenticInteractionExpectation::window_focused},
	{"text_focused", AgenticInteractionExpectation::text_focused}};
[[nodiscard]] Json expectations_json(AgenticInteractionExpectation value) {
	Json result = Json::make_array();
	for (const auto& [label, flag] : expectation_names)
		if (has_expectation(value, flag))
			result.array.emplace_back(label);
	return result;
}
[[nodiscard]] bool decode_step(const Json& value, AgenticInteractionStep& step) {
	if (!object_fields(value, {"step_id", "operation", "target", "destination", "path", "keys",
							   "text", "recipe", "frames", "minimum_duration_ns", "deadline_ns",
							   "scroll", "button", "expect", "require_hover_before_down"}))
		return false;
	const auto* operation = value.find("operation");
	if (!operation || operation->kind != Json::Kind::string)
		return false;
	bool recognized = false;
	for (std::uint32_t index = 0;
		 index <= static_cast<std::uint32_t>(AgenticInteractionOperation::assert_observation);
		 ++index) {
		auto candidate = static_cast<AgenticInteractionOperation>(index);
		if (operation->text == name(candidate) ||
			(operation->text == "hover" &&
			 candidate == AgenticInteractionOperation::pointer_move)) {
			step.operation = candidate;
			recognized = true;
			break;
		}
	}
	if (!recognized || !string_field(value, "step_id", step.step_id, 128) ||
		!decimal_field(value, "frames", step.frames) ||
		!decimal_field(value, "minimum_duration_ns", step.minimum_duration_ns) ||
		!decimal_field(value, "deadline_ns", step.deadline_ns) ||
		!bool_field(value, "require_hover_before_down", step.require_hover_before_down))
		return false;
	for (auto [key, destination] :
		 {std::pair{"target", &step.target}, {"destination", &step.destination}}) {
		if (const auto* member = value.find(key)) {
			AgenticInteractionTarget target;
			if (!decode_target(*member, target))
				return false;
			destination->emplace(std::move(target));
		}
	}
	if (const auto* path = value.find("path")) {
		if (path->kind != Json::Kind::array || path->array.size() > hard_maximum_frames)
			return false;
		step.path.reserve(path->array.size());
		for (const auto& point : path->array) {
			AgenticInteractionPoint decoded;
			if (!object_fields(point, {"x", "y"}) || !point.find("x") || !point.find("y") ||
				!number_field(point, "x", decoded.x) || !number_field(point, "y", decoded.y))
				return false;
			step.path.emplace_back(decoded);
		}
	}
	if (const auto* keys = value.find("keys")) {
		if (keys->kind != Json::Kind::array || keys->array.size() > FrameInput::kKeyboardKeyCount)
			return false;
		step.keys.reserve(keys->array.size());
		for (const auto& key : keys->array) {
			std::uint64_t decoded = 0;
			if (!numeric_slot(key, FrameInput::kKeyboardKeyCount - 1, decoded))
				return false;
			step.keys.emplace_back(static_cast<std::uint16_t>(decoded));
		}
	}
	if (const auto* text = value.find("text"))
		if (text->kind != Json::Kind::string || !decode_utf8(text->text, step.text, 1048576))
			return false;
	if (const auto* button = value.find("button")) {
		std::uint64_t decoded = 0;
		if (!numeric_slot(*button, FrameInput::kMouseButtonCount - 1, decoded))
			return false;
		step.button = static_cast<std::uint8_t>(decoded);
	}
	if (const auto* scroll = value.find("scroll"))
		if (!object_fields(*scroll, {"x", "y"}) || !number_field(*scroll, "x", step.scroll_x) ||
			!number_field(*scroll, "y", step.scroll_y))
			return false;
	if (const auto* recipe = value.find("recipe")) {
		if (!object_fields(*recipe, {"button", "prepare_up_frames", "down_frames", "hold_frames",
									 "up_frames", "repeat", "inter_click_gap_frames"}) ||
			!decimal_field(*recipe, "prepare_up_frames", step.recipe.prepare_up_frames) ||
			!decimal_field(*recipe, "down_frames", step.recipe.down_frames) ||
			!decimal_field(*recipe, "hold_frames", step.recipe.hold_frames) ||
			!decimal_field(*recipe, "up_frames", step.recipe.up_frames) ||
			!decimal_field(*recipe, "repeat", step.recipe.repeat) ||
			!decimal_field(*recipe, "inter_click_gap_frames", step.recipe.inter_click_gap_frames))
			return false;
		if (const auto* button = recipe->find("button")) {
			std::uint64_t decoded = 0;
			if (!numeric_slot(*button, FrameInput::kMouseButtonCount - 1, decoded))
				return false;
			step.recipe.button = static_cast<std::uint8_t>(decoded);
		}
	}
	if (const auto* expect = value.find("expect")) {
		if (expect->kind != Json::Kind::array ||
			expect->array.size() > std::size(expectation_names))
			return false;
		for (const auto& member : expect->array) {
			bool found = false;
			if (member.kind != Json::Kind::string)
				return false;
			for (const auto& [label, flag] : expectation_names)
				if (member.text == label) {
					if (has_expectation(step.expect, flag))
						return false;
					step.expect = step.expect | flag;
					found = true;
					break;
				}
			if (!found)
				return false;
		}
	}
	return true;
}
[[nodiscard]] Json limits_json(const AgenticInteractionLimits& value) {
	return Json::make_object(
		{{"maximum_frames", decimal(value.maximum_frames)},
		 {"maximum_spans", decimal(value.maximum_spans)},
		 {"maximum_steps", decimal(value.maximum_steps)},
		 {"maximum_target_candidates", decimal(value.maximum_target_candidates)},
		 {"maximum_text_codepoints", decimal(value.maximum_text_codepoints)},
		 {"maximum_observations", decimal(value.maximum_observations)},
		 {"maximum_artifact_bytes", decimal(value.maximum_artifact_bytes)},
		 {"maximum_duration_ns", decimal(value.maximum_duration_ns)}});
}
[[nodiscard]] bool decode_limits(const Json& value, AgenticInteractionLimits& limits) noexcept {
	return object_fields(value, {"maximum_frames", "maximum_spans", "maximum_steps",
								 "maximum_target_candidates", "maximum_text_codepoints",
								 "maximum_observations", "maximum_artifact_bytes",
								 "maximum_duration_ns"}) &&
		   decimal_field(value, "maximum_frames", limits.maximum_frames) &&
		   decimal_field(value, "maximum_spans", limits.maximum_spans) &&
		   decimal_field(value, "maximum_steps", limits.maximum_steps) &&
		   decimal_field(value, "maximum_target_candidates", limits.maximum_target_candidates) &&
		   decimal_field(value, "maximum_text_codepoints", limits.maximum_text_codepoints) &&
		   decimal_field(value, "maximum_observations", limits.maximum_observations) &&
		   decimal_field(value, "maximum_artifact_bytes", limits.maximum_artifact_bytes) &&
		   decimal_field(value, "maximum_duration_ns", limits.maximum_duration_ns);
}
[[nodiscard]] bool valid_limits(const AgenticInteractionLimits& limits,
								const AgenticInteractionLimits& available) noexcept {
	return limits.maximum_frames && limits.maximum_frames <= hard_maximum_frames &&
		   limits.maximum_frames <= available.maximum_frames && limits.maximum_spans &&
		   limits.maximum_spans <= hard_maximum_frames &&
		   limits.maximum_spans <= available.maximum_spans && limits.maximum_steps &&
		   limits.maximum_steps <= 4096 && limits.maximum_steps <= available.maximum_steps &&
		   limits.maximum_target_candidates && limits.maximum_target_candidates <= 4096 &&
		   limits.maximum_target_candidates <= available.maximum_target_candidates &&
		   limits.maximum_text_codepoints <= 1048576 &&
		   limits.maximum_text_codepoints <= available.maximum_text_codepoints &&
		   limits.maximum_observations && limits.maximum_observations <= 131072 &&
		   limits.maximum_observations <= available.maximum_observations &&
		   limits.maximum_artifact_bytes && limits.maximum_artifact_bytes <= hard_maximum_bytes &&
		   limits.maximum_artifact_bytes <= available.maximum_artifact_bytes &&
		   limits.maximum_duration_ns && limits.maximum_duration_ns <= hard_maximum_duration &&
		   limits.maximum_duration_ns <= available.maximum_duration_ns;
}
[[nodiscard]] bool valid_target(const AgenticInteractionTarget& target, std::uint64_t window,
								const AgenticInteractionCapabilities& capabilities) noexcept {
	if (target.window_id && target.window_id != window)
		return false;
	if (!finite_coordinate(target.anchor_x) || !finite_coordinate(target.anchor_y) ||
		target.anchor_x < 0 || target.anchor_x > 1 || target.anchor_y < 0 || target.anchor_y > 1)
		return false;
	switch (target.kind) {
	case AgenticTargetKind::coordinates:
		return finite_coordinate(target.point.x) && finite_coordinate(target.point.y);
	case AgenticTargetKind::clay_id:
		return capabilities.geometry_targets && target.clay_id &&
			   target.clay_id <= std::numeric_limits<std::uint32_t>::max();
	case AgenticTargetKind::clay_name:
		return capabilities.geometry_targets && !target.clay_name.empty() &&
			   target.clay_name.size() <= 1024;
	case AgenticTargetKind::flow_instance:
		return capabilities.geometry_targets && target.flow_instance_id;
	case AgenticTargetKind::flow_debug:
		return capabilities.geometry_targets && !target.flow_debug_name.empty() &&
			   target.flow_debug_name.size() <= 1024;
	case AgenticTargetKind::custom_region:
		return capabilities.custom_targets && !target.custom_id.empty() &&
			   target.custom_id.size() <= 1024;
	}
	return false;
}
[[nodiscard]] Json frame_key_json(const EvidenceFrameKey& key) {
	return Json::make_object({{"run_id", key.run_id},
							  {"app_tick", decimal(key.app_tick)},
							  {"window_id", decimal(key.window_id)},
							  {"window_frame", decimal(key.window_frame)}});
}
[[nodiscard]] Json span_json(const AgenticInteractionSpan& span, bool include_text, bool compiled) {
	Json result = Json::make_object({{"offset", decimal(span.offset)},
									 {"count", decimal(span.count)},
									 {"input", to_json(span.input, include_text)}});
	if (compiled) {
		result.set("step_index", Json::number(span.step_index));
		result.set("minimum_duration_ns", decimal(span.minimum_duration_ns));
		result.set("pointer_progress", Json(span.pointer_progress));
		result.set("target_pointer", span.target_pointer);
		result.set("destination_pointer", span.destination_pointer);
		result.set("resolve_before", span.resolve_before);
		result.set("require_hover_before", span.require_hover_before);
		result.set("preserve_pointer", span.preserve_pointer);
	}
	return result;
}
[[nodiscard]] Json region_json(const AgenticInteractionRegion& value) {
	return Json::make_object({{"x", Json(value.x)},
							  {"y", Json(value.y)},
							  {"width", Json(value.width)},
							  {"height", Json(value.height)}});
}
[[nodiscard]] Json ids_json(std::span<const std::uint64_t> values) {
	Json result = Json::make_array();
	result.array.reserve(values.size());
	for (const auto value : values)
		result.array.emplace_back(decimal(value));
	return result;
}
} // namespace

Status validate_interaction_input(const AgenticInputSnapshot& input,
								  std::uint64_t maximum_text_codepoints) noexcept {
	if (!finite_coordinate(input.pointer_x) || !finite_coordinate(input.pointer_y) ||
		!finite_coordinate(input.scroll_x) || !finite_coordinate(input.scroll_y) ||
		input.text.size() > maximum_text_codepoints ||
		input.keys_down.size() > FrameInput::kKeyboardKeyCount ||
		input.buttons_down.size() > FrameInput::kMouseButtonCount)
		return unexpectedError(invalid());
	std::uint16_t previous_key = 0;
	bool first = true;
	for (const auto key : input.keys_down) {
		if (!known_key(key) || (!first && key <= previous_key))
			return unexpectedError(invalid());
		first = false;
		previous_key = key;
	}
	std::uint8_t previous_button = 0;
	first = true;
	for (const auto button : input.buttons_down) {
		if (button >= FrameInput::kMouseButtonCount || (!first && button <= previous_button))
			return unexpectedError(invalid());
		first = false;
		previous_button = button;
	}
	for (const auto codepoint : input.text)
		if (!scalar(codepoint))
			return unexpectedError(invalid());
	return {};
}

Result<AgenticInteractionPlan> interaction_plan_from_json(const Json& input) noexcept {
	try {
		if (!object_fields(input,
						   {"schema", "schema_version", "mode", "window_id", "run_id",
							"first_window_frame", "clock", "coordinates", "baseline", "fixture_id",
							"fixture_digest", "seed", "deadline_ns", "idempotency_key", "limits",
							"content", "frames", "spans", "steps"}))
			return unexpectedError(invalid());
		if (const auto* schema = input.find("schema");
			schema && schema->string_or() != "FlowUi.Agentic.InteractionPlan")
			return unexpectedError(invalid());
		if (const auto* version = input.find("schema_version");
			version && version->string_or() != "1.0")
			return unexpectedError(invalid());
		AgenticInteractionPlan result;
		if (!decimal_field(input, "window_id", result.window_id, true) || !result.window_id ||
			!decimal_field(input, "first_window_frame", result.first_window_frame) ||
			!decimal_field(input, "seed", result.seed) ||
			!decimal_field(input, "deadline_ns", result.deadline_ns) ||
			!string_field(input, "run_id", result.run_id, 128) ||
			!string_field(input, "fixture_id", result.fixture_id) ||
			!string_field(input, "fixture_digest", result.fixture_digest) ||
			!string_field(input, "idempotency_key", result.idempotency_key, 128))
			return unexpectedError(invalid());
		if (const auto* mode = input.find("mode")) {
			if (mode->string_or() == "raw_frames")
				result.mode = AgenticInteractionMode::raw_frames;
			else if (mode->string_or() == "common_controls")
				result.mode = AgenticInteractionMode::common_controls;
			else
				return unexpectedError(invalid());
		}
		if (const auto* clock = input.find("clock")) {
			if (clock->string_or() == "window_input_frames")
				result.clock = AgenticInteractionClock::window_input_frames;
			else if (clock->string_or() == "exact_window_frames")
				result.clock = AgenticInteractionClock::exact_window_frames;
			else
				return unexpectedError(invalid());
		}
		if (const auto* coordinates = input.find("coordinates");
			coordinates && coordinates->string_or() != "window_logical")
			return unexpectedError(invalid());
		if (const auto* baseline = input.find("baseline")) {
			if (baseline->string_or() == "neutral_before_start")
				result.baseline = AgenticInteractionBaseline::neutral_before_start;
			else if (baseline->string_or() == "require_neutral")
				result.baseline = AgenticInteractionBaseline::require_neutral;
			else
				return unexpectedError(invalid());
		}
		if (const auto* limits = input.find("limits");
			limits && !decode_limits(*limits, result.limits))
			return unexpectedError(invalid());
		if (const auto* content = input.find("content")) {
			if (!object_fields(*content,
							   {"persistence", "include_native_input", "record_observations"}) ||
				!bool_field(*content, "include_native_input", result.include_native_input) ||
				!bool_field(*content, "record_observations", result.record_observations))
				return unexpectedError(invalid());
			if (const auto* persistence = content->find("persistence")) {
				result.persistence_declared = true;
				if (persistence->string_or() == "redact_text")
					result.persistence = AgenticInteractionPersistence::redact_text;
				else if (persistence->string_or() == "retain_text")
					result.persistence = AgenticInteractionPersistence::retain_text;
				else if (persistence->string_or() == "volatile")
					result.persistence = AgenticInteractionPersistence::volatile_plan;
				else
					return unexpectedError(invalid());
			}
		}
		if (input.find("frames") && input.find("spans"))
			return unexpectedError(invalid());
		if (const auto* frames = input.find("frames")) {
			if (frames->kind != Json::Kind::array || frames->array.size() > hard_maximum_frames)
				return unexpectedError(budget());
			result.spans.reserve(frames->array.size());
			for (const auto& frame : frames->array) {
				AgenticInteractionSpan row;
				row.step_index = no_step;
				if (!decode_input(frame, row.input, true))
					return unexpectedError(invalid());
				if (result.clock == AgenticInteractionClock::window_input_frames) {
					if (frame.find("window_frame") ||
						!decimal_field(frame, "offset", row.offset, true))
						return unexpectedError(invalid());
				} else {
					std::uint64_t address = 0;
					if (frame.find("offset") ||
						!decimal_field(frame, "window_frame", address, true))
						return unexpectedError(invalid());
					if (result.spans.empty()) {
						if (input.find("first_window_frame") &&
							address != result.first_window_frame)
							return unexpectedError(invalid());
						result.first_window_frame = address;
					}
					if (address < result.first_window_frame)
						return unexpectedError(invalid());
					row.offset = address - result.first_window_frame;
				}
				result.spans.emplace_back(std::move(row));
			}
		}
		if (const auto* spans = input.find("spans")) {
			if (spans->kind != Json::Kind::array || spans->array.size() > hard_maximum_frames)
				return unexpectedError(budget());
			result.spans.reserve(spans->array.size());
			for (const auto& span : spans->array) {
				AgenticInteractionSpan row;
				row.step_index = no_step;
				const auto* state = span.find("input");
				if (!object_fields(span, {"offset", "count", "input"}) || !state ||
					!decode_input(*state, row.input) ||
					!decimal_field(span, "offset", row.offset, true) ||
					!decimal_field(span, "count", row.count, true))
					return unexpectedError(invalid());
				result.spans.emplace_back(std::move(row));
			}
		}
		if (const auto* steps = input.find("steps")) {
			if (steps->kind != Json::Kind::array || steps->array.size() > 4096)
				return unexpectedError(budget());
			result.steps.reserve(steps->array.size());
			for (const auto& step : steps->array) {
				AgenticInteractionStep decoded;
				if (!decode_step(step, decoded))
					return unexpectedError(invalid());
				result.steps.emplace_back(std::move(decoded));
			}
		}
		return result;
	} catch (...) {
		return unexpectedError(makeError(ErrorCode::AllocationFailed, ErrorSite::AgenticCommand));
	}
}

Result<AgenticCompiledInteraction>
compile_interaction_plan(const AgenticInteractionPlan& plan,
						 const AgenticInteractionCapabilities& capabilities) noexcept {
	try {
		if (!plan.window_id || plan.window_id > std::numeric_limits<std::uint32_t>::max() ||
			!valid_limits(plan.limits, capabilities.limits) || !plan.deadline_ns ||
			plan.deadline_ns > plan.limits.maximum_duration_ns || plan.run_id.size() > 128 ||
			(!plan.run_id.empty() && !valid_identifier(plan.run_id)) ||
			plan.idempotency_key.size() > 128 || plan.fixture_id.size() > 1024 ||
			plan.fixture_digest.size() > 1024)
			return unexpectedError(invalid());
		if (plan.mode != AgenticInteractionMode::raw_frames &&
			plan.mode != AgenticInteractionMode::common_controls)
			return unexpectedError(invalid());
		if (plan.clock != AgenticInteractionClock::window_input_frames &&
			plan.clock != AgenticInteractionClock::exact_window_frames)
			return unexpectedError(invalid());
		if ((plan.baseline != AgenticInteractionBaseline::neutral_before_start &&
			 plan.baseline != AgenticInteractionBaseline::require_neutral) ||
			(plan.persistence != AgenticInteractionPersistence::redact_text &&
			 plan.persistence != AgenticInteractionPersistence::retain_text &&
			 plan.persistence != AgenticInteractionPersistence::volatile_plan))
			return unexpectedError(invalid());
		if (plan.clock == AgenticInteractionClock::exact_window_frames &&
			(!capabilities.exact_addresses || plan.run_id.empty() || !plan.first_window_frame ||
			 plan.baseline != AgenticInteractionBaseline::require_neutral ||
			 plan.mode != AgenticInteractionMode::raw_frames))
			return unexpectedError(invalid());
		if ((plan.mode == AgenticInteractionMode::raw_frames &&
			 (!capabilities.raw_frames || plan.spans.empty() || !plan.steps.empty())) ||
			(plan.mode == AgenticInteractionMode::common_controls &&
			 (!capabilities.common_controls || plan.steps.empty() || !plan.spans.empty())))
			return unexpectedError(invalid());
		if (plan.spans.size() > plan.limits.maximum_spans ||
			plan.steps.size() > plan.limits.maximum_steps)
			return unexpectedError(budget());
		std::uint64_t source_storage = sizeof(AgenticInteractionPlan) + plan.run_id.size() +
									   plan.fixture_id.size() + plan.fixture_digest.size() +
									   plan.idempotency_key.size();
		for (const auto& span : plan.spans) {
			if (!validate_interaction_input(span.input, plan.limits.maximum_text_codepoints))
				return unexpectedError(invalid());
			if (!add_checked(source_storage,
							 sizeof(AgenticInteractionSpan) +
								 span.input.text.size() * sizeof(char32_t) +
								 span.input.keys_down.size() * sizeof(std::uint16_t) +
								 span.input.buttons_down.size(),
							 plan.limits.maximum_artifact_bytes))
				return unexpectedError(budget());
		}
		for (const auto& step : plan.steps) {
			if (step.path.size() > plan.limits.maximum_frames ||
				step.keys.size() > FrameInput::kKeyboardKeyCount ||
				step.text.size() > plan.limits.maximum_text_codepoints || step.step_id.size() > 128)
				return unexpectedError(budget());
			if (!add_checked(source_storage,
							 sizeof(AgenticInteractionStep) +
								 step.path.size() * sizeof(AgenticInteractionPoint) +
								 step.keys.size() * sizeof(std::uint16_t) +
								 step.text.size() * sizeof(char32_t) + step.step_id.size(),
							 plan.limits.maximum_artifact_bytes))
				return unexpectedError(budget());
			for (const auto* target : {&step.target, &step.destination})
				if (*target) {
					if ((*target)->clay_name.size() > 1024 ||
						(*target)->flow_debug_name.size() > 1024 ||
						(*target)->custom_id.size() > 1024 ||
						!add_checked(source_storage,
									 (*target)->clay_name.size() +
										 (*target)->flow_debug_name.size() +
										 (*target)->custom_id.size(),
									 plan.limits.maximum_artifact_bytes))
						return unexpectedError(budget());
				}
		}
		AgenticCompiledInteraction result;
		result.spans.reserve(static_cast<std::size_t>(std::min<std::uint64_t>(
			plan.limits.maximum_spans,
			std::max<std::size_t>(plan.spans.size(), plan.steps.size() * 5))));
		std::uint64_t storage = sizeof(AgenticCompiledInteraction) + source_storage;
		if (storage > plan.limits.maximum_artifact_bytes)
			return unexpectedError(budget());
		auto emit = [&](AgenticInteractionSpan span) -> Status {
			if (!span.count || span.offset != result.frame_count ||
				result.spans.size() >= plan.limits.maximum_spans ||
				!add_checked(result.frame_count, span.count, plan.limits.maximum_frames))
				return unexpectedError(budget());
			if (auto status =
					validate_interaction_input(span.input, plan.limits.maximum_text_codepoints);
				!status)
				return status;
			if (!add_checked(result.text_codepoints, span.input.text.size(),
							 plan.limits.maximum_text_codepoints))
				return unexpectedError(budget());
			std::uint64_t bytes = sizeof(AgenticInteractionSpan) +
								  span.input.text.size() * sizeof(char32_t) +
								  span.input.keys_down.size() * sizeof(std::uint16_t) +
								  span.input.buttons_down.size();
			if (!add_checked(storage, bytes, plan.limits.maximum_artifact_bytes))
				return unexpectedError(budget());
			result.spans.emplace_back(std::move(span));
			return {};
		};
		if (plan.mode == AgenticInteractionMode::raw_frames) {
			for (const auto& source : plan.spans) {
				if (source.offset != result.frame_count || !source.count ||
					source.minimum_duration_ns || source.target_pointer ||
					source.destination_pointer || source.resolve_before ||
					source.require_hover_before || source.preserve_pointer)
					return unexpectedError(invalid());
				AgenticInteractionSpan span = source;
				span.step_index = no_step;
				if (auto status = emit(std::move(span)); !status)
					return unexpectedError(status.error());
			}
		} else {
			AgenticInputSnapshot state;
			state.pointer_inside = true;
			bool dynamic_pointer = false;
			for (std::size_t step_index = 0; step_index < plan.steps.size(); ++step_index) {
				const auto& step = plan.steps[step_index];
				const bool keyboard_operation =
					step.operation == AgenticInteractionOperation::key_down ||
					step.operation == AgenticInteractionOperation::key_up ||
					step.operation == AgenticInteractionOperation::key_chord;
				if ((!step.keys.empty() && !keyboard_operation) ||
					(!step.text.empty() &&
					 step.operation != AgenticInteractionOperation::text_enter) ||
					(!step.path.empty() && step.operation != AgenticInteractionOperation::drag) ||
					(step.destination && step.operation != AgenticInteractionOperation::drag) ||
					((step.scroll_x != 0 || step.scroll_y != 0) &&
					 step.operation != AgenticInteractionOperation::scroll))
					return unexpectedError(invalid());
				if (step.step_id.size() > 128 || !step.frames ||
					step.frames > plan.limits.maximum_frames || !step.deadline_ns ||
					step.deadline_ns > plan.deadline_ns ||
					step.minimum_duration_ns > step.deadline_ns ||
					step.path.size() > plan.limits.maximum_frames ||
					step.button >= FrameInput::kMouseButtonCount ||
					!finite_coordinate(step.scroll_x) || !finite_coordinate(step.scroll_y) ||
					(static_cast<std::uint32_t>(step.expect) & ~255u))
					return unexpectedError(invalid());
				for (std::size_t prior = 0; prior < step_index; ++prior)
					if (!step.step_id.empty() && step.step_id == plan.steps[prior].step_id)
						return unexpectedError(invalid());
				AgenticInputSnapshot validated_keys;
				validated_keys.keys_down = step.keys;
				validated_keys.text = step.text;
				if (!validate_interaction_input(validated_keys,
												plan.limits.maximum_text_codepoints))
					return unexpectedError(invalid());
				if (step.minimum_duration_ns &&
					step.operation != AgenticInteractionOperation::pointer_hold &&
					step.operation != AgenticInteractionOperation::wait)
					return unexpectedError(invalid());
				if (step.target && !valid_target(*step.target, plan.window_id, capabilities))
					return unexpectedError(invalid());
				if (step.destination &&
					!valid_target(*step.destination, plan.window_id, capabilities))
					return unexpectedError(invalid());
				for (const auto& point : step.path)
					if (!finite_coordinate(point.x) || !finite_coordinate(point.y))
						return unexpectedError(invalid());
				const auto target_flags = static_cast<std::uint32_t>(step.expect) & 15u;
				if (target_flags &&
					(!step.target || step.target->kind == AgenticTargetKind::coordinates ||
					 !capabilities.primary_observations ||
					 ((target_flags & 14u) &&
					  (step.operation == AgenticInteractionOperation::click ? step.recipe.button
																			: step.button) != 0)))
					return unexpectedError(agentic_error(AgenticError::unavailable));
				if ((has_expectation(step.expect, AgenticInteractionExpectation::text_focused) &&
					 !capabilities.text_focus_observations) ||
					(has_expectation(step.expect, AgenticInteractionExpectation::frame_presented) &&
					 !capabilities.presented_observations))
					return unexpectedError(agentic_error(AgenticError::unavailable));
				if (step.expect != AgenticInteractionExpectation::none)
					result.requires_observations = true;
				if (step.target) {
					if (step.target->kind == AgenticTargetKind::coordinates) {
						state.pointer_x = step.target->point.x;
						state.pointer_y = step.target->point.y;
						dynamic_pointer = false;
					} else {
						dynamic_pointer = true;
						result.requires_geometry = true;
					}
				}
				if (step.destination && step.destination->kind != AgenticTargetKind::coordinates)
					result.requires_geometry = true;
				bool first_span = true;
				auto append = [&](std::uint64_t count, bool before_down = false,
								  double progress = 0.0, bool destination = false,
								  std::uint64_t minimum_duration = 0) -> Status {
					if (!count)
						return {};
					AgenticInteractionSpan span;
					span.input = state;
					span.offset = result.frame_count;
					span.count = count;
					span.minimum_duration_ns = minimum_duration;
					span.step_index = static_cast<std::uint32_t>(step_index);
					span.target_pointer =
						step.target && step.target->kind != AgenticTargetKind::coordinates;
					span.destination_pointer = destination;
					span.pointer_progress = progress;
					span.resolve_before =
						first_span && (span.target_pointer ||
									   (step.destination &&
										step.destination->kind != AgenticTargetKind::coordinates));
					span.preserve_pointer = dynamic_pointer && !step.target;
					span.require_hover_before =
						before_down && step.require_hover_before_down && span.target_pointer;
					first_span = false;
					auto status = emit(std::move(span));
					state.text.clear();
					state.scroll_x = 0;
					state.scroll_y = 0;
					return status;
				};
				auto button_down = [&](std::uint8_t button, bool down) {
					auto position = std::lower_bound(state.buttons_down.begin(),
													 state.buttons_down.end(), button);
					if (down && (position == state.buttons_down.end() || *position != button))
						state.buttons_down.insert(position, button);
					else if (!down && position != state.buttons_down.end() && *position == button)
						state.buttons_down.erase(position);
				};
				auto update_keys = [&](bool down) {
					for (const auto key : step.keys) {
						auto position =
							std::lower_bound(state.keys_down.begin(), state.keys_down.end(), key);
						if (down && (position == state.keys_down.end() || *position != key))
							state.keys_down.insert(position, key);
						else if (!down && position != state.keys_down.end() && *position == key)
							state.keys_down.erase(position);
					}
				};
				Status status;
				switch (step.operation) {
				case AgenticInteractionOperation::pointer_move:
					if (!step.target)
						return unexpectedError(invalid());
					status = append(step.frames);
					break;
				case AgenticInteractionOperation::pointer_down:
					if (step.target && step.target->kind != AgenticTargetKind::coordinates &&
						step.require_hover_before_down) {
						if (std::binary_search(state.buttons_down.begin(), state.buttons_down.end(),
											   step.button))
							return unexpectedError(invalid());
						status = append(1);
						if (!status)
							break;
					}
					button_down(step.button, true);
					status = append(step.frames, true);
					break;
				case AgenticInteractionOperation::pointer_up:
					button_down(step.button, false);
					status = append(step.frames);
					break;
				case AgenticInteractionOperation::pointer_hold:
					if (!std::binary_search(state.buttons_down.begin(), state.buttons_down.end(),
											step.button))
						return unexpectedError(invalid());
					status = append(step.frames, false, 0, false, step.minimum_duration_ns);
					break;
				case AgenticInteractionOperation::click: {
					const auto& recipe = step.recipe;
					if (!step.target || recipe.button >= FrameInput::kMouseButtonCount ||
						!recipe.down_frames || !recipe.repeat ||
						recipe.repeat > plan.limits.maximum_frames ||
						(recipe.repeat > 1 && !recipe.up_frames) ||
						(step.target->kind != AgenticTargetKind::coordinates &&
						 step.require_hover_before_down && !recipe.prepare_up_frames) ||
						(!recipe.prepare_up_frames &&
						 std::binary_search(state.buttons_down.begin(), state.buttons_down.end(),
											recipe.button)) ||
						(has_expectation(step.expect, AgenticInteractionExpectation::target_held) &&
						 recipe.down_frames < 2 && !recipe.hold_frames) ||
						(has_expectation(step.expect,
										 AgenticInteractionExpectation::target_released) &&
						 !recipe.up_frames))
						return unexpectedError(invalid());
					for (std::uint64_t repetition = 0; repetition < recipe.repeat; ++repetition) {
						button_down(recipe.button, false);
						status = append(recipe.prepare_up_frames);
						if (!status)
							break;
						button_down(recipe.button, true);
						status = append(recipe.down_frames, true);
						if (!status)
							break;
						status = append(recipe.hold_frames);
						if (!status)
							break;
						if (recipe.up_frames)
							button_down(recipe.button, false);
						status = append(recipe.up_frames);
						if (!status)
							break;
						if (repetition + 1 < recipe.repeat) {
							status = append(recipe.inter_click_gap_frames);
							if (!status)
								break;
						}
					}
					break;
				}
				case AgenticInteractionOperation::drag: {
					if (!step.target || (step.path.empty() == !step.destination))
						return unexpectedError(invalid());
					button_down(step.button, false);
					status = append(1);
					if (!status)
						break;
					button_down(step.button, true);
					status = append(1, true);
					if (!status)
						break;
					const auto start = AgenticInteractionPoint{state.pointer_x, state.pointer_y};
					for (std::uint64_t frame = 0; frame < step.frames; ++frame) {
						const double progress =
							static_cast<double>(frame + 1) / static_cast<double>(step.frames);
						if (!step.path.empty()) {
							const double path_progress =
								progress * static_cast<double>(step.path.size());
							const auto endpoint = std::min<std::size_t>(
								static_cast<std::size_t>(path_progress), step.path.size() - 1);
							const auto previous = endpoint ? step.path[endpoint - 1] : start;
							const double fraction =
								endpoint + 1 == step.path.size() && progress == 1
									? 1
									: path_progress - static_cast<double>(endpoint);
							state.pointer_x =
								previous.x + (step.path[endpoint].x - previous.x) * fraction;
							state.pointer_y =
								previous.y + (step.path[endpoint].y - previous.y) * fraction;
						} else if (step.target->kind == AgenticTargetKind::coordinates &&
								   step.destination->kind == AgenticTargetKind::coordinates) {
							state.pointer_x =
								start.x + (step.destination->point.x - start.x) * progress;
							state.pointer_y =
								start.y + (step.destination->point.y - start.y) * progress;
						}
						status = append(1, false, progress, bool(step.destination));
						if (!status)
							break;
						if (!step.path.empty())
							result.spans.back().target_pointer = false;
					}
					if (!status)
						break;
					button_down(step.button, false);
					status = append(1, false, 1, bool(step.destination));
					if (!step.path.empty()) {
						result.spans.back().target_pointer = false;
						dynamic_pointer = false;
					}
					break;
				}
				case AgenticInteractionOperation::scroll:
					state.scroll_x = step.scroll_x;
					state.scroll_y = step.scroll_y;
					status = append(step.frames);
					break;
				case AgenticInteractionOperation::key_down:
				case AgenticInteractionOperation::key_up:
					if (step.keys.empty())
						return unexpectedError(invalid());
					update_keys(step.operation == AgenticInteractionOperation::key_down);
					status = append(step.frames);
					break;
				case AgenticInteractionOperation::key_chord:
					if (step.keys.empty())
						return unexpectedError(invalid());
					update_keys(false);
					status = append(1);
					if (!status)
						break;
					update_keys(true);
					status = append(step.frames);
					if (!status)
						break;
					update_keys(false);
					status = append(1);
					break;
				case AgenticInteractionOperation::text_enter:
					if (step.text.empty())
						return unexpectedError(invalid());
					state.text = step.text;
					status = append(step.frames);
					break;
				case AgenticInteractionOperation::wait:
				case AgenticInteractionOperation::assert_observation:
					if (step.operation == AgenticInteractionOperation::assert_observation &&
						step.expect == AgenticInteractionExpectation::none)
						return unexpectedError(invalid());
					status = append(step.frames, false, 0, false, step.minimum_duration_ns);
					break;
				default:
					return unexpectedError(invalid());
				}
				if (!status)
					return unexpectedError(status.error());
			}
		}
		if (!result.frame_count || (result.requires_observations && !plan.record_observations))
			return unexpectedError(invalid());
		if (result.text_codepoints && !plan.persistence_declared)
			return unexpectedError(invalid());
		if (plan.clock == AgenticInteractionClock::exact_window_frames &&
			result.frame_count - 1 >
				std::numeric_limits<std::uint64_t>::max() - plan.first_window_frame)
			return unexpectedError(invalid());
		result.cleanup_required = !result.spans.back().input.keys_down.empty() ||
								  !result.spans.back().input.buttons_down.empty();
		result.replayable_when_retained =
			!result.text_codepoints ||
			plan.persistence == AgenticInteractionPersistence::retain_text;
		result.plan = plan;
		for (std::size_t index = 0; index < result.plan.steps.size(); ++index) {
			auto& step = result.plan.steps[index];
			if (step.step_id.empty())
				step.step_id = "step_" + std::to_string(index);
			if (step.target && !step.target->window_id)
				step.target->window_id = plan.window_id;
			if (step.destination && !step.destination->window_id)
				step.destination->window_id = plan.window_id;
		}
		result.limitations = {"one_window_per_plan", "dt_from_application_clock",
							  "synthetic_focus_is_not_os_focus",
							  "input_delivery_does_not_prove_application_completion",
							  "primary_membership_is_not_exclusive_consumption"};
		if (plan.baseline == AgenticInteractionBaseline::neutral_before_start)
			result.limitations.emplace_back("neutral_baseline_outside_requested_offsets");
		if (!result.replayable_when_retained)
			result.limitations.emplace_back("retained_redacted_text_is_not_replayable");
		if (plan.persistence == AgenticInteractionPersistence::volatile_plan)
			result.limitations.emplace_back("volatile_payloads_are_not_persisted");
		const auto canonical = write_json(to_json(result.plan, true));
		if (!add_checked(storage, canonical.size(), plan.limits.maximum_artifact_bytes))
			return unexpectedError(budget());
		// Reserve independent input and observation evidence, including variable held-state
		// payloads.
		std::uint64_t estimated_evidence = 0;
		for (const auto& span : result.spans) {
			std::uint64_t per_frame =
				768 + span.input.keys_down.size() * 6 + span.input.buttons_down.size() * 3;
			if (plan.include_native_input)
				per_frame += 4096;
			if (plan.record_observations)
				per_frame += 1024;
			if (span.count > plan.limits.maximum_artifact_bytes / per_frame ||
				!add_checked(estimated_evidence, span.count * per_frame,
							 plan.limits.maximum_artifact_bytes) ||
				!add_checked(estimated_evidence, span.input.text.size() * 12,
							 plan.limits.maximum_artifact_bytes))
				return unexpectedError(budget());
		}
		if (plan.record_observations && result.frame_count > plan.limits.maximum_observations)
			return unexpectedError(budget());
		if (!add_checked(storage, estimated_evidence, plan.limits.maximum_artifact_bytes))
			return unexpectedError(budget());
		result.estimated_bytes = storage;
		auto digest = interaction_plan_digest(result.plan);
		if (!digest)
			return unexpectedError(digest.error());
		result.digest = std::move(*digest);
		return result;
	} catch (...) {
		return unexpectedError(makeError(ErrorCode::AllocationFailed, ErrorSite::AgenticCommand));
	}
}

Result<std::string> interaction_plan_digest(const AgenticInteractionPlan& plan) noexcept {
	try {
		auto identity = to_json(plan, true);
		if (plan.clock == AgenticInteractionClock::window_input_frames)
			identity.set("run_id", "");
		identity.set("idempotency_key", "");
		return sha256(write_json(identity));
	} catch (...) {
		return unexpectedError(makeError(ErrorCode::AllocationFailed, ErrorSite::AgenticCommand));
	}
}

Result<AgenticInputSnapshot> interaction_input_at(const AgenticCompiledInteraction& plan,
												  std::uint64_t offset) noexcept {
	try {
		if (offset >= plan.frame_count)
			return unexpectedError(invalid());
		const auto position =
			std::upper_bound(plan.spans.begin(), plan.spans.end(), offset,
							 [](std::uint64_t value, const AgenticInteractionSpan& span) {
								 return value < span.offset;
							 });
		if (position == plan.spans.begin())
			return unexpectedError(invalid());
		const auto& span = *std::prev(position);
		if (offset - span.offset >= span.count)
			return unexpectedError(invalid());
		AgenticInputSnapshot result = span.input;
		if (offset != span.offset) {
			result.text.clear();
			result.scroll_x = 0;
			result.scroll_y = 0;
		}
		return result;
	} catch (...) {
		return unexpectedError(makeError(ErrorCode::AllocationFailed, ErrorSite::AgenticCommand));
	}
}

Result<FrameInput> materialize_interaction_input(const AgenticInputSnapshot& snapshot,
												 const FrameInput& native_input) noexcept {
	try {
		if (auto status = validate_interaction_input(snapshot); !status)
			return unexpectedError(status.error());
		FrameInput result;
		result.dt = native_input.dt;
		result.droppedTextInputCount = native_input.droppedTextInputCount;
		result.mouseX = static_cast<float>(snapshot.pointer_x);
		result.mouseY = static_cast<float>(snapshot.pointer_y);
		result.scrollX = static_cast<float>(snapshot.scroll_x);
		result.scrollY = static_cast<float>(snapshot.scroll_y);
		result.pointerInside = snapshot.pointer_inside;
		result.windowFocused = snapshot.window_focused;
		for (const auto key : snapshot.keys_down)
			result.keyDown[key] = true;
		for (const auto button : snapshot.buttons_down)
			result.mouseDown[button] = true;
		result.shift = result.keyDown[FlowUi::keyCode(FlowUi::Key::LeftShift)] ||
					   result.keyDown[FlowUi::keyCode(FlowUi::Key::RightShift)];
		result.ctrl = result.keyDown[FlowUi::keyCode(FlowUi::Key::LeftControl)] ||
					  result.keyDown[FlowUi::keyCode(FlowUi::Key::RightControl)];
		result.alt = result.keyDown[FlowUi::keyCode(FlowUi::Key::LeftAlt)] ||
					 result.keyDown[FlowUi::keyCode(FlowUi::Key::RightAlt)];
		result.super = result.keyDown[FlowUi::keyCode(FlowUi::Key::LeftSuper)] ||
					   result.keyDown[FlowUi::keyCode(FlowUi::Key::RightSuper)];
		result.text = snapshot.text;
		return result;
	} catch (...) {
		return unexpectedError(makeError(ErrorCode::AllocationFailed, ErrorSite::AgenticCommand));
	}
}

Result<AgenticInputSnapshot> snapshot_interaction_input(const FrameInput& input) noexcept {
	try {
		AgenticInputSnapshot result;
		result.pointer_x = input.mouseX;
		result.pointer_y = input.mouseY;
		result.scroll_x = input.scrollX;
		result.scroll_y = input.scrollY;
		result.pointer_inside = input.pointerInside;
		result.window_focused = input.windowFocused;
		result.text = input.text;
		result.keys_down.reserve(128);
		result.buttons_down.reserve(FrameInput::kMouseButtonCount);
		for (std::size_t key = 0; key < input.keyDown.size(); ++key)
			if (input.keyDown[key])
				result.keys_down.emplace_back(static_cast<std::uint16_t>(key));
		for (std::size_t button = 0; button < input.mouseDown.size(); ++button)
			if (input.mouseDown[button])
				result.buttons_down.emplace_back(static_cast<std::uint8_t>(button));
		if (auto status = validate_interaction_input(result); !status)
			return unexpectedError(status.error());
		return result;
	} catch (...) {
		return unexpectedError(makeError(ErrorCode::AllocationFailed, ErrorSite::AgenticCommand));
	}
}

AgenticInteractionExpectation
observed_interaction_facts(const AgenticInteractionObservation& observation,
						   std::uint64_t target_clay_id) noexcept {
	AgenticInteractionExpectation result = AgenticInteractionExpectation::none;
	if (!observation.layout_completed || observation.canceled)
		return result;
	result = result | AgenticInteractionExpectation::layout_completed;
	if (observation.presented)
		result = result | AgenticInteractionExpectation::frame_presented;
	if (observation.window_focused)
		result = result | AgenticInteractionExpectation::window_focused;
	if (observation.text_focus_supported && observation.text_focused)
		result = result | AgenticInteractionExpectation::text_focused;
	if (target_clay_id) {
		for (const auto& [values, flag] :
			 {std::pair{&observation.hovered, AgenticInteractionExpectation::target_hovered},
			  {&observation.pressed, AgenticInteractionExpectation::target_pressed},
			  {&observation.held, AgenticInteractionExpectation::target_held},
			  {&observation.released, AgenticInteractionExpectation::target_released}})
			if (std::find(values->begin(), values->end(), target_clay_id) != values->end())
				result = result | flag;
	}
	return result;
}

std::string_view name(AgenticInteractionOperation value) noexcept {
	switch (value) {
	case AgenticInteractionOperation::pointer_move:
		return "pointer.move";
	case AgenticInteractionOperation::pointer_down:
		return "pointer.down";
	case AgenticInteractionOperation::pointer_up:
		return "pointer.up";
	case AgenticInteractionOperation::pointer_hold:
		return "pointer.hold";
	case AgenticInteractionOperation::click:
		return "click";
	case AgenticInteractionOperation::drag:
		return "drag";
	case AgenticInteractionOperation::scroll:
		return "scroll";
	case AgenticInteractionOperation::key_down:
		return "key.down";
	case AgenticInteractionOperation::key_up:
		return "key.up";
	case AgenticInteractionOperation::key_chord:
		return "key.chord";
	case AgenticInteractionOperation::text_enter:
		return "text.enter";
	case AgenticInteractionOperation::wait:
		return "wait";
	case AgenticInteractionOperation::assert_observation:
		return "assert";
	}
	return "invalid";
}
std::string_view name(AgenticInteractionPhase value) noexcept {
	switch (value) {
	case AgenticInteractionPhase::queued:
		return "queued";
	case AgenticInteractionPhase::validating:
		return "validating";
	case AgenticInteractionPhase::acquiring_control:
		return "acquiring_control";
	case AgenticInteractionPhase::synchronizing:
		return "synchronizing";
	case AgenticInteractionPhase::executing:
		return "executing";
	case AgenticInteractionPhase::draining_observations:
		return "draining_observations";
	case AgenticInteractionPhase::releasing_control:
		return "releasing_control";
	case AgenticInteractionPhase::completed:
		return "completed";
	case AgenticInteractionPhase::canceled:
		return "canceled";
	case AgenticInteractionPhase::failed:
		return "failed";
	}
	return "invalid";
}
std::string_view name(AgenticInteractionVerdict value) noexcept {
	switch (value) {
	case AgenticInteractionVerdict::pending:
		return "pending";
	case AgenticInteractionVerdict::passed:
		return "passed";
	case AgenticInteractionVerdict::failed:
		return "failed";
	case AgenticInteractionVerdict::unavailable:
		return "unavailable";
	}
	return "invalid";
}

Json to_json(const AgenticInputSnapshot& value, bool include_text) {
	Json keys = Json::make_array(), buttons = Json::make_array();
	keys.array.reserve(value.keys_down.size());
	buttons.array.reserve(value.buttons_down.size());
	for (const auto key : value.keys_down)
		keys.array.emplace_back(Json::number(key));
	for (const auto button : value.buttons_down)
		buttons.array.emplace_back(Json::number(button));
	Json result = Json::make_object(
		{{"pointer",
		  Json::make_object({{"x", Json(value.pointer_x)}, {"y", Json(value.pointer_y)}})},
		 {"scroll", Json::make_object({{"x", Json(value.scroll_x)}, {"y", Json(value.scroll_y)}})},
		 {"pointer_inside", value.pointer_inside},
		 {"window_focused", value.window_focused},
		 {"buttons_down", std::move(buttons)},
		 {"keys_down", std::move(keys)}});
	if (include_text)
		result.set("text", encode_utf8(value.text));
	else if (!value.text.empty()) {
		result.set("text_redacted", true);
		result.set("text_codepoints", decimal(value.text.size()));
	}
	return result;
}
Json to_json(const AgenticInteractionTarget& value) {
	Json result = Json::make_object({{"window_id", decimal(value.window_id)},
									 {"anchor", Json::make_object({{"x", Json(value.anchor_x)},
																   {"y", Json(value.anchor_y)}})}});
	switch (value.kind) {
	case AgenticTargetKind::coordinates:
		result.set("point",
				   Json::make_object({{"x", Json(value.point.x)}, {"y", Json(value.point.y)}}));
		break;
	case AgenticTargetKind::clay_id:
		result.set("clay_id", decimal(value.clay_id));
		break;
	case AgenticTargetKind::clay_name:
		result.set("clay_name", value.clay_name);
		break;
	case AgenticTargetKind::flow_instance:
		result.set("flow_instance_id", decimal(value.flow_instance_id));
		break;
	case AgenticTargetKind::flow_debug:
		result.set("flow_debug_name", value.flow_debug_name);
		break;
	case AgenticTargetKind::custom_region:
		result.set("custom_id", value.custom_id);
		break;
	}
	if (value.tree_generation)
		result.set("tree_generation", decimal(*value.tree_generation));
	if (value.layout_revision)
		result.set("layout_revision", decimal(*value.layout_revision));
	if (value.custom_revision)
		result.set("custom_revision", decimal(*value.custom_revision));
	return result;
}
Json to_json(const AgenticResolvedInteractionTarget& value) {
	return Json::make_object(
		{{"selector", to_json(value.selector)},
		 {"label", value.label},
		 {"bounds", region_json(value.bounds)},
		 {"visible_region", region_json(value.visible_region)},
		 {"layout_point", Json::make_object({{"x", Json(value.layout_point.x)},
											 {"y", Json(value.layout_point.y)}})},
		 {"window_point", Json::make_object({{"x", Json(value.window_point.x)},
											 {"y", Json(value.window_point.y)}})},
		 {"matching_clay_ids", ids_json(value.matching_clay_ids)},
		 {"tree_generation", decimal(value.tree_generation)},
		 {"layout_revision", decimal(value.layout_revision)},
		 {"clay_id", decimal(value.clay_id)},
		 {"app_tick", decimal(value.app_tick)},
		 {"window_frame", decimal(value.window_frame)},
		 {"flow_instance_id", decimal(value.flow_instance_id)},
		 {"custom_revision", decimal(value.custom_revision)},
		 {"paint_order", Json::number(value.paint_order)},
		 {"z_index", Json(static_cast<double>(value.z_index))},
		 {"floating", value.floating},
		 {"layout_to_window_x", Json(value.layout_to_window_x)},
		 {"layout_to_window_y", Json(value.layout_to_window_y)},
		 {"hit_test_supported", value.hit_test_supported},
		 {"hit_test_verified", value.hit_test_verified}});
}
Json to_json(const AgenticInteractionPlan& value, bool include_text) {
	Json result = Json::make_object(
		{{"schema", "FlowUi.Agentic.InteractionPlan"},
		 {"schema_version", "1.0"},
		 {"mode",
		  value.mode == AgenticInteractionMode::raw_frames ? "raw_frames" : "common_controls"},
		 {"window_id", decimal(value.window_id)},
		 {"run_id", value.run_id},
		 {"first_window_frame", decimal(value.first_window_frame)},
		 {"clock", value.clock == AgenticInteractionClock::window_input_frames
					   ? "window_input_frames"
					   : "exact_window_frames"},
		 {"coordinates", "window_logical"},
		 {"baseline", value.baseline == AgenticInteractionBaseline::neutral_before_start
						  ? "neutral_before_start"
						  : "require_neutral"},
		 {"fixture_id", value.fixture_id},
		 {"fixture_digest", value.fixture_digest},
		 {"seed", decimal(value.seed)},
		 {"deadline_ns", decimal(value.deadline_ns)},
		 {"idempotency_key", value.idempotency_key},
		 {"limits", limits_json(value.limits)}});
	Json content = Json::make_object({{"include_native_input", value.include_native_input},
									  {"record_observations", value.record_observations}});
	if (value.persistence_declared)
		content.set("persistence",
					value.persistence == AgenticInteractionPersistence::retain_text ? "retain_text"
					: value.persistence == AgenticInteractionPersistence::volatile_plan
						? "volatile"
						: "redact_text");
	result.set("content", std::move(content));
	if (value.mode == AgenticInteractionMode::raw_frames) {
		Json spans = Json::make_array();
		spans.array.reserve(value.spans.size());
		for (const auto& span : value.spans)
			spans.array.emplace_back(span_json(span, include_text, false));
		result.set("spans", std::move(spans));
	} else {
		Json steps = Json::make_array();
		steps.array.reserve(value.steps.size());
		for (const auto& step : value.steps) {
			Json keys = Json::make_array(), path = Json::make_array();
			for (const auto key : step.keys)
				keys.array.emplace_back(Json::number(key));
			for (const auto point : step.path)
				path.array.emplace_back(
					Json::make_object({{"x", Json(point.x)}, {"y", Json(point.y)}}));
			Json row = Json::make_object(
				{{"step_id", step.step_id},
				 {"operation", name(step.operation)},
				 {"frames", decimal(step.frames)},
				 {"minimum_duration_ns", decimal(step.minimum_duration_ns)},
				 {"deadline_ns", decimal(step.deadline_ns)},
				 {"button", Json::number(step.button)},
				 {"keys", std::move(keys)},
				 {"path", std::move(path)},
				 {"expect", expectations_json(step.expect)},
				 {"require_hover_before_down", step.require_hover_before_down},
				 {"scroll",
				  Json::make_object({{"x", Json(step.scroll_x)}, {"y", Json(step.scroll_y)}})},
				 {"recipe",
				  Json::make_object(
					  {{"button", Json::number(step.recipe.button)},
					   {"prepare_up_frames", decimal(step.recipe.prepare_up_frames)},
					   {"down_frames", decimal(step.recipe.down_frames)},
					   {"hold_frames", decimal(step.recipe.hold_frames)},
					   {"up_frames", decimal(step.recipe.up_frames)},
					   {"repeat", decimal(step.recipe.repeat)},
					   {"inter_click_gap_frames", decimal(step.recipe.inter_click_gap_frames)}})}});
			if (step.target)
				row.set("target", to_json(*step.target));
			if (step.destination)
				row.set("destination", to_json(*step.destination));
			if (include_text)
				row.set("text", encode_utf8(step.text));
			else if (!step.text.empty()) {
				row.set("text_redacted", true);
				row.set("text_codepoints", decimal(step.text.size()));
			}
			steps.array.emplace_back(std::move(row));
		}
		result.set("steps", std::move(steps));
	}
	return result;
}
Json to_json(const AgenticCompiledInteraction& value, bool include_text) {
	Json result = to_json(value.plan, include_text);
	Json spans = Json::make_array(), limitations = Json::make_array();
	spans.array.reserve(value.spans.size());
	for (const auto& span : value.spans)
		spans.array.emplace_back(span_json(span, include_text, true));
	for (const auto& reason : value.limitations)
		limitations.array.emplace_back(reason);
	result.set("compiled",
			   Json::make_object({{"recipe_version", "1.0"},
								  {"digest", value.digest},
								  {"frame_count", decimal(value.frame_count)},
								  {"estimated_bytes", decimal(value.estimated_bytes)},
								  {"text_codepoints", decimal(value.text_codepoints)},
								  {"requires_geometry", value.requires_geometry},
								  {"requires_observations", value.requires_observations},
								  {"cleanup_required", value.cleanup_required},
								  {"replayable_when_retained", value.replayable_when_retained},
								  {"limitations", std::move(limitations)},
								  {"spans", std::move(spans)}}));
	return result;
}
Json to_json(const AgenticInteractionCapabilities& value) {
	return Json::make_object({{"schema", "FlowUi.Agentic.InteractionCapabilities"},
							  {"schema_version", "1.0"},
							  {"limits", limits_json(value.limits)},
							  {"raw_frames", value.raw_frames},
							  {"common_controls", value.common_controls},
							  {"geometry_targets", value.geometry_targets},
							  {"custom_targets", value.custom_targets},
							  {"primary_observations", value.primary_observations},
							  {"text_focus_observations", value.text_focus_observations},
							  {"presented_observations", value.presented_observations},
							  {"recording", value.recording},
							  {"exact_addresses", value.exact_addresses},
							  {"maximum_windows_per_plan", Json::number(1)},
							  {"key_mapping", "GLFW_3_key_codes"},
							  {"dt_policy", "application_clock"},
							  {"offset_zero", "first_eligible_input_selection_after_baseline"}});
}
Json to_json(const AgenticInteractionReceipt& value, bool include_text) {
	Json result = Json::make_object(
		{{"schema", "FlowUi.Agentic.InputFrame"},
		 {"schema_version", "1.0"},
		 {"plan_digest", value.plan_digest},
		 {"job_id", value.job_id},
		 {"run_id", value.run_id},
		 {"sequence", decimal(value.sequence)},
		 {"offset", decimal(value.offset)},
		 {"selected_ns", decimal(value.selected_ns)},
		 {"step_index", Json::number(value.step_index)},
		 {"selected", value.selected},
		 {"missed", value.missed},
		 {"cleanup", value.cleanup},
		 {"baseline", value.baseline},
		 {"reason", value.reason},
		 {"effective_input", to_json(value.effective_input, include_text)},
		 {"dt_policy", "application_clock"},
		 {"synthetic_focus", value.synthetic_focus},
		 {"text_replayable", include_text || value.effective_input.text.empty()}});
	if (value.frame)
		result.set("frame", frame_key_json(*value.frame));
	if (value.native_input)
		result.set("native_input", to_json(*value.native_input, include_text));
	return result;
}
Json to_json(const AgenticInteractionObservation& value) {
	Json result = Json::make_object({{"schema", "FlowUi.Agentic.InteractionObservation"},
									 {"schema_version", "1.0"},
									 {"plan_digest", value.plan_digest},
									 {"job_id", value.job_id},
									 {"run_id", value.run_id},
									 {"sequence", decimal(value.sequence)},
									 {"offset", decimal(value.offset)},
									 {"observed_ns", decimal(value.observed_ns)},
									 {"step_index", Json::number(value.step_index)},
									 {"hovered", ids_json(value.hovered)},
									 {"pressed", ids_json(value.pressed)},
									 {"held", ids_json(value.held)},
									 {"released", ids_json(value.released)},
									 {"layout_completed", value.layout_completed},
									 {"presented", value.presented},
									 {"canceled", value.canceled},
									 {"input_suppressed", value.input_suppressed},
									 {"window_focused", value.window_focused},
									 {"text_focus_supported", value.text_focus_supported},
									 {"text_focused", value.text_focused},
									 {"reason", value.reason}});
	if (value.frame)
		result.set("frame", frame_key_json(*value.frame));
	if (value.target)
		result.set("target", to_json(*value.target));
	if (value.target_observation)
		result.set("target_observation", *value.target_observation);
	return result;
}
Json to_json(const AgenticInteractionResult& value) {
	Json steps = Json::make_array(), limitations = Json::make_array();
	for (const auto& step : value.steps)
		steps.array.emplace_back(
			Json::make_object({{"step_id", step.step_id},
							   {"expected", expectations_json(step.expected)},
							   {"observed", expectations_json(step.observed)},
							   {"first_offset", decimal(step.first_offset)},
							   {"end_offset_exclusive", decimal(step.end_offset_exclusive)},
							   {"verdict", name(step.verdict)},
							   {"reason", step.reason}}));
	for (const auto& reason : value.limitations)
		limitations.array.emplace_back(reason);
	const auto& coverage = value.coverage;
	Json result = Json::make_object(
		{{"schema", "FlowUi.Agentic.InteractionResult"},
		 {"schema_version", "1.0"},
		 {"plan_digest", value.plan_digest},
		 {"job_id", value.job_id},
		 {"run_id", value.run_id},
		 {"window_id", decimal(value.window_id)},
		 {"phase", name(value.phase)},
		 {"assertion_verdict", name(value.assertion_verdict)},
		 {"execution_succeeded", value.execution_succeeded},
		 {"evidence_complete", value.evidence_complete},
		 {"cleanup_complete", value.cleanup_complete},
		 {"handoff_complete", value.handoff_complete},
		 {"reason", value.reason},
		 {"steps", std::move(steps)},
		 {"limitations", std::move(limitations)},
		 {"coverage",
		  Json::make_object({{"scheduled", decimal(coverage.scheduled)},
							 {"consumed", decimal(coverage.consumed)},
							 {"missed", decimal(coverage.missed)},
							 {"canceled", decimal(coverage.canceled)},
							 {"observed", decimal(coverage.observed)},
							 {"asserted", decimal(coverage.asserted)},
							 {"retained_inputs", decimal(coverage.retained_inputs)},
							 {"retained_observations", decimal(coverage.retained_observations)},
							 {"lost", decimal(coverage.lost)}})}});
	if (value.activation_frame)
		result.set("activation_frame", frame_key_json(*value.activation_frame));
	if (value.first_divergence_offset)
		result.set("first_divergence_offset", decimal(*value.first_divergence_offset));
	return result;
}
} // namespace FlowUi::devSystems::agentic
#endif
