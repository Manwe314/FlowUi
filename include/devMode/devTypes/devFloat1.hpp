#pragma once

#include <cmath>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <string_view>

#include "clay.h"
#include "devMode/registry.hpp"

namespace FlowUi::devMode {

struct DevFloat1TypeInfo {
	uint64_t typeHash = 0u;
	std::string_view typeName{};
	std::string_view valueFieldName{};
};

inline constexpr DevFloat1TypeInfo kDevFloat1TypeInfos[] = {
	{
		typeHash<Clay_AspectRatioElementConfig>(),
		"Clay_AspectRatioElementConfig",
		"aspectRatio",
	},
};

inline const DevFloat1TypeInfo* findDevFloat1TypeInfo(uint64_t fieldTypeHash) {
	for (const DevFloat1TypeInfo& info : kDevFloat1TypeInfos)
	{
		if (info.typeHash == fieldTypeHash)
		{
			return &info;
		}
	}
	return nullptr;
}

inline bool isDevFloat1TypeHash(uint64_t fieldTypeHash) {
	return findDevFloat1TypeInfo(fieldTypeHash) != nullptr;
}

inline bool tryNormalizeDevFloat1Component(double value, double& outValue) {
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

inline bool tryMakeDevFloat1Value(
	uint64_t fieldTypeHash,
	double value,
	double& outValue) {
	if (!isDevFloat1TypeHash(fieldTypeHash))
	{
		return false;
	}
	return tryNormalizeDevFloat1Component(value, outValue);
}

inline bool tryCaptureDevFloat1Value(const Clay_AspectRatioElementConfig& source, double& outValue) {
	return tryMakeDevFloat1Value(
		typeHash<Clay_AspectRatioElementConfig>(),
		static_cast<double>(source.aspectRatio),
		outValue);
}

inline bool tryApplyDevFloat1Value(double source, Clay_AspectRatioElementConfig& outValue) {
	double normalized = 0.0;
	if (!tryMakeDevFloat1Value(typeHash<Clay_AspectRatioElementConfig>(), source, normalized))
	{
		return false;
	}

	outValue = Clay_AspectRatioElementConfig{
		.aspectRatio = static_cast<float>(normalized),
	};
	return true;
}

} // namespace FlowUi::devMode
