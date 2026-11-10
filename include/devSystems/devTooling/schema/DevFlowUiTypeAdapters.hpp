#pragma once

#include "FlowUi/Public/BuildConfig.hpp"

#if FLOW_UI_DEV_MODE

#include "FlowUi/Public/FlowUiTheme.hpp"
#include "FlowUi/Public/TextureRef.hpp"
#include "devSystems/devTooling/schema/DevSchemaDescriptor.hpp"
#include "FlowUi/Public/managerStructs/ActionTypes.hpp"

namespace FlowUi::devSystems::tooling {

template <>
/** @brief Built-in reflection adapter for ActionCall. */
struct DevTypeAdapter<ActionCall> {
	/** @brief Enables this built-in specialization during schema discovery. */
	static constexpr bool enabled = true;
	/** @brief Return the static typed descriptor used to publish this type's editor and field metadata. */
	static consteval auto schema() {
		return devSemanticLeaf(
			"ActionCall",
			DevEditorKind::ActionChoice,
			DevCaptureCapability::Value,
			DevEditCapability::SemanticCommand);
	}
};

template <>
/** @brief Built-in reflection adapter for TextureFitMode. */
struct DevEnumAdapter<TextureFitMode> {
	/** @brief Enables this built-in specialization during schema discovery. */
	static constexpr bool enabled = true;
	/** @brief Return the static typed descriptor used to publish this type's editor and field metadata. */
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
/** @brief Built-in reflection adapter for TextureRef. */
struct DevTypeAdapter<TextureRef> {
	/** @brief Enables this built-in specialization during schema discovery. */
	static constexpr bool enabled = true;
	/** @brief Return the static typed descriptor used to publish this type's editor and field metadata. */
	static consteval auto schema() {
		return devSemanticStruct(
			"TextureRef", DevEditorKind::ResourceChoice,
			devField<&TextureRef::handle>("handle", DevFieldOptions{}.readOnly()),
			devField<&TextureRef::uv0x>("uv0x", DevFieldOptions{}.readOnly()),
			devField<&TextureRef::uv0y>("uv0y", DevFieldOptions{}.readOnly()),
			devField<&TextureRef::uv1x>("uv1x", DevFieldOptions{}.readOnly()),
			devField<&TextureRef::uv1y>("uv1y", DevFieldOptions{}.readOnly()),
			devField<&TextureRef::fitMode>("fitMode"),
			devField<&TextureRef::tintEnabled>("tintEnabled"),
			devField<&TextureRef::skipIfUnavailable>("skipIfUnavailable",
													 DevFieldOptions{}.readOnly()),
			devField<&TextureRef::sourceWidth>("sourceWidth", DevFieldOptions{}.readOnly()),
			devField<&TextureRef::sourceHeight>("sourceHeight", DevFieldOptions{}.readOnly()));
	}
};

template <>
/** @brief Built-in reflection adapter for FlowUiTheme. */
struct DevTypeAdapter<FlowUiTheme> {
	/** @brief Enables this built-in specialization during schema discovery. */
	static constexpr bool enabled = true;
	/** @brief Return the static typed descriptor used to publish this type's editor and field metadata. */
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
			devField<&FlowUiTheme::defaultFontFamily>("defaultFontFamily", DevFieldOptions{}
				.withEditor(DevEditorKind::FontChoice).withChoiceDomain(DevChoiceDomain::FontFamily)),
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

} // namespace FlowUi::devSystems::tooling

#endif
