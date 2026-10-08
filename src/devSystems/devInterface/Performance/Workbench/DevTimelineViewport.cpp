#include "devSystems/devInterface/Performance/Workbench/DevTimelineViewport.hpp"
#if FLOW_UI_DEV_MODE && FLOWUI_PUBLIC_VULKAN_INTEROP
#include "Ui/Vk_UiRenderer.hpp"
#include "internal/ManagerStorage/FontCatalogController.hpp"
#include "internal/Text/TextLayoutService.hpp"
#include "internal/Vma.hpp"
#include "managers/ViewPortManager.hpp"
#include <algorithm>
#include <bit>
#include <chrono>
#include <cmath>
#include <cstring>

namespace FlowUi::devSystems::interface_elements {
namespace {
[[nodiscard]] Clay_BoundingBox intersection(Clay_BoundingBox first,
											Clay_BoundingBox second) noexcept {
	const float right = std::min(first.x + first.width, second.x + second.width);
	const float bottom = std::min(first.y + first.height, second.y + second.height);
	first.x = std::max(first.x, second.x);
	first.y = std::max(first.y, second.y);
	first.width = std::max(0.0f, right - first.x);
	first.height = std::max(0.0f, bottom - first.y);
	return first;
}
[[nodiscard]] bool contains(Clay_BoundingBox bounds, float horizontal, float vertical) noexcept {
	return horizontal >= bounds.x && horizontal < bounds.x + bounds.width && vertical >= bounds.y &&
		   vertical < bounds.y + bounds.height;
}
[[nodiscard]] uint32_t packed(Clay_Color color) noexcept {
	return uint32_t(color.r) | uint32_t(color.g) << 8 | uint32_t(color.b) << 16 |
		   uint32_t(color.a) << 24;
}
struct UploadSlot {
	VkBuffer buffer = VK_NULL_HANDLE;
	VmaAllocation allocation = nullptr;
	void* mapped = nullptr;
	VkDeviceSize capacity = 0;
	uint64_t generation = 0;
	VkDescriptorSet descriptors = VK_NULL_HANDLE;
};
struct SurfaceUpload {
	std::vector<UploadSlot> slots{};
	VkDevice device = VK_NULL_HANDLE;
	VmaAllocator allocator = nullptr;
	VkDescriptorPool pool = VK_NULL_HANDLE;
	~SurfaceUpload() {
		for (const auto& slot : slots)
			if (slot.buffer)
				vmaDestroyBuffer(allocator, slot.buffer, slot.allocation);
		if (pool)
			vkDestroyDescriptorPool(device, pool, nullptr);
	}
	[[nodiscard]] Status initialize(const ViewPortVulkanInterop& interop,
									VkDescriptorSetLayout layout) {
		if (pool)
			return {};
		device = interop.device;
		allocator = interop.allocator;
		slots.resize(std::max(1u, interop.framesInFlight));
		VkDescriptorPoolSize size{VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, uint32_t(slots.size())};
		VkDescriptorPoolCreateInfo info{VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO};
		info.maxSets = uint32_t(slots.size());
		info.poolSizeCount = 1;
		info.pPoolSizes = &size;
		if (vkCreateDescriptorPool(device, &info, nullptr, &pool) != VK_SUCCESS)
			return unexpectedError(
				makeError(ErrorCode::ViewportRecordingFailed, ErrorSite::ViewportRecord));
		std::vector<VkDescriptorSetLayout> layouts(slots.size(), layout);
		std::vector<VkDescriptorSet> sets(slots.size());
		VkDescriptorSetAllocateInfo allocation{VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO};
		allocation.descriptorPool = pool;
		allocation.descriptorSetCount = uint32_t(sets.size());
		allocation.pSetLayouts = layouts.data();
		if (vkAllocateDescriptorSets(device, &allocation, sets.data()) != VK_SUCCESS) {
			vkDestroyDescriptorPool(device, pool, nullptr);
			pool = VK_NULL_HANDLE;
			return unexpectedError(
				makeError(ErrorCode::ViewportRecordingFailed, ErrorSite::ViewportRecord));
		}
		for (size_t index = 0; index < slots.size(); ++index)
			slots[index].descriptors = sets[index];
		return {};
	}
	[[nodiscard]] Status upload(uint32_t index, std::span<const UiInstance> instances,
								uint64_t generation) {
		if (index >= slots.size() || instances.empty())
			return unexpectedError(
				makeError(ErrorCode::ViewportRecordingFailed, ErrorSite::ViewportRecord));
		auto& slot = slots[index];
		const VkDeviceSize bytes = instances.size_bytes();
		if (slot.generation == generation && slot.buffer)
			return {};
		if (bytes > slot.capacity) {
			VkBufferCreateInfo info{VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO};
			info.size = std::max(bytes, slot.capacity + slot.capacity / 2 + 4096);
			info.usage = VK_BUFFER_USAGE_STORAGE_BUFFER_BIT;
			VmaAllocationCreateInfo allocation{};
			allocation.usage = VMA_MEMORY_USAGE_AUTO_PREFER_HOST;
			allocation.flags = VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT |
							   VMA_ALLOCATION_CREATE_MAPPED_BIT;
			VkBuffer replacement{};
			VmaAllocation replacement_allocation{};
			VmaAllocationInfo mapped{};
			if (vmaCreateBuffer(allocator, &info, &allocation, &replacement,
								&replacement_allocation, &mapped) != VK_SUCCESS)
				return unexpectedError(
					makeError(ErrorCode::ViewportRecordingFailed, ErrorSite::ViewportRecord));
			// The hosting runtime has waited this slot's fence before recording any callback.
			if (slot.buffer)
				vmaDestroyBuffer(allocator, slot.buffer, slot.allocation);
			slot.buffer = replacement;
			slot.allocation = replacement_allocation;
			slot.mapped = mapped.pMappedData;
			slot.capacity = info.size;
			VkDescriptorBufferInfo binding{slot.buffer, 0, slot.capacity};
			VkWriteDescriptorSet write{VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET};
			write.dstSet = slot.descriptors;
			write.dstBinding = 0;
			write.descriptorCount = 1;
			write.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
			write.pBufferInfo = &binding;
			vkUpdateDescriptorSets(device, 1, &write, 0, nullptr);
		}
		if (!slot.mapped)
			return unexpectedError(
				makeError(ErrorCode::ViewportRecordingFailed, ErrorSite::ViewportRecord));
		std::memcpy(slot.mapped, instances.data(), size_t(bytes));
		if (vmaFlushAllocation(allocator, slot.allocation, 0, bytes) != VK_SUCCESS)
			return unexpectedError(
				makeError(ErrorCode::ViewportRecordingFailed, ErrorSite::ViewportRecord));
		slot.generation = generation;
		return {};
	}
};
struct Surface {
	TimelineLayout pending{}, presented{};
	std::array<uint64_t, 13> layout_key{};
	std::array<uint64_t, 4> lanes_key{};
	std::vector<TimelineTrackLane> lanes{};
	std::vector<UiInstance> base_instances{};
	std::vector<UiRun> base_runs{};
	std::array<uint64_t, 12> render_key{};
	uint64_t layout_generation = 0, upload_generation = 1;
	size_t uploaded_hover = timeline_no_parent;
	bool pending_ready = false, failed = false;
	[[nodiscard]] const TimelineLayout& scene() const noexcept {
		return pending_ready ? pending : presented;
	}
	std::unique_ptr<SurfaceUpload> upload = std::make_unique<SurfaceUpload>();
	std::string key{};
	Clay_ElementId image_id{}, body_id{}, clip_id{}, column_clip_id{};
	Clay_BoundingBox bounds{}, effective_clip{}, presented_bounds{}, presented_clip{};
	uint64_t identity = 0, last_serial = 0;
	float content_y = 0, presented_content_y = 0, press_x = 0, press_y = 0;
	size_t hovered = timeline_no_parent, pressed = timeline_no_parent;
	size_t last_clicked = timeline_no_parent;
	uint64_t last_click_revision = 0;
	std::chrono::steady_clock::time_point last_click_time{};
	VkFormat format = VK_FORMAT_UNDEFINED;
	bool seen = false, recorded = false, available = false, panning = false, scrubbing = false,
		 primary = false;
};
} // namespace
struct DevTimelineController::Impl {
	std::vector<std::unique_ptr<Surface>> surfaces{};
	std::vector<std::unique_ptr<Surface>> retired{};
	std::vector<UiInstance> instances{};
	std::vector<UiRun> runs{};
	::FlowUi::detail::text::TextLayoutService text{};
	ViewPortManager* viewports = nullptr;
	VulkanUiRenderer* renderer = nullptr;
	const ::FlowUi::detail::manager_storage::FontFrameView* fonts = nullptr;
	float font_scale = 96.0f / 72.0f;
	uint32_t texture_slot = 0;
	uint64_t next_identity = 1;
	bool tooltip_emitted = false;
	TimelineViewportStats stats{};

