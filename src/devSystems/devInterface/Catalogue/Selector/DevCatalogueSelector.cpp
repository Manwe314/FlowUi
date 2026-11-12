#include "devSystems/devInterface/Catalogue/Selector/DevCatalogueSelector.hpp"
#include "FlowUi/Public/input/Key.hpp"

#if FLOW_UI_DEV_MODE
#include "FSEL/RadioChoice.hpp"
#include "FSEL/SplitterHandle.hpp"
#include "FSEL/TextInput.hpp"
#include "devSystems/devInterface/Catalogue/DevCatalogueFocus.hpp"
#include "devSystems/devInterface/Diagnostics/DevDiagnosticsUi.hpp"
#include "devSystems/devInterface/Inspect/Selector/DevInspectSelectorElements.hpp"
#include "devSystems/devInterface/Permanents/Elements/DevSelectorSearch.hpp"
#include "devSystems/devInterface/Permanents/Elements/DevSelectorStyle.hpp"
#include <algorithm>
#include <cctype>
#include <cmath>

namespace FlowUi::devSystems::interface_elements {
namespace {
namespace ui = diagnostics_ui;
using Context = DevCatalogueSelector::BuildContext;
using State = DevCatalogueSelector::State;
enum Action : uint64_t { Domain = 1, All, Select, Group, Expand, Clear, ExpandIcon, SelectRaster };
constexpr float browser_row_height = 25;

[[nodiscard]] uint64_t group_identity(std::string_view name) noexcept {
	uint64_t hash = 1469598103934665603ull;
	for (const unsigned char character : name)
		hash = (hash ^ character) * 1099511628211ull;
	return hash;
}

void activate(void* target, uint64_t action, uint64_t value) {
	auto& navigation = *static_cast<State*>(target);
	auto& state = *navigation.catalogue;
	auto& domain = state.domains[state.domain];
	state.keyboard_active = true;
	state.domain_focus = action == Domain;
	if (navigation.ui_manager) {
		navigation.ui_manager->inputFields().requestCaret(FlowElementID{},
														  CaretRequestKind::ClearAll);
		navigation.ui_manager->shortcuts().setFocusedElement(
			action == Domain ? navigation.domains_id : navigation.browser_id);
	}
	if (action == Domain) {
		select_catalogue_domain(state, static_cast<DevCatalogueDomain>(value));
		state.focused_domain = value;
		navigation.restore_scroll = true;
	} else if (action == All) {
		domain.group.clear();
		state.selected_identity = 0;
		state.selected_missing = state.selected_outside_filter = false;
		state.dirty = true;
	} else if (action == Select) {
		const auto asset = std::ranges::find(state.assets, value, &DevCatalogueAsset::identity);
		if (asset != state.assets.end() && domain.group != asset->group)
			domain.group.clear();
		select_catalogue_asset(state, value);
		state.focused_identity = value;
	} else if (action == Clear) {
		domain.search.clear();
		domain.group.clear();
		state.dirty = true;
	} else if (action == ExpandIcon) {
		const auto found = std::ranges::find(domain.expanded_icons, value);
		if (found == domain.expanded_icons.end())
			domain.expanded_icons.emplace_back(value);
		else
			domain.expanded_icons.erase(found);
	} else {
		const auto found =
			std::ranges::find(navigation.rows, value, &DevCatalogueSelector::BrowserRow::identity);
		if (found == navigation.rows.end())
			return;
		if (action == SelectRaster) {
			const auto asset = std::ranges::find(state.assets, found->asset_identity,
												 &DevCatalogueAsset::identity);
			if (asset != state.assets.end() && domain.group != asset->group)
				domain.group.clear();
			select_catalogue_asset(state, found->asset_identity);
			domain.view = 1;
			domain.page = found->raster_index;
			domain.raster_identity = found->raster_identity;
		} else if (action == Expand) {
			const auto collapsed = std::ranges::find(domain.collapsed_groups, found->name);
			if (collapsed == domain.collapsed_groups.end())
				domain.collapsed_groups.emplace_back(found->name);
			else
				domain.collapsed_groups.erase(collapsed);
		} else {
			domain.group = found->name;
			state.selected_identity = 0;
			state.selected_missing = false;
			state.dirty = true;
		}
		state.focused_identity = value;
	}
}

constexpr auto select_domain =
	UiAction("flowui.catalogue.selector.domain",
			 [](State& navigation, uint64_t value) { activate(&navigation, Domain, value); });

template <class Identity>
void domain_choice(Context& context, Identity identity, std::string_view label, uint64_t value,
				   bool focused, std::string_view badge) {
	auto style = selector_bar_style();
	style.layoutDirection = CLAY_LEFT_TO_RIGHT;
	style.contentGap = 8;
	if (focused) {
		style.borderWidth = Clay_BorderWidth{3, 0, 0, 0, 0};
		style.idleOverrides.borderColor = interface_theme::kAccentCurrent;
		style.hoveredOverrides.borderColor = interface_theme::kAccentCurrent;
	}
	FSEL::RadioChoiceParameters parameters{};
	parameters.onSelected = ActionCall{
		context.uiManager.actions().uiActions().make(select_domain, context.state(), value)};
	parameters.choiceValue = value;
	parameters.selectedValue = &context.state().catalogue->domain;
	parameters.style = style;
	context.uiManager.createElement(FSEL::kRadioChoice, identity)
		.setParameters(parameters)
		.setDevInternalCapture(true)
		.construct();
	const bool selected = context.state().catalogue->domain == value;
	const auto element = ::FlowUi::detail::element_id::resolveLocal(
		context.id, FSEL::RadioChoice::definitionId, identity.token);
	const bool hovered = context.uiManager.getPreviousFramesInteraction().isHovered(
		context.uiManager.toClayEID(element));
	Clay_TextElementConfig configuration{};
	configuration.fontSize = 11;
	configuration.textColor =
		selected || hovered ? interface_theme::kTextCanvas : interface_theme::kTextSecondary;
	configuration.wrapMode = CLAY_TEXT_WRAP_NONE;
	auto caption = ui::column(0);
	caption.layout.sizing.height = CLAY_SIZING_GROW(0);
	caption.layout.childAlignment.y = CLAY_ALIGN_Y_CENTER;
	caption.clip = {.horizontal = true, .scrollInputDisabled = true};
	CLAY(context.clayID(Keyed("row-caption", identity.token)), caption)
	CLAY_TEXT(context.uiManager.toClayString(label), CLAY_TEXT_CONFIG(configuration));
	auto count = ui::row(19);
	count.layout.sizing.width = CLAY_SIZING_FIXED(32);
	count.layout.childAlignment.x = CLAY_ALIGN_X_RIGHT;
	CLAY(context.clayID(Keyed("row-count", identity.token)), count) {
		configuration.textColor = interface_theme::kTextSecondary;
		configuration.textAlignment = CLAY_TEXT_ALIGN_RIGHT;
		CLAY_TEXT(context.uiManager.toClayString(badge), CLAY_TEXT_CONFIG(configuration));
	}
	context.uiManager.drawConstructed();
}

template <class Identity>
void control(Context& context, Identity identity, std::string_view label, uint64_t action,
			 uint64_t value, bool selected, bool focused, std::string_view badge = {},
			 std::string_view help = {}) {
	if (action == Domain) {
		domain_choice(context, identity, label, value, focused, badge);
		return;
	}
	context.uiManager.createElement(ui::kControl, identity)
		.setParameters({.label = std::string(label),
						.activate = activate,
						.target = &context.state(),
						.action = action,
						.value = value,
						.selected = selected,
						.focused = focused,
						.keyboard_activation = action == All || action == Clear,
						.domain_row = true})
		.setDevInternalCapture(true)
		.construct();
	Clay_TextElementConfig text{};
	text.fontSize = 12;
	text.textColor = selected ? interface_theme::kAccentCurrent : interface_theme::kTextSecondary;
	text.wrapMode = CLAY_TEXT_WRAP_NONE;
	auto caption = ui::column(0);
	caption.layout.sizing = {CLAY_SIZING_PERCENT(badge.empty() ? 1.0f : .64f), CLAY_SIZING_GROW(0)};
	caption.layout.childAlignment.y = CLAY_ALIGN_Y_CENTER;
	caption.clip = {.horizontal = true, .vertical = true, .scrollInputDisabled = true};
	CLAY(context.clayID(Keyed("row-caption", identity.token)), caption)
	CLAY_TEXT(context.uiManager.toClayString(label), CLAY_TEXT_CONFIG(text));
	if (!badge.empty()) {
		auto spacer = ui::column(0);
		CLAY(context.clayID(Keyed("badge-spacer", identity.token)), spacer) {}
		auto count = ui::column(0);
		count.layout.sizing = {CLAY_SIZING_FIXED(32), CLAY_SIZING_GROW(0)};
		count.layout.childAlignment = {CLAY_ALIGN_X_RIGHT, CLAY_ALIGN_Y_CENTER};
		count.clip = {.horizontal = true, .vertical = true, .scrollInputDisabled = true};
		CLAY(context.clayID(Keyed("row-count", identity.token)), count) {
			if (badge == "—") {
				Clay_ElementDeclaration dash{};
				dash.layout.sizing = {CLAY_SIZING_FIXED(10), CLAY_SIZING_FIXED(1)};
				dash.backgroundColor = interface_theme::kTextMuted;
				CLAY(context.clayID(Keyed("unavailable-count", identity.token)), dash) {}
			} else {
				text.textColor = interface_theme::kTextSecondary;
				text.textAlignment = CLAY_TEXT_ALIGN_RIGHT;
				CLAY_TEXT(context.uiManager.toClayString(badge), CLAY_TEXT_CONFIG(text));
			}
		}
	}
	context.uiManager.drawConstructed();
	if (!help.empty()) {
		const auto element = ::FlowUi::detail::element_id::resolveLocal(
			context.id, ui::Control::definitionId, identity.token);
		if (context.uiManager.getPreviousFramesInteraction().isHovered(
				context.uiManager.toClayEID(element)))
			ui::pointer_tooltip(context, Keyed("row-full-key", identity.token), help);
	}
	if (action == All || action == Clear)
		catalogue_focus::register_element(context, *context.state().catalogue,
										  ui::Control::definitionId, identity);
}

void keyboard(Context& context) {
	auto& state = *context.state().catalogue;
	const auto& input = context.uiManager.getCurrentFrameInput();
	const auto& previous = context.uiManager.getPreviousFrameInput();
	const auto focused = context.uiManager.shortcuts().focusedElement().id;
	const bool domain_focus = focused == context.state().domains_id.id;
	const bool browser_focus = focused == context.state().browser_id.id;
	if (domain_focus && (!state.keyboard_active || !state.domain_focus))
		state.focused_domain = state.domain;
	state.keyboard_active = domain_focus || browser_focus;
	state.domain_focus = domain_focus;
	if (!input.windowFocused || !state.keyboard_active ||
		context.uiManager.inputFields().hasPrimaryFieldFocus() ||
		context.uiManager.popups().has_open_popups())
		return;
	const auto pressed = [&](int key) { return input.keyDown[key] && !previous.keyDown[key]; };
	if (pressed(FlowUi::keyCode(FlowUi::Key::Escape))) {
		state.selected_identity = 0;
		state.selected_missing = false;
		state.dirty = true;
	}
	if (state.domain_focus) {
		if (pressed(FlowUi::keyCode(FlowUi::Key::Down)) ||
			pressed(FlowUi::keyCode(FlowUi::Key::Right)) ||
			pressed(FlowUi::keyCode(FlowUi::Key::Up)) ||
			pressed(FlowUi::keyCode(FlowUi::Key::Left))) {
			const bool backwards = pressed(FlowUi::keyCode(FlowUi::Key::Up)) ||
								   pressed(FlowUi::keyCode(FlowUi::Key::Left));
			activate(&context.state(), Domain, (state.focused_domain + (backwards ? 8 : 1)) % 9);
		} else if (pressed(FlowUi::keyCode(FlowUi::Key::Space)) ||
				   pressed(FlowUi::keyCode(FlowUi::Key::Enter)))
			activate(&context.state(), Domain, state.focused_domain);
		if (pressed(FlowUi::keyCode(FlowUi::Key::Down)) ||
			pressed(FlowUi::keyCode(FlowUi::Key::Right)) ||
			pressed(FlowUi::keyCode(FlowUi::Key::Up)) ||
			pressed(FlowUi::keyCode(FlowUi::Key::Left))) {
			const auto scroll = Clay_GetScrollContainerData(context.state().domains_id);
			if (scroll.found && scroll.scrollPosition) {
				const float top = 30 + static_cast<float>(state.focused_domain) * 25;
				if (top < -scroll.scrollPosition->y)
					scroll.scrollPosition->y = -top;
				else if (top + 25 >
						 -scroll.scrollPosition->y + scroll.scrollContainerDimensions.height)
					scroll.scrollPosition->y = scroll.scrollContainerDimensions.height - top - 25;
			}
		}
		return;
	}
	const auto& rows = context.state().rows;
	if (rows.empty())
		return;
	const auto found = std::ranges::find(rows, state.focused_identity,
										 &DevCatalogueSelector::BrowserRow::identity);
	std::size_t position = found == rows.end() ? 0 : static_cast<std::size_t>(found - rows.begin());
	if (pressed(FlowUi::keyCode(FlowUi::Key::Home)))
		position = 0;
	else if (pressed(FlowUi::keyCode(FlowUi::Key::End)))
		position = rows.size() - 1;
	else if (pressed(FlowUi::keyCode(FlowUi::Key::Down)))
		position = std::min(position + 1, rows.size() - 1);
	else if (pressed(FlowUi::keyCode(FlowUi::Key::Up)))
		position = position ? position - 1 : 0;
	state.focused_identity = rows[position].identity;
	if (pressed(FlowUi::keyCode(FlowUi::Key::Enter)) ||
		pressed(FlowUi::keyCode(FlowUi::Key::Space)))
		activate(&context.state(),
				 rows[position].group	 ? Group
				 : rows[position].raster ? SelectRaster
										 : Select,
				 rows[position].identity);
	if (rows[position].group && (pressed(FlowUi::keyCode(FlowUi::Key::Left)) ||
								 pressed(FlowUi::keyCode(FlowUi::Key::Right)))) {
		const auto& collapsed = state.domains[state.domain].collapsed_groups;
		const bool expanded = std::ranges::find(collapsed, rows[position].name) == collapsed.end();
		if ((pressed(FlowUi::keyCode(FlowUi::Key::Left)) && expanded) ||
			(pressed(FlowUi::keyCode(FlowUi::Key::Right)) && !expanded))
			activate(&context.state(), Expand, rows[position].identity);
	}
	if (!rows[position].group && !rows[position].raster &&
		state.domain == static_cast<uint64_t>(DevCatalogueDomain::Icons) &&
		(pressed(FlowUi::keyCode(FlowUi::Key::Left)) ||
		 pressed(FlowUi::keyCode(FlowUi::Key::Right)))) {
		const auto& expanded = state.domains[state.domain].expanded_icons;
		const bool open = std::ranges::find(expanded, rows[position].identity) != expanded.end();
		if ((pressed(FlowUi::keyCode(FlowUi::Key::Left)) && open) ||
			(pressed(FlowUi::keyCode(FlowUi::Key::Right)) && !open))
			activate(&context.state(), ExpandIcon, rows[position].identity);
	}
	if (pressed(FlowUi::keyCode(FlowUi::Key::Up)) || pressed(FlowUi::keyCode(FlowUi::Key::Down)) ||
		pressed(FlowUi::keyCode(FlowUi::Key::Home)) || pressed(FlowUi::keyCode(FlowUi::Key::End)))
		state.reveal_selection = true;
}

void build_rows(Context& context) {
	auto& state = *context.state().catalogue;
	auto& rows = context.state().rows;
	rows.clear();
	auto& browser_assets = context.state().browser_assets;
	browser_assets.clear();
	browser_assets.reserve(state.assets.size());
	const auto& search = state.domains[state.domain].search;
	const auto matches = [&](std::string_view value) {
		return search.empty() || std::ranges::search(value, search, [](char left, char right) {
									 return std::tolower(static_cast<unsigned char>(left)) ==
											std::tolower(static_cast<unsigned char>(right));
								 }).begin() != value.end();
	};
	for (size_t index = 0; index < state.assets.size(); ++index) {
		const auto& asset = state.assets[index];
		if (static_cast<uint64_t>(asset.domain) == state.domain &&
			(matches(asset.name) || matches(asset.group)))
			browser_assets.emplace_back(index);
	}
	size_t row_capacity = browser_assets.size() * 2;
	for (const auto asset_index : browser_assets)
		row_capacity += state.assets[asset_index].icons.size();
	rows.reserve(row_capacity);
	std::string last_group;
	for (const auto asset_index : browser_assets) {
		const auto& asset = state.assets[asset_index];
		if (asset.group != last_group) {
			last_group = asset.group;
			if (!last_group.empty())
				rows.emplace_back(DevCatalogueSelector::BrowserRow{
					.name = last_group, .identity = group_identity(last_group), .group = true});
		}
		if (std::ranges::find(state.domains[state.domain].collapsed_groups, asset.group) ==
			state.domains[state.domain].collapsed_groups.end()) {
			std::string_view label = asset.name;
			if (asset.domain == DevCatalogueDomain::Icons ||
				asset.domain == DevCatalogueDomain::Images) {
				const auto separator = label.find_last_of("/:\\");
				if (separator != std::string_view::npos && separator + 1 < label.size())
					label.remove_prefix(separator + 1);
			}
			rows.emplace_back(DevCatalogueSelector::BrowserRow{.name = std::string(label),
															   .identity = asset.identity,
															   .asset_identity = asset.identity});
			if (asset.domain == DevCatalogueDomain::Icons &&
				std::ranges::find(state.domains[state.domain].expanded_icons, asset.identity) !=
					state.domains[state.domain].expanded_icons.end()) {
				for (size_t index = 0; index < asset.icons.size(); ++index) {
					const auto& raster = asset.icons[index];
					rows.emplace_back(DevCatalogueSelector::BrowserRow{
						.name = std::to_string(raster.targetWidth) + " x " +
								std::to_string(raster.targetHeight) + " px · Page " +
								std::to_string(raster.atlasLayer),
						.identity = group_identity(std::to_string(asset.identity) + "/raster/" +
												   std::to_string(raster.atlasTexture.packed())),
						.asset_identity = asset.identity,
						.raster_identity = raster.atlasTexture.packed(),
						.raster_index = index,
						.raster = true});
				}
			}
		}
	}
}
} // namespace

void DevCatalogueSelector::buildElement(BuildContext& context) {
	if (!context.params.interfaceState)
		return;
	auto& state = context.params.interfaceState->catalogue;
	context.state().catalogue = &state;
	context.state().ui_manager = &context.uiManager;
	context.state().domains_id = context.clayID("domains");
	context.state().browser_id = context.clayID(Keyed("browser", state.domain));
	if (state.dirty)
		rebuild_catalogue_filter(state);
	build_rows(context);
	keyboard(context);
	if (state.dirty) {
		rebuild_catalogue_filter(state);
		build_rows(context);
	}
	auto root = ui::column(0);
	root.layout.sizing.width = CLAY_SIZING_PERCENT(1);
	root.layout.sizing.height = CLAY_SIZING_PERCENT(1);
	root.clip = {.horizontal = true, .scrollInputDisabled = true};
	const auto previous_dimensions = Clay_GetElementData(context.clayID());
	const auto parent_dimensions =
		Clay_GetElementData(Clay_ElementId{.id = Clay_GetOpenElementId()});
	const auto dimensions = previous_dimensions.found && previous_dimensions.boundingBox.height > 0
								? previous_dimensions
								: parent_dimensions;
	const float height = dimensions.found && dimensions.boundingBox.height > 0
							 ? dimensions.boundingBox.height
							 : Clay_GetLayoutDimensions().height - 120;
	const float maximum = std::min(255.0f, std::max(64.0f, height - 150));
	state.domain_height = std::clamp(state.domain_height, std::min(100.0f, maximum), maximum);
	CLAY(context.clayID(), root) {
		auto domains = ui::column(0);
		domains.layout.sizing.width = CLAY_SIZING_PERCENT(1);
		domains.layout.sizing.height = CLAY_SIZING_FIXED(state.domain_height);
		const auto domain_scroll_data = Clay_GetScrollContainerData(context.clayID("domains"));
		domains.clip = {.vertical = true,
						.childOffset = domain_scroll_data.found && domain_scroll_data.scrollPosition
										   ? *domain_scroll_data.scrollPosition
										   : Clay_Vector2{}};
		CLAY(context.clayID("domains"), domains) {
			auto heading = ui::row(30);
			heading.layout.padding = {12, 12, 0, 0};
			heading.backgroundColor = interface_theme::kDepth1Panel;
			heading.border = {.color = interface_theme::kBorderPrimary, .width = {0, 0, 0, 1, 0}};
			CLAY(context.clayID("heading"), heading)
			ui::text(context, "Catalogue:", interface_theme::kTextCanvas, 12);
			for (uint64_t domain_index = 0; domain_index < 9; ++domain_index) {
				const bool selected = state.domain == domain_index;
				const auto label =
					catalogue_domain_name(static_cast<DevCatalogueDomain>(domain_index));
				control(context, Keyed("domain", domain_index), label, Domain, domain_index,
						selected,
						state.keyboard_active && state.domain_focus &&
							state.focused_domain == domain_index,
						state.domain_status[domain_index].empty()
							? std::to_string(state.counts[domain_index])
							: "—");
			}
		}
		catalogue_focus::register_target(context, state, context.state().domains_id);
		FSEL::SplitterHandleParameters splitter{};
		splitter.axis = FSEL::SplitterAxis::Vertical;
		splitter.targetExtent = &state.domain_height;
		splitter.minExtent = std::min(100.0f, maximum);
		splitter.maxExtent = maximum;
		splitter.thickness = 1;
		splitter.hitThickness = 8;
		context.uiManager.createElement(FSEL::kSplitterHandle, "domain-splitter")
			.setParameters(splitter)
			.setDevInternalCapture(true)
			.draw();
		// A domain radio can activate while its row is built. Publish the new domain's
		// owned rows before borrowing its search and scroll settings for the lower browser.
		if (state.dirty) {
			rebuild_catalogue_filter(state);
			build_rows(context);
		}
		const auto rendered_domain = state.domain;
		auto& domain = state.domains[rendered_domain];
		context.state().browser_id = context.clayID(Keyed("browser", rendered_domain));
		auto browser_section = ui::column(0);
		browser_section.layout.sizing = {.width = CLAY_SIZING_GROW(0),
										 .height = CLAY_SIZING_GROW(0)};
		browser_section.clip = {.horizontal = true, .scrollInputDisabled = true};
		CLAY(context.clayID("browser-section"), browser_section) {
			control(context, LocalElementName{"all"},
					"All " + std::string(catalogue_domain_name(
								 static_cast<DevCatalogueDomain>(rendered_domain))),
					All, 0, !state.selected_identity && domain.group.empty(), false,
					std::to_string(context.state().browser_assets.size()));
			const auto before = domain.search;
			const auto placeholder = "Filter " +
									 std::string(catalogue_domain_name(
										 static_cast<DevCatalogueDomain>(rendered_domain))) +
									 "...";
			auto filter_bar = ui::row(36);
			filter_bar.layout.padding = {8, 8, 4, 4};
			filter_bar.backgroundColor = interface_theme::kDepth1Panel;
			filter_bar.border = {.color = interface_theme::kBorderPrimary,
								 .width = {0, 0, 0, 1, 0}};
			CLAY(context.clayID("filter-bar"), filter_bar) {
				context.uiManager
					.createElement(kDevSelectorSearch, Keyed("search", rendered_domain))
					.setParameters({.query = &domain.search,
									.placeholder = placeholder,
									.inline_control = true})
					.setDevInternalCapture(true)
					.draw();
			}
			const auto search_element = ::FlowUi::detail::element_id::resolveLocal(
				context.id, DevSelectorSearch::definitionId,
				Keyed("search", rendered_domain).token);
			const auto search_input = ::FlowUi::detail::element_id::resolveLocal(
				search_element, FSEL::TextInput::definitionId,
				LocalElementName{"search-input"}.token);
			catalogue_focus::register_target(
				context, state, context.uiManager.toClayEID(search_input), search_input);
			if (before != domain.search) {
				state.dirty = true;
				rebuild_catalogue_filter(state);
				build_rows(context);
			}

			if (state.selected_outside_filter)
				control(context, LocalElementName{"clear"}, "Selected outside filter · Clear",
						Clear, 0, false, false);
			auto browser = ui::column(0);
			browser.layout.sizing.width = CLAY_SIZING_PERCENT(1);
			browser.layout.sizing.height = CLAY_SIZING_GROW(0);
			const auto browser_id = context.clayID(Keyed("browser", rendered_domain));
			const auto scroll = Clay_GetScrollContainerData(browser_id);
			if (scroll.found && scroll.scrollPosition) {
				if (context.state().restore_scroll && rendered_domain == state.domain)
					scroll.scrollPosition->y = domain.selector_scroll;
				else
					domain.selector_scroll = scroll.scrollPosition->y;
			}
			if (rendered_domain == state.domain)
				context.state().restore_scroll = false;
			if (state.reveal_selection) {
				const auto identity = state.keyboard_active && !state.domain_focus
										  ? state.focused_identity
										  : state.selected_identity;
				const auto found =
					std::ranges::find(context.state().rows, identity, &BrowserRow::identity);
				if (found != context.state().rows.end()) {
					const float offset = static_cast<float>(found - context.state().rows.begin()) *
										 browser_row_height;
					const float visible_height =
						scroll.found ? scroll.scrollContainerDimensions.height : 200;
					if (offset < -domain.selector_scroll)
						domain.selector_scroll = -offset;
					else if (offset + browser_row_height > -domain.selector_scroll + visible_height)
						domain.selector_scroll = visible_height - offset - browser_row_height;
					if (scroll.found && scroll.scrollPosition)
						scroll.scrollPosition->y = domain.selector_scroll;
				}
				state.reveal_selection = false;
			}
			browser.clip = {
				.horizontal = true, .vertical = true, .childOffset = {0, domain.selector_scroll}};
			catalogue_focus::register_target(context, state, browser_id);
			CLAY(browser_id, browser) {
				const auto& rows = context.state().rows;
				const auto first = std::min(
					rows.size(),
					static_cast<std::size_t>(std::max(
						0.0f, std::floor(-domain.selector_scroll / browser_row_height) - 2)));
				const auto visible_height =
					scroll.found ? scroll.scrollContainerDimensions.height : height;
				const auto last =
					std::min(rows.size(), first +
											  static_cast<std::size_t>(std::max(
												  1.0f, visible_height / browser_row_height)) +
											  5);
				const auto spacer = [&](std::string_view name, float spacer_height) {
					auto declaration = ui::column(0);
					declaration.layout.sizing.height =
						CLAY_SIZING_FIXED(std::max(0.0f, spacer_height));
					CLAY(context.clayID(RuntimeName(name)), declaration) {}
				};
				spacer("before", static_cast<float>(first) * browser_row_height);
				uint64_t selected_browser_key = state.selected_identity;
				if (!state.selected_identity && !domain.group.empty())
					selected_browser_key = group_identity(domain.group);
				else if (state.selected_identity && domain.raster_identity) {
					const auto selected_raster = std::ranges::find_if(rows, [&](const auto& item) {
						return item.raster && item.asset_identity == state.selected_identity &&
							   item.raster_identity == domain.raster_identity;
					});
					if (selected_raster != rows.end())
						selected_browser_key = selected_raster->identity;
				}
				context.state().browser_selection = selected_browser_key;
				for (auto position = first; position < last; ++position) {
					const auto& item = rows[position];
					const auto asset = std::ranges::find(state.assets, item.asset_identity,
														 &DevCatalogueAsset::identity);
					const bool icon =
						!item.group && !item.raster &&
						state.domain == static_cast<uint64_t>(DevCatalogueDomain::Icons);
					const bool children = item.group || icon;
					const bool expanded =
						item.group
							? std::ranges::find(domain.collapsed_groups, item.name) ==
								  domain.collapsed_groups.end()
							: icon && std::ranges::find(domain.expanded_icons, item.identity) !=
										  domain.expanded_icons.end();
					context.state().browser_expanded = expanded;
					const uint32_t depth = item.group	 ? 0
										   : item.raster ? 2
										   : asset != state.assets.end() && !asset->group.empty()
											   ? 1
											   : 0;
					context.uiManager.createElement(kDevNode, Keyed("browser-node", item.identity))
						.setParameters(DevNodeParameters{
							.debugName = item.name,
							.forest_scroll_id = browser_id,
							.selected_key = &context.state().browser_selection,
							.expanded = children ? &context.state().browser_expanded : nullptr,
							.selectionKey = item.identity,
							.depth = depth,
							.hasChildren = children,
						})
						.setDevInternalCapture(true)
						.draw();
					if (children && expanded != context.state().browser_expanded)
						activate(&context.state(), item.group ? Expand : ExpandIcon, item.identity);
					if (selected_browser_key != context.state().browser_selection) {
						activate(&context.state(),
								 context.state().browser_selection ? item.group	   ? Group
																	 : item.raster ? SelectRaster
																				   : Select
																   : All,
								 item.identity);
						selected_browser_key = context.state().browser_selection;
					}
					const auto node_id = ::FlowUi::detail::element_id::resolveLocal(
						context.id, DevNode::definitionId,
						Keyed("browser-node", item.identity).token);
					if (asset != state.assets.end() &&
						context.uiManager.getPreviousFramesInteraction().isHovered(
							context.uiManager.toClayEID(node_id)))
						ui::pointer_tooltip(context, Keyed("row-full-key", item.identity),
											asset->name);
				}
				spacer("after", static_cast<float>(rows.size() - last) * browser_row_height);
				if (rows.empty())
					ui::text(context, "No assets match this filter", interface_theme::kTextMuted,
							 12);
			}
		}
	}
}
} // namespace FlowUi::devSystems::interface_elements
#endif
