#include "devSystems/devAgenticWorkflow/AgenticCommands.hpp"

#include "devSystems/devAgenticWorkflow/AgenticAnalysis.hpp"
#include "devSystems/devAgenticWorkflow/AgenticImage.hpp"
#include "devSystems/devAgenticWorkflow/AgenticInteraction.hpp"
#include "devSystems/devAgenticWorkflow/AgenticKnowledge.hpp"

#include <algorithm>
#include <array>
#include <charconv>
#include <cmath>
#include <cwchar>
#include <fstream>
#include <limits>
#ifdef _WIN32
#define NOMINMAX
#include <windows.h>
#else
#include <cerrno>
#include <fcntl.h>
#include <poll.h>
#include <sys/wait.h>
#include <unistd.h>
#endif

namespace FlowUi::devSystems::agentic {
namespace {
using Access = AgenticCommandAccess;
constexpr std::array registry{
	AgenticCommandDescriptor{"capabilities",
							 "Discover compiled/runtime capabilities and exact target.", "", 65536,
							 2000, Access::read, false},
	AgenticCommandDescriptor{
		"instances", "List owner-private run descriptors without their authentication nonce.", "",
		65536, 2000, Access::read, false},
	AgenticCommandDescriptor{"status",
							 "Inspect the selected live application and safe-point liveness.",
							 "local_attachment", 65536, 2000, Access::read, true},
	AgenticCommandDescriptor{"context.find",
							 "Find bounded semantic topics by explicit intent and keywords.",
							 "knowledge", 12000, 2000, Access::read, false},
	AgenticCommandDescriptor{"context.read", "Read a semantic topic or validated source selector.",
							 "knowledge", 12000, 2000, Access::read, false},
	AgenticCommandDescriptor{"context.validate",
							 "Check pack digests and matching source selectors.", "knowledge",
							 65536, 2000, Access::read, false},
	AgenticCommandDescriptor{"capture.plan",
							 "Resolve requested collection, objective and resource budgets.",
							 "capture", 65536, 2000, Access::read, true},
	AgenticCommandDescriptor{"capture.start",
							 "Enqueue one bounded collection job at the application's safe point.",
							 "capture", 65536, 2000, Access::collect, true},
	AgenticCommandDescriptor{"capture.stop",
							 "Stop admission and finalize the selected capture normally.",
							 "capture", 65536, 2000, Access::collect, true},
	AgenticCommandDescriptor{"job.status", "Recover progress or sealed evidence by job identity.",
							 "capture", 65536, 2000, Access::read, true},
	AgenticCommandDescriptor{
		"job.cancel", "Request bounded cancellation without freeing submitted GPU resources.",
		"capture", 65536, 2000, Access::collect, true},
	AgenticCommandDescriptor{"scenario.list",
							 "List application-declared workloads and their modes.", "scenarios",
							 65536, 2000, Access::read, true},
	AgenticCommandDescriptor{"scenario.run",
							 "Run a registered scenario through normal application safe points.",
							 "scenarios", 65536, 2000, Access::collect, true},
	AgenticCommandDescriptor{
		"interaction.capabilities",
		"Discover input control, observations, recording and exported actions.", "interaction",
		65536, 2000, Access::read, true},
	AgenticCommandDescriptor{
		"interaction.targets",
		"Resolve bounded revisioned input targets without widget-type assumptions.", "interaction",
		65536, 2000, Access::read, true},
	AgenticCommandDescriptor{"interaction.plan",
							 "Validate and compile a finite input plan without executing it.",
							 "offline", 65536, 2000, Access::read, false},
	AgenticCommandDescriptor{
		"interaction.start",
		"Admit an immutable input plan through the selected application's normal loop.",
		"interaction", 65536, 2000, Access::mutate, true},
	AgenticCommandDescriptor{"interaction.record.start",
							 "Record bounded effective input with an explicit text-content policy.",
							 "interaction_recording", 65536, 2000, Access::collect, true},
	AgenticCommandDescriptor{"interaction.record.stop",
							 "Stop the selected input recorder and publish its retained evidence.",
							 "interaction_recording", 65536, 2000, Access::collect, true},
	AgenticCommandDescriptor{"interaction.query",
							 "Read bounded live results or inventoried interaction evidence.",
							 "interaction", 65536, 2000, Access::read, false},
	AgenticCommandDescriptor{
		"action.list",
		"Discover explicitly exported application actions and typed invocation contracts.",
		"application_actions", 65536, 2000, Access::read, true},
	AgenticCommandDescriptor{
		"action.invoke", "Invoke one exported revision-checked application action with evidence.",
		"application_actions", 65536, 2000, Access::mutate, true},
	AgenticCommandDescriptor{"query.performance",
							 "Analyze retained frame cadence and query native timing evidence.",
							 "offline", 65536, 2000, Access::read, false},
	AgenticCommandDescriptor{"query.diagnostics",
							 "Query diagnostics without conflating build and runtime producers.",
							 "offline", 65536, 2000, Access::read, false},
	AgenticCommandDescriptor{"query.memory", "Query bounded retained memory samples or events.",
							 "offline", 65536, 2000, Access::read, false},
	AgenticCommandDescriptor{"query.visual",
							 "Query image metadata, owned trees, themes or resource catalogues.",
							 "offline", 65536, 2000, Access::read, false},
	AgenticCommandDescriptor{"artifact.list",
							 "List the selected manifest's verified artifact inventory.", "offline",
							 65536, 2000, Access::read, false},
	AgenticCommandDescriptor{"artifact.read",
							 "Read bounded text from an explicitly inventoried artifact.",
							 "offline", 65536, 2000, Access::read, false},
	AgenticCommandDescriptor{
		"artifact.image",
		"Export an inventoried image or physical-pixel crop to an explicit destination.", "offline",
		65536, 2000, Access::read, false},
	AgenticCommandDescriptor{"visual.compare",
							 "Compare same-sized source RGB images, with optional heatmap output.",
							 "offline", 65536, 2000, Access::read, false},
	AgenticCommandDescriptor{"visual.contact-sheet",
							 "Export a bounded image state matrix with explicit source order.",
							 "offline", 65536, 2000, Access::read, false},
	AgenticCommandDescriptor{
		"compare", "Check comparability before reporting performance or issue differences.",
		"offline", 65536, 2000, Access::read, false},
	AgenticCommandDescriptor{"objective.check",
							 "Evaluate a declared objective over independent valid captures.",
							 "offline", 65536, 2000, Access::read, false},
	AgenticCommandDescriptor{"changes.preview",
							 "Inspect a typed tooling review with expected revisions.", "changes",
							 65536, 2000, Access::read, true},
	AgenticCommandDescriptor{"changes.apply",
							 "Commit an explicitly selected revision-checked review transaction.",
							 "changes", 65536, 2000, Access::mutate, true},
	AgenticCommandDescriptor{"store.list",
							 "List immutable sealed captures; the index is only a cache.",
							 "offline", 65536, 2000, Access::read, false},
	AgenticCommandDescriptor{"store.validate",
							 "Verify manifests, paths, byte counts, checksums and references.",
							 "offline", 65536, 2000, Access::read, false},
	AgenticCommandDescriptor{"store.repair-index",
							 "Rebuild the store catalogue from authoritative manifests.", "offline",
							 65536, 2000, Access::administer, false},
	AgenticCommandDescriptor{"store.prune",
							 "Dry-run or explicitly remove whole unpinned, unreferenced captures.",
							 "offline", 65536, 2000, Access::administer, false},
	AgenticCommandDescriptor{"ingest.build",
							 "Preserve and normalize bounded external compiler/test evidence.",
							 "offline", 65536, 2000, Access::administer, false},
	AgenticCommandDescriptor{
		"setup", "Print project-specific MCP registration without editing client settings.", "",
		65536, 2000, Access::read, false},
	AgenticCommandDescriptor{
		"launch",
		"Launch an explicit executable with structured argv and bounded source-labelled logs.",
		"launch", 65536, 2000, Access::administer, false},
};

[[nodiscard]] auto command_error(ErrorCode code) noexcept {
	return unexpectedError(makeError(code, ErrorSite::AgenticCommand));
}
[[nodiscard]] const Json& argument(const Json& arguments, std::string_view key) noexcept {
	static const Json missing{};
	const auto* value = arguments.find(key);
	return value ? *value : missing;
}
[[nodiscard]] std::string_view text_argument(const Json& arguments, std::string_view key,
											 std::string_view fallback = {}) noexcept {
	return argument(arguments, key).string_or(fallback);
}
[[nodiscard]] std::uint64_t unsigned_argument(const Json& arguments, std::string_view key,
											  std::uint64_t fallback) noexcept {
	return argument(arguments, key).uint64_or(fallback);
}
[[nodiscard]] std::uint64_t byte_limit(const Json& arguments,
									   std::uint64_t fallback = 65536) noexcept {
	return std::clamp<std::uint64_t>(unsigned_argument(arguments, "max_bytes", fallback), 1024,
									 1024u * 1024u);
}
[[nodiscard]] Result<std::string> read_text_file(const std::filesystem::path& path,
												 std::uint64_t maximum_bytes) {
	std::error_code error;
	const auto bytes = std::filesystem::file_size(path, error);
	if (error)
		return command_error(ErrorCode::AgenticUnavailable);
	if (bytes > maximum_bytes)
		return command_error(ErrorCode::AgenticBudgetExhausted);
	std::ifstream stream(path, std::ios::binary);
	if (!stream)
		return command_error(ErrorCode::AgenticUnavailable);
	std::string contents(static_cast<std::size_t>(bytes), '\0');
	stream.read(contents.data(), static_cast<std::streamsize>(bytes));
	if (!stream || static_cast<std::uint64_t>(stream.gcount()) != bytes)
		return command_error(ErrorCode::AgenticUnavailable);
	return contents;
}
[[nodiscard]] Status write_output(const std::filesystem::path& path,
								  std::span<const std::uint8_t> bytes) {
	if (path.empty())
		return command_error(ErrorCode::AgenticInvalidRequest);
	std::ofstream output(path, std::ios::binary | std::ios::trunc);
	if (!output)
		return command_error(ErrorCode::AgenticPublicationFailed);
	output.write(reinterpret_cast<const char*>(bytes.data()),
				 static_cast<std::streamsize>(bytes.size()));
	output.close();
	if (output.fail())
		return command_error(ErrorCode::AgenticPublicationFailed);
	return {};
}
[[nodiscard]] Json page_json(AgenticPage page) {
	return Json::make_object({{"records", std::move(page.records)},
							  {"cursor", std::move(page.cursor)},
							  {"manifest_digest", std::move(page.manifest_digest)},
							  {"truncated", page.truncated}});
}
[[nodiscard]] Json descriptors_json() {
	auto descriptors = Json::make_array();
	for (const auto& descriptor : registry) {
		descriptors.array.emplace_back(Json::make_object(
			{{"operation", Json(descriptor.operation)},
			 {"version", "1.0"},
			 {"description", Json(descriptor.description)},
			 {"input_schema", "FlowUi.Agentic.CommandRequest"},
			 {"output_schema", "FlowUi.Agentic.CommandResult"},
			 {"requires_live_app", descriptor.requires_live_app},
			 {"capability", Json(descriptor.capability)},
			 {"maximum_default_bytes", Json::number(descriptor.default_maximum_bytes)},
			 {"timeout_ms", Json::number(descriptor.timeout_ms)},
			 {"access", descriptor.access == Access::read	   ? "read"
						: descriptor.access == Access::collect ? "collect"
						: descriptor.access == Access::mutate  ? "mutate"
															   : "administer"}}));
	}
	return descriptors;
}
[[nodiscard]] Result<AgenticObjective> objective_argument(const Json& arguments) {
	AgenticObjective objective;
	const auto& supplied = argument(arguments, "objective");
	if (supplied.kind == Json::Kind::string) {
		const auto value = supplied.string_or();
		const auto separator = value.find(':');
		const auto second_separator =
			value.find(':', separator == std::string_view::npos ? value.size() : separator + 1);
		if (separator == std::string_view::npos || second_separator == std::string_view::npos)
			return command_error(ErrorCode::AgenticInvalidRequest);
		objective.metric = value.substr(0, separator);
		objective.comparison = value.substr(separator + 1, second_separator - separator - 1);
		const auto threshold = value.substr(second_separator + 1);
		const auto converted = std::from_chars(
			threshold.data(), threshold.data() + threshold.size(), objective.threshold);
		if (converted.ec != std::errc{} || converted.ptr != threshold.data() + threshold.size())
			return command_error(ErrorCode::AgenticInvalidRequest);
	} else if (supplied.kind == Json::Kind::object) {
		objective.metric = text_argument(supplied, "metric", objective.metric);
		objective.comparison = text_argument(supplied, "operator", "gt");
		objective.threshold = argument(supplied, "threshold").number_or(40.0);
		objective.minimum_valid_intervals =
			unsigned_argument(supplied, "minimum_valid_intervals", 120);
		objective.minimum_duration_ns =
			unsigned_argument(supplied, "minimum_duration_ns", 10000000000ull);
		objective.minimum_passes = static_cast<std::uint32_t>(
			std::min<std::uint64_t>(unsigned_argument(supplied, "minimum_passes", 5), 100));
		if (const auto* p95 = supplied.find("maximum_p95_interval_ns"))
			objective.maximum_p95_interval_ns = p95->number_or();
	} else if (supplied.kind != Json::Kind::null)
		return command_error(ErrorCode::AgenticInvalidRequest);
	objective.minimum_passes = static_cast<std::uint32_t>(std::min<std::uint64_t>(
		unsigned_argument(arguments, "minimum_passes", objective.minimum_passes), 100));
	objective.minimum_valid_intervals =
		unsigned_argument(arguments, "minimum_valid_intervals", objective.minimum_valid_intervals);
	objective.minimum_duration_ns =
		unsigned_argument(arguments, "minimum_duration_ns", objective.minimum_duration_ns);
	if (!std::isfinite(objective.threshold) || objective.threshold <= 0 ||
		objective.minimum_passes == 0 || objective.minimum_valid_intervals == 0 ||
		objective.minimum_duration_ns == 0)
		return command_error(ErrorCode::AgenticInvalidRequest);
	return objective;
}
[[nodiscard]] Result<Json> list_instances(const std::filesystem::path& store_root,
										  std::uint64_t maximum_bytes) {
	auto instances = Json::make_array();
	std::error_code error;
	const auto directory = store_root / "runtime" / "instances";
	if (!std::filesystem::exists(directory, error))
		return Json::make_object({{"instances", std::move(instances)}});
	std::size_t retained_bytes = 0;
	for (const auto& entry : std::filesystem::directory_iterator(directory)) {
		if (instances.array.size() >= 128 || entry.path().extension() != ".json" ||
			entry.is_symlink() || !entry.is_regular_file())
			continue;
		auto text = read_text_file(entry.path(), 16384);
		if (!text)
			continue;
		auto descriptor = parse_json(*text, 16384);
		if (!descriptor || text_argument(*descriptor, "schema") != "FlowUi.Agentic.Instance")
			continue;
		auto visible =
			Json::make_object({{"run_id", argument(*descriptor, "run_id")},
							   {"protocol", argument(*descriptor, "protocol")},
							   {"pid", argument(*descriptor, "pid")},
							   {"last_safe_point_ns", argument(*descriptor, "last_safe_point_ns")},
							   {"liveness", "descriptor_only_use_status_to_authenticate"}});
		retained_bytes += write_json(visible).size();
		if (retained_bytes > maximum_bytes / 2)
			break;
		instances.array.emplace_back(std::move(visible));
	}
	return Json::make_object(
		{{"instances", std::move(instances)}, {"resolved_store", store_root.generic_string()}});
}
[[nodiscard]] Result<Json> setup_configuration(const AgenticCommandContext& context,
											   const Json& arguments) {
	const auto client = text_argument(arguments, "client", "generic");
	const auto workspace =
		std::filesystem::absolute(context.workspace.empty() ? std::filesystem::current_path()
															: context.workspace)
			.lexically_normal()
			.generic_string();
	const auto launcher = text_argument(arguments, "launcher", "flowui-agent-mcp");
	const auto command = Json::make_object(
		{{"command", Json(launcher)}, {"args", Json::make_array({"--workspace", workspace})}});
	Json configuration;
	std::string destination;
	if (client == "codex") {
		destination = ".codex/config.toml";
		configuration =
			Json::make_object({{"mcp_servers", Json::make_object({{"flowui", command}})}});
	} else if (client == "vscode") {
		destination = ".vscode/mcp.json";
		auto server = command;
		server.set("type", "stdio");
		configuration =
			Json::make_object({{"servers", Json::make_object({{"flowui", std::move(server)}})}});
	} else if (client == "generic" || client == "claude-code" || client == "cursor" ||
			   client == "antigravity" || client == "gemini") {
		destination = client == "cursor"		? ".cursor/mcp.json"
					  : client == "antigravity" ? ".agents/mcp_config.json"
					  : client == "gemini"		? ".gemini/settings.json"
												: ".mcp.json";
		configuration =
			Json::make_object({{"mcpServers", Json::make_object({{"flowui", command}})}});
	} else
		return command_error(ErrorCode::AgenticInvalidRequest);
	auto result = Json::make_object(
		{{"client", Json(client)},
		 {"destination", destination},
		 {"configuration", std::move(configuration)},
		 {"written", false},
		 {"context_pointer", "flowui-agent context read --topic agentic.evidence_contract --json"},
		 {"client_validation", "configuration_generated_client_execution_not_claimed"}});
	if (client == "codex")
		result.set("configuration_text",
				   "[mcp_servers.flowui]\ncommand = " + write_json(Json(launcher)) +
					   "\nargs = [\"--workspace\", " + write_json(Json(workspace)) + "]\n");
	return result;
}

#ifndef _WIN32
struct LaunchHandshake {
	std::int64_t process_id = 0;
	std::int32_t native_error = 0;
};

[[nodiscard]] bool write_launch_state(const std::filesystem::path& directory,
									  const Json& state) noexcept {
	try {
		const auto pending = directory / "session.pending";
		std::ofstream stream(pending, std::ios::binary | std::ios::trunc);
		const auto bytes = write_json(state);
		stream.write(bytes.data(), static_cast<std::streamsize>(bytes.size()));
		stream.close();
		if (!stream.fail()) {
			std::error_code error;
			std::filesystem::rename(pending, directory / "session.json", error);
			return !error;
		}
		return false;
	} catch (...) {
		return false;
	}
}

[[noreturn]] void supervise_launch(const std::filesystem::path& executable,
								   const std::vector<std::string>& argument_storage,
								   const std::filesystem::path& working_directory,
								   const std::filesystem::path& store_root,
								   const std::filesystem::path& launch_directory,
								   const std::string& run_id, std::uint64_t maximum_log_bytes,
								   int handshake_descriptor) noexcept {
	try {
		::setsid();
		std::array<int, 2> stdout_pipe{-1, -1};
		std::array<int, 2> stderr_pipe{-1, -1};
		if (::pipe(stdout_pipe.data()) != 0 || ::pipe(stderr_pipe.data()) != 0) {
			const LaunchHandshake handshake{0, errno};
			const auto written = ::write(handshake_descriptor, &handshake, sizeof(handshake));
			if (written != sizeof(handshake))
				::close(handshake_descriptor);
			::_exit(1);
		}
		::fcntl(handshake_descriptor, F_SETFD, FD_CLOEXEC);
		const auto application_process = ::fork();
		if (application_process == 0) {
			::close(stdout_pipe[0]);
			::close(stderr_pipe[0]);
			::dup2(stdout_pipe[1], STDOUT_FILENO);
			::dup2(stderr_pipe[1], STDERR_FILENO);
			::close(stdout_pipe[1]);
			::close(stderr_pipe[1]);
			const auto null_input = ::open("/dev/null", O_RDONLY);
			if (null_input >= 0) {
				::dup2(null_input, STDIN_FILENO);
				::close(null_input);
			}
			if (::chdir(working_directory.c_str()) != 0 ||
				::setenv("FLOWUI_AGENTIC_STORE", store_root.c_str(), 1) != 0 ||
				::setenv("FLOWUI_AGENTIC_RUN_ID", run_id.c_str(), 1) != 0 ||
				::setenv("FLOWUI_AGENTIC_ENDPOINT_ENABLE", "1", 1) != 0)
				::_exit(126);
			std::vector<char*> argv;
			argv.reserve(argument_storage.size() + 2);
			argv.emplace_back(const_cast<char*>(executable.c_str()));
			for (const auto& value : argument_storage)
				argv.emplace_back(const_cast<char*>(value.c_str()));
			argv.emplace_back(nullptr);
			::execv(executable.c_str(), argv.data());
			const std::string failure =
				"flowui-agent launch: exec failed (" + std::to_string(errno) + ")\n";
			const auto written = ::write(STDERR_FILENO, failure.data(), failure.size());
			if (written < 0)
				::close(STDERR_FILENO);
			::_exit(127);
		}
		::close(stdout_pipe[1]);
		::close(stderr_pipe[1]);
		if (application_process < 0) {
			const LaunchHandshake handshake{0, errno};
			const auto written = ::write(handshake_descriptor, &handshake, sizeof(handshake));
			if (written != sizeof(handshake))
				::close(handshake_descriptor);
			::_exit(1);
		}
		const LaunchHandshake handshake{application_process, 0};
		const auto written = ::write(handshake_descriptor, &handshake, sizeof(handshake));
		if (written != sizeof(handshake))
			::close(handshake_descriptor);
		::close(handshake_descriptor);
		const auto null_output = ::open("/dev/null", O_RDWR);
		if (null_output >= 0) {
			::dup2(null_output, STDIN_FILENO);
			::dup2(null_output, STDOUT_FILENO);
			::dup2(null_output, STDERR_FILENO);
			if (null_output > STDERR_FILENO)
				::close(null_output);
		}
		std::array<int, 2> descriptors{stdout_pipe[0], stderr_pipe[0]};
		std::array<std::ofstream, 2> logs{
			std::ofstream(launch_directory / "stdout.log", std::ios::binary),
			std::ofstream(launch_directory / "stderr.log", std::ios::binary)};
		std::array<std::uint64_t, 2> observed_bytes{};
		std::array<std::uint64_t, 2> retained_bytes{};
		for (const auto descriptor : descriptors)
			::fcntl(descriptor, F_SETFL, ::fcntl(descriptor, F_GETFL) | O_NONBLOCK);
		Json state = Json::make_object({{"schema", "FlowUi.Agentic.LaunchSession"},
										{"schema_version", "1.0"},
										{"run_id", run_id},
										{"pid", decimal(application_process)},
										{"status", "running"},
										{"lifetime_policy", "leave_running"},
										{"started_at_utc", utc_timestamp()}});
		const auto initial_state_saved = write_launch_state(launch_directory, state);
		if (!initial_state_saved)
			state.set("state_publication_failed", true);
		bool process_finished = false;
		int process_status = 0;
		std::uint64_t exited_ns = 0;
		std::array<char, 8192> buffer{};
		while (!process_finished || descriptors[0] >= 0 || descriptors[1] >= 0) {
			std::array<pollfd, 2> pending{
				{{descriptors[0], POLLIN | POLLHUP, 0}, {descriptors[1], POLLIN | POLLHUP, 0}}};
			const auto poll_status = ::poll(pending.data(), pending.size(), 100);
			if (poll_status >= 0) {
				for (std::size_t source_index = 0; source_index < descriptors.size();
					 ++source_index) {
					if (descriptors[source_index] < 0 || pending[source_index].revents == 0)
						continue;
					const auto amount =
						::read(descriptors[source_index], buffer.data(), buffer.size());
					if (amount > 0) {
						observed_bytes[source_index] += static_cast<std::uint64_t>(amount);
						const auto retained = std::min<std::uint64_t>(
							amount, maximum_log_bytes - retained_bytes[source_index]);
						logs[source_index].write(buffer.data(),
												 static_cast<std::streamsize>(retained));
						retained_bytes[source_index] += retained;
					} else if (amount == 0 || (errno != EAGAIN && errno != EINTR)) {
						::close(descriptors[source_index]);
						descriptors[source_index] = -1;
					}
				}
			}
			if (!process_finished &&
				::waitpid(application_process, &process_status, WNOHANG) == application_process) {
				process_finished = true;
				exited_ns = monotonic_nanoseconds();
			}
			if (process_finished && monotonic_nanoseconds() - exited_ns > 2000000000ull) {
				for (auto& descriptor : descriptors)
					if (descriptor >= 0) {
						::close(descriptor);
						descriptor = -1;
					}
			}
		}
		for (auto& log : logs)
			log.close();
		state.set("status", "exited");
		state.set("finished_at_utc", utc_timestamp());
		state.set("exit_code",
				  Json::number(WIFEXITED(process_status) ? WEXITSTATUS(process_status)
														 : 128 + WTERMSIG(process_status)));
		state.set("stdout", Json::make_object({{"path", "stdout.log"},
											   {"observed_bytes", decimal(observed_bytes[0])},
											   {"retained_bytes", decimal(retained_bytes[0])},
											   {"truncated", observed_bytes[0] > retained_bytes[0]},
											   {"write_failed", logs[0].fail()}}));
		state.set("stderr", Json::make_object({{"path", "stderr.log"},
											   {"observed_bytes", decimal(observed_bytes[1])},
											   {"retained_bytes", decimal(retained_bytes[1])},
											   {"truncated", observed_bytes[1] > retained_bytes[1]},
											   {"write_failed", logs[1].fail()}}));
		const auto final_state_saved = write_launch_state(launch_directory, state);
		if (!final_state_saved)
			::_exit(1);
		::_exit(0);
	} catch (...) {
		const LaunchHandshake handshake{0, ENOMEM};
		const auto written = ::write(handshake_descriptor, &handshake, sizeof(handshake));
		if (written != sizeof(handshake))
			::close(handshake_descriptor);
		::_exit(1);
	}
}
#endif

#ifdef _WIN32
class OwnedWindowsHandle {
public:
	explicit OwnedWindowsHandle(HANDLE handle = INVALID_HANDLE_VALUE) noexcept : handle_(handle) {}
	~OwnedWindowsHandle() { reset(); }
	OwnedWindowsHandle(const OwnedWindowsHandle&) = delete;
	OwnedWindowsHandle& operator=(const OwnedWindowsHandle&) = delete;
	[[nodiscard]] HANDLE get() const noexcept { return handle_; }
	void reset(HANDLE handle = INVALID_HANDLE_VALUE) noexcept {
		if (handle_ != INVALID_HANDLE_VALUE && handle_ != nullptr)
			CloseHandle(handle_);
		handle_ = handle;
	}

private:
	HANDLE handle_;
};

[[nodiscard]] std::wstring windows_text(std::string_view text) {
	if (text.empty())
		return {};
	const auto size = MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, text.data(),
										  static_cast<int>(text.size()), nullptr, 0);
	if (size <= 0)
		return {};
	std::wstring result(static_cast<std::size_t>(size), L'\0');
	if (MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, text.data(),
							static_cast<int>(text.size()), result.data(), size) != size)
		return {};
	return result;
}

