#pragma once

#include <cmath>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <string_view>
#include <type_traits>

#include "clay.h"
#include "devMode/devTypes/devEnum1.hpp"
#include "devMode/devTypes/devFloat2.hpp"
#include "devMode/devRuntime.hpp"
#include "devMode/registry.hpp"

namespace FlowUi::devMode {

struct DevTaggedUnionTypeInfo {
	uint64_t typeHash = 0u;
	std::string_view typeName{};
	std::string_view tagFieldName{};
	uint64_t tagEnumTypeHash = 0u;
	std::string_view minMaxFieldName{};
	std::string_view minFieldName{};
	std::string_view maxFieldName{};
	std::string_view percentFieldName{};
};

inline constexpr DevTaggedUnionTypeInfo kDevTaggedUnionTypeInfos[] = {
	{
		typeHash<Clay_SizingAxis>(),
		"Clay_SizingAxis",
		"type",
		typeHash<Clay__SizingType>(),
		"minMax",
		"min",
		"max",
		"percent",
	},
};

inline const DevTaggedUnionTypeInfo* findDevTaggedUnionTypeInfo(uint64_t fieldTypeHash) {
	for (const DevTaggedUnionTypeInfo& info : kDevTaggedUnionTypeInfos)
	{
		if (info.typeHash == fieldTypeHash)
		{
			return &info;
		}
	}
	return nullptr;
}

inline bool isDevTaggedUnionTypeHash(uint64_t fieldTypeHash) {
	return findDevTaggedUnionTypeInfo(fieldTypeHash) != nullptr;
}

inline bool devSizingAxisTagUsesPercent(uint8_t tagNumeric) {
	return tagNumeric == static_cast<uint8_t>(CLAY__SIZING_TYPE_PERCENT);
}

inline bool tryNormalizeDevTaggedUnionTagNumeric(
	uint64_t tagEnumTypeHash,
	int64_t numeric,
	uint8_t& outNumeric) {
	if (!tryNormalizeDevEnum1Numeric(numeric, outNumeric))
	{
		return false;
	}

	std::string_view enumName{};
	return tryDevEnum1ValueToName(tagEnumTypeHash, outNumeric, enumName);
}

inline bool tryNormalizeDevTaggedUnionPercent(double value, double& outValue) {
	if (!std::isfinite(value))
	{
		return false;
	}
	if (value < static_cast<double>(-std::numeric_limits<float>::max()) ||
		value > static_cast<double>(std::numeric_limits<float>::max()))
	{
		return false;
	}
	outValue = value;
	return true;
}

inline bool tryMakeDevTaggedUnionValue(
	uint64_t fieldTypeHash,
	int64_t tagNumeric,
	double min,
	double max,
	double percent,
	DevTaggedUnionValue& outValue) {
	const DevTaggedUnionTypeInfo* info = findDevTaggedUnionTypeInfo(fieldTypeHash);
	if (info == nullptr)
	{
		return false;
	}

	uint8_t normalizedTag = 0u;
	if (!tryNormalizeDevTaggedUnionTagNumeric(info->tagEnumTypeHash, tagNumeric, normalizedTag))
	{
		return false;
	}

	DevFloat2Value normalizedMinMax{};
	double normalizedPercent = 0.0;
	if (devSizingAxisTagUsesPercent(normalizedTag))
	{
		if (!tryNormalizeDevTaggedUnionPercent(percent, normalizedPercent))
		{
			return false;
		}
		normalizedMinMax = DevFloat2Value{};
	}
	else
	{
		if (!tryMakeDevFloat2Value(
			typeHash<Clay_SizingMinMax>(),
			min,
			max,
			normalizedMinMax))
		{
			return false;
		}
		normalizedPercent = 0.0;
	}

	outValue = DevTaggedUnionValue{
		.tag = DevEnum1Value{.numeric = normalizedTag},
		.minMax = normalizedMinMax,
		.percent = normalizedPercent,
	};
	return true;
}

inline bool tryCaptureDevTaggedUnionValue(const Clay_SizingAxis& source, DevTaggedUnionValue& outValue) {
	using UnderlyingTag = std::underlying_type_t<Clay__SizingType>;
	const uint8_t tagNumeric = static_cast<uint8_t>(static_cast<UnderlyingTag>(source.type));

	if (devSizingAxisTagUsesPercent(tagNumeric))
	{
		return tryMakeDevTaggedUnionValue(
			typeHash<Clay_SizingAxis>(),
			static_cast<int64_t>(tagNumeric),
			0.0,
			0.0,
			static_cast<double>(source.size.percent),
			outValue);
	}

	return tryMakeDevTaggedUnionValue(
		typeHash<Clay_SizingAxis>(),
		static_cast<int64_t>(tagNumeric),
		static_cast<double>(source.size.minMax.min),
		static_cast<double>(source.size.minMax.max),
		0.0,
		outValue);
}

inline bool tryApplyDevTaggedUnionValue(const DevTaggedUnionValue& source, Clay_SizingAxis& outValue) {
	DevTaggedUnionValue normalized{};
	if (!tryMakeDevTaggedUnionValue(
		typeHash<Clay_SizingAxis>(),
		static_cast<int64_t>(source.tag.numeric),
		source.minMax.first,
		source.minMax.second,
		source.percent,
		normalized))
	{
		return false;
	}

	outValue = Clay_SizingAxis{};
	outValue.type = static_cast<Clay__SizingType>(normalized.tag.numeric);
	if (devSizingAxisTagUsesPercent(normalized.tag.numeric))
	{
		outValue.size = {.percent = static_cast<float>(normalized.percent)};
	}
	else
	{
		outValue.size = {
			.minMax = Clay_SizingMinMax{
				.min = static_cast<float>(normalized.minMax.first),
				.max = static_cast<float>(normalized.minMax.second),
			},
		};
	}

	return true;
}

} // namespace FlowUi::devMode
