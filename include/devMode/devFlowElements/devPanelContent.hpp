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

		if (!state.hierarchyWidthInitialized)
		{
			state.hierarchyWidthPx = context.params.defaultHierarchyWidthPx;
			if (state.hierarchyWidthPx < minHierarchyWidth)
			{
				state.hierarchyWidthPx = minHierarchyWidth;
			}
			else if (state.hierarchyWidthPx > maxHierarchyWidth)
			{
				state.hierarchyWidthPx = maxHierarchyWidth;
			}
			state.hierarchyWidthInitialized = true;
		}
		else
		{
			if (state.hierarchyWidthPx < minHierarchyWidth)
			{
				state.hierarchyWidthPx = minHierarchyWidth;
			}
			else if (state.hierarchyWidthPx > maxHierarchyWidth)
			{
				state.hierarchyWidthPx = maxHierarchyWidth;
			}
		}

		Clay_ElementDeclaration root{};
		root.id = context.uiManager.toClayEID(context.elementID);
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
					.width = state.hierarchyWidthPx,
					.backgroundColor = context.params.hierarchyBackgroundColor,
				})
				.draw();

			devDynamicSeparatorParams separatorParams{};
			separatorParams.orientation = devDynamicSeparatorParams::Orientation::Vertical;
			separatorParams.reverseDrag = false;
			separatorParams.width = (context.params.separatorThicknessPx < 1) ? 1 : context.params.separatorThicknessPx;
			separatorParams.height = 1;
			separatorParams.minValue = minHierarchyWidth;
			separatorParams.maxValue = maxHierarchyWidth;
			separatorParams.getValue = [&state]() {
				return state.hierarchyWidthPx;
			};
			separatorParams.setValue = [&state, minHierarchyWidth, maxHierarchyWidth](int nextWidth) {
				if (nextWidth < minHierarchyWidth)
				{
					nextWidth = minHierarchyWidth;
				}
				else if (nextWidth > maxHierarchyWidth)
				{
					nextWidth = maxHierarchyWidth;
				}
				state.hierarchyWidthPx = nextWidth;
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
