#pragma once

#include <algorithm>
#include <cstdint>
#include <optional>
#include <span>
#include <string>
#include <string_view>

#include "FSEL/StandardIcons.hpp"
#include "FSEL/TextInput.hpp"
#include "FSEL/Theme.hpp"
#include "FSEL/internal/ComboBoxLayout.hpp"
#include "FSEL/internal/ScrollIndicatorMath.hpp"
#include "FlowUi/Public/elementAuthoring/ElementBuilder.hpp"
#include "FlowUi/Public/managers/PopupManager.hpp"

namespace FlowUi::FSEL {

/** One borrowed display option with a stable unique selection value. */
struct ComboBoxOption {
	/** Stable identity and selected value. Values must be unique within options. */
	uint64_t value = 0;
	/** Borrowed UTF-8 display text; keep its storage valid through the frame. */
	std::string_view text{};
	/** Borrowed texture reference for content artwork; an empty reference draws no icon. */
	TextureRef icon{};
	/** Allow interaction; false also selects disabled styling. */
	bool enabled = true;
};

/** How ComboBox resolves its option popup width. */
enum class ComboBoxPopupWidthPolicy : uint8_t {
	/** Match the measured trigger width. */
	MatchTrigger,
	/** Fit option content while keeping at least the trigger width. */
	ContentAtLeastTrigger,
	/** Use popupWidth, falling back to the measured trigger width. */
	Fixed,
};

/**
 * Per-invocation ComboBox configuration. Borrowed models and text must remain valid through the
 * frame; empty visual overrides inherit the active FSELTheme.
 */
struct ComboBoxParameters {
	/** Optional notification after the bound value changes. */
	ActionCall onChanged{};
	/** Optional notification after the option popup opens. */
	ActionCall onOpened{};
	/** Optional notification after the option popup closes. */
	ActionCall onClosed{};

	/** Disclosure artwork when closed; the cached standard icon is used when omitted. */
	std::optional<TextureRef> closedIcon = std::nullopt;
	/** Disclosure artwork when open; the cached standard icon is used when omitted. */
	std::optional<TextureRef> openIcon = std::nullopt;
	/** Borrowed option array; keep the array and its text storage valid through the frame. */
	std::span<const ComboBoxOption> options{};
	/** Borrowed UTF-8 text shown when no value is displayed. */
	std::string_view placeholder = "Select...";
	/** Borrowed UTF-8 placeholder for the optional search editor. */
	std::string_view popupSearchPlaceholder = "Filter options...";
	/** Borrowed authoritative selection. Null disables the control. */
	uint64_t* selectedValue = nullptr;
	/** Optional borrowed open state. Null selects element-owned state; keep the mode stable per element ID. */
	bool* open = nullptr;
	/** Optional popup-local search model. When present, a fixed search header is
	 * rendered above the independently scrollable option viewport. Filtering is
	 * owned by the caller so catalogue-specific matching remains possible. */
	std::string* popupSearchValue = nullptr;
	/** Clay pointer capture rule for popup content. */
	Clay_PointerCaptureMode pointerCaptureMode = CLAY_POINTER_CAPTURE_MODE_CAPTURE;

	/** Clay width and height sizing rules for the root element. Empty inherits the active theme. */
	std::optional<Clay_Sizing> sizing = std::nullopt;

