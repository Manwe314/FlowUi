#include "devSystems/devInterface/Inspect/Workbench/DevPreviewViewPortRendering.hpp"

#if FLOWUI_PUBLIC_VULKAN_INTEROP

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstring>
#include <memory>
#include <vector>

#include "devSystems/devInterface/Inspect/Workbench/DevInspectWorkbench.hpp"
#include "Ui/Vk_UiRenderer.hpp"
#include "internal/Vma.hpp"

namespace FlowUi::devSystems {
namespace {

constexpr float kUnconstrainedExtentDimension = 65535.0f;

[[nodiscard]] RectF intersect_rects(const RectF& rect_a, const RectF& rect_b) noexcept {
	const float min_x = std::max(rect_a.x, rect_b.x);
	const float min_y = std::max(rect_a.y, rect_b.y);
	const float max_x = std::min(rect_a.x + rect_a.w, rect_b.x + rect_b.w);
	const float max_y = std::min(rect_a.y + rect_a.h, rect_b.y + rect_b.h);
	return RectF{
		min_x,
		min_y,
		std::max(0.0f, max_x - min_x),
		std::max(0.0f, max_y - min_y),
	};
}

void transformReplayGeometry(
	const interface_elements::DevPreviewState& state,
	const DevUiReplaySource& replay,
	float canvas_width, float canvas_height,
	std::span<UiInstance> instances,
	std::span<UiRun> runs) noexcept {
	const float zoom_scale = std::max(state.camera.zoomScale, 1.0e-6f);
	const float origin_x = replay.sourceRootBounds.x;
	const float origin_y = replay.sourceRootBounds.y;
	const auto transform_x = [&](float source_x) noexcept {
		return (source_x - origin_x - state.camera.panX) * zoom_scale + canvas_width * 0.5f;
	};
	const auto transform_y = [&](float source_y) noexcept {
		return (source_y - origin_y - state.camera.panY) * zoom_scale + canvas_height * 0.5f;
	};
	for (UiInstance& instance : instances) {
		instance.x = transform_x(instance.x);
		instance.y = transform_y(instance.y);
		instance.w *= zoom_scale;
		instance.h *= zoom_scale;
		instance.r0 *= zoom_scale;
		instance.r1 *= zoom_scale;
		instance.r2 *= zoom_scale;
		instance.r3 *= zoom_scale;
		instance.borderL *= zoom_scale;
		instance.borderT *= zoom_scale;
		instance.borderR *= zoom_scale;
		instance.borderB *= zoom_scale;
	}
	const RectF viewport_bounds{0.0f, 0.0f, canvas_width, canvas_height};
	for (UiRun& run : runs) {
		if (run.scissor.x == 0.0f && run.scissor.y == 0.0f &&
			run.scissor.w >= kUnconstrainedExtentDimension &&
			run.scissor.h >= kUnconstrainedExtentDimension) {
			run.scissor = viewport_bounds;
			continue;
		}
		const RectF transformed_scissor{
			transform_x(run.scissor.x),
			transform_y(run.scissor.y),
			run.scissor.w * zoom_scale,
			run.scissor.h * zoom_scale,
		};
		run.scissor = intersect_rects(transformed_scissor, viewport_bounds);
	}
}

} // namespace

struct DevPreviewViewPortRenderer::Impl {
	struct FrameSlot {
		VkBuffer instanceBuffer = VK_NULL_HANDLE;
		VmaAllocation instanceAllocation = nullptr;
		VkDeviceSize capacityBytes = 0u;
		void* mappedData = nullptr;
		VkDescriptorSet globalsSet = VK_NULL_HANDLE;
		uint64_t recorded_request_key = 0u;
		bool is_dirty = true;
	};

