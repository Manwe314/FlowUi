#pragma once

#include "FlowUi/BuildConfig.hpp"

#if FLOW_UI_DEV_MODE

#include <clay.h>

#include "devSystems/devTooling/schema/DevSchemaDescriptor.hpp"

namespace FlowUi::devMode {

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

} // namespace FlowUi::devMode

#endif
