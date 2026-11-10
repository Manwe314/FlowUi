#pragma once

/** @file
 * @brief Public AgenticAction declarations for optional development services.
 */
#include "FlowUi/Public/BuildConfig.hpp"
#if FLOW_UI_AGENTIC_WORKFLOW
#include "devSystems/devAgenticWorkflow/AgenticTypes.hpp"
#include "FlowUi/Public/managerStructs/ActionTypes.hpp"
#include <functional>
#include <vector>
namespace FlowUi {
class App;
class ActionManager;
} // namespace FlowUi
namespace FlowUi::devSystems::agentic {
/** Explicit export metadata; merely appearing in the development catalogue grants no permission. */
struct AgenticActionDeclaration {
	/** @var action_id
	 * @brief Application-declared exported action identity.
	 */
	/** @var description
	 * @brief Human-readable declaration metadata; owned when stored as std::string.
	 */
	std::string action_id, description;
	/** @var argument_schema
	 * @brief Owned JSON schema describing accepted exported action arguments.
	 */
	/** @var result_schema
	 * @brief Owned JSON schema describing exported action results.
	 */
	Json argument_schema = Json::make_object(), result_schema = Json::make_object();
	/** @brief Expected or declared monotonically versioned application export identity. */
	std::uint64_t revision = 1;
	/** @var mutating
	 * @brief Whether this declaration may change application state.
	 */
	/** @var asynchronous
	 * @brief Whether action dispatch requires a later completion acknowledgement.
	 */
	bool mutating = true, asynchronous = false;
};
/** Owned request identity forwarded unchanged to application adapters and invocation receipts. */
struct AgenticActionInvocation {
	/** @var action_id
	 * @brief Application-declared exported action identity.
	 */
	/** @var plan_id
	 * @brief Caller-supplied owning interaction plan identity.
	 */
	/** @var step_id
	 * @brief Application-declared semantic step identity.
	 */
	/** @var request_id
	 * @brief Caller retry and response correlation identity.
	 */
	std::string action_id, plan_id, step_id, request_id;
	/**
	 * @brief Owned request or semantic action arguments; callback objects are never serialized.
	 */
	Json arguments = Json::make_object();
	/** @var revision
	 * @brief Expected or declared monotonically versioned application export identity.
	 */
	/** @var deadline_monotonic_ns
	 * @brief Absolute monotonic completion deadline, in nanoseconds.
	 */
	/** @var invocation_sequence
	 * @brief Monotonic action invocation sequence assigned by the executor.
	 */
	/** @var window_id
	 * @brief Stable application window identity associated with this request or evidence.
	 */
	std::uint64_t revision = 0, deadline_monotonic_ns = 0, invocation_sequence = 0, window_id = 0;
};
/** Dispatch and business completion are deliberately independent facts. */
struct AgenticActionReceipt {
	/** @brief Owned action request and sequencing metadata associated with this receipt. */
	AgenticActionInvocation invocation;
	/** @var result
	 * @brief Owned decoded action result representation.
	 */
	/** @var completion_token
	 * @brief Owned adapter token used to poll asynchronous completion without redispatching.
	 */
	Json result = Json::make_object(), completion_token;
	/**
	 * @brief Explicit machine-readable or diagnostic explanation of an outcome or limitation.
	 */
	std::string reason;
	/** @var dispatched_monotonic_ns
	 * @brief Monotonic application dispatch timestamp, in nanoseconds.
	 */
	/** @var completed_monotonic_ns
	 * @brief Monotonic completion timestamp, in nanoseconds; zero means unavailable.
	 */
	std::uint64_t dispatched_monotonic_ns = 0, completed_monotonic_ns = 0;
	/** @var dispatched
	 * @brief Whether dispatch reached the application action boundary.
	 */
	/** @var completed
	 * @brief Whether application business completion has been acknowledged.
	 */
	/** @var completion_succeeded
	 * @brief Whether acknowledged application completion succeeded; dispatch alone is insufficient.
	 */
	bool dispatched = false, completed = false, completion_succeeded = false;
};
/** Application-owned typed boundary: decode before invoking; return owned result and pending token.
 * Callbacks execute only on the ordinary platform-thread safe point. */
struct AgenticActionAdapter {
	/** @brief Required typed decoder invoked before any application dispatch side effect. */
	std::function<Result<Json>(const Json&)> decode_arguments;
	/** @brief Required application dispatch callback receiving validated typed arguments. */
	std::function<Result<AgenticActionReceipt>(App&, const AgenticActionInvocation&, const Json&)>
		invoke;
	/**
	 * @brief Optional completion callback for a previously dispatched asynchronous action.
	 */
	std::function<Result<AgenticActionReceipt>(App&, const AgenticActionReceipt&)> poll_completion;
	/**
	 * @brief Optional platform-thread predicate returning current action availability or an
	 * explicit error.
	 */
	std::function<Result<bool>(const App&)> available;
	/**
	 * @brief Optional callback requesting application cancellation; prior dispatch is not rolled
	 * back.
	 */
	std::function<Status(App&, const AgenticActionReceipt&)> cancel;
};
/** Explicit exports and typed adapters. The runtime owns sequencing, deadlines and exclusivity. */
class AgenticActionRegistry {
public:
	/** @brief Construct an empty exported-action registry bounded by maximum_actions. */
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
