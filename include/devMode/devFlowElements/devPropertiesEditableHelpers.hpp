#pragma once

#include "devMode/devFlowElements/common.hpp"
#include "devMode/devTypes/devEnum1.hpp"
#include "devMode/devTypes/devEnum2.hpp"
#include "devMode/devTypes/devFloat1.hpp"
#include "devMode/devTypes/devFloat2.hpp"
#include "devMode/devTypes/devFloat4.hpp"
#include "devMode/devTypes/devEdgeU16.hpp"
#include "devMode/devTypes/devTaggedUnion.hpp"
#include "devMode/devTypes/devCompositeStruct.hpp"
#include "devMode/devFlowElements/devPropertiesSelection.hpp"

inline bool devFieldTypeIsBool(uint64_t fieldTypeHash) {
	return fieldTypeHash == FlowUi::devMode::typeHash<bool>();
}

inline bool devFieldTypeIsString(uint64_t fieldTypeHash) {
	return fieldTypeHash == FlowUi::devMode::typeHash<std::string>();
}

inline bool devFieldTypeIsIntegral(uint64_t fieldTypeHash) {
	return
		fieldTypeHash == FlowUi::devMode::typeHash<int8_t>() ||
		fieldTypeHash == FlowUi::devMode::typeHash<int16_t>() ||
		fieldTypeHash == FlowUi::devMode::typeHash<int32_t>() ||
		fieldTypeHash == FlowUi::devMode::typeHash<int64_t>() ||
		fieldTypeHash == FlowUi::devMode::typeHash<uint8_t>() ||
		fieldTypeHash == FlowUi::devMode::typeHash<uint16_t>() ||
		fieldTypeHash == FlowUi::devMode::typeHash<uint32_t>() ||
		fieldTypeHash == FlowUi::devMode::typeHash<uint64_t>();
}

inline bool devFieldTypeIsFloating(uint64_t fieldTypeHash) {
	return
		fieldTypeHash == FlowUi::devMode::typeHash<float>() ||
		fieldTypeHash == FlowUi::devMode::typeHash<double>() ||
		FlowUi::devMode::isDevFloat1TypeHash(fieldTypeHash);
}

inline bool devFieldTypeIsEnum1(uint64_t fieldTypeHash) {
	return FlowUi::devMode::isDevEnum1TypeHash(fieldTypeHash);
}

inline bool devFieldTypeIsEnum2(uint64_t fieldTypeHash) {
	return FlowUi::devMode::isDevEnum2TypeHash(fieldTypeHash);
}

inline bool devFieldTypeIsFloat2(uint64_t fieldTypeHash) {
	return FlowUi::devMode::isDevFloat2TypeHash(fieldTypeHash);
}

inline bool devFieldTypeIsFloat4(uint64_t fieldTypeHash) {
	return FlowUi::devMode::isDevFloat4TypeHash(fieldTypeHash);
}

inline bool devFieldTypeIsEdgeU16(uint64_t fieldTypeHash) {
	return FlowUi::devMode::isDevEdgeU16TypeHash(fieldTypeHash);
}

inline bool devFieldTypeIsTaggedUnion(uint64_t fieldTypeHash) {
	return FlowUi::devMode::isDevTaggedUnionTypeHash(fieldTypeHash);
}

inline bool devFieldTypeIsCompositeStruct(uint64_t fieldTypeHash) {
	return FlowUi::devMode::isDevCompositeStructTypeHash(fieldTypeHash);
}

inline std::string devFieldTypeDisplayName(
	uint64_t fieldTypeHash,
	std::string_view fallbackTypeToken = {}) {
	using FlowUi::devMode::typeHash;

	if (fieldTypeHash == typeHash<bool>())
	{
		return "bool";
	}
	if (fieldTypeHash == typeHash<std::string>())
	{
		return "std::string";
	}
	if (fieldTypeHash == typeHash<int8_t>())
	{
		return "int8_t";
	}
	if (fieldTypeHash == typeHash<int16_t>())
	{
		return "int16_t";
	}
	if (fieldTypeHash == typeHash<int32_t>())
	{
		return "int32_t";
	}
	if (fieldTypeHash == typeHash<int64_t>())
	{
		return "int64_t";
	}
	if (fieldTypeHash == typeHash<uint8_t>())
	{
		return "uint8_t";
	}
	if (fieldTypeHash == typeHash<uint16_t>())
	{
		return "uint16_t";
	}
	if (fieldTypeHash == typeHash<uint32_t>())
	{
		return "uint32_t";
	}
	if (fieldTypeHash == typeHash<uint64_t>())
	{
		return "uint64_t";
	}
	if (fieldTypeHash == typeHash<float>())
	{
		return "float";
	}
	if (fieldTypeHash == typeHash<double>())
	{
		return "double";
	}
	if (const FlowUi::devMode::DevFloat1TypeInfo* info = FlowUi::devMode::findDevFloat1TypeInfo(fieldTypeHash))
	{
		return std::string(info->typeName);
	}
	if (const FlowUi::devMode::DevEnum1TypeInfo* info = FlowUi::devMode::findDevEnum1TypeInfo(fieldTypeHash))
	{
		return std::string(info->typeName);
	}
	if (const FlowUi::devMode::DevEnum2TypeInfo* info = FlowUi::devMode::findDevEnum2TypeInfo(fieldTypeHash))
	{
		return std::string(info->typeName);
	}
	if (const FlowUi::devMode::DevFloat2TypeInfo* info = FlowUi::devMode::findDevFloat2TypeInfo(fieldTypeHash))
	{
		return std::string(info->typeName);
	}
	if (const FlowUi::devMode::DevFloat4TypeInfo* info = FlowUi::devMode::findDevFloat4TypeInfo(fieldTypeHash))
	{
		return std::string(info->typeName);
	}
	if (const FlowUi::devMode::DevEdgeU16TypeInfo* info = FlowUi::devMode::findDevEdgeU16TypeInfo(fieldTypeHash))
	{
		return std::string(info->typeName);
	}
	if (const FlowUi::devMode::DevTaggedUnionTypeInfo* info = FlowUi::devMode::findDevTaggedUnionTypeInfo(fieldTypeHash))
	{
		return std::string(info->typeName);
	}
	if (const FlowUi::devMode::DevCompositeStructTypeInfo* info = FlowUi::devMode::findDevCompositeStructTypeInfo(fieldTypeHash))
	{
		return std::string(info->typeName);
	}

	const FlowUi::devMode::DevRegistry& registry = FlowUi::devMode::DevRegistry::instance();
	if (const FlowUi::devMode::EnumDescriptor* descriptor = registry.findEnumByTypeHash(fieldTypeHash))
	{
		if (!descriptor->name.empty())
		{
			return descriptor->name;
		}
		if (!descriptor->typeToken.empty())
		{
			return descriptor->typeToken;
		}
	}
	if (const FlowUi::devMode::StructDescriptor* descriptor = registry.findStructByTypeHash(fieldTypeHash))
	{
		if (!descriptor->name.empty())
		{
			return descriptor->name;
		}
		if (!descriptor->typeToken.empty())
		{
			return descriptor->typeToken;
		}
	}
	if (!fallbackTypeToken.empty())
	{
		return std::string(fallbackTypeToken);
	}
	return "<unknown>";
}

inline uint64_t normalizedSelectionFlowId(const devPropertiesSelectionNode& selection) {
	if (selection.flowId != 0u)
	{
		return selection.flowId;
	}
	if (!selection.elementId.empty())
	{
		return FlowUi::toFlowId(selection.elementId);
	}
	return 0u;
}

inline std::string trimDevInputText(std::string_view text) {
	std::size_t begin = 0u;
	while (begin < text.size() && std::isspace(static_cast<unsigned char>(text[begin])) != 0)
	{
		++begin;
	}
	std::size_t end = text.size();
	while (end > begin && std::isspace(static_cast<unsigned char>(text[end - 1u])) != 0)
	{
		--end;
	}
	return std::string(text.substr(begin, end - begin));
}

inline bool tryParseInt64FromText(std::string_view text, int64_t& outValue) {
	const std::string trimmed = trimDevInputText(text);
	if (trimmed.empty())
	{
		return false;
	}

	errno = 0;
	char* end = nullptr;
	const long long parsed = std::strtoll(trimmed.c_str(), &end, 10);
	if (end == nullptr || *end != '\0' || errno == ERANGE)
	{
		return false;
	}

	if (
		parsed < static_cast<long long>(std::numeric_limits<int64_t>::min()) ||
		parsed > static_cast<long long>(std::numeric_limits<int64_t>::max()))
	{
		return false;
	}

	outValue = static_cast<int64_t>(parsed);
	return true;
}

