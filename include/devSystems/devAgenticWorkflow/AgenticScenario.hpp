#pragma once

/** @file
 * @brief Public AgenticScenario declarations for optional development services.
 */
#include "FlowUi/Public/BuildConfig.hpp"
#if FLOW_UI_AGENTIC_WORKFLOW
#include "devSystems/devAgenticWorkflow/AgenticCapture.hpp"
#include <functional>

namespace FlowUi {
class App;
}
namespace FlowUi::devSystems::agentic {
/** Callback result; pending steps resume through normal application polling. */
enum class AgenticScenarioProgress : std::uint8_t {
	pending, /**< Resume this nonblocking stage at a later safe point. */
	complete /**< This scenario stage completed. */
};
/** One application-declared semantic action or assertion, copied into workload evidence. */
struct AgenticScenarioAction {
	/**
	 * @brief Owned request or semantic action arguments; callback objects are never serialized.
	 */
	Json arguments = Json::make_object();
	/**
	 * @brief Exact producer frame key associated with the semantic action, when available.
	 */
	std::optional<EvidenceFrameKey> observed_frame;
	/** @brief Application-declared semantic action name recorded as workload evidence. */
	std::string name;
	/** @brief Application-declared semantic target name recorded with this action. */
	std::string target;
	/** @brief Optional application state revision observed by the semantic action. */
	std::optional<std::uint64_t> state_revision;
	/** @brief Whether this semantic action records an assertion result. */
	bool assertion = false;
	/** @brief Whether this recorded semantic assertion passed. */
	bool passed = true;
};
/** Semantic workload declaration owned by the application, including source/fixture identity. */
struct AgenticScenarioDescriptor {
	/** @brief Application-declared semantic workload identity. */
	std::string scenario_id;
	/**
	 * @brief Application-declared workload revision string used to reject incompatible scenario
	 * requests.
	 */
	std::string revision;
	/**
	 * @brief Content digest used to bind immutable evidence or compiled workload identity.
	 */
	std::string digest;
	/** @brief Application-declared fixture identity used to reproduce the workload. */
	std::string fixture_id;
	/** @brief Owned human-readable scenario description. */
	std::string description;
	/** @brief Owned declared workload surface identities. */
	Json surfaces = Json::make_array();
	/** @brief Owned declared scenario assertions used to identify the workload. */
	Json assertions = Json::make_array();
	/** @brief Deterministic application-supplied workload seed. */
	std::uint64_t seed = 0;
	/** @brief Relative operation or scenario deadline duration, in nanoseconds. */
	std::uint64_t deadline_ns = 30'000'000'000ull;
	/** @brief Execution backend required by the application-declared scenario. */
	ExecutionMode mode = ExecutionMode::windowed;
	/**
	 * @brief Whether the application declares this scenario deterministic for its seed and
	 * fixture.
	 */
	bool deterministic = true;
	/**
	 * @brief Whether the application declares the scenario replayable with its fixture and seed.
	 */
	bool replayable = true;
	/** @brief Whether this declaration may change application state. */
	bool mutating = false;
	/** @brief Whether the scenario declares dependencies on external services. */
	bool external_services = false;
};
/** Safe-point callback context; callbacks must never wait for a future UI frame. */
struct AgenticScenarioContext {
	/** @brief Borrowed application dependency; it must outlive this operation or service. */
	App* application = nullptr;
	/** @brief Borrowed live job identity valid only during this scenario callback. */
	std::string_view job_id;
	/**
	 * @brief Producer-supplied application tick identity; independent of window frame and
	 * resource slot.
	 */
	std::uint64_t app_tick = 0;
	/** @brief Elapsed scenario time on the monotonic clock, in nanoseconds. */
	std::uint64_t elapsed_ns = 0;
	/** @brief Deterministic application-supplied workload seed. */
	std::uint64_t seed = 0;
	/**
	 * @brief Whether cancellation has been requested, without implying cleanup is complete.
	 */
	bool cancellation_requested = false;
	/**
	 * @brief Borrowed callback-local evidence sink state; never retain beyond the scenario
	 * callback.
	 */
	void* action_evidence = nullptr;
	/**
	 * @brief Callback-local evidence writer; call only during the owning scenario callback on its
	 * thread.
	 */
	Status (*record_action_sink)(void*, const AgenticScenarioAction&) noexcept = nullptr;
	/** Record owned semantic arguments and optional observed identities during this callback.
	 * The context/sink must not be retained or called from another thread. */
	[[nodiscard]] Status record_action(const AgenticScenarioAction& action) const noexcept;
};
/**
 * @brief Nonblocking platform-thread scenario stage returning pending, completion or an explicit
 * error.
 */
using AgenticScenarioCallback =
	std::function<Result<AgenticScenarioProgress>(const AgenticScenarioContext&)>;
/** Setup/readiness, measured step, assertions and reset all have explicit completion/deadlines. */
struct AgenticScenario {
	/**
	 * @brief Owned workload declaration; callbacks remain separate from serializable metadata.
	 */
	AgenticScenarioDescriptor descriptor;
	/**
	 * @brief Nonblocking scenario readiness callback; pending resumes at later safe points.
	 */
	AgenticScenarioCallback setup;
	/**
	 * @brief Nonblocking measured scenario callback; pending resumes at later safe points.
	 */
	AgenticScenarioCallback step;
	/** @brief Nonblocking scenario assertion callback invoked after measured work. */
	AgenticScenarioCallback verify;
	/**
	 * @brief Nonblocking application cleanup/reset callback; pending resumes at later safe
	 * points.
	 */
	AgenticScenarioCallback cleanup;
};
/** Serialize only the declaration; callback objects never cross a transport boundary. */
[[nodiscard]] Json to_json(const AgenticScenarioDescriptor& descriptor);
} // namespace FlowUi::devSystems::agentic

#endif // FLOW_UI_AGENTIC_WORKFLOW