	/** Popup attachment points and offset relative to the trigger. */
	PopupPlacement placement = {
		.anchorPoint = PopupAttachmentPoint::BottomLeft,
		.popupPoint = PopupAttachmentPoint::TopLeft,
		.offset = Clay_Vector2{0.0f, 4.0f},
	};
	/** Optional fixed search header height in layout pixels. */
	std::optional<float> popupSearchHeight = std::nullopt;
	/** Explicit popup width in layout pixels; interpreted by popupWidthPolicy. */
	std::optional<float> popupWidth = std::nullopt;
	/** Maximum popup viewport height in layout pixels. Empty inherits the active theme. */
	std::optional<float> popupMaxHeight = std::nullopt;
	/** Option row height in layout pixels. Empty inherits the active theme. */
	std::optional<float> optionHeight = std::nullopt;
	/** Rendered icon side length in layout pixels. Empty inherits the active theme. */
	std::optional<float> iconSize = std::nullopt;
	/** Registered font family used for field or label text. Empty inherits the active theme. */
	std::optional<FontFamilyId> fontFamily = std::nullopt;
	/** Font weight requested for text, such as 400 for normal. Empty inherits the active theme. */
	std::optional<uint32_t> fontWeight = std::nullopt;
	/** Rule for resolving popup width from trigger, content and popupWidth. */
	ComboBoxPopupWidthPolicy popupWidthPolicy = ComboBoxPopupWidthPolicy::MatchTrigger;
	/** Popup stacking category. */
	PopupLayer layer = PopupLayer::CasualPopup;
	/** Dismissal and anchor-blocking behavior for outside pointer presses. */
	PopupOutsidePressPolicy outsidePress =
		PopupOutsidePressPolicy::DismissAndBlockAnchor;
	/** Text font size in layout pixels. Empty inherits the active theme. */
	std::optional<uint16_t> fontSize = std::nullopt;
	/** Viewport edge correction policy passed to PopupManager. */
	PopupOverflowPolicy overflow{};
	/** Font style requested for text. Empty inherits the active theme. */
	std::optional<FontStyle> fontStyle = std::nullopt;
	/** Cursor requested while this enabled control is hovered. Empty inherits the active theme. */
	std::optional<CursorType> cursor = std::nullopt;
	/** Priority used to arbitrate this control's cursor request. Empty inherits the active theme. */
	std::optional<uint8_t> cursorPriority = std::nullopt;
	/** Alignment of the trigger label; popup options retain their normal alignment. */
	Clay_TextAlignment labelAlignment = CLAY_TEXT_ALIGN_LEFT;
	/** Allow interaction; false also selects disabled styling. */
	bool enabled = true;
	/** Draw the popup scroll indicator when its content overflows. */
	bool showScrollIndicator = true;
};

/** Per-instance interaction state maintained by the element and retained by ElementManager. */
struct ComboBoxState {
	/** Retained popup visibility used when no application open model is supplied. */
	bool open = false;
	/** Whether an enabled trigger press is waiting for release. */
	bool triggerArmed = false;
	/** Whether mouse-up was observed for the armed trigger. */
	bool triggerMouseUpObserved = false;
	/** Whether an option press is waiting for release. */
	bool optionArmed = false;
	/** Whether mouse-up was observed for the armed option. */
	bool optionMouseUpObserved = false;
	/** Stable value of the option captured by the current press. */
	uint64_t armedOptionValue = 0;
};

/** App-wide borrowed icon references resolved once for this element definition. */
struct ComboBoxResources {
	/** Borrowed cached open icon texture; empty if the semantic key was unavailable. */
	TextureRef openIcon{};
	/** Borrowed cached closed icon texture; empty if the semantic key was unavailable. */
	TextureRef closedIcon{};

	/** Construct empty icon references. */
	ComboBoxResources() = default;
	/**
	 * Resolve available standard icons from the app; missing icons remain empty. @param app Owning
	 * application whose icon resources outlive these references.
	 */
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
	/** Per-invocation configuration copied into ElementBuilder. */
	using Parameters = ComboBoxParameters;
	/** Per-instance state owned and retained by ElementManager. */
	using State = ComboBoxState;
	/** Immutable app-wide resources shared by this element definition. */
	using Resources = ComboBoxResources;
	/** Frame-scoped context supplied to the element build hooks. */
	using BuildContext = ElementBuildContext<ComboBox>;
	/** Frame-scoped context supplied to pointer and logic hooks. */
	using InteractionContext = ElementInteractionContext<ComboBox>;

	/** Stable definition identity used for registration, state and semantic parts. */
	static constexpr FlowDefinitionID definitionId = DefinitionID("FSEL.combo-box");
	/** Human-readable element name used by development tools. */
	static constexpr std::string_view debugName = "FSEL ComboBox";

	/** Definition-time semantic parts used to address child surfaces and interaction targets. */
	struct Parts {
		/** Semantic identity of the popup part relative to its owner instance. */
		static constexpr FlowElementPart popup = Part("popup");
	};

	/**
	 * ElementBuilder hover hook; requests the enabled control cursor. @param context Borrowed
	 * context valid for this callback only.
	 */
	static void onHovered(InteractionContext& context) {
		if (!isEnabled(context.params)) return;
		const auto& theme = context.uiManager.theme<FSELTheme>().comboBoxTheme;
		context.uiManager.requestCursor(
			context.params.cursor.value_or(theme.cursor),
			context.params.cursorPriority.value_or(theme.cursorPriority));
	}

	/**
	 * ElementBuilder press hook; begins the enabled control interaction. @param context Borrowed
	 * context valid for this callback only.
	 */
	static void onPressed(InteractionContext& context) {
		if (!isEnabled(context.params)) {
			clearTrigger(context.state());
			return;
		}
		context.state().triggerArmed = true;
		context.state().triggerMouseUpObserved = false;
	}

