#pragma once

#include "devSystems/devMonitoringAndReporting/memory/DevMemoryTypes.hpp"

#if FLOW_UI_DEV_MODE

#include <utility>

namespace FlowUi::devSystems::memory_sources {

inline constexpr auto kStorageCpu = makeMemorySourceDescriptor(
	"flowui.memory.storage.cpu", MemoryDomain::StorageCpu, MemorySourceKind::Allocator);
inline constexpr auto kStorageMetadata = makeMemorySourceDescriptor(
	"flowui.memory.storage.metadata", MemoryDomain::StorageCpu, MemorySourceKind::Container,
	MemoryAccuracy::Estimate, kStorageCpu.id);
inline constexpr auto kStorageTransient = makeMemorySourceDescriptor(
	"flowui.memory.storage.transient", MemoryDomain::TransientCpu, MemorySourceKind::Arena,
	MemoryAccuracy::Exact, kStorageCpu.id);
inline constexpr auto kStorageGpu = makeMemorySourceDescriptor(
	"flowui.memory.storage.gpu", MemoryDomain::GpuAllocation, MemorySourceKind::Resource,
	MemoryAccuracy::AllocatorRequested);
inline constexpr auto kMonitoring = makeMemorySourceDescriptor(
	"flowui.memory.development.monitoring", MemoryDomain::ManagerCpu,
	MemorySourceKind::Development, MemoryAccuracy::AllocatorRequested);

#define FLOWUI_STORAGE_CLASS_SOURCE(Name, Stable, Parent, Target) \
	inline constexpr auto Name = makeMemorySourceDescriptor(Stable, MemoryDomain::StorageCpu, \
		MemorySourceKind::Allocator, MemoryAccuracy::Exact, Parent.id, makeMemorySourceId(Target))
FLOWUI_STORAGE_CLASS_SOURCE(kStoragePersistent, "flowui.memory.storage.cpu.persistent", kStorageCpu,
	"flowui.tuning.storage.initial_persistent_cpu_bytes");
FLOWUI_STORAGE_CLASS_SOURCE(kStorageStringPool, "flowui.memory.storage.cpu.string_pool", kStorageCpu,
	"flowui.tuning.storage.initial_string_bytes");
FLOWUI_STORAGE_CLASS_SOURCE(kStorageFrameTransient, "flowui.memory.storage.cpu.frame_transient", kStorageTransient,
	"flowui.tuning.storage.transient_bytes_per_frame_per_window");
FLOWUI_STORAGE_CLASS_SOURCE(kStorageWorkerTransient, "flowui.memory.storage.cpu.worker_transient", kStorageTransient,
	"flowui.tuning.storage.transient_bytes_per_worker");
FLOWUI_STORAGE_CLASS_SOURCE(kStorageDecodeTransient, "flowui.memory.storage.cpu.decode_transient", kStorageTransient,
	"flowui.tuning.storage.initial_decode_scratch_bytes");
FLOWUI_STORAGE_CLASS_SOURCE(kStorageUploadStaging, "flowui.memory.storage.cpu.upload_staging", kStorageTransient,
	"flowui.tuning.storage.initial_upload_staging_bytes");
#undef FLOWUI_STORAGE_CLASS_SOURCE

inline constexpr auto kManagers = makeMemorySourceDescriptor(
	"flowui.memory.managers", MemoryDomain::ManagerCpu, MemorySourceKind::Manager,
	MemoryAccuracy::Estimate);
inline constexpr auto kElements = makeMemorySourceDescriptor(
	"flowui.memory.managers.elements", MemoryDomain::ManagerCpu, MemorySourceKind::Manager,
	MemoryAccuracy::Estimate, kManagers.id, makeMemorySourceId("flowui.tuning.elements"));
inline constexpr auto kInputFields = makeMemorySourceDescriptor(
	"flowui.memory.managers.input_fields", MemoryDomain::ManagerCpu, MemorySourceKind::Manager,
	MemoryAccuracy::Estimate, kManagers.id, makeMemorySourceId("flowui.tuning.input_fields"));
inline constexpr auto kInputTextPayload = makeMemorySourceDescriptor(
	"flowui.memory.managers.input_fields.text_payload", MemoryDomain::ManagerCpu,
	MemorySourceKind::Container, MemoryAccuracy::AllocatorRequested, kInputFields.id,
	makeMemorySourceId("flowui.tuning.input_text_bytes"));
inline constexpr auto kFonts = makeMemorySourceDescriptor(
	"flowui.memory.managers.fonts", MemoryDomain::ManagerCpu, MemorySourceKind::Manager,
	MemoryAccuracy::Estimate, kManagers.id, makeMemorySourceId("flowui.tuning.fonts"));
inline constexpr auto kFontAtlasCpuPixels = makeMemorySourceDescriptor(
	"flowui.memory.managers.fonts.atlas_cpu_pixels", MemoryDomain::ManagerCpu,
	MemorySourceKind::Resource, MemoryAccuracy::AllocatorRequested, kFonts.id,
	makeMemorySourceId("flowui.tuning.font_atlas_layers"));
inline constexpr auto kIcons = makeMemorySourceDescriptor(
	"flowui.memory.managers.icons", MemoryDomain::ManagerCpu, MemorySourceKind::Manager,
	MemoryAccuracy::Estimate, kManagers.id, makeMemorySourceId("flowui.tuning.icons"));
inline constexpr auto kIconSvgDocuments = makeMemorySourceDescriptor(
	"flowui.memory.managers.icons.svg_documents", MemoryDomain::ManagerCpu,
	MemorySourceKind::Container, MemoryAccuracy::Estimate, kIcons.id,
	makeMemorySourceId("flowui.tuning.icon_documents"));
inline constexpr auto kIconAtlasMetadata = makeMemorySourceDescriptor(
	"flowui.memory.managers.icons.atlas_metadata", MemoryDomain::ManagerCpu,
	MemorySourceKind::Container, MemoryAccuracy::AllocatorRequested, kIcons.id,
	makeMemorySourceId("flowui.tuning.icon_atlas_metadata"));
inline constexpr auto kViewports = makeMemorySourceDescriptor(
	"flowui.memory.managers.viewports", MemoryDomain::ManagerCpu, MemorySourceKind::Manager,
	MemoryAccuracy::Estimate, kManagers.id, makeMemorySourceId("flowui.tuning.viewports"));
inline constexpr auto kPopups = makeMemorySourceDescriptor(
	"flowui.memory.managers.popups", MemoryDomain::ManagerCpu, MemorySourceKind::Manager,
	MemoryAccuracy::Estimate, kManagers.id, makeMemorySourceId("flowui.tuning.popups"));
inline constexpr auto kShortcuts = makeMemorySourceDescriptor(
	"flowui.memory.managers.shortcuts", MemoryDomain::ManagerCpu, MemorySourceKind::Manager,
	MemoryAccuracy::Estimate, kManagers.id, makeMemorySourceId("flowui.tuning.shortcuts"));
inline constexpr auto kActions = makeMemorySourceDescriptor(
	"flowui.memory.managers.actions", MemoryDomain::ManagerCpu, MemorySourceKind::Manager,
	MemoryAccuracy::Estimate, kManagers.id, makeMemorySourceId("flowui.tuning.actions"));
inline constexpr auto kThemes = makeMemorySourceDescriptor(
	"flowui.memory.managers.themes", MemoryDomain::ManagerCpu, MemorySourceKind::Manager,
	MemoryAccuracy::Estimate, kManagers.id, makeMemorySourceId("flowui.tuning.themes"));
inline constexpr auto kUiLayout = makeMemorySourceDescriptor(
	"flowui.memory.managers.ui_layout", MemoryDomain::ManagerCpu, MemorySourceKind::Manager,
	MemoryAccuracy::Estimate, kManagers.id, makeMemorySourceId("flowui.tuning.ui_layout"));
inline constexpr auto kRenderer = makeMemorySourceDescriptor(
	"flowui.memory.managers.renderer", MemoryDomain::ManagerCpu, MemorySourceKind::Manager,
	MemoryAccuracy::Estimate, kManagers.id, makeMemorySourceId("flowui.tuning.renderer"));
inline constexpr auto kRendererFramePayload = makeMemorySourceDescriptor(
	"flowui.memory.managers.renderer.frame_payload", MemoryDomain::ManagerCpu,
	MemorySourceKind::Container, MemoryAccuracy::Exact, kRenderer.id,
	makeMemorySourceId("flowui.tuning.storage.initial_instance_bytes_per_frame"));

inline constexpr auto kTemporaries = makeMemorySourceDescriptor(
	"flowui.memory.temporaries", MemoryDomain::TransientCpu, MemorySourceKind::Arena,
	MemoryAccuracy::AllocatorRequested);
inline constexpr auto kImageDecode = makeMemorySourceDescriptor(
	"flowui.memory.temporaries.image_decode_rgba", MemoryDomain::TransientCpu,
	MemorySourceKind::Resource, MemoryAccuracy::Exact, kTemporaries.id);
inline constexpr auto kIconRaster = makeMemorySourceDescriptor(
	"flowui.memory.temporaries.icon_raster", MemoryDomain::TransientCpu,
	MemorySourceKind::Resource, MemoryAccuracy::Exact, kTemporaries.id);
inline constexpr auto kIconTightUpload = makeMemorySourceDescriptor(
	"flowui.memory.temporaries.icon_tight_upload", MemoryDomain::TransientCpu,
	MemorySourceKind::Resource, MemoryAccuracy::AllocatorRequested, kTemporaries.id);
inline constexpr auto kFontDecode = makeMemorySourceDescriptor(
	"flowui.memory.temporaries.font_decode", MemoryDomain::TransientCpu,
	MemorySourceKind::Resource, MemoryAccuracy::Exact, kTemporaries.id);
inline constexpr auto kFontAtlasCombine = makeMemorySourceDescriptor(
	"flowui.memory.temporaries.font_atlas_combine", MemoryDomain::TransientCpu,
	MemorySourceKind::Resource, MemoryAccuracy::AllocatorRequested, kTemporaries.id);

inline constexpr auto kProcess = makeMemorySourceDescriptor(
	"flowui.memory.process", MemoryDomain::Process, MemorySourceKind::Process,
	MemoryAccuracy::Estimate);
inline constexpr auto kVulkanHeaps = makeMemorySourceDescriptor(
	"flowui.memory.vulkan.heaps", MemoryDomain::VulkanHeap, MemorySourceKind::GpuHeap,
	MemoryAccuracy::AllocatorRequested);

inline constexpr StaticMemorySourceDescriptor kAll[] = {
	kStorageCpu, kStorageMetadata, kStorageTransient, kStorageGpu, kMonitoring,
	kStoragePersistent, kStorageStringPool, kStorageFrameTransient, kStorageWorkerTransient,
	kStorageDecodeTransient, kStorageUploadStaging,
	kManagers, kElements, kInputFields, kInputTextPayload, kFonts, kFontAtlasCpuPixels,
	kIcons, kIconSvgDocuments, kIconAtlasMetadata, kViewports, kPopups,
	kShortcuts, kActions, kThemes, kUiLayout, kRenderer, kTemporaries,
	kRendererFramePayload,
	kImageDecode, kIconRaster, kIconTightUpload, kFontDecode, kFontAtlasCombine,
	kProcess, kVulkanHeaps,
};

inline std::vector<MemoryTuningTargetDescriptor> tuningTargets() {
	using Metric = MemoryTuningMetric;
	using Unit = MemoryCapacityUnit;
	using Policy = MemoryApplyPolicy;
	std::vector<MemoryTuningTargetDescriptor> result{
		{makeMemorySourceId("flowui.tuning.storage.initial_persistent_cpu_bytes"), kStoragePersistent.id,
			Metric::PeakLogicalBytes, Unit::Bytes, 64u, UINT64_MAX, 64u, 4ull * 1024ull * 1024ull,
			"storage.initialPersistentCpuBytes", Policy::RestartRequired},
		{makeMemorySourceId("flowui.tuning.storage.initial_string_bytes"), kStorageStringPool.id,
			Metric::PeakLogicalBytes, Unit::Bytes, 64u, UINT64_MAX, 64u, 1ull * 1024ull * 1024ull,
			"storage.initialStringBytes", Policy::RestartRequired},
		{makeMemorySourceId("flowui.tuning.storage.transient_bytes_per_frame_per_window"), kStorageFrameTransient.id,
			Metric::PeakLogicalBytes, Unit::Bytes, 64u, UINT64_MAX, 64u, 1ull * 1024ull * 1024ull,
			"storage.transientBytesPerFramePerWindow", Policy::RestartRequired},
		{makeMemorySourceId("flowui.tuning.storage.transient_bytes_per_worker"), kStorageWorkerTransient.id,
			Metric::PeakLogicalBytes, Unit::Bytes, 64u, UINT64_MAX, 64u, 1ull * 1024ull * 1024ull,
			"storage.transientBytesPerWorker", Policy::RestartRequired},
		{makeMemorySourceId("flowui.tuning.storage.initial_decode_scratch_bytes"), kStorageDecodeTransient.id,
			Metric::PeakLogicalBytes, Unit::Bytes, 64u, UINT64_MAX, 64u, 8ull * 1024ull * 1024ull,
			"storage.initialDecodeScratchBytes", Policy::RestartRequired},
		{makeMemorySourceId("flowui.tuning.storage.initial_upload_staging_bytes"), kStorageUploadStaging.id,
			Metric::PeakLogicalBytes, Unit::Bytes, 64u, UINT64_MAX, 64u, 16ull * 1024ull * 1024ull,
			"storage.initialUploadStagingBytes", Policy::RestartRequired},
	};
	const auto addEntries = [&](const StaticMemorySourceDescriptor& source, std::string key,
		uint64_t defaultValue = 0u) {
		if (source.tuningTarget == 0u) return;
		result.push_back({source.tuningTarget, source.id, Metric::CapacityCount, Unit::Entries,
			0u, UINT64_MAX, 1u, defaultValue, std::move(key), Policy::RestartRequired});
	};
	addEntries(kElements, "managers.elementsReserve");
	addEntries(kInputFields, "managers.inputFieldsReserve");
	addEntries(kFonts, "managers.fontsReserve");
	addEntries(kIcons, "managers.iconsReserve");
	addEntries(kViewports, "managers.viewportsReserve");
	addEntries(kPopups, "managers.popupsReserve");
	addEntries(kShortcuts, "managers.shortcutsReserve");
	addEntries(kActions, "managers.actionsReserve");
	addEntries(kThemes, "managers.themesReserve");
	addEntries(kUiLayout, "managers.uiLayoutReserve");
	addEntries(kRenderer, "managers.rendererReserve");
	result.push_back({kInputTextPayload.tuningTarget, kInputTextPayload.id,
		Metric::BackingAllocatedBytes, Unit::Bytes, 0u, UINT64_MAX, 1u, 0u,
		"managers.inputTextBytesReserve", Policy::RestartRequired});
	result.push_back({kFontAtlasCpuPixels.tuningTarget, kFontAtlasCpuPixels.id,
		Metric::BackingAllocatedBytes, Unit::Bytes, 0u, UINT64_MAX, 1u, 0u,
		"managers.fontAtlasCpuPixelBytesReserve", Policy::RebuildRequired});
	result.push_back({kRendererFramePayload.tuningTarget, kRendererFramePayload.id,
		Metric::LogicalLiveBytes, Unit::Bytes, 256u, UINT64_MAX, 256u, 1024ull * 1024ull,
		"storage.initialInstanceBytesPerFrame", Policy::RestartRequired});
	addEntries(kIconSvgDocuments, "managers.iconDocumentsReserve");
	addEntries(kIconAtlasMetadata, "managers.iconAtlasMetadataReserve");
	return result;
}

} // namespace FlowUi::devSystems::memory_sources

#endif
