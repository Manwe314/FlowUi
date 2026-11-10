#ifdef NDEBUG
#undef NDEBUG
#endif
#include "FSEL/RadioChoice.hpp"
#include "FSEL/SplitterHandle.hpp"
#include "FlowUi/Public/Flow.hpp"
#include "devSystems/devInterface/Catalogue/Selector/DevCatalogueSelector.hpp"
#include "devSystems/devInterface/Changes/Selector/DevChangesSelector.hpp"
#include "devSystems/devInterface/Diagnostics/DevDiagnosticsUi.hpp"
#include "devSystems/devInterface/Inspect/Inspector/TypeEditorElements/DevTypeEditorElements.hpp"
#include "devSystems/devInterface/Inspect/Selector/DevInspectSelectorElements.hpp"
#include "devSystems/devInterface/Permanents/Elements/DevSelectorSearch.hpp"
#include "devSystems/devTooling/DevTooling.hpp"
#include <algorithm>
#include <cassert>
#include <cmath>
#include <iostream>

using namespace FlowUi;
using namespace FlowUi::devSystems;
using namespace FlowUi::devSystems::interface_elements;
namespace {
constexpr auto catalogue_selector = Global<kDevCatalogueSelector>("catalogue.selector.regression");
constexpr auto changes_selector = Global<kDevChangesSelector>("catalogue.changes.regression");
constexpr auto color_editor_id = Global<kDevColorTypeEditor>("catalogue.local.color.regression");
constexpr TextureHandle raster_texture{.index = 1, .generation = 1};

template <class Identity>
[[nodiscard]] FlowElementID local(FlowElementID parent, FlowDefinitionID definition,
								  Identity identity) noexcept {
	return ::FlowUi::detail::element_id::resolveLocal(parent, definition, identity.token);
}
[[nodiscard]] const tooling::DevClayNode& clay_node(App& app, FlowElementID identity) noexcept {
	const auto& nodes = app.ui().devTreeSnapshot().clay.nodes;
	const auto found =
		std::ranges::find(nodes, app.ui().toClayEID(identity).id, &tooling::DevClayNode::clayId);
	assert(found != nodes.end());
	return *found;
}
[[nodiscard]] FlowElementID node_with_text(App& app, std::string_view text) noexcept {
	const auto& snapshot = app.ui().devTreeSnapshot();
	const auto found = std::ranges::find_if(
		snapshot.clay.nodes, [&](const auto& node) { return snapshot.string(node.text) == text; });
	assert(found != snapshot.clay.nodes.end());
	assert(found->directFlowOwner < snapshot.flow.nodes.size());
	return FlowElementID{snapshot.flow.nodes[found->directFlowOwner].instance.value};
}
void check_left_indicator(App& app, FlowElementID identity) {
	const auto& width = clay_node(app, identity).declaration.border.width;
	assert(width.left == 3 && width.right == 0 && width.top == 0 && width.bottom == 0);
}
} // namespace

