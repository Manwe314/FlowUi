#pragma once

/** @file
 * @brief Public AgenticCommands declarations for optional development services.
 */
#include "FlowUi/Public/BuildConfig.hpp"
#if FLOW_UI_AGENTIC_WORKFLOW

#include "devSystems/devAgenticWorkflow/AgenticJson.hpp"

#include <filesystem>
#include <functional>
#include <span>

namespace FlowUi::devSystems::agentic {

/** Stable operation classification shared by CLI, local IPC and the MCP adapter. */
enum class AgenticCommandAccess : std::uint8_t {
	read, /**< Read retained or declared state. */
	collect, /**< Admit bounded evidence collection. */
	mutate, /**< Request explicitly permitted application mutation. */
	administer /**< Manage evidence store or service administration. */
};

/** One command contract. Handlers never select an arbitrary executable or file reader. */
struct AgenticCommandDescriptor {
	/** @brief Static command operation name used by CLI and protocol adapters. */
	std::string_view operation;
	/** @brief Human-readable declaration metadata; owned when stored as std::string. */
	std::string_view description;
	/** @brief Capability name required to dispatch this command. */
	std::string_view capability;
	/** @brief Default bounded command response size, in bytes. */
	std::uint32_t default_maximum_bytes = 65536;
	/** @brief Bounded command transport wait, in milliseconds. */
	std::uint32_t timeout_ms = 2000;
	/** @brief Operation permission class used by command dispatch. */
	AgenticCommandAccess access = AgenticCommandAccess::read;
	/** @brief Whether this command needs an authenticated live application endpoint. */
	bool requires_live_app = false;
};

/** Owned decoded request; asynchronous handlers retain no caller string views. */
struct AgenticCommandRequest {
	/**
	 * @brief Owned request or semantic action arguments; callback objects are never serialized.
	 */
	Json arguments = Json::make_object();
	/** @brief Caller retry and response correlation identity. */
	std::string request_id;
	/** @brief Owned command operation name validated against the registry. */
	std::string operation;
};

/** Versioned JSON result and the documented CLI exit code. */
struct AgenticCommandResponse {
	/** @brief Owned versioned command response, including transport status and result. */
	Json envelope;
	/** @brief CLI process exit code associated with the versioned response. */
	int exit_code = 0;
};

/** Invoke the selected App's same command contract through authenticated local IPC. */
using AgenticLiveCommandHandler = std::function<Result<Json>(const AgenticCommandRequest&)>;

/** Offline dependencies and an optional selected live endpoint; no App/GPU ownership. */
struct AgenticCommandContext {
	/** @brief Evidence store root used by offline command handling. */
	std::filesystem::path store_root;
	/** @brief Installed semantic knowledge pack root used by offline command handling. */
	std::filesystem::path knowledge_path;
	/**
	 * @brief Optional matching source checkout root used for provenance or knowledge validation.
	 */
	std::filesystem::path source_root;
	/** @brief Working directory used for explicitly bounded offline command operations. */
	std::filesystem::path workspace;
	/**
	 * @brief Optional authenticated selected-run command callback; no App ownership is
	 * transferred.
	 */
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
