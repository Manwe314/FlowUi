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
	auto& model = state->theme_editor_model;
	if (!model.refresh(*app))
		return;
	const auto catalogue = model.catalogue();
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
			const auto* selected_presentation = model.find(*selected);
			if (!selected_presentation)
				return;
			const auto& selected_leaves = selected_presentation->fields;
			std::vector<const ThemePresentation*> variant_tokens;
			variant_tokens.reserve(catalogue.size());
			for (const auto& variant : catalogue)
				variant_tokens.emplace_back(model.find(variant));
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
			const bool filter_changed =
				sandbox.filter_generation != model.generation() ||
				sandbox.filter_type != selected->type ||
				sandbox.filter_variant != selected->variant ||
				sandbox.filter_group != state->inspect_theme_group ||
				sandbox.filter_query != state->searchQuery ||
				sandbox.filter_token_query != sandbox.token_search ||
				sandbox.filter_all_variants != state->inspect_theme_all_variants;
			if (filter_changed) {
				const bool selection_changed =
					sandbox.filter_type != selected->type ||
					sandbox.filter_variant != selected->variant ||
					sandbox.filter_group != state->inspect_theme_group ||
					sandbox.filter_query != state->searchQuery ||
					sandbox.filter_token_query != sandbox.token_search ||
					sandbox.filter_all_variants != state->inspect_theme_all_variants;
				sandbox.token_indices.clear();
				sandbox.token_indices.reserve(selected_leaves.size());
				sandbox.changed_fields.clear();
				std::size_t changed_capacity = 0;
				for (std::size_t variant_index = 0; variant_index < catalogue.size();
					 ++variant_index) {
					const auto& variant = catalogue[variant_index];
					if (variant.type == selected->type &&
						(state->inspect_theme_all_variants || variant.variant == selected->variant))
						changed_capacity += variant_tokens[variant_index]->changed_indices.size();
				}
				sandbox.changed_fields.reserve(changed_capacity);
				sandbox.live_count = 0;
				sandbox.startup_count = 0;
				const auto matches = [&](const ThemeLeaf& leaf, const ThemeTokenValue& value) {
					return (!state->inspect_theme_group ||
							(leaf.key.nestedPath.empty()
								 ? leaf.key.field
								 : leaf.key.nestedPath.front()) == state->inspect_theme_group) &&
						   (state->searchQuery.empty() || contains(leaf.path, state->searchQuery) ||
							contains(selected->typeName, state->searchQuery)) &&
						   (sandbox.token_search.empty() ||
							contains(leaf.path, sandbox.token_search) ||
							contains(value.live_text, sandbox.token_search));
				};
				for (std::size_t leaf_index = 0; leaf_index < selected_leaves.size(); ++leaf_index)
					if (matches(selected_leaves[leaf_index],
								selected_presentation->values[leaf_index]))
						sandbox.token_indices.emplace_back(leaf_index);
				for (std::size_t variant_index = 0; variant_index < catalogue.size();
					 ++variant_index) {
					const auto& variant = catalogue[variant_index];
					if (variant.type != selected->type || (!state->inspect_theme_all_variants &&
														   variant.variant != selected->variant))
						continue;
					const auto* presentation = variant_tokens[variant_index];
					for (const auto leaf_index : presentation->changed_indices) {
						const auto& value = presentation->values[leaf_index];
						if (!matches(presentation->fields[leaf_index], value))
							continue;
						sandbox.live_count += value.live_edit;
						sandbox.startup_count += value.startup_edit;
						sandbox.changed_fields.emplace_back(
							State::FilteredChange{variant_index, leaf_index});
					}
				}
				sandbox.filter_generation = model.generation();
				sandbox.filter_type = selected->type;
				sandbox.filter_variant = selected->variant;
				sandbox.filter_group = state->inspect_theme_group;
				sandbox.filter_query = state->searchQuery;
				sandbox.filter_token_query = sandbox.token_search;
				sandbox.filter_all_variants = state->inspect_theme_all_variants;
				if (selection_changed) {
					const auto scroll =
						Clay_GetScrollContainerData(context.clayID("tokens-scroll"));
					if (scroll.found && scroll.scrollPosition)
						*scroll.scrollPosition = {};
				}
			}
			Clay_TextElementConfig token_style{};
			token_style.fontSize = 11;
			const float token_line_height = context.uiManager.inputContentElement(token_style)
												.layout.sizing.height.size.minMax.min;
			const float token_value_height = std::max(18.0f, token_line_height) + 10.0f;
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
									   bool accent = false, std::string_view display = {},
									   bool compact = false) {
				auto line = row();
				if (compact)
					line.layout.sizing.height = CLAY_SIZING_FIXED(token_value_height);
				const auto value_text = [&](std::string_view label, uint16_t size,
											Clay_Color color) {
					Clay_TextElementConfig style{};
					style.fontSize = size;
					style.textColor = color;
					style.wrapMode = compact ? CLAY_TEXT_WRAP_NONE : CLAY_TEXT_WRAP_WORDS;
					CLAY_TEXT(context.uiManager.toClayString(label), CLAY_TEXT_CONFIG(style));
				};
				line.layout.padding = Clay_Padding{4, 4, 5, 5};
				line.border = {interface_theme::kBorderPrimary, {0, 0, 0, 1, 0}};
				CLAY(context.clayID(RuntimeName(identity)), line) {
					auto caption = column();
					caption.layout.sizing.width = CLAY_SIZING_PERCENT(.32f);
					caption.backgroundColor = {};
					CLAY(context.clayID(RuntimeName(std::string(identity) + "-label")), caption) {
						value_text(label, 10, interface_theme::kTextSecondary);
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
						value_text(display, 11,
								   accent ? interface_theme::kAccentSeaGlass
										  : interface_theme::kTextCanvas);
					}
				}
			};
			const auto draw_tokens = [&] {
				// Fixed single-line overview rows permit exact spacer heights at any font scale.
				// Full reflected values remain available in the existing inspector editors.
				const auto scroll = Clay_GetScrollContainerData(context.clayID("tokens-scroll"));
				const std::size_t variant_count =
					state->inspect_theme_all_variants
						? std::ranges::count_if(
							  catalogue,
							  [&](const auto& variant) { return variant.type == selected->type; })
						: 1;
				const float row_height =
					16.0f + token_line_height +
					static_cast<float>(variant_count) * (token_value_height + 6.0f);
				constexpr float row_gap = 7.0f;
				const float stride = row_height + row_gap;
				const float heading_height = token_line_height + 8.0f;
				const float content_start = 8.0f + heading_height + row_gap;
				const float viewport_height =
					scroll.found ? scroll.scrollContainerDimensions.height : 600.0f;
				const float scroll_offset =
					scroll.found && scroll.scrollPosition ? -scroll.scrollPosition->y : 0.0f;
				const std::size_t total = sandbox.token_indices.size();
				const std::size_t estimated_first = static_cast<std::size_t>(
					std::max(0.0f, scroll_offset - content_start) / stride);
				const std::size_t first =
					std::min(total ? total - 1 : 0, estimated_first > 2 ? estimated_first - 2 : 0);
				const std::size_t visible_count =
					static_cast<std::size_t>(std::ceil(std::max(0.0f, viewport_height) / stride)) +
					5;
				const std::size_t end = first + std::min(total - first, visible_count);
				auto heading = row();
				heading.layout.sizing.height = CLAY_SIZING_FIXED(heading_height);
				CLAY(context.clayID("token-summary"), heading) {
					Clay_TextElementConfig heading_style{};
					heading_style.fontSize = 10;
					heading_style.textColor = interface_theme::kTextSecondary;
					heading_style.wrapMode = CLAY_TEXT_WRAP_NONE;
					const std::string summary =
						std::to_string(total) + " tokens · " +
						(state->inspect_theme_all_variants
							 ? "Compare registered variants · active marked *"
							 : std::string(selected->variant));
					CLAY_TEXT(context.uiManager.toClayString(summary),
							  CLAY_TEXT_CONFIG(heading_style));
				}
				const auto spacer = [&](std::string_view identity, std::size_t count) {
					if (!count)
						return;
					Clay_ElementDeclaration declaration{};
					declaration.layout.sizing = {
						CLAY_SIZING_GROW(0),
						CLAY_SIZING_FIXED(static_cast<float>(count) * stride - row_gap)};
					CLAY(context.clayID(RuntimeName(identity)), declaration);
				};
				spacer("tokens-before", first);
				sandbox.emitted_token_rows = 0;
				for (std::size_t row_index = first; row_index < end; ++row_index) {
					const auto leaf_index = sandbox.token_indices[row_index];
					const auto& leaf = selected_leaves[leaf_index];
					auto token_section = section(row_height);
					CLAY(context.clayID(RuntimeName("token-" + leaf.path)), token_section) {
						Clay_TextElementConfig path_style{};
						path_style.fontSize = 11;
						path_style.textColor = interface_theme::kTextCanvas;
						path_style.wrapMode = CLAY_TEXT_WRAP_NONE;
						CLAY_TEXT(context.uiManager.toClayString(leaf.path),
								  CLAY_TEXT_CONFIG(path_style));
						if (!state->inspect_theme_all_variants)
							value_row("effective-" + leaf.path, "Value", leaf, leaf.live, false,
									  selected_presentation->values[leaf_index].live_display, true);
						else
							for (std::size_t variant_index = 0; variant_index < catalogue.size();
								 ++variant_index) {
								const auto& variant = catalogue[variant_index];
								if (variant.type != selected->type)
									continue;
								const auto* presentation = variant_tokens[variant_index];
								// Shared schema traversal gives the same field order for every
								// variant.
								if (leaf_index >= presentation->fields.size() ||
									presentation->fields[leaf_index].key != leaf.key)
									continue;
								const auto& other = presentation->fields[leaf_index];
								value_row(
									"compare-" + std::string(variant.variant) + "-" + leaf.path,
									std::string(variant.variant) + (variant.active ? " *" : ""),
									other, other.live, false,
									presentation->values[leaf_index].live_display, true);
							}
					}
					++sandbox.emitted_token_rows;
				}
				spacer("tokens-after", total - end);
				if (!total)
					text(context.uiManager, "No tokens match this filter");
			};
			const auto draw_code = [&] {
				const bool source_usable_type = selected_presentation->source_usable;
				text(context.uiManager,
					 std::string(selected->typeName) + " / " +
						 (state->inspect_theme_all_variants ? "All registered variants"
															: std::string(selected->variant)),
					 10);
				if (!source_usable_type)
					text(context.uiManager, "This local theme type has no exportable C++ name.");
				if (sandbox.code_generation != model.generation() ||
					sandbox.code_type != selected->type ||
					sandbox.code_variant != selected->variant ||
					sandbox.code_all_variants != state->inspect_theme_all_variants) {
					sandbox.code.clear();
					sandbox.assignment_count = 0;
					sandbox.omitted_count = 0;
					for (std::size_t variant_index = 0; variant_index < catalogue.size();
						 ++variant_index) {
						const auto& variant = catalogue[variant_index];
						if (variant.type != selected->type ||
							(!state->inspect_theme_all_variants &&
							 variant.variant != selected->variant))
							continue;
						const auto* presentation = variant_tokens[variant_index];
						sandbox.code += presentation->code;
						sandbox.assignment_count += presentation->assignment_count;
						sandbox.omitted_count += presentation->omitted_count;
					}
					sandbox.code_generation = model.generation();
					sandbox.code_type = selected->type;
					sandbox.code_variant = selected->variant;
					sandbox.code_all_variants = state->inspect_theme_all_variants;
				}
				const auto assignment_count = sandbox.assignment_count;
				const auto omitted_count = sandbox.omitted_count;
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
				text(context.uiManager,
					 std::to_string(sandbox.live_count) + " Live vs startup · " +
						 std::to_string(sandbox.startup_count) + " Startup vs authored",
					 10);
				for (const auto& change : sandbox.changed_fields) {
					const auto& variant = catalogue[change.variant_index];
					const auto* presentation = variant_tokens[change.variant_index];
					const auto& leaf = presentation->fields[change.leaf_index];
					const auto& value = presentation->values[change.leaf_index];
					const std::string identity =
						"change-" + std::string(variant.variant) + "-" + leaf.path;
					CLAY(context.clayID(RuntimeName(identity)), section()) {
						text(context.uiManager, std::string(variant.variant) + " / " + leaf.path,
							 11, interface_theme::kTextCanvas);
						text(context.uiManager,
							 !value.comparison_available ? "Comparison unavailable"
							 : value.live_edit			 ? "Live edit"
														 : "Startup customization",
							 10,
							 value.live_edit ? interface_theme::kAccentSeaGlass
											 : interface_theme::kTextMuted);
						value_row(identity + "-authored", "Authored", leaf, leaf.authored, false,
								  value.authored_display);
						value_row(identity + "-startup", "Startup", leaf, leaf.startup, false,
								  value.startup_display);
						value_row(identity + "-live", "Live", leaf, leaf.live, value.live_edit,
								  value.live_display);
					}
				}
				if (!sandbox.live_count)
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
