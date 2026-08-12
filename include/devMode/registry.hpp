#pragma once

#include <cstddef>
#include <cstdint>
#include <cstring>
#include <string>
#include <string_view>
#include <type_traits>
#include <unordered_map>
#include <utility>
#include <vector>

#include "FlowUi/BuildConfig.hpp"
#include "devMode/devRuntime.hpp"
#include "internal/TypeOperations.hpp"
#include "managers/structs/FlowUiElementConcepts.hpp"

namespace FlowUi::devMode {

template <typename Owner, typename Member>
struct FieldInfo;

using FlowUi::detail::typeToken;
using FlowUi::detail::hashString64;
using FlowUi::detail::typeHash;

struct DevStructSourceMetadata {
	std::string sourceFile{};
	uint32_t sourceLine = 0u;
	uint32_t sourceColumn = 0u;
	std::string sourceFunction{};
	bool hasSourceMetadata = false;
};

inline DevStructSourceMetadata makeDevStructSourceMetadata(
	std::string_view sourceFile,
	uint32_t sourceLine,
	uint32_t sourceColumn = 0u,
	std::string_view sourceFunction = {}) {
	DevStructSourceMetadata metadata{};
	metadata.hasSourceMetadata = !sourceFile.empty() && sourceLine != 0u;
	if (!metadata.hasSourceMetadata) {
		return metadata;
	}

	metadata.sourceFile = std::string(sourceFile);
	metadata.sourceLine = sourceLine;
	metadata.sourceColumn = sourceColumn;
	metadata.sourceFunction = std::string(sourceFunction);
	return metadata;
}

template <typename EnumT>
struct EnumValueInfo {
	const char* name = "";
	EnumT value{};
};

template <typename EnumT>
constexpr EnumValueInfo<EnumT> makeEnumValueInfo(const char* name, EnumT value) {
	return EnumValueInfo<EnumT>{
		name,
		value,
	};
}

struct EnumValueDescriptor {
	uint8_t value = 0u;
	std::string name{};
};

struct EnumDescriptor {
	uint64_t typeHash = 0u;
	std::string name{};
	std::string typeToken{};
	std::vector<EnumValueDescriptor> values{};
};

struct FieldDescriptor;

using FieldCaptureFunction = bool(*)(const void* owner, const FieldDescriptor& field, DevValue& outValue);
using FieldApplyFunction = bool(*)(void* owner, const FieldDescriptor& field, const DevValue& value);

struct FieldDescriptor {
	std::string name{};
	uint64_t fieldHash = 0u;
	uint64_t ownerTypeHash = 0u;
	uint64_t fieldTypeHash = 0u;
	std::string ownerTypeToken{};
	std::string fieldTypeToken{};
	std::size_t fieldTypeSize = 0u;
	std::vector<std::byte> memberPointerBytes{};
	FieldCaptureFunction captureFieldFunction = nullptr;
	FieldApplyFunction applyFieldFunction = nullptr;
};

struct StructDescriptor {
	uint64_t typeHash = 0u;
	std::string name{};
	std::string typeToken{};
	std::size_t size = 0u;
	std::string sourceFile{};
	uint32_t sourceLine = 0u;
	uint32_t sourceColumn = 0u;
	std::string sourceFunction{};
	bool hasSourceMetadata = false;
	std::vector<FieldDescriptor> fields{};
};

struct ElementDescriptor {
	uint64_t definitionTypeHash = 0u;
	FlowDefinitionID definitionId{};
	std::string definitionName{};
	std::string definitionTypeToken{};
	uint64_t paramsStructTypeHash = 0u;
	uint64_t stateStructTypeHash = 0u;
	uint64_t resourcesStructTypeHash = 0u;
};

class DevRegistry {
public:
	static DevRegistry& instance() {
		static DevRegistry registry{};
		return registry;
	}

	template <typename StructT, typename... FieldTs>
	void registerStruct(std::string_view structName, FieldTs&&... fields) {
		registerStructWithSource<StructT>(
			structName,
			DevStructSourceMetadata{},
			std::forward<FieldTs>(fields)...);
	}

