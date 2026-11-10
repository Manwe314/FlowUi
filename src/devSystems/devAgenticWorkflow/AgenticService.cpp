#include "devSystems/devAgenticWorkflow/AgenticService.hpp"
#include "devSystems/devAgenticWorkflow/AgenticInteractionRuntime.hpp"
#include "devSystems/devAgenticWorkflow/AgenticVerification.hpp"
#include "devSystems/devAgenticWorkflow/frameCapture/AgenticFrameCapture.hpp"
#if FLOW_UI_AGENTIC_WORKFLOW
#include "FlowUi/Public/App.hpp"
#include "devSystems/devAgenticWorkflow/AgenticAdapters.hpp"
#include "devSystems/devAgenticWorkflow/AgenticLocalTransport.hpp"
#include "devSystems/devAgenticWorkflow/AgenticManifest.hpp"
#include "devSystems/devAgenticWorkflow/AgenticProvenance.hpp"
#include "devSystems/devInterface/Permanents/Backend/DevInterface.hpp"
#include "devSystems/devMonitoringAndReporting/DevMonitoringAndReporting.hpp"
#include "devSystems/devMonitoringAndReporting/errors/DevError.hpp"
#include "devSystems/devMonitoringAndReporting/memory/DevMemory.hpp"
#include "devSystems/devMonitoringAndReporting/reporting/DevPerformanceCapture.hpp"
#include "devSystems/devMonitoringAndReporting/reporting/DevTimingReporting.hpp"
#include "devSystems/devMonitoringAndReporting/timing/DevTiming.hpp"
#include "devSystems/devMonitoringAndReporting/timing/DevTimingZone.hpp"
#include "devSystems/devTooling/DevTooling.hpp"
#include "FlowUi/Public/managers/ThemeManager.hpp"
#include "FlowUi/Public/managers/UiManager.hpp"
#include <algorithm>
#include <atomic>
#include <condition_variable>
#include <cstdlib>
#include <fstream>
#include <future>
#include <limits>
#include <mutex>
#include <thread>
#if defined(_WIN32)
#include <process.h>
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#else
#include <unistd.h>
#endif

namespace FlowUi::devSystems::agentic {
namespace {
[[nodiscard]] bool terminal(JobPhase phase) noexcept {
	return phase == JobPhase::sealed || phase == JobPhase::failed || phase == JobPhase::canceled;
}
[[nodiscard]] std::string argument_string(const Json& arguments, std::string_view key) {
	const auto* value = arguments.find(key);
	return value ? std::string(value->string_or()) : std::string{};
}
[[nodiscard]] std::uint64_t argument_integer(const Json& arguments, std::string_view key) noexcept {
	const auto* value = arguments.find(key);
	return value ? value->uint64_or() : 0;
}
[[nodiscard]] std::uint64_t retained_json_bytes(const Json& value) noexcept {
	std::uint64_t bytes = value.text.capacity() + value.array.capacity() * sizeof(Json) +
						  value.object.capacity() * sizeof(std::pair<std::string, Json>);
	for (const auto& member : value.array)
		bytes += retained_json_bytes(member);
	for (const auto& [key, member] : value.object)
		bytes += key.capacity() + retained_json_bytes(member);
	return bytes;
}
[[nodiscard]] Status atomic_json(const std::filesystem::path& destination,
								 const Json& value) noexcept {
	try {
		auto temporary = destination;
		temporary += ".pending";
		std::ofstream output(temporary, std::ios::binary | std::ios::trunc);
		const auto bytes = write_json(value);
		output.write(bytes.data(), static_cast<std::streamsize>(bytes.size()));
		output.close();
		if (!output)
			return unexpectedError(agentic_error(AgenticError::publication_failed));
#if defined(_WIN32)
		if (!MoveFileExW(temporary.c_str(), destination.c_str(), MOVEFILE_REPLACE_EXISTING))
			return unexpectedError(makeError(ErrorCode::AgenticPublicationFailed,
											 ErrorSite::AgenticPublish, 0, 0, GetLastError()));
#else
		std::filesystem::rename(temporary, destination);
#endif
		return {};
	} catch (...) {
		return unexpectedError(agentic_error(AgenticError::publication_failed));
	}
}
void redact_source_paths(Json& value) {
	for (auto& [key, member] : value.object) {
		if (key == "file" || key == "source_file" || key == "header_source_file" ||
			key == "source_path")
			member = Json{};
		else
			redact_source_paths(member);
	}
	for (auto& member : value.array)
		redact_source_paths(member);
}
[[nodiscard]] std::uint64_t process_id() noexcept {
#if defined(_WIN32)
	return static_cast<std::uint64_t>(_getpid());
#else
	return static_cast<std::uint64_t>(getpid());
#endif
}
[[nodiscard]] std::string process_instance() {
	static const std::string identity = [] {
		auto generated = make_identifier("process");
		if (!generated)
			throw FlowUiException(generated.error());
		return std::move(*generated);
	}();
	return identity;
}
} // namespace

struct AgenticWorkflowService::Impl {
	struct Job {
		AgenticCaptureSnapshot snapshot;
		AgenticJobStatus status;
		AgenticProducerBaseline baseline;
		std::string request_digest, idempotency_key;
		std::uint64_t prepare_tick = 0, finalizing_ns = 0;
		std::uint64_t admitted_images = 0, image_bytes = 0, image_failure_baseline = 0;
		std::uint64_t scenario_start_ns = 0;
		std::size_t scenario_index = SIZE_MAX;
		bool controller_requested = false, scenario_setup = false, scenario_verified = false,
			 provenance_prepared = false;
		bool scenario_cleanup = false, stop_requested = false;
		bool verification_owned = false, checkpoint_active = false;
		bool checkpoint_image = false, checkpoint_tree = false, checkpoint_admitted = false,
			 checkpoint_tree_recorded = false;
		std::uint64_t checkpoint_window = 0;
		DevTimingConfig previous_timing_config{};
		bool previous_value_collection = true;
		std::vector<std::pair<std::uint64_t, bool>> previous_tree_collection;
	};
	struct PendingCommand {
		AgenticCommandRequest request;
		std::promise<Result<Json>> result;
	};
	struct CaptureReceipt {
		AgenticJobStatus status;
		std::string key;
		std::string digest;
		std::uint64_t admitted_ns = 0;
	};
	static constexpr std::size_t maximum_capture_receipts = 4096;
	static constexpr std::uint64_t capture_receipt_retention_ns = 86'400'000'000'000ull;
	struct Verification {
		Json request, cases = Json::make_array(), current, report;
		std::string job_id, digest, key, phase = "preparing", child_job, interaction_job;
		std::string execution_status = "completed";
		std::string report_detail = "failures";
		std::string termination_reason;
		std::uint64_t admitted_ns = 0, case_started_ns = 0, phase_started_ns = 0;
		std::uint64_t sequence = 1, peak_owned_bytes = 0, readback_bytes = 0, disk_bytes = 0;
		std::uint64_t image_count = 0, checkpoint_tick = 0;
		std::uint64_t admission_ns = 0, setup_ns = 0, execution_ns = 0, assertion_ns = 0;
		std::uint64_t publication_ns = 0, queue_ns = 0;
		std::size_t case_index = 0, scenario_index = SIZE_MAX, checkpoint_index = 0;
		std::size_t checkpoint_image_begin = 0, checkpoint_tree_begin = 0;
		std::size_t harvested_actions = 0;
		bool canceled = false, dirty = false, case_failed = false, case_incomplete = false;
		bool terminal = false, publishing = false, report_published = false;
		bool cleanup_started = false;
	};
	struct VerificationReceipt {
		std::string key, digest, job_id;
		std::uint64_t admitted_ns = 0;
	};
	struct VerificationPublication {
		Json report;
		std::filesystem::path path;
		std::string job_id;
	};
	struct Review {
		std::string identity;
		tooling::DevChangesReviewPlan plan;
		std::uint64_t theme_revision = 0;
	};
	struct MutationReceipt {
		std::string operation_id, digest;
		Json result;
	};
	AgenticDependencies dependencies;
	AgenticWorkflowConfig config;
	AgenticCapabilities capabilities;
	std::unique_ptr<AgenticStore> store;
	std::filesystem::path run_path, descriptor_path;
	AgenticLocalEndpoint endpoint;
	AgenticLocalServer server;
	std::vector<std::unique_ptr<Job>> jobs;
	std::vector<CaptureReceipt> capture_receipts;
	std::vector<std::unique_ptr<Verification>> verifications;
	std::vector<VerificationReceipt> verification_receipts;
	std::optional<VerificationPublication> verification_publication;
	std::vector<AgenticScenario> scenarios;
	std::vector<PendingCommand> commands, command_batch;
	std::vector<Review> reviews;
	std::vector<MutationReceipt> receipts;
	std::unique_ptr<AgenticInteractionRuntime> interaction;
	std::optional<AgenticCaptureSnapshot> publication;
	std::string publishing_job;
	std::atomic<std::uint64_t> writer_owned_bytes{0};
	std::jthread writer;
	mutable std::recursive_mutex mutex;
	std::condition_variable_any condition;
	std::thread::id platform_thread = std::this_thread::get_id();
	AgenticSafePoint safe_point;
	std::uint64_t last_session_ns = 0;
	std::size_t active_job = SIZE_MAX;
	bool admission = false, stop_writer = false, configured = false, manual_busy = false;
	bool optional_collection = true;
	bool verification_admission = false, verification_dirty = false;
	AgenticWorkflowService* owner_service = nullptr;

