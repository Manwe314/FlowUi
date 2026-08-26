#pragma once

#include "FlowUi/BuildConfig.hpp"

#if FLOW_UI_DEV_MODE

#include "FlowUi/PublicStructs.hpp"
#include "devSystems/devTooling/schema/DevSchemaDescriptor.hpp"
#include "managers/structs/ActionManagerStructs.hpp"

namespace FlowUi::devMode {

template <>
struct DevTypeAdapter<ActionCall> {
	static constexpr bool enabled = true;
	static consteval auto schema() {
		// The action catalogue is manager-owned and is not retained in the type graph.
		// A later binding snapshot may upgrade this to a live ActionChoice control.
		return devSemanticLeaf(
			"ActionCall",
			DevEditorKind::ActionChoice,
			DevCaptureCapability::Value,
			DevEditCapability::ViewOnly,
			DevCapabilityReason::NoEditAdapter);
	}
};

template <>
struct DevEnumAdapter<TextureFitMode> {
	static constexpr bool enabled = true;
	static consteval auto schema() {
		return devEnum<TextureFitMode>(
			"TextureFitMode",
			devEnumValue("Stretch", TextureFitMode::Stretch),
			devEnumValue("Contain", TextureFitMode::Contain),
			devEnumValue("Cover", TextureFitMode::Cover),
			devEnumValue("None", TextureFitMode::None));
	}
};

template <>
struct DevEnumAdapter<TextureSamplingMode> {
	static constexpr bool enabled = true;
	static consteval auto schema() {
		return devEnum<TextureSamplingMode>(
			"TextureSamplingMode",
			devEnumValue("Linear", TextureSamplingMode::Linear),
			devEnumValue("Nearest", TextureSamplingMode::Nearest));
	}
};

template <>
struct DevTypeAdapter<TextureRef> {
	static constexpr bool enabled = true;
	static consteval auto schema() {
		return devSemanticStruct(
			"TextureRef", DevEditorKind::ResourceChoice,
			devField<&TextureRef::handle>("handle", DevFieldOptions{}.readOnly()),
			devField<&TextureRef::uv0x>("uv0x", DevFieldOptions{}.readOnly()),
			devField<&TextureRef::uv0y>("uv0y", DevFieldOptions{}.readOnly()),
			devField<&TextureRef::uv1x>("uv1x", DevFieldOptions{}.readOnly()),
			devField<&TextureRef::uv1y>("uv1y", DevFieldOptions{}.readOnly()),
			devField<&TextureRef::fitMode>("fitMode"),
			devField<&TextureRef::samplingMode>("samplingMode"),
			devField<&TextureRef::tintEnabled>("tintEnabled"),
			devField<&TextureRef::skipIfUnavailable>(
				"skipIfUnavailable", DevFieldOptions{}.readOnly()),
			devField<&TextureRef::sourceWidth>("sourceWidth", DevFieldOptions{}.readOnly()),
			devField<&TextureRef::sourceHeight>("sourceHeight", DevFieldOptions{}.readOnly()));
	}
};

template <>
struct DevTypeAdapter<FlowUiTheme> {
	static constexpr bool enabled = true;
	static consteval auto schema() {
		return devStruct(
			"FlowUiTheme",
			devField<&FlowUiTheme::primary>("primary"),
			devField<&FlowUiTheme::primaryHover>("primaryHover"),
			devField<&FlowUiTheme::primaryActive>("primaryActive"),
			devField<&FlowUiTheme::onPrimary>("onPrimary"),
			devField<&FlowUiTheme::background>("background"),
			devField<&FlowUiTheme::surface>("surface"),
			devField<&FlowUiTheme::surfaceHeader>("surfaceHeader"),
			devField<&FlowUiTheme::surfaceHover>("surfaceHover"),
			devField<&FlowUiTheme::surfaceSelected>("surfaceSelected"),
			devField<&FlowUiTheme::textPrimary>("textPrimary"),
			devField<&FlowUiTheme::textSecondary>("textSecondary"),
			devField<&FlowUiTheme::textDisabled>("textDisabled"),
			devField<&FlowUiTheme::textLink>("textLink"),
			devField<&FlowUiTheme::border>("border"),
			devField<&FlowUiTheme::borderFocused>("borderFocused"),
			devField<&FlowUiTheme::divider>("divider"),
			devField<&FlowUiTheme::success>("success"),
			devField<&FlowUiTheme::warning>("warning"),
			devField<&FlowUiTheme::danger>("danger"),
			devField<&FlowUiTheme::radiusSmall>("radiusSmall"),
			devField<&FlowUiTheme::radiusMedium>("radiusMedium"),
			devField<&FlowUiTheme::radiusLarge>("radiusLarge"),
			devField<&FlowUiTheme::radiusPill>("radiusPill"),
			devField<&FlowUiTheme::defaultFontFamily>("defaultFontFamily"),
			devField<&FlowUiTheme::fontSizeSmall>("fontSizeSmall"),
			devField<&FlowUiTheme::fontSizeMedium>("fontSizeMedium"),
			devField<&FlowUiTheme::fontSizeLarge>("fontSizeLarge"),
			devField<&FlowUiTheme::fontSizeHeader>("fontSizeHeader"),
			devField<&FlowUiTheme::spacingXs>("spacingXs"),
			devField<&FlowUiTheme::spacingSm>("spacingSm"),
			devField<&FlowUiTheme::spacingMd>("spacingMd"),
			devField<&FlowUiTheme::spacingLg>("spacingLg"));
	}
};

} // namespace FlowUi::devMode

#endif