	template <typename StructT, typename... FieldTs>
	void registerStructWithSource(
		std::string_view structName,
		const DevStructSourceMetadata& sourceMetadata,
		FieldTs&&... fields) {
		const uint64_t structTypeHash = typeHash<StructT>();
		const auto existingIt = structIndexByTypeHash_.find(structTypeHash);
		if (existingIt != structIndexByTypeHash_.end()) {
			if (sourceMetadata.hasSourceMetadata) {
				StructDescriptor& existing = structs_[existingIt->second];
				if (!existing.hasSourceMetadata) {
					applyStructSourceMetadata(existing, sourceMetadata);
				}
			}
			return;
		}

		StructDescriptor descriptor{};
		descriptor.typeHash = structTypeHash;
		descriptor.name = std::string(structName);
		descriptor.typeToken = std::string(typeToken<StructT>());
		descriptor.size = sizeof(StructT);
		applyStructSourceMetadata(descriptor, sourceMetadata);
		descriptor.fields.reserve(sizeof...(fields));
		if constexpr (sizeof...(fields) > 0) {
			(appendField<StructT>(descriptor.fields, std::forward<FieldTs>(fields)), ...);
		}

		const std::size_t index = structs_.size();
		structIndexByTypeHash_.emplace(descriptor.typeHash, index);
		structIndexByName_.emplace(descriptor.name, index);
		structs_.push_back(std::move(descriptor));
	}

	template <typename DefinitionT>
	void registerElement(std::string_view definitionName) {
		static_assert(
			FlowUi::FlowElement<DefinitionT>,
			"FlowUi dev registry requires a valid Flow element definition type.");
		const uint64_t definitionTypeHash = typeHash<DefinitionT>();
		const auto existingIt = elementIndexByDefinitionTypeHash_.find(definitionTypeHash);
		if (existingIt != elementIndexByDefinitionTypeHash_.end()) {
			return;
		}

		ElementDescriptor descriptor{};
		descriptor.definitionTypeHash = definitionTypeHash;
		descriptor.definitionId = DefinitionT::definitionId;
		descriptor.definitionName = std::string(definitionName);
		descriptor.definitionTypeToken = std::string(typeToken<DefinitionT>());
		descriptor.paramsStructTypeHash = typeHash<FlowUi::ParametersOf<DefinitionT>>();
		if constexpr (FlowUi::HasState<DefinitionT>) {
			descriptor.stateStructTypeHash = typeHash<FlowUi::StateOf<DefinitionT>>();
		}
		if constexpr (FlowUi::HasResources<DefinitionT>) {
			descriptor.resourcesStructTypeHash = typeHash<FlowUi::ResourcesOf<DefinitionT>>();
		}

		const std::size_t index = elements_.size();
		elementIndexByDefinitionTypeHash_.emplace(descriptor.definitionTypeHash, index);
		elementIndexByDefinitionId_.emplace(descriptor.definitionId, index);
		elementIndexByName_.emplace(descriptor.definitionName, index);
		elements_.push_back(std::move(descriptor));
	}

	template <typename EnumT, typename... EnumValueTs>
	void registerEnum(std::string_view enumName, EnumValueTs&&... values) {
		static_assert(std::is_enum_v<EnumT>, "FLOWUI_DEV_REGISTER_ENUM(...) only supports enum types.");
		static_assert(
			std::is_same_v<std::underlying_type_t<EnumT>, uint8_t>,
			"FLOWUI_DEV_REGISTER_ENUM(...) currently supports only enum types backed by uint8_t.");

		const uint64_t enumTypeHash = typeHash<EnumT>();
		const auto existingIt = enumIndexByTypeHash_.find(enumTypeHash);
		if (existingIt != enumIndexByTypeHash_.end()) {
			return;
		}

		EnumDescriptor descriptor{};
		descriptor.typeHash = enumTypeHash;
		descriptor.name = std::string(enumName);
		descriptor.typeToken = std::string(typeToken<EnumT>());
		descriptor.values.reserve(sizeof...(values));
		if constexpr (sizeof...(values) > 0) {
			(appendEnumValue<EnumT>(descriptor.values, std::forward<EnumValueTs>(values)), ...);
		}

		const std::size_t index = enums_.size();
		enumIndexByTypeHash_.emplace(descriptor.typeHash, index);
		enumIndexByName_.emplace(descriptor.name, index);
		enums_.push_back(std::move(descriptor));
	}

	const std::vector<StructDescriptor>& getStructs() const { return structs_; }
	const std::vector<ElementDescriptor>& getElements() const { return elements_; }
	const std::vector<EnumDescriptor>& getEnums() const { return enums_; }

