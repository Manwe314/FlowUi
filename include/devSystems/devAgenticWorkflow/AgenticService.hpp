#pragma once
#include "FlowUi/BuildConfig.hpp"
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
	App* application = nullptr;
	DevMonitoringAndReporting* monitoring = nullptr;
	DevTooling* tooling = nullptr;
	ThemeManager* themes = nullptr;
	DevInterface* interface = nullptr;
	const ResourceConfig* resource_configuration = nullptr;
	DevTimingRecorder* platform_timing = nullptr;
};
/** The boundary following producer consumption and staged mutations, before window construction. */
struct AgenticSafePoint {
	std::uint64_t app_tick = 0;
	std::uint64_t monotonic_ns = 0;
	std::uint64_t pending_images = 0;
	std::uint64_t image_failures = 0;
};
/** Shutdown waits only for CPU publication; renderer fences remain owned by the App. */
struct AgenticShutdownPolicy {
	bool publish_partial = true;
	bool device_lost = false;
};
/** App-owned evidence orchestration, bounded jobs, scenario execution and background publication.
 */
class AgenticWorkflowService {
public:
	explicit AgenticWorkflowService(const AgenticDependencies& dependencies);
	~AgenticWorkflowService();
	AgenticWorkflowService(const AgenticWorkflowService&) = delete;
	AgenticWorkflowService& operator=(const AgenticWorkflowService&) = delete;
	/** Resolve one launch handoff; explicit configuration takes precedence over environment. */
	[[nodiscard]] Status configure(const AgenticWorkflowConfig& configuration) noexcept;
	/** Validate, copy and enqueue without calling application producers or writing artifacts. */
	[[nodiscard]] Result<AgenticJobId>
	request_capture(const AgenticCaptureRequest& request) noexcept;
	[[nodiscard]] Result<AgenticJobStatus> query_job(std::string_view job_id) const noexcept;
	[[nodiscard]] Status cancel_job(std::string_view job_id) noexcept;
	[[nodiscard]] Status stop_capture(std::string_view job_id) noexcept;
	[[nodiscard]] Result<AgenticCapabilities> capabilities() const noexcept;
	/** Register application semantics before any scenario using them is admitted. */
	[[nodiscard]] Status register_scenario(AgenticScenario scenario) noexcept;
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
	[[nodiscard]] Status accept_completed_image(AgenticCompletedImage image) noexcept;
	/** Copy a selected published tree while its App frame still owns the publication. */
	void observe_tree(std::uint64_t window_id) noexcept;
	/** Used by manual capture admission to arbitrate the same application capture lease. */
	[[nodiscard]] bool capture_lease_active() const noexcept;
	[[nodiscard]] bool optional_collection_enabled() const noexcept;
	/** Apply the active policy to a newly constructed window and remember its prior state. */
	void prepare_window_collection(std::uint64_t window_id) noexcept;
	[[nodiscard]] bool visual_capture_enabled() const noexcept;
	/** Estimated owned queue/snapshot bytes, excluding thread stacks and encoder temporaries. */
	[[nodiscard]] std::uint64_t owned_memory_bytes() const noexcept;
	/** Transport entry point: executes through the platform queue with a bounded wait. */
	[[nodiscard]] Result<Json> submit_command(const AgenticCommandRequest& request) noexcept;
	/** Stop new local commands before draining renderer resources. */
	void stop_admission() noexcept;
	[[nodiscard]] Status shutdown(const AgenticShutdownPolicy& policy = {}) noexcept;
	/** Keep scenario/App references valid when the movable public App wrapper moves. */
	void rebind_application(App& application) noexcept;

private:
	friend class AgenticInteractionRuntime;
	/** Copy the active scenario owner's policy before any child observations are retained. */
	[[nodiscard]] Result<AgenticContentPolicy>
	interaction_content_policy(std::string_view owner_job_id) const noexcept;
	struct Impl;
	std::unique_ptr<Impl> impl_;
};
} // namespace FlowUi::devSystems::agentic
#endif
