#pragma once

#include "devMode/devFlowElements/common.hpp"

enum class devPropertiesSelectionKind : uint8_t {
	None = 0,
	Instance = 1,
	Definition = 2,
};

enum class devPropertiesStructScope : uint8_t {
	Parameters = 0,
	State = 1,
	Resources = 2,
};

struct devPropertiesSelectionNode {
	devPropertiesSelectionKind kind = devPropertiesSelectionKind::None;
	devPropertiesStructScope structScope = devPropertiesStructScope::Parameters;
	uint64_t definitionId = 0u;
	uint64_t definitionTypeHash = 0u;
	uint64_t flowId = 0u;
	std::string elementId{};
	std::string definitionDisplayName{};
	std::string definitionTypeToken{};
	std::string authoredInstanceKey{};
	std::string authoredDefinitionKey{};
	uint64_t sourceLocationHash = 0u;
};

inline bool isDevPropertiesSelectionNull(const devPropertiesSelectionNode& selection) {
	return selection.kind == devPropertiesSelectionKind::None || selection.definitionId == 0u;
}

inline devPropertiesSelectionNode makeDevPropertiesSelectionFromInstanceNode(
	const FlowUi::devMode::ElementTreePlaceholder::FlatNode& node) {
	devPropertiesSelectionNode selection{};
	selection.kind = devPropertiesSelectionKind::Instance;
	selection.structScope = devPropertiesStructScope::Parameters;
	selection.definitionId = node.definitionId;
	selection.definitionTypeHash = node.definitionTypeHash;
	selection.elementId = node.elementId;
	selection.flowId = (node.flowId != 0u) ? node.flowId : FlowUi::toFlowId(selection.elementId);
	selection.definitionDisplayName = node.definitionDisplayName;
	selection.definitionTypeToken = node.definitionTypeToken;
	selection.authoredInstanceKey = node.authoredInstanceKey;
	selection.authoredDefinitionKey = node.authoredDefinitionKey;
	selection.sourceLocationHash = node.sourceLocationHash;
	return selection;
}

inline devPropertiesSelectionNode makeDevPropertiesSelectionFromDefinitionDescriptor(
	const FlowUi::devMode::ElementDescriptor& descriptor,
	devPropertiesStructScope structScope) {
	devPropertiesSelectionNode selection{};
	selection.kind = devPropertiesSelectionKind::Definition;
	selection.structScope = structScope;
	selection.definitionId = descriptor.definitionId;
	selection.definitionTypeHash = descriptor.definitionTypeHash;
	selection.definitionDisplayName = descriptor.definitionName;
	selection.definitionTypeToken = descriptor.definitionTypeToken;
	selection.authoredDefinitionKey = descriptor.definitionName;
	return selection;
}

inline bool setSelectedDevPropertiesNode(const devPropertiesSelectionNode& selection);