inline bool tryParseDoubleFromText(std::string_view text, double& outValue) {
	const std::string trimmed = trimDevInputText(text);
	if (trimmed.empty())
	{
		return false;
	}

	errno = 0;
	char* end = nullptr;
	const double parsed = std::strtod(trimmed.c_str(), &end);
	if (end == nullptr || *end != '\0' || errno == ERANGE || !std::isfinite(parsed))
	{
		return false;
	}

	outValue = parsed;
	return true;
}

inline bool tryCoerceDevValueToBool(const FlowUi::devMode::DevValue& value, bool& outValue) {
	if (const bool* boolValue = std::get_if<bool>(&value))
	{
		outValue = *boolValue;
		return true;
	}
	if (const int64_t* intValue = std::get_if<int64_t>(&value))
	{
		outValue = (*intValue != 0);
		return true;
	}
	if (const double* doubleValue = std::get_if<double>(&value))
	{
		outValue = (*doubleValue != 0.0);
		return true;
	}
	return false;
}

inline bool tryCoerceDevValueToInt64(const FlowUi::devMode::DevValue& value, int64_t& outValue) {
	if (const int64_t* intValue = std::get_if<int64_t>(&value))
	{
		outValue = *intValue;
		return true;
	}
	if (const bool* boolValue = std::get_if<bool>(&value))
	{
		outValue = *boolValue ? int64_t{1} : int64_t{0};
		return true;
	}
	if (const double* doubleValue = std::get_if<double>(&value))
	{
		if (!std::isfinite(*doubleValue) || std::trunc(*doubleValue) != *doubleValue)
		{
			return false;
		}
		if (
			*doubleValue < static_cast<double>(std::numeric_limits<int64_t>::min()) ||
			*doubleValue > static_cast<double>(std::numeric_limits<int64_t>::max()))
		{
			return false;
		}
		outValue = static_cast<int64_t>(*doubleValue);
		return true;
	}
	return false;
}

inline bool tryCoerceDevValueToDouble(const FlowUi::devMode::DevValue& value, double& outValue) {
	if (const double* doubleValue = std::get_if<double>(&value))
	{
		if (!std::isfinite(*doubleValue))
		{
			return false;
		}
		outValue = *doubleValue;
		return true;
	}
	if (const int64_t* intValue = std::get_if<int64_t>(&value))
	{
		outValue = static_cast<double>(*intValue);
		return true;
	}
	if (const bool* boolValue = std::get_if<bool>(&value))
	{
		outValue = *boolValue ? 1.0 : 0.0;
		return true;
	}
	return false;
}

inline bool tryCoerceDevValueToEnum1Numeric(const FlowUi::devMode::DevValue& value, uint8_t& outValue) {
	if (const FlowUi::devMode::DevEnum1Value* enumValue = std::get_if<FlowUi::devMode::DevEnum1Value>(&value))
	{
		outValue = enumValue->numeric;
		return true;
	}
	if (const int64_t* intValue = std::get_if<int64_t>(&value))
	{
		return FlowUi::devMode::tryNormalizeDevEnum1Numeric(*intValue, outValue);
	}
	if (const double* doubleValue = std::get_if<double>(&value))
	{
		if (!std::isfinite(*doubleValue) || std::trunc(*doubleValue) != *doubleValue)
		{
			return false;
		}
		return FlowUi::devMode::tryNormalizeDevEnum1Numeric(static_cast<int64_t>(*doubleValue), outValue);
	}
	if (const bool* boolValue = std::get_if<bool>(&value))
	{
		outValue = *boolValue ? uint8_t{1} : uint8_t{0};
		return true;
	}
	return false;
}

inline bool tryCoerceDevValueToEnum2Value(
	uint64_t fieldTypeHash,
	const FlowUi::devMode::DevValue& value,
	FlowUi::devMode::DevEnum2Value& outValue) {
	if (const FlowUi::devMode::DevEnum2Value* enumValue = std::get_if<FlowUi::devMode::DevEnum2Value>(&value))
	{
		return FlowUi::devMode::tryMakeDevEnum2Value(
			fieldTypeHash,
			static_cast<int64_t>(enumValue->first.numeric),
			static_cast<int64_t>(enumValue->second.numeric),
			outValue);
	}
	return false;
}

inline bool tryCoerceDevValueToFloat2Value(
	uint64_t fieldTypeHash,
	const FlowUi::devMode::DevValue& value,
	FlowUi::devMode::DevFloat2Value& outValue) {
	if (const FlowUi::devMode::DevFloat2Value* floatValue = std::get_if<FlowUi::devMode::DevFloat2Value>(&value))
	{
		return FlowUi::devMode::tryMakeDevFloat2Value(fieldTypeHash, floatValue->first, floatValue->second, outValue);
	}
	return false;
}

inline bool tryCoerceDevValueToFloat4Value(
	uint64_t fieldTypeHash,
	const FlowUi::devMode::DevValue& value,
	FlowUi::devMode::DevFloat4Value& outValue) {
	if (const FlowUi::devMode::DevFloat4Value* floatValue = std::get_if<FlowUi::devMode::DevFloat4Value>(&value))
	{
		return FlowUi::devMode::tryMakeDevFloat4Value(
			fieldTypeHash,
			floatValue->first,
			floatValue->second,
			floatValue->third,
			floatValue->fourth,
			outValue);
	}
	return false;
}

inline bool tryCoerceDevValueToEdgeU16Value(
	uint64_t fieldTypeHash,
	const FlowUi::devMode::DevValue& value,
	FlowUi::devMode::DevEdgeU16Value& outValue) {
	if (const FlowUi::devMode::DevEdgeU16Value* edgeValue = std::get_if<FlowUi::devMode::DevEdgeU16Value>(&value))
	{
		return FlowUi::devMode::tryMakeDevEdgeU16Value(
			fieldTypeHash,
			static_cast<int64_t>(edgeValue->first),
			static_cast<int64_t>(edgeValue->second),
			static_cast<int64_t>(edgeValue->third),
			static_cast<int64_t>(edgeValue->fourth),
			static_cast<int64_t>(edgeValue->fifth),
			outValue);
	}
	return false;
}

inline bool tryCoerceDevValueToTaggedUnionValue(
	uint64_t fieldTypeHash,
	const FlowUi::devMode::DevValue& value,
	FlowUi::devMode::DevTaggedUnionValue& outValue) {
	if (const FlowUi::devMode::DevTaggedUnionValue* taggedUnionValue =
		std::get_if<FlowUi::devMode::DevTaggedUnionValue>(&value))
	{
		return FlowUi::devMode::tryMakeDevTaggedUnionValue(
			fieldTypeHash,
			static_cast<int64_t>(taggedUnionValue->tag.numeric),
			taggedUnionValue->minMax.first,
			taggedUnionValue->minMax.second,
			taggedUnionValue->percent,
			outValue);
	}
	return false;
}

inline bool tryCoerceDevValueToCompositeStructValue(
	uint64_t fieldTypeHash,
	const FlowUi::devMode::DevValue& value,
	FlowUi::devMode::DevCompositeStructValue& outValue) {
	if (const FlowUi::devMode::DevCompositeStructValue* compositeValue =
		std::get_if<FlowUi::devMode::DevCompositeStructValue>(&value))
	{
		return FlowUi::devMode::tryMakeDevCompositeStructValue(fieldTypeHash, *compositeValue, outValue);
	}
	return false;
}