[[nodiscard]] std::string wire_path(const std::filesystem::path& path) {
	const auto text = path.generic_u8string();
	return std::string(reinterpret_cast<const char*>(text.data()), text.size());
}

[[nodiscard]] Status windows_launch_state(const std::filesystem::path& directory,
										  const Json& state) {
	const auto bytes = write_json(state);
	auto written =
		write_output(directory / "session.pending",
					 std::span<const std::uint8_t>(
						 reinterpret_cast<const std::uint8_t*>(bytes.data()), bytes.size()));
	if (!written)
		return written;
	if (!MoveFileExW((directory / "session.pending").c_str(), (directory / "session.json").c_str(),
					 MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH))
		return command_error(ErrorCode::AgenticPublicationFailed);
	return {};
}

[[nodiscard]] Result<Json> launch_windows(const AgenticCommandContext& context,
										  const Json& arguments) {
	const auto executable_text = text_argument(arguments, "executable");
	if (executable_text.empty() || executable_text.find('\0') != std::string_view::npos ||
		text_argument(arguments, "working_directory").find('\0') != std::string_view::npos)
		return command_error(ErrorCode::AgenticInvalidRequest);
	const auto executable = std::filesystem::absolute(std::filesystem::u8path(executable_text));
	const auto working_directory = arguments.find("working_directory")
									   ? std::filesystem::absolute(std::filesystem::u8path(
											 text_argument(arguments, "working_directory")))
									   : context.workspace;
	if (!std::filesystem::is_regular_file(executable) ||
		!std::filesystem::is_directory(working_directory))
		return command_error(ErrorCode::AgenticUnavailable);
	const auto& argv = argument(arguments, "argv");
	if ((argv.kind != Json::Kind::null && argv.kind != Json::Kind::array) ||
		argv.array.size() > 128)
		return command_error(ErrorCode::AgenticInvalidRequest);
	std::size_t argument_bytes = 0;
	for (const auto& value : argv.array) {
		if (value.kind != Json::Kind::string || value.text.find('\0') != std::string::npos)
			return command_error(ErrorCode::AgenticInvalidRequest);
		argument_bytes += value.text.size();
		if (argument_bytes > 65536)
			return command_error(ErrorCode::AgenticBudgetExhausted);
	}
	auto run_id = make_identifier("run");
	if (!run_id)
		return unexpectedError(run_id.error());
	AgenticStore store(context.store_root);
	auto initialized = store.initialize();
	if (!initialized)
		return unexpectedError(initialized.error());
	const auto launch_root = context.store_root / "runtime/launches";
	std::filesystem::create_directories(launch_root);
	const auto launch_directory = launch_root / *run_id;
	if (!std::filesystem::create_directory(launch_directory))
		return command_error(ErrorCode::AgenticPublicationFailed);
	Json configuration = Json::make_object(
		{{"schema", "FlowUi.Agentic.LaunchConfig"},
		 {"schema_version", "1.0"},
		 {"run_id", *run_id},
		 {"executable", wire_path(executable)},
		 {"working_directory", wire_path(working_directory)},
		 {"store_root", wire_path(std::filesystem::absolute(context.store_root))},
		 {"argv", argv.kind == Json::Kind::array ? argv : Json::make_array()},
		 {"log_bytes", decimal(std::clamp<std::uint64_t>(
						   unsigned_argument(arguments, "log_bytes", 1024u * 1024u), 1024,
						   16u * 1024u * 1024u))}});
	const auto config_path = launch_directory / "launch.pending.json";
	const auto config_bytes = write_json(configuration);
	auto written = write_output(
		config_path,
		std::span<const std::uint8_t>(reinterpret_cast<const std::uint8_t*>(config_bytes.data()),
									  config_bytes.size()));
	if (!written)
		return unexpectedError(written.error());
	std::wstring module_name(32768, L'\0');
	const auto module_length =
		GetModuleFileNameW(nullptr, module_name.data(), static_cast<DWORD>(module_name.size()));
	if (module_length == 0 || module_length >= module_name.size())
		return command_error(ErrorCode::AgenticUnavailable);
	module_name.resize(module_length);
	const auto module_path = std::filesystem::path(module_name);
	const auto invocation = quote_windows_argument(wire_path(module_path)) +
							" --flowui-launch-supervisor " +
							quote_windows_argument(wire_path(config_path));
	auto command_line = windows_text(invocation);
	STARTUPINFOW startup{};
	startup.cb = sizeof(startup);
	PROCESS_INFORMATION supervisor{};
	if (!CreateProcessW(module_name.c_str(), command_line.data(), nullptr, nullptr, FALSE,
						DETACHED_PROCESS | CREATE_NEW_PROCESS_GROUP, nullptr,
						working_directory.c_str(), &startup, &supervisor))
		return command_error(ErrorCode::AgenticUnavailable);
	OwnedWindowsHandle supervisor_process(supervisor.hProcess);
	OwnedWindowsHandle supervisor_thread(supervisor.hThread);
	for (std::uint32_t attempt = 0; attempt < 400; ++attempt) {
		std::error_code error;
		if (std::filesystem::is_regular_file(launch_directory / "session.json", error)) {
			auto state = read_text_file(launch_directory / "session.json", 16384);
			if (!state)
				return unexpectedError(state.error());
			auto decoded = parse_json(*state, 16384);
			if (!decoded || text_argument(*decoded, "status") == "failed")
				return command_error(ErrorCode::AgenticUnavailable);
			return Json::make_object(
				{{"run_id", *run_id},
				 {"pid", argument(*decoded, "pid")},
				 {"supervisor_pid", decimal(supervisor.dwProcessId)},
				 {"launch_session", wire_path(launch_directory / "session.json")},
				 {"stdout", wire_path(launch_directory / "stdout.log")},
				 {"stderr", wire_path(launch_directory / "stderr.log")},
				 {"lifetime_policy", "leave_running"},
				 {"next", "Select this run with --instance after the application publishes its "
						  "endpoint."}});
		}
		if (WaitForSingleObject(supervisor_process.get(), 0) == WAIT_OBJECT_0)
			return command_error(ErrorCode::AgenticUnavailable);
		Sleep(5);
	}
	return command_error(ErrorCode::AgenticTimeout);
}

