#include "devSystems/devAgenticWorkflow/AgenticInteraction.hpp"
#include "devSystems/devAgenticWorkflow/AgenticManifest.hpp"

#include <cstdlib>
#include <iostream>

using namespace FlowUi::devSystems::agentic;
namespace {
void require(bool condition, std::string_view message) {
	if (!condition) {
		std::cerr << message << '\n';
		std::exit(1);
	}
}
Json parse(std::string_view text) {
	auto result = parse_json(text);
	require(bool(result), "invalid schema fixture JSON");
	return std::move(*result);
}
} // namespace

int main() {
	Json tree = parse(
		R"({"schema":"FlowUi.Agentic.Tree","schema_version":"1.0","run_id":"run_schema","window_id":"2","window_frame":"7","app_tick":"9","ui_runtime_frame":"6","generation":"13","complete":true,"truncated":false,"text_policy":"redacted","flow_nodes":[],"flow_roots":[],"clay_status":"disabled","diagnostics":[]})");
	require(bool(validate_agentic_document(tree, "FlowUi.Agentic.Tree")),
			"empty published tree rejected");
	tree.find("flow_nodes")
		->array.emplace_back(parse(
			R"({"index":0,"parent":null,"first_child":null,"next_sibling":null,"subtree_end_exclusive":1,"depth":0,"definition_id":"18446744073709551615","instance_id":"901","debug_name":"Users","definition_name":"UsersRoot","definition_type":"UsersRoot","source":null,"flags":0})"));
	tree.find("flow_roots")->array.emplace_back(Json::number(0));
	require(bool(validate_agentic_document(tree, "FlowUi.Agentic.Tree")),
			"nullable local-ref source or exact uint64 rejected");
	tree.find("flow_nodes")->array.front().set("source", Json::number(12));
	require(!validate_agentic_document(tree, "FlowUi.Agentic.Tree"),
			"invalid source union accepted");
	tree.find("flow_nodes")->array.front().set("source", Json{});
	tree.find("flow_nodes")->array.front().set("parent", Json::number(0));
	require(!validate_agentic_document(tree, "FlowUi.Agentic.Tree"),
			"self-referencing tree node accepted");
	tree.find("flow_nodes")->array.front().set("parent", Json{});
	tree.find("flow_roots")->array.front() = Json::number(8);
	require(!validate_agentic_document(tree, "FlowUi.Agentic.Tree"),
			"out-of-range tree root accepted");
	tree.find("flow_roots")->array.front() = Json::number(0);
	tree.set("window_frame", Json::number(7));
	require(!validate_agentic_document(tree, "FlowUi.Agentic.Tree"),
			"numeric uint64 frame accepted");

	auto themes = parse(
		R"({"schema":"FlowUi.Agentic.Themes","schema_version":"1.0","run_id":"run_schema","theme_revision":"1","schema_generation":"3","schema_fingerprint":"5","collection_available":true,"variants":[]})");
	require(bool(validate_agentic_document(themes, "FlowUi.Agentic.Themes")),
			"no registered themes rejected");
	themes.find("variants")->array.emplace_back(Json::make_object());
	require(!validate_agentic_document(themes, "FlowUi.Agentic.Themes"),
			"empty malformed theme variant accepted");

	auto event = parse(
		R"({"schema":"FlowUi.Agentic.MemoryEvent","schema_version":"1.0","run_id":"run_schema","capture_id":"cap_schema","sequence":"1","generation":"2","source_id":"3","app_tick":"4","window_id":"0","window_frame":"0","submission_serial":"0","operation":1,"monotonic_ns":"8","bytes_before":"0","bytes_after":"18446744073709551615","bytes_changed":"10"})");
	require(bool(validate_agentic_document(event, "FlowUi.Agentic.MemoryEvent")),
			"memory event contract rejected");
	event.set("bytes_after", "18446744073709551616");
	require(!validate_agentic_document(event, "FlowUi.Agentic.MemoryEvent"),
			"uint64 overflow accepted");
	require(bool(validate_agentic_document(Json::make_array(), "FlowUi.Agentic.Sources")),
			"inventory-scoped empty source dictionary rejected");
	require(!validate_agentic_document(Json::make_array({Json::make_object()}),
									   "FlowUi.Agentic.Sources"),
			"malformed source dictionary accepted");

	auto action = parse(
		R"({"schema":"FlowUi.Agentic.Action","schema_version":"1.0","run_id":"run_schema","capture_id":"cap_schema","job_id":"job_schema","scenario_id":"users.scenario","phase":"measured_step","app_tick":"4","monotonic_ns":"8","duration_ns":"2","result":"complete"})");
	require(bool(validate_agentic_document(action, "FlowUi.Agentic.Action")),
			"phase action rejected");
	action.set("action", "open_users");
	action.set("target", "users");
	action.set("arguments", Json::make_object());
	action.set("assertion", false);
	action.set("passed", true);
	require(!validate_agentic_document(action, "FlowUi.Agentic.Action"),
			"ambiguous oneOf action shape accepted");

	auto request = parse(
		R"({"schema":"FlowUi.Agentic.CommandRequest","schema_version":"1.0","request_id":"request_schema","operation":"capabilities","arguments":{}})");
	require(bool(validate_agentic_document(request, "FlowUi.Agentic.CommandRequest")),
			"command request rejected");
	request.set("arguments", Json::make_array());
	require(!validate_agentic_document(request, "FlowUi.Agentic.CommandRequest"),
			"array command arguments accepted");

	AgenticInteractionPlan input_plan;
	input_plan.window_id = 1;
	input_plan.spans.emplace_back();
	auto source_plan = to_json(input_plan);
	require(bool(validate_agentic_document(source_plan, "FlowUi.Agentic.InteractionPlan")),
			"canonical neutral interaction source rejected");
	source_plan.find("spans")->array.front().find("input")->set(
		"keys_down", Json::make_array({Json::number(33)}));
	require(!validate_agentic_document(source_plan, "FlowUi.Agentic.InteractionPlan"),
			"unsupported backend key slot accepted by interaction artifact schema");
	AgenticInteractionReceipt receipt;
	receipt.plan_digest = std::string(64, 'a');
	receipt.job_id = "interaction_schema";
	receipt.run_id = "run_schema";
	receipt.frame = EvidenceFrameKey{"run_schema", 2, 3, 1};
	receipt.selected = true;
	receipt.effective_input.text.emplace_back(U'\u03bb');
	auto delivered = to_json(receipt, false);
	require(bool(validate_agentic_document(delivered, "FlowUi.Agentic.InputFrame")),
			"redacted input receipt rejected");
	require(!delivered.find("text_replayable")->bool_or(true),
			"redacted text falsely advertised replayable");
	delivered.find("frame")->set("run_id", "run_other");
	require(!validate_agentic_document(delivered, "FlowUi.Agentic.InputFrame"),
			"input receipt cross-run frame accepted");
	AgenticInteractionResult interaction_result;
	interaction_result.plan_digest = std::string(64, 'b');
	interaction_result.job_id = "interaction_schema";
	interaction_result.run_id = "run_schema";
	interaction_result.window_id = 1;
	interaction_result.activation_frame = EvidenceFrameKey{"run_schema", 2, 3, 1};
	auto completed = to_json(interaction_result);
	require(bool(validate_agentic_document(completed, "FlowUi.Agentic.InteractionResult")),
			"pending interaction result rejected");
	completed.find("coverage")->set("consumed", Json::number(1));
	require(!validate_agentic_document(completed, "FlowUi.Agentic.InteractionResult"),
			"numeric interaction coverage count accepted");
	AgenticCaptureRequest capture_policy;
	require(!capture_policy.content.include_interaction_labels &&
				!capture_policy.content.include_interaction_outcomes,
			"interaction content retention must be explicit");
	auto policy_json = to_json(capture_policy);
	policy_json.find("content")->set("include_interaction_labels", true);
	policy_json.find("content")->set("include_interaction_outcomes", true);
	auto decoded_policy = capture_request_from_json(policy_json);
	require(bool(decoded_policy) && decoded_policy->content.include_interaction_labels &&
				decoded_policy->content.include_interaction_outcomes,
			"interaction content policy did not survive the request codec");
	policy_json.find("content")->set("include_interaction_outcomes", "true");
	require(!capture_request_from_json(policy_json),
			"string interaction policy accepted as boolean");
	std::cout << "Agentic artifact schema contracts passed\n";
}
