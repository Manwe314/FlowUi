#pragma once

#include "devMode/devFlowElements/common.hpp"
#include "devMode/devFlowElements/devDynamicSeparator.hpp"
#include "devMode/devFlowElements/devHierarchy.hpp"
#include "devMode/devFlowElements/devPanelContentShared.hpp"
#include "devMode/devFlowElements/devProperties.hpp"

inline const DevPanelContentDef kDevPanelContent = {
	nullptr,
	nullptr,
	nullptr,
	nullptr,
	nullptr,
	nullptr,
	+[](DevPanelContentDef::BuildContext& context) {
		devPanelContentState& state = DevPanelContentDef::getOrCreateState(FlowUi::toFlowId(context.elementID));

		int separatorWidth = context.params.separatorThicknessPx;
		if (separatorWidth < 1)
		{
			separatorWidth = 1;
		}

		int minHierarchyWidth = context.params.minHierarchyWidthPx;
		if (minHierarchyWidth < 0)
		{
			minHierarchyWidth = 0;
		}

		int maxHierarchyWidth = context.params.maxHierarchyWidthPx;
		if (maxHierarchyWidth < minHierarchyWidth)
		{
			maxHierarchyWidth = minHierarchyWidth;
		}

		int minPropertiesWidth = context.params.minPropertiesWidthPx;
		if (minPropertiesWidth < 0)
		{
			minPropertiesWidth = 0;
		}

		const Clay_ElementId rootId = context.uiManager.toClayEID(context.elementID);
		const Clay_ElementData rootData = Clay_GetElementData(rootId);

		int panelWidthPx = state.lastPanelWidthPx;
		if (rootData.found)
		{
			panelWidthPx = static_cast<int>(std::lround(rootData.boundingBox.width));
		}
		if (panelWidthPx < 1)
		{
			panelWidthPx =
				std::max(1, context.params.defaultHierarchyWidthPx) +
				separatorWidth +
				std::max(1, minPropertiesWidth);
		}
		state.lastPanelWidthPx = panelWidthPx;

		int usableContentWidthPx = panelWidthPx - separatorWidth;
		if (usableContentWidthPx < 1)
		{
			usableContentWidthPx = 1;
		}

		int minAllowedHierarchyWidth = minHierarchyWidth;
		if (minAllowedHierarchyWidth > usableContentWidthPx)
		{
			minAllowedHierarchyWidth = usableContentWidthPx;
		}

		int maxAllowedHierarchyWidth = maxHierarchyWidth;
		if (maxAllowedHierarchyWidth > usableContentWidthPx)
		{
			maxAllowedHierarchyWidth = usableContentWidthPx;
		}

		const int maxAllowedByProperties = usableContentWidthPx - minPropertiesWidth;
		if (maxAllowedHierarchyWidth > maxAllowedByProperties)
		{
			maxAllowedHierarchyWidth = maxAllowedByProperties;
		}
		if (maxAllowedHierarchyWidth < minAllowedHierarchyWidth)
		{
			maxAllowedHierarchyWidth = minAllowedHierarchyWidth;
		}

		const auto clampHierarchyWidth = [&](int widthPx) -> int {
			if (widthPx < minAllowedHierarchyWidth)
			{
				return minAllowedHierarchyWidth;
			}
			if (widthPx > maxAllowedHierarchyWidth)
			{
				return maxAllowedHierarchyWidth;
			}
			return widthPx;
		};

		if (!state.hierarchySplitInitialized)
		{
			float initialRatio = context.params.defaultHierarchySplitRatio;
			if (!std::isfinite(initialRatio) || initialRatio <= 0.0f || initialRatio >= 1.0f)
			{
				initialRatio =
					(static_cast<float>(context.params.defaultHierarchyWidthPx) /
					static_cast<float>(usableContentWidthPx));
			}
			state.hierarchySplitRatio = initialRatio;
			state.hierarchySplitInitialized = true;
		}

		if (!std::isfinite(state.hierarchySplitRatio))
		{
			state.hierarchySplitRatio = context.params.defaultHierarchySplitRatio;
		}

		float minRatio = 0.0f;
		float maxRatio = 1.0f;
		if (usableContentWidthPx > 0)
		{
			minRatio = static_cast<float>(minAllowedHierarchyWidth) / static_cast<float>(usableContentWidthPx);
			maxRatio = static_cast<float>(maxAllowedHierarchyWidth) / static_cast<float>(usableContentWidthPx);
		}
		if (maxRatio < minRatio)
		{
			maxRatio = minRatio;
		}
		state.hierarchySplitRatio = std::clamp(state.hierarchySplitRatio, minRatio, maxRatio);

		int hierarchyWidthPx =
			static_cast<int>(std::lround(state.hierarchySplitRatio * static_cast<float>(usableContentWidthPx)));
		hierarchyWidthPx = clampHierarchyWidth(hierarchyWidthPx);
		state.hierarchySplitRatio = static_cast<float>(hierarchyWidthPx) / static_cast<float>(usableContentWidthPx);

		Clay_ElementDeclaration root{};
		root.id = rootId;
		root.layout.sizing = {
			.width = CLAY_SIZING_GROW(0),
			.height = CLAY_SIZING_GROW(0),
		};
		root.layout.layoutDirection = CLAY_LEFT_TO_RIGHT;
		root.layout.childGap = 0;
		root.backgroundColor = context.params.backgroundColor;

		CLAY(root){
			context.uiManager
				.createElement(kDevHierarchy, context.createChildElementId("hierarchy"))
				.setParameters(devHierarchyParams{
					.width = hierarchyWidthPx,
					.backgroundColor = context.params.hierarchyBackgroundColor,
				})
				.draw();

			devDynamicSeparatorParams separatorParams{};
			separatorParams.orientation = devDynamicSeparatorParams::Orientation::Vertical;
			separatorParams.reverseDrag = false;
			separatorParams.width = separatorWidth;
			separatorParams.height = 1;
			separatorParams.minValue = minAllowedHierarchyWidth;
			separatorParams.maxValue = maxAllowedHierarchyWidth;
			separatorParams.getValue = [hierarchyWidthPx]() {
				return hierarchyWidthPx;
			};
			separatorParams.setValue = [
				&state,
				usableContentWidthPx,
				minAllowedHierarchyWidth,
				maxAllowedHierarchyWidth
			](int nextWidth) {
				if (nextWidth < minAllowedHierarchyWidth)
				{
					nextWidth = minAllowedHierarchyWidth;
				}
				else if (nextWidth > maxAllowedHierarchyWidth)
				{
					nextWidth = maxAllowedHierarchyWidth;
				}
				if (usableContentWidthPx <= 0)
				{
					return;
				}
				state.hierarchySplitRatio =
					static_cast<float>(nextWidth) /
					static_cast<float>(usableContentWidthPx);
			};

			context.uiManager
				.createElement(kDevDynamicSeparator, context.createChildElementId("separator"))
				.setParameters(separatorParams)
				.draw();

			context.uiManager
				.createElement(kDevProperties, context.createChildElementId("properties"))
				.setParameters(devPropertiesParams{
					.backgroundColor = context.params.propertiesBackgroundColor,
					.selectedElementIdText =
						state.selectedElementId.empty()
						? std::string("placeholder")
						: state.selectedElementId,
				})
				.draw();
		};
	},
};
