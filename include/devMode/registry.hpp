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

namespace FlowUi::devMode {

template <typename Owner, typename Member>
struct FieldInfo;

template <typename T>
constexpr std::string_view typeToken() {
#if defined(__clang__) || defined(__GNUC__)
	return __PRETTY_FUNCTION__;
#elif defined(_MSC_VER)
	return __FUNCSIG__;
#else
	return "FlowUi::devMode::typeToken<unknown>()";
#endif
}

constexpr uint64_t hashString64(std::string_view text) {
	uint64_t hash = 14695981039346656037ull;
	for (const char c : text) {
		hash ^= static_cast<uint64_t>(static_cast<unsigned char>(c));
		hash *= 1099511628211ull;
	}
	return (hash == 0ull) ? 1ull : hash;
}

template <typename T>
constexpr uint64_t typeHash() {
	return hashString64(typeToken<T>());
}

struct FieldDescriptor {
	std::string name{};
	uint64_t fieldHash = 0u;
	uint64_t ownerTypeHash = 0u;
	uint64_t fieldTypeHash = 0u;
	std::string ownerTypeToken{};
	std::string fieldTypeToken{};
	std::size_t fieldTypeSize = 0u;
	std::vector<std::byte> memberPointerBytes{};
};

struct StructDescriptor {
	uint64_t typeHash = 0u;
	std::string name{};
	std::string typeToken{};
	std::size_t size = 0u;
	std::vector<FieldDescriptor> fields{};
};

struct ElementDescriptor {
	uint64_t definitionTypeHash = 0u;
	uint64_t definitionId = 0u;
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
		const uint64_t structTypeHash = typeHash<StructT>();
		const auto existingIt = structIndexByTypeHash_.find(structTypeHash);
		if (existingIt != structIndexByTypeHash_.end()) {
			return;
		}

		StructDescriptor descriptor{};
		descriptor.typeHash = structTypeHash;
		descriptor.name = std::string(structName);
		descriptor.typeToken = std::string(typeToken<StructT>());
		descriptor.size = sizeof(StructT);
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
		descriptor.paramsStructTypeHash = typeHash<typename DefinitionT::ParametersType>();
		descriptor.stateStructTypeHash = typeHash<typename DefinitionT::StateType>();
		descriptor.resourcesStructTypeHash = typeHash<typename DefinitionT::ResourcesType>();

		const std::size_t index = elements_.size();
		elementIndexByDefinitionTypeHash_.emplace(descriptor.definitionTypeHash, index);
		elementIndexByDefinitionId_.emplace(descriptor.definitionId, index);
		elementIndexByName_.emplace(descriptor.definitionName, index);
		elements_.push_back(std::move(descriptor));
	}

	const std::vector<StructDescriptor>& getStructs() const { return structs_; }
	const std::vector<ElementDescriptor>& getElements() const { return elements_; }

	std::size_t structCount() const { return structs_.size(); }
	std::size_t elementCount() const { return elements_.size(); }

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

	const ElementDescriptor* findElementByDefinitionId(uint64_t value) const {
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

private:
	std::vector<StructDescriptor> structs_{};
	std::vector<ElementDescriptor> elements_{};
	std::unordered_map<uint64_t, std::size_t> structIndexByTypeHash_{};
	std::unordered_map<std::string, std::size_t> structIndexByName_{};
	std::unordered_map<uint64_t, std::size_t> elementIndexByDefinitionTypeHash_{};
	std::unordered_map<uint64_t, std::size_t> elementIndexByDefinitionId_{};
	std::unordered_map<std::string, std::size_t> elementIndexByName_{};
};

template <typename StructT, typename... FieldTs>
inline void registerStructSchema(std::string_view structName, FieldTs&&... fields) {
	DevRegistry::instance().template registerStruct<StructT>(structName, std::forward<FieldTs>(fields)...);
}

template <typename DefinitionT>
inline void registerElementSchema(std::string_view definitionName) {
	DevRegistry::instance().template registerElement<DefinitionT>(definitionName);
}

struct DevRegistrar {
	template <typename Fn>
	explicit DevRegistrar(Fn&& fn) {
		fn();
	}
};

} // namespace FlowUi::devMode
