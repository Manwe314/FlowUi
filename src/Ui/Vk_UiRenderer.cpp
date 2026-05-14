#include "Ui/Vk_UiRenderer.hpp"
#include "managers/FontManager.hpp"
#include "managers/InputFieldManager.hpp"
#include "internal/TextLayoutEngine.hpp"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <limits>
#include <stdexcept>
#include <string>
#include <vector>

#include "internal/Vma.hpp"

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

constexpr VkDeviceSize kInitialInstanceBufferBytes = 1024u * 1024u;
constexpr uint32_t kDefaultMaxUiImageDescriptors = 256;

constexpr const char* kUiSolidVertexShaderFile = "flowui_ui_solid.vert.spv";
constexpr const char* kUiMsdfVertexShaderFile = "flowui_ui_msdf.vert.spv";
constexpr const char* kUiTexturedVertexShaderFile = "flowui_ui_textured.vert.spv";
constexpr const char* kUiSolidFragmentShaderFile = "flowui_ui_solid.frag.spv";
constexpr const char* kUiMsdfFragmentShaderFile = "flowui_ui_msdf.frag.spv";
constexpr const char* kUiTexturedFragmentShaderFile = "flowui_ui_textured.frag.spv";

static void vkCheck(VkResult result, const char* message) {
	if (result != VK_SUCCESS) {
		throw std::runtime_error(message);
	}
}

