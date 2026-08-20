#pragma once

#include <algorithm>
#include <cstdint>
#include <optional>
#include <span>
#include <string_view>

#include "FSEL/StandardIcons.hpp"
#include "FSEL/Theme.hpp"
#include "FSEL/internal/ScrollIndicatorMath.hpp"
#include "managers/FlowUiElementBuilder.hpp"
#include "managers/PopupManager.hpp"

namespace FlowUi::FSEL {

struct ComboBoxOption {
	/** Stable identity and selected value. Values must be unique within options. */
	uint64_t value = 0;
	std::string_view text{};
	TextureRef icon{};
	bool enabled = true;
};

enum class ComboBoxPopupWidthPolicy : uint8_t {
	MatchTrigger,
	ContentAtLeastTrigger,
	Fixed,
};

struct ComboBoxParameters {
	std::span<const ComboBoxOption> options{};
	/** Borrowed authoritative selection. Null disables the control. */
	uint64_t* selectedValue = nullptr;
	/** Optional borrowed open state. Null selects element-owned state; keep the mode stable per element ID. */
	bool* open = nullptr;
	std::string_view placeholder = "Select...";
	bool enabled = true;
	ActionCall onChanged{};
	ActionCall onOpened{};
	ActionCall onClosed{};

	std::optional<Clay_Sizing> sizing = std::nullopt;
	ComboBoxPopupWidthPolicy popupWidthPolicy = ComboBoxPopupWidthPolicy::MatchTrigger;
	std::optional<float> popupWidth = std::nullopt;
	std::optional<float> popupMaxHeight = std::nullopt;
	std::optional<float> optionHeight = std::nullopt;
	bool showScrollIndicator = true;

	PopupPlacement placement = {
		.anchorPoint = PopupAttachmentPoint::BottomLeft,
		.popupPoint = PopupAttachmentPoint::TopLeft,
		.offset = Clay_Vector2{0.0f, 4.0f},
	};
	PopupOverflowPolicy overflow{};
	PopupLayer layer = PopupLayer::CasualPopup;
	PopupOutsidePressPolicy outsidePress =
		PopupOutsidePressPolicy::DismissAndBlockAnchor;
	Clay_PointerCaptureMode pointerCaptureMode = CLAY_POINTER_CAPTURE_MODE_CAPTURE;

	std::optional<TextureRef> closedIcon = std::nullopt;
	std::optional<TextureRef> openIcon = std::nullopt;
	std::optional<float> iconSize = std::nullopt;
	std::optional<FontFamilyId> fontFamily = std::nullopt;
	std::optional<uint32_t> fontWeight = std::nullopt;
	std::optional<FontStyle> fontStyle = std::nullopt;
	std::optional<uint16_t> fontSize = std::nullopt;
	std::optional<CursorType> cursor = std::nullopt;
	std::optional<uint8_t> cursorPriority = std::nullopt;
};

struct ComboBoxState {
	bool open = false;
	bool triggerArmed = false;
	bool triggerMouseUpObserved = false;
	bool optionArmed = false;
	bool optionMouseUpObserved = false;
	uint64_t armedOptionValue = 0;
};

struct ComboBoxResources {
	TextureRef openIcon{};
	TextureRef closedIcon{};

	ComboBoxResources() = default;
	explicit ComboBoxResources(App& app) {
#if FLOWUI_INCLUDE_ICON_MANAGER
		IconManager& icons = app.icons();
		if (icons.contains(standard_icons::kComboBoxOpenKey)) {
			openIcon = icons.textureRef(standard_icons::kComboBoxOpenKey);
		}
		if (icons.contains(standard_icons::kComboBoxClosedKey)) {
			closedIcon = icons.textureRef(standard_icons::kComboBoxClosedKey);
		}
#else
		(void)app;
#endif
	}
};

/** Build-only text/icon combo box. Custom option content is intentionally composed separately. */
struct ComboBox {
	using Parameters = ComboBoxParameters;
	using State = ComboBoxState;
	using Resources = ComboBoxResources;
	using BuildContext = ElementBuildContext<ComboBox>;
	using InteractionContext = ElementInteractionContext<ComboBox>;

	static constexpr FlowDefinitionID definitionId = DefinitionID("FSEL.combo-box");
	static constexpr std::string_view debugName = "FSEL ComboBox";

	struct Parts {
		static constexpr FlowElementPart popup = Part("popup");
	};

