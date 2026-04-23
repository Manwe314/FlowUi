#pragma once

#include <cstddef>
#include <cstdint>
#include <cmath>
#include <limits>
#include <optional>
#include <string>
#include <string_view>
#include <stdexcept>
#include <type_traits>
#include <utility>
#include <vector>
#if FLOW_UI_DEV_MODE
#if defined(__has_include)
#if __has_include(<source_location>)
#include <source_location>
#if defined(__cpp_lib_source_location) && (__cpp_lib_source_location >= 201907L)
#define FLOWUI_HAS_STD_SOURCE_LOCATION 1
#endif
#endif
#endif
#endif

#include "clay.h"
#include "FlowUi/PublicStructs.hpp"
#include "FlowUi/App.hpp"
#if FLOW_UI_DEV_MODE
#include "devMode/devRuntime.hpp"
#include "devMode/devTypes/devEnum1.hpp"
#include "devMode/devTypes/devEnum2.hpp"
#include "devMode/devTypes/devFloat1.hpp"
#include "devMode/devTypes/devFloat2.hpp"
#include "devMode/devTypes/devFloat4.hpp"
#include "devMode/devTypes/devEdgeU16.hpp"
#include "devMode/devTypes/devTaggedUnion.hpp"
#include "devMode/devTypes/devCompositeStruct.hpp"
#include "devMode/registry.hpp"
#endif

namespace FlowUi {


struct InteractionSnapshot {
	std::vector<Clay_ElementId> hoveredElementIds;
    std::vector<Clay_ElementId> pressedElementIds;
    std::vector<Clay_ElementId> heldElementIds;
    std::vector<Clay_ElementId> releasedElementIds;

    static bool contains(const std::vector<Clay_ElementId>& list, Clay_ElementId id)
	{
        for (const auto& item : list)
            if (item.id == id.id)
				return true;
        return false;
    }

    bool isHovered(Clay_ElementId id)  const { return contains(hoveredElementIds, id);  }
    bool isPressed(Clay_ElementId id)  const { return contains(pressedElementIds, id);  }
    bool isHeld(Clay_ElementId id)     const { return contains(heldElementIds, id);     }
    bool isReleased(Clay_ElementId id) const { return contains(releasedElementIds, id); }
};

struct NoElementParameters {};
struct NoElementState {};
struct NoElementResources {};

class UiManager;
Clay_ElementId flowUiToClayElementId(UiManager& uiManager, std::string_view elementID);
const InteractionSnapshot& flowUiPreviousInteraction(const UiManager& uiManager);
void flowUiPushConstructedElement(UiManager& uiManager, Clay_ElementId elementId);
#if FLOW_UI_DEV_MODE
std::size_t flowUiDevBeginCapturedFlowElement(
	UiManager& uiManager,
	uint64_t definitionId,
	uint64_t definitionTypeHash,
	std::string_view definitionTypeToken,
	std::string_view elementID,
	uint64_t flowId,
	bool isInternalToDevMode);
bool flowUiDevEndCapturedFlowElement(UiManager& uiManager);
devMode::DevRuntime& flowUiDevRuntime(UiManager& uiManager);
#endif

#if FLOW_UI_DEV_MODE
namespace devCaptureDetail {

#if defined(FLOWUI_HAS_STD_SOURCE_LOCATION)
using DevSourceLocation = std::source_location;
#else
struct DevSourceLocation {
	const char* file = "";
	const char* function = "";
	uint_least32_t lineValue = 0u;
	uint_least32_t columnValue = 0u;