	/**
	 * ElementBuilder release hook; completes or cancels the pending interaction. @param context
	 * Borrowed context valid for this callback only.
	 */
	static void onReleased(InteractionContext& context) {
		if (!context.state().triggerArmed) return;
		clearTrigger(context.state());
		if (isEnabled(context.params)) {
			setOpen(context, !resolvedOpen(context));
		}
	}

	/**
	 * ElementBuilder logic hook; reconciles retained interaction state with the current input.
	 * @param context Borrowed context valid for this callback only.
	 */
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

	/**
	 * ElementBuilder draw hook; emits this control and its children for the active frame. @param
	 * context Borrowed context valid for this callback only.
	 */
	static void buildElement(BuildContext& context) {
		const auto& theme = context.uiManager.theme<FSELTheme>().comboBoxTheme;
		const bool enabled = isEnabled(context.params);
		const bool open = context.params.open ? *context.params.open : context.state().open;
		const ComboBoxOption* selected = selectedOption(context.params);
		const FSELComboBoxStateTheme appearance = resolveTriggerAppearance(context, enabled, open);

		Clay_ElementDeclaration trigger{};
		trigger.layout.sizing = context.params.sizing.value_or(Clay_Sizing{
			.width = CLAY_SIZING_FIXED(theme.width),
			.height = CLAY_SIZING_FIXED(theme.height),
		});
		const auto retained = Clay_GetElementData(context.clayID());
		const auto content_layout = detail::combo_box::fit_content(
			detail::combo_box::resolved_extent(trigger.layout.sizing.width,
											   retained.found ? retained.boundingBox.width : 0.0f,
											   theme.width),
			detail::combo_box::resolved_extent(trigger.layout.sizing.height,
											   retained.found ? retained.boundingBox.height : 0.0f,
											   theme.height),
			theme.triggerPadding, context.params.iconSize.value_or(theme.iconSize),
			theme.contentGap, selected && selected->icon.handle, true,
			context.params.fontSize.value_or(theme.fontSize));
		trigger.layout.padding = content_layout.padding;
		trigger.layout.childGap = content_layout.gap;
		trigger.layout.childAlignment = {
			.x = CLAY_ALIGN_X_LEFT,
			.y = CLAY_ALIGN_Y_CENTER,
		};
		trigger.layout.layoutDirection = CLAY_LEFT_TO_RIGHT;
		trigger.backgroundColor = appearance.backgroundColor;
		trigger.cornerRadius = theme.cornerRadius;
		trigger.border = {.color = appearance.borderColor, .width = theme.borderWidth};
		trigger.clip = {
			.horizontal = true,
			.vertical = true,
			.scrollInputDisabled = true,
		};

		Clay_TextElementConfig textConfig =
			makeTextConfig(context, selected ? appearance.textColor : theme.placeholderColor);
		textConfig.fontSize = static_cast<uint16_t>(
			std::max(1.0f, std::min<float>(textConfig.fontSize, content_layout.content_height)));
		textConfig.textAlignment = context.params.labelAlignment;
		CLAY(context.clayID(), trigger) {
			if (selected && content_layout.show_leading_icon) {
				drawIcon(context, context.clayID("selected-icon"), selected->icon,
						 appearance.iconColor, content_layout.icon_size);
			}
			Clay_ElementDeclaration labelRoot{};
			labelRoot.layout.sizing = {
				.width = content_layout.label_width > 0.0f
					? CLAY_SIZING_GROW(0, content_layout.label_width) : CLAY_SIZING_FIXED(0),
				.height = CLAY_SIZING_FIT(0),
			};
			labelRoot.layout.childAlignment.x =
				context.params.labelAlignment == CLAY_TEXT_ALIGN_CENTER	 ? CLAY_ALIGN_X_CENTER
				: context.params.labelAlignment == CLAY_TEXT_ALIGN_RIGHT ? CLAY_ALIGN_X_RIGHT
																		 : CLAY_ALIGN_X_LEFT;
			labelRoot.clip = {
				.horizontal = true,
				.vertical = false,
				.scrollInputDisabled = true,
			};
			CLAY(context.clayID("label"), labelRoot) {
				CLAY_TEXT(context.uiManager.toClayString(selected ? selected->text
																  : context.params.placeholder),
						  CLAY_TEXT_CONFIG(textConfig));
			}
			drawDisclosure(context, open, appearance.iconColor, content_layout.icon_size);
		}

		if (open)
			drawPopup(context, theme);
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

	static void drawDisclosure(BuildContext& context, bool open, Clay_Color color, float iconSize) {
		if (iconSize <= 0.0f)
			return;
		Clay_ElementDeclaration slot{};
		slot.layout.sizing = {
			.width = CLAY_SIZING_FIXED(iconSize),
			.height = CLAY_SIZING_GROW(0),
		};
		slot.layout.childAlignment = {
			.x = CLAY_ALIGN_X_CENTER,
			.y = CLAY_ALIGN_Y_CENTER,
		};
		CLAY(context.clayID("disclosure-slot"), slot) {
			const TextureRef icon =
				open ? context.params.openIcon.value_or(context.resources().openIcon)
					 : context.params.closedIcon.value_or(context.resources().closedIcon);
			if (icon.handle) {
				drawIcon(context, context.clayID("disclosure-icon"), icon, color, iconSize);
			} else {
				Clay_TextElementConfig fallback = makeTextConfig(context, color);
				fallback.fontSize = static_cast<uint16_t>(
					std::max(1.0f, std::min<float>(fallback.fontSize, iconSize)));
				fallback.textAlignment = CLAY_TEXT_ALIGN_CENTER;
				CLAY_TEXT(context.uiManager.toClayString(open ? "^" : "v"),
						  CLAY_TEXT_CONFIG(fallback));
			}
		}
	}

	static void drawPopup(BuildContext& context, const FSELComboBoxTheme& theme) {
		const float optionHeight =
			std::max(context.params.optionHeight.value_or(theme.optionHeight), 1.0f);
		const auto& surface = context.uiManager.theme<FSELTheme>().popupSurfaceTheme;
		const float minimumPopupHeight =
			optionHeight + static_cast<float>(surface.padding.top + surface.padding.bottom);
		const float maximumHeight = std::max(
			context.params.popupMaxHeight.value_or(theme.popupMaxHeight), minimumPopupHeight);
		const float contentHeight =
			context.params.options.empty()
				? optionHeight
				: optionHeight * static_cast<float>(context.params.options.size()) +
					  static_cast<float>(theme.optionGap) *
						  static_cast<float>(context.params.options.size() - 1u);
		const float searchHeight = context.params.popupSearchValue
			? std::max(context.params.popupSearchHeight.value_or(30.0f), 1.0f) : 0.0f;
		const float searchGap = context.params.popupSearchValue
			? static_cast<float>(theme.scrollGap) : 0.0f;
		const float popupHeight = std::min(
			contentHeight + static_cast<float>(surface.padding.top + surface.padding.bottom),
			maximumHeight) + searchHeight + searchGap;

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
		const PopupFrame frame = context.uiManager.popups().request(popupId, request).value_or(PopupFrame{});
		if (!frame.visible && !frame.measureOnly) return;

		Clay_ElementDeclaration popup{};
		popup.layout.sizing = {
			.width = popupWidthSizing,
			.height = CLAY_SIZING_FIXED(popupHeight),
		};
		popup.layout.padding = surface.padding;
		popup.layout.childGap = context.params.popupSearchValue ? theme.scrollGap : 0;
		popup.layout.childAlignment = {.x = CLAY_ALIGN_X_LEFT, .y = CLAY_ALIGN_Y_TOP};
		popup.layout.layoutDirection = CLAY_TOP_TO_BOTTOM;
		popup.backgroundColor = surface.backgroundColor;
		popup.cornerRadius = surface.cornerRadius;
		popup.border = {.color = surface.borderColor, .width = surface.borderWidth};
		popup.floating = frame.floating;

		const Clay_ElementId scrollId = context.clayID("scroll-viewport");
		const Clay_ScrollContainerData scroll = Clay_GetScrollContainerData(scrollId);
		const float trackHeight = std::max(
			popupHeight - searchHeight - searchGap -
				static_cast<float>(surface.padding.top + surface.padding.bottom),
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
			if (context.params.popupSearchValue) {
				Clay_ElementDeclaration searchSlot{};
				searchSlot.layout.sizing = {
					.width = CLAY_SIZING_GROW(0), .height = CLAY_SIZING_FIXED(searchHeight)};
				searchSlot.layout.childAlignment = {
					.x = CLAY_ALIGN_X_LEFT, .y = CLAY_ALIGN_Y_CENTER};
				searchSlot.clip = {.horizontal = true, .vertical = true};
				CLAY(context.clayID("popup-search-slot"), searchSlot) {
					TextInputParameters search{};
					search.value = context.params.popupSearchValue;
					search.syncPolicy = TextFieldSyncPolicy::Live;
					search.placeholder = context.params.popupSearchPlaceholder;
					search.enabled = context.params.enabled;
					search.readOnly = !context.params.enabled;
					search.maxBytes = 96u;
					search.sizing = Clay_Sizing{
						.width = CLAY_SIZING_GROW(0), .height = CLAY_SIZING_GROW(0)};
					// Match the working 30 px inspector search fields: the default 8 px
					// vertical padding leaves too little height for a managed text line.
					search.padding = Clay_Padding{8, 8, 5, 5};
					search.fontSize = context.params.fontSize.value_or(theme.fontSize);
					search.floatingZIndex = frame.floating.zIndex;
					context.uiManager.createElement(kTextInput, "popup-search")
						.setParameters(std::move(search)).setDevInternalCapture(true).draw();
				}
			}
			Clay_ElementDeclaration contentRow{};
			contentRow.layout.sizing = {
				.width = CLAY_SIZING_GROW(0), .height = CLAY_SIZING_GROW(0)};
			contentRow.layout.layoutDirection = CLAY_LEFT_TO_RIGHT;
			contentRow.layout.childGap = theme.scrollGap;
			CLAY(context.clayID("popup-content"), contentRow) {
			Clay_ElementDeclaration viewport{};
			viewport.layout.sizing = {
				.width = CLAY_SIZING_GROW(0),
				.height = CLAY_SIZING_GROW(0),
			};
			viewport.layout.layoutDirection = CLAY_TOP_TO_BOTTOM;
			viewport.clip = {
				.vertical = true,
				.childOffset = scroll.found && scroll.scrollPosition
					? *scroll.scrollPosition : Clay_Vector2{},
			};
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
		} else if (context.params.selectedValue && *context.params.selectedValue == option.value) {
			appearance = theme.optionSelected;
		}

		Clay_ElementDeclaration row{};
		row.layout.sizing = {
			.width = CLAY_SIZING_GROW(0),
			.height = CLAY_SIZING_FIXED(height),
		};
		const auto retained = Clay_GetElementData(id);
		const auto content_layout = detail::combo_box::fit_content(
			retained.found && retained.boundingBox.width > 0.0f
				? retained.boundingBox.width
				: context.params.popupWidth.value_or(theme.width),
			height, theme.optionPadding, context.params.iconSize.value_or(theme.iconSize),
			theme.contentGap, static_cast<bool>(option.icon.handle), false,
			context.params.fontSize.value_or(theme.fontSize));
		row.layout.padding = content_layout.padding;
		row.layout.childGap = content_layout.gap;
		row.layout.childAlignment = {.x = CLAY_ALIGN_X_LEFT, .y = CLAY_ALIGN_Y_CENTER};
		row.layout.layoutDirection = CLAY_LEFT_TO_RIGHT;
		row.backgroundColor = appearance.backgroundColor;
		row.cornerRadius = theme.cornerRadius;
		row.clip = {.horizontal = true, .vertical = true, .scrollInputDisabled = true};
		CLAY(id, row) {
			if (content_layout.show_leading_icon) {
				drawIcon(context, context.clayID(Keyed("option-icon", option.value)), option.icon,
						 appearance.iconColor, content_layout.icon_size);
			}
			Clay_ElementDeclaration labelSlot{};
			labelSlot.layout.sizing = {
				.width = content_layout.label_width > 0.0f
					? CLAY_SIZING_GROW(0, content_layout.label_width) : CLAY_SIZING_FIXED(0),
				.height = CLAY_SIZING_FIT(0)};
			labelSlot.clip = {.horizontal = true, .vertical = true, .scrollInputDisabled = true};
			CLAY(context.clayID(Keyed("option-label", option.value)), labelSlot) {
				auto text_config = makeTextConfig(context, appearance.textColor);
				text_config.fontSize = static_cast<uint16_t>(std::max(
					1.0f, std::min<float>(text_config.fontSize, content_layout.content_height)));
				CLAY_TEXT(context.uiManager.toClayString(option.text),
						  CLAY_TEXT_CONFIG(text_config));
			}
		}
	}

	static void drawScrollIndicator(BuildContext& context,
									const detail::scroll_indicator::ThumbGeometry& thumb,
									float trackHeight, const FSELComboBoxTheme& theme) {
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

/**
 * Reusable empty definition tag passed to UiManager::createElement(); no per-instance state is
 * stored in this object.
 */
inline constexpr ComboBox kComboBox{};
static_assert(validate_flow_element<ComboBox>());
static_assert(DrawableFlowElement<ComboBox>);
static_assert(!ConstructibleFlowElement<ComboBox>);

} // namespace FlowUi::FSEL