	static void onHovered(InteractionContext& context) {
		if (!isEnabled(context.params)) return;
		const auto& theme = context.uiManager.theme<FSELTheme>().comboBoxTheme;
		context.uiManager.requestCursor(
			context.params.cursor.value_or(theme.cursor),
			context.params.cursorPriority.value_or(theme.cursorPriority));
	}

	static void onPressed(InteractionContext& context) {
		if (!isEnabled(context.params)) {
			clearTrigger(context.state());
			return;
		}
		context.state().triggerArmed = true;
		context.state().triggerMouseUpObserved = false;
	}

	static void onReleased(InteractionContext& context) {
		if (!context.state().triggerArmed) return;
		clearTrigger(context.state());
		if (isEnabled(context.params)) {
			setOpen(context, !resolvedOpen(context));
		}
	}

	static void runLogic(InteractionContext& context) {
		auto& state = context.state();
		if (context.params.open) state.open = *context.params.open;

		if (context.uiManager.popups().consumeDismissed(context.part(Parts::popup))) {
			setOpen(context, false);
		}

		if (!isEnabled(context.params)) {
			if (resolvedOpen(context)) setOpen(context, false);
			clearTrigger(state);
			clearOption(state);
			return;
		}

		const InteractionSnapshot& interaction =
			context.uiManager.getPreviousFramesInteraction();
		for (const ComboBoxOption& option : context.params.options) {
			if (!option.enabled) continue;
			const Clay_ElementId optionId = context.clayID(Keyed("option", option.value));
			if (interaction.isPressed(optionId)) {
				state.optionArmed = true;
				state.optionMouseUpObserved = false;
				state.armedOptionValue = option.value;
			}
			if (state.optionArmed && state.armedOptionValue == option.value &&
				interaction.isReleased(optionId)) {
				const bool changed = *context.params.selectedValue != option.value;
				*context.params.selectedValue = option.value;
				clearOption(state);
				if (changed) (void)context.invoke(context.params.onChanged);
				setOpen(context, false);
				break;
			}
		}

		const bool pointerDown = context.uiManager.getCurrentFrameInput().mouseDown[0];
		maintainArm(pointerDown, state.triggerArmed, state.triggerMouseUpObserved);
		maintainArm(pointerDown, state.optionArmed, state.optionMouseUpObserved);
	}

	static void buildElement(BuildContext& context) {
		const auto& theme = context.uiManager.theme<FSELTheme>().comboBoxTheme;
		const bool enabled = isEnabled(context.params);
		const bool open = context.params.open ? *context.params.open : context.state().open;
		const ComboBoxOption* selected = selectedOption(context.params);
		const FSELComboBoxStateTheme appearance = resolveTriggerAppearance(
			context, enabled, open);

		Clay_ElementDeclaration trigger{};
		trigger.layout.sizing = context.params.sizing.value_or(Clay_Sizing{
			.width = CLAY_SIZING_FIXED(theme.width),
			.height = CLAY_SIZING_FIXED(theme.height),
		});
		trigger.layout.padding = theme.triggerPadding;
		trigger.layout.childGap = theme.contentGap;
		trigger.layout.childAlignment = {
			.x = CLAY_ALIGN_X_LEFT,
			.y = CLAY_ALIGN_Y_CENTER,
		};
		trigger.layout.layoutDirection = CLAY_LEFT_TO_RIGHT;
		trigger.backgroundColor = appearance.backgroundColor;
		trigger.cornerRadius = theme.cornerRadius;
		trigger.border = {.color = appearance.borderColor, .width = theme.borderWidth};

		const Clay_TextElementConfig textConfig = makeTextConfig(
			context, selected ? appearance.textColor : theme.placeholderColor);
		CLAY(context.clayID(), trigger) {
			if (selected && selected->icon.handle) {
				drawIcon(context, context.clayID("selected-icon"), selected->icon,
					appearance.iconColor, resolvedIconSize(context, theme));
			}
			Clay_ElementDeclaration labelRoot{};
			labelRoot.layout.sizing = {
				.width = CLAY_SIZING_GROW(0),
				.height = CLAY_SIZING_FIT(0),
			};
			CLAY(context.clayID("label"), labelRoot) {
				CLAY_TEXT(
					context.uiManager.toClayString(selected ? selected->text : context.params.placeholder),
					CLAY_TEXT_CONFIG(textConfig));
			}
			drawDisclosure(context, open, appearance.iconColor, theme);
		}

		if (open) drawPopup(context, theme);
	}

private:
	static bool isEnabled(const Parameters& params) {
		return params.enabled && params.selectedValue != nullptr;
	}