[[nodiscard]] Status windows_supervise(const std::filesystem::path& configuration_path) {
	auto bytes = read_text_file(configuration_path, 131072);
	if (!bytes)
		return unexpectedError(bytes.error());
	auto decoded = parse_json(*bytes, 131072);
	if (!decoded || text_argument(*decoded, "schema") != "FlowUi.Agentic.LaunchConfig" ||
		!valid_identifier(text_argument(*decoded, "run_id")))
		return command_error(ErrorCode::AgenticInvalidRequest);
	const auto launch_directory = configuration_path.parent_path();
	std::error_code remove_error;
	std::filesystem::remove(configuration_path, remove_error);
	if (remove_error)
		return command_error(ErrorCode::AgenticPublicationFailed);
	const auto executable = std::filesystem::u8path(text_argument(*decoded, "executable"));
	const auto working_directory =
		std::filesystem::u8path(text_argument(*decoded, "working_directory"));
	std::string invocation = quote_windows_argument(text_argument(*decoded, "executable"));
	for (const auto& value : argument(*decoded, "argv").array)
		invocation += " " + quote_windows_argument(value.string_or());
	auto command_line = windows_text(invocation);
	if (command_line.size() >= 32767)
		return command_error(ErrorCode::AgenticBudgetExhausted);
	std::vector<std::wstring> environment_entries;
	const auto native_environment = GetEnvironmentStringsW();
	if (native_environment == nullptr)
		return command_error(ErrorCode::AgenticUnavailable);
	for (const wchar_t* entry = native_environment; *entry != L'\0';
		 entry += std::wcslen(entry) + 1) {
		if (_wcsnicmp(entry, L"FLOWUI_AGENTIC_STORE=", std::size(L"FLOWUI_AGENTIC_STORE=") - 1) ==
				0 ||
			_wcsnicmp(entry, L"FLOWUI_AGENTIC_RUN_ID=", std::size(L"FLOWUI_AGENTIC_RUN_ID=") - 1) ==
				0 ||
			_wcsnicmp(entry, L"FLOWUI_AGENTIC_ENDPOINT_ENABLE=",
					  std::size(L"FLOWUI_AGENTIC_ENDPOINT_ENABLE=") - 1) == 0)
			continue;
		environment_entries.emplace_back(entry);
	}
	FreeEnvironmentStringsW(native_environment);
	for (const auto& entry :
		 {"FLOWUI_AGENTIC_STORE=" + std::string(text_argument(*decoded, "store_root")),
		  "FLOWUI_AGENTIC_RUN_ID=" + std::string(text_argument(*decoded, "run_id")),
		  std::string("FLOWUI_AGENTIC_ENDPOINT_ENABLE=1")}) {
		environment_entries.emplace_back(windows_text(entry));
	}
	std::sort(environment_entries.begin(), environment_entries.end(),
			  [](const auto& first, const auto& second) {
				  return _wcsicmp(first.c_str(), second.c_str()) < 0;
			  });
	std::vector<wchar_t> environment;
	for (const auto& entry : environment_entries) {
		environment.insert(environment.end(), entry.begin(), entry.end());
		environment.emplace_back(L'\0');
	}
	environment.emplace_back(L'\0');
	SECURITY_ATTRIBUTES inheritable{sizeof(SECURITY_ATTRIBUTES), nullptr, TRUE};
	HANDLE stdout_read_raw = INVALID_HANDLE_VALUE, stdout_write_raw = INVALID_HANDLE_VALUE;
	HANDLE stderr_read_raw = INVALID_HANDLE_VALUE, stderr_write_raw = INVALID_HANDLE_VALUE;
	if (!CreatePipe(&stdout_read_raw, &stdout_write_raw, &inheritable, 0))
		return command_error(ErrorCode::AgenticUnavailable);
	OwnedWindowsHandle stdout_read(stdout_read_raw), stdout_write(stdout_write_raw);
	if (!CreatePipe(&stderr_read_raw, &stderr_write_raw, &inheritable, 0))
		return command_error(ErrorCode::AgenticUnavailable);
	OwnedWindowsHandle stderr_read(stderr_read_raw), stderr_write(stderr_write_raw);
	SetHandleInformation(stdout_read.get(), HANDLE_FLAG_INHERIT, 0);
	SetHandleInformation(stderr_read.get(), HANDLE_FLAG_INHERIT, 0);
	OwnedWindowsHandle null_input(CreateFileW(L"NUL", GENERIC_READ,
											  FILE_SHARE_READ | FILE_SHARE_WRITE, &inheritable,
											  OPEN_EXISTING, 0, nullptr));
	STARTUPINFOW startup{};
	startup.cb = sizeof(startup);
	startup.dwFlags = STARTF_USESTDHANDLES;
	startup.hStdInput = null_input.get();
	startup.hStdOutput = stdout_write.get();
	startup.hStdError = stderr_write.get();
	PROCESS_INFORMATION application{};
	Json state = Json::make_object({{"schema", "FlowUi.Agentic.LaunchSession"},
									{"schema_version", "1.0"},
									{"run_id", argument(*decoded, "run_id")},
									{"lifetime_policy", "leave_running"},
									{"started_at_utc", utc_timestamp()}});
	if (!CreateProcessW(executable.c_str(), command_line.data(), nullptr, nullptr, TRUE,
						CREATE_UNICODE_ENVIRONMENT | CREATE_NEW_PROCESS_GROUP, environment.data(),
						working_directory.c_str(), &startup, &application)) {
		state.set("status", "failed");
		state.set("native_error", Json::number(GetLastError()));
		const auto saved = windows_launch_state(launch_directory, state);
		if (!saved)
			return saved;
		return command_error(ErrorCode::AgenticUnavailable);
	}
	OwnedWindowsHandle application_process(application.hProcess),
		application_thread(application.hThread);
	stdout_write.reset();
	stderr_write.reset();
	null_input.reset();
	state.set("pid", decimal(application.dwProcessId));
	state.set("status", "running");
	auto saved = windows_launch_state(launch_directory, state);
	if (!saved)
		return saved;
	std::array<HANDLE, 2> streams{stdout_read.get(), stderr_read.get()};
	std::array<std::ofstream, 2> logs{
		std::ofstream(launch_directory / "stdout.log", std::ios::binary),
		std::ofstream(launch_directory / "stderr.log", std::ios::binary)};
	std::array<std::uint64_t, 2> observed{}, retained{};
	const auto maximum_log_bytes = unsigned_argument(*decoded, "log_bytes", 1024u * 1024u);
	std::array<char, 8192> buffer{};
	bool finished = false;
	std::uint64_t exited_ns = 0;
	while (!finished || streams[0] != INVALID_HANDLE_VALUE || streams[1] != INVALID_HANDLE_VALUE) {
		for (std::size_t source_index = 0; source_index < streams.size(); ++source_index) {
			if (streams[source_index] == INVALID_HANDLE_VALUE)
				continue;
			DWORD available = 0;
			if (!PeekNamedPipe(streams[source_index], nullptr, 0, nullptr, &available, nullptr)) {
				streams[source_index] = INVALID_HANDLE_VALUE;
				continue;
			}
			if (available > 0) {
				DWORD amount = 0;
				if (!ReadFile(streams[source_index], buffer.data(),
							  std::min<DWORD>(available, buffer.size()), &amount, nullptr)) {
					streams[source_index] = INVALID_HANDLE_VALUE;
					continue;
				}
				observed[source_index] += amount;
				const auto kept =
					std::min<std::uint64_t>(amount, maximum_log_bytes - retained[source_index]);
				logs[source_index].write(buffer.data(), static_cast<std::streamsize>(kept));
				retained[source_index] += kept;
			}
		}
		if (!finished && WaitForSingleObject(application_process.get(), 10) == WAIT_OBJECT_0) {
			finished = true;
			exited_ns = monotonic_nanoseconds();
		}
		if (finished && monotonic_nanoseconds() - exited_ns > 2000000000ull)
			break;
		if (finished)
			Sleep(2);
	}
	for (auto& log : logs)
		log.close();
	DWORD exit_code = 0;
	if (!GetExitCodeProcess(application_process.get(), &exit_code))
		return command_error(ErrorCode::AgenticUnavailable);
	state.set("status", "exited");
	state.set("exit_code", Json::number(exit_code));
	state.set("finished_at_utc", utc_timestamp());
	state.set("stdout", Json::make_object({{"path", "stdout.log"},
										   {"observed_bytes", decimal(observed[0])},
										   {"retained_bytes", decimal(retained[0])},
										   {"truncated", observed[0] > retained[0]},
										   {"write_failed", logs[0].fail()}}));
	state.set("stderr", Json::make_object({{"path", "stderr.log"},
										   {"observed_bytes", decimal(observed[1])},
										   {"retained_bytes", decimal(retained[1])},
										   {"truncated", observed[1] > retained[1]},
										   {"write_failed", logs[1].fail()}}));
	return windows_launch_state(launch_directory, state);
}
#endif

