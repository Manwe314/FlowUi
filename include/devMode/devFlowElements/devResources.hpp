#pragma once

#include "devMode/devFlowElements/common.hpp"
#include "devMode/devFlowElements/devEnum1Input.hpp"
#include "devMode/devFlowElements/devHeader.hpp"
#include "devMode/devFlowElements/devHierarchyContent.hpp"
#include "devMode/devFlowElements/devNineSplit.hpp"

#if FLOW_UI_DEV_MODE
namespace FlowUi::devMode {

inline void initializeDevFlowElementResourcesFromApp(App& app) {
	auto resources = elementSet(
		kDevHeader,
		kDevHierarchyContent,
		kDevEnum1Input,
		kDevNineSplit);
	app.elements().prepare(resources);
}

} // namespace FlowUi::devMode
#endif
