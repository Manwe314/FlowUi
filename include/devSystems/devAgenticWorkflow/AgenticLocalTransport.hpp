#pragma once
#include "FlowUi/BuildConfig.hpp"
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
	std::string endpoint{};
	std::string run_id{};
	std::string nonce{};
	std::uint64_t timeout_ms = 2000;
	std::size_t maximum_message_bytes = 1024u * 1024u;
};

/** Handlers may wait concurrently on eight transport workers; mutations run at a safe point. */
using AgenticLocalHandler = std::function<Json(const Json&)>;

/** Bounded owner-private local command server, independent of MCP and application stdout. */
class AgenticLocalServer {
public:
	AgenticLocalServer();
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
