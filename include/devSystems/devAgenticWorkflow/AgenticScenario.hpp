#pragma once
#include "FlowUi/BuildConfig.hpp"
#if FLOW_UI_AGENTIC_WORKFLOW
#include "devSystems/devAgenticWorkflow/AgenticCapture.hpp"
#include <functional>

namespace FlowUi {
class App;
}
namespace FlowUi::devSystems::agentic {
/** Callback result; pending steps resume through normal application polling. */
enum class AgenticScenarioProgress : std::uint8_t { pending, complete };
/** One application-declared semantic action or assertion, copied into workload evidence. */
struct AgenticScenarioAction {
	Json arguments = Json::make_object();
	std::optional<EvidenceFrameKey> observed_frame;
	std::string name;
	std::string target;
	std::optional<std::uint64_t> state_revision;
	bool assertion = false;
	bool passed = true;
};
/** Semantic workload declaration owned by the application, including source/fixture identity. */
struct AgenticScenarioDescriptor {
	std::string scenario_id;
	std::string revision;
	std::string digest;
	std::string fixture_id;
	std::string description;
	Json surfaces = Json::make_array();
	Json assertions = Json::make_array();
	std::uint64_t seed = 0;
	std::uint64_t deadline_ns = 30'000'000'000ull;
	ExecutionMode mode = ExecutionMode::windowed;
	bool deterministic = true;
	bool replayable = true;
	bool mutating = false;
	bool external_services = false;
};
/** Safe-point callback context; callbacks must never wait for a future UI frame. */
struct AgenticScenarioContext {
	App* application = nullptr;
	std::string_view job_id;
	std::uint64_t app_tick = 0;
	std::uint64_t elapsed_ns = 0;
	std::uint64_t seed = 0;
	bool cancellation_requested = false;
	void* action_evidence = nullptr;
	Status (*record_action_sink)(void*, const AgenticScenarioAction&) noexcept = nullptr;
	/** Record owned semantic arguments and optional observed identities during this callback.
	 * The context/sink must not be retained or called from another thread. */
	[[nodiscard]] Status record_action(const AgenticScenarioAction& action) const noexcept;
};
using AgenticScenarioCallback =
	std::function<Result<AgenticScenarioProgress>(const AgenticScenarioContext&)>;
/** Setup/readiness, measured step, assertions and reset all have explicit completion/deadlines. */
struct AgenticScenario {
	AgenticScenarioDescriptor descriptor;
	AgenticScenarioCallback setup;
	AgenticScenarioCallback step;
	AgenticScenarioCallback verify;
	AgenticScenarioCallback cleanup;
};
/** Serialize only the declaration; callback objects never cross a transport boundary. */
[[nodiscard]] Json to_json(const AgenticScenarioDescriptor& descriptor);
} // namespace FlowUi::devSystems::agentic

#endif // FLOW_UI_AGENTIC_WORKFLOW