[[nodiscard]] Result<Json> launch_application(const AgenticCommandContext& context,
											  const Json& arguments) {
#ifdef _WIN32
	return launch_windows(context, arguments);
#else
	const auto executable_text = text_argument(arguments, "executable");
	if (executable_text.empty() || executable_text.find('\0') != std::string_view::npos ||
		text_argument(arguments, "working_directory").find('\0') != std::string_view::npos)
		return command_error(ErrorCode::AgenticInvalidRequest);
	const auto executable = std::filesystem::absolute(std::filesystem::path(executable_text));
	const auto working_directory = arguments.find("working_directory")
									   ? std::filesystem::absolute(std::filesystem::path(
											 text_argument(arguments, "working_directory")))
									   : context.workspace;
	if (!std::filesystem::is_regular_file(executable) ||
		!std::filesystem::is_directory(working_directory) ||
		::access(executable.c_str(), X_OK) != 0)
		return command_error(ErrorCode::AgenticUnavailable);
	const auto& argv = argument(arguments, "argv");
	if (argv.kind != Json::Kind::null && argv.kind != Json::Kind::array)
		return command_error(ErrorCode::AgenticInvalidRequest);
	if (argv.array.size() > 128)
		return command_error(ErrorCode::AgenticBudgetExhausted);
	std::vector<std::string> argument_storage;
	argument_storage.reserve(argv.array.size());
	std::size_t total_argument_bytes = 0;
	for (const auto& value : argv.array) {
		if (value.kind != Json::Kind::string || value.text.find('\0') != std::string::npos)
			return command_error(ErrorCode::AgenticInvalidRequest);
		total_argument_bytes += value.text.size();
		if (total_argument_bytes > 65536)
			return command_error(ErrorCode::AgenticBudgetExhausted);
		argument_storage.emplace_back(value.text);
	}
	auto run_id = make_identifier("run");
	if (!run_id)
		return unexpectedError(run_id.error());
	AgenticStore store(context.store_root);
	auto initialized = store.initialize();
	if (!initialized)
		return unexpectedError(initialized.error());
	const auto launch_root = context.store_root / "runtime/launches";
	std::filesystem::create_directories(launch_root);
	const auto launch_directory = launch_root / *run_id;
	if (!std::filesystem::create_directory(launch_directory))
		return command_error(ErrorCode::AgenticPublicationFailed);
	std::filesystem::permissions(launch_directory, std::filesystem::perms::owner_all);
	std::array<int, 2> handshake_pipe{-1, -1};
	if (::pipe(handshake_pipe.data()) != 0)
		return command_error(ErrorCode::AgenticUnavailable);
	const auto supervisor = ::fork();
	if (supervisor == 0) {
		::close(handshake_pipe[0]);
		supervise_launch(
			executable, argument_storage, working_directory, context.store_root, launch_directory,
			*run_id,
			std::clamp<std::uint64_t>(unsigned_argument(arguments, "log_bytes", 1024u * 1024u),
									  1024, 16u * 1024u * 1024u),
			handshake_pipe[1]);
	}
	::close(handshake_pipe[1]);
	if (supervisor < 0) {
		::close(handshake_pipe[0]);
		return command_error(ErrorCode::AgenticUnavailable);
	}
	pollfd pending{handshake_pipe[0], POLLIN, 0};
	LaunchHandshake handshake;
	const auto ready = ::poll(&pending, 1, 2000);
	const auto bytes = ready > 0 ? ::read(handshake_pipe[0], &handshake, sizeof(handshake)) : -1;
	::close(handshake_pipe[0]);
	if (bytes != sizeof(handshake) || handshake.native_error != 0 || handshake.process_id <= 0)
		return command_error(ErrorCode::AgenticUnavailable);
	return Json::make_object(
		{{"run_id", *run_id},
		 {"pid", decimal(handshake.process_id)},
		 {"supervisor_pid", decimal(supervisor)},
		 {"launch_session", (launch_directory / "session.json").generic_string()},
		 {"stdout", (launch_directory / "stdout.log").generic_string()},
		 {"stderr", (launch_directory / "stderr.log").generic_string()},
		 {"lifetime_policy", "leave_running"},
		 {"next",
		  "Select this run with --instance after the application publishes its endpoint."}});
#endif
}
[[nodiscard]] Result<AgenticCompletedImage> selected_image(const AgenticManifest& manifest,
														   std::string_view image_id,
														   std::uint64_t maximum_bytes) {
	const auto direct = std::find_if(
		manifest.artifacts.begin(), manifest.artifacts.end(), [&](const auto& artifact) {
			return artifact.artifact_id == image_id && artifact.media_type.starts_with("image/");
		});
	const auto inventory =
		std::find_if(manifest.artifacts.begin(), manifest.artifacts.end(),
					 [](const auto& artifact) { return artifact.path == "visual/frames.jsonl"; });
	if (inventory == manifest.artifacts.end())
		return command_error(ErrorCode::AgenticUnavailable);
	auto inventory_bytes =
		read_artifact(manifest, inventory->artifact_id,
					  std::min<std::uint64_t>(maximum_bytes, 16u * 1024u * 1024u));
	if (!inventory_bytes)
		return unexpectedError(inventory_bytes.error());
	const std::string_view records(reinterpret_cast<const char*>(inventory_bytes->data()),
								   inventory_bytes->size());
	std::optional<Json> metadata;
	for (std::size_t cursor = 0; cursor < records.size();) {
		const auto newline = records.find('\n', cursor);
		const auto line = records.substr(
			cursor, newline == std::string_view::npos ? records.size() - cursor : newline - cursor);
		if (!line.empty()) {
			auto record = parse_json(line, 65536u);
			if (!record)
				return unexpectedError(record.error());
			const bool matches =
				text_argument(*record, "image_id") == image_id ||
				(direct != manifest.artifacts.end() &&
				 (text_argument(*record, "path") == direct->path ||
				  text_argument(*record, "source_ppm_artifact_id") == direct->artifact_id));
			if (matches) {
				if (metadata)
					return command_error(ErrorCode::AgenticInvalidScope);
				metadata.emplace(std::move(*record));
			}
		}
		if (newline == std::string_view::npos)
			break;
		cursor = newline + 1u;
	}
	if (!metadata)
		return command_error(ErrorCode::AgenticUnavailable);
	if (text_argument(*metadata, "run_id") != manifest.run_id ||
		text_argument(*metadata, "capture_id") != manifest.capture_id)
		return command_error(ErrorCode::AgenticIntegrityFailed);
	const auto selected =
		direct != manifest.artifacts.end()
			? direct
			: std::find_if(manifest.artifacts.begin(), manifest.artifacts.end(),
						   [&](const auto& artifact) {
							   return artifact.path == text_argument(*metadata, "path") &&
									  artifact.media_type.starts_with("image/");
						   });
	if (selected == manifest.artifacts.end())
		return command_error(ErrorCode::AgenticInvalidScope);
	auto bytes = read_artifact(manifest, selected->artifact_id, maximum_bytes);
	if (!bytes)
		return unexpectedError(bytes.error());
	auto image = decode_image(*bytes, maximum_bytes);
	if (!image)
		return unexpectedError(image.error());
	const auto restored = restore_image_metadata(*image, *metadata);
	if (!restored)
		return unexpectedError(restored.error());
	return image;
}
/** List revisioned tree identities without expanding whole retained geometry into a response. */
[[nodiscard]] Result<Json> tree_metadata_page(const AgenticManifest& manifest,
											  const Json& arguments) {
	auto filter = argument(arguments, "filter");
	if (filter.kind == Json::Kind::null)
		filter = Json::make_object();
	if (filter.kind != Json::Kind::object)
		return command_error(ErrorCode::AgenticInvalidRequest);
	for (const auto key : {"window_id", "app_tick", "window_frame"})
		if (const auto* selected = arguments.find(key))
			filter.set(key, *selected);
	if (const auto* window = arguments.find("window"))
		filter.set("window_id", *window);
	for (const auto& [key, value] : filter.object) {
		if (key == "window_id" || key == "app_tick" || key == "window_frame" ||
			key == "generation" || key == "first_app_tick" || key == "end_app_tick_exclusive") {
			if (!parse_decimal(value))
				return command_error(ErrorCode::AgenticInvalidRequest);
		} else if (key == "complete" || key == "truncated") {
			if (value.kind != Json::Kind::boolean)
				return command_error(ErrorCode::AgenticInvalidRequest);
		} else if (key == "artifact_id" || key == "run_id" || key == "path" || key == "schema" ||
				   key == "schema_version") {
			if (value.kind != Json::Kind::string)
				return command_error(ErrorCode::AgenticInvalidRequest);
		} else
			return command_error(ErrorCode::AgenticInvalidRequest);
	}
	const auto selected_artifact = text_argument(arguments, "artifact");
	const auto selected_tree = text_argument(arguments, "tree");
	if (!selected_artifact.empty() && !selected_tree.empty() && selected_artifact != selected_tree)
		return command_error(ErrorCode::AgenticInvalidRequest);
	const auto selected = selected_tree.empty() ? selected_artifact : selected_tree;
	if (!selected.empty()) {
		const auto found =
			std::find_if(manifest.artifacts.begin(), manifest.artifacts.end(),
						 [&](const auto& artifact) { return artifact.artifact_id == selected; });
		if (found == manifest.artifacts.end() || found->schema != "FlowUi.Agentic.Tree")
			return command_error(ErrorCode::AgenticInvalidScope);
		filter.set("artifact_id", Json(selected));
	}
	const auto prefix = manifest.digest + ':' +
						sha256("query.visual:trees:metadata:1.0" + write_json(filter)) + ':';
	std::uint64_t offset = 0;
	const auto cursor = text_argument(arguments, "cursor");
	if (!cursor.empty()) {
		if (!cursor.starts_with(prefix))
			return command_error(ErrorCode::AgenticStaleGeneration);
		auto decoded = parse_decimal(Json(cursor.substr(prefix.size())));
		if (!decoded || *decoded > manifest.artifacts.size())
			return command_error(ErrorCode::AgenticInvalidRequest);
		offset = *decoded;
	}
	const auto limit =
		std::clamp<std::uint64_t>(unsigned_argument(arguments, "limit", 100), 1, 1000);
	const auto maximum_bytes = byte_limit(arguments) / 2;
	std::uint64_t result_bytes = 2;
	AgenticPage page;
	page.manifest_digest = manifest.digest;
	page.records.array.reserve(
		static_cast<std::size_t>(std::min<std::uint64_t>(limit, manifest.artifacts.size())));
	for (std::size_t index = static_cast<std::size_t>(offset); index < manifest.artifacts.size();
		 ++index) {
		const auto& artifact = manifest.artifacts[index];
		if (artifact.schema != "FlowUi.Agentic.Tree" ||
			(!selected.empty() && artifact.artifact_id != selected))
			continue;
		auto tree = read_artifact_json(manifest, artifact.artifact_id, 16u * 1024u * 1024u);
		if (!tree)
			return unexpectedError(tree.error());
		Json metadata = Json::make_object({{"artifact_id", artifact.artifact_id},
										   {"path", artifact.path},
										   {"sha256", artifact.sha256},
										   {"bytes", decimal(artifact.bytes)},
										   {"schema", artifact.schema},
										   {"schema_version", artifact.schema_version}});
		for (const auto key : {"run_id", "window_id", "app_tick", "window_frame", "generation",
							   "complete", "truncated"})
			if (const auto* value = tree->find(key))
				metadata.set(key, *value);
		for (const auto& [source, destination] :
			 {std::pair{"flow_nodes", "flow_node_count"}, {"clay_nodes", "clay_node_count"}})
			if (const auto* nodes = tree->find(source))
				metadata.set(destination, Json::number(nodes->array.size()));
		if (const auto* status = tree->find("clay_status"))
			metadata.set("clay_status", *status);
		bool matches = true;
		for (const auto& [key, value] : filter.object) {
			if (key == "first_app_tick" || key == "end_app_tick_exclusive") {
				const auto tick = unsigned_argument(metadata, "app_tick", 0);
				matches =
					key == "first_app_tick" ? tick >= value.uint64_or() : tick < value.uint64_or();
			} else {
				const auto* actual = metadata.find(key);
				matches = actual && write_json(*actual) == write_json(value);
			}
			if (!matches)
				break;
		}
		if (!matches)
			continue;
		const auto record_bytes = write_json(metadata).size() + 1;
		if (page.records.array.size() == limit ||
			record_bytes > maximum_bytes - std::min(result_bytes, maximum_bytes)) {
			if (page.records.array.empty())
				return command_error(ErrorCode::AgenticBudgetExhausted);
			page.cursor = prefix + std::to_string(index);
			page.truncated = true;
			break;
		}
		result_bytes += record_bytes;
		page.records.array.emplace_back(std::move(metadata));
	}
	return page_json(std::move(page));
}

