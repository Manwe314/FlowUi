#pragma once

#include <cmath>
#include <cstdint>
#include <limits>
#include <string>
#include <type_traits>
#include <utility>

#include "clay.h"
#include "FlowUi/BuildConfig.hpp"
#include "devMode/registry.hpp"
#include "devMode/devTypes/devEnum1.hpp"
#include "devMode/devTypes/devEnum2.hpp"
#include "devMode/devTypes/devFloat1.hpp"
#include "devMode/devTypes/devFloat2.hpp"
#include "devMode/devTypes/devFloat4.hpp"
#include "devMode/devTypes/devEdgeU16.hpp"
#include "devMode/devTypes/devTaggedUnion.hpp"
#include "devMode/devTypes/devCompositeStruct.hpp"

namespace FlowUi::devMode {

namespace detail {

template <typename T>
bool tryAssignIntegralFromInt64(int64_t value, T& out) {
	if constexpr (!std::is_integral_v<T> || std::is_same_v<T, bool>) {
		return false;
	} else if constexpr (std::is_signed_v<T>) {
		if (value < static_cast<int64_t>(std::numeric_limits<T>::min()) ||
			value > static_cast<int64_t>(std::numeric_limits<T>::max())) {
			return false;
		}
		out = static_cast<T>(value);
		return true;
	} else {
		if (value < 0) {
			return false;
		}
		const uint64_t unsignedValue = static_cast<uint64_t>(value);
		if (unsignedValue > static_cast<uint64_t>(std::numeric_limits<T>::max())) {
			return false;
		}
		out = static_cast<T>(unsignedValue);
		return true;
	}
}

template <typename T>
bool tryAssignFromDevValue(const DevValue& value, T& out) {
	if constexpr (std::is_same_v<T, Clay_ChildAlignment>) {
		if (const auto* enumValue = std::get_if<DevEnum2Value>(&value)) {
			return tryApplyDevEnum2Value(*enumValue, out);
		}
		return false;
	} else if constexpr (std::is_same_v<T, Clay_FloatingAttachPoints>) {
		if (const auto* enumValue = std::get_if<DevEnum2Value>(&value)) {
			return tryApplyDevEnum2Value(*enumValue, out);
		}
		return false;
	} else if constexpr (std::is_same_v<T, Clay_Vector2>) {
		if (const auto* floatValue = std::get_if<DevFloat2Value>(&value)) {
			return tryApplyDevFloat2Value(*floatValue, out);
		}
		return false;
	} else if constexpr (std::is_same_v<T, Clay_Dimensions>) {
		if (const auto* floatValue = std::get_if<DevFloat2Value>(&value)) {
			return tryApplyDevFloat2Value(*floatValue, out);
		}
		return false;
	} else if constexpr (std::is_same_v<T, Clay_SizingMinMax>) {
		if (const auto* floatValue = std::get_if<DevFloat2Value>(&value)) {
			return tryApplyDevFloat2Value(*floatValue, out);
		}
		return false;
	} else if constexpr (std::is_same_v<T, Clay_Color>) {
		if (const auto* floatValue = std::get_if<DevFloat4Value>(&value)) {
			return tryApplyDevFloat4Value(*floatValue, out);
		}
		return false;
	} else if constexpr (std::is_same_v<T, Clay_CornerRadius>) {
		if (const auto* floatValue = std::get_if<DevFloat4Value>(&value)) {
			return tryApplyDevFloat4Value(*floatValue, out);
		}
		return false;
	} else if constexpr (std::is_same_v<T, Clay_AspectRatioElementConfig>) {
		double numeric = 0.0;
		if (!tryAssignFromDevValue(value, numeric)) {
			return false;
		}
		return tryApplyDevFloat1Value(numeric, out);
	} else if constexpr (std::is_same_v<T, Clay_SizingAxis>) {
		if (const auto* taggedUnionValue = std::get_if<DevTaggedUnionValue>(&value)) {
			return tryApplyDevTaggedUnionValue(*taggedUnionValue, out);
		}
		return false;
	} else if constexpr (std::is_same_v<T, Clay_Sizing>) {
		if (const auto* compositeValue = std::get_if<DevCompositeStructValue>(&value)) {
			return tryApplyDevCompositeStructValue(*compositeValue, out);
		}
		return false;
	} else if constexpr (std::is_same_v<T, Clay_LayoutConfig>) {
		if (const auto* compositeValue = std::get_if<DevCompositeStructValue>(&value)) {
			return tryApplyDevCompositeStructValue(*compositeValue, out);
		}
		return false;
	} else if constexpr (std::is_same_v<T, Clay_TextElementConfig>) {
		if (const auto* compositeValue = std::get_if<DevCompositeStructValue>(&value)) {
			return tryApplyDevCompositeStructValue(*compositeValue, out);
		}
		return false;
	} else if constexpr (std::is_same_v<T, Clay_FloatingElementConfig>) {
		if (const auto* compositeValue = std::get_if<DevCompositeStructValue>(&value)) {
			return tryApplyDevCompositeStructValue(*compositeValue, out);
		}
		return false;
	} else if constexpr (std::is_same_v<T, Clay_ClipElementConfig>) {
		if (const auto* compositeValue = std::get_if<DevCompositeStructValue>(&value)) {
			return tryApplyDevCompositeStructValue(*compositeValue, out);
		}
		return false;
	} else if constexpr (std::is_same_v<T, Clay_BorderElementConfig>) {
		if (const auto* compositeValue = std::get_if<DevCompositeStructValue>(&value)) {
			return tryApplyDevCompositeStructValue(*compositeValue, out);
		}
		return false;
	} else if constexpr (std::is_same_v<T, Clay_ElementDeclaration>) {
		if (const auto* compositeValue = std::get_if<DevCompositeStructValue>(&value)) {
			return tryApplyDevCompositeStructValue(*compositeValue, out);
		}
		return false;
	} else if constexpr (std::is_same_v<T, Clay_Padding>) {
		if (const auto* edgeValue = std::get_if<DevEdgeU16Value>(&value)) {
			return tryApplyDevEdgeU16Value(*edgeValue, out);
		}
		return false;
	} else if constexpr (std::is_same_v<T, Clay_BorderWidth>) {
		if (const auto* edgeValue = std::get_if<DevEdgeU16Value>(&value)) {
			return tryApplyDevEdgeU16Value(*edgeValue, out);
		}
		return false;
	} else if constexpr (std::is_enum_v<T>) {
		using Underlying = std::underlying_type_t<T>;
		Underlying underlying{};
		if (!tryAssignFromDevValue(value, underlying)) {
			return false;
		}
		out = static_cast<T>(underlying);
		return true;
	} else if constexpr (std::is_same_v<T, bool>) {
		if (const auto* boolValue = std::get_if<bool>(&value)) {
			out = *boolValue;
			return true;
		}
		if (const auto* intValue = std::get_if<int64_t>(&value)) {
			out = (*intValue != 0);
			return true;
		}
		if (const auto* doubleValue = std::get_if<double>(&value)) {
			if (!std::isfinite(*doubleValue)) {
				return false;
			}
			out = (*doubleValue != 0.0);
			return true;
		}
		return false;
	} else if constexpr (std::is_integral_v<T>) {
		if (const auto* enumValue = std::get_if<DevEnum1Value>(&value)) {
			return tryAssignIntegralFromInt64(static_cast<int64_t>(enumValue->numeric), out);
		}
		if (const auto* intValue = std::get_if<int64_t>(&value)) {
			return tryAssignIntegralFromInt64(*intValue, out);
		}
		if (const auto* doubleValue = std::get_if<double>(&value)) {
			if (!std::isfinite(*doubleValue) || std::trunc(*doubleValue) != *doubleValue) {
				return false;
			}
			if (*doubleValue < static_cast<double>(std::numeric_limits<int64_t>::min()) ||
				*doubleValue > static_cast<double>(std::numeric_limits<int64_t>::max())) {
				return false;
			}
			return tryAssignIntegralFromInt64(static_cast<int64_t>(*doubleValue), out);
		}
		if (const auto* boolValue = std::get_if<bool>(&value)) {
			return tryAssignIntegralFromInt64(*boolValue ? int64_t{1} : int64_t{0}, out);
		}
		return false;
	} else if constexpr (std::is_floating_point_v<T>) {
		if (const auto* doubleValue = std::get_if<double>(&value)) {
			if (!std::isfinite(*doubleValue)) {
				return false;
			}
			out = static_cast<T>(*doubleValue);
			return true;
		}
		if (const auto* intValue = std::get_if<int64_t>(&value)) {
			out = static_cast<T>(*intValue);
			return true;
		}
		if (const auto* boolValue = std::get_if<bool>(&value)) {
			out = *boolValue ? static_cast<T>(1) : static_cast<T>(0);
			return true;
		}
		return false;
	} else if constexpr (std::is_same_v<T, std::string>) {
		if (const auto* textValue = std::get_if<std::string>(&value)) {
			out = *textValue;
			return true;
		}
		return false;
	} else {
		return false;
	}
}

template <typename FieldT>
bool tryCaptureToDevValue(const FieldT& source, uint64_t fieldTypeHash, DevValue& outValue) {
	if constexpr (std::is_same_v<FieldT, bool>) {
		outValue = source;
		return true;
	} else if constexpr (std::is_integral_v<FieldT>) {
		if constexpr (std::is_signed_v<FieldT>) {
			outValue = static_cast<int64_t>(source);
			return true;
		} else {
			using UnsignedFieldT = std::make_unsigned_t<FieldT>;
			const UnsignedFieldT unsignedValue = static_cast<UnsignedFieldT>(source);
			if (unsignedValue <= static_cast<UnsignedFieldT>(std::numeric_limits<int64_t>::max())) {
				outValue = static_cast<int64_t>(unsignedValue);
			} else {
				outValue = static_cast<double>(unsignedValue);
			}
			return true;
		}
	} else if constexpr (std::is_floating_point_v<FieldT>) {
		if (!std::isfinite(static_cast<double>(source))) {
			return false;
		}
		outValue = static_cast<double>(source);
		return true;
	} else if constexpr (std::is_same_v<FieldT, std::string>) {
		outValue = source;
		return true;
	} else if constexpr (std::is_same_v<FieldT, Clay_ChildAlignment>) {
		if (fieldTypeHash != typeHash<Clay_ChildAlignment>()) {
			return false;
		}
		DevEnum2Value enumValue{};
		if (!tryCaptureDevEnum2Value(source, enumValue)) {
			return false;
		}
		outValue = enumValue;
		return true;
	} else if constexpr (std::is_same_v<FieldT, Clay_FloatingAttachPoints>) {
		if (fieldTypeHash != typeHash<Clay_FloatingAttachPoints>()) {
			return false;
		}
		DevEnum2Value enumValue{};
		if (!tryCaptureDevEnum2Value(source, enumValue)) {
			return false;
		}
		outValue = enumValue;
		return true;
	} else if constexpr (std::is_same_v<FieldT, Clay_Vector2>) {
		if (fieldTypeHash != typeHash<Clay_Vector2>()) {
			return false;
		}
		DevFloat2Value floatValue{};
		if (!tryCaptureDevFloat2Value(source, floatValue)) {
			return false;
		}
		outValue = floatValue;
		return true;
	} else if constexpr (std::is_same_v<FieldT, Clay_Dimensions>) {
		if (fieldTypeHash != typeHash<Clay_Dimensions>()) {
			return false;
		}
		DevFloat2Value floatValue{};
		if (!tryCaptureDevFloat2Value(source, floatValue)) {
			return false;
		}
		outValue = floatValue;
		return true;
	} else if constexpr (std::is_same_v<FieldT, Clay_SizingMinMax>) {
		if (fieldTypeHash != typeHash<Clay_SizingMinMax>()) {
			return false;
		}
		DevFloat2Value floatValue{};
		if (!tryCaptureDevFloat2Value(source, floatValue)) {
			return false;
		}
		outValue = floatValue;
		return true;
	} else if constexpr (std::is_same_v<FieldT, Clay_Color>) {
		if (fieldTypeHash != typeHash<Clay_Color>()) {
			return false;
		}
		DevFloat4Value floatValue{};
		if (!tryCaptureDevFloat4Value(source, floatValue)) {
			return false;
		}
		outValue = floatValue;
		return true;
	} else if constexpr (std::is_same_v<FieldT, Clay_CornerRadius>) {
		if (fieldTypeHash != typeHash<Clay_CornerRadius>()) {
			return false;
		}
		DevFloat4Value floatValue{};
		if (!tryCaptureDevFloat4Value(source, floatValue)) {
			return false;
		}
		outValue = floatValue;
		return true;
	} else if constexpr (std::is_same_v<FieldT, Clay_AspectRatioElementConfig>) {
		if (fieldTypeHash != typeHash<Clay_AspectRatioElementConfig>()) {
			return false;
		}
		double floatValue = 0.0;
		if (!tryCaptureDevFloat1Value(source, floatValue)) {
			return false;
		}
		outValue = floatValue;
		return true;
	} else if constexpr (std::is_same_v<FieldT, Clay_SizingAxis>) {
		if (fieldTypeHash != typeHash<Clay_SizingAxis>()) {
			return false;
		}
		DevTaggedUnionValue taggedUnionValue{};
		if (!tryCaptureDevTaggedUnionValue(source, taggedUnionValue)) {
			return false;
		}
		outValue = taggedUnionValue;
		return true;
	} else if constexpr (std::is_same_v<FieldT, Clay_Sizing>) {
		if (fieldTypeHash != typeHash<Clay_Sizing>()) {
			return false;
		}
		DevCompositeStructValue compositeValue{};
		if (!tryCaptureDevCompositeStructValue(source, compositeValue)) {
			return false;
		}
		outValue = compositeValue;
		return true;
	} else if constexpr (std::is_same_v<FieldT, Clay_LayoutConfig>) {
		if (fieldTypeHash != typeHash<Clay_LayoutConfig>()) {
			return false;
		}
		DevCompositeStructValue compositeValue{};
		if (!tryCaptureDevCompositeStructValue(source, compositeValue)) {
			return false;
		}
		outValue = compositeValue;
		return true;
	} else if constexpr (std::is_same_v<FieldT, Clay_TextElementConfig>) {
		if (fieldTypeHash != typeHash<Clay_TextElementConfig>()) {
			return false;
		}
		DevCompositeStructValue compositeValue{};
		if (!tryCaptureDevCompositeStructValue(source, compositeValue)) {
			return false;
		}
		outValue = compositeValue;
		return true;
	} else if constexpr (std::is_same_v<FieldT, Clay_FloatingElementConfig>) {
		if (fieldTypeHash != typeHash<Clay_FloatingElementConfig>()) {
			return false;
		}
		DevCompositeStructValue compositeValue{};
		if (!tryCaptureDevCompositeStructValue(source, compositeValue)) {
			return false;
		}
		outValue = compositeValue;
		return true;
	} else if constexpr (std::is_same_v<FieldT, Clay_ClipElementConfig>) {
		if (fieldTypeHash != typeHash<Clay_ClipElementConfig>()) {
			return false;
		}
		DevCompositeStructValue compositeValue{};
		if (!tryCaptureDevCompositeStructValue(source, compositeValue)) {
			return false;
		}
		outValue = compositeValue;
		return true;
	} else if constexpr (std::is_same_v<FieldT, Clay_BorderElementConfig>) {
		if (fieldTypeHash != typeHash<Clay_BorderElementConfig>()) {
			return false;
		}
		DevCompositeStructValue compositeValue{};
		if (!tryCaptureDevCompositeStructValue(source, compositeValue)) {
			return false;
		}
		outValue = compositeValue;
		return true;
	} else if constexpr (std::is_same_v<FieldT, Clay_ElementDeclaration>) {
		if (fieldTypeHash != typeHash<Clay_ElementDeclaration>()) {
			return false;
		}
		DevCompositeStructValue compositeValue{};
		if (!tryCaptureDevCompositeStructValue(source, compositeValue)) {
			return false;
		}
		outValue = compositeValue;
		return true;
	} else if constexpr (std::is_same_v<FieldT, Clay_Padding>) {
		if (fieldTypeHash != typeHash<Clay_Padding>()) {
			return false;
		}
		DevEdgeU16Value edgeValue{};
		if (!tryCaptureDevEdgeU16Value(source, edgeValue)) {
			return false;
		}
		outValue = edgeValue;
		return true;
	} else if constexpr (std::is_same_v<FieldT, Clay_BorderWidth>) {
		if (fieldTypeHash != typeHash<Clay_BorderWidth>()) {
			return false;
		}
		DevEdgeU16Value edgeValue{};
		if (!tryCaptureDevEdgeU16Value(source, edgeValue)) {
			return false;
		}
		outValue = edgeValue;
		return true;
	} else if constexpr (std::is_enum_v<FieldT>) {
		if (!isDevEnum1TypeHash(fieldTypeHash)) {
			return false;
		}
		using Underlying = std::underlying_type_t<FieldT>;
		const Underlying rawValue = static_cast<Underlying>(source);
		const int64_t rawValueInt64 = static_cast<int64_t>(rawValue);
		uint8_t normalizedNumeric = 0u;
		if (!tryNormalizeDevEnum1Numeric(rawValueInt64, normalizedNumeric)) {
			return false;
		}
		outValue = DevEnum1Value{.numeric = normalizedNumeric};
		return true;
	} else {
		return false;
	}
}

template <typename OwnerT, typename MemberT>
bool captureReflectedField(const void* owner, const FieldDescriptor& field, DevValue& outValue) {
	if (owner == nullptr) {
		return false;
	}
	MemberT OwnerT::* memberPointer = nullptr;
	if (!DevRegistry::tryGetMemberPointer<OwnerT, MemberT>(field, memberPointer) || memberPointer == nullptr) {
		return false;
	}

	const OwnerT& typedOwner = *static_cast<const OwnerT*>(owner);
	return tryCaptureToDevValue<MemberT>(typedOwner.*memberPointer, field.fieldTypeHash, outValue);
}

template <typename OwnerT, typename MemberT>
bool applyReflectedField(void* owner, const FieldDescriptor& field, const DevValue& value) {
	if (owner == nullptr) {
		return false;
	}
	MemberT OwnerT::* memberPointer = nullptr;
	if (!DevRegistry::tryGetMemberPointer<OwnerT, MemberT>(field, memberPointer) || memberPointer == nullptr) {
		return false;
	}

	OwnerT& typedOwner = *static_cast<OwnerT*>(owner);
	MemberT& destination = typedOwner.*memberPointer;
	return tryAssignFromDevValue<MemberT>(value, destination);
}

} // namespace detail

template <typename Owner, typename Member>
struct FieldInfo {
	const char* name = "";
	Member Owner::* member = nullptr;
	FieldCaptureFunction captureFieldFunction = nullptr;
	FieldApplyFunction applyFieldFunction = nullptr;
};

template <typename Owner, typename Member>
constexpr FieldInfo<Owner, Member> makeFieldInfo(const char* name, Member Owner::* member) {
	return FieldInfo<Owner, Member>{
		name,
		member,
		&detail::captureReflectedField<Owner, Member>,
		&detail::applyReflectedField<Owner, Member>,
	};
}

} // namespace FlowUi::devMode