	VkDevice device = VK_NULL_HANDLE;
	VmaAllocator allocator = nullptr;
	VkFormat colorFormat = VK_FORMAT_UNDEFINED;
	VkDescriptorSetLayout globalsLayout = VK_NULL_HANDLE;
	VkDescriptorPool descriptorPool = VK_NULL_HANDLE;
	uint64_t active_selected_node_key = 0u;
	std::vector<FrameSlot> frameSlots{};
	std::vector<UiInstance> cpuInstances{};
	std::vector<UiRun> runs{};
	std::vector<RectF> scissorStack{};

	void init(
		const FlowUi::ViewPortVulkanInterop& interop,
		VkFormat format,
		VkDescriptorSetLayout sourceGlobalsLayout) {
		if (interop.device == VK_NULL_HANDLE || interop.allocator == nullptr ||
			sourceGlobalsLayout == VK_NULL_HANDLE) return;
		if (device == interop.device && colorFormat == format &&
			globalsLayout == sourceGlobalsLayout && !frameSlots.empty()) {
			return;
		}
		destroy();
		device = interop.device;
		allocator = interop.allocator;
		colorFormat = format;
		globalsLayout = sourceGlobalsLayout;
		frameSlots.resize(std::max(1u, interop.framesInFlight));
		VkDescriptorPoolSize poolSize{};
		poolSize.type = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
		poolSize.descriptorCount = static_cast<uint32_t>(frameSlots.size());
		VkDescriptorPoolCreateInfo poolInfo{VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO};
		poolInfo.maxSets = static_cast<uint32_t>(frameSlots.size());
		poolInfo.poolSizeCount = 1u;
		poolInfo.pPoolSizes = &poolSize;
		if (vkCreateDescriptorPool(device, &poolInfo, nullptr, &descriptorPool) != VK_SUCCESS) {
			destroy();
			return;
		}
		std::vector<VkDescriptorSetLayout> layouts(frameSlots.size(), globalsLayout);
		std::vector<VkDescriptorSet> sets(frameSlots.size(), VK_NULL_HANDLE);
		VkDescriptorSetAllocateInfo allocateInfo{VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO};
		allocateInfo.descriptorPool = descriptorPool;
		allocateInfo.descriptorSetCount = static_cast<uint32_t>(layouts.size());
		allocateInfo.pSetLayouts = layouts.data();
		if (vkAllocateDescriptorSets(device, &allocateInfo, sets.data()) != VK_SUCCESS) {
			destroy();
			return;
		}
		for (size_t frameSlot = 0u; frameSlot < frameSlots.size(); ++frameSlot) {
			frameSlots[frameSlot].globalsSet = sets[frameSlot];
		}
	}

	[[nodiscard]] bool ensureCapacity(uint32_t frameSlot, VkDeviceSize requiredBytes) {
		if (frameSlot >= frameSlots.size() || requiredBytes == 0u) return false;
		FrameSlot& slot = frameSlots[frameSlot];
		if (slot.instanceBuffer != VK_NULL_HANDLE && slot.capacityBytes >= requiredBytes) return true;
		VkDeviceSize capacity = slot.capacityBytes > 0u
			? slot.capacityBytes : 256u * sizeof(UiInstance);
		while (capacity < requiredBytes) capacity += std::max<VkDeviceSize>(1u, capacity / 2u);
		VkBufferCreateInfo bufferInfo{VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO};
		bufferInfo.size = capacity;
		bufferInfo.usage = VK_BUFFER_USAGE_STORAGE_BUFFER_BIT;
		bufferInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
		VmaAllocationCreateInfo allocationInfo{};
		allocationInfo.usage = VMA_MEMORY_USAGE_AUTO_PREFER_HOST;
		allocationInfo.flags = VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT |
			VMA_ALLOCATION_CREATE_MAPPED_BIT;
		VkBuffer replacementBuffer = VK_NULL_HANDLE;
		VmaAllocation replacementAllocation = nullptr;
		VmaAllocationInfo replacementInfo{};
		if (vmaCreateBuffer(
			allocator, &bufferInfo, &allocationInfo, &replacementBuffer,
			&replacementAllocation, &replacementInfo) != VK_SUCCESS) return false;
		if (slot.instanceBuffer != VK_NULL_HANDLE) {
			vmaDestroyBuffer(allocator, slot.instanceBuffer, slot.instanceAllocation);
		}
		slot.instanceBuffer = replacementBuffer;
		slot.instanceAllocation = replacementAllocation;
		slot.capacityBytes = capacity;
		slot.mappedData = replacementInfo.pMappedData;
		VkDescriptorBufferInfo descriptorBuffer{};
		descriptorBuffer.buffer = slot.instanceBuffer;
		descriptorBuffer.range = slot.capacityBytes;
		VkWriteDescriptorSet write{VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET};
		write.dstSet = slot.globalsSet;
		write.dstBinding = 0u;
		write.descriptorCount = 1u;
		write.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
		write.pBufferInfo = &descriptorBuffer;
		vkUpdateDescriptorSets(device, 1u, &write, 0u, nullptr);
		return slot.mappedData != nullptr;
	}

