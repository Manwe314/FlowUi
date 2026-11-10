#pragma once

#include "devSystems/devMonitoringAndReporting/memory/DevMemoryTypes.hpp"

#if FLOW_UI_DEV_MODE

#include "FlowUi/Private/storageSystem/capacity/MemoryCapacityTargets.hpp"
#include <utility>

namespace FlowUi::devSystems::memory_sources {

/** Static source descriptor for CPU storage root. */
inline constexpr auto kStorageCpu = makeMemorySourceDescriptor(
	"flowui.memory.storage.cpu", MemoryDomain::StorageCpu, MemorySourceKind::Allocator);
/** Static source descriptor for CPU storage metadata containers. */
inline constexpr auto kStorageMetadata = makeMemorySourceDescriptor(
	"flowui.memory.storage.metadata", MemoryDomain::StorageCpu, MemorySourceKind::Container,
	MemoryAccuracy::Estimate, kStorageCpu.id);
/** Static source descriptor for transient CPU storage. */
inline constexpr auto kStorageTransient = makeMemorySourceDescriptor(
	"flowui.memory.storage.transient", MemoryDomain::TransientCpu, MemorySourceKind::Arena,
	MemoryAccuracy::Exact, kStorageCpu.id);
/** Static source descriptor for GPU allocations published by storage. */
inline constexpr auto kStorageGpu = makeMemorySourceDescriptor(
	"flowui.memory.storage.gpu", MemoryDomain::GpuAllocation, MemorySourceKind::Resource,
	MemoryAccuracy::AllocatorRequested);
/** Static source descriptor for developer instrumentation root. */
inline constexpr auto kDevelopment = makeMemorySourceDescriptor(
	"flowui.memory.development", MemoryDomain::ManagerCpu,
	MemorySourceKind::Development, MemoryAccuracy::Estimate);
/** Static source descriptor for monitoring overhead. */
inline constexpr auto kMonitoring = makeMemorySourceDescriptor(
	"flowui.memory.development.monitoring", MemoryDomain::ManagerCpu,
	MemorySourceKind::Development, MemoryAccuracy::AllocatorRequested, kDevelopment.id);
/** Static source descriptor for developer tooling overhead. */
inline constexpr auto kDevTooling = makeMemorySourceDescriptor(
	"flowui.memory.development.tooling", MemoryDomain::ManagerCpu,
	MemorySourceKind::Development, MemoryAccuracy::Estimate, kDevelopment.id);
/** Static source descriptor for memory workbench overhead. */
inline constexpr auto kMemoryWorkbench = makeMemorySourceDescriptor(
	"flowui.memory.development.memory_workbench", MemoryDomain::ManagerCpu,
	MemorySourceKind::Development, MemoryAccuracy::Estimate, kDevelopment.id);
/** Static source descriptor for developer element-tree capture. */
inline constexpr auto kDevTreeCapture = makeMemorySourceDescriptor(
	"flowui.memory.development.tooling.tree_capture", MemoryDomain::ManagerCpu,
	MemorySourceKind::Development, MemoryAccuracy::AllocatorRequested, kDevTooling.id);
#if FLOW_UI_AGENTIC_WORKFLOW
/** Static source descriptor for agentic workflow CPU overhead. */
inline constexpr auto kAgenticCpu = makeMemorySourceDescriptor(
	"flowui.memory.development.agentic.cpu", MemoryDomain::ManagerCpu,
	MemorySourceKind::Development, MemoryAccuracy::Estimate, kDevelopment.id);
/** Static source descriptor for agentic workflow GPU readback. */
inline constexpr auto kAgenticReadback = makeMemorySourceDescriptor(
	"flowui.memory.development.agentic.readback", MemoryDomain::GpuAllocation,
	MemorySourceKind::Development, MemoryAccuracy::AllocatorRequested, kDevelopment.id);
#endif

#define FLOWUI_STORAGE_CLASS_SOURCE(Name, Stable, Parent, Target) \
	inline constexpr auto Name = makeMemorySourceDescriptor(Stable, MemoryDomain::StorageCpu, \
		MemorySourceKind::Allocator, MemoryAccuracy::Exact, Parent.id, makeMemorySourceId(Target))
/** Static source descriptor for persistent CPU pool. */
FLOWUI_STORAGE_CLASS_SOURCE(kStoragePersistent, "flowui.memory.storage.cpu.persistent", kStorageCpu,
	"flowui.tuning.storage.initial_persistent_cpu_bytes");
/** Static source descriptor for interned string pool. */
FLOWUI_STORAGE_CLASS_SOURCE(kStorageStringPool, "flowui.memory.storage.cpu.string_pool", kStorageCpu,
	"flowui.tuning.storage.initial_string_bytes");
/** Static source descriptor for per-window frame arenas. */
FLOWUI_STORAGE_CLASS_SOURCE(kStorageFrameTransient, "flowui.memory.storage.cpu.frame_transient", kStorageTransient,
	"flowui.tuning.storage.transient_bytes_per_frame_per_window");
/** Static source descriptor for worker scratch arenas. */
FLOWUI_STORAGE_CLASS_SOURCE(kStorageWorkerTransient, "flowui.memory.storage.cpu.worker_transient", kStorageTransient,
	"flowui.tuning.storage.transient_bytes_per_worker");
/** Static source descriptor for decode scratch arenas. */
FLOWUI_STORAGE_CLASS_SOURCE(kStorageDecodeTransient, "flowui.memory.storage.cpu.decode_transient", kStorageTransient,
	"flowui.tuning.storage.initial_decode_scratch_bytes");
/** Static source descriptor for GPU upload staging. */
FLOWUI_STORAGE_CLASS_SOURCE(kStorageUploadStaging, "flowui.memory.storage.cpu.upload_staging", kStorageTransient,
	"flowui.tuning.storage.initial_upload_staging_bytes");
#undef FLOWUI_STORAGE_CLASS_SOURCE

/** Static source descriptor for manager CPU root. */
inline constexpr auto kManagers = makeMemorySourceDescriptor(
	"flowui.memory.managers", MemoryDomain::ManagerCpu, MemorySourceKind::Manager,
	MemoryAccuracy::Estimate);
/** Static source descriptor for element state/resource metadata. */
inline constexpr auto kElements = makeMemorySourceDescriptor(
	"flowui.memory.managers.elements", MemoryDomain::ManagerCpu, MemorySourceKind::Manager,
	MemoryAccuracy::Estimate, kManagers.id, makeMemorySourceId("flowui.tuning.elements"));
/** Static source descriptor for input-field manager backing. */
inline constexpr auto kInputFields = makeMemorySourceDescriptor(
	"flowui.memory.managers.input_fields", MemoryDomain::ManagerCpu, MemorySourceKind::Manager,
	MemoryAccuracy::Estimate, kManagers.id, makeMemorySourceId("flowui.tuning.input_fields"));
/** Static source descriptor for input text payload. */
inline constexpr auto kInputTextPayload = makeMemorySourceDescriptor(
	"flowui.memory.managers.input_fields.text_payload", MemoryDomain::ManagerCpu,
	MemorySourceKind::Container, MemoryAccuracy::AllocatorRequested, kInputFields.id,
	makeMemorySourceId("flowui.tuning.input_text_bytes"));
/** Static source descriptor for font-manager CPU backing. */
inline constexpr auto kFonts = makeMemorySourceDescriptor(
	"flowui.memory.managers.fonts", MemoryDomain::ManagerCpu, MemorySourceKind::Manager,
	MemoryAccuracy::Estimate, kManagers.id, makeMemorySourceId("flowui.tuning.fonts"));
/** Static source descriptor for font atlas CPU pixel copies. */
inline constexpr auto kFontAtlasCpuPixels = makeMemorySourceDescriptor(
	"flowui.memory.managers.fonts.atlas_cpu_pixels", MemoryDomain::ManagerCpu,
	MemorySourceKind::Resource, MemoryAccuracy::AllocatorRequested, kFonts.id,
	makeMemorySourceId("flowui.tuning.font_atlas_layers"));
/** Static source descriptor for icon-manager CPU backing. */
inline constexpr auto kIcons = makeMemorySourceDescriptor(
	"flowui.memory.managers.icons", MemoryDomain::ManagerCpu, MemorySourceKind::Manager,
	MemoryAccuracy::Estimate, kManagers.id, makeMemorySourceId("flowui.tuning.icons"));
/** Static source descriptor for retained SVG documents. */
inline constexpr auto kIconSvgDocuments = makeMemorySourceDescriptor(
	"flowui.memory.managers.icons.svg_documents", MemoryDomain::ManagerCpu,
	MemorySourceKind::Container, MemoryAccuracy::Estimate, kIcons.id,
	makeMemorySourceId("flowui.tuning.icon_documents"));
/** Static source descriptor for icon atlas metadata. */
inline constexpr auto kIconAtlasMetadata = makeMemorySourceDescriptor(
	"flowui.memory.managers.icons.atlas_metadata", MemoryDomain::ManagerCpu,
	MemorySourceKind::Container, MemoryAccuracy::AllocatorRequested, kIcons.id,
	makeMemorySourceId("flowui.tuning.icon_atlas_metadata"));
/** Static source descriptor for viewport manager metadata. */
inline constexpr auto kViewports = makeMemorySourceDescriptor(
	"flowui.memory.managers.viewports", MemoryDomain::ManagerCpu, MemorySourceKind::Manager,
	MemoryAccuracy::Estimate, kManagers.id, makeMemorySourceId("flowui.tuning.viewports"));
/** Static source descriptor for popup manager backing. */
inline constexpr auto kPopups = makeMemorySourceDescriptor(
	"flowui.memory.managers.popups", MemoryDomain::ManagerCpu, MemorySourceKind::Manager,
	MemoryAccuracy::Estimate, kManagers.id, makeMemorySourceId("flowui.tuning.popups"));
/** Static source descriptor for shortcut registrations. */
inline constexpr auto kShortcuts = makeMemorySourceDescriptor(
	"flowui.memory.managers.shortcuts", MemoryDomain::ManagerCpu, MemorySourceKind::Manager,
	MemoryAccuracy::Estimate, kManagers.id, makeMemorySourceId("flowui.tuning.shortcuts"));
/** Static source descriptor for action bindings. */
inline constexpr auto kActions = makeMemorySourceDescriptor(
	"flowui.memory.managers.actions", MemoryDomain::ManagerCpu, MemorySourceKind::Manager,
	MemoryAccuracy::Estimate, kManagers.id, makeMemorySourceId("flowui.tuning.actions"));
/** Static source descriptor for theme variants. */
inline constexpr auto kThemes = makeMemorySourceDescriptor(
	"flowui.memory.managers.themes", MemoryDomain::ManagerCpu, MemorySourceKind::Manager,
	MemoryAccuracy::Estimate, kManagers.id, makeMemorySourceId("flowui.tuning.themes"));
/** Static source descriptor for UI layout backing. */
inline constexpr auto kUiLayout = makeMemorySourceDescriptor(
	"flowui.memory.managers.ui_layout", MemoryDomain::ManagerCpu, MemorySourceKind::Manager,
	MemoryAccuracy::Estimate, kManagers.id, makeMemorySourceId("flowui.tuning.ui_layout"));
/** Static source descriptor for renderer CPU backing. */
inline constexpr auto kRenderer = makeMemorySourceDescriptor(
	"flowui.memory.managers.renderer", MemoryDomain::ManagerCpu, MemorySourceKind::Manager,
	MemoryAccuracy::Estimate, kManagers.id, makeMemorySourceId("flowui.tuning.renderer"));
/** Static source descriptor for per-frame renderer instance data. */
inline constexpr auto kRendererFramePayload = makeMemorySourceDescriptor(
	"flowui.memory.managers.renderer.frame_payload", MemoryDomain::ManagerCpu,
	MemorySourceKind::Container, MemoryAccuracy::Exact, kRenderer.id,
	makeMemorySourceId("flowui.tuning.storage.initial_instance_bytes_per_frame"));

/** Static source descriptor for temporary buffer root. */
inline constexpr auto kTemporaries = makeMemorySourceDescriptor(
	"flowui.memory.temporaries", MemoryDomain::TransientCpu, MemorySourceKind::Arena,
	MemoryAccuracy::AllocatorRequested);
/** Static source descriptor for decoded image RGBA buffers. */
inline constexpr auto kImageDecode = makeMemorySourceDescriptor(
	"flowui.memory.temporaries.image_decode_rgba", MemoryDomain::TransientCpu,
	MemorySourceKind::Resource, MemoryAccuracy::Exact, kTemporaries.id);
/** Static source descriptor for temporary SVG raster bytes. */
inline constexpr auto kIconRaster = makeMemorySourceDescriptor(
	"flowui.memory.temporaries.icon_raster", MemoryDomain::TransientCpu,
	MemorySourceKind::Resource, MemoryAccuracy::Exact, kTemporaries.id);
/** Static source descriptor for temporary packed icon upload buffers. */
inline constexpr auto kIconTightUpload = makeMemorySourceDescriptor(
	"flowui.memory.temporaries.icon_tight_upload", MemoryDomain::TransientCpu,
	MemorySourceKind::Resource, MemoryAccuracy::AllocatorRequested, kTemporaries.id);
/** Static source descriptor for temporary font decode buffers. */
inline constexpr auto kFontDecode = makeMemorySourceDescriptor(
	"flowui.memory.temporaries.font_decode", MemoryDomain::TransientCpu,
	MemorySourceKind::Resource, MemoryAccuracy::Exact, kTemporaries.id);
/** Static source descriptor for temporary combined atlas pixels. */
inline constexpr auto kFontAtlasCombine = makeMemorySourceDescriptor(
	"flowui.memory.temporaries.font_atlas_combine", MemoryDomain::TransientCpu,
	MemorySourceKind::Resource, MemoryAccuracy::AllocatorRequested, kTemporaries.id);

/** Static source descriptor for process-wide platform accounting. */
inline constexpr auto kProcess = makeMemorySourceDescriptor(
	"flowui.memory.process", MemoryDomain::Process, MemorySourceKind::Process,
	MemoryAccuracy::Estimate);
/** Static source descriptor for Vulkan/VMA heap accounting. */
inline constexpr auto kVulkanHeaps = makeMemorySourceDescriptor(
	"flowui.memory.vulkan.heaps", MemoryDomain::VulkanHeap, MemorySourceKind::GpuHeap,
	MemoryAccuracy::AllocatorRequested);

/** Built-in source catalog registered by the monitoring service. */
inline constexpr StaticMemorySourceDescriptor kAll[] = {
	kStorageCpu, kStorageMetadata, kStorageTransient, kStorageGpu,
	kDevelopment, kMonitoring, kDevTooling, kDevTreeCapture, kMemoryWorkbench,
	kStoragePersistent, kStorageStringPool, kStorageFrameTransient, kStorageWorkerTransient,
	kStorageDecodeTransient, kStorageUploadStaging,
	kManagers, kElements, kInputFields, kInputTextPayload, kFonts, kFontAtlasCpuPixels,
	kIcons, kIconSvgDocuments, kIconAtlasMetadata, kViewports, kPopups,
	kShortcuts, kActions, kThemes, kUiLayout, kRenderer, kTemporaries,
	kRendererFramePayload,
	kImageDecode, kIconRaster, kIconTightUpload, kFontDecode, kFontAtlasCombine,
	kProcess, kVulkanHeaps,
};

/** Physical parent for a diagnostic demand child; child bytes must not be added again. */
[[nodiscard]] constexpr MemorySourceId
initial_capacity_parent(const FlowUi::detail::InitialCapacityDefinition& definition) noexcept {
	const auto key = definition.key;
	if (key.starts_with("flowui.tuning.storage.") || key.starts_with("flowui.tuning.allocator."))
		return kStorageMetadata.id;
	if (key.starts_with("flowui.tuning.input_fields."))
		return kInputFields.id;
	if (key.starts_with("flowui.tuning.elements."))
		return kElements.id;
	if (key.starts_with("flowui.tuning.popups."))
		return kPopups.id;
	if (key.starts_with("flowui.tuning.shortcuts."))
		return kShortcuts.id;
	if (key.starts_with("flowui.tuning.actions."))
		return kActions.id;
	if (key.starts_with("flowui.tuning.fonts."))
		return kFonts.id;
	if (key.starts_with("flowui.tuning.icons."))
		return kIcons.id;
	if (key.starts_with("flowui.tuning.themes."))
		return kThemes.id;
	if (key.starts_with("flowui.tuning.viewports."))
		return kViewports.id;
	if (key.starts_with("flowui.tuning.development."))
		return kDevTreeCapture.id;
	if (key.starts_with("flowui.tuning.ui."))
		return kUiLayout.id;
	return kManagers.id;
}

/**
 * Return owned built-in capacity target metadata, including compiled subsystem targets at memory level
 * two and above.
 */
inline std::vector<MemoryTuningTargetDescriptor> tuningTargets() {
	using Metric = MemoryTuningMetric;
	using Unit = MemoryCapacityUnit;
	using Policy = MemoryApplyPolicy;
	std::vector<MemoryTuningTargetDescriptor> result{
		{"storage.initialPersistentCpuBytes", makeMemorySourceId("flowui.tuning.storage.initial_persistent_cpu_bytes"), kStoragePersistent.id,
			64u, UINT64_MAX, 64u, 4ull * 1024ull * 1024ull,
			Metric::PeakLogicalBytes, Unit::Bytes, Policy::RestartRequired},
		{"storage.initialStringBytes", makeMemorySourceId("flowui.tuning.storage.initial_string_bytes"), kStorageStringPool.id,
			64u, UINT64_MAX, 64u, 1ull * 1024ull * 1024ull,
			Metric::PeakLogicalBytes, Unit::Bytes, Policy::RestartRequired},
		{"storage.transientBytesPerFramePerWindow", makeMemorySourceId("flowui.tuning.storage.transient_bytes_per_frame_per_window"), kStorageFrameTransient.id,
			64u, UINT64_MAX, 64u, 1ull * 1024ull * 1024ull,
			Metric::PeakLogicalBytes, Unit::Bytes, Policy::RestartRequired},
		{"storage.transientBytesPerWorker", makeMemorySourceId("flowui.tuning.storage.transient_bytes_per_worker"), kStorageWorkerTransient.id,
			64u, UINT64_MAX, 64u, 1ull * 1024ull * 1024ull,
			Metric::PeakLogicalBytes, Unit::Bytes, Policy::RestartRequired},
		{"storage.initialDecodeScratchBytes", makeMemorySourceId("flowui.tuning.storage.initial_decode_scratch_bytes"), kStorageDecodeTransient.id,
			64u, UINT64_MAX, 64u, 8ull * 1024ull * 1024ull,
			Metric::PeakLogicalBytes, Unit::Bytes, Policy::RestartRequired},
		{"storage.initialUploadStagingBytes", makeMemorySourceId("flowui.tuning.storage.initial_upload_staging_bytes"), kStorageUploadStaging.id,
			64u, UINT64_MAX, 64u, 16ull * 1024ull * 1024ull,
			Metric::PeakLogicalBytes, Unit::Bytes, Policy::RestartRequired},
	};
	result.push_back({"storage.initialInstanceBytesPerFrame", kRendererFramePayload.tuningTarget,
					  kRendererFramePayload.id, 256u, SIZE_MAX / 2ull, 256u, 1024ull * 1024ull,
					  Metric::LogicalLiveBytes, Unit::Bytes, Policy::RestartRequired});
#if FLOWUI_DEV_MEMORY_LEVEL >= 2
	for (const auto& definition : FlowUi::detail::initial_capacity_definitions) {
		if (!definition.storage_member.empty())
			continue;
		result.push_back(
			{std::string(definition.key), definition.id(), definition.id(), 0u, definition.maximum,
			 definition.alignment, definition.default_value, Metric::ObjectCount,
			 definition.unit == FlowUi::detail::InitialCapacityUnit::Bytes ? Unit::Bytes
																		   : Unit::Entries,
			 Policy::RestartRequired});
	}
#endif
	return result;
}

} // namespace FlowUi::devSystems::memory_sources

#endif