	void record(const ViewPortRenderContext& context, uint64_t identity) {
		auto found = std::ranges::find_if(
			surfaces, [&](const auto& surface) { return surface->identity == identity; });
		if (found == surfaces.end())
			return;
		auto& surface = **found;
		surface.recorded = false;
		if (!surface.seen || !context.vulkan || !context.commandBuffer || !renderer || !fonts ||
			context.colorFormat != renderer->devReplayTargetFormat() || !context.extent.width ||
			!context.extent.height || surface.bounds.width <= 0 || surface.bounds.height <= 0 ||
			surface.effective_clip.height <= 0)
			return;
		instances.clear();
		runs.clear();
		const auto& scene = surface.scene();
		const float scale_x = context.extent.width / surface.bounds.width;
		const float scale_y = context.extent.height / surface.bounds.height;
		const RectF scissor{(surface.effective_clip.x - surface.bounds.x) * scale_x,
							(surface.effective_clip.y - surface.bounds.y) * scale_y,
							surface.effective_clip.width * scale_x,
							surface.effective_clip.height * scale_y};
		const auto rectangle = [&](Clay_BoundingBox bounds, Clay_Color color) {
			UiInstance instance{};
			instance.x = std::round(bounds.x * scale_x);
			instance.y = std::round((bounds.y - surface.content_y) * scale_y);
			instance.w =
				std::max(1.0f, std::round((bounds.x + bounds.width) * scale_x) - instance.x);
			instance.h = std::max(
				1.0f,
				std::round((bounds.y + bounds.height - surface.content_y) * scale_y) - instance.y);
			instance.colorRGBA = packed(color);
			instances.emplace_back(instance);
		};
		const auto visible = [&](const TimelineDisplayItem& item) {
			return item.screen_space || (item.bounds.y + item.bounds.height > surface.content_y &&
										 item.bounds.y < surface.content_y + surface.bounds.height);
		};
		const std::array<uint64_t, 12> render_key{surface.layout_generation,
												  fonts->publicationRevision,
												  reinterpret_cast<uintptr_t>(fonts->fonts),
												  std::bit_cast<uint32_t>(font_scale),
												  context.extent.width,
												  context.extent.height,
												  std::bit_cast<uint32_t>(surface.content_y),
												  std::bit_cast<uint32_t>(surface.bounds.width),
												  std::bit_cast<uint32_t>(surface.bounds.height),
												  std::bit_cast<uint32_t>(scissor.y),
												  std::bit_cast<uint32_t>(scissor.w),
												  std::bit_cast<uint32_t>(scissor.h)};
		if (render_key != surface.render_key || surface.base_instances.empty()) {
			std::vector<TimelineDisplayItem> render_items;
			render_items.reserve(scene.items.size());
			for (const auto& item : scene.items)
				if (!item.screen_space && visible(item))
					render_items.emplace_back(item);
			for (const auto& item : scene.items)
				if (item.screen_space) {
					render_items.emplace_back(item);
					render_items.back().bounds.y += surface.content_y;
				}
			++stats.base_builds;
			++surface.upload_generation;
			instances.reserve(scene.items.size() * 2 + 1);
			rectangle({0, surface.content_y, surface.bounds.width, surface.bounds.height},
					  interface_theme::kDepth0Keel);
			for (const auto& item : render_items)
				if (visible(item)) {
					auto clipped_bounds = item.bounds;
					if (!item.screen_space)
						clipped_bounds = intersection(
							clipped_bounds,
							{0, surface.content_y + scene.ruler_height, surface.bounds.width,
							 std::max(0.0f, surface.bounds.height - scene.ruler_height)});
					if (clipped_bounds.height > 0)
						rectangle(clipped_bounds, item.color);
				}
			runs.emplace_back(UiRun{UiType::Solid, scissor, 0, uint32_t(instances.size())});
			for (const auto& item : render_items) {
				if (!visible(item) || item.label.empty() || item.bounds.width < 18)
					continue;
				const auto& layout = text.layout({.text = item.label,
												  .fontView = fonts,
												  .pointsToPixelsScale = font_scale,
												  .fontSize = 11,
												  .includeGlyphGeometry = true});
				if (!layout.success)
					continue;
				const uint32_t first = uint32_t(instances.size());
				const float top =
					item.bounds.y + std::max(0.0f, (item.bounds.height - layout.lineHeight) / 2);
				for (const auto& glyph : layout.glyphs) {
					if (glyph.x + item.text_offset_x > item.bounds.width)
						break;
					if (glyph.x + item.text_offset_x + glyph.width < 0)
						continue;
					UiInstance instance{};
					instance.type = uint32_t(UiType::Msdf);
					instance.x = (item.bounds.x + item.text_offset_x + 4 + glyph.x) * scale_x;
					instance.y = (top - surface.content_y + glyph.y) * scale_y;
					instance.w = glyph.width * scale_x;
					instance.h = glyph.height * scale_y;
					instance.colorRGBA = packed(interface_theme::kTextCanvas);
					instance.uv0x = glyph.u0;
					instance.uv0y = glyph.v0;
					instance.uv1x = glyph.u1;
					instance.uv1y = glyph.v1;
					instance.atlasLayer = layout.atlasLayer;
					instance.r0 = layout.distanceRangePx;
					instances.emplace_back(instance);
				}
				const auto clipped = intersection(
					{scissor.x,
					 item.screen_space ? scissor.y
									   : std::max(scissor.y, scene.ruler_height * scale_y),
					 scissor.w,
					 item.screen_space ? scissor.h
									   : std::max(0.0f, scissor.h - scene.ruler_height * scale_y)},
					{item.bounds.x * scale_x, (item.bounds.y - surface.content_y) * scale_y,
					 item.bounds.width * scale_x, item.bounds.height * scale_y});
				if (instances.size() > first)
					runs.emplace_back(UiRun{UiType::Msdf,
											{clipped.x, clipped.y, clipped.width, clipped.height},
											first,
											uint32_t(instances.size()) - first});
			}
			surface.base_instances = instances;
			surface.base_runs = runs;
			surface.render_key = render_key;
		} else {
			instances = surface.base_instances;
			runs = surface.base_runs;
		}
		const uint32_t outline_start = uint32_t(instances.size());
		for (size_t index = 0; index < scene.items.size(); ++index) {
			const auto& item = scene.items[index];
			if (item.bounds.width <= 3 && !item.selected && index != surface.hovered)
				continue;
			if (!visible(item) || (!item.selected && index != surface.hovered &&
								   item.command.action == TimelineAction::None))
				continue;
			const auto bounds = item.bounds;
			const float thickness =
				index == surface.hovered || item.focused_root || item.selected ? 2.0f : 1.0f;
			const auto outline_color = index == surface.hovered ? interface_theme::kTextCanvas
									   : item.selected			? interface_theme::kAccentSeaGlass
									   : item.focused_root		? Flow_Color("#F59E0B")
																: Flow_Color("#10232A");
			rectangle({bounds.x, bounds.y, bounds.width, thickness}, outline_color);
			rectangle({bounds.x, bounds.y + bounds.height - thickness, bounds.width, thickness},
					  outline_color);
			rectangle({bounds.x, bounds.y, std::min(thickness, bounds.width), bounds.height},
					  outline_color);
			rectangle({bounds.x + std::max(0.0f, bounds.width - thickness), bounds.y,
					   std::min(thickness, bounds.width), bounds.height},
					  outline_color);
		}
		if (instances.size() > outline_start)
			runs.emplace_back(UiRun{UiType::Solid,
									{std::max(scissor.x, scene.label_width * scale_x),
									 std::max(scissor.y, scene.ruler_height * scale_y),
									 std::max(0.0f, scissor.w - scene.label_width * scale_x),
									 std::max(0.0f, scissor.h - scene.ruler_height * scale_y)},
									outline_start,
									uint32_t(instances.size()) - outline_start});
		if (surface.uploaded_hover != surface.hovered) {
			surface.uploaded_hover = surface.hovered;
			++surface.upload_generation;
		}
		const bool initialized =
			surface.upload->initialize(*context.vulkan, renderer->devReplayGlobalsLayout())
				.has_value();
		const bool needs_upload =
			initialized && context.frameIndex < surface.upload->slots.size() &&
			surface.upload->slots[context.frameIndex].generation != surface.upload_generation;
		const bool uploaded = initialized && surface.upload->upload(context.frameIndex, instances,
																	surface.upload_generation);
		if (!uploaded) {
			surface.failed = true;
			surface.available = false;
			return;
		}
		surface.failed = false;
		renderer->recordExternalReplay(context.commandBuffer, context.extent,
									   surface.upload->slots[context.frameIndex].descriptors,
									   texture_slot, runs);
		surface.recorded = true;
		++stats.surfaces;
		stats.instances += instances.size();
		stats.runs += runs.size();
		if (needs_upload)
			stats.uploaded_bytes += instances.size() * sizeof(UiInstance);
		stats.visible_pixels += uint64_t(context.extent.width) * context.extent.height;
	}
	void input(Surface& surface, UiManager& manager, DevTimelineState& state, size_t card_index) {
		const auto& input = manager.getCurrentFrameInput();
		const auto& previous = manager.getPreviousFrameInput();
		const bool valid = input.windowFocused && surface.available &&
						   surface.presented.revision == state.snapshot_revision;
		const bool inside = valid && contains(surface.presented_clip, input.mouseX, input.mouseY);
		const float horizontal = input.mouseX - surface.presented_bounds.x;
		const float vertical =
			input.mouseY - surface.presented_bounds.y + surface.presented_content_y;
		const bool plot =
			inside && horizontal >= surface.presented.label_width &&
			input.mouseY - surface.presented_bounds.y >= surface.presented.ruler_height;
		surface.hovered =
			plot ? timeline_hit(surface.presented, horizontal, vertical) : timeline_no_parent;
		if (!valid) {
			surface.panning = surface.primary = surface.scrubbing = false;
			return;
		}
		const bool keyboard_owner =
			surface.presented.kind == TimelineSurfaceKind::Card ||
			(surface.presented.kind == TimelineSurfaceKind::Macro && state.cards.empty());
		const bool next_sample = input.keyDown[264] && !previous.keyDown[264];
		const bool previous_sample = input.keyDown[265] && !previous.keyDown[265];
		if (keyboard_owner && (next_sample || previous_sample) && !input.ctrl && !input.alt &&
			!input.super && !manager.inputFields().hasPrimaryFieldFocus()) {
			std::vector<size_t> candidates;
			candidates.reserve(surface.presented.items.size());
			size_t current_candidate = timeline_no_parent;
			for (size_t item_index = 0; item_index < surface.presented.items.size(); ++item_index) {
				const auto& item = surface.presented.items[item_index];
				if ((item.command.action != TimelineAction::Inspect &&
					 item.command.action != TimelineAction::Open) ||
					item.command.members.empty())
					continue;
				if (std::ranges::find(item.command.members, state.inspected_sample) !=
					item.command.members.end())
					current_candidate = candidates.size();
				candidates.emplace_back(item_index);
			}
			if (!candidates.empty() && state.pending.action == TimelineAction::None) {
				const auto selected = current_candidate == timeline_no_parent
										  ? (next_sample ? 0 : candidates.size() - 1)
									  : next_sample ? (current_candidate + 1) % candidates.size()
									  : current_candidate ? current_candidate - 1
														  : candidates.size() - 1;
				const auto& item = surface.presented.items[candidates[selected]];
				auto command = item.command;
				command.revision = state.snapshot_revision;
				command.index =
					surface.presented.kind == TimelineSurfaceKind::Card ? card_index + 1 : 0;
				if (surface.presented.kind == TimelineSurfaceKind::Card &&
					command.members.size() == 1)
					command.action = TimelineAction::Inspect;
				state.pending = std::move(command);
				if (surface.presented.kind == TimelineSurfaceKind::Card &&
					card_index < state.cards.size()) {
					if (item.bounds.y <
						surface.presented_content_y + surface.presented.ruler_height)
						state.cards[card_index].scroll_y =
							-std::max(0.0f, item.bounds.y - surface.presented.ruler_height);
					else if (item.bounds.y + item.bounds.height >
							 surface.presented_content_y + surface.presented_bounds.height)
						state.cards[card_index].scroll_y =
							-std::max(0.0f, item.bounds.y + item.bounds.height -
												surface.presented_bounds.height);
					state.restore_minor_scroll = true;
				}
			}
		}
		if (plot && input.mouseDown[2] && !previous.mouseDown[2] &&
			surface.presented.kind != TimelineSurfaceKind::Minimap) {
			surface.panning = true;
			if (surface.presented.kind == TimelineSurfaceKind::Macro)
				state.minimap_follow_selection = false;
			surface.primary = false;
		}
		if (!input.mouseDown[2])
			surface.panning = false;
		const double fraction =
			std::clamp(double(horizontal - surface.presented.label_width) /
						   std::max(1.0f, surface.presented.width - surface.presented.label_width),
					   0.0, 1.0);
		if (plot && surface.presented.kind == TimelineSurfaceKind::Minimap && input.scrollY &&
			!input.shift) {
			state.minimap_zoom = std::clamp(
				state.minimap_zoom * std::pow(1.04, std::clamp(double(input.scrollY), -3.0, 3.0)),
				1.0, 1000.0);
		}
		if (((plot && surface.presented.kind == TimelineSurfaceKind::Card) || surface.panning) &&
			surface.presented.kind == TimelineSurfaceKind::Card &&
			card_index < state.cards.size() &&
			(surface.panning || input.scrollX || (input.scrollY && !input.shift))) {
			auto& focus = state.cards[card_index];
			uint64_t domain_start = UINT64_MAX, domain_end = 0;
			for (const auto index : focus.roots) {
				const auto& block = state.snapshot.blocks[index];
				domain_start = std::min(domain_start, block.start_ns);
				domain_end = std::max(domain_end, timeline_end(block.start_ns, block.duration_ns));
			}
			const auto domain_duration = std::max(uint64_t{1}, domain_end - domain_start);
			if (!focus.visible_duration_ns) {
				focus.visible_start_ns = domain_start;
				focus.visible_duration_ns = domain_duration;
			}
			const auto anchor = timeline_end(focus.visible_start_ns,
											 uint64_t(fraction * focus.visible_duration_ns));
			if (plot && input.scrollY && !input.shift) {
				focus.zoom = std::clamp(
					focus.zoom * std::pow(1.04, std::clamp(double(input.scrollY), -3.0, 3.0)), 1.0,
					1000.0);
				focus.visible_duration_ns =
					std::max(uint64_t{1}, uint64_t(domain_duration / focus.zoom));
				const auto offset = uint64_t(fraction * focus.visible_duration_ns);
				focus.visible_start_ns = anchor > offset ? anchor - offset : 0;
			}
			const double delta =
				(surface.panning ? previous.mouseX - input.mouseX : input.scrollX * 24) /
				std::max(1.0f, surface.presented.width - surface.presented.label_width) *
				focus.visible_duration_ns;
			if (delta < 0)
				focus.visible_start_ns -= std::min(focus.visible_start_ns, uint64_t(-delta));
			else
				focus.visible_start_ns = timeline_end(focus.visible_start_ns, uint64_t(delta));
			focus.visible_start_ns =
				std::clamp(focus.visible_start_ns, domain_start,
						   std::max(domain_start, domain_end > focus.visible_duration_ns
													  ? domain_end - focus.visible_duration_ns
													  : domain_start));
		}
		if ((plot && surface.presented.kind == TimelineSurfaceKind::Macro) ||
			(surface.panning && surface.presented.kind == TimelineSurfaceKind::Macro)) {
			if (input.scrollY && plot && !input.shift) {
				state.minimap_follow_selection = false;
				const auto anchor = timeline_end(state.visible_start_ns,
												 uint64_t(fraction * state.visible_duration_ns));
				state.zoom *= std::pow(1.04, std::clamp(double(input.scrollY), -3.0, 3.0));
				clamp_timeline_view(state);
				const auto offset = uint64_t(fraction * state.visible_duration_ns);
				state.visible_start_ns = anchor > offset ? anchor - offset : 0;
			}
			const double delta =
				(surface.panning ? previous.mouseX - input.mouseX : input.scrollX * 24) /
				std::max(1.0f, surface.presented.width - surface.presented.label_width) *
				state.visible_duration_ns;
			if (delta < 0)
				state.visible_start_ns -= std::min(state.visible_start_ns, uint64_t(-delta));
			else
				state.visible_start_ns = timeline_end(state.visible_start_ns, uint64_t(delta));
			if (delta != 0)
				clamp_timeline_view(state);
		}
		if (plot && input.mouseDown[0] && !previous.mouseDown[0] && !surface.panning) {
			surface.primary = true;
			surface.pressed = surface.hovered;
			surface.press_x = input.mouseX;
			surface.press_y = input.mouseY;
			surface.scrubbing = false;
		}
		if (surface.primary && input.mouseDown[0]) {
			const bool moved =
				std::hypot(input.mouseX - surface.press_x, input.mouseY - surface.press_y) >= 3;
			if (surface.presented.kind == TimelineSurfaceKind::Minimap &&
				(moved || surface.pressed == timeline_no_parent)) {
				if (!surface.scrubbing)
					state.cards.clear();
				surface.scrubbing = true;
				center_timeline(state,
								timeline_end(surface.presented.start_ns,
											 uint64_t(fraction * surface.presented.duration_ns)));
			} else if (moved)
				surface.pressed = timeline_no_parent;
		}
		if (surface.primary && !input.mouseDown[0]) {
			if (!surface.scrubbing && surface.pressed != timeline_no_parent &&
				surface.pressed == surface.hovered && plot) {
				auto command = surface.presented.items[surface.pressed].command;
				command.revision = surface.presented.revision;
				if (command.action == TimelineAction::Inspect) {
					const auto now = std::chrono::steady_clock::now();
					if (surface.last_clicked == command.members.front() &&
						surface.last_click_revision == state.snapshot_revision &&
						now - surface.last_click_time < std::chrono::milliseconds(400)) {
						command.action = TimelineAction::Open;
						command.index = card_index + 1;
						surface.last_clicked = timeline_no_parent;
					} else
						surface.last_clicked = command.members.front();
					surface.last_click_revision = state.snapshot_revision;
					surface.last_click_time = now;
				}
				if (surface.presented.kind == TimelineSurfaceKind::Minimap &&
					!command.members.empty()) {
					const auto timestamp =
						timeline_end(surface.presented.start_ns,
									 uint64_t(fraction * surface.presented.duration_ns));
					uint64_t nearest = UINT64_MAX;
					for (const auto index : command.members) {
						const auto start = state.snapshot.frames[index].start_ns;
						const auto distance =
							start > timestamp ? start - timestamp : timestamp - start;
						if (distance < nearest) {
							nearest = distance;
							command.index = index;
						}
					}
					command.members.clear();
				}
				if (state.pending.action == TimelineAction::None)
					state.pending = std::move(command);
			}
			surface.primary = surface.scrubbing = false;
		}
		if (surface.panning || surface.scrubbing)
			surface.hovered = timeline_no_parent;
	}
};
DevTimelineController::DevTimelineController() : impl_(std::make_unique<Impl>()) {}
DevTimelineController::~DevTimelineController() = default;
void DevTimelineController::begin_frame(ViewPortManager& viewports, VulkanUiRenderer& renderer,
										uint64_t completed_serial) {
	impl_->viewports = &viewports;
	impl_->renderer = &renderer;
	impl_->fonts = nullptr;
	impl_->tooltip_emitted = false;
	impl_->stats = {};
	std::erase_if(impl_->retired,
				  [&](const auto& surface) { return surface->last_serial <= completed_serial; });
	for (auto& surface : impl_->surfaces) {
		surface->seen = false;
		surface->recorded = false;
	}
}
void DevTimelineController::prepare(const ::FlowUi::detail::manager_storage::FontFrameView& fonts,
									float font_scale, uint32_t texture_slot) {
	impl_->fonts = &fonts;
	impl_->font_scale = font_scale;
	impl_->texture_slot = texture_slot;
	for (auto& surface : impl_->surfaces) {
		if (!surface->seen) {
			if (auto* viewport = impl_->viewports->getViewPort(surface->key))
				viewport->setRenderCallback({});
			const auto removed = impl_->viewports->remove(surface->key);
			if (!removed)
				surface->available = false;
			impl_->retired.emplace_back(std::move(surface));
			continue;
		}
		const auto image = Clay_GetElementData(surface->image_id);
		const auto body = Clay_GetElementData(surface->body_id);
		const auto clip = Clay_GetElementData(surface->clip_id);
		if (!image.found || !body.found ||
			std::abs(image.boundingBox.width - surface->scene().width) > .5f) {
			surface->effective_clip = {};
			surface->available = false;
			continue;
		}
		if (surface->available &&
			(std::abs(image.boundingBox.width - surface->presented_bounds.width) > .5f ||
			 std::abs(image.boundingBox.x - surface->presented_bounds.x) > .5f ||
			 std::abs(image.boundingBox.y - surface->presented_bounds.y) > .5f)) {
			surface->primary = surface->panning = surface->scrubbing = false;
			surface->last_clicked = timeline_no_parent;
			surface->hovered = timeline_no_parent;
		}
		surface->bounds = image.boundingBox;
		surface->effective_clip = intersection(image.boundingBox, body.boundingBox);
		if (clip.found)
			surface->effective_clip = intersection(surface->effective_clip, clip.boundingBox);
		const auto column = Clay_GetElementData(surface->column_clip_id);
		if (column.found)
			surface->effective_clip = intersection(surface->effective_clip, column.boundingBox);
		// Use the actual floating image offset; a large scroll never changes the meaning of pixels.
		surface->content_y = image.boundingBox.y - body.boundingBox.y;
	}
	std::erase(impl_->surfaces, nullptr);
}
TimelineViewportStats DevTimelineController::stats() const noexcept {
	return impl_->stats;
}
void DevTimelineController::submitted(uint64_t serial) {
	for (auto& surface : impl_->surfaces) {
		if (!surface->recorded) {
			surface->available = false;
			continue;
		}
		surface->last_serial = serial;
		if (surface->pending_ready)
			surface->presented = std::move(surface->pending);
		surface->pending_ready = false;
		surface->available = true;
		surface->presented_bounds = surface->bounds;
		surface->presented_clip = surface->effective_clip;
		surface->presented_content_y = surface->content_y;
	}
}
void DevTimelineController::destroy_drained() {
	if (impl_->viewports)
		for (const auto& surface : impl_->surfaces)
			if (auto* viewport = impl_->viewports->getViewPort(surface->key))
				viewport->setRenderCallback({});
	impl_->surfaces.clear();
	impl_->retired.clear();
}
bool DevTimelineController::owns_scroll(const FrameInput& input) const noexcept {
	if (input.shift)
		return false;
	for (const auto& surface : impl_->surfaces)
		if (surface->available && contains(surface->presented_clip, input.mouseX, input.mouseY) &&
			input.mouseY - surface->presented_bounds.y >= surface->presented.ruler_height &&
			input.mouseX - surface->presented_bounds.x >= surface->presented.label_width)
			return true;
	return false;
}
void DevTimelineController::draw(UiManager& manager, Clay_ElementId id, Clay_ElementId clip,
								 DevTimelineState& state, const DevPerformanceSelection& selection,
								 TimelineSurfaceKind kind, size_t card_index,
								 Clay_ElementId column_clip) {
	const auto measured = Clay_GetElementData(id);
	const auto clip_data = Clay_GetElementData(clip);
	const auto column_data = Clay_GetElementData(column_clip);
	const float candidate_width =
		measured.found && clip_data.found
			? std::min(measured.boundingBox.width, clip_data.boundingBox.width)
		: measured.found  ? measured.boundingBox.width
		: clip_data.found ? clip_data.boundingBox.width
						  : 600;
	const float width = column_data.found ? std::min(candidate_width, column_data.boundingBox.width)
										  : candidate_width;
	if (measured.found && clip_data.found &&
		intersection(measured.boundingBox, clip_data.boundingBox).height <= 0) {
		const auto layout = build_timeline_layout(state, selection, kind, card_index, width);
		Clay_ElementDeclaration placeholder{};
		placeholder.layout.sizing = {.width = CLAY_SIZING_GROW(0),
									 .height = CLAY_SIZING_FIXED(layout.height)};
		CLAY(id, placeholder);
		return;
	}
	const auto card_identity = kind == TimelineSurfaceKind::Card && card_index < state.cards.size()
								   ? state.cards[card_index].surface_identity
								   : 0;
	const std::string key = "performance.timeline." + std::to_string(manager.windowId()) + "." +
							std::to_string(id.id) + "." +
							std::to_string(impl_->renderer->devReplayTargetFormat());
	auto found = std::ranges::find_if(impl_->surfaces,
									  [&](const auto& surface) { return surface->key == key; });
	if (found == impl_->surfaces.end()) {
		auto surface = std::make_unique<Surface>();
		surface->key = key;
		surface->identity = impl_->next_identity++;
		surface->format = impl_->renderer->devReplayTargetFormat();
		const auto created = impl_->viewports->create(
			key, {.colorFormat = surface->format, .clearColor = {0, 0, 0, 0}});
		if (!created) {
			Clay_TextElementConfig text{};
			text.fontSize = 11;
			text.textColor = interface_theme::kTextSecondary;
			CLAY_TEXT(
				manager.toClayString("Timeline viewport unavailable: target creation failed."),
				CLAY_TEXT_CONFIG(text));
			return;
		}
		if (auto* viewport = impl_->viewports->getViewPort(key))
			viewport->setRenderCallback([controller = impl_.get(), identity = surface->identity](
											const ViewPortRenderContext& context) {
				try {
					controller->record(context, identity);
				} catch (...) {
					for (auto& entry : controller->surfaces)
						if (entry->identity == identity) {
							entry->available = entry->recorded = false;
							entry->failed = true;
						}
				}
			});
		impl_->surfaces.emplace_back(std::move(surface));
		found = std::prev(impl_->surfaces.end());
	}
	auto& surface = **found;
	impl_->input(surface, manager, state, card_index);
	surface.body_id = id;
	surface.clip_id = clip;
	surface.column_clip_id = column_clip;
	const uint64_t depth = kind == TimelineSurfaceKind::Card && card_index < state.cards.size()
							   ? state.cards[card_index].active_depth
							   : state.active_depth;
	const std::array<uint64_t, 13> layout_key{
		state.snapshot_revision,
		(kind == TimelineSurfaceKind::Card && card_index < state.cards.size()
			 ? state.cards[card_index].visible_start_ns
			 : state.visible_start_ns),
		(kind == TimelineSurfaceKind::Card && card_index < state.cards.size()
			 ? state.cards[card_index].visible_duration_ns
			 : state.visible_duration_ns),
		state.selected_frame,
		state.inspected_sample,
		(kind == TimelineSurfaceKind::Macro ? state.track_preferences_revision : 0),
		selection.category_mask,
		selection.hardware_domain,
		depth,
		(kind == TimelineSurfaceKind::Macro && !state.cards.empty()
			 ? state.cards.front().surface_identity
			 : card_identity),
		std::bit_cast<uint32_t>(width),
		uint64_t(kind),
		std::bit_cast<uint64_t>(state.minimap_zoom)};
	if (surface.layout_key != layout_key || surface.scene().height == 0) {
		const std::array<uint64_t, 4> lanes_key{
			state.snapshot_revision, depth, card_identity,
			kind == TimelineSurfaceKind::Macro ? state.track_preferences_revision : 0};
		if (kind != TimelineSurfaceKind::Minimap && surface.lanes_key != lanes_key) {
			const std::span<const size_t> roots =
				kind == TimelineSurfaceKind::Card && card_index < state.cards.size()
					? std::span<const size_t>(state.cards[card_index].roots)
					: std::span<const size_t>{};
			surface.lanes =
				kind == TimelineSurfaceKind::Macro
					? timeline_major_lanes(state.snapshot, selection, state.track_preferences)
					: timeline_lanes(state.snapshot, uint32_t(depth), roots);
			surface.lanes_key = lanes_key;
		}
		surface.pending =
			build_timeline_layout(state, selection, kind, card_index, width, &surface.lanes);
		++impl_->stats.layout_builds;
		surface.pending_ready = true;
		surface.layout_key = layout_key;
		++surface.layout_generation;
		surface.hovered = timeline_no_parent;
		if (!surface.panning && !surface.scrubbing)
			surface.primary = false;
	}
	Clay_ElementDeclaration placeholder{};
	placeholder.layout.sizing = {.width = CLAY_SIZING_GROW(0),
								 .height = CLAY_SIZING_FIXED(surface.scene().height)};
	placeholder.backgroundColor = interface_theme::kDepth0Keel;
	placeholder.clip.horizontal = true;
	placeholder.clip.scrollInputDisabled = true;
	CLAY(id, placeholder) {
		if (surface.failed) {
			Clay_TextElementConfig text{};
			text.fontSize = 11;
			text.textColor = interface_theme::kTextSecondary;
			CLAY_TEXT(
				manager.toClayString("Timeline viewport unavailable: rendering resources failed."),
				CLAY_TEXT_CONFIG(text));
		}
		float offset = 0;
		float height = std::min(surface.scene().height,
								clip_data.found ? clip_data.boundingBox.height : 800.0f);
		if (measured.found && clip_data.found) {
			const auto visible = intersection(measured.boundingBox, clip_data.boundingBox);
			offset = std::max(0.0f, visible.y - measured.boundingBox.y);
			height = visible.height;
		}
		if (height > 0 && width > 0) {
			surface.seen = true;
			surface.content_y = offset;
			surface.image_id =
				Clay_GetElementIdWithIndex(CLAY_STRING("timeline.viewport.image"), id.id);
			Clay_ElementDeclaration image{};
			image.layout.sizing = {.width = CLAY_SIZING_FIXED(width),
								   .height = CLAY_SIZING_FIXED(height)};
			image.floating.attachTo = CLAY_ATTACH_TO_PARENT;
			image.floating.clipTo = CLAY_CLIP_TO_ATTACHED_PARENT;
			image.floating.offset.y = offset;
			image.floating.pointerCaptureMode = CLAY_POINTER_CAPTURE_MODE_PASSTHROUGH;
			auto texture = impl_->viewports->getTexture(key);
			texture.fitMode = TextureFitMode::Stretch;
			image.image.imageData = manager.imageData(texture);
			CLAY(surface.image_id, image);
			if (!impl_->tooltip_emitted && surface.hovered < surface.presented.items.size() &&
				surface.presented.revision == state.snapshot_revision) {
				const auto detail = timeline_item_detail(state, surface.presented, surface.hovered);
				if (!detail.empty()) {
					impl_->tooltip_emitted = true;
					const auto& input = manager.getCurrentFrameInput();
					Clay_ElementDeclaration tooltip{};
					tooltip.layout.sizing = {.width = CLAY_SIZING_FIXED(std::min(420.0f, width)),
											 .height = CLAY_SIZING_FIT(0)};
					tooltip.layout.padding = {8, 8, 6, 6};
					tooltip.backgroundColor = interface_theme::kDepth3Elevated;
					tooltip.border = {.color = interface_theme::kAccentSeaGlass,
									  .width = {1, 1, 1, 1, 0}};
					tooltip.floating.attachTo = CLAY_ATTACH_TO_ROOT;
					tooltip.floating.offset = {
						std::max(0.0f, std::min(input.mouseX + 12,
												surface.presented_bounds.x + width - 420)),
						std::max(0.0f, input.mouseY - 110)};
					tooltip.floating.zIndex = 100;
					tooltip.floating.pointerCaptureMode = CLAY_POINTER_CAPTURE_MODE_PASSTHROUGH;
					CLAY(Clay_GetElementIdWithIndex(CLAY_STRING("timeline.tooltip"), id.id),
						 tooltip) {
						Clay_TextElementConfig text{};
						text.fontSize = 11;
						text.textColor = interface_theme::kTextCanvas;
						CLAY_TEXT(manager.toClayString(detail), CLAY_TEXT_CONFIG(text));
					}
				}
			}
		}
	}
}
void timeline_viewport(UiManager& manager, Clay_ElementId id, DevTimelineParameters parameters,
					   TimelineSurfaceKind kind) {
	if (auto* controller = manager.timeline_controller();
		controller && parameters.timeline && parameters.selection)
		controller->draw(manager, id, parameters.canvas_clip, *parameters.timeline,
						 *parameters.selection, kind, parameters.card_index,
						 parameters.column_clip);
}
} // namespace FlowUi::devSystems::interface_elements
#elif FLOW_UI_DEV_MODE
namespace FlowUi::devSystems::interface_elements {
void timeline_viewport(UiManager& manager, Clay_ElementId, DevTimelineParameters,
					   TimelineSurfaceKind) {
	Clay_TextElementConfig text{};
	text.fontSize = 11;
	text.textColor = interface_theme::kTextSecondary;
	CLAY_TEXT(manager.toClayString("Timeline viewport unavailable: Vulkan interop is disabled."),
			  CLAY_TEXT_CONFIG(text));
}
} // namespace FlowUi::devSystems::interface_elements
#endif
