#include "devSystems/devInterface/Inspect/Workbench/DevThemeWorkbench.hpp"
#include "devSystems/devInterface/Inspect/DevThemeEditorData.hpp"
#if FLOW_UI_DEV_MODE
#include "FSEL/Checkbox.hpp"
#include "FSEL/Label.hpp"
#include "FSEL/ProgressBar.hpp"
#include "FSEL/Slider.hpp"
#include "FSEL/Switch.hpp"
#include "FSEL/TextInput.hpp"
#include "devSystems/devInterface/Inspect/Workbench/DevInspectWorkbench.hpp"
#include <array>
#include <cmath>
#include <cstdio>
namespace FlowUi::devSystems::interface_elements {
using namespace theme_editor;
namespace {
/** Compact typed values; raw serialization remains in the generated code. */
[[nodiscard]] std::string token_value(const devMode::DevSchemaGeneration& schema,
									  const ThemeLeaf& leaf, const void* value) {
	const auto* type = schema.type(leaf.field->valueType);
	if (value && type && type->id == ::FlowUi::detail::typeHash<Clay_Color>()) {
		const auto color = *static_cast<const Clay_Color*>(value);
		const auto channel = [](float component) noexcept {
			return std::isfinite(component)
					   ? static_cast<unsigned>(std::clamp(std::round(component), 0.0f, 255.0f))
					   : 0u;
		};
		std::array<char, 32> buffer{};
		std::snprintf(buffer.data(), buffer.size(), "#%02X%02X%02X", channel(color.r),
					  channel(color.g), channel(color.b));
		return std::string(buffer.data()) +
			   (channel(color.a) == 255 ? "" : " · A " + std::to_string(channel(color.a)));
	}
	if (value && type && type->id == ::FlowUi::detail::typeHash<Clay_Padding>()) {
		const auto padding = *static_cast<const Clay_Padding*>(value);
		return "L " + std::to_string(padding.left) + " · R " + std::to_string(padding.right) +
			   " · T " + std::to_string(padding.top) + " · B " + std::to_string(padding.bottom);
	}
	if (value && type && type->kind == devMode::DevTypeKind::Enumeration &&
		type->enumeration.values.first <= schema.enumValues.size() &&
		type->enumeration.values.count <=
			schema.enumValues.size() - type->enumeration.values.first) {
		const auto* operations = leaf.field->valueType.value < schema.typeOperations.size()
									 ? schema.typeOperations[leaf.field->valueType.value]
									 : nullptr;
		long double numeric = 0;
		if (operations && operations->numericValue && operations->numericValue(value, numeric)) {
			for (const auto& option :
				 std::span(schema.enumValues)
					 .subspan(type->enumeration.values.first, type->enumeration.values.count)) {
				const auto option_value =
					type->enumeration.isSigned
						? static_cast<long double>(static_cast<int64_t>(option.bits))
						: static_cast<long double>(option.bits);
				if (numeric == option_value)
					return std::string(schema.string(option.name));
			}
		}
	}
	return format_value(schema, leaf, value);
}
constexpr auto kCopyThemeCode =
	UiAction("flowui.dev_interface.theme.copy-code", [](App& app, DevThemeWorkbench::State& state) {
		app.setClipboardText(state.code);
		state.copy_feedback = "Copied C++";
	});
constexpr auto kToggleSample =
	UiAction("flowui.dev_interface.theme.toggle-sample", [](bool& value) { value = !value; });

template <typename Content>
void workbench_card(DevThemeWorkbench::BuildContext& context, std::string_view name,
					std::string_view title, const Content& content,
					const std::function<void()>& toolbar = {}, bool preview = false) {
	auto surface = column();
	surface.layout.sizing = {CLAY_SIZING_PERCENT(.5f), CLAY_SIZING_PERCENT(1)};
	surface.layout.childGap = 0;
	surface.cornerRadius = CLAY_CORNER_RADIUS(5);
	surface.border = {interface_theme::kBorderPrimary, {1, 1, 1, 1, 0}};
	surface.clip = {.horizontal = true, .vertical = true, .scrollInputDisabled = true};
	CLAY(context.clayID(RuntimeName(name)), surface) {
		// The clipping card must contain a percentage-bounded frame; a growing
		// child would expand to the content height instead of forming a viewport.
		CLAY(context.clayID(RuntimeName(std::string(name) + "-frame")), column(true)) {
			auto header = section(34, interface_theme::kDepth2Ink);
			header.layout.padding = Clay_Padding{10, 10, 0, 0};
			header.layout.childAlignment.y = CLAY_ALIGN_Y_CENTER;
			CLAY(context.clayID(RuntimeName(std::string(name) + "-header")), header) {
				text(context.uiManager, title, 10, interface_theme::kTextCanvas);
			}
			if (toolbar) {
				CLAY(context.clayID(RuntimeName(std::string(name) + "-toolbar")), section()) {
					toolbar();
				}
			}
			const auto scroll_id = context.clayID(RuntimeName(std::string(name) + "-scroll"));
			auto viewport = scroll_surface(scroll_id);
			viewport.clip.scrollInputDisabled = preview;
			CLAY(scroll_id, viewport) {
				auto body = column();
				body.layout.padding = CLAY_PADDING_ALL(8);
				body.layout.childGap = 7;
				if (preview)
					body.layout.sizing.height = CLAY_SIZING_PERCENT(1);
				CLAY(context.clayID(RuntimeName(std::string(name) + "-content")), body) {
					content();
				}
			}
		}
	}
}

} // namespace
void DevThemeWorkbench::buildElement(BuildContext& context) {
	auto* app = context.params.app;
	auto* state = context.params.interfaceState;
	if (!app || !state)
		return;
	auto& sandbox = context.state();
	const auto schema = app->devTooling().schemas().view();
	const auto catalogue = themes(*app);
	const auto* selected = selected_theme(catalogue, *state);
	auto root = column(true);
	root.backgroundColor = interface_theme::kDepth0Keel;
	CLAY(context.clayID(), root) {
		auto header = section(44);
		header.layout.padding = Clay_Padding{14, 14, 0, 0};
		header.layout.layoutDirection = CLAY_LEFT_TO_RIGHT;
		header.layout.childAlignment.y = CLAY_ALIGN_Y_CENTER;
		header.clip = {.horizontal = true, .scrollInputDisabled = true};
		CLAY(context.clayID("header"), header) {
			text(context.uiManager,
				 selected ? (state->inspect_theme_all_variants ? std::string_view{"All Variants"}
															   : selected->variant)
						  : std::string_view{"No theme selected"},
				 14, interface_theme::kTextCanvas);
		}
		const std::string metadata =
			selected ? (std::to_string(selected->payloadSize) + " bytes · revision " +
						std::to_string(selected->revision) + (selected->active ? " · Active" : ""))
					 : "Select a theme to open its workbench";
		context.uiManager.createElement(kDevInspectWorkbenchSubtitle, "subtitle")
			.setParameters(DevInspectWorkbenchSubtitleParameters{
				selected ? selected->typeName : std::string_view{"Theme"}, metadata})
			.setDevInternalCapture(true)
			.draw();
		if (schema && selected) {
			const auto selected_leaves = leaves(*schema, *selected);
			struct VariantTokens {
				std::vector<ThemeLeaf> fields{};
				const ThemeView* theme = nullptr;
			};
			std::vector<VariantTokens> variant_tokens;
			variant_tokens.reserve(catalogue.size());
			for (const auto& variant : catalogue)
				if (variant.type == selected->type)
					variant_tokens.emplace_back(VariantTokens{leaves(*schema, variant), &variant});
			if (sandbox.preview_theme_type != selected->type) {
				sandbox.preview_theme_type = selected->type;
				sandbox.preview_layout = 0;
			}
			std::vector<const ThemeManager::DevThemePreview*> previews;
			previews.reserve(16);
			app->themes().visitDevPreviews(
				selected->type, [&](const auto& scene) { previews.emplace_back(&scene); });
			const bool builtin =
				previews.empty() && selected->type == ::FlowUi::detail::typeHash<FSEL::FSELTheme>();
			sandbox.preview_options.clear();
			sandbox.preview_options.reserve(previews.size() + 8);
			sandbox.preview_options.emplace_back(FSEL::ComboBoxOption{0, "Vertical layout"});
			sandbox.preview_options.emplace_back(FSEL::ComboBoxOption{1, "Horizontal layout"});
			if (builtin) {
				for (auto label :
					 {"FSEL Button", "FSEL Button (disabled)", "FSEL Checkbox", "FSEL Switch",
					  "FSEL Slider", "FSEL ProgressBar", "FSEL TextInput", "FSEL Label"})
					sandbox.preview_options.emplace_back(
						FSEL::ComboBoxOption{sandbox.preview_options.size(), label});
			} else {
				for (const auto* preview : previews)
					sandbox.preview_options.emplace_back(
						FSEL::ComboBoxOption{sandbox.preview_options.size(), preview->debug_name});
			}
			if (sandbox.preview_layout >= sandbox.preview_options.size())
				sandbox.preview_layout = 0;
			const auto preview_toolbar = [&] {
				FSEL::ComboBoxParameters layout{};
				layout.options = sandbox.preview_options;
				layout.selectedValue = &sandbox.preview_layout;
				layout.fontSize = 11;
				layout.sizing = Clay_Sizing{CLAY_SIZING_GROW(0), CLAY_SIZING_FIXED(30)};
				context.uiManager.createElement(FSEL::kComboBox, "preview-layout")
					.setParameters(layout)
					.setDevInternalCapture(true)
					.draw();
			};
			const auto draw_specimens = [&] {
				const auto active = std::ranges::find_if(catalogue, [&](const auto& variant) {
					return variant.type == selected->type && variant.active;
				});
				const bool has_preview = builtin || !previews.empty();
				const bool inactive = !state->inspect_theme_all_variants && !selected->active;
				if (!has_preview || inactive) {
					auto message = column();
					message.layout.sizing.height = CLAY_SIZING_GROW(0);
					message.layout.childAlignment = {CLAY_ALIGN_X_CENTER, CLAY_ALIGN_Y_CENTER};
					CLAY(context.clayID("preview-message"), message) {
						text(context.uiManager,
							 !has_preview
								 ? "No component preview registered for " +
									   std::string(selected->typeName) +
									   ". Register a preview scene to see your elements here. "
									   "Reflected values are available in Token Overview."
								 : "Editing " + std::string(selected->variant) +
									   "; registered scenes use " +
									   (active != catalogue.end() ? std::string(active->variant)
																  : "no active variant") +
									   ". Activate this variant in the Inspector to preview it.");
					}
					return;
				}
				text(context.uiManager,
					 std::string(selected->typeName) + " / " + std::string(selected->variant) +
						 (state->inspect_theme_all_variants ? " · Active variant preview"
															: " · Component preview"),
					 10);
				auto examples = column();
				if (sandbox.preview_layout == 1)
					examples.layout.sizing.width = CLAY_SIZING_FIT(0);
				examples.layout.layoutDirection =
					sandbox.preview_layout == 1 ? CLAY_LEFT_TO_RIGHT : CLAY_TOP_TO_BOTTOM;
				CLAY(context.clayID("preview-elements"), examples) {
					if (builtin) {
						if (sandbox.preview_layout < 2 || sandbox.preview_layout == 2 ||
							sandbox.preview_layout == 3)
							CLAY(context.clayID("buttons"), row()) {
								for (uint32_t specimen = 0; specimen < 2; ++specimen) {
									if (sandbox.preview_layout >= 2 &&
										sandbox.preview_layout != specimen + 2)
										continue;
									FSEL::ButtonParameters parameters{};
									parameters.text = specimen ? "Disabled" : "Interactive button";
									parameters.contentMode = FSEL::ButtonContentMode::TextOnly;
									parameters.enabled = specimen == 0;
									parameters.onActivate =
										ActionCall{app->actions().uiActions().make(
											kToggleSample, sandbox.sample_checked)};
									context.uiManager
										.createElement(FSEL::kButton,
													   RuntimeName("sample-button-" +
																   std::to_string(specimen)))
										.setParameters(parameters)
										.setDevInternalCapture(true)
										.draw();
								}
							}
						if (sandbox.preview_layout < 2 || sandbox.preview_layout == 4 ||
							sandbox.preview_layout == 5)
							CLAY(context.clayID("toggles"), row()) {
								if (sandbox.preview_layout < 2 || sandbox.preview_layout == 4) {
									FSEL::CheckboxParameters checkbox{};
									checkbox.isChecked = sandbox.sample_checked;
									checkbox.onToggle = ActionCall{app->actions().uiActions().make(
										kToggleSample, sandbox.sample_checked)};
									context.uiManager
										.createElement(FSEL::kCheckbox, "sample-checkbox")
										.setParameters(checkbox)
										.setDevInternalCapture(true)
										.draw();
								}
								if (sandbox.preview_layout < 2 || sandbox.preview_layout == 5) {
									FSEL::SwitchParameters toggle{};
									toggle.isOn = sandbox.sample_switch;
									toggle.onToggle = ActionCall{app->actions().uiActions().make(
										kToggleSample, sandbox.sample_switch)};
									context.uiManager.createElement(FSEL::kSwitch, "sample-switch")
										.setParameters(toggle)
										.setDevInternalCapture(true)
										.draw();
								}
							}
						if (sandbox.preview_layout < 2 || sandbox.preview_layout == 6) {
							FSEL::SliderParameters slider{};
							slider.value = &sandbox.sample_value;
							slider.length = 260;
							context.uiManager.createElement(FSEL::kSlider, "sample-slider")
								.setParameters(slider)
								.setDevInternalCapture(true)
								.draw();
						}
						if (sandbox.preview_layout < 2 || sandbox.preview_layout == 7) {
							FSEL::ProgressBarParameters progress{};
							progress.value = sandbox.sample_value;
							progress.sizing =
								Clay_Sizing{CLAY_SIZING_GROW(0), CLAY_SIZING_FIXED(8)};
							context.uiManager.createElement(FSEL::kProgressBar, "sample-progress")
								.setParameters(progress)
								.setDevInternalCapture(true)
								.draw();
						}
						if (sandbox.preview_layout < 2 || sandbox.preview_layout == 8) {
							FSEL::TextInputParameters input{};
							input.fontSize = 12;
							input.padding = Clay_Padding{8, 8, 4, 4};
							input.value = &sandbox.sample_text;
							input.sizing = Clay_Sizing{CLAY_SIZING_GROW(0), CLAY_SIZING_FIXED(34)};
							context.uiManager.createElement(FSEL::kTextInput, "sample-input")
								.setParameters(input)
								.setDevInternalCapture(true)
								.draw();
						}
						if (sandbox.preview_layout < 2 || sandbox.preview_layout == 9) {
							FSEL::LabelParameters label{};
							label.text = sandbox.sample_text;
							context.uiManager.createElement(FSEL::kLabel, "sample-label")
								.setParameters(label)
								.setDevInternalCapture(true)
								.draw();
						}
					}
					if (!builtin) {
						for (std::size_t preview_index = 0; preview_index < previews.size();
							 ++preview_index) {
							if (sandbox.preview_layout >= 2 &&
								sandbox.preview_layout != preview_index + 2)
								continue;
							auto preview_slot = column();
							if (sandbox.preview_layout == 1)
								preview_slot.layout.sizing.width = CLAY_SIZING_FIT(0);
							CLAY(context.clayID(
									 RuntimeName("preview-" + std::to_string(preview_index))),
								 preview_slot) {
								text(context.uiManager, previews[preview_index]->debug_name, 11,
									 interface_theme::kTextCanvas);
								try {
									previews[preview_index]->draw(*app, context.uiManager);
								} catch (...) {
									text(context.uiManager, "Preview callback failed", 11,
										 interface_theme::kStatusAmber);
								}
							}
						}
					}
				}
			};
			const auto matches = [&](const ThemeLeaf& leaf) {
				return (!state->inspect_theme_group ||
						(leaf.key.nestedPath.empty()
							 ? leaf.key.field
							 : leaf.key.nestedPath.front()) == state->inspect_theme_group) &&
					   (state->searchQuery.empty() || contains(leaf.path, state->searchQuery) ||
						contains(selected->typeName, state->searchQuery)) &&
					   (sandbox.token_search.empty() || contains(leaf.path, sandbox.token_search) ||
						contains(format_value(*schema, leaf, leaf.live), sandbox.token_search));
			};
			const auto token_toolbar = [&] {
				FSEL::TextInputParameters search{};
				search.value = &sandbox.token_search;
				search.placeholder = "Filter tokens or values...";
				search.sizing = Clay_Sizing{CLAY_SIZING_GROW(0), CLAY_SIZING_FIXED(30)};
				search.padding = Clay_Padding{8, 8, 4, 4};
				search.fontSize = 12;
				context.uiManager.createElement(FSEL::kTextInput, "token-search")
					.setParameters(search)
					.setDevInternalCapture(true)
					.draw();
			};
			const auto value_row = [&](std::string_view identity, std::string_view label,
									   const ThemeLeaf& leaf, const void* value,
									   bool accent = false) {
				auto line = row();
				line.layout.padding = Clay_Padding{4, 4, 5, 5};
				line.border = {interface_theme::kBorderPrimary, {0, 0, 0, 1, 0}};
				CLAY(context.clayID(RuntimeName(identity)), line) {
					auto caption = column();
					caption.layout.sizing.width = CLAY_SIZING_PERCENT(.32f);
					caption.backgroundColor = {};
					CLAY(context.clayID(RuntimeName(std::string(identity) + "-label")), caption) {
						text(context.uiManager, label, 10);
					}
					const auto* type = schema->type(leaf.field->valueType);
					if (value && type && type->id == ::FlowUi::detail::typeHash<Clay_Color>()) {
						Clay_ElementDeclaration swatch{};
						swatch.layout.sizing = {CLAY_SIZING_FIXED(18), CLAY_SIZING_FIXED(18)};
						swatch.backgroundColor = *static_cast<const Clay_Color*>(value);
						swatch.cornerRadius = CLAY_CORNER_RADIUS(3);
						CLAY(context.clayID(RuntimeName(std::string(identity) + "-swatch")),
							 swatch);
					}
					auto value_cell = column();
					value_cell.backgroundColor = {};
					CLAY(context.clayID(RuntimeName(std::string(identity) + "-value")),
						 value_cell) {
						text(context.uiManager, token_value(*schema, leaf, value), 11,
							 accent ? interface_theme::kAccentSeaGlass
									: interface_theme::kTextCanvas);
					}
				}
			};
			const auto draw_tokens = [&] {
				text(context.uiManager,
					 state->inspect_theme_all_variants
						 ? "Compare registered variants · active marked *"
						 : "Effective values · " + std::string(selected->variant),
					 10);
				std::size_t count = 0;
				for (const auto& leaf : selected_leaves) {
					if (!matches(leaf))
						continue;
					++count;
					CLAY(context.clayID(RuntimeName("token-" + leaf.path)), section()) {
						text(context.uiManager, leaf.path, 11, interface_theme::kTextCanvas);
						if (!state->inspect_theme_all_variants)
							value_row("effective-" + leaf.path, "Value", leaf, leaf.live);
						else
							for (const auto& variant : catalogue) {
								if (variant.type != selected->type)
									continue;
								const auto cached =
									std::ranges::find_if(variant_tokens, [&](const auto& entry) {
										return entry.theme == &variant;
									});
								const auto& tokens = cached->fields;
								const auto found =
									std::ranges::find_if(tokens, [&](const auto& token) {
										return token.key == leaf.key;
									});
								if (found != tokens.end())
									value_row(
										"compare-" + std::string(variant.variant) + "-" + leaf.path,
										std::string(variant.variant) + (variant.active ? " *" : ""),
										*found, found->live);
							}
					}
				}
				if (!count)
					text(context.uiManager, "No tokens match this filter");
			};
			const auto draw_code = [&] {
				sandbox.code.clear();
				std::size_t assignment_count = 0;
				std::size_t omitted_count = 0;
				text(context.uiManager,
					 std::string(selected->typeName) + " / " +
						 (state->inspect_theme_all_variants ? "All registered variants"
															: std::string(selected->variant)),
					 10);
				const bool source_usable_type =
					selected->typeName.find("{anonymous}") == std::string_view::npos &&
					selected->typeName.find('(') == std::string_view::npos;
				if (!source_usable_type)
					text(context.uiManager, "This local theme type has no exportable C++ name.");
				if (source_usable_type)
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
								if (value) {
									++assignment_count;
									sandbox.code +=
										"  theme." + leaf.path + " = " + value->cpp + ";\n";
								} else {
									++omitted_count;
									sandbox.code +=
										"  // Runtime-only value omitted: " + leaf.path + "\n";
								}
							}
						sandbox.code += "  app.themes().registerTheme(" +
										cpp_string(variant.variant) + ", theme, " +
										(variant.active ? "true" : "false") + ");\n}\n";
					}
				CLAY(context.clayID("code-actions"), row()) {
					button(
						context.uiManager, "copy", "Copy C++",
						ActionCall{app->actions().uiActions().make(kCopyThemeCode, *app, sandbox)},
						!sandbox.code.empty());
					if (!sandbox.copy_feedback.empty())
						text(context.uiManager, sandbox.copy_feedback, 10);
				}
				text(context.uiManager,
					 std::to_string(assignment_count) + " assignments · " +
						 std::to_string(omitted_count) + " omitted values",
					 10);
				if (!assignment_count)
					text(context.uiManager, "No exportable changes");
				if (omitted_count)
					text(context.uiManager,
						 "Some values cannot be exported; see omission comments.", 10,
						 interface_theme::kStatusAmber);
				Clay_TextElementConfig code_style{};
				code_style.fontSize = 11;
				code_style.textColor = interface_theme::kTextCanvas;
				code_style.wrapMode = CLAY_TEXT_WRAP_NONE;
				CLAY_TEXT(context.uiManager.toClayString(sandbox.code),
						  CLAY_TEXT_CONFIG(code_style));
			};
			const auto draw_changes = [&] {
				std::size_t live_count = 0;
				std::size_t startup_count = 0;
				for (const auto& variant : catalogue) {
					if (variant.type != selected->type || (!state->inspect_theme_all_variants &&
														   variant.variant != selected->variant))
						continue;
					for (const auto& leaf : leaves(*schema, variant)) {
						if (!matches(leaf))
							continue;
						live_count += format_value(*schema, leaf, leaf.live) !=
									  format_value(*schema, leaf, leaf.startup);
						startup_count += format_value(*schema, leaf, leaf.startup) !=
										 format_value(*schema, leaf, leaf.authored);
					}
				}
				text(context.uiManager,
					 std::to_string(live_count) + " Live vs startup · " +
						 std::to_string(startup_count) + " Startup vs authored",
					 10);
				for (const auto& variant : catalogue) {
					if (variant.type != selected->type || (!state->inspect_theme_all_variants &&
														   variant.variant != selected->variant))
						continue;
					for (const auto& leaf : leaves(*schema, variant)) {
						if (!matches(leaf) || !changed(*schema, leaf))
							continue;
						const bool live_edit = format_value(*schema, leaf, leaf.live) !=
											   format_value(*schema, leaf, leaf.startup);
						const std::string identity =
							"change-" + std::string(variant.variant) + "-" + leaf.path;
						CLAY(context.clayID(RuntimeName(identity)), section()) {
							text(context.uiManager,
								 std::string(variant.variant) + " / " + leaf.path, 11,
								 interface_theme::kTextCanvas);
							text(context.uiManager,
								 live_edit ? "Live edit" : "Startup customization", 10,
								 live_edit ? interface_theme::kAccentSeaGlass
										   : interface_theme::kTextMuted);
							value_row(identity + "-authored", "Authored", leaf, leaf.authored);
							value_row(identity + "-startup", "Startup", leaf, leaf.startup);
							value_row(identity + "-live", "Live", leaf, leaf.live, live_edit);
						}
					}
				}
				if (!live_count)
					text(context.uiManager, "No live differences from startup in this selection");
			};
			auto content = column();
			content.layout.sizing.height = CLAY_SIZING_GROW(0);
			content.layout.padding = CLAY_PADDING_ALL(14);
			content.layout.childGap = 12;
			content.backgroundColor = interface_theme::kDepth0Keel;
			CLAY(context.clayID("content"), content) {
				auto grid_row = row();
				grid_row.layout.sizing.height = CLAY_SIZING_PERCENT(.5f);
				grid_row.layout.childGap = 12;
				CLAY(context.clayID("top-row"), grid_row) {
					workbench_card(context, "specimens", "PREVIEW", draw_specimens, preview_toolbar,
								   true);
					workbench_card(context, "tokens", "TOKEN OVERVIEW", draw_tokens, token_toolbar);
				}
				CLAY(context.clayID("bottom-row"), grid_row) {
					workbench_card(context, "changes", "CHANGES", draw_changes);
					workbench_card(context, "code", "C++ CODE", draw_code);
				}
			}
		} else {
			auto empty = column();
			empty.layout.sizing.height = CLAY_SIZING_GROW(0);
			empty.layout.childAlignment = {CLAY_ALIGN_X_CENTER, CLAY_ALIGN_Y_CENTER};
			CLAY(context.clayID("empty-selection"), empty) {
				text(context.uiManager, "Select a theme to open its workbench", 12,
					 interface_theme::kTextMuted);
			}
		}
	}
}
} // namespace FlowUi::devSystems::interface_elements
#endif