[[nodiscard]] Result<Json> offline_command(const AgenticCommandContext& context,
										   const AgenticCommandRequest& request) {
	const auto& arguments = request.arguments;
	const auto operation = std::string_view(request.operation);
	const auto maximum_bytes = byte_limit(arguments);
	AgenticStore store(context.store_root);
	if (operation == "capabilities")
		return Json::make_object(
			{{"mode", "offline"},
			 {"schema_version", "1.0"},
			 {"commands", descriptors_json()},
			 {"knowledge_pack", context.knowledge_path.generic_string()},
			 {"resolved_store", context.store_root.generic_string()},
			 {"limitations", Json::make_array({"no_live_instance_selected",
											   "offline_analysis_does_not_execute_application"})}});
	if (operation == "instances")
		return list_instances(context.store_root, maximum_bytes);
	if (operation == "setup")
		return setup_configuration(context, arguments);
	if (operation == "launch")
		return launch_application(context, arguments);
	if (operation == "interaction.plan") {
		auto plan = interaction_plan_from_json(argument(arguments, "plan"));
		if (!plan)
			return unexpectedError(plan.error());
		AgenticInteractionCapabilities planning_capabilities;
		planning_capabilities.custom_targets = true;
		planning_capabilities.text_focus_observations = true;
		auto compiled = compile_interaction_plan(*plan, planning_capabilities);
		if (!compiled)
			return unexpectedError(compiled.error());
		compiled->limitations.emplace_back(
			"offline_planning_requires_current_runtime_capability_and_geometry_validation");
		return to_json(*compiled);
	}
	if (operation.starts_with("context.")) {
		AgenticKnowledge knowledge(context.knowledge_path, context.source_root);
		if (operation == "context.find")
			return knowledge.find(text_argument(arguments, "intent"), byte_limit(arguments, 12000),
								  static_cast<std::uint32_t>(std::min<std::uint64_t>(
									  unsigned_argument(arguments, "limit", 3), 3)));
		if (operation == "context.validate")
			return knowledge.validate();
		return knowledge.read(
			text_argument(arguments, "topic", text_argument(arguments, "selector")),
			byte_limit(arguments, 12000));
	}
	if (operation == "store.list")
		return store.list_captures();
	if (operation == "store.repair-index") {
		auto status = store.repair_index();
		if (!status)
			return unexpectedError(status.error());
		return Json::make_object({{"repaired", true}});
	}
	if (operation == "store.prune")
		return store.prune(unsigned_argument(arguments, "older_than_seconds", 604800),
						   !argument(arguments, "delete").bool_or(false));
	if (operation == "ingest.build") {
		auto text =
			read_text_file(std::filesystem::path(text_argument(arguments, "log")), 1024u * 1024u);
		if (!text)
			return unexpectedError(text.error());
		auto issues =
			ingest_build_diagnostics(*text, text_argument(arguments, "producer", "compiler"),
									 text_argument(arguments, "build_id", "unknown"));
		if (!issues)
			return unexpectedError(issues.error());
		if (!argument(arguments, "publish").bool_or(false))
			return issues;
		auto initialized = store.initialize();
		if (!initialized)
			return unexpectedError(initialized.error());
		auto run_id = make_identifier("run");
		auto capture_id = make_identifier("cap");
		auto job_id = make_identifier("job");
		if (!run_id || !capture_id || !job_id)
			return command_error(ErrorCode::AgenticPublicationFailed);
		Json provenance = Json::make_object(
			{{"producer", "external_build"},
			 {"build_id", Json(text_argument(arguments, "build_id", "unknown"))}});
		auto reserved = store.reserve_run(*run_id, provenance);
		if (!reserved)
			return unexpectedError(reserved.error());
		AgenticCaptureSnapshot snapshot;
		snapshot.run_id = *run_id;
		snapshot.capture_id = *capture_id;
		snapshot.job_id = *job_id;
		snapshot.request.mode = ExecutionMode::offline;
		snapshot.request.performance = SectionRequirement::omit;
		snapshot.request.runtime_diagnostics = SectionRequirement::omit;
		snapshot.provenance = std::move(provenance);
		snapshot.requested_at_utc = utc_timestamp();
		auto added = add_text_artifact(snapshot, "build_diagnostics", "diagnostics/build.json",
									   "application/json", "FlowUi.Agentic.BuildDiagnostics",
									   write_json(*issues));
		if (!added)
			return unexpectedError(added.error());
		added = add_text_artifact(snapshot, "build_log", "diagnostics/build.log", "text/plain",
								  "FlowUi.Agentic.BuildLog", std::move(*text));
		if (!added)
			return unexpectedError(added.error());
		SectionCoverage coverage;
		coverage.status = SectionStatus::complete;
		snapshot.coverage.emplace_back("build_diagnostics", std::move(coverage));
		auto published = store.publish(std::move(snapshot));
		if (!published)
			return unexpectedError(published.error());
		return Json::make_object({{"capture_id", published->capture_id},
								  {"manifest", published->manifest_path.generic_string()},
								  {"diagnostics", std::move(*issues)}});
	}
	if (operation == "compare") {
		auto baseline = store.resolve_capture(text_argument(arguments, "baseline"));
		auto candidate = store.resolve_capture(text_argument(arguments, "candidate"));
		if (!baseline)
			return unexpectedError(baseline.error());
		if (!candidate)
			return unexpectedError(candidate.error());
		std::optional<AgenticObjective> objective;
		if (arguments.find("objective")) {
			auto decoded = objective_argument(arguments);
			if (!decoded)
				return unexpectedError(decoded.error());
			objective = std::move(*decoded);
		}
		auto result = compare_captures(*baseline, *candidate, objective);
		if (!result)
			return unexpectedError(result.error());
		if (argument(arguments, "publish").bool_or(false)) {
			auto published = publish_comparison(*baseline, *candidate, *result);
			if (!published)
				return unexpectedError(published.error());
			result->set("comparison_manifest", published->generic_string());
		}
		return result;
	}
	if (operation == "objective.check") {
		auto objective = objective_argument(arguments);
		if (!objective)
			return unexpectedError(objective.error());
		std::vector<AgenticManifest> passes;
		const auto& pass_ids = argument(arguments, "passes");
		if (pass_ids.kind == Json::Kind::array) {
			if (pass_ids.array.size() > 100)
				return command_error(ErrorCode::AgenticBudgetExhausted);
			passes.reserve(pass_ids.array.size());
			for (const auto& pass : pass_ids.array) {
				auto manifest = store.resolve_capture(pass.string_or());
				if (!manifest)
					return unexpectedError(manifest.error());
				passes.emplace_back(std::move(*manifest));
			}
		} else {
			auto manifest = store.resolve_capture(
				text_argument(arguments, "capture", text_argument(arguments, "manifest")));
			if (!manifest)
				return unexpectedError(manifest.error());
			passes.emplace_back(std::move(*manifest));
		}
		return check_objective(passes, *objective);
	}
	auto capture_selector =
		text_argument(arguments, "capture", text_argument(arguments, "manifest"));
	if (operation == "visual.compare" && arguments.find("baseline")) {
		const auto baseline = text_argument(arguments, "baseline");
		if (baseline.empty() || (!capture_selector.empty() && capture_selector != baseline))
			return command_error(ErrorCode::AgenticInvalidRequest);
		capture_selector = baseline;
	}
	auto manifest = store.resolve_capture(capture_selector);
	if (!manifest)
		return unexpectedError(manifest.error());
	if (operation == "store.validate")
		return Json::make_object({{"valid", true},
								  {"capture_id", manifest->capture_id},
								  {"manifest_digest", manifest->digest},
								  {"partial", manifest->partial},
								  {"artifact_count", Json::number(manifest->artifacts.size())}});
	if (operation == "artifact.list")
		return Json::make_object({{"capture_id", manifest->capture_id},
								  {"manifest_digest", manifest->digest},
								  {"artifacts", argument(manifest->document, "artifacts")}});
	if (operation == "artifact.read") {
		const auto artifact_id = text_argument(arguments, "artifact");
		const auto artifact = std::find_if(
			manifest->artifacts.begin(), manifest->artifacts.end(),
			[&](const auto& candidate) { return candidate.artifact_id == artifact_id; });
		if (artifact == manifest->artifacts.end())
			return command_error(ErrorCode::AgenticUnavailable);
		if (!artifact->media_type.starts_with("text/") &&
			artifact->media_type.find("json") == std::string::npos)
			return command_error(ErrorCode::AgenticInvalidRequest);
		auto bytes = read_artifact(*manifest, artifact_id, maximum_bytes / 2);
		if (!bytes)
			return unexpectedError(bytes.error());
		return Json::make_object({{"artifact_id", Json(artifact_id)},
								  {"media_type", artifact->media_type},
								  {"sha256", artifact->sha256},
								  {"bytes", decimal(bytes->size())},
								  {"content", std::string(bytes->begin(), bytes->end())},
								  {"truncated", false}});
	}
	if (operation == "artifact.image") {
		auto image = selected_image(
			*manifest, text_argument(arguments, "image", text_argument(arguments, "artifact")),
			64u * 1024u * 1024u);
		if (!image)
			return unexpectedError(image.error());
		const auto& crop = argument(arguments, "crop");
		if (crop.kind != Json::Kind::null && crop.kind != Json::Kind::object)
			return command_error(ErrorCode::AgenticInvalidRequest);
		const auto* flow_selector = arguments.find("crop_flow_node");
		if (flow_selector && crop.kind != Json::Kind::null)
			return command_error(ErrorCode::AgenticInvalidRequest);
		if (flow_selector) {
			if (!image->tree_available || image->tree_artifact_id.empty())
				return command_error(ErrorCode::AgenticUnavailable);
			auto tree = read_artifact_json(*manifest, image->tree_artifact_id, 16u * 1024u * 1024u);
			if (!tree)
				return unexpectedError(tree.error());
			auto rectangle = flow_node_image_crop(*image, *tree, *flow_selector);
			if (!rectangle)
				return unexpectedError(rectangle.error());
			auto cropped = crop_image(*image, *rectangle);
			if (!cropped)
				return unexpectedError(cropped.error());
			image = std::move(cropped);
		}
		if (crop.kind == Json::Kind::object) {
			for (const auto key : {"x", "y", "width", "height"})
				if (const auto* value = crop.find(key);
					value && (value->kind != Json::Kind::number ||
							  value->uint64_or(UINT64_MAX) > UINT32_MAX))
					return command_error(ErrorCode::AgenticInvalidRequest);
			AgenticImageCrop rectangle{
				static_cast<std::uint32_t>(unsigned_argument(crop, "x", 0)),
				static_cast<std::uint32_t>(unsigned_argument(crop, "y", 0)),
				static_cast<std::uint32_t>(unsigned_argument(crop, "width", 0)),
				static_cast<std::uint32_t>(unsigned_argument(crop, "height", 0))};
			auto cropped = crop_image(*image, rectangle);
			if (!cropped)
				return unexpectedError(cropped.error());
			image = std::move(cropped);
		}
		const auto format = text_argument(arguments, "format", "png");
		if (format != "png" && format != "ppm")
			return command_error(ErrorCode::AgenticInvalidRequest);
		auto bytes = format == "ppm" ? encode_ppm(*image) : encode_png(*image);
		if (!bytes)
			return unexpectedError(bytes.error());
		const auto destination = std::filesystem::path(text_argument(arguments, "output"));
		auto written = write_output(destination, *bytes);
		if (!written)
			return unexpectedError(written.error());
		return Json::make_object(
			{{"capture_id", manifest->capture_id},
			 {"image_id", image->image_id},
			 {"output", destination.generic_string()},
			 {"media_type", format == "png" ? "image/png" : "image/x-portable-pixmap"},
			 {"width", Json::number(image->width)},
			 {"height", Json::number(image->height)},
			 {"bytes", decimal(bytes->size())},
			 {"sha256", sha256(*bytes)},
			 {"metadata", image_metadata(*image)},
			 {"crop_transform",
			  Json::make_object(
				  {{"x", Json::number(image->crop_x)},
				   {"y", Json::number(image->crop_y)},
				   {"original_width", Json::number(image->original_width)},
				   {"original_height", Json::number(image->original_height)},
				   {"logical_to_physical_scale_x",
					Json(static_cast<double>(image->logical_to_physical_scale_x))},
				   {"logical_to_physical_scale_y",
					Json(static_cast<double>(image->logical_to_physical_scale_y))}})}});
	}
	if (operation == "visual.compare") {
		auto other = store.resolve_capture(text_argument(arguments, "candidate"));
		if (!other)
			return unexpectedError(other.error());
		auto baseline =
			selected_image(*manifest, text_argument(arguments, "image"), 64u * 1024u * 1024u);
		auto candidate = selected_image(
			*other, text_argument(arguments, "candidate_image", text_argument(arguments, "image")),
			64u * 1024u * 1024u);
		if (!baseline)
			return unexpectedError(baseline.error());
		if (!candidate)
			return unexpectedError(candidate.error());
		auto difference = compare_images(*baseline, *candidate,
										 static_cast<std::uint8_t>(std::min<std::uint64_t>(
											 unsigned_argument(arguments, "tolerance", 0), 255)));
		if (!difference)
			return unexpectedError(difference.error());
		if (arguments.find("output")) {
			auto heatmap = encode_png(difference->heatmap);
			if (!heatmap)
				return unexpectedError(heatmap.error());
			auto written =
				write_output(std::filesystem::path(text_argument(arguments, "output")), *heatmap);
			if (!written)
				return unexpectedError(written.error());
		}
		return Json::make_object(
			{{"mean_absolute_channel_difference",
			  Json(difference->mean_absolute_channel_difference)},
			 {"changed_pixel_fraction", Json(difference->changed_pixel_fraction)},
			 {"changed_pixels", decimal(difference->changed_pixels)},
			 {"maximum_channel_difference", Json::number(difference->maximum_channel_difference)},
			 {"review_status", "requires_visual_and_interaction_review"}});
	}
	if (operation == "visual.contact-sheet") {
		const auto& image_ids = argument(arguments, "images");
		if (image_ids.kind != Json::Kind::array || image_ids.array.empty() ||
			image_ids.array.size() > 16)
			return command_error(ErrorCode::AgenticInvalidRequest);
		std::vector<AgenticCompletedImage> images;
		images.reserve(image_ids.array.size());
		for (const auto& image_id : image_ids.array) {
			auto image = selected_image(*manifest, image_id.string_or(), 64u * 1024u * 1024u);
			if (!image)
				return unexpectedError(image.error());
			images.emplace_back(std::move(*image));
		}
		auto sheet =
			make_contact_sheet(images, static_cast<std::uint32_t>(std::clamp<std::uint64_t>(
										   unsigned_argument(arguments, "columns", 4), 1, 16)));
		if (!sheet)
			return unexpectedError(sheet.error());
		auto bytes = encode_png(*sheet);
		if (!bytes)
			return unexpectedError(bytes.error());
		auto written =
			write_output(std::filesystem::path(text_argument(arguments, "output")), *bytes);
		if (!written)
			return unexpectedError(written.error());
		return Json::make_object({{"images", image_ids},
								  {"order", "row_major"},
								  {"output", argument(arguments, "output")},
								  {"width", Json::number(sheet->width)},
								  {"height", Json::number(sheet->height)}});
	}
	if (operation == "query.performance" && !arguments.find("artifact")) {
		auto frames = read_frames(*manifest);
		if (!frames)
			return unexpectedError(frames.error());
		std::optional<std::uint64_t> window;
		if (arguments.find("window"))
			window = unsigned_argument(arguments, "window", 0);
		const auto top = unsigned_argument(arguments, "top", 10);
		if (top == 0 || top > 100)
			return command_error(ErrorCode::AgenticInvalidRequest);
		const auto* source_option = arguments.find("include_source");
		if (source_option && source_option->kind != Json::Kind::boolean)
			return command_error(ErrorCode::AgenticInvalidRequest);
		auto result = analyze_performance(*frames, window);
		if (result) {
			auto contributors =
				analyze_timing_artifact(*manifest, window, static_cast<std::uint32_t>(top),
										source_option && source_option->boolean);
			if (contributors)
				result->set("contributors", std::move(*contributors));
			else if (contributors.error().code == ErrorCode::AgenticUnavailable)
				result->set(
					"contributors",
					Json::make_object(
						{{"status", "unavailable"},
						 {"reason", "native_timing_artifact_not_retained_or_not_requested"}}));
			else
				return unexpectedError(contributors.error());
			result->set("capture_id", manifest->capture_id);
			result->set("manifest_digest", manifest->digest);
		}
		return result;
	}
	if (operation == "query.visual" && text_argument(arguments, "kind") == "trees")
		return tree_metadata_page(*manifest, arguments);
	if (operation == "query.visual" && text_argument(arguments, "kind") == "subtree") {
		const auto tree_id = text_argument(arguments, "tree", text_argument(arguments, "artifact"));
		const auto& selector = argument(arguments, "flow_node");
		if (tree_id.empty() || selector.kind == Json::Kind::null)
			return command_error(ErrorCode::AgenticInvalidRequest);
		const auto inventoried = std::find_if(
			manifest->artifacts.begin(), manifest->artifacts.end(), [&](const auto& artifact) {
				return artifact.artifact_id == tree_id && artifact.schema == "FlowUi.Agentic.Tree";
			});
		if (inventoried == manifest->artifacts.end())
			return command_error(ErrorCode::AgenticInvalidScope);
		auto tree = read_artifact_json(*manifest, tree_id, 16u * 1024u * 1024u);
		if (!tree)
			return unexpectedError(tree.error());
		auto selected = select_flow_node(*tree, selector);
		if (!selected)
			return unexpectedError(selected.error());
		const auto* flow_nodes = tree->find("flow_nodes");
		const Json* selected_node = nullptr;
		for (const auto& node : flow_nodes->array)
			if (unsigned_argument(node, "index", UINT64_MAX) == *selected)
				selected_node = &node;
		const auto end =
			selected_node ? unsigned_argument(*selected_node, "subtree_end_exclusive", 0) : 0;
		if (end <= *selected)
			return command_error(ErrorCode::AgenticIncompleteCapture);
		const auto limit =
			std::clamp<std::uint64_t>(unsigned_argument(arguments, "limit", 100), 1, 1000);
		std::uint64_t retained = 0, retained_bytes = 2048u;
		bool truncated = false;
		Json flows = Json::make_array(), clay = Json::make_array();
		const auto append_bounded = [&](Json& destination, const Json& node) {
			const auto bytes = write_json(node).size();
			if (retained >= limit ||
				bytes > maximum_bytes - std::min(retained_bytes, maximum_bytes)) {
				truncated = true;
				return;
			}
			destination.array.emplace_back(node);
			++retained;
			retained_bytes += bytes;
		};
		for (const auto& node : flow_nodes->array) {
			const auto index = unsigned_argument(node, "index", UINT64_MAX);
			if (index >= *selected && index < end)
				append_bounded(flows, node);
		}
		if (flows.array.empty())
			return command_error(ErrorCode::AgenticBudgetExhausted);
		const auto* clay_nodes = tree->find("clay_nodes");
		const auto clay_root = unsigned_argument(*selected_node, "clay_root", UINT64_MAX);
		if (clay_nodes && clay_nodes->kind == Json::Kind::array && clay_root != UINT64_MAX) {
			std::uint64_t clay_end = 0;
			for (const auto& node : clay_nodes->array)
				if (unsigned_argument(node, "index", UINT64_MAX) == clay_root)
					clay_end = unsigned_argument(node, "subtree_end_exclusive", 0);
			for (const auto& node : clay_nodes->array) {
				const auto index = unsigned_argument(node, "index", UINT64_MAX);
				if (index >= clay_root && index < clay_end)
					append_bounded(clay, node);
			}
		}
		auto result = Json::make_object({{"artifact_id", Json(tree_id)},
										 {"manifest_digest", manifest->digest},
										 {"run_id", argument(*tree, "run_id")},
										 {"window_id", argument(*tree, "window_id")},
										 {"window_frame", argument(*tree, "window_frame")},
										 {"app_tick", argument(*tree, "app_tick")},
										 {"generation", argument(*tree, "generation")},
										 {"tree_complete", argument(*tree, "complete")},
										 {"selected_node_index", Json::number(*selected)},
										 {"flow_nodes", std::move(flows)},
										 {"clay_nodes", std::move(clay)},
										 {"truncated", truncated}});
		if (write_json(result).size() > maximum_bytes)
			return command_error(ErrorCode::AgenticBudgetExhausted);
		return result;
	}
	std::string artifact_id(text_argument(arguments, "artifact"));
	if (artifact_id.empty()) {
		std::string path;
		if (operation == "query.diagnostics")
			path = text_argument(arguments, "domain", "runtime") == "build"
					   ? "diagnostics/build.json"
					   : "diagnostics/runtime.json";
		else if (operation == "query.memory")
			path = text_argument(arguments, "kind", "samples") == "events" ? "memory/events.jsonl"
																		   : "memory/samples.csv";
		else if (operation == "query.visual") {
			const auto kind = text_argument(arguments, "kind", "frames");
			path = kind == "themes"		 ? "visual/themes.json"
				   : kind == "catalogue" ? "visual/catalogue.json"
										 : "visual/frames.jsonl";
		} else if (operation == "interaction.query") {
			const auto kind = text_argument(arguments, "kind", "results");
			if (kind != "plan" && kind != "inputs" && kind != "observations" && kind != "results")
				return command_error(ErrorCode::AgenticInvalidRequest);
			path = "interaction/" + std::string(kind) +
				   (kind == "inputs" || kind == "observations" ? ".jsonl" : ".json");
		}
		const auto found =
			std::find_if(manifest->artifacts.begin(), manifest->artifacts.end(),
						 [&](const auto& artifact) { return artifact.path == path; });
		if (found == manifest->artifacts.end())
			return command_error(ErrorCode::AgenticUnavailable);
		artifact_id = found->artifact_id;
	}
	auto filter = argument(arguments, "filter");
	if (filter.kind == Json::Kind::null)
		filter = Json::make_object();
	for (const auto key : {"window_id", "app_tick", "window_frame", "severity", "source_id",
						   "image_id", "entity_id", "job_id", "plan_digest", "sequence", "offset"})
		if (const auto* value = arguments.find(key))
			filter.set(key, *value);
	if (const auto* window = arguments.find("window"))
		filter.set("window_id", *window);
	auto page = query_artifact(
		*manifest, artifact_id, filter,
		static_cast<std::uint32_t>(std::clamp<std::uint64_t>(
			unsigned_argument(arguments, "limit", unsigned_argument(arguments, "top", 100)), 1,
			1000)),
		maximum_bytes / 2, text_argument(arguments, "cursor"));
	if (!page)
		return unexpectedError(page.error());
	return page_json(std::move(*page));
}

