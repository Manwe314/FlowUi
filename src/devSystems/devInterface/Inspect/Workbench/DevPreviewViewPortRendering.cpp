#include "devSystems/devInterface/Inspect/Workbench/DevPreviewViewPortRendering.hpp"

#if FLOWUI_PUBLIC_VULKAN_INTEROP

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <cstring>
#include <memory>
#include <unordered_set>
#include <vector>

#include "devSystems/devInterface/Inspect/Workbench/DevInspectWorkbench.hpp"
#include "devSystems/devTooling/overlay/DevOverlayService.hpp"
#include "devSystems/devTooling/tree/DevTreeTypes.hpp"
#include "Ui/Vk_UiRenderer.hpp"
#include "internal/Vma.hpp"

namespace FlowUi::devSystems {

namespace {

uint32_t packColorRGBA(uint8_t r, uint8_t g, uint8_t b, uint8_t a) {
	return (static_cast<uint32_t>(r)) |
	       (static_cast<uint32_t>(g) << 8) |
	       (static_cast<uint32_t>(b) << 16) |
	       (static_cast<uint32_t>(a) << 24);
}

uint32_t packClayColor(Clay_Color c) {
	return packColorRGBA(
		static_cast<uint8_t>(std::clamp(c.r, 0.0f, 255.0f)),
		static_cast<uint8_t>(std::clamp(c.g, 0.0f, 255.0f)),
		static_cast<uint8_t>(std::clamp(c.b, 0.0f, 255.0f)),
		static_cast<uint8_t>(std::clamp(c.a, 0.0f, 255.0f)));
}

UiInstance makeSolidRect(
	float x, float y, float w, float h,
	uint32_t colorRGBA,
	float radius = 0.0f,
	float borderL = 0.0f, float borderT = 0.0f,
	float borderR = 0.0f, float borderB = 0.0f,
	uint32_t solidMode = 0) {
	UiInstance inst{};
	inst.type = 0; // Solid
	inst.x = x;
	inst.y = y;
	inst.w = w;
	inst.h = h;
	inst.colorRGBA = colorRGBA;
	inst.r0 = radius;
	inst.r1 = radius;
	inst.r2 = radius;
	inst.r3 = radius;
	inst.borderL = borderL;
	inst.borderT = borderT;
	inst.borderR = borderR;
	inst.borderB = borderB;
	inst.solidMode = solidMode;
	return inst;
}

Clay_Vector2 worldToCanvas(
	const interface_elements::DevPreviewState& state,
	float worldX, float worldY, float canvasW, float canvasH) {
	const float zoom = std::max(state.camera.zoomScale, 1.0e-6f);
	return Clay_Vector2{
		.x = (worldX - state.camera.panX) * zoom + canvasW * 0.5f,
		.y = (worldY - state.camera.panY) * zoom + canvasH * 0.5f,
	};
}

void buildPreviewGridInstances(
	const interface_elements::DevPreviewState& state,
	float canvasW, float canvasH,
	std::vector<UiInstance>& outInstances) {
	if (canvasW <= 0.0f || canvasH <= 0.0f) return;
	float minorStep = 10.0f;
	float majorStep = 50.0f;
	if (minorStep * state.camera.zoomScale < 6.0f) minorStep = 50.0f;
	if (minorStep * state.camera.zoomScale < 6.0f) minorStep = 200.0f;
	if (majorStep < minorStep) majorStep = minorStep * 4.0f;

	const float zoom = std::max(state.camera.zoomScale, 1.0e-6f);
	const float worldMinX = state.camera.panX - (canvasW * 0.5f) / zoom;
	const float worldMaxX = state.camera.panX + (canvasW * 0.5f) / zoom;
	const float worldMinY = state.camera.panY - (canvasH * 0.5f) / zoom;
	const float worldMaxY = state.camera.panY + (canvasH * 0.5f) / zoom;

	const int firstX = static_cast<int>(std::floor(worldMinX / minorStep));
	const int lastX = static_cast<int>(std::ceil(worldMaxX / minorStep));
	const int firstY = static_cast<int>(std::floor(worldMinY / minorStep));
	const int lastY = static_cast<int>(std::ceil(worldMaxY / minorStep));

	const int maxLines = 160;
	const uint32_t axisColor = packColorRGBA(86, 204, 242, 255);
	const uint32_t majorColor = packColorRGBA(63, 77, 85, 160);
	const uint32_t minorColor = packColorRGBA(38, 52, 59, 112);

	for (int grid = firstX, count = 0; grid <= lastX && count < maxLines; ++grid, ++count) {
		const float world = static_cast<float>(grid) * minorStep;
		const float x = worldToCanvas(state, world, 0.0f, canvasW, canvasH).x;
		if (x < 0.0f || x > canvasW) continue;
		const bool axis = std::abs(world) < 0.01f;
		const bool major = std::fmod(std::abs(world), majorStep) < 0.01f;
		const uint32_t color = axis ? axisColor : major ? majorColor : minorColor;
		outInstances.push_back(makeSolidRect(x, 0.0f, axis ? 1.5f : 1.0f, canvasH, color));
	}

	for (int grid = firstY, count = 0; grid <= lastY && count < maxLines; ++grid, ++count) {
		const float world = static_cast<float>(grid) * minorStep;
		const float y = worldToCanvas(state, 0.0f, world, canvasW, canvasH).y;
		if (y < 0.0f || y > canvasH) continue;
		const bool axis = std::abs(world) < 0.01f;
		const bool major = std::fmod(std::abs(world), majorStep) < 0.01f;
		const uint32_t color = axis ? axisColor : major ? majorColor : minorColor;
		outInstances.push_back(makeSolidRect(0.0f, y, canvasW, axis ? 1.5f : 1.0f, color));
	}
}

#if FLOW_UI_DEV_CAPTURE_CLAY
void buildCapturedSubtreeInstances(
	const interface_elements::DevPreviewState& state,
	const interface_elements::PreviewSelection& selection,
	const DevUiReplaySource& replay,
	float canvasW, float canvasH,
	std::vector<UiInstance>& outInstances) {
	if (!selection.clay || !selection.snapshot) return;
	const Clay_BoundingBox origin = selection.clay->bounds;
	const std::span<const tooling::DevClayNode> nodes =
		tooling::fullClaySubtree(*selection.snapshot, selection.flowIndex);
	const float zoom = state.camera.zoomScale;

	std::unordered_set<uint32_t> capturedNodeIds{};
	capturedNodeIds.reserve(nodes.size());
	for (const tooling::DevClayNode& node : nodes) {
		capturedNodeIds.insert(node.clayId);
	}

#if FLOW_UI_DEV_MODE
	if (replay && replay.prepared && !replay.prepared->instances.empty() &&
		replay.prepared->instanceClayIds.size() == replay.prepared->instances.size()) {
		for (size_t i = 0; i < replay.prepared->instances.size(); ++i) {
			const uint32_t ownerId = replay.prepared->instanceClayIds[i];
			if (ownerId == 0 || !capturedNodeIds.contains(ownerId)) continue;

			UiInstance inst = replay.prepared->instances[i];
			const float worldX = inst.x - origin.x;
			const float worldY = inst.y - origin.y;
			const Clay_Vector2 p = worldToCanvas(state, worldX, worldY, canvasW, canvasH);
			inst.x = p.x;
			inst.y = p.y;
			inst.w *= zoom;
			inst.h *= zoom;
			inst.r0 *= zoom;
			inst.r1 *= zoom;
			inst.r2 *= zoom;
			inst.r3 *= zoom;
			inst.borderL *= zoom;
			inst.borderT *= zoom;
			inst.borderR *= zoom;
			inst.borderB *= zoom;
			outInstances.push_back(inst);
		}
		return;
	}
#endif

	for (const tooling::DevClayNode& node : nodes) {
		const float worldX = node.bounds.x - origin.x;
		const float worldY = node.bounds.y - origin.y;
		const Clay_Vector2 p = worldToCanvas(state, worldX, worldY, canvasW, canvasH);
		const float w = std::max(0.0f, node.bounds.width * zoom);
		const float h = std::max(0.0f, node.bounds.height * zoom);
		if (p.x + w < 0.0f || p.x > canvasW || p.y + h < 0.0f || p.y > canvasH) continue;

		uint32_t fill = packClayColor(node.declaration.backgroundColor);
		if (node.pointerPresence.imageData && fill == 0) {
			fill = packColorRGBA(38, 61, 74, 255);
		}
		if (fill != 0) {
			outInstances.push_back(makeSolidRect(p.x, p.y, w, h, fill));
		}
		const uint32_t border = packClayColor(node.declaration.border.color);
		if (border != 0) {
			outInstances.push_back(makeSolidRect(p.x, p.y, w, h, border, 0.0f,
				node.declaration.border.width.left * zoom,
				node.declaration.border.width.top * zoom,
				node.declaration.border.width.right * zoom,
				node.declaration.border.width.bottom * zoom, 1));
		}
	}
}

void buildSidecarInstances(
	const interface_elements::DevPreviewState& state,
	const interface_elements::PreviewSelection& selection,
	float canvasW, float canvasH,
	std::vector<UiInstance>& outInstances) {
	if (!selection.clay || !selection.snapshot) return;
	const Clay_BoundingBox origin = selection.clay->bounds;
	const std::span<const tooling::DevClayNode> nodes =
		tooling::fullClaySubtree(*selection.snapshot, selection.flowIndex);
	const float zoom = state.camera.zoomScale;

	if (tooling::hasFlag(state.sidecarFlags, tooling::DevOverlayModeFlags::TreeHierarchy)) {
		const uint32_t cyan = packColorRGBA(47, 128, 237, 255);
		for (const tooling::DevClayNode& node : nodes) {
			const Clay_Vector2 p = worldToCanvas(state, node.bounds.x - origin.x, node.bounds.y - origin.y, canvasW, canvasH);
			outInstances.push_back(makeSolidRect(p.x, p.y, node.bounds.width * zoom, node.bounds.height * zoom, cyan, 0.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1));
		}
	}

	if (tooling::hasFlag(state.sidecarFlags, tooling::DevOverlayModeFlags::BoxModel)) {
		const Clay_Vector2 p = worldToCanvas(state, 0.0f, 0.0f, canvasW, canvasH);
		const uint32_t amberFill = packColorRGBA(39, 174, 96, 32);
		const uint32_t amberBorder = packColorRGBA(242, 153, 74, 255);
		outInstances.push_back(makeSolidRect(p.x, p.y, selection.clay->bounds.width * zoom, selection.clay->bounds.height * zoom, amberFill));
		outInstances.push_back(makeSolidRect(p.x, p.y, selection.clay->bounds.width * zoom, selection.clay->bounds.height * zoom, amberBorder, 0.0f, 2.0f, 2.0f, 2.0f, 2.0f, 1));
	}

	if (tooling::hasFlag(state.sidecarFlags, tooling::DevOverlayModeFlags::RulersAndDistance)) {
		const Clay_Vector2 p = worldToCanvas(state, 0.0f, 0.0f, canvasW, canvasH);
		const float right = p.x + selection.clay->bounds.width * zoom;
		const float bottom = p.y + selection.clay->bounds.height * zoom;
		const uint32_t lineCol = packColorRGBA(86, 204, 242, 128);
		outInstances.push_back(makeSolidRect(p.x, 0.0f, 1.0f, canvasH, lineCol));
		outInstances.push_back(makeSolidRect(right, 0.0f, 1.0f, canvasH, lineCol));
		outInstances.push_back(makeSolidRect(0.0f, p.y, canvasW, 1.0f, lineCol));
		outInstances.push_back(makeSolidRect(0.0f, bottom, canvasW, 1.0f, lineCol));
	}
}
#endif

void buildRulerToolInstances(
	const interface_elements::DevPreviewState& state,
	float canvasW, float canvasH,
	std::vector<UiInstance>& outInstances) {
	if (!state.ruler.pointAValid) return;
	const Clay_Vector2 a = worldToCanvas(state, state.ruler.pointAX, state.ruler.pointAY, canvasW, canvasH);
	const Clay_Vector2 b = worldToCanvas(state, state.ruler.pointBX, state.ruler.pointBY, canvasW, canvasH);
	const float left = std::min(a.x, b.x);
	const float top = std::min(a.y, b.y);
	const uint32_t seaGlass = packColorRGBA(46, 204, 113, 255);
	outInstances.push_back(makeSolidRect(left, a.y, std::max(1.5f, std::abs(b.x - a.x)), 1.5f, seaGlass));
	outInstances.push_back(makeSolidRect(b.x, top, 1.5f, std::max(1.5f, std::abs(b.y - a.y)), seaGlass));
	outInstances.push_back(makeSolidRect(a.x - 3.0f, a.y - 3.0f, 6.0f, 6.0f, seaGlass, 3.0f));
	outInstances.push_back(makeSolidRect(b.x - 3.0f, b.y - 3.0f, 6.0f, 6.0f, seaGlass, 3.0f));
}

} // namespace

struct DevPreviewViewPortRenderer::Impl {
	VkDevice device = VK_NULL_HANDLE;
	VmaAllocator allocator = nullptr;
	VkFormat colorFormat = VK_FORMAT_UNDEFINED;

