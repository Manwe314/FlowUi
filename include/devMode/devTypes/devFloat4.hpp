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

struct DevFloat4TypeInfo {
	uint64_t typeHash = 0u;
	std::string_view typeName{};
	std::string_view firstFieldName{};
	std::string_view secondFieldName{};
	std::string_view thirdFieldName{};
	std::string_view fourthFieldName{};
};

inline constexpr DevFloat4TypeInfo kDevFloat4TypeInfos[] = {
	{
		typeHash<Clay_Color>(),
		"Clay_Color",
		"r",
		"g",
		"b",
		"a",
	},
	{
		typeHash<Clay_CornerRadius>(),
		"Clay_CornerRadius",
		"topLeft",
		"topRight",
		"bottomLeft",
		"bottomRight",
	},
};

inline const DevFloat4TypeInfo* findDevFloat4TypeInfo(uint64_t fieldTypeHash) {
	for (const DevFloat4TypeInfo& info : kDevFloat4TypeInfos)
	{
		if (info.typeHash == fieldTypeHash)
		{
			return &info;
		}
	}
	return nullptr;
}

inline bool isDevFloat4TypeHash(uint64_t fieldTypeHash) {
	return findDevFloat4TypeInfo(fieldTypeHash) != nullptr;
}

inline bool tryNormalizeDevFloat4Component(double value, double& outValue) {
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

inline bool tryMakeDevFloat4Value(
	uint64_t fieldTypeHash,
	double first,
	double second,
	double third,
	double fourth,
	DevFloat4Value& outValue) {
	if (!isDevFloat4TypeHash(fieldTypeHash))
	{
		return false;
	}

	double normalizedFirst = 0.0;
	double normalizedSecond = 0.0;
	double normalizedThird = 0.0;
	double normalizedFourth = 0.0;
	if (
		!tryNormalizeDevFloat4Component(first, normalizedFirst) ||
		!tryNormalizeDevFloat4Component(second, normalizedSecond) ||
		!tryNormalizeDevFloat4Component(third, normalizedThird) ||
		!tryNormalizeDevFloat4Component(fourth, normalizedFourth))
	{
		return false;
	}

	outValue = DevFloat4Value{
		.first = normalizedFirst,
		.second = normalizedSecond,
		.third = normalizedThird,
		.fourth = normalizedFourth,
	};
	return true;
}

inline bool tryCaptureDevFloat4Value(const Clay_Color& source, DevFloat4Value& outValue) {
	return tryMakeDevFloat4Value(
		typeHash<Clay_Color>(),
		static_cast<double>(source.r),
		static_cast<double>(source.g),
		static_cast<double>(source.b),
		static_cast<double>(source.a),
		outValue);
}

inline bool tryCaptureDevFloat4Value(const Clay_CornerRadius& source, DevFloat4Value& outValue) {
	return tryMakeDevFloat4Value(
		typeHash<Clay_CornerRadius>(),
		static_cast<double>(source.topLeft),
		static_cast<double>(source.topRight),
		static_cast<double>(source.bottomLeft),
		static_cast<double>(source.bottomRight),
		outValue);
}

inline bool tryApplyDevFloat4Value(const DevFloat4Value& source, Clay_Color& outValue) {
	DevFloat4Value normalized{};
	if (!tryMakeDevFloat4Value(
		typeHash<Clay_Color>(),
		source.first,
		source.second,
		source.third,
		source.fourth,
		normalized))
	{
		return false;
	}

	outValue = Clay_Color{
		.r = static_cast<float>(normalized.first),
		.g = static_cast<float>(normalized.second),
		.b = static_cast<float>(normalized.third),
		.a = static_cast<float>(normalized.fourth),
	};
	return true;
}

inline bool tryApplyDevFloat4Value(const DevFloat4Value& source, Clay_CornerRadius& outValue) {
	DevFloat4Value normalized{};
	if (!tryMakeDevFloat4Value(
		typeHash<Clay_CornerRadius>(),
		source.first,
		source.second,
		source.third,
		source.fourth,
		normalized))
	{
		return false;
	}

	outValue = Clay_CornerRadius{
		.topLeft = static_cast<float>(normalized.first),
		.topRight = static_cast<float>(normalized.second),
		.bottomLeft = static_cast<float>(normalized.third),
		.bottomRight = static_cast<float>(normalized.fourth),
	};
	return true;
}

} // namespace FlowUi::devMode
