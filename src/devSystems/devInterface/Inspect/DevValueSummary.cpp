#include "devSystems/devInterface/Inspect/DevValueSummary.hpp"

#if FLOW_UI_DEV_MODE
#include "FlowUi/Public/App.hpp"
#include "devSystems/devTooling/DevTooling.hpp"
#include "FlowUi/Public/managers/ActionManager.hpp"
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <iomanip>
#include <sstream>

namespace FlowUi::devSystems::interface_elements {
namespace {
std::string_view schemaTypeName(const tooling::DevSchemaGeneration& schema,
								const tooling::DevTypeSchema* type) {
	if (!type)
		return "Unknown";
	std::string_view name = schema.string(type->displayName);
	if (name.empty())
		name = schema.string(type->cppTypeName);
	return name.empty() ? std::string_view{"Unknown"} : name;
}

std::string normalizedTypeName(const tooling::DevSchemaGeneration& schema,
							   const tooling::DevTypeSchema* type) {
	if (!type)
		return "Unknown";
	if (type->kind == tooling::DevTypeKind::Optional) {
		return "Optional " + normalizedTypeName(schema, schema.type(type->elementType));
	}
	switch (type->kind) {
	case tooling::DevTypeKind::Boolean:
		return "Boolean";
	case tooling::DevTypeKind::SignedInteger:
		return "Integer";
	case tooling::DevTypeKind::UnsignedInteger:
		return "Unsigned Integer";
	case tooling::DevTypeKind::FloatingPoint:
		return "Float";
	case tooling::DevTypeKind::Text:
		return "Text";
	case tooling::DevTypeKind::Sequence:
		return "Sequence of " + normalizedTypeName(schema, schema.type(type->elementType));
	default:
		break;
	}
	std::string name(schemaTypeName(schema, type));
	if (name.starts_with("std::"))
		name.erase(0, 5);
	if (name.starts_with("FlowUi::"))
		name.erase(0, 8);
	return name;
}

std::string boundedText(std::string text, std::size_t maximum = 56u) {
	if (text.size() <= maximum)
		return text;
	std::size_t end = maximum > 3u ? maximum - 3u : 0u;
	while (end > 0 && (static_cast<unsigned char>(text[end]) & 0xC0u) == 0x80u)
		--end;
	text.resize(end);
	text += "...";
	return text;
}

std::string escapedSingleLine(std::string_view text) {
	std::string result;
	result.reserve(std::min<std::size_t>(text.size(), 2048u));
	for (char character : text) {
		if (result.size() >= 2048u)
			break;
		switch (character) {
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
			if (static_cast<unsigned char>(character) >= 0x20u)
				result.push_back(character);
			break;
		}
	}
	if (result.size() < text.size())
		result += "...";
	return result;
}

std::string sizingAxisQuick(std::string_view prefix, const Clay_SizingAxis& axis) {
	char buffer[64]{};
	switch (axis.type) {
	case CLAY__SIZING_TYPE_FIT:
		std::snprintf(buffer, sizeof(buffer), "%.*s Fit %.0f-%.0f", static_cast<int>(prefix.size()),
					  prefix.data(), axis.size.minMax.min, axis.size.minMax.max);
		break;
	case CLAY__SIZING_TYPE_GROW:
		std::snprintf(buffer, sizeof(buffer), "%.*s Grow %.0f-%.0f",
					  static_cast<int>(prefix.size()), prefix.data(), axis.size.minMax.min,
					  axis.size.minMax.max);
		break;
	case CLAY__SIZING_TYPE_PERCENT:
		std::snprintf(buffer, sizeof(buffer), "%.*s %.0f%%", static_cast<int>(prefix.size()),
					  prefix.data(), axis.size.percent * 100.0f);
		break;
	case CLAY__SIZING_TYPE_FIXED:
		std::snprintf(buffer, sizeof(buffer), "%.*s %.0f px", static_cast<int>(prefix.size()),
					  prefix.data(), axis.size.minMax.min);
		break;
	default:
		std::snprintf(buffer, sizeof(buffer), "%.*s Unknown", static_cast<int>(prefix.size()),
					  prefix.data());
		break;
	}
	return buffer;
}

DevQuickView summarize_value(const tooling::DevSchemaGeneration& schema,
							 const tooling::DevTypeSchema* type, const void* value, App* app,
							 const tooling::DevFieldSchema* field,
							 DevSummaryAvailability availability, std::uint16_t depth) {
	if (depth >= 16)
		return DevQuickView{.primary = "Recursive value · depth limit",
							.kind = DevQuickView::Kind::Status,
							.status = DevQuickStatus::Warning};
	if (!type)
		return DevQuickView{
			.primary = "Unregistered · Unknown",
			.kind = DevQuickView::Kind::Status,
			.status = DevQuickStatus::Warning,
		};
	if (!value) {
		std::string reason;
		if (availability == DevSummaryAvailability::CopyFailed)
			reason = "Value copy failed";
		else if (availability == DevSummaryAvailability::CollectionDisabled)
			reason = "Value collection disabled";
		else if (availability == DevSummaryAvailability::SchemaChanged)
			reason = "Schema changed · awaiting capture";
		else if (type->reason == tooling::DevCapabilityReason::NoSchemaDeclaration)
			reason = "Unregistered · " + normalizedTypeName(schema, type);
		else if (type->reason == tooling::DevCapabilityReason::RawPointer)
			reason = "Pointer · metadata only";
		else if (type->reason == tooling::DevCapabilityReason::CallableType)
			reason = "Callable · metadata only";
		else if (type->reason == tooling::DevCapabilityReason::NoCaptureAdapter)
			reason = "No capture adapter · " + normalizedTypeName(schema, type);
		else if (type->capture != tooling::DevCaptureCapability::Value)
			reason = "Metadata only · " + normalizedTypeName(schema, type);
		else
			reason = "Value not captured";
		return DevQuickView{.primary = std::move(reason),
							.kind = DevQuickView::Kind::Status,
							.status = type->capture == tooling::DevCaptureCapability::Value
										  ? DevQuickStatus::Unavailable
										  : DevQuickStatus::Warning};
	}
	if (type->reason == tooling::DevCapabilityReason::NoSchemaDeclaration)
		return DevQuickView{
			.primary = "Unregistered · " + normalizedTypeName(schema, type) + " (" +
					   std::to_string(type->size) + " bytes)",
			.kind = DevQuickView::Kind::Status,
			.status = DevQuickStatus::Warning,
		};
	const std::size_t typeIndex = static_cast<std::size_t>(type - schema.types.data());
	const tooling::DevTypeOps* operations =
		typeIndex < schema.typeOperations.size() ? schema.typeOperations[typeIndex] : nullptr;
	if (type->kind == tooling::DevTypeKind::Optional) {
		if (!operations || !operations->optionalHasValue || !operations->optionalValueAddress) {
			return DevQuickView{
				.primary = "Value unavailable",
				.kind = DevQuickView::Kind::Status,
				.status = DevQuickStatus::Unavailable,
			};
		}
		if (!operations->optionalHasValue(value))
			return DevQuickView{
				.primary = "Unset / inherits",
			};
		return summarize_value(schema, schema.type(type->elementType),
							   operations->optionalValueAddress(value), app, field, availability,
							   depth + 1);
	}
	const std::string_view name = schemaTypeName(schema, type);
	char buffer[128]{};
	if (name == "Clay_Padding") {
		const auto& padding = *static_cast<const Clay_Padding*>(value);
		if (padding.left == padding.right && padding.left == padding.top &&
			padding.left == padding.bottom) {
			std::snprintf(buffer, sizeof(buffer), "%u all", padding.left);
			return DevQuickView{
				.primary = buffer,
				.kind = DevQuickView::Kind::Spacing,
			};
		}
		std::snprintf(buffer, sizeof(buffer), "T %u · R %u", padding.top, padding.right);
		char second[64]{};
		std::snprintf(second, sizeof(second), "B %u · L %u", padding.bottom, padding.left);
		return DevQuickView{
			.primary = buffer,
			.secondary = second,
			.kind = DevQuickView::Kind::Spacing,
		};
	}
	if (name == "Clay_BorderWidth") {
		const auto& width = *static_cast<const Clay_BorderWidth*>(value);
		if (width.left == width.right && width.left == width.top && width.left == width.bottom) {
			std::snprintf(buffer, sizeof(buffer), "%u all · Between %u", width.left,
						  width.betweenChildren);
		} else {
			std::snprintf(buffer, sizeof(buffer), "L%u R%u T%u B%u · Between %u", width.left,
						  width.right, width.top, width.bottom, width.betweenChildren);
		}
		return DevQuickView{
			.primary = buffer,
			.kind = DevQuickView::Kind::Spacing,
		};
	}
	if (name == "Clay_Color") {
		const auto& color = *static_cast<const Clay_Color*>(value);
		std::snprintf(buffer, sizeof(buffer), "#%02X%02X%02X%02X",
					  static_cast<unsigned>(std::lround(std::clamp(color.r, 0.0f, 255.0f))),
					  static_cast<unsigned>(std::lround(std::clamp(color.g, 0.0f, 255.0f))),
					  static_cast<unsigned>(std::lround(std::clamp(color.b, 0.0f, 255.0f))),
					  static_cast<unsigned>(std::lround(std::clamp(color.a, 0.0f, 255.0f))));
		return DevQuickView{
			.primary = buffer,
			.swatch = color,
			.kind = DevQuickView::Kind::Color,
		};
	}
	if (type->kind == tooling::DevTypeKind::Boolean) {
		long double numeric = 0.0L;
		if (!operations || !operations->numericValue || !operations->numericValue(value, numeric))
			return DevQuickView{.primary = "No boolean capture adapter",
								.kind = DevQuickView::Kind::Status,
								.status = DevQuickStatus::Unavailable};
		return DevQuickView{.primary = numeric == 0.0L ? "Off" : "On"};
	}
	if (field && field->choiceDomain != tooling::DevChoiceDomain::None &&
		(field->choiceDomain == tooling::DevChoiceDomain::FontFace ||
		 field->choiceDomain == tooling::DevChoiceDomain::FontFamily)) {
		long double numeric = 0.0L;
		if (operations && operations->numericValue && operations->numericValue(value, numeric)) {
			const std::uint64_t identity = static_cast<std::uint64_t>(numeric);
			if (app)
				for (const tooling::DevFontCatalogEntry& font :
					 app->devTooling().catalogues().queryFonts()) {
					const std::uint64_t candidate =
						field->choiceDomain == tooling::DevChoiceDomain::FontFamily
							? static_cast<std::uint64_t>(font.familyHandle)
							: static_cast<std::uint64_t>(font.fontHandle);
					if (candidate != identity)
						continue;
					std::string label(font.familyName);
					if (field->choiceDomain == tooling::DevChoiceDomain::FontFace) {
						label += " · ";
						label += font.faceName.empty() ? "Regular" : std::string(font.faceName);
					}
					return DevQuickView{
						.primary = std::move(label),
					};
				}
			if (!app)
				return DevQuickView{.primary = "Font #" + std::to_string(identity)};
			return DevQuickView{
				.primary = "Missing font · " + std::to_string(identity),
				.kind = DevQuickView::Kind::Status,
				.status = DevQuickStatus::Missing,
			};
		}
	}
	if (name == "ActionCall") {
		const auto& call = *static_cast<const ActionCall*>(value);
		if (!call)
			return DevQuickView{
				.primary = "None",
			};
		if (app) {
			if (const std::optional<ActionDebugInfo> info = app->actions().debugInfo(call)) {
				return DevQuickView{
					.primary = std::string(info->kind == ActionCallKind::App ? "APP " : "UI ") +
							   std::string(info->debugName),
					.status = info->bound ? DevQuickStatus::Normal : DevQuickStatus::Missing,
				};
			}
		}
		return DevQuickView{
			.primary = "Missing action",
			.kind = DevQuickView::Kind::Status,
			.status = DevQuickStatus::Missing,
		};
	}
	if (name == "TextureRef") {
		const auto& texture = *static_cast<const TextureRef*>(value);
		if (!texture.handle && !texture.sourceKey.empty()) {
			return DevQuickView{
				.primary = std::string(texture.sourceKey),
			};
		}
		if (!texture.handle)
			return DevQuickView{
				.primary = "No texture",
				.kind = DevQuickView::Kind::Status,
				.status = DevQuickStatus::Missing,
			};
		if (app) {
			for (const tooling::DevImageCatalogEntry& image :
				 app->devTooling().catalogues().queryImages()) {
				if (image.textureHandle != texture.handle)
					continue;
				return DevQuickView{
					.primary = std::string(image.key.name),
					.status = image.status == tooling::DevResourceStatus::Ready
								  ? DevQuickStatus::Normal
								  : DevQuickStatus::Warning,
				};
			}
			for (const tooling::DevIconCatalogEntry& icon :
				 app->devTooling().catalogues().queryIcons()) {
				if (icon.atlasTexture != texture.handle ||
					std::abs(icon.atlasUv.x - texture.uv0x) > 0.00001f ||
					std::abs(icon.atlasUv.y - texture.uv0y) > 0.00001f)
					continue;
				return DevQuickView{
					.primary = "Icon · " + std::string(icon.iconName),
				};
			}
		}
		std::snprintf(buffer, sizeof(buffer), "Anonymous texture · %016llX",
					  static_cast<unsigned long long>(texture.handle.packed()));
		return DevQuickView{
			.primary = buffer,
			.kind = DevQuickView::Kind::Status,
			.status = DevQuickStatus::Warning,
		};
	}
	if (name == "Clay_CornerRadius") {
		const auto& radius = *static_cast<const Clay_CornerRadius*>(value);
		if (radius.topLeft == radius.topRight && radius.topLeft == radius.bottomRight &&
			radius.topLeft == radius.bottomLeft) {
			std::snprintf(buffer, sizeof(buffer), "%.0f all", radius.topLeft);
		} else {
			std::snprintf(buffer, sizeof(buffer), "TL%.0f TR%.0f BR%.0f BL%.0f", radius.topLeft,
						  radius.topRight, radius.bottomRight, radius.bottomLeft);
		}
		return DevQuickView{
			.primary = buffer,
		};
	}
	if (name == "Clay_Dimensions") {
		const auto& dimensions = *static_cast<const Clay_Dimensions*>(value);
		std::snprintf(buffer, sizeof(buffer), "%.2f × %.2f", dimensions.width, dimensions.height);
		return DevQuickView{
			.primary = buffer,
		};
	}
	if (name == "Clay_Vector2") {
		const auto& vector = *static_cast<const Clay_Vector2*>(value);
		std::snprintf(buffer, sizeof(buffer), "%.2f, %.2f", vector.x, vector.y);
		return DevQuickView{
			.primary = buffer,
		};
	}
	if (name == "Clay_SizingMinMax") {
		const auto& range = *static_cast<const Clay_SizingMinMax*>(value);
		std::snprintf(buffer, sizeof(buffer), "%.2f – %.2f", range.min, range.max);
		return DevQuickView{
			.primary = buffer,
		};
	}
	if (name == "Clay_SizingAxis") {
		const auto& axis = *static_cast<const Clay_SizingAxis*>(value);
		return DevQuickView{
			.primary = sizingAxisQuick("", axis),
			.kind = DevQuickView::Kind::Sizing,
		};
	}
	if (name == "Clay_Sizing") {
		const auto& sizing = *static_cast<const Clay_Sizing*>(value);
		return DevQuickView{
			.primary = sizingAxisQuick("W", sizing.width),
			.secondary = sizingAxisQuick("H", sizing.height),
			.kind = DevQuickView::Kind::Sizing,
		};
	}
	if (type->kind == tooling::DevTypeKind::Enumeration &&
		type->enumeration.values.first <= schema.enumValues.size() &&
		type->enumeration.values.count <=
			schema.enumValues.size() - type->enumeration.values.first) {
		long double numeric = 0.0L;
		if (operations && operations->numericValue && operations->numericValue(value, numeric)) {
			const uint64_t bits = static_cast<uint64_t>(numeric);
			for (uint32_t index = 0; index < type->enumeration.values.count; ++index) {
				const tooling::DevEnumValueSchema& option =
					schema.enumValues[type->enumeration.values.first + index];
				if (option.bits == bits)
					return DevQuickView{
						.primary = std::string(schema.string(option.name)),
					};
			}
			std::snprintf(buffer, sizeof(buffer), "Unknown · %llu",
						  static_cast<unsigned long long>(bits));
			return DevQuickView{
				.primary = buffer,
				.kind = DevQuickView::Kind::Status,
				.status = DevQuickStatus::Warning,
			};
		}
	}
	if (type->kind == tooling::DevTypeKind::SignedInteger ||
		type->kind == tooling::DevTypeKind::UnsignedInteger ||
		type->kind == tooling::DevTypeKind::FloatingPoint) {
		long double numeric = 0.0L;
		if (operations && operations->numericValue && operations->numericValue(value, numeric)) {
			std::ostringstream stream;
			if (type->kind == tooling::DevTypeKind::FloatingPoint) {
				stream << std::setprecision(6) << std::defaultfloat << numeric;
			} else
				stream << std::fixed << std::setprecision(0) << numeric;
			return DevQuickView{
				.primary = stream.str(),
			};
		}
	}
	if (type->kind == tooling::DevTypeKind::Text && operations && operations->textView) {
		return DevQuickView{
			.primary = escapedSingleLine(operations->textView(value)),
		};
	}
	if (type->kind == tooling::DevTypeKind::Sequence && operations && operations->sequenceSize) {
		std::snprintf(buffer, sizeof(buffer), "%zu items", operations->sequenceSize(value));
		return DevQuickView{
			.primary = buffer,
		};
	}
	if (type->kind == tooling::DevTypeKind::Object) {
		std::snprintf(buffer, sizeof(buffer), "%u fields", type->fields.count);
		return DevQuickView{
			.primary = buffer,
		};
	}
	if (type->kind == tooling::DevTypeKind::Pointer && operations && operations->pointerValue) {
		const void* pointer = nullptr;
		if (operations->pointerValue(value, pointer)) {
			if (!pointer)
				return DevQuickView{
					.primary = "Null",
				};
			std::snprintf(buffer, sizeof(buffer), "Address · %p", pointer);
			return DevQuickView{
				.primary = buffer,
			};
		}
	}
	if (type->kind == tooling::DevTypeKind::Opaque) {
		std::snprintf(buffer, sizeof(buffer), "Opaque · %u bytes", type->size);
		return DevQuickView{
			.primary = buffer,
			.kind = DevQuickView::Kind::Status,
		};
	}
	return DevQuickView{
		.primary = boundedText("Unregistered · " + normalizedTypeName(schema, type)),
		.kind = DevQuickView::Kind::Status,
		.status = DevQuickStatus::Warning,
	};
}

} // namespace

DevQuickView format_value_summary(const tooling::DevSchemaGeneration& schema,
								  const tooling::DevTypeSchema* type, const void* value, App* app,
								  const tooling::DevFieldSchema* field,
								  DevSummaryAvailability availability) {
	DevQuickView result = summarize_value(schema, type, value, app, field, availability, 0);
	result.detail = boundedText(
		result.primary + (result.secondary.empty() ? "" : " · " + result.secondary), 4096);
	result.primary = boundedText(std::move(result.primary), 96);
	result.secondary = boundedText(std::move(result.secondary), 96);
	return result;
}

} // namespace FlowUi::devSystems::interface_elements
#endif