// User-facing convenience macro for field reflection metadata.
#define FLOWUI_DEV_REFLECT_FIELD(TYPE, MEMBER) ::FlowUi::devMode::makeFieldInfo<TYPE>(#MEMBER, &TYPE::MEMBER)
#define FLOWUI_DEV_ENUM_VALUE(ENUM_VALUE) ::FlowUi::devMode::makeEnumValueInfo(#ENUM_VALUE, (ENUM_VALUE))

#define FLOWUI_DEV_DETAIL_CONCAT_INNER(a, b) a##b
#define FLOWUI_DEV_DETAIL_CONCAT(a, b) FLOWUI_DEV_DETAIL_CONCAT_INNER(a, b)
#define FLOWUI_DEV_DETAIL_GET_MACRO(_1, _2, NAME, ...) NAME

#if FLOW_UI_DEV_MODE
#define FLOWUI_DEV_REGISTER_STRUCT(TYPE, ...) \
	[[maybe_unused]] static const ::FlowUi::devMode::DevRegistrar FLOWUI_DEV_DETAIL_CONCAT(_flowui_dev_struct_reg_, __COUNTER__) = \
		::FlowUi::devMode::DevRegistrar([]() { \
			::FlowUi::devMode::registerStructSchemaWithSource<TYPE>( \
				#TYPE, \
				::FlowUi::devMode::makeDevStructSourceMetadata( \
					__FILE__, \
					static_cast<std::uint32_t>(__LINE__), \
					0u, \
					__func__) \
				__VA_OPT__(,) __VA_ARGS__); \
		})

