#include "Ui/Vk_UiRenderer.hpp"
#if FLOW_UI_DEV_MODE
#include "devSystems/devTooling/overlay/DevOverlayCommandBuffer.hpp"
#endif
#include "managers/InputFieldManager.hpp"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <limits>
#include <span>
#include <stdexcept>
#include <string>
#include <vector>

#if FLOW_UI_DEV_MODE
#include "devSystems/devMonitoringAndReporting/memory/DevContainerMemory.hpp"
#include "devSystems/devMonitoringAndReporting/memory/DevMemorySources.hpp"
#include "devSystems/devMonitoringAndReporting/timing/DevGpuTiming.hpp"
#include "devSystems/devMonitoringAndReporting/timing/DevTimingZone.hpp"
#endif

#if FLOW_UI_DEV_MODE && FLOWUI_DEV_MEMORY_LEVEL >= 2
void VulkanUiRenderer::appendDevMemorySamples(
	FlowUi::devSystems::MemorySampleSink& sink,
	const PreparedUiFrame* prepared) const noexcept {
	try {
		FlowUi::devSystems::DevContainerMemoryAccumulator memory{};
		memory.add(descriptors_.globalsSets);
		memory.add(descriptors_.texturesSets);
		memory.add(frameResources_);
		memory.add(boundFontAtlasRevisionByFrame_);
		memory.liveBytes += textLayoutService_.cacheBytes();
		memory.capacityBytes += textLayoutService_.cacheBytes();
		memory.objectCount += textLayoutService_.cacheEntryCount();
		memory.capacityCount += FlowUi::detail::text::TextLayoutService::MaxCacheEntries;
		if (prepared) {
			memory.liveBytes += static_cast<uint64_t>(prepared->instanceCount) * sizeof(UiInstance);
			memory.liveBytes += static_cast<uint64_t>(prepared->runs.size()) * sizeof(UiRun);
			memory.capacityBytes += static_cast<uint64_t>(prepared->runs.size()) * sizeof(UiRun);
			memory.objectCount += prepared->instanceCount + prepared->runs.size();
			memory.capacityCount += prepared->runs.size();
		}
		FlowUi::devSystems::appendManagerSample(
			sink, FlowUi::devSystems::memory_sources::kRenderer.id, memory, windowId_);
		if (prepared) {
			FlowUi::devSystems::MemoryValueSample framePayload{
				.source = FlowUi::devSystems::memory_sources::kRendererFramePayload.id,
				.window = windowId_,
				.logicalLiveBytes = static_cast<uint64_t>(prepared->instanceCount) * sizeof(UiInstance) +
					static_cast<uint64_t>(prepared->runs.size()) * sizeof(UiRun),
				.objectCount = prepared->instanceCount + prepared->runs.size(),
			};
			(void)sink.append(framePayload);
		}
	} catch (...) {}
}
#endif

namespace {

struct UiQuadVertex {
	float x = 0.0f;
	float y = 0.0f;
	float u = 0.0f;
	float v = 0.0f;
};

struct GlyphQuad {
	float x = 0.0f;
	float y = 0.0f;
	float w = 0.0f;
	float h = 0.0f;
	float u0 = 0.0f;
	float v0 = 0.0f;
	float u1 = 1.0f;
	float v1 = 1.0f;
	int byteStartOffset = 0;
	int byteEndOffset = 0;
};

struct UiPushConstants {
	float viewportW = 0.0f;
	float viewportH = 0.0f;
	uint32_t instanceBaseIndex = 0;
	uint32_t _pad = 0;
};

constexpr uint32_t kDefaultMaxUiImageDescriptors = 256;

constexpr const char* kUiSolidVertexShaderFile = "flowui_ui_solid.vert.spv";
constexpr const char* kUiMsdfVertexShaderFile = "flowui_ui_msdf.vert.spv";
constexpr const char* kUiTexturedVertexShaderFile = "flowui_ui_textured.vert.spv";
constexpr const char* kUiSolidFragmentShaderFile = "flowui_ui_solid.frag.spv";
constexpr const char* kUiMsdfFragmentShaderFile = "flowui_ui_msdf.frag.spv";
constexpr const char* kUiTexturedFragmentShaderFile = "flowui_ui_textured.frag.spv";

namespace storage = FlowUi::detail::storage;

template <typename NativeHandle>
static NativeHandle NativeHandleFromBits(uint64_t value) noexcept {
	if constexpr (std::is_pointer_v<NativeHandle>) {
		return reinterpret_cast<NativeHandle>(static_cast<uintptr_t>(value));
	} else {
		return static_cast<NativeHandle>(value);
	}
}

template <typename NativeHandle>
static uint64_t NativeHandleBits(NativeHandle value) noexcept {
	if constexpr (std::is_pointer_v<NativeHandle>) {
		return static_cast<uint64_t>(reinterpret_cast<uintptr_t>(value));
	} else {
		return static_cast<uint64_t>(value);
	}
}

constexpr storage::RendererLayoutKey kUiRendererLayoutKey{
	.textureDescriptorCapacity = kDefaultMaxUiImageDescriptors,
	.shaderInterfaceRevision = 1u,
	.pushConstantBytes = sizeof(UiPushConstants),
	.descriptorFeatureFlags = 0x3u,
};
constexpr uint32_t kUiPipelineStateRevision = 1u;
constexpr uint64_t kUiShaderSetFingerprint = 0x464c4f5755490003ull;

static void vkCheck(VkResult result, FlowUi::ErrorSite site) {
	if (result != VK_SUCCESS) {
		FlowUi::ErrorCode code = result == VK_ERROR_DEVICE_LOST
			? FlowUi::ErrorCode::VulkanDeviceLost : FlowUi::ErrorCode::VulkanNativeCallFailed;
		if (result == VK_ERROR_OUT_OF_HOST_MEMORY || result == VK_ERROR_OUT_OF_DEVICE_MEMORY) {
			code = FlowUi::ErrorCode::AllocationFailed;
		}
		throw FlowUi::FlowUiException(FlowUi::makeError(
			code, site, 0u, 0u,
			static_cast<std::uint32_t>(result)));
	}
}

static std::vector<char> readFile(const std::string& path) {
	std::ifstream file(path, std::ios::ate | std::ios::binary);
	if (!file) {
		throw FlowUi::FlowUiException(FlowUi::makeError(FlowUi::ErrorCode::ShaderUnavailable, FlowUi::ErrorSite::RendererLoadShader));
	}

	const std::streamsize size = file.tellg();
	if (size <= 0) {
		throw FlowUi::FlowUiException(FlowUi::makeError(FlowUi::ErrorCode::ShaderInvalid, FlowUi::ErrorSite::RendererLoadShader));
	}

	std::vector<char> buffer(static_cast<size_t>(size));
	file.seekg(0, std::ios::beg);
	file.read(buffer.data(), size);
	if (!file) {
		throw FlowUi::FlowUiException(FlowUi::makeError(FlowUi::ErrorCode::ShaderInvalid, FlowUi::ErrorSite::RendererLoadShader));
	}
	if ((buffer.size() % 4) != 0) {
		throw FlowUi::FlowUiException(FlowUi::makeError(FlowUi::ErrorCode::ShaderInvalid, FlowUi::ErrorSite::RendererLoadShader));
	}
	return buffer;
}

static std::vector<char> readShaderFile(const char* fileName) {
	const std::string relativePath = std::string("shaders/") + fileName;
#if defined(FLOWUI_SHADER_OUTPUT_DIR)
	const std::string configuredPath = std::string(FLOWUI_SHADER_OUTPUT_DIR) + "/" + fileName;
	if (std::filesystem::exists(configuredPath)) {
		return readFile(configuredPath);
	}
#endif
	if (std::filesystem::exists(relativePath)) {
		return readFile(relativePath);
	}

	(void)fileName;
	throw FlowUi::FlowUiException(FlowUi::makeError(FlowUi::ErrorCode::ShaderUnavailable, FlowUi::ErrorSite::RendererLoadShader));
}

static VkShaderModule createShaderModule(VkDevice device, const std::vector<char>& code) {
	VkShaderModuleCreateInfo createInfo{};
	createInfo.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
	createInfo.codeSize = code.size();
	createInfo.pCode = reinterpret_cast<const uint32_t*>(code.data());

	VkShaderModule module = VK_NULL_HANDLE;
	vkCheck(vkCreateShaderModule(device, &createInfo, nullptr, &module),
		FlowUi::ErrorSite::RendererLoadShader);
	return module;
}

static RectF Intersect(const RectF& a, const RectF& b) {
	const float x0 = std::max(a.x, b.x);
	const float y0 = std::max(a.y, b.y);
	const float x1 = std::min(a.x + a.w, b.x + b.w);
	const float y1 = std::min(a.y + a.h, b.y + b.h);
	return RectF{
		x0,
		y0,
		std::max(0.0f, x1 - x0),
		std::max(0.0f, y1 - y0),
	};
}

static bool RectEqual(const RectF& a, const RectF& b) {
	return a.x == b.x && a.y == b.y && a.w == b.w && a.h == b.h;
}

static RectF ScaleBoundingBox(const Clay_BoundingBox& box, float scaleX, float scaleY) {
	return RectF{
		box.x * scaleX,
		box.y * scaleY,
		box.width * scaleX,
		box.height * scaleY,
	};
}

static float UniformScale(float scaleX, float scaleY) {
	return std::min(scaleX, scaleY);
}

static VkRect2D ToVkRect2D(const RectF& r, int framebufferWidth, int framebufferHeight) {
	int32_t x0 = static_cast<int32_t>(std::floor(r.x));
	int32_t y0 = static_cast<int32_t>(std::floor(r.y));
	int32_t x1 = static_cast<int32_t>(std::ceil(r.x + r.w));
	int32_t y1 = static_cast<int32_t>(std::ceil(r.y + r.h));

	x0 = std::max<int32_t>(0, std::min<int32_t>(x0, framebufferWidth));
	y0 = std::max<int32_t>(0, std::min<int32_t>(y0, framebufferHeight));
	x1 = std::max<int32_t>(0, std::min<int32_t>(x1, framebufferWidth));
	y1 = std::max<int32_t>(0, std::min<int32_t>(y1, framebufferHeight));

	VkRect2D rect{};
	rect.offset = { x0, y0 };
	rect.extent = {
		static_cast<uint32_t>(std::max(0, x1 - x0)),
		static_cast<uint32_t>(std::max(0, y1 - y0)),
	};
	return rect;
}

static uint32_t PackRGBA8(const Clay_Color& color) {
	const auto pack = [](float v) -> uint32_t {
		const float clamped = std::clamp(v, 0.0f, 255.0f);
		return static_cast<uint32_t>(clamped + 0.5f);
	};
	return pack(color.r) | (pack(color.g) << 8u) | (pack(color.b) << 16u) | (pack(color.a) << 24u);
}

static bool ByteRangesIntersect(size_t aStart, size_t aEnd, size_t bStart, size_t bEnd) {
	return aStart < bEnd && aEnd > bStart;
}

constexpr uint32_t kTexturedFlagTintEnabled = 1u << 0u;

struct TexturedImagePlacement {
	RectF drawBounds{};
	float uv0x = 0.0f;
	float uv0y = 0.0f;
	float uv1x = 1.0f;
	float uv1y = 1.0f;
};

static const FlowUi::TextureRef& ResolveTextureRef(const Clay_RenderCommand& command) {
	static const FlowUi::TextureRef kDefaultTextureRef{};
	const auto* textureRef = reinterpret_cast<const FlowUi::TextureRef*>(command.renderData.image.imageData);
	return textureRef ? *textureRef : kDefaultTextureRef;
}

static bool HasValidSourceDimensions(const FlowUi::TextureRef& textureRef) {
	return textureRef.sourceWidth > 0 && textureRef.sourceHeight > 0;
}

static TexturedImagePlacement ResolveTexturedImagePlacement(
	const RectF& bounds,
	const FlowUi::TextureRef& textureRef)
{
	TexturedImagePlacement placement{};
	placement.drawBounds = bounds;
	placement.uv0x = textureRef.uv0x;
	placement.uv0y = textureRef.uv0y;
	placement.uv1x = textureRef.uv1x;
	placement.uv1y = textureRef.uv1y;

	if (bounds.w <= 0.0f || bounds.h <= 0.0f) {
		return placement;
	}
	if (!HasValidSourceDimensions(textureRef) || textureRef.fitMode == FlowUi::TextureFitMode::Stretch) {
		return placement;
	}

	const float sourceWidth = static_cast<float>(textureRef.sourceWidth);
	const float sourceHeight = static_cast<float>(textureRef.sourceHeight);
	const float sourceAspect = sourceWidth / sourceHeight;
	const float boundsAspect = bounds.w / bounds.h;

	switch (textureRef.fitMode)
	{
		case FlowUi::TextureFitMode::Contain:
		{
			const float scale = std::min(bounds.w / sourceWidth, bounds.h / sourceHeight);
			const float drawWidth = sourceWidth * scale;
			const float drawHeight = sourceHeight * scale;
			placement.drawBounds.x = bounds.x + (bounds.w - drawWidth) * 0.5f;
			placement.drawBounds.y = bounds.y + (bounds.h - drawHeight) * 0.5f;
			placement.drawBounds.w = drawWidth;
			placement.drawBounds.h = drawHeight;
			break;
		}
		case FlowUi::TextureFitMode::Cover:
		{
			const float uvRangeX = placement.uv1x - placement.uv0x;
			const float uvRangeY = placement.uv1y - placement.uv0y;

			if (sourceAspect > boundsAspect) {
				const float visibleSourceWidth = sourceHeight * boundsAspect;
				const float cropRatio = std::clamp((sourceWidth - visibleSourceWidth) / (2.0f * sourceWidth), 0.0f, 0.5f);
				placement.uv0x += uvRangeX * cropRatio;
				placement.uv1x -= uvRangeX * cropRatio;
			} else if (sourceAspect < boundsAspect) {
				const float visibleSourceHeight = sourceWidth / boundsAspect;
				const float cropRatio = std::clamp((sourceHeight - visibleSourceHeight) / (2.0f * sourceHeight), 0.0f, 0.5f);
				placement.uv0y += uvRangeY * cropRatio;
				placement.uv1y -= uvRangeY * cropRatio;
			}
			break;
		}
		case FlowUi::TextureFitMode::None:
		{
			placement.drawBounds.w = sourceWidth;
			placement.drawBounds.h = sourceHeight;
			placement.drawBounds.x = bounds.x + (bounds.w - sourceWidth) * 0.5f;
			placement.drawBounds.y = bounds.y + (bounds.h - sourceHeight) * 0.5f;
			break;
		}
		case FlowUi::TextureFitMode::Stretch:
		default:
			break;
	}

	return placement;
}

template <typename T>
struct BoundedWriter {
	std::span<T> storage{};
	size_t count = 0;