	static constexpr DevSourceLocation current(
		const char* fileName = __builtin_FILE(),
		const char* functionName = __builtin_FUNCTION(),
		uint_least32_t line = __builtin_LINE(),
		uint_least32_t column = 0u) noexcept {
		return DevSourceLocation{
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
	uint64_t definitionId,
	uint64_t flowId,
	std::string_view elementID,
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

	runtime.captureLastSeenParamField(definitionId, flowId, elementID, fieldHash, capturedValue);
}

template <typename ParamsT>
void applyParameterOverrides(
	UiManager& uiManager,
	uint64_t definitionId,
	uint64_t flowId,
	std::string_view elementID,
	ParamsT& params) {
	const devMode::StructDescriptor* paramsStruct = devMode::DevRegistry::instance().template findStruct<ParamsT>();
	if (!paramsStruct || paramsStruct->fields.empty()) {
		return;
	}

	devMode::DevRuntime& runtime = flowUiDevRuntime(uiManager);
	for (const devMode::FieldDescriptor& field : paramsStruct->fields) {
		const uint64_t fieldHash = (field.fieldHash == 0u) ? devMode::hashString64(field.name) : field.fieldHash;
		const auto applySingleOverrideValue = [&](const devMode::DevValue& value) {
			if (field.applyFieldFunction == nullptr) {
				return;
			}
			(void)field.applyFieldFunction(static_cast<void*>(&params), field, value);
		};

		// Definition-level defaults are applied first...
		if (const devMode::DevValue* definitionOverride = runtime.findDefinitionParamOverride(definitionId, fieldHash)) {
			applySingleOverrideValue(*definitionOverride);
		}
		// ...then instance-level overrides are applied last and win.
		if (const devMode::DevValue* instanceOverride =
			runtime.findInstanceParamOverride(definitionId, flowId, elementID, fieldHash)) {
			applySingleOverrideValue(*instanceOverride);
		}

		captureParameterSnapshotField(runtime, definitionId, flowId, elementID, field, fieldHash, params);
	}
}

} // namespace devCaptureDetail
#endif

template <typename Parameters = NoElementParameters>
struct ElementBuildContext;

template <typename Parameters = NoElementParameters>
struct ElementInteractionContext;

template <typename Parameters>
struct ElementBuildContext
{
    using ParametersType = std::conditional_t<std::is_void_v<Parameters>, NoElementParameters, Parameters>;

    UiManager& uiManager;
    std::string_view elementID;
    ParametersType& params;

    std::string createChildElementId(std::string_view localChildId) const
    {
        std::string full = std::string(elementID) + "/" + std::string(localChildId);
        return full;
    }
};

template <typename Parameters>
struct ElementInteractionContext
{
    using ParametersType = std::conditional_t<std::is_void_v<Parameters>, NoElementParameters, Parameters>;

    UiManager& uiManager;
    std::string_view elementID;
    ParametersType& params;
    const InteractionSnapshot& previousInteraction;

    std::string createChildElementId(std::string_view localChildId) const
    {
        std::string full = std::string(elementID) + "/" + std::string(localChildId);
        return full;
    }
};


template <
	typename Parameters = NoElementParameters,
	typename State = void,
	typename Resources = void,
	uint64_t DefinitionId = 0,
	bool IsDevInternal = false>
struct ElementDefinition
{
    using ParametersType = std::conditional_t<std::is_void_v<Parameters>, NoElementParameters, Parameters>;
    using StateType = std::conditional_t<std::is_void_v<State>, NoElementState, State>;
    using ResourcesType = std::conditional_t<std::is_void_v<Resources>, NoElementResources, Resources>;
    using BuildContext = ElementBuildContext<Parameters>;
    using InteractionContext = ElementInteractionContext<Parameters>;
    using StatePoolEntry = std::pair<uint64_t, StateType>;

    static constexpr uint64_t definitionId = DefinitionId;
    static constexpr bool isDevInternal = IsDevInternal;
    static constexpr bool hasState = !std::is_void_v<State>;
    static constexpr bool hasResources = !std::is_void_v<Resources>;
    static inline std::optional<ResourcesType> resources{};
    static inline std::vector<StatePoolEntry> statePool{};

    static ResourcesType& getResources(App& app)
    {
        static_assert(hasResources, "FlowUi: getResources is only available when ElementDefinition Resources template argument is not void.");
        if (!resources.has_value()) {
            if constexpr (std::is_constructible_v<ResourcesType, App&>) {
                resources.emplace(app);
            } else if constexpr (std::is_constructible_v<ResourcesType, UiManager&>) {
                resources.emplace(app.ui());
            } else {
                resources.emplace();
            }
        }
        return *resources;
    }

