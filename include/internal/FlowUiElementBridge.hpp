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

namespace element {
struct ElementRegistrationDescriptor;
}

Clay_ElementId toClayElementId(UiManager& uiManager, std::string_view elementID);
const InteractionSnapshot& previousInteraction(const UiManager& uiManager);
void pushConstructedElement(UiManager& uiManager, Clay_ElementId elementId);
// transitional: temporary bridge lets the current header-defined ElementBuilder register normalized metadata without depending on UiManager's final concept-based element surface.
void ensureElementDefinitionRegistered(
	UiManager& uiManager,
	const element::ElementRegistrationDescriptor& descriptor);

#if FLOW_UI_DEV_MODE
// transitional: temporary dev-only claim bridge exists until concept-based element dispatch owns the Flow-root claim directly.
void claimFlowRootForDev(
	UiManager& uiManager,
	uint64_t flowId,
	uint64_t definitionId,
	std::string_view logicalId,
	std::string_view fileName,
	uint32_t line,
	uint32_t column,
	std::string_view functionName);

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