	void push(const T& value) {
		if (count >= storage.size()) {
			throw FlowUi::FlowUiException(FlowUi::makeError(FlowUi::ErrorCode::RendererCapacityExceeded, FlowUi::ErrorSite::RendererConvertCommands));
		}
		storage[count++] = value;
	}

	[[nodiscard]] bool empty() const noexcept { return count == 0; }
	[[nodiscard]] T& back() { return storage[count - 1u]; }
	void pop() noexcept { if (count > 0) --count; }
};

struct UiBuildUpperBound {
	size_t instances = 0;
	size_t runs = 0;
	size_t scissorDepth = 1;
};

static size_t CheckedSizeAdd(size_t lhs, size_t rhs) {
	if (rhs > std::numeric_limits<size_t>::max() - lhs) {
		throw FlowUi::FlowUiException(FlowUi::makeError(FlowUi::ErrorCode::ArithmeticOverflow, FlowUi::ErrorSite::RendererConvertCommands));
	}
	return lhs + rhs;
}

static UiBuildUpperBound ComputeBuildUpperBound(
	const Clay_RenderCommandArray& commands,
	const FlowUi::detail::InputFieldFrameOverrides& overrides) {
	if (commands.capacity < 0 || commands.length < 0 || commands.length > commands.capacity ||
		(commands.length > 0 && commands.internalArray == nullptr)) {
		throw FlowUi::FlowUiException(FlowUi::makeError(FlowUi::ErrorCode::RenderCommandInvalid, FlowUi::ErrorSite::RendererConvertCommands));
	}

	UiBuildUpperBound result{};
	result.instances = overrides.rects.size();
	result.runs = overrides.rects.size();
	result.scissorDepth = CheckedSizeAdd(
		static_cast<size_t>(commands.length), 1u);
	for (int32_t i = 0; i < commands.length; ++i) {
		const Clay_RenderCommand& command = commands.internalArray[i];
		size_t commandInstances = 0;
		switch (command.commandType) {
			case CLAY_RENDER_COMMAND_TYPE_RECTANGLE:
			case CLAY_RENDER_COMMAND_TYPE_BORDER:
			case CLAY_RENDER_COMMAND_TYPE_IMAGE:
				commandInstances = 1u;
				break;
			case CLAY_RENDER_COMMAND_TYPE_TEXT:
				commandInstances = static_cast<size_t>(std::max(1, command.renderData.text.stringContents.length));
				break;
			default:
				break;
		}
		result.instances = CheckedSizeAdd(result.instances, commandInstances);
		if (commandInstances > 0) result.runs = CheckedSizeAdd(result.runs, 1u);
	}
	if (result.instances > std::numeric_limits<uint32_t>::max() ||
		result.runs > std::numeric_limits<uint32_t>::max()) {
		throw FlowUi::FlowUiException(FlowUi::makeError(FlowUi::ErrorCode::RendererCapacityExceeded, FlowUi::ErrorSite::RendererConvertCommands));
	}
	return result;
}

static UiType PickType(const Clay_RenderCommand& command) {
	switch (command.commandType) {
		case CLAY_RENDER_COMMAND_TYPE_RECTANGLE:
		case CLAY_RENDER_COMMAND_TYPE_BORDER:
			return UiType::Solid;
		case CLAY_RENDER_COMMAND_TYPE_TEXT:
			return UiType::Msdf;
		case CLAY_RENDER_COMMAND_TYPE_IMAGE:
		case CLAY_RENDER_COMMAND_TYPE_CUSTOM:
			return UiType::Textured;
		default:
			return UiType::Solid;
	}
}

static void EmitSolidRect(
	const Clay_RenderCommand& command,
	float uiToFramebufferScaleX,
	float uiToFramebufferScaleY,
	BoundedWriter<UiInstance>& instances) {
	const RectF bounds = ScaleBoundingBox(command.boundingBox, uiToFramebufferScaleX, uiToFramebufferScaleY);
	const float radiusScale = UniformScale(uiToFramebufferScaleX, uiToFramebufferScaleY);
	UiInstance inst{};
	inst.type = static_cast<uint32_t>(UiType::Solid);
	inst.x = bounds.x;
	inst.y = bounds.y;
	inst.w = bounds.w;
	inst.h = bounds.h;
	inst.colorRGBA = PackRGBA8(command.renderData.rectangle.backgroundColor);
	inst.r0 = command.renderData.rectangle.cornerRadius.topLeft * radiusScale;
	inst.r1 = command.renderData.rectangle.cornerRadius.topRight * radiusScale;
	inst.r2 = command.renderData.rectangle.cornerRadius.bottomRight * radiusScale;
	inst.r3 = command.renderData.rectangle.cornerRadius.bottomLeft * radiusScale;
	inst.solidMode = 0u;
	instances.push(inst);
}

static void EmitSolidRectOverride(
	const FlowUi::detail::InputFieldRectOverride& rectOverride,
	float uiToFramebufferScaleX,
	float uiToFramebufferScaleY,
	BoundedWriter<UiInstance>& instances) {
	Clay_RenderCommand command{};
	command.commandType = CLAY_RENDER_COMMAND_TYPE_RECTANGLE;
	command.boundingBox = rectOverride.boundingBox;
	command.renderData.rectangle.backgroundColor = rectOverride.color;
	command.renderData.rectangle.cornerRadius = Clay_CornerRadius{};
	EmitSolidRect(command, uiToFramebufferScaleX, uiToFramebufferScaleY, instances);
}

static void EmitSolidBorder(
	const Clay_RenderCommand& command,
	float uiToFramebufferScaleX,
	float uiToFramebufferScaleY,
	BoundedWriter<UiInstance>& instances) {
	const RectF bounds = ScaleBoundingBox(command.boundingBox, uiToFramebufferScaleX, uiToFramebufferScaleY);
	const float radiusScale = UniformScale(uiToFramebufferScaleX, uiToFramebufferScaleY);
	UiInstance inst{};
	inst.type = static_cast<uint32_t>(UiType::Solid);
	inst.x = bounds.x;
	inst.y = bounds.y;
	inst.w = bounds.w;
	inst.h = bounds.h;
	inst.colorRGBA = PackRGBA8(command.renderData.border.color);
	inst.r0 = command.renderData.border.cornerRadius.topLeft * radiusScale;
	inst.r1 = command.renderData.border.cornerRadius.topRight * radiusScale;
	inst.r2 = command.renderData.border.cornerRadius.bottomRight * radiusScale;
	inst.r3 = command.renderData.border.cornerRadius.bottomLeft * radiusScale;
	inst.borderL = static_cast<float>(command.renderData.border.width.left) * uiToFramebufferScaleX;
	inst.borderT = static_cast<float>(command.renderData.border.width.top) * uiToFramebufferScaleY;
	inst.borderR = static_cast<float>(command.renderData.border.width.right) * uiToFramebufferScaleX;
	inst.borderB = static_cast<float>(command.renderData.border.width.bottom) * uiToFramebufferScaleY;
	inst.solidMode = 1u;
	instances.push(inst);
}

static void EmitTextMsdf(
	const Clay_RenderCommand& command,
	const FlowUi::detail::manager_storage::FontFrameView* fontView,
	FlowUi::detail::text::TextLayoutService& textLayoutService,
	float pointsToPixelsScale,
	float uiToFramebufferScaleX,
	float uiToFramebufferScaleY,
	const FlowUi::detail::InputFieldTextColorOverride* textColorOverride,
	uint8_t tabWidth,
	BoundedWriter<UiInstance>& instances,
	uint32_t& textGlyphCount)
{
	const Clay_BoundingBox& bounds = command.boundingBox;
	const Clay_TextRenderData& textData = command.renderData.text;

	const uint32_t defaultColor = PackRGBA8(textData.textColor);
	const bool hasTextColorOverride = textColorOverride && !textColorOverride->ranges.empty();
	const uint32_t overrideColor = hasTextColorOverride
		? PackRGBA8(textColorOverride->color)
		: 0u;
	size_t overrideCursor = 0u;
	const auto emitGlyph = [&](const GlyphQuad& glyph, uint32_t atlasLayer, float distanceRangePx) {
		uint32_t glyphColor = defaultColor;
		if (hasTextColorOverride) {
			const size_t glyphStart = static_cast<size_t>(std::max(0, glyph.byteStartOffset));
			const size_t glyphEnd = static_cast<size_t>(std::max(glyph.byteStartOffset, glyph.byteEndOffset));
			while (overrideCursor < textColorOverride->ranges.size() &&
				textColorOverride->ranges[overrideCursor].endByteOffset <= glyphStart) {
				++overrideCursor;
			}
			for (size_t rangeIndex = overrideCursor; rangeIndex < textColorOverride->ranges.size(); ++rangeIndex) {
				const FlowUi::detail::InputFieldTextColorRangeOverride& range = textColorOverride->ranges[rangeIndex];
				if (range.startByteOffset >= glyphEnd) break;
				if (ByteRangesIntersect(glyphStart, glyphEnd, range.startByteOffset, range.endByteOffset)) {
					glyphColor = overrideColor;
					break;
				}
			}
		}

		UiInstance inst{};
		inst.type = static_cast<uint32_t>(UiType::Msdf);
		inst.x = glyph.x * uiToFramebufferScaleX;
		inst.y = glyph.y * uiToFramebufferScaleY;
		inst.w = glyph.w * uiToFramebufferScaleX;
		inst.h = glyph.h * uiToFramebufferScaleY;
		inst.uv0x = glyph.u0;
		inst.uv0y = glyph.v0;
		inst.uv1x = glyph.u1;
		inst.uv1y = glyph.v1;
		inst.colorRGBA = glyphColor;
		inst.atlasLayer = atlasLayer;
		inst.r0 = distanceRangePx;
		instances.push(inst);
		++textGlyphCount;
	};

	const FlowUi::detail::text::TextLayoutResult& layoutResult = textLayoutService.layout(
		FlowUi::detail::text::TextLayoutRequest{
			.text = textData.stringContents.chars
				? std::string_view(
					textData.stringContents.chars,
					static_cast<size_t>(std::max(0, textData.stringContents.length)))
				: std::string_view{},
			.fontView = fontView,
			.fontId = static_cast<FlowUi::FontId>(textData.fontId),
			.pointsToPixelsScale = pointsToPixelsScale,
			.fontSize = textData.fontSize,
			.letterSpacing = textData.letterSpacing,
			.tabWidth = tabWidth,
			.includeGlyphGeometry = true,
		});
	if (layoutResult.success) {
		for (const FlowUi::detail::text::TextLayoutGlyph& glyph : layoutResult.glyphs) {
			emitGlyph(GlyphQuad{
				bounds.x + glyph.x, bounds.y + glyph.y, glyph.width, glyph.height,
				glyph.u0, glyph.v0, glyph.u1, glyph.v1,
				static_cast<int>(std::min<size_t>(glyph.clusterStartByte, static_cast<size_t>(std::numeric_limits<int>::max()))),
				static_cast<int>(std::min<size_t>(glyph.clusterEndByte, static_cast<size_t>(std::numeric_limits<int>::max()))),
			}, layoutResult.atlasLayer, layoutResult.distanceRangePx);
		}
	} else {
		emitGlyph(GlyphQuad{
			bounds.x, bounds.y, bounds.width, bounds.height,
			0.0f, 0.0f, 1.0f, 1.0f,
			0, std::max(0, textData.stringContents.length),
		}, 0u, 2.0f);
	}
}

static void EmitTexturedImage(
	const Clay_RenderCommand& command,
	float uiToFramebufferScaleX,
	float uiToFramebufferScaleY,
	std::span<const storage::BindingHotRecord> textureBindings,
	BoundedWriter<UiInstance>& instances)
{
	const RectF bounds = ScaleBoundingBox(command.boundingBox, uiToFramebufferScaleX, uiToFramebufferScaleY);
	const float radiusScale = UniformScale(uiToFramebufferScaleX, uiToFramebufferScaleY);
	const FlowUi::TextureRef& textureRef = ResolveTextureRef(command);
	if (!textureRef.handle && textureRef.skipIfUnavailable) {
		return;
	}
	const TexturedImagePlacement placement = ResolveTexturedImagePlacement(bounds, textureRef);
	(void)textureRef.samplingMode;
	if (placement.drawBounds.w <= 0.0f || placement.drawBounds.h <= 0.0f) {
		return;
	}

	UiInstance inst{};
	inst.type = static_cast<uint32_t>(UiType::Textured);
	inst.x = placement.drawBounds.x;
	inst.y = placement.drawBounds.y;
	inst.w = placement.drawBounds.w;
	inst.h = placement.drawBounds.h;
	inst.uv0x = placement.uv0x;
	inst.uv0y = placement.uv0y;
	inst.uv1x = placement.uv1x;
	inst.uv1y = placement.uv1y;
	inst.colorRGBA = PackRGBA8(command.renderData.image.backgroundColor);
	const storage::BindingHotRecord* binding = nullptr;
	if (textureRef.handle && textureRef.handle.index < textureBindings.size()) {
		const storage::BindingHotRecord& candidate = textureBindings[textureRef.handle.index];
		if (candidate.textureGeneration == textureRef.handle.generation) binding = &candidate;
	}
	inst.texIndex = binding ? binding->descriptorIndex : 0u;
	inst.solidMode = textureRef.tintEnabled ? kTexturedFlagTintEnabled : 0u;
	inst.r0 = command.renderData.image.cornerRadius.topLeft * radiusScale;
	inst.r1 = command.renderData.image.cornerRadius.topRight * radiusScale;
	inst.r2 = command.renderData.image.cornerRadius.bottomRight * radiusScale;
	inst.r3 = command.renderData.image.cornerRadius.bottomLeft * radiusScale;
	instances.push(inst);
}

struct UiBuildResult {
	uint32_t instanceCount = 0;
	uint32_t runCount = 0;
	uint32_t textGlyphCount = 0;
	uint32_t imageCommandCount = 0;
};

static UiBuildResult BuildInstancesAndRunsFromClay(
	const Clay_RenderCommandArray& commands,
	const FlowUi::detail::InputFieldFrameOverrides& inputFieldOverrides,
	VkExtent2D extent,
	const FlowUi::detail::manager_storage::FontFrameView* fontView,
	FlowUi::detail::text::TextLayoutService& textLayoutService,
	float pointsToPixelsScale,
	float uiToFramebufferScaleX,
	float uiToFramebufferScaleY,
	std::span<const storage::BindingHotRecord> textureBindings,
	std::span<UiInstance> instanceStorage,
	std::span<UiRun> runStorage,
	std::span<RectF> scissorStorage)
{
	BoundedWriter<UiInstance> outInstances{instanceStorage};
	BoundedWriter<UiRun> outRuns{runStorage};
	if (scissorStorage.empty()) {
		throw FlowUi::FlowUiException(FlowUi::makeError(FlowUi::ErrorCode::RendererCapacityExceeded, FlowUi::ErrorSite::RendererConvertCommands));
	}
	size_t scissorDepth = 1u;
	UiBuildResult result{};

	const RectF fullScissor{ 0.0f, 0.0f, static_cast<float>(extent.width), static_cast<float>(extent.height) };
	scissorStorage[0] = fullScissor;
	RectF currentScissor = fullScissor;

	bool runBarrier = false;

	auto closeActiveRun = [&]() {
		if (outRuns.empty()) {
			return;
		}
		UiRun& run = outRuns.back();
		run.instanceCount = static_cast<uint32_t>(outInstances.count) - run.firstInstance;
		if (run.instanceCount == 0) {
			outRuns.pop();
		}
	};

	auto beginRunIfNeeded = [&](UiType type, const RectF& scissor) {
		if (outRuns.empty() || runBarrier) {
			closeActiveRun();
			outRuns.push(UiRun{ type, scissor, static_cast<uint32_t>(outInstances.count), 0u });
			runBarrier = false;
			return;
		}

		const UiRun& current = outRuns.back();
		if (current.type != type || !RectEqual(current.scissor, scissor)) {
			closeActiveRun();
			outRuns.push(UiRun{ type, scissor, static_cast<uint32_t>(outInstances.count), 0u });
		}
	};

	const std::vector<FlowUi::detail::InputFieldRectOverride>& inputRectOverrides = inputFieldOverrides.rects;
	size_t inputRectOverrideCursor = 0u;
	const std::vector<FlowUi::detail::InputFieldTextColorOverride>& inputTextColorOverrides = inputFieldOverrides.textColorOverrides;
	size_t inputTextColorOverrideCursor = 0u;
	const std::vector<FlowUi::detail::InputFieldTextLayoutOverride>& inputTextLayoutOverrides = inputFieldOverrides.textLayoutOverrides;
	size_t inputTextLayoutOverrideCursor = 0u;
	auto emitInputOverridesBefore = [&](int32_t commandIndex) {
		while (inputRectOverrideCursor < inputRectOverrides.size()) {
			const FlowUi::detail::InputFieldRectOverride& rectOverride = inputRectOverrides[inputRectOverrideCursor];
			if (rectOverride.insertBeforeCommandIndex > commandIndex) {
				break;
			}
			beginRunIfNeeded(UiType::Solid, currentScissor);
			EmitSolidRectOverride(rectOverride, uiToFramebufferScaleX, uiToFramebufferScaleY, outInstances);
			++inputRectOverrideCursor;
		}
	};

	for (int32_t i = 0; i < commands.length; ++i) {
		emitInputOverridesBefore(i);
		while (
			inputTextColorOverrideCursor < inputTextColorOverrides.size() &&
			inputTextColorOverrides[inputTextColorOverrideCursor].commandIndex < i) {
			++inputTextColorOverrideCursor;
		}
		const FlowUi::detail::InputFieldTextColorOverride* inputTextColorOverride = nullptr;
		if (
			inputTextColorOverrideCursor < inputTextColorOverrides.size() &&
			inputTextColorOverrides[inputTextColorOverrideCursor].commandIndex == i) {
			inputTextColorOverride = &inputTextColorOverrides[inputTextColorOverrideCursor];
			++inputTextColorOverrideCursor;
			while (
				inputTextColorOverrideCursor < inputTextColorOverrides.size() &&
				inputTextColorOverrides[inputTextColorOverrideCursor].commandIndex == i) {
				++inputTextColorOverrideCursor;
			}
		}
		while (inputTextLayoutOverrideCursor < inputTextLayoutOverrides.size() &&
			inputTextLayoutOverrides[inputTextLayoutOverrideCursor].commandIndex < i) {
			++inputTextLayoutOverrideCursor;
		}
		uint8_t inputTabWidth = 4;
		if (inputTextLayoutOverrideCursor < inputTextLayoutOverrides.size() &&
			inputTextLayoutOverrides[inputTextLayoutOverrideCursor].commandIndex == i) {
			inputTabWidth = inputTextLayoutOverrides[inputTextLayoutOverrideCursor].tabWidth;
			++inputTextLayoutOverrideCursor;
		}
		const Clay_RenderCommand& command = commands.internalArray[i];

		if (command.commandType == CLAY_RENDER_COMMAND_TYPE_NONE) {
			continue;
		}
		if (command.commandType == CLAY_RENDER_COMMAND_TYPE_SCISSOR_START) {
			const RectF clip = ScaleBoundingBox(command.boundingBox, uiToFramebufferScaleX, uiToFramebufferScaleY);
			if (scissorDepth >= scissorStorage.size()) {
				throw FlowUi::FlowUiException(FlowUi::makeError(FlowUi::ErrorCode::RendererCapacityExceeded, FlowUi::ErrorSite::RendererConvertCommands));
			}
			scissorStorage[scissorDepth] = Intersect(scissorStorage[scissorDepth - 1u], clip);
			currentScissor = scissorStorage[scissorDepth++];
			runBarrier = true;
			continue;
		}
		if (command.commandType == CLAY_RENDER_COMMAND_TYPE_SCISSOR_END) {
			if (scissorDepth > 1u) --scissorDepth;
			currentScissor = scissorStorage[scissorDepth - 1u];
			runBarrier = true;
			continue;
		}
		if (command.commandType == CLAY_RENDER_COMMAND_TYPE_CUSTOM) {
			// Placeholder: custom command execution hook should run here in strict command order.
			closeActiveRun();
			runBarrier = true;
			continue;
		}
		if (command.commandType != CLAY_RENDER_COMMAND_TYPE_RECTANGLE &&
			command.commandType != CLAY_RENDER_COMMAND_TYPE_BORDER &&
			command.commandType != CLAY_RENDER_COMMAND_TYPE_TEXT &&
			command.commandType != CLAY_RENDER_COMMAND_TYPE_IMAGE) {
			continue;
		}

		const UiType type = PickType(command);
		beginRunIfNeeded(type, currentScissor);
		switch (command.commandType) {
			case CLAY_RENDER_COMMAND_TYPE_RECTANGLE:
				EmitSolidRect(command, uiToFramebufferScaleX, uiToFramebufferScaleY, outInstances);
				break;
			case CLAY_RENDER_COMMAND_TYPE_BORDER:
				EmitSolidBorder(command, uiToFramebufferScaleX, uiToFramebufferScaleY, outInstances);
				break;
			case CLAY_RENDER_COMMAND_TYPE_TEXT:
				EmitTextMsdf(
					command,
					fontView,
					textLayoutService,
					pointsToPixelsScale,
					uiToFramebufferScaleX,
					uiToFramebufferScaleY,
					inputTextColorOverride,
					inputTabWidth,
					outInstances,
					result.textGlyphCount);
				break;
			case CLAY_RENDER_COMMAND_TYPE_IMAGE:
				EmitTexturedImage(
					command, uiToFramebufferScaleX, uiToFramebufferScaleY, textureBindings, outInstances);
				++result.imageCommandCount;
				break;
			default:
				break;
		}
	}

	emitInputOverridesBefore(commands.length);
	closeActiveRun();
	result.instanceCount = static_cast<uint32_t>(outInstances.count);
	result.runCount = static_cast<uint32_t>(outRuns.count);
	return result;
}

static bool IsSrgbColorFormat(VkFormat format) {
	switch (format) {
		case VK_FORMAT_R8G8B8A8_SRGB:
		case VK_FORMAT_B8G8R8A8_SRGB:
			return true;
		default:
			return false;
	}
}

static VkPipeline createGraphicsPipeline(
	VkDevice device,
	VkPipelineLayout layout,
	VkFormat format,
	const char* vertexFile,
	const char* fragmentFile,
	bool requiresUvVertexAttribute) {
	if (format == VK_FORMAT_UNDEFINED) {
		throw FlowUi::FlowUiException(FlowUi::makeError(FlowUi::ErrorCode::RendererConfigurationInvalid, FlowUi::ErrorSite::RendererPublishPipeline));
	}

	const std::vector<char> vertexCode = readShaderFile(vertexFile);
	const std::vector<char> fragmentCode = readShaderFile(fragmentFile);
	VkShaderModule vertexModule = createShaderModule(device, vertexCode);
	VkShaderModule fragmentModule = createShaderModule(device, fragmentCode);

	VkPipelineShaderStageCreateInfo shaderStages[2]{};
	shaderStages[0].sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
	shaderStages[0].stage = VK_SHADER_STAGE_VERTEX_BIT;
	shaderStages[0].module = vertexModule;
	shaderStages[0].pName = "main";
	shaderStages[1].sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
	shaderStages[1].stage = VK_SHADER_STAGE_FRAGMENT_BIT;
	shaderStages[1].module = fragmentModule;
	shaderStages[1].pName = "main";

	const uint32_t decodeVertexColorFromSrgb = IsSrgbColorFormat(format) ? 1u : 0u;
	VkSpecializationMapEntry vertexSpecializationEntry{};
	vertexSpecializationEntry.constantID = 0u;
	vertexSpecializationEntry.offset = 0u;
	vertexSpecializationEntry.size = sizeof(uint32_t);

	VkSpecializationInfo vertexSpecializationInfo{};
	vertexSpecializationInfo.mapEntryCount = 1u;
	vertexSpecializationInfo.pMapEntries = &vertexSpecializationEntry;
	vertexSpecializationInfo.dataSize = sizeof(decodeVertexColorFromSrgb);
	vertexSpecializationInfo.pData = &decodeVertexColorFromSrgb;
	shaderStages[0].pSpecializationInfo = &vertexSpecializationInfo;

	VkVertexInputBindingDescription binding{};
	binding.binding = 0;
	binding.stride = sizeof(UiQuadVertex);
	binding.inputRate = VK_VERTEX_INPUT_RATE_VERTEX;

	VkVertexInputAttributeDescription attributes[2]{};
	attributes[0].location = 0;
	attributes[0].binding = 0;
	attributes[0].format = VK_FORMAT_R32G32_SFLOAT;
	attributes[0].offset = static_cast<uint32_t>(offsetof(UiQuadVertex, x));
	attributes[1].location = 1;
	attributes[1].binding = 0;
	attributes[1].format = VK_FORMAT_R32G32_SFLOAT;
	attributes[1].offset = static_cast<uint32_t>(offsetof(UiQuadVertex, u));

	VkPipelineVertexInputStateCreateInfo vertexInput{};
	vertexInput.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
	vertexInput.vertexBindingDescriptionCount = 1;
	vertexInput.pVertexBindingDescriptions = &binding;
	vertexInput.vertexAttributeDescriptionCount = requiresUvVertexAttribute ? 2u : 1u;
	vertexInput.pVertexAttributeDescriptions = attributes;

	VkPipelineInputAssemblyStateCreateInfo inputAssembly{};
	inputAssembly.sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
	inputAssembly.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_STRIP;
	inputAssembly.primitiveRestartEnable = VK_FALSE;

	VkPipelineViewportStateCreateInfo viewportState{};
	viewportState.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
	viewportState.viewportCount = 1;
	viewportState.scissorCount = 1;

	VkPipelineRasterizationStateCreateInfo rasterization{};
	rasterization.sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
	rasterization.depthClampEnable = VK_FALSE;
	rasterization.rasterizerDiscardEnable = VK_FALSE;
	rasterization.polygonMode = VK_POLYGON_MODE_FILL;
	rasterization.cullMode = VK_CULL_MODE_NONE;
	rasterization.frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE;
	rasterization.depthBiasEnable = VK_FALSE;
	rasterization.lineWidth = 1.0f;

	VkPipelineMultisampleStateCreateInfo multisample{};
	multisample.sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
	multisample.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;
	multisample.sampleShadingEnable = VK_FALSE;

	VkPipelineColorBlendAttachmentState blendAttachment{};
	blendAttachment.blendEnable = VK_TRUE;
	blendAttachment.srcColorBlendFactor = VK_BLEND_FACTOR_SRC_ALPHA;
	blendAttachment.dstColorBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
	blendAttachment.colorBlendOp = VK_BLEND_OP_ADD;
	blendAttachment.srcAlphaBlendFactor = VK_BLEND_FACTOR_ONE;
	blendAttachment.dstAlphaBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
	blendAttachment.alphaBlendOp = VK_BLEND_OP_ADD;
	blendAttachment.colorWriteMask =
		VK_COLOR_COMPONENT_R_BIT |
		VK_COLOR_COMPONENT_G_BIT |
		VK_COLOR_COMPONENT_B_BIT |
		VK_COLOR_COMPONENT_A_BIT;

	VkPipelineColorBlendStateCreateInfo blendState{};
	blendState.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
	blendState.logicOpEnable = VK_FALSE;
	blendState.attachmentCount = 1;
	blendState.pAttachments = &blendAttachment;

	const VkDynamicState dynamicStates[] = {
		VK_DYNAMIC_STATE_VIEWPORT,
		VK_DYNAMIC_STATE_SCISSOR,
	};
	VkPipelineDynamicStateCreateInfo dynamicState{};
	dynamicState.sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO;
	dynamicState.dynamicStateCount = 2;
	dynamicState.pDynamicStates = dynamicStates;

	VkPipelineRenderingCreateInfo rendering{};
	rendering.sType = VK_STRUCTURE_TYPE_PIPELINE_RENDERING_CREATE_INFO;
	rendering.colorAttachmentCount = 1;
	rendering.pColorAttachmentFormats = &format;

	VkGraphicsPipelineCreateInfo pipelineInfo{};
	pipelineInfo.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
	pipelineInfo.pNext = &rendering;
	pipelineInfo.stageCount = 2;
	pipelineInfo.pStages = shaderStages;
	pipelineInfo.pVertexInputState = &vertexInput;
	pipelineInfo.pInputAssemblyState = &inputAssembly;
	pipelineInfo.pViewportState = &viewportState;
	pipelineInfo.pRasterizationState = &rasterization;
	pipelineInfo.pMultisampleState = &multisample;
	pipelineInfo.pDepthStencilState = nullptr;
	pipelineInfo.pColorBlendState = &blendState;
	pipelineInfo.pDynamicState = &dynamicState;
	pipelineInfo.layout = layout;
	pipelineInfo.renderPass = VK_NULL_HANDLE;
	pipelineInfo.subpass = 0;

	VkPipeline pipeline = VK_NULL_HANDLE;
	const VkResult result = vkCreateGraphicsPipelines(device, VK_NULL_HANDLE, 1, &pipelineInfo, nullptr, &pipeline);

	vkDestroyShaderModule(device, fragmentModule, nullptr);
	vkDestroyShaderModule(device, vertexModule, nullptr);
	vkCheck(result, FlowUi::ErrorSite::RendererPublishPipeline);
	return pipeline;
}

static void DestroyPipelines(VkDevice device, VulkanUiRenderer::Pipelines& pipelines_) {
	if (pipelines_.solid != VK_NULL_HANDLE) {
		vkDestroyPipeline(device, pipelines_.solid, nullptr);
		pipelines_.solid = VK_NULL_HANDLE;
	}
	if (pipelines_.msdf != VK_NULL_HANDLE) {
		vkDestroyPipeline(device, pipelines_.msdf, nullptr);
		pipelines_.msdf = VK_NULL_HANDLE;
	}
	if (pipelines_.textured != VK_NULL_HANDLE) {
		vkDestroyPipeline(device, pipelines_.textured, nullptr);
		pipelines_.textured = VK_NULL_HANDLE;
	}
}

static void CreatePipelines(VkDevice device, VulkanUiRenderer::Pipelines& pipelines_, VkFormat format) {
	pipelines_.solid = createGraphicsPipeline(
		device,
		pipelines_.layout,
		format,
		kUiSolidVertexShaderFile,
		kUiSolidFragmentShaderFile,
		false);
	pipelines_.msdf = createGraphicsPipeline(
		device,
		pipelines_.layout,
		format,
		kUiMsdfVertexShaderFile,
		kUiMsdfFragmentShaderFile,
		true);
	pipelines_.textured = createGraphicsPipeline(
		device,
		pipelines_.layout,
		format,
		kUiTexturedVertexShaderFile,
		kUiTexturedFragmentShaderFile,
		true);
}

static void DestroyPipelineObjects(VkDevice device, VulkanUiRenderer::Pipelines& pipelines_) {
	DestroyPipelines(device, pipelines_);
	if (pipelines_.layout != VK_NULL_HANDLE) {
		vkDestroyPipelineLayout(device, pipelines_.layout, nullptr);
		pipelines_.layout = VK_NULL_HANDLE;
	}
}

static void DestroyDescriptorObjects(VkDevice device, VulkanUiRenderer::Descriptors& descriptors_) {
	if (descriptors_.pool != VK_NULL_HANDLE) {
		vkDestroyDescriptorPool(device, descriptors_.pool, nullptr);
		descriptors_.pool = VK_NULL_HANDLE;
	}
	descriptors_.globalsSets.clear();
	descriptors_.texturesSets.clear();

	if (descriptors_.set0 != VK_NULL_HANDLE) {
		vkDestroyDescriptorSetLayout(device, descriptors_.set0, nullptr);
		descriptors_.set0 = VK_NULL_HANDLE;
	}
	if (descriptors_.set1 != VK_NULL_HANDLE) {
		vkDestroyDescriptorSetLayout(device, descriptors_.set1, nullptr);
		descriptors_.set1 = VK_NULL_HANDLE;
	}
}

static void CreateLayoutObjects(VkDevice device, VulkanUiRenderer& renderer) {
	if (renderer.maxUiImageDescriptors_ == 0u) {
		throw FlowUi::FlowUiException(FlowUi::makeError(FlowUi::ErrorCode::RendererConfigurationInvalid, FlowUi::ErrorSite::RendererPublishLayout));
	}
	VkDescriptorSetLayoutBinding globalsBinding{};
	globalsBinding.binding = 0;
	globalsBinding.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
	globalsBinding.descriptorCount = 1;
	globalsBinding.stageFlags = VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT;

	VkDescriptorSetLayoutCreateInfo set0Info{};
	set0Info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
	set0Info.bindingCount = 1;
	set0Info.pBindings = &globalsBinding;
	vkCheck(vkCreateDescriptorSetLayout(device, &set0Info, nullptr, &renderer.descriptors_.set0),
		FlowUi::ErrorSite::RendererPublishLayout);

	std::array<VkDescriptorSetLayoutBinding, 2> textureBindings{};
	textureBindings[0].binding = 0;
	textureBindings[0].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
	textureBindings[0].descriptorCount = 1;
	textureBindings[0].stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;

	textureBindings[1].binding = 1;
	textureBindings[1].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
	textureBindings[1].descriptorCount = renderer.maxUiImageDescriptors_;
	textureBindings[1].stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;

	const std::array<VkDescriptorBindingFlags, 2> bindingFlags = {
		0u,
		VK_DESCRIPTOR_BINDING_PARTIALLY_BOUND_BIT | VK_DESCRIPTOR_BINDING_UPDATE_AFTER_BIND_BIT,
	};
	VkDescriptorSetLayoutBindingFlagsCreateInfo bindingFlagsInfo{};
	bindingFlagsInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_BINDING_FLAGS_CREATE_INFO;
	bindingFlagsInfo.bindingCount = static_cast<uint32_t>(bindingFlags.size());
	bindingFlagsInfo.pBindingFlags = bindingFlags.data();

	VkDescriptorSetLayoutCreateInfo set1Info{};
	set1Info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
	set1Info.pNext = &bindingFlagsInfo;
	set1Info.flags = VK_DESCRIPTOR_SET_LAYOUT_CREATE_UPDATE_AFTER_BIND_POOL_BIT;
	set1Info.bindingCount = static_cast<uint32_t>(textureBindings.size());
	set1Info.pBindings = textureBindings.data();
	vkCheck(vkCreateDescriptorSetLayout(device, &set1Info, nullptr, &renderer.descriptors_.set1),
		FlowUi::ErrorSite::RendererPublishLayout);

	const std::array<VkDescriptorSetLayout, 2> setLayouts = {
		renderer.descriptors_.set0,
		renderer.descriptors_.set1,
	};
	VkPushConstantRange pushRange{};
	pushRange.stageFlags = VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT;
	pushRange.offset = 0;
	pushRange.size = sizeof(UiPushConstants);
	VkPipelineLayoutCreateInfo pipelineLayoutInfo{};
	pipelineLayoutInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
	pipelineLayoutInfo.setLayoutCount = static_cast<uint32_t>(setLayouts.size());
	pipelineLayoutInfo.pSetLayouts = setLayouts.data();
	pipelineLayoutInfo.pushConstantRangeCount = 1;
	pipelineLayoutInfo.pPushConstantRanges = &pushRange;
	vkCheck(vkCreatePipelineLayout(device, &pipelineLayoutInfo, nullptr, &renderer.pipelines_.layout),
		FlowUi::ErrorSite::RendererPublishLayout);
}

static void CreateDescriptorObjects(VkDevice device, VulkanUiRenderer& renderer) {
	if (renderer.descriptors_.set0 == VK_NULL_HANDLE || renderer.descriptors_.set1 == VK_NULL_HANDLE) {
		FlowUi::detail::terminateForFatalError(
			FlowUi::makeError(FlowUi::ErrorCode::RendererNativeResourceInvalid, FlowUi::ErrorSite::RendererPublishDescriptors));
	}
	renderer.frameResourceCount_ = std::max<uint32_t>(1u, renderer.frameResourceCount_);

	std::array<VkDescriptorPoolSize, 2> poolSizes{};
	poolSizes[0].type = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
	poolSizes[0].descriptorCount = renderer.frameResourceCount_;
	poolSizes[1].type = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
	poolSizes[1].descriptorCount = renderer.frameResourceCount_ * (1u + renderer.maxUiImageDescriptors_);

	VkDescriptorPoolCreateInfo poolInfo{};
	poolInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
	poolInfo.flags = VK_DESCRIPTOR_POOL_CREATE_UPDATE_AFTER_BIND_BIT;
	poolInfo.maxSets = renderer.frameResourceCount_ * 2u;
	poolInfo.poolSizeCount = static_cast<uint32_t>(poolSizes.size());
	poolInfo.pPoolSizes = poolSizes.data();
	vkCheck(vkCreateDescriptorPool(device, &poolInfo, nullptr, &renderer.descriptors_.pool),
		FlowUi::ErrorSite::RendererPublishDescriptors);

	renderer.descriptors_.globalsSets.assign(renderer.frameResourceCount_, VK_NULL_HANDLE);
	renderer.descriptors_.texturesSets.assign(renderer.frameResourceCount_, VK_NULL_HANDLE);

	std::vector<VkDescriptorSetLayout> set0Layouts(renderer.frameResourceCount_, renderer.descriptors_.set0);
	VkDescriptorSetAllocateInfo set0AllocInfo{};
	set0AllocInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
	set0AllocInfo.descriptorPool = renderer.descriptors_.pool;
	set0AllocInfo.descriptorSetCount = renderer.frameResourceCount_;
	set0AllocInfo.pSetLayouts = set0Layouts.data();
	vkCheck(
		vkAllocateDescriptorSets(device, &set0AllocInfo, renderer.descriptors_.globalsSets.data()),
		FlowUi::ErrorSite::RendererPublishDescriptors);

	std::vector<VkDescriptorSetLayout> set1Layouts(renderer.frameResourceCount_, renderer.descriptors_.set1);
	VkDescriptorSetAllocateInfo set1AllocInfo{};
	set1AllocInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
	set1AllocInfo.descriptorPool = renderer.descriptors_.pool;
	set1AllocInfo.descriptorSetCount = renderer.frameResourceCount_;
	set1AllocInfo.pSetLayouts = set1Layouts.data();
	vkCheck(
		vkAllocateDescriptorSets(device, &set1AllocInfo, renderer.descriptors_.texturesSets.data()),
		FlowUi::ErrorSite::RendererPublishDescriptors);
}

static void CreatePipelineObjects(VkDevice device, VulkanUiRenderer& renderer) {
	if (renderer.pipelines_.layout == VK_NULL_HANDLE) {
		FlowUi::detail::terminateForFatalError(
			FlowUi::makeError(FlowUi::ErrorCode::RendererNativeResourceInvalid, FlowUi::ErrorSite::RendererPublishPipeline));
	}
	CreatePipelines(device, renderer.pipelines_, renderer.targetFormat_);
}

static void UpdateInstanceBufferDescriptorForFrame(const VulkanUiRenderer& renderer, VkDevice device, uint32_t frameSlot) {
	if (frameSlot >= renderer.descriptors_.globalsSets.size() || frameSlot >= renderer.frameResources_.size()) {
		return;
	}
	if (renderer.descriptors_.globalsSets[frameSlot] == VK_NULL_HANDLE) {
		return;
	}
	const VulkanUiRenderer::UiFrameResources& instanceBuffer = renderer.frameResources_[frameSlot];
	if (instanceBuffer.nativeBuffer.nativeBuffer == 0) {
		return;
	}

	VkDescriptorBufferInfo ssboInfo{};
	ssboInfo.buffer = NativeHandleFromBits<VkBuffer>(instanceBuffer.nativeBuffer.nativeBuffer);
	ssboInfo.offset = 0;
	ssboInfo.range = instanceBuffer.capacityBytes;

	VkWriteDescriptorSet write{};
	write.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
	write.dstSet = renderer.descriptors_.globalsSets[frameSlot];
	write.dstBinding = 0;
	write.dstArrayElement = 0;
	write.descriptorCount = 1;
	write.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
	write.pBufferInfo = &ssboInfo;
	vkUpdateDescriptorSets(device, 1, &write, 0, nullptr);
}

static void UpdateFontDescriptorForFrame(
	VulkanUiRenderer& renderer,
	VkDevice device,
	uint32_t frameSlot,
	const FlowUi::detail::manager_storage::FontFrameView* fontView) {
	if (frameSlot >= renderer.descriptors_.texturesSets.size()) {
		return;
	}
	if (renderer.descriptors_.texturesSets[frameSlot] == VK_NULL_HANDLE) {
		return;
	}
	VkDescriptorImageInfo fontAtlasInfo{};
	if (!renderer.sharedByteResources_) return;
	fontAtlasInfo.sampler = renderer.sharedByteResources_->nativeLinearSampler;
	fontAtlasInfo.imageView = renderer.sharedByteResources_->nativePlaceholderFontView;
	fontAtlasInfo.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
	if (fontView) {
		const FlowUi::Font::AtlasArrayResource& atlas = fontView->atlas;
		if (atlas.view != VK_NULL_HANDLE && atlas.sampler != VK_NULL_HANDLE && atlas.layersUsed > 0u) {
			fontAtlasInfo.sampler = atlas.sampler;
			fontAtlasInfo.imageView = atlas.view;
		}
	}

	VkWriteDescriptorSet write{};
	write.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
	write.dstSet = renderer.descriptors_.texturesSets[frameSlot];
	write.dstBinding = 0;
	write.descriptorCount = 1;
	write.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
	write.pImageInfo = &fontAtlasInfo;
	vkUpdateDescriptorSets(device, 1, &write, 0, nullptr);
}

static void InitializeDescriptorBindings(VulkanContext& vk, VulkanUiRenderer& renderer) {
	if (renderer.boundFontAtlasRevisionByFrame_.size() != renderer.frameResourceCount_) {
		renderer.boundFontAtlasRevisionByFrame_.assign(renderer.frameResourceCount_, UINT32_MAX);
	}
	for (uint32_t frameSlot = 0u; frameSlot < renderer.frameResourceCount_; ++frameSlot) {
		UpdateInstanceBufferDescriptorForFrame(renderer, vk.device, frameSlot);
		UpdateFontDescriptorForFrame(renderer, vk.device, frameSlot, nullptr);
		renderer.boundFontAtlasRevisionByFrame_[frameSlot] = UINT32_MAX;
	}
}

static void EnsureInstanceBufferCapacity(
	VulkanContext& vk,
	VulkanUiRenderer& renderer,
	uint32_t frameSlot,
	uint64_t requiredBytes) {
	if (requiredBytes == 0) return;
	if (!renderer.storage_ || renderer.windowId_ == FlowUi::InvalidWindowId) {
		throw FlowUi::FlowUiException(FlowUi::makeError(FlowUi::ErrorCode::ObjectNotInitialized, FlowUi::ErrorSite::RendererPublishDescriptors));
	}
	if (frameSlot >= renderer.frameResources_.size()) {
		throw FlowUi::FlowUiException(FlowUi::makeError(FlowUi::ErrorCode::FramePhaseViolation, FlowUi::ErrorSite::RendererPublishDescriptors));
	}

	VulkanUiRenderer::UiFrameResources& slot = renderer.frameResources_[frameSlot];
	if (slot.instanceBuffer && slot.nativeBuffer.nativeBuffer != 0 && slot.capacityBytes >= requiredBytes) return;
	if (slot.capacityBytes != 0u && !renderer.allowInstanceGrowth_) {
		throw FlowUi::FlowUiException(FlowUi::makeError(
			FlowUi::ErrorCode::RendererCapacityExceeded, FlowUi::ErrorSite::RendererPublishDescriptors,
			renderer.windowId_,
			requiredBytes));
	}

	const uint64_t newSize = FlowUi::detail::growUiInstanceCapacity(
		slot.capacityBytes, requiredBytes, renderer.initialInstanceBytes_);

	storage::BufferDesc desc{};
	desc.size = newSize;
	desc.usage = storage::BufferUsage::Storage;
	desc.memory = storage::MemoryPreference::HostVisible;
	desc.sharing = storage::ResourceSharing::FrameLocal;
	desc.access = storage::AccessMode::CpuWrite;
	desc.persistentlyMapped = true;
	desc.window = renderer.windowId_;
	desc.frameSlot = frameSlot;
	desc.debugName = renderer.storage_->intern("FlowUi UI instance buffer");

	storage::BufferHandle replacement = renderer.storage_->createBuffer(desc);
	try {
		const storage::NativeBufferView replacementNative = renderer.storage_->nativeBuffer(replacement);
		if (replacementNative.nativeBuffer == 0 || replacementNative.size < newSize) {
			FlowUi::detail::terminateForFatalError(
				FlowUi::makeError(FlowUi::ErrorCode::RendererGenerationStale, FlowUi::ErrorSite::RendererPublishDescriptors));
		}

		const storage::BufferHandle oldHandle = slot.instanceBuffer;
		const storage::NativeBufferView oldNative = slot.nativeBuffer;
		const uint64_t oldCapacity = slot.capacityBytes;
		slot.instanceBuffer = replacement;
		slot.nativeBuffer = replacementNative;
		slot.capacityBytes = newSize;
		try {
			UpdateInstanceBufferDescriptorForFrame(renderer, vk.device, frameSlot);
		} catch (...) {
			slot.instanceBuffer = oldHandle;
			slot.nativeBuffer = oldNative;
			slot.capacityBytes = oldCapacity;
			throw;
		}
		replacement = {};
		if (oldHandle) renderer.storage_->releaseBuffer(oldHandle);
	} catch (...) {
		if (replacement) renderer.storage_->releaseBuffer(replacement);
		throw;
	}
}

static VkPipeline PipelineForType(const VulkanUiRenderer::Pipelines& pipelines_, UiType type) {
	switch (type) {
		case UiType::Solid:
			return pipelines_.solid;
		case UiType::Msdf:
			return pipelines_.msdf;
		case UiType::Textured:
			return pipelines_.textured;
		default:
			return pipelines_.solid;
	}
}

static void FlushRun(VkCommandBuffer commandBuffer, const VulkanUiRenderer& renderer, VkExtent2D extent, const UiRun& run)
{
	if (run.instanceCount == 0)
		return;

	const VkRect2D scissor = ToVkRect2D(run.scissor, static_cast<int>(extent.width), static_cast<int>(extent.height));
	vkCmdSetScissor(commandBuffer, 0, 1, &scissor);

	const VkPipeline pipeline = PipelineForType(renderer.pipelines_, run.type);
	vkCmdBindPipeline(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline);

	UiPushConstants push{};
	push.viewportW = static_cast<float>(extent.width);
	push.viewportH = static_cast<float>(extent.height);
	push.instanceBaseIndex = run.firstInstance;
	vkCmdPushConstants(
		commandBuffer,
		renderer.pipelines_.layout,
		VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT,
		0,
		static_cast<uint32_t>(sizeof(UiPushConstants)),
		&push);

	vkCmdDraw(commandBuffer, 4, run.instanceCount, 0, 0);
}

} // namespace

