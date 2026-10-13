#include "devSystems/devInterface/Memory/Workbench/DevMemoryHistoryViewport.hpp"
#if FLOW_UI_DEV_MODE && FLOWUI_PUBLIC_VULKAN_INTEROP
#include "devSystems/devInterface/Permanents/Backend/DevTheme.hpp"
#include "devSystems/devMonitoringAndReporting/memory/DevMemorySources.hpp"
#include "managers/ViewPortManager.hpp"
#include <GLFW/glfw3.h>
#include <algorithm>
#include <cmath>
#include <cstdio>
namespace FlowUi::devSystems::interface_elements {
namespace {
[[nodiscard]] float linear_channel(float channel) noexcept {
	const float normalized = channel / 255;
	return normalized <= .04045f ? normalized / 12.92f
								 : std::pow((normalized + .055f) / 1.055f, 2.4f);
}
bool contains(Clay_BoundingBox bounds, float horizontal, float vertical) noexcept {
	return horizontal >= bounds.x && vertical >= bounds.y && horizontal < bounds.x + bounds.width &&
		   vertical < bounds.y + bounds.height;
}
struct Rectangle {
	Clay_BoundingBox bounds;
	Clay_Color color;
};
} // namespace
struct DevMemoryHistoryController::Impl {
	ViewPortManager* viewports = nullptr;
	DevMemoryWorkbenchState* state = nullptr;
	DevMemoryReporting* reporting = nullptr;
	std::vector<Rectangle> pending{};
	Clay_ElementId image{};
	Clay_BoundingBox bounds{}, presented_bounds{};
	uint64_t revision = 0, presented_revision = 0, presentation_bytes = 0;
	uint64_t presented_begin = 0, presented_end = 0, presented_scope = 0, presented_window = 0;
	std::vector<MemoryHistoryLane> presented_lanes{};
	std::string key = "flowui.memory.history";
	bool seen = false, was_seen = false, created = false, recorded = false, pressed = false,
		 dragged = false, keyboard_focus = false;
	float press_x = 0;
	uint64_t press_ns = 0;
	void record(const ViewPortRenderContext& context) noexcept {
		if (bounds.width <= 0 || bounds.height <= 0)
			return;
		recorded = true;
		const float scale_x = context.extent.width / bounds.width,
					scale_y = context.extent.height / bounds.height;
		for (const auto& rectangle : pending) {
			const int left = std::clamp(int(std::floor(rectangle.bounds.x * scale_x)), 0,
										int(context.extent.width));
			const int top = std::clamp(int(std::floor(rectangle.bounds.y * scale_y)), 0,
									   int(context.extent.height));
			const int right =
				std::clamp(int(std::ceil((rectangle.bounds.x + rectangle.bounds.width) * scale_x)),
						   left, int(context.extent.width));
			const int bottom =
				std::clamp(int(std::ceil((rectangle.bounds.y + rectangle.bounds.height) * scale_y)),
						   top, int(context.extent.height));
			if (right == left || bottom == top)
				continue;
			VkClearAttachment attachment{};
			attachment.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
			attachment.clearValue.color = {{linear_channel(rectangle.color.r),
											linear_channel(rectangle.color.g),
											linear_channel(rectangle.color.b), 1}};
			VkClearRect region{};
			region.rect = {{left, top}, {uint32_t(right - left), uint32_t(bottom - top)}};
			region.layerCount = 1;
			vkCmdClearAttachments(context.commandBuffer, 1, &attachment, 1, &region);
		}
	}
	void pause() {
		if (state && reporting) {
			hold_memory_history(*state, *reporting);
			state->following = false;
		}
	}
};
DevMemoryHistoryController::DevMemoryHistoryController() : impl_(std::make_unique<Impl>()) {}
DevMemoryHistoryController::~DevMemoryHistoryController() = default;
void DevMemoryHistoryController::begin_frame(ViewPortManager& viewports) {
	impl_->viewports = &viewports;
	if (!impl_->seen) {
		if (impl_->state) {
			impl_->state->following = true;
			impl_->state->selected_ns = 0;
			impl_->state->next_live_ns = 0;
		}
		// A hidden element's state is not borrowed across frame construction.
		impl_->state = nullptr;
		impl_->reporting = nullptr;
		impl_->presented_bounds = {};
	}
	impl_->was_seen = impl_->seen;
	impl_->seen = false;
	impl_->recorded = false;
}
void DevMemoryHistoryController::draw(UiManager& manager, Clay_ElementId image,
									  DevMemoryWorkbenchState& state,
									  DevMemoryReporting& reporting) {
	auto& controller = *impl_;
	controller.state = &state;
	controller.presentation_bytes = state.tracks.capacity() * sizeof(MemoryWorkbenchTrack) +
									state.lanes.capacity() * sizeof(MemoryHistoryLane);
	for (const auto& track : state.tracks)
		controller.presentation_bytes += track.keys.capacity() * sizeof(MemorySourceKey) +
										 track.history_sources.capacity() * sizeof(MemorySourceId) +
										 track.title.capacity() + track.note.capacity();
	for (const auto& lane : state.lanes)
		controller.presentation_bytes +=
			lane.columns.capacity() * sizeof(MemoryHistoryColumn) + lane.label.capacity();

	controller.reporting = &reporting;
	controller.seen = true;
	controller.image = image;
	state.externally_paused = reporting.status().freeze_owner_count > 0;
	const auto& input = manager.getCurrentFrameInput();
	const auto& previous = manager.getPreviousFrameInput();
	const bool valid_scene = controller.presented_scope == state.scope &&
							 controller.presented_window == state.window &&
							 controller.presented_revision == state.layout_revision;
	const bool chart_scroll =
		input.keyDown[GLFW_KEY_LEFT_CONTROL] || input.keyDown[GLFW_KEY_RIGHT_CONTROL];
	const bool hovered =
		valid_scene && contains(controller.presented_bounds, input.mouseX, input.mouseY);
	const auto time_at = [&](float horizontal) {
		const auto fraction = std::clamp(double(horizontal - controller.presented_bounds.x) /
											 std::max(1.0f, controller.presented_bounds.width),
										 0.0, 1.0);
		return controller.presented_begin +
			   uint64_t(fraction * (controller.presented_end - controller.presented_begin));
	};
	if (hovered && input.mouseDown[0] && !previous.mouseDown[0]) {
		controller.pause();
		controller.pressed = true;
		controller.dragged = false;
		controller.keyboard_focus = true;
		controller.press_x = input.mouseX;
		controller.press_ns = time_at(input.mouseX);
	}
	const auto ruler_bounds =
		Clay_BoundingBox{controller.presented_bounds.x - 84, controller.presented_bounds.y, 78,
						 controller.presented_bounds.height};
	if (valid_scene && chart_scroll && input.scrollY &&
		contains(ruler_bounds, input.mouseX, input.mouseY)) {
		state.manual_scale = std::clamp(
			double(state.history_scale_bytes) * std::pow(1.25, -input.scrollY), 1.0, 9e18);
		state.scale_mode = 1;
		state.next_history_ns = 0;
	}
	if (hovered && chart_scroll && input.scrollY) {
		controller.pause();
		zoom_memory_view(state, std::pow(1.25, -input.scrollY),
						 std::clamp(double(input.mouseX - controller.presented_bounds.x) /
										std::max(1.0f, controller.presented_bounds.width),
									0.0, 1.0));
	}
	if (controller.pressed && input.mouseDown[0]) {
		const auto delta = input.mouseX - controller.press_x;
		if (std::abs(delta) > 3) {
			controller.dragged = true;
			if (state.gesture_mode == 1) {
				const auto current = time_at(input.mouseX);
				state.range_begin_ns = std::min(controller.press_ns, current);
				state.range_end_ns = std::max(controller.press_ns, current);
			} else {
				const auto width = std::max<uint64_t>(1, state.view_end_ns - state.view_begin_ns);
				const auto shift = int64_t(
					double(delta) / std::max(1.0f, controller.presented_bounds.width) * width);
				const auto begin =
					shift > 0 ? state.view_begin_ns - std::min(state.view_begin_ns, uint64_t(shift))
							  : state.view_begin_ns + uint64_t(-shift);
				state.view_begin_ns =
					std::clamp(begin, state.retained_begin_ns,
							   std::max(state.retained_begin_ns, state.retained_end_ns > width
																	 ? state.retained_end_ns - width
																	 : state.retained_begin_ns));
				state.view_end_ns = state.view_begin_ns + width;
				controller.press_x = input.mouseX;
			}
			state.next_history_ns = 0;
			state.analysis.last_refresh_ns = 0;
		}
	}
	if (controller.pressed && !input.mouseDown[0]) {
		controller.pressed = false;
		if (hovered && !controller.dragged) {
			state.selected_ns = time_at(input.mouseX);
			state.next_live_ns = 0;
			state.next_history_ns = 0;
		}
	}
	if (controller.keyboard_focus && valid_scene && input.windowFocused &&
		!manager.inputFields().hasPrimaryFieldFocus()) {
		const auto pressed_key = [&](int key) {
			return input.keyDown[size_t(key)] && !previous.keyDown[size_t(key)];
		};
		if (pressed_key(GLFW_KEY_LEFT) || pressed_key(GLFW_KEY_RIGHT)) {
			controller.pause();
			pan_memory_view(state, pressed_key(GLFW_KEY_LEFT) ? -1 : 1);
		}
		if (pressed_key(GLFW_KEY_EQUAL) || pressed_key(GLFW_KEY_MINUS)) {
			controller.pause();
			zoom_memory_view(state, pressed_key(GLFW_KEY_EQUAL) ? .5 : 2);
		}
		if (pressed_key(GLFW_KEY_HOME)) {
			controller.pause();
			state.fit_pending = true;
			state.next_history_ns = 0;
		}
		if (pressed_key(GLFW_KEY_ESCAPE)) {
			state.selected_ns = state.range_begin_ns = state.range_end_ns = 0;
			state.next_live_ns = 0;
		}
	}
	state.tooltip.clear();
	if (hovered && !controller.presented_lanes.empty() && controller.presented_bounds.width > 0) {
		const auto lane_index =
			std::min(controller.presented_lanes.size() - 1,
					 size_t(std::max(0.0f, input.mouseY - controller.presented_bounds.y) /
							std::max(1.0f, controller.presented_bounds.height) *
							controller.presented_lanes.size()));
		const auto& lane = controller.presented_lanes[lane_index];
		const auto column =
			std::min(lane.columns.size() - 1,
					 size_t(std::max(0.0f, input.mouseX - controller.presented_bounds.x) /
							controller.presented_bounds.width * lane.columns.size()));
		const auto& value = lane.columns[column];
		char interval[96];
		const double length = double(controller.presented_end - controller.presented_begin) / 1e9;
		std::snprintf(interval, sizeof(interval), " · [%+.3f, %+.3f) s",
					  -length + length * column / lane.columns.size(),
					  -length + length * (column + 1) / lane.columns.size());
		state.tooltip = lane.label + interval;
		if (!value.available)
			state.tooltip += " · No retained observation";
		else {
			const auto active_label = state.history_metric == 2						   ? "Objects"
									  : state.history_metric == 1					   ? "Backing"
									  : lane.key.source == memory_sources::kProcess.id ? "Resident"
									  : lane.key.source == memory_sources::kVulkanHeaps.id
										  ? "Allocations"
										  : "Active";
			const auto backing_label = state.history_metric == 2 ? "Capacity count"
									   : lane.key.source == memory_sources::kProcess.id ? "Virtual"
									   : lane.key.source == memory_sources::kVulkanHeaps.id
										   ? "Blocks"
										   : "Backing";
			state.tooltip += " · " + std::string(active_label) +
							 (value.value_fields & uint16_t(MemoryValueField::LogicalLive)
								  ? " " +
										(state.history_metric == 2
											 ? std::to_string(value.active_min)
											 : memory_bytes(value.active_min, state.decimal)) +
										" – " +
										(state.history_metric == 2
											 ? std::to_string(value.active_max)
											 : memory_bytes(value.active_max, state.decimal))
								  : " unavailable");
			state.tooltip += " · " + std::string(backing_label) +
							 (value.value_fields & uint16_t(MemoryValueField::Backing)
								  ? " " + (state.history_metric == 2
											   ? std::to_string(value.backing_max)
											   : memory_bytes(value.backing_max, state.decimal))
								  : " unavailable");
			state.tooltip += " · sampled/held bucket envelope";
			if (uint16_t(value.flags) & uint16_t(MemorySampleFlag::Incomplete))
				state.tooltip += " · incomplete observation";
		}
	}
	if (!controller.created) {
		const auto result = controller.viewports->create(
			controller.key,
			ViewPortCreateInfo{.clearColor = {linear_channel(interface_theme::kDepth0Keel.r),
											  linear_channel(interface_theme::kDepth0Keel.g),
											  linear_channel(interface_theme::kDepth0Keel.b), 1}});
		controller.created = result.has_value();
		if (controller.created)
			controller.viewports->getViewPort(controller.key)
				->setRenderCallback([this](const auto& context) { impl_->record(context); });
	}
	if (!controller.created) {
		state.viewport_error =
			"History viewport creation failed; live distribution remains available";
		return;
	}
	state.viewport_error.clear();
	Clay_ElementDeclaration declaration{};
	declaration.layout.sizing = {.width = CLAY_SIZING_GROW(0), .height = CLAY_SIZING_GROW(0)};
	declaration.image.imageData =
		manager.imageData(controller.viewports->getTexture(controller.key));
	CLAY(image, declaration) {}
}
void DevMemoryHistoryController::prepare() {
	auto& controller = *impl_;
	if (!controller.seen || !controller.state)
		return;
	const auto next = Clay_GetElementData(controller.image).boundingBox;
	if (next.width <= 0 || next.height <= 0)
		return;
	auto& state = *controller.state;
	if (controller.revision == state.layout_revision && next.width == controller.bounds.width &&
		next.height == controller.bounds.height) {
		controller.bounds = next;
		return;
	}
	controller.bounds = next;
	controller.pending.clear();
	controller.pending.reserve(
		state.lanes.size() *
		((state.lanes.empty() ? 0 : state.lanes.front().columns.size()) * 4 + 1));
	const float lane_height = next.height / std::max<size_t>(1, state.lanes.size());
	for (size_t lane_index = 0; lane_index < state.lanes.size(); ++lane_index) {
		const auto& lane = state.lanes[lane_index];
		const auto scale = std::max<uint64_t>(1, lane.scale);
		const float top = lane_index * lane_height;
		const float bottom = top + lane_height - 8;
		const float plot_height = lane_height - 16;
		for (int tick = 0; tick < 5; ++tick) {
			controller.pending.emplace_back(Rectangle{{next.width * tick / 4, top, 1, lane_height},
													  interface_theme::kBorderPrimary});
			controller.pending.emplace_back(
				Rectangle{{0, top + 8 + plot_height * tick / 4, next.width, 1},
						  interface_theme::kBorderPrimary});
		}
		const float column_width = next.width / std::max<size_t>(1, lane.columns.size());
		for (size_t column = 0; column < lane.columns.size(); ++column) {
			const auto& value = lane.columns[column];
			if (!value.available || value.mixed_epochs)
				continue;
			const float left = column * column_width;
			const auto position = [&](uint64_t bytes) {
				return bottom - float(std::clamp(double(bytes) / scale, 0.0, 1.0)) * plot_height;
			};
			if (state.show_backing && (value.value_fields & uint16_t(MemoryValueField::Backing)))
				controller.pending.emplace_back(
					Rectangle{{left, position(value.backing_max), std::max(1.0f, column_width), 2},
							  Flow_Color("#BA8CFF")});
			if (value.value_fields & uint16_t(MemoryValueField::LogicalLive)) {
				const auto high = position(value.active_max), low = position(value.active_min);
				controller.pending.emplace_back(Rectangle{
					{left, high, std::max(1.0f, column_width), std::max(2.0f, low - high)},
					Flow_Color("#49B9FF")});
			}
		}
		controller.pending.emplace_back(
			Rectangle{{0, bottom, next.width, 1}, interface_theme::kBorderVisible});
	}
	if (state.selected_ns && state.view_end_ns > state.view_begin_ns &&
		state.selected_ns >= state.view_begin_ns && state.selected_ns <= state.view_end_ns) {
		const auto position = float(double(state.selected_ns - state.view_begin_ns) /
									(state.view_end_ns - state.view_begin_ns)) *
							  next.width;
		controller.pending.emplace_back(
			Rectangle{{position, 0, 2, next.height}, interface_theme::kAccentSignalCoral});
	}
	if (state.range_end_ns > state.range_begin_ns && state.view_end_ns > state.view_begin_ns) {
		for (const auto timestamp : {state.range_begin_ns, state.range_end_ns})
			if (timestamp >= state.view_begin_ns && timestamp <= state.view_end_ns) {
				const auto position = float(double(timestamp - state.view_begin_ns) /
											(state.view_end_ns - state.view_begin_ns)) *
									  next.width;
				controller.pending.emplace_back(
					Rectangle{{position, 0, 2, next.height}, interface_theme::kStatusAmber});
			}
	}
	controller.revision = state.layout_revision;
}
void DevMemoryHistoryController::submitted() {
	auto& controller = *impl_;
	if (!controller.recorded || !controller.state)
		return;
	controller.presented_bounds = controller.bounds;
	if (controller.presented_revision != controller.revision)
		controller.presented_lanes = controller.state->lanes;
	controller.presented_scope = controller.state->scope;
	controller.presented_window = controller.state->window;
	controller.presented_revision = controller.revision;
	controller.presented_begin = controller.state->view_begin_ns;
	controller.presented_end = controller.state->view_end_ns;
}
void DevMemoryHistoryController::destroy_drained() {
	if (impl_->created && impl_->viewports)
		impl_->viewports->getViewPort(impl_->key)->clearRenderCallback();
	impl_->state = nullptr;
	impl_->reporting = nullptr;
	impl_->created = false;
}
uint64_t DevMemoryHistoryController::capacity_bytes() const noexcept {
	uint64_t result = impl_->presentation_bytes + impl_->pending.capacity() * sizeof(Rectangle) +
					  impl_->presented_lanes.capacity() * sizeof(MemoryHistoryLane);
	for (const auto& lane : impl_->presented_lanes)
		result += lane.columns.capacity() * sizeof(MemoryHistoryColumn) + lane.label.capacity();
	return result;
}
bool DevMemoryHistoryController::owns_scroll(const FrameInput& input) const noexcept {
	const auto bounds =
		Clay_BoundingBox{impl_->presented_bounds.x - 84, impl_->presented_bounds.y,
						 impl_->presented_bounds.width + 84, impl_->presented_bounds.height};
	return (input.keyDown[GLFW_KEY_LEFT_CONTROL] || input.keyDown[GLFW_KEY_RIGHT_CONTROL]) &&
		   impl_->was_seen && contains(bounds, input.mouseX, input.mouseY);
}
} // namespace FlowUi::devSystems::interface_elements
#endif
