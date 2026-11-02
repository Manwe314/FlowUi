#include "devSystems/devInterface/Inspect/DevThemeEditorModel.hpp"
#if FLOW_UI_DEV_MODE
#include "devSystems/devInterface/Inspect/DevInspectContentParameters.hpp"
#include "devSystems/devInterface/Inspect/DevThemeEditorData.hpp"
#include "managers/structs/InputFieldManagerStructs.hpp"
#include <array>
#include <cmath>
#include <cstdio>

namespace FlowUi::devSystems::interface_elements::theme_editor {
namespace {
/** Overlay style is an opaque reflected FSEL token; compare its documented fields explicitly. */
[[nodiscard]] ValueComparison compare_overlay(const InputFieldOverlayStyle& left,
											  const InputFieldOverlayStyle& right) noexcept {
	if (left.caretShape != right.caretShape)
		return ValueComparison::Different;
	const auto components = [](const InputFieldOverlayStyle& style) noexcept {
		return std::array<double, 18>{style.caretThicknessPx,
									  style.caretBlockWidthPx,
									  style.caretHeightOverflowTopPx,
									  style.caretHeightOverflowBottomPx,
									  style.caretBlinkPeriodSeconds,
									  style.caretBlinkVisibleSeconds,
									  style.caretColor.r,
									  style.caretColor.g,
									  style.caretColor.b,
									  style.caretColor.a,
									  style.selectionBoxColor.r,
									  style.selectionBoxColor.g,
									  style.selectionBoxColor.b,
									  style.selectionBoxColor.a,
									  style.selectedTextColor.r,
									  style.selectedTextColor.g,
									  style.selectedTextColor.b,
									  style.selectedTextColor.a};
	};
	const auto left_components = components(left);
	const auto right_components = components(right);
	bool unavailable = false;
	for (std::size_t component_index = 0; component_index < left_components.size();
		 ++component_index) {
		const double left_component = left_components[component_index];
		const double right_component = right_components[component_index];
		if (!std::isfinite(left_component) || !std::isfinite(right_component)) {
			unavailable = true;
			continue;
		}
		if (left_component != right_component ||
			(left_component == 0 && std::signbit(left_component) != std::signbit(right_component)))
			return ValueComparison::Different;
	}
	return unavailable ? ValueComparison::Unavailable : ValueComparison::Equal;
}
[[nodiscard]] ValueComparison compare_reflected(const devMode::DevSchemaGeneration& schema,
												devMode::DevTypeIndex index, const void* left,
												const void* right, uint32_t depth) noexcept {
	if (!left || !right || depth > 24)
		return ValueComparison::Unavailable;
	if (left == right)
		return ValueComparison::Equal;
	const auto* type = schema.type(index);
	const auto* operations =
		index.value < schema.typeOperations.size() ? schema.typeOperations[index.value] : nullptr;
	if (!type || !operations)
		return ValueComparison::Unavailable;
	if (type->id == ::FlowUi::detail::typeHash<InputFieldOverlayStyle>())
		return compare_overlay(*static_cast<const InputFieldOverlayStyle*>(left),
							   *static_cast<const InputFieldOverlayStyle*>(right));
	switch (type->kind) {
	case devMode::DevTypeKind::Boolean:
	case devMode::DevTypeKind::SignedInteger:
	case devMode::DevTypeKind::UnsignedInteger:
	case devMode::DevTypeKind::Enumeration:
	case devMode::DevTypeKind::FloatingPoint: {
		long double left_numeric = 0, right_numeric = 0;
		if (!operations->numericValue || !operations->numericValue(left, left_numeric) ||
			!operations->numericValue(right, right_numeric) || !std::isfinite(left_numeric) ||
			!std::isfinite(right_numeric))
			return ValueComparison::Unavailable;
		const bool equal =
			left_numeric == right_numeric &&
			(type->kind != devMode::DevTypeKind::FloatingPoint || left_numeric != 0 ||
			 std::signbit(left_numeric) == std::signbit(right_numeric));
		return equal ? ValueComparison::Equal : ValueComparison::Different;
	}
	case devMode::DevTypeKind::Text:
		if (operations->textView)
			return operations->textView(left) == operations->textView(right)
					   ? ValueComparison::Equal
					   : ValueComparison::Different;
		break;
	case devMode::DevTypeKind::Optional:
		if (operations->optionalHasValue && operations->optionalValueAddress) {
			const bool left_present = operations->optionalHasValue(left);
			if (left_present != operations->optionalHasValue(right))
				return ValueComparison::Different;
			return left_present
					   ? compare_reflected(schema, type->elementType,
										   operations->optionalValueAddress(left),
										   operations->optionalValueAddress(right), depth + 1)
					   : ValueComparison::Equal;
		}
		break;
	case devMode::DevTypeKind::Sequence:
		if (operations->sequenceSize && operations->sequenceElementAddress) {
			const auto count = operations->sequenceSize(left);
			if (count != operations->sequenceSize(right))
				return ValueComparison::Different;
			bool unavailable = false;
			for (std::size_t element_index = 0; element_index < count; ++element_index) {
				const auto comparison = compare_reflected(
					schema, type->elementType,
					operations->sequenceElementAddress(left, element_index),
					operations->sequenceElementAddress(right, element_index), depth + 1);
				if (comparison == ValueComparison::Different)
					return comparison;
				unavailable |= comparison == ValueComparison::Unavailable;
			}
			return unavailable ? ValueComparison::Unavailable : ValueComparison::Equal;
		}
		break;
	case devMode::DevTypeKind::Object: {
		const auto cpp_type = schema.string(type->cppTypeName);
		// Resource/action identity is defined by the bake serializer, not its incidental fields.
		if (cpp_type.ends_with("TextureRef") || cpp_type.ends_with("ActionCall"))
			break;
		if (schema.fieldsOf(index).empty())
			break;
		bool unavailable = false;
		for (const auto& field : schema.fieldsOf(index)) {
			const auto* field_operations = field.operations < schema.fieldOperations.size()
											   ? schema.fieldOperations[field.operations]
											   : nullptr;
			if (!field_operations || !field_operations->constAddress) {
				unavailable = true;
				continue;
			}
			const auto comparison =
				compare_reflected(schema, field.valueType, field_operations->constAddress(left),
								  field_operations->constAddress(right), depth + 1);
			if (comparison == ValueComparison::Different)
				return comparison;
			unavailable |= comparison == ValueComparison::Unavailable;
		}
		return unavailable ? ValueComparison::Unavailable : ValueComparison::Equal;
	}
	default:
		break;
	}
	const auto left_value = tooling::serialize_dev_value(schema, index, left);
	const auto right_value = tooling::serialize_dev_value(schema, index, right);
	if (!left_value || !right_value)
		return ValueComparison::Unavailable;
	return left_value->json == right_value->json ? ValueComparison::Equal
												 : ValueComparison::Different;
}
/** Compact typed values; raw serialization remains in the generated code. */
[[nodiscard]] std::string token_value(const devMode::DevSchemaGeneration& schema,
									  const ThemeLeaf& leaf, const void* value,
									  std::string_view raw) {
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
	return std::string(raw);
}

[[nodiscard]] devMode::DevFieldId group_of(const ThemeLeaf& leaf) noexcept {
	return leaf.key.nestedPath.empty() ? leaf.key.field : leaf.key.nestedPath.front();
}
void refresh_values(const devMode::DevSchemaGeneration& schema, ThemePresentation& presentation,
					uint64_t& serialization_count) {
	std::vector<ThemeTokenValue> values;
	values.reserve(presentation.fields.size());
	std::vector<std::size_t> changed_indices;
	changed_indices.reserve(presentation.fields.size());
	for (const auto& leaf : presentation.fields) {
		ThemeTokenValue token{};
		const auto live_authored =
			compare_values(schema, leaf.field->valueType, leaf.live, leaf.authored);
		const auto live_startup =
			compare_values(schema, leaf.field->valueType, leaf.live, leaf.startup);
		const auto startup_authored =
			compare_values(schema, leaf.field->valueType, leaf.startup, leaf.authored);
		token.changed =
			live_authored != ValueComparison::Equal || startup_authored != ValueComparison::Equal;
		token.live_edit = live_startup != ValueComparison::Equal;
		token.startup_edit = startup_authored != ValueComparison::Equal;
		token.comparison_available = live_authored != ValueComparison::Unavailable &&
									 live_startup != ValueComparison::Unavailable &&
									 startup_authored != ValueComparison::Unavailable;
		++serialization_count;
		const auto serialized =
			tooling::serialize_dev_value(schema, leaf.field->valueType, leaf.live);
		token.exportable = serialized.has_value();
		token.live_text = serialized ? serialized->json : "Unavailable";
		if (!serialized) {
			const auto* type = schema.type(leaf.field->valueType);
			const auto* operations = leaf.field->valueType.value < schema.typeOperations.size()
										 ? schema.typeOperations[leaf.field->valueType.value]
										 : nullptr;
			if (leaf.live && type && type->kind == devMode::DevTypeKind::Text && operations &&
				operations->textView)
				token.live_text = operations->textView(leaf.live);
		}
		if (serialized && token.changed)
			token.live_cpp = serialized->cpp;
		if (live_authored == ValueComparison::Equal)
			token.authored_text = token.live_text;
		else {
			++serialization_count;
			token.authored_text = format_value(schema, leaf, leaf.authored);
		}
		if (live_startup == ValueComparison::Equal)
			token.startup_text = token.live_text;
		else if (startup_authored == ValueComparison::Equal)
			token.startup_text = token.authored_text;
		else {
			++serialization_count;
			token.startup_text = format_value(schema, leaf, leaf.startup);
		}
		token.live_display = token_value(schema, leaf, leaf.live, token.live_text);
		token.authored_display = token_value(schema, leaf, leaf.authored, token.authored_text);
		token.startup_display = token_value(schema, leaf, leaf.startup, token.startup_text);
		if (token.changed)
			changed_indices.emplace_back(values.size());
		values.emplace_back(std::move(token));
	}
	presentation.values = std::move(values);
	presentation.changed_indices = std::move(changed_indices);
}
void refresh_code(ThemePresentation& presentation) {
	presentation.code.clear();
	presentation.assignment_count = 0;
	presentation.omitted_count = 0;
	presentation.source_usable = presentation.type_name.find("{anonymous}") == std::string::npos &&
								 presentation.type_name.find('(') == std::string::npos;
	if (!presentation.source_usable)
		return;
	presentation.code = "{\n  auto theme = app.themes().getTheme<" + presentation.type_name + ">(" +
						cpp_string(presentation.variant) + ");\n";
	for (const auto leaf_index : presentation.changed_indices) {
		const auto& leaf = presentation.fields[leaf_index];
		const auto& value = presentation.values[leaf_index];
		if (value.exportable) {
			++presentation.assignment_count;
			presentation.code += "  theme." + leaf.path + " = " + value.live_cpp + ";\n";
		} else {
			++presentation.omitted_count;
			presentation.code += "  // Runtime-only value omitted: " + leaf.path + "\n";
		}
	}
	presentation.code += "  app.themes().registerTheme(" + cpp_string(presentation.variant) +
						 ", theme, " + (presentation.source.active ? "true" : "false") + ");\n}\n";
}
} // namespace
ValueComparison compare_values(const devMode::DevSchemaGeneration& schema,
							   devMode::DevTypeIndex type, const void* left,
							   const void* right) noexcept {
	return compare_reflected(schema, type, left, right, 0);
}
const ThemePresentation* ThemeEditorModel::find(const ThemeView& theme) const noexcept {
	const auto found = std::ranges::find_if(presentations_, [&](const auto& presentation) {
		return presentation.source.type == theme.type && presentation.variant == theme.variant;
	});
	return found == presentations_.end() ? nullptr : &*found;
}
bool ThemeEditorModel::mixed(devMode::DevTypeId type, devMode::DevFieldId group) const noexcept {
	return std::ranges::any_of(mixed_groups_, [&](const auto& entry) {
		return entry.type == type && entry.group == group;
	});
}
Status ThemeEditorModel::refresh(App& app) noexcept {
	try {
		const auto schema = app.devTooling().schemas().view();
		auto& catalogue = pending_catalogue_;
		catalogue.clear();
		catalogue.reserve(app.themes().devThemeCount());
		const auto visitor = [](void* destination, const ThemeView& theme) noexcept {
			try {
				static_cast<std::vector<ThemeView>*>(destination)->emplace_back(theme);
				return true;
			} catch (...) {
				return false;
			}
		};
		if (!app.themes().visitDevCatalogueThemes(&catalogue, visitor)) {
			presentations_.clear();
			catalogue_.clear();
			mixed_groups_.clear();
			schema_ = {};
			++generation_;
			return unexpectedError(
				makeError(ErrorCode::AllocationFailed, ErrorSite::ThemeRegisterVariant));
		}
		if (schema)
			for (auto& theme : catalogue) {
				const auto* root = schema->findTheme(theme.type);
				const auto* type = root ? schema->type(root->themeType) : nullptr;
				if (type)
					theme.typeName = schema->string(type->cppTypeName);
			}
		std::ranges::sort(catalogue, [](const auto& left, const auto& right) {
			return left.typeName == right.typeName ? left.variant < right.variant
												   : left.typeName < right.typeName;
		});
		const bool schema_changed = schema.shared() != schema_.shared();
		bool updated = schema_changed || catalogue.size() != catalogue_.size();
		if (schema_changed)
			presentations_.clear();
		std::erase_if(presentations_, [&](const auto& entry) {
			return std::ranges::none_of(catalogue, [&](const auto& theme) {
				return theme.type == entry.source.type && theme.variant == entry.variant;
			});
		});
		presentations_.reserve(catalogue.size());
		if (schema)
			for (const auto& theme : catalogue) {
				auto found = std::ranges::find_if(presentations_, [&](const auto& entry) {
					return entry.source.type == theme.type && entry.variant == theme.variant;
				});
				if (found == presentations_.end()) {
					presentations_.emplace_back();
					found = presentations_.end() - 1;
					found->variant = theme.variant;
					found->type_name = theme.typeName;
				}
				const bool identity_changed = found->source.type != theme.type ||
											  found->source.payload != theme.payload ||
											  found->source.authored != theme.authored ||
											  found->source.startup != theme.startup ||
											  found->source.payloadSize != theme.payloadSize;
				const bool values_changed =
					identity_changed || found->source.revision != theme.revision;
				const bool metadata_changed =
					found->source.active != theme.active || found->type_name != theme.typeName;
				found->type_name = theme.typeName;
				if (identity_changed) {
					found->fields = leaves(*schema, theme);
					++leaf_rebuild_count_;
				}
				if (values_changed) {
					refresh_values(*schema, *found, serialization_count_);
					++value_refresh_count_;
				}
				found->source = theme;
				if (values_changed || metadata_changed) {
					refresh_code(*found);
					updated = true;
				}
			}
		if (updated && schema) {
			mixed_groups_.clear();
			mixed_groups_.reserve(schema->fields.size());
			for (std::size_t reference_index = 0; reference_index < presentations_.size();
				 ++reference_index) {
				const auto& reference = presentations_[reference_index];
				if (std::ranges::any_of(std::span(presentations_).first(reference_index),
										[&](const auto& entry) {
											return entry.source.type == reference.source.type;
										}))
					continue;
				for (std::size_t variant_index = reference_index + 1;
					 variant_index < presentations_.size(); ++variant_index) {
					const auto& variant = presentations_[variant_index];
					if (reference.source.type != variant.source.type)
						continue;
					for (std::size_t leaf_index = 0; leaf_index < reference.fields.size();
						 ++leaf_index) {
						const auto& leaf = reference.fields[leaf_index];
						const bool aligned = leaf_index < variant.fields.size() &&
											 variant.fields[leaf_index].key == leaf.key;
						if (!aligned || compare_values(*schema, leaf.field->valueType, leaf.live,
													   variant.fields[leaf_index].live) !=
											ValueComparison::Equal) {
							const auto group = group_of(leaf);
							if (!mixed(reference.source.type, group))
								mixed_groups_.emplace_back(
									MixedGroup{reference.source.type, group});
						}
					}
				}
			}
		}
		schema_ = schema;
		catalogue_.swap(catalogue);
		if (updated)
			++generation_;
		return {};
	} catch (...) {
		// Never expose cached addresses after a failed refresh or registration change.
		presentations_.clear();
		catalogue_.clear();
		mixed_groups_.clear();
		schema_ = {};
		++generation_;
		return unexpectedError(
			makeError(ErrorCode::AllocationFailed, ErrorSite::ThemeRegisterVariant));
	}
}
} // namespace FlowUi::devSystems::interface_elements::theme_editor
#endif
