#pragma once

/** @file
 * @brief Public AgenticService declarations for optional development services.
 */
#include "FlowUi/Public/BuildConfig.hpp"
#if FLOW_UI_AGENTIC_WORKFLOW
#include "devSystems/devAgenticWorkflow/AgenticCommands.hpp"
#include "devSystems/devAgenticWorkflow/AgenticImage.hpp"
#include "devSystems/devAgenticWorkflow/AgenticInteraction.hpp"
#include "devSystems/devAgenticWorkflow/AgenticScenario.hpp"
#include "devSystems/devAgenticWorkflow/AgenticSnapshot.hpp"
#include <memory>

namespace FlowUi {
class App;
class ThemeManager;
struct ResourceConfig;
struct FrameInput;
} // namespace FlowUi
namespace FlowUi::devSystems {
class DevMonitoringAndReporting;
class DevTooling;
class DevInterface;
class DevTimingRecorder;
} // namespace FlowUi::devSystems
namespace FlowUi::devSystems::agentic {
class AgenticInteractionRuntime;
namespace frame_capture {
struct AgenticFrameCaptureRequest;
}
/** Non-owning application dependencies; the service shuts down before their destruction. */
struct AgenticDependencies {
	/** @brief Borrowed application dependency; it must outlive this operation or service. */
	App* application = nullptr;
	/** @brief Borrowed monitoring dependency; it must outlive the service. */
	DevMonitoringAndReporting* monitoring = nullptr;
	/** @brief Borrowed developer tooling dependency; it must outlive the service. */
	DevTooling* tooling = nullptr;
	/** @brief Borrowed theme manager; it must outlive the service. */
	ThemeManager* themes = nullptr;
	/**
	 * @brief Borrowed built-in developer interface dependency; it must outlive the service.
	 */
	DevInterface* interface = nullptr;
	/** @brief Borrowed resource lookup policy; it must outlive the service. */
	const ResourceConfig* resource_configuration = nullptr;
	/** @brief Borrowed platform-thread timing recorder; it must outlive the service. */
	DevTimingRecorder* platform_timing = nullptr;
};
/** The boundary following producer consumption and staged mutations, before window construction. */
struct AgenticSafePoint {
	/**
	 * @brief Producer-supplied application tick identity; independent of window frame and
	 * resource slot.
	 */
	std::uint64_t app_tick = 0;
	/** @brief Safe-point timestamp from the shared monotonic clock, in nanoseconds. */
	std::uint64_t monotonic_ns = 0;
	/** @brief Number of renderer image readbacks pending at this safe point. */
	std::uint64_t pending_images = 0;
	/**
	 * @brief Readback allocation, conversion or admission failures observed at this safe point.
	 */
	std::uint64_t image_failures = 0;
};
/** Shutdown waits only for CPU publication; renderer fences remain owned by the App. */
struct AgenticShutdownPolicy {
	/**
	 * @brief Whether shutdown should publish explicitly partial evidence for admitted jobs.
	 */
	bool publish_partial = true;
	/**
	 * @brief Whether shutdown follows device loss and must reject untrusted pending image work.
	 */
	bool device_lost = false;
};
/** App-owned evidence orchestration, bounded jobs, scenario execution and background publication.
 */
class AgenticWorkflowService {
public:
	/**
	 * @brief Bind borrowed application dependencies; configure explicitly before requesting live
	 * operations.
	 */
	explicit AgenticWorkflowService(const AgenticDependencies& dependencies);
	/**
	 * @brief Shut down owned service work before the borrowed dependencies are destroyed.
	 */
	~AgenticWorkflowService();
	AgenticWorkflowService(const AgenticWorkflowService&) = delete;
	AgenticWorkflowService& operator=(const AgenticWorkflowService&) = delete;
	/** Resolve one launch handoff; explicit configuration takes precedence over environment. */
	[[nodiscard]] Status configure(const AgenticWorkflowConfig& configuration) noexcept;
	/** Validate, copy and enqueue without calling application producers or writing artifacts. */
	[[nodiscard]] Result<AgenticJobId>
	request_capture(const AgenticCaptureRequest& request) noexcept;
	/**
	 * @brief Return an owned job status snapshot, or an explicit error for an unknown job
	 * identity.
	 */
	[[nodiscard]] Result<AgenticJobStatus> query_job(std::string_view job_id) const noexcept;
	/**
	 * @brief Request bounded cancellation and cleanup of an admitted job; returns an error for an
	 * unknown identity.
	 */
	[[nodiscard]] Status cancel_job(std::string_view job_id) noexcept;
	/**
	 * @brief Request the active measurement cutoff while preserving admitted evidence for
	 * finalization.
	 */
	[[nodiscard]] Status stop_capture(std::string_view job_id) noexcept;
	/**
	 * @brief Return an owned effective capability snapshot, including configured bounds and
	 * limitations.
	 */
	[[nodiscard]] Result<AgenticCapabilities> capabilities() const noexcept;
	/** Register application semantics before any scenario using them is admitted. */
	[[nodiscard]] Status register_scenario(AgenticScenario scenario) noexcept;
	/**
	 * @brief Return owned serializable application scenario declarations without their callbacks.
	 */
	[[nodiscard]] Result<Json> scenarios() const noexcept;
	/** Non-owning executor access for registering application targets/actions on the platform
	 * thread. Unavailable unless input or action control was explicitly enabled in AppConfig. */
	[[nodiscard]] Result<AgenticInteractionRuntime*> interaction_runtime() noexcept;
	/** Start/await a bounded interaction subplan; scenario callbacks may reuse their capture lease.
	 */
	[[nodiscard]] Result<AgenticJobId>
	start_interaction(const Json& plan, std::string_view idempotency_key = {}) noexcept;
	/** Inspect execution completion independently of the owning capture's publication phase. */
	[[nodiscard]] Result<AgenticInteractionResult>
	query_interaction(std::string_view job_id) const noexcept;
	/** Select effective input after native assessment and before coordinate conversion. */
	[[nodiscard]] Status select_frame_input(const EvidenceFrameKey& key, FrameInput& input,
											float ui_scale) noexcept;
	/** Observe completed UI facts before the next layout replaces their storage. */
	void observe_interaction(const EvidenceFrameKey& key) noexcept;
	/** Test whether ordinary development input filters must yield to the execution lease. */
	[[nodiscard]] bool interaction_controls_window(std::uint64_t window_id) const noexcept;
	/** Admit queued commands and move each active job through its bounded lifecycle. */
	void advance_safe_point(const AgenticSafePoint& safe_point) noexcept;
	/** Append an App-stamped cadence observation; no file or producer access occurs here. */
	void observe_frame(AgenticFrameRecord frame) noexcept;
	/** Copy exact frame ownership into a readback request for this job and window. */
	[[nodiscard]] frame_capture::AgenticFrameCaptureRequest
	image_request(const AgenticFrameRecord& frame) noexcept;
	/**
	 * @brief Transfer a fence-completed owned image into its admitted job, subject to capture
	 * bounds.
	 */
	[[nodiscard]] Status accept_completed_image(AgenticCompletedImage image) noexcept;
	/** Copy a selected published tree while its App frame still owns the publication. */
	void observe_tree(std::uint64_t window_id) noexcept;
	/** Used by manual capture admission to arbitrate the same application capture lease. */
	[[nodiscard]] bool capture_lease_active() const noexcept;
	/**
	 * @brief Return whether active admitted jobs request optional live evidence collection.
	 */
	[[nodiscard]] bool optional_collection_enabled() const noexcept;
	/** Apply the active policy to a newly constructed window and remember its prior state. */
	void prepare_window_collection(std::uint64_t window_id) noexcept;
	/** @brief Return whether effective service policy enables image capture resources. */
	[[nodiscard]] bool visual_capture_enabled() const noexcept;
	/** Estimated owned queue/snapshot bytes, excluding thread stacks and encoder temporaries. */
	[[nodiscard]] std::uint64_t owned_memory_bytes() const noexcept;
	/** Transport entry point: executes through the platform queue with a bounded wait. */
	[[nodiscard]] Result<Json> submit_command(const AgenticCommandRequest& request) noexcept;
	/** Stop new local commands before draining renderer resources. */
	void stop_admission() noexcept;
	/**
	 * @brief Stop admission, finish bounded CPU publication and release service work using the
	 * supplied partial-publication policy.
	 */
	[[nodiscard]] Status shutdown(const AgenticShutdownPolicy& policy = {}) noexcept;
	/** Keep scenario/App references valid when the movable public App wrapper moves. */
	void rebind_application(App& application) noexcept;

private:
	friend class AgenticInteractionRuntime;
	/* Copy the active scenario owner's policy before any child observations are retained. */
	[[nodiscard]] Result<AgenticContentPolicy>
	interaction_content_policy(std::string_view owner_job_id) const noexcept;
	/* Bound child-plan reservations to the owning capture's accepted snapshot budget. */
	[[nodiscard]] Result<std::uint64_t>
	interaction_snapshot_budget(std::string_view owner_job_id) const noexcept;
	/* Remember child input ownership so parent cancellation precedes fixture cleanup. */
	[[nodiscard]] Status track_owned_interaction(std::string_view owner_job_id,
												 std::string_view interaction_job_id) noexcept;
	struct Impl;
	std::unique_ptr<Impl> impl_;
};
} // namespace FlowUi::devSystems::agentic
#endif
