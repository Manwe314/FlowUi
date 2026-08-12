#pragma once

#include "FlowUi/BuildConfig.hpp"

#if FLOW_UI_DEV_MODE

#include <cmath>
#include <cstdint>
#include <limits>
#include <string>
#include <string_view>
#include <type_traits>

#if defined(__has_include)
#if __has_include(<source_location>)
#include <source_location>
#if defined(__cpp_lib_source_location) && (__cpp_lib_source_location >= 201907L)
#define FLOWUI_HAS_STD_SOURCE_LOCATION 1
#endif
#endif
#endif

#include "clay.h"
#include "devMode/devRuntime.hpp"
#include "devMode/devTypes/devCompositeStruct.hpp"
#include "devMode/devTypes/devEdgeU16.hpp"
#include "devMode/devTypes/devEnum1.hpp"
#include "devMode/devTypes/devEnum2.hpp"
#include "devMode/devTypes/devFloat1.hpp"
#include "devMode/devTypes/devFloat2.hpp"
#include "devMode/devTypes/devFloat4.hpp"
#include "devMode/devTypes/devTaggedUnion.hpp"
#include "devMode/registry.hpp"
#include "internal/FlowUiElementBridge.hpp"

namespace FlowUi {

class UiManager;

namespace devMode::elementCapture {

#if defined(FLOWUI_HAS_STD_SOURCE_LOCATION)
using SourceLocation = std::source_location;
#else
struct SourceLocation {
	const char* file = "";
	const char* function = "";
	uint_least32_t lineValue = 0u;
	uint_least32_t columnValue = 0u;

	static constexpr SourceLocation current(
		const char* fileName = __builtin_FILE(),
		const char* functionName = __builtin_FUNCTION(),
		uint_least32_t line = __builtin_LINE(),
		uint_least32_t column = 0u) noexcept {
		return SourceLocation{
			.file = fileName ? fileName : "",
			.function = functionName ? functionName : "",
			.lineValue = line,
			.columnValue = column,
		};
	}

