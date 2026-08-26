#include "devSystems/devTooling/overlay/DevOverlayService.hpp"

#if FLOW_UI_DEV_MODE

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdio>
#include <limits>
#include <string_view>
#include <utility>

namespace FlowUi::devSystems::tooling {
namespace {

constexpr uint32_t kContentColor = 0x2F80ED30u;
constexpr uint32_t kPaddingColor = 0x27AE6040u;
constexpr uint32_t kBorderColor = 0xF2994AEEu;
constexpr uint32_t kGapColor = 0x9B51E040u;
constexpr uint32_t kBadgeColor = 0x1E1E1EFAu;
constexpr uint32_t kGuideColor = 0xEB5757AAu;
constexpr uint32_t kDistanceColor = 0x2F80EDFFu;

/** Overlay primitives use the report's 0xRRGGBBAA notation; UiInstance is byte-packed RGBA. */
[[nodiscard]] constexpr uint32_t nativeUiColor(uint32_t rrggbbaa) noexcept {
	return ((rrggbbaa >> 24u) & 0xFFu) |
		(((rrggbbaa >> 16u) & 0xFFu) << 8u) |
		(((rrggbbaa >> 8u) & 0xFFu) << 16u) |
		((rrggbbaa & 0xFFu) << 24u);
}

[[nodiscard]] float finiteScale(float scale) noexcept {
	return std::isfinite(scale) ? std::clamp(scale, 0.05f, 32.0f) : 1.0f;
}

[[nodiscard]] RectF scaledBounds(const Clay_BoundingBox& bounds, float scale) noexcept {
	return RectF{
		bounds.x * scale,
		bounds.y * scale,
		std::max(0.0f, bounds.width * scale),
		std::max(0.0f, bounds.height * scale),
	};
}

void addFilled(
	DevOverlayCommandBuffer& out,
	RectF bounds,
	uint32_t color,
	float radius = 0.0f) {
	if (bounds.w <= 0.0f || bounds.h <= 0.0f) return;
	DevOverlayPrimitive primitive{};
	primitive.kind = DevOverlayPrimitiveKind::FilledRect;
	primitive.bounds = bounds;
	primitive.colorRGBA = color;
	for (float& value : primitive.cornerRadius) value = radius;
	out.primitives.push_back(std::move(primitive));
}

void addLine(DevOverlayCommandBuffer& out, RectF bounds, uint32_t color) {
	if (bounds.w <= 0.0f || bounds.h <= 0.0f) return;
	DevOverlayPrimitive primitive{};
	primitive.kind = DevOverlayPrimitiveKind::Line;
	primitive.bounds = bounds;
	primitive.colorRGBA = color;
	out.primitives.push_back(std::move(primitive));
}

void addStroke(
	DevOverlayCommandBuffer& out,
	RectF bounds,
	uint32_t color,
	float left,
	float top,
	float right,
	float bottom,
	const std::array<float, 4>& radii = {}) {
	if (bounds.w <= 0.0f || bounds.h <= 0.0f) return;
	DevOverlayPrimitive primitive{};
	primitive.kind = DevOverlayPrimitiveKind::StrokedRect;
	primitive.bounds = bounds;
	primitive.colorRGBA = color;
	primitive.borderWidth[0] = std::max(0.0f, left);
	primitive.borderWidth[1] = std::max(0.0f, top);
	primitive.borderWidth[2] = std::max(0.0f, right);
	primitive.borderWidth[3] = std::max(0.0f, bottom);
	std::copy(radii.begin(), radii.end(), primitive.cornerRadius);
	out.primitives.push_back(std::move(primitive));
}

void addLabel(
	DevOverlayCommandBuffer& out,
	float x,
	float y,
	std::string text,
	float scale,
	uint32_t textColor = 0xFFFFFFFFu,
	uint32_t backgroundColor = kBadgeColor) {
	if (text.empty()) return;
	const float size = 11.0f * scale;
	const float width = std::max(20.0f * scale, (static_cast<float>(text.size()) * 6.4f + 8.0f) * scale);
	const float height = 17.0f * scale;
	addFilled(out, RectF{x, y, width, height}, backgroundColor, 3.0f * scale);
	DevOverlayPrimitive label{};
	label.kind = DevOverlayPrimitiveKind::TextLabel;
	label.bounds = RectF{x + 4.0f * scale, y + 1.0f * scale, width - 8.0f * scale, height};
	label.colorRGBA = textColor;
	label.textLabel = std::move(text);
	label.textSizePoints = size;
	out.primitives.push_back(std::move(label));
}

[[nodiscard]] std::string pixelLabel(float value) {
	char text[48]{};
	std::snprintf(text, sizeof(text), "%.1f px", value);
	return text;
}

[[nodiscard]] std::string dimensionsLabel(const RectF& bounds, float scale) {
	char text[64]{};
	const float divisor = std::max(scale, 1.0e-6f);
	std::snprintf(text, sizeof(text), "%.1f x %.1f px", bounds.w / divisor, bounds.h / divisor);
	return text;
}

#if FLOW_UI_DEV_CAPTURE_CLAY
struct ResolvedTarget {
	const DevFlowNode* flow = nullptr;
	const DevClayNode* clay = nullptr;
	DevFlowNodeIndex flowIndex = InvalidFlowNode;
	DevClayNodeIndex clayIndex = InvalidClayNode;

