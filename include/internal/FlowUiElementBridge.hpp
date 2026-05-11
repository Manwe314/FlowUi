#pragma once

#include <cstddef>
#include <cstdint>
#include <string_view>

#include "FlowUi/BuildConfig.hpp"
#include "clay.h"

namespace FlowUi {

struct InteractionSnapshot;
class UiManager;

namespace detail {

Clay_ElementId toClayElementId(UiManager& uiManager, std::string_view elementID);
const InteractionSnapshot& previousInteraction(const UiManager& uiManager);
void pushConstructedElement(UiManager& uiManager, Clay_ElementId elementId);

#if FLOW_UI_DEV_MODE
namespace devModeBridge {

std::size_t beginCapturedFlowElement(
	UiManager& uiManager,
	uint64_t definitionId,
	uint64_t definitionTypeHash,
	std::string_view definitionTypeToken,
	std::string_view elementID,
	uint64_t flowId,
	bool isInternalToDevMode);

bool endCapturedFlowElement(UiManager& uiManager);

} // namespace devModeBridge
#endif

} // namespace detail

} // namespace FlowUi
