#pragma once

#include "FlowUi/BuildConfig.hpp"

#if FLOW_UI_DEV_MODE

#include <clay.h>

#include "devSystems/devTooling/schema/DevSchemaDescriptor.hpp"

namespace FlowUi::devMode {

template <> struct DevEnumAdapter<Clay_LayoutDirection> {
	static constexpr bool enabled = true;
	static consteval auto schema() { return devEnum<Clay_LayoutDirection>("Clay_LayoutDirection",
		devEnumValue("Horizontal", CLAY_LEFT_TO_RIGHT), devEnumValue("Vertical", CLAY_TOP_TO_BOTTOM)); }
};
template <> struct DevEnumAdapter<Clay_LayoutAlignmentX> {
	static constexpr bool enabled = true;
	static consteval auto schema() { return devEnum<Clay_LayoutAlignmentX>("Clay_LayoutAlignmentX",
		devEnumValue("Left", CLAY_ALIGN_X_LEFT), devEnumValue("Right", CLAY_ALIGN_X_RIGHT),
		devEnumValue("Center", CLAY_ALIGN_X_CENTER)); }
};
template <> struct DevEnumAdapter<Clay_LayoutAlignmentY> {
	static constexpr bool enabled = true;
	static consteval auto schema() { return devEnum<Clay_LayoutAlignmentY>("Clay_LayoutAlignmentY",
		devEnumValue("Top", CLAY_ALIGN_Y_TOP), devEnumValue("Bottom", CLAY_ALIGN_Y_BOTTOM),
		devEnumValue("Center", CLAY_ALIGN_Y_CENTER)); }
};
template <> struct DevEnumAdapter<Clay_TextElementConfigWrapMode> {
	static constexpr bool enabled = true;
	static consteval auto schema() { return devEnum<Clay_TextElementConfigWrapMode>("Clay_TextWrapMode",
		devEnumValue("Words", CLAY_TEXT_WRAP_WORDS), devEnumValue("Newlines", CLAY_TEXT_WRAP_NEWLINES),
		devEnumValue("None", CLAY_TEXT_WRAP_NONE)); }
};
template <> struct DevEnumAdapter<Clay_TextAlignment> {
	static constexpr bool enabled = true;
	static consteval auto schema() { return devEnum<Clay_TextAlignment>("Clay_TextAlignment",
		devEnumValue("Left", CLAY_TEXT_ALIGN_LEFT), devEnumValue("Center", CLAY_TEXT_ALIGN_CENTER),
		devEnumValue("Right", CLAY_TEXT_ALIGN_RIGHT)); }
};
template <> struct DevEnumAdapter<Clay_PointerCaptureMode> {
	static constexpr bool enabled = true;
	static consteval auto schema() { return devEnum<Clay_PointerCaptureMode>("Clay_PointerCaptureMode",
		devEnumValue("Capture", CLAY_POINTER_CAPTURE_MODE_CAPTURE),
		devEnumValue("Passthrough", CLAY_POINTER_CAPTURE_MODE_PASSTHROUGH)); }
};
template <> struct DevEnumAdapter<Clay_FloatingAttachToElement> {
	static constexpr bool enabled = true;
	static consteval auto schema() { return devEnum<Clay_FloatingAttachToElement>("Clay_FloatingAttachToElement",
		devEnumValue("None", CLAY_ATTACH_TO_NONE), devEnumValue("Parent", CLAY_ATTACH_TO_PARENT),
		devEnumValue("Element", CLAY_ATTACH_TO_ELEMENT_WITH_ID), devEnumValue("Root", CLAY_ATTACH_TO_ROOT)); }
};
template <> struct DevEnumAdapter<Clay_FloatingClipToElement> {
	static constexpr bool enabled = true;
	static consteval auto schema() { return devEnum<Clay_FloatingClipToElement>("Clay_FloatingClipToElement",
		devEnumValue("None", CLAY_CLIP_TO_NONE),
		devEnumValue("Attached parent", CLAY_CLIP_TO_ATTACHED_PARENT)); }
};
template <> struct DevEnumAdapter<Clay_FloatingAttachPointType> {
	static constexpr bool enabled = true;
	static consteval auto schema() { return devEnum<Clay_FloatingAttachPointType>("Clay_FloatingAttachPoint",
		devEnumValue("Left top", CLAY_ATTACH_POINT_LEFT_TOP), devEnumValue("Left center", CLAY_ATTACH_POINT_LEFT_CENTER),
		devEnumValue("Left bottom", CLAY_ATTACH_POINT_LEFT_BOTTOM), devEnumValue("Center top", CLAY_ATTACH_POINT_CENTER_TOP),
		devEnumValue("Center", CLAY_ATTACH_POINT_CENTER_CENTER), devEnumValue("Center bottom", CLAY_ATTACH_POINT_CENTER_BOTTOM),
		devEnumValue("Right top", CLAY_ATTACH_POINT_RIGHT_TOP), devEnumValue("Right center", CLAY_ATTACH_POINT_RIGHT_CENTER),
		devEnumValue("Right bottom", CLAY_ATTACH_POINT_RIGHT_BOTTOM)); }
};

template <>
struct DevEnumAdapter<Clay__SizingType> {
	static constexpr bool enabled = true;
	static consteval auto schema() {
		return devEnum<Clay__SizingType>(
			"Clay__SizingType",
			devEnumValue("Fit", CLAY__SIZING_TYPE_FIT),
			devEnumValue("Grow", CLAY__SIZING_TYPE_GROW),
			devEnumValue("Percent", CLAY__SIZING_TYPE_PERCENT),
			devEnumValue("Fixed", CLAY__SIZING_TYPE_FIXED));
	}
};

template <>
struct DevTypeAdapter<Clay_Color> {
	static constexpr bool enabled = true;
	static consteval auto schema() {
		return devSemanticStruct(
			"Clay_Color", DevEditorKind::Color,
			devField<&Clay_Color::r>("r", DevFieldOptions{}.numericRange(0.0, 255.0)),
			devField<&Clay_Color::g>("g", DevFieldOptions{}.numericRange(0.0, 255.0)),
			devField<&Clay_Color::b>("b", DevFieldOptions{}.numericRange(0.0, 255.0)),
			devField<&Clay_Color::a>("a", DevFieldOptions{}.numericRange(0.0, 255.0)));
	}
};

template <>
struct DevTypeAdapter<Clay_Padding> {
	static constexpr bool enabled = true;
	static consteval auto schema() {
		return devSemanticStruct(
			"Clay_Padding", DevEditorKind::Spacing,
			devField<&Clay_Padding::left>("left"),
			devField<&Clay_Padding::right>("right"),
			devField<&Clay_Padding::top>("top"),
			devField<&Clay_Padding::bottom>("bottom"));
	}
};

template <> struct DevTypeAdapter<Clay_BorderWidth> {
	static constexpr bool enabled = true;
	static consteval auto schema() { return devSemanticStruct("Clay_BorderWidth", DevEditorKind::Spacing,
		devField<&Clay_BorderWidth::left>("left"), devField<&Clay_BorderWidth::right>("right"),
		devField<&Clay_BorderWidth::top>("top"), devField<&Clay_BorderWidth::bottom>("bottom"),
		devField<&Clay_BorderWidth::betweenChildren>("betweenChildren")); }
};

template <>
struct DevTypeAdapter<Clay_CornerRadius> {
	static constexpr bool enabled = true;
	static consteval auto schema() {
		return devSemanticStruct(
			"Clay_CornerRadius", DevEditorKind::CornerRadius,
			devField<&Clay_CornerRadius::topLeft>("topLeft"),
			devField<&Clay_CornerRadius::topRight>("topRight"),
			devField<&Clay_CornerRadius::bottomLeft>("bottomLeft"),
			devField<&Clay_CornerRadius::bottomRight>("bottomRight"));
	}
};

template <>
struct DevTypeAdapter<Clay_Dimensions> {
	static constexpr bool enabled = true;
	static consteval auto schema() {
		return devSemanticStruct(
			"Clay_Dimensions", DevEditorKind::Vector,
			devField<&Clay_Dimensions::width>("width"),
			devField<&Clay_Dimensions::height>("height"));
	}
};

template <>
struct DevTypeAdapter<Clay_Vector2> {
	static constexpr bool enabled = true;
	static consteval auto schema() {
		return devSemanticStruct(
			"Clay_Vector2", DevEditorKind::Vector,
			devField<&Clay_Vector2::x>("x"),
			devField<&Clay_Vector2::y>("y"));
	}
};

template <>
struct DevTypeAdapter<Clay_SizingMinMax> {
	static constexpr bool enabled = true;
	static consteval auto schema() {
		return devSemanticStruct(
			"Clay_SizingMinMax", DevEditorKind::Vector,
			devField<&Clay_SizingMinMax::min>("min"),
			devField<&Clay_SizingMinMax::max>("max"));
	}
};

template <>
struct DevTypeAdapter<Clay_SizingAxis> {
	static constexpr bool enabled = true;
	static consteval auto schema() {
		// The SizingAxis editor interprets the active union member from `type`.
		// Generic traversal deliberately never exposes inactive union storage.
		return devSemanticStruct(
			"Clay_SizingAxis", DevEditorKind::SizingAxis,
			devField<&Clay_SizingAxis::type>("type"));
	}
};

template <>
struct DevTypeAdapter<Clay_Sizing> {
	static constexpr bool enabled = true;
	static consteval auto schema() {
		return devSemanticStruct(
			"Clay_Sizing", DevEditorKind::Sizing,
			devField<&Clay_Sizing::width>("width"),
			devField<&Clay_Sizing::height>("height"));
	}
};

template <> struct DevTypeAdapter<Clay_ChildAlignment> {
	static constexpr bool enabled = true;
	static consteval auto schema() { return devSemanticStruct("Clay_ChildAlignment", DevEditorKind::Custom,
		devField<&Clay_ChildAlignment::x>("x"), devField<&Clay_ChildAlignment::y>("y")); }
};
template <> struct DevTypeAdapter<Clay_FloatingAttachPoints> {
	static constexpr bool enabled = true;
	static consteval auto schema() { return devSemanticStruct("Clay_FloatingAttachPoints", DevEditorKind::Custom,
		devField<&Clay_FloatingAttachPoints::element>("element"), devField<&Clay_FloatingAttachPoints::parent>("parent")); }
};
template <> struct DevTypeAdapter<Clay_AspectRatioElementConfig> {
	static constexpr bool enabled = true;
	static consteval auto schema() { return devSemanticStruct("Clay_AspectRatioElementConfig", DevEditorKind::Custom,
		devField<&Clay_AspectRatioElementConfig::aspectRatio>("ratio", DevFieldOptions{}.numericRange(0.0, 10000.0))); }
};
template <> struct DevTypeAdapter<Clay_LayoutConfig> {
	static constexpr bool enabled = true;
	static consteval auto schema() { return devSemanticStruct("Clay_LayoutConfig", DevEditorKind::Custom,
		devField<&Clay_LayoutConfig::sizing>("sizing"), devField<&Clay_LayoutConfig::padding>("padding"),
		devField<&Clay_LayoutConfig::childGap>("childGap"), devField<&Clay_LayoutConfig::childAlignment>("childAlignment"),
		devField<&Clay_LayoutConfig::layoutDirection>("layoutDirection")); }
};
template <> struct DevTypeAdapter<Clay_TextElementConfig> {
	static constexpr bool enabled = true;
	static consteval auto schema() { return devSemanticStruct("Clay_TextElementConfig", DevEditorKind::Custom,
		devField<&Clay_TextElementConfig::textColor>("textColor"), devField<&Clay_TextElementConfig::fontId>("fontId"),
		devField<&Clay_TextElementConfig::fontSize>("fontSize"), devField<&Clay_TextElementConfig::letterSpacing>("letterSpacing"),
		devField<&Clay_TextElementConfig::lineHeight>("lineHeight"), devField<&Clay_TextElementConfig::wrapMode>("wrapMode"),
		devField<&Clay_TextElementConfig::textAlignment>("textAlignment"),
		devField<&Clay_TextElementConfig::userData>("userData", DevFieldOptions{}.readOnly())); }
};
template <> struct DevTypeAdapter<Clay_FloatingElementConfig> {
	static constexpr bool enabled = true;
	static consteval auto schema() { return devSemanticStruct("Clay_FloatingElementConfig", DevEditorKind::Custom,
		devField<&Clay_FloatingElementConfig::attachTo>("attachTo"), devField<&Clay_FloatingElementConfig::attachPoints>("attachPoints"),
		devField<&Clay_FloatingElementConfig::offset>("offset"), devField<&Clay_FloatingElementConfig::expand>("expand"),
		devField<&Clay_FloatingElementConfig::parentId>("parentId"), devField<&Clay_FloatingElementConfig::zIndex>("zIndex"),
		devField<&Clay_FloatingElementConfig::pointerCaptureMode>("pointerCaptureMode"), devField<&Clay_FloatingElementConfig::clipTo>("clipTo")); }
};
template <> struct DevTypeAdapter<Clay_ClipElementConfig> {
	static constexpr bool enabled = true;
	static consteval auto schema() { return devSemanticStruct("Clay_ClipElementConfig", DevEditorKind::Custom,
		devField<&Clay_ClipElementConfig::horizontal>("horizontal"), devField<&Clay_ClipElementConfig::vertical>("vertical"),
		devField<&Clay_ClipElementConfig::scrollInputDisabled>("scrollInputDisabled"),
		devField<&Clay_ClipElementConfig::childOffset>("childOffset")); }
};
template <> struct DevTypeAdapter<Clay_BorderElementConfig> {
	static constexpr bool enabled = true;
	static consteval auto schema() { return devSemanticStruct("Clay_BorderElementConfig", DevEditorKind::Custom,
		devField<&Clay_BorderElementConfig::color>("color"), devField<&Clay_BorderElementConfig::width>("width")); }
};

} // namespace FlowUi::devMode

#endif