FlowUi::detail::UiConversionCapacity FlowUi::detail::measureUiConversionCapacity(
	const Clay_RenderCommandArray& commands,
	const InputFieldFrameOverrides& overrides) {
	const UiBuildUpperBound upper = ComputeBuildUpperBound(commands, overrides);
	return UiConversionCapacity{
		.instances = upper.instances,
		.runs = upper.runs,
		.scissorDepth = upper.scissorDepth,
	};
}

FlowUi::detail::UiConversionResult FlowUi::detail::buildUiInstancesDirect(
	const Clay_RenderCommandArray& commands,
	const InputFieldFrameOverrides& overrides,
	VkExtent2D extent,
	const manager_storage::FontFrameView* fontView,
	float pointsToPixelsScale,
	float uiToFramebufferScaleX,
	float uiToFramebufferScaleY,
	std::span<UiInstance> instances,
	std::span<UiRun> runs,
	std::span<RectF> scissorStack,
	std::span<const storage::BindingHotRecord> textureBindings) {
	text::TextLayoutService textLayoutService;
	const UiBuildResult built = BuildInstancesAndRunsFromClay(
		commands,
		overrides,
		extent,
		fontView,
		textLayoutService,
		pointsToPixelsScale,
		uiToFramebufferScaleX,
		uiToFramebufferScaleY,
		textureBindings,
		instances,
		runs,
		scissorStack);
	return UiConversionResult{
		.instanceCount = built.instanceCount,
		.runCount = built.runCount,
		.textGlyphCount = built.textGlyphCount,
		.imageCommandCount = built.imageCommandCount,
	};
}

