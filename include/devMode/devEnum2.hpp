#pragma once

#include <cstddef>
#include <cstdint>
#include <string_view>
#include <type_traits>

#include "clay.h"
#include "devMode/devEnum1.hpp"
#include "devMode/devRuntime.hpp"
#include "devMode/registry.hpp"

namespace FlowUi::devMode {

static_assert(std::is_same_v<std::underlying_type_t<Clay_LayoutAlignmentX>, uint8_t>);
static_assert(std::is_same_v<std::underlying_type_t<Clay_LayoutAlignmentY>, uint8_t>);
static_assert(std::is_same_v<std::underlying_type_t<Clay_FloatingAttachPointType>, uint8_t>);

struct DevEnum2TypeInfo {
	uint64_t typeHash = 0u;
	std::string_view typeName{};
	std::string_view firstFieldName{};
	std::string_view secondFieldName{};
	uint64_t firstEnumTypeHash = 0u;
	uint64_t secondEnumTypeHash = 0u;
};

inline constexpr DevEnum2TypeInfo kDevEnum2TypeInfos[] = {
	{
		typeHash<Clay_ChildAlignment>(),
		"Clay_ChildAlignment",
		"x",
		"y",
		typeHash<Clay_LayoutAlignmentX>(),
		typeHash<Clay_LayoutAlignmentY>(),
	},
	{
		typeHash<Clay_FloatingAttachPoints>(),
		"Clay_FloatingAttachPoints",
		"element",
		"parent",
		typeHash<Clay_FloatingAttachPointType>(),
		typeHash<Clay_FloatingAttachPointType>(),
	},
};

inline const DevEnum2TypeInfo* findDevEnum2TypeInfo(uint64_t fieldTypeHash) {
	for (const DevEnum2TypeInfo& info : kDevEnum2TypeInfos)
	{
		if (info.typeHash == fieldTypeHash)
		{
			return &info;
		}
	}
	return nullptr;
}

inline bool isDevEnum2TypeHash(uint64_t fieldTypeHash) {
	return findDevEnum2TypeInfo(fieldTypeHash) != nullptr;
}

inline bool tryNormalizeDevEnum2ComponentNumeric(
	uint64_t componentEnumTypeHash,
	int64_t numeric,
	uint8_t& outNumeric) {
	if (!tryNormalizeDevEnum1Numeric(numeric, outNumeric))
	{
		return false;
	}

	std::string_view enumName{};
	return tryDevEnum1ValueToName(componentEnumTypeHash, outNumeric, enumName);
}

inline bool tryMakeDevEnum2Value(
	uint64_t fieldTypeHash,
	int64_t firstNumeric,
	int64_t secondNumeric,
	DevEnum2Value& outValue) {
	const DevEnum2TypeInfo* info = findDevEnum2TypeInfo(fieldTypeHash);
	if (info == nullptr)
	{
		return false;
	}

	uint8_t normalizedFirst = 0u;
	uint8_t normalizedSecond = 0u;
	if (
		!tryNormalizeDevEnum2ComponentNumeric(info->firstEnumTypeHash, firstNumeric, normalizedFirst) ||
		!tryNormalizeDevEnum2ComponentNumeric(info->secondEnumTypeHash, secondNumeric, normalizedSecond))
	{
		return false;
	}

	outValue = DevEnum2Value{
		.first = DevEnum1Value{.numeric = normalizedFirst},
		.second = DevEnum1Value{.numeric = normalizedSecond},
	};
	return true;
}

inline bool tryDevEnum2ValueToNames(
	uint64_t fieldTypeHash,
	const DevEnum2Value& value,
	std::string_view& outFirstName,
	std::string_view& outSecondName) {
	const DevEnum2TypeInfo* info = findDevEnum2TypeInfo(fieldTypeHash);
	if (info == nullptr)
	{
		return false;
	}

	return
		tryDevEnum1ValueToName(info->firstEnumTypeHash, value.first.numeric, outFirstName) &&
		tryDevEnum1ValueToName(info->secondEnumTypeHash, value.second.numeric, outSecondName);
}

inline bool tryDevEnum2NamesToValue(
	uint64_t fieldTypeHash,
	std::string_view firstName,
	std::string_view secondName,
	DevEnum2Value& outValue) {
	const DevEnum2TypeInfo* info = findDevEnum2TypeInfo(fieldTypeHash);
	if (info == nullptr)
	{
		return false;
	}

	uint8_t firstNumeric = 0u;
	uint8_t secondNumeric = 0u;
	if (
		!tryDevEnum1NameToValue(info->firstEnumTypeHash, firstName, firstNumeric) ||
		!tryDevEnum1NameToValue(info->secondEnumTypeHash, secondName, secondNumeric))
	{
		return false;
	}

	outValue = DevEnum2Value{
		.first = DevEnum1Value{.numeric = firstNumeric},
		.second = DevEnum1Value{.numeric = secondNumeric},
	};
	return true;
}

inline bool tryCaptureDevEnum2Value(const Clay_ChildAlignment& source, DevEnum2Value& outValue) {
	using UnderlyingX = std::underlying_type_t<Clay_LayoutAlignmentX>;
	using UnderlyingY = std::underlying_type_t<Clay_LayoutAlignmentY>;
	return tryMakeDevEnum2Value(
		typeHash<Clay_ChildAlignment>(),
		static_cast<int64_t>(static_cast<UnderlyingX>(source.x)),
		static_cast<int64_t>(static_cast<UnderlyingY>(source.y)),
		outValue);
}

inline bool tryCaptureDevEnum2Value(const Clay_FloatingAttachPoints& source, DevEnum2Value& outValue) {
	using UnderlyingAttach = std::underlying_type_t<Clay_FloatingAttachPointType>;
	return tryMakeDevEnum2Value(
		typeHash<Clay_FloatingAttachPoints>(),
		static_cast<int64_t>(static_cast<UnderlyingAttach>(source.element)),
		static_cast<int64_t>(static_cast<UnderlyingAttach>(source.parent)),
		outValue);
}

inline bool tryApplyDevEnum2Value(const DevEnum2Value& source, Clay_ChildAlignment& outValue) {
	DevEnum2Value normalized{};
	if (!tryMakeDevEnum2Value(
		typeHash<Clay_ChildAlignment>(),
		static_cast<int64_t>(source.first.numeric),
		static_cast<int64_t>(source.second.numeric),
		normalized))
	{
		return false;
	}

	outValue = Clay_ChildAlignment{
		.x = static_cast<Clay_LayoutAlignmentX>(normalized.first.numeric),
		.y = static_cast<Clay_LayoutAlignmentY>(normalized.second.numeric),
	};
	return true;
}

inline bool tryApplyDevEnum2Value(const DevEnum2Value& source, Clay_FloatingAttachPoints& outValue) {
	DevEnum2Value normalized{};
	if (!tryMakeDevEnum2Value(
		typeHash<Clay_FloatingAttachPoints>(),
		static_cast<int64_t>(source.first.numeric),
		static_cast<int64_t>(source.second.numeric),
		normalized))
	{
		return false;
	}

	outValue = Clay_FloatingAttachPoints{
		.element = static_cast<Clay_FloatingAttachPointType>(normalized.first.numeric),
		.parent = static_cast<Clay_FloatingAttachPointType>(normalized.second.numeric),
	};
	return true;
}

} // namespace FlowUi::devMode
