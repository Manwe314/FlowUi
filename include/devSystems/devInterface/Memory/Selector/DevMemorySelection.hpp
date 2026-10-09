#pragma once
#include "FlowUi/BuildConfig.hpp"
#if FLOW_UI_DEV_MODE
#include "FlowUi/WindowId.hpp"
#include <array>
#include <cstddef>
#include <cstdint>
#include <string_view>

namespace FlowUi::devSystems {
/** Stable selector identities, independent of reporting-source and row indexes.
 * Append new identities to preserve existing retained row keys. */
enum class DevMemoryNodeId : uint64_t {
	None = 0,
	CpuStorage,
	PersistentPool,
	StringPool,
	StorageMetadata,
	TransientArenas,
	FrameArenas,
	WorkerArenas,
	DecodeScratch,
	UploadStaging,
	Managers,
	Elements,
	InputFields,
	TextPayload,
	Fonts,
	AtlasCpuPixels,
	Icons,
	SvgDocuments,
	AtlasMetadata,
	Viewports,
	Popups,
	Shortcuts,
	Actions,
	Themes,
	UiLayout,
	Renderer,
	FramePayload,
	TemporaryCpuResources,
	ImageDecodeRgba,
	IconRaster,
	IconUploadBuffer,
	FontDecode,
	FontAtlasCombine,
	GpuResources,
	GpuBuffers,
	GpuImages,
	GpuHeaps,
	ProcessMemory,
	DevelopmentOverhead,
	Monitoring,
	Tooling,
	TreeCapture,
	SharedCpuStorage,
	SharedManagers,
};
/** One predefined row; scope policy describes navigation, not byte accounting. */
struct DevMemoryNodeDescriptor {
	std::string_view label;
	DevMemoryNodeId id;
	DevMemoryNodeId parent;
	bool application_wide;
};
inline constexpr auto memory_selector_nodes = std::to_array<DevMemoryNodeDescriptor>({
	{"CPU Storage", DevMemoryNodeId::CpuStorage, DevMemoryNodeId::None, false},
	{"Transient Arenas", DevMemoryNodeId::TransientArenas, DevMemoryNodeId::CpuStorage, false},
	{"Frame Arenas", DevMemoryNodeId::FrameArenas, DevMemoryNodeId::TransientArenas, false},
	{"Worker Arenas", DevMemoryNodeId::WorkerArenas, DevMemoryNodeId::TransientArenas, false},
	{"Decode Scratch", DevMemoryNodeId::DecodeScratch, DevMemoryNodeId::TransientArenas, false},
	{"Upload Staging", DevMemoryNodeId::UploadStaging, DevMemoryNodeId::TransientArenas, false},
	{"Managers", DevMemoryNodeId::Managers, DevMemoryNodeId::None, false},
	{"Input Fields", DevMemoryNodeId::InputFields, DevMemoryNodeId::Managers, false},
	{"Text Payload", DevMemoryNodeId::TextPayload, DevMemoryNodeId::InputFields, false},
	{"Viewports", DevMemoryNodeId::Viewports, DevMemoryNodeId::Managers, false},
	{"Popups", DevMemoryNodeId::Popups, DevMemoryNodeId::Managers, false},
	{"Shortcuts", DevMemoryNodeId::Shortcuts, DevMemoryNodeId::Managers, false},
	{"UI Layout", DevMemoryNodeId::UiLayout, DevMemoryNodeId::Managers, false},
	{"Renderer", DevMemoryNodeId::Renderer, DevMemoryNodeId::Managers, false},
	{"Frame Payload", DevMemoryNodeId::FramePayload, DevMemoryNodeId::Renderer, false},
	{"CPU Storage", DevMemoryNodeId::SharedCpuStorage, DevMemoryNodeId::None, true},
	{"Persistent Pool", DevMemoryNodeId::PersistentPool, DevMemoryNodeId::SharedCpuStorage, true},
	{"String Pool", DevMemoryNodeId::StringPool, DevMemoryNodeId::SharedCpuStorage, true},
	{"Storage Metadata", DevMemoryNodeId::StorageMetadata, DevMemoryNodeId::SharedCpuStorage, true},
	{"Managers", DevMemoryNodeId::SharedManagers, DevMemoryNodeId::None, true},
	{"Elements", DevMemoryNodeId::Elements, DevMemoryNodeId::SharedManagers, true},
	{"Fonts", DevMemoryNodeId::Fonts, DevMemoryNodeId::SharedManagers, true},
	{"Atlas CPU Pixels", DevMemoryNodeId::AtlasCpuPixels, DevMemoryNodeId::Fonts, true},
	{"Icons", DevMemoryNodeId::Icons, DevMemoryNodeId::SharedManagers, true},
	{"SVG Documents", DevMemoryNodeId::SvgDocuments, DevMemoryNodeId::Icons, true},
	{"Atlas Metadata", DevMemoryNodeId::AtlasMetadata, DevMemoryNodeId::Icons, true},
	{"Actions", DevMemoryNodeId::Actions, DevMemoryNodeId::SharedManagers, true},
	{"Themes", DevMemoryNodeId::Themes, DevMemoryNodeId::SharedManagers, true},
	{"Temporary CPU Resources", DevMemoryNodeId::TemporaryCpuResources, DevMemoryNodeId::None,
	 true},
	{"Image Decode RGBA", DevMemoryNodeId::ImageDecodeRgba, DevMemoryNodeId::TemporaryCpuResources,
	 true},
	{"Icon Raster", DevMemoryNodeId::IconRaster, DevMemoryNodeId::TemporaryCpuResources, true},
	{"Icon Upload Buffer", DevMemoryNodeId::IconUploadBuffer,
	 DevMemoryNodeId::TemporaryCpuResources, true},
	{"Font Decode", DevMemoryNodeId::FontDecode, DevMemoryNodeId::TemporaryCpuResources, true},
	{"Font Atlas Combine", DevMemoryNodeId::FontAtlasCombine,
	 DevMemoryNodeId::TemporaryCpuResources, true},
	{"GPU Resources", DevMemoryNodeId::GpuResources, DevMemoryNodeId::None, true},
	{"GPU Buffers", DevMemoryNodeId::GpuBuffers, DevMemoryNodeId::GpuResources, true},
	{"GPU Images", DevMemoryNodeId::GpuImages, DevMemoryNodeId::GpuResources, true},
	{"GPU Heaps", DevMemoryNodeId::GpuHeaps, DevMemoryNodeId::None, true},
	{"Process Memory", DevMemoryNodeId::ProcessMemory, DevMemoryNodeId::None, true},
	{"Development Overhead", DevMemoryNodeId::DevelopmentOverhead, DevMemoryNodeId::None, true},
	{"Monitoring", DevMemoryNodeId::Monitoring, DevMemoryNodeId::DevelopmentOverhead, true},
	{"Tooling", DevMemoryNodeId::Tooling, DevMemoryNodeId::DevelopmentOverhead, true},
	{"Tree Capture", DevMemoryNodeId::TreeCapture, DevMemoryNodeId::Tooling, true},
});
/** Heap keys occupy a separate range from fixed node identities. */
inline constexpr uint64_t memory_heap_node_base = uint64_t{1} << 32;
/** Always-bound window modifier and optional tree selection. Zero selects Overview. */
struct DevMemorySelection {
	std::array<bool, memory_selector_nodes.size() + 1> expanded = [] {
		std::array<bool, memory_selector_nodes.size() + 1> values{};
		for (const auto& node : memory_selector_nodes)
			values[static_cast<std::size_t>(node.id)] = node.parent == DevMemoryNodeId::None;
		return values;
	}();
	uint64_t selected_node = 0;
	WindowId window_scope = InvalidWindowId; // All Windows; not a reporting-source identity.
};
/** Find a fixed row without interpreting an unattributed memory sample as All Windows. */
[[nodiscard]] constexpr const DevMemoryNodeDescriptor* memory_node(uint64_t identity) noexcept {
	for (const auto& node : memory_selector_nodes)
		if (static_cast<uint64_t>(node.id) == identity)
			return &node;
	return nullptr;
}
/** Whether this fixed node has predetermined children (heaps add runtime children). */
[[nodiscard]] constexpr bool memory_node_has_children(DevMemoryNodeId identity) noexcept {
	if (identity == DevMemoryNodeId::GpuHeaps)
		return true;
	for (const auto& node : memory_selector_nodes)
		if (node.parent == identity)
			return true;
	return false;
}
/** Whether a row lies below an ancestor; dynamic heap leaves belong to GPU Heaps. */
[[nodiscard]] constexpr bool memory_node_descends_from(uint64_t identity,
													   DevMemoryNodeId ancestor) noexcept {
	if (identity >= memory_heap_node_base)
		return ancestor == DevMemoryNodeId::GpuHeaps;
	const auto* node = memory_node(identity);
	while (node && node->parent != DevMemoryNodeId::None) {
		if (node->parent == ancestor)
			return true;
		node = memory_node(static_cast<uint64_t>(node->parent));
	}
	return false;
}
/** Node applicability leaves the remembered window modifier intact. */
[[nodiscard]] constexpr bool memory_selection_application_wide(uint64_t identity) noexcept {
	const auto* node = memory_node(identity);
	return identity >= memory_heap_node_base || (node && node->application_wide);
}
} // namespace FlowUi::devSystems
#endif
