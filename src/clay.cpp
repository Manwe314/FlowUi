#define CLAY_IMPLEMENTATION
#include "clay.h"

#include "FlowUi/BuildConfig.hpp"

#if FLOW_UI_DEV_MODE && FLOW_UI_DEV_CAPTURE_CLAY
#include "internal/ClayDevTreeBridge.hpp"

namespace FlowUi::detail {

uint32_t clayDevLayoutElementCount(const Clay_Context& context) noexcept {
	return context.layoutElements.length > 0
		? static_cast<uint32_t>(context.layoutElements.length) : 0u;
}

uint32_t clayDevTreeRootCount(const Clay_Context& context) noexcept {
	return context.layoutElementTreeRoots.length > 0
		? static_cast<uint32_t>(context.layoutElementTreeRoots.length) : 0u;
}

ClayDevVisitResult clayDevVisitTrees(
	const Clay_Context& context,
	std::span<ClayDevTraversalEntry> scratch,
	void* userData,
	ClayDevTreeVisitor visitor) noexcept {
	if (Clay_GetCurrentContext() != &context) return ClayDevVisitResult::WrongCurrentContext;
	if (scratch.size() < static_cast<size_t>(context.layoutElements.length)) {
		return ClayDevVisitResult::ScratchTooSmall;
	}

	for (int32_t rootIndex = 0; rootIndex < context.layoutElementTreeRoots.length; ++rootIndex) {
		const Clay__LayoutElementTreeRoot& root = context.layoutElementTreeRoots.internalArray[rootIndex];
		if (root.layoutElementIndex < 0 || root.layoutElementIndex >= context.layoutElements.length) {
			return ClayDevVisitResult::InvalidClayIndex;
		}
		const ClayDevRootView rootView{
			.layoutElementIndex = root.layoutElementIndex,
			.parentId = root.parentId,
			.clipElementId = root.clipElementId,
			.zIndex = root.zIndex,
			.paintOrder = static_cast<uint32_t>(rootIndex),
		};
		if (visitor.onRoot && !visitor.onRoot(userData, rootView)) {
			return ClayDevVisitResult::VisitorStopped;
		}

		size_t stackSize = 0;
		scratch[stackSize++] = {
			.layoutElementIndex = root.layoutElementIndex,
			.parentLayoutElementIndex = -1,
			.rootPaintOrder = static_cast<uint32_t>(rootIndex),
			.depthWithinRoot = 0,
		};
		while (stackSize > 0) {
			const ClayDevTraversalEntry entry = scratch[--stackSize];
			if (entry.layoutElementIndex < 0 ||
				entry.layoutElementIndex >= context.layoutElements.length) {
				return ClayDevVisitResult::InvalidClayIndex;
			}
			const Clay_LayoutElement& element =
				context.layoutElements.internalArray[entry.layoutElementIndex];
			ClayDevElementView view{
				.layoutElementIndex = entry.layoutElementIndex,
				.parentLayoutElementIndex = entry.parentLayoutElementIndex,
				.rootPaintOrder = entry.rootPaintOrder,
				.depthWithinRoot = entry.depthWithinRoot,
				.id = element.id,
				.dimensions = element.dimensions,
				.minDimensions = element.minDimensions,
				.isText = element.isTextElement,
				.exiting = element.exiting,
			};
			if (entry.layoutElementIndex < context.layoutElementIdStrings.length) {
				view.idString = context.layoutElementIdStrings.internalArray[entry.layoutElementIndex];
			}
			if (entry.layoutElementIndex < context.layoutElementClipElementIds.length) {
				view.clipElementId = static_cast<uint32_t>(
					context.layoutElementClipElementIds.internalArray[entry.layoutElementIndex]);
			}
			Clay_LayoutElementHashMapItem* hashItem = Clay__GetHashMapItem(element.id);
			if (hashItem && hashItem->layoutElement) {
				view.bounds = hashItem->boundingBox;
				view.boundsAvailable = true;
			}
			if (element.isTextElement) {
				view.textConfig = element.textConfig;
				view.text = element.textElementData.text;
				view.unwrappedTextDimensions = element.textElementData.preferredDimensions;
				view.wrappedLineCount = element.textElementData.wrappedLines.length > 0
					? static_cast<uint32_t>(element.textElementData.wrappedLines.length) : 0u;
			} else {
				view.declaration = element.config;
			}
			if (visitor.onElement && !visitor.onElement(userData, view)) {
				return ClayDevVisitResult::VisitorStopped;
			}
			for (int32_t child = element.children.length - 1; child >= 0; --child) {
				if (stackSize >= scratch.size()) return ClayDevVisitResult::ScratchTooSmall;
				const int32_t childIndex = element.children.elements[child];
				if (childIndex < 0 || childIndex >= context.layoutElements.length) {
					return ClayDevVisitResult::InvalidClayIndex;
				}
				scratch[stackSize++] = {
					.layoutElementIndex = childIndex,
					.parentLayoutElementIndex = entry.layoutElementIndex,
					.rootPaintOrder = entry.rootPaintOrder,
					.depthWithinRoot = entry.depthWithinRoot + 1u,
				};
			}
		}
	}
	return ClayDevVisitResult::Complete;
}

} // namespace FlowUi::detail
#endif