	std::size_t structCount() const { return structs_.size(); }
	std::size_t elementCount() const { return elements_.size(); }
	std::size_t enumCount() const { return enums_.size(); }

	const StructDescriptor* findStructByTypeHash(uint64_t value) const {
		const auto it = structIndexByTypeHash_.find(value);
		if (it == structIndexByTypeHash_.end()) {
			return nullptr;
		}
		return &structs_[it->second];
	}

	const StructDescriptor* findStructByName(std::string_view value) const {
		const auto it = structIndexByName_.find(std::string(value));
		if (it == structIndexByName_.end()) {
			return nullptr;
		}
		return &structs_[it->second];
	}

	const FieldDescriptor* findFieldByName(const StructDescriptor& structure, std::string_view fieldName) const {
		for (const FieldDescriptor& field : structure.fields) {
			if (field.name == fieldName) {
				return &field;
			}
		}
		return nullptr;
	}

	template <typename StructT>
	const StructDescriptor* findStruct() const {
		return findStructByTypeHash(typeHash<StructT>());
	}

	const ElementDescriptor* findElementByDefinitionTypeHash(uint64_t value) const {
		const auto it = elementIndexByDefinitionTypeHash_.find(value);
		if (it == elementIndexByDefinitionTypeHash_.end()) {
			return nullptr;
		}
		return &elements_[it->second];
	}

	const ElementDescriptor* findElementByDefinitionId(FlowDefinitionID value) const {
		const auto it = elementIndexByDefinitionId_.find(value);
		if (it == elementIndexByDefinitionId_.end()) {
			return nullptr;
		}
		return &elements_[it->second];
	}

	const ElementDescriptor* findElementByName(std::string_view value) const {
		const auto it = elementIndexByName_.find(std::string(value));
		if (it == elementIndexByName_.end()) {
			return nullptr;
		}
		return &elements_[it->second];
	}

	template <typename DefinitionT>
	const ElementDescriptor* findElement() const {
		return findElementByDefinitionTypeHash(typeHash<DefinitionT>());
	}

	const EnumDescriptor* findEnumByTypeHash(uint64_t value) const {
		const auto it = enumIndexByTypeHash_.find(value);
		if (it == enumIndexByTypeHash_.end()) {
			return nullptr;
		}
		return &enums_[it->second];
	}

	const EnumDescriptor* findEnumByName(std::string_view value) const {
		const auto it = enumIndexByName_.find(std::string(value));
		if (it == enumIndexByName_.end()) {
			return nullptr;
		}
		return &enums_[it->second];
	}

	bool isEnumTypeHash(uint64_t value) const {
		return findEnumByTypeHash(value) != nullptr;
	}

	bool tryEnumValueToName(uint64_t enumTypeHash, uint8_t numeric, std::string_view& outName) const {
		const EnumDescriptor* descriptor = findEnumByTypeHash(enumTypeHash);
		if (descriptor == nullptr) {
			return false;
		}

		for (const EnumValueDescriptor& value : descriptor->values) {
			if (value.value == numeric) {
				outName = value.name;
				return true;
			}
		}
		return false;
	}

	bool tryEnumNameToValue(uint64_t enumTypeHash, std::string_view name, uint8_t& outNumeric) const {
		const EnumDescriptor* descriptor = findEnumByTypeHash(enumTypeHash);
		if (descriptor == nullptr) {
			return false;
		}

		for (const EnumValueDescriptor& value : descriptor->values) {
			if (value.name == name) {
				outNumeric = value.value;
				return true;
			}
		}
		return false;
	}

	const StructDescriptor* findParamsStruct(const ElementDescriptor& element) const {
		return findStructByTypeHash(element.paramsStructTypeHash);
	}

	const StructDescriptor* findStateStruct(const ElementDescriptor& element) const {
		return findStructByTypeHash(element.stateStructTypeHash);
	}

	const StructDescriptor* findResourcesStruct(const ElementDescriptor& element) const {
		return findStructByTypeHash(element.resourcesStructTypeHash);
	}

	template <typename OwnerT, typename MemberT>
	static bool tryGetMemberPointer(const FieldDescriptor& field, MemberT OwnerT::*& outMemberPointer) {
		if (field.ownerTypeHash != typeHash<OwnerT>() || field.fieldTypeHash != typeHash<MemberT>()) {
			return false;
		}
		if (field.memberPointerBytes.size() != sizeof(outMemberPointer)) {
			return false;
		}

		std::memcpy(&outMemberPointer, field.memberPointerBytes.data(), sizeof(outMemberPointer));
		return true;
	}

