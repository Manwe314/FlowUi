#include "devSystems/devAgenticWorkflow/AgenticAnalysis.hpp"
#include "devSystems/devAgenticWorkflow/AgenticKnowledge.hpp"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <limits>
#include <string>
#include <thread>

using namespace FlowUi;
using namespace FlowUi::devSystems::agentic;
namespace {
void require(bool condition, std::string_view message) {
	if (!condition) {
		std::cerr << message << '\n';
		std::exit(1);
	}
}
void write(const std::filesystem::path& path, std::string_view content) {
	std::filesystem::create_directories(path.parent_path());
	std::ofstream stream(path, std::ios::binary);
	stream << content;
	stream.close();
	require(!stream.fail(), "fixture write failed");
}
[[nodiscard]] AgenticFrameRecord frame(std::string run_id, std::uint64_t number,
									   std::uint64_t timestamp, std::uint64_t window = 2) {
	AgenticFrameRecord result;
	result.key = {std::move(run_id), number, number, window};
	result.begin_ns = timestamp - 100;
	result.end_ns = timestamp;
	result.present_ns = timestamp;
	result.submit_ns = timestamp - 50;
	result.cpu_duration_ns = 50;
	result.submission_serial = number + 7;
	result.device_id = 1;
	result.queue_id = 1;
	result.submitted = true;
	result.presented = true;
	return result;
}
[[nodiscard]] AgenticCaptureSnapshot snapshot(std::string run_id, std::string capture_id,
											  std::uint64_t interval = 20000000) {
	AgenticCaptureSnapshot result;
	result.run_id = std::move(run_id);
	result.capture_id = std::move(capture_id);
	result.job_id = "job_format_test";
	result.request.profile = CaptureProfile::performance_verify;
	result.request.runtime_diagnostics = SectionRequirement::omit;
	result.request.window_ids = {2};
	result.request.scenario_id = "users_fixture";
	result.request.scenario_revision = "1";
	result.request.scenario_digest = "fixture_digest";
	result.request.fixture_id = "users100";
	result.first_app_tick = 1;
	result.end_app_tick_exclusive = 502;
	result.measurement_begin_ns = 1000000000;
	result.measurement_end_ns = 1000000000 + 500 * interval;
	result.provenance = Json::make_object(
		{{"build_profile_digest", "release"},
		 {"hardware_digest", "gpu1"},
		 {"settings_digest", "settings1"},
		 {"instrumentation_digest", "instrumentation1"},
		 {"resource_digest", "resources1"},
		 {"measurement_instrumentation", Json::make_object({{"cpu_timing_level", Json::number(1)},
															{"optional_inspection", false}})},
		 {"capability_flags", Json::make_object({{"gpu_timing", true}, {"clay_geometry", true}})}});
	result.frames.reserve(501);
	for (std::uint64_t number = 1; number <= 501; ++number)
		result.frames.emplace_back(
			frame(result.run_id, number, 1000000000 + (number - 1) * interval));
	SectionCoverage coverage;
	coverage.status = SectionStatus::complete;
	coverage.covered_ranges = {{1, 502}};
	coverage.producer_records = 501;
	coverage.retained_records = 501;
	coverage.finalization_watermark = 502;
	result.coverage.emplace_back("performance", std::move(coverage));
	return result;
}
void json_contract() {
	require(sha256("") == "e3b0c44298fc1c149afbf4c8996fb92427ae41e4649b934ca495991b7852b855",
			"SHA empty vector mismatch");
	require(sha256("abc") == "ba7816bf8f01cfea414140de5dae2223b00361a396177a9cb410ff61f20015ad",
			"SHA abc vector mismatch");
	auto decoded = parse_json(
		R"({"id":"18446744073709551615","unicode":"\ud83d\ude00","long":9007199254740993})");
	require(bool(decoded), "lossless JSON decode failed");
	require(*parse_decimal(*decoded->find("id")) == std::numeric_limits<std::uint64_t>::max(),
			"uint64 corrupted");
	require(decoded->find("long")->text == "9007199254740993", "JSON number token corrupted");
	require(bool(parse_json(write_json(*decoded))), "JSON roundtrip failed");
	require(!parse_json(R"({"id":1,"id":2})"), "duplicate JSON keys accepted");
	require(!parse_json("[1,]"), "trailing array comma accepted");
	require(!parse_json("01"), "leading zero JSON number accepted");
	require(!parse_json(R"("\ud800")"), "unpaired surrogate accepted");
	require(!parse_decimal(Json("18446744073709551616")), "uint64 overflow accepted");
	require(!parse_decimal(Json("01")), "noncanonical uint64 accepted");
	require(!parse_json("\"\xc0\x80\""), "overlong UTF8 accepted");
}
void cadence_contract() {
	std::vector<AgenticFrameRecord> frames;
	frames.reserve(8);
	for (std::uint64_t number = 1; number <= 5; ++number)
		frames.emplace_back(frame("run_cadence", number, 1000000000 + number * 25000000));
	auto analysis = analyze_performance(frames, 2);
	require(bool(analysis), "cadence analysis failed");
	const auto& metrics = analysis->find("windows")->array.front();
	require(metrics.find("observed_fps")->number_or() == 40.0, "N-1 cadence formula wrong");
	require(metrics.find("valid_intervals")->uint64_or() == 4, "interval count wrong");
	frames[2].canceled = true;
	frames[2].presented = false;
	auto canceled = analyze_performance(frames, 2);
	require(canceled->find("windows")->array.front().find("valid_intervals")->uint64_or() == 2,
			"canceled frame joined across a gap");
	require(canceled->find("windows")->array.front().find("segments")->array.size() == 2,
			"disjoint intervals merged");
	frames.emplace_back(frame("run_cadence", 1, 100, 3));
	auto windows = analyze_performance(frames);
	require(windows->find("windows")->array.size() == 2, "windows were combined");
	frames[0].observers = ObserverActivity::image_readback;
	auto tainted = analyze_performance(frames, 2);
	require(tainted->find("windows")->array.front().find("excluded_frames")->uint64_or() == 2,
			"observer interference ignored");
	std::vector<AgenticFrameRecord> duplicate{frame("run_duplicate", 1, 1000000000),
											  frame("run_duplicate", 1, 1020000000),
											  frame("run_duplicate", 2, 1040000000)};
	auto ambiguous = analyze_performance(duplicate, 2);
	require(ambiguous->find("windows")->array.front().find("discontinuities")->uint64_or() == 1,
			"duplicate frame identity disappeared into a singleton segment");
}
void planning_contract() {
	AgenticCaptureRequest request;
	request.window_ids = {2};
	AgenticCapabilities capabilities;
	capabilities.enabled = true;
	capabilities.modes = {ExecutionMode::windowed};
	capabilities.profiles = {CaptureProfile::performance_diagnose,
							 CaptureProfile::performance_verify};
	capabilities.timing_level = 1;
	capabilities.runtime_diagnostics = true;
	auto plan = plan_capture(request, capabilities);
	require(plan && plan->feasible, "supported plan rejected");
	request.limits.maximum_ticks = 12000;
	auto infeasible = plan_capture(request, capabilities);
	require(infeasible && !infeasible->feasible, "retention overflow accepted");
	request.limits.maximum_ticks = 4096;
	request.profile = CaptureProfile::performance_verify;
	request.visual = SectionRequirement::required;
	require(!plan_capture(request, capabilities)->feasible, "image-tainted verify plan accepted");
	require(!capture_request_from_json(*parse_json(R"({"duration_ns":9007199254740993})")),
			"wire uint64 number accepted");
	require(!capture_request_from_json(*parse_json(R"({"mode":"headless"})")),
			"unknown execution mode accepted");
	auto content = capture_request_from_json(*parse_json(
		R"({"content":{"include_tree_text":true,"include_stacks":false},"visual_sources":{"window":false,"viewports":true}})"));
	require(content && content->content.include_tree_text && !content->content.include_stacks &&
				!content->capture_window && content->capture_viewports,
			"content/visual source policy decode failed");
	require(!capture_request_from_json(*parse_json(R"({"content":{"include_tree_text":"true"}})")),
			"untyped content policy accepted");
	request.visual = SectionRequirement::omit;
	request.duration_ns = 10000000000ull;
	request.maximum_expected_tick_cadence = 1200;
	require(!plan_capture(request, capabilities)->feasible,
			"declared high cadence overflow accepted");
	AgenticCaptureRequest objective_request;
	objective_request.profile = CaptureProfile::performance_verify;
	objective_request.window_ids = {2};
	objective_request.objective = AgenticObjective{};
	auto resolved = plan_capture(objective_request, capabilities);
	require(resolved && resolved->feasible && resolved->request.duration_ns == 11000000000ull,
			"present span margin not resolved into capture duration");
	objective_request.maximum_expected_tick_cadence = 400;
	auto margin_overflow = plan_capture(objective_request, capabilities);
	require(margin_overflow && !margin_overflow->feasible,
			"resolved margin excluded from retention feasibility");
}
void store_contract(const std::filesystem::path& root) {
	AgenticStore store(root);
	require(!std::filesystem::exists(root), "constructor created store");
	auto reserved =
		store.reserve_run("run_format", Json::make_object({{"application_id", "test"}}));
	require(bool(reserved), "run reservation failed");
	require(!store.reserve_run("run_format", Json::make_object()),
			"duplicate run reservation accepted");
	auto first = store.publish(snapshot("run_format", "cap_before"));
	require(bool(first), "capture publication failed");
	require(bool(read_manifest(first->manifest_path)), "sealed manifest failed validation");
	require(!store.publish(snapshot("run_format", "cap_before")), "immutable capture overwritten");
	auto second = store.publish(snapshot("run_format", "cap_after"));
	require(bool(second), "second capture failed");
	std::array<AgenticManifest, 5> passes;
	for (std::size_t index = 0; index < passes.size(); ++index) {
		auto published = store.publish(snapshot("run_format", "cap_pass_" + std::to_string(index)));
		require(bool(published), "pass publication failed");
		passes[index] = std::move(*published);
	}
	AgenticObjective objective;
	auto verdict = check_objective(passes, objective);
	require(verdict && verdict->find("verdict")->string_or() == "pass",
			"valid five-pass objective failed");
	auto exactly = store.publish(snapshot("run_format", "cap_exactly40", 25000000));
	require(bool(exactly), "40FPS capture failed");
	AgenticObjective single = objective;
	single.minimum_passes = 1;
	std::array<AgenticManifest, 1> exact_pass{*exactly};
	auto strict = check_objective(exact_pass, single);
	require(strict && strict->find("verdict")->string_or() == "fail",
			"strict gt40 accepted equality");
	auto comparison = compare_captures(*first, *second);
	require(comparison && comparison->find("compatibility")->string_or() == "compatible",
			"matching captures incompatible");
	auto comparison_path = publish_comparison(*first, *second, *comparison);
	require(bool(comparison_path), "immutable comparison publication failed");
	auto changed = snapshot("run_format", "cap_changed_fixture");
	changed.request.fixture_id = "users50";
	auto third = store.publish(std::move(changed));
	auto incompatible = compare_captures(*first, *third);
	require(incompatible && incompatible->find("compatibility")->string_or() == "incompatible",
			"workload reduction accepted");
	auto page = query_artifact(*first, "frame_relations", Json::make_object(), 2, 4096);
	require(page && page->records.array.size() == 2 && !page->cursor.empty(),
			"bounded page failed");
	auto next =
		query_artifact(*first, "frame_relations", Json::make_object(), 2, 4096, page->cursor);
	require(next && next->records.array.front().find("window_frame")->uint64_or() == 3,
			"page cursor repeated records");
	require(!query_artifact(*second, "frame_relations", Json::make_object(), 2, 4096, page->cursor),
			"cursor rebound to different manifest");
	require(!read_artifact(*first, "../../run.json"), "artifact endpoint read arbitrary path");
	auto native_snapshot = snapshot("run_format", "cap_native_numbers");
	const std::string native =
		R"({"schema":"FlowUi.Diagnostics","schema_version":1,"occurrences":[{"id":9007199254740993,"firstAppTick":1,"lastAppTick":2,"error":{"domain":1,"code":7,"impact":1,"name":"NativeWarning","site_name":"NativeSite","nativeCode":0},"steps":[{"sequence":18446744073709551615}]}]})";
	require(bool(add_text_artifact(native_snapshot, "runtime_diagnostics",
								   "diagnostics/runtime.json", "application/json",
								   "FlowUi.Diagnostics", native, "1", "native_numbers")),
			"native artifact fixture failed");
	auto native_capture = store.publish(std::move(native_snapshot));
	require(bool(native_capture), "native artifact capture failed");
	auto native_page = query_artifact(*native_capture, "runtime_diagnostics",
									  Json::make_object({{"severity", "warning"}}));
	require(native_page && native_page->records.array.size() == 1,
			"runtime impact severity normalization failed");
	const auto& occurrence = native_page->records.array.front();
	require(occurrence.find("occurrence_id")->kind == Json::Kind::string &&
				occurrence.find("occurrence_id")->text == "9007199254740993",
			"native uint64 query lost precision");
	require(occurrence.find("steps")->array.front().find("sequence")->kind == Json::Kind::string,
			"nested native sequence not normalized");
	auto missing = snapshot("run_format", "cap_missing_required");
	missing.request.runtime_diagnostics = SectionRequirement::required;
	auto partial = store.publish(std::move(missing));
	require(partial && partial->partial, "missing required section published complete");
	auto dangling = snapshot("run_format", "cap_dangling");
	dangling.correlations.array.emplace_back(
		Json::make_object({{"schema", "FlowUi.Agentic.Correlation"},
						   {"schema_version", "1.0"},
						   {"source", Json::make_object({{"artifact_id", "frame_relations"}})},
						   {"target", Json::make_object({{"artifact_id", "missing_target"}})},
						   {"relationship", "shares_context"},
						   {"method", "explicit_identity"},
						   {"confidence", "explicit"},
						   {"resolution", "resolved"}}));
	require(!store.publish(std::move(dangling)), "resolved edge with absent artifact accepted");
	auto bad_feature = first->document;
	bad_feature.set("required_features", Json::make_array({"unknown_required_feature"}));
	require(!validate_agentic_document(bad_feature, "FlowUi.Agentic.Capture"),
			"unknown required feature accepted");
	auto capacity = snapshot("run_format", "cap_capacity");
	auto owned = snapshot_owned_bytes(capacity);
	require(bool(owned), "snapshot accounting failed");
	capacity.request.limits.maximum_snapshot_bytes = *owned + 7;
	require(!add_text_artifact(capacity, "test", "test.txt", "text/plain", "test", "12345678"),
			"combined frame/text memory budget ignored");
	write(root / "index.json", "corrupt");
	require(bool(store.resolve_capture("cap_before")), "stale index invalidated sealed capture");
	require(bool(store.repair_index()), "index repair failed");
	auto selected = store.prune(0, true);
	require(bool(selected), "prune selection failed");
	for (const auto& entry : selected->find("selected")->array)
		require(entry.find("capture_id")->string_or() != "cap_before" &&
					entry.find("capture_id")->string_or() != "cap_after",
				"referenced comparison evidence selected for prune");
	auto limited = snapshot("run_format", "cap_budget");
	limited.request.limits.maximum_artifact_bytes = 8;
	require(!store.publish(std::move(limited)), "oversize artifact published complete");
	require(!std::filesystem::exists(*reserved / "captures" / "cap_budget" / "manifest.json"),
			"failed publication visible as sealed");
	const auto foreign = *reserved / "captures" / ".cap_foreign.pending";
	write(foreign / ".owner.json",
		  write_json(Json::make_object({{"schema", "FlowUi.Agentic.StagingOwner"},
										{"schema_version", "1.0"},
										{"store_id", "store_someone_else"},
										{"capture_id", "cap_foreign"}})));
	auto cleanup = store.prune(0, true);
	require(cleanup && !cleanup->find("staging")->array.empty(),
			"expired owned staging not selected");
	for (const auto& entry : cleanup->find("staging")->array)
		require(entry.find("capture_id")->string_or() != "cap_foreign",
				"foreign staging selected for removal");
	auto tamper = store.publish(snapshot("run_format", "cap_corrupt"));
	require(bool(tamper), "tamper fixture failed");
	write(tamper->manifest_path.parent_path() / "summary.md", "corrupt");
	require(!read_manifest(tamper->manifest_path), "corrupted artifact accepted");
	auto traversal = first->document;
	traversal.find("artifacts")->array.front().set("path", "../run.json");
	write(first->manifest_path.parent_path() / "invalid.json", write_json(traversal));
	require(!read_manifest(first->manifest_path.parent_path() / "invalid.json"),
			"traversal path accepted");
#ifndef _WIN32
	auto symlinked = store.publish(snapshot("run_format", "cap_symlink"));
	require(bool(symlinked), "symlink fixture failed");
	auto summary_path = symlinked->manifest_path.parent_path() / "summary.md";
	std::filesystem::remove(summary_path);
	std::filesystem::create_symlink(root / "store.json", summary_path);
	require(!read_manifest(symlinked->manifest_path), "symlink escape accepted");
#endif
}
void memory_interval_filter_contract(const std::filesystem::path& root) {
	AgenticStore store(root);
	require(bool(store.initialize()), "memory filter store failed");
	require(bool(store.reserve_run("run_memory_filter", Json::make_object())),
			"memory filter run failed");
	AgenticCaptureSnapshot capture;
	capture.run_id = "run_memory_filter";
	capture.capture_id = "cap_memory_filter";
	capture.job_id = "job_memory_filter";
	capture.request.performance = SectionRequirement::omit;
	capture.request.runtime_diagnostics = SectionRequirement::omit;
	const std::string csv =
		"source_id,window_id,sequence,first_app_tick,end_app_tick_exclusive,logical_live_bytes\n"
		"1,2,1,10,20,100\n"
		"2,2,2,20,30,200\n"
		"1,2,3,30,40,300\n"
		"3,2,4,18446744073709551613,18446744073709551615,400\n"
		"4,2,5,20,20,0\n";
	require(bool(add_text_artifact(capture, "memory_samples", "memory/samples.csv", "text/csv",
								   "FlowUi.Agentic.MemorySamples", csv, "1.0", "decimal_text")),
			"memory sample fixture failed");
	require(bool(add_text_artifact(capture, "point_samples", "points.csv", "text/csv",
								   "Test.PointSamples", "app_tick,window_id\n20,2\n30,3\n", "1.0",
								   "decimal_text")),
			"point sample fixture failed");
	auto published = store.publish(std::move(capture));
	require(bool(published), "memory filter publication failed");
	auto page = query_artifact(
		*published, "memory_samples",
		Json::make_object({{"first_app_tick", "20"}, {"end_app_tick_exclusive", "30"}}));
	require(page && page->records.array.size() == 1 &&
				page->records.array.front().find("sequence")->string_or() == "2",
			"half-open touching memory intervals incorrectly overlap");
	page = query_artifact(
		*published, "memory_samples",
		Json::make_object({{"first_app_tick", "15"}, {"end_app_tick_exclusive", "25"}}), 1, 4096);
	require(page && page->records.array.size() == 1 && !page->cursor.empty(),
			"overlapping memory ranges failed pagination");
	auto next = query_artifact(
		*published, "memory_samples",
		Json::make_object({{"first_app_tick", "15"}, {"end_app_tick_exclusive", "25"}}), 1, 4096,
		page->cursor);
	require(next && next->records.array.size() == 1 &&
				next->records.array.front().find("sequence")->string_or() == "2",
			"memory overlap cursor lost next interval");
	require(!query_artifact(*published, "memory_samples", Json::make_object({{"app_tick", "20"}}),
							1, 4096, page->cursor),
			"changed memory query accepted old cursor");
	for (const auto& [tick, sequence] : {std::pair{"10", "1"},
										 {"19", "1"},
										 {"20", "2"},
										 {"29", "2"},
										 {"30", "3"},
										 {"18446744073709551614", "4"}}) {
		page = query_artifact(*published, "memory_samples",
							  Json::make_object({{"app_tick", tick}, {"window_id", "2"}}));
		require(page && page->records.array.size() == 1 &&
					page->records.array.front().find("sequence")->string_or() == sequence,
				"memory point containment or scalar window filter failed");
	}
	for (const auto tick : {"9", "40", "18446744073709551615"}) {
		page =
			query_artifact(*published, "memory_samples", Json::make_object({{"app_tick", tick}}));
		require(page && page->records.array.empty(), "out-of-range memory point matched");
	}
	page =
		query_artifact(*published, "memory_samples", Json::make_object({{"first_app_tick", "25"}}));
	require(page && page->records.array.size() == 3, "one-sided lower memory overlap failed");
	page = query_artifact(*published, "memory_samples",
						  Json::make_object({{"end_app_tick_exclusive", "20"}}));
	require(page && page->records.array.size() == 1, "one-sided upper memory overlap failed");
	page = query_artifact(
		*published, "memory_samples",
		Json::make_object({{"first_app_tick", "20"}, {"end_app_tick_exclusive", "20"}}));
	require(page && page->records.array.empty(), "empty requested interval matched memory samples");
	page = query_artifact(
		*published, "point_samples",
		Json::make_object(
			{{"first_app_tick", "20"}, {"end_app_tick_exclusive", "30"}, {"window_id", "2"}}));
	require(page && page->records.array.size() == 1 &&
				page->records.array.front().find("app_tick")->string_or() == "20",
			"scalar frame/tick range semantics changed");
}
void theme_field_query_contract(const std::filesystem::path& root) {
	AgenticStore store(root);
	require(bool(store.initialize()), "theme projection store failed");
	require(bool(store.reserve_run("run_themes", Json::make_object())), "theme projection run failed");
	AgenticCaptureSnapshot capture;
	capture.run_id = "run_themes";
	capture.capture_id = "cap_themes";
	capture.job_id = "job_themes";
	capture.request.performance = SectionRequirement::omit;
	capture.request.runtime_diagnostics = SectionRequirement::omit;
	Json variants = Json::make_array();
	for (std::uint64_t variant_index = 0; variant_index < 2; ++variant_index) {
		Json fields = Json::make_array();
		for (std::uint64_t field_index = 0; field_index < 3; ++field_index) {
			fields.array.emplace_back(
				Json::make_object({{"path", "field." + std::to_string(field_index)},
								   {"field_id", decimal(field_index + 100)},
								   {"display_name", "Field"},
								   {"hint", ""},
								   {"type_id", "18446744073709551615"},
								   {"type_name", "double"},
								   {"type_kind", Json::number(4)},
								   {"edit_capability", Json::number(1)},
								   {"capability_reason", Json::number(0)},
								   {"constraints", Json::make_object()},
								   {"source", Json{}},
								   {"effective", Json::number(variant_index * 10 + field_index)},
								   {"authored", Json::number(field_index)},
								   {"startup", Json::number(field_index)},
								   {"preview", Json{}},
								   {"preview_observation", "effective_payload_only"}}));
		}
		variants.array.emplace_back(
			Json::make_object({{"type_id", "77"},
							   {"type_name", "TestTheme"},
							   {"variant", variant_index ? "ocean" : "default"},
							   {"active", variant_index == 0},
							   {"revision", decimal(variant_index + 2)},
							   {"schema_generation", "9"},
							   {"status", "reflected"},
							   {"fields", std::move(fields)}}));
	}
	Json document = Json::make_object({{"schema", "FlowUi.Agentic.Themes"},
									   {"schema_version", "1.0"},
									   {"run_id", "run_themes"},
									   {"theme_revision", "5"},
									   {"schema_generation", "9"},
									   {"schema_fingerprint", "42"},
									   {"collection_available", true},
									   {"variants", std::move(variants)}});
	const auto original = write_json(document);
	require(bool(add_text_artifact(capture, "visual_themes", "visual/themes.json",
								   "application/json", "FlowUi.Agentic.Themes", original)),
			"theme projection artifact failed");
	auto published = store.publish(std::move(capture));
	require(bool(published), "theme projection publication failed");
	auto page = query_artifact(*published, "visual_themes", Json::make_object(), 2, 4096);
	require(page && page->records.array.size() == 2 && !page->cursor.empty() && page->truncated,
			"theme fields did not paginate independently from their full document");
	const auto& first = page->records.array.front();
	require(first.find("path")->string_or() == "field.0" &&
				first.find("variant")->string_or() == "default" &&
				first.find("type_id")->string_or() == "18446744073709551615" &&
				first.find("variant_type_id")->string_or() == "77" &&
				first.find("variant_revision")->string_or() == "2" &&
				first.find("run_id")->string_or() == "run_themes" &&
				first.find("schema_generation")->string_or() == "9" && !first.find("variants") &&
				!first.find("schema"),
			"theme field or owning variant identity changed in projection");
	const auto cursor = page->cursor;
	auto next = query_artifact(*published, "visual_themes", Json::make_object(), 2, 4096, cursor);
	require(next && next->records.array.size() == 2 &&
				next->records.array.front().find("path")->string_or() == "field.2" &&
				next->records.array.back().find("variant")->string_or() == "ocean",
			"theme cursor crossed variants incorrectly");
	page =
		query_artifact(*published, "visual_themes",
					   Json::make_object({{"variant", "ocean"}, {"path", "field.1"}}), 100, 4096);
	require(page && page->records.array.size() == 1 &&
				page->records.array.front().find("effective")->uint64_or() == 11 &&
				page->records.array.front().find("authored")->uint64_or() == 1 &&
				!page->records.array.front().find("active")->bool_or(),
			"exact theme variant/path filter or retained values failed");
	page = query_artifact(*published, "visual_themes", Json::make_object({{"active", true}}), 100,
						  4096);
	require(page && page->records.array.size() == 3, "theme active filter failed");
	require(!query_artifact(*published, "visual_themes", Json::make_object({{"variant", "ocean"}}),
							2, 4096, cursor),
			"changed theme filter accepted a foreign cursor");
	auto foreign = *published;
	foreign.digest = std::string(64, '0');
	require(!query_artifact(foreign, "visual_themes", Json::make_object(), 2, 4096, cursor),
			"foreign manifest accepted a theme cursor");
	auto retained = read_artifact(*published, "visual_themes");
	require(retained && std::string(retained->begin(), retained->end()) == original,
			"query projection rewrote immutable nested theme bytes");
	require(!query_artifact(*published, "visual_themes", Json::make_object(), 2, 128),
			"oversized first theme field exceeded response budget");
	const auto invalid = published->manifest_path.parent_path() / "visual/themes.json";
	document.set("variants", Json::make_object());
	const auto invalid_text = write_json(document);
	write(invalid, invalid_text);
	for (auto& artifact : published->artifacts)
		if (artifact.artifact_id == "visual_themes") {
			artifact.sha256 = sha256(invalid_text);
			artifact.bytes = invalid_text.size();
		}
	require(!query_artifact(*published, "visual_themes"),
			"malformed theme shape produced a successful singleton");
}
void timing_contributor_contract(const std::filesystem::path& root) {
	AgenticStore store(root);
	require(bool(store.reserve_run("run_timing", Json::make_object())),
			"timing contributor run failed");
	const std::vector<std::string> headers{"schema_version",
										   "record_kind",
										   "capture_generation",
										   "app_tick",
										   "window_id",
										   "frame_number",
										   "record_index",
										   "zone_type_id",
										   "zone_name",
										   "zone_role",
										   "zone_category",
										   "source_file",
										   "source_function",
										   "source_line",
										   "source_column",
										   "start_ns",
										   "duration_ns",
										   "direct_child_ns",
										   "exclusive_ns",
										   "invocation_id",
										   "parent_invocation_id",
										   "track",
										   "depth",
										   "quality_flags",
										   "zone_index",
										   "parent_zone_index",
										   "submission_serial",
										   "device_identity",
										   "queue_identity",
										   "timestamp_valid_bits",
										   "timestamp_period_ns",
										   "start_tick",
										   "duration_ticks"};
	std::vector<Json> rows;
	rows.reserve(16);
	const auto emit = [&](std::initializer_list<std::pair<std::string, Json>> fields) {
		Json row = Json::make_object();
		for (const auto& header : headers)
			row.set(header, "0");
		row.set("schema_version", "1");
		row.set("capture_generation", "1");
		row.set("app_tick", "1");
		row.set("window_id", "2");
		row.set("frame_number", "1");
		row.set("quality_flags", "1");
		row.set("timestamp_valid_bits", "64");
		row.set("timestamp_period_ns", "1");
		row.set("parent_zone_index", "4294967295");
		for (const auto& field : fields)
			row.set(field.first, field.second);
		rows.emplace_back(std::move(row));
	};
	emit({{"record_kind", "zone_descriptor"},
		  {"zone_type_id", "1"},
		  {"zone_name", "parent_frame"},
		  {"zone_category", "1"},
		  {"zone_role", "0"},
		  {"source_file", "src/Parent.cpp"},
		  {"source_function", "Parent::frame"},
		  {"source_line", "10"}});
	emit({{"record_kind", "zone_descriptor"},
		  {"zone_type_id", "18446744073709551615"},
		  {"zone_name", "child, work"},
		  {"zone_category", "3"},
		  {"zone_role", "0"},
		  {"source_file", "src/Child.cpp"},
		  {"source_function", "Child::work"},
		  {"source_line", "20"}});
	emit({{"record_kind", "zone_descriptor"},
		  {"zone_type_id", "3"},
		  {"zone_name", "wait"},
		  {"zone_category", "8"},
		  {"zone_role", "1"}});
	emit({{"record_kind", "zone_descriptor"},
		  {"zone_type_id", "4"},
		  {"zone_name", "gpu_parent"},
		  {"zone_category", "7"},
		  {"zone_role", "3"}});
	emit({{"record_kind", "zone_descriptor"},
		  {"zone_type_id", "5"},
		  {"zone_name", "gpu_child"},
		  {"zone_category", "7"},
		  {"zone_role", "3"}});
	emit({{"record_kind", "zone_descriptor"},
		  {"zone_type_id", "6"},
		  {"zone_name", "shared_poll"},
		  {"zone_category", "8"},
		  {"zone_role", "1"}});
	emit({{"record_kind", "cpu_sample"},
		  {"zone_type_id", "1"},
		  {"duration_ns", "1000"},
		  {"exclusive_ns", "1000"},
		  {"invocation_id", "21"},
		  {"quality_flags", "2"},
		  {"track", "1"}});
	emit({{"record_kind", "cpu_sample"},
		  {"zone_type_id", "1"},
		  {"duration_ns", "100"},
		  {"direct_child_ns", "80"},
		  {"exclusive_ns", "20"},
		  {"invocation_id", "10"},
		  {"track", "1"}});
	emit({{"record_kind", "cpu_sample"},
		  {"zone_type_id", "18446744073709551615"},
		  {"duration_ns", "60"},
		  {"exclusive_ns", "60"},
		  {"invocation_id", "11"},
		  {"parent_invocation_id", "10"},
		  {"depth", "1"},
		  {"track", "1"}});
	emit({{"record_kind", "cpu_sample"},
		  {"zone_type_id", "3"},
		  {"duration_ns", "20"},
		  {"exclusive_ns", "20"},
		  {"invocation_id", "12"},
		  {"parent_invocation_id", "10"},
		  {"depth", "1"},
		  {"track", "1"}});
	emit({{"record_kind", "cpu_sample"},
		  {"zone_type_id", "6"},
		  {"duration_ns", "5"},
		  {"exclusive_ns", "5"},
		  {"invocation_id", "13"},
		  {"track", "1"},
		  {"window_id", "0"},
		  {"frame_number", "0"}});
	emit({{"record_kind", "gpu_sample"},
		  {"zone_type_id", "4"},
		  {"duration_ns", "100"},
		  {"zone_index", "0"},
		  {"quality_flags", "3"},
		  {"submission_serial", "7"},
		  {"device_identity", "1"},
		  {"queue_identity", "1"}});
	emit({{"record_kind", "gpu_sample"},
		  {"zone_type_id", "5"},
		  {"duration_ns", "60"},
		  {"zone_index", "1"},
		  {"parent_zone_index", "0"},
		  {"depth", "1"},
		  {"quality_flags", "3"},
		  {"submission_serial", "7"},
		  {"device_identity", "1"},
		  {"queue_identity", "1"}});
	const auto csv = [&] {
		std::string result;
		const auto field = [&](std::string_view value) {
			result.push_back('"');
			for (const char character : value) {
				if (character == '"')
					result.push_back('"');
				result.push_back(character);
			}
			result.push_back('"');
		};
		for (std::size_t index = 0; index < headers.size(); ++index) {
			if (index)
				result.push_back(',');
			field(headers[index]);
		}
		result += "\r\n";
		for (const auto& row : rows) {
			for (std::size_t index = 0; index < headers.size(); ++index) {
				if (index)
					result.push_back(',');
				field(row.find(headers[index])->string_or());
			}
			result += "\r\n";
		}
		return result;
	};
	auto capture = snapshot("run_timing", "cap_timing");
	capture.timing_capture_generation = 1;
	require(
		bool(add_text_artifact(capture, "performance_timing", "performance/timing.csv", "text/csv",
							   "FlowUi.PerformanceCaptureCSV", csv(), "1", "decimal_text")),
		"timing artifact creation failed");
	auto published = store.publish(std::move(capture));
	require(bool(published), "timing contributor capture publication failed");
	auto ranked = analyze_timing_artifact(*published, 2, 10, true);
	require(bool(ranked), "timing contributor analysis failed");
	require(ranked->find("cpu_observed_exclusive_total_ns")->uint64_or() == 105,
			"CPU parent/child work double counted");
	require(ranked->find("cpu_roles_exclusive_ns")->find("wait")->uint64_or() == 25,
			"shared app/window waits not classified");
	const auto& cpu = ranked->find("cpu")->array;
	require(cpu.front().find("zone_type_id")->text == "18446744073709551615",
			"exclusive ranking or zone uint64 changed");
	require(cpu.front().find("observed_exclusive_total_ns")->uint64_or() == 60,
			"child exclusive duration wrong");
	require(cpu.front().find("source")->find("file")->string_or() == "src/Child.cpp",
			"ranked source selector lost");
	const auto parent = std::find_if(cpu.begin(), cpu.end(), [](const auto& value) {
		return value.find("zone_type_id")->string_or() == "1";
	});
	require(parent != cpu.end() &&
				parent->find("evidence")->find("invocation_id")->string_or() == "10",
			"representative evidence points at canceled sample");
	require(ranked->find("excluded_cpu_samples")->uint64_or() == 1,
			"canceled CPU sample ranked as completed work");
	require(ranked->find("gpu_observed_exclusive_total_ns")->uint64_or() == 100,
			"GPU parent/child work double counted");
	const auto& gpu = ranked->find("gpu")->array;
	require(gpu.front().find("zone_type_id")->string_or() == "5" &&
				gpu.front().find("uncalibrated_samples")->uint64_or() == 1,
			"GPU ranking/calibration domain lost");
	auto top = analyze_timing_artifact(*published, 2, 1, false);
	require(top && top->find("truncated")->bool_or() &&
				!top->find("cpu")->array.front().find("source"),
			"top/source query flags ignored");
	rows.back().set("parent_zone_index", "999");
	auto orphan = snapshot("run_timing", "cap_orphan_gpu");
	orphan.timing_capture_generation = 1;
	require(
		bool(add_text_artifact(orphan, "performance_timing", "performance/timing.csv", "text/csv",
							   "FlowUi.PerformanceCaptureCSV", csv(), "1", "decimal_text")),
		"orphan fixture failed");
	auto orphan_capture = store.publish(std::move(orphan));
	require(bool(orphan_capture), "orphan capture failed");
	auto uncertain = analyze_timing_artifact(*orphan_capture, 2);
	require(uncertain && !uncertain->find("gpu_exclusive_complete")->bool_or() &&
				uncertain->find("gpu_observed_exclusive_total_ns")->kind == Json::Kind::null,
			"orphan GPU nesting produced complete exclusive sum");
}
void diagnostics_contract() {
	const auto diagnostics =
		ingest_build_diagnostics("src/Users.cpp:42:7: warning: unused variable "
								 "[-Wunused-variable]\nsrc/Users.cpp:10:3: note: declared here\n",
								 "clang", "build_test");
	require(bool(diagnostics), "compiler ingestion failed");
	const auto* records = diagnostics->find("diagnostics");
	require(records && records->array.size() == 1, "compiler note counted as independent issue");
	require(records->array.front().find("related_notes")->array.size() == 1, "related note lost");
	auto diff = diff_diagnostics(*diagnostics,
								 Json::make_object({{"diagnostics", Json::make_array()}}), false);
	require(diff && diff->find("groups")->array.front().find("state")->string_or() ==
						"not_re_exercised",
			"unexercised issue reported fixed");
}
void knowledge_contract(const std::filesystem::path& root) {
	const auto pack = root / "knowledge";
	const auto source = root / "source";
	const std::string topic =
		"topic_id: test.contract\n\nCadence is measured from window observations.\n";
	const std::string header = "class Example {};\n";
	write(pack / "systems" / "test.md", topic);
	write(source / "Example.hpp", header);
	write(pack / "START_HERE.md", "Start with capabilities and a concrete scenario.\n");
	write(pack / "index.json", write_json(Json::make_object(
								   {{"topics", Json::make_array({Json::make_object(
												   {{"topic_id", "test.contract"},
													{"path", "systems/test.md"},
													{"summary", "window cadence contract"},
													{"keywords", Json::make_array({"performance"})},
													{"sha256", sha256(topic)}})})}})));
	write(
		pack / "source-map.json",
		write_json(Json::make_object(
			{{"selectors", Json::make_array({Json::make_object({{"selector_id", "test.source"},
																{"file", "Example.hpp"},
																{"symbol", "Example"},
																{"sha256", sha256(header)}})})}})));
	AgenticKnowledge knowledge(pack, source);
	require(knowledge.find("performance")->find("topics")->array.size() == 1,
			"semantic topic lookup failed");
	require(knowledge.validate()->find("valid")->bool_or(), "valid semantic pack rejected");
	require(bool(knowledge.read("test.source")), "source selector read failed");
	write(source / "Example.hpp", "class Changed {};\n");
	require(!knowledge.read("test.source"), "stale source selector accepted");
	require(!knowledge.validate()->find("valid")->bool_or(), "source digest drift undetected");
	require(!knowledge.read("../source/Example.hpp"), "context lookup escaped pack inventory");
	auto small = knowledge.read("test.contract", 256);
	require(!small || write_json(*small).size() <= 256,
			"context result exceeded encoded byte bound");
}
} // namespace
int main() {
	auto identity = make_identifier("flowui_agentic_format_test");
	require(bool(identity), "test identity failed");
	const auto root = std::filesystem::temp_directory_path() / *identity;
	json_contract();
	cadence_contract();
	planning_contract();
	store_contract(root / "store");
	memory_interval_filter_contract(root / "memory_filter");
	theme_field_query_contract(root / "themes");
	timing_contributor_contract(root / "timing");
	diagnostics_contract();
	knowledge_contract(root);
	std::filesystem::remove_all(root);
	std::cout << "Agentic format contracts passed\n";
	return 0;
}
