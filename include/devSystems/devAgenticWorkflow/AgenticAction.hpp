#pragma once
#include "FlowUi/BuildConfig.hpp"
#if FLOW_UI_AGENTIC_WORKFLOW
#include "devSystems/devAgenticWorkflow/AgenticTypes.hpp"
#include "managers/structs/ActionManagerStructs.hpp"
#include <functional>
#include <vector>
namespace FlowUi {
class App;
class ActionManager;
} // namespace FlowUi
namespace FlowUi::devSystems::agentic {
/** Explicit export metadata; merely appearing in the development catalogue grants no permission. */
struct AgenticActionDeclaration {
	std::string action_id, description;
	Json argument_schema = Json::make_object(), result_schema = Json::make_object();
	std::uint64_t revision = 1;
	bool mutating = true, asynchronous = false;
};
/** Owned request identity forwarded unchanged to application adapters and invocation receipts. */
struct AgenticActionInvocation {
	std::string action_id, plan_id, step_id, request_id;
	Json arguments = Json::make_object();
	std::uint64_t revision = 0, deadline_monotonic_ns = 0, invocation_sequence = 0, window_id = 0;
};
/** Dispatch and business completion are deliberately independent facts. */
struct AgenticActionReceipt {
	AgenticActionInvocation invocation;
	Json result = Json::make_object(), completion_token;
	std::string reason;
	std::uint64_t dispatched_monotonic_ns = 0, completed_monotonic_ns = 0;
	bool dispatched = false, completed = false, completion_succeeded = false;
};
/** Application-owned typed boundary: decode before invoking; return owned result and pending token.
 * Callbacks execute only on the ordinary platform-thread safe point. */
struct AgenticActionAdapter {
	std::function<Result<Json>(const Json&)> decode_arguments;
	std::function<Result<AgenticActionReceipt>(App&, const AgenticActionInvocation&, const Json&)>
		invoke;
	std::function<Result<AgenticActionReceipt>(App&, const AgenticActionReceipt&)> poll_completion;
	std::function<Result<bool>(const App&)> available;
	std::function<Status(App&, const AgenticActionReceipt&)> cancel;
};
/** Explicit exports and typed adapters. The runtime owns sequencing, deadlines and exclusivity. */
class AgenticActionRegistry {
public:
	explicit AgenticActionRegistry(std::size_t maximum_actions = 256) noexcept;
	/** Export a bound parameterless application action through ActionManager dispatch. */
	[[nodiscard]] Status register_bound_action(AgenticActionDeclaration declaration,
											   AppActionCall call) noexcept;
	/** Export an explicit application decoder and result/completion adapter. */
	[[nodiscard]] Status register_action(AgenticActionDeclaration declaration,
										 AgenticActionAdapter adapter) noexcept;
	/** Remove an export only when its declared revision still matches. */
	[[nodiscard]] Status unregister_action(std::string_view action_id,
										   std::uint64_t revision) noexcept;
	/** Discover exports with current binding/availability and declared completion contract. */
	[[nodiscard]] Result<Json> list(App& application, std::size_t limit = 100,
									std::size_t cursor = 0) const noexcept;
	/** Validate expected revision and decode all arguments before any dispatch side effect. */
	[[nodiscard]] Result<AgenticActionReceipt>
	invoke(App& application, const AgenticActionInvocation& invocation) noexcept;
	/** Poll application acknowledgement without redispatching a pending action. */
	[[nodiscard]] Result<AgenticActionReceipt> poll(App& application,
													const AgenticActionReceipt& receipt) noexcept;
	/** Invoke a declared cancellation adapter; dispatched work cannot be rolled back implicitly. */
	[[nodiscard]] Status cancel(App& application, const AgenticActionReceipt& receipt) noexcept;
	/** Owned result representation suitable for copied action evidence. */
	[[nodiscard]] static Json describe(const AgenticActionReceipt& receipt);

private:
	struct Entry {
		AgenticActionDeclaration declaration;
		AgenticActionAdapter adapter;
		AppActionCall bound{};
	};
	[[nodiscard]] const Entry* find(std::string_view action_id) const noexcept;
	[[nodiscard]] Status insert(Entry entry) noexcept;
	std::vector<Entry> entries_;
	std::size_t maximum_actions_;
};
} // namespace FlowUi::devSystems::agentic
#endif