	void destroy() {
		if (allocator) {
			for (FrameSlot& slot : frameSlots) {
				if (slot.instanceBuffer != VK_NULL_HANDLE) {
					vmaDestroyBuffer(allocator, slot.instanceBuffer, slot.instanceAllocation);
				}
			}
		}
		if (device != VK_NULL_HANDLE && descriptorPool != VK_NULL_HANDLE) {
			vkDestroyDescriptorPool(device, descriptorPool, nullptr);
		}
		frameSlots.clear();
		descriptorPool = VK_NULL_HANDLE;
		globalsLayout = VK_NULL_HANDLE;
		device = VK_NULL_HANDLE;
		allocator = nullptr;
		colorFormat = VK_FORMAT_UNDEFINED;
	}
};

DevPreviewViewPortRenderer::DevPreviewViewPortRenderer()
	: impl_(std::make_unique<Impl>()) {}

DevPreviewViewPortRenderer::~DevPreviewViewPortRenderer() {
	if (impl_) {
		impl_->destroy();
	}
}

void DevPreviewViewPortRenderer::record(
	const FlowUi::ViewPortRenderContext& ctx,
	interface_elements::DevPreviewState& state,
	const DevUiReplaySource& replay) {
	if (!impl_ || !ctx.vulkan || ctx.commandBuffer == VK_NULL_HANDLE ||
		ctx.extent.width == 0u || ctx.extent.height == 0u) {
		return;
	}

	state.canvasWidth = static_cast<float>(ctx.extent.width);
	state.canvasHeight = static_cast<float>(ctx.extent.height);

	// Flush all frames in flight when selected node key changes
	if (state.lastSelectedNodeKey != impl_->active_selected_node_key) {
		impl_->active_selected_node_key = state.lastSelectedNodeKey;
		for (Impl::FrameSlot& slot : impl_->frameSlots) {
			slot.is_dirty = true;
			slot.recorded_request_key = 0u;
		}
	}

	if (replay && replay.renderer) {
		impl_->init(
			*ctx.vulkan,
			ctx.colorFormat,
			replay.renderer->devReplayGlobalsLayout());
	}
	if (ctx.frameIndex >= impl_->frameSlots.size()) return;
	Impl::FrameSlot& frameSlot = impl_->frameSlots[ctx.frameIndex];

	// Clear frame slot and drop draw calls if replay is mismatched, missing, or mid-transition
	if (!replay || replay.requestKey != state.lastSelectedNodeKey ||
		!replay.renderer || ctx.colorFormat != replay.renderer->devReplayTargetFormat()) {
		if (frameSlot.mappedData && frameSlot.capacityBytes > 0u) {
			std::memset(frameSlot.mappedData, 0, static_cast<size_t>(frameSlot.capacityBytes));
			if (impl_->allocator && frameSlot.instanceAllocation) {
				(void)vmaFlushAllocation(
					impl_->allocator, frameSlot.instanceAllocation, 0u, frameSlot.capacityBytes);
			}
		}
		frameSlot.is_dirty = true;
		frameSlot.recorded_request_key = 0u;
		return;
	}

	const float canvas_width = static_cast<float>(ctx.extent.width);
	const float canvas_height = static_cast<float>(ctx.extent.height);
	const FlowUi::detail::InputFieldFrameOverrides no_overrides{};
	const FlowUi::detail::UiConversionCapacity capacity =
		FlowUi::detail::measureUiConversionCapacity(*replay.commands, no_overrides);
	if (capacity.instances == 0u || capacity.runs == 0u) {
		if (frameSlot.mappedData && frameSlot.capacityBytes > 0u) {
			std::memset(frameSlot.mappedData, 0, static_cast<size_t>(frameSlot.capacityBytes));
			if (impl_->allocator && frameSlot.instanceAllocation) {
				(void)vmaFlushAllocation(
					impl_->allocator, frameSlot.instanceAllocation, 0u, frameSlot.capacityBytes);
			}
		}
		frameSlot.is_dirty = false;
		frameSlot.recorded_request_key = replay.requestKey;
		return;
	}

	impl_->cpuInstances.resize(capacity.instances);
	impl_->runs.resize(capacity.runs);
	impl_->scissorStack.resize(capacity.scissorDepth);
	constexpr VkExtent2D unconstrained_extent{65535u, 65535u};
	const FlowUi::detail::UiConversionResult converted =
		FlowUi::detail::buildUiInstancesDirect(
			*replay.commands,
			no_overrides,
			unconstrained_extent,
			replay.fontFrameView,
			replay.pointsToPixelsScale,
			1.0f,
			1.0f,
			impl_->cpuInstances,
			impl_->runs,
			impl_->scissorStack,
			replay.textureBindings);
	impl_->cpuInstances.resize(converted.instanceCount);
	impl_->runs.resize(converted.runCount);
	if (impl_->cpuInstances.empty() || impl_->runs.empty()) {
		if (frameSlot.mappedData && frameSlot.capacityBytes > 0u) {
			std::memset(frameSlot.mappedData, 0, static_cast<size_t>(frameSlot.capacityBytes));
			if (impl_->allocator && frameSlot.instanceAllocation) {
				(void)vmaFlushAllocation(
					impl_->allocator, frameSlot.instanceAllocation, 0u, frameSlot.capacityBytes);
			}
		}
		frameSlot.is_dirty = false;
		frameSlot.recorded_request_key = replay.requestKey;
		return;
	}

	transformReplayGeometry(
		state, replay, canvas_width, canvas_height, impl_->cpuInstances, impl_->runs);
	const VkDeviceSize copy_size = static_cast<VkDeviceSize>(
		impl_->cpuInstances.size() * sizeof(UiInstance));
	if (!impl_->ensureCapacity(ctx.frameIndex, copy_size)) return;

	std::memcpy(frameSlot.mappedData, impl_->cpuInstances.data(), static_cast<size_t>(copy_size));
	if (copy_size < frameSlot.capacityBytes && frameSlot.mappedData) {
		std::memset(
			static_cast<char*>(frameSlot.mappedData) + copy_size,
			0,
			static_cast<size_t>(frameSlot.capacityBytes - copy_size));
	}
	if (vmaFlushAllocation(
		impl_->allocator, frameSlot.instanceAllocation, 0u, frameSlot.capacityBytes) != VK_SUCCESS) return;

	replay.renderer->recordExternalReplay(
		ctx.commandBuffer,
		ctx.extent,
		frameSlot.globalsSet,
		replay.textureFrameSlot,
		impl_->runs);

	frameSlot.is_dirty = false;
	frameSlot.recorded_request_key = replay.requestKey;
}

} // namespace FlowUi::devSystems

#endif
