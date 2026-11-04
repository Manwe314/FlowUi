#include "devSystems/devAgenticWorkflow/AgenticInteraction.hpp"
#include "devSystems/devAgenticWorkflow/AgenticManifest.hpp"

#include <cmath>
#include <cstdlib>
#include <iostream>
#include <limits>

using namespace FlowUi;
using namespace FlowUi::devSystems::agentic;
namespace {
void require(bool condition, std::string_view message) {
	if (!condition) {
		std::cerr << message << '\n';
		std::exit(1);
	}
}
[[nodiscard]] AgenticInteractionPlan raw_plan() {
	AgenticInteractionPlan result;
	result.window_id = 1;
	AgenticInteractionSpan span;
	span.input.pointer_x = 12;
	span.input.pointer_y = 18;
	span.input.pointer_inside = true;
	span.count = 3;
	result.spans.emplace_back(std::move(span));
	return result;
}
[[nodiscard]] AgenticInteractionTarget point(double x = 12, double y = 18) {
	AgenticInteractionTarget target;
	target.point = {x, y};
	return target;
}
[[nodiscard]] AgenticInteractionPlan controls(AgenticInteractionStep step) {
	AgenticInteractionPlan result;
	result.window_id = 1;
	result.mode = AgenticInteractionMode::common_controls;
	result.steps.emplace_back(std::move(step));
	return result;
}
void raw_contracts() {
	auto json = parse_json(
		R"({"schema":"FlowUi.Agentic.InteractionPlan","schema_version":"1.0","mode":"raw_frames","window_id":"1","clock":"window_input_frames","coordinates":"window_logical","baseline":"neutral_before_start","frames":[{"offset":"0","pointer":{"x":240,"y":120},"pointer_inside":true},{"offset":"1","pointer":{"x":240,"y":120},"pointer_inside":true,"buttons_down":[0]},{"offset":"2","pointer":{"x":240,"y":120},"pointer_inside":true,"buttons_down":[]}]})");
	require(bool(json), "raw example JSON failed");
	auto decoded = interaction_plan_from_json(*json);
	require(bool(decoded), "report raw example did not decode");
	auto compiled = compile_interaction_plan(*decoded);
	require(bool(compiled) && compiled->frame_count == 3, "raw frame count changed");
	require(compiled->spans.front().offset == 0 &&
				compiled->plan.baseline == AgenticInteractionBaseline::neutral_before_start,
			"baseline secretly shifted requested offset zero");
	require(interaction_input_at(*compiled, 0)->buttons_down.empty(),
			"neutral frame inherited button");
	require(interaction_input_at(*compiled, 1)->buttons_down == std::vector<std::uint8_t>{0},
			"down edge lost");
	require(interaction_input_at(*compiled, 2)->buttons_down.empty(), "up edge lost");
	require(!interaction_input_at(*compiled, 3), "out-of-range row resolved");
	auto again = compile_interaction_plan(*interaction_plan_from_json(to_json(compiled->plan)));
	require(bool(again) && again->digest == compiled->digest,
			"canonical plan digest did not round-trip");
	auto rebound = compiled->plan;
	rebound.run_id = "run_first";
	rebound.idempotency_key = "first-request";
	const auto first_run = compile_interaction_plan(rebound);
	rebound.run_id = "run_second";
	rebound.idempotency_key = "second-request";
	const auto second_run = compile_interaction_plan(rebound);
	require(first_run && second_run && first_run->digest == second_run->digest,
			"relative replay workload identity depends on ephemeral run or retry identity");
	auto redacted = raw_plan();
	redacted.persistence_declared = true;
	redacted.spans.front().input.text = {U'a'};
	const auto first_text = interaction_plan_digest(redacted);
	redacted.spans.front().input.text = {U'b'};
	const auto second_text = interaction_plan_digest(redacted);
	require(first_text && second_text && *first_text != *second_text,
			"redacted recordings with different original text have the same workload identity");
	require(!interaction_plan_from_json(to_json(*compiled)),
			"planning output silently executed as source request");
	auto gap = raw_plan();
	gap.spans.front().offset = 1;
	require(!compile_interaction_plan(gap), "initial schedule gap accepted");
	auto duplicate = raw_plan();
	duplicate.spans.emplace_back(duplicate.spans.front());
	require(!compile_interaction_plan(duplicate), "duplicate schedule row accepted");
	auto overflow = raw_plan();
	overflow.spans.front().count = std::numeric_limits<std::uint64_t>::max();
	require(!compile_interaction_plan(overflow), "overflowed span accepted");
	auto neutral =
		interaction_plan_from_json(*parse_json(R"({"window_id":"1","frames":[{"offset":"0"}]})"));
	require(bool(neutral) && neutral->spans.front().input.keys_down.empty() &&
				neutral->spans.front().input.buttons_down.empty() &&
				neutral->spans.front().input.text.empty(),
			"omitted state inherited native payload");
	require(!interaction_plan_from_json(*parse_json(
				R"({"window_id":"1","coordinates":"framebuffer","frames":[{"offset":"0"}]})")),
			"framebuffer coordinates accepted");
	require(!interaction_plan_from_json(
				*parse_json(R"({"window_id":"1","frames":[{"offset":"0","dt":0.1}]})")),
			"App dt override accepted");
	require(!interaction_plan_from_json(
				*parse_json(R"({"window_id":"1","frames":[{"offset":"0","keys_down":[340,65]}]})")),
			"unsorted wire keys accepted");
	require(!interaction_plan_from_json(
				*parse_json(R"({"window_id":"1","frames":[{"offset":"0","buttons_down":[0,0]}]})")),
			"duplicate wire buttons accepted");
	require(!interaction_plan_from_json(
				*parse_json(R"({"window_id":"1","frames":[{"offset":"0","keys_down":[1]}]})")),
			"unknown backend key slot accepted");
	require(!interaction_plan_from_json(*parse_json(
				R"({"window_id":"1","frames":[{"offset":"0","pointer":{"x":1e100}}]})")),
			"unrepresentable coordinate accepted");
}
void transient_and_materialization_contracts() {
	auto source = raw_plan();
	source.persistence_declared = true;
	source.persistence = AgenticInteractionPersistence::retain_text;
	source.spans.front().input.text = {U'\0', U'文', U'😀'};
	source.spans.front().input.scroll_y = -2;
	source.spans.front().input.keys_down = {65, 340, 345, 346, 347};
	auto compiled = compile_interaction_plan(source);
	require(bool(compiled), "Unicode span rejected");
	auto first = interaction_input_at(*compiled, 0);
	auto repeated = interaction_input_at(*compiled, 1);
	require(bool(first) && first->text.size() == 3 && first->scroll_y == -2,
			"first transient payload missing");
	require(bool(repeated) && repeated->text.empty() && repeated->scroll_y == 0 &&
				repeated->keys_down == first->keys_down,
			"held span repeated text/scroll or dropped held keys");
	FrameInput native;
	native.dt = 0.123;
	native.droppedTextInputCount = 17;
	native.mouseDown[4] = true;
	native.keyDown[66] = true;
	native.text = {U'n'};
	native.scrollY = 9;
	auto effective = materialize_interaction_input(*first, native);
	require(bool(effective) && effective->dt == native.dt && effective->droppedTextInputCount == 17,
			"synthetic state overwrote real clock/loss facts");
	require(!effective->mouseDown[4] && !effective->keyDown[66] && effective->text == first->text &&
				effective->scrollY == -2,
			"physical payload leaked through complete replacement");
	require(effective->shift && effective->ctrl && effective->alt && effective->super,
			"modifiers not derived from effective keys");
	auto snapshot = snapshot_interaction_input(*effective);
	require(bool(snapshot) && write_json(to_json(*snapshot)) == write_json(to_json(*first)),
			"materialized snapshot round-trip changed state");
	auto roundtrip = interaction_plan_from_json(to_json(compiled->plan));
	require(bool(roundtrip) && roundtrip->spans.front().input.text == first->text,
			"Unicode wire round-trip corrupted codepoints");
	auto invalid_scalar = source;
	invalid_scalar.spans.front().input.text = {static_cast<char32_t>(0xd800)};
	require(!compile_interaction_plan(invalid_scalar), "surrogate scalar accepted");
	auto infinite = source;
	infinite.spans.front().input.pointer_x = std::numeric_limits<double>::infinity();
	require(!compile_interaction_plan(infinite), "nonfinite typed coordinate accepted");
	auto undeclared = source;
	undeclared.persistence_declared = false;
	require(!compile_interaction_plan(undeclared),
			"text plan did not require declared persistence");
	auto redacted = source;
	redacted.persistence = AgenticInteractionPersistence::redact_text;
	auto redacted_compiled = compile_interaction_plan(redacted);
	require(bool(redacted_compiled) && !redacted_compiled->replayable_when_retained &&
				!interaction_plan_from_json(to_json(redacted_compiled->plan, false)),
			"redacted source silently replayable");
	auto receipt = AgenticInteractionReceipt{};
	receipt.effective_input = *first;
	const auto retained = to_json(receipt);
	require(retained.find("effective_input")->find("text_redacted") && !retained.find("frame"),
			"default receipt leaked text or invented frame identity");
}
void mechanical_contracts() {
	AgenticInteractionStep click;
	click.operation = AgenticInteractionOperation::click;
	click.target = point();
	click.recipe.hold_frames = 8;
	auto compiled = compile_interaction_plan(controls(click));
	require(bool(compiled) && compiled->frame_count == 11 && !compiled->cleanup_required,
			"click/hold recipe frame count wrong");
	require(interaction_input_at(*compiled, 0)->buttons_down.empty() &&
				interaction_input_at(*compiled, 1)->buttons_down.size() == 1 &&
				interaction_input_at(*compiled, 9)->buttons_down.size() == 1 &&
				interaction_input_at(*compiled, 10)->buttons_down.empty(),
			"click down/hold/up states collapsed");
	click.recipe.hold_frames = 0;
	click.recipe.repeat = 2;
	click.recipe.inter_click_gap_frames = 2;
	compiled = compile_interaction_plan(controls(click));
	require(bool(compiled) && compiled->frame_count == 8, "repeated click gap count wrong");
	click.recipe.repeat = 1;
	click.recipe.prepare_up_frames = 0;
	click.recipe.up_frames = 0;
	compiled = compile_interaction_plan(controls(click));
	require(bool(compiled) && compiled->frame_count == 1 && compiled->cleanup_required,
			"down-only click did not require cleanup");
	click.recipe.repeat = 2;
	require(!compile_interaction_plan(controls(click)), "repeated click without release accepted");
	click.recipe.repeat = 1;
	click.recipe.down_frames = 0;
	require(!compile_interaction_plan(controls(click)), "zero down click accepted");
	AgenticInteractionStep drag;
	drag.operation = AgenticInteractionOperation::drag;
	drag.target = point(0, 0);
	drag.destination = point(100, 50);
	drag.frames = 4;
	compiled = compile_interaction_plan(controls(drag));
	require(bool(compiled) && compiled->frame_count == 7 && !compiled->cleanup_required,
			"drag frame count wrong");
	require(interaction_input_at(*compiled, 2)->pointer_x == 25 &&
				interaction_input_at(*compiled, 5)->pointer_x == 100 &&
				interaction_input_at(*compiled, 6)->buttons_down.empty(),
			"drag interpolation/release wrong");
	drag.destination.reset();
	drag.path = {{10, 0}, {10, 10}};
	drag.frames = 4;
	compiled = compile_interaction_plan(controls(drag));
	require(bool(compiled) && interaction_input_at(*compiled, 2)->pointer_x == 5 &&
				interaction_input_at(*compiled, 3)->pointer_x == 10 &&
				interaction_input_at(*compiled, 4)->pointer_y == 5 &&
				interaction_input_at(*compiled, 5)->pointer_y == 10,
			"piecewise drag path wrong");
	AgenticInteractionStep chord;
	chord.operation = AgenticInteractionOperation::key_chord;
	chord.keys = {83, 341};
	chord.frames = 2;
	compiled = compile_interaction_plan(controls(chord));
	require(bool(compiled) && compiled->frame_count == 4 &&
				interaction_input_at(*compiled, 0)->keys_down.empty() &&
				interaction_input_at(*compiled, 1)->keys_down.size() == 2 &&
				interaction_input_at(*compiled, 3)->keys_down.empty(),
			"key chord release contract wrong");
	AgenticInteractionStep down;
	down.operation = AgenticInteractionOperation::pointer_down;
	auto hold_plan = controls(down);
	AgenticInteractionStep hold;
	hold.operation = AgenticInteractionOperation::pointer_hold;
	hold.frames = 8;
	hold.minimum_duration_ns = 1000000;
	hold_plan.steps.emplace_back(hold);
	compiled = compile_interaction_plan(hold_plan);
	require(bool(compiled) && compiled->spans.back().minimum_duration_ns == 1000000 &&
				compiled->cleanup_required,
			"hold minimum monotonic duration contract missing");
	require(!compile_interaction_plan(controls(hold)), "hold without prior down accepted");
	AgenticInteractionStep text;
	text.operation = AgenticInteractionOperation::text_enter;
	text.text = {U'a'};
	text.frames = 3;
	auto text_plan = controls(text);
	text_plan.persistence_declared = true;
	compiled = compile_interaction_plan(text_plan);
	require(bool(compiled) && interaction_input_at(*compiled, 0)->text.size() == 1 &&
				interaction_input_at(*compiled, 1)->text.empty(),
			"text-enter repeated Unicode across held frames");
	chord.text = {U'a'};
	require(!compile_interaction_plan(controls(chord)),
			"irrelevant text silently ignored on chord");
}
void target_and_observation_contracts() {
	AgenticInteractionStep click;
	click.operation = AgenticInteractionOperation::click;
	AgenticInteractionTarget selector;
	selector.kind = AgenticTargetKind::clay_name;
	selector.clay_name = "document-entry";
	click.target = selector;
	click.recipe.hold_frames = 1;
	click.expect = AgenticInteractionExpectation::target_hovered |
				   AgenticInteractionExpectation::target_pressed |
				   AgenticInteractionExpectation::target_held |
				   AgenticInteractionExpectation::target_released;
	auto compiled = compile_interaction_plan(controls(click));
	require(bool(compiled) && compiled->requires_geometry && compiled->requires_observations &&
				compiled->spans.front().resolve_before && compiled->spans[1].require_hover_before,
			"semantic target did not require observed hover before press");
	require(compiled->plan.steps.front().target->window_id == 1 &&
				compiled->plan.steps.front().step_id == "step_0",
			"canonical target/id not normalized");
	click.recipe.button = 1;
	require(!compile_interaction_plan(controls(click)),
			"unsupported secondary-button press assertions advertised");
	click.recipe.button = 0;
	click.recipe.hold_frames = 0;
	require(!compile_interaction_plan(controls(click)),
			"held assertion accepted without a held frame");
	click.recipe.hold_frames = 1;
	AgenticInteractionCapabilities unavailable;
	unavailable.geometry_targets = false;
	require(!compile_interaction_plan(controls(click), unavailable),
			"missing geometry capability ignored");
	click.target->window_id = 2;
	require(!compile_interaction_plan(controls(click)), "cross-window target accepted");
	AgenticInteractionObservation observation;
	observation.hovered = {42};
	observation.pressed = {42};
	observation.presented = true;
	require(observed_interaction_facts(observation, 42) == AgenticInteractionExpectation::none,
			"missing layout falsely asserted");
	observation.layout_completed = true;
	auto facts = observed_interaction_facts(observation, 42);
	require(has_expectation(facts, AgenticInteractionExpectation::target_pressed) &&
				!has_expectation(facts, AgenticInteractionExpectation::target_held),
			"observation facts fabricated requested state");
	observation.canceled = true;
	require(observed_interaction_facts(observation, 42) == AgenticInteractionExpectation::none,
			"canceled layout asserted success");
	observation.canceled = false;
	observation.frame = EvidenceFrameKey{"run_x", 9007199254740993ull, 18446744073709551615ull, 1};
	const auto encoded = to_json(observation);
	require(encoded.find("frame")->find("window_frame")->text == "18446744073709551615",
			"observation uint64 identity lost");
	AgenticInteractionResult result;
	result.first_divergence_offset = 0;
	require(to_json(result).find("first_divergence_offset")->text == "0" &&
				!to_json(result).find("activation_frame"),
			"offset-zero divergence lost or activation frame fabricated");
}
void exact_and_budget_contracts() {
	auto source = raw_plan();
	source.clock = AgenticInteractionClock::exact_window_frames;
	source.run_id = "run_exact";
	source.first_window_frame = std::numeric_limits<std::uint64_t>::max() - 2;
	source.baseline = AgenticInteractionBaseline::require_neutral;
	require(bool(compile_interaction_plan(source)), "valid maximal exact frame addresses rejected");
	source.spans.front().count = 4;
	require(!compile_interaction_plan(source), "exact address overflow accepted");
	source = raw_plan();
	source.spans.front().count = 3;
	source.limits.maximum_frames = 2;
	require(!compile_interaction_plan(source), "compiled frames exceeded hard request budget");
	source = raw_plan();
	source.limits.maximum_artifact_bytes = 1024;
	require(!compile_interaction_plan(source), "artifact byte estimate not enforced");
	source = raw_plan();
	source.limits.maximum_observations = 2;
	require(!compile_interaction_plan(source), "observation retention budget not enforced");
	source.record_observations = false;
	require(bool(compile_interaction_plan(source)), "optional observations unnecessarily required");
	auto exact_json = parse_json(
		R"({"window_id":"1","run_id":"run_exact","clock":"exact_window_frames","baseline":"require_neutral","frames":[{"window_frame":"10"},{"window_frame":"11"}]})");
	auto decoded = interaction_plan_from_json(*exact_json);
	require(bool(decoded) && decoded->first_window_frame == 10 &&
				compile_interaction_plan(*decoded)->frame_count == 2,
			"explicit exact-address rows decoded incorrectly");
	require(
		!interaction_plan_from_json(*parse_json(
			R"({"window_id":"1","run_id":"run_exact","clock":"exact_window_frames","baseline":"require_neutral","frames":[{"window_frame":"10"},{"window_frame":"9"}]})")),
		"backward exact addresses accepted");
}
void schema_identity_contracts() {
	AgenticInteractionReceipt receipt;
	receipt.job_id = "interaction_schema";
	receipt.run_id = "run_schema";
	receipt.plan_digest = std::string(64, 'a');
	receipt.frame = EvidenceFrameKey{"run_schema", 1, 1, 1};
	receipt.selected = true;
	require(bool(validate_agentic_document(to_json(receipt), "FlowUi.Agentic.InputFrame")),
			"valid input receipt schema rejected");
	receipt.frame->run_id = "run_other";
	require(!validate_agentic_document(to_json(receipt), "FlowUi.Agentic.InputFrame"),
			"cross-run receipt frame accepted");
	AgenticInteractionObservation observation;
	observation.job_id = "interaction_schema";
	observation.run_id = "run_schema";
	observation.plan_digest = std::string(64, 'a');
	observation.frame = EvidenceFrameKey{"run_schema", 1, 1, 1};
	require(bool(validate_agentic_document(to_json(observation),
										   "FlowUi.Agentic.InteractionObservation")),
			"valid observation schema rejected");
	observation.frame->run_id = "run_other";
	require(
		!validate_agentic_document(to_json(observation), "FlowUi.Agentic.InteractionObservation"),
		"cross-run observation frame accepted");
	AgenticInteractionResult result;
	result.job_id = "interaction_schema";
	result.run_id = "run_schema";
	result.window_id = 1;
	result.plan_digest = std::string(64, 'a');
	result.activation_frame = EvidenceFrameKey{"run_schema", 1, 1, 1};
	require(bool(validate_agentic_document(to_json(result), "FlowUi.Agentic.InteractionResult")),
			"valid result schema rejected");
	result.activation_frame->window_id = 2;
	require(!validate_agentic_document(to_json(result), "FlowUi.Agentic.InteractionResult"),
			"cross-window result activation accepted");
	auto plan = compile_interaction_plan(raw_plan());
	require(bool(plan) &&
				bool(validate_agentic_document(to_json(*plan), "FlowUi.Agentic.InteractionPlan")),
			"compiled plan schema rejected");
}
} // namespace
int main() {
	raw_contracts();
	transient_and_materialization_contracts();
	mechanical_contracts();
	target_and_observation_contracts();
	exact_and_budget_contracts();
	schema_identity_contracts();
	std::cout << "Agentic interaction contracts passed\n";
}