uint64_t FlowUi::detail::growUiInstanceCapacity(
	uint64_t currentBytes,
	uint64_t requiredBytes,
	uint64_t initialBytes) {
	if (requiredBytes == 0) return currentBytes;
	uint64_t result = currentBytes > 0 ? currentBytes : std::max<uint64_t>(initialBytes, 1u);
	while (result < requiredBytes) {
		const uint64_t increment = std::max<uint64_t>(1u, result / 2u);
		if (increment > std::numeric_limits<uint64_t>::max() - result) {
			throw FlowUi::FlowUiException(FlowUi::makeError(FlowUi::ErrorCode::RendererCapacityExceeded, FlowUi::ErrorSite::RendererConvertCommands));
		}
		result += increment;
	}
	return result;
}

void destroySharedUiByteResources(
	storage::IStorageSystem& storageSystem,
	SharedUiByteResources& resources) noexcept {
	try { if (resources.placeholderFontView) storageSystem.releaseImageView(resources.placeholderFontView); } catch (...) {}
	try { if (resources.placeholderUiView) storageSystem.releaseImageView(resources.placeholderUiView); } catch (...) {}
	try { if (resources.placeholderFontImage) storageSystem.releaseImage(resources.placeholderFontImage); } catch (...) {}
	try { if (resources.placeholderUiImage) storageSystem.releaseImage(resources.placeholderUiImage); } catch (...) {}
	try { if (resources.linearSampler) storageSystem.releaseSampler(resources.linearSampler); } catch (...) {}
	try { if (resources.quadBuffer) storageSystem.releaseBuffer(resources.quadBuffer); } catch (...) {}
	resources = {};
}