	void beginFrame() {}
	void endFrame() {}

private:
	static void applyStructSourceMetadata(
		StructDescriptor& destination,
		const DevStructSourceMetadata& sourceMetadata) {
		if (!sourceMetadata.hasSourceMetadata) {
			return;
		}

		destination.sourceFile = sourceMetadata.sourceFile;
		destination.sourceLine = sourceMetadata.sourceLine;
		destination.sourceColumn = sourceMetadata.sourceColumn;
		destination.sourceFunction = sourceMetadata.sourceFunction;
		destination.hasSourceMetadata = true;
	}

	template <typename OwnerT, typename MemberT>
	static FieldDescriptor buildFieldDescriptor(const FieldInfo<OwnerT, MemberT>& fieldInfo) {
		FieldDescriptor descriptor{};
		descriptor.name = fieldInfo.name ? fieldInfo.name : "";
		descriptor.fieldHash = hashString64(descriptor.name);
		descriptor.ownerTypeHash = typeHash<OwnerT>();
		descriptor.fieldTypeHash = typeHash<MemberT>();
		descriptor.ownerTypeToken = std::string(typeToken<OwnerT>());
		descriptor.fieldTypeToken = std::string(typeToken<MemberT>());
		descriptor.fieldTypeSize = sizeof(MemberT);

		descriptor.memberPointerBytes.resize(sizeof(fieldInfo.member));
		std::memcpy(descriptor.memberPointerBytes.data(), &fieldInfo.member, sizeof(fieldInfo.member));
		descriptor.captureFieldFunction = fieldInfo.captureFieldFunction;
		descriptor.applyFieldFunction = fieldInfo.applyFieldFunction;
		return descriptor;
	}

	template <typename StructT, typename OwnerT, typename MemberT>
	static void appendField(
		std::vector<FieldDescriptor>& destination,
		const FieldInfo<OwnerT, MemberT>& fieldInfo) {
		static_assert(
			std::is_same_v<std::remove_cv_t<StructT>, std::remove_cv_t<OwnerT>>,
			"FLOWUI_DEV_REGISTER_STRUCT(...) received a field that does not belong to the registered struct type.");
		destination.push_back(buildFieldDescriptor<OwnerT, MemberT>(fieldInfo));
	}

	template <typename EnumT>
	static void appendEnumValue(
		std::vector<EnumValueDescriptor>& destination,
		const EnumValueInfo<EnumT>& enumValueInfo) {
		static_assert(std::is_enum_v<EnumT>, "FLOWUI_DEV_REGISTER_ENUM(...) only supports enum values.");
		static_assert(
			std::is_same_v<std::underlying_type_t<EnumT>, uint8_t>,
			"FLOWUI_DEV_REGISTER_ENUM(...) currently supports only enum values backed by uint8_t.");

		EnumValueDescriptor descriptor{};
		descriptor.value = static_cast<uint8_t>(enumValueInfo.value);
		descriptor.name = enumValueInfo.name ? enumValueInfo.name : "";
		if (descriptor.name.empty()) {
			return;
		}
		destination.push_back(std::move(descriptor));
	}

private:
	std::vector<StructDescriptor> structs_{};
	std::vector<ElementDescriptor> elements_{};
	std::vector<EnumDescriptor> enums_{};
	std::unordered_map<uint64_t, std::size_t> structIndexByTypeHash_{};
	std::unordered_map<std::string, std::size_t> structIndexByName_{};
	std::unordered_map<uint64_t, std::size_t> elementIndexByDefinitionTypeHash_{};
	std::unordered_map<FlowDefinitionID, std::size_t, FlowDefinitionIDHash>
		elementIndexByDefinitionId_{};
	std::unordered_map<std::string, std::size_t> elementIndexByName_{};
	std::unordered_map<uint64_t, std::size_t> enumIndexByTypeHash_{};
	std::unordered_map<std::string, std::size_t> enumIndexByName_{};
};

struct RegistryRegistrar {
	template <typename Fn>
	explicit RegistryRegistrar(Fn&& fn) {
		fn();
	}
};

} // namespace FlowUi::devMode
