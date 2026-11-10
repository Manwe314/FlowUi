#include "devSystems/devInterface/Inspect/Selector/DevThemeSelector.hpp"
#include "devSystems/devInterface/Inspect/DevThemeEditorData.hpp"
#if FLOW_UI_DEV_MODE
#include "devSystems/devInterface/Inspect/Selector/DevInspectSelectorElements.hpp"
namespace FlowUi::devSystems::interface_elements {
using namespace theme_editor;
namespace {}
void DevThemeSelector::buildElement(BuildContext& context) {
	auto* app = context.params.app;
	auto* state = context.params.interfaceState;
	if (!app || !state)
		return;
	const auto schema = app->devTooling().schemas().view();
	auto& model = state->theme_editor_model;
	if (!model.refresh(*app))
		return;
	const auto catalogue = model.catalogue();
	auto& cache = context.state();
	if (state->inspect_theme_key == 0 && cache.resolved_key != 0) {
		cache.resolved_key = 0;
		state->inspect_theme_type = 0;
		state->inspect_theme_group = 0;
	}
	std::vector<tooling::DevTypeId> types;
	types.reserve(catalogue.size());
	for (const auto& theme : catalogue)
		if (types.empty() || types.back() != theme.type)
			types.emplace_back(theme.type);
	if (cache.options_generation != model.generation()) {
		cache.labels.clear();
		cache.options.clear();
		cache.labels.reserve(types.size() + 3);
		cache.options.reserve(types.size() + 3);
		cache.labels = {"All Themes", "Active Only", "Modified Only"};
		tooling::DevTypeId last_type = 0;
		for (const auto& theme : catalogue)
			if (theme.type != last_type) {
				cache.labels.emplace_back(theme.typeName);
				last_type = theme.type;
			}
		for (std::size_t index = 0; index < cache.labels.size(); ++index)
			cache.options.emplace_back(
				FSEL::ComboBoxOption{index < 3 ? index : types[index - 3], cache.labels[index]});
		cache.options_generation = model.generation();
	}
	const auto panel = column(true);
	CLAY(context.clayID(), panel) {
		context.uiManager.createElement(kDevInterfaceSelectorTitle, "title")
			.setDevInternalCapture(true)
			.draw();
		context.uiManager.createElement(kDevInterfaceSelectorControls, "controls")
			.setParameters(DevInterfaceSelectorControlsParameters{
				&state->inspectForest, &state->inspect_theme_filter, cache.options,
				FLOW_UI_DEV_CAPTURE_CLAY != 0})
			.setDevInternalCapture(true)
			.draw();
		context.uiManager.createElement(kDevSelectorSearch, "search")
			.setParameters(DevSelectorSearchParameters{&state->searchQuery,
													   "Search themes, variants, tokens..."})
			.setDevInternalCapture(true)
			.draw();
		auto scroll = scroll_surface(context.clayID("tree"));
		scroll.backgroundColor = interface_theme::kDepth0Keel;
		scroll.border = {interface_theme::kBorderPrimary, {0, 0, 0, 1, 0}};
		CLAY(context.clayID("tree"), scroll) {
			auto tree_content = column();
			tree_content.layout.childGap = 0;
			tree_content.backgroundColor = interface_theme::kDepth0Keel;
			CLAY(context.clayID("tree-content"), tree_content) {
				tooling::DevTypeId last_type = 0;
				bool type_expanded = true;
				for (const auto& theme : catalogue) {
					if (state->inspect_theme_filter > 2 &&
						state->inspect_theme_filter != theme.type)
						continue;
					if (state->inspect_theme_filter == 1 && !theme.active)
						continue;
					const auto* presentation = model.find(theme);
					const auto fields = presentation
											? std::span<const ThemeLeaf>(presentation->fields)
											: std::span<const ThemeLeaf>{};
					const std::size_t modifications =
						presentation ? presentation->changed_indices.size() : 0;
					if (state->inspect_theme_filter == 2 && modifications == 0)
						continue;
					const bool match_theme = state->searchQuery.empty() ||
											 contains(theme.typeName, state->searchQuery) ||
											 contains(theme.variant, state->searchQuery);
					if (!match_theme && !std::ranges::any_of(fields, [&](const auto& leaf) {
							return contains(leaf.path, state->searchQuery);
						}))
						continue;
					if (theme.type != last_type) {
						last_type = theme.type;
						const auto key = stableNodeKey(theme.type, 0x5448454d45ull);
						bool expanded = true;
						context.uiManager
							.createElement(kDevNode,
										   RuntimeName("type-" + std::to_string(theme.type)))
							.setParameters(DevNodeParameters{
								.debugName = theme.typeName,
								.interfaceState = state,
								.app = app,
								.expandedOutput = &expanded,
								.selected_key = &state->inspect_theme_key,
								.selectionKey = key,
								.hasChildren = true,
							})
							.setDevInternalCapture(true)
							.draw();
						if (state->inspect_theme_key == key && cache.resolved_key != key) {
							cache.resolved_key = key;
							state->inspect_theme_type = theme.type;
							state->inspect_theme_variant = std::string(theme.variant);
							state->inspect_theme_all_variants = true;
							state->inspect_theme_group = 0;
						}
						type_expanded = expanded;
					}
					if (!type_expanded)
						continue;
					const auto key = stableNodeKey(
						theme.type, ::FlowUi::detail::identity_hash::hashBytes(theme.variant));
					bool expanded = true;
					context.uiManager
						.createElement(kDevNode, RuntimeName("variant-" + std::to_string(key)))
						.setParameters(DevNodeParameters{
							.debugName = theme.variant,
							.badgeText = theme.active ? "ACTIVE" : "",
							.interfaceState = state,
							.app = app,
							.expandedOutput = &expanded,
							.selected_key = &state->inspect_theme_key,
							.selectionKey = key,
							.badgeColor = interface_theme::kStatusGreen,
							.depth = 1,
							.hasChildren = schema && schema->findTheme(theme.type),
							.hasChanges = modifications != 0,
						})
						.setDevInternalCapture(true)
						.draw();
					if (state->inspect_theme_key == key && cache.resolved_key != key) {
						cache.resolved_key = key;
						state->inspect_theme_type = theme.type;
						state->inspect_theme_variant = std::string(theme.variant);
						state->inspect_theme_all_variants = false;
						state->inspect_theme_group = 0;
					}
					const auto* theme_schema = schema ? schema->findTheme(theme.type) : nullptr;
					if (!expanded || !theme_schema)
						continue;
					for (const auto& field : schema->fieldsOf(theme_schema->themeType)) {
						const auto* type = schema->type(field.valueType);
						if (!type || type->kind != tooling::DevTypeKind::Object ||
							type->editor != tooling::DevEditorKind::ObjectGroup)
							continue;
						if (!match_theme && !std::ranges::any_of(fields, [&](const auto& leaf) {
								return !leaf.key.nestedPath.empty() &&
									   leaf.key.nestedPath.front() == field.id &&
									   contains(leaf.path, state->searchQuery);
							}))
							continue;
						const auto group_key = stableNodeKey(key, field.id);
						context.uiManager
							.createElement(kDevNode,
										   RuntimeName("group-" + std::to_string(group_key)))
							.setParameters(DevNodeParameters{
								.debugName = schema->string(field.displayName),
								.interfaceState = state,
								.app = app,
								.selected_key = &state->inspect_theme_key,
								.selectionKey = group_key,
								.depth = 2,
							})
							.setDevInternalCapture(true)
							.draw();
						if (state->inspect_theme_key == group_key &&
							cache.resolved_key != group_key) {
							cache.resolved_key = group_key;
							state->inspect_theme_type = theme.type;
							state->inspect_theme_variant = std::string(theme.variant);
							state->inspect_theme_all_variants = false;
							state->inspect_theme_group = field.id;
						}
					}
				}
			}
		}
		auto footer = section(60, interface_theme::kDepth2Ink);
		footer.layout.padding = Clay_Padding{10, 10, 6, 6};
		footer.layout.childGap = 2;
		footer.layout.childAlignment.y = CLAY_ALIGN_Y_CENTER;
		footer.border.width = {};

		CLAY(context.clayID("footer"), footer) {
			text(context.uiManager, "Theme Types: " + std::to_string(types.size()), 10,
				 interface_theme::kTextMuted);
			text(context.uiManager, "Variants: " + std::to_string(catalogue.size()), 10,
				 interface_theme::kTextMuted);
			text(context.uiManager,
				 "Live Changes: " +
					 std::to_string(app->devTooling().overrides().themeBakeRecords().size()),
				 10, interface_theme::kTextMuted);
		}
	}
}

} // namespace FlowUi::devSystems::interface_elements
#endif
