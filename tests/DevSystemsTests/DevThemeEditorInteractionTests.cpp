#ifdef NDEBUG
#undef NDEBUG
#endif
#include "FSEL/Theme.hpp"
#include "FlowUi/Flow.hpp"
#include "devSystems/devInterface/Inspect/Inspector/DevInspectInspector.hpp"
#include "devSystems/devInterface/Inspect/Inspector/TypeEditorElements/DevTypeEditorElements.hpp"
#include "devSystems/devInterface/Inspect/Selector/DevInspectSelector.hpp"
#include "devSystems/devInterface/Inspect/Selector/DevInspectSelectorElements.hpp"
#include "devSystems/devInterface/Inspect/Themes/DevThemeEditor.hpp"
#include "devSystems/devInterface/Inspect/Workbench/DevInspectWorkbench.hpp"
#include "devSystems/devTooling/DevTooling.hpp"
#include <cassert>
#include <filesystem>
#include <iostream>

using namespace FlowUi;
using namespace FlowUi::devSystems;
using namespace FlowUi::devSystems::interface_elements;
namespace {
struct BrandTheme {
	std::string name = "Owned string baseline";
	float radius = 8;
	bool enabled = false;
};
FLOWUI_DEV_SCHEMA(BrandTheme, FLOWUI_DEV_FIELD(BrandTheme, name),
				  FLOWUI_DEV_FIELD(BrandTheme, radius), FLOWUI_DEV_FIELD(BrandTheme, enabled))
constexpr auto selector_id = Global<kDevInspectSelector>("theme-test.selector");
constexpr auto inspector_id = Global<kDevInspectInspector>("theme-test.inspector");
constexpr auto workbench_id = Global<kDevInspectWorkbench>("theme-test.workbench");
} // namespace
int main() {
	AppConfig config{};
	config.window.title = "Theme editor verification";
	config.window.width = 1440;
	config.window.height = 940;
	config.dev.excludeInternalDevElementsFromCapture = false;
	config.vk.enableValidation = true;
	auto app = makeApplication(config);
	DevInterfaceState session{};
	session.inspectForest = kDevInterfaceFlowForest;
	assert(app.themes().registerTheme<BrandTheme>("dark", BrandTheme{}, true));
	assert(app.themes().registerTheme<BrandTheme>("light", BrandTheme{"Light variant", 14, false},
												  false));
	assert(app.themes().registerTheme<FSEL::FSELTheme>("light", FSEL::FSELTheme{}, false));
	auto& pipeline = app.devTooling().bakePipeline();
	const auto manifest =
		std::filesystem::temp_directory_path() /
		("flowui-theme-test-" + std::to_string(reinterpret_cast<std::uintptr_t>(&app))) /
		"active.flowchanges";
	pipeline.setManifestPath(manifest);
	uint64_t frames = 0;
	const auto draw = [&](FlowElementID control = {}, int event = 0, float inspector_width = 340) {
		assert(app.beginFrame());
		auto& ui = app.ui();
		auto& interaction = const_cast<InteractionSnapshot&>(ui.getPreviousFramesInteraction());
		interaction = {};
		auto& input = const_cast<FrameInput&>(ui.getCurrentFrameInput());
		input.mouseX = -1;
		input.mouseY = -1;
		input.mouseDown = {};
		input.mouseDown[0] = event == 1;
		input.windowFocused = true;
		if (control) {
			const auto element = ui.toClayEID(control);
			const auto bounds = Clay_GetElementData(element).boundingBox;
			input.mouseX = bounds.x + bounds.width / 2;
			input.mouseY = bounds.y + bounds.height / 2;
			interaction.hoveredElementIds.emplace_back(element.id);
			if (event == 1)
				interaction.pressedElementIds.emplace_back(element.id);
			if (event == 2)
				interaction.releasedElementIds.emplace_back(element.id);
		}
		Clay_ElementDeclaration root{};
		root.layout.sizing = {CLAY_SIZING_FIXED(1440), CLAY_SIZING_FIXED(940)};
		root.backgroundColor = Flow_Color("#071A22");
		CLAY(CLAY_ID("theme-test-root"), root) {
			Clay_ElementDeclaration selector{};
			selector.layout.sizing = {CLAY_SIZING_FIXED(300), CLAY_SIZING_GROW(0)};
			selector.clip = {true, true};
			CLAY(CLAY_ID("theme-test-selector"), selector)
			ui.createElement(kDevInspectSelector, selector_id)
				.setParameters({&app, &session})
				.draw();
			Clay_ElementDeclaration workbench{};
			workbench.layout.sizing = {CLAY_SIZING_GROW(0), CLAY_SIZING_GROW(0)};
			workbench.clip = {true, true};
			CLAY(CLAY_ID("theme-test-workbench"), workbench)
			ui.createElement(kDevInspectWorkbench, workbench_id)
				.setParameters({&app, &session})
				.draw();
			Clay_ElementDeclaration inspector{};
			inspector.layout.sizing = {CLAY_SIZING_FIXED(inspector_width), CLAY_SIZING_GROW(0)};
			inspector.clip = {true, true};
			CLAY(CLAY_ID("theme-test-inspector"), inspector)
			ui.createElement(kDevInspectInspector, inspector_id)
				.setParameters({&app, &session})
				.draw();
		}
		assert(app.endFrame());
		assert(app.drawFrame());
		++frames;
	};
	const auto click = [&](FlowElementID control) {
		draw(control, 1);
		draw(control, 2);
		draw();
	};
	draw();
	draw();
	const auto controls = ::FlowUi::detail::element_id::resolveLocal(
		FlowElementID{selector_id.value}, DevInterfaceSelectorControls::definitionId,
		LocalElementName{"controls"}.token);
	const auto themes_choice = ::FlowUi::detail::element_id::resolveLocal(
		controls, FSEL::RadioChoice::definitionId, LocalElementName{"themes-choice"}.token);
	click(themes_choice);
	assert(session.inspectForest == kDevInterfaceThemeForest);
	auto schema = app.devTooling().schemas().view();
	const auto theme_type = ::FlowUi::detail::typeHash<BrandTheme>();
	const auto* brand = schema->findTheme(theme_type);
	assert(brand);
	const auto key = [&](devMode::DevTypeId type, std::initializer_list<std::string_view> names) {
		const auto* theme = schema->findTheme(type);
		assert(theme);
		auto owner = theme->themeType;
		tooling::DevOverrideFieldKey result{type, 0, {}};
		std::size_t remaining = names.size();
		for (const auto name : names) {
			const auto fields = schema->fieldsOf(owner);
			const auto found = std::ranges::find_if(
				fields, [&](const auto& field) { return schema->string(field.name) == name; });
			assert(found != fields.end());
			if (--remaining) {
				result.nestedPath.emplace_back(found->id);
				owner = found->valueType;
			} else
				result.field = found->id;
		}
		return result;
	};
	const auto radius = key(theme_type, {"radius"});
	const auto submit = [&](tooling::DevOverrideCommandKind kind, std::string variant,
							const tooling::DevOverrideFieldKey& field, const auto& value,
							bool preview_clear = false) {
		tooling::DevOverrideCommand command{};
		command.kind = kind;
		command.theme = {field.ownerType, std::move(variant)};
		command.field = field;
		if (kind != tooling::DevOverrideCommandKind::ClearThemeField &&
			kind != tooling::DevOverrideCommandKind::ResetThemeField)
			assert(app.devTooling().overrides().copyValue(value, command.value) ==
				   devMode::DevValueOperationStatus::Success);
		if (preview_clear)
			command.layer = tooling::DevOverrideLayer::EphemeralPreview;
		tooling::DevChangeSet changes{};
		changes.transaction = session.nextEditTransaction++;
		changes.commands.emplace_back(std::move(command));
		assert(app.devTooling().overrides().submit(std::move(changes)));
		draw();
		assert(!app.devTooling().overrides().commandResults().empty());
		assert(app.devTooling().overrides().commandResults().back().applied);
	};
	using Kind = tooling::DevOverrideCommandKind;
	submit(Kind::SetThemeField, "*", radius, 20.0f);
	assert(app.themes().getTheme<BrandTheme>("dark").radius == 20);
	assert(app.themes().getTheme<BrandTheme>("light").radius == 20);
	submit(Kind::SetThemeField, "light", radius, 30.0f);
	submit(Kind::SetThemeField, "*", radius, 22.0f);
	assert(app.themes().getTheme<BrandTheme>("dark").radius == 22);
	assert(app.themes().getTheme<BrandTheme>("light").radius == 30);
	submit(Kind::BeginBatchDrag, "dark", radius, 35.0f);
	submit(Kind::UpdateBatchDrag, "dark", radius, 36.0f);
	assert(app.themes().getTheme<BrandTheme>("dark").radius == 36);
	for (const auto& record : app.devTooling().overrides().themeBakeRecords())
		if (record.target.variant == "dark")
			assert(record.preview_only);
	submit(Kind::ClearThemeField, "dark", radius, 0.0f, true);
	assert(app.themes().getTheme<BrandTheme>("dark").radius == 22);
	submit(Kind::BeginBatchDrag, "light", radius, 37.0f);
	submit(Kind::EndBatchDrag, "light", radius, 38.0f);
	assert(app.themes().getTheme<BrandTheme>("light").radius == 38);
	submit(Kind::BeginBatchDrag, "light", radius, 40.0f);
	submit(Kind::ClearThemeField, "light", radius, 0.0f, true);
	assert(app.themes().getTheme<BrandTheme>("light").radius == 38);
	assert(app.themes().clone_dev_variant(theme_type, "dark", "cloned"));
	draw();
	assert(!app.themes().clone_dev_variant(theme_type, "dark", "cloned"));
	assert(!app.themes().set_dev_active_variant(theme_type, "missing"));
	assert(app.themes().set_dev_active_variant(theme_type, "light"));
	assert(app.themes().getActiveTheme<BrandTheme>().radius == 38);
	assert(
		app.themes().registerTheme<BrandTheme>("future", BrandTheme{"Future", 70, false}, false));
	draw();
	assert(app.themes().getTheme<BrandTheme>("future").radius == 22);
	submit(Kind::ClearThemeField, "*", radius, 0.0f);
	assert(app.themes().getTheme<BrandTheme>("dark").radius == 8);
	assert(app.themes().getTheme<BrandTheme>("light").radius == 38);
	assert(app.themes().getTheme<BrandTheme>("cloned").radius == 22);
	assert(app.themes().getTheme<BrandTheme>("future").radius == 70);
	const auto name = key(theme_type, {"name"});
	submit(Kind::SetThemeField, "dark", name, std::string("Changed owned value"));
	submit(Kind::ClearThemeField, "dark", name, std::string{});
	assert(app.themes().getTheme<BrandTheme>("dark").name == "Owned string baseline");
	const auto fsel_type = ::FlowUi::detail::typeHash<FSEL::FSELTheme>();
	const auto color_key = key(fsel_type, {"buttonTheme", "idle", "backgroundColor"});
	submit(Kind::SetThemeField, "default", color_key, Flow_Color("#d96c35"));
	session.inspect_theme_type = fsel_type;
	session.inspect_theme_variant = "default";
	session.inspect_theme_group = color_key.nestedPath.front();
	session.inspect_theme_key = 0;
	session.inspect_theme_all_variants = false;
	draw();
	draw();
	const auto theme_inspector = ::FlowUi::detail::element_id::resolveLocal(
		FlowElementID{inspector_id.value}, DevThemeInspector::definitionId,
		LocalElementName{"themes"}.token);
	const auto scope_button = ::FlowUi::detail::element_id::resolveLocal(
		theme_inspector, FSEL::Button::definitionId, LocalElementName{"scope"}.token);
	const auto scope_bounds = Clay_GetElementData(app.ui().toClayEID(scope_button)).boundingBox;
	assert(scope_bounds.width > 60 && scope_bounds.height >= 28);
	const auto theme_workbench = ::FlowUi::detail::element_id::resolveLocal(
		FlowElementID{workbench_id.value}, DevThemeWorkbench::definitionId,
		LocalElementName{"themes"}.token);
	const auto sample_button = ::FlowUi::detail::element_id::resolveLocal(
		theme_workbench, FSEL::Button::definitionId, RuntimeName("sample-button-0").token);
	const auto sample_bounds = Clay_GetElementData(app.ui().toClayEID(sample_button)).boundingBox;
	assert(sample_bounds.width > 100 && sample_bounds.height > 20);
	std::cout << "Theme review frame=" << frames - 1 << " phase=fsel-group\n";
	session.inspect_theme_tab = 1;
	draw();
	draw();
	std::cout << "Theme review frame=" << frames - 1 << " phase=changes\n";
	assert(pipeline.bake().applied);
	bool saw_fsel = false;
	for (const auto& entry : pipeline.manifest().entries)
		if (entry.themeType == fsel_type) {
			saw_fsel = true;
			assert(entry.fieldPath == "buttonTheme.idle.backgroundColor");
			assert(!entry.cppValue.empty());
			assert(entry.sourceHeader.ends_with("Theme.hpp"));
		}
	assert(saw_fsel);
	submit(Kind::ResetThemeField, "default", color_key, Clay_Color{});
	assert(pipeline.bake().applied);
	for (const auto& entry : pipeline.manifest().entries)
		assert(entry.themeType != fsel_type);
	submit(Kind::SetThemeField, "*", color_key, Flow_Color("#26a69a"));
	assert(pipeline.bake().applied);
	bool saw_general = false;
	for (const auto& entry : pipeline.manifest().entries)
		if (entry.themeType == fsel_type) {
			saw_general = true;
			assert(entry.themeVariant == "*");
		}
	assert(saw_general);
	session.inspect_theme_type = theme_type;
	session.inspect_theme_variant = "dark";
	session.inspect_theme_group = 0;
	session.inspect_theme_tab = 0;
	draw();
	draw();
	// Drive the shared field-card header and boolean editor through real retained callbacks.
	const auto inspector = ::FlowUi::detail::element_id::resolveLocal(
		FlowElementID{inspector_id.value}, DevThemeInspector::definitionId,
		LocalElementName{"themes"}.token);
	const auto enabled = key(theme_type, {"enabled"});
	const auto field_id = ::FlowUi::detail::element_id::resolveLocal(
		inspector, DevEditorCard::definitionId,
		RuntimeName("field-" + std::to_string(enabled.field) + "-0").token);
	const auto header = ::FlowUi::detail::element_id::resolveLocal(
		field_id, DevEditorCardHeader::definitionId, LocalElementName{"header"}.token);
	click(header);
	bool expanded = false;
	for (const auto& node : app.ui().devTreeSnapshot().flow.nodes)
		if (node.definition == DevBooleanTypeEditor::definitionId)
			expanded = true;
	assert(expanded);
	FlowElementID toggle{};
	for (const auto& node : app.ui().devTreeSnapshot().flow.nodes)
		if (node.definition == DevBooleanTypeEditor::definitionId)
			toggle = ::FlowUi::detail::element_id::resolveLocal(FlowElementID{node.instance.value},
																FSEL::Switch::definitionId,
																LocalElementName{"value"}.token);
	assert(toggle);
	click(toggle);
	assert(app.themes().getTheme<BrandTheme>("dark").enabled);
	assert(session.editUndoStack.size() == 1);
	const auto transaction = session.editUndoStack.back();
	assert(
		app.devTooling().overrides().submit({session.nextEditTransaction++, transaction.inverse}));
	draw();
	assert(!app.themes().getTheme<BrandTheme>("dark").enabled);
	assert(
		app.devTooling().overrides().submit({session.nextEditTransaction++, transaction.forward}));
	draw();
	assert(app.themes().getTheme<BrandTheme>("dark").enabled);
	session.inspect_theme_all_variants = true;
	draw();
	click(toggle);
	assert(app.themes().getTheme<BrandTheme>("light").enabled);
	assert(app.themes().getTheme<BrandTheme>("dark").enabled);
	const auto general_transaction = session.editUndoStack.back();
	assert(app.devTooling().overrides().submit(
		{session.nextEditTransaction++, general_transaction.inverse}));
	draw();
	assert(!app.themes().getTheme<BrandTheme>("light").enabled);
	assert(app.themes().getTheme<BrandTheme>("dark").enabled);
	std::cout << "Theme review frame=" << frames - 1 << " phase=custom-editor\n";
	// Test a narrower inspector and the empty-selection route without leaking Clay parents.
	draw({}, 0, 260);
	session.inspect_theme_type = 0;
	draw();
	draw();
	std::cout << "Theme review frame=" << frames - 1 << " phase=empty\n";
	std::filesystem::remove_all(manifest.parent_path());
	std::cout << "Theme editor verification passed in " << frames << " frames\n";
}
