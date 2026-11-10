#include "command_line.hpp"

#include "devSystems/devAgenticWorkflow/AgenticLocalTransport.hpp"
#include "devSystems/devAgenticWorkflow/AgenticManifest.hpp"

#include <algorithm>
#include <array>
#include <charconv>
#include <cmath>
#include <fstream>

namespace FlowUi::devSystems::agentic {
namespace {
[[nodiscard]] auto invalid_arguments() noexcept {
	return unexpectedError(makeError(ErrorCode::AgenticInvalidRequest, ErrorSite::AgenticCommand));
}
[[nodiscard]] const Json& member(const Json& value, std::string_view key) noexcept {
	static const Json missing{};
	const auto* found = value.find(key);
	return found ? *found : missing;
}
[[nodiscard]] Result<Json> read_json_file(const std::filesystem::path& path,
										  std::size_t maximum_bytes) {
	std::error_code error;
	if (!std::filesystem::is_regular_file(path, error) || std::filesystem::is_symlink(path, error))
		return invalid_arguments();
	const auto bytes = std::filesystem::file_size(path, error);
	if (error || bytes > maximum_bytes)
		return invalid_arguments();
	std::ifstream stream(path, std::ios::binary);
	std::string contents(static_cast<std::size_t>(bytes), '\0');
	stream.read(contents.data(), static_cast<std::streamsize>(bytes));
	if (!stream || static_cast<std::uint64_t>(stream.gcount()) != bytes)
		return invalid_arguments();
	return parse_json(contents, maximum_bytes);
}
[[nodiscard]] Result<Json> duration_nanoseconds(std::string_view text) {
	std::uint64_t scale = 1;
	if (text.ends_with("ms")) {
		scale = 1000000;
		text.remove_suffix(2);
	} else if (text.ends_with("ns"))
		text.remove_suffix(2);
	else if (text.ends_with("s")) {
		scale = 1000000000;
		text.remove_suffix(1);
	} else
		return invalid_arguments();
	double count = 0;
	const auto converted = std::from_chars(text.data(), text.data() + text.size(), count);
	if (converted.ec != std::errc{} || converted.ptr != text.data() + text.size() ||
		!std::isfinite(count) || count <= 0 ||
		count * static_cast<double>(scale) > 86400000000000.0)
		return invalid_arguments();
	return decimal(static_cast<std::uint64_t>(count * static_cast<double>(scale)));
}
[[nodiscard]] bool known_option(std::string_view option) noexcept {
	constexpr std::array options{"instance",
								 "capture",
								 "manifest",
								 "profile",
								 "window",
								 "scenario",
								 "mode",
								 "duration",
								 "measurement_ticks",
								 "warmup_ticks",
								 "seed",
								 "fixture_id",
								 "scenario_revision",
								 "scenario_digest",
								 "objective",
								 "minimum_passes",
								 "minimum_valid_intervals",
								 "minimum_duration_ns",
								 "idempotency_key",
								 "job",
								 "topic",
								 "selector",
								 "intent",
								 "max_bytes",
								 "limit",
								 "top",
								 "artifact",
								 "domain",
								 "kind",
								 "severity",
								 "source_id",
								 "entity_id",
								 "app_tick",
								 "window_frame",
								 "cursor",
								 "baseline",
								 "candidate",
								 "image",
								 "candidate_image",
								 "format",
								 "output",
								 "crop",
								 "crop_flow_node",
								 "flow_node",
								 "tree",
								 "passes",
								 "images",
								 "columns",
								 "tolerance",
								 "client",
								 "launcher",
								 "older_than_seconds",
								 "log",
								 "producer",
								 "build_id",
								 "decisions",
								 "review_id",
								 "schema_generation",
								 "proposal_revision",
								 "theme_revision",
								 "disk_revision",
								 "operation_id",
								 "executable",
								 "argv",
								 "working_directory",
								 "log_bytes",
								 "maximum_expected_tick_cadence",
								 "window_id",
								 "plan",
								 "request",
								 "wait_timeout_ms",
								 "after_sequence",
								 "plan_artifact",
								 "target",
								 "max_frames",
								 "deadline_ns",
								 "action_id",
								 "revision",
								 "arguments",
								 "sequence",
								 "offset",
								 "plan_digest"};
	return std::find(options.begin(), options.end(), option) != options.end();
}
[[nodiscard]] bool boolean_option(std::string_view option) noexcept {
	constexpr std::array options{"allow_degraded", "require_pass",	 "delete",
								 "publish",		   "include_source", "print",
								 "dry_run",		   "include_text",	 "rebind_run"};
	return std::find(options.begin(), options.end(), option) != options.end();
}
[[nodiscard]] Result<Json> parse_option_value(std::string_view option, std::string_view value) {
	if (option == "crop" || option == "argv" || option == "decisions" || option == "plan" ||
		option == "plan_artifact" || option == "target" || option == "arguments" ||
		option == "request" || (option == "capture" && value.starts_with("{")))
		return parse_json(value, 65536);
	if ((option == "crop_flow_node" || option == "flow_node") && value.starts_with("{")) {
		auto selector = parse_json(value, 4096);
		if (!selector || selector->kind != Json::Kind::object)
			return invalid_arguments();
		return selector;
	}
	if (option == "maximum_expected_tick_cadence" || option == "limit" || option == "top" ||
		option == "max_bytes" || option == "wait_timeout_ms") {
		auto decoded = parse_json(value, 32);
		if (!decoded || decoded->kind != Json::Kind::number ||
			decoded->uint64_or(UINT64_MAX) == UINT64_MAX)
			return invalid_arguments();
		return decoded;
	}
	if (option == "passes" || option == "images") {
		auto values = Json::make_array();
		while (!value.empty()) {
			const auto separator = value.find(',');
			const auto part = value.substr(0, separator);
			if (part.empty())
				return invalid_arguments();
			values.array.emplace_back(part);
			if (separator == std::string_view::npos)
				break;
			value.remove_prefix(separator + 1);
		}
		return values;
	}
	if (option == "duration")
		return duration_nanoseconds(value);
	return Json(value);
}
[[nodiscard]] std::filesystem::path locate_pack(const std::filesystem::path& executable,
												const std::filesystem::path& workspace) {
	std::vector<std::filesystem::path> candidates;
	candidates.reserve(4);
	if (!executable.empty())
		candidates.emplace_back(executable.parent_path().parent_path() /
								"share/flowui/agentic/knowledge");
#ifdef FLOWUI_AGENTIC_INSTALLED_KNOWLEDGE_DIR
	candidates.emplace_back(FLOWUI_AGENTIC_INSTALLED_KNOWLEDGE_DIR);
#endif
#ifdef FLOWUI_AGENTIC_SOURCE_KNOWLEDGE_DIR
	candidates.emplace_back(FLOWUI_AGENTIC_SOURCE_KNOWLEDGE_DIR);
#endif
	candidates.emplace_back(workspace / "include/devSystems/devAgenticWorkflow/knowledge");
	for (const auto& candidate : candidates) {
		std::error_code error;
		if (std::filesystem::is_regular_file(candidate / "index.json", error))
			return candidate.lexically_normal();
	}
	return candidates.back();
}
void normalize_capture_arguments(Json& arguments) {
	if (const auto* duration = arguments.find("duration")) {
		auto measurement = member(arguments, "measurement");
		if (measurement.kind != Json::Kind::object)
			measurement = Json::make_object();
		measurement.set("duration_ns", *duration);
		arguments.set("duration_ns", *duration);
		arguments.set("measurement", std::move(measurement));
	}
	if (const auto* window = arguments.find("window"))
		arguments.set("window_ids", Json::make_array({*window}));
	if (const auto* scenario = arguments.find("scenario");
		scenario && scenario->kind == Json::Kind::string) {
		auto descriptor = Json::make_object({{"id", *scenario}});
		if (const auto* revision = arguments.find("scenario_revision"))
			descriptor.set("revision", *revision);
		if (const auto* fixture = arguments.find("fixture_id"))
			descriptor.set("fixture_id", *fixture);
		if (const auto* seed = arguments.find("seed"))
			descriptor.set("seed", *seed);
		arguments.set("scenario_id", *scenario);
		arguments.set("scenario", std::move(descriptor));
	}
	if (const auto* warmup = arguments.find("warmup_ticks"))
		arguments.set("warmup", Json::make_object({{"minimum_ticks", *warmup}}));
}
} // namespace

Result<AgenticCommandLine>
parse_command_line(std::span<const std::string_view> arguments) noexcept {
	try {
		AgenticCommandLine result;
		result.context.workspace = std::filesystem::current_path();
		std::filesystem::path explicit_store;
		std::filesystem::path explicit_pack;
		std::vector<std::string> operation_words;
		operation_words.reserve(3);
		for (std::size_t index = 0; index < arguments.size(); ++index) {
			const auto argument = arguments[index];
			if (argument == "--help" || argument == "-h") {
				result.help = true;
				continue;
			}
			if (argument == "--json")
				continue;
			if (!argument.starts_with("--")) {
				operation_words.emplace_back(argument);
				continue;
			}
			std::string option(argument.substr(2));
			std::replace(option.begin(), option.end(), '-', '_');
			if (boolean_option(option)) {
				result.request.arguments.set(option, true);
				continue;
			}
			if (index + 1 >= arguments.size())
				return invalid_arguments();
			const auto value = arguments[++index];
			if (option == "workspace") {
				result.context.workspace = std::filesystem::absolute(std::filesystem::path(value));
				continue;
			}
			if (option == "store") {
				explicit_store = std::filesystem::path(value);
				continue;
			}
			if (option == "knowledge_pack") {
				explicit_pack = std::filesystem::path(value);
				continue;
			}
			if (option == "source_root") {
				result.context.source_root =
					std::filesystem::absolute(std::filesystem::path(value));
				continue;
			}
			if (option == "request_id") {
				result.request.request_id = value;
				continue;
			}
			if (option == "arguments_json") {
				auto decoded = parse_json(value, 1024u * 1024u);
				if (!decoded || decoded->kind != Json::Kind::object)
					return invalid_arguments();
				for (auto& field : decoded->object)
					result.request.arguments.set(std::move(field.first), std::move(field.second));
				continue;
			}
			if (option == "plan_file" || option == "request_file") {
				auto decoded = read_json_file(std::filesystem::path(value), 1024u * 1024u);
				if (!decoded || decoded->kind != Json::Kind::object)
					return invalid_arguments();
				result.request.arguments.set(option == "plan_file" ? "plan" : "request",
											 std::move(*decoded));
				continue;
			}
			if (!known_option(option))
				return invalid_arguments();
			auto decoded = parse_option_value(option, value);
			if (!decoded)
				return unexpectedError(decoded.error());
			result.request.arguments.set(option == "job" ? "job_id" : option, std::move(*decoded));
		}
		if (operation_words.empty())
			result.help = true;
		if (operation_words.size() > 3)
			return invalid_arguments();
		if (!operation_words.empty())
			result.request.operation = operation_words.front();
		for (std::size_t word_index = 1; word_index < operation_words.size(); ++word_index)
			result.request.operation += "." + operation_words[word_index];
		if (result.request.request_id.empty()) {
			auto identifier = make_identifier("request");
			if (!identifier)
				return unexpectedError(identifier.error());
			result.request.request_id = std::move(*identifier);
		}
		result.context.workspace =
			std::filesystem::absolute(result.context.workspace).lexically_normal();
		if (!explicit_store.empty())
			result.context.store_root =
				std::filesystem::absolute(explicit_store).lexically_normal();
		else {
			const auto project_config = result.context.workspace / ".flowui/agentic/config.json";
			std::error_code error;
			if (std::filesystem::is_regular_file(project_config, error)) {
				auto configuration = read_json_file(project_config, 16384);
				if (!configuration)
					return unexpectedError(configuration.error());
				const auto configured_root = member(*configuration, "store_path").string_or();
				if (!configured_root.empty())
					result.context.store_root =
						result.context.workspace / std::filesystem::path(configured_root);
			}
			if (result.context.store_root.empty())
				result.context.store_root = result.context.workspace / ".flowui/agentic/v1";
		}
		std::filesystem::path executable;
#if defined(__linux__)
		std::error_code executable_error;
		executable = std::filesystem::read_symlink("/proc/self/exe", executable_error);
#endif
		result.context.knowledge_path = explicit_pack.empty()
											? locate_pack(executable, result.context.workspace)
											: std::filesystem::absolute(explicit_pack);
		if (result.context.source_root.empty() &&
			std::filesystem::is_regular_file(result.context.workspace / "include/FlowUi/Public/App.hpp"))
			result.context.source_root = result.context.workspace;
		if (result.request.operation.starts_with("capture.") ||
			result.request.operation == "scenario.run")
			normalize_capture_arguments(result.request.arguments);
		if (result.request.operation == "verification.plan" ||
			result.request.operation == "verification.run") {
			const auto nested_instance =
				member(member(result.request.arguments, "request"), "instance").string_or();
			const auto selected_instance = member(result.request.arguments, "instance").string_or();
			if (!nested_instance.empty()) {
				if (!selected_instance.empty() && selected_instance != nested_instance)
					return invalid_arguments();
				result.request.arguments.set("instance", nested_instance);
			}
		}
		if (result.request.arguments.find("crop") &&
			result.request.arguments.find("crop_flow_node"))
			return invalid_arguments();
		if (!result.help) {
			auto validated = decode_command_request(encode_command_request(result.request));
			if (!validated)
				return unexpectedError(validated.error());
		}
		return result;
	} catch (...) {
		return invalid_arguments();
	}
}

Status select_live_instance(AgenticCommandLine& command_line) noexcept {
	try {
		const auto instance = member(command_line.request.arguments, "instance").string_or();
		if (instance.empty())
			return {};
		if (!valid_identifier(instance))
			return invalid_arguments();
		const auto path = command_line.context.store_root / "runtime/instances" /
						  (std::string(instance) + ".json");
		auto descriptor = read_json_file(path, 16384);
		if (!descriptor && command_line.request.operation == "verification.report")
			return {};
		if (!descriptor || member(*descriptor, "schema").string_or() != "FlowUi.Agentic.Instance" ||
			member(*descriptor, "schema_version").string_or() != "1.0" ||
			member(*descriptor, "protocol").string_or() != "FlowUi.Agentic.Local/1" ||
			member(*descriptor, "run_id").string_or() != instance)
			return unexpectedError(
				makeError(ErrorCode::AgenticUnavailable, ErrorSite::AgenticTransport));
		AgenticLocalEndpoint endpoint;
		endpoint.endpoint = member(*descriptor, "endpoint").string_or();
		endpoint.run_id = instance;
		endpoint.nonce = member(*descriptor, "nonce").string_or();
		if (command_line.request.operation == "verification.run" ||
			command_line.request.operation == "verification.status")
			endpoint.timeout_ms = 25000;
		command_line.context.live_handler =
			[endpoint = std::move(endpoint)](const AgenticCommandRequest& request) {
				return send_local_command(endpoint, encode_command_request(request));
			};
		return {};
	} catch (...) {
		return unexpectedError(
			makeError(ErrorCode::AgenticUnavailable, ErrorSite::AgenticTransport));
	}
}

Json command_line_help() {
	auto operations = Json::make_array();
	for (const auto& descriptor : agentic_command_registry())
		operations.array.emplace_back(descriptor.operation);
	return Json::make_object(
		{{"usage", "flowui-agent <operation> [sub-operation] [sub-operation] [--workspace PATH] "
				   "[--store PATH] "
				   "[--instance RUN_ID] [--json]"},
		 {"operations", std::move(operations)},
		 {"structured_arguments", "--arguments-json OBJECT; --plan JSON or --plan-file PATH; "
								  "--request JSON or --request-file PATH; "
								  "64-bit wire values are decimal strings"},
		 {"store_discovery", "explicit --store, workspace .flowui/agentic/config.json store_path, "
							 "workspace .flowui/agentic/v1"},
		 {"examples",
		  Json::make_array(
			  {"flowui-agent capabilities --json",
			   "flowui-agent context find --intent performance --json",
			   "flowui-agent capture plan --instance RUN_ID --profile "
			   "performance_verify --window 1 --duration 10s --json",
			   "flowui-agent query performance --capture CAPTURE_ID --json",
			   "flowui-agent interaction plan --plan-file plan.json --json",
			   "flowui-agent interaction start --instance RUN_ID --plan-file "
			   "plan.json --idempotency-key CASE --json",
			   "flowui-agent verification run --request-file suite.json --require-pass --json",
			   "flowui-agent verification status --instance RUN_ID --job JOB_ID "
			   "--after-sequence 1 --wait-timeout-ms 20000 --json",
			   "flowui-agent setup --client codex --print --json"})}});
}

} // namespace FlowUi::devSystems::agentic
