#pragma once
#include "FlowUi/Public/BuildConfig.hpp"
#if FLOW_UI_DEV_MODE
#include "FSEL/Button.hpp"
#include "devSystems/devInterface/Inspect/DevThemeEditorModel.hpp"
#include "devSystems/devInterface/Permanents/Backend/DevInterfaceIcons.hpp"
#include "devSystems/devInterface/Permanents/Backend/DevInterfaceState.hpp"
#include "devSystems/devInterface/Permanents/Backend/DevTheme.hpp"
#include "devSystems/devInterface/Permanents/Elements/DevActionButton.hpp"
#include "devSystems/devTooling/DevTooling.hpp"
#include "FlowUi/Public/managers/ThemeManager.hpp"
#include "FlowUi/Public/managers/UiManager.hpp"
#include <algorithm>
#include <cctype>

namespace FlowUi::devSystems::interface_elements::theme_editor {
[[nodiscard]] inline std::vector<ThemeView> themes(App& app) {
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
[[nodiscard]] inline const ThemeView* selected_theme(std::span<const ThemeView> catalogue,
													 const DevInterfaceState& state) noexcept {
	for (const auto& theme : catalogue)
		if (theme.type == state.inspect_theme_type &&
			(state.inspect_theme_all_variants ? theme.active
											  : theme.variant == state.inspect_theme_variant))
			return &theme;
	return nullptr;
}
[[nodiscard]] inline tooling::DevFieldIndex
field_index(const tooling::DevSchemaGeneration& schema,
			const tooling::DevFieldSchema& field) noexcept {
	return {static_cast<uint32_t>(&field - schema.fields.data() + 1)};
}
[[nodiscard]] inline std::string format_value(const tooling::DevSchemaGeneration& schema,
											  const ThemeLeaf& leaf, const void* value,
											  bool cpp = false) {
	const auto formatted = tooling::serialize_dev_value(schema, leaf.field->valueType, value);
	if (formatted)
		return cpp ? formatted->cpp : formatted->json;
	const auto* type = schema.type(leaf.field->valueType);
	const auto* operations = leaf.field->valueType.value < schema.typeOperations.size()
								 ? schema.typeOperations[leaf.field->valueType.value]
								 : nullptr;
	if (!cpp && value && type && type->kind == tooling::DevTypeKind::Text && operations &&
		operations->textView)
		return std::string(operations->textView(value));
	return "Unavailable";
}
inline void collect_leaves(const tooling::DevSchemaGeneration& schema, tooling::DevTypeIndex owner,
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
		if (type->kind == tooling::DevTypeKind::Object &&
			type->editor == tooling::DevEditorKind::ObjectGroup) {
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
[[nodiscard]] inline std::vector<ThemeLeaf> leaves(const tooling::DevSchemaGeneration& schema,
												   const ThemeView& theme) {
	std::vector<ThemeLeaf> result;
	result.reserve(256);
	const auto* root = schema.findTheme(theme.type);
	if (root)
		collect_leaves(schema, root->themeType, theme.payload, theme.authored, theme.startup,
					   {theme.type, 0, {}}, "", result);
	return result;
}
[[nodiscard]] inline bool changed(const tooling::DevSchemaGeneration& schema,
								  const ThemeLeaf& leaf) noexcept {
	return compare_values(schema, leaf.field->valueType, leaf.live, leaf.authored) !=
			   ValueComparison::Equal ||
		   compare_values(schema, leaf.field->valueType, leaf.startup, leaf.authored) !=
			   ValueComparison::Equal;
}
[[nodiscard]] inline bool contains(std::string_view text, std::string_view query) {
	return std::search(text.begin(), text.end(), query.begin(), query.end(),
					   [](unsigned char left, unsigned char right) {
						   return std::tolower(left) == std::tolower(right);
					   }) != text.end();
}
[[nodiscard]] inline Clay_ElementDeclaration column(bool bounded = false) noexcept {
	Clay_ElementDeclaration declaration{};
	declaration.layout.sizing = bounded
									? Clay_Sizing{CLAY_SIZING_PERCENT(1), CLAY_SIZING_PERCENT(1)}
									: Clay_Sizing{CLAY_SIZING_GROW(0), CLAY_SIZING_FIT(0)};
	declaration.layout.layoutDirection = CLAY_TOP_TO_BOTTOM;
	declaration.layout.childGap = bounded ? 0 : 8;
	declaration.backgroundColor = interface_theme::kDepth1Panel;
	return declaration;
}
[[nodiscard]] inline Clay_ElementDeclaration card() noexcept {
	auto declaration = column();
	declaration.layout.padding = CLAY_PADDING_ALL(12);
	declaration.backgroundColor = interface_theme::kDepth2Ink;
	declaration.cornerRadius = CLAY_CORNER_RADIUS(6);
	declaration.border = {interface_theme::kBorderVisible, {1, 1, 1, 1, 0}};
	return declaration;
}
inline void text(UiManager& ui, std::string_view label, uint16_t size = 11,
				 Clay_Color color = interface_theme::kTextSecondary) {
	Clay_TextElementConfig style{};
	style.fontSize = size;
	style.textColor = color;
	style.wrapMode = CLAY_TEXT_WRAP_WORDS;
	CLAY_TEXT(ui.toClayString(label), CLAY_TEXT_CONFIG(style));
}
[[nodiscard]] inline TextureRef action_icon([[maybe_unused]] App* app,
											[[maybe_unused]] std::string_view key) {
#if FLOWUI_INCLUDE_ICON_MANAGER
	if (app) {
		auto& icons = app->icons();
		if (!icons.contains(key))
			interface_icons::registerDevInterfaceIcons(icons);
		if (icons.contains(key))
			return icons.textureRef(key);
	}
#endif
	return {};
}
template <class Context>
inline void button(Context& context, std::string_view id, std::string_view label, ActionCall action,
				   bool enabled = true, TextureRef icon = {}) {
	auto& ui = context.uiManager;
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
	action_button::set_icon(parameters, icon);
	ui.createElement(FSEL::kButton, RuntimeName(id))
		.setParameters(parameters)
		.setDevInternalCapture(true)
		.draw();
	if (icon.handle)
		action_button::tooltip(context, RuntimeName(id), label);
}
[[nodiscard]] inline Clay_ElementDeclaration row() noexcept {
	auto declaration = column();
	declaration.layout.layoutDirection = CLAY_LEFT_TO_RIGHT;
	declaration.layout.childAlignment.y = CLAY_ALIGN_Y_CENTER;
	declaration.backgroundColor = {};
	return declaration;
}
[[nodiscard]] inline std::string cpp_string(std::string_view value) {
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
/* A complete horizontal section, sharing the inspector's hairlines and spacing. */
[[nodiscard]] inline Clay_ElementDeclaration
section(float height = 0, Clay_Color background = interface_theme::kDepth1Panel) noexcept {
	auto declaration = column();
	declaration.layout.sizing.height = height > 0 ? CLAY_SIZING_FIXED(height) : CLAY_SIZING_FIT(0);
	declaration.layout.padding = Clay_Padding{12, 12, 8, 8};
	declaration.layout.childGap = 6;
	declaration.backgroundColor = background;
	declaration.border = {interface_theme::kBorderPrimary, {0, 0, 0, 1, 0}};
	return declaration;
}

/* Match the existing bounded scroll surfaces, including their retained child offset. */
[[nodiscard]] inline Clay_ElementDeclaration scroll_surface(Clay_ElementId identity) noexcept {
	auto declaration = column();
	declaration.layout.sizing.height = CLAY_SIZING_GROW(0);
	declaration.layout.childGap = 0;
	const auto scroll = Clay_GetScrollContainerData(identity);
	declaration.clip = {.horizontal = true,
						.vertical = true,
						.childOffset = scroll.found && scroll.scrollPosition
										   ? *scroll.scrollPosition
										   : Clay_Vector2{}};
	return declaration;
}

} // namespace FlowUi::devSystems::interface_elements::theme_editor
#endif
