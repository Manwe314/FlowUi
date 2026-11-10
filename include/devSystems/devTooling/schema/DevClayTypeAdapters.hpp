#pragma once

#include "FlowUi/Public/BuildConfig.hpp"

#if FLOW_UI_DEV_MODE

#include <clay.h>

#include "devSystems/devTooling/schema/DevSchemaDescriptor.hpp"

namespace FlowUi::devSystems::tooling {

/** @brief Built-in reflection adapter for Clay_LayoutDirection. */
template <> struct DevEnumAdapter<Clay_LayoutDirection> {
	/** @brief Enables this built-in specialization during schema discovery. */
	static constexpr bool enabled = true;
	/** @brief Return the static typed descriptor used to publish this type's editor and field metadata. */
	static consteval auto schema() { return devEnum<Clay_LayoutDirection>("Clay_LayoutDirection",
		devEnumValue("Horizontal", CLAY_LEFT_TO_RIGHT), devEnumValue("Vertical", CLAY_TOP_TO_BOTTOM)); }
};
/** @brief Built-in reflection adapter for Clay_LayoutAlignmentX. */
template <> struct DevEnumAdapter<Clay_LayoutAlignmentX> {
	/** @brief Enables this built-in specialization during schema discovery. */
	static constexpr bool enabled = true;
	/** @brief Return the static typed descriptor used to publish this type's editor and field metadata. */
	static consteval auto schema() { return devEnum<Clay_LayoutAlignmentX>("Clay_LayoutAlignmentX",
		devEnumValue("Left", CLAY_ALIGN_X_LEFT), devEnumValue("Right", CLAY_ALIGN_X_RIGHT),
		devEnumValue("Center", CLAY_ALIGN_X_CENTER)); }
};
/** @brief Built-in reflection adapter for Clay_LayoutAlignmentY. */
template <> struct DevEnumAdapter<Clay_LayoutAlignmentY> {
	/** @brief Enables this built-in specialization during schema discovery. */
	static constexpr bool enabled = true;
	/** @brief Return the static typed descriptor used to publish this type's editor and field metadata. */
	static consteval auto schema() { return devEnum<Clay_LayoutAlignmentY>("Clay_LayoutAlignmentY",
		devEnumValue("Top", CLAY_ALIGN_Y_TOP), devEnumValue("Bottom", CLAY_ALIGN_Y_BOTTOM),
		devEnumValue("Center", CLAY_ALIGN_Y_CENTER)); }
};
/** @brief Built-in reflection adapter for Clay_TextElementConfigWrapMode. */
template <> struct DevEnumAdapter<Clay_TextElementConfigWrapMode> {
	/** @brief Enables this built-in specialization during schema discovery. */
	static constexpr bool enabled = true;
	/** @brief Return the static typed descriptor used to publish this type's editor and field metadata. */
	static consteval auto schema() { return devEnum<Clay_TextElementConfigWrapMode>("Clay_TextWrapMode",
		devEnumValue("Words", CLAY_TEXT_WRAP_WORDS), devEnumValue("Newlines", CLAY_TEXT_WRAP_NEWLINES),
		devEnumValue("None", CLAY_TEXT_WRAP_NONE)); }
};
/** @brief Built-in reflection adapter for Clay_TextAlignment. */
template <> struct DevEnumAdapter<Clay_TextAlignment> {
	/** @brief Enables this built-in specialization during schema discovery. */
	static constexpr bool enabled = true;
	/** @brief Return the static typed descriptor used to publish this type's editor and field metadata. */
	static consteval auto schema() { return devEnum<Clay_TextAlignment>("Clay_TextAlignment",
		devEnumValue("Left", CLAY_TEXT_ALIGN_LEFT), devEnumValue("Center", CLAY_TEXT_ALIGN_CENTER),
		devEnumValue("Right", CLAY_TEXT_ALIGN_RIGHT)); }
};
/** @brief Built-in reflection adapter for Clay_PointerCaptureMode. */
template <> struct DevEnumAdapter<Clay_PointerCaptureMode> {
	/** @brief Enables this built-in specialization during schema discovery. */
	static constexpr bool enabled = true;
	/** @brief Return the static typed descriptor used to publish this type's editor and field metadata. */
	static consteval auto schema() { return devEnum<Clay_PointerCaptureMode>("Clay_PointerCaptureMode",
		devEnumValue("Capture", CLAY_POINTER_CAPTURE_MODE_CAPTURE),
		devEnumValue("Passthrough", CLAY_POINTER_CAPTURE_MODE_PASSTHROUGH)); }
};
/** @brief Built-in reflection adapter for Clay_FloatingAttachToElement. */
template <> struct DevEnumAdapter<Clay_FloatingAttachToElement> {
	/** @brief Enables this built-in specialization during schema discovery. */
	static constexpr bool enabled = true;
	/** @brief Return the static typed descriptor used to publish this type's editor and field metadata. */
	static consteval auto schema() { return devEnum<Clay_FloatingAttachToElement>("Clay_FloatingAttachToElement",
		devEnumValue("None", CLAY_ATTACH_TO_NONE), devEnumValue("Parent", CLAY_ATTACH_TO_PARENT),
		devEnumValue("Element", CLAY_ATTACH_TO_ELEMENT_WITH_ID), devEnumValue("Root", CLAY_ATTACH_TO_ROOT)); }
};
/** @brief Built-in reflection adapter for Clay_FloatingClipToElement. */
template <> struct DevEnumAdapter<Clay_FloatingClipToElement> {
	/** @brief Enables this built-in specialization during schema discovery. */
	static constexpr bool enabled = true;
	/** @brief Return the static typed descriptor used to publish this type's editor and field metadata. */
	static consteval auto schema() { return devEnum<Clay_FloatingClipToElement>("Clay_FloatingClipToElement",
		devEnumValue("None", CLAY_CLIP_TO_NONE),
		devEnumValue("Attached parent", CLAY_CLIP_TO_ATTACHED_PARENT)); }
};
/** @brief Built-in reflection adapter for Clay_FloatingAttachPointType. */
template <> struct DevEnumAdapter<Clay_FloatingAttachPointType> {
	/** @brief Enables this built-in specialization during schema discovery. */
	static constexpr bool enabled = true;
	/** @brief Return the static typed descriptor used to publish this type's editor and field metadata. */
	static consteval auto schema() { return devEnum<Clay_FloatingAttachPointType>("Clay_FloatingAttachPoint",
		devEnumValue("Left top", CLAY_ATTACH_POINT_LEFT_TOP), devEnumValue("Left center", CLAY_ATTACH_POINT_LEFT_CENTER),
		devEnumValue("Left bottom", CLAY_ATTACH_POINT_LEFT_BOTTOM), devEnumValue("Center top", CLAY_ATTACH_POINT_CENTER_TOP),
		devEnumValue("Center", CLAY_ATTACH_POINT_CENTER_CENTER), devEnumValue("Center bottom", CLAY_ATTACH_POINT_CENTER_BOTTOM),
		devEnumValue("Right top", CLAY_ATTACH_POINT_RIGHT_TOP), devEnumValue("Right center", CLAY_ATTACH_POINT_RIGHT_CENTER),
		devEnumValue("Right bottom", CLAY_ATTACH_POINT_RIGHT_BOTTOM)); }
};

template <>
/** @brief Built-in reflection adapter for Clay__SizingType. */
struct DevEnumAdapter<Clay__SizingType> {
	/** @brief Enables this built-in specialization during schema discovery. */
	static constexpr bool enabled = true;
	/** @brief Return the static typed descriptor used to publish this type's editor and field metadata. */
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
/** @brief Built-in reflection adapter for Clay_Color. */
struct DevTypeAdapter<Clay_Color> {
	/** @brief Enables this built-in specialization during schema discovery. */
	static constexpr bool enabled = true;
	/** @brief Return the static typed descriptor used to publish this type's editor and field metadata. */
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
/** @brief Built-in reflection adapter for Clay_Padding. */
struct DevTypeAdapter<Clay_Padding> {
	/** @brief Enables this built-in specialization during schema discovery. */
	static constexpr bool enabled = true;
	/** @brief Return the static typed descriptor used to publish this type's editor and field metadata. */
	static consteval auto schema() {
		return devSemanticStruct(
			"Clay_Padding", DevEditorKind::Spacing,
			devField<&Clay_Padding::left>("left"),
			devField<&Clay_Padding::right>("right"),
			devField<&Clay_Padding::top>("top"),
			devField<&Clay_Padding::bottom>("bottom"));
	}
};

/** @brief Built-in reflection adapter for Clay_BorderWidth. */
template <> struct DevTypeAdapter<Clay_BorderWidth> {
	/** @brief Enables this built-in specialization during schema discovery. */
	static constexpr bool enabled = true;
	/** @brief Return the static typed descriptor used to publish this type's editor and field metadata. */
	static consteval auto schema() { return devSemanticStruct("Clay_BorderWidth", DevEditorKind::Spacing,
		devField<&Clay_BorderWidth::left>("left"), devField<&Clay_BorderWidth::right>("right"),
		devField<&Clay_BorderWidth::top>("top"), devField<&Clay_BorderWidth::bottom>("bottom"),
		devField<&Clay_BorderWidth::betweenChildren>("betweenChildren")); }
};

template <>
/** @brief Built-in reflection adapter for Clay_CornerRadius. */
struct DevTypeAdapter<Clay_CornerRadius> {
	/** @brief Enables this built-in specialization during schema discovery. */
	static constexpr bool enabled = true;
	/** @brief Return the static typed descriptor used to publish this type's editor and field metadata. */
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
/** @brief Built-in reflection adapter for Clay_Dimensions. */
struct DevTypeAdapter<Clay_Dimensions> {
	/** @brief Enables this built-in specialization during schema discovery. */
	static constexpr bool enabled = true;
	/** @brief Return the static typed descriptor used to publish this type's editor and field metadata. */
	static consteval auto schema() {
		return devSemanticStruct(
			"Clay_Dimensions", DevEditorKind::Vector,
			devField<&Clay_Dimensions::width>("width"),
			devField<&Clay_Dimensions::height>("height"));
	}
};

template <>
/** @brief Built-in reflection adapter for Clay_Vector2. */
struct DevTypeAdapter<Clay_Vector2> {
	/** @brief Enables this built-in specialization during schema discovery. */
	static constexpr bool enabled = true;
	/** @brief Return the static typed descriptor used to publish this type's editor and field metadata. */
	static consteval auto schema() {
		return devSemanticStruct(
			"Clay_Vector2", DevEditorKind::Vector,
			devField<&Clay_Vector2::x>("x"),
			devField<&Clay_Vector2::y>("y"));
	}
};

template <>
/** @brief Built-in reflection adapter for Clay_SizingMinMax. */
struct DevTypeAdapter<Clay_SizingMinMax> {
	/** @brief Enables this built-in specialization during schema discovery. */
	static constexpr bool enabled = true;
	/** @brief Return the static typed descriptor used to publish this type's editor and field metadata. */
	static consteval auto schema() {
		return devSemanticStruct(
			"Clay_SizingMinMax", DevEditorKind::Vector,
			devField<&Clay_SizingMinMax::min>("min"),
			devField<&Clay_SizingMinMax::max>("max"));
	}
};

template <>
/** @brief Built-in reflection adapter for Clay_SizingAxis. */
struct DevTypeAdapter<Clay_SizingAxis> {
	/** @brief Enables this built-in specialization during schema discovery. */
	static constexpr bool enabled = true;
	/** @brief Return the static typed descriptor used to publish this type's editor and field metadata. */
	static consteval auto schema() {
		// The SizingAxis editor interprets the active union member from `type`.
		// Generic traversal deliberately never exposes inactive union storage.
		return devSemanticStruct(
			"Clay_SizingAxis", DevEditorKind::SizingAxis,
			devField<&Clay_SizingAxis::type>("type"));
	}
};

template <>
/** @brief Built-in reflection adapter for Clay_Sizing. */
struct DevTypeAdapter<Clay_Sizing> {
	/** @brief Enables this built-in specialization during schema discovery. */
	static constexpr bool enabled = true;
	/** @brief Return the static typed descriptor used to publish this type's editor and field metadata. */
	static consteval auto schema() {
		return devSemanticStruct(
			"Clay_Sizing", DevEditorKind::Sizing,
			devField<&Clay_Sizing::width>("width"),
			devField<&Clay_Sizing::height>("height"));
	}
};

/** @brief Built-in reflection adapter for Clay_ChildAlignment. */
template <> struct DevTypeAdapter<Clay_ChildAlignment> {
	/** @brief Enables this built-in specialization during schema discovery. */
	static constexpr bool enabled = true;
	/** @brief Return the static typed descriptor used to publish this type's editor and field metadata. */
	static consteval auto schema() { return devSemanticStruct("Clay_ChildAlignment", DevEditorKind::Custom,
		devField<&Clay_ChildAlignment::x>("x"), devField<&Clay_ChildAlignment::y>("y")); }
};
/** @brief Built-in reflection adapter for Clay_FloatingAttachPoints. */
template <> struct DevTypeAdapter<Clay_FloatingAttachPoints> {
	/** @brief Enables this built-in specialization during schema discovery. */
	static constexpr bool enabled = true;
	/** @brief Return the static typed descriptor used to publish this type's editor and field metadata. */
	static consteval auto schema() { return devSemanticStruct("Clay_FloatingAttachPoints", DevEditorKind::AttachmentPoints,
		devField<&Clay_FloatingAttachPoints::element>("element"), devField<&Clay_FloatingAttachPoints::parent>("parent")); }
};
/** @brief Built-in reflection adapter for Clay_AspectRatioElementConfig. */
template <> struct DevTypeAdapter<Clay_AspectRatioElementConfig> {
	/** @brief Enables this built-in specialization during schema discovery. */
	static constexpr bool enabled = true;
	/** @brief Return the static typed descriptor used to publish this type's editor and field metadata. */
	static consteval auto schema() { return devSemanticStruct("Clay_AspectRatioElementConfig", DevEditorKind::Custom,
		devField<&Clay_AspectRatioElementConfig::aspectRatio>("ratio", DevFieldOptions{}.numericRange(0.0, 10000.0))); }
};
/** @brief Built-in reflection adapter for Clay_LayoutConfig. */
template <> struct DevTypeAdapter<Clay_LayoutConfig> {
	/** @brief Enables this built-in specialization during schema discovery. */
	static constexpr bool enabled = true;
	/** @brief Return the static typed descriptor used to publish this type's editor and field metadata. */
	static consteval auto schema() { return devSemanticStruct("Clay_LayoutConfig", DevEditorKind::Custom,
		devField<&Clay_LayoutConfig::sizing>("sizing"), devField<&Clay_LayoutConfig::padding>("padding"),
		devField<&Clay_LayoutConfig::childGap>("childGap"), devField<&Clay_LayoutConfig::childAlignment>("childAlignment"),
		devField<&Clay_LayoutConfig::layoutDirection>("layoutDirection")); }
};
/** @brief Built-in reflection adapter for Clay_TextElementConfig. */
template <> struct DevTypeAdapter<Clay_TextElementConfig> {
	/** @brief Enables this built-in specialization during schema discovery. */
	static constexpr bool enabled = true;
	/** @brief Return the static typed descriptor used to publish this type's editor and field metadata. */
	static consteval auto schema() { return devSemanticStruct("Clay_TextElementConfig", DevEditorKind::Custom,
		devField<&Clay_TextElementConfig::textColor>("textColor"),
		devField<&Clay_TextElementConfig::fontId>("fontId", DevFieldOptions{}
			.withEditor(DevEditorKind::FontChoice).withChoiceDomain(DevChoiceDomain::FontFace)),
		devField<&Clay_TextElementConfig::fontSize>("fontSize"), devField<&Clay_TextElementConfig::letterSpacing>("letterSpacing"),
		devField<&Clay_TextElementConfig::lineHeight>("lineHeight"), devField<&Clay_TextElementConfig::wrapMode>("wrapMode"),
		devField<&Clay_TextElementConfig::textAlignment>("textAlignment"),
		devField<&Clay_TextElementConfig::userData>("userData", DevFieldOptions{}.readOnly())); }
};
/** @brief Built-in reflection adapter for Clay_FloatingElementConfig. */
template <> struct DevTypeAdapter<Clay_FloatingElementConfig> {
	/** @brief Enables this built-in specialization during schema discovery. */
	static constexpr bool enabled = true;
	/** @brief Return the static typed descriptor used to publish this type's editor and field metadata. */
	static consteval auto schema() { return devSemanticStruct("Clay_FloatingElementConfig", DevEditorKind::Custom,
		devField<&Clay_FloatingElementConfig::attachTo>("attachTo"), devField<&Clay_FloatingElementConfig::attachPoints>("attachPoints"),
		devField<&Clay_FloatingElementConfig::offset>("offset"), devField<&Clay_FloatingElementConfig::expand>("expand"),
		devField<&Clay_FloatingElementConfig::parentId>("parentId"), devField<&Clay_FloatingElementConfig::zIndex>("zIndex"),
		devField<&Clay_FloatingElementConfig::pointerCaptureMode>("pointerCaptureMode"), devField<&Clay_FloatingElementConfig::clipTo>("clipTo")); }
};
/** @brief Built-in reflection adapter for Clay_ClipElementConfig. */
template <> struct DevTypeAdapter<Clay_ClipElementConfig> {
	/** @brief Enables this built-in specialization during schema discovery. */
	static constexpr bool enabled = true;
	/** @brief Return the static typed descriptor used to publish this type's editor and field metadata. */
	static consteval auto schema() { return devSemanticStruct("Clay_ClipElementConfig", DevEditorKind::Custom,
		devField<&Clay_ClipElementConfig::horizontal>("horizontal"), devField<&Clay_ClipElementConfig::vertical>("vertical"),
		devField<&Clay_ClipElementConfig::scrollInputDisabled>("scrollInputDisabled"),
		devField<&Clay_ClipElementConfig::childOffset>("childOffset")); }
};
/** @brief Built-in reflection adapter for Clay_BorderElementConfig. */
template <> struct DevTypeAdapter<Clay_BorderElementConfig> {
	/** @brief Enables this built-in specialization during schema discovery. */
	static constexpr bool enabled = true;
	/** @brief Return the static typed descriptor used to publish this type's editor and field metadata. */
	static consteval auto schema() { return devSemanticStruct("Clay_BorderElementConfig", DevEditorKind::Custom,
		devField<&Clay_BorderElementConfig::color>("color"), devField<&Clay_BorderElementConfig::width>("width")); }
};

} // namespace FlowUi::devSystems::tooling

#endif
