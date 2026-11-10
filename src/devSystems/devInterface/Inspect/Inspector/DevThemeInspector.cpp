#include "devSystems/devInterface/Inspect/Inspector/DevThemeInspector.hpp"
#include "devSystems/devInterface/Inspect/DevThemeEditorData.hpp"
#if FLOW_UI_DEV_MODE
#include "FSEL/RadioChoice.hpp"
#include "FSEL/TextInput.hpp"
#include "devSystems/devInterface/Inspect/Inspector/TypeEditorElements/DevTypeEditorElements.hpp"
#include "devSystems/devInterface/Inspect/Selector/DevInspectSelectorElements.hpp"
namespace FlowUi::devSystems::interface_elements {
using namespace theme_editor;
namespace {
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
constexpr auto kThemeFieldAction = UiAction(
	"flowui.dev_interface.theme.field-action",
	[](DevThemeInspector::State& target, tooling::DevFieldIndex index, uint8_t operation) {
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
				command.value) != tooling::DevValueOperationStatus::Success)
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
} // namespace
void DevThemeInspector::buildElement(BuildContext& context) {
	context.state().app = context.params.app;
	context.state().interface_state = context.params.interfaceState;
	auto* app = context.params.app;
	auto* state = context.params.interfaceState;
	if (!app || !state)
		return;
	const auto schema = app->devTooling().schemas().view();
	auto& model = state->theme_editor_model;
	if (!model.refresh(*app))
		return;
	const auto catalogue = model.catalogue();
	const auto* selected = selected_theme(catalogue, *state);
	CLAY(context.clayID(), column(true)) {
		context.uiManager.createElement(kDevInterfaceInspectorTitle, "title")
			.setDevInternalCapture(true)
			.draw();
		if (selected && schema) {
			context.uiManager.createElement(kDevInterfaceInspectorIdentity, "identity")
				.setParameters(DevInterfaceInspectorIdentityParameters{
					.instanceName = state->inspect_theme_all_variants
										? std::string_view{"All Variants"}
										: selected->variant,
					.definitionName = selected->typeName})
				.setDevInternalCapture(true)
				.draw();
			CLAY(context.clayID("metadata"), section()) {
				text(context.uiManager,
					 std::to_string(selected->payloadSize) + " bytes · revision " +
						 std::to_string(selected->revision) + (selected->active ? " · Active" : ""),
					 10, interface_theme::kTextMuted);
				CLAY(context.clayID("actions"), row()) {
					button(context, "scope",
						   state->inspect_theme_all_variants ? "Edit this variant"
															 : "Edit all variants",
						   ActionCall{app->actions().uiActions().make(kToggleScope, *app, *state)});
					button(
						context, "activate", "Set Active",
						ActionCall{app->actions().uiActions().make(kActivateTheme, *app, *state)},
						!selected->active && !state->inspect_theme_all_variants);
				}
				CLAY(context.clayID("clone"), row()) {
					FSEL::TextInputParameters name{};
					name.value = &context.state().clone_name;
					name.placeholder = "New variant name";
					name.fontSize = 11;
					name.padding = Clay_Padding{8, 8, 4, 4};
					name.sizing = Clay_Sizing{CLAY_SIZING_GROW(0), CLAY_SIZING_FIXED(28)};
					context.uiManager.createElement(FSEL::kTextInput, "clone-name")
						.setParameters(name)
						.setDevInternalCapture(true)
						.draw();
					button(
						context, "clone", "Clone Variant",
						ActionCall{app->actions().uiActions().make(kCloneTheme, context.state())},
						true, action_icon(app, interface_icons::kCopyKey));
				}
			}
			auto tabs = section(38, interface_theme::kDepth2Ink);
			tabs.layout.padding = {};
			tabs.layout.childGap = 0;
			tabs.layout.layoutDirection = CLAY_LEFT_TO_RIGHT;
			CLAY(context.clayID("tabs"), tabs) {
				for (uint64_t tab = 0; tab < 2; ++tab) {
					FSEL::RadioChoiceParameters parameters{};
					parameters.choiceValue = tab;
					parameters.selectedValue = &state->inspect_theme_tab;
					parameters.style.sizing = {CLAY_SIZING_GROW(0), CLAY_SIZING_GROW(0)};
					parameters.style.padding = Clay_Padding{5, 5, 0, 0};
					parameters.style.childAlignment = {CLAY_ALIGN_X_CENTER, CLAY_ALIGN_Y_CENTER};
					parameters.style.borderWidth = {0, 0, 0, 2, 0};
					parameters.style.cornerRadius = {};
					parameters.style.idleOverrides.backgroundColor = interface_theme::kDepth2Ink;
					parameters.style.idleOverrides.borderColor = interface_theme::kDepth2Ink;
					parameters.style.hoveredOverrides.backgroundColor =
						interface_theme::kHoverSurface;
					parameters.style.hoveredOverrides.borderColor = interface_theme::kHoverSurface;
					parameters.style.pressedOverrides.backgroundColor =
						interface_theme::kSelectedRow;
					parameters.style.pressedOverrides.borderColor = interface_theme::kAccentCurrent;
					parameters.style.selectedOverrides.backgroundColor =
						interface_theme::kDepth1Panel;
					parameters.style.selectedOverrides.borderColor =
						interface_theme::kAccentCurrent;
					context.uiManager
						.createElement(FSEL::kRadioChoice,
									   RuntimeName("tab-" + std::to_string(tab)))
						.setParameters(parameters)
						.setDevInternalCapture(true)
						.construct();
					text(context.uiManager, tab == 0 ? "Tokens & Styles" : "Changes", 10,
						 state->inspect_theme_tab == tab ? interface_theme::kTextCanvas
														 : interface_theme::kTextSecondary);
					context.uiManager.drawConstructed();
				}
			}
			if (state->inspect_theme_group) {
				CLAY(context.clayID("group-filter"), section()) {
					button(context, "show-all", "Show all token groups",
						   ActionCall{app->actions().uiActions().make(kClearGroup, *state)});
				}
			}
			auto scroll = scroll_surface(context.clayID("fields"));
			CLAY(context.clayID("fields"), scroll) {
				auto cards = column();
				cards.layout.padding = CLAY_PADDING_ALL(9);
				cards.layout.childGap = 7;
				CLAY(context.clayID("cards"), cards) {
					const auto* root = schema->findTheme(selected->type);
					if (!root)
						text(context.uiManager, "This theme has no reflected schema");
					else if (state->inspect_theme_tab == 0) {
						for (const auto& field : schema->fieldsOf(root->themeType)) {
							if (state->inspect_theme_group &&
								state->inspect_theme_group != field.id)
								continue;
							const auto* operations =
								field.operations < schema->fieldOperations.size()
									? schema->fieldOperations[field.operations]
									: nullptr;
							const void* value = operations && operations->constAddress
													? operations->constAddress(selected->payload)
													: nullptr;
							std::string label(schema->string(field.displayName));
							if (state->inspect_theme_all_variants &&
								model.mixed(selected->type, field.id))
								label += " · Mixed";
							context.uiManager
								.createElement(
									kDevEditorCard,
									RuntimeName("field-" + std::to_string(field.id) + "-" +
												std::to_string(state->inspect_theme_key)))
								.setParameters(DevEditorCardParameters{
									.schema = schema,
									.binding =
										DevEditorBinding{
											.elementName = std::string(selected->typeName),
											.theme_variant = state->inspect_theme_all_variants
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
						const auto* presentation = model.find(*selected);
						for (const auto leaf_index : presentation->changed_indices) {
							const auto& leaf = presentation->fields[leaf_index];
							const auto& value = presentation->values[leaf_index];
							if (state->inspect_theme_group &&
								(leaf.key.nestedPath.empty()
									 ? leaf.key.field
									 : leaf.key.nestedPath.front()) != state->inspect_theme_group)
								continue;
							++count;
							const auto index = field_index(*schema, *leaf.field);
							CLAY(context.clayID(RuntimeName(leaf.path)), card()) {
								text(context.uiManager, leaf.path, 12,
									 interface_theme::kTextCanvas);
								text(context.uiManager, "Authored  " + value.authored_text);
								text(context.uiManager, "Baked  " + value.startup_text);
								text(context.uiManager, "Live  " + value.live_text, 11,
									 interface_theme::kAccentSeaGlass);
								CLAY(context.clayID(RuntimeName(leaf.path + "-actions")), row()) {
									button(
										context, "revert-" + leaf.path, "Revert Live",
										ActionCall{app->actions().uiActions().make(
											kThemeFieldAction, context.state(), index, uint8_t{0})},
										true, action_icon(app, interface_icons::kRevertKey));
									button(
										context, "authored-" + leaf.path, "Authored",
										ActionCall{app->actions().uiActions().make(
											kThemeFieldAction, context.state(), index, uint8_t{1})},
										!state->inspect_theme_all_variants);
									button(
										context, "broadcast-" + leaf.path, "All Variants",
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
		} else {
			auto empty = column();
			empty.layout.sizing.height = CLAY_SIZING_GROW(0);
			empty.layout.childAlignment = {CLAY_ALIGN_X_CENTER, CLAY_ALIGN_Y_CENTER};
			CLAY(context.clayID("empty-selection"), empty) {
				text(context.uiManager, "Select a theme from the selector", 12,
					 interface_theme::kTextMuted);
			}
		}
	}
}

} // namespace FlowUi::devSystems::interface_elements
#endif
