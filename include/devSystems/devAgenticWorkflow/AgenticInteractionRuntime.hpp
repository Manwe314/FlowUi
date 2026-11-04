#pragma once
#include "FlowUi/BuildConfig.hpp"
#if FLOW_UI_AGENTIC_WORKFLOW
#include "devSystems/devAgenticWorkflow/AgenticCommands.hpp"
#include "devSystems/devAgenticWorkflow/AgenticInteraction.hpp"
#include "devSystems/devAgenticWorkflow/AgenticSnapshot.hpp"
#include "managers/structs/InputStructs.hpp"
#include <memory>

namespace FlowUi {
class App;
}
namespace FlowUi::devSystems::agentic {
class AgenticWorkflowService;
class AgenticInteractionTargetRegistry;
class AgenticActionRegistry;
/** Platform-thread executor owned by the App service; workers receive only sealed evidence. */
class AgenticInteractionRuntime {
public:
	AgenticInteractionRuntime(AgenticWorkflowService& service, App& application,
							  const AgenticWorkflowConfig& configuration);
	~AgenticInteractionRuntime();
	AgenticInteractionRuntime(const AgenticInteractionRuntime&) = delete;
	AgenticInteractionRuntime& operator=(const AgenticInteractionRuntime&) = delete;
	/** Dispatch a validated live interaction/action operation on the platform thread. */
	[[nodiscard]] Result<Json> command(const AgenticCommandRequest& request) noexcept;
	/** C++ scenario entry; an owner job permits composition without taking a second capture lease.
	 */
	[[nodiscard]] Result<AgenticJobId> start_plan(const Json& plan,
												  std::string_view idempotency_key = {},
												  std::string_view owner_job_id = {}) noexcept;
	/** Owned execution result for nonblocking application/scenario completion checks. */
	[[nodiscard]] Result<AgenticInteractionResult>
	query_result(std::string_view job_id) const noexcept;
	/** Replace assessed input only for an executor-owned window, otherwise apply physical handoff.
	 */
	[[nodiscard]] Status select_input(const EvidenceFrameKey& key, FrameInput& input,
									  float ui_scale) noexcept;
	/** Copy the completed layout's interaction/focus observations before its storage is reused. */
	void observe_ui(const EvidenceFrameKey& key) noexcept;
	/** Preserve cancellation/presentation outcome independently of input delivery. */
	void observe_frame(const AgenticFrameRecord& frame) noexcept;
	/** Advance acknowledged work without manufacturing application frames. */
	void advance(std::uint64_t app_tick, std::uint64_t now_ns) noexcept;
	/** Attach immutable bounded interaction artifacts to their owning capture before publication.
	 */
	[[nodiscard]] Status materialize(AgenticCaptureSnapshot& snapshot) noexcept;
	/** A capture cutoff stops new input and waits only for bounded release observations. */
	[[nodiscard]] bool ready_to_materialize(std::string_view capture_job_id) noexcept;
	/** Stop execution and record unavailable release observations when no further frame can run. */
	void shutdown() noexcept;
	/** Carry the non-owning App dependency across movement of the public App wrapper. */
	void rebind_application(App& application) noexcept;
	[[nodiscard]] bool active() const noexcept;
	[[nodiscard]] bool controls_window(std::uint64_t window_id) const noexcept;
	[[nodiscard]] bool admits_capture() const noexcept;
	[[nodiscard]] bool owns_job(std::string_view job_id) const noexcept;
	[[nodiscard]] std::uint64_t owned_memory_bytes() const noexcept;
	[[nodiscard]] AgenticInteractionTargetRegistry& targets() noexcept;
	[[nodiscard]] AgenticActionRegistry& actions() noexcept;

private:
	friend class AgenticWorkflowService;
	/** Owner-synchronized metadata/cancellation never enters application callbacks or JSON
	 * encoding. */
	[[nodiscard]] std::string_view capture_job_id(std::string_view job_id) const noexcept;
	[[nodiscard]] Status request_cancel(std::string_view job_id) noexcept;
	struct Impl;
	std::unique_ptr<Impl> impl_;
};
} // namespace FlowUi::devSystems::agentic
#endif