    static StateType& getOrCreateState(uint64_t elementFlowId)
    {
        static_assert(hasState, "FlowUi: getOrCreateState is only available when ElementDefinition State template argument is not void.");
        for (StatePoolEntry& entry : statePool) {
            if (entry.first == elementFlowId) {
                return entry.second;
            }
        }
        statePool.emplace_back(elementFlowId, StateType{});
        return statePool.back().second;
    }

    static StateType* tryGetState(uint64_t elementFlowId)
    {
        static_assert(hasState, "FlowUi: tryGetState is only available when ElementDefinition State template argument is not void.");
        for (StatePoolEntry& entry : statePool) {
            if (entry.first == elementFlowId) {
                return &entry.second;
            }
        }
        return nullptr;
    }

    static const StateType* tryGetStateConst(uint64_t elementFlowId)
    {
        static_assert(hasState, "FlowUi: tryGetStateConst is only available when ElementDefinition State template argument is not void.");
        for (const StatePoolEntry& entry : statePool) {
            if (entry.first == elementFlowId) {
                return &entry.second;
            }
        }
        return nullptr;
    }

    static bool eraseState(uint64_t elementFlowId)
    {
        static_assert(hasState, "FlowUi: eraseState is only available when ElementDefinition State template argument is not void.");
        for (std::size_t i = 0; i < statePool.size(); ++i) {
            if (statePool[i].first == elementFlowId) {
                statePool[i] = std::move(statePool.back());
                statePool.pop_back();
                return true;
            }
        }
        return false;
    }

    void (*onHovered)(InteractionContext&) = nullptr;
    void (*onPressed)(InteractionContext&) = nullptr;
    void (*onHeld)(InteractionContext&) = nullptr;
    void (*onReleased)(InteractionContext&) = nullptr;

    void (*runLogic)(InteractionContext&) = nullptr;

    Clay_ElementDeclaration (*constructElement)(BuildContext&) = nullptr;
    void (*buildElement)(BuildContext&) = nullptr;
};


enum class ElementDrawOptions : uint32_t
{
    Default = 0,
    SkipEventCallbacks = 1u << 0,
    SkipLogicCallback  = 1u << 1,
    SkipBuildCallback  = 1u << 2,
};

inline ElementDrawOptions operator|(ElementDrawOptions a, ElementDrawOptions b)
{
    return static_cast<ElementDrawOptions>(static_cast<uint32_t>(a) | static_cast<uint32_t>(b));
}

inline bool elementDrawOptionsHas(ElementDrawOptions value, ElementDrawOptions flag)
{
    return (static_cast<uint32_t>(value) & static_cast<uint32_t>(flag)) != 0;
}


template <
	typename Parameters = NoElementParameters,
	typename State = void,
	typename Resources = void,
	uint64_t DefinitionId = 0,
	bool IsDevInternal = false>
class ElementBuilder {
public:
    using DefinitionType = ElementDefinition<Parameters, State, Resources, DefinitionId, IsDevInternal>;
    using ParametersType = typename DefinitionType::ParametersType;
    using BuildContext = typename DefinitionType::BuildContext;
    using InteractionContext = typename DefinitionType::InteractionContext;

    ElementBuilder(UiManager& uiManager, const DefinitionType* definition, std::string elementID
#if FLOW_UI_DEV_MODE
		, devCaptureDetail::DevSourceLocation sourceLocation = devCaptureDetail::DevSourceLocation::current()
#endif
		) :
        uiManager_(uiManager),
        elementDefinition_(definition),
        elementID_(std::move(elementID)),
		captureAsDevInternal_(DefinitionType::isDevInternal)
#if FLOW_UI_DEV_MODE
		, sourceLocation_(sourceLocation)
#endif
		{}

    ElementBuilder& setParameters(const ParametersType& parameters)
    {
        params_ = parameters;
        return *this;
    }

    ElementBuilder& setParameters(ParametersType&& parameters)
    {
        params_ = std::move(parameters);
        return *this;
    }