void initSharedUiByteResources(
	storage::IStorageSystem& storageSystem,
	SharedUiByteResources& resources) {
	destroySharedUiByteResources(storageSystem, resources);
	std::array<storage::BlobHandle, 3> uploadBlobs{};
	try {
		const std::array<UiQuadVertex, 4> quadVertices = {
			UiQuadVertex{0.0f, 0.0f, 0.0f, 0.0f},
			UiQuadVertex{1.0f, 0.0f, 1.0f, 0.0f},
			UiQuadVertex{0.0f, 1.0f, 0.0f, 1.0f},
			UiQuadVertex{1.0f, 1.0f, 1.0f, 1.0f},
		};
		storage::BufferDesc quadDesc{};
		quadDesc.size = sizeof(quadVertices);
		quadDesc.usage = storage::BufferUsage::Vertex | storage::BufferUsage::TransferDestination;
		quadDesc.memory = storage::MemoryPreference::DeviceLocal;
		quadDesc.debugName = storageSystem.intern("FlowUi shared UI quad");
		resources.quadBuffer = storageSystem.createBuffer(quadDesc);

		storage::ImageDesc fontImageDesc{};
		fontImageDesc.format = storage::PixelFormat::Rgba8Unorm;
		fontImageDesc.type = storage::ImageType::Image2DArray;
		fontImageDesc.usage = storage::ImageUsage::Sampled | storage::ImageUsage::TransferDestination;
		fontImageDesc.memory = storage::MemoryPreference::DeviceLocal;
		fontImageDesc.debugName = storageSystem.intern("FlowUi placeholder font image");
		resources.placeholderFontImage = storageSystem.createImage(fontImageDesc);
		storage::ImageViewDesc fontViewDesc{};
		fontViewDesc.type = storage::ImageType::Image2DArray;
		fontViewDesc.debugName = storageSystem.intern("FlowUi placeholder font view");
		resources.placeholderFontView = storageSystem.createImageView(resources.placeholderFontImage, fontViewDesc);

		storage::ImageDesc uiImageDesc{};
		uiImageDesc.format = storage::PixelFormat::Rgba8Unorm;
		uiImageDesc.usage = storage::ImageUsage::Sampled | storage::ImageUsage::TransferDestination;
		uiImageDesc.memory = storage::MemoryPreference::DeviceLocal;
		uiImageDesc.debugName = storageSystem.intern("FlowUi placeholder UI image");
		resources.placeholderUiImage = storageSystem.createImage(uiImageDesc);
		storage::ImageViewDesc uiViewDesc{};
		uiViewDesc.debugName = storageSystem.intern("FlowUi placeholder UI view");
		resources.placeholderUiView = storageSystem.createImageView(resources.placeholderUiImage, uiViewDesc);

		storage::SamplerDesc samplerDesc{};
		samplerDesc.debugName = storageSystem.intern("FlowUi shared linear sampler");
		resources.linearSampler = storageSystem.acquireSampler(samplerDesc);

		uploadBlobs[0] = storageSystem.createBlob(
			std::as_bytes(std::span<const UiQuadVertex>(quadVertices)),
			storageSystem.intern("FlowUi quad upload"));
		const std::array<std::byte, 4> transparentPixel{};
		uploadBlobs[1] = storageSystem.createBlob(transparentPixel, storageSystem.intern("FlowUi font placeholder upload"));
		uploadBlobs[2] = storageSystem.createBlob(transparentPixel, storageSystem.intern("FlowUi UI placeholder upload"));

		(void)storageSystem.enqueueUpload(storage::UploadRequest{
			.destination = storage::UploadDestination::Buffer,
			.source = uploadBlobs[0],
			.byteCount = sizeof(quadVertices),
			.destinationBuffer = resources.quadBuffer,
		});
		(void)storageSystem.enqueueUpload(storage::UploadRequest{
			.destination = storage::UploadDestination::Image,
			.source = uploadBlobs[1],
			.byteCount = transparentPixel.size(),
			.destinationImage = resources.placeholderFontImage,
		});
		(void)storageSystem.enqueueUpload(storage::UploadRequest{
			.destination = storage::UploadDestination::Image,
			.source = uploadBlobs[2],
			.byteCount = transparentPixel.size(),
			.destinationImage = resources.placeholderUiImage,
		});
		storageSystem.flushUploads();
		for (const storage::BlobHandle blob : uploadBlobs) storageSystem.releaseBlob(blob);
		uploadBlobs = {};
		storageSystem.collect();

		const storage::NativeBufferView quadNative = storageSystem.nativeBuffer(resources.quadBuffer);
		const storage::NativeImageViewInfo fontViewNative = storageSystem.nativeImageView(resources.placeholderFontView);
		const storage::NativeImageViewInfo uiViewNative = storageSystem.nativeImageView(resources.placeholderUiView);
		const storage::NativeSamplerInfo samplerNative = storageSystem.nativeSampler(resources.linearSampler);
		if (quadNative.nativeBuffer == 0 || fontViewNative.nativeImageView == 0 ||
			uiViewNative.nativeImageView == 0 || samplerNative.nativeSampler == 0) {
			FlowUi::detail::terminateForFatalError(
				FlowUi::makeError(FlowUi::ErrorCode::RendererNativeResourceInvalid, FlowUi::ErrorSite::RendererConvertCommands));
		}
		resources.nativeQuadBuffer = NativeHandleFromBits<VkBuffer>(quadNative.nativeBuffer);
		resources.nativePlaceholderFontView = NativeHandleFromBits<VkImageView>(fontViewNative.nativeImageView);
		resources.nativePlaceholderUiView = NativeHandleFromBits<VkImageView>(uiViewNative.nativeImageView);
		resources.nativeLinearSampler = NativeHandleFromBits<VkSampler>(samplerNative.nativeSampler);
	} catch (...) {
		for (const storage::BlobHandle blob : uploadBlobs) {
			try { if (blob) storageSystem.releaseBlob(blob); } catch (...) {}
		}
		destroySharedUiByteResources(storageSystem, resources);
		try { storageSystem.collect(); } catch (...) {}
		throw;
	}
}

