#pragma once
#include "FlowUi/BuildConfig.hpp"
#if FLOW_UI_AGENTIC_WORKFLOW

#include "devSystems/devAgenticWorkflow/AgenticJson.hpp"

#include <filesystem>
#include <functional>
#include <span>

namespace FlowUi::devSystems::agentic {

/** Stable operation classification shared by CLI, local IPC and the MCP adapter. */
enum class AgenticCommandAccess : std::uint8_t { read, collect, mutate, administer };

/** One command contract. Handlers never select an arbitrary executable or file reader. */
struct AgenticCommandDescriptor {
	std::string_view operation;
	std::string_view description;
	std::string_view capability;
	std::uint32_t default_maximum_bytes = 65536;
	std::uint32_t timeout_ms = 2000;
	AgenticCommandAccess access = AgenticCommandAccess::read;
	bool requires_live_app = false;
};

/** Owned decoded request; asynchronous handlers retain no caller string views. */
struct AgenticCommandRequest {
	Json arguments = Json::make_object();
	std::string request_id;
	std::string operation;
};

/** Versioned JSON result and the documented CLI exit code. */
struct AgenticCommandResponse {
	Json envelope;
	int exit_code = 0;
};

/** Invoke the selected App's same command contract through authenticated local IPC. */
using AgenticLiveCommandHandler = std::function<Result<Json>(const AgenticCommandRequest&)>;

/** Offline dependencies and an optional selected live endpoint; no App/GPU ownership. */
struct AgenticCommandContext {
	std::filesystem::path store_root;
	std::filesystem::path knowledge_path;
	std::filesystem::path source_root;
	std::filesystem::path workspace;
	AgenticLiveCommandHandler live_handler;
};

/** Return every implemented operation's single authoritative descriptor. */
[[nodiscard]] std::span<const AgenticCommandDescriptor> agentic_command_registry() noexcept;
/** Decode and validate the bounded common request envelope. */
[[nodiscard]] Result<AgenticCommandRequest> decode_command_request(const Json& value) noexcept;
/** Validate finite interaction/action command arguments before defaults or runtime side effects. */
[[nodiscard]] Status
validate_interaction_command_arguments(const AgenticCommandRequest& request) noexcept;
/** Encode a validated request without exposing C++ callbacks or layout details. */
[[nodiscard]] Json encode_command_request(const AgenticCommandRequest& request);
/** Construct a small successful response, also usable by safe-point live handlers. */
[[nodiscard]] Json command_success(const AgenticCommandRequest& request, Json result,
								   std::string_view operation_status = "completed");
/** Construct an explicit failure response with a stable machine-readable reason. */
[[nodiscard]] Json command_failure(const AgenticCommandRequest& request, int exit_code,
								   std::string_view code, std::string_view message);
/** Dispatch through the same registry for CLI and every protocol adapter. */
[[nodiscard]] AgenticCommandResponse
execute_agentic_command(const AgenticCommandContext& context,
						const AgenticCommandRequest& request) noexcept;

/** Quote one argv entry according to the Windows C runtime command-line rules. */
[[nodiscard]] std::string quote_windows_argument(std::string_view argument);
#ifdef _WIN32
/** Internal detached launch monitor entry; consumes an owner-private one-use configuration. */
[[nodiscard]] Status
run_agentic_launch_supervisor(const std::filesystem::path& configuration) noexcept;
#endif

} // namespace FlowUi::devSystems::agentic

#endif // FLOW_UI_AGENTIC_WORKFLOW
