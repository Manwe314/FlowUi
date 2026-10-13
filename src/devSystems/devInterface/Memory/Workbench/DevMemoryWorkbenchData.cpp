#include "devSystems/devInterface/Memory/Workbench/DevMemoryWorkbenchData.hpp"
#if FLOW_UI_DEV_MODE
#include "devSystems/devMonitoringAndReporting/memory/DevMemorySources.hpp"
#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstdio>
namespace FlowUi::devSystems::interface_elements {
namespace {
using Node = DevMemoryNodeId;
using Recipe = MemoryTrackRecipe;
struct Mapping {
	Node node;
	MemorySourceId source;
	Recipe recipe;
};
constexpr Mapping mappings[] = {
	{Node::PersistentPool, memory_sources::kStoragePersistent.id, Recipe::Pool},
	{Node::StringPool, memory_sources::kStorageStringPool.id, Recipe::Pool},
	{Node::StorageMetadata, memory_sources::kStorageMetadata.id, Recipe::Container},
	{Node::FrameArenas, memory_sources::kStorageFrameTransient.id, Recipe::Arena},
	{Node::WorkerArenas, memory_sources::kStorageWorkerTransient.id, Recipe::Arena},
	{Node::DecodeScratch, memory_sources::kStorageDecodeTransient.id, Recipe::Arena},
	{Node::UploadStaging, memory_sources::kStorageUploadStaging.id, Recipe::Arena},
	{Node::Elements, memory_sources::kElements.id, Recipe::Container},
	{Node::InputFields, memory_sources::kInputFields.id, Recipe::Container},
	{Node::TextPayload, memory_sources::kInputTextPayload.id, Recipe::Container},
	{Node::Fonts, memory_sources::kFonts.id, Recipe::Container},
	{Node::AtlasCpuPixels, memory_sources::kFontAtlasCpuPixels.id, Recipe::Container},
	{Node::Icons, memory_sources::kIcons.id, Recipe::Container},
	{Node::SvgDocuments, memory_sources::kIconSvgDocuments.id, Recipe::Container},
	{Node::AtlasMetadata, memory_sources::kIconAtlasMetadata.id, Recipe::Container},
	{Node::Viewports, memory_sources::kViewports.id, Recipe::Container},
	{Node::Popups, memory_sources::kPopups.id, Recipe::Container},
	{Node::Shortcuts, memory_sources::kShortcuts.id, Recipe::Container},
	{Node::Actions, memory_sources::kActions.id, Recipe::Container},
	{Node::Themes, memory_sources::kThemes.id, Recipe::Container},
	{Node::UiLayout, memory_sources::kUiLayout.id, Recipe::Container},
	{Node::Renderer, memory_sources::kRenderer.id, Recipe::Independent},
	{Node::FramePayload, memory_sources::kRendererFramePayload.id, Recipe::ActiveOnly},
	{Node::ImageDecodeRgba, memory_sources::kImageDecode.id, Recipe::ActiveOnly},
	{Node::IconRaster, memory_sources::kIconRaster.id, Recipe::ActiveOnly},
	{Node::IconUploadBuffer, memory_sources::kIconTightUpload.id, Recipe::ActiveOnly},
	{Node::FontDecode, memory_sources::kFontDecode.id, Recipe::ActiveOnly},
	{Node::FontAtlasCombine, memory_sources::kFontAtlasCombine.id, Recipe::ActiveOnly},
	{Node::ProcessMemory, memory_sources::kProcess.id, Recipe::Independent},
	{Node::Monitoring, memory_sources::kMonitoring.id, Recipe::Container},
	{Node::Tooling, memory_sources::kDevTooling.id, Recipe::Container},
	{Node::TreeCapture, memory_sources::kDevTreeCapture.id, Recipe::Container}};
uint64_t sum(uint64_t left, uint64_t right) noexcept {
	return left > UINT64_MAX - right ? UINT64_MAX : left + right;
}
void accumulate(MemoryValueSample& result, const MemoryValueSample& value) noexcept {
	if (result.logicalLiveBytes > UINT64_MAX - value.logicalLiveBytes ||
		result.backingAllocatedBytes > UINT64_MAX - value.backingAllocatedBytes ||
		result.reusableBytes > UINT64_MAX - value.reusableBytes ||
		result.retiredBytes > UINT64_MAX - value.retiredBytes)
		result.flags = result.flags | MemorySampleFlag::Incomplete;
	result.logicalLiveBytes = sum(result.logicalLiveBytes, value.logicalLiveBytes);
	result.backingAllocatedBytes = sum(result.backingAllocatedBytes, value.backingAllocatedBytes);
	result.reusableBytes = sum(result.reusableBytes, value.reusableBytes);
	result.retiredBytes = sum(result.retiredBytes, value.retiredBytes);
	result.objectCount = sum(result.objectCount, value.objectCount);
	result.capacityCount = sum(result.capacityCount, value.capacityCount);
	result.flags = result.flags | value.flags;
	result.value_fields &= value.value_fields;
}
const Mapping* mapping(Node node) noexcept {
	for (const auto& entry : mappings)
		if (entry.node == node)
			return &entry;
	return nullptr;
}
void append_leaf(DevMemoryWorkbenchState& state, Node node, const DevMemorySelection& selection,
				 const MemoryLiveRead& read, bool included = false) {
	const auto* entry = mapping(node);
	if (!entry)
		return;
	const auto* descriptor = memory_node(uint64_t(node));
	MemoryWorkbenchTrack track;
	track.title = std::string(descriptor->label);
	track.identity = uint64_t(node);
	track.parent_identity = uint64_t(memory_node(uint64_t(node))->parent);
	track.recipe = entry->recipe;
	track.history_sources = {entry->source};
	track.window_scoped = !descriptor->application_wide;
	track.included = included;
	track.estimated = entry->recipe == Recipe::Container || entry->recipe == Recipe::Independent;
	track.note = included ? "Included/overlapping slice — do not add to parent" : "";

	const bool storage_source = entry->recipe == Recipe::Arena || entry->recipe == Recipe::Pool ||
								node == Node::StorageMetadata;
	const bool sampling_supported =
		read.status.runtimeLevel != MemoryMonitoringLevel::Disabled &&
		(storage_source || node == Node::ProcessMemory ||
		 read.status.runtimeLevel >= MemoryMonitoringLevel::SubsystemCapacity);
	for (const auto& source : read.sources) {
		if (source.key.source != entry->source ||
			(uint16_t(source.value.flags) & uint16_t(MemorySampleFlag::TuningAggregate)))
			continue;
		if (!descriptor->application_wide &&
			(source.key.window == InvalidWindowId ||
			 (selection.window_scope && source.key.window != selection.window_scope)))
			continue;
		if (descriptor->application_wide && node != Node::TreeCapture &&
			source.key.window != InvalidWindowId)
			continue;
		track.available = sampling_supported && source.lastSampledTick != 0;
		accumulate(track.value, source.value);
		track.keys.emplace_back(source.key);
		track.sampled_tick = std::max(track.sampled_tick, source.lastSampledTick);
	}
	if (read.storage && (entry->recipe == Recipe::Arena || entry->recipe == Recipe::Pool ||
						 node == Node::StorageMetadata)) {
		track.value = {};
		track.available = false;
		track.padding_bytes = 0;
		for (const auto& allocator : read.storage->allocators) {
			MemorySourceId source = 0;
			using Class = ::FlowUi::detail::storage::MemoryClass;
			switch (allocator.memoryClass) {
			case Class::Persistent:
				source = memory_sources::kStoragePersistent.id;
				break;
			case Class::StringPool:
				source = memory_sources::kStorageStringPool.id;
				break;
			case Class::ResourceMetadata:
				source = memory_sources::kStorageMetadata.id;
				break;
			case Class::FrameTransient:
				source = memory_sources::kStorageFrameTransient.id;
				break;
			case Class::WorkerTransient:
				source = memory_sources::kStorageWorkerTransient.id;
				break;
			case Class::DecodeTransient:
				source = memory_sources::kStorageDecodeTransient.id;
				break;
			case Class::UploadStaging:
				source = memory_sources::kStorageUploadStaging.id;
				break;
			default:
				break;
			}
			if (source != entry->source ||
				(!descriptor->application_wide &&
				 (!allocator.window ||
				  (selection.window_scope && allocator.window != selection.window_scope))))
				continue;
			track.available = true;
			accumulate(track.value,
					   MemoryValueSample{.logicalLiveBytes = allocator.liveBytes,
										 .reusableBytes = allocator.reusableBytes,
										 .backingAllocatedBytes = allocator.reservedBytes});
			track.padding_bytes = sum(track.padding_bytes, allocator.paddingBytes);
			track.largest_block_bytes =
				std::max(track.largest_block_bytes, allocator.largestReusableBlockBytes);
		}
	}
	if (!track.available)
		track.note = "Not sampled / unsupported at current monitoring level";
	if (node == Node::Renderer)
		track.note = "Related live/backing estimates; not a partition";
	if (node == Node::FramePayload)
		track.note = "Active payload; backing not reported";
	if (node == Node::TreeCapture)
		track.note = "Window-owned tree/scratch producers; independent of Tooling footprint";
	if (node == Node::ProcessMemory) {
		track.note = "Resident / virtual; independent process measurements";
		if (read.environment) {
			const auto& process = read.environment->process;
			const bool resident =
				(process.availableFields & uint32_t(ProcessMemoryFieldBit::ResidentBytes)) != 0;
			const bool virtual_memory =
				(process.availableFields & uint32_t(ProcessMemoryFieldBit::VirtualBytes)) != 0;
			track.available = resident || virtual_memory;
			if (!resident || !virtual_memory) {
				track.recipe = Recipe::ActiveOnly;
				track.title = resident ? "Resident memory" : "Virtual memory";
				track.value.logicalLiveBytes =
					resident ? process.residentBytes : process.virtualBytes;
				track.value.value_fields = uint16_t(MemoryValueField::LogicalLive);
				track.note = "Only this process field is supported; the other is unavailable";
			}
		}
	}

	if (uint16_t(track.value.flags) & uint16_t(MemorySampleFlag::Incomplete))
		track.note += " · Incomplete attribution; dropped or unmatched operations";

	// Source-level instance rows are retained under a selected manager/storage leaf.
	const bool detailed = selection.selected_node == uint64_t(node);
	std::vector<MemoryWorkbenchTrack> detail;
	if (detailed && !descriptor->application_wide && !selection.window_scope &&
		entry->recipe != Recipe::Arena) {
		detail.reserve(track.keys.size());
		for (const auto& key : track.keys) {
			auto current = std::ranges::find(read.sources, key, &MemoryCurrentSource::key);
			if (current == read.sources.end())
				continue;
			auto row = track;
			row.title = "Window " + std::to_string(key.window) + " / " + track.title;
			row.identity = (uint64_t(node) << 32) ^ key.window;
			row.keys = {key};
			row.value = current->value;
			row.included = true;
			row.parent_identity = track.identity;
			row.note = "Instance contribution included in the scope summary";
			detail.emplace_back(std::move(row));
		}
	}
	if (detailed && entry->recipe == Recipe::Arena && read.storage) {
		detail.reserve(read.storage->allocators.size());
		for (const auto& allocator : read.storage->allocators) {
			using Class = ::FlowUi::detail::storage::MemoryClass;
			const bool matches =
				(node == Node::FrameArenas && allocator.memoryClass == Class::FrameTransient) ||
				(node == Node::WorkerArenas && allocator.memoryClass == Class::WorkerTransient) ||
				(node == Node::DecodeScratch && allocator.memoryClass == Class::DecodeTransient) ||
				(node == Node::UploadStaging && allocator.memoryClass == Class::UploadStaging);
			if (!matches || !allocator.window ||
				(selection.window_scope && allocator.window != selection.window_scope))
				continue;
			auto row = track;
			row.keys.clear();
			row.history_sources.clear();
			row.title = "Window " + std::to_string(allocator.window) + " / slot " +
						std::to_string(allocator.frameSlot);
			if (node == Node::WorkerArenas)
				row.title += " / worker " + std::to_string(allocator.workerIndex);
			row.identity = (uint64_t(node) << 40) ^ (uint64_t(allocator.window) << 24) ^
						   (uint64_t(allocator.frameSlot) << 12) ^ allocator.workerIndex;
			row.value = {.logicalLiveBytes = allocator.liveBytes,
						 .reusableBytes = allocator.reusableBytes,
						 .backingAllocatedBytes = allocator.reservedBytes};
			row.padding_bytes = allocator.paddingBytes;
			row.largest_block_bytes = allocator.largestReusableBlockBytes;
			row.included = true;
			row.parent_identity = track.identity;
			for (const auto& target : memory_sources::tuningTargets())
				if (target.source == entry->source && target.unit == MemoryCapacityUnit::Bytes) {
					row.reference_bytes = target.productionDefault;
				}

			row.note = "Allocator instance · largest reusable block " +
					   memory_bytes(row.largest_block_bytes, state.decimal);
			detail.emplace_back(std::move(row));
		}
	}
	for (const auto& target : memory_sources::tuningTargets())
		if (target.source == entry->source && target.unit == MemoryCapacityUnit::Bytes &&
			(descriptor->application_wide || track.keys.size() == 1) &&
			entry->recipe != Recipe::ActiveOnly && entry->recipe != Recipe::Arena) {
			track.reference_bytes = target.productionDefault;
			track.note += " · Default reserve " +
						  memory_bytes(target.productionDefault, state.decimal) +
						  " (not active configuration)";
		}

	if (track.available && (track.recipe == Recipe::Pool || track.recipe == Recipe::Container ||
							track.recipe == Recipe::Arena)) {
		const auto accounted =
			sum(sum(track.value.logicalLiveBytes, track.value.reusableBytes), track.padding_bytes);
		if (accounted > track.value.backingAllocatedBytes ||
			(track.recipe == Recipe::Arena && accounted != track.value.backingAllocatedBytes)) {
			track.recipe = Recipe::Independent;
			track.note += " · Accounting does not form a verified partition";
		} else if (track.recipe == Recipe::Pool && accounted < track.value.backingAllocatedBytes) {
			track.note +=
				" · Other retained " +
				memory_bytes(track.value.backingAllocatedBytes - accounted, state.decimal) +
				" (not classified as reusable/padding)";
		}
	}
	state.tracks.emplace_back(std::move(track));
	for (auto& row : detail)
		state.tracks.emplace_back(std::move(row));
}
void append_gpu(DevMemoryWorkbenchState& state, Node node, const MemoryLiveRead& read) {
	MemoryWorkbenchTrack track;
	track.identity = uint64_t(node);
	track.parent_identity = uint64_t(memory_node(uint64_t(node))->parent);
	track.title = node == Node::GpuImages ? "GPU Images" : "GPU Buffers";
	track.recipe = Recipe::Resource;
	track.note = "GPU allocation bytes: active / awaiting release (images use reported byte size)";
	if (read.storage && read.storage->includesResourceKinds) {
		using Kind = ::FlowUi::detail::storage::ResourceKind;
		const auto kind = node == Node::GpuImages ? Kind::GpuImage : Kind::GpuBuffer;
		const auto& resource = read.storage->resources[size_t(kind)];
		track.available = true;
		track.value.logicalLiveBytes = resource.liveBytes;
		track.value.retiredBytes = resource.retiredBytes;
		track.value.backingAllocatedBytes = sum(resource.liveBytes, resource.retiredBytes);
		track.value.objectCount = resource.live;
	}
	state.tracks.emplace_back(std::move(track));
}
void append_heaps(DevMemoryWorkbenchState& state, const MemoryLiveRead& read, uint64_t selected) {
	if (!read.environment || !read.environment->gpu.available) {
		MemoryWorkbenchTrack track;
		track.title = "GPU Heaps";
		track.note = "GPU heap probe unavailable";
		state.tracks.emplace_back(std::move(track));
		return;
	}
	for (const auto& heap : read.environment->gpu.heaps) {
		if (selected >= memory_heap_node_base && selected != memory_heap_node_base + heap.heapIndex)
			continue;
		MemoryWorkbenchTrack track;
		track.title = "Heap " + std::to_string(heap.heapIndex) + " · budget usage";
		track.identity = memory_heap_node_base + heap.heapIndex;
		track.parent_identity = uint64_t(Node::GpuHeaps);
		track.recipe = Recipe::Budget;
		track.available = read.environment->gpu.memoryBudgetExtensionEnabled;
		track.value.logicalLiveBytes = heap.allocatorUsageBytes;
		track.value.backingAllocatedBytes = heap.allocatorBudgetBytes;
		track.reference_bytes = heap.heapSizeBytes;
		track.note = "Budget varies; heap size " + memory_bytes(heap.heapSizeBytes, state.decimal) +
					 "; no retained per-heap history";
		state.tracks.emplace_back(track);
		track.title = "Heap " + std::to_string(heap.heapIndex) + " · allocation / block bytes";
		track.identity ^= uint64_t{1} << 48;
		track.recipe = Recipe::Independent;
		track.available = true;
		track.value.logicalLiveBytes = heap.allocationBytes;
		track.value.backingAllocatedBytes = heap.blockBytes;
		track.reference_bytes = 0;
		state.tracks.emplace_back(track);
		track.title = "Heap " + std::to_string(heap.heapIndex) + " · physical heap size";
		track.identity ^= uint64_t{3} << 48;
		track.recipe = Recipe::ActiveOnly;
		track.value = {.logicalLiveBytes = heap.heapSizeBytes};
		track.note = "Physical-size reference; not current usage or guaranteed free memory";
		state.tracks.emplace_back(std::move(track));
	}
}
void append_node(DevMemoryWorkbenchState& state, Node node, const DevMemorySelection& selection,
				 const MemoryLiveRead& read) {
	if (mapping(node)) {
		append_leaf(state, node, selection, read);
		if (node == Node::ProcessMemory && read.environment) {
			const auto& process = read.environment->process;
			for (const auto field :
				 {ProcessMemoryFieldBit::PrivateBytes, ProcessMemoryFieldBit::SharedBytes,
				  ProcessMemoryFieldBit::PeakResidentBytes}) {
				if (!(process.availableFields & uint32_t(field)))
					continue;
				MemoryWorkbenchTrack row;
				row.identity = (uint64_t(node) << 32) | uint32_t(field);
				row.parent_identity = uint64_t(node);
				row.recipe = Recipe::ActiveOnly;
				row.available = true;
				row.estimated = true;
				row.title = field == ProcessMemoryFieldBit::PrivateBytes  ? "Private memory"
							: field == ProcessMemoryFieldBit::SharedBytes ? "Shared memory"
																		  : "Peak resident memory";
				row.value.logicalLiveBytes =
					field == ProcessMemoryFieldBit::PrivateBytes  ? process.privateBytes
					: field == ProcessMemoryFieldBit::SharedBytes ? process.sharedBytes
																  : process.peakResidentBytes;
				row.note = "Independent platform measurement; not an additive process partition";
				state.tracks.emplace_back(std::move(row));
			}
		}
		if (node == Node::Monitoring) {
			MemoryWorkbenchTrack row;
			row.identity = uint64_t(node) << 32;
			row.parent_identity = uint64_t(node);
			row.title = "Current reporting tables";
			row.available = true;
			row.estimated = true;
			row.recipe = Recipe::ActiveOnly;
			row.value.logicalLiveBytes = read.status.overhead.currentTableEstimatedBytes;
			row.note = "Estimate · live tables continue updating while history is frozen";
			state.tracks.emplace_back(std::move(row));
		}
		for (const auto& child : memory_selector_nodes)
			if (child.parent == node)
				append_leaf(state, child.id, selection, read, true);
		return;
	}
	if (node == Node::GpuResources || node == Node::GpuBuffers || node == Node::GpuImages) {
		if (node != Node::GpuImages)
			append_gpu(state, Node::GpuBuffers, read);
		if (node != Node::GpuBuffers)
			append_gpu(state, Node::GpuImages, read);
		return;
	}

	if (node == Node::GpuHeaps) {
		MemoryWorkbenchTrack total;
		total.identity = uint64_t(node);
		total.title = "All heaps · allocator allocations / blocks";
		total.history_sources = {memory_sources::kVulkanHeaps.id};
		total.note = "Allocator byte totals; budgets are shown per heap, not summed";
		for (const auto& source : read.sources)
			if (source.key.source == memory_sources::kVulkanHeaps.id) {
				total.value = source.value;
				total.available = read.environment && read.environment->gpu.available;
				total.keys.emplace_back(source.key);
			}
		state.tracks.emplace_back(std::move(total));
		append_heaps(state, read, 0);
		return;
	}
	const auto begin = state.tracks.size();
	for (const auto& child : memory_selector_nodes)
		if (child.parent == node)
			append_node(state, child.id, selection, read);
	if (node == Node::DevelopmentOverhead) {
		MemoryWorkbenchTrack workbench;
		workbench.identity = memory_sources::kMemoryWorkbench.id;
		workbench.parent_identity = uint64_t(node);
		workbench.title = "Memory workbench presentation";
		workbench.estimated = true;
		workbench.history_sources = {memory_sources::kMemoryWorkbench.id};
		workbench.note =
			"CPU geometry/view capacity estimate; viewport GPU targets belong to the GPU views";
		for (const auto& source : read.sources)
			if (source.key.source == memory_sources::kMemoryWorkbench.id &&
				!(uint16_t(source.value.flags) & uint16_t(MemorySampleFlag::TuningAggregate))) {
				workbench.available = true;
				accumulate(workbench.value, source.value);
				workbench.keys.emplace_back(source.key);
			}
		state.tracks.emplace_back(std::move(workbench));
	}

	if (node == Node::CpuStorage || node == Node::TransientArenas ||
		node == Node::SharedCpuStorage) {
		MemoryWorkbenchTrack summary;
		summary.identity = uint64_t(node);
		summary.parent_identity = uint64_t(memory_node(uint64_t(node))->parent);
		summary.title = std::string(memory_node(uint64_t(node))->label) + " · summary";
		summary.recipe = node == Node::SharedCpuStorage ? Recipe::Container : Recipe::Arena;
		summary.available = true;
		summary.note = "Actual allocator categories counted once; source high-water marks are not "
					   "a simultaneous peak";
		for (size_t index = begin; index < state.tracks.size(); ++index) {
			const auto& child = state.tracks[index];
			if (child.included)
				continue;
			summary.available &= child.available;
			accumulate(summary.value, child.value);
			summary.padding_bytes = sum(summary.padding_bytes, child.padding_bytes);
			summary.keys.insert(summary.keys.end(), child.keys.begin(), child.keys.end());
			summary.history_sources.insert(summary.history_sources.end(),
										   child.history_sources.begin(),
										   child.history_sources.end());
			summary.window_scoped = node != Node::SharedCpuStorage;
		}
		state.tracks.insert(state.tracks.begin() + begin, std::move(summary));
	}
	if (node == Node::Managers || node == Node::SharedManagers) {
		MemoryWorkbenchTrack summary;
		summary.identity = uint64_t(node);
		summary.parent_identity = uint64_t(memory_node(uint64_t(node))->parent);
		summary.title =
			std::string(memory_node(uint64_t(node))->label) + " · independent estimates";
		summary.note = "Subtotal unavailable: manager/storage overlap is not fully attributed";
		summary.window_scoped = node == Node::Managers;
		for (size_t index = begin; index < state.tracks.size(); ++index)
			if (!state.tracks[index].included)
				summary.history_sources.insert(summary.history_sources.end(),
											   state.tracks[index].history_sources.begin(),
											   state.tracks[index].history_sources.end());
		state.tracks.insert(state.tracks.begin() + begin, std::move(summary));
	}
}
} // namespace
uint64_t memory_now_ns() noexcept {
	return uint64_t(std::chrono::duration_cast<std::chrono::nanoseconds>(
						std::chrono::steady_clock::now().time_since_epoch())
						.count());
}
std::string memory_bytes(uint64_t bytes, bool decimal) {
	const char* binary[] = {"B", "KiB", "MiB", "GiB", "TiB", "PiB", "EiB"};
	const char* metric[] = {"B", "kB", "MB", "GB", "TB", "PB", "EB"};
	const double base = decimal ? 1000 : 1024;
	double value = double(bytes);
	size_t unit = 0;
	while (value >= base && unit < 6) {
		value /= base;
		++unit;
	}
	char output[64];
	std::snprintf(output, sizeof(output), unit ? "%.2f %s" : "%.0f %s", value,
				  decimal ? metric[unit] : binary[unit]);
	return output;
}
std::string memory_track_reading(const MemoryWorkbenchTrack& track, bool decimal) {
	if (!track.available)
		return "Unavailable";
	const auto logical = uint16_t(MemoryValueField::LogicalLive),
			   backing = uint16_t(MemoryValueField::Backing);
	const bool process =
		track.identity == uint64_t(Node::ProcessMemory) && track.recipe == Recipe::Independent;
	const bool heaps = track.identity == uint64_t(Node::GpuHeaps);
	const std::string active_label = process						  ? "Resident"
									 : heaps						  ? "Allocations"
									 : track.recipe == Recipe::Budget ? "Usage"
																	  : "Active";
	const std::string backing_label = process							 ? "Virtual"
									  : heaps							 ? "Blocks"
									  : track.recipe == Recipe::Budget	 ? "Budget"
									  : track.recipe == Recipe::Resource ? "Retained resource bytes"
																		 : "Backing";
	std::string result =
		(track.value.value_fields & logical)
			? active_label + " " + memory_bytes(track.value.logicalLiveBytes, decimal)
			: active_label + " not reported";
	if (track.recipe != Recipe::ActiveOnly)
		result += " · " + backing_label +
				  ((track.value.value_fields & backing)
					   ? " " + memory_bytes(track.value.backingAllocatedBytes, decimal)
					   : " not reported");
	return result;
}
uint64_t memory_axis_max(uint64_t extent) noexcept {
	if (!extent)
		return 1;
	const long double target = static_cast<long double>(extent) * 1.10L;
	long double power = std::pow(10.0L, std::floor(std::log10(target)));
	for (const int multiplier : {1, 2, 5, 10})
		if (power * multiplier >= target)
			return power * multiplier >= UINT64_MAX ? UINT64_MAX
													: uint64_t(std::ceil(power * multiplier));
	return UINT64_MAX;
}
void resolve_memory_tracks(DevMemoryWorkbenchState& state, const DevMemorySelection& selection,
						   const MemoryLiveRead& read) {
	state.tracks.clear();
	state.tracks.reserve(64);
	if (selection.selected_node >= memory_heap_node_base)
		append_heaps(state, read, selection.selected_node);
	else if (selection.selected_node)
		append_node(state, Node(selection.selected_node), selection, read);
	else {
		for (Node node : {Node::CpuStorage, Node::SharedCpuStorage, Node::Managers,
						  Node::SharedManagers, Node::TemporaryCpuResources, Node::GpuResources,
						  Node::GpuHeaps, Node::ProcessMemory, Node::DevelopmentOverhead})

		{
			const auto begin = state.tracks.size();
			append_node(state, node, selection, read);
			if (state.tracks.size() == begin)
				continue;
			if (state.tracks[begin].identity != uint64_t(node)) {
				MemoryWorkbenchTrack summary;
				summary.identity = uint64_t(node);
				summary.parent_identity = uint64_t(memory_node(uint64_t(node))->parent);
				summary.title = std::string(memory_node(uint64_t(node))->label);
				summary.note =
					"Independent accounting view; select scope to inspect reported contributors";
				state.tracks.insert(state.tracks.begin() + begin, std::move(summary));
			}
			auto& summary = state.tracks[begin];
			if (node == Node::CpuStorage)
				summary.title = "Window CPU Storage";
			if (node == Node::SharedCpuStorage)
				summary.title = "Shared CPU Storage";
			if (node == Node::Managers)
				summary.title = "Window Managers";
			if (node == Node::SharedManagers)
				summary.title = "Shared Managers";
			if (selection.window_scope && memory_selection_application_wide(uint64_t(node))) {
				summary.title = "Application-wide context / " + summary.title;
				summary.note = "Excluded from selected-window totals · " + summary.note;
			}
			if (node == Node::GpuResources) {
				summary.available = true;
				summary.recipe = Recipe::Resource;
				summary.estimated = true;
				for (size_t index = begin + 1; index < state.tracks.size(); ++index) {
					summary.available &= state.tracks[index].available;
					accumulate(summary.value, state.tracks[index].value);
				}
				summary.note += " · GPU resource byte view overlaps heap/process context";
			}

			for (size_t index = begin + 1; index < state.tracks.size(); ++index)
				if (!state.tracks[index].included) {
					for (const auto source : state.tracks[index].history_sources)
						if (std::ranges::find(summary.history_sources, source) ==
							summary.history_sources.end())
							summary.history_sources.emplace_back(source);
					for (const auto& key : state.tracks[index].keys)
						if (std::ranges::find(summary.keys, key) == summary.keys.end())
							summary.keys.emplace_back(key);
				}
			state.tracks.erase(state.tracks.begin() + begin + 1, state.tracks.end());
		}
	}
	uint64_t extent = 0;
	for (const auto& track : state.tracks)
		if (track.available)
			extent = std::max({extent, track.value.logicalLiveBytes,
							   track.value.backingAllocatedBytes, track.reference_bytes});
	const auto now = memory_now_ns();
	const auto target = memory_axis_max(extent);
	const bool changed =
		state.scope != selection.selected_node || state.window != selection.window_scope;
	if (changed || target > state.scale_bytes || state.fit_pending) {
		state.scale_bytes = target;
		state.shrink_since_ns = 0;
	} else if (extent < state.scale_bytes / 2) {
		if (!state.shrink_since_ns)
			state.shrink_since_ns = now;
		if (now - state.shrink_since_ns >= 10'000'000'000ull) {
			state.scale_bytes = target;
			state.shrink_since_ns = 0;
		}
	} else
		state.shrink_since_ns = 0;
	state.scope = selection.selected_node;
	state.window = selection.window_scope;
}
void pan_memory_view(DevMemoryWorkbenchState& state, int direction) noexcept {
	const auto width = state.view_end_ns > state.view_begin_ns
						   ? state.view_end_ns - state.view_begin_ns
						   : state.duration_ns;
	const auto step = std::max<uint64_t>(1, width / 4);
	const auto maximum =
		state.retained_end_ns > width ? state.retained_end_ns - width : state.retained_begin_ns;
	const auto begin = direction < 0 ? state.view_begin_ns - std::min(state.view_begin_ns, step)
					   : state.view_begin_ns > UINT64_MAX - step ? UINT64_MAX
																 : state.view_begin_ns + step;
	state.view_begin_ns =
		std::clamp(begin, state.retained_begin_ns, std::max(state.retained_begin_ns, maximum));
	state.view_end_ns = state.view_begin_ns + width;
	state.following = false;
	state.next_history_ns = 0;
}
void zoom_memory_view(DevMemoryWorkbenchState& state, double factor, double anchor) noexcept {
	if (!std::isfinite(factor) || factor <= 0)
		return;
	const auto old_width = state.view_end_ns > state.view_begin_ns
							   ? state.view_end_ns - state.view_begin_ns
							   : state.duration_ns;
	const auto available = state.retained_end_ns > state.retained_begin_ns
							   ? state.retained_end_ns - state.retained_begin_ns
							   : old_width;
	const auto width =
		uint64_t(std::clamp(static_cast<long double>(old_width) * factor, 1.0L,
							static_cast<long double>(std::max<uint64_t>(1, available))));
	anchor = std::clamp(anchor, 0.0, 1.0);
	const auto pivot = state.view_begin_ns + uint64_t(old_width * anchor);
	const auto left = uint64_t(width * anchor);
	const auto begin = pivot > left ? pivot - left : 0;
	state.view_begin_ns =
		std::clamp(begin, state.retained_begin_ns,
				   std::max(state.retained_begin_ns, state.retained_end_ns > width
														 ? state.retained_end_ns - width
														 : state.retained_begin_ns));
	state.view_end_ns = state.view_begin_ns + width;
	state.duration_ns = width;
	state.following = false;
	state.next_history_ns = 0;
}
bool visit_memory_history(const DevMemoryWorkbenchState& state, DevMemoryReporting& reporting,
						  const std::function<void(const MemorySampleSegment&)>& visitor) {
	const auto* saved = selected_memory_dataset(state.analysis);
	if (!saved && state.held_data)
		saved = &*state.held_data;
	if (saved) {
		for (const auto& segment : saved->segments)
			visitor(segment);
		return true;
	}
	return reporting.visit_history_chunks(reporting.status(), [&](auto records) {
		for (const auto& segment : records)
			visitor(segment);
	});
}
void hold_memory_history(DevMemoryWorkbenchState& state, DevMemoryReporting& reporting) {
	if (selected_memory_dataset(state.analysis) || state.held_data)
		return;
	MemoryDataset held;
	held.live = reporting.read_live();
	held.begin_ns = state.view_begin_ns;
	held.end_ns = state.view_end_ns ? state.view_end_ns : held.live.status.last_consumed_ns;
	held.name = "Held recent observations";
	held.segments.reserve(8192);
	bool limited = false;
	const bool coherent = reporting.visit_history_chunks(held.live.status, [&](auto records) {
		for (auto segment : records) {
			if (segment.beginNs >= held.end_ns || segment.endNs <= held.begin_ns)
				continue;
			if (held.segments.size() == 8192) {
				limited = true;
				continue;
			}
			segment.endNs = std::min(segment.endNs, held.end_ns);
			held.segments.emplace_back(segment);
		}
	});
	held.complete = coherent && !limited;
	state.message = held.complete ? "View held; live collection continues"
								  : "Held view has missing/limited observations";
	state.held_data = std::move(held);
}
void resolve_memory_history_tracks(DevMemoryWorkbenchState& state, DevMemoryReporting& reporting,
								   uint64_t timestamp) {
	for (auto& track : state.tracks) {
		track.value = {};
		track.available = false;
		track.padding_bytes = 0;
		track.largest_block_bytes = 0;
		track.note = "Historical source samples; padding/allocator/resource detail not retained";
		track.recipe = track.recipe == Recipe::Arena ? Recipe::Container : track.recipe;
	}
	const auto reconstruct = [&](const auto& segment) {
		if (segment.beginNs > timestamp || segment.endNs <= timestamp)
			return;
		for (auto& track : state.tracks)
			if (track.identity != uint64_t(Node::Managers) &&
				track.identity != uint64_t(Node::SharedManagers) &&
				(!track.included || track.keys.empty() ||
				 std::ranges::find(track.keys,
								   MemorySourceKey{segment.value.source, segment.value.window}) !=
					 track.keys.end()) &&
				!(uint16_t(segment.value.flags) & uint16_t(MemorySampleFlag::TuningAggregate)) &&
				std::ranges::find(track.history_sources, segment.value.source) !=
					track.history_sources.end() &&
				(!track.window_scoped ||
				 (segment.value.window &&
				  (!state.window || segment.value.window == state.window)))) {
				accumulate(track.value, segment.value);
				track.available = true;
			}
	};
	const bool complete = visit_memory_history(state, reporting, reconstruct);
	if (!complete)
		state.message = "Selected observations expired during read";
	uint64_t extent = 0;
	for (const auto& track : state.tracks)
		if (track.available)
			extent =
				std::max({extent, track.value.logicalLiveBytes, track.value.backingAllocatedBytes});
	state.scale_bytes = memory_axis_max(extent);
}
void build_memory_history(DevMemoryWorkbenchState& state, DevMemoryReporting& reporting,
						  size_t count) {
	count = std::clamp<size_t>(count, 32, 2048);
	auto track = std::ranges::find(state.tracks, state.focus, &MemoryWorkbenchTrack::identity);
	if (track == state.tracks.end())
		track = std::ranges::find_if(
			state.tracks, [](const auto& item) { return !item.history_sources.empty(); });
	if (track == state.tracks.end() || track->history_sources.empty()) {
		state.lanes.clear();
		state.all_lanes.clear();
		state.lane_options.clear();
		state.statistics_label.clear();
		state.view_begin_ns = 0;
		state.view_end_ns = 0;
		state.message = "No retained series for this scope";
		++state.layout_revision;
		return;
	}
	std::vector<MemoryHistoryLane> lanes;
	lanes.reserve(12);
	uint64_t view_begin = state.view_begin_ns, view_end = state.view_end_ns, maximum = 0;
	uint64_t observed_maximum = 0, history_revision = 0;
	long double weighted_sum = 0, total_weight = 0;
	bool omitted = false;
	const auto project = [&](const auto& walk, const MemoryReportingStatus& snapshot) {
		uint64_t oldest = UINT64_MAX, newest = 0;
		if (!walk([&](const auto& segment) {
				if (uint16_t(segment.value.flags) & uint16_t(MemorySampleFlag::TuningAggregate))
					return;
				if (std::ranges::find(track->history_sources, segment.value.source) ==
					track->history_sources.end())
					return;
				if (track->window_scoped &&
					(!segment.value.window ||
					 (state.window && segment.value.window != state.window)))
					return;
				const auto end_ns = std::min(segment.endNs, snapshot.last_consumed_ns);
				if (end_ns <= segment.beginNs)
					return;
				const MemorySourceKey key{segment.value.source, segment.value.window};
				if (track->included && !track->keys.empty() &&
					std::ranges::find(track->keys, key) == track->keys.end())
					return;
				auto lane = std::ranges::find(lanes, key, &MemoryHistoryLane::key);
				if (lane == lanes.end()) {
					if (lanes.size() == 12) {
						omitted = true;
						return;
					}
					MemoryHistoryLane row;
					row.key = key;
					row.independent = track->recipe == Recipe::Independent;
					row.label = track->title;
					for (const auto& entry : mappings)
						if (entry.source == key.source) {
							row.label = std::string(memory_node(uint64_t(entry.node))->label);
							row.independent = entry.recipe == Recipe::Independent;
						}
					if (key.window)
						row.label += " · Window " + std::to_string(key.window);
					if (key.source == memory_sources::kVulkanHeaps.id)
						row.label += " · Allocations / Blocks";
					if (key.source == memory_sources::kProcess.id)
						row.label += " · Resident / Virtual";
					else if (row.independent)
						row.label += " · independent Active / Backing";
					row.columns.resize(count);
					lanes.emplace_back(std::move(row));
				}
				oldest = std::min(oldest, segment.beginNs);
				newest = std::max(newest, end_ns);
			}))
			return false;
		if (oldest == UINT64_MAX || newest <= oldest)
			return true;
		if (state.following || !view_end || state.fit_pending) {
			view_end = newest;
			view_begin =
				state.fit_pending
					? oldest
					: std::max(oldest, newest > state.duration_ns ? newest - state.duration_ns : 0);
		}
		state.retained_begin_ns = oldest;
		state.retained_end_ns = newest;
		if (view_begin < oldest)
			view_begin = oldest;
		if (view_end > newest)
			view_end = newest;
		if (view_end <= view_begin) {
			view_begin = oldest;
			view_end = newest;
		}
		const auto duration = std::max<uint64_t>(1, view_end - view_begin);
		const auto statistics_begin = state.range_begin_ns ? state.range_begin_ns : view_begin;
		const auto statistics_end = state.range_end_ns ? state.range_end_ns : view_end;
		if (!walk([&](const auto& segment) {
				const auto end_ns = std::min(segment.endNs, snapshot.last_consumed_ns);
				if (end_ns <= view_begin || segment.beginNs >= view_end)
					return;
				auto lane = std::ranges::find(
					lanes, MemorySourceKey{segment.value.source, segment.value.window},
					&MemoryHistoryLane::key);
				if (lane == lanes.end())
					return;
				const auto demand_field = state.history_metric == 2 ? MemoryValueField::Objects
										  : state.history_metric == 1
											  ? MemoryValueField::Backing
											  : MemoryValueField::LogicalLive;
				const auto backing_field = state.history_metric == 2 ? MemoryValueField::Capacity
																	 : MemoryValueField::Backing;
				const auto demand = state.history_metric == 2 ? segment.value.objectCount
									: state.history_metric == 1
										? segment.value.backingAllocatedBytes
										: segment.value.logicalLiveBytes;
				const auto backing = state.history_metric == 2
										 ? segment.value.capacityCount
										 : segment.value.backingAllocatedBytes;
				if (lane == lanes.begin() + std::min<size_t>(state.history_mode
																 ? state.comparison_indices[0]
																 : state.series_index,
															 lanes.size() - 1) &&
					(segment.value.value_fields & uint16_t(demand_field))) {
					const auto from = std::max(segment.beginNs, statistics_begin),
							   to = std::min(end_ns, statistics_end);
					if (to > from) {
						total_weight += to - from;
						weighted_sum += static_cast<long double>(to - from) * demand;
						observed_maximum = std::max(observed_maximum, demand);
					}
				}
				const auto begin = std::max(segment.beginNs, view_begin),
						   end = std::min(end_ns, view_end);
				const auto first =
					std::min(count - 1, size_t(static_cast<long double>(begin - view_begin) *
											   count / duration));
				const auto last =
					std::min(count - 1, size_t(static_cast<long double>(end - view_begin - 1) *
											   count / duration));
				for (size_t column = first; column <= last; ++column) {
					auto& pixel = lane->columns[column];
					pixel.mixed_epochs |= pixel.available && pixel.epoch != segment.history_epoch;
					if (segment.value.value_fields & uint16_t(demand_field))
						pixel.value_fields |= uint16_t(MemoryValueField::LogicalLive);
					if (segment.value.value_fields & uint16_t(backing_field))
						pixel.value_fields |= uint16_t(MemoryValueField::Backing);
					pixel.flags = pixel.flags | segment.value.flags;
					pixel.available = true;
					pixel.epoch = segment.history_epoch;
					pixel.sequence = segment.sequence;
					pixel.first_ns = pixel.first_ns ? std::min(pixel.first_ns, begin) : begin;
					pixel.last_ns = std::max(pixel.last_ns, end);
					if (segment.value.value_fields & uint16_t(demand_field)) {
						pixel.active_min = std::min(pixel.active_min, demand);
						pixel.active_max = std::max(pixel.active_max, demand);
					}
					if (segment.value.value_fields & uint16_t(backing_field))
						pixel.backing_max = std::max(pixel.backing_max, backing);
					maximum = std::max({maximum, pixel.active_max, pixel.backing_max});
				}
			}))
			return false;
		history_revision = snapshot.history_revision;
		return true;
	};
	const auto* saved = selected_memory_dataset(state.analysis);
	if (!saved && state.held_data)
		saved = &*state.held_data;
	const auto snapshot = saved ? saved->live.status : reporting.status();
	const auto walk = [&](const auto& visitor) {
		return visit_memory_history(state, reporting, visitor);
	};
	const bool complete = project(walk, snapshot);
	if (!complete) {
		state.lanes.clear();
		state.all_lanes.clear();
		state.lane_options.clear();
		state.statistics_label.clear();
		state.message = "History refresh pending: retention changed during traversal";
		++state.layout_revision;
		return;
	}
	state.all_lanes = std::move(lanes);
	state.lane_options.clear();
	state.lane_options.reserve(state.all_lanes.size());
	for (size_t index = 0; index < state.all_lanes.size(); ++index) {
		auto& lane = state.all_lanes[index];
		uint64_t extent = 0;
		for (const auto& value : lane.columns)
			if (value.available)
				extent = std::max(
					{extent, value.active_max, state.show_backing ? value.backing_max : 0});
		lane.scale = state.scale_mode == 1 ? std::max<uint64_t>(1, uint64_t(state.manual_scale))
										   : memory_axis_max(extent);
		state.lane_options.emplace_back(FSEL::ComboBoxOption{.value = index, .text = lane.label});
	}
	state.lanes.clear();
	if (!state.all_lanes.empty()) {
		state.series_index = std::min<uint64_t>(state.series_index, state.all_lanes.size() - 1);
		if (!state.history_mode)
			state.lanes.emplace_back(state.all_lanes[state.series_index]);
		else
			for (auto index : state.comparison_indices)
				if (index < state.all_lanes.size() &&
					!std::ranges::any_of(
						state.lanes,
						[&](const auto& lane) { return lane.key == state.all_lanes[index].key; }))
					state.lanes.emplace_back(state.all_lanes[index]);
	}
	if (state.scale_mode == 2)
		for (auto& lane : state.lanes)
			lane.scale = memory_axis_max(maximum);
	state.view_begin_ns = view_begin;
	state.view_end_ns = view_end;
	state.fit_pending = false;
	state.history_revision = history_revision;
	state.history_scale_bytes = state.lanes.empty() ? 1 : state.lanes.front().scale;
	state.statistics_label =
		total_weight
			? "Selected series · observed maximum " +
				  (state.history_metric == 2 ? std::to_string(observed_maximum)
											 : memory_bytes(observed_maximum, state.decimal)) +
				  " · time-weighted mean " +
				  (state.history_metric == 2
					   ? std::to_string(uint64_t(weighted_sum / total_weight))
					   : memory_bytes(uint64_t(weighted_sum / total_weight), state.decimal))
			: "No interval statistics for this range";
	state.message = state.lanes.empty() ? "No retained history"
					: omitted			? "12 available constituents; choose a narrower scope"
					: state.lanes.size() > 1
						? "Constituent source lanes — independent peaks are not summed"
						: "Sampled intervals · independent demand/backing outlines";
	if (saved && !saved->complete)
		state.message += " · Partial coverage: missing observations are unavailable";
	++state.history_reads;
	++state.layout_revision;
}

} // namespace FlowUi::devSystems::interface_elements
#endif
