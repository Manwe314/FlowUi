#pragma once

#include <cstddef>
#include <cstdint>
#include <limits>
#include <string_view>
#include <type_traits>

#include "clay.h"
#include "devMode/registry.hpp"

namespace FlowUi::devMode {

static_assert(std::is_same_v<std::underlying_type_t<Clay_LayoutDirection>, uint8_t>);
static_assert(std::is_same_v<std::underlying_type_t<Clay_LayoutAlignmentX>, uint8_t>);
static_assert(std::is_same_v<std::underlying_type_t<Clay_LayoutAlignmentY>, uint8_t>);
static_assert(std::is_same_v<std::underlying_type_t<Clay__SizingType>, uint8_t>);
static_assert(std::is_same_v<std::underlying_type_t<Clay_TextElementConfigWrapMode>, uint8_t>);
static_assert(std::is_same_v<std::underlying_type_t<Clay_TextAlignment>, uint8_t>);
static_assert(std::is_same_v<std::underlying_type_t<Clay_FloatingAttachPointType>, uint8_t>);
static_assert(std::is_same_v<std::underlying_type_t<Clay_PointerCaptureMode>, uint8_t>);
static_assert(std::is_same_v<std::underlying_type_t<Clay_FloatingAttachToElement>, uint8_t>);
static_assert(std::is_same_v<std::underlying_type_t<Clay_FloatingClipToElement>, uint8_t>);

struct DevEnum1NamedValue {
	uint8_t value = 0u;
	std::string_view name{};
};

struct DevEnum1TypeInfo {
	uint64_t typeHash = 0u;
	std::string_view typeName{};
	const DevEnum1NamedValue* values = nullptr;
	std::size_t valueCount = 0u;
};

inline constexpr DevEnum1NamedValue kDevEnum1LayoutDirectionValues[] = {
	{static_cast<uint8_t>(CLAY_LEFT_TO_RIGHT), "CLAY_LEFT_TO_RIGHT"},
	{static_cast<uint8_t>(CLAY_TOP_TO_BOTTOM), "CLAY_TOP_TO_BOTTOM"},
};

inline constexpr DevEnum1NamedValue kDevEnum1LayoutAlignmentXValues[] = {
	{static_cast<uint8_t>(CLAY_ALIGN_X_LEFT), "CLAY_ALIGN_X_LEFT"},
	{static_cast<uint8_t>(CLAY_ALIGN_X_RIGHT), "CLAY_ALIGN_X_RIGHT"},
	{static_cast<uint8_t>(CLAY_ALIGN_X_CENTER), "CLAY_ALIGN_X_CENTER"},
};

inline constexpr DevEnum1NamedValue kDevEnum1LayoutAlignmentYValues[] = {
	{static_cast<uint8_t>(CLAY_ALIGN_Y_TOP), "CLAY_ALIGN_Y_TOP"},
	{static_cast<uint8_t>(CLAY_ALIGN_Y_BOTTOM), "CLAY_ALIGN_Y_BOTTOM"},
	{static_cast<uint8_t>(CLAY_ALIGN_Y_CENTER), "CLAY_ALIGN_Y_CENTER"},
};

inline constexpr DevEnum1NamedValue kDevEnum1SizingTypeValues[] = {
	{static_cast<uint8_t>(CLAY__SIZING_TYPE_FIT), "CLAY__SIZING_TYPE_FIT"},
	{static_cast<uint8_t>(CLAY__SIZING_TYPE_GROW), "CLAY__SIZING_TYPE_GROW"},
	{static_cast<uint8_t>(CLAY__SIZING_TYPE_PERCENT), "CLAY__SIZING_TYPE_PERCENT"},
	{static_cast<uint8_t>(CLAY__SIZING_TYPE_FIXED), "CLAY__SIZING_TYPE_FIXED"},
};

inline constexpr DevEnum1NamedValue kDevEnum1TextWrapModeValues[] = {
	{static_cast<uint8_t>(CLAY_TEXT_WRAP_WORDS), "CLAY_TEXT_WRAP_WORDS"},
	{static_cast<uint8_t>(CLAY_TEXT_WRAP_NEWLINES), "CLAY_TEXT_WRAP_NEWLINES"},
	{static_cast<uint8_t>(CLAY_TEXT_WRAP_NONE), "CLAY_TEXT_WRAP_NONE"},
};

inline constexpr DevEnum1NamedValue kDevEnum1TextAlignmentValues[] = {
	{static_cast<uint8_t>(CLAY_TEXT_ALIGN_LEFT), "CLAY_TEXT_ALIGN_LEFT"},
	{static_cast<uint8_t>(CLAY_TEXT_ALIGN_CENTER), "CLAY_TEXT_ALIGN_CENTER"},
	{static_cast<uint8_t>(CLAY_TEXT_ALIGN_RIGHT), "CLAY_TEXT_ALIGN_RIGHT"},
};

inline constexpr DevEnum1NamedValue kDevEnum1FloatingAttachPointValues[] = {
	{static_cast<uint8_t>(CLAY_ATTACH_POINT_LEFT_TOP), "CLAY_ATTACH_POINT_LEFT_TOP"},
	{static_cast<uint8_t>(CLAY_ATTACH_POINT_LEFT_CENTER), "CLAY_ATTACH_POINT_LEFT_CENTER"},
	{static_cast<uint8_t>(CLAY_ATTACH_POINT_LEFT_BOTTOM), "CLAY_ATTACH_POINT_LEFT_BOTTOM"},
	{static_cast<uint8_t>(CLAY_ATTACH_POINT_CENTER_TOP), "CLAY_ATTACH_POINT_CENTER_TOP"},
	{static_cast<uint8_t>(CLAY_ATTACH_POINT_CENTER_CENTER), "CLAY_ATTACH_POINT_CENTER_CENTER"},
	{static_cast<uint8_t>(CLAY_ATTACH_POINT_CENTER_BOTTOM), "CLAY_ATTACH_POINT_CENTER_BOTTOM"},
	{static_cast<uint8_t>(CLAY_ATTACH_POINT_RIGHT_TOP), "CLAY_ATTACH_POINT_RIGHT_TOP"},
	{static_cast<uint8_t>(CLAY_ATTACH_POINT_RIGHT_CENTER), "CLAY_ATTACH_POINT_RIGHT_CENTER"},
	{static_cast<uint8_t>(CLAY_ATTACH_POINT_RIGHT_BOTTOM), "CLAY_ATTACH_POINT_RIGHT_BOTTOM"},
};

inline constexpr DevEnum1NamedValue kDevEnum1PointerCaptureModeValues[] = {
	{static_cast<uint8_t>(CLAY_POINTER_CAPTURE_MODE_CAPTURE), "CLAY_POINTER_CAPTURE_MODE_CAPTURE"},
	{static_cast<uint8_t>(CLAY_POINTER_CAPTURE_MODE_PASSTHROUGH), "CLAY_POINTER_CAPTURE_MODE_PASSTHROUGH"},
};

inline constexpr DevEnum1NamedValue kDevEnum1FloatingAttachToElementValues[] = {
	{static_cast<uint8_t>(CLAY_ATTACH_TO_NONE), "CLAY_ATTACH_TO_NONE"},
	{static_cast<uint8_t>(CLAY_ATTACH_TO_PARENT), "CLAY_ATTACH_TO_PARENT"},
	{static_cast<uint8_t>(CLAY_ATTACH_TO_ELEMENT_WITH_ID), "CLAY_ATTACH_TO_ELEMENT_WITH_ID"},
	{static_cast<uint8_t>(CLAY_ATTACH_TO_ROOT), "CLAY_ATTACH_TO_ROOT"},
};

inline constexpr DevEnum1NamedValue kDevEnum1FloatingClipToElementValues[] = {
	{static_cast<uint8_t>(CLAY_CLIP_TO_NONE), "CLAY_CLIP_TO_NONE"},
	{static_cast<uint8_t>(CLAY_CLIP_TO_ATTACHED_PARENT), "CLAY_CLIP_TO_ATTACHED_PARENT"},
};

inline constexpr DevEnum1TypeInfo kDevEnum1TypeInfos[] = {
	{
		typeHash<Clay_LayoutDirection>(),
		"Clay_LayoutDirection",
		kDevEnum1LayoutDirectionValues,
		sizeof(kDevEnum1LayoutDirectionValues) / sizeof(kDevEnum1LayoutDirectionValues[0]),
	},
	{
		typeHash<Clay_LayoutAlignmentX>(),
		"Clay_LayoutAlignmentX",
		kDevEnum1LayoutAlignmentXValues,
		sizeof(kDevEnum1LayoutAlignmentXValues) / sizeof(kDevEnum1LayoutAlignmentXValues[0]),
	},
	{
		typeHash<Clay_LayoutAlignmentY>(),
		"Clay_LayoutAlignmentY",
		kDevEnum1LayoutAlignmentYValues,
		sizeof(kDevEnum1LayoutAlignmentYValues) / sizeof(kDevEnum1LayoutAlignmentYValues[0]),
	},
	{
		typeHash<Clay__SizingType>(),
		"Clay__SizingType",
		kDevEnum1SizingTypeValues,
		sizeof(kDevEnum1SizingTypeValues) / sizeof(kDevEnum1SizingTypeValues[0]),
	},
	{
		typeHash<Clay_TextElementConfigWrapMode>(),
		"Clay_TextElementConfigWrapMode",
		kDevEnum1TextWrapModeValues,
		sizeof(kDevEnum1TextWrapModeValues) / sizeof(kDevEnum1TextWrapModeValues[0]),
	},
	{
		typeHash<Clay_TextAlignment>(),
		"Clay_TextAlignment",
		kDevEnum1TextAlignmentValues,
		sizeof(kDevEnum1TextAlignmentValues) / sizeof(kDevEnum1TextAlignmentValues[0]),
	},
	{
		typeHash<Clay_FloatingAttachPointType>(),
		"Clay_FloatingAttachPointType",
		kDevEnum1FloatingAttachPointValues,
		sizeof(kDevEnum1FloatingAttachPointValues) / sizeof(kDevEnum1FloatingAttachPointValues[0]),
	},
	{
		typeHash<Clay_PointerCaptureMode>(),
		"Clay_PointerCaptureMode",
		kDevEnum1PointerCaptureModeValues,
		sizeof(kDevEnum1PointerCaptureModeValues) / sizeof(kDevEnum1PointerCaptureModeValues[0]),
	},
	{
		typeHash<Clay_FloatingAttachToElement>(),
		"Clay_FloatingAttachToElement",
		kDevEnum1FloatingAttachToElementValues,
		sizeof(kDevEnum1FloatingAttachToElementValues) / sizeof(kDevEnum1FloatingAttachToElementValues[0]),
	},
	{
		typeHash<Clay_FloatingClipToElement>(),
		"Clay_FloatingClipToElement",
		kDevEnum1FloatingClipToElementValues,
		sizeof(kDevEnum1FloatingClipToElementValues) / sizeof(kDevEnum1FloatingClipToElementValues[0]),
	},
};

inline const DevEnum1TypeInfo* findDevEnum1TypeInfo(uint64_t fieldTypeHash) {
	for (const DevEnum1TypeInfo& info : kDevEnum1TypeInfos)
	{
		if (info.typeHash == fieldTypeHash)
		{
			return &info;
		}
	}
	return nullptr;
}

inline bool isDevEnum1TypeHash(uint64_t fieldTypeHash) {
	return findDevEnum1TypeInfo(fieldTypeHash) != nullptr;
}

inline bool tryNormalizeDevEnum1Numeric(int64_t numeric, uint8_t& outValue) {
	if (numeric < 0 || numeric > static_cast<int64_t>(std::numeric_limits<uint8_t>::max()))
	{
		return false;
	}
	outValue = static_cast<uint8_t>(numeric);
	return true;
}

inline bool tryDevEnum1ValueToName(
	uint64_t fieldTypeHash,
	uint8_t numeric,
	std::string_view& outName) {
	const DevEnum1TypeInfo* info = findDevEnum1TypeInfo(fieldTypeHash);
	if (info == nullptr)
	{
		return false;
	}

	for (std::size_t i = 0; i < info->valueCount; ++i)
	{
		if (info->values[i].value == numeric)
		{
			outName = info->values[i].name;
			return true;
		}
	}
	return false;
}

inline bool tryDevEnum1NameToValue(
	uint64_t fieldTypeHash,
	std::string_view name,
	uint8_t& outNumeric) {
	const DevEnum1TypeInfo* info = findDevEnum1TypeInfo(fieldTypeHash);
	if (info == nullptr)
	{
		return false;
	}

	for (std::size_t i = 0; i < info->valueCount; ++i)
	{
		if (info->values[i].name == name)
		{
			outNumeric = info->values[i].value;
			return true;
		}
	}
	return false;
}

} // namespace FlowUi::devMode