	constexpr const char* file_name() const noexcept { return file ? file : ""; }
	constexpr const char* function_name() const noexcept { return function ? function : ""; }
	constexpr uint_least32_t line() const noexcept { return lineValue; }
	constexpr uint_least32_t column() const noexcept { return columnValue; }
};
#endif

devMode::DevRuntime& runtime(UiManager& uiManager);

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
bool tryAssignFromDevValue(const devMode::DevValue& value, T& out) {
	if constexpr (std::is_same_v<T, Clay_ChildAlignment>) {
		if (const auto* enumValue = std::get_if<devMode::DevEnum2Value>(&value)) {
			return devMode::tryApplyDevEnum2Value(*enumValue, out);
		}
		return false;
	} else if constexpr (std::is_same_v<T, Clay_FloatingAttachPoints>) {
		if (const auto* enumValue = std::get_if<devMode::DevEnum2Value>(&value)) {
			return devMode::tryApplyDevEnum2Value(*enumValue, out);
		}
		return false;
	} else if constexpr (std::is_same_v<T, Clay_Vector2>) {
		if (const auto* floatValue = std::get_if<devMode::DevFloat2Value>(&value)) {
			return devMode::tryApplyDevFloat2Value(*floatValue, out);
		}
		return false;
	} else if constexpr (std::is_same_v<T, Clay_Dimensions>) {
		if (const auto* floatValue = std::get_if<devMode::DevFloat2Value>(&value)) {
			return devMode::tryApplyDevFloat2Value(*floatValue, out);
		}
		return false;
	} else if constexpr (std::is_same_v<T, Clay_SizingMinMax>) {
		if (const auto* floatValue = std::get_if<devMode::DevFloat2Value>(&value)) {
			return devMode::tryApplyDevFloat2Value(*floatValue, out);
		}
		return false;
	} else if constexpr (std::is_same_v<T, Clay_Color>) {
		if (const auto* floatValue = std::get_if<devMode::DevFloat4Value>(&value)) {
			return devMode::tryApplyDevFloat4Value(*floatValue, out);
		}
		return false;
	} else if constexpr (std::is_same_v<T, Clay_CornerRadius>) {
		if (const auto* floatValue = std::get_if<devMode::DevFloat4Value>(&value)) {
			return devMode::tryApplyDevFloat4Value(*floatValue, out);
		}
		return false;
	} else if constexpr (std::is_same_v<T, Clay_AspectRatioElementConfig>) {
		double numeric = 0.0;
		if (!tryAssignFromDevValue(value, numeric)) {
			return false;
		}
		return devMode::tryApplyDevFloat1Value(numeric, out);
	} else if constexpr (std::is_same_v<T, Clay_SizingAxis>) {
		if (const auto* taggedUnionValue = std::get_if<devMode::DevTaggedUnionValue>(&value)) {
			return devMode::tryApplyDevTaggedUnionValue(*taggedUnionValue, out);
		}
		return false;
	} else if constexpr (std::is_same_v<T, Clay_Sizing>) {
		if (const auto* compositeValue = std::get_if<devMode::DevCompositeStructValue>(&value)) {
			return devMode::tryApplyDevCompositeStructValue(*compositeValue, out);
		}
		return false;
	} else if constexpr (std::is_same_v<T, Clay_LayoutConfig>) {
		if (const auto* compositeValue = std::get_if<devMode::DevCompositeStructValue>(&value)) {
			return devMode::tryApplyDevCompositeStructValue(*compositeValue, out);
		}
		return false;
	} else if constexpr (std::is_same_v<T, Clay_TextElementConfig>) {
		if (const auto* compositeValue = std::get_if<devMode::DevCompositeStructValue>(&value)) {
			return devMode::tryApplyDevCompositeStructValue(*compositeValue, out);
		}
		return false;
	} else if constexpr (std::is_same_v<T, Clay_FloatingElementConfig>) {
		if (const auto* compositeValue = std::get_if<devMode::DevCompositeStructValue>(&value)) {
			return devMode::tryApplyDevCompositeStructValue(*compositeValue, out);
		}
		return false;
	} else if constexpr (std::is_same_v<T, Clay_ClipElementConfig>) {
		if (const auto* compositeValue = std::get_if<devMode::DevCompositeStructValue>(&value)) {
			return devMode::tryApplyDevCompositeStructValue(*compositeValue, out);
		}
		return false;
	} else if constexpr (std::is_same_v<T, Clay_BorderElementConfig>) {
		if (const auto* compositeValue = std::get_if<devMode::DevCompositeStructValue>(&value)) {
			return devMode::tryApplyDevCompositeStructValue(*compositeValue, out);
		}
		return false;
	} else if constexpr (std::is_same_v<T, Clay_ElementDeclaration>) {
		if (const auto* compositeValue = std::get_if<devMode::DevCompositeStructValue>(&value)) {
			return devMode::tryApplyDevCompositeStructValue(*compositeValue, out);
		}
		return false;
	} else if constexpr (std::is_same_v<T, Clay_Padding>) {
		if (const auto* edgeValue = std::get_if<devMode::DevEdgeU16Value>(&value)) {
			return devMode::tryApplyDevEdgeU16Value(*edgeValue, out);
		}
		return false;
	} else if constexpr (std::is_same_v<T, Clay_BorderWidth>) {
		if (const auto* edgeValue = std::get_if<devMode::DevEdgeU16Value>(&value)) {
			return devMode::tryApplyDevEdgeU16Value(*edgeValue, out);
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
		if (const auto* enumValue = std::get_if<devMode::DevEnum1Value>(&value)) {
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

template <typename ParamsT, typename FieldT>
bool tryApplyOverrideField(ParamsT& params, const devMode::FieldDescriptor& field, const devMode::DevValue& value) {
	FieldT ParamsT::* memberPointer = nullptr;
	if (!devMode::DevRegistry::tryGetMemberPointer<ParamsT, FieldT>(field, memberPointer) || memberPointer == nullptr) {
		return false;
	}

	FieldT& destination = params.*memberPointer;
	return tryAssignFromDevValue<FieldT>(value, destination);
}

template <typename ParamsT, typename FieldT>
bool tryCaptureParameterFieldValue(
	const ParamsT& params,
	const devMode::FieldDescriptor& field,
	devMode::DevValue& outValue) {
	FieldT ParamsT::* memberPointer = nullptr;
	if (!devMode::DevRegistry::tryGetMemberPointer<ParamsT, FieldT>(field, memberPointer) || memberPointer == nullptr) {
		return false;
	}

	const FieldT& source = params.*memberPointer;
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
		if (field.fieldTypeHash != devMode::typeHash<Clay_ChildAlignment>()) {
			return false;
		}
		devMode::DevEnum2Value enumValue{};
		if (!devMode::tryCaptureDevEnum2Value(source, enumValue)) {
			return false;
		}
		outValue = enumValue;
		return true;
	} else if constexpr (std::is_same_v<FieldT, Clay_FloatingAttachPoints>) {
		if (field.fieldTypeHash != devMode::typeHash<Clay_FloatingAttachPoints>()) {
			return false;
		}
		devMode::DevEnum2Value enumValue{};
		if (!devMode::tryCaptureDevEnum2Value(source, enumValue)) {
			return false;
		}
		outValue = enumValue;
		return true;
	} else if constexpr (std::is_same_v<FieldT, Clay_Vector2>) {
		if (field.fieldTypeHash != devMode::typeHash<Clay_Vector2>()) {
			return false;
		}
		devMode::DevFloat2Value floatValue{};
		if (!devMode::tryCaptureDevFloat2Value(source, floatValue)) {
			return false;
		}
		outValue = floatValue;
		return true;
	} else if constexpr (std::is_same_v<FieldT, Clay_Dimensions>) {
		if (field.fieldTypeHash != devMode::typeHash<Clay_Dimensions>()) {
			return false;
		}
		devMode::DevFloat2Value floatValue{};
		if (!devMode::tryCaptureDevFloat2Value(source, floatValue)) {
			return false;
		}
		outValue = floatValue;
		return true;
	} else if constexpr (std::is_same_v<FieldT, Clay_SizingMinMax>) {
		if (field.fieldTypeHash != devMode::typeHash<Clay_SizingMinMax>()) {
			return false;
		}
		devMode::DevFloat2Value floatValue{};
		if (!devMode::tryCaptureDevFloat2Value(source, floatValue)) {
			return false;
		}
		outValue = floatValue;
		return true;
	} else if constexpr (std::is_same_v<FieldT, Clay_Color>) {
		if (field.fieldTypeHash != devMode::typeHash<Clay_Color>()) {
			return false;
		}
		devMode::DevFloat4Value floatValue{};
		if (!devMode::tryCaptureDevFloat4Value(source, floatValue)) {
			return false;
		}
		outValue = floatValue;
		return true;
	} else if constexpr (std::is_same_v<FieldT, Clay_CornerRadius>) {
		if (field.fieldTypeHash != devMode::typeHash<Clay_CornerRadius>()) {
			return false;
		}
		devMode::DevFloat4Value floatValue{};
		if (!devMode::tryCaptureDevFloat4Value(source, floatValue)) {
			return false;
		}
		outValue = floatValue;
		return true;
	} else if constexpr (std::is_same_v<FieldT, Clay_AspectRatioElementConfig>) {
		if (field.fieldTypeHash != devMode::typeHash<Clay_AspectRatioElementConfig>()) {
			return false;
		}
		double floatValue = 0.0;
		if (!devMode::tryCaptureDevFloat1Value(source, floatValue)) {
			return false;
		}
		outValue = floatValue;
		return true;
	} else if constexpr (std::is_same_v<FieldT, Clay_SizingAxis>) {
		if (field.fieldTypeHash != devMode::typeHash<Clay_SizingAxis>()) {
			return false;
		}
		devMode::DevTaggedUnionValue taggedUnionValue{};
		if (!devMode::tryCaptureDevTaggedUnionValue(source, taggedUnionValue)) {
			return false;
		}
		outValue = taggedUnionValue;
		return true;
	} else if constexpr (std::is_same_v<FieldT, Clay_Sizing>) {
		if (field.fieldTypeHash != devMode::typeHash<Clay_Sizing>()) {
			return false;
		}
		devMode::DevCompositeStructValue compositeValue{};
		if (!devMode::tryCaptureDevCompositeStructValue(source, compositeValue)) {
			return false;
		}
		outValue = compositeValue;
		return true;
	} else if constexpr (std::is_same_v<FieldT, Clay_LayoutConfig>) {
		if (field.fieldTypeHash != devMode::typeHash<Clay_LayoutConfig>()) {
			return false;
		}
		devMode::DevCompositeStructValue compositeValue{};
		if (!devMode::tryCaptureDevCompositeStructValue(source, compositeValue)) {
			return false;
		}
		outValue = compositeValue;
		return true;
	} else if constexpr (std::is_same_v<FieldT, Clay_TextElementConfig>) {
		if (field.fieldTypeHash != devMode::typeHash<Clay_TextElementConfig>()) {
			return false;
		}
		devMode::DevCompositeStructValue compositeValue{};
		if (!devMode::tryCaptureDevCompositeStructValue(source, compositeValue)) {
			return false;
		}
		outValue = compositeValue;
		return true;
	} else if constexpr (std::is_same_v<FieldT, Clay_FloatingElementConfig>) {
		if (field.fieldTypeHash != devMode::typeHash<Clay_FloatingElementConfig>()) {
			return false;
		}
		devMode::DevCompositeStructValue compositeValue{};
		if (!devMode::tryCaptureDevCompositeStructValue(source, compositeValue)) {
			return false;
		}
		outValue = compositeValue;
		return true;
	} else if constexpr (std::is_same_v<FieldT, Clay_ClipElementConfig>) {
		if (field.fieldTypeHash != devMode::typeHash<Clay_ClipElementConfig>()) {
			return false;
		}
		devMode::DevCompositeStructValue compositeValue{};
		if (!devMode::tryCaptureDevCompositeStructValue(source, compositeValue)) {
			return false;
		}
		outValue = compositeValue;
		return true;
	} else if constexpr (std::is_same_v<FieldT, Clay_BorderElementConfig>) {
		if (field.fieldTypeHash != devMode::typeHash<Clay_BorderElementConfig>()) {
			return false;
		}
		devMode::DevCompositeStructValue compositeValue{};
		if (!devMode::tryCaptureDevCompositeStructValue(source, compositeValue)) {
			return false;
		}
		outValue = compositeValue;
		return true;
	} else if constexpr (std::is_same_v<FieldT, Clay_ElementDeclaration>) {
		if (field.fieldTypeHash != devMode::typeHash<Clay_ElementDeclaration>()) {
			return false;
		}
		devMode::DevCompositeStructValue compositeValue{};
		if (!devMode::tryCaptureDevCompositeStructValue(source, compositeValue)) {
			return false;
		}
		outValue = compositeValue;
		return true;
	} else if constexpr (std::is_same_v<FieldT, Clay_Padding>) {
		if (field.fieldTypeHash != devMode::typeHash<Clay_Padding>()) {
			return false;
		}
		devMode::DevEdgeU16Value edgeValue{};
		if (!devMode::tryCaptureDevEdgeU16Value(source, edgeValue)) {
			return false;
		}
		outValue = edgeValue;
		return true;
	} else if constexpr (std::is_same_v<FieldT, Clay_BorderWidth>) {
		if (field.fieldTypeHash != devMode::typeHash<Clay_BorderWidth>()) {
			return false;
		}
		devMode::DevEdgeU16Value edgeValue{};
		if (!devMode::tryCaptureDevEdgeU16Value(source, edgeValue)) {
			return false;
		}
		outValue = edgeValue;
		return true;
	} else if constexpr (std::is_enum_v<FieldT>) {
		if (!devMode::isDevEnum1TypeHash(field.fieldTypeHash)) {
			return false;
		}
		using Underlying = std::underlying_type_t<FieldT>;
		const Underlying rawValue = static_cast<Underlying>(source);
		const int64_t rawValueInt64 = static_cast<int64_t>(rawValue);
		uint8_t normalizedNumeric = 0u;
		if (!devMode::tryNormalizeDevEnum1Numeric(rawValueInt64, normalizedNumeric)) {
			return false;
		}
		outValue = devMode::DevEnum1Value{.numeric = normalizedNumeric};
		return true;
	} else {
		return false;
	}
}

template <typename ParamsT>
void captureParameterSnapshotField(
	devMode::DevRuntime& runtime,
	FlowDefinitionID definitionId,
	FlowElementID elementId,
	const devMode::FieldDescriptor& field,
	uint64_t fieldHash,
	const ParamsT& params) {
	if (field.captureFieldFunction == nullptr) {
		return;
	}

	devMode::DevValue capturedValue{};
	if (!field.captureFieldFunction(static_cast<const void*>(&params), field, capturedValue)) {
		return;
	}

	runtime.captureLastSeenParamField(definitionId, elementId, fieldHash, capturedValue);
}

template <typename ParamsT>
void applyParameterOverrides(
	UiManager& uiManager,
	FlowDefinitionID definitionId,
	FlowElementID elementId,
	ParamsT& params) {
	const devMode::StructDescriptor* paramsStruct = devMode::DevRegistry::instance().template findStruct<ParamsT>();
	if (!paramsStruct || paramsStruct->fields.empty()) {
		return;
	}

	devMode::DevRuntime& devRuntime = runtime(uiManager);
	for (const devMode::FieldDescriptor& field : paramsStruct->fields) {
		const uint64_t fieldHash = (field.fieldHash == 0u) ? devMode::hashString64(field.name) : field.fieldHash;
		const auto applySingleOverrideValue = [&](const devMode::DevValue& value) {
			if (field.applyFieldFunction == nullptr) {
				return;
			}
			(void)field.applyFieldFunction(static_cast<void*>(&params), field, value);
		};

		if (const devMode::DevValue* definitionOverride = devRuntime.findDefinitionParamOverride(definitionId, fieldHash)) {
			applySingleOverrideValue(*definitionOverride);
		}
		if (const devMode::DevValue* instanceOverride =
			devRuntime.findInstanceParamOverride(definitionId, elementId, fieldHash)) {
			applySingleOverrideValue(*instanceOverride);
		}

		captureParameterSnapshotField(devRuntime, definitionId, elementId, field, fieldHash, params);
	}
}

} // namespace devMode::elementCapture

} // namespace FlowUi

#endif
