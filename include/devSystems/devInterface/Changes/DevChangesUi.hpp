#pragma once
#include "FlowUi/BuildConfig.hpp"
#if FLOW_UI_DEV_MODE
#include "FSEL/RadioChoice.hpp"
#include "devSystems/devInterface/Changes/DevChangesModel.hpp"
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
	CloseReview
};

/** UI callbacks retain only owned identities; commands are deferred to service_changes. */
inline void activate(void* target, uint64_t action, uint64_t value) {
	auto& state = *static_cast<DevChangesState*>(target);
	if (action == Select || action == Toggle) {
		const auto found = std::ranges::find(state.nodes, value, &DevChangesNode::identity);
		if (found != state.nodes.end()) {
			state.selected_key = found->key;
			state.focused_identity = value;
			state.keyboard_active = true;
			state.workbench_page = 0;
			if (action == Toggle && !found->leaf)
				found->expanded = !found->expanded;
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
		state.selected_outside_filter = false;
	}
	if (action == Section && value < state.inspector_expanded.size())
		state.inspector_expanded[value] = !state.inspector_expanded[value];
	if (action == Review)
		state.review_open = !state.review_open;
	if (action == CloseReview)
		state.review_open = false;
	if (action == Issues) {
		state.status = 3;
		state.domain = 0;
		state.search.clear();
		state.review_open = false;
		state.model_dirty = true;
	}
	if (action == Acknowledge) {
		state.acknowledge_incomplete = !state.acknowledge_incomplete;
		state.pending = DevChangesCommand::Refresh;
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
	const float available = std::max(1.0f, parent.found ? parent.boundingBox.width - 30 : 180);
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
/** Shared release-inside control reuses the established Diagnostics/Memory treatment. */
template <class Context, class Identity>
void button(Context& context, const Identity& identity, std::string label, DevChangesState& state,
			uint64_t action, uint64_t value = 0, bool enabled = true, std::string tooltip = {},
			Icon icon = Icon::None) {
	diagnostics_ui::button_with_identity(context, identity, std::move(label), &state, activate,
										 action, value, enabled, std::move(tooltip), icon);
}
/** Shared centered segmented selector; filtering never changes candidate scope. */
template <class Context>
void choice(Context& context, IndexedElementName identity, std::string_view label, uint64_t value,
			uint64_t& selected, uint16_t horizontal_padding = 7) {
	FSEL::RadioChoiceParameters parameters{};
	parameters.choiceValue = value;
	parameters.selectedValue = &selected;
	parameters.style = selector_choice_style();
	parameters.style.padding = Clay_Padding{horizontal_padding, horizontal_padding, 0, 0};
	context.uiManager.createElement(FSEL::kRadioChoice, identity)
		.setParameters(parameters)
		.setDevInternalCapture(true)
		.construct();
	Clay_TextElementConfig config{};
	config.fontSize = 11;
	config.textColor =
		selected == value ? interface_theme::kTextCanvas : interface_theme::kTextSecondary;
	config.wrapMode = CLAY_TEXT_WRAP_NONE;
	CLAY_TEXT(context.uiManager.toClayString(label), CLAY_TEXT_CONFIG(config));
	context.uiManager.drawConstructed();
}
/** Three independent Clay scroll containers retain their positions across refreshed plans. */
template <class Context>
[[nodiscard]] Clay_ElementDeclaration viewport(Context& context, LocalElementName identity,
											   float& retained_scroll) noexcept {
	auto result = column(0);
	result.layout.sizing.height = CLAY_SIZING_GROW(0);
	const auto scroll = Clay_GetScrollContainerData(context.clayID(identity));
	if (scroll.found && scroll.scrollPosition)
		retained_scroll = scroll.scrollPosition->y;
	result.clip = {.vertical = true,
				   .childOffset = scroll.found && scroll.scrollPosition
									  ? *scroll.scrollPosition
									  : Clay_Vector2{0, retained_scroll}};
	return result;
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
	return present ? (text_value.empty() ? "Unavailable" : std::string(text_value)) : "Absent";
}
} // namespace FlowUi::devSystems::interface_elements::changes_ui
#endif
