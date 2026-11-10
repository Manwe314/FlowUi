#include "devSystems/devAgenticWorkflow/AgenticAction.hpp"
#if FLOW_UI_AGENTIC_WORKFLOW
#include "FlowUi/Public/App.hpp"
#include "FlowUi/Public/managers/ActionManager.hpp"
#include <algorithm>
#include <limits>

namespace FlowUi::devSystems::agentic {
namespace {
[[nodiscard]] Status rejected(AgenticError error) noexcept {
	return unexpectedError(agentic_error(error));
}
[[nodiscard]] bool bounded_json(const Json& value, std::size_t maximum_bytes = 1024u * 1024u) {
	return write_json(value).size() <= maximum_bytes;
}
[[nodiscard]] Json declaration_json(const AgenticActionDeclaration& declaration, bool enabled,
									bool bound) {
	return Json::make_object(
		{{"action_id", declaration.action_id},
		 {"revision", decimal(declaration.revision)},
		 {"description", declaration.description},
		 {"argument_schema", declaration.argument_schema},
		 {"result_schema", declaration.result_schema},
		 {"mutating", declaration.mutating},
		 {"completion_contract",
		  declaration.asynchronous ? "application_acknowledgement" : "dispatch_completed"},
		 {"enabled", enabled},
		 {"bound_action", bound},
		 {"explicitly_exported", true}});
}
} // namespace
AgenticActionRegistry::AgenticActionRegistry(std::size_t maximum_actions) noexcept
	: maximum_actions_(std::min<std::size_t>(maximum_actions, 4096)) {}
const AgenticActionRegistry::Entry*
AgenticActionRegistry::find(std::string_view action_id) const noexcept {
	for (const auto& entry : entries_)
		if (entry.declaration.action_id == action_id)
			return &entry;
	return nullptr;
}
Status AgenticActionRegistry::insert(Entry entry) noexcept {
	try {
		const auto& declaration = entry.declaration;
		if (!valid_identifier(declaration.action_id) || !declaration.revision ||
			declaration.description.size() > 4096 ||
			declaration.argument_schema.kind != Json::Kind::object ||
			declaration.result_schema.kind != Json::Kind::object ||
			!bounded_json(declaration.argument_schema, 65536) ||
			!bounded_json(declaration.result_schema, 65536))
			return rejected(AgenticError::invalid_request);
		for (auto& existing : entries_)
			if (existing.declaration.action_id == declaration.action_id) {
				if (declaration.revision <= existing.declaration.revision)
					return rejected(AgenticError::stale_generation);
				existing = std::move(entry);
				return {};
			}
		if (entries_.size() >= maximum_actions_)
			return rejected(AgenticError::budget_exhausted);
		entries_.emplace_back(std::move(entry));
		return {};
	} catch (...) {
		return rejected(AgenticError::budget_exhausted);
	}
}
Status AgenticActionRegistry::register_bound_action(AgenticActionDeclaration declaration,
													AppActionCall call) noexcept {
	if (!call || declaration.asynchronous)
		return rejected(AgenticError::invalid_request);
	try {
		declaration.argument_schema =
			Json::make_object({{"type", "object"}, {"additionalProperties", false}});
		Entry entry;
		entry.declaration = std::move(declaration);
		entry.bound = call;
		return insert(std::move(entry));
	} catch (...) {
		return rejected(AgenticError::budget_exhausted);
	}
}
Status AgenticActionRegistry::register_action(AgenticActionDeclaration declaration,
											  AgenticActionAdapter adapter) noexcept {
	if (!adapter.invoke || !adapter.decode_arguments ||
		(declaration.asynchronous && !adapter.poll_completion))
		return rejected(AgenticError::invalid_request);
	try {
		Entry entry;
		entry.declaration = std::move(declaration);
		entry.adapter = std::move(adapter);
		return insert(std::move(entry));
	} catch (...) {
		return rejected(AgenticError::budget_exhausted);
	}
}
Status AgenticActionRegistry::unregister_action(std::string_view action_id,
												std::uint64_t revision) noexcept {
	for (auto entry = entries_.begin(); entry != entries_.end(); ++entry)
		if (entry->declaration.action_id == action_id) {
			if (entry->declaration.revision != revision)
				return rejected(AgenticError::stale_generation);
			entries_.erase(entry);
			return {};
		}
	return rejected(AgenticError::unavailable);
}
Result<Json> AgenticActionRegistry::list(App& application, std::size_t limit,
										 std::size_t cursor) const noexcept {
	try {
		if (!limit || limit > 1000 || cursor > entries_.size())
			return unexpectedError(agentic_error(AgenticError::invalid_request));
		Json rows = Json::make_array();
		rows.array.reserve(std::min(limit, entries_.size() - cursor));
		for (std::size_t index = cursor; index < entries_.size() && rows.array.size() < limit;
			 ++index) {
			const auto entry = entries_[index];
			bool enabled = false;
			if (entry.bound)
				enabled = application.actions().availability(ActionCall(entry.bound)).enabled;
			else if (entry.adapter.available) {
				auto availability = entry.adapter.available(application);
				enabled = availability && *availability;
			} else
				enabled = true;
			rows.array.emplace_back(
				declaration_json(entry.declaration, enabled, static_cast<bool>(entry.bound)));
		}
		const auto end = cursor + rows.array.size();
		return Json::make_object({{"actions", std::move(rows)},
								  {"next_cursor", end < entries_.size() ? decimal(end) : Json()},
								  {"total", decimal(entries_.size())},
								  {"catalogue_actions_are_not_exports", true}});
	} catch (...) {
		return unexpectedError(agentic_error(AgenticError::invalid_request));
	}
}
Result<AgenticActionReceipt>
AgenticActionRegistry::invoke(App& application,
							  const AgenticActionInvocation& invocation) noexcept {
	try {
		const auto* selected = find(invocation.action_id);
		if (!selected)
			return unexpectedError(agentic_error(AgenticError::unavailable));
		const Entry owned_entry = *selected;
		const auto* entry = &owned_entry;
		if (!invocation.revision || entry->declaration.revision != invocation.revision)
			return unexpectedError(agentic_error(AgenticError::stale_generation));
		if (invocation.arguments.kind != Json::Kind::object ||
			!bounded_json(invocation.arguments) || invocation.plan_id.size() > 256 ||
			invocation.step_id.size() > 256 || invocation.request_id.size() > 256)
			return unexpectedError(agentic_error(AgenticError::invalid_request));
		const auto now = monotonic_nanoseconds();
		if (invocation.deadline_monotonic_ns && now >= invocation.deadline_monotonic_ns)
			return unexpectedError(agentic_error(AgenticError::timeout));
		if (entry->adapter.available) {
			auto available = entry->adapter.available(application);
			if (!available)
				return unexpectedError(available.error());
			if (!*available)
				return unexpectedError(agentic_error(AgenticError::unavailable));
		}
		AgenticActionReceipt receipt;
		receipt.invocation = invocation;
		if (entry->bound) {
			if (!invocation.arguments.object.empty())
				return unexpectedError(agentic_error(AgenticError::invalid_request));
			if (!application.actions().appActions().isBound(entry->bound) ||
				!application.actions().availability(ActionCall(entry->bound)).enabled)
				return unexpectedError(agentic_error(AgenticError::unavailable));
			receipt.dispatched_monotonic_ns = monotonic_nanoseconds();
			try {
				const auto status = application.actions().invoke(
					ActionCall(entry->bound),
					{.kind = ActionInvocationSourceKind::Agentic,
					 .window = static_cast<WindowId>(invocation.window_id),
					 .sourceId = invocation.invocation_sequence});
				receipt.dispatched = status == ActionInvocationStatus::Invoked;
				receipt.completed = true;
				receipt.completion_succeeded = receipt.dispatched;
				if (!receipt.dispatched)
					receipt.reason = "binding_unavailable_at_dispatch";
			} catch (...) {
				receipt.dispatched = true;
				receipt.completed = true;
				receipt.reason = "application_callback_failed";
			}
			receipt.completed_monotonic_ns = monotonic_nanoseconds();
			return receipt;
		}
		auto decoded = entry->adapter.decode_arguments(invocation.arguments);
		if (!decoded)
			return unexpectedError(decoded.error());
		if (!bounded_json(*decoded))
			return unexpectedError(agentic_error(AgenticError::budget_exhausted));
		const auto* current = find(invocation.action_id);
		if (!current || current->declaration.revision != invocation.revision)
			return unexpectedError(agentic_error(AgenticError::stale_generation));
		// Copy callbacks before user code: an adapter may update its own exports safely.
		const auto callback = entry->adapter.invoke;
		const bool asynchronous = entry->declaration.asynchronous;
		receipt.dispatched_monotonic_ns = monotonic_nanoseconds();
		try {
			auto invoked = callback(application, invocation, *decoded);
			if (!invoked) {
				receipt.dispatched = true;
				receipt.completed = true;
				receipt.reason = "application_adapter_failed";
				receipt.completed_monotonic_ns = monotonic_nanoseconds();
				return receipt;
			}
			invoked->invocation = invocation;
			invoked->dispatched = true;
			invoked->dispatched_monotonic_ns = receipt.dispatched_monotonic_ns;
			if (!bounded_json(invoked->result) || !bounded_json(invoked->completion_token, 65536)) {
				receipt.dispatched = true;
				receipt.completed = true;
				receipt.reason = "application_result_budget_exhausted";
				receipt.completed_monotonic_ns = monotonic_nanoseconds();
				return receipt;
			}
			if (!asynchronous && !invoked->completed) {
				invoked->completed = true;
				invoked->completion_succeeded = false;
				invoked->reason = "synchronous_completion_missing";
			}
			if (invoked->completed)
				invoked->completed_monotonic_ns = monotonic_nanoseconds();
			return invoked;
		} catch (...) {
			receipt.dispatched = true;
			receipt.completed = true;
			receipt.reason = "application_callback_failed";
			receipt.completed_monotonic_ns = monotonic_nanoseconds();
			return receipt;
		}
	} catch (...) {
		return unexpectedError(agentic_error(AgenticError::budget_exhausted));
	}
}
Result<AgenticActionReceipt>
AgenticActionRegistry::poll(App& application, const AgenticActionReceipt& receipt) noexcept {
	try {
		if (receipt.completed)
			return receipt;
		const auto* entry = find(receipt.invocation.action_id);
		if (!entry || entry->declaration.revision != receipt.invocation.revision)
			return unexpectedError(agentic_error(AgenticError::stale_generation));
		if (!receipt.dispatched || !entry->adapter.poll_completion)
			return unexpectedError(agentic_error(AgenticError::invalid_request));
		if (receipt.invocation.deadline_monotonic_ns &&
			monotonic_nanoseconds() >= receipt.invocation.deadline_monotonic_ns) {
			auto expired = receipt;
			expired.completed = true;
			expired.completion_succeeded = false;
			expired.reason = "application_completion_timeout";
			expired.completed_monotonic_ns = monotonic_nanoseconds();
			return expired;
		}
		const auto callback = entry->adapter.poll_completion;
		auto observed = callback(application, receipt);
		if (!observed)
			return unexpectedError(observed.error());
		if (!bounded_json(observed->result) || !bounded_json(observed->completion_token, 65536))
			return unexpectedError(agentic_error(AgenticError::budget_exhausted));
		observed->invocation = receipt.invocation;
		observed->dispatched = receipt.dispatched;
		observed->dispatched_monotonic_ns = receipt.dispatched_monotonic_ns;
		observed->completed_monotonic_ns = observed->completed ? monotonic_nanoseconds() : 0;
		return observed;
	} catch (...) {
		return unexpectedError(agentic_error(AgenticError::invalid_request));
	}
}
Status AgenticActionRegistry::cancel(App& application,
									 const AgenticActionReceipt& receipt) noexcept {
	try {
		const auto* entry = find(receipt.invocation.action_id);
		if (!entry || entry->declaration.revision != receipt.invocation.revision)
			return rejected(AgenticError::stale_generation);
		if (receipt.completed || !receipt.dispatched)
			return {};
		if (!entry->adapter.cancel)
			return rejected(AgenticError::unavailable);
		const auto callback = entry->adapter.cancel;
		return callback(application, receipt);
	} catch (...) {
		return rejected(AgenticError::invalid_request);
	}
}
Json AgenticActionRegistry::describe(const AgenticActionReceipt& receipt) {
	return Json::make_object(
		{{"action_id", receipt.invocation.action_id},
		 {"revision", decimal(receipt.invocation.revision)},
		 {"source", "agentic_exported_action"},
		 {"plan_id", receipt.invocation.plan_id},
		 {"step_id", receipt.invocation.step_id},
		 {"request_id", receipt.invocation.request_id},
		 {"invocation_sequence", decimal(receipt.invocation.invocation_sequence)},
		 {"window_id", decimal(receipt.invocation.window_id)},
		 {"dispatched", receipt.dispatched},
		 {"completed", receipt.completed},
		 {"completion_succeeded", receipt.completion_succeeded},
		 {"reason", receipt.reason},
		 {"result", receipt.result},
		 {"dispatched_monotonic_ns", decimal(receipt.dispatched_monotonic_ns)},
		 {"completed_monotonic_ns", decimal(receipt.completed_monotonic_ns)},
		 {"deadline_monotonic_ns", decimal(receipt.invocation.deadline_monotonic_ns)}});
}
} // namespace FlowUi::devSystems::agentic
#endif