void VulkanUiRenderer::init(
	const FlowUi::VulkanConfig& vulkanConfig,
	const FlowUi::UiConfig& uiConfig,
	VulkanContext& vk,
	VkFormat swapFormat,
	storage::IStorageSystem& storageSystem,
	FlowUi::WindowId windowId,
	const SharedUiByteResources& sharedResources,
	uint64_t initialInstanceBytes,
	uint32_t textureDescriptorCapacity,
	bool allowInstanceGrowth) {
	if (vk.device == VK_NULL_HANDLE || vk.allocator == nullptr) {
		throw FlowUi::FlowUiException(FlowUi::makeError(FlowUi::ErrorCode::ObjectNotInitialized, FlowUi::ErrorSite::RendererInitialize));
	}
	if (swapFormat == VK_FORMAT_UNDEFINED) {
		throw FlowUi::FlowUiException(FlowUi::makeError(FlowUi::ErrorCode::RendererConfigurationInvalid, FlowUi::ErrorSite::RendererInitialize));
	}

	destroy(vk, storageSystem);

	try {
		storage_ = &storageSystem;
		windowId_ = windowId;
		sharedByteResources_ = &sharedResources;
		initialInstanceBytes_ = std::max<uint64_t>(1u, initialInstanceBytes);
		allowInstanceGrowth_ = allowInstanceGrowth;
		maxUiImageDescriptors_ = textureDescriptorCapacity;
		if (maxUiImageDescriptors_ != kDefaultMaxUiImageDescriptors) {
			throw FlowUi::FlowUiException(FlowUi::makeError(FlowUi::ErrorCode::RendererConfigurationInvalid, FlowUi::ErrorSite::RendererInitialize));
		}
		VkPhysicalDeviceDescriptorIndexingProperties indexingProperties{};
		indexingProperties.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_DESCRIPTOR_INDEXING_PROPERTIES;
		VkPhysicalDeviceProperties2 properties{};
		properties.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PROPERTIES_2;
		properties.pNext = &indexingProperties;
		vkGetPhysicalDeviceProperties2(vk.phys, &properties);
		const uint32_t requiredSampledImages = maxUiImageDescriptors_ + 1u; // font atlas + UI array
		const uint32_t supportedSampledImages = std::min({
			properties.properties.limits.maxPerStageDescriptorSampledImages,
			properties.properties.limits.maxDescriptorSetSampledImages,
			indexingProperties.maxPerStageDescriptorUpdateAfterBindSampledImages,
			indexingProperties.maxDescriptorSetUpdateAfterBindSampledImages,
		});
		if (requiredSampledImages > supportedSampledImages) {
			throw FlowUi::FlowUiException(FlowUi::makeError(FlowUi::ErrorCode::VulkanFeatureMissing, FlowUi::ErrorSite::RendererInitialize));
		}
		targetFormat_ = swapFormat;
		frameResourceCount_ = std::max<uint32_t>(1u, vulkanConfig.framesInFlight);
		frameResources_.assign(frameResourceCount_, UiFrameResources{});
		boundFontAtlasRevisionByFrame_.assign(frameResourceCount_, UINT32_MAX);

		const float configuredDpi = std::max(1.0f, uiConfig.dpi);
		pointsToPixelsScale_ = std::max(0.0f, uiConfig.fontScale) * (configuredDpi / 72.0f);
		if (pointsToPixelsScale_ <= 0.0f) {
			pointsToPixelsScale_ = configuredDpi / 72.0f;
		}

		for (uint32_t frameSlot = 0u; frameSlot < frameResourceCount_; ++frameSlot) {
			EnsureInstanceBufferCapacity(vk, *this, frameSlot, initialInstanceBytes_);
		}

		storage::RendererLayoutHandle acquiredLayout = storageSystem.acquireRendererLayout(kUiRendererLayoutKey);
		if (!acquiredLayout) {
			try {
				CreateLayoutObjects(vk.device, *this);
			} catch (...) {
				DestroyPipelineObjects(vk.device, pipelines_);
				DestroyDescriptorObjects(vk.device, descriptors_);
				throw;
			}
			const storage::NativeRendererLayout candidate{
				.globalsSetLayout = NativeHandleBits(descriptors_.set0),
				.texturesSetLayout = NativeHandleBits(descriptors_.set1),
				.pipelineLayout = NativeHandleBits(pipelines_.layout),
			};
			try {
				const auto published = storageSystem.publishRendererLayout(
					kUiRendererLayoutKey, candidate, storageSystem.intern("FlowUi UI renderer layout"));
				acquiredLayout = published.handle;
				if (published.ownershipTransferred) {
					descriptors_.set0 = VK_NULL_HANDLE;
					descriptors_.set1 = VK_NULL_HANDLE;
					pipelines_.layout = VK_NULL_HANDLE;
				} else {
					DestroyPipelineObjects(vk.device, pipelines_);
					DestroyDescriptorObjects(vk.device, descriptors_);
				}
			} catch (...) {
				DestroyPipelineObjects(vk.device, pipelines_);
				DestroyDescriptorObjects(vk.device, descriptors_);
				throw;
			}
		}
		layoutHandle_ = acquiredLayout;
		nativeLayout_ = storageSystem.nativeRendererLayout(layoutHandle_);
		if (nativeLayout_.globalsSetLayout == 0 || nativeLayout_.texturesSetLayout == 0 ||
			nativeLayout_.pipelineLayout == 0) {
			FlowUi::detail::terminateForFatalError(
				FlowUi::makeError(FlowUi::ErrorCode::RendererNativeResourceInvalid, FlowUi::ErrorSite::RendererInitialize));
		}
		descriptors_.set0 = NativeHandleFromBits<VkDescriptorSetLayout>(nativeLayout_.globalsSetLayout);
		descriptors_.set1 = NativeHandleFromBits<VkDescriptorSetLayout>(nativeLayout_.texturesSetLayout);
		pipelines_.layout = NativeHandleFromBits<VkPipelineLayout>(nativeLayout_.pipelineLayout);

		const storage::RendererPipelineKey pipelineKey{
			.layout = layoutHandle_,
			.nativeColorFormat = static_cast<uint32_t>(targetFormat_),
			.sampleCount = 1u,
			.pipelineStateRevision = kUiPipelineStateRevision,
			.shaderSetFingerprint = kUiShaderSetFingerprint,
		};
		storage::RendererPipelineBundleHandle acquiredPipeline =
			storageSystem.acquireRendererPipelineBundle(pipelineKey);
		if (!acquiredPipeline) {
			try {
				CreatePipelineObjects(vk.device, *this);
			} catch (...) {
				DestroyPipelines(vk.device, pipelines_);
				throw;
			}
			const storage::NativeRendererPipelineBundle candidate{
				.pipelineLayout = nativeLayout_.pipelineLayout,
				.pipelines = {
					NativeHandleBits(pipelines_.solid),
					NativeHandleBits(pipelines_.msdf),
					NativeHandleBits(pipelines_.textured),
				},
			};
			try {
				const auto published = storageSystem.publishRendererPipelineBundle(
					pipelineKey, candidate, storageSystem.intern("FlowUi UI renderer pipelines"));
				acquiredPipeline = published.handle;
				if (published.ownershipTransferred) {
					pipelines_.solid = VK_NULL_HANDLE;
					pipelines_.msdf = VK_NULL_HANDLE;
					pipelines_.textured = VK_NULL_HANDLE;
				} else {
					DestroyPipelines(vk.device, pipelines_);
				}
			} catch (...) {
				DestroyPipelines(vk.device, pipelines_);
				throw;
			}
		}
		pipelineBundleHandle_ = acquiredPipeline;
		nativePipelineBundle_ = storageSystem.nativeRendererPipelineBundle(pipelineBundleHandle_);
		pipelines_.solid = NativeHandleFromBits<VkPipeline>(nativePipelineBundle_.pipelines[0]);
		pipelines_.msdf = NativeHandleFromBits<VkPipeline>(nativePipelineBundle_.pipelines[1]);
		pipelines_.textured = NativeHandleFromBits<VkPipeline>(nativePipelineBundle_.pipelines[2]);

		try {
			CreateDescriptorObjects(vk.device, *this);
		} catch (...) {
			if (descriptors_.pool != VK_NULL_HANDLE) {
				vkDestroyDescriptorPool(vk.device, descriptors_.pool, nullptr);
				descriptors_.pool = VK_NULL_HANDLE;
			}
			throw;
		}
		std::vector<uint64_t> globalBits(frameResourceCount_);
		std::vector<uint64_t> textureBits(frameResourceCount_);
		for (uint32_t i = 0; i < frameResourceCount_; ++i) {
			globalBits[i] = NativeHandleBits(descriptors_.globalsSets[i]);
			textureBits[i] = NativeHandleBits(descriptors_.texturesSets[i]);
		}
		try {
			descriptorBundleHandle_ = storageSystem.adoptWindowDescriptorBundle(
				storage::WindowDescriptorBundleDesc{
					.window = windowId_,
					.layout = layoutHandle_,
					.framesInFlight = frameResourceCount_,
					.descriptorCapacity = maxUiImageDescriptors_,
					.debugName = storageSystem.intern("FlowUi window UI descriptors"),
				},
				storage::NativeWindowDescriptorBundle{
					.descriptorPool = NativeHandleBits(descriptors_.pool),
					.globalsSets = globalBits,
					.textureSets = textureBits,
				});
			descriptors_.pool = VK_NULL_HANDLE;
		} catch (...) {
			if (descriptors_.pool != VK_NULL_HANDLE) {
				vkDestroyDescriptorPool(vk.device, descriptors_.pool, nullptr);
				descriptors_.pool = VK_NULL_HANDLE;
			}
			throw;
		}
		nativeDescriptors_ = storageSystem.nativeWindowDescriptorBundle(descriptorBundleHandle_);
		descriptors_.pool = NativeHandleFromBits<VkDescriptorPool>(nativeDescriptors_.descriptorPool);
		descriptors_.globalsSets.assign(frameResourceCount_, VK_NULL_HANDLE);
		descriptors_.texturesSets.assign(frameResourceCount_, VK_NULL_HANDLE);
		for (uint32_t i = 0; i < frameResourceCount_; ++i) {
			descriptors_.globalsSets[i] = NativeHandleFromBits<VkDescriptorSet>(nativeDescriptors_.globalsSets[i]);
			descriptors_.texturesSets[i] = NativeHandleFromBits<VkDescriptorSet>(nativeDescriptors_.textureSets[i]);
		}
		InitializeDescriptorBindings(vk, *this);
	} catch (...) {
		destroy(vk, storageSystem);
		throw;
	}
}

