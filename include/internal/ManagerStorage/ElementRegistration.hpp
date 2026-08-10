#pragma once

#include <cstddef>
#include <cstdint>
#include <string_view>

#include "FlowUi/App.hpp"

namespace FlowUi::detail::element {

struct ElementTypeOperations {
	void (*defaultConstruct)(void* destination) = nullptr;
	void (*constructWithApp)(void* destination, App& app) = nullptr;
	void (*destroy)(void* object) noexcept = nullptr;
};

struct ElementRegistrationDescriptor {
	FlowDefinitionId definitionId = 0;
	uint64_t definitionTypeHash = 0;
	uint64_t parametersTypeHash = 0;
	uint64_t stateTypeHash = 0;
	uint64_t resourcesTypeHash = 0;
	size_t parametersSize = 0;
	size_t parametersAlignment = 0;
	size_t stateSize = 0;
	size_t stateAlignment = 0;
	size_t resourcesSize = 0;
	size_t resourcesAlignment = 0;
	bool hasState = false;
	bool hasResources = false;
	ElementTypeOperations stateOperations{};
	ElementTypeOperations resourceOperations{};
	std::string_view debugName{};
	std::string_view definitionTypeName{};
	std::string_view parametersTypeName{};
	std::string_view stateTypeName{};
	std::string_view resourcesTypeName{};
};

} // namespace FlowUi::detail::element
