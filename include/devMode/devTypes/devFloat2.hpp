#pragma once

#include <cmath>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <string_view>

#include "clay.h"
#include "devMode/devRuntime.hpp"
#include "devMode/registry.hpp"

namespace FlowUi::devMode {

struct DevFloat2TypeInfo {
	uint64_t typeHash = 0u;
	std::string_view typeName{};
	std::string_view firstFieldName{};
	std::string_view secondFieldName{};
};

inline constexpr DevFloat2TypeInfo kDevFloat2TypeInfos[] = {
	{
		typeHash<Clay_Vector2>(),
		"Clay_Vector2",
		"x",
		"y",
	},
	{
		typeHash<Clay_Dimensions>(),
		"Clay_Dimensions",
		"width",
		"height",
	},
	{
		typeHash<Clay_SizingMinMax>(),
		"Clay_SizingMinMax",
		"min",
		"max",
	},
};

inline const DevFloat2TypeInfo* findDevFloat2TypeInfo(uint64_t fieldTypeHash) {
	for (const DevFloat2TypeInfo& info : kDevFloat2TypeInfos)
	{
		if (info.typeHash == fieldTypeHash)
		{
			return &info;
		}
	}
	return nullptr;
}

inline bool isDevFloat2TypeHash(uint64_t fieldTypeHash) {
	return findDevFloat2TypeInfo(fieldTypeHash) != nullptr;
}

inline bool tryNormalizeDevFloat2Component(double value, double& outValue) {
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

inline bool tryMakeDevFloat2Value(
	uint64_t fieldTypeHash,
	double first,
	double second,
	DevFloat2Value& outValue) {
	if (!isDevFloat2TypeHash(fieldTypeHash))
	{
		return false;
	}

	double normalizedFirst = 0.0;
	double normalizedSecond = 0.0;
	if (
		!tryNormalizeDevFloat2Component(first, normalizedFirst) ||
		!tryNormalizeDevFloat2Component(second, normalizedSecond))
	{
		return false;
	}

	outValue = DevFloat2Value{
		.first = normalizedFirst,
		.second = normalizedSecond,
	};
	return true;
}

inline bool tryCaptureDevFloat2Value(const Clay_Vector2& source, DevFloat2Value& outValue) {
	return tryMakeDevFloat2Value(
		typeHash<Clay_Vector2>(),
		static_cast<double>(source.x),
		static_cast<double>(source.y),
		outValue);
}

inline bool tryCaptureDevFloat2Value(const Clay_Dimensions& source, DevFloat2Value& outValue) {
	return tryMakeDevFloat2Value(
		typeHash<Clay_Dimensions>(),
		static_cast<double>(source.width),
		static_cast<double>(source.height),
		outValue);
}

inline bool tryCaptureDevFloat2Value(const Clay_SizingMinMax& source, DevFloat2Value& outValue) {
	return tryMakeDevFloat2Value(
		typeHash<Clay_SizingMinMax>(),
		static_cast<double>(source.min),
		static_cast<double>(source.max),
		outValue);
}

inline bool tryApplyDevFloat2Value(const DevFloat2Value& source, Clay_Vector2& outValue) {
	DevFloat2Value normalized{};
	if (!tryMakeDevFloat2Value(
		typeHash<Clay_Vector2>(),
		source.first,
		source.second,
		normalized))
	{
		return false;
	}

	outValue = Clay_Vector2{
		.x = static_cast<float>(normalized.first),
		.y = static_cast<float>(normalized.second),
	};
	return true;
}

inline bool tryApplyDevFloat2Value(const DevFloat2Value& source, Clay_Dimensions& outValue) {
	DevFloat2Value normalized{};
	if (!tryMakeDevFloat2Value(
		typeHash<Clay_Dimensions>(),
		source.first,
		source.second,
		normalized))
	{
		return false;
	}

	outValue = Clay_Dimensions{
		.width = static_cast<float>(normalized.first),
		.height = static_cast<float>(normalized.second),
	};
	return true;
}

inline bool tryApplyDevFloat2Value(const DevFloat2Value& source, Clay_SizingMinMax& outValue) {
	DevFloat2Value normalized{};
	if (!tryMakeDevFloat2Value(
		typeHash<Clay_SizingMinMax>(),
		source.first,
		source.second,
		normalized))
	{
		return false;
	}

	outValue = Clay_SizingMinMax{
		.min = static_cast<float>(normalized.first),
		.max = static_cast<float>(normalized.second),
	};
	return true;
}

} // namespace FlowUi::devMode
