#include "HeadlessVulkanFixture.hpp"
#include "TestHarness.hpp"

#include <algorithm>
#include <cstddef>
#include <iostream>

#include "internal/StorageSystem/FlowStorageSystem.hpp"
#include "devSystems/devMonitoringAndReporting/memory/DevEnvironmentMemoryProbe.hpp"

namespace {

namespace storage = FlowUi::detail::storage;

storage::StorageConfig config() {
	storage::StorageConfig result{};
	result.initialPersistentCpuBytes = 1024u;
	result.initialStringBytes = 256u;
	result.initialDecodeScratchBytes = 128u;
	result.initialUploadStagingBytes = 192u;
	result.transientBytesPerFramePerWindow = 256u;
	result.transientBytesPerWorker = 96u;
	result.cpuSoftBudgetBytes = 1u << 20u;
	result.gpuSoftBudgetBytes = 1u << 20u;
	result.expectedBindingsPerWindow = 8u;
	return result;
}

void testConsolidatedStorageSnapshot(FlowUi::test::HeadlessVulkanFixture& vulkan) {
	storage::FlowStorageSystem system(vulkan.context());
	system.initialize(config());
	system.registerWindow(7u, storage::WindowStorageDesc{
		.framesInFlight = 1u,
		.workerCount = 1u,
		.initialTextureBindings = 8u,
		.maxTextureBindings = 16u,
		.transientBytesPerFrame = 256u,
		.transientBytesPerWorker = 96u,
	});
	const storage::MemoryBlock persistent = system.allocatePersistent(
		40u, alignof(std::max_align_t), storage::MemoryClass::Persistent, 0u);
	const storage::FrameToken frame = system.beginFrame(
		7u, storage::FrameStorageDesc{.frameSlot = 0u, .frameNumber = 1u});
	FLOWUI_CHECK(system.frameArena(frame, storage::MemoryClass::FrameTransient).allocate(24u) != nullptr);
	FLOWUI_CHECK(system.frameArena(frame, storage::MemoryClass::DecodeTransient).allocate(16u) != nullptr);
	FLOWUI_CHECK(system.frameArena(frame, storage::MemoryClass::UploadStaging).allocate(12u) != nullptr);
	FLOWUI_CHECK(system.workerArena(frame, 0u).allocate(8u) != nullptr);

	storage::StorageMemorySnapshot snapshot{};
	system.appendMemorySnapshot(storage::StorageMemorySnapshotRequest{
		.detail = storage::StorageMemoryDetail::IndividualResources,
	}, snapshot);
	FLOWUI_CHECK(snapshot.mutationSequence != 0u);
	FLOWUI_CHECK(snapshot.includesResourceKinds);
	FLOWUI_CHECK(snapshot.includesWindows);
	FLOWUI_CHECK(snapshot.includesIndividualResources);
	FLOWUI_CHECK(snapshot.resourceMetadataBytes >= snapshot.resourceMetadataLiveBytes);
	FLOWUI_CHECK(snapshot.windows.size() == 1u);
	FLOWUI_CHECK(snapshot.totals.cpu[static_cast<size_t>(storage::MemoryClass::Persistent)].liveBytes >= 40u);
	FLOWUI_CHECK(snapshot.totals.cpu[static_cast<size_t>(storage::MemoryClass::FrameTransient)].liveBytes >= 24u);
	FLOWUI_CHECK(snapshot.totals.cpu[static_cast<size_t>(storage::MemoryClass::DecodeTransient)].liveBytes >= 16u);
	FLOWUI_CHECK(snapshot.totals.cpu[static_cast<size_t>(storage::MemoryClass::UploadStaging)].liveBytes >= 12u);
	FLOWUI_CHECK(std::any_of(snapshot.allocators.begin(), snapshot.allocators.end(), [](const auto& allocator) {
		return allocator.memoryClass == storage::MemoryClass::UploadStaging &&
			allocator.liveBytes >= 12u && allocator.reservedBytes >= 192u;
	}));

	const uint64_t firstSequence = snapshot.mutationSequence;
	system.appendMemorySnapshot(storage::StorageMemorySnapshotRequest{}, snapshot);
	FLOWUI_CHECK(snapshot.mutationSequence == firstSequence);
	system.cancelFrame(frame);
	system.releasePersistent(persistent);
	system.appendMemorySnapshot(storage::StorageMemorySnapshotRequest{}, snapshot);
	FLOWUI_CHECK(snapshot.mutationSequence > firstSequence);
	system.unregisterWindow(7u, 0u);
	system.shutdown();
}

void testGpuHeapSnapshot(FlowUi::test::HeadlessVulkanFixture& vulkan) {
	FlowUi::devSystems::DevEnvironmentMemoryProbe probe;
	probe.setConfig(FlowUi::devSystems::DevMemoryConfig{
		.level = FlowUi::devSystems::MemoryMonitoringLevel::SubsystemCapacity,
		.gpuMemory = true,
		.processMemory = false,
		.detailedVmaStatistics = true,
	});
	probe.initializeVulkan(vulkan.context());
	probe.advanceVmaFrameIndex(3u);
	const auto snapshot = probe.sample(1u, 0u, true);
	FLOWUI_CHECK(snapshot.gpu.available);
	FLOWUI_CHECK(!snapshot.gpu.heaps.empty());
	FLOWUI_CHECK(snapshot.gpu.detailedStatisticsIncluded);
	FLOWUI_CHECK(snapshot.gpu.sampleSequence == 1u);
	for (const auto& heap : snapshot.gpu.heaps) {
		FLOWUI_CHECK(heap.allocationBytes <= heap.blockBytes);
		FLOWUI_CHECK(heap.heapSizeBytes != 0u);
		if (!snapshot.gpu.memoryBudgetExtensionEnabled) {
			FLOWUI_CHECK((static_cast<uint16_t>(heap.flags) &
				static_cast<uint16_t>(FlowUi::devSystems::MemorySampleFlag::Estimate)) != 0u);
		}
	}
	probe.detachVulkan();
}

} // namespace

int main() {
	try {
		FlowUi::test::HeadlessVulkanFixture vulkan;
		FlowUi::test::Runner runner;
		runner.run("consolidated storage memory snapshot", [&] {
			testConsolidatedStorageSnapshot(vulkan);
		});
		runner.run("gpu heap memory snapshot", [&] {
			testGpuHeapSnapshot(vulkan);
		});
		return runner.finish();
	} catch (const FlowUi::test::VulkanUnavailable& error) {
#ifdef FLOWUI_TEST_REQUIRE_VULKAN_DEVICE
		std::cerr << "FAIL: " << error.what() << '\n';
		return 1;
#else
		std::cout << "SKIP: " << error.what() << '\n';
		return 77;
#endif
	}
}