#define FLOWUI_DEV_REGISTER_ELEMENT_1(DEFINITION_TYPE) \
	[[maybe_unused]] static const ::FlowUi::devMode::DevRegistrar FLOWUI_DEV_DETAIL_CONCAT(_flowui_dev_element_reg_, __COUNTER__) = \
		::FlowUi::devMode::DevRegistrar([]() { \
			::FlowUi::devMode::registerElementSchema<DEFINITION_TYPE>(#DEFINITION_TYPE); \
		})

#define FLOWUI_DEV_REGISTER_ELEMENT_2(DEFINITION_TYPE, DISPLAY_NAME) \
	[[maybe_unused]] static const ::FlowUi::devMode::DevRegistrar FLOWUI_DEV_DETAIL_CONCAT(_flowui_dev_element_reg_, __COUNTER__) = \
		::FlowUi::devMode::DevRegistrar([]() { \
			::FlowUi::devMode::registerElementSchema<DEFINITION_TYPE>(DISPLAY_NAME); \
		})

#define FLOWUI_DEV_REGISTER_ELEMENT(...) \
	FLOWUI_DEV_DETAIL_GET_MACRO(__VA_ARGS__, FLOWUI_DEV_REGISTER_ELEMENT_2, FLOWUI_DEV_REGISTER_ELEMENT_1)(__VA_ARGS__)

#define FLOWUI_DEV_REGISTER_ENUM(ENUM_TYPE, ...) \
	[[maybe_unused]] static const ::FlowUi::devMode::DevRegistrar FLOWUI_DEV_DETAIL_CONCAT(_flowui_dev_enum_reg_, __COUNTER__) = \
		::FlowUi::devMode::DevRegistrar([]() { \
			::FlowUi::devMode::registerEnumSchema<ENUM_TYPE>(#ENUM_TYPE __VA_OPT__(,) __VA_ARGS__); \
		})
#else
#define FLOWUI_DEV_REGISTER_STRUCT(...)
#define FLOWUI_DEV_REGISTER_ELEMENT(...)
#define FLOWUI_DEV_REGISTER_ENUM(...)
#endif
