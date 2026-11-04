#include "devSystems/devAgenticWorkflow/AgenticCommands.hpp"
#include "devSystems/devAgenticWorkflow/AgenticInteraction.hpp"
#include "devSystems/devAgenticWorkflow/AgenticManifest.hpp"
#include "devSystems/devAgenticWorkflow/AgenticTypes.hpp"

#include <cstdlib>
#include <filesystem>
#include <iostream>

using namespace FlowUi::devSystems::agentic;

namespace {
void require(bool condition) {
	if (!condition) {
		std::cerr << "Agentic command contract failed\n";
		std::exit(1);
	}
}
} // namespace

int main() {
	require(quote_windows_argument("") == "\"\"");
	require(quote_windows_argument("plain") == "\"plain\"");
	require(quote_windows_argument("two words") == "\"two words\"");
	require(quote_windows_argument("a\"b") == "\"a\\\"b\"");
	require(quote_windows_argument("ends\\") == "\"ends\\\\\"");
	require(quote_windows_argument("before\\\"after") == "\"before\\\\\\\"after\"");
	require(quote_windows_argument("$(unsafe);`literal`") == "\"$(unsafe);`literal`\"");

	auto request_id = make_identifier("request");
	auto root_id = make_identifier("command_test");
	require(request_id && root_id);
	const auto directory = std::filesystem::temp_directory_path() / *root_id;
	AgenticCommandContext context;
	context.workspace = directory;
	context.store_root = directory / ".flowui/agentic/v1";
	AgenticCommandRequest request;
	request.request_id = *request_id;
	request.operation = "capabilities";
	auto response = execute_agentic_command(context, request);
	require(response.exit_code == 0);
	require(!std::filesystem::exists(directory));
	require(response.envelope.find("result")->find("mode")->string_or() == "offline");
	require(response.envelope.find("result")->find("commands")->array.size() ==
			agentic_command_registry().size());

	request.operation = "arbitrary.shell";
	response = execute_agentic_command(context, request);
	require(response.exit_code == 2);
	request.operation = "capture.start";
	response = execute_agentic_command(context, request);
	require(response.exit_code == 3);
	for (const auto& descriptor : agentic_command_registry())
		if (descriptor.operation == "interaction.start" ||
			descriptor.operation == "action.invoke") {
			require(descriptor.access == AgenticCommandAccess::mutate);
			require(descriptor.requires_live_app);
		}
	request.operation = "interaction.plan";
	auto input_plan = parse_json(
		R"({"schema":"FlowUi.Agentic.InteractionPlan","schema_version":"1.0","mode":"raw_frames","window_id":"1","clock":"window_input_frames","coordinates":"window_logical","baseline":"neutral_before_start","frames":[{"offset":"0","pointer":{"x":20,"y":30}},{"offset":"1","pointer":{"x":20,"y":30},"buttons_down":[0]},{"offset":"2","pointer":{"x":20,"y":30}}]})");
	require(input_plan.has_value());
	request.arguments = Json::make_object({{"plan", *input_plan}});
	response = execute_agentic_command(context, request);
	require(response.exit_code == 0);
	require(response.envelope.find("result")->find("compiled")->find("frame_count")->string_or() ==
			"3");
	require(!std::filesystem::exists(directory));
	input_plan->find("frames")->array.back().set("offset", "1");
	request.arguments.set("plan", *input_plan);
	require(execute_agentic_command(context, request).exit_code != 0);
	request.operation = "interaction.targets";
	request.arguments = Json::make_object({{"window_id", "1"}, {"limit", Json::number(10)}});
	require(bool(validate_interaction_command_arguments(request)));
	for (const auto& malformed : {Json("invalid"), Json("01"), Json::number(1), Json(true)}) {
		request.arguments.set("window_id", malformed);
		require(!validate_interaction_command_arguments(request));
		require(execute_agentic_command(context, request).exit_code == 2);
	}
	request.arguments = Json::make_object({{"window_id", "1"}, {"limit", Json("10")}});
	require(!validate_interaction_command_arguments(request));
	request.arguments.set("limit", Json(1.5));
	require(!validate_interaction_command_arguments(request));
	request.arguments = Json::make_object({{"window_id", "1"}, {"cursor", "1"}});
	require(!validate_interaction_command_arguments(request));
	request.operation = "interaction.record.start";
	request.arguments = Json::make_object({{"include_text", "false"}});
	require(!validate_interaction_command_arguments(request));
	request.arguments = Json::make_object({{"deadline_ns", "18446744073709551616"}});
	require(!validate_interaction_command_arguments(request));
	request.operation = "action.invoke";
	request.arguments = Json::make_object(
		{{"action_id", "app.save"}, {"revision", "1"}, {"arguments", Json::make_object()}});
	require(bool(validate_interaction_command_arguments(request)));
	request.arguments.set("revision", Json::number(1));
	require(!validate_interaction_command_arguments(request));
	request.arguments.set("revision", "1");
	request.arguments.set("script", "unregistered callback");
	require(!validate_interaction_command_arguments(request));
	request.operation = "interaction.query";
	request.arguments =
		Json::make_object({{"capture", "cap_source"}, {"cursor", "opaque_archive_cursor"}});
	require(bool(validate_interaction_command_arguments(request)));
	request.arguments.set("job_id", "interaction_live");
	require(!validate_interaction_command_arguments(request));
	request.operation = "interaction.start";
	request.arguments = Json::make_object(
		{{"plan", Json::make_object({{"clock", "exact_window_frames"}})}, {"rebind_run", true}});
	require(!validate_interaction_command_arguments(request));

	request.operation = "setup";
	for (const auto client :
		 {"codex", "claude-code", "antigravity", "gemini", "cursor", "vscode", "generic"}) {
		request.arguments = Json::make_object({{"client", client}});
		response = execute_agentic_command(context, request);
		require(response.exit_code == 0);
		require(response.envelope.find("result")->find("written")->bool_or(true) == false);
		require(!std::filesystem::exists(directory));
	}
	request.arguments = Json::make_object({{"client", "unknown"}});
	require(execute_agentic_command(context, request).exit_code == 2);

	request.operation = "capture.start";
	request.arguments = Json::make_object();
	std::uint32_t invocations = 0;
	context.live_handler = [&](const AgenticCommandRequest& live_request) -> FlowUi::Result<Json> {
		++invocations;
		require(live_request.operation == request.operation);
		if (live_request.arguments.find("objective")) {
			require(live_request.arguments.find("objective")->kind == Json::Kind::object);
			require(live_request.arguments.find("objective")->find("threshold")->number_or() ==
					40.0);
		}
		return command_success(live_request, Json::make_object({{"job_id", "job_test"}}),
							   "accepted");
	};
	response = execute_agentic_command(context, request);
	require(response.exit_code == 0 && invocations == 1);
	require(response.envelope.find("operation_status")->string_or() == "accepted");
	require(response.envelope.find("result")->find("job_id")->string_or() == "job_test");
	request.arguments.set("objective", "window_present_call_cadence_fps:gt:40");
	response = execute_agentic_command(context, request);
	require(response.exit_code == 0 && invocations == 2);
	const auto ordinary_store = context.store_root;
	for (int component = 0; component < 8; ++component)
		context.store_root /= std::string(180, 's');
	request.arguments.set("max_bytes", Json::number(1024));
	response = execute_agentic_command(context, request);
	require(response.exit_code == 0 && invocations == 3);
	require(response.envelope.find("result")->find("job_id")->string_or() == "job_test");
	require(write_json(response.envelope).size() <= 1024);
	context.store_root = ordinary_store;

	context.live_handler = [&](const AgenticCommandRequest& live_request) -> FlowUi::Result<Json> {
		return command_success(live_request, Json(std::string(2000, 'x')));
	};
	request.arguments.set("max_bytes", Json::number(1024));
	response = execute_agentic_command(context, request);
	require(response.exit_code == 4);
	require(write_json(response.envelope).size() < 1024);

	Json invalid = encode_command_request(request);
	invalid.set("arguments", Json::make_array());
	require(!decode_command_request(invalid));

	AgenticStore store(context.store_root);
	require(bool(store.initialize()));
	require(bool(store.reserve_run("run_source", Json::make_object())));
	AgenticInteractionPlan source;
	source.run_id = "run_source";
	source.window_id = 1;
	source.spans.emplace_back();
	const auto publish_source = [&](std::string capture_id, bool include_text) {
		AgenticCaptureSnapshot snapshot;
		snapshot.run_id = source.run_id;
		snapshot.capture_id = std::move(capture_id);
		snapshot.job_id = "job_source";
		snapshot.request.performance = SectionRequirement::omit;
		snapshot.request.runtime_diagnostics = SectionRequirement::omit;
		snapshot.request.window_ids = {1};
		require(bool(add_text_artifact(snapshot, "interaction_plan", "interaction/plan.json",
									   "application/json", "FlowUi.Agentic.InteractionPlan",
									   write_json(to_json(source, include_text)))));
		auto published = store.publish(std::move(snapshot));
		require(bool(published));
		return std::move(*published);
	};
	const auto published = publish_source("cap_source", true);
	const auto source_sha256 = sha256(write_json(to_json(source)));
	invocations = 0;
	context.live_handler = [&](const AgenticCommandRequest& live_request) -> FlowUi::Result<Json> {
		++invocations;
		require(bool(validate_interaction_command_arguments(live_request)));
		require(live_request.arguments.find("plan_artifact") == nullptr);
		require(live_request.arguments.find("plan")->find("run_id")->string_or().empty());
		const auto* provenance = live_request.arguments.find("replay_source");
		require(provenance && provenance->find("run_id")->string_or() == "run_source");
		require(provenance->find("artifact_sha256")->string_or() == source_sha256);
		require(provenance->find("manifest_digest")->string_or() == published.digest);
		return command_success(live_request,
							   Json::make_object({{"job_id", "interaction_replayed"}}), "accepted");
	};
	request.operation = "interaction.start";
	request.arguments =
		Json::make_object({{"plan_artifact", Json::make_object({{"capture", "cap_source"},
																{"artifact", "interaction_plan"},
																{"sha256", source_sha256}})},
						   {"rebind_run", true}});
	response = execute_agentic_command(context, request);
	require(response.exit_code == 0 && invocations == 1);
	request.arguments.find("plan_artifact")->set("sha256", std::string(64, '0'));
	require(execute_agentic_command(context, request).exit_code == 4 && invocations == 1);
	source.clock = AgenticInteractionClock::exact_window_frames;
	source.baseline = AgenticInteractionBaseline::require_neutral;
	source.first_window_frame = 100;
	const auto exact = publish_source("cap_exact", true);
	request.arguments.find("plan_artifact")->set("capture", exact.capture_id);
	request.arguments.find("plan_artifact")->set("sha256", sha256(write_json(to_json(source))));
	require(execute_agentic_command(context, request).exit_code == 2 && invocations == 1);
	source.clock = AgenticInteractionClock::window_input_frames;
	source.baseline = AgenticInteractionBaseline::neutral_before_start;
	source.first_window_frame = 0;
	source.persistence_declared = true;
	source.persistence = AgenticInteractionPersistence::redact_text;
	source.spans.front().input.text.emplace_back(U'\u03bb');
	const auto redacted = publish_source("cap_redacted", false);
	request.arguments.find("plan_artifact")->set("capture", redacted.capture_id);
	request.arguments.find("plan_artifact")
		->set("sha256", sha256(write_json(to_json(source, false))));
	require(execute_agentic_command(context, request).exit_code == 6 && invocations == 1);
	AgenticCaptureSnapshot tree_snapshot;
	tree_snapshot.run_id = "run_source";
	tree_snapshot.capture_id = "cap_trees";
	tree_snapshot.job_id = "job_trees";
	tree_snapshot.request.performance = SectionRequirement::omit;
	tree_snapshot.request.runtime_diagnostics = SectionRequirement::omit;
	tree_snapshot.request.window_ids = {1, 2};
	for (std::uint64_t index = 0; index < 3; ++index) {
		Json tree = Json::make_object({{"schema", "FlowUi.Agentic.Tree"},
									   {"schema_version", "1.0"},
									   {"run_id", "run_source"},
									   {"window_id", decimal(index == 1 ? 2 : 1)},
									   {"window_frame", decimal(index + 20)},
									   {"app_tick", decimal(index + 10)},
									   {"ui_runtime_frame", decimal(index + 20)},
									   {"generation", decimal(index + 30)},
									   {"complete", true},
									   {"truncated", false},
									   {"text_policy", "redacted"},
									   {"flow_nodes", Json::make_array()},
									   {"flow_roots", Json::make_array()},
									   {"clay_nodes", Json::make_array()},
									   {"clay_roots", Json::make_array()},
									   {"diagnostics", Json::make_array()}});
		require(
			bool(add_text_artifact(tree_snapshot, "tree_" + std::to_string(index),
								   "visual/tree_" + std::to_string(index) + ".json",
								   "application/json", "FlowUi.Agentic.Tree", write_json(tree))));
	}
	// A distinct image-row sentinel proves kind=trees never falls through to image metadata.
	require(bool(add_text_artifact(tree_snapshot, "visual_frames", "visual/frames.jsonl",
								   "application/x-ndjson", "CommandTest.ImageRows",
								   "{\"image_id\":\"image_sentinel\"}\n")));
	const auto tree_capture = store.publish(std::move(tree_snapshot));
	require(bool(tree_capture));
	request.operation = "query.visual";
	request.arguments = Json::make_object(
		{{"capture", "cap_trees"}, {"kind", "trees"}, {"limit", Json::number(1)}});
	response = execute_agentic_command(context, request);
	require(response.exit_code == 0);
	const auto first_tree_page = *response.envelope.find("result");
	require(first_tree_page.find("records")->array.size() == 1);
	const auto& first_tree = first_tree_page.find("records")->array.front();
	require(first_tree.find("schema")->string_or() == "FlowUi.Agentic.Tree");
	require(first_tree.find("artifact_id")->string_or() == "tree_0");
	require(first_tree.find("flow_node_count")->uint64_or(1) == 0);
	require(first_tree.find("image_id") == nullptr && first_tree.find("flow_nodes") == nullptr);
	require(first_tree_page.find("truncated")->bool_or());
	const auto cursor = std::string(first_tree_page.find("cursor")->string_or());
	require(!cursor.empty());
	request.arguments.set("cursor", cursor);
	response = execute_agentic_command(context, request);
	require(response.exit_code == 0);
	require(response.envelope.find("result")
				->find("records")
				->array.front()
				.find("artifact_id")
				->string_or() == "tree_1");
	request.arguments.set("window_id", "1");
	require(execute_agentic_command(context, request).exit_code != 0);
	request.arguments.set("cursor", "");
	request.arguments.set("limit", Json::number(10));
	response = execute_agentic_command(context, request);
	require(response.exit_code == 0 &&
			response.envelope.find("result")->find("records")->array.size() == 2);
	request.arguments.set(
		"filter", Json::make_object({{"first_app_tick", "11"}, {"end_app_tick_exclusive", "13"}}));
	response = execute_agentic_command(context, request);
	require(response.exit_code == 0 &&
			response.envelope.find("result")->find("records")->array.size() == 1);
	require(response.envelope.find("result")
				->find("records")
				->array.front()
				.find("artifact_id")
				->string_or() == "tree_2");
	request.arguments =
		Json::make_object({{"capture", "cap_trees"}, {"kind", "trees"}, {"tree", "tree_1"}});
	response = execute_agentic_command(context, request);
	require(response.exit_code == 0 &&
			response.envelope.find("result")->find("records")->array.size() == 1);
	request.arguments.set("artifact", "tree_0");
	require(execute_agentic_command(context, request).exit_code == 2);
	request.arguments = Json::make_object(
		{{"capture", "cap_trees"}, {"kind", "trees"}, {"artifact", "visual_frames"}});
	require(execute_agentic_command(context, request).exit_code != 0);
	request.arguments =
		Json::make_object({{"capture", "cap_source"}, {"kind", "trees"}, {"cursor", cursor}});
	require(execute_agentic_command(context, request).exit_code != 0);
	std::error_code cleanup_error;
	std::filesystem::remove_all(directory, cleanup_error);
	require(!cleanup_error);
	std::cout << "Agentic command contracts passed\n";
}
