#include "TestHarness.hpp"

#include <algorithm>
#include <atomic>
#include <cstdint>
#include <chrono>
#include <thread>
#include <vector>

#include "devSystems/devMonitoringAndReporting/memory/DevExternalMemoryScope.hpp"
#include "devSystems/devMonitoringAndReporting/memory/DevMemory.hpp"
#include "devSystems/devMonitoringAndReporting/memory/DevMemorySources.hpp"
#include "devSystems/devMonitoringAndReporting/reporting/DevMemoryReporting.hpp"

namespace {

using namespace FlowUi::devSystems;

#if FLOWUI_DEV_MEMORY_LEVEL == 0
void compileRemovalContract() {
	FLOWUI_DEV_MEMORY_STORAGE(undefinedRecorder(), undefinedRecord());
	FLOWUI_DEV_MEMORY_SUBSYSTEM(undefinedRecorder(), undefinedRecord());
	FLOWUI_DEV_MEMORY_LIFETIME(undefinedRecorder(), undefinedRecord());
	FLOWUI_DEV_MEMORY_DEEP(undefinedRecorder(), undefinedRecord());
}
#endif

void testBoundedRecorderAndRuntimeLevel() {
	DevMemory memory(DevMemoryConfig{
		.level = MemoryMonitoringLevel::StorageSummary,
		.producerEventCapacity = 2u,
	});
	MemoryOperationRecord summary{
		.source = makeMemorySourceId("flowui.memory.storage.cpu"),
		.operation = MemoryOperation::LogicalAllocate,
		.detailLevel = MemoryMonitoringLevel::StorageSummary,
		.bytesChanged = 16u,
	};
#if FLOWUI_DEV_MEMORY_LEVEL == 0
	FLOWUI_CHECK(!memory.recorder().tryRecord(summary));
	DevMemoryReporting reporting(memory);
	reporting.consume(7u);
	const MemoryReportingStatus status = reporting.status();
	FLOWUI_CHECK(status.runtimeLevel == MemoryMonitoringLevel::Disabled);
	FLOWUI_CHECK(status.consumedOperations == 0u);
	FLOWUI_CHECK(status.segments.capacity == 1u);
	FLOWUI_CHECK(status.events.capacity == 1u);
	return;
#else
	FLOWUI_CHECK(memory.recorder().tryRecord(summary));
	FLOWUI_CHECK(memory.recorder().tryRecord(summary));
	FLOWUI_CHECK(!memory.recorder().tryRecord(summary));

	MemoryOperationRecord detailed = summary;
	detailed.detailLevel = MemoryMonitoringLevel::DetailedLifetimes;
	FLOWUI_CHECK(!memory.recorder().tryRecord(detailed));
	const MemoryQualitySnapshot quality = memory.qualitySnapshot();
	FLOWUI_CHECK(quality.recordedOperations == 2u);
	FLOWUI_CHECK(quality.droppedOperations == 1u);
	FLOWUI_CHECK(quality.suppressedOperations == 1u);

	DevMemoryReporting reporting(memory, MemoryReportingConfig{.managerSampleEveryTicks = 1u});
	reporting.consume(7u);
	const MemoryReportingStatus status = reporting.status();
	FLOWUI_CHECK(status.consumedOperations == 2u);
	FLOWUI_CHECK(status.lastConsumedAppTick == 7u);
	FLOWUI_CHECK(status.runtimeLevel == MemoryMonitoringLevel::StorageSummary);
	FLOWUI_CHECK(!status.hasStorageSnapshot);
#endif
}

struct TraceProbeOwner {
	MemorySourceId source = 0u;
	uint64_t value = 0u;
};

void sampleTraceProbe(const void* owner, MemoryProbeContext& context) noexcept {
	const auto& probe = *static_cast<const TraceProbeOwner*>(owner);
	(void)context.sink.append(MemoryValueSample{
		.source = probe.source,
		.logicalLiveBytes = probe.value,
		.backingAllocatedBytes = probe.value,
		.peakLogicalBytes = probe.value,
		.capacityCount = probe.value,
	});
}

void testCoalescedWeightedHistoryAndOverwriteContract() {
#if FLOWUI_DEV_MEMORY_LEVEL >= 2
	DevMemory memory(DevMemoryConfig{.level = MemoryMonitoringLevel::SubsystemCapacity,
		.gpuMemory = false, .processMemory = false});
	constexpr auto source = makeMemorySourceDescriptor(
		"flowui.memory.test.trace", MemoryDomain::ManagerCpu, MemorySourceKind::Container);
	FLOWUI_CHECK(memory.registerSource(source));
	TraceProbeOwner owner{source.id, 10u};
	FLOWUI_CHECK(memory.registerProbe({source.id, &owner, &sampleTraceProbe}));
	DevMemoryReporting reporting(memory, MemoryReportingConfig{
		.segmentCapacity = 16u, .eventByteCapacity = 1u,
		.managerSampleEveryTicks = 1u, .quantileWindowSegments = 16u});
	for (uint64_t tick = 1u; tick <= 9u; ++tick) reporting.consume(tick);
	owner.value = 100u;
	reporting.consume(10u);
	auto segments = reporting.segmentSnapshot();
	uint32_t traceSegments = 0u;
	for (const auto& segment : segments) if (segment.value.source == source.id) ++traceSegments;
	FLOWUI_CHECK(traceSegments == 2u);
	auto statistics = reporting.statistics(MemoryStatisticsQuery{
		.source = source.id, .percentiles = {0.90, 0.95}});
	FLOWUI_CHECK(statistics.has_value());
	FLOWUI_CHECK(statistics->totalWeight == 10u);
	FLOWUI_CHECK(statistics->percentiles[0].value == 10u);
	FLOWUI_CHECK(statistics->percentiles[1].value == 100u);

	reporting.setConfig(MemoryReportingConfig{.segmentCapacity = 2u, .managerSampleEveryTicks = 1u});
	owner.value = 200u; reporting.consume(11u);
	owner.value = 300u; reporting.consume(12u);
	owner.value = 400u; reporting.consume(13u);
	const auto status = reporting.status();
	FLOWUI_CHECK(status.segments.overwriteCount != 0u);
	FLOWUI_CHECK(status.segments.oldestRetainedSequence > 1u);
	const auto current = reporting.currentSourceSnapshot();
	auto found = std::find_if(current.begin(), current.end(), [&](const auto& value) {
		return value.key.source == source.id;
	});
	FLOWUI_CHECK(found != current.end());
	FLOWUI_CHECK(found->sessionPeak.logicalLiveBytes == 400u);
#endif
}

void testCaptureProfileAndProductionGrowthReplay() {
#if FLOWUI_DEV_MEMORY_LEVEL >= 2
	DevMemory memory(DevMemoryConfig{.level = MemoryMonitoringLevel::SubsystemCapacity,
		.gpuMemory = false, .processMemory = false});
	constexpr auto source = makeMemorySourceDescriptor(
		"flowui.memory.test.capacity", MemoryDomain::ManagerCpu, MemorySourceKind::Container);
	constexpr MemoryTuningTargetId targetId = makeMemorySourceId("flowui.tuning.test.capacity");
	FLOWUI_CHECK(memory.registerSource(source));
	FLOWUI_CHECK(memory.registerTuningTarget(MemoryTuningTargetDescriptor{
		.id = targetId, .source = source.id, .metric = MemoryTuningMetric::LogicalLiveBytes,
		.unit = MemoryCapacityUnit::Bytes, .minimum = 16u, .alignment = 16u,
		.productionDefault = 32u, .configKey = "test.capacity",
		.applyPolicy = MemoryApplyPolicy::RestartRequired}));
	TraceProbeOwner owner{source.id, 1000u};
	FLOWUI_CHECK(memory.registerProbe({source.id, &owner, &sampleTraceProbe}));
	DevMemoryReporting reporting(memory, MemoryReportingConfig{
		.segmentCapacity = 64u, .eventByteCapacity = 2u * sizeof(RetainedMemoryEvent),
		.managerSampleEveryTicks = 1u, .quantileWindowSegments = 64u,
		.retainLifetimeEvents = true});
	const MemoryCaptureId capture = reporting.beginCapture("representative", 1u);
	reporting.consume(1u); // excluded warm-up spike
	owner.value = 64u;
	for (uint64_t tick = 2u; tick <= 20u; ++tick) reporting.consume(tick);
	owner.value = 1024u;
	reporting.consume(21u);
	FLOWUI_CHECK(reporting.endCapture(capture));
	const auto preview = reporting.previewCapacityProfile(CapacityProfileRequest{
		.capture = capture, .percentile = 0.95, .growthFactor = 1.5f});
	auto recommendation = std::find_if(preview.recommendations.begin(), preview.recommendations.end(),
		[&](const auto& value) { return value.target.id == targetId; });
	FLOWUI_CHECK(recommendation != preview.recommendations.end());
	FLOWUI_CHECK(recommendation->percentileDemand == 64u);
	FLOWUI_CHECK(recommendation->proposedInitialCapacity == 64u);
	FLOWUI_CHECK(recommendation->observedMaximum == 1024u);
	FLOWUI_CHECK(recommendation->simulation.finalCapacity >= 1024u);
	const uint64_t replay[] = {64u, 64u, 1024u};
	const auto replayed = FlowUi::simulateMemoryGrowth(
		replay, recommendation->proposedInitialCapacity, preview.profile.growthFactor,
		recommendation->target.alignment);
	FLOWUI_CHECK(replayed.finalCapacity == recommendation->simulation.finalCapacity);
	FLOWUI_CHECK(preview.profile.metadata.complete);
#endif
}

void testEventRingAndMultithreadedProducerDrain() {
#if FLOWUI_DEV_MEMORY_LEVEL >= 3
	DevMemory memory(DevMemoryConfig{.level = MemoryMonitoringLevel::DetailedLifetimes,
		.producerEventCapacity = 1024u, .gpuMemory = false, .processMemory = false});
	DevMemoryReporting reporting(memory, MemoryReportingConfig{
		.segmentCapacity = 8u, .eventByteCapacity = 2u * sizeof(RetainedMemoryEvent),
		.managerSampleEveryTicks = 1u, .retainLifetimeEvents = true});
	constexpr uint32_t threadCount = 4u;
	constexpr uint32_t recordsPerThread = 100u;
	std::atomic<bool> allRecorded{true};
	std::vector<std::thread> threads;
	for (uint32_t thread = 0u; thread < threadCount; ++thread) threads.emplace_back([&, thread] {
		for (uint32_t index = 0u; index < recordsPerThread; ++index) {
			if (!memory.recorder().tryRecord(MemoryOperationRecord{
				.source = memory_sources::kImageDecode.id,
				.operation = MemoryOperation::ExternalAcquire,
				.detailLevel = MemoryMonitoringLevel::DetailedLifetimes,
				.bytesChanged = 64u, .threadTrack = thread})) allRecorded.store(false);
		}
	});
	for (auto& thread : threads) thread.join();
	FLOWUI_CHECK(allRecorded.load());
	reporting.consume(1u);
	const auto status = reporting.status();
	FLOWUI_CHECK(status.consumedOperations == threadCount * recordsPerThread);
	FLOWUI_CHECK(status.events.retainedCount == 2u);
	FLOWUI_CHECK(status.events.overwriteCount == threadCount * recordsPerThread - 2u);
	FLOWUI_CHECK(reporting.eventSnapshot().front().sequence == status.events.oldestRetainedSequence);
	FLOWUI_CHECK(status.overhead.retainedCapacityBytes != 0u);
#endif
}

void testTypedStorageCapacityApplication() {
	FlowUi::detail::storage::StorageConfig config{};
	FlowUi::MemoryCapacityProfile profile{
		.settings = {
			{FlowUi::makeMemoryCapacityTargetId("flowui.tuning.storage.initial_persistent_cpu_bytes"), 8192u},
			{FlowUi::makeMemoryCapacityTargetId("flowui.tuning.storage.initial_string_bytes"), 4096u},
		},
		.growthFactor = 1.25f,
		.allowRuntimeGrowth = false,
	};
	FLOWUI_CHECK(FlowUi::detail::storage::applyMemoryCapacityProfile(config, profile) == 2u);
	FLOWUI_CHECK(config.initialPersistentCpuBytes == 8192u);
	FLOWUI_CHECK(config.initialStringBytes == 4096u);
	FLOWUI_CHECK(config.growthFactor == 1.25f);
	FLOWUI_CHECK(!config.allowRuntimeGrowth);
}

void testStableSourceRegistryAndCollisionDetection() {
	DevMemory memory;
	constexpr auto source = makeMemorySourceDescriptor(
		"flowui.memory.test.source",
		MemoryDomain::ManagerCpu,
		MemorySourceKind::Container);
	FLOWUI_CHECK(memory.registerSource(source));
	FLOWUI_CHECK(memory.registerSource(source));

	auto collision = source;
	collision.kind = MemorySourceKind::Arena;
	FLOWUI_CHECK(!memory.registerSource(collision));
	FLOWUI_CHECK(memory.qualitySnapshot().sourceCollisions == 1u);
	const auto descriptors = memory.descriptorSnapshot();
	FLOWUI_CHECK(descriptors.size() >= 6u); // five built-ins plus this source
}

struct ProbeOwner { uint64_t bytes = 0u; };

void sampleProbe(const void* owner, MemoryProbeContext& context) noexcept {
	const auto& probe = *static_cast<const ProbeOwner*>(owner);
	(void)context.sink.append(MemoryValueSample{
		.source = memory_sources::kThemes.id,
		.logicalLiveBytes = probe.bytes,
		.backingAllocatedBytes = probe.bytes,
		.flags = MemorySampleFlag::Estimate,
	});
}

void testManagerProbeAndTemporaryLifetime() {
#if FLOWUI_DEV_MEMORY_LEVEL >= 2
	DevMemory memory(DevMemoryConfig{.level = MemoryMonitoringLevel::SubsystemCapacity});
	ProbeOwner owner{.bytes = 4096u};
	FLOWUI_CHECK(memory.registerProbe({memory_sources::kThemes.id, &owner, &sampleProbe}));
	{
		DevExternalMemoryScope temporary(
			&memory.recorder(), memory_sources::kImageDecode.id, 2048u, 17u);
	}
	DevMemoryReporting reporting(memory, MemoryReportingConfig{.managerSampleEveryTicks = 1u});
	reporting.consume(11u);
	const auto samples = reporting.managerSamples();
	FLOWUI_CHECK(samples.size() == 1u);
	FLOWUI_CHECK(samples.front().logicalLiveBytes == 4096u);
	FLOWUI_CHECK(reporting.status().consumedOperations == 2u);
	owner.bytes = 8192u;
	reporting.consume(12u);
	const auto grown = reporting.managerSamples();
	FLOWUI_CHECK(grown.size() == 1u);
	FLOWUI_CHECK(grown.front().peakLogicalBytes == 8192u);
	FLOWUI_CHECK(grown.front().peakBackingAllocatedBytes == 8192u);
	FLOWUI_CHECK(grown.front().growthOps == 1u);
#endif
}

void testProcessProbeAvailability() {
#if FLOWUI_DEV_MEMORY_LEVEL >= 1
	DevMemory memory(DevMemoryConfig{
		.level = MemoryMonitoringLevel::SubsystemCapacity,
		.gpuMemory = false,
		.processMemory = true,
	});
	const uint64_t now = static_cast<uint64_t>(std::chrono::duration_cast<std::chrono::nanoseconds>(
		std::chrono::steady_clock::now().time_since_epoch()).count());
	const MemoryEnvironmentSnapshot snapshot = memory.sampleEnvironment(now, 0u, true);
#if defined(__linux__) || defined(_WIN32) || defined(__APPLE__)
	FLOWUI_CHECK(snapshot.process.availableFields != 0u);
	FLOWUI_CHECK(snapshot.process.accuracy == MemoryAccuracy::Estimate);
#endif
	FLOWUI_CHECK(!snapshot.gpu.available);
#endif
}

} // namespace

int main() {
	FlowUi::test::Runner runner;
	runner.run("bounded recorder and runtime level", testBoundedRecorderAndRuntimeLevel);
	runner.run("stable source registry and collision detection", testStableSourceRegistryAndCollisionDetection);
	runner.run("manager probe and temporary lifetime", testManagerProbeAndTemporaryLifetime);
	runner.run("process probe availability", testProcessProbeAvailability);
	runner.run("coalesced weighted history and overwrite contract", testCoalescedWeightedHistoryAndOverwriteContract);
	runner.run("capture profile and production growth replay", testCaptureProfileAndProductionGrowthReplay);
	runner.run("event ring and multithreaded producer drain", testEventRingAndMultithreadedProducerDrain);
	runner.run("typed storage capacity application", testTypedStorageCapacityApplication);
	return runner.finish();
}
