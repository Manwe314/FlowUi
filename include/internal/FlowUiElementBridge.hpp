#pragma once

#include <cstddef>
#include <cstdint>
#include <string_view>

#include "FlowUi/BuildConfig.hpp"
#include "FlowUi/ElementID.hpp"
namespace FlowUi {

class UiManager;

namespace detail {

#if FLOW_UI_DEV_MODE
// Dev-only bridge keeps the header-only typed builder decoupled from
// UiManager's runtime capture implementation.
void claimFlowRootForDev(
	UiManager& uiManager,
	FlowElementID elementId,
	FlowDefinitionID definitionId,
	std::string_view fileName,
	uint32_t line,
	uint32_t column,
	std::string_view functionName,
	bool automaticIdentity);

namespace devModeBridge {

std::size_t beginCapturedFlowElement(
	UiManager& uiManager,
	FlowDefinitionID definitionId,
	uint64_t definitionTypeHash,
	std::string_view definitionTypeToken,
	FlowElementID elementId,
	bool isInternalToDevMode);

bool endCapturedFlowElement(UiManager& uiManager);

} // namespace devModeBridge
#endif

} // namespace detail

} // namespace FlowUi