	[[nodiscard]] explicit operator bool() const noexcept { return flow && clay; }
};

[[nodiscard]] ResolvedTarget resolveTarget(
	const DevOverlayTargetSpec& target,
	const DevTreeSnapshot& snapshot) noexcept {
	if (!target.isValid() || target.flowNodeIndex >= snapshot.flow.nodes.size()) return {};
	const DevFlowNode& flow = snapshot.flow.nodes[target.flowNodeIndex];
	if (target.definition && target.definition != flow.definition) return {};
	if (target.instanceKey && target.instanceKey != flow.instance) return {};
	if (flow.clayRoot >= snapshot.clay.nodes.size()) return {};
	return ResolvedTarget{
		.flow = &flow,
		.clay = &snapshot.clay.nodes[flow.clayRoot],
		.flowIndex = target.flowNodeIndex,
		.clayIndex = flow.clayRoot,
	};
}

void buildParentGap(
	const DevTreeSnapshot& snapshot,
	const ResolvedTarget& target,
	float scale,
	DevOverlayCommandBuffer& out) {
	if (!target || target.clay->parent >= snapshot.clay.nodes.size()) return;
	const DevClayNode& parent = snapshot.clay.nodes[target.clay->parent];
	const float gap = static_cast<float>(parent.declaration.layout.childGap) * scale;
	if (gap <= 0.0f) return;
	const RectF selected = scaledBounds(target.clay->bounds, scale);
	const bool horizontal = parent.declaration.layout.layoutDirection == CLAY_LEFT_TO_RIGHT;
	DevClayNodeIndex previous = InvalidClayNode;
	for (DevClayNodeIndex child = parent.firstChild;
		child < snapshot.clay.nodes.size() && child != target.clayIndex;
		child = snapshot.clay.nodes[child].nextSibling) {
		previous = child;
	}
	if (previous < snapshot.clay.nodes.size()) {
		const RectF sibling = scaledBounds(snapshot.clay.nodes[previous].bounds, scale);
		if (horizontal) addFilled(out, RectF{sibling.x + sibling.w, selected.y,
			std::min(gap, std::max(0.0f, selected.x - sibling.x - sibling.w)), selected.h}, kGapColor);
		else addFilled(out, RectF{selected.x, sibling.y + sibling.h, selected.w,
			std::min(gap, std::max(0.0f, selected.y - sibling.y - sibling.h))}, kGapColor);
	}
	if (target.clay->nextSibling < snapshot.clay.nodes.size()) {
		const RectF sibling = scaledBounds(snapshot.clay.nodes[target.clay->nextSibling].bounds, scale);
		if (horizontal) addFilled(out, RectF{selected.x + selected.w, selected.y, std::min(gap, std::max(0.0f, sibling.x - selected.x - selected.w)), selected.h}, kGapColor);
		else addFilled(out, RectF{selected.x, selected.y + selected.h, selected.w, std::min(gap, std::max(0.0f, sibling.y - selected.y - selected.h))}, kGapColor);
	}
}

void buildBoxModel(
	const DevTreeSnapshot& snapshot,
	const ResolvedTarget& target,
	float scale,
	DevOverlayCommandBuffer& out) {
	const DevClayNode& node = *target.clay;
	const RectF bounds = scaledBounds(node.bounds, scale);
	const Clay_Padding& padding = node.declaration.layout.padding;
	const Clay_BorderWidth& border = node.declaration.border.width;
	const float left = static_cast<float>(border.left) * scale;
	const float top = static_cast<float>(border.top) * scale;
	const float right = static_cast<float>(border.right) * scale;
	const float bottom = static_cast<float>(border.bottom) * scale;
	const float padLeft = static_cast<float>(padding.left) * scale;
	const float padTop = static_cast<float>(padding.top) * scale;
	const float padRight = static_cast<float>(padding.right) * scale;
	const float padBottom = static_cast<float>(padding.bottom) * scale;
	const RectF inner{
		bounds.x + left,
		bounds.y + top,
		std::max(0.0f, bounds.w - left - right),
		std::max(0.0f, bounds.h - top - bottom),
	};
	const RectF content{
		inner.x + padLeft,
		inner.y + padTop,
		std::max(0.0f, inner.w - padLeft - padRight),
		std::max(0.0f, inner.h - padTop - padBottom),
	};
	const Clay_CornerRadius& radius = node.declaration.cornerRadius;
	DevOverlayPrimitive contentPrimitive{};
	contentPrimitive.kind = DevOverlayPrimitiveKind::FilledRect;
	contentPrimitive.bounds = content;
	contentPrimitive.colorRGBA = kContentColor;
	contentPrimitive.cornerRadius[0] = std::max(0.0f, radius.topLeft * scale - top - padTop);
	contentPrimitive.cornerRadius[1] = std::max(0.0f, radius.topRight * scale - top - padRight);
	contentPrimitive.cornerRadius[2] = std::max(0.0f, radius.bottomRight * scale - bottom - padRight);
	contentPrimitive.cornerRadius[3] = std::max(0.0f, radius.bottomLeft * scale - bottom - padLeft);
	if (content.w > 0.0f && content.h > 0.0f) out.primitives.push_back(std::move(contentPrimitive));
	addFilled(out, RectF{content.x, inner.y, content.w, padTop}, kPaddingColor);
	addFilled(out, RectF{content.x, content.y + content.h, content.w, padBottom}, kPaddingColor);
	addFilled(out, RectF{inner.x, inner.y, padLeft, inner.h}, kPaddingColor);
	addFilled(out, RectF{content.x + content.w, inner.y, padRight, inner.h}, kPaddingColor);
	const std::array<float, 4> radii{
		radius.topLeft * scale,
		radius.topRight * scale,
		radius.bottomRight * scale,
		radius.bottomLeft * scale,
	};
	addStroke(
		out, bounds, kBorderColor,
		std::max(left, 1.5f * scale), std::max(top, 1.5f * scale),
		std::max(right, 1.5f * scale), std::max(bottom, 1.5f * scale), radii);
	buildParentGap(snapshot, target, scale, out);
	addLabel(out, content.x + 4.0f * scale, content.y + 4.0f * scale, dimensionsLabel(content, scale), scale);
}

void addHorizontalDistance(
	const RectF& primary,
	const RectF& secondary,
	float scale,
	DevOverlayCommandBuffer& out) {
	float start = 0.0f;
	float end = 0.0f;
	if (primary.x + primary.w <= secondary.x) {
		start = primary.x + primary.w;
		end = secondary.x;
	} else if (secondary.x + secondary.w <= primary.x) {
		start = secondary.x + secondary.w;
		end = primary.x;
	} else return;
	const float y0 = std::max(primary.y, secondary.y);
	const float y1 = std::min(primary.y + primary.h, secondary.y + secondary.h);
	const float y = y1 >= y0 ? (y0 + y1) * 0.5f : (primary.y + primary.h * 0.5f + secondary.y + secondary.h * 0.5f) * 0.5f;
	const float thickness = 1.5f * scale;
	addLine(out, RectF{start, y, end - start, thickness}, kDistanceColor);
	addLine(out, RectF{start, y - 3.5f * scale, thickness, 7.0f * scale}, kDistanceColor);
	addLine(out, RectF{end - thickness, y - 3.5f * scale, thickness, 7.0f * scale}, kDistanceColor);
	addLabel(out, (start + end) * 0.5f - 18.0f * scale, y + 3.0f * scale, pixelLabel((end - start) / scale), scale, 0x56CCF2FFu, 0x111827FAu);
}

void addVerticalDistance(
	const RectF& primary,
	const RectF& secondary,
	float scale,
	DevOverlayCommandBuffer& out) {
	float start = 0.0f;
	float end = 0.0f;
	if (primary.y + primary.h <= secondary.y) {
		start = primary.y + primary.h;
		end = secondary.y;
	} else if (secondary.y + secondary.h <= primary.y) {
		start = secondary.y + secondary.h;
		end = primary.y;
	} else return;
	const float x0 = std::max(primary.x, secondary.x);
	const float x1 = std::min(primary.x + primary.w, secondary.x + secondary.w);
	const float x = x1 >= x0 ? (x0 + x1) * 0.5f : (primary.x + primary.w * 0.5f + secondary.x + secondary.w * 0.5f) * 0.5f;
	const float thickness = 1.5f * scale;
	addLine(out, RectF{x, start, thickness, end - start}, kDistanceColor);
	addLine(out, RectF{x - 3.5f * scale, start, 7.0f * scale, thickness}, kDistanceColor);
	addLine(out, RectF{x - 3.5f * scale, end - thickness, 7.0f * scale, thickness}, kDistanceColor);
	addLabel(out, x + 3.0f * scale, (start + end) * 0.5f - 8.0f * scale, pixelLabel((end - start) / scale), scale, 0x56CCF2FFu, 0x111827FAu);
}

void buildRulers(
	const DevTreeSnapshot& snapshot,
	const ResolvedTarget& primaryTarget,
	const ResolvedTarget& secondaryTarget,
	float viewportWidth,
	float viewportHeight,
	float scale,
	DevOverlayCommandBuffer& out) {
	const RectF primary = scaledBounds(primaryTarget.clay->bounds, scale);
	const float line = std::max(1.0f, scale);
	addLine(out, RectF{0.0f, primary.y, viewportWidth * scale, line}, kGuideColor);
	addLine(out, RectF{0.0f, primary.y + primary.h, viewportWidth * scale, line}, kGuideColor);
	addLine(out, RectF{primary.x, 0.0f, line, viewportHeight * scale}, kGuideColor);
	addLine(out, RectF{primary.x + primary.w, 0.0f, line, viewportHeight * scale}, kGuideColor);

	const bool compareToParent = !secondaryTarget &&
		primaryTarget.clay->parent < snapshot.clay.nodes.size();
	const DevClayNode* comparison = secondaryTarget ? secondaryTarget.clay : nullptr;
	if (compareToParent) {
		comparison = &snapshot.clay.nodes[primaryTarget.clay->parent];
	}
	if (!comparison || comparison == primaryTarget.clay) return;
	const RectF secondary = scaledBounds(comparison->bounds, scale);
	if (compareToParent) {
		const float leftGap = std::max(0.0f, primary.x - secondary.x);
		const float rightGap = std::max(0.0f, secondary.x + secondary.w - primary.x - primary.w);
		const RectF horizontalEdge{
			leftGap <= rightGap ? secondary.x : secondary.x + secondary.w,
			primary.y, 0.0f, primary.h};
		const float topGap = std::max(0.0f, primary.y - secondary.y);
		const float bottomGap = std::max(0.0f, secondary.y + secondary.h - primary.y - primary.h);
		const RectF verticalEdge{
			primary.x,
			topGap <= bottomGap ? secondary.y : secondary.y + secondary.h,
			primary.w, 0.0f};
		addHorizontalDistance(primary, horizontalEdge, scale, out);
		addVerticalDistance(primary, verticalEdge, scale, out);
	} else {
		addHorizontalDistance(primary, secondary, scale, out);
		addVerticalDistance(primary, secondary, scale, out);
	}
	constexpr float epsilon = 0.5f;
	if (std::abs(primary.x - secondary.x) < epsilon ||
		std::abs((primary.x + primary.w) - (secondary.x + secondary.w)) < epsilon) {
		const float x = std::abs(primary.x - secondary.x) < epsilon ? primary.x : primary.x + primary.w;
		addLine(out, RectF{x, 0.0f, 1.5f * scale, viewportHeight * scale}, 0x00C7BEFFu);
	}
	if (std::abs(primary.y - secondary.y) < epsilon ||
		std::abs((primary.y + primary.h) - (secondary.y + secondary.h)) < epsilon) {
		const float y = std::abs(primary.y - secondary.y) < epsilon ? primary.y : primary.y + primary.h;
		addLine(out, RectF{0.0f, y, viewportWidth * scale, 1.5f * scale}, 0x00C7BEFFu);
	}
}

[[nodiscard]] uint32_t siblingIndex(const DevTreeSnapshot& snapshot, DevFlowNodeIndex index) noexcept {
	if (index >= snapshot.flow.nodes.size()) return 0;
	const DevFlowNodeIndex parent = snapshot.flow.nodes[index].parent;
	if (parent >= snapshot.flow.nodes.size()) return 0;
	uint32_t sibling = 0;
	for (DevFlowNodeIndex node = snapshot.flow.nodes[parent].firstChild;
		node < snapshot.flow.nodes.size() && node != index;
		node = snapshot.flow.nodes[node].nextSibling) {
		++sibling;
	}
	return sibling;
}

void buildTreeHierarchy(
	const DevTreeSnapshot& snapshot,
	const ResolvedTarget& target,
	float scale,
	DevOverlayCommandBuffer& out) {
	constexpr std::array<uint32_t, 6> palette{
		0xEB5757FFu, 0xF2994AFFu, 0xF2C94CFFu,
		0x27AE60FFu, 0x2F80EDFFu, 0x9B51E0FFu,
	};
	const uint32_t end = std::min<uint32_t>(target.flow->subtreeEnd, snapshot.flow.nodes.size());
	for (DevFlowNodeIndex index = target.flowIndex + 1u; index < end; ++index) {
		const DevFlowNode& flow = snapshot.flow.nodes[index];
		if (flow.clayRoot >= snapshot.clay.nodes.size()) continue;
		const uint32_t relativeDepth = flow.depth >= target.flow->depth ? flow.depth - target.flow->depth : 0u;
		const uint32_t color = palette[relativeDepth % palette.size()] ^
			((siblingIndex(snapshot, index) * 0x1F1F1F00u) & 0x00FFFFFFu);
		const RectF bounds = scaledBounds(snapshot.clay.nodes[flow.clayRoot].bounds, scale);
		addStroke(out, bounds, color, scale, scale, scale, scale);
		if (flow.parent < snapshot.flow.nodes.size()) {
			const DevFlowNode& parent = snapshot.flow.nodes[flow.parent];
			if (parent.clayRoot < snapshot.clay.nodes.size()) {
				const RectF parentBounds = scaledBounds(snapshot.clay.nodes[parent.clayRoot].bounds, scale);
				const float parentX = parentBounds.x + parentBounds.w * 0.5f;
				const float parentY = parentBounds.y + parentBounds.h * 0.5f;
				const float childX = bounds.x + bounds.w * 0.5f;
				const float childY = bounds.y + bounds.h * 0.5f;
				addLine(out, RectF{std::min(parentX, childX), parentY,
					std::max(scale, std::abs(childX - parentX)), scale}, color);
				addLine(out, RectF{childX, std::min(parentY, childY), scale,
					std::max(scale, std::abs(childY - parentY))}, color);
			}
		}
		std::string name(snapshot.string(flow.debugName));
		if (name.empty()) name = "flow";
		char prefix[24]{};
		std::snprintf(prefix, sizeof(prefix), "L%u: ", relativeDepth);
		addLabel(out, bounds.x + 2.0f * scale, bounds.y + 2.0f * scale, std::string(prefix) + name, scale);
	}
	if (target.flow->parent < snapshot.flow.nodes.size()) {
		const DevFlowNode& parent = snapshot.flow.nodes[target.flow->parent];
		if (parent.clayRoot < snapshot.clay.nodes.size()) {
			addStroke(out, scaledBounds(snapshot.clay.nodes[parent.clayRoot].bounds, scale),
				0xFFFFFF80u, 1.5f * scale, 1.5f * scale, 1.5f * scale, 1.5f * scale);
		}
	}
}

void buildTypography(
	const DevTreeSnapshot& snapshot,
	const ResolvedTarget& target,
	float scale,
	DevOverlayCommandBuffer& out) {
	const std::span<const DevClayNode> nodes = fullClaySubtree(snapshot, target.flowIndex);
	for (const DevClayNode& node : nodes) {
		if (!hasFlag(node.flags, DevClayNodeFlag::Text)) continue;
		const RectF bounds = scaledBounds(node.bounds, scale);
		const float fontSize = static_cast<float>(std::max<uint16_t>(1u, node.textConfig.fontSize)) * scale;
		const float baseline = bounds.y + std::min(bounds.h, fontSize * 0.80f);
		const float capHeight = fontSize * 0.70f;
		const float descender = fontSize * 0.20f;
		addLine(out, RectF{bounds.x, baseline, bounds.w, 1.5f * scale}, 0xFF007AFFu);
		addLine(out, RectF{bounds.x, baseline - capHeight, bounds.w, scale}, 0x00C7BEFFu);
		addLine(out, RectF{bounds.x, baseline + descender, bounds.w, scale}, 0xFFCC00FFu);
		addStroke(out, RectF{bounds.x, bounds.y,
			std::max(bounds.w, node.unwrappedTextDimensions.width * scale),
			std::max(bounds.h, node.unwrappedTextDimensions.height * scale)},
			0xF2994A80u, scale, scale, scale, scale);
	}
}

void buildClipOverlay(
	const DevTreeSnapshot& snapshot,
	const ResolvedTarget& target,
	float scale,
	DevOverlayCommandBuffer& out) {
	const std::span<const DevClayNode> nodes = fullClaySubtree(snapshot, target.flowIndex);
	for (const DevClayNode& node : nodes) {
		if (!node.declaration.clip.horizontal && !node.declaration.clip.vertical) continue;
		const RectF clip = scaledBounds(node.bounds, scale);
		RectF extent = clip;
		if (node.firstChild < snapshot.clay.nodes.size()) {
			const uint32_t end = std::min<uint32_t>(node.subtreeEnd, snapshot.clay.nodes.size());
			for (uint32_t index = node.firstChild; index < end; ++index) {
				const RectF child = scaledBounds(snapshot.clay.nodes[index].bounds, scale);
				const float right = std::max(extent.x + extent.w, child.x + child.w);
				const float bottom = std::max(extent.y + extent.h, child.y + child.h);
				extent.x = std::min(extent.x, child.x);
				extent.y = std::min(extent.y, child.y);
				extent.w = right - extent.x;
				extent.h = bottom - extent.y;
			}
		}
		addFilled(out, extent, 0xEB575720u);
		addStroke(out, clip, 0xEB5757FFu, 2.0f * scale, 2.0f * scale, 2.0f * scale, 2.0f * scale);
		const float overflowX = std::max(0.0f, extent.w - clip.w) / scale;
		const float overflowY = std::max(0.0f, extent.h - clip.h) / scale;
		if (overflowX > 0.5f || overflowY > 0.5f) {
			char text[80]{};
			std::snprintf(text, sizeof(text), "Overflow: %.1f px X / %.1f px Y", overflowX, overflowY);
			addLabel(out, clip.x + 3.0f * scale, clip.y + 3.0f * scale, text, scale, 0xFFFFFFFFu, 0xEB5757FAu);
		}
	}
}

void buildRenderDiagnostics(
	const DevTreeSnapshot& snapshot,
	const ResolvedTarget& target,
	float scale,
	DevOverlayCommandBuffer& out,
	const Clay_RenderCommandArray* renderCommands) {
	if (renderCommands && renderCommands->length > 0 &&
		renderCommands->length <= renderCommands->capacity && renderCommands->internalArray) {
		const RectF targetBounds = scaledBounds(target.clay->bounds, scale);
		Clay_RenderCommandType previousType = CLAY_RENDER_COMMAND_TYPE_NONE;
		const void* previousTexture = nullptr;
		for (int32_t index = 0; index < renderCommands->length; ++index) {
			const Clay_RenderCommand& command = renderCommands->internalArray[index];
			const bool renderable = command.commandType == CLAY_RENDER_COMMAND_TYPE_RECTANGLE ||
				command.commandType == CLAY_RENDER_COMMAND_TYPE_BORDER ||
				command.commandType == CLAY_RENDER_COMMAND_TYPE_TEXT ||
				command.commandType == CLAY_RENDER_COMMAND_TYPE_IMAGE;
			const RectF bounds = scaledBounds(command.boundingBox, scale);
			const float left = std::max(bounds.x, targetBounds.x);
			const float top = std::max(bounds.y, targetBounds.y);
			const float right = std::min(bounds.x + bounds.w, targetBounds.x + targetBounds.w);
			const float bottom = std::min(bounds.y + bounds.h, targetBounds.y + targetBounds.h);
			const bool intersectsTarget = right > left && bottom > top;
			if (renderable && intersectsTarget) {
				addFilled(out, RectF{left, top, right - left, bottom - top}, 0x00FF001Au);
				const void* texture = command.commandType == CLAY_RENDER_COMMAND_TYPE_IMAGE
					? command.renderData.image.imageData : nullptr;
				const bool batchBreak = previousType != CLAY_RENDER_COMMAND_TYPE_NONE &&
					(command.commandType != previousType || texture != previousTexture);
				if (batchBreak) addFilled(out, RectF{bounds.x - 3.0f * scale, bounds.y - 3.0f * scale,
					6.0f * scale, 6.0f * scale}, 0xFF5722FFu, 1.5f * scale);
				previousType = command.commandType;
				previousTexture = texture;
			} else if (command.commandType == CLAY_RENDER_COMMAND_TYPE_SCISSOR_START ||
				command.commandType == CLAY_RENDER_COMMAND_TYPE_SCISSOR_END) {
				if (intersectsTarget) addFilled(out, RectF{bounds.x - 3.0f * scale, bounds.y - 3.0f * scale,
					6.0f * scale, 6.0f * scale}, 0xFF5722FFu, 1.5f * scale);
				previousType = CLAY_RENDER_COMMAND_TYPE_NONE;
				previousTexture = nullptr;
			}
		}
		return;
	}
	const std::span<const DevClayNode> nodes = fullClaySubtree(snapshot, target.flowIndex);
	for (const DevClayNode& node : nodes) {
		const RectF bounds = scaledBounds(node.bounds, scale);
		addFilled(out, bounds, 0x00FF001Au);
		const bool breakCandidate = node.declaration.clip.horizontal || node.declaration.clip.vertical ||
			node.pointerPresence.imageData || node.clipClayId != 0u;
		if (breakCandidate) {
			addFilled(out, RectF{bounds.x - 3.0f * scale, bounds.y - 3.0f * scale,
				6.0f * scale, 6.0f * scale}, 0xFF5722FFu, 1.5f * scale);
		}
	}
}
#endif

[[nodiscard]] UiInstance solidInstance(
	const DevOverlayPrimitive& primitive,
	float scaleX,
	float scaleY) noexcept {
	const float uniform = std::min(scaleX, scaleY);
	UiInstance instance{};
	instance.type = static_cast<uint32_t>(UiType::Solid);
	instance.x = primitive.bounds.x * scaleX;
	instance.y = primitive.bounds.y * scaleY;
	instance.w = primitive.bounds.w * scaleX;
	instance.h = primitive.bounds.h * scaleY;
	instance.colorRGBA = nativeUiColor(primitive.colorRGBA);
	instance.r0 = primitive.cornerRadius[0] * uniform;
	instance.r1 = primitive.cornerRadius[1] * uniform;
	instance.r2 = primitive.cornerRadius[2] * uniform;
	instance.r3 = primitive.cornerRadius[3] * uniform;
	instance.borderL = primitive.borderWidth[0] * scaleX;
	instance.borderT = primitive.borderWidth[1] * scaleY;
	instance.borderR = primitive.borderWidth[2] * scaleX;
	instance.borderB = primitive.borderWidth[3] * scaleY;
	instance.solidMode = primitive.kind == DevOverlayPrimitiveKind::StrokedRect ? 1u : 0u;
	return instance;
}

} // namespace

void DevOverlayService::generateOverlayCommands(
	const DevOverlaySelectionSpec& selection,
	const DevTreeSnapshot& treeSnapshot,
	float viewportWidth,
	float viewportHeight,
	DevOverlayCommandBuffer& outCommandBuffer,
	const Clay_RenderCommandArray* renderCommands) noexcept {
	outCommandBuffer.clear();
#if FLOW_UI_DEV_CAPTURE_CLAY
	try {
		const ResolvedTarget primary = resolveTarget(selection.primaryTarget, treeSnapshot);
		if (!primary) return;
		const ResolvedTarget secondary = resolveTarget(selection.secondaryTarget, treeSnapshot);
		const float scale = finiteScale(selection.uiScaleFactor);
		if (hasFlag(selection.modeFlags, DevOverlayModeFlags::BoxModel))
			buildBoxModel(treeSnapshot, primary, scale, outCommandBuffer);
		if (hasFlag(selection.modeFlags, DevOverlayModeFlags::RulersAndDistance))
			buildRulers(treeSnapshot, primary, secondary, viewportWidth, viewportHeight, scale, outCommandBuffer);
		if (hasFlag(selection.modeFlags, DevOverlayModeFlags::TreeHierarchy))
			buildTreeHierarchy(treeSnapshot, primary, scale, outCommandBuffer);
		if (hasFlag(selection.modeFlags, DevOverlayModeFlags::Typography))
			buildTypography(treeSnapshot, primary, scale, outCommandBuffer);
		if (hasFlag(selection.modeFlags, DevOverlayModeFlags::ScissorAndClip))
			buildClipOverlay(treeSnapshot, primary, scale, outCommandBuffer);
		if (hasFlag(selection.modeFlags, DevOverlayModeFlags::RenderRunDiagnostics))
			buildRenderDiagnostics(treeSnapshot, primary, scale, outCommandBuffer, renderCommands);
	} catch (...) {
		outCommandBuffer.clear();
	}
#else
	(void)selection;
	(void)treeSnapshot;
	(void)viewportWidth;
	(void)viewportHeight;
	(void)renderCommands;
#endif
}

void DevOverlayService::buildUiRendererInstances(
	DevOverlayCommandBuffer& commandBuffer,
	const ::FlowUi::detail::manager_storage::FontFrameView& fontView,
	float pointsToPixelsScale,
	float uiToFramebufferScaleX,
	float uiToFramebufferScaleY,
	float framebufferWidth,
	float framebufferHeight) noexcept {
	commandBuffer.instances.clear();
	commandBuffer.runs.clear();
	try {
		const float scaleX = std::max(uiToFramebufferScaleX, 1.0e-6f);
		const float scaleY = std::max(uiToFramebufferScaleY, 1.0e-6f);
		const RectF fullScissor{0.0f, 0.0f, framebufferWidth, framebufferHeight};
		const uint32_t solidStart = static_cast<uint32_t>(commandBuffer.instances.size());
		for (const DevOverlayPrimitive& primitive : commandBuffer.primitives) {
			if (primitive.kind == DevOverlayPrimitiveKind::TextLabel ||
				primitive.kind == DevOverlayPrimitiveKind::TextureQuad) continue;
			commandBuffer.instances.push_back(solidInstance(primitive, scaleX, scaleY));
		}
		const uint32_t solidCount = static_cast<uint32_t>(commandBuffer.instances.size()) - solidStart;
		if (solidCount != 0u) commandBuffer.runs.push_back(UiRun{UiType::Solid, fullScissor, solidStart, solidCount});

		const uint32_t msdfStart = static_cast<uint32_t>(commandBuffer.instances.size());
		for (const DevOverlayPrimitive& primitive : commandBuffer.primitives) {
			if (primitive.kind != DevOverlayPrimitiveKind::TextLabel || primitive.textLabel.empty()) continue;
			const auto& layout = textLayoutService_.layout(::FlowUi::detail::text::TextLayoutRequest{
				.text = primitive.textLabel,
				.fontView = &fontView,
				.fontId = primitive.fontId,
				.pointsToPixelsScale = pointsToPixelsScale,
				.fontSize = static_cast<uint16_t>(std::clamp(std::lround(primitive.textSizePoints), 1l, 65535l)),
				.letterSpacing = primitive.letterSpacing,
				.tabWidth = 4,
				.includeGlyphGeometry = true,
			});
			if (!layout.success) continue;
			for (const ::FlowUi::detail::text::TextLayoutGlyph& glyph : layout.glyphs) {
				UiInstance instance{};
				instance.type = static_cast<uint32_t>(UiType::Msdf);
				instance.x = (primitive.bounds.x + glyph.x) * scaleX;
				instance.y = (primitive.bounds.y + glyph.y) * scaleY;
				instance.w = glyph.width * scaleX;
				instance.h = glyph.height * scaleY;
				instance.colorRGBA = nativeUiColor(primitive.colorRGBA);
				instance.uv0x = glyph.u0;
				instance.uv0y = glyph.v0;
				instance.uv1x = glyph.u1;
				instance.uv1y = glyph.v1;
				instance.atlasLayer = layout.atlasLayer;
				instance.r0 = layout.distanceRangePx;
				commandBuffer.instances.push_back(instance);
			}
		}
		const uint32_t msdfCount = static_cast<uint32_t>(commandBuffer.instances.size()) - msdfStart;
		if (msdfCount != 0u) commandBuffer.runs.push_back(UiRun{UiType::Msdf, fullScissor, msdfStart, msdfCount});

		const uint32_t textureStart = static_cast<uint32_t>(commandBuffer.instances.size());
		for (const DevOverlayPrimitive& primitive : commandBuffer.primitives) {
			if (primitive.kind != DevOverlayPrimitiveKind::TextureQuad) continue;
			UiInstance instance = solidInstance(primitive, scaleX, scaleY);
			instance.type = static_cast<uint32_t>(UiType::Textured);
			instance.texIndex = primitive.textureIndex;
			commandBuffer.instances.push_back(instance);
		}
		const uint32_t textureCount = static_cast<uint32_t>(commandBuffer.instances.size()) - textureStart;
		if (textureCount != 0u) commandBuffer.runs.push_back(UiRun{UiType::Textured, fullScissor, textureStart, textureCount});
	} catch (...) {
		commandBuffer.instances.clear();
		commandBuffer.runs.clear();
	}
}

} // namespace FlowUi::devSystems::tooling

#endif
