#pragma once
#include "FlowUi/BuildConfig.hpp"
#if FLOW_UI_DEV_MODE
#include "FSEL/RadioChoice.hpp"
#include "devSystems/devInterface/Changes/DevChangesModel.hpp"
#include "devSystems/devInterface/Changes/DevChangesViewportState.hpp"
#include "devSystems/devInterface/Diagnostics/DevDiagnosticsUi.hpp"
#include "devSystems/devInterface/Permanents/Elements/DevSelectorStyle.hpp"
#include "internal/Text/CurrentTextShaper.hpp"
#include "managers/FontManager.hpp"

namespace FlowUi::devSystems::interface_elements::changes_ui {
using diagnostics_ui::card;
using diagnostics_ui::column;
using diagnostics_ui::kControl;
using diagnostics_ui::row;
using Icon = diagnostics_icons::Icon;

/** Bind submitted controls to a panel viewport while its content is being built. */
class FocusViewportScope {
public:
	FocusViewportScope(DevChangesState& changes, Clay_ElementId viewport) noexcept
		: changes_(changes), previous_(changes.registration_viewport) {
		changes_.registration_viewport = viewport;
	}
	~FocusViewportScope() { changes_.registration_viewport = previous_; }
	FocusViewportScope(const FocusViewportScope&) = delete;
	FocusViewportScope& operator=(const FocusViewportScope&) = delete;

private:
	DevChangesState& changes_;
	Clay_ElementId previous_{};
};

/** Register a submitted, enabled control without reading optional captured developer trees. */
template <class Context>
void register_focus_target(Context& context, DevChangesState& changes, Clay_ElementId identity,
						   bool enabled = true, FlowElementID text_input = {},
						   uint64_t* radio_selection = nullptr, uint64_t radio_value = 0,
						   Clay_ElementId viewport = {}) {
	if (!enabled || !identity.id)
		return;
	const auto found = std::ranges::find(changes.focus_targets, identity.id,
										 [](const auto& target) { return target.identity.id; });
	const DevChangesFocusTarget target{identity,
									   viewport.id ? viewport : changes.registration_viewport,
									   text_input, radio_selection, radio_value};
	if (found == changes.focus_targets.end())
		changes.focus_targets.emplace_back(target);
	else
		*found = target;
	if (context.uiManager.getPreviousFramesInteraction().isPressed(identity)) {
		context.uiManager.shortcuts().setFocusedElement(identity);
		if (!text_input.value && context.uiManager.inputFields().hasPrimaryFieldFocus())
			context.uiManager.inputFields().requestCaret(FlowElementID{},
														 CaretRequestKind::ClearAll);
	}
}

/** Register a control whose enclosing Changes panel supplies its shared session. */
template <class Context>
void register_focus_target(Context& context, Clay_ElementId identity, bool enabled = true,
						   FlowElementID text_input = {}, uint64_t* radio_selection = nullptr,
						   uint64_t radio_value = 0, Clay_ElementId viewport = {}) {
	if (context.params.interfaceState)
		register_focus_target(context, context.params.interfaceState->changes, identity, enabled,
							  text_input, radio_selection, radio_value, viewport);
}

/** Traverse the preceding submitted order, then collect this frame's live targets. */
template <class Context>
void begin_focus_frame(Context& context, DevChangesState& changes) {
	const auto& input = context.uiManager.getCurrentFrameInput();
	const auto& previous = context.uiManager.getPreviousFrameInput();
	const auto focused = context.uiManager.shortcuts().focusedElement().id;
	const auto found = std::ranges::find(changes.focus_targets, focused,
										 [](const auto& target) { return target.identity.id; });
	if (input.windowFocused && !changes.focus_targets.empty() && input.keyDown[GLFW_KEY_TAB] &&
		!previous.keyDown[GLFW_KEY_TAB]) {
		const bool reverse =
			input.keyDown[GLFW_KEY_LEFT_SHIFT] || input.keyDown[GLFW_KEY_RIGHT_SHIFT];
		const auto count = changes.focus_targets.size();
		const auto position = found == changes.focus_targets.end()
								  ? (reverse ? 0 : count - 1)
								  : static_cast<std::size_t>(found - changes.focus_targets.begin());
		const auto& target =
			changes.focus_targets[(position + count + (reverse ? count - 1 : 1)) % count];
		context.uiManager.inputFields().requestCaret(FlowElementID{}, CaretRequestKind::ClearAll);
		context.uiManager.shortcuts().setFocusedElement(target.identity);
		if (target.text_input.value)
			context.uiManager.inputFields().requestCaret(target.text_input,
														 CaretRequestKind::SetPrimary);
		if (target.viewport.id) {
			const auto bounds = Clay_GetElementData(target.identity);
			const auto viewport = Clay_GetElementData(target.viewport);
			const auto scroll = Clay_GetScrollContainerData(target.viewport);
			if (bounds.found && viewport.found && scroll.found && scroll.scrollPosition) {
				if (bounds.boundingBox.y < viewport.boundingBox.y)
					scroll.scrollPosition->y += viewport.boundingBox.y - bounds.boundingBox.y;
				else if (bounds.boundingBox.y + bounds.boundingBox.height >
						 viewport.boundingBox.y + viewport.boundingBox.height)
					scroll.scrollPosition->y -= bounds.boundingBox.y + bounds.boundingBox.height -
												viewport.boundingBox.y -
												viewport.boundingBox.height;
			}
		}
	} else if (input.windowFocused && found != changes.focus_targets.end() &&
			   found->radio_selection && !context.uiManager.inputFields().hasPrimaryFieldFocus() &&
			   ((input.keyDown[GLFW_KEY_ENTER] && !previous.keyDown[GLFW_KEY_ENTER]) ||
				(input.keyDown[GLFW_KEY_SPACE] && !previous.keyDown[GLFW_KEY_SPACE]))) {
		*found->radio_selection = found->radio_value;
		changes.model_dirty = true;
	}
	changes.focus_targets.clear();
	if (changes.focus_targets.capacity() < 256)
		changes.focus_targets.reserve(256);
	changes.registration_viewport = {};
}
/** Output operation identity is shared by the selector, queue and context panel. */
[[nodiscard]] inline Clay_Color operation_color(tooling::DevChangesOperation operation) noexcept {
	switch (operation) {
	case tooling::DevChangesOperation::Add:
		return interface_theme::kAccentSeaGlass;
	case tooling::DevChangesOperation::Update:
		return interface_theme::kAccentSignalBlue;
	case tooling::DevChangesOperation::Remove:
		return interface_theme::kStatusAmber;
	default:
		return interface_theme::kTextSecondary;
	}
}
/** Friendly operation names never replace the independent validity or decision dimensions. */
[[nodiscard]] inline std::string_view
operation_label(tooling::DevChangesOperation operation) noexcept {
	switch (operation) {
	case tooling::DevChangesOperation::Add:
		return "New";
	case tooling::DevChangesOperation::Update:
		return "Update";
	case tooling::DevChangesOperation::Remove:
		return "Removal";
	case tooling::DevChangesOperation::Keep:
		return "Saved";
	default:
		return "Unchanged";
	}
}
enum Action : uint64_t {
	Select = 1,
	Toggle,
	Refresh,
	Bake,
	Include,
	Omit,
	Keep,
	Remove,
	Undo,
	Redo,
	Edit,
	Overview,
	Section,
	Review,
	Issues,
	Acknowledge,
	Back,
	ApplyProposal,
	ApplySaved,
	DiscardLive,
	ResetAuthored,
	UndoLive,
	RedoLive,
	StagePolicy,
	PreviousPage,
	NextPage,
	CloseReview,
	ClearSelection,
	RevealSelection,
	NextIssue,
	GroupDecisions,
	CloseGroupDecisions,
	OpenPolicy,
	ClosePolicy,
	ToggleRawManifest,
	ToggleOutputTechnical,
	DismissMessage
};

/** UI callbacks retain only owned identities; commands are deferred to service_changes. */
inline void activate(void* target, uint64_t action, uint64_t value) {
	auto& state = *static_cast<DevChangesState*>(target);
	if (action == Select || action == Toggle) {
		const auto found = std::ranges::find(state.nodes, value, &DevChangesNode::identity);
		if (found != state.nodes.end()) {
			state.focused_identity = value;
			state.keyboard_active = true;
			if (action == Toggle && !found->leaf)
				found->expanded = !found->expanded;
			if (action == Select) {
				changes_select_key(state, found->key);
			}
			state.model_dirty = true;
		}
	}
	if (action == Refresh)
		state.pending = DevChangesCommand::Refresh;
	if (action == Bake)
		state.pending = DevChangesCommand::Bake;
	if (action == Include)
		queue_changes_decision(state, tooling::DevChangesDecision::IncludeProposal);
	if (action == Omit)
		queue_changes_decision(state, tooling::DevChangesDecision::OmitProposal);
	if (action == Keep)
		queue_changes_decision(state, tooling::DevChangesDecision::RetainDisk);
	if (action == Remove)
		queue_changes_decision(state, tooling::DevChangesDecision::Remove);
	if (action == Undo)
		state.pending = DevChangesCommand::Undo;
	if (action == Redo)
		state.pending = DevChangesCommand::Redo;
	if (action == Edit)
		state.pending = DevChangesCommand::Edit;
	if (action == Back)
		state.pending = DevChangesCommand::Back;
	if (action == ApplyProposal)
		state.pending = DevChangesCommand::ApplyProposal;
	if (action == ApplySaved)
		state.pending = DevChangesCommand::ApplySaved;
	if (action == DiscardLive)
		state.pending = DevChangesCommand::DiscardLive;
	if (action == ResetAuthored)
		state.pending = DevChangesCommand::ResetAuthored;
	if (action == UndoLive)
		state.pending = DevChangesCommand::UndoLive;
	if (action == RedoLive)
		state.pending = DevChangesCommand::RedoLive;
	if (action == StagePolicy) {
		if (value == 1 && !state.policy_growth_valid) {
			state.message = "Complete a valid growth factor draft before staging.";
			state.message_is_error = true;
			return;
		}
		state.pending_policy = value;
		state.pending = DevChangesCommand::StagePolicy;
	}
	if (action == PreviousPage && state.workbench_page) {
		--state.workbench_page;
		state.workbench_page_changed = true;
	}
	if (action == NextPage) {
		++state.workbench_page;
		state.workbench_page_changed = true;
	}
	if (action == Overview) {
		state.selected_key.clear();
		state.queue_scope_key.clear();
		state.selected_outside_filter = false;
		state.workbench_page = 0;
		state.workbench_page_changed = true;
		state.group_decisions_open = false;
	}
	if (action == Section && value < state.inspector_expanded.size())
		state.inspector_expanded[value] = !state.inspector_expanded[value];
	if (action == Review) {
		state.view = 1;
		state.group_decisions_open = false;
	}
	if (action == CloseReview)
		state.view = 0;
	if (action == Issues) {
		state.status = 3;
		state.domain = 0;
		state.search.clear();
		state.model_dirty = true;
	}
	if (action == Acknowledge) {
		state.acknowledge_incomplete = !state.acknowledge_incomplete;
		state.pending = DevChangesCommand::Refresh;
	}
	if (action == ClearSelection) {
		state.selected_key.clear();
		state.selected_outside_filter = false;
	}
	if (action == RevealSelection) {
		state.search.clear();
		state.status = state.domain = 0;
		std::string ancestor_key = state.selected_key;
		while (!ancestor_key.empty()) {
			const auto ancestor =
				std::ranges::find(state.nodes, ancestor_key, &DevChangesNode::key);
			if (ancestor == state.nodes.end())
				break;
			if (!ancestor->leaf)
				ancestor->expanded = true;
			ancestor_key = ancestor->parent;
		}
		state.reveal_selection_pending = true;
		state.model_dirty = true;
	}
	if (action == NextIssue && !state.review.rows.empty()) {
		const auto selected = std::ranges::find(state.review.rows, state.selected_key,
												&tooling::DevChangesReviewRow::key);
		const auto start = selected == state.review.rows.end()
							   ? 0
							   : static_cast<std::size_t>(selected - state.review.rows.begin()) + 1;
		for (std::size_t offset = 0; offset < state.review.rows.size(); ++offset) {
			const auto& candidate = state.review.rows[(start + offset) % state.review.rows.size()];
			if (candidate.blocking) {
				changes_select_key(state, candidate.key);
				state.view = 0;
				break;
			}
		}
	}
	if (action == GroupDecisions)
		state.group_decisions_open = !state.group_decisions_open;
	if (action == CloseGroupDecisions)
		state.group_decisions_open = false;
	if (action == OpenPolicy) {
		state.view = 2;
		state.group_decisions_open = false;
	}
	if (action == ClosePolicy)
		state.view = 0;
	if (action == ToggleRawManifest)
		state.raw_manifest_open = !state.raw_manifest_open;
	if (action == ToggleOutputTechnical)
		state.output_technical_open = !state.output_technical_open;
	if (action == DismissMessage) {
		state.message.clear();
		state.message_is_error = false;
	}
}

/** Resolve the same default face used by the window's Clay text configuration. */
template <class Context>
[[nodiscard]] const Font::FontFaceData* text_face(Context& context) {
	const Font::FontFaceData* face =
		context.params.app ? context.params.app->fonts().getFontById(0) : nullptr;
	if (!face && context.params.app)
		context.params.app->fonts().visitDevFonts(
			&face, [](void* target, const FontManager::DevFontView& font) {
				*static_cast<const Font::FontFaceData**>(target) = font.face;
				return false;
			});
	return face;
}

/** Measure with the application's actual face and resolved line height, preserving UTF-8. */
template <class Context>
[[nodiscard]] std::string ellipsize(Context& context, std::string_view label, float width,
									uint16_t size = 12) {
	if (width <= 0)
		return {};
	const auto* face = text_face(context);
	if (!face) {
		const auto limit = static_cast<std::size_t>(width / (size * 1.1f));
		return label.size() <= limit ? std::string(label)
			   : limit > 3			 ? std::string(label.substr(0, limit - 3)) + "..."
									 : std::string{};
	}
	const ::FlowUi::detail::text::CurrentTextShaper shaper{};
	auto layout = shaper.shape({.text = label, .pointsToPixelsScale = 1, .fontSize = size}, *face);
	Clay_TextElementConfig config{};
	config.fontSize = size;
	const float height =
		context.uiManager.inputContentElement(config).layout.sizing.height.size.minMax.min;
	const float scale = layout.lineHeight > 0 ? height / layout.lineHeight : 1;
	if (layout.measuredWidth * scale <= width - 2)
		return std::string(label);
	const auto suffix =
		shaper.shape({.text = "...", .pointsToPixelsScale = 1, .fontSize = size}, *face);
	const float available = width - suffix.measuredWidth * scale - 3;
	std::size_t prefix = 0;
	for (const auto& cluster : layout.clusters) {
		if (cluster.xEnd * scale > available)
			break;
		prefix = cluster.endByte;
	}
	return prefix ? std::string(label.substr(0, prefix)) + "..." : std::string{};
}

/** Preserve full technical values, splitting oversized tokens at measured UTF-8 boundaries. */
template <class Context>
void text(Context& context, std::string_view label,
		  Clay_Color color = interface_theme::kTextSecondary, uint16_t size = 12) {
	std::string display;
	display.reserve(label.size() + label.size() / 24);
	const auto parent = Clay_GetElementData(Clay_ElementId{.id = Clay_GetOpenElementId()});
	const float available = std::max(
		1.0f, parent.found && parent.boundingBox.width > 30 ? parent.boundingBox.width - 30 : 180);
	Clay_TextElementConfig config{};
	config.fontSize = size;
	const float resolved_height =
		context.uiManager.inputContentElement(config).layout.sizing.height.size.minMax.min;
	const auto* face = text_face(context);
	const ::FlowUi::detail::text::CurrentTextShaper shaper{};
	float scale = 1;
	if (face) {
		const auto metrics =
			shaper.shape({.text = "M", .pointsToPixelsScale = 1, .fontSize = size}, *face);
		if (metrics.lineHeight > 0)
			scale = resolved_height / metrics.lineHeight;
	}
	const auto whitespace = [](char character) {
		return character == ' ' || character == '\n' || character == '\t' || character == '\r';
	};
	std::size_t offset = 0;
	while (offset < label.size()) {
		if (whitespace(label[offset])) {
			display += label[offset++];
			continue;
		}
		const auto word_end = label.find_first_of(" \n\t\r", offset);
		const auto end = word_end == std::string_view::npos ? label.size() : word_end;
		const auto word = label.substr(offset, end - offset);
		auto layout =
			face ? shaper.shape({.text = word, .pointsToPixelsScale = 1, .fontSize = size}, *face)
				 : ::FlowUi::detail::text::TextLayoutResult{};
		if (face && layout.success && !layout.clusters.empty()) {
			if (layout.measuredWidth * scale <= available)
				display += word;
			else {
				std::size_t first_cluster = 0;
				while (first_cluster < layout.clusters.size()) {
					std::size_t after_cluster = first_cluster + 1;
					const float start_x = layout.clusters[first_cluster].xStart;
					while (after_cluster < layout.clusters.size() &&
						   (layout.clusters[after_cluster].xEnd - start_x) * scale <= available - 2)
						++after_cluster;
					const auto first_byte = layout.clusters[first_cluster].startByte;
					auto segment = word.substr(
						first_byte, layout.clusters[after_cluster - 1].endByte - first_byte);
					while (after_cluster > first_cluster + 1) {
						const auto measured = shaper.shape(
							{.text = segment, .pointsToPixelsScale = 1, .fontSize = size}, *face);
						if (measured.measuredWidth * scale <= available)
							break;
						--after_cluster;
						segment = word.substr(
							first_byte, layout.clusters[after_cluster - 1].endByte - first_byte);
					}
					display += segment;
					first_cluster = after_cluster;
					if (first_cluster < layout.clusters.size())
						display += '\n';
				}
			}
		} else {
			const auto byte_limit = static_cast<std::size_t>(
				std::max(1.0f, available / std::max(1.0f, resolved_height * .5f)));
			std::size_t word_offset = 0, line_bytes = 0;
			while (word_offset < word.size()) {
				auto character_end = word_offset + 1;
				while (character_end < word.size() &&
					   (static_cast<unsigned char>(word[character_end]) & 0xc0u) == 0x80u)
					++character_end;
				const auto character_bytes = character_end - word_offset;
				if (line_bytes && line_bytes + character_bytes > byte_limit) {
					display += '\n';
					line_bytes = 0;
				}
				display += word.substr(word_offset, character_bytes);
				line_bytes += character_bytes;
				word_offset = character_end;
			}
		}
		offset = end;
	}
	diagnostics_ui::text(context, display, color, size);
}
/** Keep explanatory prose on a labeled hover target using the Diagnostics pointer help. */
template <class Context, class Identity>
void hinted_text(Context& context, const Identity& identity, std::string_view label,
				 std::string_view description, Clay_Color color = interface_theme::kTextSecondary,
				 uint16_t size = 12) {
	auto heading = row();
	heading.layout.sizing.height = CLAY_SIZING_FIT(0);
	CLAY(context.clayID(identity), heading) {
		text(context, label, color, size);
		diagnostics_ui::icon_with_identity(context, Keyed("hint-icon", identity.token), Icon::Info,
										   interface_theme::kTextMuted, 12);
		if (context.uiManager.getPreviousFramesInteraction().isHovered(context.clayID(identity)))
			diagnostics_ui::pointer_tooltip(context, Keyed("hint-tooltip", identity.token),
											description);
	}
}
/** Shared release-inside control reuses the established Diagnostics/Memory treatment. */
template <class Context, class Identity>
void button(Context& context, const Identity& identity, std::string label, DevChangesState& state,
			uint64_t action, uint64_t value = 0, bool enabled = true, std::string tooltip = {},
			Icon icon = Icon::None) {
	diagnostics_ui::button_with_identity(context, identity, std::move(label), &state, activate,
										 action, value, enabled, std::move(tooltip), icon);
	const auto element = ::FlowUi::detail::element_id::resolveLocal(
		context.id, diagnostics_ui::Control::definitionId, identity.token);
	register_focus_target(context, state, context.uiManager.toClayEID(element), enabled);
}
/** Shared segmented or full-width selector; filtering never changes candidate scope. */
template <class Context>
void choice(Context& context, IndexedElementName identity, std::string_view label, uint64_t value,
			uint64_t& selected, uint16_t horizontal_padding = 7, bool selection_bar = false) {
	FSEL::RadioChoiceParameters parameters{};
	parameters.choiceValue = value;
	parameters.selectedValue = &selected;
	parameters.style = selector_choice_style();
	parameters.style.padding = Clay_Padding{horizontal_padding, horizontal_padding, 0, 0};
	if (selection_bar)
		parameters.style = selector_bar_style();
	const auto element = ::FlowUi::detail::element_id::resolveLocal(
		context.id, FSEL::RadioChoice::definitionId, identity.token);
	const auto clay_identity = context.uiManager.toClayEID(element);
	if (context.uiManager.shortcuts().focusedElement().id == clay_identity.id) {
		parameters.style.borderWidth =
			selection_bar ? Clay_BorderWidth{3, 1, 1, 1, 0} : Clay_BorderWidth{1, 1, 1, 1, 0};
		parameters.style.idleOverrides.borderColor = interface_theme::kAccentCurrent;
		parameters.style.hoveredOverrides.borderColor = interface_theme::kAccentCurrent;
		parameters.style.selectedOverrides.borderColor = interface_theme::kAccentCurrent;
	}
	context.uiManager.createElement(FSEL::kRadioChoice, identity)
		.setParameters(parameters)
		.setDevInternalCapture(true)
		.construct();
	Clay_TextElementConfig config{};
	config.fontSize = 11;
	config.textColor =
		selected == value ||
				(selection_bar &&
				 context.uiManager.getPreviousFramesInteraction().isHovered(clay_identity))
			? interface_theme::kTextCanvas
			: interface_theme::kTextSecondary;
	config.wrapMode = CLAY_TEXT_WRAP_NONE;
	if (selection_bar) {
		auto caption = column(0);
		caption.layout.sizing.height = CLAY_SIZING_GROW(0);
		caption.layout.childAlignment.y = CLAY_ALIGN_Y_CENTER;
		caption.clip = {.horizontal = true, .scrollInputDisabled = true};
		CLAY(context.clayID(Keyed("choice-label", identity.token)), caption)
		CLAY_TEXT(context.uiManager.toClayString(label), CLAY_TEXT_CONFIG(config));
	} else {
		CLAY_TEXT(context.uiManager.toClayString(label), CLAY_TEXT_CONFIG(config));
	}
	context.uiManager.drawConstructed();
	register_focus_target(context, clay_identity, true, {}, &selected, value);
}
/** Full-width radio row using the Performance selector's colors, stripe and alignment. */
template <class Context>
void bar_choice(Context& context, IndexedElementName identity, std::string_view label,
				uint64_t value, uint64_t& selected) {
	choice(context, identity, label, value, selected, 7, true);
}
/** Three independent Clay scroll containers retain their positions across refreshed plans. */
template <class Context>
[[nodiscard]] Clay_ElementDeclaration viewport(Context& context, LocalElementName identity,
											   float& retained_scroll,
											   ViewportState* restoration = nullptr) noexcept {
	auto result = column(0);
	result.layout.sizing.height = CLAY_SIZING_GROW(0);
	const auto scroll = Clay_GetScrollContainerData(context.clayID(identity));
	if (restoration && !scroll.found) {
		restoration->requested_offset = retained_scroll;
		restoration->settling_frames = retained_scroll < 0 ? 4 : 0;
	}
	if (restoration && restoration->settling_frames) {
		const auto& input = context.uiManager.getCurrentFrameInput();
		const float lower_bound = std::min(0.0f, scroll.scrollContainerDimensions.height -
													 scroll.contentDimensions.height);
		const bool moved_without_clamping =
			scroll.found && scroll.scrollPosition &&
			std::abs(scroll.scrollPosition->y - restoration->requested_offset) > 0.5f &&
			std::abs(scroll.scrollPosition->y - lower_bound) > 0.5f;
		const bool navigating = input.mouseDown[0] || input.scrollY != 0 || input.scrollX != 0 ||
								input.keyDown[GLFW_KEY_TAB] || input.keyDown[GLFW_KEY_UP] ||
								input.keyDown[GLFW_KEY_DOWN] || input.keyDown[GLFW_KEY_PAGE_UP] ||
								input.keyDown[GLFW_KEY_PAGE_DOWN] || input.keyDown[GLFW_KEY_HOME] ||
								input.keyDown[GLFW_KEY_END] || input.keyDown[GLFW_KEY_ENTER] ||
								input.keyDown[GLFW_KEY_SPACE];
		if (navigating || moved_without_clamping)
			restoration->settling_frames = 0;
	}
	if (restoration && restoration->settling_frames) {
		retained_scroll = restoration->requested_offset;
		if (scroll.found && scroll.scrollPosition)
			scroll.scrollPosition->y = retained_scroll;
		--restoration->settling_frames;
	} else if (scroll.found && scroll.scrollPosition)
		retained_scroll = scroll.scrollPosition->y;
	result.clip = {.vertical = true,
				   .childOffset = scroll.found && scroll.scrollPosition
									  ? *scroll.scrollPosition
									  : Clay_Vector2{0, retained_scroll}};
	return result;
}
/** Seed newly recreated Clay scroll state after opening a retained panel viewport. */
template <class Context, class Identity>
void restore_viewport_scroll(Context& context, const Identity& identity, float retained_scroll) {
	const auto scroll = Clay_GetScrollContainerData(context.clayID(identity));
	if (scroll.found && scroll.scrollPosition && scroll.scrollContainerDimensions.height == 0)
		scroll.scrollPosition->y = retained_scroll;
}
[[nodiscard]] inline Clay_Color validity_color(const tooling::DevChangesReviewRow& row) noexcept {
	if (row.blocking)
		return interface_theme::kStatusRed;
	if (row.validity != tooling::DevChangesValidity::Ready)
		return interface_theme::kStatusAmber;
	if (row.decision == tooling::DevChangesDecision::OmitProposal)
		return interface_theme::kTextMuted;
	return interface_theme::kAccentSeaGlass;
}
/** Explicit absence and zero/default semantics remain distinct. */
[[nodiscard]] inline std::string value(std::string_view text_value, bool present) {
	return present ? (text_value.empty() ? "\"\"" : std::string(text_value)) : "Absent";
}
} // namespace FlowUi::devSystems::interface_elements::changes_ui
#endif