inline bool devValuesEquivalentForEditableField(
	uint64_t fieldTypeHash,
	const FlowUi::devMode::DevValue& lhs,
	const FlowUi::devMode::DevValue& rhs) {
	if (devFieldTypeIsCompositeStruct(fieldTypeHash))
	{
		FlowUi::devMode::DevCompositeStructValue lhsComposite{};
		FlowUi::devMode::DevCompositeStructValue rhsComposite{};
		return
			tryCoerceDevValueToCompositeStructValue(fieldTypeHash, lhs, lhsComposite) &&
			tryCoerceDevValueToCompositeStructValue(fieldTypeHash, rhs, rhsComposite) &&
			lhsComposite == rhsComposite;
	}

	if (devFieldTypeIsTaggedUnion(fieldTypeHash))
	{
		FlowUi::devMode::DevTaggedUnionValue lhsTagged{};
		FlowUi::devMode::DevTaggedUnionValue rhsTagged{};
		if (
			!tryCoerceDevValueToTaggedUnionValue(fieldTypeHash, lhs, lhsTagged) ||
			!tryCoerceDevValueToTaggedUnionValue(fieldTypeHash, rhs, rhsTagged))
		{
			return false;
		}

		if (lhsTagged.tag != rhsTagged.tag)
		{
			return false;
		}

		const auto nearlyEqual = [](double a, double b) {
			const double diff = std::fabs(a - b);
			const double scale = std::max(1.0, std::max(std::fabs(a), std::fabs(b)));
			return diff <= (1e-9 * scale);
		};

		if (FlowUi::devMode::devSizingAxisTagUsesPercent(lhsTagged.tag.numeric))
		{
			return nearlyEqual(lhsTagged.percent, rhsTagged.percent);
		}

		return
			nearlyEqual(lhsTagged.minMax.first, rhsTagged.minMax.first) &&
			nearlyEqual(lhsTagged.minMax.second, rhsTagged.minMax.second);
	}

	if (devFieldTypeIsEdgeU16(fieldTypeHash))
	{
		FlowUi::devMode::DevEdgeU16Value lhsEdge{};
		FlowUi::devMode::DevEdgeU16Value rhsEdge{};
		return
			tryCoerceDevValueToEdgeU16Value(fieldTypeHash, lhs, lhsEdge) &&
			tryCoerceDevValueToEdgeU16Value(fieldTypeHash, rhs, rhsEdge) &&
			lhsEdge == rhsEdge;
	}

	if (devFieldTypeIsFloat4(fieldTypeHash))
	{
		FlowUi::devMode::DevFloat4Value lhsFloat{};
		FlowUi::devMode::DevFloat4Value rhsFloat{};
		if (
			!tryCoerceDevValueToFloat4Value(fieldTypeHash, lhs, lhsFloat) ||
			!tryCoerceDevValueToFloat4Value(fieldTypeHash, rhs, rhsFloat))
		{
			return false;
		}

		const auto nearlyEqual = [](double a, double b) {
			const double diff = std::fabs(a - b);
			const double scale = std::max(1.0, std::max(std::fabs(a), std::fabs(b)));
			return diff <= (1e-9 * scale);
		};
		return
			nearlyEqual(lhsFloat.first, rhsFloat.first) &&
			nearlyEqual(lhsFloat.second, rhsFloat.second) &&
			nearlyEqual(lhsFloat.third, rhsFloat.third) &&
			nearlyEqual(lhsFloat.fourth, rhsFloat.fourth);
	}

	if (devFieldTypeIsFloat2(fieldTypeHash))
	{
		FlowUi::devMode::DevFloat2Value lhsFloat{};
		FlowUi::devMode::DevFloat2Value rhsFloat{};
		if (
			!tryCoerceDevValueToFloat2Value(fieldTypeHash, lhs, lhsFloat) ||
			!tryCoerceDevValueToFloat2Value(fieldTypeHash, rhs, rhsFloat))
		{
			return false;
		}

		const double diffFirst = std::fabs(lhsFloat.first - rhsFloat.first);
		const double scaleFirst = std::max(1.0, std::max(std::fabs(lhsFloat.first), std::fabs(rhsFloat.first)));
		if (diffFirst > (1e-9 * scaleFirst))
		{
			return false;
		}
		const double diffSecond = std::fabs(lhsFloat.second - rhsFloat.second);
		const double scaleSecond = std::max(1.0, std::max(std::fabs(lhsFloat.second), std::fabs(rhsFloat.second)));
		return diffSecond <= (1e-9 * scaleSecond);
	}

	if (devFieldTypeIsEnum2(fieldTypeHash))
	{
		FlowUi::devMode::DevEnum2Value lhsEnum{};
		FlowUi::devMode::DevEnum2Value rhsEnum{};
		return
			tryCoerceDevValueToEnum2Value(fieldTypeHash, lhs, lhsEnum) &&
			tryCoerceDevValueToEnum2Value(fieldTypeHash, rhs, rhsEnum) &&
			lhsEnum == rhsEnum;
	}

	if (devFieldTypeIsEnum1(fieldTypeHash))
	{
		uint8_t lhsEnum = 0u;
		uint8_t rhsEnum = 0u;
		return
			tryCoerceDevValueToEnum1Numeric(lhs, lhsEnum) &&
			tryCoerceDevValueToEnum1Numeric(rhs, rhsEnum) &&
			lhsEnum == rhsEnum;
	}

	if (devFieldTypeIsString(fieldTypeHash))
	{
		const std::string* lhsText = std::get_if<std::string>(&lhs);
		const std::string* rhsText = std::get_if<std::string>(&rhs);
		return lhsText != nullptr && rhsText != nullptr && *lhsText == *rhsText;
	}

	if (devFieldTypeIsBool(fieldTypeHash))
	{
		bool lhsBool = false;
		bool rhsBool = false;
		return
			tryCoerceDevValueToBool(lhs, lhsBool) &&
			tryCoerceDevValueToBool(rhs, rhsBool) &&
			lhsBool == rhsBool;
	}

	if (devFieldTypeIsIntegral(fieldTypeHash))
	{
		int64_t lhsInt = 0;
		int64_t rhsInt = 0;
		return
			tryCoerceDevValueToInt64(lhs, lhsInt) &&
			tryCoerceDevValueToInt64(rhs, rhsInt) &&
			lhsInt == rhsInt;
	}

	if (devFieldTypeIsFloating(fieldTypeHash))
	{
		double lhsDouble = 0.0;
		double rhsDouble = 0.0;
		if (!tryCoerceDevValueToDouble(lhs, lhsDouble) || !tryCoerceDevValueToDouble(rhs, rhsDouble))
		{
			return false;
		}
		const double diff = std::fabs(lhsDouble - rhsDouble);
		const double scale = std::max(1.0, std::max(std::fabs(lhsDouble), std::fabs(rhsDouble)));
		return diff <= (1e-9 * scale);
	}

	return lhs == rhs;
}

inline std::optional<FlowUi::devMode::DevValue> findFallbackDefinitionParamValueFromLastSeenInstances(
	const FlowUi::devMode::DevRuntime& runtime,
	uint64_t definitionId,
	uint64_t fieldHash) {
	bool found = false;
	uint64_t bestFlowId = std::numeric_limits<uint64_t>::max();
	std::string bestElementId{};
	FlowUi::devMode::DevValue bestValue{};

	for (const auto& [scopeKey, snapshot] : runtime.lastSeenParamsByInstance())
	{
		if (scopeKey.definitionId != definitionId)
		{
			continue;
		}
		const auto it = snapshot.valuesByFieldHash.find(fieldHash);
		if (it == snapshot.valuesByFieldHash.end())
		{
			continue;
		}

		if (
			!found ||
			scopeKey.flowId < bestFlowId ||
			(scopeKey.flowId == bestFlowId && scopeKey.elementId < bestElementId))
		{
			found = true;
			bestFlowId = scopeKey.flowId;
			bestElementId = scopeKey.elementId;
			bestValue = it->second;
		}
	}

	if (!found)
	{
		return std::nullopt;
	}
	return bestValue;
}