	static const ComboBoxOption* selectedOption(const Parameters& params) {
		if (!params.selectedValue) return nullptr;
		for (const ComboBoxOption& option : params.options) {
			if (option.value == *params.selectedValue) return &option;
		}
		return nullptr;
	}

	static bool resolvedOpen(const InteractionContext& context) {
		return context.params.open ? *context.params.open : context.state().open;
	}

	static void setOpen(InteractionContext& context, bool value) {
		const bool old = resolvedOpen(context);
		context.state().open = value;
		if (context.params.open) *context.params.open = value;
		if (old == value) return;
		(void)context.invoke(value ? context.params.onOpened : context.params.onClosed);
	}

	static void clearTrigger(State& state) {
		state.triggerArmed = false;
		state.triggerMouseUpObserved = false;
	}

	static void clearOption(State& state) {
		state.optionArmed = false;
		state.optionMouseUpObserved = false;
		state.armedOptionValue = 0;
	}

	static void maintainArm(bool pointerDown, bool& armed, bool& mouseUpObserved) {
		if (!armed) return;
		if (pointerDown) {
			if (mouseUpObserved) {
				armed = false;
				mouseUpObserved = false;
			}
			return;
		}
		if (mouseUpObserved) {
			armed = false;
			mouseUpObserved = false;
		} else {
			mouseUpObserved = true;
		}
	}

	static float resolvedIconSize(const BuildContext& context, const FSELComboBoxTheme& theme) {
		return std::max(context.params.iconSize.value_or(theme.iconSize), 1.0f);
	}

	static Clay_TextElementConfig makeTextConfig(
		BuildContext& context,
		Clay_Color color) {
		const auto& theme = context.uiManager.theme<FSELTheme>().comboBoxTheme;
		Clay_TextElementConfig config{};
		config.textColor = color;
		config.fontId = context.uiManager.resolveFont(
			context.params.fontFamily.value_or(theme.fontFamily),
			context.params.fontWeight.value_or(theme.fontWeight),
			context.params.fontStyle.value_or(theme.fontStyle));
		config.fontSize = context.params.fontSize.value_or(theme.fontSize);
		config.wrapMode = CLAY_TEXT_WRAP_NONE;
		config.textAlignment = CLAY_TEXT_ALIGN_LEFT;
		return config;
	}

	static FSELComboBoxStateTheme resolveTriggerAppearance(
		BuildContext& context,
		bool enabled,
		bool open) {
		const auto& theme = context.uiManager.theme<FSELTheme>().comboBoxTheme;
		if (!enabled) return theme.disabled;
		if (open) return theme.open;
		return context.uiManager.getPreviousFramesInteraction().isHovered(context.clayID())
			? theme.hovered : theme.idle;
	}

	static void drawIcon(
		BuildContext& context,
		Clay_ElementId id,
		TextureRef texture,
		Clay_Color tint,
		float size) {
		texture.tintEnabled = true;
		Clay_ElementDeclaration declaration{};
		declaration.layout.sizing = {
			.width = CLAY_SIZING_FIXED(size),
			.height = CLAY_SIZING_FIXED(size),
		};
		declaration.backgroundColor = tint;
		declaration.image = {.imageData = context.uiManager.imageData(texture)};
		CLAY(id, declaration);
	}

	static void drawDisclosure(
		BuildContext& context,
		bool open,
		Clay_Color color,
		const FSELComboBoxTheme& theme) {
		const TextureRef icon = open
			? context.params.openIcon.value_or(context.resources().openIcon)
			: context.params.closedIcon.value_or(context.resources().closedIcon);
		if (icon.handle) {
			drawIcon(context, context.clayID("disclosure-icon"), icon, color,
				resolvedIconSize(context, theme));
			return;
		}
		Clay_TextElementConfig fallback = makeTextConfig(context, color);
		fallback.textAlignment = CLAY_TEXT_ALIGN_CENTER;
		CLAY_TEXT(context.uiManager.toClayString(open ? "^" : "v"),
			CLAY_TEXT_CONFIG(fallback));
	}