	template <typename MergeFn>
	ElementBuilder& mergeParams(MergeFn&& mergeFn)
	{
		static_assert(
			std::is_invocable_v<MergeFn, ParametersType&>,
			"FlowUi: mergeParams expects a callable that can be invoked with ParametersType&.");
		std::forward<MergeFn>(mergeFn)(params_);
		return *this;
	}

	ElementBuilder& withElementID(std::string_view elementID)
    {
        elementID_.assign(elementID.data(), elementID.size());
        return *this;
    }

	ElementBuilder& setDevInternalCapture(bool isDevInternal = true)
	{
		captureAsDevInternal_ = isDevInternal;
		return *this;
	}

    void construct(ElementDrawOptions options = ElementDrawOptions::Default);
    void draw(ElementDrawOptions options = ElementDrawOptions::Default);

private:
    UiManager& uiManager_;
    const DefinitionType* elementDefinition_;
    std::string elementID_;
    ParametersType params_{};
	bool captureAsDevInternal_ = false;
#if FLOW_UI_DEV_MODE
	devCaptureDetail::DevSourceLocation sourceLocation_{};
#endif
};

template <typename Parameters, typename State, typename Resources, uint64_t DefinitionId, bool IsDevInternal>
void ElementBuilder<Parameters, State, Resources, DefinitionId, IsDevInternal>::construct(ElementDrawOptions options)
{
    if (!elementDefinition_ || !elementDefinition_->constructElement) {
        throw std::runtime_error("FlowUi: elementDefinition is null or missing constructElement callback.");
    }

    const Clay_ElementId rootElementId = flowUiToClayElementId(uiManager_, elementID_);
    const uint64_t elementFlowId = toFlowId(elementID_);
#if FLOW_UI_DEV_MODE
    const std::size_t captureIndex = flowUiDevBeginCapturedFlowElement(
        uiManager_,
        DefinitionType::definitionId,
        devMode::typeHash<DefinitionType>(),
        devMode::typeToken<DefinitionType>(),
        elementID_,
        elementFlowId,
		captureAsDevInternal_);
	if (captureIndex != devMode::DevRuntime::kInvalidCaptureIndex) {
		(void)flowUiDevRuntime(uiManager_).setCapturedElementSource(
			captureIndex,
			sourceLocation_.file_name(),
			static_cast<uint32_t>(sourceLocation_.line()),
			static_cast<uint32_t>(sourceLocation_.column()),
			sourceLocation_.function_name());
	}

    struct DevCaptureRollback {
        UiManager& uiManager;
        bool enabled = true;
        ~DevCaptureRollback() {
            if (enabled) {
                (void)flowUiDevEndCapturedFlowElement(uiManager);
            }
        }
    } devCaptureRollback{uiManager_, true};
#endif

    if (!elementDrawOptionsHas(options, ElementDrawOptions::SkipEventCallbacks)) {
        const InteractionSnapshot& previousInteraction = flowUiPreviousInteraction(uiManager_);

        InteractionContext eventContext{
            uiManager_,
            elementID_,
            params_,
            previousInteraction
        };

        if (elementDefinition_->onHovered && previousInteraction.isHovered(rootElementId)) {
            elementDefinition_->onHovered(eventContext);
        }
        if (elementDefinition_->onPressed && previousInteraction.isPressed(rootElementId)) {
            elementDefinition_->onPressed(eventContext);
        }
        if (elementDefinition_->onHeld && previousInteraction.isHeld(rootElementId)) {
            elementDefinition_->onHeld(eventContext);
        }
        if (elementDefinition_->onReleased && previousInteraction.isReleased(rootElementId)) {
            elementDefinition_->onReleased(eventContext);
        }
    }

    if (!elementDrawOptionsHas(options, ElementDrawOptions::SkipLogicCallback) && elementDefinition_->runLogic) {
        const InteractionSnapshot& previousInteraction = flowUiPreviousInteraction(uiManager_);
        InteractionContext logicContext{
            uiManager_,
            elementID_,
            params_,
            previousInteraction
        };
        elementDefinition_->runLogic(logicContext);
    }

#if FLOW_UI_DEV_MODE
    devCaptureDetail::applyParameterOverrides<ParametersType>(
        uiManager_,
        DefinitionType::definitionId,
        elementFlowId,
        elementID_,
        params_);
#endif

    BuildContext buildContext{
        uiManager_,
        elementID_,
        params_
    };

    Clay_ElementDeclaration declaration = elementDefinition_->constructElement(buildContext);
    declaration.id = rootElementId;
    Clay__OpenElement();
    Clay__ConfigureOpenElement(declaration);
    flowUiPushConstructedElement(uiManager_, rootElementId);
#if FLOW_UI_DEV_MODE
    devCaptureRollback.enabled = false;
#endif
}

template <typename Parameters, typename State, typename Resources, uint64_t DefinitionId, bool IsDevInternal>
void ElementBuilder<Parameters, State, Resources, DefinitionId, IsDevInternal>::draw(ElementDrawOptions options)
{
    if (!elementDefinition_ || !elementDefinition_->buildElement) {
        throw std::runtime_error("FlowUi: elementDefinition is null or missing buildElement callback.");
    }

    const Clay_ElementId rootElementId = flowUiToClayElementId(uiManager_, elementID_);
    const uint64_t elementFlowId = toFlowId(elementID_);
#if FLOW_UI_DEV_MODE
    const std::size_t captureIndex = flowUiDevBeginCapturedFlowElement(
        uiManager_,
        DefinitionType::definitionId,
        devMode::typeHash<DefinitionType>(),
        devMode::typeToken<DefinitionType>(),
        elementID_,
        elementFlowId,
		captureAsDevInternal_);
	if (captureIndex != devMode::DevRuntime::kInvalidCaptureIndex) {
		(void)flowUiDevRuntime(uiManager_).setCapturedElementSource(
			captureIndex,
			sourceLocation_.file_name(),
			static_cast<uint32_t>(sourceLocation_.line()),
			static_cast<uint32_t>(sourceLocation_.column()),
			sourceLocation_.function_name());
	}

    struct DevCaptureCloseOnExit {
        UiManager& uiManager;
        ~DevCaptureCloseOnExit() {
            (void)flowUiDevEndCapturedFlowElement(uiManager);
        }
    } devCaptureCloseOnExit{uiManager_};
#endif

    if (!elementDrawOptionsHas(options, ElementDrawOptions::SkipEventCallbacks)) {
        const InteractionSnapshot& previousInteraction = flowUiPreviousInteraction(uiManager_);

        InteractionContext eventContext{
            uiManager_,
            elementID_,
            params_,
            previousInteraction
        };

        if (elementDefinition_->onHovered && previousInteraction.isHovered(rootElementId)) {
            elementDefinition_->onHovered(eventContext);
        }
        if (elementDefinition_->onPressed && previousInteraction.isPressed(rootElementId)) {
            elementDefinition_->onPressed(eventContext);
        }
        if (elementDefinition_->onHeld && previousInteraction.isHeld(rootElementId)) {
            elementDefinition_->onHeld(eventContext);
        }
        if (elementDefinition_->onReleased && previousInteraction.isReleased(rootElementId)) {
            elementDefinition_->onReleased(eventContext);
        }
    }

    if (!elementDrawOptionsHas(options, ElementDrawOptions::SkipLogicCallback) && elementDefinition_->runLogic) {
        const InteractionSnapshot& previousInteraction = flowUiPreviousInteraction(uiManager_);
        InteractionContext logicContext{
            uiManager_,
            elementID_,
            params_,
            previousInteraction
        };
        elementDefinition_->runLogic(logicContext);
    }

    if (!elementDrawOptionsHas(options, ElementDrawOptions::SkipBuildCallback)) {
#if FLOW_UI_DEV_MODE
        devCaptureDetail::applyParameterOverrides<ParametersType>(
            uiManager_,
            DefinitionType::definitionId,
            elementFlowId,
            elementID_,
            params_);
#endif
        BuildContext buildContext{
            uiManager_,
            elementID_,
            params_
        };
        elementDefinition_->buildElement(buildContext);
    }
}

} // namespace FlowUi
