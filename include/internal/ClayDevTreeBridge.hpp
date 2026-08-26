#pragma once

#include "FlowUi/BuildConfig.hpp"

#if FLOW_UI_DEV_MODE && FLOW_UI_DEV_CAPTURE_CLAY

#include <cstdint>
#include <span>

#include <clay.h>

namespace FlowUi::detail {

struct ClayDevTraversalEntry {
	int32_t layoutElementIndex = -1;
	int32_t parentLayoutElementIndex = -1;
	uint32_t rootPaintOrder = 0;
	uint32_t depthWithinRoot = 0;
};

struct ClayDevRootView {
	int32_t layoutElementIndex = -1;
	uint32_t parentId = 0;
	uint32_t clipElementId = 0;
	int16_t zIndex = 0;
	uint32_t paintOrder = 0;
};

struct ClayDevElementView {
	int32_t layoutElementIndex = -1;
	int32_t parentLayoutElementIndex = -1;
	uint32_t rootPaintOrder = 0;
	uint32_t depthWithinRoot = 0;
	uint32_t id = 0;
	Clay_String idString{};
	Clay_BoundingBox bounds{};
	Clay_Dimensions dimensions{};
	Clay_Dimensions minDimensions{};
	uint32_t clipElementId = 0;
	Clay_ElementDeclaration declaration{};
	Clay_TextElementConfig textConfig{};
	Clay_String text{};
	Clay_Dimensions unwrappedTextDimensions{};
	uint32_t wrappedLineCount = 0;
	bool boundsAvailable = false;
	bool isText = false;
	bool exiting = false;
};

enum class ClayDevVisitResult : uint8_t {
	Complete,
	WrongCurrentContext,
	ScratchTooSmall,
	InvalidClayIndex,
	VisitorStopped,
};

struct ClayDevTreeVisitor {
	bool (*onRoot)(void*, const ClayDevRootView&) = nullptr;
	bool (*onElement)(void*, const ClayDevElementView&) = nullptr;
};

[[nodiscard]] uint32_t clayDevLayoutElementCount(const Clay_Context& context) noexcept;
[[nodiscard]] uint32_t clayDevTreeRootCount(const Clay_Context& context) noexcept;
[[nodiscard]] ClayDevVisitResult clayDevVisitTrees(
	const Clay_Context& context,
	std::span<ClayDevTraversalEntry> scratch,
	void* userData,
	ClayDevTreeVisitor visitor) noexcept;

} // namespace FlowUi::detail

#endif