	static void drawPopup(BuildContext& context, const FSELComboBoxTheme& theme) {
		const float optionHeight = std::max(
			context.params.optionHeight.value_or(theme.optionHeight), 1.0f);
		const auto& surface = context.uiManager.theme<FSELTheme>().popupSurfaceTheme;
		const float minimumPopupHeight = optionHeight +
			static_cast<float>(surface.padding.top + surface.padding.bottom);
		const float maximumHeight = std::max(
			context.params.popupMaxHeight.value_or(theme.popupMaxHeight),
			minimumPopupHeight);
		const float contentHeight = context.params.options.empty() ? optionHeight :
			optionHeight * static_cast<float>(context.params.options.size()) +
			static_cast<float>(theme.optionGap) *
				static_cast<float>(context.params.options.size() - 1u);
		const float popupHeight = std::min(
			contentHeight + static_cast<float>(surface.padding.top + surface.padding.bottom),
			maximumHeight);

		const Clay_ElementData triggerData = Clay_GetElementData(context.clayID());
		const float triggerWidth = triggerData.found && triggerData.boundingBox.width > 0.0f
			? triggerData.boundingBox.width : theme.width;
		const float requestedWidth = std::max(
			context.params.popupWidth.value_or(theme.width), 1.0f);
		float expectedWidth = triggerWidth;
		Clay_SizingAxis popupWidthSizing = CLAY_SIZING_FIXED(triggerWidth);
		switch (context.params.popupWidthPolicy) {
		case ComboBoxPopupWidthPolicy::MatchTrigger:
			break;
		case ComboBoxPopupWidthPolicy::Fixed:
			expectedWidth = requestedWidth;
			popupWidthSizing = CLAY_SIZING_FIXED(requestedWidth);
			break;
		case ComboBoxPopupWidthPolicy::ContentAtLeastTrigger:
			expectedWidth = 0.0f;
			popupWidthSizing = CLAY_SIZING_FIT(
				triggerWidth,
				std::max(triggerWidth, requestedWidth));
			break;
		}

		PopupRequest request{};
		request.anchor = PopupAnchor::element(context.id);
		request.placement = context.params.placement;
		request.overflow = context.params.overflow;
		request.layer = context.params.layer;
		request.pointerCaptureMode = context.params.pointerCaptureMode;
		request.outsidePress = context.params.outsidePress;
		if (expectedWidth > 0.0f) {
			request.expectedSize = Clay_Dimensions{expectedWidth, popupHeight};
		}
		const FlowElementPartID popupId = context.part(Parts::popup);
		const PopupFrame frame = context.uiManager.popups().request(popupId, request);
		if (!frame.visible && !frame.measureOnly) return;

		Clay_ElementDeclaration popup{};
		popup.layout.sizing = {
			.width = popupWidthSizing,
			.height = CLAY_SIZING_FIXED(popupHeight),
		};
		popup.layout.padding = surface.padding;
		popup.layout.childGap = theme.scrollGap;
		popup.layout.childAlignment = {.x = CLAY_ALIGN_X_LEFT, .y = CLAY_ALIGN_Y_TOP};
		popup.layout.layoutDirection = CLAY_LEFT_TO_RIGHT;
		popup.backgroundColor = surface.backgroundColor;
		popup.cornerRadius = surface.cornerRadius;
		popup.border = {.color = surface.borderColor, .width = surface.borderWidth};
		popup.floating = frame.floating;

		const Clay_ElementId scrollId = context.clayID("scroll-viewport");
		const Clay_ScrollContainerData scroll = Clay_GetScrollContainerData(scrollId);
		const float trackHeight = std::max(
			popupHeight - static_cast<float>(surface.padding.top + surface.padding.bottom),
			0.0f);
		const auto thumb = scroll.found && scroll.scrollPosition
			? detail::scroll_indicator::calculate(
				scroll.scrollContainerDimensions.height,
				scroll.contentDimensions.height,
				scroll.scrollPosition->y,
				trackHeight,
				theme.scrollThumbMinimum)
			: detail::scroll_indicator::ThumbGeometry{};

		CLAY(context.uiManager.toClayEID(popupId), popup) {
			Clay_ElementDeclaration viewport{};
			viewport.layout.sizing = {
				.width = CLAY_SIZING_GROW(0),
				.height = CLAY_SIZING_GROW(0),
			};
			viewport.layout.layoutDirection = CLAY_TOP_TO_BOTTOM;
			viewport.clip = {.vertical = true};
			CLAY(scrollId, viewport) {
				Clay_ElementDeclaration options{};
				options.layout.sizing = {
					.width = CLAY_SIZING_GROW(0),
					.height = CLAY_SIZING_FIT(0),
				};
				options.layout.childGap = theme.optionGap;
				options.layout.layoutDirection = CLAY_TOP_TO_BOTTOM;
				CLAY(context.clayID("options"), options) {
					for (const ComboBoxOption& option : context.params.options) {
						drawOption(context, option, optionHeight, theme);
					}
				}
			}
			if (context.params.showScrollIndicator && thumb.visible) {
				drawScrollIndicator(context, thumb, trackHeight, theme);
			}
		}
	}

