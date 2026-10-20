#include "devSystems/devInterface/Memory/Workbench/DevMemoryHistoryViewport.hpp"
#if FLOW_UI_DEV_MODE && FLOWUI_PUBLIC_VULKAN_INTEROP
#include "Ui/Vk_UiRenderer.hpp"
#include "devSystems/devInterface/Permanents/Backend/DevTheme.hpp"
#include "devSystems/devMonitoringAndReporting/memory/DevMemorySources.hpp"
#include "internal/ManagerStorage/FontCatalogController.hpp"
#include "internal/Text/TextLayoutService.hpp"
#include "internal/Vma.hpp"
#include "managers/ViewPortManager.hpp"
#include <GLFW/glfw3.h>
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstring>
namespace FlowUi::devSystems::interface_elements {
namespace {
constexpr float gutter = 68, navigator_height = 18;
[[nodiscard]] bool contains(Clay_BoundingBox bounds, float horizontal, float vertical) noexcept {
	return horizontal >= bounds.x && vertical >= bounds.y && horizontal < bounds.x + bounds.width &&
		   vertical < bounds.y + bounds.height;
}
[[nodiscard]] uint32_t packed(Clay_Color color) noexcept {
	return uint32_t(color.r) | uint32_t(color.g) << 8 | uint32_t(color.b) << 16 |
		   uint32_t(color.a) << 24;
}
[[nodiscard]] std::string reading(uint64_t value, bool counts, bool decimal) {
	return counts ? std::to_string(value) : memory_bytes(value, decimal);
}
[[nodiscard]] std::string axis_reading(uint64_t value, bool counts, bool decimal) {
	if (counts && value >= 1'000'000) {
		char compact[32];
		std::snprintf(compact, sizeof(compact), "%.1f%s",
					  double(value) / (value >= 1'000'000'000 ? 1e9 : 1e6),
					  value >= 1'000'000'000 ? "G" : "M");
		return compact;
	}
	auto label = reading(value, counts, decimal);
	const auto point = label.find('.');
	if (point != std::string::npos) {
		auto end = label.find(' ', point);
		if (end != std::string::npos) {
			while (end > point + 1 && label[end - 1] == '0')
				label.erase(--end, 1);
			if (end == point + 1)
				label.erase(point, 1);
		}
	}
	return label;
}
[[nodiscard]] std::string change(uint64_t before, uint64_t after, bool counts, bool decimal) {
	return std::string(after >= before ? "+" : "-") +
		   reading(after >= before ? after - before : before - after, counts, decimal);
}
[[nodiscard]] std::string_view active_label(const MemoryHistoryLane& lane) noexcept {
	return lane.metric											? "Objects"
		   : lane.key.source == memory_sources::kProcess.id		? "Resident"
		   : lane.key.source == memory_sources::kVulkanHeaps.id ? "Allocations"
																: "Active";
}
[[nodiscard]] std::string_view backing_label(const MemoryHistoryLane& lane) noexcept {
	return lane.metric											? "Capacity"
		   : lane.key.source == memory_sources::kProcess.id		? "Virtual"
		   : lane.key.source == memory_sources::kVulkanHeaps.id ? "Blocks"
																: "Backing";
}
[[nodiscard]] std::string_view operation_label(MemoryOperation operation) noexcept {
	constexpr std::array<std::string_view, 14> names{
		"Allocate",		"Release",	   "Arena reset",	   "Pool merge",	  "Backing grow",
		"Backing trim", "Reserve",	   "Reallocate",	   "Rehash",		  "GPU create",
		"GPU retire",	"GPU destroy", "External acquire", "External release"};
	const auto index = size_t(operation);
	return index < names.size() ? names[index] : "Operation";
}
[[nodiscard]] Clay_Color operation_color(MemoryOperation operation) noexcept {
	switch (operation) {
	case MemoryOperation::BackingGrow:
	case MemoryOperation::ContainerReallocate:
	case MemoryOperation::ContainerRehash:
		return interface_theme::kStatusAmber;
	case MemoryOperation::BackingTrim:
	case MemoryOperation::ArenaReset:
	case MemoryOperation::PoolBlockMerge:
	case MemoryOperation::GpuDestroy:
		return interface_theme::kAccentSeaGlass;
	default:
		return interface_theme::kTextSecondary;
	}
}
struct Rectangle {
	Clay_BoundingBox bounds{};
	Clay_Color color{};
};
struct Label {
	std::string value{};
	Clay_BoundingBox bounds{};
	Clay_Color color{};
	bool right = false;
};
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
} // namespace
struct DevMemoryHistoryController::Impl {
	std::vector<Rectangle> pending{};
	std::vector<Label> labels{};
	std::vector<UiInstance> instances{};
	std::vector<UiRun> runs{};
	std::vector<MemoryHistoryLane> presented_lanes{};
	std::unique_ptr<SurfaceUpload> upload = std::make_unique<SurfaceUpload>();
	::FlowUi::detail::text::TextLayoutService text{};
	ViewPortManager* viewports = nullptr;
	VulkanUiRenderer* renderer = nullptr;
	DevMemoryWorkbenchState* state = nullptr;
	DevMemoryReporting* reporting = nullptr;
	const ::FlowUi::detail::manager_storage::FontFrameView* fonts = nullptr;
	Clay_ElementId image{};
	Clay_BoundingBox bounds{}, presented_bounds{};
	std::string key = "flowui.memory.history";
	uint64_t presented_begin = 0, presented_end = 0, presented_retained_begin = 0,
			 presented_retained_end = 0;
	uint64_t presented_settings = 0, presented_scope = 0, presented_window = 0;
	uint64_t press_ns = 0, press_begin = 0, press_end = 0, generation = 0, hover_ns = 0,
			 last_click_ns = 0, navigator_offset_ns = 0;
	float font_scale = 1, press_x = 0, last_click_x = 0;
	uint32_t texture_slot = 0;
	size_t hover_lane = 0;
	int drag_button = -1, navigator_drag = 0;
	bool seen = false, was_seen = false, created = false, recorded = false, replay_failed = false,
		 dragged = false, keyboard_focus = false;
	void pause() {
		if (state && reporting) {
			hold_memory_history(*state, *reporting);
			state->following = false;
		}
	}
	void record(const ViewPortRenderContext& context) {
		if (!seen || !context.vulkan || !renderer || !fonts || !context.commandBuffer ||
			bounds.width <= gutter || bounds.height <= navigator_height ||
			context.colorFormat != renderer->devReplayTargetFormat())
			return;
		instances.clear();
		runs.clear();
		const float scale_x = context.extent.width / bounds.width,
					scale_y = context.extent.height / bounds.height;
		const RectF scissor{0, 0, float(context.extent.width), float(context.extent.height)};
		instances.reserve(pending.size() + labels.size() * 24);
		for (const auto& rectangle : pending) {
			UiInstance instance{};
			instance.x = std::round(rectangle.bounds.x * scale_x);
			instance.y = std::round(rectangle.bounds.y * scale_y);
			instance.w =
				std::max(1.0f, std::round((rectangle.bounds.x + rectangle.bounds.width) * scale_x) -
								   instance.x);
			instance.h = std::max(
				1.0f,
				std::round((rectangle.bounds.y + rectangle.bounds.height) * scale_y) - instance.y);
			instance.colorRGBA = packed(rectangle.color);
			instances.emplace_back(instance);
		}
		runs.emplace_back(UiRun{UiType::Solid, scissor, 0, uint32_t(instances.size())});
		for (const auto& label : labels) {
			const auto& layout = text.layout({.text = label.value,
											  .fontView = fonts,
											  .pointsToPixelsScale = font_scale,
											  .fontSize = 10,
											  .includeGlyphGeometry = true});
			if (!layout.success)
				continue;
			float text_width = 0;
			for (const auto& glyph : layout.glyphs)
				text_width = std::max(text_width, glyph.x + glyph.width);
			const float offset =
				label.right ? std::max(0.0f, label.bounds.width - text_width - 4) : 2;
			const float top =
				label.bounds.y + std::max(0.0f, (label.bounds.height - layout.lineHeight) / 2);
			const auto first = uint32_t(instances.size());
			for (const auto& glyph : layout.glyphs) {
				UiInstance instance{};
				instance.type = uint32_t(UiType::Msdf);
				instance.x = (label.bounds.x + offset + glyph.x) * scale_x;
				instance.y = (top + glyph.y) * scale_y;
				instance.w = glyph.width * scale_x;
				instance.h = glyph.height * scale_y;
				instance.colorRGBA = packed(label.color);
				instance.uv0x = glyph.u0;
				instance.uv0y = glyph.v0;
				instance.uv1x = glyph.u1;
				instance.uv1y = glyph.v1;
				instance.atlasLayer = layout.atlasLayer;
				instance.r0 = layout.distanceRangePx;
				instances.emplace_back(instance);
			}
			if (instances.size() > first)
				runs.emplace_back(
					UiRun{UiType::Msdf,
						  {label.bounds.x * scale_x, label.bounds.y * scale_y,
						   label.bounds.width * scale_x, label.bounds.height * scale_y},
						  first,
						  uint32_t(instances.size()) - first});
		}
		if (!upload->initialize(*context.vulkan, renderer->devReplayGlobalsLayout()) ||
			!upload->upload(context.frameIndex, instances, ++generation)) {
			if (state)
				state->viewport_error = "History upload failed";
			return;
		}
		renderer->recordExternalReplay(context.commandBuffer, context.extent,
									   upload->slots[context.frameIndex].descriptors, texture_slot,
									   runs);
		replay_failed = false;
		recorded = true;
	}
};
DevMemoryHistoryController::DevMemoryHistoryController() : impl_(std::make_unique<Impl>()) {}
DevMemoryHistoryController::~DevMemoryHistoryController() = default;
void DevMemoryHistoryController::begin_frame(ViewPortManager& viewports, VulkanUiRenderer& renderer,
											 uint64_t completed_serial) {
	auto& controller = *impl_;
	controller.viewports = &viewports;
	controller.renderer = &renderer;
	// Upload buffers are reused only after the hosting frame-slot fence; there are no retired
	// surfaces.
	controller.generation = std::max(controller.generation, completed_serial);
	if (!controller.seen) {
		if (controller.state) {
			controller.state->following = controller.state->analysis.dataset == 0;
			controller.state->held_data.reset();
			controller.state->held_events.clear();
			controller.state->selected_ns = controller.state->hovered_ns = 0;
			controller.state->range_begin_ns = controller.state->range_end_ns = 0;
			controller.state->history_settings_open = controller.state->history_window_open = false;
			controller.state->next_live_ns = controller.state->next_history_ns = 0;
		}
		controller.state = nullptr;
		controller.reporting = nullptr;
		controller.presented_bounds = {};
		controller.drag_button = -1;
	}
	controller.was_seen = controller.seen;
	controller.seen = false;
	controller.recorded = false;
	controller.fonts = nullptr;
}
void DevMemoryHistoryController::draw(UiManager& manager, Clay_ElementId image,
									  DevMemoryWorkbenchState& state,
									  DevMemoryReporting& reporting) {
	auto& controller = *impl_;
	controller.state = &state;
	controller.reporting = &reporting;
	controller.seen = true;
	controller.image = image;
	state.externally_paused = reporting.status().freeze_owner_count > 0;
	const auto& input = manager.getCurrentFrameInput();
	const auto& previous = manager.getPreviousFrameInput();
	const auto bounds = controller.presented_bounds;
	const bool valid = controller.presented_scope == state.scope &&
					   controller.presented_window == state.window &&
					   controller.presented_settings == state.history_settings_key &&
					   controller.presented_end > controller.presented_begin &&
					   controller.presented_retained_end > controller.presented_retained_begin &&
					   bounds.width > gutter && bounds.height > navigator_height;
	const bool covered = state.history_settings_open || state.history_window_open;
	if (!valid || covered || !input.windowFocused)
		controller.drag_button = -1;
	const bool hovered = valid && !covered && contains(bounds, input.mouseX, input.mouseY);
	const float plot_width = std::max(1.0f, bounds.width - gutter - 8);
	const bool navigator = hovered && input.mouseX >= bounds.x + gutter &&
						   input.mouseX <= bounds.x + gutter + plot_width &&
						   input.mouseY >= bounds.y + bounds.height - navigator_height;
	const bool plot = hovered && input.mouseY < bounds.y + bounds.height - navigator_height &&
					  input.mouseX >= bounds.x + gutter;
	const auto fraction = [&](float horizontal) {
		return std::clamp(double(horizontal - bounds.x - gutter) / plot_width, 0.0, 1.0);
	};
	const auto time_at = [&](float horizontal) {
		return controller.presented_begin +
			   uint64_t(fraction(horizontal) *
						(controller.presented_end - controller.presented_begin));
	};
	controller.hover_ns = plot ? time_at(input.mouseX) : 0;
	state.hovered_ns = controller.hover_ns;
	if (plot && !controller.presented_lanes.empty())
		controller.hover_lane = std::min(controller.presented_lanes.size() - 1,
										 size_t(std::max(0.0f, input.mouseY - bounds.y) /
												std::max(1.0f, bounds.height - navigator_height) *
												controller.presented_lanes.size()));
	if ((plot || navigator) && controller.drag_button < 0) {
		for (int button = 0; button < (navigator ? 1 : 3); ++button)
			if (input.mouseDown[size_t(button)] && !previous.mouseDown[size_t(button)]) {
				controller.pause();
				controller.drag_button = button;
				controller.dragged = false;
				controller.keyboard_focus = true;
				controller.press_x = input.mouseX;
				controller.press_ns = time_at(input.mouseX);
				controller.press_begin = state.view_begin_ns;
				controller.press_end = state.view_end_ns;
				controller.navigator_drag = 0;
				if (navigator) {
					const auto retained = std::max<uint64_t>(
						1, controller.presented_retained_end - controller.presented_retained_begin);
					const float left = bounds.x + gutter +
									   float(double(controller.presented_begin -
													controller.presented_retained_begin) /
											 retained) *
										   plot_width;
					const float right = bounds.x + gutter +
										float(double(controller.presented_end -
													 controller.presented_retained_begin) /
											  retained) *
											plot_width;
					const auto left_distance = std::abs(input.mouseX - left);
					const auto right_distance = std::abs(input.mouseX - right);
					controller.navigator_drag = std::min(left_distance, right_distance) <= 5
													? (left_distance <= right_distance ? 2 : 3)
													: 1;
					const auto timestamp = controller.presented_retained_begin +
										   uint64_t(fraction(input.mouseX) * retained);
					// Preserve the grab position inside the curtain; outside clicks center it.
					controller.navigator_offset_ns =
						input.mouseX >= left && input.mouseX <= right
							? timestamp - controller.presented_begin
							: (controller.press_end - controller.press_begin) / 2;
				}
				break;
			}
	}
	const auto move_view = [&](long double begin, uint64_t width) {
		width = std::min(width, state.retained_end_ns - state.retained_begin_ns);
		state.view_begin_ns =
			uint64_t(std::clamp(begin, static_cast<long double>(state.retained_begin_ns),
								static_cast<long double>(state.retained_end_ns - width)));
		state.view_end_ns = state.view_begin_ns + width;
		state.duration_ns = width;
		state.next_history_ns = 0;
	};
	if (controller.drag_button >= 0 && valid && input.mouseDown[size_t(controller.drag_button)]) {
		const auto delta = input.mouseX - controller.press_x;
		controller.dragged |= std::abs(delta) >= 3;
		if (controller.navigator_drag) {
			const auto timestamp =
				controller.presented_retained_begin +
				uint64_t(fraction(input.mouseX) *
						 (controller.presented_retained_end - controller.presented_retained_begin));
			const auto width = controller.press_end - controller.press_begin;
			if (controller.navigator_drag == 2) {
				state.view_begin_ns =
					std::clamp(timestamp, state.retained_begin_ns, controller.press_end - 1);
				state.view_end_ns = controller.press_end;
			} else if (controller.navigator_drag == 3) {
				state.view_begin_ns = controller.press_begin;
				state.view_end_ns =
					std::clamp(timestamp, controller.press_begin + 1, state.retained_end_ns);
			} else
				move_view(static_cast<long double>(timestamp) - controller.navigator_offset_ns,
						  width);
			state.duration_ns = state.view_end_ns - state.view_begin_ns;
			state.next_history_ns = 0;
		} else if (controller.dragged && controller.drag_button == 0) {
			const auto current = time_at(input.mouseX);
			state.selected_ns = 0;
			state.range_begin_ns = std::min(controller.press_ns, current);
			state.range_end_ns = std::max(controller.press_ns, current);
			state.next_history_ns = 0;
			state.analysis.last_refresh_ns = 0;
		} else if (controller.dragged)
			move_view(static_cast<long double>(controller.press_begin) -
						  double(delta) / plot_width *
							  (controller.press_end - controller.press_begin),
					  controller.press_end - controller.press_begin);
	}
	if (controller.drag_button >= 0 && !input.mouseDown[size_t(controller.drag_button)]) {
		if (plot && controller.drag_button == 0 && !controller.dragged &&
			!controller.navigator_drag) {
			const auto now = memory_now_ns();
			if (now - controller.last_click_ns < 350'000'000ull &&
				std::abs(input.mouseX - controller.last_click_x) < 3)
				state.selected_ns = state.range_begin_ns = state.range_end_ns = 0;
			else {
				state.selected_ns = time_at(input.mouseX);
				state.range_begin_ns = state.range_end_ns = 0;
			}
			controller.last_click_ns = now;
			controller.last_click_x = input.mouseX;
			state.next_live_ns = state.next_history_ns = 0;
		}
		controller.drag_button = -1;
	}
	if (plot && input.scrollY) {
		controller.pause();
		if (input.shift)
			move_view(static_cast<long double>(state.view_begin_ns) +
						  input.scrollY * .1 * (state.view_end_ns - state.view_begin_ns),
					  state.view_end_ns - state.view_begin_ns);
		else
			zoom_memory_view(state, std::pow(1.25, -input.scrollY), fraction(input.mouseX));
	}
	if (controller.keyboard_focus && valid && !covered && input.windowFocused &&
		!manager.inputFields().hasPrimaryFieldFocus()) {
		const auto pressed = [&](int key) {
			return input.keyDown[size_t(key)] && !previous.keyDown[size_t(key)];
		};
		if (pressed(GLFW_KEY_LEFT) || pressed(GLFW_KEY_RIGHT)) {
			controller.pause();
			pan_memory_view(state, pressed(GLFW_KEY_LEFT) ? -1 : 1);
		}
		if (pressed(GLFW_KEY_EQUAL) || pressed(GLFW_KEY_MINUS)) {
			controller.pause();
			zoom_memory_view(state, pressed(GLFW_KEY_EQUAL) ? .5 : 2);
		}
		if (pressed(GLFW_KEY_HOME)) {
			controller.pause();
			state.fit_pending = true;
			state.next_history_ns = 0;
		}
		if (pressed(GLFW_KEY_ESCAPE)) {
			state.selected_ns = state.range_begin_ns = state.range_end_ns = 0;
			state.next_live_ns = state.next_history_ns = 0;
		}
	}
	// A gesture changes the time transform immediately. Reproject held evidence before
	// publishing that transform, so submitted geometry and hit testing describe one scene.
	if (!state.next_history_ns) {
		build_memory_history(state, reporting, size_t(plot_width));
		state.next_history_ns = memory_now_ns() + 100'000'000ull;
	}
	state.inspected_lane = controller.hover_lane;
	state.tooltip.clear();
	if (!state.lanes.empty()) {
		const auto& lane = state.lanes[std::min(controller.hover_lane, state.lanes.size() - 1)];
		const auto& range = lane.range;
		const auto field_reading = [&](uint64_t value, uint16_t fields, MemoryValueField field) {
			return fields & uint16_t(field) ? reading(value, lane.metric, state.decimal)
											: "unavailable";
		};
		if (state.range_end_ns > state.range_begin_ns) {
			char duration[48];
			std::snprintf(duration, sizeof(duration), "Range %.3f s",
						  double(range.duration_ns) / 1e9);
			state.tooltip = duration;
			const auto metric_summary = [&](std::string_view label, uint64_t before, uint64_t after,
											MemoryValueField field) {
				state.tooltip += " · " + std::string(label) + ": " +
								 field_reading(before, range.before_fields, field) + " -> " +
								 field_reading(after, range.after_fields, field);
				if ((range.before_fields & range.after_fields) & uint16_t(field))
					state.tooltip += " (" + change(before, after, lane.metric, state.decimal) + ")";
			};
			metric_summary(active_label(lane), range.active_before, range.active_after,
						   MemoryValueField::LogicalLive);
			metric_summary(backing_label(lane), range.backing_before, range.backing_after,
						   MemoryValueField::Backing);
			if (range.incomplete)
				state.tooltip += " · incomplete coverage";
		} else {
			const auto timestamp = controller.hover_ns ? controller.hover_ns : state.selected_ns;
			if (timestamp >= state.view_begin_ns && timestamp < state.view_end_ns &&
				!lane.columns.empty()) {
				const auto index = std::min(
					lane.columns.size() - 1,
					size_t(static_cast<long double>(timestamp - state.view_begin_ns) *
						   lane.columns.size() / (state.view_end_ns - state.view_begin_ns)));
				const auto& value = lane.columns[index];
				char time[48];
				std::snprintf(time, sizeof(time), "t = %+.3f s",
							  -double(state.view_end_ns - timestamp) / 1e9);
				state.tooltip = time;
				if (lane.point_ns == timestamp && lane.point_available) {
					const auto& point = lane.point;
					const auto active_field =
						lane.metric ? MemoryValueField::Objects : MemoryValueField::LogicalLive;
					const auto backing_field =
						lane.metric ? MemoryValueField::Capacity : MemoryValueField::Backing;
					const auto active = lane.metric ? point.objectCount : point.logicalLiveBytes;
					const auto backing =
						lane.metric ? point.capacityCount : point.backingAllocatedBytes;
					state.tooltip += " · " + std::string(active_label(lane)) + ": " +
									 field_reading(active, point.value_fields, active_field);
					state.tooltip += " · " + std::string(backing_label(lane)) + ": " +
									 field_reading(backing, point.value_fields, backing_field);
					if (!lane.metric && (point.value_fields & uint16_t(MemoryValueField::Reusable)))
						state.tooltip +=
							" · Reusable " + memory_bytes(point.reusableBytes, state.decimal);
					if (uint16_t(point.flags) & uint16_t(MemorySampleFlag::Incomplete))
						state.tooltip += " · incomplete observation";
				} else if (!value.available || value.mixed_epochs || value.gap)
					state.tooltip += " · No continuous retained observation";
				else
					state.tooltip += " · Bucket " + std::string(active_label(lane)) + " " +
									 field_reading(value.active_min, value.value_fields,
												   MemoryValueField::LogicalLive) +
									 "–" +
									 field_reading(value.active_max, value.value_fields,
												   MemoryValueField::LogicalLive) +
									 " · " + std::string(backing_label(lane)) + " " +
									 field_reading(value.backing_min, value.value_fields,
												   MemoryValueField::Backing) +
									 "–" +
									 field_reading(value.backing_max, value.value_fields,
												   MemoryValueField::Backing);

				if (plot)
					for (const auto& event : lane.events) {
						const auto distance =
							std::abs(static_cast<long double>(event.timestampNs) - timestamp) /
							std::max<uint64_t>(1, state.view_end_ns - state.view_begin_ns) *
							plot_width;
						if (distance <= 4) {
							state.tooltip +=
								" · " + std::string(operation_label(event.operation)) + " " +
								change(event.bytesBefore, event.bytesAfter, false, state.decimal) +
								" · tick " + std::to_string(event.appTick);
							break;
						}
					}
			}
		}
	}
	if (!controller.created) {
		const auto result = controller.viewports->create(
			controller.key,
			ViewPortCreateInfo{.colorFormat = controller.renderer->devReplayTargetFormat(),
							   .clearColor = {0, 0, 0, 1}});
		controller.created = result.has_value();
		if (controller.created)
			controller.viewports->getViewPort(controller.key)
				->setRenderCallback([this](const auto& context) noexcept {
					try {
						impl_->record(context);
					} catch (...) {
						impl_->replay_failed = true;
						impl_->recorded = false;
					}
				});
	}
	if (!controller.created) {
		state.viewport_error = "History viewport creation failed";
		return;
	}
	state.viewport_error = controller.replay_failed ? "History replay failed; retrying" : "";
	Clay_ElementDeclaration declaration{};
	declaration.layout.sizing = {.width = CLAY_SIZING_GROW(0), .height = CLAY_SIZING_GROW(0)};
	declaration.image.imageData =
		manager.imageData(controller.viewports->getTexture(controller.key));
	CLAY(image, declaration) {}
}
void DevMemoryHistoryController::prepare(
	const ::FlowUi::detail::manager_storage::FontFrameView& fonts, float font_scale,
	uint32_t texture_slot) {
	auto& controller = *impl_;
	controller.fonts = &fonts;
	controller.font_scale = font_scale;
	controller.texture_slot = texture_slot;
	if (!controller.seen || !controller.state)
		return;
	controller.bounds = Clay_GetElementData(controller.image).boundingBox;
	const auto bounds = controller.bounds;
	if (bounds.width <= gutter + 8 || bounds.height <= navigator_height)
		return;
	auto& state = *controller.state;
	controller.pending.clear();
	controller.labels.clear();
	controller.pending.reserve(state.lanes.size() * 4096 + 128);
	controller.labels.reserve(state.lanes.size() * 10 + 8);
	const auto rectangle = [&](float left, float top, float width, float height, Clay_Color color) {
		if (width > 0 && height > 0)
			controller.pending.emplace_back(Rectangle{{left, top, width, height}, color});
	};
	const auto label = [&](std::string value, float left, float top, float width, float height,
						   Clay_Color color, bool right = false) {
		controller.labels.emplace_back(
			Label{std::move(value), {left, top, width, height}, color, right});
	};
	rectangle(0, 0, bounds.width, bounds.height, interface_theme::kDepth0Keel);
	const float plot_width = bounds.width - gutter - 8;
	const float lane_height =
		(bounds.height - navigator_height) / std::max<size_t>(1, state.lanes.size());
	const uint64_t duration = std::max<uint64_t>(1, state.view_end_ns - state.view_begin_ns);
	const auto time_x = [&](uint64_t timestamp) {
		return gutter + float(std::clamp(
							(static_cast<long double>(timestamp) - state.view_begin_ns) / duration,
							0.0L, 1.0L)) *
							plot_width;
	};
	for (size_t lane_index = 0; lane_index < state.lanes.size(); ++lane_index) {
		const auto& lane = state.lanes[lane_index];
		const float top = lane_index * lane_height, graph_top = top + 25,
					bottom = top + lane_height - 26;
		const float height = std::max(1.0f, bottom - graph_top);
		const auto value_y = [&](uint64_t value) {
			return bottom -
				   float(std::clamp(double(value) / std::max<uint64_t>(1, lane.scale), 0.0, 1.0)) *
					   height;
		};
		const auto active_color = lane.metric ? Flow_Color("#33465F") : Flow_Color("#49B9FF");
		const auto backing_color = lane.metric ? Flow_Color("#10B981") : Flow_Color("#BA8CFF");
		label(lane.label, gutter + 2, top + 2, std::max(1.0f, plot_width - 175), 18,
			  interface_theme::kTextSecondary);
		rectangle(bounds.width - 172, top + 10, 8, 2, active_color);
		label(std::string(active_label(lane)), bounds.width - 161, top + 2, 75, 18, active_color);
		rectangle(bounds.width - 83, top + 10, 8, 2, backing_color);
		label(std::string(backing_label(lane)), bounds.width - 72, top + 2, 66, 18, backing_color);
		for (int tick = 0; tick < 5; ++tick) {
			const float vertical = graph_top + height * tick / 4;
			rectangle(gutter, vertical, plot_width, 1, interface_theme::kBorderPrimary);
			label(axis_reading(uint64_t(static_cast<long double>(lane.scale) * (4 - tick) / 4),
							   lane.metric, state.decimal),
				  0, vertical - 7, gutter - 6, 14, interface_theme::kTextMuted, true);
		}
		// Choose a 1/2/5 decade cadence in relative seconds; labels remain stable during panning.
		const double seconds = double(duration) / 1e9;
		const double rough = seconds / std::max(2.0f, plot_width / 110);
		const double decade = std::pow(10.0, std::floor(std::log10(std::max(1e-9, rough))));
		const double normalized = rough / decade;
		const double cadence = (normalized <= 1	  ? 1
								: normalized <= 2 ? 2
								: normalized <= 5 ? 5
												  : 10) *
							   decade;
		for (double offset = 0; offset <= seconds; offset += cadence) {
			const float horizontal = gutter + plot_width * float(1 - offset / seconds);
			rectangle(horizontal, graph_top, 1, height, interface_theme::kBorderPrimary);
			char time[48];
			std::snprintf(time, sizeof(time), "%.*fs", cadence < 1 ? (cadence < .01 ? 3 : 2) : 1,
						  offset == 0 ? 0 : -offset);
			label(time, std::clamp(horizontal - 25, gutter, bounds.width - 58), bottom + 9, 52, 16,
				  interface_theme::kTextMuted);
		}
		rectangle(gutter, graph_top, 1, height, interface_theme::kBorderVisible);
		if (state.range_end_ns > state.range_begin_ns &&
			state.range_end_ns >= state.view_begin_ns &&
			state.range_begin_ns <= state.view_end_ns) {
			const auto left = time_x(state.range_begin_ns), right = time_x(state.range_end_ns);
			auto fill = interface_theme::kAccentSeaGlass;
			fill.a = 32;
			rectangle(left, graph_top, right - left, height, fill);
			for (const auto timestamp : {state.range_begin_ns, state.range_end_ns})
				if (timestamp >= state.view_begin_ns && timestamp <= state.view_end_ns)
					rectangle(time_x(timestamp), graph_top, 1.5f, height,
							  interface_theme::kAccentSeaGlass);
		}
		const auto series = [&](bool backing, Clay_Color color) {
			const uint16_t field =
				uint16_t(backing ? MemoryValueField::Backing : MemoryValueField::LogicalLive);
			const MemoryHistoryColumn* previous = nullptr;
			for (size_t column_index = 0; column_index < lane.columns.size(); ++column_index) {
				const auto& value = lane.columns[column_index];
				if (!value.available || value.mixed_epochs || value.gap ||
					!(value.value_fields & field)) {
					previous = nullptr;
					continue;
				}
				const float left =
								std::max(time_x(value.first_ns),
										 gutter + plot_width * column_index / lane.columns.size()),
							right = std::min(time_x(value.last_ns),
											 gutter + plot_width * (column_index + 1) /
														  lane.columns.size());
				const auto first = backing ? value.backing_first : value.active_first,
						   last = backing ? value.backing_last : value.active_last;
				const float high = value_y(backing ? value.backing_max : value.active_max),
							low = value_y(backing ? value.backing_min : value.active_min);
				auto fill = color;
				fill.a = backing ? 18 : 28;
				rectangle(left, value_y(last), std::max(1.0f, right - left), bottom - value_y(last),
						  fill);
				rectangle(left, value_y(last), std::max(1.0f, right - left), 2, color);
				rectangle(left, high, 1, std::max(2.0f, low - high), color);
				if (previous && previous->epoch == value.epoch &&
					previous->last_ns >= value.first_ns) {
					const float previous_y = value_y(backing ? previous->backing_last
															 : previous->active_last),
								current_y = value_y(first);
					rectangle(left, std::min(previous_y, current_y), 2,
							  std::max(2.0f, std::abs(previous_y - current_y)), color);
				}
				previous = &value;
			}
		};
		const bool reported = std::ranges::any_of(
			lane.columns, [](const auto& value) { return value.available && value.value_fields; });
		if (!reported)
			label(lane.metric ? "Objects / capacity not reported by this source"
							  : "No retained byte observations",
				  gutter + 8, graph_top + height / 2 - 8, plot_width - 16, 18,
				  interface_theme::kTextSecondary);
		series(true, backing_color);
		series(false, active_color);
		for (const auto& event : lane.events)
			rectangle(time_x(event.timestampNs), bottom + 2, 2, 6,
					  operation_color(event.operation));
		if (controller.hover_ns >= state.view_begin_ns && controller.hover_ns < state.view_end_ns)
			rectangle(time_x(controller.hover_ns), graph_top, 1, height,
					  interface_theme::kBorderVisible);
		if (state.selected_ns >= state.view_begin_ns && state.selected_ns < state.view_end_ns &&
			state.selected_ns)
			rectangle(time_x(state.selected_ns), graph_top, 1, height,
					  interface_theme::kAccentSignalCoral);
	}
	if (state.lanes.empty())
		label(state.message, gutter, 40, plot_width, 22, interface_theme::kTextMuted);
	const float nav_top = bounds.height - navigator_height;
	rectangle(0, nav_top, bounds.width, 1, interface_theme::kBorderVisible);
	label("Retained", 2, nav_top, gutter - 4, navigator_height, interface_theme::kTextMuted);
	if (state.retained_end_ns > state.retained_begin_ns) {
		for (const auto& lane : state.lanes) {
			uint64_t maximum = 1;
			for (const auto& value : lane.overview)
				maximum = std::max({maximum, value.active_max, value.backing_max});
			const float column_width = plot_width / std::max<size_t>(1, lane.overview.size());
			const auto overview_series = [&](bool backing, Clay_Color color) {
				color.a = 180;
				const MemoryHistoryColumn* previous = nullptr;
				const auto value_y = [&](uint64_t value) {
					return nav_top + 15 - float(double(value) / maximum) * 11;
				};
				for (size_t column = 0; column < lane.overview.size(); ++column) {
					const auto& value = lane.overview[column];
					const auto field =
						backing ? MemoryValueField::Backing : MemoryValueField::LogicalLive;
					if (!value.available || value.mixed_epochs || value.gap ||
						!(value.value_fields & uint16_t(field))) {
						previous = nullptr;
						continue;
					}
					const float left = gutter + column * column_width;
					const float high = value_y(backing ? value.backing_max : value.active_max);
					const float low = value_y(backing ? value.backing_min : value.active_min);
					const float last = value_y(backing ? value.backing_last : value.active_last);
					rectangle(left, last, column_width, 1, color);
					rectangle(left, high, 1, std::max(1.0f, low - high), color);
					if (previous && previous->epoch == value.epoch &&
						previous->last_ns >= value.first_ns) {
						const float previous_y =
							value_y(backing ? previous->backing_last : previous->active_last);
						const float first_y =
							value_y(backing ? value.backing_first : value.active_first);
						rectangle(left, std::min(previous_y, first_y), 1,
								  std::max(1.0f, std::abs(previous_y - first_y)), color);
					}
					previous = &value;
				}
			};
			overview_series(true, lane.metric ? Flow_Color("#10B981") : Flow_Color("#BA8CFF"));
			overview_series(false, lane.metric ? Flow_Color("#33465F") : Flow_Color("#49B9FF"));
		}
		const auto nav_x = [&](uint64_t timestamp) {
			return gutter + float(double(timestamp - state.retained_begin_ns) /
								  (state.retained_end_ns - state.retained_begin_ns)) *
								plot_width;
		};
		const float left = nav_x(state.view_begin_ns), right = nav_x(state.view_end_ns);
		auto dim = interface_theme::kDepth0Keel;
		dim.a = 150;
		rectangle(gutter, nav_top + 1, left - gutter, 17, dim);
		rectangle(right, nav_top + 1, gutter + plot_width - right, 17, dim);
		auto curtain = interface_theme::kBorderVisible;
		curtain.a = 64;
		rectangle(left, nav_top + 1, right - left, 17, curtain);
		rectangle(left, nav_top + 1, 1, 17, interface_theme::kAccentSeaGlass);
		rectangle(right, nav_top + 1, 1, 17, interface_theme::kAccentSeaGlass);
	}
}
void DevMemoryHistoryController::submitted(uint64_t serial) {
	auto& controller = *impl_;
	if (!controller.recorded || !controller.state)
		return;
	controller.generation = std::max(controller.generation, serial);
	controller.presented_bounds = controller.bounds;
	controller.presented_lanes = controller.state->lanes;
	controller.presented_scope = controller.state->scope;
	controller.presented_window = controller.state->window;
	controller.presented_settings = controller.state->history_settings_key;
	controller.presented_begin = controller.state->view_begin_ns;
	controller.presented_end = controller.state->view_end_ns;
	controller.presented_retained_begin = controller.state->retained_begin_ns;
	controller.presented_retained_end = controller.state->retained_end_ns;
}
void DevMemoryHistoryController::destroy_drained() {
	if (impl_->created && impl_->viewports)
		impl_->viewports->getViewPort(impl_->key)->clearRenderCallback();
	impl_->upload = std::make_unique<SurfaceUpload>();
	impl_->state = nullptr;
	impl_->reporting = nullptr;
	impl_->created = false;
}
uint64_t DevMemoryHistoryController::capacity_bytes() const noexcept {
	uint64_t result =
		impl_->pending.capacity() * sizeof(Rectangle) + impl_->labels.capacity() * sizeof(Label) +
		impl_->instances.capacity() * sizeof(UiInstance) + impl_->runs.capacity() * sizeof(UiRun);
	const auto lanes_capacity = [](const auto& lanes) {
		uint64_t bytes = lanes.capacity() * sizeof(MemoryHistoryLane);
		for (const auto& lane : lanes)
			bytes +=
				(lane.columns.capacity() + lane.overview.capacity()) * sizeof(MemoryHistoryColumn) +
				lane.events.capacity() * sizeof(MemoryOperationRecord) + lane.label.capacity();
		return bytes;
	};
	result += lanes_capacity(impl_->presented_lanes);
	if (impl_->state) {
		result += lanes_capacity(impl_->state->lanes) + lanes_capacity(impl_->state->all_lanes) +
				  impl_->state->tracks.capacity() * sizeof(MemoryWorkbenchTrack);
		for (const auto& sources : impl_->state->lane_sources)
			result += lanes_capacity(sources);
		for (const auto& track : impl_->state->tracks)
			result += track.keys.capacity() * sizeof(MemorySourceKey) +
					  track.history_sources.capacity() * sizeof(MemorySourceId) +
					  track.title.capacity() + track.note.capacity();
	}
	return result;
}
bool DevMemoryHistoryController::owns_scroll(const FrameInput& input) const noexcept {
	return impl_->was_seen &&
		   (!impl_->state ||
			!impl_->state->history_settings_open && !impl_->state->history_window_open) &&
		   contains(impl_->presented_bounds, input.mouseX, input.mouseY);
}
} // namespace FlowUi::devSystems::interface_elements
#endif