inline std::string devValueToEditableText(const FlowUi::devMode::DevValue& value) {
	auto formatDouble = [](double numeric) -> std::string {
		std::ostringstream stream{};
		stream.precision(16);
		stream << numeric;
		return stream.str();
	};

	if (const std::string* textValue = std::get_if<std::string>(&value))
	{
		return *textValue;
	}
	if (const int64_t* intValue = std::get_if<int64_t>(&value))
	{
		return std::to_string(*intValue);
	}
	if (const double* doubleValue = std::get_if<double>(&value))
	{
		return formatDouble(*doubleValue);
	}
	if (const bool* boolValue = std::get_if<bool>(&value))
	{
		return *boolValue ? "true" : "false";
	}
	if (const FlowUi::devMode::DevEnum1Value* enumValue = std::get_if<FlowUi::devMode::DevEnum1Value>(&value))
	{
		return std::to_string(enumValue->numeric);
	}
	if (const FlowUi::devMode::DevEnum2Value* enumValue = std::get_if<FlowUi::devMode::DevEnum2Value>(&value))
	{
		return std::to_string(enumValue->first.numeric) + "," + std::to_string(enumValue->second.numeric);
	}
	if (const FlowUi::devMode::DevFloat2Value* floatValue = std::get_if<FlowUi::devMode::DevFloat2Value>(&value))
	{
		return formatDouble(floatValue->first) + "," + formatDouble(floatValue->second);
	}
	if (const FlowUi::devMode::DevFloat4Value* floatValue = std::get_if<FlowUi::devMode::DevFloat4Value>(&value))
	{
		return
			formatDouble(floatValue->first) + "," +
			formatDouble(floatValue->second) + "," +
			formatDouble(floatValue->third) + "," +
			formatDouble(floatValue->fourth);
	}
	if (const FlowUi::devMode::DevEdgeU16Value* edgeValue = std::get_if<FlowUi::devMode::DevEdgeU16Value>(&value))
	{
		return
			std::to_string(edgeValue->first) + "," +
			std::to_string(edgeValue->second) + "," +
			std::to_string(edgeValue->third) + "," +
			std::to_string(edgeValue->fourth) + "," +
			std::to_string(edgeValue->fifth);
	}
	if (const FlowUi::devMode::DevTaggedUnionValue* taggedUnionValue =
		std::get_if<FlowUi::devMode::DevTaggedUnionValue>(&value))
	{
		std::ostringstream stream{};
		stream.precision(16);
		std::string_view enumName{};
		if (!FlowUi::devMode::tryDevEnum1ValueToName(
			FlowUi::devMode::typeHash<Clay__SizingType>(),
			taggedUnionValue->tag.numeric,
			enumName))
		{
			enumName = "unknown";
		}

		stream << "type=" << enumName;
		if (FlowUi::devMode::devSizingAxisTagUsesPercent(taggedUnionValue->tag.numeric))
		{
			stream << ",percent=" << taggedUnionValue->percent;
		}
		else
		{
			stream << ",min=" << taggedUnionValue->minMax.first;
			stream << ",max=" << taggedUnionValue->minMax.second;
		}
		return stream.str();
	}
	if (const FlowUi::devMode::DevCompositeStructValue* compositeValue =
		std::get_if<FlowUi::devMode::DevCompositeStructValue>(&value))
	{
		const FlowUi::devMode::DevCompositeStructTypeInfo* info =
			FlowUi::devMode::findDevCompositeStructTypeInfo(compositeValue->typeHash);
		if (info != nullptr)
		{
			return std::string(info->typeName);
		}
		return std::string("composite_struct");
	}
	return {};
}

inline std::string devValueToEditableTextForField(
	uint64_t fieldTypeHash,
	const FlowUi::devMode::DevValue& value) {
	if (devFieldTypeIsCompositeStruct(fieldTypeHash))
	{
		FlowUi::devMode::DevCompositeStructValue compositeValue{};
		if (!tryCoerceDevValueToCompositeStructValue(fieldTypeHash, value, compositeValue))
		{
			return {};
		}

		auto enumNameOrNumeric = [](
			uint64_t enumTypeHash,
			uint8_t numeric) -> std::string {
			std::string_view name{};
			if (FlowUi::devMode::tryDevEnum1ValueToName(enumTypeHash, numeric, name))
			{
				return std::string(name);
			}
			return std::to_string(numeric);
		};

		auto sizingAxisSummary = [&](const FlowUi::devMode::DevTaggedUnionValue& axis) -> std::string {
			const std::string typeName = enumNameOrNumeric(
				FlowUi::devMode::typeHash<Clay__SizingType>(),
				axis.tag.numeric);
			if (FlowUi::devMode::devSizingAxisTagUsesPercent(axis.tag.numeric))
			{
				std::ostringstream stream{};
				stream.precision(16);
				stream << "type=" << typeName << ", percent=" << axis.percent;
				return stream.str();
			}

			std::ostringstream stream{};
			stream.precision(16);
			stream
				<< "type=" << typeName
				<< ", min=" << axis.minMax.first
				<< ", max=" << axis.minMax.second;
			return stream.str();
		};

		if (fieldTypeHash == FlowUi::devMode::typeHash<Clay_Sizing>())
		{
			return
				std::string("width{") + sizingAxisSummary(compositeValue.sizing.width) +
				"}, height{" + sizingAxisSummary(compositeValue.sizing.height) + "}";
		}

		if (fieldTypeHash == FlowUi::devMode::typeHash<Clay_LayoutConfig>())
		{
			return
				std::string("layoutDirection=") +
				enumNameOrNumeric(
					FlowUi::devMode::typeHash<Clay_LayoutDirection>(),
					compositeValue.layoutConfig.layoutDirection.numeric) +
				", childGap=" + std::to_string(compositeValue.layoutConfig.childGap);
		}

		if (fieldTypeHash == FlowUi::devMode::typeHash<Clay_TextElementConfig>())
		{
			return
				std::string("fontId=") + std::to_string(compositeValue.textElementConfig.fontId) +
				", fontSize=" + std::to_string(compositeValue.textElementConfig.fontSize) +
				", wrapMode=" +
				enumNameOrNumeric(
					FlowUi::devMode::typeHash<Clay_TextElementConfigWrapMode>(),
					compositeValue.textElementConfig.wrapMode.numeric);
		}

		if (fieldTypeHash == FlowUi::devMode::typeHash<Clay_FloatingElementConfig>())
		{
			return
				std::string("attachTo=") +
				enumNameOrNumeric(
					FlowUi::devMode::typeHash<Clay_FloatingAttachToElement>(),
					compositeValue.floatingElementConfig.attachTo.numeric) +
				", clipTo=" +
				enumNameOrNumeric(
					FlowUi::devMode::typeHash<Clay_FloatingClipToElement>(),
					compositeValue.floatingElementConfig.clipTo.numeric) +
				", zIndex=" + std::to_string(compositeValue.floatingElementConfig.zIndex);
		}

		if (fieldTypeHash == FlowUi::devMode::typeHash<Clay_ClipElementConfig>())
		{
			return
				std::string("horizontal=") +
				(compositeValue.clipElementConfig.horizontal ? "true" : "false") +
				", vertical=" +
				(compositeValue.clipElementConfig.vertical ? "true" : "false") +
				", scrollInputDisabled=" +
				(compositeValue.clipElementConfig.scrollInputDisabled ? "true" : "false");
		}

		if (fieldTypeHash == FlowUi::devMode::typeHash<Clay_BorderElementConfig>())
		{
			return
				std::string("width.left=") + std::to_string(compositeValue.borderElementConfig.width.first) +
				", width.right=" + std::to_string(compositeValue.borderElementConfig.width.second) +
				", width.top=" + std::to_string(compositeValue.borderElementConfig.width.third) +
				", width.bottom=" + std::to_string(compositeValue.borderElementConfig.width.fourth);
		}

		if (fieldTypeHash == FlowUi::devMode::typeHash<Clay_ElementDeclaration>())
		{
			return
				std::string("id=") + std::to_string(compositeValue.elementDeclaration.id.id) +
				", layoutDirection=" +
				enumNameOrNumeric(
					FlowUi::devMode::typeHash<Clay_LayoutDirection>(),
					compositeValue.elementDeclaration.layout.layoutDirection.numeric);
		}
	}

	if (devFieldTypeIsTaggedUnion(fieldTypeHash))
	{
		FlowUi::devMode::DevTaggedUnionValue taggedUnionValue{};
		if (!tryCoerceDevValueToTaggedUnionValue(fieldTypeHash, value, taggedUnionValue))
		{
			return {};
		}

		const FlowUi::devMode::DevTaggedUnionTypeInfo* info = FlowUi::devMode::findDevTaggedUnionTypeInfo(fieldTypeHash);
		if (info == nullptr)
		{
			return {};
		}

		auto formatDouble = [](double numeric) -> std::string {
			std::ostringstream stream{};
			stream.precision(16);
			stream << numeric;
			return stream.str();
		};

		std::string_view enumName{};
		if (!FlowUi::devMode::tryDevEnum1ValueToName(
			info->tagEnumTypeHash,
			taggedUnionValue.tag.numeric,
			enumName))
		{
			return {};
		}

		std::string text = std::string(info->tagFieldName) + "=" + std::string(enumName);
		if (FlowUi::devMode::devSizingAxisTagUsesPercent(taggedUnionValue.tag.numeric))
		{
			text += ", " + std::string(info->percentFieldName) + "=" + formatDouble(taggedUnionValue.percent);
		}
		else
		{
			text += ", " + std::string(info->minFieldName) + "=" + formatDouble(taggedUnionValue.minMax.first);
			text += ", " + std::string(info->maxFieldName) + "=" + formatDouble(taggedUnionValue.minMax.second);
		}
		return text;
	}

	if (FlowUi::devMode::isDevFloat1TypeHash(fieldTypeHash))
	{
		double numericValue = 0.0;
		if (!tryCoerceDevValueToDouble(value, numericValue))
		{
			return {};
		}

		double normalized = 0.0;
		if (!FlowUi::devMode::tryMakeDevFloat1Value(fieldTypeHash, numericValue, normalized))
		{
			return {};
		}

		const FlowUi::devMode::DevFloat1TypeInfo* info = FlowUi::devMode::findDevFloat1TypeInfo(fieldTypeHash);
		if (info == nullptr)
		{
			return {};
		}

		std::ostringstream stream{};
		stream.precision(16);
		stream << normalized;
		return std::string(info->valueFieldName) + "=" + stream.str();
	}

	if (devFieldTypeIsEdgeU16(fieldTypeHash))
	{
		FlowUi::devMode::DevEdgeU16Value edgeValue{};
		if (!tryCoerceDevValueToEdgeU16Value(fieldTypeHash, value, edgeValue))
		{
			return {};
		}

		const FlowUi::devMode::DevEdgeU16TypeInfo* info = FlowUi::devMode::findDevEdgeU16TypeInfo(fieldTypeHash);
		if (info == nullptr)
		{
			return {};
		}

		std::string text =
			std::string(info->firstFieldName) + "=" + std::to_string(edgeValue.first) +
			", " + std::string(info->secondFieldName) + "=" + std::to_string(edgeValue.second) +
			", " + std::string(info->thirdFieldName) + "=" + std::to_string(edgeValue.third) +
			", " + std::string(info->fourthFieldName) + "=" + std::to_string(edgeValue.fourth);
		if (info->fifthFieldUsed)
		{
			text += ", " + std::string(info->fifthFieldName) + "=" + std::to_string(edgeValue.fifth);
		}
		return text;
	}

	if (devFieldTypeIsFloat4(fieldTypeHash))
	{
		FlowUi::devMode::DevFloat4Value floatValue{};
		if (!tryCoerceDevValueToFloat4Value(fieldTypeHash, value, floatValue))
		{
			return {};
		}

		const FlowUi::devMode::DevFloat4TypeInfo* info = FlowUi::devMode::findDevFloat4TypeInfo(fieldTypeHash);
		if (info == nullptr)
		{
			return {};
		}

		auto formatDouble = [](double numeric) -> std::string {
			std::ostringstream stream{};
			stream.precision(16);
			stream << numeric;
			return stream.str();
		};

		return
			std::string(info->firstFieldName) + "=" + formatDouble(floatValue.first) +
			", " + std::string(info->secondFieldName) + "=" + formatDouble(floatValue.second) +
			", " + std::string(info->thirdFieldName) + "=" + formatDouble(floatValue.third) +
			", " + std::string(info->fourthFieldName) + "=" + formatDouble(floatValue.fourth);
	}

	if (devFieldTypeIsFloat2(fieldTypeHash))
	{
		FlowUi::devMode::DevFloat2Value floatValue{};
		if (!tryCoerceDevValueToFloat2Value(fieldTypeHash, value, floatValue))
		{
			return {};
		}

		const FlowUi::devMode::DevFloat2TypeInfo* info = FlowUi::devMode::findDevFloat2TypeInfo(fieldTypeHash);
		if (info == nullptr)
		{
			return {};
		}

		auto formatDouble = [](double numeric) -> std::string {
			std::ostringstream stream{};
			stream.precision(16);
			stream << numeric;
			return stream.str();
		};

		return
			std::string(info->firstFieldName) + "=" + formatDouble(floatValue.first) +
			", " + std::string(info->secondFieldName) + "=" + formatDouble(floatValue.second);
	}

	if (devFieldTypeIsEnum2(fieldTypeHash))
	{
		FlowUi::devMode::DevEnum2Value enumValue{};
		if (!tryCoerceDevValueToEnum2Value(fieldTypeHash, value, enumValue))
		{
			return {};
		}

		const FlowUi::devMode::DevEnum2TypeInfo* info = FlowUi::devMode::findDevEnum2TypeInfo(fieldTypeHash);
		if (info == nullptr)
		{
			return {};
		}

		std::string_view firstName{};
		std::string_view secondName{};
		if (!FlowUi::devMode::tryDevEnum2ValueToNames(fieldTypeHash, enumValue, firstName, secondName))
		{
			return {};
		}

		return
			std::string(info->firstFieldName) + "=" + std::string(firstName) +
			", " + std::string(info->secondFieldName) + "=" + std::string(secondName);
	}

	if (devFieldTypeIsEnum1(fieldTypeHash))
	{
		uint8_t numeric = 0u;
		if (!tryCoerceDevValueToEnum1Numeric(value, numeric))
		{
			return {};
		}

		std::string_view enumName{};
		if (FlowUi::devMode::tryDevEnum1ValueToName(fieldTypeHash, numeric, enumName))
		{
			return std::string(enumName);
		}
		return std::to_string(numeric);
	}

	return devValueToEditableText(value);
}

