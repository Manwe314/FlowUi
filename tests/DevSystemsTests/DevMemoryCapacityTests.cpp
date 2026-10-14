#ifdef NDEBUG
#undef NDEBUG
#endif
#include <cassert>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <unordered_map>

#include "CppCodeGenerator.hpp"
#include "internal/MemoryCapacityManifest.hpp"
#include "internal/MemoryCapacityProfile.hpp"
#include "ManifestParser.hpp"
#include "devSystems/devInterface/Memory/Analysis/DevMemorySession.hpp"
#include "devSystems/devMonitoringAndReporting/memory/DevContainerMemory.hpp"
#include "devSystems/devMonitoringAndReporting/memory/DevMemory.hpp"
#include "devSystems/devMonitoringAndReporting/memory/DevMemorySources.hpp"
#include "devSystems/devMonitoringAndReporting/reporting/DevMemoryReporting.hpp"
#include "devSystems/devTooling/DevTooling.hpp"
#include "internal/InitialMemoryCapacity.hpp"

using namespace FlowUi;
using namespace FlowUi::devSystems;
namespace {
struct Samples final : MemorySampleSink {
	std::vector<MemoryValueSample> values{};
	bool append(const MemoryValueSample& sample) noexcept override {
		try {
			values.emplace_back(sample);
			return true;
		} catch (...) {
			return false;
		}
	}
};
void check_resolution() {
	const auto entries = makeMemoryCapacityTargetId("flowui.tuning.input_fields.registry_entries");
	const auto bytes = makeMemoryCapacityTargetId("flowui.tuning.storage.initial_string_bytes");
	MemoryCapacityProfile caller;
	caller.storage.initialStringBytes = 256;
	caller.settings = {{bytes, 512}, {entries, 17}, {entries, 17}};
	auto result = FlowUi::detail::resolve_memory_capacity_profile(caller);
	assert(result && result->storage.initialStringBytes == 512 && result->settings.size() == 2);
	const std::array compiled{MemoryCapacitySetting{bytes, 1024},
							  MemoryCapacitySetting{entries, 128}};
	result = FlowUi::detail::resolve_memory_capacity_profile(caller, compiled);
	assert(result && result->storage.initialStringBytes == 1024 && result->settings.size() == 2);
	caller.settings.emplace_back(MemoryCapacitySetting{entries, 18});
	assert(!FlowUi::detail::resolve_memory_capacity_profile(caller));
	caller.settings = {{bytes, 0}};
	result = FlowUi::detail::resolve_memory_capacity_profile(caller);
	assert(result && result->storage.initialStringBytes == 1024 * 1024);
	caller.settings = {{entries, UINT64_MAX}};
	assert(!FlowUi::detail::resolve_memory_capacity_profile(caller));
	caller.settings = {{makeMemoryCapacityTargetId("flowui.tuning.elements"), 16}};
	assert(!FlowUi::detail::resolve_memory_capacity_profile(caller));
	caller.settings.clear();
	for (const auto& definition : FlowUi::detail::initial_capacity_definitions) {
		assert(definition.id() != 0 &&
			   FlowUi::detail::valid_initial_capacity(definition, definition.default_value));
		assert(
			&FlowUi::detail::initial_capacity_definitions[static_cast<size_t>(definition.target)] ==
			&definition);
	}
	caller.growthFactor = NAN;
	assert(!FlowUi::detail::resolve_memory_capacity_profile(caller));
}
void check_demand_and_growth() {
	Samples samples;
	FlowUi::detail::MemoryDemandPeaks peaks;
	std::vector<uint64_t> vector;
	for (uint64_t index = 0; index < 112; ++index)
		vector.emplace_back(index);
	peaks.record(FlowUi::detail::InitialCapacityTarget::input_fields_rect_override, vector.size());
	vector.clear();
	append_capacity_demand(samples,
						   FlowUi::detail::InitialCapacityTarget::input_fields_rect_override,
						   vector, peaks, 7);
	assert(samples.values.back().objectCount == 112 && samples.values.back().logicalLiveBytes == 0);
	append_capacity_demand(samples,
						   FlowUi::detail::InitialCapacityTarget::input_fields_rect_override,
						   vector, peaks, 7);
	assert(samples.values.back().objectCount == 0);
	std::unordered_map<uint64_t, uint64_t> hash;
	hash.reserve(128);
	hash.emplace(1, 2);
	append_capacity_demand(samples, FlowUi::detail::InitialCapacityTarget::input_fields_registry,
						   hash, peaks);
	assert(samples.values.back().objectCount == 1 && samples.values.back().capacityCount >= 128);
	const auto vector_transitions = [](size_t initial) {
		std::vector<uint64_t> values;
		FlowUi::detail::reserve_initial_capacity(values, initial);
		size_t transitions = 0, previous = values.capacity();
		for (uint64_t index = 0; index < 112; ++index) {
			values.emplace_back(index);
			if (previous != values.capacity()) {
				++transitions;
				previous = values.capacity();
			}
		}
		values.clear();
		assert(values.capacity() >= initial);
		return transitions;
	};
	const auto hash_transitions = [](size_t initial) {
		std::unordered_map<uint64_t, uint64_t> values;
		FlowUi::detail::reserve_initial_capacity(values, initial);
		size_t transitions = 0, previous = values.bucket_count();
		for (uint64_t index = 0; index < 112; ++index) {
			values.emplace(index, index);
			if (previous != values.bucket_count()) {
				++transitions;
				previous = values.bucket_count();
			}
		}
		return transitions;
	};
	assert(vector_transitions(0) > 0 && vector_transitions(128) == 0);
	assert(hash_transitions(0) > 0 && hash_transitions(128) == 0);
	// Global hint across windows is a maximum, while flags and declared metrics survive ingestion.
	struct Owner {
		std::vector<uint64_t> values;
		WindowId window;
		FlowUi::detail::MemoryDemandPeaks peaks;
	};
	Owner first{{1, 2, 3}, 7, {}}, second{{1, 2, 3, 4, 5}, 8, {}};
	constexpr auto source =
		makeMemoryCapacityTargetId("flowui.tuning.input_fields.registry_entries");
	const auto probe = +[](const void* owner, MemoryProbeContext& context) noexcept {
		const auto& value = *static_cast<const Owner*>(owner);
		append_capacity_demand(context.sink,
							   FlowUi::detail::InitialCapacityTarget::input_fields_registry,
							   value.values, value.peaks, value.window);
	};
	DevMemory memory(DevMemoryConfig{.gpuMemory = false, .processMemory = false});
	assert(memory.registerProbe({source, &first, probe}));
	assert(memory.registerProbe({source, &second, probe}));
	DevMemoryReporting reporting(memory, MemoryReportingConfig{.managerSampleEveryTicks = 1});
	reporting.consume(1);
	const auto read = reporting.read_live();
	const auto found =
		std::find_if(read.sources.begin(), read.sources.end(), [](const auto& value) {
			return value.key.source == source && value.key.window == InvalidWindowId;
		});
	assert(found != read.sources.end() && found->value.objectCount == 5);
	assert(uint16_t(found->value.flags) & uint16_t(MemorySampleFlag::Estimate));
	FlowUi::detail::MemoryDemandPeaks gated;
	gated.bind(memory.demand_control());
	gated.record(FlowUi::detail::InitialCapacityTarget::input_fields_registry, 112);
	memory.setConfig(memory.config());
	assert(gated.consume(FlowUi::detail::InitialCapacityTarget::input_fields_registry, 0) == 112);
	gated.record(FlowUi::detail::InitialCapacityTarget::input_fields_registry, 112);
	auto disabled = memory.config();
	disabled.level = MemoryMonitoringLevel::Disabled;
	memory.setConfig(disabled);
	gated.record(FlowUi::detail::InitialCapacityTarget::input_fields_registry, 512);
	disabled.level = MemoryMonitoringLevel::SubsystemCapacity;
	memory.setConfig(disabled);
	assert(gated.consume(FlowUi::detail::InitialCapacityTarget::input_fields_registry, 0) == 0);
}
FlowUi::detail::MemoryCapacityManifest selection() {
	FlowUi::detail::MemoryCapacityManifest result;
	result.targets.emplace_back(FlowUi::detail::MemoryCapacitySelection{
		.key = "flowui.tuning.input_fields.registry_entries",
		.value = 128,
		.observed_maximum = 112,
		.headroom = 16});
	return result;
}
void check_manifest_and_pipeline(const std::filesystem::path& directory) {
	std::filesystem::create_directories(directory);
	const auto active = directory / "active.flowchanges", review = directory / "review.flowchanges";
	const std::string initial =
		R"({"manifestVersion":1,"schemaFingerprint":"0x42","bakedChanges":[{"targetKind":"Element","targetScope":"Definition","definitionId":"0x1","fieldId":"0x2","fieldPath":"width","valueTypeId":"0x3","ownerCppType":"FlowUi::WindowConfig","sourceHeader":"FlowUi/PublicStructs.hpp","cppValue":"9","valueJson":"9"}]})";
	{
		std::ofstream output(active);
		output << initial;
	}
	DevTooling tooling;
	auto& pipeline = tooling.bakePipeline();
	pipeline.setManifestPath(active);
	auto section = selection();
	section.growth_factor = 2.f;
	section.allow_runtime_growth = false;
	section.targets.front().evidence.source_name = "capture \"label\"\nrange";
	assert(pipeline.save_memory_capacities(section).applied); // no published schema
	assert(pipeline.manifest().manifestVersion == 2 && pipeline.manifest().entries.size() == 1);
	flowui::bake_tool::Manifest parsed;
	std::string error;
	assert(flowui::bake_tool::parseManifest(active, parsed, error));
	assert(parsed.memoryCapacity && parsed.memoryCapacity->targets.size() == 1);
	assert(parsed.memoryCapacity->targets.front().evidence.source_name ==
		   section.targets.front().evidence.source_name);
	section.targets.front().value = 256;
	assert(pipeline.save_memory_capacities(section, {}, review).applied);
	assert(pipeline.manifest().memoryCapacity->targets.front().value == 128);
	assert(flowui::bake_tool::parseManifest(review, parsed, error) &&
		   parsed.memoryCapacity->targets.front().value == 256);
	section.targets.front().key = "flowui.tuning.popups.stack_entries";
	assert(pipeline.save_memory_capacities(section).applied);
	assert(pipeline.manifest().memoryCapacity->targets.size() == 2);
	assert(flowui::bake_tool::parseManifest(active, parsed, error));
	assert(flowui::bake_tool::generateCpp(parsed, directory / "generated", error));
	const auto generated = directory / "generated" / "FlowUiBakedChanges.cpp";
	const auto timestamp = std::filesystem::last_write_time(generated);
	assert(flowui::bake_tool::generateCpp(parsed, directory / "generated", error));
	assert(std::filesystem::last_write_time(generated) == timestamp);
	const std::array<std::string_view, 1> removed{"flowui.tuning.popups.stack_entries"};
	FlowUi::detail::MemoryCapacityManifest empty;
	assert(pipeline.save_memory_capacities(empty, removed).applied);
	assert(pipeline.manifest().memoryCapacity->targets.size() == 1);
	const auto old = pipeline.manifest().memoryCapacity->targets.front().value;
	const auto read_file = [&] {
		std::ifstream input(active);
		return std::string((std::istreambuf_iterator<char>(input)), {});
	};
	const auto old_contents = read_file();
	auto blocked_temporary = active;
	blocked_temporary += ".tmp";
	std::filesystem::create_directory(blocked_temporary);
	assert(!pipeline.save_memory_capacities(empty).applied);
	assert(read_file() == old_contents &&
		   pipeline.manifest().memoryCapacity->targets.front().value == old);
	std::filesystem::remove(blocked_temporary);
	section.targets.front().key = "unknown";
	assert(!pipeline.save_memory_capacities(section).applied);
	assert(pipeline.manifest().memoryCapacity->targets.front().value == old);
	// Rename failure preserves the previous destination and the active manifest.
	std::filesystem::create_directories(directory / "occupied");
	{
		std::ofstream output(directory / "occupied" / "keep");
		output << "old";
	}
	assert(!pipeline.save_memory_capacities(empty, {}, directory / "occupied").applied);
	assert(std::filesystem::exists(directory / "occupied" / "keep"));
	assert(pipeline.reset_memory_capacities().applied);
	assert(!pipeline.manifest().memoryCapacity && pipeline.manifest().entries.size() == 1);
	assert(flowui::bake_tool::parseManifest(active, parsed, error) && !parsed.memoryCapacity);
	// Invalid values and previews are rejected by the same validator in runtime and generator.
	section = selection();
	section.targets.emplace_back(section.targets.front());
	assert(!FlowUi::detail::validate_memory_capacity_manifest(section, error));
	section = selection();
	section.targets.front().evidence.kind = "synthetic";
	assert(!FlowUi::detail::validate_memory_capacity_manifest(section, error));
	section = selection();
	section.targets.front().evidence.complete = false;
	assert(!FlowUi::detail::validate_memory_capacity_manifest(section, error));
	section.targets.front().evidence.acknowledge_incomplete = true;
	assert(FlowUi::detail::validate_memory_capacity_manifest(section, error));
	section.targets.front().observed_maximum = UINT64_MAX;
	section.targets.front().headroom = 1;
	assert(!FlowUi::detail::validate_memory_capacity_manifest(section, error));
	uint64_t decimal = 0;
	assert(FlowUi::detail::parse_capacity_unsigned("18446744073709551615", decimal) &&
		   decimal == UINT64_MAX);
	assert(!FlowUi::detail::parse_capacity_unsigned("18446744073709551616", decimal));
	assert(!FlowUi::detail::parse_capacity_unsigned("-1", decimal));
	assert(!FlowUi::detail::parse_capacity_unsigned("1e2", decimal));
	{
		std::ofstream output(active);
		output << R"({"manifestVersion":2,"manifestVersion":1,"bakedChanges":[]})";
	}
	assert(!flowui::bake_tool::parseManifest(active, parsed, error));
}
void check_staged_export() {
	using namespace interface_elements;
	MemoryAnalysisState state;
	const auto targets = memory_sources::tuningTargets();
	const auto id = makeMemoryCapacityTargetId("flowui.tuning.input_fields.registry_entries");
	const auto found = std::find_if(targets.begin(), targets.end(),
									[&](const auto& target) { return target.id == id; });
	assert(found != targets.end());
	state.evidence = {{112, 1, 1, 7, 0}};
	state.plan = *replay_memory_plan(state.evidence, *found, 128, 1.5);
	assert(stage_memory_setting(state, *found));
	assert(!export_staged_memory_capacities(state));
	auto exported = export_staged_memory_capacities(state, true);
	assert(exported && exported->targets.front().value == 128 && !exported->growth_factor);
	state.staged.front().synthetic = true;
	assert(!export_staged_memory_capacities(state, true));
}
} // namespace
int main(int argument_count, char** arguments) {
	assert(argument_count == 2);
	check_resolution();
	check_demand_and_growth();
	check_manifest_and_pipeline(arguments[1]);
	check_staged_export();
	std::cout << "Capacity resolution, interval demand, reserve outcome, persistence and "
				 "generation passed.\n";
}