	VkBuffer instanceBuffer = VK_NULL_HANDLE;
	VmaAllocation instanceAllocation = nullptr;
	VkDeviceSize instanceBufferSize = 0;
	void* mappedData = nullptr;

	std::vector<UiInstance> cpuInstances{};

	void init(const FlowUi::ViewPortVulkanInterop& interop, VkFormat format) {
		if (interop.device == VK_NULL_HANDLE) return;
		if (device == interop.device && colorFormat == format && instanceBuffer != VK_NULL_HANDLE) {
			return;
		}
		destroy();
		device = interop.device;
		allocator = interop.allocator;
		colorFormat = format;

		VkBufferCreateInfo bufferInfo{};
		bufferInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
		bufferInfo.size = 256u * sizeof(UiInstance);
		bufferInfo.usage = VK_BUFFER_USAGE_STORAGE_BUFFER_BIT;
		bufferInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

		VmaAllocationCreateInfo allocInfo{};
		allocInfo.usage = VMA_MEMORY_USAGE_AUTO_PREFER_HOST;
		allocInfo.flags = VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT |
		                  VMA_ALLOCATION_CREATE_MAPPED_BIT;

		VmaAllocationInfo resultInfo{};
		if (vmaCreateBuffer(allocator, &bufferInfo, &allocInfo, &instanceBuffer, &instanceAllocation, &resultInfo) == VK_SUCCESS) {
			instanceBufferSize = bufferInfo.size;
			mappedData = resultInfo.pMappedData;
		}
	}