[[nodiscard]] bool has_redacted_interaction_input(const Json& value) noexcept {
	if (value.kind == Json::Kind::object) {
		for (const auto& [key, member] : value.object) {
			if ((key == "text_redacted" || key == "non_replayable") && member.bool_or(false))
				return true;
			if (has_redacted_interaction_input(member))
				return true;
		}
	} else if (value.kind == Json::Kind::array)
		for (const auto& member : value.array)
			if (has_redacted_interaction_input(member))
				return true;
	return false;
}

[[nodiscard]] Result<Json> retained_interaction_plan(const AgenticCommandContext& context,
													 const Json& selector, bool rebind_run) {
	if (selector.kind != Json::Kind::object || selector.object.size() != 3)
		return command_error(ErrorCode::AgenticInvalidRequest);
	for (const auto& [key, value] : selector.object)
		if ((key != "capture" && key != "artifact" && key != "sha256") ||
			value.kind != Json::Kind::string)
			return command_error(ErrorCode::AgenticInvalidRequest);
	AgenticStore store(context.store_root);
	auto manifest = store.resolve_capture(text_argument(selector, "capture"));
	if (!manifest)
		return unexpectedError(manifest.error());
	const auto artifact_id = text_argument(selector, "artifact");
	const auto found =
		std::find_if(manifest->artifacts.begin(), manifest->artifacts.end(),
					 [&](const auto& artifact) { return artifact.artifact_id == artifact_id; });
	if (found == manifest->artifacts.end())
		return command_error(ErrorCode::AgenticUnavailable);
	if (found->schema != "FlowUi.Agentic.InteractionPlan" ||
		found->sha256 != text_argument(selector, "sha256"))
		return command_error(ErrorCode::AgenticIntegrityFailed);
	auto bytes = read_artifact(*manifest, artifact_id, 1024u * 1024u);
	if (!bytes)
		return unexpectedError(bytes.error());
	auto document =
		parse_json(std::string_view(reinterpret_cast<const char*>(bytes->data()), bytes->size()),
				   1024u * 1024u);
	if (!document)
		return unexpectedError(document.error());
	if (has_redacted_interaction_input(*document))
		return command_error(ErrorCode::AgenticIncompleteCapture);
	auto plan = interaction_plan_from_json(*document);
	if (!plan)
		return unexpectedError(plan.error());
	const auto source_run_id = plan->run_id;
	if (rebind_run) {
		if (plan->clock != AgenticInteractionClock::window_input_frames)
			return command_error(ErrorCode::AgenticInvalidRequest);
		plan->run_id.clear();
	}
	return Json::make_object(
		{{"plan", to_json(*plan)},
		 {"replay_source", Json::make_object({{"run_id", source_run_id},
											  {"artifact_id", found->artifact_id},
											  {"artifact_sha256", found->sha256},
											  {"manifest_digest", manifest->digest},
											  {"capture_id", manifest->capture_id}})}});
}