static std::vector<char> readFile(const std::string& path) {
	std::ifstream file(path, std::ios::ate | std::ios::binary);
	if (!file) {
		throw std::runtime_error(std::string("Failed to open shader file: ") + path);
	}

	const std::streamsize size = file.tellg();
	if (size <= 0) {
		throw std::runtime_error(std::string("Shader file is empty: ") + path);
	}

	std::vector<char> buffer(static_cast<size_t>(size));
	file.seekg(0, std::ios::beg);
	file.read(buffer.data(), size);
	if (!file) {
		throw std::runtime_error(std::string("Failed to read shader file: ") + path);
	}
	if ((buffer.size() % 4) != 0) {
		throw std::runtime_error(std::string("Shader file size is not SPIR-V aligned: ") + path);
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

#if defined(FLOWUI_SHADER_OUTPUT_DIR)
	throw std::runtime_error(
		"Failed to open shader file: " + std::string(fileName) +
		" (looked in '" + configuredPath + "' and '" + relativePath + "')");
#else
	throw std::runtime_error(
		"Failed to open shader file: " + std::string(fileName) +
		" (looked in '" + relativePath + "')");
#endif
}

static VkShaderModule createShaderModule(VkDevice device, const std::vector<char>& code) {
	VkShaderModuleCreateInfo createInfo{};
	createInfo.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
	createInfo.codeSize = code.size();
	createInfo.pCode = reinterpret_cast<const uint32_t*>(code.data());

	VkShaderModule module = VK_NULL_HANDLE;
	vkCheck(vkCreateShaderModule(device, &createInfo, nullptr, &module), "Failed to create shader module.");
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

static bool LayoutMsdfTextToGlyphs(
	const Clay_TextRenderData& text,
	const Clay_BoundingBox& bounds,
	const FontManager* fontManager,
	float pointsToPixelsScale,
	std::vector<GlyphQuad>& outGlyphs,
	uint32_t& outAtlasLayer,
	float& outDistanceRangePx)
{
	outGlyphs.clear();
	outAtlasLayer = 0u;
	outDistanceRangePx = 2.0f;

	const FlowUi::Font::FontFaceData* fontFace = FlowUi::detail::ResolveFontFace(fontManager, text.fontId);
	if (!fontFace) {
		return false;
	}

	const FlowUi::detail::TextLayoutResult layoutResult = FlowUi::detail::LayoutTextLine(
		FlowUi::detail::TextLayoutRequest{
			.text = text.stringContents,
			.fontFace = fontFace,
			.pointsToPixelsScale = pointsToPixelsScale,
			.fontSize = text.fontSize,
			.letterSpacing = text.letterSpacing,
			.lineOriginX = bounds.x,
			.lineOriginY = bounds.y,
			.emitGlyphQuads = true,
		},
		[&outGlyphs](const FlowUi::detail::TextLayoutGlyphQuad& glyph) {
			outGlyphs.push_back(GlyphQuad{
				glyph.x,
				glyph.y,
				glyph.w,
				glyph.h,
				glyph.u0,
				glyph.v0,
				glyph.u1,
				glyph.v1,
				glyph.byteStartOffset,
				glyph.byteEndOffset,
			});
		});

	if (!layoutResult.success) {
		return false;
	}

	outAtlasLayer = layoutResult.atlasLayer;
	outDistanceRangePx = layoutResult.distanceRangePx;
	return true;
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
	std::vector<UiInstance>& instances) {
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
	instances.push_back(inst);
}

static void EmitSolidRectOverride(
	const FlowUi::detail::InputFieldRectOverride& rectOverride,
	float uiToFramebufferScaleX,
	float uiToFramebufferScaleY,
	std::vector<UiInstance>& instances) {
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
	std::vector<UiInstance>& instances) {
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
	instances.push_back(inst);
}

static void EmitTextMsdf(
	const Clay_RenderCommand& command,
	const FontManager* fontManager,
	float pointsToPixelsScale,
	float uiToFramebufferScaleX,
	float uiToFramebufferScaleY,
	const FlowUi::detail::InputFieldTextColorOverride* textColorOverride,
	std::vector<UiInstance>& instances)
{
	const Clay_BoundingBox& bounds = command.boundingBox;
	const Clay_TextRenderData& textData = command.renderData.text;

	std::vector<GlyphQuad> glyphs;
	glyphs.reserve(static_cast<size_t>(std::max(1, textData.stringContents.length)));
	uint32_t atlasLayer = 0u;
	float distanceRangePx = 2.0f;
	const bool hasLayout = LayoutMsdfTextToGlyphs(
		textData,
		bounds,
		fontManager,
		pointsToPixelsScale,
		glyphs,
		atlasLayer,
		distanceRangePx);
	if (!hasLayout) {
		glyphs.push_back(GlyphQuad{
			bounds.x,
			bounds.y,
			bounds.width,
			bounds.height,
			0.0f,
			0.0f,
			1.0f,
			1.0f,
			0,
			std::max(0, textData.stringContents.length),
		});
	}

	const uint32_t defaultColor = PackRGBA8(textData.textColor);
	const bool hasTextColorOverride = textColorOverride && !textColorOverride->ranges.empty();
	const uint32_t overrideColor = hasTextColorOverride
		? PackRGBA8(textColorOverride->color)
		: 0u;

	for (const GlyphQuad& glyph : glyphs) {
		uint32_t glyphColor = defaultColor;
		if (hasTextColorOverride) {
			const size_t glyphStart = static_cast<size_t>(std::max(0, glyph.byteStartOffset));
			const size_t glyphEnd = static_cast<size_t>(std::max(glyph.byteStartOffset, glyph.byteEndOffset));
			for (const FlowUi::detail::InputFieldTextColorRangeOverride& range : textColorOverride->ranges) {
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
		instances.push_back(inst);
	}
}

static void EmitTexturedImage(
	const Clay_RenderCommand& command,
	float uiToFramebufferScaleX,
	float uiToFramebufferScaleY,
	std::vector<UiInstance>& instances)
{
	const RectF bounds = ScaleBoundingBox(command.boundingBox, uiToFramebufferScaleX, uiToFramebufferScaleY);
	const float radiusScale = UniformScale(uiToFramebufferScaleX, uiToFramebufferScaleY);
	const FlowUi::TextureRef& textureRef = ResolveTextureRef(command);
	const TexturedImagePlacement placement = ResolveTexturedImagePlacement(bounds, textureRef);
	(void)textureRef.samplingMode; // Sampling mode is intentionally a no-op in textured pipeline V1.
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
	inst.texIndex = textureRef.id;
	inst.solidMode = textureRef.tintEnabled ? kTexturedFlagTintEnabled : 0u;
	inst.r0 = command.renderData.image.cornerRadius.topLeft * radiusScale;
	inst.r1 = command.renderData.image.cornerRadius.topRight * radiusScale;
	inst.r2 = command.renderData.image.cornerRadius.bottomRight * radiusScale;
	inst.r3 = command.renderData.image.cornerRadius.bottomLeft * radiusScale;
	instances.push_back(inst);
}

static void BuildInstancesAndRunsFromClay(
	const Clay_RenderCommandArray& commands,
	const FlowUi::detail::InputFieldFrameOverrides& inputFieldOverrides,
	VkExtent2D extent,
	const FontManager* fontManager,
	float pointsToPixelsScale,
	float uiToFramebufferScaleX,
	float uiToFramebufferScaleY,
	std::vector<UiInstance>& outInstances,
	std::vector<UiRun>& outRuns)
{
	outInstances.clear();
	outRuns.clear();

	const RectF fullScissor{ 0.0f, 0.0f, static_cast<float>(extent.width), static_cast<float>(extent.height) };
	std::vector<RectF> scissorStack;
	scissorStack.push_back(fullScissor);
	RectF currentScissor = fullScissor;

	bool runBarrier = false;

	auto closeActiveRun = [&]() {
		if (outRuns.empty()) {
			return;
		}
		UiRun& run = outRuns.back();
		run.instanceCount = static_cast<uint32_t>(outInstances.size()) - run.firstInstance;
		if (run.instanceCount == 0) {
			outRuns.pop_back();
		}
	};

	auto beginRunIfNeeded = [&](UiType type, const RectF& scissor) {
		if (outRuns.empty() || runBarrier) {
			closeActiveRun();
			outRuns.emplace_back(UiRun{ type, scissor, static_cast<uint32_t>(outInstances.size()), 0u });
			runBarrier = false;
			return;
		}

		const UiRun& current = outRuns.back();
		if (current.type != type || !RectEqual(current.scissor, scissor)) {
			closeActiveRun();
			outRuns.emplace_back(UiRun{ type, scissor, static_cast<uint32_t>(outInstances.size()), 0u });
		}
	};

	const std::vector<FlowUi::detail::InputFieldRectOverride>& inputRectOverrides = inputFieldOverrides.rects;
	size_t inputRectOverrideCursor = 0u;
	const std::vector<FlowUi::detail::InputFieldTextColorOverride>& inputTextColorOverrides = inputFieldOverrides.textColorOverrides;
	size_t inputTextColorOverrideCursor = 0u;
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
		const Clay_RenderCommand& command = commands.internalArray[i];

		if (command.commandType == CLAY_RENDER_COMMAND_TYPE_NONE) {
			continue;
		}
		if (command.commandType == CLAY_RENDER_COMMAND_TYPE_SCISSOR_START) {
			const RectF clip = ScaleBoundingBox(command.boundingBox, uiToFramebufferScaleX, uiToFramebufferScaleY);
			scissorStack.push_back(Intersect(scissorStack.back(), clip));
			currentScissor = scissorStack.back();
			runBarrier = true;
			continue;
		}
		if (command.commandType == CLAY_RENDER_COMMAND_TYPE_SCISSOR_END) {
			if (scissorStack.size() > 1) {
				scissorStack.pop_back();
			}
			currentScissor = scissorStack.back();
			runBarrier = true;
			continue;
		}
		if (command.commandType == CLAY_RENDER_COMMAND_TYPE_CUSTOM) {
			// Placeholder: custom command execution hook should run here in strict command order.
			closeActiveRun();
			runBarrier = true;
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
					fontManager,
					pointsToPixelsScale,
					uiToFramebufferScaleX,
					uiToFramebufferScaleY,
					inputTextColorOverride,
					outInstances);
				break;
			case CLAY_RENDER_COMMAND_TYPE_IMAGE:
				EmitTexturedImage(command, uiToFramebufferScaleX, uiToFramebufferScaleY, outInstances);
				break;
			default:
				break;
		}
	}

	emitInputOverridesBefore(commands.length);
	closeActiveRun();
}

static void DestroyBuffer(VulkanContext& vk, VulkanUiRenderer::AllocatedBuffer& buffer) {
	if (buffer.buffer != VK_NULL_HANDLE) {
		vmaDestroyBuffer(vk.allocator, buffer.buffer, buffer.allocation);
	}
	buffer.buffer = VK_NULL_HANDLE;
	buffer.allocation = nullptr;
	buffer.mapped = nullptr;
	buffer.size = 0;
}

static void CreateMappedBuffer(
	VulkanContext& vk,
	VkDeviceSize size,
	VkBufferUsageFlags usage,
	VulkanUiRenderer::AllocatedBuffer& outBuffer) {
	VkBufferCreateInfo bufferInfo{};
	bufferInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
	bufferInfo.size = size;
	bufferInfo.usage = usage;
	bufferInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

	VmaAllocationCreateInfo allocationCreateInfo{};
	allocationCreateInfo.usage = VMA_MEMORY_USAGE_AUTO_PREFER_HOST;
	allocationCreateInfo.flags =
		VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT |
		VMA_ALLOCATION_CREATE_MAPPED_BIT;

	VmaAllocationInfo allocationInfo{};
	vkCheck(
		vmaCreateBuffer(
			vk.allocator,
			&bufferInfo,
			&allocationCreateInfo,
			&outBuffer.buffer,
			&outBuffer.allocation,
			&allocationInfo),
		"Failed to create mapped UI buffer.");

	outBuffer.size = size;
	outBuffer.mapped = allocationInfo.pMappedData;
	if (!outBuffer.mapped) {
		vkCheck(vmaMapMemory(vk.allocator, outBuffer.allocation, &outBuffer.mapped), "Failed to map UI buffer.");
	}
}

static void UploadBytesToMappedBuffer(
	VulkanContext& vk,
	const VulkanUiRenderer::AllocatedBuffer& buffer,
	const void* data,
	VkDeviceSize size) {
	if (size == 0) {
		return;
	}
	if (buffer.buffer == VK_NULL_HANDLE || buffer.mapped == nullptr || size > buffer.size) {
		throw std::runtime_error("UI upload buffer is invalid or too small.");
	}
	std::memcpy(buffer.mapped, data, static_cast<size_t>(size));
	vkCheck(vmaFlushAllocation(vk.allocator, buffer.allocation, 0, size), "Failed to flush UI upload buffer.");
}

static void DestroyImage(VulkanContext& vk, VulkanUiRenderer::AllocatedImage& image) {
	if (image.view != VK_NULL_HANDLE) {
		vkDestroyImageView(vk.device, image.view, nullptr);
		image.view = VK_NULL_HANDLE;
	}
	if (image.image != VK_NULL_HANDLE) {
		vmaDestroyImage(vk.allocator, image.image, image.allocation);
	}
	image.image = VK_NULL_HANDLE;
	image.allocation = nullptr;
}

static void TransitionImageToShaderRead(VulkanContext& vk, VkImage image, uint32_t layers) {
	VkCommandPool commandPool = VK_NULL_HANDLE;
	VkCommandPoolCreateInfo poolCreateInfo{};
	poolCreateInfo.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
	poolCreateInfo.queueFamilyIndex = vk.graphicsQFamily;
	poolCreateInfo.flags = VK_COMMAND_POOL_CREATE_TRANSIENT_BIT;
	vkCheck(vkCreateCommandPool(vk.device, &poolCreateInfo, nullptr, &commandPool), "Failed to create temp command pool.");

	VkCommandBuffer commandBuffer = VK_NULL_HANDLE;
	VkCommandBufferAllocateInfo allocInfo{};
	allocInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
	allocInfo.commandPool = commandPool;
	allocInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
	allocInfo.commandBufferCount = 1;
	vkCheck(vkAllocateCommandBuffers(vk.device, &allocInfo, &commandBuffer), "Failed to allocate temp command buffer.");

	VkCommandBufferBeginInfo beginInfo{};
	beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
	beginInfo.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
	vkCheck(vkBeginCommandBuffer(commandBuffer, &beginInfo), "Failed to begin temp command buffer.");

	VkImageMemoryBarrier barrier{};
	barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
	barrier.srcAccessMask = 0;
	barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
	barrier.oldLayout = VK_IMAGE_LAYOUT_UNDEFINED;
	barrier.newLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
	barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
	barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
	barrier.image = image;
	barrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
	barrier.subresourceRange.baseMipLevel = 0;
	barrier.subresourceRange.levelCount = 1;
	barrier.subresourceRange.baseArrayLayer = 0;
	barrier.subresourceRange.layerCount = layers;

	vkCmdPipelineBarrier(
		commandBuffer,
		VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,
		VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,
		0,
		0,
		nullptr,
		0,
		nullptr,
		1,
		&barrier);

	vkCheck(vkEndCommandBuffer(commandBuffer), "Failed to end temp command buffer.");

	VkSubmitInfo submitInfo{};
	submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
	submitInfo.commandBufferCount = 1;
	submitInfo.pCommandBuffers = &commandBuffer;
	vkCheck(vkQueueSubmit(vk.graphicsQ, 1, &submitInfo, VK_NULL_HANDLE), "Failed to submit temp command buffer.");
	vkCheck(vkQueueWaitIdle(vk.graphicsQ), "Failed to wait for temp queue idle.");

	vkDestroyCommandPool(vk.device, commandPool, nullptr);
}

static void CreatePlaceholderImage(
	VulkanContext& vk,
	uint32_t arrayLayers,
	VkImageViewType viewType,
	VulkanUiRenderer::AllocatedImage& outImage) {
	VkImageCreateInfo imageInfo{};
	imageInfo.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
	imageInfo.imageType = VK_IMAGE_TYPE_2D;
	imageInfo.format = VK_FORMAT_R8G8B8A8_UNORM;
	imageInfo.extent = { 1, 1, 1 };
	imageInfo.mipLevels = 1;
	imageInfo.arrayLayers = arrayLayers;
	imageInfo.samples = VK_SAMPLE_COUNT_1_BIT;
	imageInfo.tiling = VK_IMAGE_TILING_OPTIMAL;
	imageInfo.usage = VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT;
	imageInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
	imageInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;

	VmaAllocationCreateInfo allocationCreateInfo{};
	allocationCreateInfo.usage = VMA_MEMORY_USAGE_AUTO_PREFER_DEVICE;

	vkCheck(
		vmaCreateImage(
			vk.allocator,
			&imageInfo,
			&allocationCreateInfo,
			&outImage.image,
			&outImage.allocation,
			nullptr),
		"Failed to create placeholder UI image.");

	VkImageViewCreateInfo viewInfo{};
	viewInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
	viewInfo.image = outImage.image;
	viewInfo.viewType = viewType;
	viewInfo.format = imageInfo.format;
	viewInfo.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
	viewInfo.subresourceRange.baseMipLevel = 0;
	viewInfo.subresourceRange.levelCount = 1;
	viewInfo.subresourceRange.baseArrayLayer = 0;
	viewInfo.subresourceRange.layerCount = arrayLayers;
	vkCheck(vkCreateImageView(vk.device, &viewInfo, nullptr, &outImage.view), "Failed to create placeholder UI image view.");

	TransitionImageToShaderRead(vk, outImage.image, arrayLayers);
}

static void CreateLinearSampler(VulkanContext& vk, VkSampler& sampler) {
	VkSamplerCreateInfo samplerInfo{};
	samplerInfo.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
	samplerInfo.magFilter = VK_FILTER_LINEAR;
	samplerInfo.minFilter = VK_FILTER_LINEAR;
	samplerInfo.mipmapMode = VK_SAMPLER_MIPMAP_MODE_LINEAR;
	samplerInfo.addressModeU = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
	samplerInfo.addressModeV = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
	samplerInfo.addressModeW = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
	samplerInfo.mipLodBias = 0.0f;
	samplerInfo.anisotropyEnable = VK_FALSE;
	samplerInfo.maxAnisotropy = 1.0f;
	samplerInfo.compareEnable = VK_FALSE;
	samplerInfo.minLod = 0.0f;
	samplerInfo.maxLod = 0.0f;
	samplerInfo.borderColor = VK_BORDER_COLOR_FLOAT_TRANSPARENT_BLACK;
	samplerInfo.unnormalizedCoordinates = VK_FALSE;
	vkCheck(vkCreateSampler(vk.device, &samplerInfo, nullptr, &sampler), "Failed to create UI sampler.");
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
		throw std::runtime_error("Cannot create UI pipeline with undefined target format.");
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
	vkCheck(result, "Failed to create UI graphics pipeline.");
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

static VkDescriptorImageInfo PlaceholderUiImageInfo(const VulkanUiRenderer& renderer) {
	VkDescriptorImageInfo info{};
	info.sampler = renderer.linearSampler_;
	info.imageView = renderer.placeholderUiTexture_.view;
	info.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
	return info;
}

static void CreateDescriptorObjects(VkDevice device, VulkanUiRenderer& renderer) {
	if (renderer.maxUiImageDescriptors_ == 0u) {
		throw std::runtime_error("UI texture descriptor capacity must be greater than zero.");
	}
	renderer.frameResourceCount_ = std::max<uint32_t>(1u, renderer.frameResourceCount_);

	VkDescriptorSetLayoutBinding globalsBinding{};
	globalsBinding.binding = 0;
	globalsBinding.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
	globalsBinding.descriptorCount = 1;
	globalsBinding.stageFlags = VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT;

	VkDescriptorSetLayoutCreateInfo set0Info{};
	set0Info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
	set0Info.bindingCount = 1;
	set0Info.pBindings = &globalsBinding;
	vkCheck(vkCreateDescriptorSetLayout(device, &set0Info, nullptr, &renderer.descriptors_.set0), "Failed to create UI set0 layout.");

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
	vkCheck(vkCreateDescriptorSetLayout(device, &set1Info, nullptr, &renderer.descriptors_.set1), "Failed to create UI set1 layout.");

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
	vkCheck(vkCreateDescriptorPool(device, &poolInfo, nullptr, &renderer.descriptors_.pool), "Failed to create UI descriptor pool.");

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
		"Failed to allocate UI globals sets.");

	std::vector<VkDescriptorSetLayout> set1Layouts(renderer.frameResourceCount_, renderer.descriptors_.set1);
	VkDescriptorSetAllocateInfo set1AllocInfo{};
	set1AllocInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
	set1AllocInfo.descriptorPool = renderer.descriptors_.pool;
	set1AllocInfo.descriptorSetCount = renderer.frameResourceCount_;
	set1AllocInfo.pSetLayouts = set1Layouts.data();
	vkCheck(
		vkAllocateDescriptorSets(device, &set1AllocInfo, renderer.descriptors_.texturesSets.data()),
		"Failed to allocate UI textures sets.");
}

static void CreatePipelineObjects(VkDevice device, VulkanUiRenderer& renderer) {
	const std::array<VkDescriptorSetLayout, 2> setLayouts = { renderer.descriptors_.set0, renderer.descriptors_.set1 };
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
	vkCheck(vkCreatePipelineLayout(device, &pipelineLayoutInfo, nullptr, &renderer.pipelines_.layout), "Failed to create UI pipeline layout.");

	CreatePipelines(device, renderer.pipelines_, renderer.targetFormat_);
}

static void UpdateInstanceBufferDescriptorForFrame(const VulkanUiRenderer& renderer, VkDevice device, uint32_t frameSlot) {
	if (frameSlot >= renderer.descriptors_.globalsSets.size() || frameSlot >= renderer.instanceBuffersByFrame_.size()) {
		return;
	}
	if (renderer.descriptors_.globalsSets[frameSlot] == VK_NULL_HANDLE) {
		return;
	}
	const VulkanUiRenderer::AllocatedBuffer& instanceBuffer = renderer.instanceBuffersByFrame_[frameSlot];
	if (instanceBuffer.buffer == VK_NULL_HANDLE) {
		return;
	}

	VkDescriptorBufferInfo ssboInfo{};
	ssboInfo.buffer = instanceBuffer.buffer;
	ssboInfo.offset = 0;
	ssboInfo.range = instanceBuffer.size;

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

static void UpdateTextureDescriptorsForFrame(VulkanUiRenderer& renderer, VkDevice device, uint32_t frameSlot) {
	if (frameSlot >= renderer.descriptors_.texturesSets.size()) {
		return;
	}
	if (renderer.descriptors_.texturesSets[frameSlot] == VK_NULL_HANDLE) {
		return;
	}
	VkDescriptorImageInfo fontAtlasInfo{};
	fontAtlasInfo.sampler = renderer.linearSampler_;
	fontAtlasInfo.imageView = renderer.placeholderFontAtlas_.view;
	fontAtlasInfo.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
	if (renderer.fontManager_) {
		const FlowUi::Font::AtlasArrayResource& atlas = renderer.fontManager_->getAtlasResource();
		if (atlas.view != VK_NULL_HANDLE && atlas.sampler != VK_NULL_HANDLE && atlas.layersUsed > 0u) {
			fontAtlasInfo.sampler = atlas.sampler;
			fontAtlasInfo.imageView = atlas.view;
		}
	}

	const VkDescriptorImageInfo uiImageTemplate = PlaceholderUiImageInfo(renderer);
	if (renderer.uiTextureSlotInfos_.size() != renderer.maxUiImageDescriptors_) {
		renderer.uiTextureSlotInfos_.assign(renderer.maxUiImageDescriptors_, uiImageTemplate);
	}

	std::array<VkWriteDescriptorSet, 2> writes{};
	writes[0].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
	writes[0].dstSet = renderer.descriptors_.texturesSets[frameSlot];
	writes[0].dstBinding = 0;
	writes[0].descriptorCount = 1;
	writes[0].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
	writes[0].pImageInfo = &fontAtlasInfo;

	writes[1].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
	writes[1].dstSet = renderer.descriptors_.texturesSets[frameSlot];
	writes[1].dstBinding = 1;
	writes[1].descriptorCount = renderer.maxUiImageDescriptors_;
	writes[1].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
	writes[1].pImageInfo = renderer.uiTextureSlotInfos_.data();

	vkUpdateDescriptorSets(device, static_cast<uint32_t>(writes.size()), writes.data(), 0, nullptr);
	if (frameSlot < renderer.textureDescriptorsDirtyByFrame_.size()) {
		renderer.textureDescriptorsDirtyByFrame_[frameSlot] = false;
	}
}

static void UpdateTextureDescriptorsAllFrames(VulkanUiRenderer& renderer, VkDevice device) {
	for (uint32_t frameSlot = 0u; frameSlot < renderer.frameResourceCount_; ++frameSlot) {
		UpdateTextureDescriptorsForFrame(renderer, device, frameSlot);
	}
}

static void RecreateDescriptorAndPipelineObjects(VulkanContext& vk, VulkanUiRenderer& renderer) {
	DestroyPipelineObjects(vk.device, renderer.pipelines_);
	DestroyDescriptorObjects(vk.device, renderer.descriptors_);
	CreateDescriptorObjects(vk.device, renderer);
	CreatePipelineObjects(vk.device, renderer);
	if (renderer.textureDescriptorsDirtyByFrame_.size() != renderer.frameResourceCount_) {
		renderer.textureDescriptorsDirtyByFrame_.assign(renderer.frameResourceCount_, true);
	}
	if (renderer.boundFontAtlasRevisionByFrame_.size() != renderer.frameResourceCount_) {
		renderer.boundFontAtlasRevisionByFrame_.assign(renderer.frameResourceCount_, UINT32_MAX);
	}
	for (uint32_t frameSlot = 0u; frameSlot < renderer.frameResourceCount_; ++frameSlot) {
		UpdateInstanceBufferDescriptorForFrame(renderer, vk.device, frameSlot);
		UpdateTextureDescriptorsForFrame(renderer, vk.device, frameSlot);
		renderer.boundFontAtlasRevisionByFrame_[frameSlot] = UINT32_MAX;
	}
}

static void EnsureInstanceBufferCapacity(
	VulkanContext& vk,
	VulkanUiRenderer& renderer,
	uint32_t frameSlot,
	size_t requiredInstances) {
	if (requiredInstances == 0) {
		return;
	}
	if (requiredInstances > (std::numeric_limits<VkDeviceSize>::max() / sizeof(UiInstance))) {
		throw std::runtime_error("UI instance count exceeds addressable buffer size.");
	}

	if (frameSlot >= renderer.instanceBuffersByFrame_.size()) {
		throw std::runtime_error("UI renderer frame slot index is out of bounds.");
	}

	VulkanUiRenderer::AllocatedBuffer& instanceBuffer = renderer.instanceBuffersByFrame_[frameSlot];
	const VkDeviceSize requiredBytes = static_cast<VkDeviceSize>(requiredInstances) * sizeof(UiInstance);
	if (instanceBuffer.buffer != VK_NULL_HANDLE && instanceBuffer.size >= requiredBytes) {
		return;
	}

	VkDeviceSize newSize = instanceBuffer.size > 0 ? instanceBuffer.size : kInitialInstanceBufferBytes;
	while (newSize < requiredBytes) {
		newSize *= 2;
	}

	DestroyBuffer(vk, instanceBuffer);
	CreateMappedBuffer(vk, newSize, VK_BUFFER_USAGE_STORAGE_BUFFER_BIT, instanceBuffer);
	UpdateInstanceBufferDescriptorForFrame(renderer, vk.device, frameSlot);
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

void VulkanUiRenderer::init(const FlowUi::AppConfig& config, VulkanContext& vk, VkFormat swapFormat) {
	if (vk.device == VK_NULL_HANDLE || vk.allocator == nullptr) {
		throw std::runtime_error("Vulkan device + allocator must be initialized before UI renderer init.");
	}
	if (swapFormat == VK_FORMAT_UNDEFINED) {
		throw std::runtime_error("Swapchain format must be valid before UI renderer init.");
	}

	destroy(vk);

	try {
		maxUiImageDescriptors_ = kDefaultMaxUiImageDescriptors;
		targetFormat_ = swapFormat;
		frameResourceCount_ = std::max<uint32_t>(1u, config.vk.framesInFlight);
		instanceBuffersByFrame_.assign(frameResourceCount_, AllocatedBuffer{});
		textureDescriptorsDirtyByFrame_.assign(frameResourceCount_, true);
		boundFontAtlasRevisionByFrame_.assign(frameResourceCount_, UINT32_MAX);

		const float configuredDpi = std::max(1.0f, config.ui.dpi);
		pointsToPixelsScale_ = std::max(0.0f, config.ui.fontScale) * (configuredDpi / 72.0f);
		if (pointsToPixelsScale_ <= 0.0f) {
			pointsToPixelsScale_ = configuredDpi / 72.0f;
		}

		CreateLinearSampler(vk, linearSampler_);

		CreatePlaceholderImage(vk, 1, VK_IMAGE_VIEW_TYPE_2D_ARRAY, placeholderFontAtlas_);
		CreatePlaceholderImage(vk, 1, VK_IMAGE_VIEW_TYPE_2D, placeholderUiTexture_);
		uiTextureSlotInfos_.assign(maxUiImageDescriptors_, PlaceholderUiImageInfo(*this));
		RecreateDescriptorAndPipelineObjects(vk, *this);

		const std::array<UiQuadVertex, 4> quadVertices = {
			UiQuadVertex{ 0.0f, 0.0f, 0.0f, 0.0f },
			UiQuadVertex{ 1.0f, 0.0f, 1.0f, 0.0f },
			UiQuadVertex{ 0.0f, 1.0f, 0.0f, 1.0f },
			UiQuadVertex{ 1.0f, 1.0f, 1.0f, 1.0f },
		};
		CreateMappedBuffer(
			vk,
			static_cast<VkDeviceSize>(quadVertices.size() * sizeof(UiQuadVertex)),
			VK_BUFFER_USAGE_VERTEX_BUFFER_BIT,
			quadVertexBuffer_);
		UploadBytesToMappedBuffer(
			vk,
			quadVertexBuffer_,
			quadVertices.data(),
			static_cast<VkDeviceSize>(quadVertices.size() * sizeof(UiQuadVertex)));

		for (uint32_t frameSlot = 0u; frameSlot < frameResourceCount_; ++frameSlot) {
			CreateMappedBuffer(
				vk,
				kInitialInstanceBufferBytes,
				VK_BUFFER_USAGE_STORAGE_BUFFER_BIT,
				instanceBuffersByFrame_[frameSlot]);
			UpdateInstanceBufferDescriptorForFrame(*this, vk.device, frameSlot);
		}

		instancesScratch_.clear();
		runsScratch_.clear();
		instancesScratch_.reserve(4096);
		runsScratch_.reserve(256);
	} catch (...) {
		destroy(vk);
		throw;
	}
}

void VulkanUiRenderer::setFontManager(const FontManager* manager) {
	fontManager_ = manager;
	boundFontAtlasRevisionByFrame_.assign(frameResourceCount_, UINT32_MAX);
}

void VulkanUiRenderer::destroy(VulkanContext& vk)
{
	instancesScratch_.clear();
	runsScratch_.clear();
	uiTextureSlotInfos_.clear();
	textureDescriptorsDirtyByFrame_.clear();
	boundFontAtlasRevisionByFrame_.clear();
	frameResourceCount_ = 1u;

	if (vk.device == VK_NULL_HANDLE || vk.allocator == nullptr) {
		pipelines_ = Pipelines{};
		descriptors_ = Descriptors{};
		instanceBuffersByFrame_.clear();
		quadVertexBuffer_ = AllocatedBuffer{};
		placeholderFontAtlas_ = AllocatedImage{};
		placeholderUiTexture_ = AllocatedImage{};
		linearSampler_ = VK_NULL_HANDLE;
		targetFormat_ = VK_FORMAT_UNDEFINED;
		fontManager_ = nullptr;
		pointsToPixelsScale_ = 96.0f / 72.0f;
		return;
	}

	DestroyPipelineObjects(vk.device, pipelines_);
	DestroyDescriptorObjects(vk.device, descriptors_);

	for (AllocatedBuffer& buffer : instanceBuffersByFrame_) {
		DestroyBuffer(vk, buffer);
	}
	instanceBuffersByFrame_.clear();
	DestroyBuffer(vk, quadVertexBuffer_);
	DestroyImage(vk, placeholderFontAtlas_);
	DestroyImage(vk, placeholderUiTexture_);

	if (linearSampler_ != VK_NULL_HANDLE) {
		vkDestroySampler(vk.device, linearSampler_, nullptr);
		linearSampler_ = VK_NULL_HANDLE;
	}

	targetFormat_ = VK_FORMAT_UNDEFINED;
	fontManager_ = nullptr;
	pointsToPixelsScale_ = 96.0f / 72.0f;
}

void VulkanUiRenderer::onSwapchainFormatChanged(VulkanContext& vk, VkFormat newFormat)
{
	if (vk.device == VK_NULL_HANDLE || newFormat == VK_FORMAT_UNDEFINED) {
		return;
	}
	if (newFormat == targetFormat_ && pipelines_.solid != VK_NULL_HANDLE && pipelines_.msdf != VK_NULL_HANDLE &&
		pipelines_.textured != VK_NULL_HANDLE) {
		return;
	}

	DestroyPipelines(vk.device, pipelines_);
	targetFormat_ = newFormat;
	CreatePipelines(vk.device, pipelines_, targetFormat_);
}

uint32_t VulkanUiRenderer::textureSlotCapacity() const {
	return maxUiImageDescriptors_;
}

void VulkanUiRenderer::reserveTextureSlots(VulkanContext& vk, uint32_t minCapacity) {
	if (minCapacity <= maxUiImageDescriptors_) {
		return;
	}
	if (vk.device == VK_NULL_HANDLE || vk.allocator == nullptr || linearSampler_ == VK_NULL_HANDLE ||
		placeholderUiTexture_.view == VK_NULL_HANDLE || targetFormat_ == VK_FORMAT_UNDEFINED) {
		throw std::runtime_error("UI renderer is not initialized for texture slot growth.");
	}

	uint32_t newCapacity = std::max<uint32_t>(2u, maxUiImageDescriptors_);
	while (newCapacity < minCapacity) {
		newCapacity *= 2u;
	}

	const VkDescriptorImageInfo placeholderInfo = PlaceholderUiImageInfo(*this);
	std::vector<VkDescriptorImageInfo> oldSlotInfos = uiTextureSlotInfos_;
	maxUiImageDescriptors_ = newCapacity;
	uiTextureSlotInfos_.assign(maxUiImageDescriptors_, placeholderInfo);
	if (!oldSlotInfos.empty()) {
		const size_t preserved = std::min(oldSlotInfos.size(), uiTextureSlotInfos_.size());
		for (size_t i = 0; i < preserved; ++i) {
			uiTextureSlotInfos_[i] = oldSlotInfos[i];
		}
	}

	RecreateDescriptorAndPipelineObjects(vk, *this);
	textureDescriptorsDirtyByFrame_.assign(frameResourceCount_, true);
	boundFontAtlasRevisionByFrame_.assign(frameResourceCount_, UINT32_MAX);
}

void VulkanUiRenderer::setTextureSlotBinding(uint32_t slot, VkImageView view, VkSampler sampler) {
	if (slot >= uiTextureSlotInfos_.size()) {
		throw std::runtime_error("UI texture slot index out of bounds.");
	}
	if (view == VK_NULL_HANDLE || sampler == VK_NULL_HANDLE) {
		throw std::runtime_error("UI texture slot binding requires valid image view + sampler.");
	}

	VkDescriptorImageInfo info{};
	info.sampler = sampler;
	info.imageView = view;
	info.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
	uiTextureSlotInfos_[slot] = info;
	textureDescriptorsDirtyByFrame_.assign(frameResourceCount_, true);
}

void VulkanUiRenderer::clearTextureSlotBinding(uint32_t slot) {
	if (slot >= uiTextureSlotInfos_.size()) {
		throw std::runtime_error("UI texture slot index out of bounds.");
	}
	uiTextureSlotInfos_[slot] = PlaceholderUiImageInfo(*this);
	textureDescriptorsDirtyByFrame_.assign(frameResourceCount_, true);
}

void VulkanUiRenderer::rebuildTextureDescriptors(VkDevice device) {
	UpdateTextureDescriptorsAllFrames(*this, device);
	textureDescriptorsDirtyByFrame_.assign(frameResourceCount_, false);
}

void VulkanUiRenderer::render(
	VulkanContext& vk,
	VkCommandBuffer cmd,
	const Clay_RenderCommandArray& renderCommands,
	const FlowUi::detail::InputFieldFrameOverrides& inputFieldOverrides,
	VkExtent2D extent,
	VkImageView targetView,
	uint32_t frameIndex,
	float uiToFramebufferScaleX,
	float uiToFramebufferScaleY)
{
	if (cmd == VK_NULL_HANDLE || targetView == VK_NULL_HANDLE) {
		return;
	}
	if (pipelines_.layout == VK_NULL_HANDLE || pipelines_.solid == VK_NULL_HANDLE) {
		return;
	}
	if (frameResourceCount_ == 0u || instanceBuffersByFrame_.empty() ||
		descriptors_.globalsSets.empty() || descriptors_.texturesSets.empty()) {
		return;
	}

	const uint32_t frameSlot = frameIndex % frameResourceCount_;
	if (frameSlot >= instanceBuffersByFrame_.size() ||
		frameSlot >= descriptors_.globalsSets.size() ||
		frameSlot >= descriptors_.texturesSets.size()) {
		return;
	}

	const AllocatedBuffer& instanceBuffer = instanceBuffersByFrame_[frameSlot];
	if (instanceBuffer.buffer == VK_NULL_HANDLE) {
		return;
	}

	uint32_t latestFontAtlasRevision = 0u;
	if (fontManager_) {
		latestFontAtlasRevision = fontManager_->getAtlasResource().bindingRevision;
	}
	bool textureDescriptorsDirty = true;
	if (frameSlot < textureDescriptorsDirtyByFrame_.size()) {
		textureDescriptorsDirty = textureDescriptorsDirtyByFrame_[frameSlot];
	}
	uint32_t boundRevision = UINT32_MAX;
	if (frameSlot < boundFontAtlasRevisionByFrame_.size()) {
		boundRevision = boundFontAtlasRevisionByFrame_[frameSlot];
	}
	if (textureDescriptorsDirty || latestFontAtlasRevision != boundRevision) {
		UpdateTextureDescriptorsForFrame(*this, vk.device, frameSlot);
		if (frameSlot < boundFontAtlasRevisionByFrame_.size()) {
			boundFontAtlasRevisionByFrame_[frameSlot] = latestFontAtlasRevision;
		}
	}

	const float clampedScaleX = std::max(uiToFramebufferScaleX, 1.0e-6f);
	const float clampedScaleY = std::max(uiToFramebufferScaleY, 1.0e-6f);
	BuildInstancesAndRunsFromClay(
		renderCommands,
		inputFieldOverrides,
		extent,
		fontManager_,
		pointsToPixelsScale_,
		clampedScaleX,
		clampedScaleY,
		instancesScratch_,
		runsScratch_);
	if (instancesScratch_.empty() || runsScratch_.empty()) {
		return;
	}

	EnsureInstanceBufferCapacity(vk, *this, frameSlot, instancesScratch_.size());
	const AllocatedBuffer& activeInstanceBuffer = instanceBuffersByFrame_[frameSlot];
	UploadBytesToMappedBuffer(
		vk,
		activeInstanceBuffer,
		instancesScratch_.data(),
		static_cast<VkDeviceSize>(instancesScratch_.size() * sizeof(UiInstance)));

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

	const VkBuffer vertexBuffer = quadVertexBuffer_.buffer;
	const VkDeviceSize vertexOffset = 0;
	vkCmdBindVertexBuffers(cmd, 0, 1, &vertexBuffer, &vertexOffset);

	const VkDescriptorSet sets[2] = {
		descriptors_.globalsSets[frameSlot],
		descriptors_.texturesSets[frameSlot],
	};
	vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, pipelines_.layout, 0, 2, sets, 0, nullptr);

	for (const UiRun& run : runsScratch_) {
		FlushRun(cmd, *this, extent, run);
	}

	vkCmdEndRendering(cmd);
}
