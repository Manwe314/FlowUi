#pragma once

#include <cstddef>
#include <cstdint>
#include <string_view>

#include "FlowUi/BuildConfig.hpp"
namespace FlowUi {

class UiManager;

namespace detail {

#if FLOW_UI_DEV_MODE
// transitional: temporary dev-only bridges remain until the later dev element
// and registry migration consolidates capture operations on UiManager.
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
