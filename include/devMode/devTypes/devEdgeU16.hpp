#pragma once

#include <cstddef>
#include <cstdint>
#include <limits>
#include <string_view>

#include "clay.h"
#include "devMode/devRuntime.hpp"
#include "devMode/registry.hpp"

namespace FlowUi::devMode {

struct DevEdgeU16TypeInfo {
	uint64_t typeHash = 0u;
	std::string_view typeName{};
	std::string_view firstFieldName{};
	std::string_view secondFieldName{};
	std::string_view thirdFieldName{};
	std::string_view fourthFieldName{};
	std::string_view fifthFieldName{};
	bool fifthFieldUsed = false;
};

inline constexpr DevEdgeU16TypeInfo kDevEdgeU16TypeInfos[] = {
	{
		typeHash<Clay_Padding>(),
		"Clay_Padding",
		"left",
		"right",
		"top",
		"bottom",
		"unused",
		false,
	},
	{
		typeHash<Clay_BorderWidth>(),
		"Clay_BorderWidth",
		"left",
		"right",
		"top",
		"bottom",
		"betweenChildren",
		true,
	},
};

inline const DevEdgeU16TypeInfo* findDevEdgeU16TypeInfo(uint64_t fieldTypeHash) {
	for (const DevEdgeU16TypeInfo& info : kDevEdgeU16TypeInfos)
	{
		if (info.typeHash == fieldTypeHash)
		{
			return &info;
		}
	}
	return nullptr;
}

inline bool isDevEdgeU16TypeHash(uint64_t fieldTypeHash) {
	return findDevEdgeU16TypeInfo(fieldTypeHash) != nullptr;
}

inline bool tryNormalizeDevEdgeU16Component(int64_t value, uint16_t& outValue) {
	if (value < 0 || value > static_cast<int64_t>(std::numeric_limits<uint16_t>::max()))
	{
		return false;
	}
	outValue = static_cast<uint16_t>(value);
	return true;
}

inline bool tryMakeDevEdgeU16Value(
	uint64_t fieldTypeHash,
	int64_t first,
	int64_t second,
	int64_t third,
	int64_t fourth,
	int64_t fifth,
	DevEdgeU16Value& outValue) {
	const DevEdgeU16TypeInfo* info = findDevEdgeU16TypeInfo(fieldTypeHash);
	if (info == nullptr)
	{
		return false;
	}

	uint16_t normalizedFirst = 0u;
	uint16_t normalizedSecond = 0u;
	uint16_t normalizedThird = 0u;
	uint16_t normalizedFourth = 0u;
	uint16_t normalizedFifth = 0u;
	if (
		!tryNormalizeDevEdgeU16Component(first, normalizedFirst) ||
		!tryNormalizeDevEdgeU16Component(second, normalizedSecond) ||
		!tryNormalizeDevEdgeU16Component(third, normalizedThird) ||
		!tryNormalizeDevEdgeU16Component(fourth, normalizedFourth) ||
		!tryNormalizeDevEdgeU16Component(fifth, normalizedFifth))
	{
		return false;
	}

	if (!info->fifthFieldUsed)
	{
		normalizedFifth = 0u;
	}

	outValue = DevEdgeU16Value{
		.first = normalizedFirst,
		.second = normalizedSecond,
		.third = normalizedThird,
		.fourth = normalizedFourth,
		.fifth = normalizedFifth,
	};
	return true;
}

inline bool tryCaptureDevEdgeU16Value(const Clay_Padding& source, DevEdgeU16Value& outValue) {
	return tryMakeDevEdgeU16Value(
		typeHash<Clay_Padding>(),
		static_cast<int64_t>(source.left),
		static_cast<int64_t>(source.right),
		static_cast<int64_t>(source.top),
		static_cast<int64_t>(source.bottom),
		0,
		outValue);
}

inline bool tryCaptureDevEdgeU16Value(const Clay_BorderWidth& source, DevEdgeU16Value& outValue) {
	return tryMakeDevEdgeU16Value(
		typeHash<Clay_BorderWidth>(),
		static_cast<int64_t>(source.left),
		static_cast<int64_t>(source.right),
		static_cast<int64_t>(source.top),
		static_cast<int64_t>(source.bottom),
		static_cast<int64_t>(source.betweenChildren),
		outValue);
}

inline bool tryApplyDevEdgeU16Value(const DevEdgeU16Value& source, Clay_Padding& outValue) {
	DevEdgeU16Value normalized{};
	if (!tryMakeDevEdgeU16Value(
		typeHash<Clay_Padding>(),
		static_cast<int64_t>(source.first),
		static_cast<int64_t>(source.second),
		static_cast<int64_t>(source.third),
		static_cast<int64_t>(source.fourth),
		static_cast<int64_t>(source.fifth),
		normalized))
	{
		return false;
	}

	outValue = Clay_Padding{
		.left = normalized.first,
		.right = normalized.second,
		.top = normalized.third,
		.bottom = normalized.fourth,
	};
	return true;
}

inline bool tryApplyDevEdgeU16Value(const DevEdgeU16Value& source, Clay_BorderWidth& outValue) {
	DevEdgeU16Value normalized{};
	if (!tryMakeDevEdgeU16Value(
		typeHash<Clay_BorderWidth>(),
		static_cast<int64_t>(source.first),
		static_cast<int64_t>(source.second),
		static_cast<int64_t>(source.third),
		static_cast<int64_t>(source.fourth),
		static_cast<int64_t>(source.fifth),
		normalized))
	{
		return false;
	}

	outValue = Clay_BorderWidth{
		.left = normalized.first,
		.right = normalized.second,
		.top = normalized.third,
		.bottom = normalized.fourth,
		.betweenChildren = normalized.fifth,
	};
	return true;
}

} // namespace FlowUi::devMode