void VulkanUiRenderer::destroy(
	VulkanContext& vk,
	storage::IStorageSystem& storageSystem,
	storage::SubmissionSerial lastUse)
{
	boundFontAtlasRevisionByFrame_.clear();
	textLayoutService_.clear();
	frameResourceCount_ = 1u;

	(void)vk;
	if (descriptorBundleHandle_) storageSystem.releaseWindowDescriptorBundle(descriptorBundleHandle_, lastUse);
	if (pipelineBundleHandle_) storageSystem.releaseRendererPipelineBundle(pipelineBundleHandle_, lastUse);
	if (layoutHandle_) storageSystem.releaseRendererLayout(layoutHandle_, lastUse);
	descriptorBundleHandle_ = {};
	pipelineBundleHandle_ = {};
	layoutHandle_ = {};
	nativeDescriptors_ = {};
	nativePipelineBundle_ = {};
	nativeLayout_ = {};
	pipelines_ = Pipelines{};
	descriptors_ = Descriptors{};

	for (UiFrameResources& frame : frameResources_) {
		if (frame.instanceBuffer) storageSystem.releaseBuffer(frame.instanceBuffer);
	}
	frameResources_.clear();

	targetFormat_ = VK_FORMAT_UNDEFINED;
	sharedByteResources_ = nullptr;
	storage_ = nullptr;
	windowId_ = FlowUi::InvalidWindowId;
	allowInstanceGrowth_ = true;
	pointsToPixelsScale_ = 96.0f / 72.0f;
}

void VulkanUiRenderer::onSwapchainFormatChanged(
	VulkanContext& vk,
	VkFormat newFormat,
	storage::SubmissionSerial lastUse)
{
	if (vk.device == VK_NULL_HANDLE || newFormat == VK_FORMAT_UNDEFINED) {
		return;
	}
	if (newFormat == targetFormat_ && pipelines_.solid != VK_NULL_HANDLE && pipelines_.msdf != VK_NULL_HANDLE &&
		pipelines_.textured != VK_NULL_HANDLE) {
		return;
	}

	if (!storage_ || !layoutHandle_) {
		throw FlowUi::FlowUiException(FlowUi::makeError(FlowUi::ErrorCode::ObjectNotInitialized, FlowUi::ErrorSite::RendererCreateWindowResources));
	}
	const storage::RendererPipelineKey key{
		.layout = layoutHandle_,
		.nativeColorFormat = static_cast<uint32_t>(newFormat),
		.sampleCount = 1u,
		.pipelineStateRevision = kUiPipelineStateRevision,
		.shaderSetFingerprint = kUiShaderSetFingerprint,
	};
	storage::RendererPipelineBundleHandle replacement = storage_->acquireRendererPipelineBundle(key);
	if (!replacement) {
		Pipelines candidate{};
		candidate.layout = pipelines_.layout;
		try {
			CreatePipelines(vk.device, candidate, newFormat);
		} catch (...) {
			DestroyPipelines(vk.device, candidate);
			throw;
		}
		try {
			const auto published = storage_->publishRendererPipelineBundle(
				key,
				storage::NativeRendererPipelineBundle{
					.pipelineLayout = nativeLayout_.pipelineLayout,
					.pipelines = {
						NativeHandleBits(candidate.solid),
						NativeHandleBits(candidate.msdf),
						NativeHandleBits(candidate.textured),
					},
				},
				storage_->intern("FlowUi UI renderer pipelines"));
			replacement = published.handle;
			if (!published.ownershipTransferred) DestroyPipelines(vk.device, candidate);
		} catch (...) {
			DestroyPipelines(vk.device, candidate);
			throw;
		}
	}
	const storage::NativeRendererPipelineBundle replacementNative =
		storage_->nativeRendererPipelineBundle(replacement);
	if (std::any_of(replacementNative.pipelines.begin(), replacementNative.pipelines.end(),
		[](uint64_t handle) { return handle == 0; })) {
		storage_->releaseRendererPipelineBundle(replacement, lastUse);
		FlowUi::detail::terminateForFatalError(
			FlowUi::makeError(FlowUi::ErrorCode::RendererNativeResourceInvalid, FlowUi::ErrorSite::RendererCreateWindowResources));
	}
	const storage::RendererPipelineBundleHandle previous = pipelineBundleHandle_;
	pipelineBundleHandle_ = replacement;
	nativePipelineBundle_ = replacementNative;
	targetFormat_ = newFormat;
	pipelines_.solid = NativeHandleFromBits<VkPipeline>(replacementNative.pipelines[0]);
	pipelines_.msdf = NativeHandleFromBits<VkPipeline>(replacementNative.pipelines[1]);
	pipelines_.textured = NativeHandleFromBits<VkPipeline>(replacementNative.pipelines[2]);
	if (previous) storage_->releaseRendererPipelineBundle(previous, lastUse);
}

void VulkanUiRenderer::applyTextureBindings(
	VkDevice device,
	uint32_t frameSlot,
	const storage::PreparedTextureBindings& prepared) {
	if (frameSlot >= descriptors_.texturesSets.size() || prepared.epoch == 0) {
		throw FlowUi::FlowUiException(FlowUi::makeError(FlowUi::ErrorCode::FramePhaseViolation, FlowUi::ErrorSite::RendererPublishDescriptors));
	}
	if (prepared.requiredDescriptorCapacity > maxUiImageDescriptors_ ||
		prepared.dirtyBindings.size() > maxUiImageDescriptors_) {
		throw FlowUi::FlowUiException(FlowUi::makeError(FlowUi::ErrorCode::RendererCapacityExceeded, FlowUi::ErrorSite::RendererPublishDescriptors));
	}
	std::array<VkDescriptorImageInfo, kDefaultMaxUiImageDescriptors> infos{};
	std::array<VkWriteDescriptorSet, kDefaultMaxUiImageDescriptors> writes{};
	for (size_t i = 0; i < prepared.dirtyBindings.size(); ++i) {
		const storage::DescriptorWriteRecord& binding = prepared.dirtyBindings[i];
		if (binding.descriptorIndex >= maxUiImageDescriptors_ ||
			binding.nativeImageView == 0 || binding.nativeSampler == 0) {
			FlowUi::detail::terminateForFatalError(
				FlowUi::makeError(FlowUi::ErrorCode::RendererNativeResourceInvalid, FlowUi::ErrorSite::RendererPublishDescriptors));
		}
		infos[i] = VkDescriptorImageInfo{
			.sampler = NativeHandleFromBits<VkSampler>(binding.nativeSampler),
			.imageView = NativeHandleFromBits<VkImageView>(binding.nativeImageView),
			.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
		};
		writes[i].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
		writes[i].dstSet = descriptors_.texturesSets[frameSlot];
		writes[i].dstBinding = 1;
		writes[i].dstArrayElement = binding.descriptorIndex;
		writes[i].descriptorCount = 1;
		writes[i].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
		writes[i].pImageInfo = &infos[i];
	}
	if (!prepared.dirtyBindings.empty()) {
		vkUpdateDescriptorSets(
			device, static_cast<uint32_t>(prepared.dirtyBindings.size()), writes.data(), 0, nullptr);
	}
}