[[nodiscard]] int error_exit_code(ErrorCode code) noexcept {
	switch (code) {
	case ErrorCode::AgenticInvalidRequest:
	case ErrorCode::AgenticInvalidScope:
		return 2;
	case ErrorCode::AgenticUnavailable:
	case ErrorCode::AgenticBusy:
		return 3;
	case ErrorCode::AgenticCanceled:
	case ErrorCode::AgenticTimeout:
		return 5;
	case ErrorCode::AgenticIncompleteCapture:
	case ErrorCode::AgenticIncompatibleComparison:
	case ErrorCode::AgenticStaleGeneration:
		return 6;
	default:
		return 4;
	}
}
} // namespace

std::string quote_windows_argument(std::string_view argument) {
	std::string quoted;
	quoted.reserve(argument.size() + 2);
	quoted.push_back('"');
	std::size_t pending_backslashes = 0;
	for (const auto character : argument) {
		if (character == '\\') {
			++pending_backslashes;
			continue;
		}
		if (character == '"') {
			quoted.append(pending_backslashes * 2 + 1, '\\');
			quoted.push_back('"');
		} else {
			quoted.append(pending_backslashes, '\\');
			quoted.push_back(character);
		}
		pending_backslashes = 0;
	}
	quoted.append(pending_backslashes * 2, '\\');
	quoted.push_back('"');
	return quoted;
}

#ifdef _WIN32
Status run_agentic_launch_supervisor(const std::filesystem::path& configuration) noexcept {
	try {
		return windows_supervise(configuration);
	} catch (...) {
		return command_error(ErrorCode::AgenticPublicationFailed);
	}
}
#endif

std::span<const AgenticCommandDescriptor> agentic_command_registry() noexcept {
	return registry;
}