inline std::optional<FlowUi::devMode::DevValue> parseEditableTextToDevValue(
	uint64_t fieldTypeHash,
	std::string_view text) {
	if (devFieldTypeIsCompositeStruct(fieldTypeHash))
	{
		(void)fieldTypeHash;
		(void)text;
		return std::nullopt;
	}

	if (devFieldTypeIsTaggedUnion(fieldTypeHash))
	{
		const std::string trimmed = trimDevInputText(text);
		if (trimmed.empty())
		{
			return std::nullopt;
		}

		const FlowUi::devMode::DevTaggedUnionTypeInfo* info = FlowUi::devMode::findDevTaggedUnionTypeInfo(fieldTypeHash);
		if (info == nullptr)
		{
			return std::nullopt;
		}

		std::vector<std::string> components{};
		std::size_t segmentStart = 0u;
		while (true)
		{
			const std::size_t commaPos = trimmed.find(',', segmentStart);
			const std::string token = trimDevInputText(
				commaPos == std::string::npos
				? std::string_view(trimmed).substr(segmentStart)
				: std::string_view(trimmed).substr(segmentStart, commaPos - segmentStart));
			if (token.empty())
			{
				return std::nullopt;
			}
			components.push_back(token);
			if (commaPos == std::string::npos)
			{
				break;
			}
			segmentStart = commaPos + 1u;
		}

		std::unordered_map<std::string, std::string> valuesByLabel{};
		valuesByLabel.reserve(components.size());
		for (const std::string& token : components)
		{
			const std::size_t eqPos = token.find('=');
			if (eqPos == std::string::npos)
			{
				return std::nullopt;
			}

			const std::string label = trimDevInputText(std::string_view(token).substr(0u, eqPos));
			const std::string valueToken = trimDevInputText(std::string_view(token).substr(eqPos + 1u));
			if (label.empty() || valueToken.empty())
			{
				return std::nullopt;
			}
			if (valuesByLabel.find(label) != valuesByLabel.end())
			{
				return std::nullopt;
			}
			valuesByLabel.emplace(label, valueToken);
		}

		const auto tagIt = valuesByLabel.find(std::string(info->tagFieldName));
		if (tagIt == valuesByLabel.end())
		{
			return std::nullopt;
		}

		uint8_t tagNumeric = 0u;
		if (!FlowUi::devMode::tryDevEnum1NameToValue(info->tagEnumTypeHash, tagIt->second, tagNumeric))
		{
			int64_t parsedNumeric = 0;
			if (!tryParseInt64FromText(tagIt->second, parsedNumeric) ||
				!FlowUi::devMode::tryNormalizeDevTaggedUnionTagNumeric(info->tagEnumTypeHash, parsedNumeric, tagNumeric))
			{
				return std::nullopt;
			}
		}

		double minValue = 0.0;
		double maxValue = 0.0;
		double percentValue = 0.0;
		if (FlowUi::devMode::devSizingAxisTagUsesPercent(tagNumeric))
		{
			const auto percentIt = valuesByLabel.find(std::string(info->percentFieldName));
			if (percentIt == valuesByLabel.end() || !tryParseDoubleFromText(percentIt->second, percentValue))
			{
				return std::nullopt;
			}
		}
		else
		{
			const auto minIt = valuesByLabel.find(std::string(info->minFieldName));
			const auto maxIt = valuesByLabel.find(std::string(info->maxFieldName));
			if (minIt == valuesByLabel.end() || maxIt == valuesByLabel.end())
			{
				return std::nullopt;
			}
			if (!tryParseDoubleFromText(minIt->second, minValue) || !tryParseDoubleFromText(maxIt->second, maxValue))
			{
				return std::nullopt;
			}
		}

		FlowUi::devMode::DevTaggedUnionValue value{};
		if (!FlowUi::devMode::tryMakeDevTaggedUnionValue(
			fieldTypeHash,
			static_cast<int64_t>(tagNumeric),
			minValue,
			maxValue,
			percentValue,
			value))
		{
			return std::nullopt;
		}

		return FlowUi::devMode::DevValue{value};
	}

	if (FlowUi::devMode::isDevFloat1TypeHash(fieldTypeHash))
	{
		const std::string trimmed = trimDevInputText(text);
		if (trimmed.empty())
		{
			return std::nullopt;
		}

		double parsed = 0.0;
		if (!tryParseDoubleFromText(trimmed, parsed))
		{
			return std::nullopt;
		}

		double normalized = 0.0;
		if (!FlowUi::devMode::tryMakeDevFloat1Value(fieldTypeHash, parsed, normalized))
		{
			return std::nullopt;
		}

		return FlowUi::devMode::DevValue{normalized};
	}

	if (devFieldTypeIsEdgeU16(fieldTypeHash))
	{
		const std::string trimmed = trimDevInputText(text);
		if (trimmed.empty())
		{
			return std::nullopt;
		}

		const FlowUi::devMode::DevEdgeU16TypeInfo* info = FlowUi::devMode::findDevEdgeU16TypeInfo(fieldTypeHash);
		if (info == nullptr)
		{
			return std::nullopt;
		}

		auto tryParseEdgeComponentValue = [](std::string_view token, uint16_t& outValue) -> bool {
			const std::string normalizedToken = trimDevInputText(token);
			if (normalizedToken.empty())
			{
				return false;
			}

			int64_t parsed = 0;
			if (!tryParseInt64FromText(normalizedToken, parsed))
			{
				return false;
			}
			return FlowUi::devMode::tryNormalizeDevEdgeU16Component(parsed, outValue);
		};

		std::vector<std::string> components{};
		components.reserve(5u);
		std::size_t segmentStart = 0u;
		while (true)
		{
			const std::size_t commaPos = trimmed.find(',', segmentStart);
			const std::string token = trimDevInputText(
				commaPos == std::string::npos
				? std::string_view(trimmed).substr(segmentStart)
				: std::string_view(trimmed).substr(segmentStart, commaPos - segmentStart));
			if (token.empty())
			{
				return std::nullopt;
			}
			components.push_back(token);
			if (commaPos == std::string::npos)
			{
				break;
			}
			segmentStart = commaPos + 1u;
		}

		if (components.size() < 4u || components.size() > 5u)
		{
			return std::nullopt;
		}
		if (info->fifthFieldUsed && components.size() != 5u)
		{
			return std::nullopt;
		}

		uint16_t firstValue = 0u;
		uint16_t secondValue = 0u;
		uint16_t thirdValue = 0u;
		uint16_t fourthValue = 0u;
		uint16_t fifthValue = 0u;

		bool anyLabeled = false;
		bool anyUnlabeled = false;
		for (const std::string& token : components)
		{
			if (token.find('=') == std::string::npos)
			{
				anyUnlabeled = true;
			}
			else
			{
				anyLabeled = true;
			}
		}
		if (anyLabeled && anyUnlabeled)
		{
			return std::nullopt;
		}

		if (!anyLabeled)
		{
			if (
				!tryParseEdgeComponentValue(components[0], firstValue) ||
				!tryParseEdgeComponentValue(components[1], secondValue) ||
				!tryParseEdgeComponentValue(components[2], thirdValue) ||
				!tryParseEdgeComponentValue(components[3], fourthValue))
			{
				return std::nullopt;
			}
			if (components.size() == 5u && !tryParseEdgeComponentValue(components[4], fifthValue))
			{
				return std::nullopt;
			}
		}
		else
		{
			std::unordered_map<std::string, std::string> valuesByLabel{};
			valuesByLabel.reserve(5u);

			for (const std::string& token : components)
			{
				const std::size_t eqPos = token.find('=');
				if (eqPos == std::string::npos)
				{
					return std::nullopt;
				}

				const std::string label = trimDevInputText(std::string_view(token).substr(0u, eqPos));
				const std::string value = trimDevInputText(std::string_view(token).substr(eqPos + 1u));
				if (label.empty() || value.empty())
				{
					return std::nullopt;
				}
				if (valuesByLabel.find(label) != valuesByLabel.end())
				{
					return std::nullopt;
				}
				valuesByLabel.emplace(label, value);
			}

			auto tryParseNamed = [&](
				std::string_view label,
				uint16_t& outValue) -> bool {
				const auto it = valuesByLabel.find(std::string(label));
				if (it == valuesByLabel.end())
				{
					return false;
				}
				return tryParseEdgeComponentValue(it->second, outValue);
			};

			if (
				!tryParseNamed(info->firstFieldName, firstValue) ||
				!tryParseNamed(info->secondFieldName, secondValue) ||
				!tryParseNamed(info->thirdFieldName, thirdValue) ||
				!tryParseNamed(info->fourthFieldName, fourthValue))
			{
				return std::nullopt;
			}

			const auto fifthIt = valuesByLabel.find(std::string(info->fifthFieldName));
			if (fifthIt != valuesByLabel.end())
			{
				if (!tryParseEdgeComponentValue(fifthIt->second, fifthValue))
				{
					return std::nullopt;
				}
			}
			else if (info->fifthFieldUsed)
			{
				return std::nullopt;
			}
		}

		FlowUi::devMode::DevEdgeU16Value value{};
		if (!FlowUi::devMode::tryMakeDevEdgeU16Value(
			fieldTypeHash,
			static_cast<int64_t>(firstValue),
			static_cast<int64_t>(secondValue),
			static_cast<int64_t>(thirdValue),
			static_cast<int64_t>(fourthValue),
			static_cast<int64_t>(fifthValue),
			value))
		{
			return std::nullopt;
		}
		return FlowUi::devMode::DevValue{value};
	}

	if (devFieldTypeIsFloat4(fieldTypeHash))
	{
		const std::string trimmed = trimDevInputText(text);
		if (trimmed.empty())
		{
			return std::nullopt;
		}

		const FlowUi::devMode::DevFloat4TypeInfo* info = FlowUi::devMode::findDevFloat4TypeInfo(fieldTypeHash);
		if (info == nullptr)
		{
			return std::nullopt;
		}

		auto tryParseFloatComponentValue = [](std::string_view token, double& outValue) -> bool {
			const std::string normalizedToken = trimDevInputText(token);
			if (normalizedToken.empty())
			{
				return false;
			}

			double parsed = 0.0;
			if (!tryParseDoubleFromText(normalizedToken, parsed))
			{
				return false;
			}
			return FlowUi::devMode::tryNormalizeDevFloat4Component(parsed, outValue);
		};

		std::vector<std::string> components{};
		components.reserve(4u);
		std::size_t segmentStart = 0u;
		while (true)
		{
			const std::size_t commaPos = trimmed.find(',', segmentStart);
			const std::string token = trimDevInputText(
				commaPos == std::string::npos
				? std::string_view(trimmed).substr(segmentStart)
				: std::string_view(trimmed).substr(segmentStart, commaPos - segmentStart));
			if (token.empty())
			{
				return std::nullopt;
			}
			components.push_back(token);
			if (commaPos == std::string::npos)
			{
				break;
			}
			segmentStart = commaPos + 1u;
		}
		if (components.size() != 4u)
		{
			return std::nullopt;
		}

		double firstValue = 0.0;
		double secondValue = 0.0;
		double thirdValue = 0.0;
		double fourthValue = 0.0;

		bool anyLabeled = false;
		bool anyUnlabeled = false;
		for (const std::string& token : components)
		{
			if (token.find('=') == std::string::npos)
			{
				anyUnlabeled = true;
			}
			else
			{
				anyLabeled = true;
			}
		}
		if (anyLabeled && anyUnlabeled)
		{
			return std::nullopt;
		}

		if (!anyLabeled)
		{
			if (
				!tryParseFloatComponentValue(components[0], firstValue) ||
				!tryParseFloatComponentValue(components[1], secondValue) ||
				!tryParseFloatComponentValue(components[2], thirdValue) ||
				!tryParseFloatComponentValue(components[3], fourthValue))
			{
				return std::nullopt;
			}
		}
		else
		{
			std::unordered_map<std::string, std::string> valuesByLabel{};
			valuesByLabel.reserve(4u);

			for (const std::string& token : components)
			{
				const std::size_t eqPos = token.find('=');
				if (eqPos == std::string::npos)
				{
					return std::nullopt;
				}

				const std::string label = trimDevInputText(std::string_view(token).substr(0u, eqPos));
				const std::string value = trimDevInputText(std::string_view(token).substr(eqPos + 1u));
				if (label.empty() || value.empty())
				{
					return std::nullopt;
				}
				if (valuesByLabel.find(label) != valuesByLabel.end())
				{
					return std::nullopt;
				}
				valuesByLabel.emplace(label, value);
			}

			auto tryParseNamed = [&](
				std::string_view label,
				double& outValue) -> bool {
				const auto it = valuesByLabel.find(std::string(label));
				if (it == valuesByLabel.end())
				{
					return false;
				}
				return tryParseFloatComponentValue(it->second, outValue);
			};

			if (
				!tryParseNamed(info->firstFieldName, firstValue) ||
				!tryParseNamed(info->secondFieldName, secondValue) ||
				!tryParseNamed(info->thirdFieldName, thirdValue) ||
				!tryParseNamed(info->fourthFieldName, fourthValue))
			{
				return std::nullopt;
			}
		}

		FlowUi::devMode::DevFloat4Value value{};
		if (!FlowUi::devMode::tryMakeDevFloat4Value(
			fieldTypeHash,
			firstValue,
			secondValue,
			thirdValue,
			fourthValue,
			value))
		{
			return std::nullopt;
		}
		return FlowUi::devMode::DevValue{value};
	}

	if (devFieldTypeIsFloat2(fieldTypeHash))
	{
		const std::string trimmed = trimDevInputText(text);
		if (trimmed.empty())
		{
			return std::nullopt;
		}

		const FlowUi::devMode::DevFloat2TypeInfo* info = FlowUi::devMode::findDevFloat2TypeInfo(fieldTypeHash);
		if (info == nullptr)
		{
			return std::nullopt;
		}

		auto tryParseFloatComponentValue = [](std::string_view token, double& outValue) -> bool {
			const std::string normalizedToken = trimDevInputText(token);
			if (normalizedToken.empty())
			{
				return false;
			}

			double parsed = 0.0;
			if (!tryParseDoubleFromText(normalizedToken, parsed))
			{
				return false;
			}
			return FlowUi::devMode::tryNormalizeDevFloat2Component(parsed, outValue);
		};

		auto parseComponentPair = [&](std::string_view firstToken, std::string_view secondToken)
			-> std::optional<FlowUi::devMode::DevFloat2Value> {
			double firstNumeric = 0.0;
			double secondNumeric = 0.0;
			if (
				!tryParseFloatComponentValue(firstToken, firstNumeric) ||
				!tryParseFloatComponentValue(secondToken, secondNumeric))
			{
				return std::nullopt;
			}

			FlowUi::devMode::DevFloat2Value value{};
			if (!FlowUi::devMode::tryMakeDevFloat2Value(
				fieldTypeHash,
				firstNumeric,
				secondNumeric,
				value))
			{
				return std::nullopt;
			}
			return value;
		};

		const std::size_t commaPos = trimmed.find(',');
		if (commaPos == std::string::npos)
		{
			return std::nullopt;
		}

		const std::string firstRaw = trimDevInputText(std::string_view(trimmed).substr(0u, commaPos));
		const std::string secondRaw = trimDevInputText(std::string_view(trimmed).substr(commaPos + 1u));
		if (firstRaw.empty() || secondRaw.empty())
		{
			return std::nullopt;
		}

		const std::size_t firstEq = firstRaw.find('=');
		const std::size_t secondEq = secondRaw.find('=');
		if (firstEq == std::string::npos && secondEq == std::string::npos)
		{
			if (const auto parsed = parseComponentPair(firstRaw, secondRaw); parsed.has_value())
			{
				return FlowUi::devMode::DevValue{*parsed};
			}
			return std::nullopt;
		}
		if (firstEq == std::string::npos || secondEq == std::string::npos)
		{
			return std::nullopt;
		}

		const std::string firstLabel = trimDevInputText(std::string_view(firstRaw).substr(0u, firstEq));
		const std::string firstValue = trimDevInputText(std::string_view(firstRaw).substr(firstEq + 1u));
		const std::string secondLabel = trimDevInputText(std::string_view(secondRaw).substr(0u, secondEq));
		const std::string secondValue = trimDevInputText(std::string_view(secondRaw).substr(secondEq + 1u));
		if (firstLabel.empty() || secondLabel.empty() || firstValue.empty() || secondValue.empty())
		{
			return std::nullopt;
		}

		if (firstLabel == info->firstFieldName && secondLabel == info->secondFieldName)
		{
			if (const auto parsed = parseComponentPair(firstValue, secondValue); parsed.has_value())
			{
				return FlowUi::devMode::DevValue{*parsed};
			}
			return std::nullopt;
		}
		if (firstLabel == info->secondFieldName && secondLabel == info->firstFieldName)
		{
			if (const auto parsed = parseComponentPair(secondValue, firstValue); parsed.has_value())
			{
				return FlowUi::devMode::DevValue{*parsed};
			}
			return std::nullopt;
		}
		return std::nullopt;
	}

	if (devFieldTypeIsEnum2(fieldTypeHash))
	{
		const std::string trimmed = trimDevInputText(text);
		if (trimmed.empty())
		{
			return std::nullopt;
		}

		const FlowUi::devMode::DevEnum2TypeInfo* info = FlowUi::devMode::findDevEnum2TypeInfo(fieldTypeHash);
		if (info == nullptr)
		{
			return std::nullopt;
		}

		auto tryParseEnumComponentValue = [](
			uint64_t enumTypeHash,
			std::string_view token,
			uint8_t& outNumeric) -> bool {
			const std::string normalizedToken = trimDevInputText(token);
			if (normalizedToken.empty())
			{
				return false;
			}

			if (FlowUi::devMode::tryDevEnum1NameToValue(enumTypeHash, normalizedToken, outNumeric))
			{
				return true;
			}

			int64_t numeric = 0;
			if (!tryParseInt64FromText(normalizedToken, numeric))
			{
				return false;
			}
			return FlowUi::devMode::tryNormalizeDevEnum2ComponentNumeric(enumTypeHash, numeric, outNumeric);
		};

		auto parseComponentPair = [&](std::string_view firstToken, std::string_view secondToken)
			-> std::optional<FlowUi::devMode::DevEnum2Value> {
			uint8_t firstNumeric = 0u;
			uint8_t secondNumeric = 0u;
			if (
				!tryParseEnumComponentValue(info->firstEnumTypeHash, firstToken, firstNumeric) ||
				!tryParseEnumComponentValue(info->secondEnumTypeHash, secondToken, secondNumeric))
			{
				return std::nullopt;
			}

			FlowUi::devMode::DevEnum2Value value{};
			if (!FlowUi::devMode::tryMakeDevEnum2Value(
				fieldTypeHash,
				static_cast<int64_t>(firstNumeric),
				static_cast<int64_t>(secondNumeric),
				value))
			{
				return std::nullopt;
			}
			return value;
		};

		const std::size_t commaPos = trimmed.find(',');
		if (commaPos == std::string::npos)
		{
			return std::nullopt;
		}

		const std::string firstRaw = trimDevInputText(std::string_view(trimmed).substr(0u, commaPos));
		const std::string secondRaw = trimDevInputText(std::string_view(trimmed).substr(commaPos + 1u));
		if (firstRaw.empty() || secondRaw.empty())
		{
			return std::nullopt;
		}

		const std::size_t firstEq = firstRaw.find('=');
		const std::size_t secondEq = secondRaw.find('=');
		if (firstEq == std::string::npos && secondEq == std::string::npos)
		{
			if (const auto parsed = parseComponentPair(firstRaw, secondRaw); parsed.has_value())
			{
				return FlowUi::devMode::DevValue{*parsed};
			}
			return std::nullopt;
		}
		if (firstEq == std::string::npos || secondEq == std::string::npos)
		{
			return std::nullopt;
		}

		const std::string firstLabel = trimDevInputText(std::string_view(firstRaw).substr(0u, firstEq));
		const std::string firstValue = trimDevInputText(std::string_view(firstRaw).substr(firstEq + 1u));
		const std::string secondLabel = trimDevInputText(std::string_view(secondRaw).substr(0u, secondEq));
		const std::string secondValue = trimDevInputText(std::string_view(secondRaw).substr(secondEq + 1u));
		if (firstLabel.empty() || secondLabel.empty() || firstValue.empty() || secondValue.empty())
		{
			return std::nullopt;
		}

		if (firstLabel == info->firstFieldName && secondLabel == info->secondFieldName)
		{
			if (const auto parsed = parseComponentPair(firstValue, secondValue); parsed.has_value())
			{
				return FlowUi::devMode::DevValue{*parsed};
			}
			return std::nullopt;
		}
		if (firstLabel == info->secondFieldName && secondLabel == info->firstFieldName)
		{
			if (const auto parsed = parseComponentPair(secondValue, firstValue); parsed.has_value())
			{
				return FlowUi::devMode::DevValue{*parsed};
			}
			return std::nullopt;
		}
		return std::nullopt;
	}

	if (devFieldTypeIsEnum1(fieldTypeHash))
	{
		const std::string trimmed = trimDevInputText(text);
		if (trimmed.empty())
		{
			return std::nullopt;
		}

		uint8_t numeric = 0u;
		if (FlowUi::devMode::tryDevEnum1NameToValue(fieldTypeHash, trimmed, numeric))
		{
			return FlowUi::devMode::DevValue{FlowUi::devMode::DevEnum1Value{.numeric = numeric}};
		}

		int64_t parsedNumeric = 0;
		if (!tryParseInt64FromText(trimmed, parsedNumeric))
		{
			return std::nullopt;
		}
		if (!FlowUi::devMode::tryNormalizeDevEnum1Numeric(parsedNumeric, numeric))
		{
			return std::nullopt;
		}

		std::string_view enumName{};
		if (!FlowUi::devMode::tryDevEnum1ValueToName(fieldTypeHash, numeric, enumName))
		{
			return std::nullopt;
		}

		return FlowUi::devMode::DevValue{FlowUi::devMode::DevEnum1Value{.numeric = numeric}};
	}

	if (devFieldTypeIsString(fieldTypeHash))
	{
		return FlowUi::devMode::DevValue{std::string(text)};
	}
	if (devFieldTypeIsIntegral(fieldTypeHash))
	{
		int64_t parsed = 0;
		if (!tryParseInt64FromText(text, parsed))
		{
			return std::nullopt;
		}
		return FlowUi::devMode::DevValue{parsed};
	}
	if (devFieldTypeIsFloating(fieldTypeHash))
	{
		double parsed = 0.0;
		if (!tryParseDoubleFromText(text, parsed))
		{
			return std::nullopt;
		}
		return FlowUi::devMode::DevValue{parsed};
	}
	return std::nullopt;
}

