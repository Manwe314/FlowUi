#include "devSystems/devAgenticWorkflow/AgenticScenario.hpp"

namespace FlowUi::devSystems::agentic {
Status AgenticScenarioContext::record_action(const AgenticScenarioAction& action) const noexcept {
	if (!record_action_sink || !action_evidence)
		return unexpectedError(
			makeError(ErrorCode::AgenticUnavailable, ErrorSite::AgenticScenario));
	return record_action_sink(action_evidence, action);
}
Json to_json(const AgenticScenarioDescriptor& descriptor) {
	return Json::make_object({{"scenario_id", descriptor.scenario_id},
							  {"revision", descriptor.revision},
							  {"digest", descriptor.digest},
							  {"fixture_id", descriptor.fixture_id},
							  {"description", descriptor.description},
							  {"mode", name(descriptor.mode)},
							  {"seed", decimal(descriptor.seed)},
							  {"deadline_ns", decimal(descriptor.deadline_ns)},
							  {"deterministic", descriptor.deterministic},
							  {"replayable", descriptor.replayable},
							  {"mutating", descriptor.mutating},
							  {"external_services", descriptor.external_services},
							  {"surfaces", descriptor.surfaces},
							  {"assertions", descriptor.assertions}});
}
} // namespace FlowUi::devSystems::agentic