int main() {
	AppConfig config{};
	config.window.title = "Catalogue selector regression";
	config.window.width = 600;
	config.window.height = 700;
	config.dev.excludeInternalDevElementsFromCapture = false;
	config.vk.enableValidation = true;
	auto app = makeApplication(config);
	DevInterfaceState session{};
	auto& catalogue = session.catalogue;
	catalogue.assets.reserve(42);
	catalogue.assets.emplace_back(DevCatalogueAsset{.name = "toolbar/open",
													.group = "toolbar",
													.identity = 101,
													.domain = DevCatalogueDomain::Icons});
	catalogue.assets.back().icons.emplace_back(devSystems::tooling::DevIconCatalogEntry{
		.targetWidth = 16, .targetHeight = 16, .atlasTexture = raster_texture});
	catalogue.assets.emplace_back(DevCatalogueAsset{.name = "toolbar/save",
													.group = "toolbar",
													.identity = 102,
													.domain = DevCatalogueDomain::Icons});
	catalogue.counts[static_cast<std::size_t>(DevCatalogueDomain::Icons)] = 2;

	Clay_Color preview_color{255, 255, 255, 255};
	DevTypeEditorState color_editor{};
	color_editor.app = &app;
	color_editor.interfaceState = &session;
	color_editor.editValue = &preview_color;
	color_editor.fieldName = "Catalogue tint";
	color_editor.editable = true;
	color_editor.local_value = true;
	bool show_color = false;
	bool show_changes = false;
	const auto draw = [&](FlowElementID target = {}, int event = 0, FlowElementID extra = {},
						  float pointer_y = -10, std::string_view replacement_text = {}) {
		assert(app.beginFrame());
		auto& interaction =
			const_cast<InteractionSnapshot&>(app.ui().getPreviousFramesInteraction());
		interaction = {};
		auto& input = const_cast<FrameInput&>(app.ui().getCurrentFrameInput());
		input.windowFocused = true;
		input.mouseDown = {};
		input.mouseDown[0] = event == 1 || event == 3;
		input.mouseX = -10;
		input.mouseY = pointer_y;
		for (const auto control : {target, extra}) {
			if (!control)
				continue;
			const auto clay_identity = app.ui().toClayEID(control).id;
			interaction.hoveredElementIds.emplace_back(clay_identity);
			if (event == 1)
				interaction.pressedElementIds.emplace_back(clay_identity);
			if (event == 2)
				interaction.releasedElementIds.emplace_back(clay_identity);
			if (event == 3)
				interaction.heldElementIds.emplace_back(clay_identity);
		}
		if (!replacement_text.empty()) {
			const auto hex_input = local(FlowElementID{color_editor_id.value},
										 FSEL::TextInput::definitionId, LocalElementName{"hex"});
			assert(app.ui().inputFields().replaceText(hex_input, replacement_text, true));
		}
		Clay_ElementDeclaration root{};
		root.layout.sizing = {.width = CLAY_SIZING_FIXED(280), .height = CLAY_SIZING_FIXED(520)};
		root.layout.layoutDirection = CLAY_TOP_TO_BOTTOM;
		root.clip = {.horizontal = true, .vertical = true};
		CLAY(CLAY_ID("catalogue-selector-regression-root"), root) {
			if (show_color)
				app.ui()
					.createElement(kDevColorTypeEditor, color_editor_id)
					.setParameters({.editor = &color_editor})
					.draw();
			else if (show_changes)
				app.ui()
					.createElement(kDevChangesSelector, changes_selector)
					.setParameters({&app, &session})
					.draw();
			else
				app.ui()
					.createElement(kDevCatalogueSelector, catalogue_selector)
					.setParameters({&app, &session})
					.draw();
		}
		assert(app.endFrame());
		assert(app.drawFrame());
	};
	const auto click = [&](FlowElementID target, FlowElementID extra = {}) {
		draw(target, 1, extra);
		draw(target, 2, extra);
		draw();
	};
	draw();
	draw();
	const auto catalogue_owner = FlowElementID{catalogue_selector.value};
	const auto icons_choice =
		local(catalogue_owner, FSEL::RadioChoice::definitionId,
			  Keyed("domain", static_cast<uint64_t>(DevCatalogueDomain::Icons)));
	click(icons_choice);
	assert(catalogue.domain == static_cast<uint64_t>(DevCatalogueDomain::Icons));
	check_left_indicator(app, icons_choice);
	const auto all =
		local(catalogue_owner, diagnostics_ui::Control::definitionId, LocalElementName{"all"});
	const auto search =
		local(catalogue_owner, DevSelectorSearch::definitionId, Keyed("search", catalogue.domain));
	const auto all_bounds = Clay_GetElementData(app.ui().toClayEID(all)).boundingBox;
	const auto search_bounds = Clay_GetElementData(app.ui().toClayEID(search)).boundingBox;
	assert(all_bounds.y + all_bounds.height <= search_bounds.y);
	assert(search_bounds.height == 28);

	const auto icon_node =
		local(catalogue_owner, DevNode::definitionId, Keyed("browser-node", 101));
	assert(Clay_GetElementData(app.ui().toClayEID(icon_node)).boundingBox.height == 25);
	click(icon_node);
	assert(catalogue.selected_identity == 101);
	const auto disclosure = local(icon_node, DevNode::definitionId, LocalElementName{"disclosure"});
	click(icon_node, disclosure);
	assert(catalogue.selected_identity == 101);
	assert(catalogue.domains[catalogue.domain].expanded_icons == std::vector<uint64_t>{101});
	click(node_with_text(app, "16 x 16 px · Page 0"));
	assert(catalogue.selected_identity == 101);
	assert(catalogue.domains[catalogue.domain].view == 1);
	assert(catalogue.domains[catalogue.domain].raster_identity == raster_texture.packed());
	click(icon_node, disclosure);
	assert(catalogue.domains[catalogue.domain].expanded_icons.empty());
	assert(catalogue.selected_identity == 101);

	const auto group_node = node_with_text(app, "toolbar");
	const auto group_disclosure =
		local(group_node, DevNode::definitionId, LocalElementName{"disclosure"});
	click(group_node, group_disclosure);
	assert(catalogue.domains[catalogue.domain].collapsed_groups ==
		   std::vector<std::string>{"toolbar"});
	assert(catalogue.selected_identity == 101);
	click(group_node, group_disclosure);
	assert(catalogue.domains[catalogue.domain].collapsed_groups.empty());
	click(group_node);
	assert(catalogue.selected_identity == 0 &&
		   catalogue.domains[catalogue.domain].group == "toolbar");
	click(group_node);
	assert(catalogue.domains[catalogue.domain].group.empty());

	const auto domains =
		local(catalogue_owner, DevCatalogueSelector::definitionId, LocalElementName{"domains"});
	const auto browser = local(catalogue_owner, DevCatalogueSelector::definitionId,
							   LocalElementName{"browser-section"});
	const auto splitter = local(catalogue_owner, FSEL::SplitterHandle::definitionId,
								LocalElementName{"domain-splitter"});
	const auto domains_before = Clay_GetElementData(app.ui().toClayEID(domains)).boundingBox;
	const auto browser_before = Clay_GetElementData(app.ui().toClayEID(browser)).boundingBox;
	assert(domains_before.height == 255);
	const auto splitter_before = Clay_GetElementData(app.ui().toClayEID(splitter)).boundingBox;
	const float pointer_start = splitter_before.y + splitter_before.height / 2;
	draw(splitter, 1, {}, pointer_start);
	draw(splitter, 3, {}, pointer_start - 80);
	draw(splitter, 2, {}, pointer_start - 80);
	draw();
	const auto domains_after = Clay_GetElementData(app.ui().toClayEID(domains)).boundingBox;
	const auto browser_after = Clay_GetElementData(app.ui().toClayEID(browser)).boundingBox;
	assert(std::abs(domains_before.height - domains_after.height - 80) < .5f);
	assert(std::abs(browser_after.height - browser_before.height - 80) < .5f);
	assert(std::abs(browser_before.y - browser_after.y - 80) < .5f);
	assert(std::abs(domains_after.height - catalogue.domain_height) < .5f);
	assert(std::abs(browser_after.y + browser_after.height - 520) < .5f);
	const auto domain_scroll = Clay_GetScrollContainerData(app.ui().toClayEID(domains));
	assert(domain_scroll.found &&
		   domain_scroll.contentDimensions.height > domain_scroll.scrollContainerDimensions.height);

	// A real asset list overflows only the inner browser, leaving its count and filter pinned.
	for (uint64_t asset_index = 0; asset_index < 40; ++asset_index)
		catalogue.assets.emplace_back(
			DevCatalogueAsset{.name = "toolbar/extra-" + std::to_string(asset_index),
							  .group = "toolbar",
							  .identity = 103 + asset_index,
							  .domain = DevCatalogueDomain::Icons});
	catalogue.counts[static_cast<std::size_t>(DevCatalogueDomain::Icons)] = catalogue.assets.size();
	catalogue.dirty = true;
	draw();
	draw();
	const auto browser_scroll_id =
		local(catalogue_owner, DevCatalogueSelector::definitionId,
			  Keyed("browser", static_cast<uint64_t>(DevCatalogueDomain::Icons)));
	const auto browser_bounds = Clay_GetElementData(app.ui().toClayEID(browser)).boundingBox;
	const auto viewport_bounds =
		Clay_GetElementData(app.ui().toClayEID(browser_scroll_id)).boundingBox;
	const auto browser_scroll = Clay_GetScrollContainerData(app.ui().toClayEID(browser_scroll_id));
	assert(browser_scroll.found && browser_scroll.scrollPosition);
	assert(std::abs(browser_bounds.height - browser_after.height) < .5f);
	assert(viewport_bounds.y >= browser_bounds.y);
	assert(std::abs(viewport_bounds.y + viewport_bounds.height - 520) < .5f);
	assert(std::abs(viewport_bounds.height - browser_bounds.height + 66) < .5f);
	assert(browser_scroll.contentDimensions.height >
		   browser_scroll.scrollContainerDimensions.height);
	assert(std::abs(browser_scroll.scrollContainerDimensions.height - viewport_bounds.height) <
		   .5f);
	const auto second_node =
		local(catalogue_owner, DevNode::definitionId, Keyed("browser-node", 102));
	const auto node_before_scroll =
		Clay_GetElementData(app.ui().toClayEID(second_node)).boundingBox;
	const auto count_before_scroll = Clay_GetElementData(app.ui().toClayEID(all)).boundingBox;
	browser_scroll.scrollPosition->y = -100;
	draw();
	const auto node_after_scroll = Clay_GetElementData(app.ui().toClayEID(second_node)).boundingBox;
	assert(std::abs(node_after_scroll.y - node_before_scroll.y + 100) < .5f);
	assert(Clay_GetElementData(app.ui().toClayEID(all)).boundingBox.y == count_before_scroll.y);

	show_changes = true;
	const auto changes_choice = local(FlowElementID{changes_selector.value},
									  FSEL::RadioChoice::definitionId, Keyed("domain", 2));
	draw();
	click(changes_choice);
	assert(session.changes.domain == 2);
	app.ui().shortcuts().setFocusedElement(app.ui().toClayEID(changes_choice));
	draw();
	check_left_indicator(app, changes_choice);
	// The shared color picker edits only this preview's caller-owned color.
	show_color = true;
	draw();
	draw();
	const auto hex_input = local(FlowElementID{color_editor_id.value},
								 FSEL::TextInput::definitionId, LocalElementName{"hex"});
	const auto transactions_before = app.devTooling().overrides().stats().committedTransactions;
	const auto rejected_before = app.devTooling().overrides().stats().rejectedTransactions;
	const auto results_before = app.devTooling().overrides().commandResults().size();
	const auto edit_transaction_before = session.nextEditTransaction;
	const auto undo_count_before = session.editUndoStack.size();
	app.ui().inputFields().requestCaret(hex_input, CaretRequestKind::SetPrimary);
	draw();
	draw({}, 0, {}, -10, "#33669980");
	assert(color_editor.draftValid);
	assert(preview_color.r == 51 && preview_color.g == 102 && preview_color.b == 153 &&
		   preview_color.a == 128);
	const auto* input_state =
		app.ui().elements().getStatePointerConst(FSEL::kTextInput, app.ui().windowId(), hex_input);
	assert(input_state && input_state->editSessionDirty && input_state->wasFocused);
	app.ui().inputFields().requestCaret(hex_input, CaretRequestKind::ClearAll);
	draw();
	draw();
	input_state =
		app.ui().elements().getStatePointerConst(FSEL::kTextInput, app.ui().windowId(), hex_input);
	assert(input_state && !input_state->editSessionDirty && !input_state->wasFocused);
	assert(preview_color.r == 51 && preview_color.g == 102 && preview_color.b == 153 &&
		   preview_color.a == 128);
	assert(!color_editor.previewActive);
	assert(session.nextEditTransaction == edit_transaction_before);
	assert(session.editUndoStack.size() == undo_count_before);
	const auto overrides_after = app.devTooling().overrides().stats();
	assert(overrides_after.committedTransactions == transactions_before);
	assert(overrides_after.rejectedTransactions == rejected_before);
	assert(app.devTooling().overrides().commandResults().size() == results_before);

	std::cout << "Catalogue selector interaction and split geometry passed\n";
}
