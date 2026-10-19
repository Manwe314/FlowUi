#include "devSystems/devInterface/Inspect/Themes/DevThemeEditor.hpp"
#if FLOW_UI_DEV_MODE
#include "FSEL/Button.hpp"
#include "FSEL/Checkbox.hpp"
#include "FSEL/Label.hpp"
#include "FSEL/ProgressBar.hpp"
#include "FSEL/RadioChoice.hpp"
#include "FSEL/Slider.hpp"
#include "FSEL/Switch.hpp"
#include "FSEL/TextInput.hpp"
#include "devSystems/devInterface/Inspect/Inspector/TypeEditorElements/DevTypeEditorElements.hpp"
#include "devSystems/devInterface/Inspect/Selector/DevInspectSelectorElements.hpp"
#include "devSystems/devInterface/Permanents/Backend/DevTheme.hpp"
#include "devSystems/devTooling/DevTooling.hpp"
#include "devSystems/devTooling/bake/DevBakePipeline.hpp"
#include "managers/ThemeManager.hpp"
#include "managers/UiManager.hpp"
#include <algorithm>
#include <cctype>
#include <sstream>

namespace FlowUi::devSystems::interface_elements {
namespace {
using ThemeView = ThemeManager::DevThemePayloadView;
struct ThemeLeaf {
	tooling::DevOverrideFieldKey key{};
	std::string path{};
	const devMode::DevFieldSchema* field = nullptr;
	const void* live = nullptr;
	const void* authored = nullptr;
	const void* startup = nullptr;
};
[[nodiscard]] std::vector<ThemeView> themes(App& app) {
	std::vector<ThemeView> result;
	result.reserve(app.themes().devThemeCount());
	const auto visitor = [](void* destination, const ThemeView& view) noexcept {
		try {
			static_cast<std::vector<ThemeView>*>(destination)->emplace_back(view);
			return true;
		} catch (...) {
			return false;
		}
	};
	(void)app.themes().visitDevCatalogueThemes(&result, visitor);
	const auto schema = app.devTooling().schemas().view();
	if (schema)
		for (auto& theme : result) {
			const auto* reflected = schema->findTheme(theme.type);
			const auto* type = reflected ? schema->type(reflected->themeType) : nullptr;
			if (type)
				theme.typeName = schema->string(type->cppTypeName);
		}
	std::ranges::sort(result, [](const auto& left, const auto& right) {
		return left.typeName == right.typeName ? left.variant < right.variant
											   : left.typeName < right.typeName;
	});
	return result;
}
[[nodiscard]] const ThemeView* selected_theme(const std::vector<ThemeView>& catalogue,
											  const DevInterfaceState& state) noexcept {
	for (const auto& theme : catalogue)
		if (theme.type == state.inspect_theme_type &&
			(state.inspect_theme_all_variants ? theme.active
											  : theme.variant == state.inspect_theme_variant))
			return &theme;
	return nullptr;
}
[[nodiscard]] devMode::DevFieldIndex field_index(const devMode::DevSchemaGeneration& schema,
												 const devMode::DevFieldSchema& field) noexcept {
	return {static_cast<uint32_t>(&field - schema.fields.data() + 1)};
}
[[nodiscard]] std::string format_value(const devMode::DevSchemaGeneration& schema,
									   const ThemeLeaf& leaf, const void* value, bool cpp = false) {
	const auto formatted = tooling::serialize_dev_value(schema, leaf.field->valueType, value);
	return formatted ? (cpp ? formatted->cpp : formatted->json) : "Unavailable";
}
void collect_leaves(const devMode::DevSchemaGeneration& schema, devMode::DevTypeIndex owner,
					const void* live, const void* authored, const void* startup,
					tooling::DevOverrideFieldKey key, std::string prefix,
					std::vector<ThemeLeaf>& result, uint32_t depth = 0) {
	if (depth > 24 || !live)
		return;
	for (const auto& field : schema.fieldsOf(owner)) {
		if (field.operations >= schema.fieldOperations.size())
			continue;
		const auto* operations = schema.fieldOperations[field.operations];
		const auto* type = schema.type(field.valueType);
		if (!operations || !operations->constAddress || !type)
			continue;
		const void* live_field = operations->constAddress(live);
		const void* authored_field = authored ? operations->constAddress(authored) : nullptr;
		const void* startup_field = startup ? operations->constAddress(startup) : nullptr;
		const std::string path = prefix + std::string(schema.string(field.name));
		key.field = field.id;
		if (type->kind == devMode::DevTypeKind::Object &&
			type->editor == devMode::DevEditorKind::ObjectGroup) {
			auto child_key = key;
			child_key.nestedPath.emplace_back(field.id);
			collect_leaves(schema, field.valueType, live_field, authored_field, startup_field,
						   std::move(child_key), path + ".", result, depth + 1);
		} else {
			result.emplace_back(
				ThemeLeaf{key, path, &field, live_field, authored_field, startup_field});
		}
	}
}
[[nodiscard]] std::vector<ThemeLeaf> leaves(const devMode::DevSchemaGeneration& schema,
											const ThemeView& theme) {
	std::vector<ThemeLeaf> result;
	result.reserve(256);
	const auto* root = schema.findTheme(theme.type);
	if (root)
		collect_leaves(schema, root->themeType, theme.payload, theme.authored, theme.startup,
					   {theme.type, 0, {}}, "", result);
	return result;
}
[[nodiscard]] bool changed(const devMode::DevSchemaGeneration& schema, const ThemeLeaf& leaf) {
	return format_value(schema, leaf, leaf.live) != format_value(schema, leaf, leaf.authored) ||
		   format_value(schema, leaf, leaf.startup) != format_value(schema, leaf, leaf.authored);
}
[[nodiscard]] bool contains(std::string_view text, std::string_view query) {
	return std::search(text.begin(), text.end(), query.begin(), query.end(),
					   [](unsigned char left, unsigned char right) {
						   return std::tolower(left) == std::tolower(right);
					   }) != text.end();
}
[[nodiscard]] Clay_ElementDeclaration column(bool bounded = false) noexcept {
	Clay_ElementDeclaration declaration{};
	declaration.layout.sizing = bounded
									? Clay_Sizing{CLAY_SIZING_PERCENT(1), CLAY_SIZING_PERCENT(1)}
									: Clay_Sizing{CLAY_SIZING_GROW(0), CLAY_SIZING_FIT(0)};
	declaration.layout.layoutDirection = CLAY_TOP_TO_BOTTOM;
	declaration.layout.childGap = 8;
	declaration.backgroundColor = interface_theme::kDepth1Panel;
	return declaration;
}
[[nodiscard]] Clay_ElementDeclaration card() noexcept {
	auto declaration = column();
	declaration.layout.padding = CLAY_PADDING_ALL(12);
	declaration.backgroundColor = interface_theme::kDepth2Ink;
	declaration.cornerRadius = CLAY_CORNER_RADIUS(6);
	declaration.border = {interface_theme::kBorderVisible, {1, 1, 1, 1, 0}};
	return declaration;
}
void text(UiManager& ui, std::string_view label, uint16_t size = 11,
		  Clay_Color color = interface_theme::kTextSecondary) {
	Clay_TextElementConfig style{};
	style.fontSize = size;
	style.textColor = color;
	style.wrapMode = CLAY_TEXT_WRAP_WORDS;
	CLAY_TEXT(ui.toClayString(label), CLAY_TEXT_CONFIG(style));
}
void button(UiManager& ui, std::string_view id, std::string_view label, ActionCall action,
			bool enabled = true) {
	FSEL::ButtonParameters parameters{};
	parameters.text = label;
	parameters.contentMode = FSEL::ButtonContentMode::TextOnly;
	parameters.sizing = {CLAY_SIZING_FIT(0), CLAY_SIZING_FIXED(28)};
	parameters.onActivate = action;
	parameters.enabled = enabled;
	parameters.labelFontSize = 11;
	parameters.padding = Clay_Padding{8, 8, 5, 5};
	parameters.cornerRadius = CLAY_CORNER_RADIUS(4);
	parameters.idleOverrides.backgroundColor = interface_theme::kDepth3Elevated;
	parameters.idleOverrides.labelColor = interface_theme::kTextCanvas;
	parameters.idleOverrides.borderColor = interface_theme::kBorderVisible;
	ui.createElement(FSEL::kButton, RuntimeName(id))
		.setParameters(parameters)
		.setDevInternalCapture(true)
		.draw();
}
[[nodiscard]] Clay_ElementDeclaration row() noexcept {
	auto declaration = column();
	declaration.layout.layoutDirection = CLAY_LEFT_TO_RIGHT;
	declaration.layout.childAlignment.y = CLAY_ALIGN_Y_CENTER;
	declaration.backgroundColor = {};
	return declaration;
}
// Actions resolve theme coordinates at invocation, while retained UI state owns the selection.
constexpr auto kActivateTheme =
	UiAction("flowui.dev_interface.theme.activate", [](App& app, DevInterfaceState& state) {
		const auto status = app.themes().set_dev_active_variant(state.inspect_theme_type,
																state.inspect_theme_variant);
		state.lastActionMessage =
			status ? "Active theme variant updated" : "Theme variant is unavailable";
	});
constexpr auto kCloneTheme =
	UiAction("flowui.dev_interface.theme.clone", [](DevThemeInspector::State& target) {
		if (!target.app || !target.interface_state)
			return;
		auto& state = *target.interface_state;
		const auto catalogue = themes(*target.app);
		const auto* selected = selected_theme(catalogue, state);
		if (!selected)
			return;
		const auto result = target.app->themes().clone_dev_variant(
			state.inspect_theme_type, selected->variant, target.clone_name);
		state.lastActionMessage = result ? "Cloned theme variant: " + target.clone_name
										 : "Clone failed: enter a new, nonempty variant name";
	});
constexpr auto kToggleScope =
	UiAction("flowui.dev_interface.theme.scope", [](App& app, DevInterfaceState& state) {
		if (state.inspect_theme_all_variants) {
			const auto catalogue = themes(app);
			if (const auto* selected = selected_theme(catalogue, state))
				state.inspect_theme_variant = std::string(selected->variant);
		}
		state.inspect_theme_all_variants = !state.inspect_theme_all_variants;
		state.inspect_theme_group = 0;
	});
constexpr auto kClearGroup =
	UiAction("flowui.dev_interface.theme.clear-group",
			 [](DevInterfaceState& state) { state.inspect_theme_group = 0; });
constexpr auto kBakeThemes =
	UiAction("flowui.dev_interface.theme.bake", [](App& app, DevInterfaceState& state) {
		const auto result = app.devTooling().bakeActiveEdits();
		state.lastActionMessage = result.applied ? "Changes baked for the next build"
												 : "Bake failed; see Changes diagnostics";
	});
constexpr auto kThemeFieldAction = UiAction(
	"flowui.dev_interface.theme.field-action",
	[](DevThemeInspector::State& target, devMode::DevFieldIndex index, uint8_t operation) {
		if (!target.app || !target.interface_state)
			return;
		App& app = *target.app;
		auto& state = *target.interface_state;
		const auto schema = app.devTooling().schemas().view();
		const auto catalogue = themes(app);
		const auto* selected = selected_theme(catalogue, state);
		if (!schema || !selected)
			return;
		const auto fields = leaves(*schema, *selected);
		const auto found = std::ranges::find_if(
			fields, [&](const auto& leaf) { return field_index(*schema, *leaf.field) == index; });
		if (found == fields.end())
			return;
		tooling::DevOverrideCommand command{};
		command.kind = operation == 0	? tooling::DevOverrideCommandKind::ClearThemeField
					   : operation == 1 ? tooling::DevOverrideCommandKind::ResetThemeField
										: tooling::DevOverrideCommandKind::SetThemeField;
		command.theme = {selected->type, state.inspect_theme_all_variants || operation == 2
											 ? "*"
											 : std::string(selected->variant)};
		command.field = found->key;
		if (operation != 0 &&
			tooling::DevOwnedValue::copyFrom(
				*schema, found->field->valueType, operation == 1 ? found->authored : found->live,
				command.value) != devMode::DevValueOperationStatus::Success)
			return;
		DevInterfaceEditTransaction transaction{};
		transaction.description =
			operation == 2 ? "Propagate theme token to all variants" : "Revert theme token";
		if (operation == 2) {
			for (const auto& record : app.devTooling().overrides().themeBakeRecords()) {
				if (record.target.themeType != selected->type || record.target.variant == "*" ||
					record.field != command.field || record.preview_only)
					continue;
				auto clear = command;
				clear.kind = tooling::DevOverrideCommandKind::ClearThemeField;
				clear.theme = record.target;
				clear.value.reset();
				transaction.forward.emplace_back(clear);
				clear.kind = tooling::DevOverrideCommandKind::SetThemeField;
				clear.value = record.value;
				transaction.inverse.emplace_back(std::move(clear));
			}
		}
		transaction.forward.emplace_back(command);
		auto inverse = command;
		inverse.kind = tooling::DevOverrideCommandKind::ClearThemeField;
		for (const auto& record : app.devTooling().overrides().themeBakeRecords())
			if (record.target == command.theme && record.field == command.field &&
				!record.preview_only) {
				inverse.kind = tooling::DevOverrideCommandKind::SetThemeField;
				inverse.value = record.value;
				break;
			}
		transaction.inverse.emplace_back(std::move(inverse));
		tooling::DevChangeSet changes{state.nextEditTransaction++, transaction.forward};
		if (app.devTooling().overrides().submit(std::move(changes))) {
			state.editUndoStack.emplace_back(std::move(transaction));
			state.editRedoStack.clear();
		}
	});
[[nodiscard]] std::string cpp_string(std::string_view value) {
	std::string result = "\"";
	result.reserve(value.size() + 2);
	for (char character : value) {
		switch (character) {
		case '\\':
			result += "\\\\";
			break;
		case '\"':
			result += "\\\"";
			break;
		case '\n':
			result += "\\n";
			break;
		case '\r':
			result += "\\r";
			break;
		case '\t':
			result += "\\t";
			break;
		default:
			result += character;
			break;
		}
	}
	result += '\"';
	return result;
}
constexpr auto kCopyThemeCode =
	UiAction("flowui.dev_interface.theme.copy-code",
			 [](App& app, DevThemeWorkbench::State& state) { app.setClipboardText(state.code); });
constexpr auto kToggleSample =
	UiAction("flowui.dev_interface.theme.toggle-sample", [](bool& value) { value = !value; });

template <typename Context>
void identity(Context& context, const ThemeView* selected, std::string_view title) {
	auto header = card();
	header.cornerRadius = {};
	CLAY(context.clayID("identity"), header) {
		text(context.uiManager, title, 14, interface_theme::kTextCanvas);
		if (selected) {
			text(context.uiManager, selected->typeName, 12, interface_theme::kAccentSeaGlass);
			text(context.uiManager,
				 std::to_string(selected->payloadSize) + " bytes · revision " +
					 std::to_string(selected->revision),
				 10);
			text(context.uiManager, context.params.interfaceState->inspect_theme_all_variants
										? "All variants · active variant values"
										: std::string(selected->variant) +
											  (selected->active ? " · ACTIVE" : " · inactive"));
		} else
			text(context.uiManager, "Select a theme or variant from the selector");
	}
}
} // namespace

void DevThemeSelector::buildElement(BuildContext& context) {
	auto* app = context.params.app;
	auto* state = context.params.interfaceState;
	if (!app || !state)
		return;
	const auto schema = app->devTooling().schemas().view();
	const auto catalogue = themes(*app);
	auto& cache = context.state();
	if (state->inspect_theme_key == 0 && cache.resolved_key != 0) {
		cache.resolved_key = 0;
		state->inspect_theme_type = 0;
		state->inspect_theme_group = 0;
	}
	cache.labels.clear();
	cache.options.clear();
	cache.labels.reserve(catalogue.size() + 3);
	cache.options.reserve(catalogue.size() + 3);
	cache.labels = {"All Themes", "Active Only", "Modified Only"};
	std::vector<devMode::DevTypeId> types;
	types.reserve(catalogue.size());
	for (const auto& theme : catalogue)
		if (types.empty() || types.back() != theme.type) {
			types.emplace_back(theme.type);
			cache.labels.emplace_back(theme.typeName);
		}
	for (std::size_t index = 0; index < cache.labels.size(); ++index)
		cache.options.emplace_back(
			FSEL::ComboBoxOption{index < 3 ? index : types[index - 3], cache.labels[index]});
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
		auto scroll = column();
		scroll.layout.sizing.height = CLAY_SIZING_GROW(0);
		scroll.clip.vertical = true;
		scroll.layout.childGap = 0;
		CLAY(context.clayID("tree"), scroll) {
			devMode::DevTypeId last_type = 0;
			bool type_expanded = true;
			for (const auto& theme : catalogue) {
				if (state->inspect_theme_filter > 2 && state->inspect_theme_filter != theme.type)
					continue;
				if (state->inspect_theme_filter == 1 && !theme.active)
					continue;
				const auto fields = schema ? leaves(*schema, theme) : std::vector<ThemeLeaf>{};
				const std::size_t modifications =
					schema ? std::ranges::count_if(
								 fields, [&](const auto& leaf) { return changed(*schema, leaf); })
						   : 0;
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
						.createElement(kDevNode, RuntimeName("type-" + std::to_string(theme.type)))
						.setParameters(DevNodeParameters{
							.debugName = theme.typeName,
							.detailText = "General theme",
							.badgeText = "ALL",
							.interfaceState = state,
							.app = app,
							.expandedOutput = &expanded,
							.selected_key = &state->inspect_theme_key,
							.selectionKey = key,
							.badgeColor = interface_theme::kAccentSeaGlass,
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
				const std::string detail = std::to_string(modifications) + " modified tokens";
				context.uiManager
					.createElement(kDevNode, RuntimeName("variant-" + std::to_string(key)))
					.setParameters(DevNodeParameters{
						.debugName = theme.variant,
						.detailText = detail,
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
					if (!type || type->kind != devMode::DevTypeKind::Object ||
						type->editor != devMode::DevEditorKind::ObjectGroup)
						continue;
					if (!match_theme && !std::ranges::any_of(fields, [&](const auto& leaf) {
							return !leaf.key.nestedPath.empty() &&
								   leaf.key.nestedPath.front() == field.id &&
								   contains(leaf.path, state->searchQuery);
						}))
						continue;
					const auto group_key = stableNodeKey(key, field.id);
					context.uiManager
						.createElement(kDevNode, RuntimeName("group-" + std::to_string(group_key)))
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
					if (state->inspect_theme_key == group_key && cache.resolved_key != group_key) {
						cache.resolved_key = group_key;
						state->inspect_theme_type = theme.type;
						state->inspect_theme_variant = std::string(theme.variant);
						state->inspect_theme_all_variants = false;
						state->inspect_theme_group = field.id;
					}
				}
			}
		}
		auto footer = card();
		footer.cornerRadius = {};
		CLAY(context.clayID("footer"), footer) {
			text(context.uiManager, std::to_string(types.size()) + " theme types  ·  " +
										std::to_string(catalogue.size()) + " variants");
			text(context.uiManager,
				 std::to_string(app->devTooling().overrides().themeBakeRecords().size()) +
					 " live changes");
		}
	}
}

void DevThemeInspector::buildElement(BuildContext& context) {
	context.state().app = context.params.app;
	context.state().interface_state = context.params.interfaceState;
	auto* app = context.params.app;
	auto* state = context.params.interfaceState;
	if (!app || !state)
		return;
	const auto schema = app->devTooling().schemas().view();
	const auto catalogue = themes(*app);
	const auto* selected = selected_theme(catalogue, *state);
	CLAY(context.clayID(), column(true)) {
		identity(context, selected, "Theme Inspector");
		if (selected && schema) {
			CLAY(context.clayID("actions"), row()) {
				button(context.uiManager, "scope",
					   state->inspect_theme_all_variants ? "Edit this variant"
														 : "Edit all variants",
					   ActionCall{app->actions().uiActions().make(kToggleScope, *app, *state)});
				button(context.uiManager, "activate", "Set Active",
					   ActionCall{app->actions().uiActions().make(kActivateTheme, *app, *state)},
					   !selected->active && !state->inspect_theme_all_variants);
			}
			CLAY(context.clayID("clone"), row()) {
				FSEL::TextInputParameters name{};
				name.value = &context.state().clone_name;
				name.placeholder = "New variant name";
				name.fontSize = 11;
				name.sizing = Clay_Sizing{CLAY_SIZING_GROW(0), CLAY_SIZING_FIXED(28)};
				context.uiManager.createElement(FSEL::kTextInput, "clone-name")
					.setParameters(name)
					.setDevInternalCapture(true)
					.draw();
				button(context.uiManager, "clone", "Clone Variant",
					   ActionCall{app->actions().uiActions().make(kCloneTheme, context.state())});
			}
			CLAY(context.clayID("tabs"), row()) {
				for (uint64_t tab = 0; tab < 2; ++tab) {
					FSEL::RadioChoiceParameters parameters{};
					parameters.choiceValue = tab;
					parameters.selectedValue = &state->inspect_theme_tab;
					context.uiManager
						.createElement(FSEL::kRadioChoice,
									   RuntimeName("tab-" + std::to_string(tab)))
						.setParameters(parameters)
						.setDevInternalCapture(true)
						.construct();
					text(context.uiManager, tab == 0 ? "Tokens & Styles" : "Changes", 12,
						 interface_theme::kTextCanvas);
					context.uiManager.drawConstructed();
				}
			}
			if (state->inspect_theme_group)
				button(context.uiManager, "show-all", "Show all token groups",
					   ActionCall{app->actions().uiActions().make(kClearGroup, *state)});
			auto scroll = column();
			scroll.layout.sizing.height = CLAY_SIZING_GROW(0);
			scroll.clip.vertical = true;
			scroll.layout.padding = CLAY_PADDING_ALL(8);
			CLAY(context.clayID("fields"), scroll) {
				const auto* root = schema->findTheme(selected->type);
				if (!root)
					text(context.uiManager, "This theme has no reflected schema");
				else if (state->inspect_theme_tab == 0) {
					for (const auto& field : schema->fieldsOf(root->themeType)) {
						if (state->inspect_theme_group && state->inspect_theme_group != field.id)
							continue;
						const auto* operations = field.operations < schema->fieldOperations.size()
													 ? schema->fieldOperations[field.operations]
													 : nullptr;
						const void* value = operations && operations->constAddress
												? operations->constAddress(selected->payload)
												: nullptr;
						std::string label(schema->string(field.displayName));
						if (state->inspect_theme_all_variants && operations &&
							operations->constAddress) {
							const auto reference =
								tooling::serialize_dev_value(*schema, field.valueType, value);
							for (const auto& variant : catalogue)
								if (variant.type == selected->type) {
									const auto other = tooling::serialize_dev_value(
										*schema, field.valueType,
										operations->constAddress(variant.payload));
									if (reference && other && reference->json != other->json) {
										label += " · Mixed";
										break;
									}
								}
						}
						context.uiManager
							.createElement(kDevEditorCard,
										   RuntimeName("field-" + std::to_string(field.id) + "-" +
													   std::to_string(state->inspect_theme_key)))
							.setParameters(DevEditorCardParameters{
								.schema = schema,
								.binding =
									DevEditorBinding{.elementName = std::string(selected->typeName),
													 .theme_variant =
														 state->inspect_theme_all_variants
															 ? "*"
															 : std::string(selected->variant),
													 .theme_type = selected->type,
													 .rootOwnerType = selected->type,
													 .field = field_index(*schema, field)},
								.app = app,
								.interfaceState = state,
								.value = value,
								.labelOverride = label})
							.setDevInternalCapture(true)
							.draw();
					}
				} else {
					std::size_t count = 0;
					for (const auto& leaf : leaves(*schema, *selected)) {
						if (!changed(*schema, leaf))
							continue;
						if (state->inspect_theme_group &&
							(leaf.key.nestedPath.empty()
								 ? leaf.key.field
								 : leaf.key.nestedPath.front()) != state->inspect_theme_group)
							continue;
						++count;
						const auto index = field_index(*schema, *leaf.field);
						CLAY(context.clayID(RuntimeName(leaf.path)), card()) {
							text(context.uiManager, leaf.path, 12, interface_theme::kTextCanvas);
							text(context.uiManager,
								 "Authored  " + format_value(*schema, leaf, leaf.authored));
							text(context.uiManager,
								 "Baked  " + format_value(*schema, leaf, leaf.startup));
							text(context.uiManager,
								 "Live  " + format_value(*schema, leaf, leaf.live), 11,
								 interface_theme::kAccentSeaGlass);
							CLAY(context.clayID(RuntimeName(leaf.path + "-actions")), row()) {
								button(context.uiManager, "revert-" + leaf.path, "Revert Live",
									   ActionCall{app->actions().uiActions().make(
										   kThemeFieldAction, context.state(), index, uint8_t{0})});
								button(context.uiManager, "authored-" + leaf.path, "Authored",
									   ActionCall{app->actions().uiActions().make(
										   kThemeFieldAction, context.state(), index, uint8_t{1})},
									   !state->inspect_theme_all_variants);
								button(context.uiManager, "broadcast-" + leaf.path, "All Variants",
									   ActionCall{app->actions().uiActions().make(
										   kThemeFieldAction, context.state(), index, uint8_t{2})},
									   !state->inspect_theme_all_variants);
							}
						}
					}
					if (!count)
						text(context.uiManager, "No theme changes in this selection");
				}
			}
		}
	}
}

void DevThemeWorkbench::buildElement(BuildContext& context) {
	auto* app = context.params.app;
	auto* state = context.params.interfaceState;
	if (!app || !state)
		return;
	auto& sandbox = context.state();
	const auto schema = app->devTooling().schemas().view();
	const auto catalogue = themes(*app);
	const auto* selected = selected_theme(catalogue, *state);
	CLAY(context.clayID(), column(true)) {
		identity(context, selected, "Theme Workbench");
		if (schema && selected) {
			auto scroll = column();
			scroll.layout.sizing.height = CLAY_SIZING_GROW(0);
			scroll.clip.vertical = true;
			scroll.layout.padding = CLAY_PADDING_ALL(12);
			CLAY(context.clayID("content"), scroll) {
				CLAY(context.clayID("specimens"), card()) {
					text(context.uiManager, "Live component specimens", 13,
						 interface_theme::kTextCanvas);
					text(context.uiManager, "Controls below use the application's active themes. "
											"Activate a selected variant to preview it here.");
					CLAY(context.clayID("buttons"), row()) {
						for (uint32_t specimen = 0; specimen < 2; ++specimen) {
							FSEL::ButtonParameters parameters{};
							parameters.text = specimen ? "Disabled" : "Interactive button";
							parameters.contentMode = FSEL::ButtonContentMode::TextOnly;
							parameters.enabled = specimen == 0;
							parameters.onActivate = ActionCall{app->actions().uiActions().make(
								kToggleSample, sandbox.sample_checked)};
							context.uiManager
								.createElement(FSEL::kButton, RuntimeName("sample-button-" +
																		  std::to_string(specimen)))
								.setParameters(parameters)
								.setDevInternalCapture(true)
								.draw();
						}
					}
					CLAY(context.clayID("toggles"), row()) {
						FSEL::CheckboxParameters checkbox{};
						checkbox.isChecked = sandbox.sample_checked;
						checkbox.onToggle = ActionCall{
							app->actions().uiActions().make(kToggleSample, sandbox.sample_checked)};
						context.uiManager.createElement(FSEL::kCheckbox, "sample-checkbox")
							.setParameters(checkbox)
							.setDevInternalCapture(true)
							.draw();
						FSEL::SwitchParameters toggle{};
						toggle.isOn = sandbox.sample_switch;
						toggle.onToggle = ActionCall{
							app->actions().uiActions().make(kToggleSample, sandbox.sample_switch)};
						context.uiManager.createElement(FSEL::kSwitch, "sample-switch")
							.setParameters(toggle)
							.setDevInternalCapture(true)
							.draw();
						text(context.uiManager, "Checkbox & switch");
					}
					FSEL::SliderParameters slider{};
					slider.value = &sandbox.sample_value;
					slider.length = 260;
					context.uiManager.createElement(FSEL::kSlider, "sample-slider")
						.setParameters(slider)
						.setDevInternalCapture(true)
						.draw();
					FSEL::ProgressBarParameters progress{};
					progress.value = sandbox.sample_value;
					progress.sizing = Clay_Sizing{CLAY_SIZING_GROW(0), CLAY_SIZING_FIXED(8)};
					context.uiManager.createElement(FSEL::kProgressBar, "sample-progress")
						.setParameters(progress)
						.setDevInternalCapture(true)
						.draw();
					FSEL::TextInputParameters input{};
					input.value = &sandbox.sample_text;
					input.sizing = Clay_Sizing{CLAY_SIZING_GROW(0), CLAY_SIZING_FIXED(34)};
					context.uiManager.createElement(FSEL::kTextInput, "sample-input")
						.setParameters(input)
						.setDevInternalCapture(true)
						.draw();
					FSEL::LabelParameters label{};
					label.text = sandbox.sample_text;
					context.uiManager.createElement(FSEL::kLabel, "sample-label")
						.setParameters(label)
						.setDevInternalCapture(true)
						.draw();
				}
				const auto selected_leaves = leaves(*schema, *selected);
				std::vector<std::pair<std::string_view, std::vector<ThemeLeaf>>> variant_tokens;
				variant_tokens.reserve(catalogue.size());
				for (const auto& variant : catalogue)
					if (variant.type == selected->type)
						variant_tokens.emplace_back(variant.variant, leaves(*schema, variant));
				CLAY(context.clayID("tokens"), card()) {
					text(context.uiManager, "Palette & token comparison", 13,
						 interface_theme::kTextCanvas);
					for (std::size_t leaf_index = 0; leaf_index < selected_leaves.size();
						 ++leaf_index) {
						const auto& leaf = selected_leaves[leaf_index];
						if (state->inspect_theme_group &&
							(leaf.key.nestedPath.empty()
								 ? leaf.key.field
								 : leaf.key.nestedPath.front()) != state->inspect_theme_group)
							continue;
						if (!state->searchQuery.empty() &&
							!contains(leaf.path, state->searchQuery) &&
							!contains(selected->typeName, state->searchQuery))
							continue;
						CLAY(context.clayID(RuntimeName(leaf.path)), column()) {
							const auto* type = schema->type(leaf.field->valueType);
							CLAY(context.clayID(RuntimeName(leaf.path + "-heading")), row()) {
								if (type && type->editor == devMode::DevEditorKind::Color &&
									leaf.live) {
									Clay_ElementDeclaration swatch{};
									swatch.layout.sizing = {CLAY_SIZING_FIXED(28),
															CLAY_SIZING_FIXED(20)};
									swatch.backgroundColor =
										*static_cast<const Clay_Color*>(leaf.live);
									swatch.cornerRadius = CLAY_CORNER_RADIUS(3);
									CLAY(context.clayID(RuntimeName(leaf.path + "-swatch")),
										 swatch);
								}
								text(context.uiManager, leaf.path, 11,
									 interface_theme::kTextCanvas);
							}
							std::string comparison;
							std::string first_value;
							bool mixed = false;
							for (const auto& [variant_name, tokens] : variant_tokens) {
								if (leaf_index >= tokens.size())
									continue;
								const auto value = format_value(*schema, tokens[leaf_index],
																tokens[leaf_index].live);
								if (first_value.empty())
									first_value = value;
								else
									mixed |= first_value != value;
								if (!comparison.empty())
									comparison += "   ·   ";
								comparison += std::string(variant_name) + ": " + value;
							}
							text(context.uiManager, (mixed ? "MIXED  " : "") + comparison, 10,
								 mixed ? interface_theme::kStatusAmber
									   : interface_theme::kTextSecondary);
						}
					}
				}
				CLAY(context.clayID("code"), card()) {
					text(context.uiManager, "C++ theme patch", 13, interface_theme::kTextCanvas);
					sandbox.code.clear();
					for (const auto& variant : catalogue) {
						if (variant.type != selected->type ||
							(!state->inspect_theme_all_variants &&
							 variant.variant != selected->variant))
							continue;
						const auto tokens = leaves(*schema, variant);
						sandbox.code += "{\n  auto theme = app.themes().getTheme<" +
										std::string(variant.typeName) + ">(" +
										cpp_string(variant.variant) + ");\n";
						for (const auto& leaf : tokens)
							if (changed(*schema, leaf)) {
								const auto value = tooling::serialize_dev_value(
									*schema, leaf.field->valueType, leaf.live);
								if (value)
									sandbox.code +=
										"  theme." + leaf.path + " = " + value->cpp + ";\n";
							}
						sandbox.code += "  app.themes().registerTheme(" +
										cpp_string(variant.variant) + ", theme, " +
										(variant.active ? "true" : "false") + ");\n}\n";
					}
					CLAY(context.clayID("code-actions"), row()) {
						button(context.uiManager, "copy", "Copy C++",
							   ActionCall{
								   app->actions().uiActions().make(kCopyThemeCode, *app, sandbox)});
						button(
							context.uiManager, "bake", "Bake Changes",
							ActionCall{app->actions().uiActions().make(kBakeThemes, *app, *state)});
					}
					text(context.uiManager, sandbox.code, 10);
				}
			}
		}
	}
}
} // namespace FlowUi::devSystems::interface_elements
#endif