PreparedUiFrame VulkanUiRenderer::prepareFrame(
	VulkanContext& vk,
	storage::IStorageSystem& storageSystem,
	const storage::FrameToken& frame,
	const storage::PreparedTextureBindings& textureBindings,
	const Clay_RenderCommandArray& renderCommands,
	const FlowUi::detail::InputFieldFrameOverrides& inputFieldOverrides,
	const FlowUi::detail::manager_storage::FontFrameView& fontView,
	VkExtent2D extent,
	float uiToFramebufferScaleX,
	float uiToFramebufferScaleY
#if FLOW_UI_DEV_MODE
	,
	const FlowUi::devSystems::tooling::DevOverlayCommandBuffer* devOverlay,
	FlowUi::devSystems::DevTimingRecorder* timingRecorder
#endif
	)
{
#if FLOW_UI_DEV_MODE
	FLOWUI_DEV_TIMING_ZONE_IF(
		timingRecorder, FlowUi::devSystems::TimingCategory::RendererCpu,
		FlowUi::devSystems::TimingZoneRole::Work, "flowui.renderer.prepare");
#endif
	if (!frame || frame.window != windowId_ || &storageSystem != storage_) {
		FlowUi::detail::terminateForFatalError(
			FlowUi::makeError(FlowUi::ErrorCode::RendererGenerationStale, FlowUi::ErrorSite::RendererConvertCommands));
	}
	if (textureBindings.epoch != frame.epoch) {
		FlowUi::detail::terminateForFatalError(
			FlowUi::makeError(FlowUi::ErrorCode::RendererGenerationStale, FlowUi::ErrorSite::RendererConvertCommands));
	}
	if (frameResourceCount_ == 0u || frameResources_.empty() ||
		descriptors_.globalsSets.empty() || descriptors_.texturesSets.empty()) return {};

	const uint32_t frameSlot = frame.frameSlot;
	if (frameSlot >= frameResources_.size() ||
		frameSlot >= descriptors_.globalsSets.size() ||
		frameSlot >= descriptors_.texturesSets.size()) {
		FlowUi::detail::terminateForFatalError(
			FlowUi::makeError(FlowUi::ErrorCode::RendererGenerationStale, FlowUi::ErrorSite::RendererConvertCommands));
	}
	if (!layoutHandle_ || !pipelineBundleHandle_ || !descriptorBundleHandle_) {
		FlowUi::detail::terminateForFatalError(
			FlowUi::makeError(FlowUi::ErrorCode::RendererGenerationStale, FlowUi::ErrorSite::RendererConvertCommands));
	}
	const std::array rendererUses{
		storage::useOf(layoutHandle_),
		storage::useOf(pipelineBundleHandle_),
		storage::useOf(descriptorBundleHandle_),
	};
	storageSystem.trackUses(frame, rendererUses);

	const uint32_t latestFontAtlasRevision = fontView.atlas.bindingRevision;
	uint32_t boundRevision = UINT32_MAX;
	if (frameSlot < boundFontAtlasRevisionByFrame_.size()) {
		boundRevision = boundFontAtlasRevisionByFrame_[frameSlot];
	}
	if (latestFontAtlasRevision != boundRevision) {
		UpdateFontDescriptorForFrame(*this, vk.device, frameSlot, &fontView);
		if (frameSlot < boundFontAtlasRevisionByFrame_.size()) {
			boundFontAtlasRevisionByFrame_[frameSlot] = latestFontAtlasRevision;
		}
	}

	UiBuildUpperBound upperBound{};
	{
#if FLOW_UI_DEV_MODE
		FLOWUI_DEV_TIMING_ZONE_BALANCED_IF(
			timingRecorder, FlowUi::devSystems::TimingCategory::RendererCpu,
			FlowUi::devSystems::TimingZoneRole::Work, "flowui.renderer.build_upper_bound");
#endif
		upperBound = ComputeBuildUpperBound(renderCommands, inputFieldOverrides);
	}
#if FLOW_UI_DEV_MODE
	const UiBuildUpperBound appUpperBound = upperBound;
	const bool hasDevOverlay = devOverlay && !devOverlay->instances.empty() && !devOverlay->runs.empty();
	if (hasDevOverlay) {
		upperBound.instances = CheckedSizeAdd(
			upperBound.instances, devOverlay->instances.size());
		upperBound.runs = CheckedSizeAdd(
			upperBound.runs, devOverlay->runs.size());
	}
	if (upperBound.instances > std::numeric_limits<uint32_t>::max() ||
		upperBound.runs > std::numeric_limits<uint32_t>::max()) {
		throw FlowUi::FlowUiException(FlowUi::makeError(
			FlowUi::ErrorCode::RendererCapacityExceeded,
			FlowUi::ErrorSite::RendererConvertCommands));
	}
#endif
	if (upperBound.instances == 0 || upperBound.runs == 0) {
		return PreparedUiFrame{.epoch = frame.epoch};
	}
	if (upperBound.instances > std::numeric_limits<uint64_t>::max() / sizeof(UiInstance)) {
		throw FlowUi::FlowUiException(FlowUi::makeError(FlowUi::ErrorCode::RendererCapacityExceeded, FlowUi::ErrorSite::RendererConvertCommands));
	}
	const uint64_t requiredBytes = static_cast<uint64_t>(upperBound.instances) * sizeof(UiInstance);
	{
#if FLOW_UI_DEV_MODE
		FLOWUI_DEV_TIMING_ZONE_BALANCED_IF(
			timingRecorder, FlowUi::devSystems::TimingCategory::RendererCpu,
			FlowUi::devSystems::TimingZoneRole::Work, "flowui.renderer.ensure_instance_buffer");
#endif
		EnsureInstanceBufferCapacity(vk, *this, frameSlot, requiredBytes);
	}

	storage::ArenaView arena = storageSystem.frameArena(frame, storage::MemoryClass::FrameTransient);
	std::span<UiRun> runs = arena.allocateArray<UiRun>(upperBound.runs);
	std::span<RectF> scissorStack = arena.allocateArray<RectF>(upperBound.scissorDepth);
	const storage::BufferWriteView write = storageSystem.beginBufferWrite(
		frame,
		frameResources_[frameSlot].instanceBuffer,
		0,
		requiredBytes,
		storage::BufferWriteMode::DirectMapped);
	std::span<UiInstance> instances{
		reinterpret_cast<UiInstance*>(write.data),
		upperBound.instances,
	};
	const float clampedScaleX = std::max(uiToFramebufferScaleX, 1.0e-6f);
	const float clampedScaleY = std::max(uiToFramebufferScaleY, 1.0e-6f);
	UiBuildResult built{};
	{
#if FLOW_UI_DEV_MODE
		FLOWUI_DEV_TIMING_ZONE_BALANCED_IF(
			timingRecorder, FlowUi::devSystems::TimingCategory::RendererCpu,
			FlowUi::devSystems::TimingZoneRole::Work, "flowui.renderer.build_instances_runs");
#endif
#if FLOW_UI_DEV_MODE
		if (appUpperBound.instances != 0u && appUpperBound.runs != 0u) {
			built = BuildInstancesAndRunsFromClay(
				renderCommands,
				inputFieldOverrides,
				extent,
				&fontView,
				textLayoutService_,
				pointsToPixelsScale_,
				clampedScaleX,
				clampedScaleY,
				textureBindings.bindingsByTextureIndex,
				instances.first(appUpperBound.instances),
				runs.first(appUpperBound.runs),
				scissorStack);
		}
		if (hasDevOverlay) {
			const uint32_t overlayBase = built.instanceCount;
			for (const UiInstance& instance : devOverlay->instances) {
				instances[built.instanceCount++] = instance;
			}
			for (const UiRun& sourceRun : devOverlay->runs) {
				const uint64_t sourceEnd = static_cast<uint64_t>(sourceRun.firstInstance) + sourceRun.instanceCount;
				if (sourceEnd > devOverlay->instances.size()) {
					throw FlowUi::FlowUiException(FlowUi::makeError(
						FlowUi::ErrorCode::RendererCapacityExceeded,
						FlowUi::ErrorSite::RendererConvertCommands));
				}
				UiRun run = sourceRun;
				run.firstInstance += overlayBase;
				runs[built.runCount++] = run;
			}
		}
#else
		built = BuildInstancesAndRunsFromClay(
			renderCommands,
			inputFieldOverrides,
			extent,
			&fontView,
			textLayoutService_,
			pointsToPixelsScale_,
			clampedScaleX,
			clampedScaleY,
			textureBindings.bindingsByTextureIndex,
			instances,
			runs,
			scissorStack);
#endif
	}
	{
#if FLOW_UI_DEV_MODE
		FLOWUI_DEV_TIMING_ZONE_BALANCED_IF(
			timingRecorder, FlowUi::devSystems::TimingCategory::RendererCpu,
			FlowUi::devSystems::TimingZoneRole::Work, "flowui.renderer.commit_instance_write");
#endif
		storageSystem.commitBufferWrite(
			frame,
			write,
			static_cast<uint64_t>(built.instanceCount) * sizeof(UiInstance));
	}

	if (built.instanceCount > 0 && built.runCount > 0) {
		if (!sharedByteResources_) {
			FlowUi::detail::terminateForFatalError(
				FlowUi::makeError(FlowUi::ErrorCode::RendererNativeResourceInvalid, FlowUi::ErrorSite::RendererConvertCommands));
		}
		const std::array sharedUses{
			storage::ResourceUse{storage::ResourceKind::GpuBuffer, sharedByteResources_->quadBuffer.packed()},
			storage::ResourceUse{storage::ResourceKind::GpuImage, sharedByteResources_->placeholderFontImage.packed()},
			storage::ResourceUse{storage::ResourceKind::ImageView, sharedByteResources_->placeholderFontView.packed()},
			storage::ResourceUse{storage::ResourceKind::GpuImage, sharedByteResources_->placeholderUiImage.packed()},
			storage::ResourceUse{storage::ResourceKind::ImageView, sharedByteResources_->placeholderUiView.packed()},
			storage::ResourceUse{storage::ResourceKind::Sampler, sharedByteResources_->linearSampler.packed()},
		};
		storageSystem.trackUses(frame, sharedUses);
	}
	return PreparedUiFrame{
		.runs = runs.first(built.runCount),
		.instanceCount = built.instanceCount,
		.epoch = frame.epoch,
		.originatingFrameSlot = frameSlot,
	};
}

void VulkanUiRenderer::recordPreparedFrame(
	VulkanContext& vk,
	VkCommandBuffer cmd,
	VkExtent2D extent,
	VkImageView targetView,
	uint32_t frameIndex,
	const PreparedUiFrame& prepared
#if FLOW_UI_DEV_MODE
	,
	FlowUi::devSystems::DevTimingRecorder* timingRecorder,
	FlowUi::devSystems::GpuTimingCommandContext* gpuTiming
#endif
	)
{
	(void)vk;
#if FLOW_UI_DEV_MODE
	FLOWUI_DEV_TIMING_ZONE_IF(
		timingRecorder, FlowUi::devSystems::TimingCategory::RendererCpu,
		FlowUi::devSystems::TimingZoneRole::Work, "flowui.renderer.record_ui");
#endif
	if (cmd == VK_NULL_HANDLE || targetView == VK_NULL_HANDLE || prepared.instanceCount == 0 ||
		prepared.runs.empty() || prepared.epoch == 0) return;
	if (pipelines_.layout == VK_NULL_HANDLE || pipelines_.solid == VK_NULL_HANDLE || !sharedByteResources_) return;
	if (frameResourceCount_ == 0u || frameResources_.empty() ||
		descriptors_.globalsSets.empty() || descriptors_.texturesSets.empty()) return;

	const uint32_t frameSlot = prepared.originatingFrameSlot;
	if (frameSlot >= frameResources_.size() || frameSlot >= descriptors_.globalsSets.size() ||
		frameSlot >= descriptors_.texturesSets.size()) return;
	if (frameResources_[frameSlot].nativeBuffer.nativeBuffer == 0 ||
		sharedByteResources_->nativeQuadBuffer == VK_NULL_HANDLE) return;

#if FLOW_UI_DEV_MODE
	FlowUi::devSystems::GpuCommandTimingZone gpuZone(
		gpuTiming, cmd, FlowUi::devSystems::gpu_timing_zones::kUiPass);
#endif

	VkRenderingAttachmentInfo colorAttachment{};
	colorAttachment.sType = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO;
	colorAttachment.imageView = targetView;
	colorAttachment.imageLayout = VK_IMAGE_LAYOUT_ATTACHMENT_OPTIMAL;
	colorAttachment.loadOp = VK_ATTACHMENT_LOAD_OP_LOAD;
	colorAttachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE;

	VkRenderingInfo renderingInfo{};
	renderingInfo.sType = VK_STRUCTURE_TYPE_RENDERING_INFO;
	renderingInfo.renderArea = { { 0, 0 }, extent };
	renderingInfo.layerCount = 1;
	renderingInfo.colorAttachmentCount = 1;
	renderingInfo.pColorAttachments = &colorAttachment;

	vkCmdBeginRendering(cmd, &renderingInfo);

	VkViewport viewport{};
	viewport.x = 0.0f;
	viewport.y = 0.0f;
	viewport.width = static_cast<float>(extent.width);
	viewport.height = static_cast<float>(extent.height);
	viewport.minDepth = 0.0f;
	viewport.maxDepth = 1.0f;
	vkCmdSetViewport(cmd, 0, 1, &viewport);

	VkRect2D fullScissor{};
	fullScissor.offset = { 0, 0 };
	fullScissor.extent = extent;
	vkCmdSetScissor(cmd, 0, 1, &fullScissor);

	const VkBuffer vertexBuffer = sharedByteResources_->nativeQuadBuffer;
	const VkDeviceSize vertexOffset = 0;
	vkCmdBindVertexBuffers(cmd, 0, 1, &vertexBuffer, &vertexOffset);

	const VkDescriptorSet sets[2] = {
		descriptors_.globalsSets[frameSlot],
		descriptors_.texturesSets[frameSlot],
	};
	vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, pipelines_.layout, 0, 2, sets, 0, nullptr);

	for (const UiRun& run : prepared.runs) {
		FlushRun(cmd, *this, extent, run);
	}

	vkCmdEndRendering(cmd);
}

#if FLOW_UI_DEV_MODE
VkFormat VulkanUiRenderer::devReplayTargetFormat() const noexcept {
	return targetFormat_;
}

VkDescriptorSetLayout VulkanUiRenderer::devReplayGlobalsLayout() const noexcept {
	return descriptors_.set0;
}

void VulkanUiRenderer::recordExternalReplay(
	VkCommandBuffer commandBuffer,
	VkExtent2D targetExtent,
	VkDescriptorSet globalsSet,
	uint32_t sourceTextureFrameSlot,
	std::span<const UiRun> replayRuns) const {
	if (commandBuffer == VK_NULL_HANDLE || globalsSet == VK_NULL_HANDLE ||
		targetExtent.width == 0u || targetExtent.height == 0u || replayRuns.empty() ||
		!sharedByteResources_ || pipelines_.layout == VK_NULL_HANDLE) {
		return;
	}
	if (sourceTextureFrameSlot >= descriptors_.texturesSets.size()) {
		return;
	}

	VkViewport viewport{};
	viewport.width = static_cast<float>(targetExtent.width);
	viewport.height = static_cast<float>(targetExtent.height);
	viewport.minDepth = 0.0f;
	viewport.maxDepth = 1.0f;
	vkCmdSetViewport(commandBuffer, 0u, 1u, &viewport);
	const VkBuffer vertexBuffer = sharedByteResources_->nativeQuadBuffer;
	const VkDeviceSize vertexOffset = 0u;
	vkCmdBindVertexBuffers(commandBuffer, 0u, 1u, &vertexBuffer, &vertexOffset);
	const VkDescriptorSet sets[2] = {
		globalsSet,
		descriptors_.texturesSets[sourceTextureFrameSlot],
	};
	vkCmdBindDescriptorSets(
		commandBuffer,
		VK_PIPELINE_BIND_POINT_GRAPHICS,
		pipelines_.layout,
		0u,
		2u,
		sets,
		0u,
		nullptr);
	for (const UiRun& run : replayRuns) {
		FlushRun(commandBuffer, *this, targetExtent, run);
	}
}
#endif