	static void drawOption(
		BuildContext& context,
		const ComboBoxOption& option,
		float height,
		const FSELComboBoxTheme& theme) {
		const Clay_ElementId id = context.clayID(Keyed("option", option.value));
		const InteractionSnapshot& interaction =
			context.uiManager.getPreviousFramesInteraction();
		FSELComboBoxOptionStateTheme appearance = theme.optionIdle;
		if (!option.enabled) {
			appearance = theme.optionDisabled;
		} else if (context.state().optionArmed &&
			context.state().armedOptionValue == option.value &&
			context.uiManager.getCurrentFrameInput().mouseDown[0]) {
			appearance = theme.optionPressed;
		} else if (interaction.isHovered(id)) {
			appearance = theme.optionHovered;
		} else if (context.params.selectedValue &&
			*context.params.selectedValue == option.value) {
			appearance = theme.optionSelected;
		}

		Clay_ElementDeclaration row{};
		row.layout.sizing = {
			.width = CLAY_SIZING_GROW(0),
			.height = CLAY_SIZING_FIXED(height),
		};
		row.layout.padding = theme.optionPadding;
		row.layout.childGap = theme.contentGap;
		row.layout.childAlignment = {.x = CLAY_ALIGN_X_LEFT, .y = CLAY_ALIGN_Y_CENTER};
		row.layout.layoutDirection = CLAY_LEFT_TO_RIGHT;
		row.backgroundColor = appearance.backgroundColor;
		row.cornerRadius = theme.cornerRadius;
		CLAY(id, row) {
			if (option.icon.handle) {
				drawIcon(context, context.clayID(Keyed("option-icon", option.value)),
					option.icon, appearance.iconColor, resolvedIconSize(context, theme));
			}
			CLAY_TEXT(context.uiManager.toClayString(option.text),
				CLAY_TEXT_CONFIG(makeTextConfig(context, appearance.textColor)));
		}
	}

	static void drawScrollIndicator(
		BuildContext& context,
		const detail::scroll_indicator::ThumbGeometry& thumb,
		float trackHeight,
		const FSELComboBoxTheme& theme) {
		Clay_ElementDeclaration track{};
		track.layout.sizing = {
			.width = CLAY_SIZING_FIXED(theme.scrollTrackWidth),
			.height = CLAY_SIZING_FIXED(trackHeight),
		};
		track.layout.layoutDirection = CLAY_TOP_TO_BOTTOM;
		track.backgroundColor = theme.scrollTrackColor;
		track.cornerRadius = CLAY_CORNER_RADIUS(theme.scrollTrackWidth * 0.5f);
		CLAY(context.clayID("scroll-track"), track) {
			if (thumb.offset > 0.0f) {
				Clay_ElementDeclaration spacer{};
				spacer.layout.sizing.height = CLAY_SIZING_FIXED(thumb.offset);
				CLAY(context.clayID("scroll-thumb-offset"), spacer);
			}
			Clay_ElementDeclaration thumbDeclaration{};
			thumbDeclaration.layout.sizing = {
				.width = CLAY_SIZING_GROW(0),
				.height = CLAY_SIZING_FIXED(thumb.extent),
			};
			thumbDeclaration.backgroundColor = theme.scrollThumbColor;
			thumbDeclaration.cornerRadius = CLAY_CORNER_RADIUS(theme.scrollTrackWidth * 0.5f);
			CLAY(context.clayID("scroll-thumb"), thumbDeclaration);
		}
	}
};

inline constexpr ComboBox kComboBox{};
static_assert(FlowElement<ComboBox>);
static_assert(DrawableFlowElement<ComboBox>);
static_assert(!ConstructibleFlowElement<ComboBox>);

} // namespace FlowUi::FSEL