	void destroy() {
		if (allocator && instanceBuffer) {
			vmaDestroyBuffer(allocator, instanceBuffer, instanceAllocation);
			instanceBuffer = VK_NULL_HANDLE;
			instanceAllocation = nullptr;
			mappedData = nullptr;
			instanceBufferSize = 0;
		}
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

void DevPreviewViewPortRenderer::init(
	const FlowUi::ViewPortVulkanInterop& interop, VkFormat colorFormat) {
	if (impl_) {
		impl_->init(interop, colorFormat);
	}
}

void DevPreviewViewPortRenderer::destroy() {
	if (impl_) {
		impl_->destroy();
	}
}

void DevPreviewViewPortRenderer::record(
	const FlowUi::ViewPortRenderContext& ctx,
	const interface_elements::DevPreviewState& state,
	const interface_elements::PreviewSelection& selection,
	const DevUiReplaySource& replay) {
	if (ctx.commandBuffer == VK_NULL_HANDLE || ctx.extent.width == 0 || ctx.extent.height == 0) {
		return;
	}

	VkViewport viewport{};
	viewport.x = 0.0f;
	viewport.y = 0.0f;
	viewport.width = static_cast<float>(ctx.extent.width);
	viewport.height = static_cast<float>(ctx.extent.height);
	viewport.minDepth = 0.0f;
	viewport.maxDepth = 1.0f;
	vkCmdSetViewport(ctx.commandBuffer, 0, 1, &viewport);

	VkRect2D scissor{};
	scissor.offset = {0, 0};
	scissor.extent = ctx.extent;
	vkCmdSetScissor(ctx.commandBuffer, 0, 1, &scissor);

	const float canvasW = static_cast<float>(ctx.extent.width);
	const float canvasH = static_cast<float>(ctx.extent.height);

	impl_->cpuInstances.clear();
	buildPreviewGridInstances(state, canvasW, canvasH, impl_->cpuInstances);
#if FLOW_UI_DEV_CAPTURE_CLAY
	buildCapturedSubtreeInstances(state, selection, replay, canvasW, canvasH, impl_->cpuInstances);
	buildSidecarInstances(state, selection, canvasW, canvasH, impl_->cpuInstances);
#else
	(void)selection;
	(void)replay;
#endif
	buildRulerToolInstances(state, canvasW, canvasH, impl_->cpuInstances);

	if (impl_->cpuInstances.empty() || !impl_->mappedData) {
		return;
	}

	const VkDeviceSize copySize = std::min(
		static_cast<VkDeviceSize>(impl_->cpuInstances.size() * sizeof(UiInstance)),
		impl_->instanceBufferSize);
	std::memcpy(impl_->mappedData, impl_->cpuInstances.data(), static_cast<size_t>(copySize));
	(void)vmaFlushAllocation(impl_->allocator, impl_->instanceAllocation, 0, copySize);
}

void recordDevPreviewViewPort(
	const FlowUi::ViewPortRenderContext& ctx,
	const interface_elements::DevPreviewState& state,
	const interface_elements::PreviewSelection& selection,
	const DevUiReplaySource& replay) {
	static DevPreviewViewPortRenderer s_renderer;
	if (ctx.vulkan) {
		s_renderer.init(*ctx.vulkan, ctx.colorFormat);
	}
	s_renderer.record(ctx, state, selection, replay);
}

} // namespace FlowUi::devSystems

#endif