inline std::optional<FlowUi::devMode::DevValue> findCurrentEditableFieldValue(
	FlowUi::devMode::DevRuntime& runtime,
	const devPropertiesSelectionNode& selection,
	uint64_t fieldHash) {
	using FlowUi::devMode::DevValue;

	switch (selection.kind)
	{
	case devPropertiesSelectionKind::None:
		return std::nullopt;
	case devPropertiesSelectionKind::Instance:
		switch (selection.structScope)
		{
		case devPropertiesStructScope::Parameters:
		{
			if (const DevValue* value = runtime.findInstanceParamOverride(
				selection.definitionId,
				normalizedSelectionFlowId(selection),
				selection.elementId,
				fieldHash))
			{
				return *value;
			}
			if (const DevValue* value = runtime.findDefinitionParamOverride(selection.definitionId, fieldHash))
			{
				return *value;
			}
			if (const FlowUi::devMode::StructSnapshot* snapshot = runtime.findLastSeenParams(
				selection.definitionId,
				normalizedSelectionFlowId(selection),
				selection.elementId))
			{
				const auto it = snapshot->valuesByFieldHash.find(fieldHash);
				if (it != snapshot->valuesByFieldHash.end())
				{
					return it->second;
				}
			}
			return std::nullopt;
		}
		case devPropertiesStructScope::State:
		{
			if (const DevValue* value = runtime.findStateOverride(
				selection.definitionId,
				normalizedSelectionFlowId(selection),
				selection.elementId,
				fieldHash))
			{
				return *value;
			}
			if (const FlowUi::devMode::StructSnapshot* snapshot = runtime.findLastSeenState(
				selection.definitionId,
				normalizedSelectionFlowId(selection),
				selection.elementId))
			{
				const auto it = snapshot->valuesByFieldHash.find(fieldHash);
				if (it != snapshot->valuesByFieldHash.end())
				{
					return it->second;
				}
			}
			return std::nullopt;
		}
		case devPropertiesStructScope::Resources:
		{
			if (const DevValue* value = runtime.findResourceOverride(selection.definitionId, fieldHash))
			{
				return *value;
			}
			if (const FlowUi::devMode::StructSnapshot* snapshot = runtime.findLastSeenResources(selection.definitionId))
			{
				const auto it = snapshot->valuesByFieldHash.find(fieldHash);
				if (it != snapshot->valuesByFieldHash.end())
				{
					return it->second;
				}
			}
			return std::nullopt;
		}
		}
		break;
	case devPropertiesSelectionKind::Definition:
		switch (selection.structScope)
		{
		case devPropertiesStructScope::Parameters:
		{
			if (const DevValue* value = runtime.findDefinitionParamOverride(selection.definitionId, fieldHash))
			{
				return *value;
			}
			if (const std::optional<DevValue> fallback =
				findFallbackDefinitionParamValueFromLastSeenInstances(
					runtime,
					selection.definitionId,
					fieldHash);
				fallback.has_value())
			{
				return *fallback;
			}
			return std::nullopt;
		}
		case devPropertiesStructScope::State:
			return std::nullopt;
		case devPropertiesStructScope::Resources:
		{
			if (const DevValue* value = runtime.findResourceOverride(selection.definitionId, fieldHash))
			{
				return *value;
			}
			if (const FlowUi::devMode::StructSnapshot* snapshot = runtime.findLastSeenResources(selection.definitionId))
			{
				const auto it = snapshot->valuesByFieldHash.find(fieldHash);
				if (it != snapshot->valuesByFieldHash.end())
				{
					return it->second;
				}
			}
			return std::nullopt;
		}
		}
		break;
	}

	return std::nullopt;
}

