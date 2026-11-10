#include "devSystems/devAgenticWorkflow/AgenticInteractionRuntime.hpp"
#if FLOW_UI_AGENTIC_WORKFLOW
#include "FlowUi/Public/App.hpp"
#include "devSystems/devAgenticWorkflow/AgenticAction.hpp"
#include "devSystems/devAgenticWorkflow/AgenticInteraction.hpp"
#include "devSystems/devAgenticWorkflow/AgenticInteractionTargets.hpp"
#include "devSystems/devAgenticWorkflow/AgenticManifest.hpp"
#include "devSystems/devAgenticWorkflow/AgenticService.hpp"
#include "FlowUi/Public/managers/UiManager.hpp"
#include <algorithm>
#include <array>
#include <cmath>
#include <limits>
#include <thread>

namespace FlowUi::devSystems::agentic {
namespace {
constexpr std::size_t maximum_terminal_history = 32;
constexpr std::size_t maximum_idempotency_receipts = 1024;
[[nodiscard]] bool terminal(AgenticInteractionPhase phase) noexcept {
	return phase == AgenticInteractionPhase::completed ||
		   phase == AgenticInteractionPhase::failed || phase == AgenticInteractionPhase::canceled;
}
[[nodiscard]] bool neutral(const FrameInput& input) noexcept {
	return std::ranges::none_of(input.keyDown, [](bool down) { return down; }) &&
		   std::ranges::none_of(input.mouseDown, [](bool down) { return down; });
}
[[nodiscard]] std::uint64_t integer(const Json& value, std::string_view key,
									std::uint64_t fallback = 0) noexcept {
	const auto* member = value.find(key);
	return member ? member->uint64_or(fallback) : fallback;
}
[[nodiscard]] std::string_view string(const Json& value, std::string_view key) noexcept {
	const auto* member = value.find(key);
	return member ? member->string_or() : std::string_view{};
}
[[nodiscard]] Json bounded_success(const AgenticCommandRequest& request, Json value,
								   std::string_view status = "completed") {
	const auto maximum = integer(request.arguments, "max_bytes", 65536);
	auto envelope = command_success(request, std::move(value), status);
	if (maximum < 1024 || maximum > 1024u * 1024u || write_json(envelope).size() > maximum)
		return command_failure(request, 6, "AgenticBudgetExhausted",
							   "The interaction result exceeds the requested response budget.");
	return envelope;
}
[[nodiscard]] std::string request_digest(std::string_view operation, const Json& arguments) {
	auto semantic = arguments;
	semantic.object.erase(std::remove_if(semantic.object.begin(), semantic.object.end(),
										 [](const auto& member) {
											 return member.first == "instance" ||
													member.first == "max_bytes" ||
													member.first == "idempotency_key";
										 }),
						  semantic.object.end());
	return sha256(write_json(
		Json::make_object({{"operation", operation}, {"arguments", std::move(semantic)}})));
}
[[nodiscard]] bool matches(const Json& record, const Json& arguments) {
	for (const auto key :
		 {"offset", "sequence", "plan_digest", "window_id", "app_tick", "window_frame"}) {
		const auto* expected = arguments.find(key);
		if (!expected)
			continue;
		const auto* actual = record.find(key);
		if (!actual) {
			const auto* frame = record.find("frame");
			if (frame)
				actual = frame->find(key);
		}
		if (!actual || write_json(*actual) != write_json(*expected))
			return false;
	}
	return true;
}
[[nodiscard]] bool same_frame(const EvidenceFrameKey& first,
							  const EvidenceFrameKey& second) noexcept {
	return first.window_id == second.window_id && first.window_frame == second.window_frame &&
		   first.app_tick == second.app_tick;
}
[[nodiscard]] std::uint32_t bits(AgenticInteractionExpectation value) noexcept {
	return static_cast<std::uint32_t>(value);
}
[[nodiscard]] std::uint64_t input_bytes(const AgenticInputSnapshot& input) noexcept {
	return sizeof(input) + input.text.capacity() * sizeof(char32_t) +
		   input.keys_down.capacity() * sizeof(std::uint16_t) + input.buttons_down.capacity();
}
} // namespace
struct AgenticInteractionRuntime::Impl {
	struct Execution {
		AgenticCompiledInteraction compiled;
		AgenticContentPolicy content;
		AgenticInteractionResult result;
		std::vector<AgenticInteractionReceipt> inputs;
		std::vector<AgenticInteractionObservation> observations;
		std::vector<std::uint64_t> step_started;
		std::vector<std::optional<AgenticResolvedInteractionTarget>> resolved, destinations;
		std::optional<AgenticActionInvocation> action_invocation;
		std::optional<AgenticActionReceipt> action_receipt;
		AgenticInputSnapshot previous;
		Json replay_source;
		std::string capture_job_id, capture_id, idempotency_key, request_digest, artifact_suffix;
		std::uint64_t started_ns = 0, offset = 0, actual_frames = 0, record_limit = 0,
					  gate_step = 0, owned_bytes = 0, release_started_ns = 0;
		std::uint64_t cleanup_frames = 0, recorded_text = 0;
		float ui_scale = 1.0f;
		bool recording = false, include_text = false, cancel = false, stop = false, gate = false,
			 owns_capture = true;
		bool cleanup_observed = false, baseline_observed = false, materialized = false,
			 payload_evicted = false, drain_selected = false, drain_observed = false;
		AgenticInteractionPhase final_phase = AgenticInteractionPhase::completed;
		JobPhase owner_phase = JobPhase::queued;
	};
	struct Handoff {
		std::array<bool, FrameInput::kKeyboardKeyCount> keys{};
		std::array<bool, FrameInput::kMouseButtonCount> buttons{};
		std::uint64_t window_id = 0;
	};
	struct IdempotencyReceipt {
		std::string job_id;
		std::string idempotency_key;
		std::string request_digest;
		std::string capture_job_id;
		std::string capture_id;
		bool owns_capture = true;
	};
	AgenticWorkflowService* service;
	App* application;
	AgenticWorkflowConfig configuration;
	AgenticInteractionCapabilities capabilities;
	AgenticInteractionTargetRegistry target_registry;
	AgenticActionRegistry action_registry;
	std::vector<std::unique_ptr<Execution>> executions;
	std::vector<IdempotencyReceipt> idempotency_receipts;
	std::vector<Handoff> handoffs;
	std::vector<std::pair<std::uint64_t, float>> scales;
	std::thread::id platform_thread = std::this_thread::get_id();
	std::size_t active_index = SIZE_MAX;
	std::uint64_t last_safe_tick = 0, action_sequence = 0;
	bool capture_admission = false, stopping = false;
	Impl(AgenticWorkflowService& supplied, App& app, const AgenticWorkflowConfig& config)
		: service(&supplied), application(&app), configuration(config) {
		executions.reserve(64);
		idempotency_receipts.reserve(maximum_idempotency_receipts);
		handoffs.reserve(64);
		scales.reserve(64);
		capabilities.geometry_targets = FLOW_UI_DEV_CAPTURE_CLAY != 0;
		capabilities.custom_targets = true;
		capabilities.text_focus_observations = true;
		capabilities.limits.maximum_artifact_bytes = std::min(
			capabilities.limits.maximum_artifact_bytes, config.limits.maximum_snapshot_bytes / 2);
		capabilities.limits.maximum_frames =
			std::min(capabilities.limits.maximum_frames, config.limits.maximum_ticks);
	}
	[[nodiscard]] Execution* active() noexcept {
		return active_index < executions.size() ? executions[active_index].get() : nullptr;
	}
	[[nodiscard]] Execution* find(std::string_view job_id) noexcept {
		for (auto& execution : executions)
			if (execution->result.job_id == job_id)
				return execution.get();
		return nullptr;
	}
	void collect_terminal_history() {
		for (auto& prior : executions)
			if (prior->materialized && !prior->payload_evicted) {
				std::vector<AgenticInteractionReceipt>().swap(prior->inputs);
				std::vector<AgenticInteractionObservation>().swap(prior->observations);
				std::vector<AgenticInteractionSpan>().swap(prior->compiled.spans);
				std::vector<AgenticInteractionSpan>().swap(prior->compiled.plan.spans);
				std::vector<AgenticInteractionStep>().swap(prior->compiled.plan.steps);
				std::vector<std::uint64_t>().swap(prior->step_started);
				std::vector<std::optional<AgenticResolvedInteractionTarget>>().swap(
					prior->resolved);
				std::vector<std::optional<AgenticResolvedInteractionTarget>>().swap(
					prior->destinations);
				prior->previous = {};
				prior->replay_source = {};
				prior->payload_evicted = true;
				prior->compiled.estimated_bytes = 0;
			}
		auto terminal_count = static_cast<std::size_t>(
			std::count_if(executions.begin(), executions.end(), [](const auto& execution) {
				return execution->materialized && terminal(execution->result.phase);
			}));
		for (auto position = executions.begin();
			 position != executions.end() && terminal_count > maximum_terminal_history;) {
			const auto& execution = **position;
			if (!execution.materialized || !terminal(execution.result.phase)) {
				++position;
				continue;
			}
			if (!execution.idempotency_key.empty()) {
				if (idempotency_receipts.size() == maximum_idempotency_receipts)
					idempotency_receipts.erase(idempotency_receipts.begin());
				idempotency_receipts.emplace_back(IdempotencyReceipt{
					execution.result.job_id, execution.idempotency_key, execution.request_digest,
					execution.capture_job_id, execution.capture_id, execution.owns_capture});
			}
			position = executions.erase(position);
			--terminal_count;
		}
	}
	[[nodiscard]] float scale(std::uint64_t window_id) const noexcept {
		for (const auto& entry : scales)
			if (entry.first == window_id)
				return entry.second;
		return 1.0f;
	}
	[[nodiscard]] Json fail(const AgenticCommandRequest& request, FlowUiError error) const {
		const int code =
			error.code == ErrorCode::AgenticInvalidRequest ||
					error.code == ErrorCode::AgenticInvalidScope
				? 2
			: error.code == ErrorCode::AgenticUnavailable || error.code == ErrorCode::AgenticBusy
				? 3
				: 6;
		return command_failure(request, code, errorName(error.code), errorMessage(error.code));
	}
	[[nodiscard]] Result<AgenticCaptureRequest>
	capture_request(const Json* supplied, std::uint64_t window_id, std::uint64_t deadline) {
		AgenticCaptureRequest request;
		if (supplied) {
			auto decoded = capture_request_from_json(*supplied);
			if (!decoded)
				return unexpectedError(decoded.error());
			request = std::move(*decoded);
		} else {
			const auto available = service->capabilities();
			if (!available)
				return unexpectedError(available.error());
			request.limits = available->limits;
			request.performance = SectionRequirement::omit;
			request.runtime_diagnostics = SectionRequirement::omit;
			request.visual = SectionRequirement::omit;
			request.memory = SectionRequirement::omit;
			request.duration_ns = deadline;
			request.maximum_expected_tick_cadence = 1;
		}
		if (request.window_ids.empty())
			request.window_ids = {window_id};
		if (request.window_ids.size() != 1 || request.window_ids.front() != window_id ||
			!request.scenario_id.empty())
			return unexpectedError(agentic_error(AgenticError::invalid_scope));
		return request;
	}
	[[nodiscard]] Result<AgenticJobId> admit(std::unique_ptr<Execution> execution,
											 const Json* capture, std::string_view owner = {}) {
		if (active() || stopping || (!owner.empty() && capture))
			return unexpectedError(agentic_error(AgenticError::busy));
		collect_terminal_history();
		if (executions.size() >= 64)
			return unexpectedError(agentic_error(AgenticError::busy));
		const auto reservation =
			execution->compiled.plan.limits.maximum_observations *
				(sizeof(AgenticInteractionReceipt) + sizeof(AgenticInteractionObservation)) +
			execution->compiled.estimated_bytes;
		std::uint64_t retained_reservation = 0;
		for (const auto& prior : executions)
			retained_reservation +=
				prior->inputs.capacity() * sizeof(AgenticInteractionReceipt) +
				prior->observations.capacity() * sizeof(AgenticInteractionObservation) +
				prior->compiled.estimated_bytes;
		auto reservation_budget = configuration.limits.maximum_snapshot_bytes / 2;
		if (!owner.empty()) {
			const auto owner_budget = service->interaction_snapshot_budget(owner);
			if (!owner_budget)
				return unexpectedError(owner_budget.error());
			reservation_budget = std::min(reservation_budget, *owner_budget / 2);
		}
		if (reservation > reservation_budget - std::min(reservation_budget, retained_reservation))
			return unexpectedError(agentic_error(AgenticError::budget_exhausted));
		const auto window_id = execution->compiled.plan.window_id;
		if (window_id > UINT32_MAX || !application->hasWindow(static_cast<WindowId>(window_id)))
			return unexpectedError(agentic_error(AgenticError::invalid_scope));
		if (execution->compiled.plan.clock == AgenticInteractionClock::exact_window_frames &&
			execution->compiled.plan.first_window_frame <=
				application->ui(static_cast<WindowId>(window_id)).dev_completed_window_frame())
			return unexpectedError(agentic_error(AgenticError::stale_generation));
		const auto available = service->capabilities();
		if (!available)
			return unexpectedError(available.error());
		if (!execution->compiled.plan.run_id.empty() &&
			execution->compiled.plan.run_id != available->run_id)
			return unexpectedError(agentic_error(AgenticError::invalid_scope));
		auto identity = make_identifier("interaction");
		if (!identity)
			return unexpectedError(identity.error());
		execution->compiled.plan.run_id = available->run_id;
		execution->result.job_id = *identity;
		if (execution->action_invocation)
			execution->action_invocation->plan_id = *identity;
		execution->result.run_id = available->run_id;
		execution->result.window_id = window_id;
		execution->result.plan_digest = execution->compiled.digest;
		execution->result.coverage.scheduled = execution->compiled.frame_count;
		execution->result.phase = AgenticInteractionPhase::queued;
		execution->result.reason.reserve(256);
		execution->result.limitations.reserve(32);
		execution->result.limitations = execution->compiled.limitations;
		execution->result.limitations.emplace_back(
			"effective_input_does_not_establish_os_foreground_focus");
		const auto bound = execution->compiled.plan.limits.maximum_observations;
		execution->inputs.reserve(static_cast<std::size_t>(bound + 4));
		execution->observations.reserve(static_cast<std::size_t>(bound + 4));
		execution->step_started.resize(execution->compiled.plan.steps.size());
		execution->resolved.resize(execution->compiled.plan.steps.size());
		execution->destinations.resize(execution->compiled.plan.steps.size());
		execution->result.steps.reserve(execution->compiled.plan.steps.size());
		for (const auto& step : execution->compiled.plan.steps) {
			AgenticInteractionStepResult result;
			result.step_id = step.step_id;
			result.expected = step.expect;
			execution->result.steps.emplace_back(std::move(result));
		}
		execution->started_ns = monotonic_nanoseconds();
		if (owner.empty()) {
			if (service->capture_lease_active())
				return unexpectedError(agentic_error(AgenticError::busy));
			auto requested =
				capture_request(capture, window_id, execution->compiled.plan.deadline_ns);
			if (!requested)
				return unexpectedError(requested.error());
			execution->content = requested->content;
			requested->fixture_id = execution->compiled.plan.fixture_id;
			requested->scenario_digest = execution->compiled.digest;
			requested->scenario_revision = "interaction-recipe-1.0";
			if (requested->profile == CaptureProfile::performance_verify &&
				execution->compiled.requires_geometry)
				return unexpectedError(agentic_error(AgenticError::incompatible));
			capture_admission = true;
			auto admitted = service->request_capture(*requested);
			capture_admission = false;
			if (!admitted)
				return unexpectedError(admitted.error());
			execution->capture_job_id = std::move(*admitted);
		} else {
			if (execution->compiled.requires_geometry && !service->optional_collection_enabled())
				return unexpectedError(agentic_error(AgenticError::incompatible));
			execution->capture_job_id = owner;
			execution->owns_capture = false;
		}
		const auto status = service->query_job(execution->capture_job_id);
		if (!status)
			return unexpectedError(status.error());
		execution->capture_id = status->capture_id;
		execution->owner_phase = status->phase;
		if (!owner.empty()) {
			const auto tracked = service->track_owned_interaction(owner, *identity);
			if (!tracked)
				return unexpectedError(tracked.error());
		}
		executions.emplace_back(std::move(execution));
		active_index = executions.size() - 1;
		return std::move(*identity);
	}
	void request_release(Execution& execution, std::string_view reason,
						 AgenticInteractionPhase final_phase) {
		if (terminal(execution.result.phase))
			return;
		execution.result.reason = reason;
		execution.final_phase = final_phase;
		if (execution.result.phase != AgenticInteractionPhase::releasing_control)
			execution.release_started_ns = monotonic_nanoseconds();
		if (final_phase == AgenticInteractionPhase::failed &&
			!execution.result.first_divergence_offset)
			execution.result.first_divergence_offset = execution.offset;
		execution.result.phase = AgenticInteractionPhase::releasing_control;
		execution.gate = false;
		if (execution.action_receipt && !execution.action_receipt->completed) {
			const auto canceled = action_registry.cancel(*application, *execution.action_receipt);
			if (!canceled)
				execution.result.limitations.emplace_back(
					"application_action_cancellation_unavailable");
		}
		if (execution.action_invocation)
			execution.cleanup_observed = true;
	}
	void complete(Execution& execution, bool observations_available = true) {
		execution.result.phase = execution.final_phase;
		execution.result.cleanup_complete =
			observations_available &&
			(execution.cleanup_observed || execution.action_invocation.has_value());
		execution.result.execution_succeeded =
			execution.final_phase == AgenticInteractionPhase::completed;
		execution.result.evidence_complete =
			execution.result.evidence_complete && observations_available;
		if (execution.result.assertion_verdict == AgenticInteractionVerdict::pending)
			execution.result.assertion_verdict = execution.result.execution_succeeded
													 ? AgenticInteractionVerdict::passed
													 : AgenticInteractionVerdict::unavailable;
		if (execution.action_invocation)
			execution.result.handoff_complete = true;
		if (!observations_available)
			execution.result.limitations.emplace_back("neutralization_or_observations_unavailable");
		if (execution.owns_capture) {
			const auto stopped = service->stop_capture(execution.capture_job_id);
			if (!stopped)
				execution.result.limitations.emplace_back("capture_already_stopped");
		}
		active_index = SIZE_MAX;
	}
	void bound_evidence(Execution& execution) {
		if (execution.inputs.size() >= execution.compiled.plan.limits.maximum_observations ||
			execution.observations.size() >= execution.compiled.plan.limits.maximum_observations ||
			execution.actual_frames > execution.compiled.plan.limits.maximum_frames + 4) {
			++execution.result.coverage.lost;
			execution.result.evidence_complete = false;
			request_release(execution, "interaction_evidence_budget_exhausted",
							AgenticInteractionPhase::failed);
		}
	}
	[[nodiscard]] bool charge(Execution& execution, std::uint64_t bytes, bool cleanup = false) {
		const auto bound = execution.compiled.plan.limits.maximum_artifact_bytes;
		if (bytes > bound - std::min(bound, execution.owned_bytes)) {
			++execution.result.coverage.lost;
			execution.result.evidence_complete = false;
			request_release(execution, "interaction_retained_bytes_exhausted",
							AgenticInteractionPhase::failed);
			return cleanup && bytes <= 16384;
		}
		execution.owned_bytes += bytes;
		return true;
	}
	[[nodiscard]] Json status(Execution& execution) {
		auto parent = service->query_job(execution.capture_job_id);
		auto value = parent ? to_json(*parent) : Json::make_object();
		value.set("job_id", execution.result.job_id);
		value.set("capture_job_id", execution.capture_job_id);
		value.set("capture_id", execution.capture_id);
		value.set(
			"interaction",
			Json::make_object({{"phase", name(execution.result.phase)},
							   {"reason", execution.result.reason},
							   {"execution_succeeded", execution.result.execution_succeeded},
							   {"assertion_verdict", name(execution.result.assertion_verdict)},
							   {"scheduled", decimal(execution.result.coverage.scheduled)},
							   {"consumed", decimal(execution.result.coverage.consumed)},
							   {"cleanup_complete", execution.result.cleanup_complete},
							   {"handoff_complete", execution.result.handoff_complete}}));
		return value;
	}
	[[nodiscard]] Json accepted(const Execution& execution) const {
		const auto parent = service->query_job(execution.capture_job_id);
		return Json::make_object(
			{{"job_id", execution.result.job_id},
			 {"capture_job_id", execution.capture_job_id},
			 {"capture_id", execution.capture_id},
			 {"run_id", execution.result.run_id},
			 {"phase", parent ? name(parent->phase) : "failed"},
			 {"input_anchor",
			  execution.action_invocation ? "not_applicable"
			  : execution.recording		  ? "first_recorded_effective_input_frame"
			  : execution.compiled.plan.clock == AgenticInteractionClock::exact_window_frames
				  ? "exact_window_frame"
				  : "first_valid_input_frame_after_baseline"}});
	}
	[[nodiscard]] Json result(const Execution& execution) const {
		auto value = to_json(execution.result);
		std::optional<AgenticActionReceipt> rejected;
		const auto* receipt = execution.action_receipt ? &*execution.action_receipt : nullptr;
		if (!receipt && execution.action_invocation) {
			rejected.emplace();
			rejected->invocation = *execution.action_invocation;
			rejected->reason = execution.result.reason;
			receipt = &*rejected;
		}
		if (receipt) {
			auto action = AgenticActionRegistry::describe(*receipt);
			action.set("arguments", execution.content.include_interaction_outcomes
										? receipt->invocation.arguments
										: Json::make_object());
			if (!execution.content.include_interaction_outcomes) {
				action.set("result", Json::make_object());
				action.set("result_redacted", true);
				action.set("arguments_redacted", true);
			}
			value.set("action", std::move(action));
		}
		return value;
	}
	void handoff(Execution& execution, const FrameInput& native_input) {
		auto found = std::ranges::find_if(handoffs, [&](const auto& entry) {
			return entry.window_id == execution.result.window_id;
		});
		if (found == handoffs.end()) {
			handoffs.emplace_back();
			found = std::prev(handoffs.end());
			found->window_id = execution.result.window_id;
		}
		found->keys = native_input.keyDown;
		found->buttons = native_input.mouseDown;
		execution.result.handoff_complete = neutral(native_input);
		if (!execution.result.handoff_complete)
			execution.result.limitations.emplace_back(
				"physical_held_inputs_quarantined_until_release");
	}
};
AgenticInteractionRuntime::AgenticInteractionRuntime(AgenticWorkflowService& service, App& app,
													 const AgenticWorkflowConfig& config)
	: impl_(std::make_unique<Impl>(service, app, config)) {}
AgenticInteractionRuntime::~AgenticInteractionRuntime() = default;
AgenticInteractionTargetRegistry& AgenticInteractionRuntime::targets() noexcept {
	return impl_->target_registry;
}
AgenticActionRegistry& AgenticInteractionRuntime::actions() noexcept {
	return impl_->action_registry;
}
bool AgenticInteractionRuntime::active() const noexcept {
	return impl_->active() != nullptr;
}
bool AgenticInteractionRuntime::admits_capture() const noexcept {
	return impl_->capture_admission;
}
bool AgenticInteractionRuntime::controls_window(std::uint64_t window_id) const noexcept {
	const auto* execution = impl_->active();
	if (execution && !execution->recording && !execution->action_invocation &&
		execution->result.window_id == window_id)
		return true;
	for (const auto& handoff : impl_->handoffs)
		if (handoff.window_id == window_id &&
			(std::ranges::any_of(handoff.keys, [](bool down) { return down; }) ||
			 std::ranges::any_of(handoff.buttons, [](bool down) { return down; })))
			return true;
	return false;
}
bool AgenticInteractionRuntime::owns_job(std::string_view identity) const noexcept {
	return impl_->find(identity) != nullptr;
}
std::string_view
AgenticInteractionRuntime::capture_job_id(std::string_view identity) const noexcept {
	const auto* execution = impl_->find(identity);
	return execution ? std::string_view(execution->capture_job_id) : std::string_view{};
}
Status AgenticInteractionRuntime::request_cancel(std::string_view identity) noexcept {
	auto* execution = impl_->find(identity);
	if (!execution)
		return unexpectedError(agentic_error(AgenticError::unavailable));
	execution->cancel = true;
	return {};
}
Result<AgenticInteractionResult>
AgenticInteractionRuntime::query_result(std::string_view identity) const noexcept {
	try {
		if (std::this_thread::get_id() != impl_->platform_thread)
			return unexpectedError(agentic_error(AgenticError::invalid_scope));
		const auto* execution = impl_->find(identity);
		if (!execution)
			return unexpectedError(agentic_error(AgenticError::unavailable));
		return execution->result;
	} catch (...) {
		return unexpectedError(agentic_error(AgenticError::budget_exhausted));
	}
}
void AgenticInteractionRuntime::rebind_application(App& application) noexcept {
	impl_->application = &application;
}
Result<AgenticJobId> AgenticInteractionRuntime::start_plan(const Json& source, std::string_view key,
														   std::string_view owner) noexcept {
	try {
		if (std::this_thread::get_id() != impl_->platform_thread ||
			!impl_->configuration.input_control_enabled)
			return unexpectedError(agentic_error(AgenticError::unavailable));
		auto decoded = interaction_plan_from_json(source);
		if (!decoded)
			return unexpectedError(decoded.error());
		const auto available = impl_->service->capabilities();
		if (!available)
			return unexpectedError(available.error());
		if (!decoded->run_id.empty() && decoded->run_id != available->run_id)
			return unexpectedError(agentic_error(AgenticError::invalid_scope));
		decoded->run_id = available->run_id;
		auto compiled = compile_interaction_plan(*decoded, impl_->capabilities);
		if (!compiled)
			return unexpectedError(compiled.error());
		const auto idempotency = key.empty() ? std::string_view(decoded->idempotency_key) : key;
		const auto digest = owner.empty() ? request_digest("interaction.start",
														   Json::make_object({{"plan", source}}))
										  : sha256(std::string(owner) + compiled->digest);
		if (!idempotency.empty())
			for (const auto& prior : impl_->executions)
				if (prior->idempotency_key == idempotency &&
					(owner.empty() ? prior->owns_capture
								   : !prior->owns_capture && prior->capture_job_id == owner))
					return prior->request_digest == digest
							   ? Result<AgenticJobId>(prior->result.job_id)
							   : unexpectedError(agentic_error(AgenticError::invalid_request));
		if (!idempotency.empty())
			for (const auto& prior : impl_->idempotency_receipts)
				if (prior.idempotency_key == idempotency &&
					(owner.empty() ? prior.owns_capture
								   : !prior.owns_capture && prior.capture_job_id == owner))
					return unexpectedError(agentic_error(prior.request_digest == digest
															 ? AgenticError::unavailable
															 : AgenticError::invalid_request));
		auto execution = std::make_unique<Impl::Execution>();
		execution->idempotency_key = idempotency;
		execution->request_digest = digest;
		execution->compiled = std::move(*compiled);
		if (!owner.empty()) {
			auto content = impl_->service->interaction_content_policy(owner);
			if (!content)
				return unexpectedError(content.error());
			execution->content = *content;
		}
		return impl_->admit(std::move(execution), nullptr, owner);
	} catch (...) {
		impl_->capture_admission = false;
		return unexpectedError(agentic_error(AgenticError::budget_exhausted));
	}
}
Result<Json> AgenticInteractionRuntime::command(const AgenticCommandRequest& request) noexcept {
	try {
		if (std::this_thread::get_id() != impl_->platform_thread || impl_->stopping)
			return unexpectedError(agentic_error(AgenticError::unavailable));
		const auto valid_arguments = validate_interaction_command_arguments(request);
		if (!valid_arguments)
			return impl_->fail(request, valid_arguments.error());
		const auto& arguments = request.arguments;
		const auto& operation = request.operation;
		if (operation == "interaction.capabilities") {
			auto value = to_json(impl_->capabilities);
			value.set("input_control_enabled", impl_->configuration.input_control_enabled);
			value.set("action_control_enabled", impl_->configuration.action_control_enabled);
			value.set("physical_cancel_chord", "Ctrl+Alt+Escape");
			value.set("one_app_per_process", true);
			value.set("maximum_terminal_history", decimal(maximum_terminal_history));
			value.set("maximum_idempotency_receipts", decimal(maximum_idempotency_receipts));
			value.set("idempotency_retention", "last_1024_evicted_keyed_executions");
			return bounded_success(request, std::move(value));
		}
		if (operation == "action.list") {
			auto listed = impl_->action_registry.list(
				*impl_->application, static_cast<std::size_t>(integer(arguments, "limit", 100)),
				static_cast<std::size_t>(integer(arguments, "cursor")));
			return listed ? bounded_success(request, std::move(*listed))
						  : impl_->fail(request, listed.error());
		}
		if (operation == "interaction.targets") {
			const auto window = integer(arguments, "window_id", impl_->application->mainWindowId());
			if (window > UINT32_MAX)
				return impl_->fail(request, agentic_error(AgenticError::invalid_scope));
			if (arguments.find("cursor") && !string(arguments, "cursor").empty() &&
				string(arguments, "cursor") != "0")
				return impl_->fail(request, agentic_error(AgenticError::invalid_request));
			if (const auto* target = arguments.find("target")) {
				auto source = Json::make_object(
					{{"mode", "common_controls"},
					 {"window_id", decimal(window)},
					 {"steps", Json::make_array({Json::make_object(
								   {{"operation", "pointer.move"}, {"target", *target}})})}});
				auto decoded = interaction_plan_from_json(source);
				if (!decoded || decoded->steps.empty() || !decoded->steps.front().target)
					return impl_->fail(request, agentic_error(AgenticError::invalid_request));
				auto resolved = impl_->target_registry.resolve(
					*impl_->application, static_cast<WindowId>(window),
					*decoded->steps.front().target, impl_->scale(window));
				return resolved ? bounded_success(request, to_json(*resolved))
								: impl_->fail(request, resolved.error());
			}
			auto listed = impl_->target_registry.discover(
				*impl_->application, static_cast<WindowId>(window), impl_->scale(window),
				static_cast<std::size_t>(integer(arguments, "limit", 100)));
			return listed ? bounded_success(request, std::move(*listed))
						  : impl_->fail(request, listed.error());
		}
		if (operation == "interaction.start" || operation == "interaction.record.start" ||
			operation == "action.invoke") {
			const auto digest = request_digest(operation, arguments);
			auto key = string(arguments, "idempotency_key");
			if (key.empty() && operation == "interaction.start")
				if (const auto* plan = arguments.find("plan"))
					key = string(*plan, "idempotency_key");
			if (!key.empty())
				for (auto& prior : impl_->executions)
					if (prior->owns_capture && prior->idempotency_key == key)
						return prior->request_digest == digest
								   ? bounded_success(request, impl_->accepted(*prior), "accepted")
								   : impl_->fail(request,
												 agentic_error(AgenticError::invalid_request));
			if (!key.empty())
				for (const auto& prior : impl_->idempotency_receipts)
					if (prior.owns_capture && prior.idempotency_key == key) {
						auto response = command_failure(
							request, prior.request_digest == digest ? 3 : 2,
							prior.request_digest == digest ? "result_expired"
														   : "idempotency_conflict",
							prior.request_digest == digest
								? "Runtime history expired; retrieve sealed evidence using its "
								  "capture identity."
								: "The retained idempotency key identifies a different request.");
						response.set("job_id", prior.job_id);
						response.find("error")->set(
							"details",
							Json::make_object({{"capture_id", prior.capture_id},
											   {"capture_job_id", prior.capture_job_id}}));
						return response;
					}
			auto execution = std::make_unique<Impl::Execution>();
			execution->idempotency_key = key;
			execution->request_digest = digest;
			if (operation == "interaction.start") {
				if (!impl_->configuration.input_control_enabled)
					return impl_->fail(request, agentic_error(AgenticError::unavailable));
				const auto* plan = arguments.find("plan");
				if (!plan)
					return impl_->fail(request, agentic_error(AgenticError::invalid_request));
				auto decoded = interaction_plan_from_json(*plan);
				if (!decoded)
					return impl_->fail(request, decoded.error());
				if (arguments.find("rebind_run") && arguments.find("rebind_run")->bool_or()) {
					if (decoded->clock != AgenticInteractionClock::window_input_frames)
						return impl_->fail(request, agentic_error(AgenticError::invalid_scope));
					execution->replay_source =
						Json::make_object({{"source_run_id", decoded->run_id},
										   {"verification", "caller_declared_at_live_boundary"}});
					if (const auto* source = arguments.find("replay_source"))
						execution->replay_source.set("artifact", *source);
					decoded->run_id.clear();
				}
				if (decoded->run_id.empty()) {
					auto available = impl_->service->capabilities();
					if (!available)
						return impl_->fail(request, available.error());
					decoded->run_id = available->run_id;
				}
				auto compiled = compile_interaction_plan(*decoded, impl_->capabilities);
				if (!compiled)
					return impl_->fail(request, compiled.error());
				execution->compiled = std::move(*compiled);
			} else if (operation == "interaction.record.start") {
				if (!impl_->configuration.input_control_enabled)
					return impl_->fail(request, agentic_error(AgenticError::unavailable));
				execution->recording = true;
				execution->compiled.plan.window_id =
					integer(arguments, "window_id", impl_->application->mainWindowId());
				execution->record_limit = integer(arguments, "max_frames", 256);
				execution->compiled.plan.deadline_ns =
					integer(arguments, "deadline_ns", 30'000'000'000ull);
				execution->compiled.plan.limits = impl_->capabilities.limits;
				if (!execution->record_limit ||
					execution->record_limit > execution->compiled.plan.limits.maximum_frames ||
					!execution->compiled.plan.deadline_ns ||
					execution->compiled.plan.deadline_ns >
						execution->compiled.plan.limits.maximum_duration_ns)
					return impl_->fail(request, agentic_error(AgenticError::invalid_request));
				execution->compiled.plan.persistence_declared = true;
				execution->include_text =
					arguments.find("include_text") && arguments.find("include_text")->bool_or();
				execution->compiled.plan.persistence =
					execution->include_text ? AgenticInteractionPersistence::retain_text
											: AgenticInteractionPersistence::redact_text;
				execution->compiled.digest = digest;
				execution->compiled.plan.spans.reserve(
					static_cast<std::size_t>(execution->record_limit));
			} else {
				if (!impl_->configuration.action_control_enabled)
					return impl_->fail(request, agentic_error(AgenticError::unavailable));
				AgenticActionInvocation invocation;
				invocation.action_id = string(arguments, "action_id");
				invocation.revision = integer(arguments, "revision");
				invocation.invocation_sequence = ++impl_->action_sequence;
				invocation.request_id = request.request_id;
				invocation.window_id =
					integer(arguments, "window_id", impl_->application->mainWindowId());
				if (const auto* values = arguments.find("arguments"))
					invocation.arguments = *values;
				execution->compiled.plan.deadline_ns =
					integer(arguments, "deadline_ns", 30'000'000'000ull);
				if (!execution->compiled.plan.deadline_ns ||
					execution->compiled.plan.deadline_ns >
						impl_->capabilities.limits.maximum_duration_ns)
					return impl_->fail(request, agentic_error(AgenticError::invalid_request));
				invocation.deadline_monotonic_ns =
					monotonic_nanoseconds() + execution->compiled.plan.deadline_ns;
				execution->compiled.plan.window_id = invocation.window_id;
				execution->compiled.digest = digest;
				execution->compiled.plan.limits = impl_->capabilities.limits;
				execution->action_invocation = std::move(invocation);
			}
			auto admitted = impl_->admit(std::move(execution), arguments.find("capture"));
			if (!admitted)
				return impl_->fail(request, admitted.error());
			return bounded_success(request, impl_->accepted(*impl_->find(*admitted)), "accepted");
		}
		auto* execution = impl_->find(string(arguments, "job_id"));
		if (!execution)
			return impl_->fail(request, agentic_error(AgenticError::unavailable));
		if (operation == "job.cancel")
			execution->cancel = true;
		if (operation == "interaction.record.stop") {
			if (!execution->recording)
				return impl_->fail(request, agentic_error(AgenticError::invalid_request));
			execution->stop = true;
		}
		if (operation == "interaction.query") {
			const auto kind = string(arguments, "kind");
			if (!execution->payload_evicted) {
				const auto* cursor = arguments.find("cursor");
				if (cursor && !cursor->string_or().empty() && !parse_decimal(*cursor))
					return impl_->fail(request, agentic_error(AgenticError::invalid_request));
			}
			if (execution->payload_evicted &&
				(kind == "plan" || kind == "inputs" || kind == "observations")) {
				if (impl_->active())
					return impl_->fail(request, agentic_error(AgenticError::busy));
				const auto parent = impl_->service->query_job(execution->capture_job_id);
				if (!parent || parent->manifest_path.empty())
					return impl_->fail(request, agentic_error(AgenticError::unavailable));
				const auto manifest = read_manifest(parent->manifest_path);
				if (!manifest)
					return impl_->fail(request, manifest.error());
				const auto artifact = std::string(kind == "plan"	 ? "interaction_plan"
												  : kind == "inputs" ? "interaction_inputs"
																	 : "interaction_observations") +
									  execution->artifact_suffix;
				if (kind == "plan") {
					auto data = read_artifact_json(*manifest, artifact);
					return data ? bounded_success(request, std::move(*data))
								: impl_->fail(request, data.error());
				}
				auto filter = Json::make_object();
				for (const auto key :
					 {"window_id", "app_tick", "window_frame", "sequence", "offset", "plan_digest"})
					if (const auto* value = arguments.find(key))
						filter.set(key, *value);
				auto page = query_artifact(*manifest, artifact, filter,
										   static_cast<std::uint32_t>(std::clamp<std::uint64_t>(
											   integer(arguments, "limit", 100), 1, 1000)),
										   integer(arguments, "max_bytes", 65536) / 2,
										   string(arguments, "cursor"));
				return page ? bounded_success(
								  request, Json::make_object({{"records", std::move(page->records)},
															  {"cursor", page->cursor}}))
							: impl_->fail(request, page.error());
			}
			if (kind == "plan")
				return bounded_success(request,
									   to_json(execution->compiled.plan,
											   execution->compiled.plan.persistence ==
												   AgenticInteractionPersistence::retain_text));
			if (kind == "inputs" || kind == "observations") {
				const auto limit =
					std::clamp<std::uint64_t>(integer(arguments, "limit", 100), 1, 1000);
				const auto cursor = integer(arguments, "cursor");
				const auto count =
					kind == "inputs" ? execution->inputs.size() : execution->observations.size();
				if (cursor > count)
					return impl_->fail(request, agentic_error(AgenticError::invalid_request));
				auto records = Json::make_array();
				records.array.reserve(
					static_cast<std::size_t>(std::min<std::uint64_t>(limit, count - cursor)));
				auto end = cursor;
				std::uint64_t bytes = 1024;
				const auto maximum = integer(arguments, "max_bytes", 65536);
				while (end < count && records.array.size() < limit) {
					auto record = kind == "inputs"
									  ? to_json(execution->inputs[end],
												execution->compiled.plan.persistence ==
													AgenticInteractionPersistence::retain_text)
									  : to_json(execution->observations[end]);
					if (!matches(record, arguments)) {
						++end;
						continue;
					}
					const auto size = write_json(record).size();
					if (size > maximum - std::min(maximum, bytes))
						break;
					bytes += size;
					records.array.emplace_back(std::move(record));
					++end;
				}
				if (end == cursor && cursor < count && records.array.empty())
					return impl_->fail(request, agentic_error(AgenticError::budget_exhausted));
				return bounded_success(
					request, Json::make_object({{"records", std::move(records)},
												{"cursor", end < count ? decimal(end) : Json{}},
												{"job_id", execution->result.job_id},
												{"plan_digest", execution->compiled.digest}}));
			}
			return bounded_success(request, impl_->result(*execution));
		}
		return bounded_success(request, impl_->status(*execution));
	} catch (...) {
		impl_->capture_admission = false;
		return unexpectedError(agentic_error(AgenticError::budget_exhausted));
	}
}
Status AgenticInteractionRuntime::select_input(const EvidenceFrameKey& supplied, FrameInput& input,
											   float ui_scale) noexcept {
	try {
		if (std::this_thread::get_id() != impl_->platform_thread || !std::isfinite(ui_scale) ||
			ui_scale <= 0)
			return unexpectedError(agentic_error(AgenticError::invalid_request));
		auto scale = std::ranges::find_if(
			impl_->scales, [&](const auto& entry) { return entry.first == supplied.window_id; });
		if (scale == impl_->scales.end()) {
			if (impl_->scales.size() < 64)
				impl_->scales.emplace_back(supplied.window_id, ui_scale);
		} else
			scale->second = ui_scale;
		// Assessment and emergency cancellation remain physical facts, before handoff filtering.
		const bool emergency = input.keyDown[256] && (input.keyDown[341] || input.keyDown[345]) &&
							   (input.keyDown[342] || input.keyDown[346]);
		const auto apply_native_handoff = [&]() noexcept {
			for (auto& handoff : impl_->handoffs)
				if (handoff.window_id == supplied.window_id) {
					for (std::size_t key = 0; key < input.keyDown.size(); ++key) {
						handoff.keys[key] = handoff.keys[key] && input.keyDown[key];
						if (handoff.keys[key])
							input.keyDown[key] = false;
					}
					for (std::size_t button = 0; button < input.mouseDown.size(); ++button) {
						handoff.buttons[button] =
							handoff.buttons[button] && input.mouseDown[button];
						if (handoff.buttons[button])
							input.mouseDown[button] = false;
					}
					input.shift = input.keyDown[340] || input.keyDown[344];
					input.ctrl = input.keyDown[341] || input.keyDown[345];
					input.alt = input.keyDown[342] || input.keyDown[346];
					input.super = input.keyDown[343] || input.keyDown[347];
				}
		};
		auto* execution = impl_->active();
		if (!execution || execution->action_invocation ||
			execution->result.window_id != supplied.window_id) {
			apply_native_handoff();
			return {};
		}
		auto parent = impl_->service->query_job(execution->capture_job_id);
		if (!parent || (parent->phase != JobPhase::recording &&
						!(!execution->owns_capture && (parent->phase == JobPhase::preparing ||
													   parent->phase == JobPhase::warming_up ||
													   parent->phase == JobPhase::finalizing)) &&
						execution->result.phase != AgenticInteractionPhase::releasing_control)) {
			apply_native_handoff();
			return {};
		}
		EvidenceFrameKey key = supplied;
		key.run_id = execution->result.run_id;
		const auto now = monotonic_nanoseconds();
		execution->ui_scale = ui_scale;
		if (emergency)
			execution->cancel = true;
		if (execution->result.phase == AgenticInteractionPhase::queued) {
			if (execution->recording)
				execution->result.activation_frame = key;
			execution->result.phase =
				execution->recording || execution->compiled.plan.baseline ==
											AgenticInteractionBaseline::require_neutral
					? AgenticInteractionPhase::executing
					: AgenticInteractionPhase::synchronizing;
			if (!execution->recording &&
				execution->compiled.plan.baseline == AgenticInteractionBaseline::require_neutral &&
				!neutral(impl_->application->ui(static_cast<WindowId>(key.window_id))
							 .getCurrentFrameInput()))
				impl_->request_release(*execution, "required_neutral_baseline_mismatch",
									   AgenticInteractionPhase::failed);
		}
		if (execution->cancel)
			impl_->request_release(*execution, "canceled", AgenticInteractionPhase::canceled);
		if (now >= execution->started_ns &&
			now - execution->started_ns >= execution->compiled.plan.deadline_ns &&
			execution->result.phase != AgenticInteractionPhase::releasing_control)
			impl_->request_release(*execution, "interaction_deadline_exceeded",
								   AgenticInteractionPhase::failed);
		impl_->bound_evidence(*execution);
		AgenticInteractionReceipt receipt;
		receipt.sequence = execution->inputs.size();
		receipt.offset = execution->offset;
		receipt.frame = key;
		receipt.job_id = execution->result.job_id;
		receipt.run_id = execution->result.run_id;
		receipt.plan_digest = execution->compiled.digest;
		receipt.selected_ns = now;
		receipt.selected = true;
		receipt.synthetic_focus = !execution->recording;
		if (execution->compiled.plan.include_native_input) {
			auto native = snapshot_interaction_input(input);
			if (!native)
				return unexpectedError(native.error());
			receipt.native_input = std::move(*native);
		}
		// A recorder observes effective native input while preserving the optional raw assessment.
		if (execution->recording)
			apply_native_handoff();
		auto selected = snapshot_interaction_input(input);
		if (!selected)
			return unexpectedError(selected.error());
		if (execution->recording) {
			execution->recorded_text += selected->text.size();
			if (execution->recorded_text >
				execution->compiled.plan.limits.maximum_text_codepoints) {
				impl_->request_release(*execution, "recording_text_budget_exhausted",
									   AgenticInteractionPhase::failed);
				return {};
			}
			AgenticInteractionSpan span;
			span.offset = execution->offset;
			span.input = *selected;
			execution->compiled.plan.spans.emplace_back(std::move(span));
			receipt.effective_input = *selected;
			execution->previous = *selected;
			++execution->offset;
			++execution->actual_frames;
			++execution->result.coverage.consumed;
			execution->result.coverage.scheduled = execution->offset;
			if (execution->offset >= execution->record_limit)
				execution->stop = true;
		} else {
			bool planned = false;
			if (execution->result.phase == AgenticInteractionPhase::synchronizing) {
				*selected = AgenticInputSnapshot{};
				selected->pointer_x = input.mouseX;
				selected->pointer_y = input.mouseY;
				selected->pointer_inside = input.pointerInside;
				selected->window_focused = input.windowFocused;
				receipt.baseline = true;
			} else if (execution->result.phase == AgenticInteractionPhase::releasing_control) {
				*selected = AgenticInputSnapshot{};
				selected->pointer_x = execution->previous.pointer_x;
				selected->pointer_y = execution->previous.pointer_y;
				selected->pointer_inside = execution->previous.pointer_inside;
				selected->window_focused = execution->previous.window_focused;
				receipt.cleanup = true;
				++execution->cleanup_frames;
				impl_->handoff(*execution, input);
			} else if (execution->gate ||
					   execution->result.phase == AgenticInteractionPhase::draining_observations) {
				*selected = execution->previous;
				selected->text.clear();
				selected->scroll_x = selected->scroll_y = 0;
				receipt.reason = "awaiting_observation_barrier";
				receipt.step_index = static_cast<std::uint32_t>(execution->gate_step);
				receipt.offset = execution->offset ? execution->offset - 1 : 0;
				if (execution->result.phase == AgenticInteractionPhase::draining_observations)
					execution->drain_selected = true;
			} else {
				if (execution->compiled.plan.clock ==
					AgenticInteractionClock::exact_window_frames) {
					const auto expected =
						execution->compiled.plan.first_window_frame + execution->offset;
					if (key.window_frame < expected) {
						*selected = AgenticInputSnapshot{};
						selected->pointer_x = input.mouseX;
						selected->pointer_y = input.mouseY;
						receipt.baseline = true;
						receipt.reason = "waiting_for_exact_address";
					} else if (key.window_frame > expected) {
						++execution->result.coverage.missed;
						execution->result.first_divergence_offset = execution->offset;
						impl_->request_release(*execution, "missed_exact_input_frame",
											   AgenticInteractionPhase::failed);
						*selected = AgenticInputSnapshot{};
						receipt.missed = true;
					} else
						planned = true;
				} else
					planned = true;
				if (planned) {
					auto snapshot = interaction_input_at(execution->compiled, execution->offset);
					if (!snapshot)
						return unexpectedError(snapshot.error());
					*selected = std::move(*snapshot);
					const auto span =
						std::ranges::find_if(execution->compiled.spans, [&](const auto& candidate) {
							return execution->offset >= candidate.offset &&
								   execution->offset - candidate.offset < candidate.count;
						});
					if (span == execution->compiled.spans.end())
						return unexpectedError(agentic_error(AgenticError::incomplete));
					receipt.step_index = span->step_index;
					if (span->preserve_pointer) {
						selected->pointer_x = execution->previous.pointer_x;
						selected->pointer_y = execution->previous.pointer_y;
						selected->pointer_inside = execution->previous.pointer_inside;
					}
					if (execution->compiled.plan.mode == AgenticInteractionMode::common_controls) {
						auto& step = execution->compiled.plan.steps.at(span->step_index);
						if (execution->step_started[span->step_index] == 0) {
							execution->step_started[span->step_index] = now;
							execution->result.steps[span->step_index].first_offset =
								execution->offset;
						}
						auto& target = execution->resolved[span->step_index];
						const auto resolve_pending =
							[&](const AgenticInteractionTarget& selector,
								std::optional<AgenticResolvedInteractionTarget>& cached) {
								if (cached || !planned)
									return;
								auto resolved = impl_->target_registry.resolve(
									*impl_->application, static_cast<WindowId>(key.window_id),
									selector, ui_scale);
								if (resolved) {
									cached = std::move(*resolved);
									return;
								}
								planned = false;
								const bool pending =
									resolved.error().code == ErrorCode::AgenticUnavailable ||
									resolved.error().code == ErrorCode::AgenticIncompleteCapture;
								if (pending && now - execution->step_started[span->step_index] <
												   step.deadline_ns) {
									*selected = execution->previous;
									selected->text.clear();
									selected->scroll_x = selected->scroll_y = 0;
									receipt.reason = "waiting_for_target";
								} else {
									impl_->request_release(*execution,
														   "target_resolution_failed_or_timed_out",
														   AgenticInteractionPhase::failed);
									*selected = AgenticInputSnapshot{};
								}
							};
						if (step.target)
							resolve_pending(*step.target, target);
						if (step.destination)
							resolve_pending(*step.destination,
											execution->destinations[span->step_index]);
						if (planned && target && span->target_pointer) {
							selected->pointer_x = target->window_point.x;
							selected->pointer_y = target->window_point.y;
							selected->pointer_inside = true;
						}
						if (planned && step.destination && span->destination_pointer) {
							auto& destination = execution->destinations[span->step_index];
							if (!destination) {
								auto resolved = impl_->target_registry.resolve(
									*impl_->application, static_cast<WindowId>(key.window_id),
									*step.destination, ui_scale);
								if (!resolved)
									return unexpectedError(resolved.error());
								destination = std::move(*resolved);
							}
							const auto start =
								target ? target->window_point
									   : AgenticInteractionPoint{execution->previous.pointer_x,
																 execution->previous.pointer_y};
							selected->pointer_x =
								start.x +
								(destination->window_point.x - start.x) * span->pointer_progress;
							selected->pointer_y =
								start.y +
								(destination->window_point.y - start.y) * span->pointer_progress;
						}
						if (planned && target &&
							step.operation == AgenticInteractionOperation::drag &&
							!step.path.empty() && span->pointer_progress > 0) {
							const double progress =
								span->pointer_progress * static_cast<double>(step.path.size());
							const auto endpoint = std::min<std::size_t>(
								static_cast<std::size_t>(progress), step.path.size() - 1);
							const auto previous =
								endpoint ? step.path[endpoint - 1] : target->window_point;
							const double fraction = span->pointer_progress == 1.0
														? 1.0
														: progress - static_cast<double>(endpoint);
							selected->pointer_x =
								previous.x + (step.path[endpoint].x - previous.x) * fraction;
							selected->pointer_y =
								previous.y + (step.path[endpoint].y - previous.y) * fraction;
						}
						if (planned && (!span->preserve_pointer || span->target_pointer ||
										span->destination_pointer)) {
							const auto extent = impl_->application->windowSize(
								static_cast<WindowId>(key.window_id));
							selected->pointer_inside = selected->pointer_x >= 0 &&
													   selected->pointer_y >= 0 &&
													   selected->pointer_x < extent.first &&
													   selected->pointer_y < extent.second;
						}
						if (planned && span->require_hover_before && target &&
							target->selector.kind != AgenticTargetKind::coordinates) {
							AgenticInteractionExpectation observed =
								AgenticInteractionExpectation::none;
							if (!execution->observations.empty()) {
								const auto& last = execution->observations.back();
								if (last.layout_completed && !last.canceled) {
									observed = observed_interaction_facts(last, target->clay_id);
									const auto* prior_target =
										last.step_index < execution->resolved.size() &&
												execution->resolved[last.step_index]
											? &*execution->resolved[last.step_index]
											: nullptr;
									if (prior_target &&
										prior_target->selector.kind ==
											AgenticTargetKind::custom_region &&
										prior_target->selector.custom_id ==
											target->selector.custom_id &&
										prior_target->custom_revision == target->custom_revision &&
										last.target_observation &&
										last.target_observation->find("target_hovered") &&
										last.target_observation->find("target_hovered")->bool_or())
										observed = observed |
												   AgenticInteractionExpectation::target_hovered;
								}
							}
							auto current = impl_->target_registry.resolve(
								*impl_->application, static_cast<WindowId>(key.window_id),
								target->selector, ui_scale);
							if (!has_expectation(observed,
												 AgenticInteractionExpectation::target_hovered) ||
								!current || current->clay_id != target->clay_id ||
								current->custom_revision != target->custom_revision ||
								current->flow_instance_id != target->flow_instance_id ||
								std::abs(current->window_point.x - target->window_point.x) > 0.5 ||
								std::abs(current->window_point.y - target->window_point.y) > 0.5) {
								impl_->request_release(
									*execution, "target_changed_or_hover_unobserved_before_down",
									AgenticInteractionPhase::failed);
								planned = false;
								*selected = AgenticInputSnapshot{};
							}
						}
					}
					if (planned) {
						if (execution->offset == 0)
							execution->result.activation_frame = key;
						++execution->result.coverage.consumed;
						++execution->offset;
						if (execution->compiled.plan.mode ==
							AgenticInteractionMode::common_controls) {
							const auto next = std::ranges::find_if(
								execution->compiled.spans, [&](const auto& candidate) {
									return execution->offset >= candidate.offset &&
										   execution->offset - candidate.offset < candidate.count;
								});
							if (next == execution->compiled.spans.end() ||
								next->step_index != span->step_index) {
								execution->gate = true;
								execution->gate_step = span->step_index;
								execution->result.steps[span->step_index].end_offset_exclusive =
									execution->offset;
							}
						} else if (execution->offset >= execution->compiled.frame_count) {
							execution->result.phase =
								AgenticInteractionPhase::draining_observations;
						}
					}
				}
			}
			receipt.effective_input = *selected;
			execution->previous = *selected;
			auto materialized = materialize_interaction_input(*selected, input);
			if (!materialized)
				return unexpectedError(materialized.error());
			input = std::move(*materialized);
			++execution->actual_frames;
		}
		const auto receipt_bytes = sizeof(receipt) + input_bytes(receipt.effective_input) +
								   (receipt.native_input ? input_bytes(*receipt.native_input) : 0);
		if (execution->inputs.size() < execution->compiled.plan.limits.maximum_observations + 4 &&
			impl_->charge(*execution, receipt_bytes, receipt.cleanup))
			execution->inputs.emplace_back(std::move(receipt));
		else {
			execution->result.evidence_complete = false;
			++execution->result.coverage.lost;
		}
		execution->result.coverage.retained_inputs = execution->inputs.size();
		return {};
	} catch (...) {
		if (auto* execution = impl_->active()) {
			impl_->request_release(*execution, "input_selection_failed",
								   AgenticInteractionPhase::failed);
			input.keyDown.fill(false);
			input.mouseDown.fill(false);
			input.text.clear();
			input.scrollX = input.scrollY = 0;
			input.shift = input.ctrl = input.alt = input.super = false;
		}
		return unexpectedError(agentic_error(AgenticError::budget_exhausted));
	}
}
void AgenticInteractionRuntime::observe_ui(const EvidenceFrameKey& supplied) noexcept {
	try {
		auto* execution = impl_->active();
		if (!execution || execution->result.window_id != supplied.window_id ||
			execution->inputs.empty())
			return;
		const auto& receipt = execution->inputs.back();
		if (!receipt.frame || !same_frame(*receipt.frame, supplied))
			return;
		auto& ui = impl_->application->ui(static_cast<WindowId>(supplied.window_id));
		const auto& interaction = ui.dev_completed_interaction();
		const auto maximum_ids = execution->compiled.plan.limits.maximum_target_candidates;
		if (interaction.hoveredElementIds.size() > maximum_ids ||
			interaction.pressedElementIds.size() > maximum_ids ||
			interaction.heldElementIds.size() > maximum_ids ||
			interaction.releasedElementIds.size() > maximum_ids) {
			execution->result.evidence_complete = false;
			++execution->result.coverage.lost;
			impl_->request_release(*execution, "interaction_membership_budget_exhausted",
								   AgenticInteractionPhase::failed);
			return;
		}
		AgenticInteractionObservation observation;
		observation.frame = *receipt.frame;
		observation.job_id = execution->result.job_id;
		observation.run_id = execution->result.run_id;
		observation.plan_digest = execution->compiled.digest;
		observation.sequence = execution->observations.size();
		observation.offset = receipt.offset;
		observation.step_index = receipt.step_index;
		observation.observed_ns = monotonic_nanoseconds();
		observation.layout_completed = true;
		if (execution->compiled.plan.record_observations) {
			observation.hovered.assign(interaction.hoveredElementIds.begin(),
									   interaction.hoveredElementIds.end());
			observation.pressed.assign(interaction.pressedElementIds.begin(),
									   interaction.pressedElementIds.end());
			observation.held.assign(interaction.heldElementIds.begin(),
									interaction.heldElementIds.end());
			observation.released.assign(interaction.releasedElementIds.begin(),
										interaction.releasedElementIds.end());
		}
		observation.window_focused = ui.getCurrentFrameInput().windowFocused;
		observation.text_focus_supported = true;
		observation.text_focused = ui.inputFields().hasPrimaryFieldFocus();
		observation.input_suppressed =
			ui.getCurrentFrameInput().mouseDown[0] !=
			(!receipt.effective_input.buttons_down.empty() &&
			 std::ranges::find(receipt.effective_input.buttons_down, 0) !=
				 receipt.effective_input.buttons_down.end());
		if (!receipt.baseline && !receipt.cleanup &&
			receipt.step_index < execution->resolved.size()) {
			observation.target = execution->resolved[receipt.step_index];
			auto facts = observed_interaction_facts(
				observation, observation.target ? observation.target->clay_id : 0);
			if (observation.target &&
				observation.target->selector.kind == AgenticTargetKind::custom_region) {
				auto custom = impl_->target_registry.observe(*observation.target, interaction);
				if (!custom) {
					impl_->request_release(*execution, "custom_observation_failed",
										   AgenticInteractionPhase::failed);
				} else
					for (const auto& [name, fact] :
						 std::array<std::pair<std::string_view, AgenticInteractionExpectation>, 4>{
							 {{"target_hovered", AgenticInteractionExpectation::target_hovered},
							  {"target_pressed", AgenticInteractionExpectation::target_pressed},
							  {"target_held", AgenticInteractionExpectation::target_held},
							  {"target_released", AgenticInteractionExpectation::target_released}}})
						if (custom->find(name) && custom->find(name)->bool_or())
							facts = facts | fact;
				if (custom) {
					if (!execution->content.include_interaction_outcomes)
						custom->object.erase(
							std::remove_if(custom->object.begin(), custom->object.end(),
										   [](const auto& member) {
											   return member.first == "application_outcome";
										   }),
							custom->object.end());
					observation.target_observation = std::move(*custom);
				}
			}
			execution->result.steps[receipt.step_index].observed =
				execution->result.steps[receipt.step_index].observed | facts;
		}
		if (!execution->content.include_interaction_labels && observation.target) {
			observation.target->label.clear();
			auto& selector = observation.target->selector;
			selector.kind = AgenticTargetKind::coordinates;
			selector.point = observation.target->window_point;
			selector.clay_name.clear();
			selector.flow_debug_name.clear();
			selector.custom_id.clear();
			if (observation.target_observation)
				observation.target_observation->set("target", to_json(*observation.target));
		}
		if (receipt.baseline && receipt.reason != "waiting_for_exact_address")
			execution->baseline_observed = true;
		if (receipt.cleanup && execution->cleanup_frames >= 2)
			execution->cleanup_observed = true;
		if (execution->result.phase == AgenticInteractionPhase::draining_observations &&
			execution->drain_selected)
			execution->drain_observed = true;
		if (execution->compiled.plan.record_observations) {
			auto observation_bytes =
				sizeof(observation) +
				(observation.hovered.capacity() + observation.pressed.capacity() +
				 observation.held.capacity() + observation.released.capacity()) *
					sizeof(std::uint64_t);
			if (observation.target)
				observation_bytes +=
					sizeof(AgenticResolvedInteractionTarget) + observation.target->label.size() +
					observation.target->selector.clay_name.size() +
					observation.target->selector.custom_id.size() +
					observation.target->selector.flow_debug_name.size() +
					observation.target->matching_clay_ids.capacity() * sizeof(std::uint64_t);
			if (observation.target_observation)
				observation_bytes += write_json(*observation.target_observation).size();
			if (execution->observations.size() <
					execution->compiled.plan.limits.maximum_observations + 4 &&
				impl_->charge(*execution, observation_bytes, receipt.cleanup))
				execution->observations.emplace_back(std::move(observation));
			else {
				execution->result.evidence_complete = false;
				++execution->result.coverage.lost;
			}
		}
		++execution->result.coverage.observed;
		execution->result.coverage.retained_observations = execution->observations.size();
	} catch (...) {
		if (auto* execution = impl_->active()) {
			execution->result.evidence_complete = false;
			impl_->request_release(*execution, "observation_budget_exhausted",
								   AgenticInteractionPhase::failed);
		}
	}
}
void AgenticInteractionRuntime::observe_frame(const AgenticFrameRecord& frame) noexcept {
	auto* execution = impl_->active();
	if (!execution || execution->result.window_id != frame.key.window_id)
		return;
	auto found = std::ranges::find_if(execution->observations.rbegin(),
									  execution->observations.rend(), [&](const auto& observation) {
										  return observation.frame &&
												 same_frame(*observation.frame, frame.key);
									  });
	if (found != execution->observations.rend()) {
		found->presented = frame.presented;
		found->canceled = frame.canceled;
		if (frame.canceled)
			++execution->result.coverage.canceled;
		if (found->step_index < execution->result.steps.size() && frame.presented)
			execution->result.steps[found->step_index].observed =
				execution->result.steps[found->step_index].observed |
				AgenticInteractionExpectation::frame_presented;
	} else if (frame.canceled && std::ranges::any_of(execution->inputs, [&](const auto& receipt) {
				   return receipt.frame && same_frame(*receipt.frame, frame.key);
			   })) {
		++execution->result.coverage.canceled;
		execution->result.evidence_complete = false;
		impl_->request_release(*execution, "selected_input_frame_canceled_before_layout",
							   AgenticInteractionPhase::failed);
	} else if (frame.canceled &&
			   execution->compiled.plan.clock == AgenticInteractionClock::exact_window_frames &&
			   execution->compiled.plan.first_window_frame + execution->offset ==
				   frame.key.window_frame) {
		++execution->result.coverage.missed;
		execution->result.first_divergence_offset = execution->offset;
		impl_->request_release(*execution, "exact_frame_canceled_before_selection",
							   AgenticInteractionPhase::failed);
	}
}
void AgenticInteractionRuntime::advance(std::uint64_t app_tick, std::uint64_t now) noexcept {
	try {
		if (app_tick <= impl_->last_safe_tick)
			return;
		impl_->last_safe_tick = app_tick;
		auto* execution = impl_->active();
		if (!execution)
			return;
		const auto parent = impl_->service->query_job(execution->capture_job_id);
		if (!parent || parent->phase == JobPhase::failed || parent->phase == JobPhase::canceled) {
			const bool input_started = execution->result.activation_frame.has_value();
			impl_->request_release(*execution, "owning_capture_unavailable",
								   AgenticInteractionPhase::failed);
			if (!parent || !input_started) {
				impl_->complete(*execution, false);
				return;
			}
		}
		if (!impl_->application->hasWindow(static_cast<WindowId>(execution->result.window_id))) {
			impl_->request_release(*execution, "target_window_closed",
								   AgenticInteractionPhase::failed);
			impl_->complete(*execution, false);
			return;
		}
		if (execution->cancel)
			impl_->request_release(*execution, "canceled", AgenticInteractionPhase::canceled);
		if (now >= execution->started_ns &&
			now - execution->started_ns >= execution->compiled.plan.deadline_ns &&
			execution->result.phase != AgenticInteractionPhase::releasing_control)
			impl_->request_release(*execution, "interaction_deadline_exceeded",
								   AgenticInteractionPhase::failed);
		if (execution->result.phase == AgenticInteractionPhase::releasing_control) {
			if (execution->recording) {
				execution->cleanup_observed = true;
				execution->result.handoff_complete = true;
			}
			if (execution->cleanup_observed || execution->recording ||
				execution->action_invocation) {
				impl_->complete(*execution);
				return;
			}
			if (now >= execution->release_started_ns &&
				now - execution->release_started_ns >= 2'000'000'000ull)
				impl_->complete(*execution, false);
			return;
		}
		if (parent->phase != JobPhase::recording &&
			!(!execution->owns_capture &&
			  (parent->phase == JobPhase::preparing || parent->phase == JobPhase::warming_up ||
			   parent->phase == JobPhase::finalizing)))
			return;
		if (execution->action_invocation) {
			if (!execution->action_receipt) {
				execution->action_invocation->plan_id = execution->result.job_id;
				auto receipt = impl_->action_registry.invoke(*impl_->application,
															 *execution->action_invocation);
				if (!receipt) {
					impl_->request_release(*execution, errorName(receipt.error().code),
										   AgenticInteractionPhase::failed);
					return;
				}
				execution->action_receipt = std::move(*receipt);
				execution->result.phase = AgenticInteractionPhase::executing;
			} else if (!execution->action_receipt->completed) {
				auto receipt =
					impl_->action_registry.poll(*impl_->application, *execution->action_receipt);
				if (!receipt) {
					impl_->request_release(*execution, errorName(receipt.error().code),
										   AgenticInteractionPhase::failed);
					return;
				}
				execution->action_receipt = std::move(*receipt);
			}
			if (execution->action_receipt->completed) {
				execution->result.assertion_verdict =
					execution->action_receipt->completion_succeeded
						? AgenticInteractionVerdict::passed
						: AgenticInteractionVerdict::failed;
				impl_->request_release(*execution, execution->action_receipt->reason,
									   execution->action_receipt->completion_succeeded
										   ? AgenticInteractionPhase::completed
										   : AgenticInteractionPhase::failed);
			}
			return;
		}
		if (execution->recording) {
			if (execution->stop) {
				execution->compiled.frame_count = execution->offset;
				auto digest = interaction_plan_digest(execution->compiled.plan);
				if (!digest) {
					impl_->request_release(*execution, "recorded_plan_digest_unavailable",
										   AgenticInteractionPhase::failed);
					return;
				}
				execution->compiled.digest = std::move(*digest);
				execution->result.plan_digest = execution->compiled.digest;
				for (auto& receipt : execution->inputs)
					receipt.plan_digest = execution->compiled.digest;
				for (auto& observation : execution->observations)
					observation.plan_digest = execution->compiled.digest;
				impl_->request_release(*execution, "recording_stopped",
									   AgenticInteractionPhase::completed);
				execution->cleanup_observed = true;
				execution->result.handoff_complete = true;
				impl_->complete(*execution);
			}
			return;
		}
		if (execution->result.phase == AgenticInteractionPhase::synchronizing &&
			execution->baseline_observed)
			execution->result.phase = AgenticInteractionPhase::executing;
		if (execution->result.phase == AgenticInteractionPhase::draining_observations) {
			if (execution->drain_observed &&
				execution->result.coverage.observed >= execution->inputs.size())
				impl_->request_release(*execution, "completed", AgenticInteractionPhase::completed);
			return;
		}
		if (execution->gate) {
			const auto index = execution->gate_step;
			const auto& step = execution->compiled.plan.steps[index];
			auto& result = execution->result.steps[index];
			const auto elapsed = now - execution->step_started[index];
			if (elapsed < step.deadline_ns &&
				(bits(result.observed) & bits(result.expected)) == bits(result.expected) &&
				elapsed >= step.minimum_duration_ns) {
				result.verdict = AgenticInteractionVerdict::passed;
				++execution->result.coverage.asserted;
				execution->gate = false;
				if (execution->offset >= execution->compiled.frame_count)
					execution->result.phase = AgenticInteractionPhase::draining_observations;
			} else if (elapsed >= step.deadline_ns) {
				result.verdict = AgenticInteractionVerdict::failed;
				result.reason = "step_observation_or_duration_deadline";
				execution->result.assertion_verdict = AgenticInteractionVerdict::failed;
				execution->result.first_divergence_offset = result.first_offset;
				impl_->request_release(*execution, result.reason, AgenticInteractionPhase::failed);
			}
		}
		impl_->bound_evidence(*execution);
	} catch (...) {
		if (auto* execution = impl_->active()) {
			impl_->request_release(*execution, "interaction_advance_failed",
								   AgenticInteractionPhase::failed);
		}
	}
}
bool AgenticInteractionRuntime::ready_to_materialize(std::string_view capture_job_id) noexcept {
	try {
		auto* execution = impl_->active();
		if (!execution || execution->capture_job_id != capture_job_id)
			return true;
		if (!execution->owns_capture && execution->owner_phase == JobPhase::finalizing)
			return false;
		if (execution->result.phase != AgenticInteractionPhase::releasing_control)
			impl_->request_release(*execution, "capture_cutoff_before_interaction_completion",
								   AgenticInteractionPhase::failed);
		return false;
	} catch (...) {
		return false;
	}
}
Status AgenticInteractionRuntime::materialize(AgenticCaptureSnapshot& snapshot) noexcept {
	try {
		std::size_t ordinal = 0;
		for (auto& owned : impl_->executions) {
			auto& execution = *owned;
			if (execution.capture_job_id != snapshot.job_id || execution.materialized)
				continue;
			if (!terminal(execution.result.phase)) {
				impl_->request_release(execution, "capture_cutoff_before_interaction_completion",
									   AgenticInteractionPhase::failed);
				impl_->complete(execution, false);
			}
			const bool include_text =
				execution.compiled.plan.persistence == AgenticInteractionPersistence::retain_text;
			const auto suffix = ordinal++ ? "_" + std::to_string(ordinal) : std::string{};
			execution.artifact_suffix = suffix;
			const auto directory =
				"interaction/" + (suffix.empty() ? std::string{} : suffix.substr(1) + "/");
			if (execution.compiled.plan.persistence !=
					AgenticInteractionPersistence::volatile_plan &&
				!execution.action_invocation) {
				const auto added = add_text_artifact(
					snapshot, "interaction_plan" + suffix, directory + "plan.json",
					"application/json", "FlowUi.Agentic.InteractionPlan",
					write_json(to_json(execution.compiled.plan, include_text)));
				if (!added)
					return added;
			} else
				snapshot.limitations.emplace_back("interaction_plan_payload_not_persisted");
			std::string inputs, observations;
			const auto maximum = execution.compiled.plan.limits.maximum_artifact_bytes;
			for (const auto& input : execution.inputs) {
				if (execution.compiled.plan.persistence ==
					AgenticInteractionPersistence::volatile_plan)
					break;
				auto line = write_json(to_json(input, include_text));
				if (line.size() + 1 >
					maximum - std::min<std::uint64_t>(inputs.size() + observations.size(), maximum))
					return unexpectedError(agentic_error(AgenticError::budget_exhausted));
				inputs += line;
				inputs += '\n';
			}
			for (const auto& observation : execution.observations) {
				auto line = write_json(to_json(observation));
				if (line.size() + 1 >
					maximum - std::min<std::uint64_t>(inputs.size() + observations.size(), maximum))
					return unexpectedError(agentic_error(AgenticError::budget_exhausted));
				observations += line;
				observations += '\n';
			}
			const auto added_inputs = add_text_artifact(
				snapshot, "interaction_inputs" + suffix, directory + "inputs.jsonl",
				"application/x-ndjson", "FlowUi.Agentic.InputFrame", std::move(inputs));
			if (!added_inputs)
				return added_inputs;
			const auto added_observations =
				add_text_artifact(snapshot, "interaction_observations" + suffix,
								  directory + "observations.jsonl", "application/x-ndjson",
								  "FlowUi.Agentic.InteractionObservation", std::move(observations));
			if (!added_observations)
				return added_observations;
			if (execution.compiled.plan.persistence == AgenticInteractionPersistence::volatile_plan)
				execution.result.coverage.retained_inputs = 0;
			auto result = impl_->result(execution);
			const auto added_result = add_text_artifact(
				snapshot, "interaction_results" + suffix, directory + "results.json",
				"application/json", "FlowUi.Agentic.InteractionResult", write_json(result));
			if (!added_result)
				return added_result;
			SectionCoverage coverage;
			coverage.status =
				execution.result.evidence_complete && execution.result.execution_succeeded
					? SectionStatus::complete
					: SectionStatus::partial;
			coverage.producer_records = execution.inputs.size();
			coverage.retained_records = execution.inputs.size();
			coverage.dropped_records = execution.result.coverage.lost;
			coverage.uncertain = !execution.result.evidence_complete;
			coverage.reasons = execution.result.limitations;
			if (execution.result.activation_frame)
				coverage.covered_ranges = {
					{execution.result.activation_frame->app_tick, snapshot.end_app_tick_exclusive}};
			snapshot.coverage.emplace_back("interaction" + suffix, std::move(coverage));
			snapshot.partial = snapshot.partial || !execution.result.evidence_complete ||
							   !execution.result.execution_succeeded;
			snapshot.provenance.set("interaction_plan_digest", execution.compiled.digest);
			if (execution.replay_source.kind != Json::Kind::null)
				snapshot.provenance.set("interaction_replay_source", execution.replay_source);
			const auto* prior_instrumentation = snapshot.provenance.find("instrumentation_digest");
			snapshot.provenance.set(
				"instrumentation_digest",
				sha256(write_json(Json::make_object(
					{{"base", prior_instrumentation ? *prior_instrumentation : Json{}},
					 {"interaction", execution.compiled.digest},
					 {"record_observations", execution.compiled.plan.record_observations}}))));
			snapshot.provenance.set("interaction_dt_policy", "native_app_clock");
			snapshot.provenance.set("interaction_input_policy",
									execution.recording ? "record_native_effective_input"
														: "replace_after_native_assessment");
			execution.materialized = true;
		}
		return {};
	} catch (...) {
		return unexpectedError(agentic_error(AgenticError::budget_exhausted));
	}
}
void AgenticInteractionRuntime::shutdown() noexcept {
	impl_->stopping = true;
	try {
		if (auto* execution = impl_->active()) {
			impl_->request_release(*execution, "application_shutdown",
								   AgenticInteractionPhase::canceled);
			impl_->complete(*execution, false);
		}
	} catch (...) {
		impl_->active_index = SIZE_MAX;
	}
}
std::uint64_t AgenticInteractionRuntime::owned_memory_bytes() const noexcept {
	std::uint64_t total = sizeof(Impl);
	total += impl_->idempotency_receipts.capacity() * sizeof(Impl::IdempotencyReceipt);
	for (const auto& receipt : impl_->idempotency_receipts)
		total += receipt.job_id.capacity() + receipt.idempotency_key.capacity() +
				 receipt.request_digest.capacity() + receipt.capture_job_id.capacity() +
				 receipt.capture_id.capacity();
	for (const auto& execution : impl_->executions) {
		total += sizeof(Impl::Execution) + execution->compiled.estimated_bytes +
				 execution->inputs.capacity() * sizeof(AgenticInteractionReceipt) +
				 execution->observations.capacity() * sizeof(AgenticInteractionObservation);
		for (const auto& input : execution->inputs)
			total += input.effective_input.text.capacity() * sizeof(char32_t) +
					 input.effective_input.keys_down.capacity() * sizeof(std::uint16_t) +
					 input.effective_input.buttons_down.capacity();
		for (const auto& observation : execution->observations)
			total += (observation.hovered.capacity() + observation.pressed.capacity() +
					  observation.held.capacity() + observation.released.capacity()) *
					 sizeof(std::uint64_t);
	}
	return total;
}
} // namespace FlowUi::devSystems::agentic
#endif
