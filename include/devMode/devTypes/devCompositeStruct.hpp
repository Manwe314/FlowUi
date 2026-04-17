#pragma once

#include <cstddef>
#include <cstdint>
#include <limits>
#include <string_view>
#include <type_traits>

#include "clay.h"
#include "devMode/devTypes/devEdgeU16.hpp"
#include "devMode/devTypes/devEnum1.hpp"
#include "devMode/devTypes/devEnum2.hpp"
#include "devMode/devTypes/devFloat1.hpp"
#include "devMode/devTypes/devFloat2.hpp"
#include "devMode/devTypes/devFloat4.hpp"
#include "devMode/devTypes/devTaggedUnion.hpp"
#include "devMode/devRuntime.hpp"
#include "devMode/registry.hpp"

namespace FlowUi::devMode {

struct DevCompositeStructTypeInfo {
	uint64_t typeHash = 0u;
	std::string_view typeName{};
};

inline constexpr DevCompositeStructTypeInfo kDevCompositeStructTypeInfos[] = {
	{typeHash<Clay_Sizing>(), "Clay_Sizing"},
	{typeHash<Clay_LayoutConfig>(), "Clay_LayoutConfig"},
	{typeHash<Clay_TextElementConfig>(), "Clay_TextElementConfig"},
	{typeHash<Clay_FloatingElementConfig>(), "Clay_FloatingElementConfig"},
	{typeHash<Clay_ClipElementConfig>(), "Clay_ClipElementConfig"},
	{typeHash<Clay_BorderElementConfig>(), "Clay_BorderElementConfig"},
	{typeHash<Clay_ElementDeclaration>(), "Clay_ElementDeclaration"},
};

inline const DevCompositeStructTypeInfo* findDevCompositeStructTypeInfo(uint64_t fieldTypeHash) {
	for (const DevCompositeStructTypeInfo& info : kDevCompositeStructTypeInfos)
	{
		if (info.typeHash == fieldTypeHash)
		{
			return &info;
		}
	}
	return nullptr;
}

inline bool isDevCompositeStructTypeHash(uint64_t fieldTypeHash) {
	return findDevCompositeStructTypeInfo(fieldTypeHash) != nullptr;
}

inline uint64_t devPointerToBits(const void* pointer) {
	return static_cast<uint64_t>(reinterpret_cast<uintptr_t>(pointer));
}

inline bool tryDevBitsToPointer(uint64_t bits, void*& outPointer) {
	if (bits > static_cast<uint64_t>(std::numeric_limits<uintptr_t>::max()))
	{
		return false;
	}
	outPointer = reinterpret_cast<void*>(static_cast<uintptr_t>(bits));
	return true;
}

template <typename EnumT>
inline bool tryCaptureDevEnum1Value(EnumT source, uint64_t enumTypeHash, DevEnum1Value& outValue) {
	using Underlying = std::underlying_type_t<EnumT>;
	uint8_t numeric = 0u;
	if (!tryNormalizeDevEnum1Numeric(static_cast<int64_t>(static_cast<Underlying>(source)), numeric))
	{
		return false;
	}

	std::string_view name{};
	if (!tryDevEnum1ValueToName(enumTypeHash, numeric, name))
	{
		return false;
	}

	outValue = DevEnum1Value{.numeric = numeric};
	return true;
}

template <typename EnumT>
inline bool tryApplyDevEnum1Value(uint64_t enumTypeHash, const DevEnum1Value& source, EnumT& outValue) {
	uint8_t numeric = 0u;
	if (!tryNormalizeDevEnum1Numeric(static_cast<int64_t>(source.numeric), numeric))
	{
		return false;
	}

	std::string_view name{};
	if (!tryDevEnum1ValueToName(enumTypeHash, numeric, name))
	{
		return false;
	}

	outValue = static_cast<EnumT>(numeric);
	return true;
}

inline bool tryCaptureDevElementIdValue(const Clay_ElementId& source, DevElementIdValue& outValue) {
	const int32_t length = source.stringId.length;
	if (length < 0)
	{
		return false;
	}

	std::string text{};
	if (length > 0)
	{
		if (source.stringId.chars == nullptr)
		{
			return false;
		}
		text.assign(source.stringId.chars, static_cast<std::size_t>(length));
	}

	outValue = DevElementIdValue{
		.id = source.id,
		.offset = source.offset,
		.baseId = source.baseId,
		.isStaticallyAllocated = source.stringId.isStaticallyAllocated,
		.stringId = std::move(text),
	};
	return true;
}

inline bool tryApplyDevElementIdValue(const DevElementIdValue& source, Clay_ElementId& outValue) {
	if (source.stringId.size() > static_cast<std::size_t>(std::numeric_limits<int32_t>::max()))
	{
		return false;
	}

	outValue = Clay_ElementId{};
	outValue.id = source.id;
	outValue.offset = source.offset;
	outValue.baseId = source.baseId;
	outValue.stringId.isStaticallyAllocated = source.isStaticallyAllocated;
	outValue.stringId.length = static_cast<int32_t>(source.stringId.size());
	outValue.stringId.chars = source.stringId.empty() ? nullptr : source.stringId.c_str();
	return true;
}

inline bool tryCaptureDevSizingValue(const Clay_Sizing& source, DevSizingValue& outValue) {
	DevTaggedUnionValue width{};
	DevTaggedUnionValue height{};
	if (
		!tryCaptureDevTaggedUnionValue(source.width, width) ||
		!tryCaptureDevTaggedUnionValue(source.height, height))
	{
		return false;
	}

	outValue = DevSizingValue{
		.width = width,
		.height = height,
	};
	return true;
}

inline bool tryApplyDevSizingValue(const DevSizingValue& source, Clay_Sizing& outValue) {
	Clay_SizingAxis width{};
	Clay_SizingAxis height{};
	if (
		!tryApplyDevTaggedUnionValue(source.width, width) ||
		!tryApplyDevTaggedUnionValue(source.height, height))
	{
		return false;
	}

	outValue = Clay_Sizing{
		.width = width,
		.height = height,
	};
	return true;
}

inline bool tryCaptureDevLayoutConfigValue(const Clay_LayoutConfig& source, DevLayoutConfigValue& outValue) {
	DevSizingValue sizing{};
	DevEdgeU16Value padding{};
	DevEnum2Value childAlignment{};
	DevEnum1Value layoutDirection{};
	if (
		!tryCaptureDevSizingValue(source.sizing, sizing) ||
		!tryCaptureDevEdgeU16Value(source.padding, padding) ||
		!tryCaptureDevEnum2Value(source.childAlignment, childAlignment) ||
		!tryCaptureDevEnum1Value(source.layoutDirection, typeHash<Clay_LayoutDirection>(), layoutDirection))
	{
		return false;
	}

	outValue = DevLayoutConfigValue{
		.sizing = sizing,
		.padding = padding,
		.childGap = source.childGap,
		.childAlignment = childAlignment,
		.layoutDirection = layoutDirection,
	};
	return true;
}

inline bool tryApplyDevLayoutConfigValue(const DevLayoutConfigValue& source, Clay_LayoutConfig& outValue) {
	Clay_Sizing sizing{};
	Clay_Padding padding{};
	Clay_ChildAlignment childAlignment{};
	Clay_LayoutDirection layoutDirection = CLAY_LEFT_TO_RIGHT;
	if (
		!tryApplyDevSizingValue(source.sizing, sizing) ||
		!tryApplyDevEdgeU16Value(source.padding, padding) ||
		!tryApplyDevEnum2Value(source.childAlignment, childAlignment) ||
		!tryApplyDevEnum1Value(typeHash<Clay_LayoutDirection>(), source.layoutDirection, layoutDirection))
	{
		return false;
	}

	outValue = Clay_LayoutConfig{
		.sizing = sizing,
		.padding = padding,
		.childGap = source.childGap,
		.childAlignment = childAlignment,
		.layoutDirection = layoutDirection,
	};
	return true;
}

inline bool tryCaptureDevTextElementConfigValue(const Clay_TextElementConfig& source, DevTextElementConfigValue& outValue) {
	DevFloat4Value textColor{};
	DevEnum1Value wrapMode{};
	DevEnum1Value textAlignment{};
	if (
		!tryCaptureDevFloat4Value(source.textColor, textColor) ||
		!tryCaptureDevEnum1Value(
			source.wrapMode,
			typeHash<Clay_TextElementConfigWrapMode>(),
			wrapMode) ||
		!tryCaptureDevEnum1Value(source.textAlignment, typeHash<Clay_TextAlignment>(), textAlignment))
	{
		return false;
	}

	outValue = DevTextElementConfigValue{
		.userData = DevPointerValue{.bits = devPointerToBits(source.userData)},
		.textColor = textColor,
		.fontId = source.fontId,
		.fontSize = source.fontSize,
		.letterSpacing = source.letterSpacing,
		.lineHeight = source.lineHeight,
		.wrapMode = wrapMode,
		.textAlignment = textAlignment,
	};
	return true;
}

inline bool tryApplyDevTextElementConfigValue(const DevTextElementConfigValue& source, Clay_TextElementConfig& outValue) {
	Clay_Color textColor{};
	Clay_TextElementConfigWrapMode wrapMode = CLAY_TEXT_WRAP_WORDS;
	Clay_TextAlignment textAlignment = CLAY_TEXT_ALIGN_LEFT;
	void* userData = nullptr;
	if (
		!tryApplyDevFloat4Value(source.textColor, textColor) ||
		!tryApplyDevEnum1Value(typeHash<Clay_TextElementConfigWrapMode>(), source.wrapMode, wrapMode) ||
		!tryApplyDevEnum1Value(typeHash<Clay_TextAlignment>(), source.textAlignment, textAlignment) ||
		!tryDevBitsToPointer(source.userData.bits, userData))
	{
		return false;
	}

	outValue = Clay_TextElementConfig{};
	outValue.userData = userData;
	outValue.textColor = textColor;
	outValue.fontId = source.fontId;
	outValue.fontSize = source.fontSize;
	outValue.letterSpacing = source.letterSpacing;
	outValue.lineHeight = source.lineHeight;
	outValue.wrapMode = wrapMode;
	outValue.textAlignment = textAlignment;
	return true;
}

inline bool tryCaptureDevFloatingElementConfigValue(
	const Clay_FloatingElementConfig& source,
	DevFloatingElementConfigValue& outValue) {
	DevFloat2Value offset{};
	DevFloat2Value expand{};
	DevEnum2Value attachPoints{};
	DevEnum1Value pointerCaptureMode{};
	DevEnum1Value attachTo{};
	DevEnum1Value clipTo{};
	if (
		!tryCaptureDevFloat2Value(source.offset, offset) ||
		!tryCaptureDevFloat2Value(source.expand, expand) ||
		!tryCaptureDevEnum2Value(source.attachPoints, attachPoints) ||
		!tryCaptureDevEnum1Value(
			source.pointerCaptureMode,
			typeHash<Clay_PointerCaptureMode>(),
			pointerCaptureMode) ||
		!tryCaptureDevEnum1Value(source.attachTo, typeHash<Clay_FloatingAttachToElement>(), attachTo) ||
		!tryCaptureDevEnum1Value(source.clipTo, typeHash<Clay_FloatingClipToElement>(), clipTo))
	{
		return false;
	}

	outValue = DevFloatingElementConfigValue{
		.offset = offset,
		.expand = expand,
		.parentId = source.parentId,
		.zIndex = source.zIndex,
		.attachPoints = attachPoints,
		.pointerCaptureMode = pointerCaptureMode,
		.attachTo = attachTo,
		.clipTo = clipTo,
	};
	return true;
}

inline bool tryApplyDevFloatingElementConfigValue(
	const DevFloatingElementConfigValue& source,
	Clay_FloatingElementConfig& outValue) {
	Clay_Vector2 offset{};
	Clay_Dimensions expand{};
	Clay_FloatingAttachPoints attachPoints{};
	Clay_PointerCaptureMode pointerCaptureMode = CLAY_POINTER_CAPTURE_MODE_CAPTURE;
	Clay_FloatingAttachToElement attachTo = CLAY_ATTACH_TO_NONE;
	Clay_FloatingClipToElement clipTo = CLAY_CLIP_TO_NONE;
	if (
		!tryApplyDevFloat2Value(source.offset, offset) ||
		!tryApplyDevFloat2Value(source.expand, expand) ||
		!tryApplyDevEnum2Value(source.attachPoints, attachPoints) ||
		!tryApplyDevEnum1Value(
			typeHash<Clay_PointerCaptureMode>(),
			source.pointerCaptureMode,
			pointerCaptureMode) ||
		!tryApplyDevEnum1Value(typeHash<Clay_FloatingAttachToElement>(), source.attachTo, attachTo) ||
		!tryApplyDevEnum1Value(typeHash<Clay_FloatingClipToElement>(), source.clipTo, clipTo))
	{
		return false;
	}

	outValue = Clay_FloatingElementConfig{
		.offset = offset,
		.expand = expand,
		.parentId = source.parentId,
		.zIndex = source.zIndex,
		.attachPoints = attachPoints,
		.pointerCaptureMode = pointerCaptureMode,
		.attachTo = attachTo,
		.clipTo = clipTo,
	};
	return true;
}

inline bool tryCaptureDevClipElementConfigValue(const Clay_ClipElementConfig& source, DevClipElementConfigValue& outValue) {
	DevFloat2Value childOffset{};
	if (!tryCaptureDevFloat2Value(source.childOffset, childOffset))
	{
		return false;
	}

	outValue = DevClipElementConfigValue{
		.horizontal = source.horizontal,
		.vertical = source.vertical,
		.childOffset = childOffset,
	};
	return true;
}

inline bool tryApplyDevClipElementConfigValue(const DevClipElementConfigValue& source, Clay_ClipElementConfig& outValue) {
	Clay_Vector2 childOffset{};
	if (!tryApplyDevFloat2Value(source.childOffset, childOffset))
	{
		return false;
	}

	outValue = Clay_ClipElementConfig{
		.horizontal = source.horizontal,
		.vertical = source.vertical,
		.childOffset = childOffset,
	};
	return true;
}

inline bool tryCaptureDevBorderElementConfigValue(
	const Clay_BorderElementConfig& source,
	DevBorderElementConfigValue& outValue) {
	DevFloat4Value color{};
	DevEdgeU16Value width{};
	if (
		!tryCaptureDevFloat4Value(source.color, color) ||
		!tryCaptureDevEdgeU16Value(source.width, width))
	{
		return false;
	}

	outValue = DevBorderElementConfigValue{
		.color = color,
		.width = width,
	};
	return true;
}

inline bool tryApplyDevBorderElementConfigValue(const DevBorderElementConfigValue& source, Clay_BorderElementConfig& outValue) {
	Clay_Color color{};
	Clay_BorderWidth width{};
	if (
		!tryApplyDevFloat4Value(source.color, color) ||
		!tryApplyDevEdgeU16Value(source.width, width))
	{
		return false;
	}

	outValue = Clay_BorderElementConfig{
		.color = color,
		.width = width,
	};
	return true;
}

inline bool tryCaptureDevElementDeclarationValue(
	const Clay_ElementDeclaration& source,
	DevElementDeclarationValue& outValue) {
	DevElementIdValue id{};
	DevLayoutConfigValue layout{};
	DevFloat4Value backgroundColor{};
	DevFloat4Value cornerRadius{};
	double aspectRatio = 0.0;
	DevFloatingElementConfigValue floating{};
	DevClipElementConfigValue clip{};
	DevBorderElementConfigValue border{};
	if (
		!tryCaptureDevElementIdValue(source.id, id) ||
		!tryCaptureDevLayoutConfigValue(source.layout, layout) ||
		!tryCaptureDevFloat4Value(source.backgroundColor, backgroundColor) ||
		!tryCaptureDevFloat4Value(source.cornerRadius, cornerRadius) ||
		!tryCaptureDevFloat1Value(source.aspectRatio, aspectRatio) ||
		!tryCaptureDevFloatingElementConfigValue(source.floating, floating) ||
		!tryCaptureDevClipElementConfigValue(source.clip, clip) ||
		!tryCaptureDevBorderElementConfigValue(source.border, border))
	{
		return false;
	}

	outValue = DevElementDeclarationValue{
		.id = id,
		.layout = layout,
		.backgroundColor = backgroundColor,
		.cornerRadius = cornerRadius,
		.aspectRatio = aspectRatio,
		.imageData = DevPointerValue{.bits = devPointerToBits(source.image.imageData)},
		.floating = floating,
		.customData = DevPointerValue{.bits = devPointerToBits(source.custom.customData)},
		.clip = clip,
		.border = border,
		.userData = DevPointerValue{.bits = devPointerToBits(source.userData)},
	};
	return true;
}

inline bool tryApplyDevElementDeclarationValue(
	const DevElementDeclarationValue& source,
	Clay_ElementDeclaration& outValue) {
	Clay_ElementId id{};
	Clay_LayoutConfig layout{};
	Clay_Color backgroundColor{};
	Clay_CornerRadius cornerRadius{};
	Clay_AspectRatioElementConfig aspectRatio{};
	Clay_FloatingElementConfig floating{};
	Clay_ClipElementConfig clip{};
	Clay_BorderElementConfig border{};
	void* imageData = nullptr;
	void* customData = nullptr;
	void* userData = nullptr;
	if (
		!tryApplyDevElementIdValue(source.id, id) ||
		!tryApplyDevLayoutConfigValue(source.layout, layout) ||
		!tryApplyDevFloat4Value(source.backgroundColor, backgroundColor) ||
		!tryApplyDevFloat4Value(source.cornerRadius, cornerRadius) ||
		!tryApplyDevFloat1Value(source.aspectRatio, aspectRatio) ||
		!tryApplyDevFloatingElementConfigValue(source.floating, floating) ||
		!tryApplyDevClipElementConfigValue(source.clip, clip) ||
		!tryApplyDevBorderElementConfigValue(source.border, border) ||
		!tryDevBitsToPointer(source.imageData.bits, imageData) ||
		!tryDevBitsToPointer(source.customData.bits, customData) ||
		!tryDevBitsToPointer(source.userData.bits, userData))
	{
		return false;
	}

	outValue = Clay_ElementDeclaration{};
	outValue.id = id;
	outValue.layout = layout;
	outValue.backgroundColor = backgroundColor;
	outValue.cornerRadius = cornerRadius;
	outValue.aspectRatio = aspectRatio;
	outValue.image = Clay_ImageElementConfig{.imageData = imageData};
	outValue.floating = floating;
	outValue.custom = Clay_CustomElementConfig{.customData = customData};
	outValue.clip = clip;
	outValue.border = border;
	outValue.userData = userData;
	return true;
}

inline bool tryMakeDevCompositeStructValue(
	uint64_t fieldTypeHash,
	const DevCompositeStructValue& source,
	DevCompositeStructValue& outValue) {
	if (!isDevCompositeStructTypeHash(fieldTypeHash))
	{
		return false;
	}
	if (source.typeHash != 0u && source.typeHash != fieldTypeHash)
	{
		return false;
	}

	outValue = DevCompositeStructValue{};
	outValue.typeHash = fieldTypeHash;

	if (fieldTypeHash == typeHash<Clay_Sizing>())
	{
		Clay_Sizing normalized{};
		if (!tryApplyDevSizingValue(source.sizing, normalized) ||
			!tryCaptureDevSizingValue(normalized, outValue.sizing))
		{
			return false;
		}
		return true;
	}

	if (fieldTypeHash == typeHash<Clay_LayoutConfig>())
	{
		Clay_LayoutConfig normalized{};
		if (!tryApplyDevLayoutConfigValue(source.layoutConfig, normalized) ||
			!tryCaptureDevLayoutConfigValue(normalized, outValue.layoutConfig))
		{
			return false;
		}
		return true;
	}

	if (fieldTypeHash == typeHash<Clay_TextElementConfig>())
	{
		Clay_TextElementConfig normalized{};
		if (!tryApplyDevTextElementConfigValue(source.textElementConfig, normalized) ||
			!tryCaptureDevTextElementConfigValue(normalized, outValue.textElementConfig))
		{
			return false;
		}
		return true;
	}

	if (fieldTypeHash == typeHash<Clay_FloatingElementConfig>())
	{
		Clay_FloatingElementConfig normalized{};
		if (!tryApplyDevFloatingElementConfigValue(source.floatingElementConfig, normalized) ||
			!tryCaptureDevFloatingElementConfigValue(normalized, outValue.floatingElementConfig))
		{
			return false;
		}
		return true;
	}

	if (fieldTypeHash == typeHash<Clay_ClipElementConfig>())
	{
		Clay_ClipElementConfig normalized{};
		if (!tryApplyDevClipElementConfigValue(source.clipElementConfig, normalized) ||
			!tryCaptureDevClipElementConfigValue(normalized, outValue.clipElementConfig))
		{
			return false;
		}
		return true;
	}

	if (fieldTypeHash == typeHash<Clay_BorderElementConfig>())
	{
		Clay_BorderElementConfig normalized{};
		if (!tryApplyDevBorderElementConfigValue(source.borderElementConfig, normalized) ||
			!tryCaptureDevBorderElementConfigValue(normalized, outValue.borderElementConfig))
		{
			return false;
		}
		return true;
	}

	if (fieldTypeHash == typeHash<Clay_ElementDeclaration>())
	{
		Clay_ElementDeclaration normalized{};
		if (!tryApplyDevElementDeclarationValue(source.elementDeclaration, normalized) ||
			!tryCaptureDevElementDeclarationValue(normalized, outValue.elementDeclaration))
		{
			return false;
		}
		return true;
	}

	return false;
}

inline bool tryCaptureDevCompositeStructValue(const Clay_Sizing& source, DevCompositeStructValue& outValue) {
	DevCompositeStructValue value{};
	value.typeHash = typeHash<Clay_Sizing>();
	if (!tryCaptureDevSizingValue(source, value.sizing))
	{
		return false;
	}
	return tryMakeDevCompositeStructValue(typeHash<Clay_Sizing>(), value, outValue);
}

inline bool tryCaptureDevCompositeStructValue(const Clay_LayoutConfig& source, DevCompositeStructValue& outValue) {
	DevCompositeStructValue value{};
	value.typeHash = typeHash<Clay_LayoutConfig>();
	if (!tryCaptureDevLayoutConfigValue(source, value.layoutConfig))
	{
		return false;
	}
	return tryMakeDevCompositeStructValue(typeHash<Clay_LayoutConfig>(), value, outValue);
}

inline bool tryCaptureDevCompositeStructValue(const Clay_TextElementConfig& source, DevCompositeStructValue& outValue) {
	DevCompositeStructValue value{};
	value.typeHash = typeHash<Clay_TextElementConfig>();
	if (!tryCaptureDevTextElementConfigValue(source, value.textElementConfig))
	{
		return false;
	}
	return tryMakeDevCompositeStructValue(typeHash<Clay_TextElementConfig>(), value, outValue);
}

inline bool tryCaptureDevCompositeStructValue(const Clay_FloatingElementConfig& source, DevCompositeStructValue& outValue) {
	DevCompositeStructValue value{};
	value.typeHash = typeHash<Clay_FloatingElementConfig>();
	if (!tryCaptureDevFloatingElementConfigValue(source, value.floatingElementConfig))
	{
		return false;
	}
	return tryMakeDevCompositeStructValue(typeHash<Clay_FloatingElementConfig>(), value, outValue);
}

inline bool tryCaptureDevCompositeStructValue(const Clay_ClipElementConfig& source, DevCompositeStructValue& outValue) {
	DevCompositeStructValue value{};
	value.typeHash = typeHash<Clay_ClipElementConfig>();
	if (!tryCaptureDevClipElementConfigValue(source, value.clipElementConfig))
	{
		return false;
	}
	return tryMakeDevCompositeStructValue(typeHash<Clay_ClipElementConfig>(), value, outValue);
}

inline bool tryCaptureDevCompositeStructValue(const Clay_BorderElementConfig& source, DevCompositeStructValue& outValue) {
	DevCompositeStructValue value{};
	value.typeHash = typeHash<Clay_BorderElementConfig>();
	if (!tryCaptureDevBorderElementConfigValue(source, value.borderElementConfig))
	{
		return false;
	}
	return tryMakeDevCompositeStructValue(typeHash<Clay_BorderElementConfig>(), value, outValue);
}

inline bool tryCaptureDevCompositeStructValue(const Clay_ElementDeclaration& source, DevCompositeStructValue& outValue) {
	DevCompositeStructValue value{};
	value.typeHash = typeHash<Clay_ElementDeclaration>();
	if (!tryCaptureDevElementDeclarationValue(source, value.elementDeclaration))
	{
		return false;
	}
	return tryMakeDevCompositeStructValue(typeHash<Clay_ElementDeclaration>(), value, outValue);
}

inline bool tryApplyDevCompositeStructValue(const DevCompositeStructValue& source, Clay_Sizing& outValue) {
	DevCompositeStructValue normalized{};
	if (!tryMakeDevCompositeStructValue(typeHash<Clay_Sizing>(), source, normalized))
	{
		return false;
	}
	return tryApplyDevSizingValue(normalized.sizing, outValue);
}

inline bool tryApplyDevCompositeStructValue(const DevCompositeStructValue& source, Clay_LayoutConfig& outValue) {
	DevCompositeStructValue normalized{};
	if (!tryMakeDevCompositeStructValue(typeHash<Clay_LayoutConfig>(), source, normalized))
	{
		return false;
	}
	return tryApplyDevLayoutConfigValue(normalized.layoutConfig, outValue);
}

inline bool tryApplyDevCompositeStructValue(const DevCompositeStructValue& source, Clay_TextElementConfig& outValue) {
	DevCompositeStructValue normalized{};
	if (!tryMakeDevCompositeStructValue(typeHash<Clay_TextElementConfig>(), source, normalized))
	{
		return false;
	}
	return tryApplyDevTextElementConfigValue(normalized.textElementConfig, outValue);
}

inline bool tryApplyDevCompositeStructValue(const DevCompositeStructValue& source, Clay_FloatingElementConfig& outValue) {
	DevCompositeStructValue normalized{};
	if (!tryMakeDevCompositeStructValue(typeHash<Clay_FloatingElementConfig>(), source, normalized))
	{
		return false;
	}
	return tryApplyDevFloatingElementConfigValue(normalized.floatingElementConfig, outValue);
}

inline bool tryApplyDevCompositeStructValue(const DevCompositeStructValue& source, Clay_ClipElementConfig& outValue) {
	DevCompositeStructValue normalized{};
	if (!tryMakeDevCompositeStructValue(typeHash<Clay_ClipElementConfig>(), source, normalized))
	{
		return false;
	}
	return tryApplyDevClipElementConfigValue(normalized.clipElementConfig, outValue);
}

inline bool tryApplyDevCompositeStructValue(const DevCompositeStructValue& source, Clay_BorderElementConfig& outValue) {
	DevCompositeStructValue normalized{};
	if (!tryMakeDevCompositeStructValue(typeHash<Clay_BorderElementConfig>(), source, normalized))
	{
		return false;
	}
	return tryApplyDevBorderElementConfigValue(normalized.borderElementConfig, outValue);
}

inline bool tryApplyDevCompositeStructValue(const DevCompositeStructValue& source, Clay_ElementDeclaration& outValue) {
	DevCompositeStructValue normalized{};
	if (!tryMakeDevCompositeStructValue(typeHash<Clay_ElementDeclaration>(), source, normalized))
	{
		return false;
	}
	return tryApplyDevElementDeclarationValue(normalized.elementDeclaration, outValue);
}

} // namespace FlowUi::devMode