inline void setEditableFieldOverride(
	FlowUi::devMode::DevRuntime& runtime,
	const devPropertiesSelectionNode& selection,
	uint64_t fieldHash,
	const FlowUi::devMode::DevValue& value) {
	switch (selection.kind)
	{
	case devPropertiesSelectionKind::None:
		return;
	case devPropertiesSelectionKind::Instance:
		switch (selection.structScope)
		{
		case devPropertiesStructScope::Parameters:
				runtime.setInstanceParamOverride(
					selection.definitionId,
					normalizedSelectionFlowId(selection),
					selection.elementId,
					fieldHash,
					value);
			return;
		case devPropertiesStructScope::State:
				runtime.setStateOverride(
					selection.definitionId,
					normalizedSelectionFlowId(selection),
					selection.elementId,
					fieldHash,
					value);
			return;
		case devPropertiesStructScope::Resources:
			runtime.setResourceOverride(selection.definitionId, fieldHash, value);
			return;
		}
		return;
	case devPropertiesSelectionKind::Definition:
		switch (selection.structScope)
		{
		case devPropertiesStructScope::Parameters:
			runtime.setDefinitionParamOverride(selection.definitionId, fieldHash, value);
			return;
		case devPropertiesStructScope::State:
			return;
		case devPropertiesStructScope::Resources:
			runtime.setResourceOverride(selection.definitionId, fieldHash, value);
			return;
		}
		return;
	}
}

inline std::string makeDevPropertiesFieldIdentity(
	const devPropertiesSelectionNode& selection,
	uint64_t fieldHash) {
	const std::string scopeText = std::to_string(static_cast<int>(selection.structScope));
	return
		std::to_string(selection.definitionId) +
		"/" +
		std::to_string(normalizedSelectionFlowId(selection)) +
		"/" +
		selection.elementId +
		"/" +
		scopeText +
		"/" +
		std::to_string(fieldHash);
}
