#include "FlowUi/Public/managers/UiManager.hpp"
#include "FlowUi/Private/layoutService/UiManagerState.hpp"
#if FLOW_UI_DEV_MODE
#include "devSystems/devMonitoringAndReporting/errors/DevError.hpp"
#endif

namespace FlowUi {
namespace detail {

#if FLOW_UI_DEV_MODE
	// Header-only builders enter the frame-local diagnostic tracker here.
	void claimFlowRootForDev(
		UiManager& uiManager,
		FlowElementID elementId,
		FlowDefinitionID definitionId,
		std::string_view fileName,
		uint32_t line,
		uint32_t column,
		std::string_view functionName,
		bool automaticIdentity) {
		const manager_storage::FlowRootClaimSourceForDev source{
			.definitionId = definitionId,
			.debugPath = std::string(elementId.debugName),
			.fileName = std::string(fileName),
			.functionName = std::string(functionName),
			.line = line,
			.column = column,
			.automaticIdentity = automaticIdentity,
		};
		const manager_storage::FlowRootCollisionForDev* collision =
			uiManager.state_->flowRootIdTracker.claim(
				detail::element::toInstanceKey(elementId),
				source);
		const manager_storage::ClayBridgeCollisionForDev* clayCollision =
			uiManager.state_->clayBridgeIdTracker.claim(
				detail::element::toInstanceKey(elementId),
				detail::element_id::toClayValue(elementId.value),
				source);

		if (collision && collision->first.automaticIdentity &&
			collision->duplicate.automaticIdentity) {
			static constexpr auto source =
				devSystems::makeDevErrorSource("flowui.ui.automatic_element_id_collision");
			devSystems::recordGlobalDevDiagnostic(
				makeError(
					ErrorCode::ElementDefinitionConflict,
					ErrorSite::UiManagerDefineElement,
					collision->instanceId.value,
					collision->duplicate.definitionId.value),
				source);
		} else if (collision) {
			static constexpr auto source =
				devSystems::makeDevErrorSource("flowui.ui.flow_root_collision");
			devSystems::recordGlobalDevDiagnostic(
				makeError(
					ErrorCode::ElementDefinitionConflict,
					ErrorSite::UiManagerDefineElement,
					collision->instanceId.value,
					collision->duplicate.definitionId.value),
				source);
		}

		if (clayCollision) {
			static constexpr auto source =
				devSystems::makeDevErrorSource("flowui.ui.clay_id_alias");
			devSystems::recordGlobalDevDiagnostic(
				makeError(
					ErrorCode::ElementDefinitionConflict,
					ErrorSite::UiManagerDefineElement,
					clayCollision->first.instanceId.value,
					clayCollision->duplicate.instanceId.value),
				source);
		}
	}

	namespace dev_tooling_bridge {

	devSystems::tooling::DevTreeCapture::Token
	beginCapturedFlowElement(UiManager& uiManager, FlowDefinitionID definitionId,
							 std::string_view definition_name, std::string_view definitionTypeToken,
							 FlowElementID elementId, bool is_internal_dev_element,
							 bool constructed, std::string_view sourceFile, uint32_t sourceLine,
							 uint32_t sourceColumn, std::string_view sourceFunction) noexcept {
		return uiManager.devTreeCapture().beginFlow({
			.definition = definitionId,
			.instance = elementId,
			.definitionName = definition_name,
			.definitionTypeToken = definitionTypeToken,
			.sourceFile = sourceFile,
			.sourceFunction = sourceFunction,
			.sourceLine = sourceLine,
			.sourceColumn = sourceColumn,
			.constructed = constructed,
			.internalDev = is_internal_dev_element,
			.suppress = is_internal_dev_element &&
						uiManager.devToolsConfig().excludeInternalDevElementsFromCapture,
		});
	}

	void endCapturedFlowElement(
		UiManager& uiManager,
		devSystems::tooling::DevTreeCapture::Token token,
		bool autoClosed) noexcept {
		uiManager.devTreeCapture().endFlow(token, autoClosed);
	}

	} // namespace dev_tooling_bridge
#endif

} // namespace detail
} // namespace FlowUi