	explicit Impl(AgenticDependencies supplied) : dependencies(supplied) {}
	[[nodiscard]] Job* find_job(std::string_view identity) noexcept {
		for (auto& job : jobs)
			if (job->status.job_id == identity)
				return job.get();
		return nullptr;
	}
	[[nodiscard]] CaptureReceipt* find_capture_receipt(std::string_view identity) noexcept {
		for (auto& receipt : capture_receipts)
			if (receipt.status.job_id == identity)
				return &receipt;
		return nullptr;
	}
	[[nodiscard]] Job* active() noexcept {
		return active_job < jobs.size() ? jobs[active_job].get() : nullptr;
	}
	[[nodiscard]] Verification* active_verification() noexcept {
		for (auto& verification : verifications)
			if (!verification->terminal)
				return verification.get();
		return nullptr;
	}
	[[nodiscard]] Verification* find_verification(std::string_view identity) noexcept {
		for (auto& verification : verifications)
			if (verification->job_id == identity)
				return verification.get();
		return nullptr;
	}
	[[nodiscard]] Json verification_plan(const AgenticCommandRequest& request);
	[[nodiscard]] Json verification_command(const AgenticCommandRequest& request);
	[[nodiscard]] Json verification_response(const AgenticCommandRequest& request,
											 const Verification& verification);
	void advance_verification();
	void seal_verification(Verification& verification);
	void evict_terminal_jobs() {
		if (active())
			return;
		while (jobs.size() >= 32) {
			const auto selected = std::find_if(jobs.begin(), jobs.end(), [&](const auto& job) {
				const auto* verification = active_verification();
				return terminal(job->status.phase) &&
					   (!verification || verification->child_job != job->status.job_id);
			});
			if (selected == jobs.end())
				break;
			if (auto* receipt = find_capture_receipt((*selected)->status.job_id))
				receipt->status = (*selected)->status;
			jobs.erase(selected);
		}
	}
	[[nodiscard]] Json failure(const AgenticCommandRequest& request, FlowUiError error) {
		int exit_code = 4;
		if (error.code == ErrorCode::AgenticInvalidRequest ||
			error.code == ErrorCode::AgenticInvalidScope)
			exit_code = 2;
		else if (error.code == ErrorCode::AgenticUnavailable ||
				 error.code == ErrorCode::AgenticBusy)
			exit_code = 3;
		else if (error.code == ErrorCode::AgenticCanceled ||
				 error.code == ErrorCode::AgenticTimeout)
			exit_code = 5;
		else if (error.code == ErrorCode::AgenticIncompleteCapture ||
				 error.code == ErrorCode::AgenticStaleGeneration)
			exit_code = 6;
		return command_failure(request, exit_code, errorName(error.code), errorMessage(error.code));
	}
	void writer_loop(std::stop_token stop_token) noexcept {
		for (;;) {
			std::string verification_output_identity;
			try {
				std::optional<AgenticCaptureSnapshot> owned;
				std::optional<VerificationPublication> verification_output;
				std::string_view job_id;
				{
					std::unique_lock lock(mutex);
					condition.wait(lock, stop_token, [&] {
						return stop_writer || publication.has_value() ||
							   verification_publication.has_value();
					});
					if (!publication && !verification_publication &&
						(stop_writer || stop_token.stop_requested()))
						return;
					if (verification_publication) {
						verification_output_identity = verification_publication->job_id;
						writer_owned_bytes.store(
							retained_json_bytes(verification_publication->report),
							std::memory_order_relaxed);
						verification_output.emplace(std::move(*verification_publication));
						verification_publication.reset();
					} else {
						const auto bytes = snapshot_owned_bytes(*publication);
						writer_owned_bytes.store(bytes ? *bytes : 0, std::memory_order_relaxed);
						owned.emplace(std::move(*publication));
						publication.reset();
						job_id = publishing_job;
					}
				}
				if (verification_output) {
					std::error_code directory_error;
					std::filesystem::create_directories(verification_output->path.parent_path(),
														directory_error);
					const auto report_valid = validate_agentic_document(
						verification_output->report, "FlowUi.Agentic.VerificationReport");
					const auto written =
						(directory_error || !report_valid)
							? Status(
								  unexpectedError(agentic_error(AgenticError::publication_failed)))
							: atomic_json(verification_output->path, verification_output->report);
					{
						std::lock_guard lock(mutex);
						if (auto* verification = find_verification(verification_output->job_id)) {
							verification->report = std::move(verification_output->report);
							verification->report_published = written.has_value();
							verification->publishing = false;
							verification->terminal = true;
							verification->phase = written ? "terminal" : "publication_failed";
							if (!written) {
								verification->report.set("execution_status", "failed");
								if (argument_string(verification->report, "verdict") != "fail")
									verification->report.set("verdict", "inconclusive");
								verification->report.find("limitations")
									->array.emplace_back("report_publication_failed");
							}
							verification->request = Json{};
							verification->cases = Json{};
							verification->current = Json{};
							++verification->sequence;
						}
						writer_owned_bytes.store(0, std::memory_order_relaxed);
					}
					condition.notify_all();
					continue;
				}
				const auto publication_started_ns = monotonic_nanoseconds();
				auto published = store->publish(std::move(*owned));
				{
					std::lock_guard lock(mutex);
					if (auto* job = find_job(job_id)) {
						job->status.phase = published ? JobPhase::sealed : JobPhase::failed;
						if (published) {
							if (auto* verification = active_verification();
								verification && verification->child_job == job->status.job_id) {
								verification->publication_ns +=
									monotonic_nanoseconds() - publication_started_ns;
								std::uint64_t bytes = 0;
								for (const auto& artifact : published->artifacts)
									bytes += artifact.bytes;
								verification->disk_bytes += bytes;
							}
							job->status.manifest_path = std::move(published->manifest_path);
							if (published->partial)
								job->status.reason = "partial_evidence";
						} else
							job->status.reason = errorName(published.error().code);
					}
					publishing_job.clear();
					writer_owned_bytes.store(0, std::memory_order_relaxed);
				}
				condition.notify_all();
			} catch (...) {
				// Job reason buffers are reserved at admission, before measurement.
				std::lock_guard lock(mutex);
				if (auto* verification = find_verification(verification_output_identity)) {
					verification->publishing = false;
					verification->terminal = true;
					verification->phase = "publication_failed";
					verification->request = Json{};
					verification->cases = Json{};
					verification->current = Json{};
					++verification->sequence;
					try {
						verification->report.set("execution_status", "failed");
						if (argument_string(verification->report, "verdict") != "fail")
							verification->report.set("verdict", "inconclusive");
						verification->report.find("limitations")
							->array.emplace_back("report_publication_failed");
					} catch (...) {}
				}
				if (auto* job = find_job(publishing_job)) {
					job->status.phase = JobPhase::failed;
					job->status.reason = "writer_allocation_failure";
				}
				publishing_job.clear();
				publication.reset();
				writer_owned_bytes.store(0, std::memory_order_relaxed);
				condition.notify_all();
			}
		}
	}
	void set_collection_policy(Job& job, bool enabled) {
		if (enabled) {
			dependencies.monitoring->timing().setConfig(job.previous_timing_config);
			dependencies.tooling->overrides().set_value_collection(job.previous_value_collection);
			for (const auto& [window_id, collected] : job.previous_tree_collection)
				if (dependencies.application->hasWindow(static_cast<WindowId>(window_id)))
					dependencies.application->ui(static_cast<WindowId>(window_id))
						.set_dev_tree_collection(collected);
			optional_collection = true;
		} else {
			job.previous_timing_config = dependencies.monitoring->timing().config();
			auto verification_timing = job.previous_timing_config;
			verification_timing.cpuLevel = CpuTimingLevel::OnlyFrameTime;
			dependencies.monitoring->timing().setConfig(verification_timing);
			job.previous_value_collection = dependencies.tooling->overrides().value_collection();
			job.previous_tree_collection.reserve(
				dependencies.application->devWindowSnapshot().size());
			for (const auto& window : dependencies.application->devWindowSnapshot()) {
				auto& ui = dependencies.application->ui(window.id);
				job.previous_tree_collection.emplace_back(window.id, ui.dev_tree_collection());
				ui.set_dev_tree_collection(false);
			}
			dependencies.tooling->overrides().set_value_collection(false);
			optional_collection = false;
		}
	}
	[[nodiscard]] Result<AgenticScenarioProgress>
	scenario_callback(Job& job, const AgenticScenarioCallback& callback, std::string_view phase) {
		if (!callback)
			return AgenticScenarioProgress::complete;
		struct ActionSink {
			Job* job;
			std::uint64_t tick;
			std::string_view phase;
		};
		ActionSink action_sink{&job, safe_point.app_tick, phase};
		const auto record_semantic_action =
			[](void* owner, const AgenticScenarioAction& action) noexcept -> Status {
			auto& sink = *static_cast<ActionSink*>(owner);
			try {
				auto& snapshot = sink.job->snapshot;
				if (action.name.empty() || action.name.size() > 256 || action.target.size() > 512 ||
					snapshot.actions.array.size() >= snapshot.request.limits.maximum_records)
					return unexpectedError(agentic_error(AgenticError::budget_exhausted));
				if (action.observed_frame && !action.observed_frame->run_id.empty() &&
					action.observed_frame->run_id != snapshot.run_id)
					return unexpectedError(agentic_error(AgenticError::invalid_scope));
				auto record = Json::make_object({{"schema", "FlowUi.Agentic.Action"},
												 {"schema_version", "1.0"},
												 {"run_id", snapshot.run_id},
												 {"capture_id", snapshot.capture_id},
												 {"job_id", snapshot.job_id},
												 {"scenario_id", snapshot.request.scenario_id},
												 {"phase", sink.phase},
												 {"app_tick", decimal(sink.tick)},
												 {"monotonic_ns", decimal(monotonic_nanoseconds())},
												 {"action", action.name},
												 {"target", action.target},
												 {"arguments", action.arguments},
												 {"assertion", action.assertion},
												 {"passed", action.passed}});
				if (action.state_revision)
					record.set("state_revision", decimal(*action.state_revision));
				if (action.observed_frame)
					record.set(
						"observed_frame",
						Json::make_object(
							{{"run_id", snapshot.run_id},
							 {"app_tick", decimal(action.observed_frame->app_tick)},
							 {"window_id", decimal(action.observed_frame->window_id)},
							 {"window_frame", decimal(action.observed_frame->window_frame)}}));
				const auto bytes = snapshot_owned_bytes(snapshot);
				if (!bytes || *bytes > snapshot.request.limits.maximum_snapshot_bytes ||
					write_json(record).size() >
						snapshot.request.limits.maximum_snapshot_bytes - *bytes)
					return unexpectedError(agentic_error(AgenticError::budget_exhausted));
				snapshot.actions.array.emplace_back(std::move(record));
				if (action.assertion && !action.passed)
					snapshot.partial = true;
				return {};
			} catch (...) {
				return unexpectedError(agentic_error(AgenticError::budget_exhausted));
			}
		};
		const auto started = monotonic_nanoseconds();
		Result<AgenticScenarioProgress> result = unexpectedError(
			makeError(ErrorCode::ForeignExceptionObserved, ErrorSite::AgenticScenario));
		if (dependencies.platform_timing)
			dependencies.platform_timing->setFrameContext({}, safe_point.app_tick);
		try {
			ManualTimingZone callback_timing;
			if (dependencies.platform_timing)
				callback_timing.begin(
					*dependencies.platform_timing,
					makeBuiltinTimingDescriptor(0x1b635b638a65e149ull, TimingCategory::User,
												TimingZoneRole::Work, CpuTimingLevel::Summary,
												"flowui.agentic.scenario.callback"));
			result = callback(AgenticScenarioContext{
				dependencies.application, job.status.job_id, safe_point.app_tick,
				safe_point.monotonic_ns - job.scenario_start_ns, job.snapshot.request.seed,
				job.status.cancellation_requested, &action_sink, record_semantic_action});
		} catch (...) {
			result = unexpectedError(
				makeError(ErrorCode::ForeignExceptionObserved, ErrorSite::AgenticScenario));
		}
		if (dependencies.platform_timing)
			dependencies.platform_timing->clearFrameContext();
		auto action = Json::make_object(
			{{"schema", "FlowUi.Agentic.Action"},
			 {"schema_version", "1.0"},
			 {"run_id", job.snapshot.run_id},
			 {"capture_id", job.snapshot.capture_id},
			 {"job_id", job.status.job_id},
			 {"scenario_id", job.snapshot.request.scenario_id},
			 {"phase", phase},
			 {"app_tick", decimal(safe_point.app_tick)},
			 {"monotonic_ns", decimal(safe_point.monotonic_ns)},
			 {"duration_ns", decimal(monotonic_nanoseconds() - started)},
			 {"result",
			  result ? (*result == AgenticScenarioProgress::complete ? "complete" : "pending")
					 : "failed"}});
		if (job.snapshot.actions.array.size() < job.snapshot.request.limits.maximum_records)
			job.snapshot.actions.array.emplace_back(std::move(action));
		else
			return unexpectedError(agentic_error(AgenticError::budget_exhausted));
		return result;
	}
	void finish_scenario(Job& job) {
		if (job.scenario_index == SIZE_MAX || job.scenario_cleanup)
			return;
		const auto result =
			scenario_callback(job, scenarios[job.scenario_index].cleanup, "cleanup");
		if (result && *result == AgenticScenarioProgress::complete)
			job.scenario_cleanup = true;
		if (!result) {
			job.snapshot.partial = true;
			job.snapshot.limitations.emplace_back("scenario_cleanup_failed");
			job.scenario_cleanup = true;
		}
	}
	void fail_job(Job& job, std::string reason, bool canceled = false) {
		job.status.reason = std::move(reason);
		job.status.phase = canceled ? JobPhase::canceled : JobPhase::failed;
		if (!optional_collection)
			set_collection_policy(job, true);
		finish_scenario(job);
		job.snapshot = AgenticCaptureSnapshot{};
		active_job = SIZE_MAX;
	}
	void begin_recording(Job& job, std::uint64_t first_tick, std::uint64_t first_ns,
						 std::uint64_t generation) {
		job.snapshot.first_app_tick = first_tick;
		job.snapshot.measurement_begin_ns = first_ns;
		job.snapshot.timing_capture_generation = generation;
		job.status.phase = JobPhase::recording;
		job.snapshot.provenance.set(
			"measurement_instrumentation",
			Json::make_object(
				{{"cpu_timing_level", Json::number(static_cast<std::uint64_t>(
										  dependencies.monitoring->timing().config().cpuLevel))},
				 {"error_level", Json::number(static_cast<std::uint64_t>(
									 dependencies.monitoring->errors().config().level))},
				 {"memory_level", Json::number(static_cast<std::uint64_t>(
									  dependencies.monitoring->memory().config().level))},
				 {"optional_inspection", optional_collection}}));
		job.baseline.diagnostics = dependencies.monitoring->errorReporting().status();
		job.baseline.error_producers = dependencies.monitoring->errors().producer_scope();
		job.baseline.memory = dependencies.monitoring->memoryReporting().status();
	}
	void materialize(Job& job) {
		job.status.phase = JobPhase::materializing;
		if (interaction) {
			const auto copied_interaction = interaction->materialize(job.snapshot);
			if (!copied_interaction) {
				job.snapshot.partial = true;
				job.snapshot.limitations.emplace_back("interaction_materialization_failed");
			}
		}
		for (auto& frame : job.snapshot.frames)
			frame.key.run_id = job.snapshot.run_id;
		auto copied = materialize_reporting(job.snapshot, *dependencies.monitoring, job.baseline);
		if (!copied) {
			job.snapshot.partial = true;
			job.snapshot.limitations.emplace_back(std::string(errorName(copied.error().code)));
		}
		if (job.snapshot.request.visual != SectionRequirement::omit && capabilities.themes) {
			auto tooling_result =
				materialize_tooling(job.snapshot, *dependencies.tooling, *dependencies.themes);
			if (!tooling_result) {
				job.snapshot.partial = true;
				job.snapshot.limitations.emplace_back("tooling_materialization_failed");
				for (auto& [section, coverage] : job.snapshot.coverage)
					if (section == "visual") {
						coverage.status = SectionStatus::partial;
						coverage.reasons.emplace_back("tooling_materialization_failed");
					}
			}
		}
		if (!job.snapshot.request.content.include_source_paths) {
			for (auto& artifact : job.snapshot.artifacts)
				if (artifact.media_type == "application/json" &&
					artifact.path.starts_with("visual/")) {
					const std::string_view bytes(
						reinterpret_cast<const char*>(artifact.bytes.data()),
						artifact.bytes.size());
					auto document =
						parse_json(bytes, job.snapshot.request.limits.maximum_artifact_bytes);
					if (document) {
						redact_source_paths(*document);
						const auto redacted = write_json(*document);
						artifact.bytes.assign(redacted.begin(), redacted.end());
					}
				}
		}
		if (!optional_collection)
			set_collection_policy(job, true);
		auto request_text = write_json(to_json(job.snapshot.request));
		auto request_result =
			add_text_artifact(job.snapshot, "capture_request", "request.json", "application/json",
							  "FlowUi.Agentic.CaptureRequest", std::move(request_text));
		if (!request_result) {
			fail_job(job, "request_artifact_budget_exhausted");
			return;
		}
		if (job.scenario_index != SIZE_MAX) {
			auto scenario_result =
				add_text_artifact(job.snapshot, "workload_scenario", "workload/scenario.json",
								  "application/json", "FlowUi.Agentic.Scenario",
								  write_json(to_json(scenarios[job.scenario_index].descriptor)));
			if (!scenario_result) {
				fail_job(job, "scenario_artifact_budget_exhausted");
				return;
			}
		}
		job.status.phase = JobPhase::publishing;
		publishing_job = job.status.job_id;
		publication.emplace(std::move(job.snapshot));
		active_job = SIZE_MAX;
		condition.notify_all();
	}
	void advance_job() {
		auto* job = active();
		if (!job) {
			if (!publishing_job.empty() || publication)
				return;
			for (std::size_t job_index = 0; job_index < jobs.size(); ++job_index)
				if (jobs[job_index]->status.phase == JobPhase::queued) {
					active_job = job_index;
					job = jobs[job_index].get();
					break;
				}
			if (!job)
				return;
		}
		auto& controller = dependencies.monitoring->performance_capture();
		const auto& request = job->snapshot.request;
		if (job->status.cancellation_requested) {
			if (job->controller_requested && controller.interface_work_suspended()) {
				controller.request_interface_open(safe_point.app_tick, safe_point.monotonic_ns);
				job->status.phase = JobPhase::canceling;
				if (controller.status().phase == PerformanceCapturePhase::Finalizing)
					return;
			}
			fail_job(*job, "canceled", true);
			return;
		}
		if (job->status.phase == JobPhase::queued) {
			if (controller.interface_work_suspended()) {
				fail_job(*job, "manual_capture_busy");
				return;
			}
			const auto windows = dependencies.application->devWindowSnapshot();
			if (job->snapshot.request.window_ids.empty())
				for (const auto& window : windows)
					if (!dependencies.interface || window.id != dependencies.interface->windowId())
						job->snapshot.request.window_ids.emplace_back(window.id);
			for (const auto window_id : job->snapshot.request.window_ids)
				if (window_id > UINT32_MAX ||
					!dependencies.application->hasWindow(static_cast<WindowId>(window_id)) ||
					(dependencies.interface && window_id == dependencies.interface->windowId() &&
					 !job->verification_owned)) {
					fail_job(*job, "invalid_target_window");
					return;
				}
			job->snapshot.frames.reserve(static_cast<std::size_t>(
				std::min<std::uint64_t>(request.limits.maximum_records,
										request.limits.maximum_ticks *
											std::max<std::size_t>(1, request.window_ids.size()))));
			job->snapshot.images.reserve(static_cast<std::size_t>(request.limits.maximum_images));
			job->snapshot.artifacts.reserve(32);
			job->prepare_tick = safe_point.app_tick;
			job->scenario_start_ns = safe_point.monotonic_ns;
			job->image_failure_baseline = safe_point.image_failures;
			if (request.profile == CaptureProfile::performance_verify)
				set_collection_policy(*job, false);
			job->status.phase = JobPhase::preparing;
		}
		if (job->scenario_index != SIZE_MAX) {
			const auto& scenario = scenarios[job->scenario_index];
			if (safe_point.monotonic_ns - job->scenario_start_ns >
				scenario.descriptor.deadline_ns) {
				if (job->controller_requested && controller.interface_work_suspended())
					controller.fail_capture(safe_point.app_tick, safe_point.monotonic_ns);
				fail_job(*job, "scenario_deadline_exceeded");
				return;
			}
			if (!job->scenario_setup && request.profile != CaptureProfile::startup_diagnose) {
				const auto ready = scenario_callback(*job, scenario.setup, "setup_readiness");
				if (!ready) {
					fail_job(*job, "scenario_setup_failed");
					return;
				}
				if (*ready == AgenticScenarioProgress::pending)
					return;
				job->scenario_setup = true;
				job->prepare_tick = safe_point.app_tick;
			}
		}
		if (job->status.phase == JobPhase::preparing || job->status.phase == JobPhase::warming_up) {
			job->status.phase = JobPhase::warming_up;
			if (!job->provenance_prepared) {
				const auto provenance = complete_provenance(
					job->snapshot, *dependencies.application, *dependencies.monitoring,
					*dependencies.tooling, *dependencies.resource_configuration);
				if (!provenance) {
					fail_job(*job, "provenance_preparation_failed");
					return;
				}
				job->provenance_prepared = true;
				job->prepare_tick = safe_point.app_tick;
			}
			const bool timing_capture = request.performance != SectionRequirement::omit;
			if (!timing_capture && safe_point.app_tick - job->prepare_tick < request.warmup_ticks)
				return;
			if (timing_capture && !job->controller_requested) {
				PerformanceCaptureSettings settings;
				settings.duration_ns = request.duration_ns;
				settings.warmup_ticks = request.warmup_ticks;
				settings.finalization_timeout_ns = request.limits.finalization_timeout_ns;
				settings.budget_window = request.window_ids.empty()
											 ? InvalidWindowId
											 : static_cast<WindowId>(request.window_ids.front());
				settings.reopen_interface = dependencies.interface &&
											dependencies.interface->windowId() != InvalidWindowId &&
											!dependencies.interface->suspended();
				const auto admitted = controller.request_capture(settings);
				if (!admitted) {
					fail_job(*job, "capture_lease_busy");
					return;
				}
				job->controller_requested = true;
				return;
			}
			if (!timing_capture)
				begin_recording(*job, safe_point.app_tick, monotonic_nanoseconds(), 0);
			else if (controller.status().phase == PerformanceCapturePhase::Recording)
				begin_recording(*job, controller.status().first_app_tick,
								controller.status().start_ns, controller.status().generation);
			else
				return;
		}
		if (job->status.phase == JobPhase::recording) {
			job->status.measured_ticks = safe_point.app_tick - job->snapshot.first_app_tick;
			const bool capacity_reached =
				job->status.measured_ticks >= request.limits.maximum_ticks;
			const bool ticks_elapsed = request.measurement_ticks &&
									   job->status.measured_ticks >= request.measurement_ticks;
			const bool duration_elapsed =
				safe_point.monotonic_ns >= job->snapshot.measurement_begin_ns &&
				safe_point.monotonic_ns - job->snapshot.measurement_begin_ns >= request.duration_ns;
			const bool controller_stopped =
				job->controller_requested &&
				(controller.status().phase == PerformanceCapturePhase::Finalizing ||
				 controller.status().phase == PerformanceCapturePhase::Sealed);
			if (job->scenario_index != SIZE_MAX && !job->scenario_setup &&
				request.profile == CaptureProfile::startup_diagnose && !capacity_reached &&
				!ticks_elapsed && !duration_elapsed && !job->stop_requested &&
				!controller_stopped) {
				const auto initialized = scenario_callback(
					*job, scenarios[job->scenario_index].setup, "measured_startup_setup");
				if (!initialized) {
					job->snapshot.partial = true;
					job->snapshot.limitations.emplace_back("startup_initialization_failed");
					job->stop_requested = true;
				} else if (*initialized == AgenticScenarioProgress::complete)
					job->scenario_setup = true;
			}
			if (job->scenario_index != SIZE_MAX && job->scenario_setup && !capacity_reached &&
				!ticks_elapsed && !duration_elapsed && !job->stop_requested &&
				!controller_stopped) {
				const auto stepped =
					scenario_callback(*job, scenarios[job->scenario_index].step, "measured_step");
				if (!stepped) {
					job->snapshot.partial = true;
					job->snapshot.limitations.emplace_back("scenario_step_failed");
					job->stop_requested = true;
				}
			}
			if (job->controller_requested) {
				if (capacity_reached || ticks_elapsed || job->stop_requested)
					controller.request_interface_open(safe_point.app_tick, safe_point.monotonic_ns);
				const auto& progress = controller.status();
				if (progress.phase == PerformanceCapturePhase::Finalizing ||
					progress.phase == PerformanceCapturePhase::Sealed) {
					job->snapshot.end_app_tick_exclusive = progress.end_app_tick_exclusive;
					job->snapshot.measurement_end_ns = progress.end_ns;
					job->snapshot.partial =
						job->snapshot.partial || progress.incomplete || capacity_reached;
					job->status.phase = JobPhase::finalizing;
					job->finalizing_ns = safe_point.monotonic_ns;
				}
			} else if (duration_elapsed || ticks_elapsed || capacity_reached ||
					   job->stop_requested) {
				job->snapshot.end_app_tick_exclusive = safe_point.app_tick;
				job->snapshot.measurement_end_ns = safe_point.monotonic_ns;
				job->status.phase = JobPhase::finalizing;
				job->finalizing_ns = safe_point.monotonic_ns;
				job->snapshot.partial = job->snapshot.partial || capacity_reached;
			}
		}
		if (job->status.phase != JobPhase::finalizing)
			return;
		if (job->controller_requested &&
			controller.status().phase != PerformanceCapturePhase::Sealed)
			return;
		if (interaction && !interaction->ready_to_materialize(job->status.job_id))
			return;
		const auto diagnostics = dependencies.monitoring->errorReporting().status();
		const bool grace_pending = request.runtime_diagnostics != SectionRequirement::omit &&
								   diagnostics.pendingOccurrences != 0;
		const bool images_pending =
			request.visual != SectionRequirement::omit && safe_point.pending_images != 0;
		const bool timed_out =
			safe_point.monotonic_ns - job->finalizing_ns >= request.limits.finalization_timeout_ns;
		if ((grace_pending || images_pending) && !timed_out)
			return;
		if (images_pending || safe_point.image_failures != job->image_failure_baseline) {
			job->snapshot.partial = true;
			job->snapshot.limitations.emplace_back("image_completion_failed_or_timed_out");
		}
		if (job->scenario_index != SIZE_MAX && !job->scenario_setup) {
			job->snapshot.partial = true;
			job->snapshot.limitations.emplace_back("scenario_setup_incomplete_at_cutoff");
		}
		if (job->scenario_index != SIZE_MAX && !job->scenario_verified) {
			const auto verified =
				scenario_callback(*job, scenarios[job->scenario_index].verify, "assertions");
			if (verified && *verified == AgenticScenarioProgress::pending && !timed_out)
				return;
			if (!verified || *verified == AgenticScenarioProgress::pending) {
				job->snapshot.partial = true;
				job->snapshot.limitations.emplace_back("scenario_assertions_failed_or_timed_out");
			}
			job->scenario_verified = true;
		}
		finish_scenario(*job);
		if (job->scenario_index != SIZE_MAX && !job->scenario_cleanup) {
			if (!timed_out)
				return;
			job->snapshot.partial = true;
			job->snapshot.limitations.emplace_back("scenario_cleanup_timed_out");
			job->scenario_cleanup = true;
		}
		materialize(*job);
	}
};

#include "AgenticVerificationService.inl"

AgenticWorkflowService::AgenticWorkflowService(const AgenticDependencies& dependencies)
	: impl_(std::make_unique<Impl>(dependencies)) {
	impl_->owner_service = this;
}
AgenticWorkflowService::~AgenticWorkflowService() {
	const auto result = shutdown();
	if (!result)
		std::fputs("FlowUi agentic shutdown incomplete\n", stderr);
}

Status AgenticWorkflowService::configure(const AgenticWorkflowConfig& configuration) noexcept {
	try {
		std::lock_guard lock(impl_->mutex);
		if (impl_->configured || !impl_->dependencies.application ||
			!impl_->dependencies.monitoring || !impl_->dependencies.tooling ||
			!impl_->dependencies.themes || !impl_->dependencies.resource_configuration)
			return unexpectedError(agentic_error(AgenticError::invalid_request));
		impl_->config = configuration;
		if (configuration.store_path.empty() && !configuration.enabled.has_value()) {
			if (const char* store_path = std::getenv("FLOWUI_AGENTIC_STORE");
				store_path && *store_path) {
				impl_->config.store_path = store_path;
				impl_->config.enabled = true;
				if (configuration.run_id.empty())
					if (const char* run_id = std::getenv("FLOWUI_AGENTIC_RUN_ID"))
						impl_->config.run_id = run_id;
				if (!configuration.local_attachment_enabled.has_value())
					if (const char* enable = std::getenv("FLOWUI_AGENTIC_ENDPOINT_ENABLE"))
						impl_->config.local_attachment_enabled = std::string_view(enable) == "1";
			}
		}
		auto& config = impl_->config;
		auto& capabilities = impl_->capabilities;
		capabilities.application_id = config.application_id;
		capabilities.limits = config.limits;
		capabilities.timing_level = FLOWUI_DEV_TIMING_LEVEL;
		capabilities.memory_level = FLOWUI_DEV_MEMORY_LEVEL;
		capabilities.error_level = FLOWUI_DEV_ERROR_LEVEL;
		capabilities.enabled = config.enabled.value_or(false);
		capabilities.runtime_diagnostics =
			FLOWUI_DEV_ERROR_LEVEL > 0 && impl_->dependencies.monitoring->errors().config().level !=
											  DevErrorCaptureLevel::Disabled;
		capabilities.memory = FLOWUI_DEV_MEMORY_LEVEL > 0 &&
							  impl_->dependencies.monitoring->memory().config().level !=
								  MemoryMonitoringLevel::Disabled;
		capabilities.trees = true;
		capabilities.themes = true;
		capabilities.visual = config.visual_capture_enabled;
		capabilities.local_attachment = config.local_attachment_enabled.value_or(false);
		capabilities.modes = {ExecutionMode::windowed};
		capabilities.profiles = {CaptureProfile::performance_diagnose,
								 CaptureProfile::performance_verify,
								 CaptureProfile::diagnostics_reproduce,
								 CaptureProfile::memory_diagnose, CaptureProfile::startup_diagnose};
		if (config.visual_capture_enabled)
			capabilities.profiles.emplace_back(CaptureProfile::visual_inspect);
		capabilities.limitations = {
			"compositor_display_times_unavailable",
			"worker_producer_barriers_unavailable",
			"layout_and_offscreen_harness_require_application_registration",
			"one_app_per_process_supported",
			"app_construction_evidence_requires_launch_logs",
			"capture_memory_included_in_process_totals",
			"thread_stacks_and_encoder_temporaries_not_individually_classified"};
		capabilities.details = Json::make_object(
			{{"clay_geometry", FLOW_UI_DEV_CAPTURE_CLAY != 0},
			 {"public_vulkan_interop", FLOWUI_PUBLIC_VULKAN_INTEROP != 0},
			 {"icon_manager", FLOWUI_INCLUDE_ICON_MANAGER != 0},
			 {"fsel", COMPILE_FSELI != 0},
			 {"gpu_timing", impl_->dependencies.monitoring->timing().config().gpuTimingEnabled},
			 {"runtime_timing_level",
			  Json::number(static_cast<std::uint64_t>(
				  impl_->dependencies.monitoring->timing().config().cpuLevel))},
			 {"safe_point_queue", true},
			 {"input_control_enabled", config.input_control_enabled},
			 {"action_control_enabled", config.action_control_enabled},
			 {"scenarios_registered", Json::number(0)}});
		if (!config.enabled.value_or(false)) {
			impl_->configured = true;
			return {};
		}
		if (config.store_path.empty() || config.limits.maximum_queued_jobs == 0 ||
			config.limits.maximum_queued_jobs > 256 ||
			config.limits.maximum_snapshot_bytes < 1024 || config.limits.maximum_records == 0 ||
			config.limits.maximum_ticks == 0)
			return unexpectedError(agentic_error(AgenticError::invalid_request));
		if (config.application_id.empty())
			config.application_id = "flowui.application";
		if (config.run_id.empty()) {
			auto run_id = make_identifier("run");
			if (!run_id)
				return unexpectedError(run_id.error());
			config.run_id = std::move(*run_id);
		}
		if (!valid_identifier(config.run_id))
			return unexpectedError(agentic_error(AgenticError::invalid_request));
		capabilities.run_id = config.run_id;
		capabilities.application_id = config.application_id;
		const auto reporting = impl_->dependencies.monitoring->timingReporting().status();
		capabilities.limits.maximum_ticks = std::min(
			config.limits.maximum_ticks, static_cast<std::uint64_t>(reporting.effectiveCapacity));
		impl_->store = std::make_unique<AgenticStore>(config.store_path, config.limits);
		auto initialized = impl_->store->initialize();
		if (!initialized)
			return initialized;
		auto provenance = config.provenance;
		provenance.set("application_id", config.application_id);
		provenance.set("application_revision", config.application_revision.empty()
												   ? "unknown"
												   : config.application_revision);
		provenance.set("process_instance_id", process_instance());
		provenance.set("pid", decimal(process_id()));
		provenance.set("clock_anchor_utc", utc_timestamp());
		provenance.set("clock_anchor_monotonic_ns", decimal(monotonic_nanoseconds()));
		provenance.set("clock_source", "std::chrono::steady_clock");
		provenance.set("clock_sampling_uncertainty_ns", "unknown");
		provenance.set("capabilities", to_json(capabilities));
		provenance.set("capability_flags",
					   Json::make_object(
						   {{"timing_compiled", Json::number(FLOWUI_DEV_TIMING_LEVEL)},
							{"memory_compiled", Json::number(FLOWUI_DEV_MEMORY_LEVEL)},
							{"error_compiled", Json::number(FLOWUI_DEV_ERROR_LEVEL)},
							{"clay_geometry", FLOW_UI_DEV_CAPTURE_CLAY != 0},
							{"public_vulkan_interop", FLOWUI_PUBLIC_VULKAN_INTEROP != 0},
							{"icon_manager", FLOWUI_INCLUDE_ICON_MANAGER != 0},
							{"fsel", COMPILE_FSELI != 0},
							{"gpu_timing",
							 impl_->dependencies.monitoring->timing().config().gpuTimingEnabled}}));
		config.provenance = provenance;
		auto reserved = impl_->store->reserve_run(config.run_id, provenance);
		if (!reserved)
			return unexpectedError(reserved.error());
		impl_->run_path = *reserved;
		impl_->jobs.reserve(256);
		impl_->capture_receipts.reserve(Impl::maximum_capture_receipts);
		impl_->capabilities.details.set(
			"capture_history",
			Json::make_object(
				{{"terminal_history", Json::number(32)},
				 {"idempotency_capacity", Json::number(Impl::maximum_capture_receipts)},
				 {"idempotency_retention_ns", decimal(Impl::capture_receipt_retention_ns)},
				 {"retains_terminal_status", true}}));
		impl_->verifications.reserve(32);
		impl_->verification_receipts.reserve(4096);
		impl_->capabilities.details.set(
			"verification",
			Json::make_object(
				{{"schema_version", "1.0"},
				 {"workload_kinds", Json::make_array({"registered_scenario", "interaction_plan"})},
				 {"checkpoint_triggers", Json::make_array({"case_verified", "case_failed"})},
				 {"evidence_policies",
				  Json::make_array({"assertions_only", "checkpoints_and_failure"})},
				 {"maximum_wait_timeout_ms", Json::number(20000)},
				 {"transport_margin_ms", Json::number(5000)},
				 {"terminal_history", Json::number(32)},
				 {"idempotency_capacity", Json::number(4096)},
				 {"idempotency_retention_ns", decimal(86'400'000'000'000ull)},
				 {"automatic_retention", false},
				 {"contact_sheets", false}}));
		impl_->scenarios.reserve(32);
		impl_->commands.reserve(config.limits.maximum_queued_jobs);
		impl_->command_batch.reserve(config.limits.maximum_queued_jobs);
		impl_->reviews.reserve(64);
		impl_->receipts.reserve(64);
		impl_->admission = true;
		impl_->writer = std::jthread([implementation = impl_.get()](std::stop_token stop_token) {
			implementation->writer_loop(stop_token);
		});
		impl_->configured = true;
		if (config.input_control_enabled || config.action_control_enabled)
			impl_->interaction = std::make_unique<AgenticInteractionRuntime>(
				*this, *impl_->dependencies.application, config);
		if (config.local_attachment_enabled.value_or(false)) {
			auto nonce = make_identifier("nonce");
			if (!nonce)
				return unexpectedError(nonce.error());
			impl_->endpoint.run_id = config.run_id;
			impl_->endpoint.nonce = *nonce;
#if defined(_WIN32)
			impl_->endpoint.endpoint = "\\\\.\\pipe\\flowui_" + config.run_id;
#else
			impl_->endpoint.endpoint =
				(std::filesystem::temp_directory_path() / ("flowui_" + config.run_id + ".sock"))
					.string();
#endif
			impl_->descriptor_path =
				impl_->store->root() / "runtime" / "instances" / (config.run_id + ".json");
			std::filesystem::create_directories(impl_->descriptor_path.parent_path());
			std::filesystem::permissions(impl_->descriptor_path.parent_path(),
										 std::filesystem::perms::owner_all,
										 std::filesystem::perm_options::replace);
			auto started = impl_->server.start(impl_->endpoint, [this](const Json& request) {
				auto decoded = decode_command_request(request);
				if (!decoded)
					return Json::make_object(
						{{"operation_status", "failed"}, {"exit_code", Json::number(2)}});
				auto result = submit_command(*decoded);
				return result ? std::move(*result) : impl_->failure(*decoded, result.error());
			});
			if (!started)
				return started;
			auto descriptor = Json::make_object({{"schema", "FlowUi.Agentic.Instance"},
												 {"schema_version", "1.0"},
												 {"protocol", "FlowUi.Agentic.Local/1"},
												 {"run_id", config.run_id},
												 {"nonce", impl_->endpoint.nonce},
												 {"endpoint", impl_->endpoint.endpoint},
												 {"pid", decimal(process_id())},
												 {"process_instance_id", process_instance()},
												 {"last_safe_point_ns", decimal(0)}});
			auto published = atomic_json(impl_->descriptor_path, descriptor);
			if (!published)
				return published;
			std::filesystem::permissions(impl_->descriptor_path,
										 std::filesystem::perms::owner_read |
											 std::filesystem::perms::owner_write,
										 std::filesystem::perm_options::replace);
		}
		return {};
	} catch (...) {
		return unexpectedError(agentic_error(AgenticError::publication_failed));
	}
}

Result<AgenticCapabilities> AgenticWorkflowService::capabilities() const noexcept {
	try {
		std::lock_guard lock(impl_->mutex);
		return impl_->capabilities;
	} catch (...) {
		return unexpectedError(agentic_error(AgenticError::budget_exhausted));
	}
}
Result<AgenticJobId>
AgenticWorkflowService::request_capture(const AgenticCaptureRequest& request) noexcept {
	try {
		std::lock_guard lock(impl_->mutex);
		if (!impl_->admission || !impl_->config.enabled.value_or(false))
			return unexpectedError(agentic_error(AgenticError::unavailable));
		const auto digest = sha256(write_json(to_json(request)));
		const auto now = monotonic_nanoseconds();
		std::erase_if(impl_->capture_receipts, [now](const auto& receipt) {
			return now - receipt.admitted_ns > Impl::capture_receipt_retention_ns;
		});
		if (!request.idempotency_key.empty())
			for (const auto& receipt : impl_->capture_receipts)
				if (receipt.key == request.idempotency_key)
					return receipt.digest == digest
							   ? Result<AgenticJobId>(receipt.status.job_id)
							   : unexpectedError(agentic_error(AgenticError::invalid_request));
		if (!request.idempotency_key.empty())
			for (const auto& job : impl_->jobs)
				if (job->idempotency_key == request.idempotency_key)
					return job->request_digest == digest
							   ? Result<AgenticJobId>(job->status.job_id)
							   : unexpectedError(agentic_error(AgenticError::invalid_request));
		if (impl_->active_verification() && !impl_->verification_admission)
			return unexpectedError(agentic_error(AgenticError::busy));
		impl_->evict_terminal_jobs();
		if (impl_->interaction && impl_->interaction->active() &&
			!impl_->interaction->admits_capture())
			return unexpectedError(agentic_error(AgenticError::busy));
		if (!request.idempotency_key.empty() &&
			impl_->capture_receipts.size() >= Impl::maximum_capture_receipts)
			return unexpectedError(agentic_error(AgenticError::budget_exhausted));
		auto plan = plan_capture(request, impl_->capabilities);
		if (!plan)
			return unexpectedError(plan.error());
		if (!plan->feasible)
			return unexpectedError(agentic_error(AgenticError::unavailable));
		if (impl_->manual_busy || impl_->jobs.size() == 256 ||
			std::count_if(impl_->jobs.begin(), impl_->jobs.end(), [](const auto& job) {
				return !terminal(job->status.phase);
			}) >= impl_->config.limits.maximum_queued_jobs)
			return unexpectedError(agentic_error(AgenticError::busy));
		auto job_id = make_identifier("job");
		auto capture_id = make_identifier("cap");
		if (!job_id || !capture_id)
			return unexpectedError(agentic_error(AgenticError::publication_failed));
		auto job = std::make_unique<Impl::Job>();
		job->snapshot.request = plan->request;
		job->snapshot.run_id = impl_->config.run_id;
		job->snapshot.capture_id = *capture_id;
		job->snapshot.job_id = *job_id;
		job->snapshot.requested_at_utc = utc_timestamp();
		job->snapshot.provenance = impl_->config.provenance;
		job->snapshot.limitations = plan->limitations;
		job->status.reason.reserve(256);
		job->status.job_id = *job_id;
		job->status.capture_id = *capture_id;
		job->status.required_ticks = request.measurement_ticks;
		job->status.required_intervals =
			request.objective ? request.objective->minimum_valid_intervals : 0;
		job->request_digest = digest;
		job->idempotency_key = request.idempotency_key;
		if (!request.scenario_id.empty()) {
			for (std::size_t scenario_index = 0; scenario_index < impl_->scenarios.size();
				 ++scenario_index)
				if (impl_->scenarios[scenario_index].descriptor.scenario_id == request.scenario_id)
					job->scenario_index = scenario_index;
			if (job->scenario_index == SIZE_MAX)
				return unexpectedError(agentic_error(AgenticError::unavailable));
			const auto& descriptor = impl_->scenarios[job->scenario_index].descriptor;
			if (descriptor.mode != request.mode ||
				(!request.scenario_revision.empty() &&
				 descriptor.revision != request.scenario_revision) ||
				(!request.scenario_digest.empty() && descriptor.digest != request.scenario_digest))
				return unexpectedError(agentic_error(AgenticError::stale_generation));
			job->snapshot.request.scenario_revision = descriptor.revision;
			job->snapshot.request.scenario_digest = descriptor.digest;
			job->snapshot.request.fixture_id = descriptor.fixture_id;
			job->snapshot.request.seed = request.seed ? request.seed : descriptor.seed;
		}
		if (!request.idempotency_key.empty())
			impl_->capture_receipts.emplace_back(
				Impl::CaptureReceipt{job->status, request.idempotency_key, digest, now});
		impl_->jobs.emplace_back(std::move(job));
		return std::move(*job_id);
	} catch (...) {
		return unexpectedError(agentic_error(AgenticError::budget_exhausted));
	}
}
Result<AgenticJobStatus> AgenticWorkflowService::query_job(std::string_view job_id) const noexcept {
	try {
		std::lock_guard lock(impl_->mutex);
		if (const auto* job = impl_->find_job(job_id))
			return job->status;
		if (const auto* receipt = impl_->find_capture_receipt(job_id);
			receipt &&
			monotonic_nanoseconds() - receipt->admitted_ns <= Impl::capture_receipt_retention_ns)
			return receipt->status;
		if (impl_->interaction) {
			const auto owner_id = impl_->interaction->capture_job_id(job_id);
			if (const auto* owner = impl_->find_job(owner_id)) {
				auto status = owner->status;
				status.job_id = job_id;
				return status;
			}
			if (const auto* receipt = impl_->find_capture_receipt(owner_id);
				receipt && monotonic_nanoseconds() - receipt->admitted_ns <=
							   Impl::capture_receipt_retention_ns) {
				auto status = receipt->status;
				status.job_id = job_id;
				return status;
			}
		}
		return unexpectedError(agentic_error(AgenticError::unavailable));
	} catch (...) {
		return unexpectedError(agentic_error(AgenticError::budget_exhausted));
	}
}
Status AgenticWorkflowService::cancel_job(std::string_view job_id) noexcept {
	std::lock_guard lock(impl_->mutex);
	auto* job = impl_->find_job(job_id);
	if (!job && impl_->interaction && impl_->interaction->owns_job(job_id))
		return impl_->interaction->request_cancel(job_id);
	if (!job)
		return unexpectedError(agentic_error(AgenticError::unavailable));
	if (job->status.phase == JobPhase::publishing || job->status.phase == JobPhase::sealed)
		return unexpectedError(agentic_error(AgenticError::busy));
	job->status.cancellation_requested = true;
	return {};
}
Status AgenticWorkflowService::stop_capture(std::string_view job_id) noexcept {
	std::lock_guard lock(impl_->mutex);
	auto* job = impl_->find_job(job_id);
	if (!job || terminal(job->status.phase))
		return unexpectedError(agentic_error(AgenticError::unavailable));
	job->stop_requested = true;
	return {};
}
Status AgenticWorkflowService::register_scenario(AgenticScenario scenario) noexcept {
	try {
		std::lock_guard lock(impl_->mutex);
		const auto& descriptor = scenario.descriptor;
		if (descriptor.scenario_id.empty() || descriptor.revision.empty() ||
			descriptor.fixture_id.empty() || descriptor.digest.empty() ||
			descriptor.deadline_ns == 0 || !scenario.setup || !scenario.step || !scenario.verify ||
			!scenario.cleanup)
			return unexpectedError(agentic_error(AgenticError::invalid_request));
		if (impl_->scenarios.size() >= 32 || impl_->active() ||
			std::any_of(impl_->scenarios.begin(), impl_->scenarios.end(), [&](const auto& present) {
				return present.descriptor.scenario_id == descriptor.scenario_id;
			}))
			return unexpectedError(agentic_error(AgenticError::busy));
		if (std::find(impl_->capabilities.modes.begin(), impl_->capabilities.modes.end(),
					  descriptor.mode) == impl_->capabilities.modes.end())
			impl_->capabilities.modes.emplace_back(descriptor.mode);
		impl_->scenarios.emplace_back(std::move(scenario));
		impl_->capabilities.details.set("scenarios_registered",
										Json::number(impl_->scenarios.size()));
		return {};
	} catch (...) {
		return unexpectedError(agentic_error(AgenticError::budget_exhausted));
	}
}
Result<Json> AgenticWorkflowService::scenarios() const noexcept {
	try {
		std::lock_guard lock(impl_->mutex);
		auto result = Json::make_array();
		for (const auto& scenario : impl_->scenarios)
			result.array.emplace_back(to_json(scenario.descriptor));
		return result;
	} catch (...) {
		return unexpectedError(agentic_error(AgenticError::budget_exhausted));
	}
}

void AgenticWorkflowService::advance_safe_point(const AgenticSafePoint& safe_point) noexcept {
	try {
		if (std::this_thread::get_id() != impl_->platform_thread)
			return;
		{
			std::lock_guard lock(impl_->mutex);
			impl_->safe_point = safe_point;
			impl_->manual_busy =
				!impl_->active() &&
				impl_->dependencies.monitoring->performance_capture().interface_work_suspended();
			impl_->command_batch.swap(impl_->commands);
		}
		for (auto& command : impl_->command_batch) {
			auto result = submit_command(command.request);
			command.result.set_value(std::move(result));
		}
		impl_->command_batch.clear();
		std::lock_guard lock(impl_->mutex);
		if (!impl_->admission)
			return;
		impl_->advance_job();
		if (impl_->interaction)
			impl_->interaction->advance(safe_point.app_tick, safe_point.monotonic_ns);
		impl_->advance_verification();
		impl_->condition.notify_all();
		if (!impl_->active() && impl_->publishing_job.empty() &&
			safe_point.monotonic_ns - impl_->last_session_ns >= 1'000'000'000ull) {
			impl_->last_session_ns = safe_point.monotonic_ns;
			auto session =
				Json::make_object({{"schema", "FlowUi.Agentic.Session"},
								   {"schema_version", "1.0"},
								   {"run_id", impl_->config.run_id},
								   {"last_app_tick", decimal(safe_point.app_tick)},
								   {"last_safe_point_ns", decimal(safe_point.monotonic_ns)},
								   {"status", "running"}});
			const auto written = atomic_json(impl_->run_path / "session.json", session);
			if (!written &&
				std::find(impl_->capabilities.limitations.begin(),
						  impl_->capabilities.limitations.end(),
						  "session_publication_failed") == impl_->capabilities.limitations.end())
				impl_->capabilities.limitations.emplace_back("session_publication_failed");
		}
	} catch (...) {
		std::lock_guard lock(impl_->mutex);
		if (auto* job = impl_->active()) {
			job->status.phase = JobPhase::failed;
			job->status.reason = "safe_point_failure";
			if (!impl_->optional_collection) {
				try {
					impl_->set_collection_policy(*job, true);
				} catch (...) {}
			}
			impl_->active_job = SIZE_MAX;
		}
	}
}
Result<AgenticInteractionRuntime*> AgenticWorkflowService::interaction_runtime() noexcept {
	std::lock_guard lock(impl_->mutex);
	if (std::this_thread::get_id() != impl_->platform_thread || !impl_->interaction)
		return unexpectedError(agentic_error(AgenticError::unavailable));
	return impl_->interaction.get();
}
Result<AgenticJobId>
AgenticWorkflowService::start_interaction(const Json& plan,
										  std::string_view idempotency_key) noexcept {
	std::lock_guard lock(impl_->mutex);
	if (std::this_thread::get_id() != impl_->platform_thread || !impl_->interaction)
		return unexpectedError(agentic_error(AgenticError::unavailable));
	const auto* capture = impl_->active();
	const std::string_view owner =
		capture && (capture->scenario_index != SIZE_MAX || capture->verification_owned)
			? std::string_view(capture->status.job_id)
			: std::string_view{};
	return impl_->interaction->start_plan(plan, idempotency_key, owner);
}
Status
AgenticWorkflowService::track_owned_interaction(std::string_view owner_job_id,
												std::string_view interaction_job_id) noexcept {
	try {
		std::lock_guard lock(impl_->mutex);
		if (auto* verification = impl_->active_verification()) {
			if (verification->child_job != owner_job_id)
				return unexpectedError(agentic_error(AgenticError::invalid_scope));
			verification->interaction_job = interaction_job_id;
		}
		return {};
	} catch (...) {
		return unexpectedError(agentic_error(AgenticError::budget_exhausted));
	}
}
Result<AgenticContentPolicy>
AgenticWorkflowService::interaction_content_policy(std::string_view owner) const noexcept {
	std::lock_guard lock(impl_->mutex);
	const auto* capture = impl_->active();
	if (!capture || capture->status.job_id != owner ||
		(capture->scenario_index == SIZE_MAX && !capture->verification_owned))
		return unexpectedError(agentic_error(AgenticError::invalid_scope));
	return capture->snapshot.request.content;
}
Result<std::uint64_t>
AgenticWorkflowService::interaction_snapshot_budget(std::string_view owner) const noexcept {
	std::lock_guard lock(impl_->mutex);
	const auto* capture = impl_->active();
	if (!capture || capture->status.job_id != owner ||
		(capture->scenario_index == SIZE_MAX && !capture->verification_owned))
		return unexpectedError(agentic_error(AgenticError::invalid_scope));
	return capture->snapshot.request.limits.maximum_snapshot_bytes;
}
Status AgenticWorkflowService::select_frame_input(const EvidenceFrameKey& key, FrameInput& input,
												  float ui_scale) noexcept {
	std::lock_guard lock(impl_->mutex);
	return impl_->interaction ? impl_->interaction->select_input(key, input, ui_scale) : Status{};
}
Result<AgenticInteractionResult>
AgenticWorkflowService::query_interaction(std::string_view job_id) const noexcept {
	std::lock_guard lock(impl_->mutex);
	if (!impl_->interaction)
		return unexpectedError(agentic_error(AgenticError::unavailable));
	return impl_->interaction->query_result(job_id);
}
void AgenticWorkflowService::observe_interaction(const EvidenceFrameKey& key) noexcept {
	std::lock_guard lock(impl_->mutex);
	if (impl_->interaction)
		impl_->interaction->observe_ui(key);
}
bool AgenticWorkflowService::interaction_controls_window(std::uint64_t window_id) const noexcept {
	std::lock_guard lock(impl_->mutex);
	return impl_->interaction && impl_->interaction->controls_window(window_id);
}
void AgenticWorkflowService::observe_frame(AgenticFrameRecord frame) noexcept {
	try {
		std::lock_guard lock(impl_->mutex);
		if (impl_->interaction)
			impl_->interaction->observe_frame(frame);
		auto* job = impl_->active();
		if (!job || job->status.phase != JobPhase::recording ||
			frame.key.app_tick < job->snapshot.first_app_tick)
			return;
		const auto& windows = job->snapshot.request.window_ids;
		if (std::find(windows.begin(), windows.end(), frame.key.window_id) == windows.end())
			return;
		if (job->snapshot.frames.size() >= job->snapshot.request.limits.maximum_records ||
			(job->snapshot.frames.size() + 1) * sizeof(AgenticFrameRecord) >
				job->snapshot.request.limits.maximum_snapshot_bytes -
					std::min(job->image_bytes,
							 job->snapshot.request.limits.maximum_snapshot_bytes)) {
			job->snapshot.partial = true;
			job->stop_requested = true;
			return;
		}
		if (impl_->optional_collection)
			frame.observers = frame.observers | ObserverActivity::tree_copy;
		if (impl_->dependencies.interface &&
			impl_->dependencies.interface->windowId() != InvalidWindowId &&
			!impl_->dependencies.interface->suspended())
			frame.observers = frame.observers | ObserverActivity::developer_interface;
		const auto* build = job->snapshot.provenance.find("build_profile");
		if (build && build->find("validation") && build->find("validation")->bool_or())
			frame.observers = frame.observers | ObserverActivity::validation;
		if (impl_->dependencies.monitoring->errors().config().level >
				DevErrorCaptureLevel::Causal ||
			impl_->dependencies.monitoring->timing().config().cpuLevel > CpuTimingLevel::Balanced)
			frame.observers = frame.observers | ObserverActivity::deep_instrumentation;
		if (frame.presented && !frame.interrupted && !frame.canceled) {
			const auto previous =
				std::find_if(job->snapshot.frames.rbegin(), job->snapshot.frames.rend(),
							 [&](const auto& observed) {
								 return observed.key.window_id == frame.key.window_id;
							 });
			if (previous != job->snapshot.frames.rend() && previous->presented &&
				!previous->interrupted && !previous->canceled &&
				previous->key.window_frame + 1 == frame.key.window_frame &&
				previous->present_ns < frame.present_ns)
				++job->status.measured_intervals;
		}
		job->snapshot.frames.emplace_back(std::move(frame));
	} catch (...) {
		std::lock_guard lock(impl_->mutex);
		if (auto* job = impl_->active()) {
			job->snapshot.partial = true;
			job->stop_requested = true;
		}
	}
}
frame_capture::AgenticFrameCaptureRequest
AgenticWorkflowService::image_request(const AgenticFrameRecord& frame) noexcept {
	try {
		std::lock_guard lock(impl_->mutex);
		frame_capture::AgenticFrameCaptureRequest result;
		auto* job = impl_->active();
		if (!job || job->status.phase != JobPhase::recording ||
			(job->verification_owned &&
			 (!job->checkpoint_active || !job->checkpoint_image ||
			  (job->checkpoint_window && job->checkpoint_window != frame.key.window_id))) ||
			!impl_->config.visual_capture_enabled ||
			job->snapshot.request.visual == SectionRequirement::omit ||
			job->admitted_images >= job->snapshot.request.limits.maximum_images ||
			(job->verification_owned && job->checkpoint_admitted) ||
			std::find(job->snapshot.request.window_ids.begin(),
					  job->snapshot.request.window_ids.end(),
					  frame.key.window_id) == job->snapshot.request.window_ids.end())
			return result;
		result.stamp.run_id = impl_->config.run_id;
		result.stamp.capture_id = job->status.capture_id;
		result.stamp.app_tick = frame.key.app_tick;
		result.stamp.window_frame = frame.key.window_frame;
		result.stamp.window_id = static_cast<std::uint32_t>(frame.key.window_id);
		result.stamp.frame_slot = frame.frame_slot;
		result.stamp.tree_generation = frame.tree_generation;
		const auto tree_identity = "tree_" + std::to_string(frame.key.window_id) + "_" +
								   std::to_string(frame.tree_generation);
		result.stamp.tree_available =
			frame.tree_generation != 0 &&
			std::any_of(
				job->snapshot.artifacts.begin(), job->snapshot.artifacts.end(),
				[&](const auto& artifact) { return artifact.artifact_id == tree_identity; });
		result.stamp.theme_revision = impl_->dependencies.themes->devRevision();
		const auto schema = impl_->dependencies.tooling->schemas().view();
		result.stamp.schema_generation = schema ? schema->generation : 0;
		const auto theme_retained = materialize_theme_revision(
			job->snapshot, *impl_->dependencies.tooling, *impl_->dependencies.themes);
		result.stamp.theme_available = theme_retained.has_value();
		if (theme_retained)
			result.stamp.theme_artifact_id = theme_revision_artifact_id(
				result.stamp.theme_revision, result.stamp.schema_generation);
		else
			job->snapshot.partial = true;
		result.owner = this;
		result.completed_image = [](void* owner, AgenticCompletedImage&& image) noexcept {
			return static_cast<AgenticWorkflowService*>(owner)->accept_completed_image(
				std::move(image));
		};
		result.maximum_images = static_cast<std::uint32_t>(
			job->snapshot.request.limits.maximum_images - job->admitted_images);
		result.maximum_readback_bytes =
			job->snapshot.request.limits.maximum_image_bytes -
			std::min(job->image_bytes, job->snapshot.request.limits.maximum_image_bytes);
		result.capture_window = job->snapshot.request.capture_window;
		result.capture_viewports = job->snapshot.request.capture_viewports;
		++job->admitted_images;
		if (job->verification_owned)
			job->checkpoint_admitted = true;
		return result;
	} catch (...) {
		return {};
	}
}
Status AgenticWorkflowService::accept_completed_image(AgenticCompletedImage image) noexcept {
	try {
		std::lock_guard lock(impl_->mutex);
		auto* job = impl_->active();
		if (!job || image.run_id != impl_->config.run_id ||
			image.capture_id != job->status.capture_id ||
			(job->status.phase != JobPhase::recording && job->status.phase != JobPhase::finalizing))
			return unexpectedError(agentic_error(AgenticError::stale_generation));
		if (job->snapshot.images.size() >= job->snapshot.request.limits.maximum_images ||
			image.rgb.size() >
				job->snapshot.request.limits.maximum_image_bytes -
					std::min(job->image_bytes, job->snapshot.request.limits.maximum_image_bytes)) {
			job->snapshot.partial = true;
			return unexpectedError(agentic_error(AgenticError::budget_exhausted));
		}
		job->image_bytes += image.rgb.size();
		if (auto* verification = impl_->active_verification();
			verification && verification->child_job == job->status.job_id) {
			++verification->image_count;
			verification->readback_bytes =
				std::max<std::uint64_t>(verification->readback_bytes, image.rgb.size());
		}
		job->snapshot.images.emplace_back(std::move(image));
		return {};
	} catch (...) {
		return unexpectedError(agentic_error(AgenticError::budget_exhausted));
	}
}
void AgenticWorkflowService::observe_tree(std::uint64_t window_id) noexcept {
	try {
		std::lock_guard lock(impl_->mutex);
		auto* job = impl_->active();
		if (!job || job->status.phase != JobPhase::recording ||
			(job->verification_owned &&
			 (!job->checkpoint_active || !job->checkpoint_tree ||
			  (job->checkpoint_window && job->checkpoint_window != window_id))) ||
			(job->verification_owned && job->checkpoint_tree_recorded) ||
			job->snapshot.request.visual == SectionRequirement::omit ||
			static_cast<std::uint64_t>(std::count_if(
				job->snapshot.artifacts.begin(), job->snapshot.artifacts.end(),
				[](const auto& artifact) { return artifact.schema == "FlowUi.Agentic.Tree"; })) >=
				job->snapshot.request.limits.maximum_images ||
			std::find(job->snapshot.request.window_ids.begin(),
					  job->snapshot.request.window_ids.end(),
					  window_id) == job->snapshot.request.window_ids.end())
			return;
		const auto& tree =
			impl_->dependencies.application->ui(static_cast<WindowId>(window_id)).devTreeSnapshot();
		const auto tree_id =
			"tree_" + std::to_string(window_id) + "_" + std::to_string(tree.generation);
		if (std::any_of(job->snapshot.artifacts.begin(), job->snapshot.artifacts.end(),
						[&](const auto& present) { return present.artifact_id == tree_id; }))
			return;
		auto owned = materialize_tree(tree, impl_->config.run_id,
									  job->snapshot.request.content.include_tree_text,
									  job->snapshot.request.limits.maximum_snapshot_bytes / 4);
		if (!owned) {
			job->snapshot.partial = true;
			return;
		}
		const auto copied =
			add_text_artifact(job->snapshot, tree_id, "visual/trees/" + tree_id + ".json",
							  "application/json", "FlowUi.Agentic.Tree", write_json(*owned));
		if (!copied)
			job->snapshot.partial = true;
		else if (job->verification_owned)
			job->checkpoint_tree_recorded = true;
	} catch (...) {
		std::lock_guard lock(impl_->mutex);
		if (auto* job = impl_->active())
			job->snapshot.partial = true;
	}
}
bool AgenticWorkflowService::capture_lease_active() const noexcept {
	std::lock_guard lock(impl_->mutex);
	return impl_->active_verification() || impl_->active() || !impl_->publishing_job.empty() ||
		   std::any_of(impl_->jobs.begin(), impl_->jobs.end(),
					   [](const auto& job) { return !terminal(job->status.phase); });
}
bool AgenticWorkflowService::optional_collection_enabled() const noexcept {
	std::lock_guard lock(impl_->mutex);
	return impl_->optional_collection;
}
void AgenticWorkflowService::prepare_window_collection(std::uint64_t window_id) noexcept {
	try {
		std::lock_guard lock(impl_->mutex);
		auto* job = impl_->active();
		if (!job || impl_->optional_collection)
			return;
		auto& policies = job->previous_tree_collection;
		auto& ui = impl_->dependencies.application->ui(static_cast<WindowId>(window_id));
		if (std::none_of(policies.begin(), policies.end(),
						 [&](const auto& policy) { return policy.first == window_id; })) {
			policies.emplace_back(window_id, ui.dev_tree_collection());
			if (job->status.phase == JobPhase::recording) {
				job->snapshot.partial = true;
				job->snapshot.limitations.emplace_back(
					"window_registry_changed_during_measurement");
			}
		}
		ui.set_dev_tree_collection(false);
	} catch (...) {
		std::lock_guard lock(impl_->mutex);
		if (auto* job = impl_->active())
			job->snapshot.partial = true;
	}
}
bool AgenticWorkflowService::visual_capture_enabled() const noexcept {
	std::lock_guard lock(impl_->mutex);
	return impl_->config.enabled.value_or(false) && impl_->config.visual_capture_enabled;
}
std::uint64_t AgenticWorkflowService::owned_memory_bytes() const noexcept {
	try {
		std::lock_guard lock(impl_->mutex);
		std::uint64_t bytes = sizeof(Impl) +
							  impl_->jobs.capacity() * sizeof(std::unique_ptr<Impl::Job>) +
							  impl_->jobs.size() * sizeof(Impl::Job) +
							  impl_->writer_owned_bytes.load(std::memory_order_relaxed);
		for (const auto& job : impl_->jobs) {
			const auto owned = snapshot_owned_bytes(job->snapshot);
			bytes += owned ? *owned : 0;
		}
		bytes += impl_->capture_receipts.capacity() * sizeof(Impl::CaptureReceipt);
		for (const auto& receipt : impl_->capture_receipts)
			bytes += receipt.key.capacity() + receipt.digest.capacity() +
					 receipt.status.job_id.capacity() + receipt.status.capture_id.capacity() +
					 receipt.status.reason.capacity() +
					 receipt.status.manifest_path.native().capacity() *
						 sizeof(std::filesystem::path::value_type);
		bytes += impl_->verifications.capacity() * sizeof(std::unique_ptr<Impl::Verification>) +
				 impl_->verification_receipts.capacity() * sizeof(Impl::VerificationReceipt);
		for (const auto& verification : impl_->verifications) {
			bytes += sizeof(Impl::Verification) + retained_json_bytes(verification->request) +
					 retained_json_bytes(verification->cases) +
					 retained_json_bytes(verification->current) +
					 retained_json_bytes(verification->report);
			bytes += verification->job_id.capacity() + verification->digest.capacity() +
					 verification->key.capacity() + verification->phase.capacity() +
					 verification->child_job.capacity() + verification->interaction_job.capacity() +
					 verification->execution_status.capacity() +
					 verification->report_detail.capacity() +
					 verification->termination_reason.capacity();
		}
		for (const auto& receipt : impl_->verification_receipts)
			bytes += receipt.key.capacity() + receipt.digest.capacity() + receipt.job_id.capacity();
		if (impl_->interaction)
			bytes += impl_->interaction->owned_memory_bytes();
		if (impl_->publication) {
			const auto owned = snapshot_owned_bytes(*impl_->publication);
			bytes += owned ? *owned : 0;
		}
		if (impl_->verification_publication)
			bytes += retained_json_bytes(impl_->verification_publication->report);
		return bytes;
	} catch (...) {
		return 0;
	}
}
void AgenticWorkflowService::rebind_application(App& application) noexcept {
	std::lock_guard lock(impl_->mutex);
	impl_->dependencies.application = &application;
	if (impl_->interaction)
		impl_->interaction->rebind_application(application);
}

Result<Json> AgenticWorkflowService::submit_command(const AgenticCommandRequest& request) noexcept {
	try {
		if (request.operation.starts_with("verification.") &&
			(request.operation != "verification.run" && request.operation != "verification.plan")) {
			std::unique_lock lock(impl_->mutex);
			if (request.operation == "verification.status" &&
				std::this_thread::get_id() != impl_->platform_thread) {
				const auto identity = argument_string(request.arguments, "job_id");
				const auto after_sequence = argument_integer(request.arguments, "after_sequence");
				const auto wait_ms = std::min<std::uint64_t>(
					20000, argument_integer(request.arguments, "wait_timeout_ms"));
				impl_->condition.wait_for(lock, std::chrono::milliseconds(wait_ms), [&] {
					const auto* verification = impl_->find_verification(identity);
					return !impl_->admission || !verification || verification->terminal ||
						   verification->sequence > after_sequence;
				});
			}
			return impl_->verification_command(request);
		}
		if (std::this_thread::get_id() != impl_->platform_thread) {
			std::future<Result<Json>> response;
			{
				std::lock_guard lock(impl_->mutex);
				if (!impl_->admission)
					return unexpectedError(agentic_error(AgenticError::unavailable));
				if (impl_->commands.size() >= impl_->config.limits.maximum_queued_jobs)
					return unexpectedError(agentic_error(AgenticError::busy));
				Impl::PendingCommand command;
				command.request = request;
				response = command.result.get_future();
				impl_->commands.emplace_back(std::move(command));
			}
			if (response.wait_for(std::chrono::milliseconds(1500)) != std::future_status::ready)
				return unexpectedError(
					makeError(ErrorCode::AgenticTimeout, ErrorSite::AgenticCommand));
			auto result = response.get();
			if (result && request.operation == "verification.run" &&
				argument_integer(*result, "exit_code") == 0) {
				const auto* supplied = request.arguments.find("request");
				const auto wait_ms =
					supplied && supplied->find("wait_timeout_ms")
						? std::min<std::uint64_t>(20000,
												  argument_integer(*supplied, "wait_timeout_ms"))
						: 20000;
				const auto identity = argument_string(*result, "job_id");
				std::unique_lock lock(impl_->mutex);
				impl_->condition.wait_for(lock, std::chrono::milliseconds(wait_ms), [&] {
					const auto* verification = impl_->find_verification(identity);
					return !impl_->admission || !verification || verification->terminal;
				});
				if (const auto* verification = impl_->find_verification(identity))
					return impl_->verification_response(request, *verification);
			}
			return result;
		}
		const auto& operation = request.operation;
		if (operation.starts_with("verification.")) {
			std::lock_guard lock(impl_->mutex);
			return impl_->verification_command(request);
		}
		{
			std::lock_guard lock(impl_->mutex);
			if (impl_->active_verification()) {
				const auto descriptors = agentic_command_registry();
				const auto descriptor =
					std::find_if(descriptors.begin(), descriptors.end(),
								 [&](const auto& value) { return value.operation == operation; });
				if (descriptor != descriptors.end() &&
					descriptor->access != AgenticCommandAccess::read)
					return impl_->failure(request, agentic_error(AgenticError::busy));
			}
		}
		if (operation.starts_with("interaction.") || operation.starts_with("action.") ||
			((operation == "job.cancel" || operation == "job.status") && impl_->interaction &&
			 impl_->interaction->owns_job(argument_string(request.arguments, "job_id")))) {
			std::lock_guard lock(impl_->mutex);
			if (!impl_->interaction)
				return impl_->failure(request, agentic_error(AgenticError::unavailable));
			return impl_->interaction->command(request);
		}
		if (operation == "capabilities" || operation == "status") {
			auto result = capabilities();
			if (!result)
				return impl_->failure(request, result.error());
			auto metadata = to_json(*result);
			auto windows = Json::make_array();
			for (const auto& window : impl_->dependencies.application->devWindowSnapshot()) {
				const auto logical = impl_->dependencies.application->windowSize(window.id);
				const auto physical = impl_->dependencies.application->framebufferSize(window.id);
				windows.array.emplace_back(Json::make_object(
					{{"window_id", decimal(window.id)},
					 {"title", window.title},
					 {"logical_width",
					  Json::number(static_cast<std::uint64_t>(std::max(0, logical.first)))},
					 {"logical_height",
					  Json::number(static_cast<std::uint64_t>(std::max(0, logical.second)))},
					 {"physical_width",
					  Json::number(static_cast<std::uint64_t>(std::max(0, physical.first)))},
					 {"physical_height",
					  Json::number(static_cast<std::uint64_t>(std::max(0, physical.second)))},
					 {"internal_development_window",
					  impl_->dependencies.interface &&
						  window.id == impl_->dependencies.interface->windowId()}}));
			}
			for (const auto key : {"build_profile", "application_build_profile", "hardware"})
				if (const auto* value = impl_->config.provenance.find(key))
					metadata.set(key, *value);
			metadata.set("windows", std::move(windows));
			metadata.set("last_app_tick", decimal(impl_->safe_point.app_tick));
			metadata.set("last_safe_point_ns", decimal(impl_->safe_point.monotonic_ns));
			return command_success(request, std::move(metadata));
		}
		if (operation == "capture.plan") {
			auto decoded = capture_request_from_json(request.arguments);
			auto available = capabilities();
			if (!decoded)
				return impl_->failure(request, decoded.error());
			if (!available)
				return impl_->failure(request, available.error());
			auto planned = plan_capture(*decoded, *available);
			return planned ? command_success(request, to_json(*planned))
						   : impl_->failure(request, planned.error());
		}
		if (operation == "capture.start" || operation == "scenario.run") {
			auto decoded = capture_request_from_json(request.arguments);
			if (!decoded)
				return impl_->failure(request, decoded.error());
			auto admitted = request_capture(*decoded);
			if (!admitted)
				return impl_->failure(request, admitted.error());
			auto status = query_job(*admitted);
			auto envelope = command_success(request, to_json(*status), "accepted");
			envelope.set("job_id", *admitted);
			envelope.set("run_id", impl_->config.run_id);
			envelope.set("next", Json::make_object(
									 {{"operation", "job.status"},
									  {"arguments", Json::make_object({{"job_id", *admitted}})}}));
			return envelope;
		}
		if (operation == "job.status" || operation == "job.cancel" || operation == "capture.stop") {
			const auto job_id = argument_string(request.arguments, "job_id");
			if (operation != "job.status") {
				const auto changed =
					operation == "job.cancel" ? cancel_job(job_id) : stop_capture(job_id);
				if (!changed)
					return impl_->failure(request, changed.error());
			}
			auto status = query_job(job_id);
			return status ? command_success(request, to_json(*status))
						  : impl_->failure(request, status.error());
		}
		if (operation == "scenario.list") {
			auto result = scenarios();
			return result ? command_success(request,
											Json::make_object({{"scenarios", std::move(*result)}}))
						  : impl_->failure(request, result.error());
		}
		if (operation == "changes.preview") {
			std::lock_guard lock(impl_->mutex);
			if (impl_->active() || impl_->reviews.size() >= 64)
				return impl_->failure(request, agentic_error(AgenticError::busy));
			tooling::DevChangesReviewRequest review_request;
			if (const auto* decisions = request.arguments.find("decisions")) {
				if (decisions->kind != Json::Kind::array || decisions->array.size() > 1024)
					return impl_->failure(request, agentic_error(AgenticError::invalid_request));
				for (const auto& decision : decisions->array) {
					const auto selection = argument_string(decision, "decision");
					const auto value =
						selection == "include"		 ? tooling::DevChangesDecision::IncludeProposal
						: selection == "omit"		 ? tooling::DevChangesDecision::OmitProposal
						: selection == "retain_disk" ? tooling::DevChangesDecision::RetainDisk
													 : tooling::DevChangesDecision::Remove;
					if (selection != "include" && selection != "omit" &&
						selection != "retain_disk" && selection != "remove")
						return impl_->failure(request,
											  agentic_error(AgenticError::invalid_request));
					review_request.decisions.emplace_back(tooling::DevChangesDecisionEntry{
						argument_string(decision, "key"), value,
						argument_string(decision, "disk_revision")});
				}
			}
			auto identity = make_identifier("review");
			if (!identity)
				return impl_->failure(request, identity.error());
			auto plan = impl_->dependencies.tooling->bakePipeline().query_review(review_request);
			auto rows = Json::make_array();
			for (const auto& row : plan.rows)
				rows.array.emplace_back(Json::make_object(
					{{"key", row.key},
					 {"target", row.target_name},
					 {"field_path", row.field_path},
					 {"on_disk", row.on_disk},
					 {"next_value", row.next_value},
					 {"operation", tooling::changes_operation_name(row.operation)},
					 {"validity", tooling::changes_validity_name(row.validity)},
					 {"reason", row.reason}}));
			auto result = Json::make_object(
				{{"review_id", *identity},
				 {"schema_generation", decimal(plan.schema_generation)},
				 {"proposal_revision", decimal(plan.proposal_revision)},
				 {"theme_revision", decimal(impl_->dependencies.themes->devRevision())},
				 {"disk_revision", plan.disk_revision},
				 {"can_commit", plan.can_commit},
				 {"rows", std::move(rows)}});
			if (write_json(result).size() > 65536)
				return impl_->failure(request, agentic_error(AgenticError::budget_exhausted));
			impl_->reviews.emplace_back(Impl::Review{*identity, std::move(plan),
													 impl_->dependencies.themes->devRevision()});
			return command_success(request, std::move(result));
		}
		if (operation == "changes.apply") {
			std::lock_guard lock(impl_->mutex);
			const auto operation_id = argument_string(request.arguments, "operation_id");
			const auto digest = sha256(write_json(request.arguments));
			if (!valid_identifier(operation_id))
				return impl_->failure(request, agentic_error(AgenticError::invalid_request));
			for (const auto& receipt : impl_->receipts)
				if (receipt.operation_id == operation_id)
					return receipt.digest == digest
							   ? command_success(request, receipt.result)
							   : impl_->failure(request,
												agentic_error(AgenticError::invalid_request));
			if (impl_->active() || impl_->receipts.size() == 64)
				return impl_->failure(request, agentic_error(AgenticError::busy));
			const auto review_id = argument_string(request.arguments, "review_id");
			const auto selected =
				std::find_if(impl_->reviews.begin(), impl_->reviews.end(),
							 [&](const auto& review) { return review.identity == review_id; });
			if (selected == impl_->reviews.end())
				return impl_->failure(request, agentic_error(AgenticError::unavailable));
			if (argument_integer(request.arguments, "schema_generation") !=
					selected->plan.schema_generation ||
				argument_integer(request.arguments, "proposal_revision") !=
					selected->plan.proposal_revision ||
				argument_integer(request.arguments, "theme_revision") != selected->theme_revision ||
				impl_->dependencies.themes->devRevision() != selected->theme_revision ||
				argument_string(request.arguments, "disk_revision") != selected->plan.disk_revision)
				return impl_->failure(request, agentic_error(AgenticError::stale_generation));
			const auto committed =
				impl_->dependencies.tooling->bakePipeline().commit_review(selected->plan);
			if (!committed.applied)
				return impl_->failure(request, agentic_error(AgenticError::stale_generation));
			auto result = Json::make_object(
				{{"applied", true},
				 {"operation_id", operation_id},
				 {"rebuild_required", selected->plan.rebuild_required},
				 {"schema_generation", decimal(selected->plan.schema_generation)}});
			impl_->receipts.emplace_back(Impl::MutationReceipt{operation_id, digest, result});
			return command_success(request, std::move(result));
		}
		return command_failure(request, 3, "unsupported_live_operation",
							   "This operation is unavailable on the selected application.");
	} catch (...) {
		return unexpectedError(agentic_error(AgenticError::budget_exhausted));
	}
}
void AgenticWorkflowService::stop_admission() noexcept {
	{
		std::lock_guard lock(impl_->mutex);
		impl_->admission = false;
		impl_->condition.notify_all();
		for (auto& command : impl_->commands) {
			try {
				command.result.set_value(unexpectedError(agentic_error(AgenticError::canceled)));
			} catch (...) {}
		}
		impl_->commands.clear();
	}
	impl_->server.stop();
}
Status AgenticWorkflowService::shutdown(const AgenticShutdownPolicy& policy) noexcept {
	stop_admission();
	try {
		{
			std::lock_guard lock(impl_->mutex);
			if (impl_->interaction)
				impl_->interaction->shutdown();
			if (auto* job = impl_->active()) {
				if (job->controller_requested)
					impl_->dependencies.monitoring->performance_capture().shutdown(
						impl_->safe_point.app_tick, monotonic_nanoseconds());
				job->snapshot.partial = true;
				job->snapshot.end_app_tick_exclusive = impl_->safe_point.app_tick;
				job->snapshot.measurement_end_ns = monotonic_nanoseconds();
				job->snapshot.limitations.emplace_back("application_shutdown");
				if (policy.device_lost)
					job->snapshot.limitations.emplace_back(
						"device_lost_pending_gpu_evidence_unavailable");
				if (policy.publish_partial && job->snapshot.first_app_tick &&
					impl_->publishing_job.empty())
					impl_->materialize(*job);
				else
					impl_->fail_job(*job, "application_shutdown", true);
			}
			if (auto* verification = impl_->active_verification();
				verification && !verification->publishing) {
				verification->execution_status = "canceled";
				verification->dirty = true;
				impl_->verification_dirty = true;
				impl_->seal_verification(*verification);
			}
			impl_->stop_writer = true;
		}
		impl_->condition.notify_all();
		if (impl_->writer.joinable())
			impl_->writer.join();
		std::error_code filesystem_error;
		if (!impl_->descriptor_path.empty())
			std::filesystem::remove(impl_->descriptor_path, filesystem_error);
		if (!impl_->run_path.empty()) {
			const auto written = atomic_json(
				impl_->run_path / "session.json",
				Json::make_object({{"schema", "FlowUi.Agentic.Session"},
								   {"schema_version", "1.0"},
								   {"run_id", impl_->config.run_id},
								   {"status", "stopped"},
								   {"last_app_tick", decimal(impl_->safe_point.app_tick)}}));
			if (!written)
				return written;
		}
		return {};
	} catch (...) {
		impl_->writer.request_stop();
		impl_->condition.notify_all();
		if (impl_->writer.joinable()) {
			try {
				impl_->writer.join();
			} catch (...) {
				return unexpectedError(agentic_error(AgenticError::publication_failed));
			}
		}
		return unexpectedError(agentic_error(AgenticError::publication_failed));
	}
}
} // namespace FlowUi::devSystems::agentic
#endif
