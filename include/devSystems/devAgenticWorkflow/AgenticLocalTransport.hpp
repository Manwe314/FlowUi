#pragma once

/** @file
 * @brief Public AgenticLocalTransport declarations for optional development services.
 */
#include "FlowUi/Public/BuildConfig.hpp"
#if FLOW_UI_AGENTIC_WORKFLOW

#include "devSystems/devAgenticWorkflow/AgenticJson.hpp"

#include <cstddef>
#include <cstdint>
#include <functional>
#include <memory>
#include <string>

namespace FlowUi::devSystems::agentic {

/** Owner-private local endpoint settings. The nonce authenticates a single App run. */
struct AgenticLocalEndpoint {
	/** @brief Local socket or named-pipe address for exactly one application run. */
	std::string endpoint{};
	/**
	 * @brief Immutable application run identity, separate from job and capture identities.
	 */
	std::string run_id{};
	/** @brief Owner-private authentication nonce bound to exactly one application run. */
	std::string nonce{};
	/** @brief Bounded command transport wait, in milliseconds. */
	std::uint64_t timeout_ms = 2000;
	/** @brief Maximum local request or response message size, in bytes. */
	std::size_t maximum_message_bytes = 1024u * 1024u;
};

/** Handlers may wait concurrently on eight transport workers; mutations run at a safe point. */
using AgenticLocalHandler = std::function<Json(const Json&)>;

/** Bounded owner-private local command server, independent of MCP and application stdout. */
class AgenticLocalServer {
public:
	/** @brief Construct a dormant server; call start() to create an endpoint. */
	AgenticLocalServer();
	/** @brief Stop admission and join worker threads before releasing callbacks. */
	~AgenticLocalServer();
	AgenticLocalServer(const AgenticLocalServer&) = delete;
	AgenticLocalServer& operator=(const AgenticLocalServer&) = delete;

	/** Start one local socket or eight named-pipe instances. Existing endpoints are never replaced.
	 */
	[[nodiscard]] Status start(const AgenticLocalEndpoint& endpoint,
							   AgenticLocalHandler handler) noexcept;
	/** Stop admission, interrupt pending input and join every worker before releasing its handler.
	 */
	void stop() noexcept;
	/** Return whether this server is accepting connections. */
	[[nodiscard]] bool running() const noexcept;

private:
	struct Impl;
	std::unique_ptr<Impl> impl_;
};

/** Authenticate the selected run and execute a bounded command over local IPC. */
[[nodiscard]] Result<Json> send_local_command(const AgenticLocalEndpoint& endpoint,
											  const Json& request) noexcept;

} // namespace FlowUi::devSystems::agentic

#endif // FLOW_UI_AGENTIC_WORKFLOW