Status validate_interaction_command_arguments(const AgenticCommandRequest& request) noexcept {
	const std::string_view operation = request.operation;
	if (!operation.starts_with("interaction.") && operation != "action.list" &&
		operation != "action.invoke")
		return {};
	const auto invalid = [] {
		return unexpectedError(agentic_error(AgenticError::invalid_request));
	};
	if (std::none_of(registry.begin(), registry.end(),
					 [&](const auto& descriptor) { return descriptor.operation == operation; }))
		return invalid();
	const auto& arguments = request.arguments;
	if (arguments.kind != Json::Kind::object)
		return invalid();
	const auto permitted = [&](std::string_view field) noexcept {
		if (field == "instance" || field == "max_bytes")
			return true;
		if (operation == "interaction.capabilities")
			return false;
		if (operation == "interaction.targets")
			return field == "window_id" || field == "target" || field == "limit" ||
				   field == "cursor";
		if (operation == "interaction.plan")
			return field == "plan";
		if (operation == "interaction.start")
			return field == "plan" || field == "plan_artifact" || field == "capture" ||
				   field == "idempotency_key" || field == "rebind_run" || field == "replay_source";
		if (operation == "interaction.record.start")
			return field == "window_id" || field == "max_frames" || field == "deadline_ns" ||
				   field == "include_text" || field == "capture" || field == "idempotency_key";
		if (operation == "interaction.record.stop")
			return field == "job_id";
		if (operation == "interaction.query")
			return field == "job_id" || field == "capture" || field == "manifest" ||
				   field == "kind" || field == "limit" || field == "cursor" ||
				   field == "window_id" || field == "app_tick" || field == "window_frame" ||
				   field == "sequence" || field == "offset" || field == "plan_digest";
		if (operation == "action.list")
			return field == "limit" || field == "cursor";
		if (operation == "action.invoke")
			return field == "action_id" || field == "revision" || field == "arguments" ||
				   field == "window_id" || field == "deadline_ns" || field == "capture" ||
				   field == "idempotency_key";
		return false;
	};
	for (const auto& [field, value] : arguments.object) {
		if (!permitted(field))
			return invalid();
		if (field == "window_id" || field == "max_frames" || field == "deadline_ns" ||
			field == "revision" || field == "app_tick" || field == "window_frame" ||
			field == "sequence" || field == "offset") {
			const auto parsed = parse_decimal(value);
			if (!parsed ||
				((field == "window_id" || field == "max_frames" || field == "deadline_ns" ||
				  field == "revision") &&
				 !*parsed) ||
				(field == "window_id" && *parsed > UINT32_MAX) ||
				(field == "max_frames" && *parsed > 65536) ||
				(field == "deadline_ns" && *parsed > 600'000'000'000ull))
				return invalid();
		} else if (field == "limit") {
			if (value.kind != Json::Kind::number || value.uint64_or(UINT64_MAX) < 1 ||
				value.uint64_or(UINT64_MAX) > 1000)
				return invalid();
		} else if (field == "max_bytes") {
			if ((value.kind != Json::Kind::number && value.kind != Json::Kind::string) ||
				value.uint64_or(UINT64_MAX) < 1024 || value.uint64_or(UINT64_MAX) > 1024u * 1024u)
				return invalid();
		} else if (field == "include_text" || field == "rebind_run") {
			if (value.kind != Json::Kind::boolean)
				return invalid();
		} else if (field == "plan" || field == "plan_artifact" || field == "target" ||
				   field == "arguments" || field == "replay_source" ||
				   (field == "capture" && operation != "interaction.query")) {
			if (value.kind != Json::Kind::object)
				return invalid();
			if (field == "plan_artifact" || field == "replay_source") {
				const auto expected_members = field == "plan_artifact" ? 3u : 5u;
				if (value.object.size() != expected_members)
					return invalid();
				for (const auto& [key, member] : value.object) {
					const bool digest_field =
						key == "sha256" || key == "artifact_sha256" || key == "manifest_digest";
					const bool allowed =
						field == "plan_artifact"
							? (key == "capture" || key == "artifact" || key == "sha256")
							: (key == "run_id" || key == "artifact_id" ||
							   key == "artifact_sha256" || key == "manifest_digest" ||
							   key == "capture_id");
					if (!allowed || member.kind != Json::Kind::string || member.text.empty() ||
						member.text.size() > 4096)
						return invalid();
					if (digest_field &&
						(member.text.size() != 64 ||
						 !std::all_of(member.text.begin(), member.text.end(), [](char character) {
							 return (character >= '0' && character <= '9') ||
									(character >= 'a' && character <= 'f');
						 })))
						return invalid();
					if (!digest_field && key != "capture" && !valid_identifier(member.text))
						return invalid();
				}
			}
		} else {
			if (value.kind != Json::Kind::string || value.text.size() > 4096)
				return invalid();
			if ((field == "instance" || field == "job_id" || field == "action_id") &&
				!valid_identifier(value.text))
				return invalid();
			if (field == "idempotency_key" && (value.text.empty() || value.text.size() > 128))
				return invalid();
			if ((field == "capture" || field == "manifest") && value.text.empty())
				return invalid();
			if (field == "cursor") {
				if (operation == "interaction.targets" && !value.text.empty())
					return invalid();
				if (operation == "action.list" && !value.text.empty() && !parse_decimal(value))
					return invalid();
			}
			if (field == "kind" && value.text != "plan" && value.text != "inputs" &&
				value.text != "observations" && value.text != "results")
				return invalid();
			if (field == "plan_digest" &&
				(value.text.size() != 64 ||
				 !std::all_of(value.text.begin(), value.text.end(), [](char character) {
					 return (character >= '0' && character <= '9') ||
							(character >= 'a' && character <= 'f');
				 })))
				return invalid();
		}
	}
	if (operation == "interaction.plan" && !arguments.find("plan"))
		return invalid();
	if (operation == "interaction.start") {
		if (static_cast<bool>(arguments.find("plan")) ==
			static_cast<bool>(arguments.find("plan_artifact")))
			return invalid();
		if (const auto* plan = arguments.find("plan");
			plan && argument(arguments, "rebind_run").bool_or(false)) {
			if (const auto* clock = plan->find("clock");
				clock && clock->string_or() != "window_input_frames")
				return invalid();
		}
	}
	if (operation == "interaction.record.stop" && !arguments.find("job_id"))
		return invalid();
	if (operation == "action.invoke" &&
		(!arguments.find("action_id") || !arguments.find("revision")))
		return invalid();
	if (operation == "interaction.query") {
		const auto selectors = static_cast<unsigned>(arguments.find("job_id") != nullptr) +
							   static_cast<unsigned>(arguments.find("capture") != nullptr) +
							   static_cast<unsigned>(arguments.find("manifest") != nullptr);
		if (selectors != 1)
			return invalid();
	}
	return {};
}

Result<AgenticCommandRequest> decode_command_request(const Json& value) noexcept {
	try {
		if (value.kind != Json::Kind::object || write_json(value).size() > 1024u * 1024u)
			return command_error(ErrorCode::AgenticInvalidRequest);
		AgenticCommandRequest request;
		request.operation = text_argument(value, "operation");
		request.request_id = text_argument(value, "request_id");
		if (request.request_id.empty()) {
			auto identifier = make_identifier("request");
			if (!identifier)
				return unexpectedError(identifier.error());
			request.request_id = std::move(*identifier);
		}
		if (!valid_identifier(request.request_id))
			return command_error(ErrorCode::AgenticInvalidRequest);
		const auto descriptor =
			std::find_if(registry.begin(), registry.end(), [&](const auto& candidate) {
				return candidate.operation == request.operation;
			});
		if (descriptor == registry.end())
			return command_error(ErrorCode::AgenticInvalidRequest);
		if (const auto* arguments = value.find("arguments"))
			request.arguments = *arguments;
		if (request.arguments.kind != Json::Kind::object)
			return command_error(ErrorCode::AgenticInvalidRequest);
		if (const auto* maximum_bytes = request.arguments.find("max_bytes");
			maximum_bytes &&
			(maximum_bytes->uint64_or() < 1024 || maximum_bytes->uint64_or() > 1024u * 1024u))
			return command_error(ErrorCode::AgenticInvalidRequest);
		return request;
	} catch (...) {
		return command_error(ErrorCode::AgenticBudgetExhausted);
	}
}

Json encode_command_request(const AgenticCommandRequest& request) {
	return Json::make_object({{"schema", "FlowUi.Agentic.CommandRequest"},
							  {"schema_version", "1.0"},
							  {"request_id", request.request_id},
							  {"operation", request.operation},
							  {"arguments", request.arguments}});
}

Json command_success(const AgenticCommandRequest& request, Json result, std::string_view status) {
	return Json::make_object({{"schema", "FlowUi.Agentic.CommandResult"},
							  {"schema_version", "1.0"},
							  {"request_id", request.request_id},
							  {"operation", request.operation},
							  {"operation_status", Json(status)},
							  {"exit_code", Json::number(0)},
							  {"result", std::move(result)},
							  {"limitations", Json::make_array()}});
}

Json command_failure(const AgenticCommandRequest& request, int exit_code, std::string_view code,
					 std::string_view message) {
	auto envelope = command_success(request, Json{}, "failed");
	envelope.set("exit_code", Json::number(static_cast<std::uint64_t>(exit_code)));
	envelope.set("error", Json::make_object({{"code", Json(code)}, {"message", Json(message)}}));
	return envelope;
}

AgenticCommandResponse execute_agentic_command(const AgenticCommandContext& context,
											   const AgenticCommandRequest& request) noexcept {
	try {
		auto validated = decode_command_request(encode_command_request(request));
		if (!validated)
			return {command_failure(request, 2, "invalid_request", validated.error().message()), 2};
		auto interaction_arguments = validate_interaction_command_arguments(request);
		if (!interaction_arguments)
			return {command_failure(request, 2, "invalid_arguments",
									interaction_arguments.error().message()),
					2};
		const auto descriptor =
			std::find_if(registry.begin(), registry.end(), [&](const auto& candidate) {
				return candidate.operation == request.operation;
			});
		const bool use_live =
			descriptor->requires_live_app ||
			((request.operation == "capabilities" || request.operation == "interaction.query") &&
			 context.live_handler);
		auto resolved_request = request;
		if (request.operation == "interaction.start" && request.arguments.find("plan_artifact")) {
			if (request.arguments.find("plan"))
				return {command_failure(
							request, 2, "ambiguous_plan",
							"Select a source plan or one checksum-selected inventoried plan."),
						2};
			auto plan =
				retained_interaction_plan(context, argument(request.arguments, "plan_artifact"),
										  argument(request.arguments, "rebind_run").bool_or(false));
			if (!plan) {
				const auto exit_code = error_exit_code(plan.error().code);
				return {command_failure(request, exit_code, "retained_plan_unavailable",
										plan.error().message()),
						exit_code};
			}
			std::erase_if(resolved_request.arguments.object,
						  [](const auto& member) { return member.first == "plan_artifact"; });
			resolved_request.arguments.set("plan", std::move(*plan->find("plan")));
			resolved_request.arguments.set("replay_source",
										   std::move(*plan->find("replay_source")));
		}
		if ((request.operation == "capture.plan" || request.operation == "capture.start" ||
			 request.operation == "scenario.run") &&
			request.arguments.find("objective")) {
			auto objective = objective_argument(request.arguments);
			if (!objective)
				return {
					command_failure(request, 2, "invalid_objective", objective.error().message()),
					2};
			resolved_request.arguments.set("objective", to_json(*objective));
		}
		Result<Json> result =
			use_live ? (context.live_handler
							? context.live_handler(resolved_request)
							: Result<Json>(command_error(ErrorCode::AgenticUnavailable)))
					 : offline_command(context, request);
		if (!result) {
			const auto exit_code = error_exit_code(result.error().code);
			return {command_failure(request, exit_code, "command_failed", result.error().message()),
					exit_code};
		}
		Json envelope;
		if (use_live && text_argument(*result, "schema") == "FlowUi.Agentic.CommandResult")
			envelope = std::move(*result);
		else
			envelope = command_success(request, std::move(*result));
		envelope.set("request_id", request.request_id);
		envelope.set("operation", request.operation);
		if (!envelope.find("result"))
			envelope.set("result", Json{});
		if (!envelope.find("limitations"))
			envelope.set("limitations", Json::make_array());
		int exit_code = static_cast<int>(unsigned_argument(envelope, "exit_code", 0));
		if (request.operation == "objective.check" &&
			argument(request.arguments, "require_pass").bool_or(false)) {
			const auto verdict = text_argument(argument(envelope, "result"), "verdict");
			if (verdict == "fail")
				exit_code = 7;
			else if (verdict != "pass")
				exit_code = 6;
			envelope.set("exit_code", Json::number(static_cast<std::uint64_t>(exit_code)));
		}
		envelope.set("resolved_store", context.store_root.generic_string());
		const auto maximum_bytes = byte_limit(request.arguments, descriptor->default_maximum_bytes);
		if (write_json(envelope).size() > maximum_bytes)
			std::erase_if(envelope.object,
						  [](const auto& member) { return member.first == "resolved_store"; });
		if (write_json(envelope).size() > maximum_bytes)
			return {command_failure(request, 4, "result_budget_exhausted",
									"Use a smaller result page or a narrower artifact selector."),
					4};
		return {std::move(envelope), exit_code};
	} catch (...) {
		try {
			return {
				command_failure(request, 4, "execution_failed",
								"Command execution could not complete within its resource bounds."),
				4};
		} catch (...) {
			return {Json{}, 4};
		}
	}
}

} // namespace FlowUi::devSystems::agentic
