#include "devSystems/devInterface/Permanents/Elements/DevInterfaceFooter.hpp"

#if FLOW_UI_DEV_MODE

#include <cstdio>
#include <string>

#include "devSystems/devInterface/Permanents/Backend/DevInterfaceIcons.hpp"
#include "devSystems/devInterface/Permanents/Backend/DevTheme.hpp"
#include "managers/UiManager.hpp"

namespace FlowUi::devSystems::interface_elements {
namespace {

constexpr float kFooterHeight = 32.0f;
inline constexpr LocalElementName kOperationalTag{"operational-state"};
inline constexpr LocalElementName kOperationalDot{"operational-state-dot"};
inline constexpr LocalElementName kFirstSeparatorInset{"first-separator-inset"};
inline constexpr LocalElementName kFirstSeparatorLine{"first-separator-line"};
inline constexpr LocalElementName kErrorReporter{"error-reporter"};
inline constexpr LocalElementName kErrorReporterIcon{"error-reporter-icon"};
inline constexpr LocalElementName kChangesReporter{"changes-reporter"};
inline constexpr LocalElementName kChangesReporterIcon{"changes-reporter-icon"};
inline constexpr LocalElementName kSecondSeparatorInset{"second-separator-inset"};
inline constexpr LocalElementName kSecondSeparatorLine{"second-separator-line"};
inline constexpr LocalElementName kActionSpacer{"action-spacer"};
inline constexpr LocalElementName kUnbakedStatusTag{"unbaked-status"};

Clay_TextElementConfig textConfig(Clay_Color color, uint16_t fontSize) {
	Clay_TextElementConfig config{};
	config.textColor = color;
	config.fontId = 0;
	config.fontSize = fontSize;
	config.wrapMode = CLAY_TEXT_WRAP_NONE;
	config.textAlignment = CLAY_TEXT_ALIGN_LEFT;
	return config;
}

void drawInsetSeparator(
	DevInterfaceFooter::BuildContext& context,
	LocalElementName insetId,
	LocalElementName lineId) {
	Clay_ElementDeclaration inset{};
	inset.layout.sizing = {
		.width = CLAY_SIZING_FIXED(1),
		.height = CLAY_SIZING_GROW(0),
	};
	inset.layout.padding = Clay_Padding{0, 0, 7, 7};

	Clay_ElementDeclaration line{};
	line.layout.sizing = {
		.width = CLAY_SIZING_FIXED(1),
		.height = CLAY_SIZING_GROW(0),
	};
	line.backgroundColor = interface_theme::kBorderPrimary;

	CLAY(context.clayID(insetId), inset) {
		CLAY(context.clayID(lineId), line);
	}
}

void drawOperationalTag(DevInterfaceFooter::BuildContext& context) {
	Clay_ElementDeclaration tag{};
	tag.layout.sizing = {
		.width = CLAY_SIZING_FIT(0),
		.height = CLAY_SIZING_FIXED(22),
	};
	tag.layout.padding = Clay_Padding{7, 7, 3, 3};
	tag.layout.childGap = 6;
	tag.layout.childAlignment = {
		.x = CLAY_ALIGN_X_LEFT,
		.y = CLAY_ALIGN_Y_CENTER,
	};
	tag.layout.layoutDirection = CLAY_LEFT_TO_RIGHT;
	tag.backgroundColor = interface_theme::kDepth2Ink;
	tag.cornerRadius = CLAY_CORNER_RADIUS(3);

	Clay_ElementDeclaration dot{};
	dot.layout.sizing = {
		.width = CLAY_SIZING_FIXED(7),
		.height = CLAY_SIZING_FIXED(7),
	};
	dot.backgroundColor = interface_theme::kStatusGreen;
	dot.cornerRadius = CLAY_CORNER_RADIUS(4);

	const Clay_TextElementConfig label = textConfig(interface_theme::kTextCanvas, 11);
	CLAY(context.clayID(kOperationalTag), tag) {
		CLAY(context.clayID(kOperationalDot), dot);
		CLAY_TEXT(context.uiManager.toClayString("RUNNING"), CLAY_TEXT_CONFIG(label));
	}
}

void drawReporter(
	DevInterfaceFooter::BuildContext& context,
	LocalElementName reporterId,
	LocalElementName iconId,
	TextureRef icon,
	Clay_Color color,
	std::string_view countText) {
	Clay_ElementDeclaration reporter{};
	reporter.layout.sizing = {
		.width = CLAY_SIZING_FIT(0),
		.height = CLAY_SIZING_GROW(0),
	};
	reporter.layout.childGap = 4;
	reporter.layout.childAlignment = {
		.x = CLAY_ALIGN_X_LEFT,
		.y = CLAY_ALIGN_Y_CENTER,
	};
	reporter.layout.layoutDirection = CLAY_LEFT_TO_RIGHT;

	Clay_ElementDeclaration iconDeclaration{};
	iconDeclaration.layout.sizing = {
		.width = CLAY_SIZING_FIXED(14),
		.height = CLAY_SIZING_FIXED(14),
	};
	icon.tintEnabled = true;
	iconDeclaration.backgroundColor = color;
	iconDeclaration.image = {
		.imageData = context.uiManager.imageData(icon),
	};

	const Clay_TextElementConfig number = textConfig(color, 12);
	CLAY(context.clayID(reporterId), reporter) {
		CLAY(context.clayID(iconId), iconDeclaration);
		CLAY_TEXT(context.uiManager.toClayString(countText), CLAY_TEXT_CONFIG(number));
	}
}

void drawUnbakedStatusTag(
	DevInterfaceFooter::BuildContext& context,
	uint32_t unbakedChangeCount,
	std::string_view statusText) {
	const bool hasChanges = unbakedChangeCount != 0u;
	Clay_ElementDeclaration tag{};
	tag.layout.sizing = {
		.width = CLAY_SIZING_FIT(0),
		.height = CLAY_SIZING_FIXED(22),
	};
	tag.layout.padding = Clay_Padding{8, 8, 3, 3};
	tag.layout.childAlignment = {
		.x = CLAY_ALIGN_X_CENTER,
		.y = CLAY_ALIGN_Y_CENTER,
	};
	tag.backgroundColor = hasChanges
		? interface_theme::kDepth2Ink
		: interface_theme::kDepth3Elevated;
	tag.cornerRadius = CLAY_CORNER_RADIUS(3);
	tag.border = {
		.color = hasChanges
			? interface_theme::kAccentSignalCoral
			: interface_theme::kBorderVisible,
		.width = Clay_BorderWidth{1, 1, 1, 1, 0},
	};

	const Clay_TextElementConfig label = textConfig(
		hasChanges ? interface_theme::kAccentSignalCoral : interface_theme::kAccentSeaGlass,
		11);
	CLAY(context.clayID(kUnbakedStatusTag), tag) {
		CLAY_TEXT(context.uiManager.toClayString(statusText), CLAY_TEXT_CONFIG(label));
	}
}

void drawFooterContents(
	DevInterfaceFooter::BuildContext& context,
	std::string_view errorCount,
	std::string_view changesCount,
	std::string_view unbakedStatus) {
	drawOperationalTag(context);
	drawInsetSeparator(context, kFirstSeparatorInset, kFirstSeparatorLine);
	drawReporter(
		context, kErrorReporter, kErrorReporterIcon,
		context.resources().errorIcon, interface_theme::kStatusRed, errorCount);
	drawReporter(
		context, kChangesReporter, kChangesReporterIcon,
		context.resources().unbakedChangesIcon,
		interface_theme::kAccentSignalCoral, changesCount);
	drawInsetSeparator(context, kSecondSeparatorInset, kSecondSeparatorLine);

	const Clay_TextElementConfig actionText = textConfig(interface_theme::kTextMuted, 11);
	CLAY_TEXT(
		context.uiManager.toClayString(context.params.lastActionMessage),
		CLAY_TEXT_CONFIG(actionText));

	Clay_ElementDeclaration spacer{};
	spacer.layout.sizing = {
		.width = CLAY_SIZING_GROW(0),
		.height = CLAY_SIZING_FIXED(1),
	};
	CLAY(context.clayID(kActionSpacer), spacer);

	drawUnbakedStatusTag(
		context, context.params.unbakedChangeCount, unbakedStatus);
}

} // namespace

DevInterfaceFooterResources::DevInterfaceFooterResources(App& app) {
#if FLOWUI_INCLUDE_ICON_MANAGER
	IconManager& icons = app.icons();
	interface_icons::registerDevInterfaceIcons(icons);
	if (icons.contains(interface_icons::kErrorReporterKey)) {
		errorIcon = icons.textureRef(interface_icons::kErrorReporterKey);
	}
	if (icons.contains(interface_icons::kUnbakedChangesReporterKey)) {
		unbakedChangesIcon = icons.textureRef(
			interface_icons::kUnbakedChangesReporterKey);
	}
#else
	(void)app;
#endif
}

void DevInterfaceFooter::buildElement(BuildContext& context) {
	char errorBuffer[24]{};
	char changesBuffer[24]{};
	char statusBuffer[64]{};
	std::snprintf(errorBuffer, sizeof(errorBuffer), "%u", context.params.errorCount);
	std::snprintf(changesBuffer, sizeof(changesBuffer), "%u", context.params.unbakedChangeCount);
	if (context.params.unbakedChangeCount == 0u) {
		std::snprintf(statusBuffer, sizeof(statusBuffer), "NO UNBAKED CHANGES");
	} else {
		std::snprintf(
			statusBuffer, sizeof(statusBuffer), "%u UNBAKED CHANGES",
			context.params.unbakedChangeCount);
	}

	Clay_ElementDeclaration footer{};
	footer.layout.sizing = {
		.width = CLAY_SIZING_GROW(0),
		.height = CLAY_SIZING_FIXED(kFooterHeight),
	};
	footer.layout.padding = Clay_Padding{12, 12, 0, 0};
	footer.layout.childGap = 9;
	footer.layout.childAlignment = {
		.x = CLAY_ALIGN_X_LEFT,
		.y = CLAY_ALIGN_Y_CENTER,
	};
	footer.layout.layoutDirection = CLAY_LEFT_TO_RIGHT;
	footer.backgroundColor = interface_theme::kDepth1Panel;
	footer.border = {
		.color = interface_theme::kBorderPrimary,
		.width = Clay_BorderWidth{0, 0, 1, 0, 0},
	};

	CLAY(context.clayID(), footer) {
		drawFooterContents(context, errorBuffer, changesBuffer, statusBuffer);
	}
}

} // namespace FlowUi::devSystems::interface_elements

#endif
