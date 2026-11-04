#include "FlowUi/Flow.hpp"
#include "TestHarness.hpp"
#include "devSystems/devAgenticWorkflow/AgenticLocalTransport.hpp"
#include "devSystems/devAgenticWorkflow/AgenticManifest.hpp"
#include "devSystems/devAgenticWorkflow/AgenticService.hpp"
#include "devSystems/devMonitoringAndReporting/DevMonitoringAndReporting.hpp"
#include "devSystems/devMonitoringAndReporting/errors/DevError.hpp"
#include "devSystems/devMonitoringAndReporting/reporting/DevPerformanceCapture.hpp"
#include "devSystems/devTooling/DevTooling.hpp"

#include <algorithm>
#include <chrono>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <future>
#include <thread>

namespace {
using namespace FlowUi;
using namespace FlowUi::devSystems::agentic;

struct TestDirectory {
	std::filesystem::path root;
	TestDirectory() {
		const auto identity = make_identifier("flowui_agentic_service_test");
		FLOWUI_CHECK(identity);
		root = std::filesystem::temp_directory_path() / *identity;
		FLOWUI_CHECK(std::filesystem::create_directory(root));
	}
	~TestDirectory() {
		std::error_code error;
		std::filesystem::remove_all(root, error);
	}
};

void frame(App& application, bool inject_runtime_occurrence = false) {
	FLOWUI_CHECK(application.beginFrame());
	if (inject_runtime_occurrence) {
		constexpr auto source = FlowUi::devSystems::makeDevErrorSource(
			"agentic.service.fixture.issue", "tests/AgenticWorkflowTests/AgenticServiceTests.cpp",
			"frame");
		const auto error = makeError(ErrorCode::AssetNotFound, ErrorSite::ResourceReadFile, 18u);
		auto& errors = application.devMonitoring().errors();
		const auto occurrence = errors.recordRaised(error, source);
		FLOWUI_CHECK(occurrence != 0u);
		errors.recordStep(occurrence, FlowUi::devSystems::DevErrorStepKind::Resolution, error,
						  ErrorResolution::Ignored, source);
	}
	Clay_ElementDeclaration root{};
	root.backgroundColor = {22.0f, 44.0f, 88.0f, 255.0f};
	root.layout.sizing = {.width = CLAY_SIZING_GROW(0), .height = CLAY_SIZING_GROW(0)};
	CLAY(CLAY_ID("agentic_service_fixture"), root) {
#if FLOWUI_PUBLIC_VULKAN_INTEROP
		if (application.viewPorts().contains("retained_agentic_viewport")) {
			Clay_ElementDeclaration viewport{};
			viewport.layout.sizing = {.width = CLAY_SIZING_FIXED(32),
									  .height = CLAY_SIZING_FIXED(24)};
			viewport.image.imageData = application.ui().imageData(
				application.viewPorts().getTexture("retained_agentic_viewport"));
			CLAY(CLAY_ID("agentic_service_retained_viewport"), viewport) {}
		}
#endif
	}
	FLOWUI_CHECK(application.endFrame());
	FLOWUI_CHECK(application.drawFrame());
}

AppConfig configuration(const std::filesystem::path& store, bool enabled) {
	AppConfig config;
	config.window.title = "FlowUi Agentic Workflow verification";
	config.window.width = 160;
	config.window.height = 120;
	config.vk.enableValidation = false;
	config.vk.enableDebugUtils = false;
	config.errors.observer.mode = ErrorReportingMode::SinkOnly;
	config.dev.enabled = true;
	config.agentic_workflow.store_path = store;
	config.agentic_workflow.application_id = "flowui.tests.agentic";
	config.agentic_workflow.application_revision = "service-fixture-v1";
	config.agentic_workflow.enabled = enabled;
	config.agentic_workflow.visual_capture_enabled = enabled;
	config.agentic_workflow.limits.maximum_images = 3u;
	return config;
}

void test_dormancy(const std::filesystem::path& store) {
	auto application = makeApplication(configuration(store, false));
	const auto capabilities = application.agentic_workflow().capabilities();
	FLOWUI_CHECK(capabilities && !capabilities->enabled);
	AgenticCaptureRequest request;
	FLOWUI_CHECK(!application.agentic_workflow().request_capture(request));
	frame(application);
	FLOWUI_CHECK(!std::filesystem::exists(store));
}

void test_live_capture(const std::filesystem::path& store) {
	std::uint64_t setup_calls = 0;
	std::uint64_t step_calls = 0;
	std::uint64_t verify_calls = 0;
	std::uint64_t cleanup_calls = 0;
	auto application = makeApplication(configuration(store, true));
	auto& service = application.agentic_workflow();
	const auto capabilities = service.capabilities();
	FLOWUI_CHECK(capabilities && capabilities->enabled && capabilities->visual);
	FLOWUI_CHECK(!capabilities->run_id.empty());
	FLOWUI_CHECK(std::filesystem::exists(store / "store.json"));
	AgenticCaptureRequest canceled_request;
	canceled_request.limits = capabilities->limits;
	canceled_request.idempotency_key = "cancel-before-admission";
	canceled_request.window_ids = {application.mainWindowId()};
	const auto canceled_job = service.request_capture(canceled_request);
	FLOWUI_CHECK(canceled_job);
	FLOWUI_CHECK(service.cancel_job(*canceled_job));
	frame(application);
	const auto canceled_status = service.query_job(*canceled_job);
	FLOWUI_CHECK(canceled_status && canceled_status->phase == JobPhase::canceled);

	AgenticScenario scenario;
	scenario.descriptor.scenario_id = "tests.visual.fixture";
	scenario.descriptor.revision = "1";
	scenario.descriptor.digest = "sha256:deterministic-fixture-v1";
	scenario.descriptor.fixture_id = "solid-blue-root";
	scenario.descriptor.seed = 17u;
	scenario.setup = [&](const AgenticScenarioContext& context) -> Result<AgenticScenarioProgress> {
		++setup_calls;
		FLOWUI_CHECK(context.application == &application && context.seed == 17u);
		AgenticScenarioAction action;
		action.name = "prepare_solid_blue_fixture";
		action.target = "agentic_service_fixture";
		action.arguments = Json::make_object({{"seed", decimal(context.seed)}});
		FLOWUI_CHECK(context.record_action(action));
		return AgenticScenarioProgress::complete;
	};
	scenario.step = [&](const AgenticScenarioContext& context) -> Result<AgenticScenarioProgress> {
		++step_calls;
		FLOWUI_CHECK(context.app_tick > 0u);
		return AgenticScenarioProgress::complete;
	};
	scenario.verify =
		[&](const AgenticScenarioContext& context) -> Result<AgenticScenarioProgress> {
		++verify_calls;
		FLOWUI_CHECK(!context.cancellation_requested && step_calls >= 4u);
		return AgenticScenarioProgress::complete;
	};
	scenario.cleanup =
		[&](const AgenticScenarioContext& context) -> Result<AgenticScenarioProgress> {
		++cleanup_calls;
		FLOWUI_CHECK(context.seed == 17u);
		return cleanup_calls < 2u ? AgenticScenarioProgress::pending
								  : AgenticScenarioProgress::complete;
	};
	FLOWUI_CHECK(service.register_scenario(std::move(scenario)));
	AgenticCaptureRequest request;
	request.limits = capabilities->limits;
	request.profile = CaptureProfile::visual_inspect;
	request.performance = SectionRequirement::required;
	request.runtime_diagnostics = SectionRequirement::omit;
	request.visual = SectionRequirement::required;
	request.window_ids = {application.mainWindowId()};
	request.scenario_id = "tests.visual.fixture";
	request.idempotency_key = "visual-service-pass";
	request.warmup_ticks = 1u;
	request.measurement_ticks = 5u;
	request.duration_ns = 10000000000ull;
	request.limits.maximum_images = 2u;
	const auto planned = plan_capture(request, *service.capabilities());
	FLOWUI_CHECK(planned && planned->feasible);
	FLOWUI_CHECK(
		std::ranges::find(planned->limitations,
						  "combined_visual_diagnostic_timing_suspends_development_interface") !=
		planned->limitations.end());
	const auto job = service.request_capture(request);
	FLOWUI_CHECK(job);
	const auto retried = service.request_capture(request);
	FLOWUI_CHECK(retried && *retried == *job);
	FLOWUI_CHECK(service.capture_lease_active());
	FLOWUI_CHECK(!application.request_dev_performance_capture({}));
	auto different_request = request;
	different_request.seed = 19u;
	FLOWUI_CHECK(!service.request_capture(different_request));
	const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(15);
	AgenticJobStatus status;
	while (std::chrono::steady_clock::now() < deadline) {
		frame(application);
		const auto observed = service.query_job(*job);
		FLOWUI_CHECK(observed);
		status = *observed;
		if (status.phase == JobPhase::sealed || status.phase == JobPhase::failed ||
			status.phase == JobPhase::canceled)
			break;
		std::this_thread::sleep_for(std::chrono::milliseconds(2));
	}
	if (status.phase != JobPhase::sealed)
		std::cerr << "Service capture outcome: " << name(status.phase) << ": " << status.reason
				  << '\n';
	FLOWUI_CHECK(status.phase == JobPhase::sealed);
	FLOWUI_CHECK(setup_calls == 1u && verify_calls == 1u && cleanup_calls == 2u);
	FLOWUI_CHECK(!service.capture_lease_active());
	const auto sealed_retry = service.request_capture(request);
	FLOWUI_CHECK(sealed_retry && *sealed_retry == *job);
	const auto manifest = read_manifest(status.manifest_path);
	FLOWUI_CHECK(manifest && !manifest->partial);
	// Explicit required timing is collected even when the selected profile is visual.
	const auto native_timing = read_artifact(*manifest, "performance_timing", 16u * 1024u * 1024u);
	FLOWUI_CHECK(native_timing && !native_timing->empty());
	const std::string timing_csv(native_timing->begin(), native_timing->end());
	FLOWUI_CHECK(timing_csv.find("\"1\",\"frame\",") != std::string::npos);
	const auto quality = read_artifact_json(*manifest, "capture_quality");
	FLOWUI_CHECK(quality && quality->find("sections"));
	const auto* timing_quality = quality->find("sections")->find("performance");
	FLOWUI_CHECK(timing_quality && timing_quality->find("status")->string_or() == "complete");
	FLOWUI_CHECK(timing_quality->find("generation")->uint64_or() > 0 &&
				 timing_quality->find("retained_records")->uint64_or() > 0);
	const auto image_inventory = read_artifact(*manifest, "visual_frames");
	FLOWUI_CHECK(image_inventory && !image_inventory->empty());
	const std::string records(image_inventory->begin(), image_inventory->end());
	std::size_t cursor = 0;
	std::size_t image_count = 0;
	while (cursor < records.size()) {
		const auto newline = records.find('\n', cursor);
		const auto line = std::string_view(records).substr(
			cursor, newline == std::string::npos ? records.size() - cursor : newline - cursor);
		if (!line.empty()) {
			const auto image = parse_json(line);
			FLOWUI_CHECK(image && image->find("run_id")->string_or() == capabilities->run_id);
			FLOWUI_CHECK(image->find("window_frame")->kind == Json::Kind::string);
			FLOWUI_CHECK(image->find("submission")->kind == Json::Kind::object);
			const auto image_id = image->find("image_id")->string_or();
			const auto png = read_artifact(*manifest, image_id, 1024u * 1024u);
			FLOWUI_CHECK(png);
			const auto decoded = decode_image(*png);
			FLOWUI_CHECK(decoded && decoded->width > 0u && decoded->height > 0u);
			if (const auto* tree = image->find("tree"); tree && tree->kind == Json::Kind::object)
				FLOWUI_CHECK(read_artifact(*manifest, tree->find("artifact_id")->string_or()));
			const auto* theme = image->find("theme");
			FLOWUI_CHECK(theme && theme->kind == Json::Kind::object);
			const auto theme_snapshot =
				read_artifact_json(*manifest, theme->find("artifact_id")->string_or());
			FLOWUI_CHECK(theme_snapshot && theme_snapshot->find("theme_revision")->uint64_or() ==
											   image->find("theme_revision")->uint64_or());
			if (image_count == 0u) {
				AgenticCommandContext offline;
				offline.store_root = store;
				for (const auto source_format : {"png", "ppm"}) {
					const auto selected_id = source_format == std::string_view("ppm")
												 ? std::string(image_id) + "_ppm"
												 : std::string(image_id);
					AgenticCommandRequest image_request;
					image_request.request_id = "request_image_metadata";
					image_request.operation = "artifact.image";
					image_request.arguments = Json::make_object(
						{{"capture", manifest->capture_id},
						 {"image", selected_id},
						 {"format", "png"},
						 {"output",
						  (store.parent_path() / ("derived_" + std::string(source_format) + ".png"))
							  .generic_string()},
						 {"crop", Json::make_object({{"x", Json::number(1u)},
													 {"y", Json::number(1u)},
													 {"width", Json::number(2u)},
													 {"height", Json::number(2u)}})}});
					const auto response = execute_agentic_command(offline, image_request);
					FLOWUI_CHECK(response.exit_code == 0);
					const auto* result = response.envelope.find("result");
					FLOWUI_CHECK(result && result->find("metadata"));
					FLOWUI_CHECK(result->find("metadata")->find("window_frame")->string_or() ==
								 image->find("window_frame")->string_or());
					FLOWUI_CHECK(
						result->find("metadata")->find("theme")->find("artifact_id")->string_or() ==
						theme->find("artifact_id")->string_or());
					FLOWUI_CHECK(result->find("crop_transform")->find("x")->uint64_or() == 1u);
				}
			}
			++image_count;
		}
		if (newline == std::string::npos)
			break;
		cursor = newline + 1u;
	}
	FLOWUI_CHECK(image_count == 2u);
	FLOWUI_CHECK(read_artifact_json(*manifest, "visual_themes"));
	FLOWUI_CHECK(read_artifact_json(*manifest, "visual_catalogue"));
	const auto action_records =
		query_artifact(*manifest, "scenario_actions",
					   Json::make_object({{"action", "prepare_solid_blue_fixture"}}));
	FLOWUI_CHECK(action_records && action_records->records.array.size() == 1);
	FLOWUI_CHECK(action_records->records.array.front().find("run_id")->string_or() ==
				 capabilities->run_id);

#if FLOWUI_PUBLIC_VULKAN_INTEROP
	const auto viewport_created = application.viewPorts().create(
		"retained_agentic_viewport",
		ViewPortCreateInfo{.clearColor = {0.1f, 0.4f, 0.7f, 1.0f}, .clearEveryFrame = false});
	FLOWUI_CHECK(viewport_created && *viewport_created);
	const auto frames_in_flight = application.viewPorts().getVulkanInterop().framesInFlight;
	for (std::uint32_t warmup_frame = 0; warmup_frame <= frames_in_flight; ++warmup_frame)
		frame(application);
	auto viewport_request = request;
	viewport_request.scenario_id.clear();
	viewport_request.idempotency_key = "prior-viewport-service-pass";
	viewport_request.capture_window = false;
	viewport_request.capture_viewports = true;
	const auto viewport_job = service.request_capture(viewport_request);
	FLOWUI_CHECK(viewport_job);
	const auto viewport_deadline = std::chrono::steady_clock::now() + std::chrono::seconds(15);
	while (std::chrono::steady_clock::now() < viewport_deadline) {
		frame(application);
		const auto observed = service.query_job(*viewport_job);
		FLOWUI_CHECK(observed);
		status = *observed;
		if (status.phase == JobPhase::sealed || status.phase == JobPhase::failed ||
			status.phase == JobPhase::canceled)
			break;
		std::this_thread::sleep_for(std::chrono::milliseconds(2));
	}
	FLOWUI_CHECK(status.phase == JobPhase::sealed);
	const auto viewport_manifest = read_manifest(status.manifest_path);
	FLOWUI_CHECK(viewport_manifest && !viewport_manifest->partial);
	const auto viewport_inventory = read_artifact(*viewport_manifest, "visual_frames");
	FLOWUI_CHECK(viewport_inventory && !viewport_inventory->empty());
	const std::string viewport_records(viewport_inventory->begin(), viewport_inventory->end());
	const auto first_line = viewport_records.find('\n');
	const auto viewport_image =
		parse_json(std::string_view(viewport_records).substr(0, first_line));
	FLOWUI_CHECK(viewport_image && viewport_image->find("source")->string_or() == "viewport");
	const auto* producer = viewport_image->find("producer_frame");
	FLOWUI_CHECK(producer && producer->kind == Json::Kind::object);
	FLOWUI_CHECK(producer->find("window_frame")->uint64_or() <
				 viewport_image->find("window_frame")->uint64_or());
	FLOWUI_CHECK(producer->find("submission_serial")->uint64_or() <
				 viewport_image->find("submission")->find("serial")->uint64_or());
	FLOWUI_CHECK(producer->find("source_image_revision")->uint64_or() > 0u);
	FLOWUI_CHECK(!viewport_image->find("sampling_relationship")->find("same_frame")->bool_or());
	const auto viewport_removed = application.viewPorts().remove("retained_agentic_viewport");
	FLOWUI_CHECK(viewport_removed && *viewport_removed);
#endif

	AgenticCaptureRequest performance_request;
	performance_request.limits = capabilities->limits;
	performance_request.profile = CaptureProfile::performance_verify;
	performance_request.window_ids = {application.mainWindowId()};
	performance_request.measurement_ticks = 6u;
	performance_request.warmup_ticks = 1u;
	performance_request.duration_ns = 10000000000ull;
	performance_request.runtime_diagnostics =
		capabilities->runtime_diagnostics ? SectionRequirement::required : SectionRequirement::omit;
	performance_request.memory =
		capabilities->memory ? SectionRequirement::required : SectionRequirement::omit;
	performance_request.idempotency_key = "correlated-performance-service-pass";
	const bool previous_tree_collection = application.ui().dev_tree_collection();
	const bool previous_value_collection = application.devTooling().overrides().value_collection();
	const auto performance_job = service.request_capture(performance_request);
	FLOWUI_CHECK(performance_job);
	bool injected = false;
	bool observed_suspension = false;
	const auto performance_deadline = std::chrono::steady_clock::now() + std::chrono::seconds(15);
	while (std::chrono::steady_clock::now() < performance_deadline) {
		const auto before_frame = service.query_job(*performance_job);
		FLOWUI_CHECK(before_frame);
		const bool recording = before_frame->phase == JobPhase::recording;
		if (recording) {
			observed_suspension = !application.ui().dev_tree_collection() &&
								  !application.devTooling().overrides().value_collection();
		}
		const bool emit = recording && !injected && capabilities->runtime_diagnostics;
		frame(application, emit);
		injected = injected || emit;
		const auto observed = service.query_job(*performance_job);
		FLOWUI_CHECK(observed);
		status = *observed;
		if (status.phase == JobPhase::sealed || status.phase == JobPhase::failed ||
			status.phase == JobPhase::canceled)
			break;
		std::this_thread::sleep_for(std::chrono::milliseconds(2));
	}
	if (status.phase != JobPhase::sealed)
		std::cerr << "Performance service capture outcome: " << name(status.phase) << ": "
				  << status.reason << '\n';
	FLOWUI_CHECK(status.phase == JobPhase::sealed && observed_suspension);
	FLOWUI_CHECK(application.ui().dev_tree_collection() == previous_tree_collection);
	FLOWUI_CHECK(application.devTooling().overrides().value_collection() ==
				 previous_value_collection);
	const auto performance_manifest = read_manifest(status.manifest_path);
	FLOWUI_CHECK(performance_manifest);
	FLOWUI_CHECK(read_artifact(*performance_manifest, "performance_timing", 16u * 1024u * 1024u));
	if (capabilities->runtime_diagnostics) {
		FLOWUI_CHECK(injected);
		const auto diagnostics = read_artifact_json(*performance_manifest, "runtime_diagnostics");
		FLOWUI_CHECK(diagnostics);
		const auto* occurrences = diagnostics->find("occurrences");
		FLOWUI_CHECK(occurrences && occurrences->kind == Json::Kind::array &&
					 !occurrences->array.empty());
		const auto edges = read_artifact(*performance_manifest, "correlation_edges", 1024u * 1024u);
		FLOWUI_CHECK(edges && !edges->empty());
	}
	if (capabilities->memory) {
		const auto samples =
			read_artifact(*performance_manifest, "memory_samples", 16u * 1024u * 1024u);
		FLOWUI_CHECK(samples && !samples->empty());
		FLOWUI_CHECK(read_artifact_json(*performance_manifest, "memory_sources"));
		AgenticCaptureRequest memory_request;
		memory_request.limits = capabilities->limits;
		memory_request.profile = CaptureProfile::memory_diagnose;
		memory_request.performance = SectionRequirement::omit;
		memory_request.runtime_diagnostics = SectionRequirement::omit;
		memory_request.memory = SectionRequirement::required;
		memory_request.window_ids = {application.mainWindowId()};
		memory_request.warmup_ticks = 2;
		memory_request.measurement_ticks = 12;
		memory_request.duration_ns = 10'000'000'000ull;
		memory_request.idempotency_key = "memory-publication-generation-coherence";
		const auto before_memory = application.devMonitoring().memoryReporting().status();
		const auto memory_job = service.request_capture(memory_request);
		FLOWUI_CHECK(memory_job);
		const auto memory_deadline = std::chrono::steady_clock::now() + std::chrono::seconds(15);
		AgenticJobStatus memory_status;
		while (std::chrono::steady_clock::now() < memory_deadline) {
			frame(application);
			const auto progress = service.query_job(*memory_job);
			FLOWUI_CHECK(progress);
			memory_status = *progress;
			if (memory_status.phase == JobPhase::sealed ||
				memory_status.phase == JobPhase::failed ||
				memory_status.phase == JobPhase::canceled)
				break;
			std::this_thread::sleep_for(std::chrono::milliseconds(2));
		}
		FLOWUI_CHECK(memory_status.phase == JobPhase::sealed);
		const auto after_memory = application.devMonitoring().memoryReporting().status();
		FLOWUI_CHECK(after_memory.generation > before_memory.generation + 1);
		FLOWUI_CHECK(after_memory.history_epoch == before_memory.history_epoch &&
					 after_memory.history_storage_generation ==
						 before_memory.history_storage_generation);
		const auto memory_manifest = read_manifest(memory_status.manifest_path);
		FLOWUI_CHECK(memory_manifest);
		const auto memory_quality = read_artifact_json(*memory_manifest, "capture_quality");
		FLOWUI_CHECK(memory_quality && memory_quality->find("sections"));
		const auto* observed_memory = memory_quality->find("sections")->find("memory");
		FLOWUI_CHECK(observed_memory && observed_memory->find("status")->string_or() == "complete");
		FLOWUI_CHECK(!observed_memory->find("uncertain")->bool_or());
		FLOWUI_CHECK(observed_memory->find("gaps")->array.empty() &&
					 observed_memory->find("dropped_records")->uint64_or() == 0 &&
					 observed_memory->find("evicted_records")->uint64_or() == 0 &&
					 observed_memory->find("retained_records")->uint64_or() > 0);
	}
}
[[nodiscard]] Result<Json> command_while_polling(App& application,
												 const AgenticLocalEndpoint& endpoint,
												 const AgenticCommandRequest& request) {
	auto response = std::async(std::launch::async, [&] {
		return send_local_command(endpoint, encode_command_request(request));
	});
	const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(5);
	while (response.wait_for(std::chrono::milliseconds(0)) != std::future_status::ready &&
		   std::chrono::steady_clock::now() < deadline) {
		frame(application);
		std::this_thread::sleep_for(std::chrono::milliseconds(2));
	}
	FLOWUI_CHECK(response.wait_for(std::chrono::milliseconds(0)) == std::future_status::ready);
	return response.get();
}

void test_local_endpoint(const std::filesystem::path& store) {
	std::filesystem::path descriptor_path;
	{
		auto config = configuration(store, true);
		config.agentic_workflow.local_attachment_enabled = true;
		auto application = makeApplication(config);
		auto& service = application.agentic_workflow();
		const auto capabilities = service.capabilities();
		FLOWUI_CHECK(capabilities && capabilities->local_attachment);
		descriptor_path = store / "runtime" / "instances" / (capabilities->run_id + ".json");
		std::ifstream descriptor_stream(descriptor_path, std::ios::binary);
		const std::string descriptor_bytes{std::istreambuf_iterator<char>(descriptor_stream), {}};
		const auto descriptor = parse_json(descriptor_bytes);
		FLOWUI_CHECK(descriptor);
#if !defined(_WIN32)
		const auto permissions = std::filesystem::status(descriptor_path).permissions();
		FLOWUI_CHECK((permissions &
					  (std::filesystem::perms::group_all | std::filesystem::perms::others_all)) ==
					 std::filesystem::perms::none);
#endif
		AgenticLocalEndpoint endpoint;
		endpoint.run_id = descriptor->find("run_id")->string_or();
		endpoint.endpoint = descriptor->find("endpoint")->string_or();
		endpoint.nonce = descriptor->find("nonce")->string_or();
		endpoint.timeout_ms = 2500u;
		AgenticCommandRequest discover;
		discover.request_id = "request_ipc_capabilities";
		discover.operation = "capabilities";
		const auto discovered = command_while_polling(application, endpoint, discover);
		FLOWUI_CHECK(discovered &&
					 discovered->find("operation_status")->string_or() == "completed");
		FLOWUI_CHECK(discovered->find("result")->find("run_id")->string_or() ==
					 capabilities->run_id);
		FLOWUI_CHECK(write_json(*discovered).size() < 65536u);
		auto stale_endpoint = endpoint;
		stale_endpoint.nonce = "nonce_stale_authentication_fixture";
		const auto stale = send_local_command(stale_endpoint, encode_command_request(discover));
		FLOWUI_CHECK(stale && stale->find("operation_status")->string_or() == "failed");
		FLOWUI_CHECK(stale->find("error")->find("message")->string_or() ==
					 "stale_or_unauthorized_run");

		AgenticCaptureRequest request;
		request.limits = capabilities->limits;
		request.window_ids = {application.mainWindowId()};
		request.performance = SectionRequirement::omit;
		request.runtime_diagnostics = SectionRequirement::omit;
		request.visual = SectionRequirement::omit;
		request.measurement_ticks = 3u;
		request.idempotency_key = "ipc-paused-polling-once";
		AgenticCommandRequest plan;
		plan.request_id = "request_ipc_plan";
		plan.operation = "capture.plan";
		plan.arguments = to_json(request);
		const auto planned = command_while_polling(application, endpoint, plan);
		FLOWUI_CHECK(planned && planned->find("result")->find("feasible")->bool_or());
		AgenticCommandRequest start = plan;
		start.request_id = "request_ipc_start";
		start.operation = "capture.start";
		// Deliberately stop platform polling: IPC must report a bounded timeout and
		// leave one recoverable, idempotent request rather than manufacture frames.
		const auto paused = send_local_command(endpoint, encode_command_request(start));
		FLOWUI_CHECK(paused && paused->find("operation_status")->string_or() == "failed");
		FLOWUI_CHECK(paused->find("exit_code")->uint64_or() == 5u);
		frame(application);
		start.request_id = "request_ipc_reconnect";
		const auto recovered = command_while_polling(application, endpoint, start);
		FLOWUI_CHECK(recovered && recovered->find("operation_status")->string_or() == "accepted");
		const std::string job_id(recovered->find("job_id")->string_or());
		FLOWUI_CHECK(!job_id.empty());
		const auto retried = command_while_polling(application, endpoint, start);
		FLOWUI_CHECK(retried && retried->find("job_id")->string_or() == job_id);
		AgenticCommandRequest status;
		status.request_id = "request_ipc_job_status";
		status.operation = "job.status";
		status.arguments = Json::make_object({{"job_id", job_id}});
		const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(15);
		bool sealed = false;
		while (std::chrono::steady_clock::now() < deadline) {
			const auto observed = command_while_polling(application, endpoint, status);
			FLOWUI_CHECK(observed &&
						 observed->find("operation_status")->string_or() == "completed");
			const auto phase = observed->find("result")->find("phase")->string_or();
			FLOWUI_CHECK(phase != "failed" && phase != "canceled");
			if (phase == "sealed") {
				sealed = true;
				break;
			}
		}
		FLOWUI_CHECK(sealed);
		const auto captures = AgenticStore(store).list_captures();
		FLOWUI_CHECK(captures);
		FLOWUI_CHECK(captures->kind == Json::Kind::array && captures->array.size() == 1u);
	}
	FLOWUI_CHECK(!std::filesystem::exists(descriptor_path));
}
} // namespace

int main(int argument_count, char** arguments) {
#if !defined(_WIN32)
	if (!std::getenv("DISPLAY") && !std::getenv("WAYLAND_DISPLAY")) {
		std::cout << "SKIP: native-window Agentic Workflow test requires a GUI display\n";
		return 77;
	}
#endif
	FlowUi::test::Runner runner;
	TestDirectory directory;
	const std::string_view selected = argument_count > 1 ? arguments[1] : "capture";
	if (selected == "dormant")
		runner.run("dormant app creates no agentic store",
				   [&] { test_dormancy(directory.root / "dormant"); });
	else if (selected == "capture")
		runner.run("live capture, scenario cleanup and correlated evidence",
				   [&] { test_live_capture(directory.root / "enabled"); });
	else if (selected == "ipc")
		runner.run("authenticated safe-point IPC and idempotent reconnect",
				   [&] { test_local_endpoint(directory.root / "local"); });
	else
		return 2;
	return runner.finish();
}
