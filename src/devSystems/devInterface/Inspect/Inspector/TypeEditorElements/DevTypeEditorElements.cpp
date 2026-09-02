#include "devSystems/devInterface/Inspect/Inspector/TypeEditorElements/DevTypeEditorElements.hpp"

#if FLOW_UI_DEV_MODE

#include <algorithm>
#include <array>
#include <cerrno>
#include <charconv>
#include <cctype>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <iomanip>
#include <limits>
#include <sstream>
#include <string>
#include <utility>

#include "FSEL/Button.hpp"
#include "FSEL/ComboBox.hpp"
#include "FSEL/DragValue.hpp"
#include "FSEL/PopupSurface.hpp"
#include "FSEL/RadioChoice.hpp"
#include "FSEL/Slider.hpp"
#include "FSEL/Switch.hpp"
#include "FSEL/TextInput.hpp"
#include "devSystems/devInterface/Permanents/Backend/DevInterfaceIcons.hpp"
#include "devSystems/devInterface/Permanents/Backend/DevTheme.hpp"
#include "devSystems/devTooling/DevTooling.hpp"
#include "managers/UiManager.hpp"
#include "managers/ImageManager.hpp"
#include "managers/IconManager.hpp"

namespace FlowUi::devSystems::interface_elements {
namespace {

inline constexpr float kTitleHeight = 36.0f;
inline constexpr float kIdentityHeight = 68.0f;
inline constexpr float kTabsHeight = 38.0f;
inline constexpr LocalElementName kTabCard{"tab"};
inline constexpr LocalElementName kCards{"cards"};
inline constexpr LocalElementName kEditorCard{"editor-card"};
inline constexpr LocalElementName kCardHeader{"header"};
inline constexpr LocalElementName kFieldName{"field-name"};
inline constexpr LocalElementName kSemanticType{"semantic-type"};
inline constexpr LocalElementName kQuickValue{"quick-value"};
inline constexpr LocalElementName kHeaderSpacer{"header-spacer"};
inline constexpr LocalElementName kEditingField{"editing-field"};
inline constexpr LocalElementName kEditor{"editor"};
inline constexpr LocalElementName kNestedCard{"nested-card"};
inline constexpr LocalElementName kTypeName{"type-name"};
inline constexpr LocalElementName kTypeEditor{"type-editor"};
inline constexpr LocalElementName kEditorControl{"control"};
inline constexpr LocalElementName kEditorMessage{"editor-message"};
inline constexpr LocalElementName kSemanticChild{"semantic-child"};
inline constexpr LocalElementName kChangeHeader{"change-header"};
inline constexpr LocalElementName kChangeStep{"change-step"};
inline constexpr LocalElementName kChangeAction{"change-action"};
inline constexpr LocalElementName kSequenceUp{"sequence-up"};
inline constexpr LocalElementName kSequenceDown{"sequence-down"};
inline constexpr LocalElementName kSequenceRemove{"sequence-remove"};
inline constexpr uint16_t kMaximumEditorDepth = 8u;
inline constexpr std::size_t kMaximumVisibleSequenceItems = 64u;
inline constexpr uint16_t kTypeControlFontSize = 12u;
inline constexpr uint16_t kTypeControlHorizontalPadding = 8u;
inline constexpr uint16_t kTypeControlVerticalPadding = 6u;
inline constexpr float kTypeControlHeight =
	static_cast<float>(kTypeControlFontSize) * 1.5f +
	static_cast<float>(kTypeControlVerticalPadding * 2u) + 2.0f;
inline constexpr uint64_t kColorSemanticMode =
	std::numeric_limits<uint64_t>::max() - 1u;
inline constexpr uint64_t kPaddingSemanticMode =
	std::numeric_limits<uint64_t>::max() - 2u;
inline constexpr uint64_t kBorderSemanticMode =
	std::numeric_limits<uint64_t>::max() - 3u;
inline constexpr uint64_t kAttachmentSemanticMode =
	std::numeric_limits<uint64_t>::max() - 4u;
inline constexpr uint64_t kCornerRadiusSemanticMode =
	std::numeric_limits<uint64_t>::max() - 5u;

inline constexpr std::string_view kLinkIconSvg = R"svg(
<svg width="24" height="24" viewBox="0 0 24 24" fill="none" xmlns="http://www.w3.org/2000/svg">
  <path d="M10 13a5 5 0 0 0 7.54.54l3-3a5 5 0 0 0-7.07-7.07l-1.72 1.71M14 11a5 5 0 0 0-7.54-.54l-3 3a5 5 0 0 0 7.07 7.07l1.71-1.71" stroke="#FFFFFF" stroke-width="2" stroke-linecap="round" stroke-linejoin="round"/>
</svg>)svg";

inline constexpr Clay_Padding kTypeControlPadding{
	kTypeControlHorizontalPadding,
	kTypeControlHorizontalPadding,
	kTypeControlVerticalPadding,
	kTypeControlVerticalPadding,
};

struct TabSpec {
	DevInspectorTab tab;
	std::string_view label;
};

inline constexpr std::array<TabSpec, 4> kTabs{{
	{DevInspectorTab::Parameters, "Parameters"},
	{DevInspectorTab::State, "State"},
	{DevInspectorTab::Resources, "Resources"},
	{DevInspectorTab::Changes, "Changes"},
}};

inline constexpr std::array<FSEL::ComboBoxOption, 4> kSizingAxisOptions{{
	{.value = CLAY__SIZING_TYPE_FIT, .text = "Fit"},
	{.value = CLAY__SIZING_TYPE_GROW, .text = "Grow"},
	{.value = CLAY__SIZING_TYPE_PERCENT, .text = "Percent"},
	{.value = CLAY__SIZING_TYPE_FIXED, .text = "Fixed"},
}};

inline constexpr std::array<FSEL::ComboBoxOption, 4> kTextureFitOptions{{
	{.value = static_cast<std::uint64_t>(TextureFitMode::Stretch), .text = "Stretch"},
	{.value = static_cast<std::uint64_t>(TextureFitMode::Contain), .text = "Contain"},
	{.value = static_cast<std::uint64_t>(TextureFitMode::Cover), .text = "Cover"},
	{.value = static_cast<std::uint64_t>(TextureFitMode::None), .text = "None"},
}};

inline constexpr std::array<FSEL::ComboBoxOption, 2> kTextureSamplingOptions{{
	{.value = static_cast<std::uint64_t>(TextureSamplingMode::Linear), .text = "Linear · Renderer owned"},
	{.value = static_cast<std::uint64_t>(TextureSamplingMode::Nearest), .text = "Nearest · Renderer owned"},
}};

Clay_TextElementConfig textConfig(
	Clay_Color color,
	uint16_t size,
	Clay_TextAlignment alignment = CLAY_TEXT_ALIGN_LEFT) {
	Clay_TextElementConfig config{};
	config.textColor = color;
	config.fontSize = size;
	config.wrapMode = CLAY_TEXT_WRAP_NONE;
	config.textAlignment = alignment;
	return config;
}

Clay_ElementDeclaration fixedSection(
	float height,
	Clay_Color background,
	Clay_Padding padding = {}) {
	Clay_ElementDeclaration section{};
	section.layout.sizing = {
		.width = CLAY_SIZING_GROW(0),
		.height = CLAY_SIZING_FIXED(height),
	};
	section.layout.padding = padding;
	section.backgroundColor = background;
	section.border = {
		.color = interface_theme::kBorderPrimary,
		.width = Clay_BorderWidth{0, 0, 0, 1, 0},
	};
	return section;
}

const devMode::DevFieldSchema* fieldAt(
	const devMode::DevSchemaGeneration& schema,
	devMode::DevFieldIndex index) {
	if (!index || index.value > schema.fields.size()) return nullptr;
	return &schema.fields[index.value - 1u];
}

std::string_view schemaTypeName(
	const devMode::DevSchemaGeneration& schema,
	const devMode::DevTypeSchema* type) {
	if (!type) return "Unknown";
	std::string_view name = schema.string(type->displayName);
	if (name.empty()) name = schema.string(type->cppTypeName);
	return name.empty() ? std::string_view{"Unknown"} : name;
}

std::string normalizedTypeName(
	const devMode::DevSchemaGeneration& schema,
	const devMode::DevTypeSchema* type) {
	if (!type) return "Unknown";
	if (type->kind == devMode::DevTypeKind::Optional) {
		return "Optional " + normalizedTypeName(schema, schema.type(type->elementType));
	}
	switch (type->kind) {
	case devMode::DevTypeKind::Boolean: return "Boolean";
	case devMode::DevTypeKind::SignedInteger: return "Integer";
	case devMode::DevTypeKind::UnsignedInteger: return "Unsigned Integer";
	case devMode::DevTypeKind::FloatingPoint: return "Float";
	case devMode::DevTypeKind::Text: return "Text";
	case devMode::DevTypeKind::Sequence:
		return "Sequence of " + normalizedTypeName(schema, schema.type(type->elementType));
	default: break;
	}
	std::string name(schemaTypeName(schema, type));
	if (name.starts_with("std::")) name.erase(0, 5);
	if (name.starts_with("FlowUi::")) name.erase(0, 8);
	return name;
}

devMode::DevFieldIndex fieldIndex(
	const devMode::DevSchemaGeneration& schema,
	const devMode::DevFieldSchema& field) {
	return devMode::DevFieldIndex{
		static_cast<uint32_t>(&field - schema.fields.data()) + 1u};
}

devMode::DevEditorKind editorKind(
	const devMode::DevFieldSchema* field,
	const devMode::DevTypeSchema* type) {
	if (field && field->editor != devMode::DevEditorKind::None &&
		!(field->editor == devMode::DevEditorKind::OptionalGroup && type &&
			type->kind != devMode::DevTypeKind::Optional)) return field->editor;
	if (!type) return devMode::DevEditorKind::None;
	if (type->kind == devMode::DevTypeKind::Enumeration) {
		return devMode::DevEditorKind::EnumChoice;
	}
	if (type->kind == devMode::DevTypeKind::Optional) {
		return devMode::DevEditorKind::OptionalGroup;
	}
	return type->editor;
}

std::string_view editorName(devMode::DevEditorKind editor) {
	switch (editor) {
	case devMode::DevEditorKind::Toggle: return "Boolean";
	case devMode::DevEditorKind::SignedNumber: return "Signed Number";
	case devMode::DevEditorKind::UnsignedNumber: return "Unsigned Number";
	case devMode::DevEditorKind::FloatingNumber: return "Floating Number";
	case devMode::DevEditorKind::Text: return "Text";
	case devMode::DevEditorKind::EnumChoice: return "Enum";
	case devMode::DevEditorKind::Flags: return "Flags";
	case devMode::DevEditorKind::Color: return "Clay Color";
	case devMode::DevEditorKind::Vector: return "Clay Vector";
	case devMode::DevEditorKind::Spacing: return "Clay Spacing";
	case devMode::DevEditorKind::CornerRadius: return "Clay Corner Radius";
	case devMode::DevEditorKind::Sizing: return "Clay Sizing";
	case devMode::DevEditorKind::SizingAxis: return "Clay Sizing Axis";
	case devMode::DevEditorKind::AttachmentPoints: return "Attachment Points";
	case devMode::DevEditorKind::ObjectGroup: return "Object";
	case devMode::DevEditorKind::OptionalGroup: return "Optional";
	case devMode::DevEditorKind::Sequence: return "Sequence";
	case devMode::DevEditorKind::FontChoice: return "Font";
	case devMode::DevEditorKind::ActionChoice: return "Action";
	case devMode::DevEditorKind::ResourceChoice: return "Resource";
	case devMode::DevEditorKind::Custom: return "Custom";
	case devMode::DevEditorKind::None: return "Unsupported";
	}
	return "Unsupported";
}

std::string_view claySemanticName(
	const devMode::DevSchemaGeneration& schema,
	const devMode::DevTypeSchema* type) {
	if (!type) return {};
	std::string_view name = schema.string(type->displayName);
	if (name.empty()) name = schema.string(type->cppTypeName);
	if (name == "Clay_Color") return "Clay Color";
	if (name == "Clay_Padding") return "Clay Spacing";
	if (name == "Clay_BorderWidth") return "Clay Border Width";
	if (name == "Clay_CornerRadius") return "Clay Corner Radius";
	if (name == "Clay_Vector2" || name == "Clay_Dimensions" ||
		name == "Clay_SizingMinMax") return "Clay Vector";
	if (name == "Clay_ChildAlignment") return "Clay Alignment";
	if (name == "Clay_FloatingAttachPoints") return "Clay Attach Points";
	if (name == "Clay_AspectRatioElementConfig") return "Clay Aspect Ratio";
	if (name == "Clay_SizingAxis") return "Clay Sizing Axis";
	if (name == "Clay_Sizing") return "Clay Sizing";
	if (name == "Clay_LayoutConfig") return "Clay Layout";
	if (name == "Clay_TextElementConfig") return "Clay Text Config";
	if (name == "Clay_FloatingElementConfig") return "Clay Floating Config";
	if (name == "Clay_ClipElementConfig") return "Clay Clip Config";
	if (name == "Clay_BorderElementConfig") return "Clay Border Config";
	if (name == "Clay_ElementDeclaration") return "Clay Element Declaration";
	return {};
}

std::string semanticName(
	const devMode::DevSchemaGeneration& schema,
	const devMode::DevFieldSchema* field,
	const devMode::DevTypeSchema* type) {
	if (const std::string_view clayName = claySemanticName(schema, type);
		!clayName.empty()) return std::string(clayName);
	const devMode::DevEditorKind editor = editorKind(field, type);
	if (editor != devMode::DevEditorKind::OptionalGroup || !type) {
		return std::string(editorName(editor));
	}
	const devMode::DevTypeSchema* valueType = schema.type(type->elementType);
	if (!valueType) return "Optional";
	return std::string("Optional · ") + semanticName(schema, nullptr, valueType);
}

bool editableCapability(devMode::DevEditCapability capability) noexcept {
	return capability == devMode::DevEditCapability::Editable ||
		capability == devMode::DevEditCapability::PartiallyEditable ||
		capability == devMode::DevEditCapability::SemanticCommand;
}

std::string boundedText(std::string text, std::size_t maximum = 56u) {
	if (text.size() <= maximum) return text;
	text.resize(maximum > 3u ? maximum - 3u : 0u);
	text += "...";
	return text;
}

std::uint64_t catalogueIdentity(std::uint8_t domain, std::string_view key) noexcept {
	std::uint64_t hash = 1469598103934665603ull ^ domain;
	for (const unsigned char byte : key) {
		hash ^= byte;
		hash *= 1099511628211ull;
	}
	return hash == 0u ? 1u : hash;
}

std::uint64_t catalogueNumericIdentity(std::uint8_t domain, std::uint64_t value) noexcept {
	value ^= static_cast<std::uint64_t>(domain) * 0x9e3779b97f4a7c15ull;
	value ^= value >> 30u;
	value *= 0xbf58476d1ce4e5b9ull;
	value ^= value >> 27u;
	value *= 0x94d049bb133111ebull;
	value ^= value >> 31u;
	return value == 0u ? static_cast<std::uint64_t>(domain) + 1u : value;
}

std::string_view resourceStatusName(devMode::DevResourceStatus status) noexcept {
	switch (status) {
	case devMode::DevResourceStatus::Unloaded: return "Unloaded";
	case devMode::DevResourceStatus::Loading: return "Loading";
	case devMode::DevResourceStatus::Ready: return "Ready";
	case devMode::DevResourceStatus::Failed: return "Failed";
	case devMode::DevResourceStatus::Retiring: return "Retiring";
	}
	return "Unknown";
}

std::string escapedSingleLine(std::string_view text) {
	std::string result;
	result.reserve(std::min<std::size_t>(text.size(), 56u));
	for (char character : text) {
		if (result.size() >= 56u) break;
		switch (character) {
		case '\n': result += "\\n"; break;
		case '\r': result += "\\r"; break;
		case '\t': result += "\\t"; break;
		default:
			if (static_cast<unsigned char>(character) >= 0x20u) result.push_back(character);
			break;
		}
	}
	if (result.size() < text.size()) result += "...";
	return boundedText(std::move(result));
}

std::string sizingAxisQuick(std::string_view prefix, const Clay_SizingAxis& axis) {
	char buffer[64]{};
	switch (axis.type) {
	case CLAY__SIZING_TYPE_FIT:
		std::snprintf(buffer, sizeof(buffer), "%.*s Fit %.0f-%.0f",
			static_cast<int>(prefix.size()), prefix.data(), axis.size.minMax.min, axis.size.minMax.max);
		break;
	case CLAY__SIZING_TYPE_GROW:
		std::snprintf(buffer, sizeof(buffer), "%.*s Grow %.0f-%.0f",
			static_cast<int>(prefix.size()), prefix.data(), axis.size.minMax.min, axis.size.minMax.max);
		break;
	case CLAY__SIZING_TYPE_PERCENT:
		std::snprintf(buffer, sizeof(buffer), "%.*s %.0f%%",
			static_cast<int>(prefix.size()), prefix.data(), axis.size.percent * 100.0f);
		break;
	case CLAY__SIZING_TYPE_FIXED:
		std::snprintf(buffer, sizeof(buffer), "%.*s %.0f px",
			static_cast<int>(prefix.size()), prefix.data(), axis.size.minMax.min);
		break;
	default:
		std::snprintf(buffer, sizeof(buffer), "%.*s Unknown",
			static_cast<int>(prefix.size()), prefix.data());
		break;
	}
	return buffer;
}

DevQuickView quickView(
	const devMode::DevSchemaGeneration& schema,
	const devMode::DevTypeSchema* type,
	const void* value,
	App* app = nullptr,
	const devMode::DevFieldSchema* field = nullptr) {
	if (!type) return DevQuickView{
		.kind = DevQuickView::Kind::Status,
		.primary = "Unregistered · Unknown",
		.status = DevQuickStatus::Warning};
	if (!value) return DevQuickView{
		.kind = DevQuickView::Kind::Status,
		.primary = "Value unavailable",
		.status = DevQuickStatus::Unavailable};
	const std::size_t typeIndex = static_cast<std::size_t>(type - schema.types.data());
	const devMode::DevTypeOps* operations = typeIndex < schema.typeOperations.size()
		? schema.typeOperations[typeIndex] : nullptr;
	if (type->kind == devMode::DevTypeKind::Optional) {
		if (!operations || !operations->optionalHasValue || !operations->optionalValueAddress) {
			return DevQuickView{.kind = DevQuickView::Kind::Status,
				.primary = "Value unavailable", .status = DevQuickStatus::Unavailable};
		}
		if (!operations->optionalHasValue(value)) return DevQuickView{.primary = "NullOpt"};
		return quickView(
			schema, schema.type(type->elementType),
			operations->optionalValueAddress(value), app, field);
	}
	const std::string_view name = schemaTypeName(schema, type);
	char buffer[128]{};
	if (name == "Clay_Padding") {
		const auto& padding = *static_cast<const Clay_Padding*>(value);
		if (padding.left == padding.right && padding.left == padding.top &&
			padding.left == padding.bottom) {
			std::snprintf(buffer, sizeof(buffer), "%u all", padding.left);
			return DevQuickView{.kind = DevQuickView::Kind::Spacing, .primary = buffer};
		}
		std::snprintf(buffer, sizeof(buffer), "T %u · R %u", padding.top, padding.right);
		char second[64]{};
		std::snprintf(second, sizeof(second), "B %u · L %u", padding.bottom, padding.left);
		return DevQuickView{.kind = DevQuickView::Kind::Spacing,
			.primary = buffer, .secondary = second};
	}
	if (name == "Clay_BorderWidth") {
		const auto& width = *static_cast<const Clay_BorderWidth*>(value);
		if (width.left == width.right && width.left == width.top &&
			width.left == width.bottom) {
			std::snprintf(buffer, sizeof(buffer), "%u all · Between %u",
				width.left, width.betweenChildren);
		} else {
			std::snprintf(buffer, sizeof(buffer), "L%u R%u T%u B%u · Between %u",
				width.left, width.right, width.top, width.bottom, width.betweenChildren);
		}
		return DevQuickView{.kind = DevQuickView::Kind::Spacing, .primary = buffer};
	}
	if (name == "Clay_Color") {
		const auto& color = *static_cast<const Clay_Color*>(value);
		std::snprintf(buffer, sizeof(buffer), "#%02X%02X%02X%02X",
			static_cast<unsigned>(std::lround(std::clamp(color.r, 0.0f, 255.0f))),
			static_cast<unsigned>(std::lround(std::clamp(color.g, 0.0f, 255.0f))),
			static_cast<unsigned>(std::lround(std::clamp(color.b, 0.0f, 255.0f))),
			static_cast<unsigned>(std::lround(std::clamp(color.a, 0.0f, 255.0f))));
		return DevQuickView{.kind = DevQuickView::Kind::Color,
			.primary = buffer, .swatch = color};
	}
	if (type->kind == devMode::DevTypeKind::Boolean) {
		long double numeric = 0.0L;
		return DevQuickView{.primary = operations && operations->numericValue &&
			operations->numericValue(value, numeric) ? (numeric == 0.0L ? "Off" : "On")
			: "Value unavailable"};
	}
	if (field && field->choiceDomain != devMode::DevChoiceDomain::None &&
		(field->choiceDomain == devMode::DevChoiceDomain::FontFace ||
		 field->choiceDomain == devMode::DevChoiceDomain::FontFamily)) {
		long double numeric = 0.0L;
		if (operations && operations->numericValue && operations->numericValue(value, numeric)) {
			const std::uint64_t identity = static_cast<std::uint64_t>(numeric);
			if (app) for (const devMode::DevFontCatalogEntry& font :
				app->devTooling().catalogues().queryFonts()) {
				const std::uint64_t candidate = field->choiceDomain == devMode::DevChoiceDomain::FontFamily
					? static_cast<std::uint64_t>(font.familyHandle)
					: static_cast<std::uint64_t>(font.fontHandle);
				if (candidate != identity) continue;
				std::string label(font.familyName);
				if (field->choiceDomain == devMode::DevChoiceDomain::FontFace) {
					label += " · ";
					label += font.faceName.empty() ? "Regular" : std::string(font.faceName);
				}
				return DevQuickView{.primary = std::move(label)};
			}
			return DevQuickView{.kind = DevQuickView::Kind::Status,
				.primary = "Missing font · " + std::to_string(identity),
				.status = DevQuickStatus::Missing};
		}
	}
	if (name == "ActionCall") {
		const auto& call = *static_cast<const ActionCall*>(value);
		if (!call) return DevQuickView{.primary = "None"};
		if (app) {
			if (const std::optional<ActionDebugInfo> info = app->actions().debugInfo(call)) {
				return DevQuickView{.primary = std::string(
					info->kind == ActionCallKind::App ? "APP " : "UI ") +
					std::string(info->debugName),
					.status = info->bound ? DevQuickStatus::Normal : DevQuickStatus::Missing};
			}
		}
		return DevQuickView{.kind = DevQuickView::Kind::Status,
			.primary = "Missing action", .status = DevQuickStatus::Missing};
	}
	if (name == "TextureRef") {
		const auto& texture = *static_cast<const TextureRef*>(value);
		if (!texture.handle && !texture.sourceKey.empty()) {
			return DevQuickView{.primary = std::string(texture.sourceKey)};
		}
		if (!texture.handle) return DevQuickView{.kind = DevQuickView::Kind::Status,
			.primary = "No texture", .status = DevQuickStatus::Missing};
		if (app) {
			for (const devMode::DevImageCatalogEntry& image :
				app->devTooling().catalogues().queryImages()) {
				if (image.textureHandle != texture.handle) continue;
				return DevQuickView{.primary = std::string(image.key.name),
					.status = image.status == devMode::DevResourceStatus::Ready
						? DevQuickStatus::Normal : DevQuickStatus::Warning};
			}
			for (const devMode::DevIconCatalogEntry& icon :
				app->devTooling().catalogues().queryIcons()) {
				if (icon.atlasTexture != texture.handle ||
					std::abs(icon.atlasUv.x - texture.uv0x) > 0.00001f ||
					std::abs(icon.atlasUv.y - texture.uv0y) > 0.00001f) continue;
				return DevQuickView{.primary = "Icon · " + std::string(icon.iconName)};
			}
		}
		std::snprintf(buffer, sizeof(buffer), "Anonymous texture · %016llX",
			static_cast<unsigned long long>(texture.handle.packed()));
		return DevQuickView{.kind = DevQuickView::Kind::Status,
			.primary = buffer, .status = DevQuickStatus::Warning};
	}
	if (name == "Clay_CornerRadius") {
		const auto& radius = *static_cast<const Clay_CornerRadius*>(value);
		if (radius.topLeft == radius.topRight && radius.topLeft == radius.bottomRight &&
			radius.topLeft == radius.bottomLeft) {
			std::snprintf(buffer, sizeof(buffer), "%.0f all", radius.topLeft);
		} else {
			std::snprintf(buffer, sizeof(buffer), "TL%.0f TR%.0f BR%.0f BL%.0f",
				radius.topLeft, radius.topRight, radius.bottomRight, radius.bottomLeft);
		}
		return DevQuickView{.primary = buffer};
	}
	if (name == "Clay_Dimensions") {
		const auto& dimensions = *static_cast<const Clay_Dimensions*>(value);
		std::snprintf(buffer, sizeof(buffer), "%.2f × %.2f",
			dimensions.width, dimensions.height);
		return DevQuickView{.primary = buffer};
	}
	if (name == "Clay_Vector2") {
		const auto& vector = *static_cast<const Clay_Vector2*>(value);
		std::snprintf(buffer, sizeof(buffer), "%.2f, %.2f", vector.x, vector.y);
		return DevQuickView{.primary = buffer};
	}
	if (name == "Clay_SizingMinMax") {
		const auto& range = *static_cast<const Clay_SizingMinMax*>(value);
		std::snprintf(buffer, sizeof(buffer), "%.2f – %.2f", range.min, range.max);
		return DevQuickView{.primary = buffer};
	}
	if (name == "Clay_SizingAxis") {
		const auto& axis = *static_cast<const Clay_SizingAxis*>(value);
		return DevQuickView{.kind = DevQuickView::Kind::Sizing,
			.primary = sizingAxisQuick("", axis)};
	}
	if (name == "Clay_Sizing") {
		const auto& sizing = *static_cast<const Clay_Sizing*>(value);
		return DevQuickView{.kind = DevQuickView::Kind::Sizing,
			.primary = sizingAxisQuick("W", sizing.width),
			.secondary = sizingAxisQuick("H", sizing.height)};
	}
	if (type->kind == devMode::DevTypeKind::Enumeration &&
		type->enumeration.values.first <= schema.enumValues.size() &&
		type->enumeration.values.count <=
			schema.enumValues.size() - type->enumeration.values.first) {
		long double numeric = 0.0L;
		if (operations && operations->numericValue && operations->numericValue(value, numeric)) {
			const uint64_t bits = static_cast<uint64_t>(numeric);
			for (uint32_t index = 0; index < type->enumeration.values.count; ++index) {
				const devMode::DevEnumValueSchema& option =
					schema.enumValues[type->enumeration.values.first + index];
				if (option.bits == bits) return DevQuickView{
					.primary = std::string(schema.string(option.name))};
			}
			std::snprintf(buffer, sizeof(buffer), "Unknown · %llu",
				static_cast<unsigned long long>(bits));
			return DevQuickView{.kind = DevQuickView::Kind::Status,
				.primary = buffer, .status = DevQuickStatus::Warning};
		}
	}
	if (type->kind == devMode::DevTypeKind::SignedInteger ||
		type->kind == devMode::DevTypeKind::UnsignedInteger ||
		type->kind == devMode::DevTypeKind::FloatingPoint) {
		long double numeric = 0.0L;
		if (operations && operations->numericValue && operations->numericValue(value, numeric)) {
			std::ostringstream stream;
			if (type->kind == devMode::DevTypeKind::FloatingPoint) {
				stream << std::setprecision(6) << std::defaultfloat << numeric;
			} else stream << std::fixed << std::setprecision(0) << numeric;
			return DevQuickView{.primary = stream.str()};
		}
	}
	if (type->kind == devMode::DevTypeKind::Text && operations && operations->textView) {
		return DevQuickView{.primary = escapedSingleLine(operations->textView(value))};
	}
	if (type->kind == devMode::DevTypeKind::Sequence && operations && operations->sequenceSize) {
		std::snprintf(buffer, sizeof(buffer), "%zu items", operations->sequenceSize(value));
		return DevQuickView{.primary = buffer};
	}
	if (type->kind == devMode::DevTypeKind::Object) {
		std::snprintf(buffer, sizeof(buffer), "%u fields", type->fields.count);
		return DevQuickView{.primary = buffer};
	}
	if (type->kind == devMode::DevTypeKind::Pointer && operations && operations->pointerValue) {
		const void* pointer = nullptr;
		if (operations->pointerValue(value, pointer)) {
			if (!pointer) return DevQuickView{.primary = "Null"};
			std::snprintf(buffer, sizeof(buffer), "Address · %p", pointer);
			return DevQuickView{.primary = buffer};
		}
	}
	if (type->kind == devMode::DevTypeKind::Opaque) {
		std::snprintf(buffer, sizeof(buffer), "Opaque · %u bytes", type->size);
		return DevQuickView{.kind = DevQuickView::Kind::Status, .primary = buffer};
	}
	return DevQuickView{.kind = DevQuickView::Kind::Status,
		.primary = boundedText("Unregistered · " + normalizedTypeName(schema, type)),
		.status = DevQuickStatus::Warning};
}

std::string roleName(DevEditorRole role) {
	switch (role) {
	case DevEditorRole::Parameters: return "Parameters";
	case DevEditorRole::State: return "State";
	case DevEditorRole::Resources: return "Resources";
	case DevEditorRole::Changes: return "Changes";
	}
	return "Unknown";
}

tooling::DevOverrideCommand resolvedOverrideCommand(
	const devMode::DevSchemaGeneration& schema,
	const DevEditorBinding& binding,
	tooling::DevOverrideCommandKind kind,
	tooling::DevOwnedValue value = {}) {
	const devMode::DevFieldSchema* field = fieldAt(schema, binding.field);
	tooling::DevOverrideCommand command{};
	command.kind = kind;
	command.element = tooling::DevElementOverrideTarget{
		.definition = binding.definition,
		.window = binding.window,
		.instance = binding.instance,
		.instanceDebugLabel = binding.elementName,
		.scope = tooling::DevOverrideScope::ExactInstance,
		.bakeable = true,
	};
	command.field = tooling::DevOverrideFieldKey{
		.ownerType = binding.rootOwnerType,
		.field = field ? field->id : 0u,
		.nestedPath = binding.nestedPath,
	};
	command.layer = tooling::DevOverrideLayer::LiveInstance;
	command.value = std::move(value);
	return command;
}

tooling::DevOverrideCommand fieldCommand(
	const devMode::DevSchemaGeneration& schema,
	const DevEditorCardState& card,
	tooling::DevOverrideCommandKind kind,
	tooling::DevOwnedValue value = {}) {
	return resolvedOverrideCommand(schema, card.binding, kind, std::move(value));
}

tooling::DevOverrideCommand editorFieldCommand(
	const devMode::DevSchemaGeneration& schema,
	const DevTypeEditorState& editor,
	tooling::DevOverrideCommandKind kind,
	tooling::DevOwnedValue value = {}) {
	return resolvedOverrideCommand(schema, editor.binding, kind, std::move(value));
}

bool updateEditorPreview(DevTypeEditorState& editor) {
	if (!editor.app || !editor.interfaceState || !editor.editable ||
		!editor.currentValue) return false;
	const devMode::DevSchemaView schema = editor.app->devTooling().schemas().view();
	if (!schema) return false;
	tooling::DevChangeSet changes{};
	changes.transaction = editor.interfaceState->nextEditTransaction++;
	changes.commands.push_back(editorFieldCommand(
		*schema,
		editor,
		editor.previewActive
			? tooling::DevOverrideCommandKind::UpdateBatchDrag
			: tooling::DevOverrideCommandKind::BeginBatchDrag,
		editor.currentValue));
	if (!editor.app->devTooling().overrides().submit(std::move(changes))) return false;
	editor.previewActive = true;
	return true;
}

void cancelEditorPreview(DevTypeEditorState& editor) {
	if (!editor.previewActive || !editor.app || !editor.interfaceState) return;
	const devMode::DevSchemaView schema = editor.app->devTooling().schemas().view();
	if (!schema) return;
	tooling::DevOverrideCommand clear = editorFieldCommand(
		*schema, editor, tooling::DevOverrideCommandKind::ClearElementField);
	clear.layer = tooling::DevOverrideLayer::EphemeralPreview;
	tooling::DevChangeSet changes{};
	changes.transaction = editor.interfaceState->nextEditTransaction++;
	changes.commands.push_back(std::move(clear));
	if (editor.app->devTooling().overrides().submit(std::move(changes))) {
		editor.previewActive = false;
	}
}

const tooling::DevOverrideApply::Record* currentEditorOverride(
	const App& app,
	const devMode::DevSchemaGeneration& schema,
	const DevTypeEditorState& editor) {
	const tooling::DevOverrideCommand key = editorFieldCommand(
		schema, editor, tooling::DevOverrideCommandKind::ClearElementField);
	const auto& records = app.devTooling().overrides().appliedOverrides().records();
	const auto found = std::ranges::find_if(records,
		[&key](const tooling::DevOverrideApply::Record& record) {
			return record.target == key.element && record.field == key.field &&
				record.layer == key.layer;
		});
	return found == records.end() ? nullptr : &*found;
}

const tooling::DevOverrideApply::Record* currentOverride(
	const App& app,
	const devMode::DevSchemaGeneration& schema,
	const DevEditorCardState& card) {
	const tooling::DevOverrideCommand key = fieldCommand(
		schema, card, tooling::DevOverrideCommandKind::ClearElementField);
	const auto& records = app.devTooling().overrides().appliedOverrides().records();
	const auto found = std::ranges::find_if(
		records,
		[&key](const tooling::DevOverrideApply::Record& record) {
			return record.target == key.element && record.field == key.field &&
				record.layer == key.layer;
		});
	return found == records.end() ? nullptr : &*found;
}

enum class OverrideRelation : std::uint8_t { None, Exact, Descendant };

OverrideRelation overrideRelation(
	const tooling::DevOverrideApply::Record& record,
	const tooling::DevOverrideCommand& key) {
	if (!(record.target == key.element) ||
		record.field.ownerType != key.field.ownerType ||
		record.field.nestedPath.size() < key.field.nestedPath.size() ||
		!std::equal(key.field.nestedPath.begin(), key.field.nestedPath.end(),
			record.field.nestedPath.begin())) return OverrideRelation::None;
	if (record.field.nestedPath.size() == key.field.nestedPath.size()) {
		return record.field.field == key.field.field
			? OverrideRelation::Exact : OverrideRelation::None;
	}
	return record.field.nestedPath[key.field.nestedPath.size()] == key.field.field
		? OverrideRelation::Descendant : OverrideRelation::None;
}

DevOverridePresence overridePresence(
	const App& app,
	const devMode::DevSchemaGeneration& schema,
	const DevEditorCardState& card) {
	const tooling::DevOverrideCommand key = fieldCommand(
		schema, card, tooling::DevOverrideCommandKind::ClearElementField);
	const auto& records = app.devTooling().overrides().appliedOverrides().records();
	DevOverridePresence result{};
	for (const tooling::DevOverrideApply::Record& record : records) {
		const OverrideRelation relation = overrideRelation(record, key);
		if (relation == OverrideRelation::None) continue;
		if (record.layer == tooling::DevOverrideLayer::EphemeralPreview) {
			result.preview = true;
		} else if (record.layer == tooling::DevOverrideLayer::LiveInstance) {
			if (relation == OverrideRelation::Exact) result.exactLive = true;
			else {
				result.descendantLive = true;
				++result.descendantCount;
			}
		}
	}
	return result;
}

bool submitEditTransaction(
	App& app,
	DevInterfaceState& state,
	DevInterfaceEditTransaction transaction) {
	tooling::DevChangeSet changes{
		.transaction = state.nextEditTransaction++,
		.commands = transaction.forward,
	};
	if (!changes.commands.empty() &&
		!app.devTooling().overrides().submit(std::move(changes))) return false;
	state.editUndoStack.push_back(std::move(transaction));
	state.editRedoStack.clear();
	return true;
}

bool commitEditorValue(DevTypeEditorState& editor, std::string description) {
	if (!editor.app || !editor.interfaceState || !editor.editable ||
		!editor.currentValue) return false;
	App& app = *editor.app;
	DevInterfaceState& state = *editor.interfaceState;
	const devMode::DevSchemaView schema = app.devTooling().schemas().view();
	if (!schema) return false;
	DevInterfaceEditTransaction transaction{};
	transaction.description = std::move(description);
	transaction.forward.push_back(editorFieldCommand(
		*schema, editor,
		editor.previewActive
			? tooling::DevOverrideCommandKind::EndBatchDrag
			: tooling::DevOverrideCommandKind::SetElementField,
		editor.currentValue));
	if (const tooling::DevOverrideApply::Record* prior =
		currentEditorOverride(app, *schema, editor)) {
		transaction.inverse.push_back(editorFieldCommand(
			*schema, editor, tooling::DevOverrideCommandKind::SetElementField,
			prior->value));
	} else {
		transaction.inverse.push_back(editorFieldCommand(
			*schema, editor, tooling::DevOverrideCommandKind::ClearElementField));
	}
	if (!submitEditTransaction(app, state, std::move(transaction))) return false;
	editor.previewActive = false;
	state.lastActionMessage = editor.binding.elementName + " · " +
		editor.fieldName + " updated";
	return true;
}

const devMode::DevTypeOps* typeOperations(
	const devMode::DevSchemaGeneration& schema,
	devMode::DevTypeIndex index) {
	return index.value < schema.typeOperations.size()
		? schema.typeOperations[index.value] : nullptr;
}

std::string numericDraft(
	const devMode::DevTypeSchema& type,
	const devMode::DevTypeOps* operations,
	const void* value) {
	long double numeric = 0.0L;
	if (!operations || !operations->numericValue ||
		!operations->numericValue(value, numeric)) return {};
	std::ostringstream stream;
	if (type.kind == devMode::DevTypeKind::FloatingPoint) {
		stream << std::setprecision(std::numeric_limits<long double>::digits10) << numeric;
	} else {
		stream << std::fixed << std::setprecision(0) << numeric;
	}
	return stream.str();
}

bool parseNumericDraft(std::string_view draft, long double& value) {
	if (draft.empty()) return false;
	std::string storage(draft);
	char* end = nullptr;
	errno = 0;
	value = std::strtold(storage.c_str(), &end);
	return errno != ERANGE && end == storage.c_str() + storage.size() &&
		std::isfinite(value);
}

bool applyEditorDraft(DevTypeEditorState& editor) {
	if (!editor.app || !editor.currentValue || !editor.editValue) return false;
	const devMode::DevSchemaView schema = editor.app->devTooling().schemas().view();
	const devMode::DevTypeSchema* type = schema ? schema->type(editor.operationType) : nullptr;
	const devMode::DevTypeOps* operations = schema
		? typeOperations(*schema, editor.operationType) : nullptr;
	if (!type || !operations) return false;
	devMode::DevValueOperationStatus status = devMode::DevValueOperationStatus::Unsupported;
	if (type->kind == devMode::DevTypeKind::Text && operations->assignTextValue) {
		status = operations->assignTextValue(editor.editValue, editor.draft);
	} else if (operations->assignNumericValue) {
		long double candidate = 0.0L;
		if (parseNumericDraft(editor.draft, candidate)) {
			status = operations->assignNumericValue(editor.editValue, candidate);
		}
	}
	editor.draftValid = status == devMode::DevValueOperationStatus::Success;
	return editor.draftValid;
}

constexpr auto kPreviewEditorDraft = UiAction(
	"flowui.dev_interface.type-editor.preview-draft",
	[](DevTypeEditorState& editor) {
		if (applyEditorDraft(editor)) (void)updateEditorPreview(editor);
	});

constexpr auto kCommitEditorDraft = UiAction(
	"flowui.dev_interface.type-editor.commit-draft",
	[](DevTypeEditorState& editor) {
		if (applyEditorDraft(editor)) {
			(void)commitEditorValue(editor, "Edit " + editor.fieldName);
		} else {
			cancelEditorPreview(editor);
		}
	});

bool applyEditorDragValue(DevTypeEditorState& editor) {
	if (!editor.app || !editor.currentValue || !editor.editValue) return false;
	const devMode::DevSchemaView schema = editor.app->devTooling().schemas().view();
	const devMode::DevTypeSchema* type = schema ? schema->type(editor.operationType) : nullptr;
	const devMode::DevTypeOps* operations = schema
		? typeOperations(*schema, editor.operationType) : nullptr;
	if (!type || !operations || !operations->assignNumericValue) return false;

	long double candidate = 0.0L;
	switch (type->kind) {
	case devMode::DevTypeKind::SignedInteger:
		candidate = static_cast<long double>(editor.dragSigned);
		break;
	case devMode::DevTypeKind::UnsignedInteger:
		candidate = static_cast<long double>(editor.dragUnsigned);
		break;
	case devMode::DevTypeKind::FloatingPoint:
		candidate = static_cast<long double>(editor.dragFloat);
		break;
	default:
		return false;
	}
	return operations->assignNumericValue(editor.editValue, candidate) ==
		devMode::DevValueOperationStatus::Success;
}

constexpr auto kPreviewEditorDragValue = UiAction(
	"flowui.dev_interface.type-editor.preview-drag-value",
	[](DevTypeEditorState& editor) {
		if (applyEditorDragValue(editor)) (void)updateEditorPreview(editor);
	});

constexpr auto kCommitEditorDragValue = UiAction(
	"flowui.dev_interface.type-editor.commit-drag-value",
	[](DevTypeEditorState& editor) {
		if (applyEditorDragValue(editor)) {
			(void)commitEditorValue(editor, "Edit " + editor.fieldName);
		}
	});

constexpr auto kCancelEditorDragValue = UiAction(
	"flowui.dev_interface.type-editor.cancel-drag-value",
	[](DevTypeEditorState& editor) {
		(void)applyEditorDragValue(editor);
		cancelEditorPreview(editor);
	});

constexpr auto kToggleEditorValue = UiAction(
	"flowui.dev_interface.type-editor.toggle",
	[](DevTypeEditorState& editor) {
		if (!editor.app || !editor.currentValue) return;
		const devMode::DevSchemaView schema = editor.app->devTooling().schemas().view();
		const devMode::DevTypeOps* operations = schema
			? typeOperations(*schema, editor.operationType) : nullptr;
		long double current = 0.0L;
		if (!operations || !operations->numericValue || !operations->assignNumericValue ||
			!operations->numericValue(editor.editValue, current)) return;
		if (operations->assignNumericValue(editor.editValue, current == 0.0L ? 1.0L : 0.0L) ==
			devMode::DevValueOperationStatus::Success) {
			(void)commitEditorValue(editor, "Toggle " + editor.fieldName);
		}
	});

constexpr auto kChooseEditorEnum = UiAction(
	"flowui.dev_interface.type-editor.choose-enum",
	[](DevTypeEditorState& editor) {
		if (!editor.app || !editor.currentValue) return;
		const devMode::DevSchemaView schema = editor.app->devTooling().schemas().view();
		const devMode::DevTypeOps* operations = schema
			? typeOperations(*schema, editor.operationType) : nullptr;
		if (operations && operations->assignNumericValue &&
			operations->assignNumericValue(editor.editValue,
				static_cast<long double>(editor.enumSelection)) ==
				devMode::DevValueOperationStatus::Success) {
			(void)commitEditorValue(editor, "Choose " + editor.fieldName);
		}
	});

constexpr auto kChooseCatalogueNumeric = UiAction(
	"flowui.dev_interface.type-editor.choose-catalogue-numeric",
	[](DevTypeEditorState& editor) {
		if (!editor.app || !editor.currentValue || !editor.editValue) return;
		const devMode::DevSchemaView schema = editor.app->devTooling().schemas().view();
		const devMode::DevTypeOps* operations = schema
			? typeOperations(*schema, editor.operationType) : nullptr;
		if (operations && operations->assignNumericValue &&
			operations->assignNumericValue(editor.editValue,
				static_cast<long double>(editor.enumSelection)) ==
				devMode::DevValueOperationStatus::Success) {
			(void)commitEditorValue(editor, "Choose " + editor.fieldName);
		}
	});

constexpr auto kToggleEditorFlag = UiAction(
	"flowui.dev_interface.type-editor.toggle-flag",
	[](DevTypeEditorState& editor, uint64_t mask) {
		if (!editor.app || !editor.currentValue) return;
		const auto schema = editor.app->devTooling().schemas().view();
		const auto* operations = schema ? typeOperations(*schema, editor.operationType) : nullptr;
		long double current = 0.0L;
		if (!operations || !operations->numericValue || !operations->assignNumericValue ||
			!operations->numericValue(editor.editValue, current)) return;
		const uint64_t replacement = static_cast<uint64_t>(current) ^ mask;
		if (operations->assignNumericValue(editor.editValue, static_cast<long double>(replacement)) ==
			devMode::DevValueOperationStatus::Success) {
			(void)commitEditorValue(editor, "Toggle flag in " + editor.fieldName);
		}
	});

constexpr auto kToggleOptional = UiAction(
	"flowui.dev_interface.type-editor.toggle-optional",
	[](DevTypeEditorState& editor) {
		if (!editor.app || !editor.currentValue) return;
		const devMode::DevSchemaView schema = editor.app->devTooling().schemas().view();
		const devMode::DevTypeOps* operations = schema
			? typeOperations(*schema, editor.typeIndex) : nullptr;
		if (!operations || !operations->optionalHasValue || !operations->setOptionalPresence) return;
		const bool present = operations->optionalHasValue(editor.currentValue.data());
		if (operations->setOptionalPresence(editor.currentValue.data(), !present) ==
			devMode::DevValueOperationStatus::Success) {
			(void)commitEditorValue(editor, present ? "Clear " + editor.fieldName
				: "Enable " + editor.fieldName);
		}
	});

constexpr auto kChooseSizingAxisMode = UiAction(
	"flowui.dev_interface.type-editor.sizing-axis-mode",
	[](DevTypeEditorState& editor) {
		if (!editor.currentValue) return;
		auto& axis = *static_cast<Clay_SizingAxis*>(editor.currentValue.data());
		const auto type = static_cast<Clay__SizingType>(editor.enumSelection);
		if (axis.type == type) return;
		switch (type) {
		case CLAY__SIZING_TYPE_FIT: axis = CLAY_SIZING_FIT(0); break;
		case CLAY__SIZING_TYPE_GROW: axis = CLAY_SIZING_GROW(0); break;
		case CLAY__SIZING_TYPE_PERCENT: axis = CLAY_SIZING_PERCENT(0.5f); break;
		case CLAY__SIZING_TYPE_FIXED: axis = CLAY_SIZING_FIXED(100.0f); break;
		}
		editor.semanticMode = std::numeric_limits<uint64_t>::max();
		(void)commitEditorValue(editor, "Change sizing mode for " + editor.fieldName);
	});

bool applySizingAxisValues(DevTypeEditorState& editor) {
	if (!editor.currentValue || !editor.editValue) return false;
	auto& axis = *static_cast<Clay_SizingAxis*>(editor.editValue);
	switch (axis.type) {
	case CLAY__SIZING_TYPE_PERCENT:
		editor.semanticSliderValue = std::clamp(
			editor.semanticSliderValue, 0.0, 100.0);
		axis.size.percent = static_cast<float>(editor.semanticSliderValue / 100.0);
		break;
	case CLAY__SIZING_TYPE_FIXED:
		editor.semanticValueA = std::max(editor.semanticValueA, 0.0f);
		axis.size.minMax = {editor.semanticValueA, editor.semanticValueA};
		break;
	case CLAY__SIZING_TYPE_FIT:
	case CLAY__SIZING_TYPE_GROW:
		editor.semanticValueA = std::max(editor.semanticValueA, 0.0f);
		editor.semanticValueB = std::max(editor.semanticValueB, editor.semanticValueA);
		axis.size.minMax = {editor.semanticValueA, editor.semanticValueB};
		break;
	}
	return true;
}

constexpr auto kPreviewSizingAxisValues = UiAction(
	"flowui.dev_interface.type-editor.preview-sizing-axis-values",
	[](DevTypeEditorState& editor) {
		if (applySizingAxisValues(editor)) (void)updateEditorPreview(editor);
	});

constexpr auto kCommitSizingAxisValues = UiAction(
	"flowui.dev_interface.type-editor.sizing-axis-values",
	[](DevTypeEditorState& editor) {
		if (!applySizingAxisValues(editor)) return;
		(void)commitEditorValue(editor, "Edit sizing for " + editor.fieldName);
	});

constexpr auto kCancelSizingAxisValues = UiAction(
	"flowui.dev_interface.type-editor.cancel-sizing-axis-values",
	[](DevTypeEditorState& editor) {
		(void)applySizingAxisValues(editor);
		cancelEditorPreview(editor);
	});

void updateColorHexDraft(DevTypeEditorState& editor) {
	char hex[16]{};
	std::snprintf(hex, sizeof(hex), "#%02X%02X%02X%02X",
		static_cast<unsigned>(std::lround(std::clamp(editor.semanticChannels[0], 0.0f, 255.0f))),
		static_cast<unsigned>(std::lround(std::clamp(editor.semanticChannels[1], 0.0f, 255.0f))),
		static_cast<unsigned>(std::lround(std::clamp(editor.semanticChannels[2], 0.0f, 255.0f))),
		static_cast<unsigned>(std::lround(std::clamp(editor.semanticChannels[3], 0.0f, 255.0f))));
	editor.semanticText = hex;
}

bool applyColorChannels(DevTypeEditorState& editor) {
	if (!editor.currentValue || !editor.editValue) return false;
	auto& color = *static_cast<Clay_Color*>(editor.editValue);
	color = Clay_Color{
		std::clamp(editor.semanticChannels[0], 0.0f, 255.0f),
		std::clamp(editor.semanticChannels[1], 0.0f, 255.0f),
		std::clamp(editor.semanticChannels[2], 0.0f, 255.0f),
		std::clamp(editor.semanticChannels[3], 0.0f, 255.0f)};
	updateColorHexDraft(editor);
	editor.draftValid = true;
	return true;
}

constexpr auto kPreviewColorChannels = UiAction(
	"flowui.dev_interface.type-editor.preview-color-channels",
	[](DevTypeEditorState& editor) {
		if (applyColorChannels(editor)) (void)updateEditorPreview(editor);
	});

constexpr auto kCommitColorChannels = UiAction(
	"flowui.dev_interface.type-editor.color-channels",
	[](DevTypeEditorState& editor) {
		if (!applyColorChannels(editor)) return;
		(void)commitEditorValue(editor, "Edit color for " + editor.fieldName);
	});

constexpr auto kCancelColorChannels = UiAction(
	"flowui.dev_interface.type-editor.cancel-color-channels",
	[](DevTypeEditorState& editor) {
		(void)applyColorChannels(editor);
		cancelEditorPreview(editor);
	});

constexpr auto kOpenColorPopup = UiAction(
	"flowui.dev_interface.type-editor.color-popup.open",
	[](DevColorTypeEditorState& state) { state.popupOpen = true; });

constexpr auto kCloseColorPopup = UiAction(
	"flowui.dev_interface.type-editor.color-popup.close",
	[](DevColorTypeEditorState& state) { state.popupOpen = false; });

bool applyColorHex(DevTypeEditorState& editor) {
	if (!editor.currentValue || !editor.editValue) return false;
	std::string_view text = editor.semanticText;
	if (!text.empty() && text.front() == '#') text.remove_prefix(1);
	if (text.size() != 3u && text.size() != 6u && text.size() != 8u) {
		editor.draftValid = false;
		return false;
	}
	uint32_t packed = 0u;
	const auto parsed = std::from_chars(text.data(), text.data() + text.size(), packed, 16);
	if (parsed.ec != std::errc{} || parsed.ptr != text.data() + text.size()) {
		editor.draftValid = false;
		return false;
	}
	if (text.size() == 3u) {
		const uint32_t red = (packed >> 8u) & 0xFu;
		const uint32_t green = (packed >> 4u) & 0xFu;
		const uint32_t blue = packed & 0xFu;
		packed = ((red << 4u) | red) << 24u |
			((green << 4u) | green) << 16u |
			((blue << 4u) | blue) << 8u | 0xFFu;
	} else if (text.size() == 6u) {
		packed = (packed << 8u) | 0xFFu;
	}
	editor.semanticChannels = {
		static_cast<float>((packed >> 24u) & 0xFFu),
		static_cast<float>((packed >> 16u) & 0xFFu),
		static_cast<float>((packed >> 8u) & 0xFFu),
		static_cast<float>(packed & 0xFFu)};
	editor.draftValid = true;
	auto& color = *static_cast<Clay_Color*>(editor.editValue);
	color = Clay_Color{
		static_cast<float>(editor.semanticChannels[0]),
		static_cast<float>(editor.semanticChannels[1]),
		static_cast<float>(editor.semanticChannels[2]),
		static_cast<float>(editor.semanticChannels[3])};
	updateColorHexDraft(editor);
	return true;
}

constexpr auto kPreviewColorHex = UiAction(
	"flowui.dev_interface.type-editor.preview-color-hex",
	[](DevTypeEditorState& editor) {
		if (applyColorHex(editor)) (void)updateEditorPreview(editor);
	});

constexpr auto kCommitColorHex = UiAction(
	"flowui.dev_interface.type-editor.color-hex",
	[](DevTypeEditorState& editor) {
		if (!applyColorHex(editor)) {
			cancelEditorPreview(editor);
			return;
		}
		(void)commitEditorValue(editor, "Edit color for " + editor.fieldName);
	});

bool applySpatialValues(DevTypeEditorState& editor, std::uint64_t changedIndex) {
	if (!editor.app || !editor.currentValue || !editor.editValue) return false;
	const devMode::DevSchemaView schema = editor.app->devTooling().schemas().view();
	const devMode::DevTypeSchema* type = schema ? schema->type(editor.operationType) : nullptr;
	const std::string_view name = schema && type ? schemaTypeName(*schema, type) : std::string_view{};
	if (editor.semanticLinked && changedIndex < 4u) {
		const unsigned int linked = editor.semanticUnsigned[changedIndex];
		for (std::size_t index = 0; index < 4u; ++index) editor.semanticUnsigned[index] = linked;
	}
	if (name == "Clay_Padding") {
		auto& value = *static_cast<Clay_Padding*>(editor.editValue);
		value.left = static_cast<std::uint16_t>(std::min(editor.semanticUnsigned[0], 65535u));
		value.right = static_cast<std::uint16_t>(std::min(editor.semanticUnsigned[1], 65535u));
		value.top = static_cast<std::uint16_t>(std::min(editor.semanticUnsigned[2], 65535u));
		value.bottom = static_cast<std::uint16_t>(std::min(editor.semanticUnsigned[3], 65535u));
		return true;
	}
	if (name == "Clay_BorderWidth") {
		auto& value = *static_cast<Clay_BorderWidth*>(editor.editValue);
		value.left = static_cast<std::uint16_t>(std::min(editor.semanticUnsigned[0], 65535u));
		value.right = static_cast<std::uint16_t>(std::min(editor.semanticUnsigned[1], 65535u));
		value.top = static_cast<std::uint16_t>(std::min(editor.semanticUnsigned[2], 65535u));
		value.bottom = static_cast<std::uint16_t>(std::min(editor.semanticUnsigned[3], 65535u));
		value.betweenChildren = static_cast<std::uint16_t>(
			std::min(editor.semanticUnsigned[4], 65535u));
		return true;
	}
	return false;
}

constexpr auto kPreviewSpatialValue = UiAction(
	"flowui.dev_interface.type-editor.preview-spatial",
	[](DevTypeEditorState& editor, std::uint64_t index) {
		if (applySpatialValues(editor, index)) (void)updateEditorPreview(editor);
	});

constexpr auto kCommitSpatialValue = UiAction(
	"flowui.dev_interface.type-editor.commit-spatial",
	[](DevTypeEditorState& editor, std::uint64_t index) {
		if (applySpatialValues(editor, index)) {
			(void)commitEditorValue(editor, "Edit spatial values for " + editor.fieldName);
		}
	});

constexpr auto kCancelSpatialValue = UiAction(
	"flowui.dev_interface.type-editor.cancel-spatial",
	[](DevTypeEditorState& editor, std::uint64_t index) {
		(void)applySpatialValues(editor, index);
		cancelEditorPreview(editor);
	});

constexpr auto kToggleSpatialLink = UiAction(
	"flowui.dev_interface.type-editor.toggle-spatial-link",
	[](DevTypeEditorState& editor) { editor.semanticLinked = !editor.semanticLinked; });

bool applyCornerValues(DevTypeEditorState& editor, std::uint64_t changedIndex) {
	if (!editor.currentValue || !editor.editValue) return false;
	if (editor.semanticLinked && changedIndex < editor.semanticCorners.size()) {
		const float linked = std::max(0.0f, editor.semanticCorners[changedIndex]);
		editor.semanticCorners.fill(linked);
	}
	for (float& value : editor.semanticCorners) value = std::max(0.0f, value);
	auto& radius = *static_cast<Clay_CornerRadius*>(editor.editValue);
	radius.topLeft = editor.semanticCorners[0];
	radius.topRight = editor.semanticCorners[1];
	radius.bottomLeft = editor.semanticCorners[2];
	radius.bottomRight = editor.semanticCorners[3];
	return true;
}

constexpr auto kPreviewCornerValue = UiAction(
	"flowui.dev_interface.type-editor.preview-corner-radius",
	[](DevTypeEditorState& editor, std::uint64_t index) {
		if (applyCornerValues(editor, index)) (void)updateEditorPreview(editor);
	});

constexpr auto kCommitCornerValue = UiAction(
	"flowui.dev_interface.type-editor.commit-corner-radius",
	[](DevTypeEditorState& editor, std::uint64_t index) {
		if (applyCornerValues(editor, index)) {
			(void)commitEditorValue(editor, "Edit corner radii for " + editor.fieldName);
		}
	});

constexpr auto kCancelCornerValue = UiAction(
	"flowui.dev_interface.type-editor.cancel-corner-radius",
	[](DevTypeEditorState& editor, std::uint64_t index) {
		(void)applyCornerValues(editor, index);
		cancelEditorPreview(editor);
	});

constexpr auto kChooseAttachmentPoint = UiAction(
	"flowui.dev_interface.type-editor.choose-attachment-point",
	[](DevTypeEditorState& editor, std::uint64_t anchor) {
		if (!editor.currentValue || anchor > 1u) return;
		auto& value = *static_cast<Clay_FloatingAttachPoints*>(editor.currentValue.data());
		if (anchor == 0u) {
			value.element = static_cast<Clay_FloatingAttachPointType>(editor.semanticSelections[0]);
		} else {
			value.parent = static_cast<Clay_FloatingAttachPointType>(editor.semanticSelections[1]);
		}
		(void)commitEditorValue(editor, std::string("Choose ") +
			(anchor == 0u ? "element" : "parent") + " anchor for " + editor.fieldName);
	});

constexpr auto kChooseTextureResource = UiAction(
	"flowui.dev_interface.type-editor.choose-texture-resource",
	[](DevTypeEditorState& editor) {
		if (!editor.app || !editor.currentValue) return;
		const auto found = std::ranges::find(editor.choiceIdentities, editor.resourceSelection);
		if (found == editor.choiceIdentities.end()) {
			editor.localDiagnostic = "Selected resource is no longer in the catalogue";
			return;
		}
		const std::size_t index = static_cast<std::size_t>(found - editor.choiceIdentities.begin());
		if (index >= editor.choiceKeys.size() || index >= editor.choiceResourceDomains.size()) return;
		const auto& prior = *static_cast<const TextureRef*>(editor.currentValue.data());
		TextureRef replacement{};
		const ResourceKey key{.name = editor.choiceKeys[index]};
		if (editor.choiceResourceDomains[index] == 1u) {
#if FLOWUI_INCLUDE_IMAGE_MANAGER
			replacement = editor.app->images().getTexture(key);
#endif
		} else if (editor.choiceResourceDomains[index] == 2u) {
#if FLOWUI_INCLUDE_ICON_MANAGER
			replacement = editor.app->icons().textureRef(key);
#endif
		}
		if (!replacement.handle) {
			editor.localDiagnostic = "Manager could not construct the selected texture";
			return;
		}
		replacement.fitMode = prior.fitMode;
		replacement.samplingMode = prior.samplingMode;
		replacement.tintEnabled = prior.tintEnabled;
		*static_cast<TextureRef*>(editor.currentValue.data()) = replacement;
		editor.localDiagnostic.clear();
		(void)commitEditorValue(editor, "Choose texture source for " + editor.fieldName);
	});

constexpr auto kChooseActionCall = UiAction(
	"flowui.dev_interface.type-editor.choose-action-call",
	[](DevTypeEditorState& editor) {
		if (!editor.app || !editor.currentValue) return;
		ActionCall replacement{};
		if (editor.resourceSelection != 0u) {
			const auto found = std::ranges::find(editor.choiceIdentities, editor.resourceSelection);
			if (found == editor.choiceIdentities.end()) {
				editor.localDiagnostic = "Selected action is no longer in the catalogue";
				return;
			}
			const std::size_t index = static_cast<std::size_t>(found - editor.choiceIdentities.begin());
			if (index >= editor.choiceResourceDomains.size() || index >= editor.choiceKeys.size()) return;
			const ActionCallKind kind = editor.choiceResourceDomains[index] == 1u
				? ActionCallKind::App : ActionCallKind::Ui;
			std::uint64_t stableId = 0u;
			const std::string& encoded = editor.choiceKeys[index];
			const auto parsed = std::from_chars(
				encoded.data(), encoded.data() + encoded.size(), stableId);
			if (parsed.ec != std::errc{} || parsed.ptr != encoded.data() + encoded.size()) return;
			const std::optional<ActionCall> built = editor.app->actions().makeDevActionCall(
				kind, stableId, 0u, 0u);
			if (!built) {
				editor.localDiagnostic = "Action compatibility or construction check failed";
				return;
			}
			replacement = *built;
		}
		*static_cast<ActionCall*>(editor.currentValue.data()) = replacement;
		editor.localDiagnostic.clear();
		(void)commitEditorValue(editor, "Choose action for " + editor.fieldName);
	});

constexpr auto kChooseTextureFit = UiAction(
	"flowui.dev_interface.type-editor.choose-texture-fit",
	[](DevTypeEditorState& editor) {
		if (!editor.currentValue) return;
		auto& texture = *static_cast<TextureRef*>(editor.currentValue.data());
		texture.fitMode = static_cast<TextureFitMode>(editor.resourceFitSelection);
		(void)commitEditorValue(editor, "Choose texture fit for " + editor.fieldName);
	});

constexpr auto kChooseTextureSampling = UiAction(
	"flowui.dev_interface.type-editor.choose-texture-sampling",
	[](DevTypeEditorState& editor) {
		if (!editor.currentValue) return;
		auto& texture = *static_cast<TextureRef*>(editor.currentValue.data());
		texture.samplingMode = static_cast<TextureSamplingMode>(editor.resourceSamplingSelection);
		(void)commitEditorValue(editor, "Choose texture sampling for " + editor.fieldName);
	});

constexpr auto kToggleTextureTint = UiAction(
	"flowui.dev_interface.type-editor.toggle-texture-tint",
	[](DevTypeEditorState& editor) {
		if (!editor.currentValue) return;
		auto& texture = *static_cast<TextureRef*>(editor.currentValue.data());
		editor.resourceTintEnabled = !editor.resourceTintEnabled;
		texture.tintEnabled = editor.resourceTintEnabled;
		(void)commitEditorValue(editor, "Toggle texture tint for " + editor.fieldName);
	});

constexpr auto kAddSequenceItem = UiAction(
	"flowui.dev_interface.type-editor.sequence-add",
	[](DevTypeEditorState& editor) {
		if (!editor.app || !editor.currentValue) return;
		const auto schema = editor.app->devTooling().schemas().view();
		const auto* operations = schema ? typeOperations(*schema, editor.typeIndex) : nullptr;
		if (operations && operations->sequenceAppendDefault &&
			operations->sequenceAppendDefault(editor.currentValue.data()) ==
				devMode::DevValueOperationStatus::Success) {
			(void)commitEditorValue(editor, "Add item to " + editor.fieldName);
		}
	});

constexpr auto kRemoveSequenceItem = UiAction(
	"flowui.dev_interface.type-editor.sequence-remove",
	[](DevTypeEditorState& editor, std::size_t index) {
		if (!editor.app || !editor.currentValue) return;
		const auto schema = editor.app->devTooling().schemas().view();
		const auto* operations = schema ? typeOperations(*schema, editor.typeIndex) : nullptr;
		if (operations && operations->sequenceErase &&
			operations->sequenceErase(editor.currentValue.data(), index) ==
				devMode::DevValueOperationStatus::Success) {
			(void)commitEditorValue(editor, "Remove item from " + editor.fieldName);
		}
	});

constexpr auto kMoveSequenceItem = UiAction(
	"flowui.dev_interface.type-editor.sequence-move",
	[](DevTypeEditorState& editor, uint64_t packedMove) {
		if (!editor.app || !editor.currentValue) return;
		const std::size_t from = static_cast<std::size_t>(packedMove >> 32u);
		const std::size_t to = static_cast<std::size_t>(packedMove & 0xffffffffu);
		const auto schema = editor.app->devTooling().schemas().view();
		const auto* operations = schema ? typeOperations(*schema, editor.typeIndex) : nullptr;
		if (operations && operations->sequenceMove &&
			operations->sequenceMove(editor.currentValue.data(), from, to) ==
				devMode::DevValueOperationStatus::Success) {
			(void)commitEditorValue(editor, "Reorder " + editor.fieldName);
		}
	});

constexpr auto kCollapseCard = UiAction(
	"flowui.dev_interface.editor-card.collapse",
	[](bool& collapsed) { collapsed = !collapsed; });

constexpr auto kCopyField = UiAction(
	"flowui.dev_interface.editor-card.copy",
	[](DevEditorCardState& card) {
		if (!card.interfaceState) return;
		DevInterfaceState& state = *card.interfaceState;
		if (!card.hasCurrentValue || !card.currentValue) return;
		DevInterfaceEditTransaction transaction{};
		transaction.description = "Copy " + card.typeName;
		transaction.changesClipboard = true;
		transaction.clipboardBefore = state.editorClipboard;
		state.editorClipboard = DevInterfaceEditorClipboard{
			.type = card.currentValue.type(),
			.typeName = card.typeName,
			.value = card.currentValue,
		};
		transaction.clipboardAfter = state.editorClipboard;
		state.editUndoStack.push_back(std::move(transaction));
		state.editRedoStack.clear();
		state.lastActionMessage = card.typeName + " Values of " +
			card.binding.elementName + "'s " + roleName(card.binding.role) +
			" field " + card.fieldName + " copied";
	});

constexpr auto kPasteField = UiAction(
	"flowui.dev_interface.editor-card.paste",
	[](DevEditorCardState& card) {
		if (!card.app || !card.interfaceState) return;
		App& app = *card.app;
		DevInterfaceState& state = *card.interfaceState;
		const devMode::DevSchemaView schema = app.devTooling().schemas().view();
		if (!schema || !card.editable || !state.editorClipboard ||
			state.editorClipboard.type != card.fieldType) return;
		DevInterfaceEditTransaction transaction{};
		transaction.description = "Paste " + card.typeName;
		transaction.forward.push_back(fieldCommand(
			*schema, card, tooling::DevOverrideCommandKind::SetElementField,
			state.editorClipboard.value));
		if (const tooling::DevOverrideApply::Record* prior =
			currentOverride(app, *schema, card)) {
			transaction.inverse.push_back(fieldCommand(
				*schema, card, tooling::DevOverrideCommandKind::SetElementField,
				prior->value));
		} else {
			transaction.inverse.push_back(fieldCommand(
				*schema, card, tooling::DevOverrideCommandKind::ClearElementField));
		}
		if (submitEditTransaction(app, state, std::move(transaction))) {
			state.lastActionMessage = card.typeName + " Values pasted into " +
				card.binding.elementName + "'s " + roleName(card.binding.role) +
				" field " + card.fieldName;
		}
	});

constexpr auto kResetField = UiAction(
	"flowui.dev_interface.editor-card.reset",
	[](DevEditorCardState& card) {
		if (!card.app || !card.interfaceState) return;
		App& app = *card.app;
		DevInterfaceState& state = *card.interfaceState;
		const devMode::DevSchemaView schema = app.devTooling().schemas().view();
		if (!schema || !card.editable) return;
		const tooling::DevOverrideCommand scope = fieldCommand(
			*schema, card, tooling::DevOverrideCommandKind::ClearElementField);
		DevInterfaceEditTransaction transaction{};
		transaction.description = card.overridePresence.descendantLive &&
			!card.overridePresence.exactLive
			? "Reset nested changes in " + card.typeName
			: "Reset " + card.typeName;
		for (const tooling::DevOverrideApply::Record& record :
			app.devTooling().overrides().appliedOverrides().records()) {
			if (record.layer != tooling::DevOverrideLayer::LiveInstance ||
				overrideRelation(record, scope) == OverrideRelation::None) continue;
			tooling::DevOverrideCommand clear = scope;
			clear.field = record.field;
			transaction.forward.push_back(std::move(clear));
			tooling::DevOverrideCommand restore = resolvedOverrideCommand(
				*schema, card.binding, tooling::DevOverrideCommandKind::SetElementField,
				record.value);
			restore.field = record.field;
			transaction.inverse.push_back(std::move(restore));
		}
		if (transaction.forward.empty()) return;
		if (submitEditTransaction(app, state, std::move(transaction))) {
			state.lastActionMessage = card.binding.elementName + "'s " +
				roleName(card.binding.role) + " field " + card.fieldName + " reset";
		}
	});

tooling::DevOverrideCommand commandForRecord(
	const tooling::DevOverrideApply::Record& record,
	tooling::DevOverrideCommandKind kind,
	bool includeValue = false) {
	tooling::DevOverrideCommand command{};
	command.kind = kind;
	command.element = record.target;
	command.field = record.field;
	command.layer = record.layer;
	if (includeValue) command.value = record.value;
	return command;
}

bool recordTargetsSelection(
	const tooling::DevOverrideApply::Record& record,
	FlowDefinitionID definition,
	WindowId window,
	::FlowUi::detail::element::ElementInstanceKey instance) {
	if (record.target.definition != definition) return false;
	if (record.target.scope == tooling::DevOverrideScope::Definition) return true;
	return record.target.window == window && record.target.instance == instance;
}

const tooling::DevOverrideApply::Record* winningLiveRecord(
	const App& app,
	const DevChangeCardState& card) {
	const auto& records = app.devTooling().overrides().appliedOverrides().records();
	for (const tooling::DevOverrideLayer layer : {
		tooling::DevOverrideLayer::LiveInstance,
		tooling::DevOverrideLayer::LiveDefinition}) {
		const auto found = std::ranges::find_if(records,
			[&](const tooling::DevOverrideApply::Record& record) {
				return record.layer == layer && record.field == card.field &&
					recordTargetsSelection(record, card.definition, card.window, card.instance);
			});
		if (found != records.end()) return &*found;
	}
	return nullptr;
}

constexpr auto kRevertChange = UiAction(
	"flowui.dev_interface.change-card.revert",
	[](DevChangeCardState& card) {
		if (!card.app || !card.interfaceState) return;
		App& app = *card.app;
		DevInterfaceState& state = *card.interfaceState;
		const tooling::DevOverrideApply::Record* winner = winningLiveRecord(app, card);
		if (!winner) return;
		DevInterfaceEditTransaction transaction{};
		transaction.description = "Revert " + card.fieldName;
		transaction.forward.push_back(commandForRecord(
			*winner, tooling::DevOverrideCommandKind::ClearElementField));
		transaction.inverse.push_back(commandForRecord(
			*winner, tooling::DevOverrideCommandKind::SetElementField, true));
		if (submitEditTransaction(app, state, std::move(transaction))) {
			state.lastActionMessage = card.fieldName + " reverted to the next layer";
		}
	});

constexpr auto kElevateChange = UiAction(
	"flowui.dev_interface.change-card.elevate",
	[](DevChangeCardState& card) {
		if (!card.app || !card.interfaceState) return;
		App& app = *card.app;
		DevInterfaceState& state = *card.interfaceState;
		const tooling::DevOverrideApply::Record* winner = winningLiveRecord(app, card);
		if (!winner || winner->layer != tooling::DevOverrideLayer::LiveInstance) return;
		const auto& records = app.devTooling().overrides().appliedOverrides().records();
		const auto prior = std::ranges::find_if(records,
			[&](const tooling::DevOverrideApply::Record& record) {
				return record.layer == tooling::DevOverrideLayer::LiveDefinition &&
					record.target.definition == card.definition && record.field == card.field;
			});
		tooling::DevOverrideCommand define = commandForRecord(
			*winner, tooling::DevOverrideCommandKind::SetElementField, true);
		define.element.scope = tooling::DevOverrideScope::Definition;
		define.element.instance = {};
		define.layer = tooling::DevOverrideLayer::LiveDefinition;
		if (prior != records.end()) define.element = prior->target;

		DevInterfaceEditTransaction transaction{};
		transaction.description = "Elevate " + card.fieldName;
		transaction.forward.push_back(define);
		transaction.forward.push_back(commandForRecord(
			*winner, tooling::DevOverrideCommandKind::ClearElementField));
		transaction.inverse.push_back(commandForRecord(
			*winner, tooling::DevOverrideCommandKind::SetElementField, true));
		if (prior != records.end()) {
			transaction.inverse.push_back(commandForRecord(
				*prior, tooling::DevOverrideCommandKind::SetElementField, true));
		} else {
			define.kind = tooling::DevOverrideCommandKind::ClearElementField;
			define.value.reset();
			transaction.inverse.push_back(std::move(define));
		}
		if (submitEditTransaction(app, state, std::move(transaction))) {
			state.lastActionMessage = card.fieldName + " elevated to Live Definition";
		}
	});

constexpr auto kRevertAllChanges = UiAction(
	"flowui.dev_interface.changes.revert-all",
	[](DevChangesHeaderState& header) {
		if (!header.app || !header.interfaceState) return;
		App& app = *header.app;
		DevInterfaceState& state = *header.interfaceState;
		DevInterfaceEditTransaction transaction{};
		transaction.description = "Revert all live changes";
		for (const tooling::DevOverrideApply::Record& record :
			app.devTooling().overrides().appliedOverrides().records()) {
			if ((record.layer != tooling::DevOverrideLayer::LiveInstance &&
				record.layer != tooling::DevOverrideLayer::LiveDefinition) ||
				!recordTargetsSelection(record, header.definition, header.window, header.instance)) {
				continue;
			}
			transaction.forward.push_back(commandForRecord(
				record, tooling::DevOverrideCommandKind::ClearElementField));
			transaction.inverse.push_back(commandForRecord(
				record, tooling::DevOverrideCommandKind::SetElementField, true));
		}
		if (!transaction.forward.empty() &&
			submitEditTransaction(app, state, std::move(transaction))) {
			state.lastActionMessage = "All live changes reverted for " + header.elementName;
		}
	});

constexpr auto kElevateAllChanges = UiAction(
	"flowui.dev_interface.changes.elevate-all",
	[](DevChangesHeaderState& header) {
		if (!header.app || !header.interfaceState) return;
		App& app = *header.app;
		DevInterfaceState& state = *header.interfaceState;
		const auto& records = app.devTooling().overrides().appliedOverrides().records();
		DevInterfaceEditTransaction transaction{};
		transaction.description = "Elevate all instance changes";
		for (const tooling::DevOverrideApply::Record& record : records) {
			if (record.layer != tooling::DevOverrideLayer::LiveInstance ||
				record.target.definition != header.definition ||
				record.target.window != header.window || record.target.instance != header.instance) {
				continue;
			}
			const auto prior = std::ranges::find_if(records,
				[&](const tooling::DevOverrideApply::Record& candidate) {
					return candidate.layer == tooling::DevOverrideLayer::LiveDefinition &&
						candidate.target.definition == header.definition &&
						candidate.field == record.field;
				});
			tooling::DevOverrideCommand define = commandForRecord(
				record, tooling::DevOverrideCommandKind::SetElementField, true);
			define.element.scope = tooling::DevOverrideScope::Definition;
			define.element.instance = {};
			define.layer = tooling::DevOverrideLayer::LiveDefinition;
			if (prior != records.end()) define.element = prior->target;
			transaction.forward.push_back(define);
			transaction.forward.push_back(commandForRecord(
				record, tooling::DevOverrideCommandKind::ClearElementField));
			transaction.inverse.push_back(commandForRecord(
				record, tooling::DevOverrideCommandKind::SetElementField, true));
			if (prior != records.end()) {
				transaction.inverse.push_back(commandForRecord(
					*prior, tooling::DevOverrideCommandKind::SetElementField, true));
			} else {
				define.kind = tooling::DevOverrideCommandKind::ClearElementField;
				define.value.reset();
				transaction.inverse.push_back(std::move(define));
			}
		}
		if (!transaction.forward.empty() &&
			submitEditTransaction(app, state, std::move(transaction))) {
			state.lastActionMessage = "All instance changes elevated for " + header.elementName;
		}
	});

void drawHeaderButton(
	DevEditorCardHeader::BuildContext& context,
	FlowElementPart part,
	std::string_view label,
	ActionCall action,
	bool enabled,
	TextureRef icon = {}) {
	FSEL::ButtonParameters parameters{};
	parameters.onActivate = action;
	parameters.enabled = enabled;
	parameters.contentMode = icon.handle
		? FSEL::ButtonContentMode::IconOnly : FSEL::ButtonContentMode::TextOnly;
	parameters.text = label;
	parameters.icon = icon;
	parameters.tintIcon = true;
	parameters.sizing = {
		.width = CLAY_SIZING_FIXED(24),
		.height = CLAY_SIZING_FIXED(24),
	};
	parameters.padding = Clay_Padding{0, 0, 0, 0};
	parameters.borderWidth = Clay_BorderWidth{1, 1, 1, 1, 0};
	parameters.cornerRadius = CLAY_CORNER_RADIUS(3);
	parameters.idleOverrides.backgroundColor = interface_theme::kDepth3Elevated;
	parameters.idleOverrides.labelColor = interface_theme::kTextSecondary;
	parameters.idleOverrides.iconColor = interface_theme::kTextSecondary;
	parameters.idleOverrides.borderColor = interface_theme::kBorderVisible;
	parameters.hoveredOverrides.backgroundColor = interface_theme::kHoverSurface;
	parameters.hoveredOverrides.labelColor = interface_theme::kTextCanvas;
	parameters.hoveredOverrides.iconColor = interface_theme::kTextCanvas;
	parameters.hoveredOverrides.borderColor = interface_theme::kAccentCurrent;
	parameters.pressedOverrides.backgroundColor = interface_theme::kSelectedRow;
	parameters.pressedOverrides.labelColor = interface_theme::kTextCanvas;
	parameters.pressedOverrides.iconColor = interface_theme::kTextCanvas;
	parameters.pressedOverrides.borderColor = interface_theme::kAccentSeaGlass;
	parameters.disabledOverrides.backgroundColor = interface_theme::kDepth2Ink;
	parameters.disabledOverrides.labelColor = interface_theme::kTextMuted;
	parameters.disabledOverrides.iconColor = interface_theme::kTextMuted;
	parameters.disabledOverrides.borderColor = interface_theme::kBorderPrimary;
	parameters.labelFontSize = 10;
	parameters.iconSize = 12.0f;
	context.createPart(FSEL::kButton, part)
		.setParameters(std::move(parameters))
		.setDevInternalCapture(true)
		.draw();
}

void drawSmallEditorButton(
	DevTypeEditor::BuildContext& context,
	LocalElementName id,
	uint64_t index,
	std::string_view label,
	ActionCall action) {
	FSEL::ButtonParameters parameters{};
	parameters.onActivate = action;
	parameters.enabled = static_cast<bool>(action);
	parameters.text = label;
	parameters.sizing = Clay_Sizing{
		.width = CLAY_SIZING_FIXED(22),
		.height = CLAY_SIZING_FIXED(22),
	};
	parameters.padding = Clay_Padding{};
	parameters.labelFontSize = 9;
	context.uiManager.createElement(FSEL::kButton, Keyed(id, index))
		.setParameters(std::move(parameters)).setDevInternalCapture(true).draw();
}

void drawSequenceRows(
	DevTypeEditor::BuildContext& context,
	DevTypeEditorState& state,
	const devMode::DevSchemaGeneration& schema,
	const devMode::DevTypeSchema& type,
	const devMode::DevTypeOps& operations) {
	const std::size_t totalCount = operations.sequenceSize
		? operations.sequenceSize(state.editValue) : 0u;
	const std::size_t count = std::min(totalCount, kMaximumVisibleSequenceItems);
	const devMode::DevTypeSchema* elementType = schema.type(type.elementType);
	const devMode::DevTypeOps* elementOps = typeOperations(schema, type.elementType);
	for (std::size_t index = 0; index < count; ++index) {
		const void* item = operations.sequenceElementAddress
			? operations.sequenceElementAddress(state.editValue, index) : nullptr;
		std::string summary = std::to_string(index) + "  ";
		if (elementType && item) {
			if (elementType->kind == devMode::DevTypeKind::Text && elementOps && elementOps->textView) {
				summary += std::string(elementOps->textView(item));
			} else {
				summary += numericDraft(*elementType, elementOps, item);
				if (summary.ends_with("  ")) summary += schemaTypeName(schema, elementType);
			}
		}
		ActionCall up{}, down{}, remove{};
		if (state.app && state.interfaceState && state.editable) {
			auto& actions = state.app->actions().uiActions();
			const uint64_t from = static_cast<uint64_t>(index) << 32u;
			if (index > 0) {
				const uint64_t move = from | static_cast<uint64_t>(index - 1);
				up = ActionCall{actions.make(kMoveSequenceItem, state, move)};
			}
			if (index + 1 < count) {
				const uint64_t move = from | static_cast<uint64_t>(index + 1);
				down = ActionCall{actions.make(kMoveSequenceItem, state, move)};
			}
			remove = ActionCall{actions.make(kRemoveSequenceItem, state, index)};
		}
		Clay_ElementDeclaration itemRow{};
		itemRow.layout.sizing = {.width = CLAY_SIZING_GROW(0), .height = CLAY_SIZING_FIXED(26)};
		itemRow.layout.layoutDirection = CLAY_LEFT_TO_RIGHT;
		itemRow.layout.childGap = 4;
		itemRow.layout.childAlignment = {.x = CLAY_ALIGN_X_LEFT, .y = CLAY_ALIGN_Y_CENTER};
		itemRow.layout.padding = Clay_Padding{6, 4, 0, 0};
		itemRow.backgroundColor = interface_theme::kDepth3Elevated;
		itemRow.clip = {.horizontal = true, .vertical = true};
		CLAY(context.clayID(Keyed("sequence-row", index)), itemRow) {
			Clay_ElementDeclaration textRoot{};
			textRoot.layout.sizing = {.width = CLAY_SIZING_GROW(0), .height = CLAY_SIZING_FIT(0)};
			textRoot.clip = {.horizontal = true, .vertical = true};
			CLAY(context.clayID(Keyed("sequence-text", index)), textRoot) {
				const auto style = textConfig(interface_theme::kTextSecondary, 9);
				CLAY_TEXT(context.uiManager.toClayString(summary), CLAY_TEXT_CONFIG(style));
			}
			drawSmallEditorButton(context, kSequenceUp, index, "↑", up);
			drawSmallEditorButton(context, kSequenceDown, index, "↓", down);
			if (!type.sequenceFixed) drawSmallEditorButton(
				context, kSequenceRemove, index, "×", remove);
		}
	}
	if (totalCount > count) {
		Clay_TextElementConfig style = textConfig(interface_theme::kTextMuted, 9);
		style.wrapMode = CLAY_TEXT_WRAP_WORDS;
		const std::string remainder = std::to_string(totalCount - count) +
			" additional items are not rendered";
		CLAY_TEXT(context.uiManager.toClayString(remainder), CLAY_TEXT_CONFIG(style));
	}
	if (type.sequenceFixed) return;
	ActionCall add{};
	if (state.app && state.interfaceState && state.editable && operations.sequenceAppendDefault) {
		add = ActionCall{state.app->actions().uiActions().make(kAddSequenceItem, state)};
	}
	FSEL::ButtonParameters parameters{};
	parameters.onActivate = add;
	parameters.enabled = static_cast<bool>(add);
	parameters.text = "Add item";
	parameters.sizing = Clay_Sizing{.width = CLAY_SIZING_GROW(0), .height = CLAY_SIZING_FIXED(26)};
	context.uiManager.createElement(FSEL::kButton, "sequence-add")
		.setParameters(std::move(parameters)).setDevInternalCapture(true).draw();
}

void drawMessage(
	DevInterfaceInspectorFields::BuildContext& context,
	std::string_view message) {
	Clay_ElementDeclaration empty{};
	empty.layout.sizing = {
		.width = CLAY_SIZING_GROW(0),
		.height = CLAY_SIZING_GROW(0),
	};
	empty.layout.padding = Clay_Padding{16, 16, 16, 16};
	empty.layout.childAlignment = {
		.x = CLAY_ALIGN_X_CENTER,
		.y = CLAY_ALIGN_Y_CENTER,
	};
	Clay_TextElementConfig style = textConfig(
		interface_theme::kTextMuted, 11, CLAY_TEXT_ALIGN_CENTER);
	style.wrapMode = CLAY_TEXT_WRAP_WORDS;
	CLAY(context.clayID("message"), empty) {
		CLAY_TEXT(context.uiManager.toClayString(message), CLAY_TEXT_CONFIG(style));
	}
}

void drawCard(
	DevInterfaceInspectorFields::BuildContext& context,
	const devMode::DevSchemaView& schema,
	const devMode::DevFieldSchema& field,
	DevEditorRole role,
	devMode::DevTypeId rootOwnerType,
	const void* value,
	std::string_view labelOverride = {},
	uint64_t cardKey = 0u) {
	context.uiManager.createElement(
		kDevEditorCard, Keyed(kEditorCard, cardKey == 0u ? field.id : cardKey))
		.setParameters(DevEditorCardParameters{
			.schema = schema,
			.binding = DevEditorBinding{
				.role = role,
				.definition = context.params.definition,
				.window = context.params.interfaceState
					? context.params.interfaceState->selectedWindowId : InvalidWindowId,
				.instance = context.params.instance,
				.rootOwnerType = rootOwnerType,
				.field = fieldIndex(*schema, field),
				.elementName = std::string(context.params.elementName),
			},
			.app = context.params.app,
			.interfaceState = context.params.interfaceState,
			.value = value,
			.labelOverride = labelOverride,
		})
		.setDevInternalCapture(true)
		.draw();
}

} // namespace

DevEditorCardHeaderResources::DevEditorCardHeaderResources(App& app) {
#if FLOWUI_INCLUDE_ICON_MANAGER
	IconManager& icons = app.icons();
	interface_icons::registerDevInterfaceIcons(icons);
	auto resolve = [&icons](std::string_view key) -> TextureRef {
		return icons.contains(key) ? icons.textureRef(key) : TextureRef{};
	};
	copyIcon = resolve(interface_icons::kCopyKey);
	pasteIcon = resolve(interface_icons::kPasteKey);
	revertIcon = resolve(interface_icons::kRevertKey);
	expandIcon = resolve(interface_icons::kExpandKey);
	collapseIcon = resolve(interface_icons::kCollapseKey);
#else
	(void)app;
#endif
}

void DevInterfaceInspectorTitle::buildElement(BuildContext& context) {
	Clay_ElementDeclaration title = fixedSection(
		kTitleHeight,
		interface_theme::kDepth2Ink,
		Clay_Padding{12, 12, 0, 0});
	title.layout.childAlignment = {
		.x = CLAY_ALIGN_X_LEFT,
		.y = CLAY_ALIGN_Y_CENTER,
	};
	const Clay_TextElementConfig style = textConfig(interface_theme::kTextCanvas, 14);
	CLAY(context.clayID(), title) {
		CLAY_TEXT(context.uiManager.toClayString("Inspector"), CLAY_TEXT_CONFIG(style));
	}
}

void DevInterfaceInspectorIdentity::buildElement(BuildContext& context) {
	Clay_ElementDeclaration identity = fixedSection(
		kIdentityHeight,
		interface_theme::kDepth1Panel,
		Clay_Padding{12, 12, 8, 8});
	identity.layout.layoutDirection = CLAY_TOP_TO_BOTTOM;
	identity.layout.childGap = 3;
	identity.layout.childAlignment = {
		.x = CLAY_ALIGN_X_LEFT,
		.y = CLAY_ALIGN_Y_CENTER,
	};
	identity.clip = {.horizontal = true, .vertical = true};
	Clay_TextElementConfig instanceStyle = textConfig(interface_theme::kTextCanvas, 13);
	Clay_TextElementConfig definitionStyle = textConfig(interface_theme::kTextMuted, 10);
	instanceStyle.wrapMode = CLAY_TEXT_WRAP_WORDS;
	definitionStyle.wrapMode = CLAY_TEXT_WRAP_WORDS;
	CLAY(context.clayID(), identity) {
		CLAY_TEXT(
			context.uiManager.toClayString(context.params.instanceName),
			CLAY_TEXT_CONFIG(instanceStyle));
		CLAY_TEXT(
			context.uiManager.toClayString(context.params.definitionName),
			CLAY_TEXT_CONFIG(definitionStyle));
	}
}

void DevInterfaceInspectorTabs::buildElement(BuildContext& context) {
	if (context.params.selectedTab &&
		*context.params.selectedTab > static_cast<uint64_t>(DevInspectorTab::Changes)) {
		*context.params.selectedTab = static_cast<uint64_t>(DevInspectorTab::Parameters);
	}
	Clay_ElementDeclaration strip = fixedSection(
		kTabsHeight, interface_theme::kDepth2Ink);
	strip.layout.layoutDirection = CLAY_LEFT_TO_RIGHT;
	CLAY(context.clayID(), strip) {
		for (const TabSpec& tab : kTabs) {
			const uint64_t value = static_cast<uint64_t>(tab.tab);
			const bool selected = context.params.selectedTab &&
				*context.params.selectedTab == value;
			FSEL::RadioChoiceParameters parameters{};
			parameters.choiceValue = value;
			parameters.selectedValue = context.params.selectedTab;
			parameters.style.sizing = {
				.width = CLAY_SIZING_GROW(0),
				.height = CLAY_SIZING_GROW(0),
			};
			parameters.style.padding = Clay_Padding{5, 5, 0, 0};
			parameters.style.childAlignment = {
				.x = CLAY_ALIGN_X_CENTER,
				.y = CLAY_ALIGN_Y_CENTER,
			};
			parameters.style.borderWidth = Clay_BorderWidth{0, 0, 0, 2, 0};
			parameters.style.cornerRadius = CLAY_CORNER_RADIUS(0);
			parameters.style.idleOverrides.backgroundColor = interface_theme::kDepth2Ink;
			parameters.style.idleOverrides.borderColor = interface_theme::kDepth2Ink;
			parameters.style.hoveredOverrides.backgroundColor = interface_theme::kHoverSurface;
			parameters.style.hoveredOverrides.borderColor = interface_theme::kHoverSurface;
			parameters.style.pressedOverrides.backgroundColor = interface_theme::kSelectedRow;
			parameters.style.pressedOverrides.borderColor = interface_theme::kAccentCurrent;
			parameters.style.selectedOverrides.backgroundColor = interface_theme::kDepth1Panel;
			parameters.style.selectedOverrides.borderColor = interface_theme::kAccentCurrent;

			context.uiManager.createElement(FSEL::kRadioChoice, Keyed(kTabCard, value))
				.setParameters(std::move(parameters))
				.setDevInternalCapture(true)
				.construct();
			const Clay_TextElementConfig labelStyle = textConfig(
				selected ? interface_theme::kTextCanvas : interface_theme::kTextSecondary,
				10,
				CLAY_TEXT_ALIGN_CENTER);
			CLAY_TEXT(
				context.uiManager.toClayString(tab.label),
				CLAY_TEXT_CONFIG(labelStyle));
			context.uiManager.drawConstructed();
		}
	}
}

void DevEditorCard::buildElement(BuildContext& context) {
	const devMode::DevSchemaGeneration* schema =
		context.params.schema ? &*context.params.schema : nullptr;
	const devMode::DevFieldSchema* field = schema
		? fieldAt(*schema, context.params.binding.field) : nullptr;
	const devMode::DevTypeSchema* type = field && schema
		? schema->type(field->valueType) : nullptr;
	std::string_view fieldName = context.params.labelOverride;
	if (fieldName.empty() && field && schema) fieldName = schema->string(field->displayName);
	if (fieldName.empty() && field && schema) fieldName = schema->string(field->name);
	if (fieldName.empty()) fieldName = "Unnamed Field";

	DevEditorCardState& state = context.state();
	state.app = context.params.app;
	state.interfaceState = context.params.interfaceState;
	state.binding = context.params.binding;
	state.fieldName = std::string(fieldName);
	state.typeName = schema ? normalizedTypeName(*schema, type) : "Unknown";
	state.fieldType = type ? type->id : 0u;
	state.editable = field &&
		context.params.binding.role == DevEditorRole::Parameters &&
		editableCapability(field->effectiveEdit);
	state.currentValue.reset();
	state.hasCurrentValue = false;
	if (schema && field && type && context.params.value &&
		devSystems::tooling::DevOwnedValue::copyFrom(
			*schema, field->valueType, context.params.value, state.currentValue) ==
			devMode::DevValueOperationStatus::Success) {
		state.hasCurrentValue = true;
	}
	state.overridePresence = schema && context.params.app
		? overridePresence(*context.params.app, *schema, state)
		: DevOverridePresence{};
	state.dirty = state.overridePresence.hasCommittedChange();
	const DevQuickView collapsedView = state.collapsed && schema
		? quickView(*schema, type, state.currentValue.data(), context.params.app, field)
		: DevQuickView{};
	ActionCall collapseAction{};
	if (context.params.app && context.params.interfaceState) {
		collapseAction = ActionCall{context.params.app->actions().uiActions().make(
			kCollapseCard, state.collapsed)};
	}

	Clay_ElementDeclaration card{};
	card.layout.sizing = {
		.width = CLAY_SIZING_GROW(0),
		.height = CLAY_SIZING_FIT(0),
	};
	card.layout.layoutDirection = CLAY_TOP_TO_BOTTOM;
	card.backgroundColor = interface_theme::kDepth2Ink;
	card.clip = {.horizontal = true, .vertical = false};
	card.border = {
		.color = state.dirty ? interface_theme::kAccentSignalCoral
			: state.overridePresence.preview ? interface_theme::kAccentCurrent
			: interface_theme::kBorderPrimary,
		.width = state.overridePresence.descendantLive && !state.overridePresence.exactLive
			? Clay_BorderWidth{2, 1, 1, 1, 0}
			: Clay_BorderWidth{1, 1, 1, 1, 0},
	};
	CLAY(context.clayID(), card) {
		context.uiManager.createElement(kDevEditorCardHeader, kCardHeader)
			.setParameters(DevEditorCardHeaderParameters{
				.app = context.params.app,
				.interfaceState = context.params.interfaceState,
				.cardState = &state,
				.quickView = collapsedView,
				.onToggle = collapseAction,
			})
			.setDevInternalCapture(true)
			.draw();
		if (!state.collapsed && field && type) {
			context.uiManager.createElement(kDevEditingField, kEditingField)
				.setParameters(DevEditingFieldParameters{
					.schema = context.params.schema,
					.binding = context.params.binding,
					.app = context.params.app,
					.interfaceState = context.params.interfaceState,
					.value = context.params.value,
					.depth = context.params.depth,
				})
				.setDevInternalCapture(true)
				.draw();
		}
	}
}

namespace {

template <typename Context, typename Predicate>
bool headerControlMatches(Context& context, Predicate&& predicate) {
	auto partClayID = [&](FlowElementPart part) {
		return context.uiManager.toClayEID(context.part(part));
	};
	return predicate(partClayID(DevEditorCardHeader::Parts::collapse)) ||
		predicate(partClayID(DevEditorCardHeader::Parts::copy)) ||
		predicate(partClayID(DevEditorCardHeader::Parts::paste)) ||
		predicate(partClayID(DevEditorCardHeader::Parts::reset));
}

void clearHeaderInteraction(DevEditorCardHeaderState& state) {
	state.isArmed = false;
	state.mouseUpObserved = false;
}

} // namespace

void DevEditorCardHeader::onHovered(InteractionContext& context) {
	if (context.actionAvailability(context.params.onToggle).enabled) {
		context.uiManager.requestCursor(CursorType::PointingHand, 9u);
	}
}

void DevEditorCardHeader::onPressed(InteractionContext& context) {
	auto& state = context.state();
	const bool childConsumed = headerControlMatches(context, [&](Clay_ElementId id) {
		return context.previousInteraction.isPressed(id);
	});
	if (childConsumed || !context.actionAvailability(context.params.onToggle).enabled) {
		clearHeaderInteraction(state);
		return;
	}
	state.isArmed = true;
	state.mouseUpObserved = false;
}

void DevEditorCardHeader::onReleased(InteractionContext& context) {
	auto& state = context.state();
	if (!state.isArmed) return;
	const bool childConsumed = headerControlMatches(context, [&](Clay_ElementId id) {
		return context.previousInteraction.isReleased(id);
	});
	const bool shouldToggle = !childConsumed &&
		context.actionAvailability(context.params.onToggle).enabled;
	clearHeaderInteraction(state);
	if (shouldToggle) (void)context.invoke(context.params.onToggle);
}

void DevEditorCardHeader::runLogic(InteractionContext& context) {
	auto& state = context.state();
	if (!state.isArmed) return;
	if (!context.actionAvailability(context.params.onToggle).enabled) {
		clearHeaderInteraction(state);
		return;
	}
	if (context.uiManager.getCurrentFrameInput().mouseDown[0]) {
		if (state.mouseUpObserved) clearHeaderInteraction(state);
		return;
	}
	if (state.mouseUpObserved) clearHeaderInteraction(state);
	else state.mouseUpObserved = true;
}

void DevEditorCardHeader::buildElement(BuildContext& context) {
	DevEditorCardState* card = context.params.cardState;
	if (!card) return;
	App* app = context.params.app;
	DevInterfaceState* interfaceState = context.params.interfaceState;
	ActionCall collapseAction = context.params.onToggle;
	ActionCall copyAction{};
	ActionCall pasteAction{};
	ActionCall resetAction{};
	bool canPaste = false;
	bool canReset = false;
	if (app && interfaceState) {
		auto& actions = app->actions().uiActions();
		copyAction = ActionCall{actions.make(kCopyField, *card)};
		pasteAction = ActionCall{actions.make(kPasteField, *card)};
		resetAction = ActionCall{actions.make(kResetField, *card)};
		canPaste = card->editable && interfaceState->editorClipboard &&
			interfaceState->editorClipboard.type == card->fieldType;
		const devMode::DevSchemaView schema = app->devTooling().schemas().view();
		canReset = card->editable && card->overridePresence.hasCommittedChange();
	}

	Clay_ElementDeclaration header{};
	header.layout.sizing = {
		.width = CLAY_SIZING_GROW(0),
		.height = CLAY_SIZING_FIXED(38),
	};
	header.layout.padding = Clay_Padding{9, 7, 0, 0};
	header.layout.layoutDirection = CLAY_LEFT_TO_RIGHT;
	header.layout.childGap = 5;
	header.layout.childAlignment = {
		.x = CLAY_ALIGN_X_LEFT,
		.y = CLAY_ALIGN_Y_CENTER,
	};
	header.backgroundColor = interface_theme::kDepth2Ink;
	header.clip = {.horizontal = true, .vertical = true};
	CLAY(context.clayID(), header) {
		Clay_ElementDeclaration labelArea{};
		labelArea.layout.sizing = {
			.width = CLAY_SIZING_GROW(0),
			.height = CLAY_SIZING_GROW(0),
		};
		labelArea.layout.layoutDirection = CLAY_TOP_TO_BOTTOM;
		labelArea.layout.childGap = 1;
		labelArea.layout.childAlignment = {
			.x = CLAY_ALIGN_X_LEFT,
			.y = CLAY_ALIGN_Y_CENTER,
		};
		labelArea.clip = {.horizontal = true, .vertical = true};
		CLAY(context.clayID(kFieldName), labelArea) {
			Clay_TextElementConfig nameStyle = textConfig(interface_theme::kTextCanvas, 11);
			nameStyle.wrapMode = CLAY_TEXT_WRAP_NONE;
			CLAY_TEXT(
				context.uiManager.toClayString(card->fieldName),
				CLAY_TEXT_CONFIG(nameStyle));
			if (card->collapsed && !context.params.quickView.primary.empty()) {
				const DevQuickView& quick = context.params.quickView;
				Clay_ElementDeclaration quickRow{};
				quickRow.layout.sizing = {
					.width = CLAY_SIZING_GROW(0), .height = CLAY_SIZING_FIT(0)};
				quickRow.layout.layoutDirection = CLAY_LEFT_TO_RIGHT;
				quickRow.layout.childGap = 4;
				quickRow.layout.childAlignment = {
					.x = CLAY_ALIGN_X_LEFT, .y = CLAY_ALIGN_Y_CENTER};
				quickRow.clip = {.horizontal = true, .vertical = true};
				CLAY(context.clayID(kQuickValue), quickRow) {
					if (quick.thumbnail) {
						FSEL::ButtonParameters thumbnail{};
						thumbnail.enabled = false;
						thumbnail.contentMode = FSEL::ButtonContentMode::IconOnly;
						thumbnail.icon = *quick.thumbnail;
						thumbnail.tintIcon = false;
						thumbnail.sizing = {
							.width = CLAY_SIZING_FIXED(14), .height = CLAY_SIZING_FIXED(14)};
						thumbnail.padding = Clay_Padding{};
						thumbnail.borderWidth = Clay_BorderWidth{1, 1, 1, 1, 0};
						thumbnail.cornerRadius = CLAY_CORNER_RADIUS(2);
						thumbnail.disabledOverrides.backgroundColor = interface_theme::kDepth3Elevated;
						thumbnail.disabledOverrides.borderColor = interface_theme::kBorderVisible;
						context.uiManager.createElement(FSEL::kButton, "thumbnail")
							.setParameters(std::move(thumbnail)).setDevInternalCapture(true).draw();
					}
					if (quick.swatch) {
						Clay_ElementDeclaration swatch{};
						swatch.layout.sizing = {
							.width = CLAY_SIZING_FIXED(12), .height = CLAY_SIZING_FIXED(12)};
						swatch.backgroundColor = *quick.swatch;
						swatch.cornerRadius = CLAY_CORNER_RADIUS(2);
						swatch.border = {.color = interface_theme::kBorderVisible,
							.width = Clay_BorderWidth{1, 1, 1, 1, 0}};
						CLAY(context.clayID("swatch"), swatch) {}
					}
					const Clay_Color quickColor = quick.status == DevQuickStatus::Normal
						? interface_theme::kTextMuted
						: quick.status == DevQuickStatus::Unavailable
							? interface_theme::kStatusRed : interface_theme::kStatusAmber;
					const Clay_TextElementConfig quickStyle = textConfig(quickColor, 9);
					CLAY_TEXT(context.uiManager.toClayString(quick.primary),
						CLAY_TEXT_CONFIG(quickStyle));
					if (!quick.secondary.empty()) {
						const Clay_TextElementConfig secondaryStyle =
							textConfig(interface_theme::kTextSecondary, 9);
						CLAY_TEXT(context.uiManager.toClayString(quick.secondary),
							CLAY_TEXT_CONFIG(secondaryStyle));
					}
				}
			}
		}
		drawHeaderButton(
			context, DevEditorCardHeader::Parts::collapse,
			card->collapsed ? "↓" : "↑", collapseAction, true,
			card->collapsed
				? context.resources().expandIcon : context.resources().collapseIcon);
		drawHeaderButton(
			context, DevEditorCardHeader::Parts::copy,
			"C", copyAction, card->hasCurrentValue,
			context.resources().copyIcon);
		drawHeaderButton(
			context, DevEditorCardHeader::Parts::paste,
			"P", pasteAction, canPaste,
			context.resources().pasteIcon);
		drawHeaderButton(
			context, DevEditorCardHeader::Parts::reset,
			"R", resetAction, canReset,
			context.resources().revertIcon);
	}
}

void DevEditingField::buildElement(BuildContext& context) {
	const devMode::DevSchemaGeneration* schema =
		context.params.schema ? &*context.params.schema : nullptr;
	const devMode::DevFieldSchema* parentField = schema
		? fieldAt(*schema, context.params.binding.field) : nullptr;
	const devMode::DevTypeSchema* type = parentField && schema
		? schema->type(parentField->valueType) : nullptr;
	const bool arbitraryStruct = type && type->kind == devMode::DevTypeKind::Object &&
		editorKind(parentField, type) == devMode::DevEditorKind::ObjectGroup &&
		!schema->fieldsOf(parentField->valueType).empty();
	const uint16_t inset = context.params.depth == 0u ? 8u
		: context.params.depth == 1u ? 4u : 0u;

	Clay_ElementDeclaration field{};
	field.layout.sizing = {
		.width = CLAY_SIZING_GROW(0),
		.height = CLAY_SIZING_FIT(0),
	};
	field.layout.padding = Clay_Padding{inset, inset, inset, inset};
	field.layout.layoutDirection = CLAY_TOP_TO_BOTTOM;
	field.layout.childGap = 6;
	field.backgroundColor = interface_theme::kDepth0Keel;
	CLAY(context.clayID(), field) {
		[&]() {
		if (context.params.depth >= kMaximumEditorDepth) {
			Clay_TextElementConfig style = textConfig(interface_theme::kStatusAmber, 9);
			style.wrapMode = CLAY_TEXT_WRAP_WORDS;
			CLAY_TEXT(
				context.uiManager.toClayString("Unsupported: recursive schema cycle or editor depth limit"),
				CLAY_TEXT_CONFIG(style));
			return;
		}
		if (!schema || !parentField || !type || !arbitraryStruct) {
			context.uiManager.createElement(kDevEditor, kEditor)
				.setParameters(DevEditorParameters{
					.schema = context.params.schema,
					.binding = context.params.binding,
					.app = context.params.app,
					.interfaceState = context.params.interfaceState,
					.value = context.params.value,
					.showFieldName = false,
				})
				.setDevInternalCapture(true)
				.draw();
		} else {
			for (const devMode::DevFieldSchema& child : schema->fieldsOf(parentField->valueType)) {
				const devMode::DevTypeSchema* childType = schema->type(child.valueType);
				const void* childValue = nullptr;
				if (context.params.value && child.operations < schema->fieldOperations.size()) {
					const devMode::DevFieldOps* operations = schema->fieldOperations[child.operations];
					if (operations && operations->constAddress) {
						childValue = operations->constAddress(context.params.value);
					}
				}
				DevEditorBinding childBinding = context.params.binding;
				childBinding.nestedPath.push_back(parentField->id);
				childBinding.field = fieldIndex(*schema, child);
				const bool nestedStruct = childType &&
					childType->kind == devMode::DevTypeKind::Object &&
					editorKind(&child, childType) == devMode::DevEditorKind::ObjectGroup &&
					!schema->fieldsOf(child.valueType).empty();
				if (nestedStruct) {
					context.uiManager.createElement(kDevEditorCard, Keyed(kNestedCard, child.id))
						.setParameters(DevEditorCardParameters{
							.schema = context.params.schema,
							.binding = std::move(childBinding),
							.app = context.params.app,
							.interfaceState = context.params.interfaceState,
							.value = childValue,
							.depth = static_cast<uint16_t>(context.params.depth + 1u),
						})
						.setDevInternalCapture(true)
						.draw();
				} else {
					context.uiManager.createElement(kDevEditor, Keyed(kEditor, child.id))
						.setParameters(DevEditorParameters{
							.schema = context.params.schema,
							.binding = std::move(childBinding),
							.app = context.params.app,
							.interfaceState = context.params.interfaceState,
							.value = childValue,
							.showFieldName = true,
						})
						.setDevInternalCapture(true)
						.draw();
				}
			}
		}
		}();
	}
}

void DevEditor::buildElement(BuildContext& context) {
	const devMode::DevSchemaGeneration* schema =
		context.params.schema ? &*context.params.schema : nullptr;
	const devMode::DevFieldSchema* field = schema
		? fieldAt(*schema, context.params.binding.field) : nullptr;
	const devMode::DevTypeSchema* type = field && schema
		? schema->type(field->valueType) : nullptr;
	std::string_view fieldName = field && schema ? schema->string(field->displayName) : std::string_view{};
	if (fieldName.empty() && field && schema) fieldName = schema->string(field->name);
	const std::string typeLabel = schema
		? normalizedTypeName(*schema, type) + ":" : "Unknown:";

	Clay_ElementDeclaration row{};
	row.layout.sizing = {
		.width = CLAY_SIZING_GROW(0),
		.height = CLAY_SIZING_FIT(32),
	};
	row.layout.layoutDirection = CLAY_TOP_TO_BOTTOM;
	row.layout.childGap = 7;
	row.layout.childAlignment = {
		.x = CLAY_ALIGN_X_LEFT,
		.y = CLAY_ALIGN_Y_CENTER,
	};
	CLAY(context.clayID(), row) {
		Clay_ElementDeclaration labels{};
		labels.layout.sizing = {.width = CLAY_SIZING_GROW(0), .height = CLAY_SIZING_FIT(0)};
		labels.layout.layoutDirection = CLAY_LEFT_TO_RIGHT;
		labels.layout.childGap = 6;
		labels.clip = {.horizontal = true, .vertical = true};
		CLAY(context.clayID("labels"), labels) {
			if (context.params.showFieldName && !fieldName.empty()) {
				Clay_TextElementConfig nameStyle = textConfig(interface_theme::kTextSecondary, 10);
				nameStyle.wrapMode = CLAY_TEXT_WRAP_WORDS;
				CLAY_TEXT(context.uiManager.toClayString(fieldName), CLAY_TEXT_CONFIG(nameStyle));
			} else {
				Clay_TextElementConfig typeStyle = textConfig(interface_theme::kAccentSeaGlass, 9);
				typeStyle.wrapMode = CLAY_TEXT_WRAP_WORDS;
				CLAY_TEXT(context.uiManager.toClayString(typeLabel), CLAY_TEXT_CONFIG(typeStyle));
			}
		}
		context.uiManager.createElement(kDevTypeEditor, kTypeEditor)
			.setParameters(DevTypeEditorParameters{
				.schema = context.params.schema,
				.binding = context.params.binding,
				.app = context.params.app,
				.interfaceState = context.params.interfaceState,
				.value = context.params.value,
			})
			.setDevInternalCapture(true)
			.draw();
	}
}

void DevCatalogueChoice::buildElement(BuildContext& context) {
	DevCatalogueChoiceState& state = context.state();
	auto lowercase = [](std::string_view value) {
		std::string result(value);
		std::ranges::transform(result, result.begin(), [](unsigned char character) {
			return static_cast<char>(std::tolower(character));
		});
		return result;
	};
	const std::string needle = lowercase(state.search);
	state.filteredLabels.clear();
	state.filteredOptions.clear();
	state.filteredLabels.reserve(context.params.options.size());
	for (const FSEL::ComboBoxOption& option : context.params.options) {
		if (!needle.empty() && lowercase(option.text).find(needle) == std::string::npos) {
			continue;
		}
		state.filteredLabels.emplace_back(option.text);
	}
	state.filteredOptions.reserve(state.filteredLabels.size());
	for (const FSEL::ComboBoxOption& option : context.params.options) {
		if (!needle.empty() && lowercase(option.text).find(needle) == std::string::npos) {
			continue;
		}
		const std::size_t index = state.filteredOptions.size();
		state.filteredOptions.push_back(FSEL::ComboBoxOption{
			.value = option.value, .text = state.filteredLabels[index],
			.icon = option.icon, .enabled = option.enabled});
	}
	Clay_ElementDeclaration root{};
	root.layout.sizing = {
		.width = CLAY_SIZING_GROW(0), .height = CLAY_SIZING_FIT(0)};
	root.layout.layoutDirection = CLAY_TOP_TO_BOTTOM;
	root.layout.childGap = 4;
	root.clip = {.horizontal = true, .vertical = false};
	CLAY(context.clayID(), root) {
		Clay_ElementDeclaration slot{};
		slot.layout.sizing = {
			.width = CLAY_SIZING_GROW(0), .height = CLAY_SIZING_FIXED(kTypeControlHeight)};
		slot.clip = {.horizontal = true, .vertical = false};
		CLAY(context.clayID("choice-slot"), slot) {
		FSEL::ComboBoxParameters combo{};
		combo.options = state.filteredOptions;
		combo.selectedValue = context.params.selectedValue;
		combo.onChanged = context.params.onChanged;
		combo.placeholder = context.params.placeholder;
		combo.enabled = context.params.enabled;
		combo.popupWidthPolicy = FSEL::ComboBoxPopupWidthPolicy::MatchTrigger;
		combo.popupMaxHeight = 260.0f;
		combo.optionHeight = kTypeControlHeight;
		combo.showScrollIndicator = true;
		combo.popupSearchValue = context.params.searchable ? &state.search : nullptr;
		combo.popupSearchPlaceholder = context.params.placeholder;
		combo.fontSize = kTypeControlFontSize;
		combo.sizing = Clay_Sizing{
			.width = CLAY_SIZING_GROW(0), .height = CLAY_SIZING_GROW(0)};
		context.uiManager.createElement(FSEL::kComboBox, "combo")
			.setParameters(std::move(combo)).setDevInternalCapture(true).draw();
		}
	}
}

DevNineSplitEditorResources::DevNineSplitEditorResources(App& app) {
#if FLOWUI_INCLUDE_ICON_MANAGER
	constexpr std::string_view key = "flowui/dev-interface/nine-split/link";
	(void)app.icons().registerSvg(key, kLinkIconSvg);
	linkIcon = app.icons().textureRef(key);
#else
	(void)app;
#endif
}

void DevNineSplitEditor::buildElement(BuildContext& context) {
	const float cellWidth = context.params.mode == DevNineSplitMode::Numeric ? 46.0f : 52.0f;
	const float cellHeight = context.params.mode == DevNineSplitMode::Numeric ? 36.0f : 34.0f;
	Clay_ElementDeclaration grid{};
	grid.layout.sizing = {
		.width = CLAY_SIZING_FIXED(cellWidth * 3.0f),
		.height = CLAY_SIZING_FIXED(cellHeight * 3.0f)};
	grid.layout.layoutDirection = CLAY_TOP_TO_BOTTOM;
	grid.border = {.color = interface_theme::kBorderVisible,
		.width = Clay_BorderWidth{1, 1, 1, 1, 0}};
	grid.cornerRadius = CLAY_CORNER_RADIUS(3);
	grid.clip = {.horizontal = true, .vertical = true};
	CLAY(context.clayID(), grid) {
		for (std::uint64_t row = 0u; row < 3u; ++row) {
			Clay_ElementDeclaration rowLayout{};
			rowLayout.layout.sizing = {
				.width = CLAY_SIZING_FIXED(cellWidth * 3.0f),
				.height = CLAY_SIZING_FIXED(cellHeight)};
			rowLayout.layout.layoutDirection = CLAY_LEFT_TO_RIGHT;
			CLAY(context.clayID(Keyed("row", row)), rowLayout) {
				for (std::uint64_t column = 0u; column < 3u; ++column) {
					const std::uint64_t index = row * 3u + column;
					const DevNineSplitCell& cell = context.params.cells[index];
					Clay_ElementDeclaration slot{};
					slot.layout.sizing = {
						.width = CLAY_SIZING_FIXED(cellWidth),
						.height = CLAY_SIZING_FIXED(cellHeight)};
					slot.layout.padding = Clay_Padding{2, 2, 2, 2};
					slot.layout.childAlignment = {
						.x = CLAY_ALIGN_X_CENTER, .y = CLAY_ALIGN_Y_CENTER};
					slot.backgroundColor = interface_theme::kDepth2Ink;
					slot.border = {.color = interface_theme::kBorderPrimary,
						.width = Clay_BorderWidth{
							0, column < 2u ? uint16_t{1} : uint16_t{0},
							0, row < 2u ? uint16_t{1} : uint16_t{0}, 0}};
					slot.clip = {.horizontal = true, .vertical = true};
					CLAY(context.clayID(Keyed("cell", index)), slot) {
						if (context.params.mode == DevNineSplitMode::Numeric && index == 4u) {
							FSEL::ButtonParameters link{};
							link.onActivate = context.params.centerAction;
							link.enabled = context.params.enabled &&
								static_cast<bool>(context.params.centerAction);
							link.contentMode = FSEL::ButtonContentMode::IconOnly;
							link.icon = context.resources().linkIcon;
							link.sizing = {.width = CLAY_SIZING_FIXED(26),
								.height = CLAY_SIZING_FIXED(26)};
							link.padding = Clay_Padding{4, 4, 4, 4};
							link.iconSize = 14.0f;
							link.borderWidth = Clay_BorderWidth{1, 1, 1, 1, 0};
							link.cornerRadius = CLAY_CORNER_RADIUS(13);
							link.idleOverrides.backgroundColor = context.params.centerSelected
								? interface_theme::kSelectedRow : interface_theme::kDepth3Elevated;
							link.idleOverrides.iconColor = context.params.centerSelected
								? interface_theme::kAccentCurrent : interface_theme::kTextMuted;
							link.idleOverrides.borderColor = context.params.centerSelected
								? interface_theme::kAccentCurrent : interface_theme::kBorderVisible;
							link.hoveredOverrides.backgroundColor = interface_theme::kHoverSurface;
							link.hoveredOverrides.iconColor = interface_theme::kTextCanvas;
							link.hoveredOverrides.borderColor = interface_theme::kAccentCurrent;
							context.uiManager.createElement(FSEL::kButton, "link")
								.setParameters(std::move(link)).setDevInternalCapture(true).draw();
						} else if (context.params.mode == DevNineSplitMode::Numeric &&
							cell.occupied && cell.numericValue) {
							FSEL::DragValueParameters<unsigned int> drag{};
							drag.value = cell.numericValue;
							drag.minimum = 0u;
							drag.step = 1u;
							drag.enabled = context.params.enabled;
							drag.readOnly = !context.params.enabled;
							drag.edit.onChanged = cell.onPreview;
							drag.edit.onCommit = cell.onCommit;
							drag.edit.onCancel = cell.onCancel;
							drag.sizing = {.width = CLAY_SIZING_GROW(0),
								.height = CLAY_SIZING_FIXED(28)};
							drag.fontSize = 9;
							drag.padding = Clay_Padding{3, 3, 3, 3};
							context.uiManager.createElement(
								FSEL::kDragValueUInt, Keyed("value", index))
								.setParameters(std::move(drag)).setDevInternalCapture(true).draw();
						} else if (context.params.mode == DevNineSplitMode::Numeric &&
							cell.occupied && cell.floatingValue) {
							FSEL::DragValueParameters<float> drag{};
							drag.value = cell.floatingValue;
							drag.minimum = 0.0f;
							drag.step = 0.5f;
							drag.enabled = context.params.enabled;
							drag.readOnly = !context.params.enabled;
							drag.edit.onChanged = cell.onPreview;
							drag.edit.onCommit = cell.onCommit;
							drag.edit.onCancel = cell.onCancel;
							drag.sizing = {.width = CLAY_SIZING_GROW(0),
								.height = CLAY_SIZING_FIXED(28)};
							drag.fontSize = 9;
							drag.padding = Clay_Padding{3, 3, 3, 3};
							drag.format.notation = FSEL::NumericFloatNotation::Fixed;
							drag.format.precision = 2;
							context.uiManager.createElement(
								FSEL::kDragValueFloat, Keyed("value", index))
								.setParameters(std::move(drag)).setDevInternalCapture(true).draw();
						} else if (context.params.mode == DevNineSplitMode::Radio && cell.occupied) {
							FSEL::RadioChoiceParameters radio{};
							radio.choiceValue = cell.choiceValue;
							radio.selectedValue = context.params.selectedValue;
							radio.enabled = context.params.enabled;
							radio.onSelected = context.params.onSelected;
							radio.style.sizing = {.width = CLAY_SIZING_GROW(0),
								.height = CLAY_SIZING_GROW(0)};
							radio.style.childAlignment = {
								.x = CLAY_ALIGN_X_CENTER, .y = CLAY_ALIGN_Y_CENTER};
							radio.style.idleOverrides.backgroundColor = interface_theme::kDepth3Elevated;
							radio.style.hoveredOverrides.backgroundColor = interface_theme::kHoverSurface;
							radio.style.selectedOverrides.backgroundColor = interface_theme::kSelectedRow;
							context.uiManager.createElement(
								FSEL::kRadioChoice, Keyed("choice", index))
								.setParameters(std::move(radio)).setDevInternalCapture(true).construct();
							const bool selected = context.params.selectedValue &&
								*context.params.selectedValue == cell.choiceValue;
							const auto style = textConfig(selected ? interface_theme::kTextCanvas
								: interface_theme::kTextSecondary, 7, CLAY_TEXT_ALIGN_CENTER);
							CLAY_TEXT(context.uiManager.toClayString(cell.label), CLAY_TEXT_CONFIG(style));
							context.uiManager.drawConstructed();
						}
					}
				}
			}
		}
	}
}

void DevOptionalTypeEditor::buildElement(BuildContext& context) {
	Clay_ElementDeclaration row{};
	row.layout.sizing = {
		.width = CLAY_SIZING_GROW(0), .height = CLAY_SIZING_FIXED(28)};
	row.layout.layoutDirection = CLAY_LEFT_TO_RIGHT;
	row.layout.childGap = 7;
	row.layout.childAlignment = {
		.x = CLAY_ALIGN_X_LEFT, .y = CLAY_ALIGN_Y_CENTER};
	CLAY(context.clayID(), row) {
		FSEL::SwitchParameters toggle{};
		toggle.isOn = context.params.present;
		toggle.enabled = context.params.enabled;
		toggle.onToggle = context.params.onToggle;
		toggle.trackWidth = 34.0f;
		toggle.trackHeight = 18.0f;
		context.uiManager.createElement(FSEL::kSwitch, "presence")
			.setParameters(std::move(toggle)).setDevInternalCapture(true).draw();
		const auto style = textConfig(
			context.params.present ? interface_theme::kTextSecondary
				: interface_theme::kTextMuted,
			9);
		CLAY_TEXT(
			context.uiManager.toClayString(
				context.params.present ? "Has value" : "None"),
			CLAY_TEXT_CONFIG(style));
	}
}

void DevColorTypeEditor::buildElement(BuildContext& context) {
	DevTypeEditorState* editor = context.params.editor;
	if (!editor || !editor->editValue) return;
	auto& color = *static_cast<Clay_Color*>(editor->editValue);
	const bool textFocused = context.uiManager.inputFields().hasPrimaryFieldFocus();
	if (editor->semanticMode != kColorSemanticMode ||
		(!editor->previewActive && !textFocused)) {
		editor->semanticMode = kColorSemanticMode;
		editor->semanticChannels = {color.r, color.g, color.b, color.a};
		updateColorHexDraft(*editor);
		editor->draftValid = true;
	}

	DevColorTypeEditorState& state = context.state();
	ActionCall openPopup{};
	ActionCall closePopup{};
	ActionCall channelCommit{};
	ActionCall channelPreview{};
	ActionCall channelCancel{};
	ActionCall hexCommit{};
	ActionCall hexPreview{};
	if (editor->app) {
		auto& actions = editor->app->actions().uiActions();
		openPopup = ActionCall{actions.make(kOpenColorPopup, state)};
		closePopup = ActionCall{actions.make(kCloseColorPopup, state)};
		if (editor->interfaceState && editor->editable) {
			channelPreview = ActionCall{actions.make(kPreviewColorChannels, *editor)};
			channelCommit = ActionCall{actions.make(kCommitColorChannels, *editor)};
			channelCancel = ActionCall{actions.make(kCancelColorChannels, *editor)};
			hexPreview = ActionCall{actions.make(kPreviewColorHex, *editor)};
			hexCommit = ActionCall{actions.make(kCommitColorHex, *editor)};
		}
	}

	Clay_ElementDeclaration compact{};
	compact.layout.sizing = {
		.width = CLAY_SIZING_GROW(0), .height = CLAY_SIZING_FIXED(kTypeControlHeight)};
	compact.layout.layoutDirection = CLAY_LEFT_TO_RIGHT;
	compact.layout.childGap = 7;
	compact.layout.childAlignment = {
		.x = CLAY_ALIGN_X_LEFT, .y = CLAY_ALIGN_Y_CENTER};
	compact.clip = {.horizontal = true, .vertical = false};
	CLAY(context.clayID(), compact) {
		const Clay_Color swatchColor{
			editor->semanticChannels[0], editor->semanticChannels[1],
			editor->semanticChannels[2], editor->semanticChannels[3]};
		FSEL::ButtonParameters swatch{};
		swatch.onActivate = openPopup;
		swatch.enabled = editor->app != nullptr;
		swatch.contentMode = FSEL::ButtonContentMode::None;
		swatch.sizing = {
			.width = CLAY_SIZING_FIXED(38), .height = CLAY_SIZING_FIXED(30)};
		swatch.padding = Clay_Padding{};
		swatch.borderWidth = Clay_BorderWidth{1, 1, 1, 1, 0};
		swatch.cornerRadius = CLAY_CORNER_RADIUS(3);
		swatch.idleOverrides.backgroundColor = swatchColor;
		swatch.idleOverrides.borderColor = state.popupOpen
			? interface_theme::kAccentCurrent : interface_theme::kBorderVisible;
		swatch.hoveredOverrides.backgroundColor = swatchColor;
		swatch.hoveredOverrides.borderColor = interface_theme::kAccentSeaGlass;
		swatch.pressedOverrides.backgroundColor = swatchColor;
		swatch.pressedOverrides.borderColor = interface_theme::kAccentCurrent;
		context.createPart(FSEL::kButton, Parts::swatch)
			.setParameters(std::move(swatch)).setDevInternalCapture(true).draw();

		FSEL::TextInputParameters hex{};
		hex.value = &editor->semanticText;
		hex.syncPolicy = FSEL::TextFieldSyncPolicy::Live;
		hex.actions.onChanged = hexPreview;
		hex.actions.onCommit = hexCommit;
		hex.enabled = editor->editable;
		hex.readOnly = !editor->editable;
		hex.placeholder = "#RGB, #RRGGBB, or #RRGGBBAA";
		hex.valid = editor->draftValid;
		hex.maxBytes = 9u;
		hex.sizing = {
			.width = CLAY_SIZING_GROW(0), .height = CLAY_SIZING_GROW(0)};
		hex.fontSize = kTypeControlFontSize;
		hex.padding = kTypeControlPadding;
		context.uiManager.createElement(FSEL::kTextInput, "hex")
			.setParameters(std::move(hex)).setDevInternalCapture(true).draw();
	}

	if (!state.popupOpen) return;
	context.uiManager.createElement(FSEL::kPopupSurface, "channels")
		.setParameters(FSEL::PopupSurfaceParameters{
			.popupRequest = PopupRequest{
				.anchor = PopupAnchor::element(context.part(Parts::swatch)),
				.placement = PopupPlacement{
					.anchorPoint = PopupAttachmentPoint::BottomLeft,
					.popupPoint = PopupAttachmentPoint::TopLeft,
					.offset = Clay_Vector2{0.0f, 6.0f}},
				.layer = PopupLayer::CasualPopup,
				.outsidePress = PopupOutsidePressPolicy::DismissAndBlockAnchor},
			.onDismissed = closePopup,
			.sizing = {
				.width = CLAY_SIZING_FIXED(190), .height = CLAY_SIZING_FIT(0)},
			.padding = Clay_Padding{8, 8, 8, 8},
			.childGap = 5,
			.layoutDirection = CLAY_TOP_TO_BOTTOM,
			.backgroundColor = interface_theme::kDepth1Panel,
			.borderColor = interface_theme::kBorderVisible,
			.borderWidth = Clay_BorderWidth{1, 1, 1, 1, 0},
			.cornerRadius = CLAY_CORNER_RADIUS(4)})
		.setDevInternalCapture(true)
		.construct();
	constexpr std::array<std::string_view, 4> labels{"R", "G", "B", "A"};
	for (std::uint64_t channel = 0; channel < labels.size(); ++channel) {
		Clay_ElementDeclaration row{};
		row.layout.sizing = {
			.width = CLAY_SIZING_GROW(0), .height = CLAY_SIZING_FIXED(28)};
		row.layout.layoutDirection = CLAY_LEFT_TO_RIGHT;
		row.layout.childGap = 6;
		row.layout.childAlignment = {
			.x = CLAY_ALIGN_X_LEFT, .y = CLAY_ALIGN_Y_CENTER};
		CLAY(context.clayID(Keyed("channel", channel)), row) {
			Clay_ElementDeclaration labelSlot{};
			labelSlot.layout.sizing = {
				.width = CLAY_SIZING_FIXED(12), .height = CLAY_SIZING_FIT(0)};
			CLAY(context.clayID(Keyed("channel-label", channel)), labelSlot) {
				const auto style = textConfig(interface_theme::kTextSecondary, 9);
				CLAY_TEXT(context.uiManager.toClayString(labels[channel]),
					CLAY_TEXT_CONFIG(style));
			}
			FSEL::DragValueParameters<float> drag{};
			drag.value = &editor->semanticChannels[channel];
			drag.minimum = 0.0f;
			drag.maximum = 255.0f;
			drag.step = 1.0f;
			drag.pixelsPerStep = 4.0f;
			drag.enabled = editor->editable;
			drag.readOnly = !editor->editable;
			drag.edit.onChanged = channelPreview;
			drag.edit.onCommit = channelCommit;
			drag.edit.onCancel = channelCancel;
			drag.sizing = {
				.width = CLAY_SIZING_GROW(0), .height = CLAY_SIZING_FIXED(26)};
			drag.fontSize = 9;
			drag.padding = Clay_Padding{5, 5, 3, 3};
			drag.format.precision = 0;
			drag.format.notation = FSEL::NumericFloatNotation::Fixed;
			drag.format.allowScientificInput = false;
			context.uiManager.createElement(FSEL::kDragValueFloat, Keyed("value", channel))
				.setParameters(std::move(drag)).setDevInternalCapture(true).draw();
		}
	}
	context.uiManager.drawConstructed();
}

template <typename BuildContext>
void drawSpatialTypeEditor(BuildContext& context, bool border) {
	DevTypeEditorState* editor = context.params.editor;
	if (!editor || !editor->editValue) return;
	const uint64_t wantedMode = border ? kBorderSemanticMode : kPaddingSemanticMode;
	if (editor->semanticMode != wantedMode || !editor->previewActive) {
		editor->semanticMode = wantedMode;
		if (border) {
			const auto& value = *static_cast<const Clay_BorderWidth*>(editor->editValue);
			editor->semanticUnsigned = {value.left, value.right, value.top,
				value.bottom, value.betweenChildren};
		} else {
			const auto& value = *static_cast<const Clay_Padding*>(editor->editValue);
			editor->semanticUnsigned = {
				value.left, value.right, value.top, value.bottom, 0u};
		}
	}

	DevNineSplitEditorParameters grid{};
	grid.enabled = editor->editable;
	grid.centerSelected = editor->semanticLinked;
	grid.centerLabel = editor->semanticLinked ? "Linked" : "Unlinked";
	if (editor->app && editor->interfaceState && editor->editable) {
		grid.centerAction = ActionCall{editor->app->actions().uiActions().make(
			kToggleSpatialLink, *editor)};
	}
	const auto setCell = [&](std::size_t position, std::size_t valueIndex) {
		auto& cell = grid.cells[position];
		cell.occupied = true;
		cell.numericValue = &editor->semanticUnsigned[valueIndex];
		if (editor->app && editor->interfaceState && editor->editable) {
			auto& actions = editor->app->actions().uiActions();
			cell.onPreview = ActionCall{actions.make(
				kPreviewSpatialValue, *editor, static_cast<uint64_t>(valueIndex))};
			cell.onCommit = ActionCall{actions.make(
				kCommitSpatialValue, *editor, static_cast<uint64_t>(valueIndex))};
			cell.onCancel = ActionCall{actions.make(
				kCancelSpatialValue, *editor, static_cast<uint64_t>(valueIndex))};
		}
	};
	setCell(1u, 2u);
	setCell(3u, 0u);
	setCell(5u, 1u);
	setCell(7u, 3u);
	context.uiManager.createElement(kDevNineSplitEditor, "grid")
		.setParameters(std::move(grid)).setDevInternalCapture(true).draw();

	if (!border) return;
	Clay_ElementDeclaration row{};
	row.layout.sizing = {
		.width = CLAY_SIZING_FIXED(138), .height = CLAY_SIZING_FIXED(28)};
	row.layout.layoutDirection = CLAY_LEFT_TO_RIGHT;
	row.layout.childGap = 6;
	row.layout.childAlignment = {
		.x = CLAY_ALIGN_X_LEFT, .y = CLAY_ALIGN_Y_CENTER};
	CLAY(context.clayID("between-row"), row) {
		const auto style = textConfig(interface_theme::kTextSecondary, 8);
		CLAY_TEXT(context.uiManager.toClayString("Between"), CLAY_TEXT_CONFIG(style));
		FSEL::DragValueParameters<unsigned int> drag{};
		drag.value = &editor->semanticUnsigned[4];
		drag.minimum = 0u;
		drag.enabled = editor->editable;
		drag.readOnly = !editor->editable;
		if (editor->app && editor->interfaceState && editor->editable) {
			auto& actions = editor->app->actions().uiActions();
			drag.edit.onChanged = ActionCall{actions.make(kPreviewSpatialValue, *editor, 4u)};
			drag.edit.onCommit = ActionCall{actions.make(kCommitSpatialValue, *editor, 4u)};
			drag.edit.onCancel = ActionCall{actions.make(kCancelSpatialValue, *editor, 4u)};
		}
		drag.sizing = {.width = CLAY_SIZING_GROW(0), .height = CLAY_SIZING_FIXED(26)};
		drag.fontSize = 9;
		drag.padding = Clay_Padding{4, 4, 3, 3};
		context.uiManager.createElement(FSEL::kDragValueUInt, "between")
			.setParameters(std::move(drag)).setDevInternalCapture(true).draw();
	}
}

void DevPaddingTypeEditor::buildElement(BuildContext& context) {
	drawSpatialTypeEditor(context, false);
}

void DevBorderWidthTypeEditor::buildElement(BuildContext& context) {
	drawSpatialTypeEditor(context, true);
}

void DevCornerRadiusTypeEditor::buildElement(BuildContext& context) {
	DevTypeEditorState* editor = context.params.editor;
	if (!editor || !editor->editValue) return;
	const auto& radius = *static_cast<const Clay_CornerRadius*>(editor->editValue);
	if (editor->semanticMode != kCornerRadiusSemanticMode || !editor->previewActive) {
		editor->semanticMode = kCornerRadiusSemanticMode;
		editor->semanticCorners = {
			radius.topLeft, radius.topRight, radius.bottomLeft, radius.bottomRight};
	}

	DevNineSplitEditorParameters grid{};
	grid.enabled = editor->editable;
	grid.centerSelected = editor->semanticLinked;
	grid.centerLabel = editor->semanticLinked ? "Linked" : "Unlinked";
	if (editor->app && editor->interfaceState && editor->editable) {
		grid.centerAction = ActionCall{editor->app->actions().uiActions().make(
			kToggleSpatialLink, *editor)};
	}
	const auto setCell = [&](std::size_t position, std::size_t valueIndex) {
		auto& cell = grid.cells[position];
		cell.occupied = true;
		cell.floatingValue = &editor->semanticCorners[valueIndex];
		if (editor->app && editor->interfaceState && editor->editable) {
			auto& actions = editor->app->actions().uiActions();
			cell.onPreview = ActionCall{actions.make(
				kPreviewCornerValue, *editor, static_cast<uint64_t>(valueIndex))};
			cell.onCommit = ActionCall{actions.make(
				kCommitCornerValue, *editor, static_cast<uint64_t>(valueIndex))};
			cell.onCancel = ActionCall{actions.make(
				kCancelCornerValue, *editor, static_cast<uint64_t>(valueIndex))};
		}
	};
	setCell(0u, 0u);
	setCell(2u, 1u);
	setCell(6u, 2u);
	setCell(8u, 3u);
	context.uiManager.createElement(kDevNineSplitEditor, "grid")
		.setParameters(std::move(grid)).setDevInternalCapture(true).draw();
}

void DevAttachmentPointsTypeEditor::buildElement(BuildContext& context) {
	DevTypeEditorState* editor = context.params.editor;
	if (!editor || !editor->editValue) return;
	const auto& points = *static_cast<const Clay_FloatingAttachPoints*>(editor->editValue);
	if (editor->semanticMode != kAttachmentSemanticMode || !editor->previewActive) {
		editor->semanticMode = kAttachmentSemanticMode;
		editor->semanticSelections = {
			static_cast<std::uint64_t>(points.element),
			static_cast<std::uint64_t>(points.parent)};
	}
	constexpr std::array<Clay_FloatingAttachPointType, 9> values{
		CLAY_ATTACH_POINT_LEFT_TOP, CLAY_ATTACH_POINT_CENTER_TOP,
		CLAY_ATTACH_POINT_RIGHT_TOP, CLAY_ATTACH_POINT_LEFT_CENTER,
		CLAY_ATTACH_POINT_CENTER_CENTER, CLAY_ATTACH_POINT_RIGHT_CENTER,
		CLAY_ATTACH_POINT_LEFT_BOTTOM, CLAY_ATTACH_POINT_CENTER_BOTTOM,
		CLAY_ATTACH_POINT_RIGHT_BOTTOM};
	constexpr std::array<std::string_view, 9> labels{
		"Top Left", "Top", "Top Right", "Left", "Center", "Right",
		"Bottom Left", "Bottom", "Bottom Right"};
	for (std::uint64_t anchor = 0u; anchor < 2u; ++anchor) {
		const auto heading = textConfig(interface_theme::kTextSecondary, 9);
		CLAY_TEXT(context.uiManager.toClayString(
			anchor == 0u ? "Element" : "Parent"), CLAY_TEXT_CONFIG(heading));
		DevNineSplitEditorParameters grid{};
		grid.mode = DevNineSplitMode::Radio;
		grid.enabled = editor->editable;
		grid.selectedValue = &editor->semanticSelections[anchor];
		if (editor->app && editor->interfaceState && editor->editable) {
			grid.onSelected = ActionCall{editor->app->actions().uiActions().make(
				kChooseAttachmentPoint, *editor, anchor)};
		}
		for (std::size_t index = 0; index < grid.cells.size(); ++index) {
			grid.cells[index] = DevNineSplitCell{
				.label = labels[index],
				.choiceValue = static_cast<std::uint64_t>(values[index]),
				.occupied = true};
		}
		context.uiManager.createElement(kDevNineSplitEditor, Keyed("grid", anchor))
			.setParameters(std::move(grid)).setDevInternalCapture(true).draw();
	}
}

void DevBooleanTypeEditor::buildElement(BuildContext& context) {
	DevTypeEditorState* editor = context.params.editor;
	if (!editor || !editor->app || !editor->editValue) return;
	const auto schema = editor->app->devTooling().schemas().view();
	const auto* operations = schema
		? typeOperations(*schema, editor->operationType) : nullptr;
	long double current = 0.0L;
	const bool on = operations && operations->numericValue &&
		operations->numericValue(editor->editValue, current) && current != 0.0L;
	ActionCall toggle{};
	if (editor->interfaceState && editor->editable) {
		toggle = ActionCall{editor->app->actions().uiActions().make(
			kToggleEditorValue, *editor)};
	}
	Clay_ElementDeclaration row{};
	row.layout.sizing = {
		.width = CLAY_SIZING_GROW(0), .height = CLAY_SIZING_FIXED(26)};
	row.layout.layoutDirection = CLAY_LEFT_TO_RIGHT;
	row.layout.childGap = 7;
	row.layout.childAlignment = {
		.x = CLAY_ALIGN_X_LEFT, .y = CLAY_ALIGN_Y_CENTER};
	CLAY(context.clayID(), row) {
		FSEL::SwitchParameters parameters{};
		parameters.isOn = on;
		parameters.enabled = editor->editable;
		parameters.onToggle = toggle;
		context.uiManager.createElement(FSEL::kSwitch, "value")
			.setParameters(std::move(parameters)).setDevInternalCapture(true).draw();
		const auto style = textConfig(editor->editable ? interface_theme::kTextCanvas
			: interface_theme::kTextMuted, 9);
		CLAY_TEXT(context.uiManager.toClayString(on ? "On" : "Off"),
			CLAY_TEXT_CONFIG(style));
	}
}

void DevNumericTypeEditor::buildElement(BuildContext& context) {
	DevTypeEditorState* editor = context.params.editor;
	if (!editor || !editor->app || !editor->editValue) return;
	const auto schema = editor->app->devTooling().schemas().view();
	const auto* type = schema ? schema->type(editor->operationType) : nullptr;
	const auto* operations = schema
		? typeOperations(*schema, editor->operationType) : nullptr;
	const auto* field = schema ? fieldAt(*schema, editor->binding.field) : nullptr;
	if (!schema || !type || !operations || !operations->numericValue || !field) return;
	const bool signedValue = type->kind == devMode::DevTypeKind::SignedInteger &&
		type->size <= sizeof(int);
	const bool unsignedValue = type->kind == devMode::DevTypeKind::UnsignedInteger &&
		type->size <= sizeof(unsigned int);
	const bool floatValue = type->kind == devMode::DevTypeKind::FloatingPoint &&
		type->size <= sizeof(float);
	if (!signedValue && !unsignedValue && !floatValue) return;

	long double current = 0.0L;
	const bool inputFocused = context.uiManager.inputFields().hasPrimaryFieldFocus();
	if ((editor->dragValueType != type->id ||
		(!editor->previewActive && !inputFocused)) &&
		operations->numericValue(editor->editValue, current)) {
		editor->dragValueType = type->id;
		if (signedValue) editor->dragSigned = static_cast<int>(current);
		else if (unsignedValue) editor->dragUnsigned = static_cast<unsigned int>(current);
		else editor->dragFloat = static_cast<float>(current);
	}
	ActionCall commit{}, preview{}, cancel{};
	if (editor->interfaceState && editor->editable) {
		auto& actions = editor->app->actions().uiActions();
		commit = ActionCall{actions.make(kCommitEditorDragValue, *editor)};
		preview = ActionCall{actions.make(kPreviewEditorDragValue, *editor)};
		cancel = ActionCall{actions.make(kCancelEditorDragValue, *editor)};
	}
	const auto* constraint = field->constraint < schema->constraints.size()
		? &schema->constraints[field->constraint].numeric : nullptr;
	Clay_ElementDeclaration slot{};
	slot.layout.sizing = {
		.width = CLAY_SIZING_GROW(0), .height = CLAY_SIZING_FIXED(kTypeControlHeight)};
	slot.clip = {.horizontal = true, .vertical = false};
	CLAY(context.clayID(), slot) {
		if (signedValue) {
			FSEL::DragValueParameters<int> drag{};
			drag.value = &editor->dragSigned;
			if (constraint && constraint->hasMinimum) drag.minimum = static_cast<int>(
				std::clamp(constraint->minimum,
					static_cast<double>(std::numeric_limits<int>::lowest()),
					static_cast<double>(std::numeric_limits<int>::max())));
			if (constraint && constraint->hasMaximum) drag.maximum = static_cast<int>(
				std::clamp(constraint->maximum,
					static_cast<double>(std::numeric_limits<int>::lowest()),
					static_cast<double>(std::numeric_limits<int>::max())));
			drag.step = constraint && constraint->hasStep
				? std::max(1, static_cast<int>(std::round(constraint->step))) : 1;
			drag.pixelsPerStep = 6.0f;
			drag.enabled = editor->editable;
			drag.readOnly = !editor->editable;
			drag.edit = {.onChanged = preview, .onCommit = commit, .onCancel = cancel};
			drag.sizing = {.width = CLAY_SIZING_GROW(0), .height = CLAY_SIZING_GROW(0)};
			drag.fontSize = kTypeControlFontSize;
			drag.padding = kTypeControlPadding;
			context.uiManager.createElement(FSEL::kDragValueInt, "value")
				.setParameters(std::move(drag)).setDevInternalCapture(true).draw();
		} else if (unsignedValue) {
			FSEL::DragValueParameters<unsigned int> drag{};
			drag.value = &editor->dragUnsigned;
			if (constraint && constraint->hasMinimum) drag.minimum =
				static_cast<unsigned int>(std::clamp(constraint->minimum, 0.0,
					static_cast<double>(std::numeric_limits<unsigned int>::max())));
			if (constraint && constraint->hasMaximum) drag.maximum =
				static_cast<unsigned int>(std::clamp(constraint->maximum, 0.0,
					static_cast<double>(std::numeric_limits<unsigned int>::max())));
			drag.step = constraint && constraint->hasStep
				? std::max(1u, static_cast<unsigned int>(std::round(constraint->step))) : 1u;
			drag.pixelsPerStep = 6.0f;
			drag.enabled = editor->editable;
			drag.readOnly = !editor->editable;
			drag.edit = {.onChanged = preview, .onCommit = commit, .onCancel = cancel};
			drag.sizing = {.width = CLAY_SIZING_GROW(0), .height = CLAY_SIZING_GROW(0)};
			drag.fontSize = kTypeControlFontSize;
			drag.padding = kTypeControlPadding;
			context.uiManager.createElement(FSEL::kDragValueUInt, "value")
				.setParameters(std::move(drag)).setDevInternalCapture(true).draw();
		} else {
			FSEL::DragValueParameters<float> drag{};
			drag.value = &editor->dragFloat;
			if (constraint && constraint->hasMinimum) drag.minimum =
				static_cast<float>(constraint->minimum);
			if (constraint && constraint->hasMaximum) drag.maximum =
				static_cast<float>(constraint->maximum);
			drag.step = constraint && constraint->hasStep
				? static_cast<float>(constraint->step) : 0.1f;
			drag.pixelsPerStep = 6.0f;
			drag.enabled = editor->editable;
			drag.readOnly = !editor->editable;
			drag.edit = {.onChanged = preview, .onCommit = commit, .onCancel = cancel};
			drag.sizing = {.width = CLAY_SIZING_GROW(0), .height = CLAY_SIZING_GROW(0)};
			drag.fontSize = kTypeControlFontSize;
			drag.padding = kTypeControlPadding;
			drag.format.precision = 6;
			drag.format.allowScientificInput = true;
			context.uiManager.createElement(FSEL::kDragValueFloat, "value")
				.setParameters(std::move(drag)).setDevInternalCapture(true).draw();
		}
	}
}

void DevTextTypeEditor::buildElement(BuildContext& context) {
	DevTypeEditorState* editor = context.params.editor;
	if (!editor || !editor->app) return;
	const auto schema = editor->app->devTooling().schemas().view();
	const auto* field = schema ? fieldAt(*schema, editor->binding.field) : nullptr;
	const auto* operations = schema
		? typeOperations(*schema, editor->operationType) : nullptr;
	if (!schema || !field || !operations) return;
	if (!editor->previewActive &&
		!context.uiManager.inputFields().hasPrimaryFieldFocus() &&
		operations->textView && editor->editValue) {
		editor->draft = std::string(operations->textView(editor->editValue));
		editor->draftValid = true;
	}
	ActionCall commit{}, preview{};
	if (editor->interfaceState && editor->editable) {
		auto& actions = editor->app->actions().uiActions();
		commit = ActionCall{actions.make(kCommitEditorDraft, *editor)};
		preview = ActionCall{actions.make(kPreviewEditorDraft, *editor)};
	}
	FSEL::TextInputParameters input{};
	input.value = &editor->draft;
	input.syncPolicy = FSEL::TextFieldSyncPolicy::Live;
	input.actions.onChanged = preview;
	input.actions.onCommit = commit;
	input.enabled = editor->editable;
	input.readOnly = !editor->editable;
	input.placeholder = editor->editable ? std::string_view{} : "Read only";
	input.valid = editor->draftValid;
	input.maxBytes = field->constraint < schema->constraints.size() &&
		schema->constraints[field->constraint].hasTextMaximum
			? schema->constraints[field->constraint].textMaximum : 128u;
	input.sizing = {.width = CLAY_SIZING_GROW(0), .height = CLAY_SIZING_GROW(0)};
	input.fontSize = kTypeControlFontSize;
	input.padding = kTypeControlPadding;
	Clay_ElementDeclaration slot{};
	slot.layout.sizing = {
		.width = CLAY_SIZING_GROW(0), .height = CLAY_SIZING_FIXED(kTypeControlHeight)};
	slot.clip = {.horizontal = true, .vertical = false};
	CLAY(context.clayID(), slot) {
		context.uiManager.createElement(FSEL::kTextInput, "value")
			.setParameters(std::move(input)).setDevInternalCapture(true).draw();
	}
}

void DevEnumTypeEditor::buildElement(BuildContext& context) {
	DevTypeEditorState* editor = context.params.editor;
	if (!editor || !editor->app || !editor->editValue) return;
	const auto schema = editor->app->devTooling().schemas().view();
	const auto* type = schema ? schema->type(editor->operationType) : nullptr;
	const auto* operations = schema
		? typeOperations(*schema, editor->operationType) : nullptr;
	const auto* field = schema ? fieldAt(*schema, editor->binding.field) : nullptr;
	if (!schema || !type || !operations || !field ||
		type->enumeration.values.count == 0u) return;
	const auto kind = editorKind(field, type);
	long double numeric = 0.0L;
	if (operations->numericValue) operations->numericValue(editor->editValue, numeric);
	if (kind == devMode::DevEditorKind::Flags) {
		const uint64_t bits = static_cast<uint64_t>(numeric);
		for (uint32_t index = 0; index < type->enumeration.values.count; ++index) {
			const auto& option = schema->enumValues[type->enumeration.values.first + index];
			ActionCall toggle{};
			if (editor->interfaceState && editor->editable) {
				toggle = ActionCall{editor->app->actions().uiActions().make(
					kToggleEditorFlag, *editor, option.bits)};
			}
			FSEL::ButtonParameters button{};
			button.onActivate = toggle;
			button.enabled = editor->editable;
			button.contentMode = FSEL::ButtonContentMode::TextOnly;
			const std::string label =
				std::string((bits & option.bits) == option.bits ? "✓ " : "□ ") +
				std::string(schema->string(option.name));
			button.text = label;
			button.sizing = {
				.width = CLAY_SIZING_GROW(0), .height = CLAY_SIZING_FIXED(26)};
			context.uiManager.createElement(FSEL::kButton, Keyed("flag", option.bits))
				.setParameters(std::move(button)).setDevInternalCapture(true).draw();
		}
		return;
	}

	editor->enumSelection = static_cast<uint64_t>(numeric);
	if (editor->choiceType != type->id) {
		editor->choiceType = type->id;
		editor->choiceLabels.clear();
		editor->choiceOptions.clear();
		editor->choiceLabels.reserve(type->enumeration.values.count);
		for (uint32_t index = 0; index < type->enumeration.values.count; ++index) {
			const auto& option = schema->enumValues[type->enumeration.values.first + index];
			editor->choiceLabels.emplace_back(schema->string(option.name));
		}
		editor->choiceOptions.reserve(type->enumeration.values.count);
		for (uint32_t index = 0; index < type->enumeration.values.count; ++index) {
			const auto& option = schema->enumValues[type->enumeration.values.first + index];
			editor->choiceOptions.push_back({
				.value = option.bits, .text = editor->choiceLabels[index]});
		}
	}
	ActionCall changed{};
	if (editor->interfaceState && editor->editable) {
		changed = ActionCall{editor->app->actions().uiActions().make(
			kChooseEditorEnum, *editor)};
	}
	FSEL::ComboBoxParameters combo{};
	combo.options = editor->choiceOptions;
	combo.selectedValue = &editor->enumSelection;
	combo.enabled = editor->editable;
	combo.onChanged = changed;
	combo.fontSize = kTypeControlFontSize;
	combo.optionHeight = kTypeControlHeight;
	combo.sizing = {
		.width = CLAY_SIZING_GROW(0), .height = CLAY_SIZING_GROW(0)};
	Clay_ElementDeclaration slot{};
	slot.layout.sizing = {
		.width = CLAY_SIZING_GROW(0), .height = CLAY_SIZING_FIXED(kTypeControlHeight)};
	slot.clip = {.horizontal = true, .vertical = false};
	CLAY(context.clayID(), slot) {
		context.uiManager.createElement(FSEL::kComboBox, "value")
			.setParameters(std::move(combo)).setDevInternalCapture(true).draw();
	}
}

void DevTypeEditor::buildElement(BuildContext& context) {
	const devMode::DevSchemaGeneration* schema =
		context.params.schema ? &*context.params.schema : nullptr;
	const devMode::DevFieldSchema* field = schema
		? fieldAt(*schema, context.params.binding.field) : nullptr;
	const devMode::DevTypeSchema* declaredType = field && schema
		? schema->type(field->valueType) : nullptr;
	if (!schema || !field || !declaredType) return;

	DevTypeEditorState& state = context.state();
	state.app = context.params.app;
	state.interfaceState = context.params.interfaceState;
	state.binding = context.params.binding;
	state.type = declaredType->id;
	state.typeIndex = field->valueType;
	state.operationType = field->valueType;
	state.fieldName = std::string(schema->string(field->displayName));
	if (state.fieldName.empty()) state.fieldName = std::string(schema->string(field->name));
	state.editable = context.params.binding.role == DevEditorRole::Parameters &&
		editableCapability(field->effectiveEdit) &&
		declaredType->edit != devMode::DevEditCapability::ViewOnly;
	state.currentValue.reset();
	if (context.params.value) {
		(void)tooling::DevOwnedValue::copyFrom(
			*schema, field->valueType, context.params.value, state.currentValue);
	}
	state.editValue = state.currentValue.data();

	const devMode::DevTypeSchema* type = declaredType;
	const devMode::DevTypeOps* operations = typeOperations(*schema, state.operationType);
	bool optionalPresent = false;
	if (declaredType->kind == devMode::DevTypeKind::Optional && operations &&
		operations->optionalHasValue) {
		optionalPresent = operations->optionalHasValue(state.currentValue.data());
		if (optionalPresent) {
			state.operationType = declaredType->elementType;
			state.editValue = operations->optionalMutableValueAddress
				? operations->optionalMutableValueAddress(state.currentValue.data()) : nullptr;
			type = schema->type(declaredType->elementType);
			operations = typeOperations(*schema, state.operationType);
		}
	}

	if (!state.draftInitialized && type && state.editValue) {
		if (type->kind == devMode::DevTypeKind::Text && operations && operations->textView) {
			state.draft = std::string(operations->textView(state.editValue));
		} else {
			state.draft = numericDraft(*type, operations, state.editValue);
		}
		state.draftInitialized = true;
		state.draftValid = true;
	}

	Clay_ElementDeclaration root{};
	root.layout.sizing = {
		.width = CLAY_SIZING_GROW(0),
		.height = CLAY_SIZING_FIT(0),
	};
	root.layout.layoutDirection = CLAY_TOP_TO_BOTTOM;
	root.layout.childGap = 5;
	root.layout.childAlignment = {
		.x = CLAY_ALIGN_X_LEFT,
		.y = CLAY_ALIGN_Y_CENTER,
	};
	root.clip = {.horizontal = true, .vertical = false};
	const bool dirty = state.app && currentEditorOverride(*state.app, *schema, state);
	root.layout.padding = dirty ? Clay_Padding{3, 3, 3, 3} : Clay_Padding{};
	root.border = {
		.color = dirty ? interface_theme::kAccentSignalCoral : interface_theme::kBorderPrimary,
		.width = dirty ? Clay_BorderWidth{1, 1, 1, 1, 0} : Clay_BorderWidth{},
	};
	root.cornerRadius = CLAY_CORNER_RADIUS(3);

	auto drawValueSurface = [&](std::string_view value, Clay_Color color) {
		Clay_ElementDeclaration surface{};
		surface.layout.sizing = {.width = CLAY_SIZING_GROW(0), .height = CLAY_SIZING_FIXED(26)};
		surface.layout.padding = Clay_Padding{7, 7, 0, 0};
		surface.layout.childAlignment = {.x = CLAY_ALIGN_X_LEFT, .y = CLAY_ALIGN_Y_CENTER};
		surface.backgroundColor = interface_theme::kDepth3Elevated;
		surface.cornerRadius = CLAY_CORNER_RADIUS(3);
		surface.border = {.color = interface_theme::kBorderVisible,
			.width = Clay_BorderWidth{1, 1, 1, 1, 0}};
		surface.clip = {.horizontal = true, .vertical = true};
		Clay_TextElementConfig style = textConfig(color, 9);
		style.wrapMode = CLAY_TEXT_WRAP_NONE;
		CLAY(context.clayID(kEditorMessage), surface) {
			CLAY_TEXT(context.uiManager.toClayString(value), CLAY_TEXT_CONFIG(style));
		}
	};

	CLAY(context.clayID(), root) {
		[&]() {
		if (!state.currentValue) {
			drawValueSurface("Unavailable: value was not captured", interface_theme::kTextMuted);
			return;
		}

		if (declaredType->kind == devMode::DevTypeKind::Optional) {
			ActionCall toggle{};
			if (state.app && state.interfaceState) {
				toggle = ActionCall{state.app->actions().uiActions().make(kToggleOptional, state)};
			}
			context.uiManager.createElement(kDevOptionalTypeEditor, "optional")
				.setParameters(DevOptionalTypeEditorParameters{
					.present = optionalPresent,
					.enabled = state.editable && operations != nullptr,
					.onToggle = toggle})
				.setDevInternalCapture(true).draw();
			if (!optionalPresent) {
				return;
			}
		}

		const devMode::DevEditorKind kind = editorKind(field, type);
		if (kind == devMode::DevEditorKind::Color && state.editValue) {
			context.uiManager.createElement(kDevColorTypeEditor, "color")
				.setParameters(DevColorTypeEditorParameters{.editor = &state})
				.setDevInternalCapture(true).draw();
			return;
		}
		if (kind == devMode::DevEditorKind::Spacing && state.editValue) {
			const bool border = schemaTypeName(*schema, type) == "Clay_BorderWidth";
			if (border) {
				context.uiManager.createElement(kDevBorderWidthTypeEditor, "border-width")
					.setParameters(DevPaddingTypeEditorParameters{.editor = &state})
					.setDevInternalCapture(true).draw();
			} else {
				context.uiManager.createElement(kDevPaddingTypeEditor, "padding")
					.setParameters(DevPaddingTypeEditorParameters{.editor = &state})
					.setDevInternalCapture(true).draw();
			}
			return;
		}
		if (kind == devMode::DevEditorKind::CornerRadius && state.editValue) {
			context.uiManager.createElement(kDevCornerRadiusTypeEditor, "corner-radius")
				.setParameters(DevPaddingTypeEditorParameters{.editor = &state})
				.setDevInternalCapture(true).draw();
			return;
		}
		if (kind == devMode::DevEditorKind::AttachmentPoints && state.editValue) {
			context.uiManager.createElement(
				kDevAttachmentPointsTypeEditor, "attachment-points")
				.setParameters(DevPaddingTypeEditorParameters{.editor = &state})
				.setDevInternalCapture(true).draw();
			return;
		}
		if (type && type->kind == devMode::DevTypeKind::Boolean) {
			context.uiManager.createElement(kDevBooleanTypeEditor, "boolean")
				.setParameters(DevPrimitiveTypeEditorParameters{.editor = &state})
				.setDevInternalCapture(true).draw();
			return;
		}
		const bool nativeNumber = type && (
			(type->kind == devMode::DevTypeKind::SignedInteger && type->size <= sizeof(int)) ||
			(type->kind == devMode::DevTypeKind::UnsignedInteger &&
				type->size <= sizeof(unsigned int)) ||
			(type->kind == devMode::DevTypeKind::FloatingPoint && type->size <= sizeof(float)));
		if (nativeNumber && (kind == devMode::DevEditorKind::SignedNumber ||
			kind == devMode::DevEditorKind::UnsignedNumber ||
			kind == devMode::DevEditorKind::FloatingNumber)) {
			context.uiManager.createElement(kDevNumericTypeEditor, "number")
				.setParameters(DevPrimitiveTypeEditorParameters{.editor = &state})
				.setDevInternalCapture(true).draw();
			return;
		}
		if (type && type->kind == devMode::DevTypeKind::Text &&
			kind == devMode::DevEditorKind::Text) {
			context.uiManager.createElement(kDevTextTypeEditor, "text")
				.setParameters(DevPrimitiveTypeEditorParameters{.editor = &state})
				.setDevInternalCapture(true).draw();
			return;
		}
		if (type && type->kind == devMode::DevTypeKind::Enumeration &&
			(kind == devMode::DevEditorKind::EnumChoice ||
				kind == devMode::DevEditorKind::Flags)) {
			context.uiManager.createElement(kDevEnumTypeEditor, "enum")
				.setParameters(DevPrimitiveTypeEditorParameters{.editor = &state})
				.setDevInternalCapture(true).draw();
			return;
		}
		if (kind == devMode::DevEditorKind::FontChoice) {
			long double current = 0.0L;
			if (!operations || !operations->numericValue ||
				!operations->numericValue(state.editValue, current)) {
				drawValueSurface("Value unavailable", interface_theme::kStatusAmber);
				return;
			}
			state.enumSelection = static_cast<std::uint64_t>(current);
			const std::span<const devMode::DevFontCatalogEntry> fonts =
				state.app ? state.app->devTooling().catalogues().queryFonts()
				: std::span<const devMode::DevFontCatalogEntry>{};
			const std::uint64_t revision = state.app
				? state.app->devTooling().catalogues().revisions().fontRevision : 0u;
			if (state.choiceType != type->id || state.choiceDomain != field->choiceDomain ||
				state.choiceRevision != revision ||
				state.choiceCurrentIdentity != state.enumSelection) {
				state.choiceType = type->id;
				state.choiceDomain = field->choiceDomain;
				state.choiceRevision = revision;
				state.choiceCurrentIdentity = state.enumSelection;
				state.choiceLabels.clear();
				state.choiceOptions.clear();
				std::vector<std::uint64_t> identities;
				identities.reserve(fonts.size());
				bool currentFound = false;
				for (const devMode::DevFontCatalogEntry& font : fonts) {
					const std::uint64_t identity = field->choiceDomain == devMode::DevChoiceDomain::FontFamily
						? static_cast<std::uint64_t>(font.familyHandle)
						: static_cast<std::uint64_t>(font.fontHandle);
					if (std::ranges::find(identities, identity) != identities.end()) continue;
					identities.push_back(identity);
					currentFound = currentFound || identity == state.enumSelection;
					if (field->choiceDomain == devMode::DevChoiceDomain::FontFamily) {
						state.choiceLabels.emplace_back(font.familyName);
					} else {
						std::string label(font.familyName);
						label += " · ";
						label += font.faceName.empty() ? "Regular" : std::string(font.faceName);
						label += " · ";
						label += std::to_string(font.weight);
						if (font.isItalic) label += " Italic";
						state.choiceLabels.push_back(std::move(label));
					}
				}
				if (!currentFound) {
					identities.insert(identities.begin(), state.enumSelection);
					state.choiceLabels.insert(state.choiceLabels.begin(),
						"Missing font · " + std::to_string(state.enumSelection));
				}
				state.choiceOptions.reserve(identities.size());
				for (std::size_t index = 0; index < identities.size(); ++index) {
					state.choiceOptions.push_back(FSEL::ComboBoxOption{
						.value = identities[index], .text = state.choiceLabels[index],
						.enabled = currentFound || index != 0u});
				}
			}
			ActionCall changed{};
			if (state.app && state.interfaceState && state.editable) {
				changed = ActionCall{state.app->actions().uiActions().make(
					kChooseCatalogueNumeric, state)};
			}
			context.uiManager.createElement(kDevCatalogueChoice, "font-choice")
				.setParameters(DevCatalogueChoiceParameters{
					.options = state.choiceOptions,
					.selectedValue = &state.enumSelection,
					.onChanged = changed,
					.placeholder = "Search fonts...",
					.enabled = state.editable})
				.setDevInternalCapture(true).draw();
			return;
		}
		if (kind == devMode::DevEditorKind::ActionChoice) {
			const ActionCall& action = *static_cast<const ActionCall*>(state.editValue);
			const std::span<const devMode::DevActionCatalogEntry> actions = state.app
				? state.app->devTooling().catalogues().queryActions()
				: std::span<const devMode::DevActionCatalogEntry>{};
			const std::uint64_t revision = state.app
				? state.app->devTooling().catalogues().actionRevision() : 0u;
			std::uint8_t currentDomain = 0u;
			std::uint64_t currentStableId = 0u;
			if (action && state.app) {
				if (const auto info = state.app->actions().debugInfo(action)) {
					currentDomain = info->kind == ActionCallKind::App ? 1u : 2u;
					currentStableId = info->kind == ActionCallKind::App
						? info->appId.value : info->uiRecipeId;
				}
			}
			const std::uint64_t currentIdentity = currentDomain == 0u ? 0u
				: catalogueNumericIdentity(currentDomain, currentStableId);
			if (state.choiceDomain != devMode::DevChoiceDomain::Action ||
				state.choiceRevision != revision ||
				state.choiceCurrentIdentity != currentIdentity) {
				state.choiceDomain = devMode::DevChoiceDomain::Action;
				state.choiceRevision = revision;
				state.choiceCurrentIdentity = currentIdentity;
				state.choiceLabels.clear();
				state.choiceOptions.clear();
				state.choiceKeys.clear();
				state.choiceResourceDomains.clear();
				state.choiceIdentities.clear();
				const std::size_t capacity = actions.size() + 2u;
				state.choiceLabels.reserve(capacity);
				state.choiceKeys.reserve(capacity);
				state.choiceResourceDomains.reserve(capacity);
				state.choiceIdentities.reserve(capacity);
				state.choiceLabels.emplace_back("None");
				state.choiceKeys.emplace_back("0");
				state.choiceResourceDomains.push_back(0u);
				state.choiceIdentities.push_back(0u);
				bool currentFound = currentIdentity == 0u;
				for (const devMode::DevActionCatalogEntry& entry : actions) {
					const std::uint8_t domain = entry.kind == devMode::DevActionKind::AppActionBinding
						? 1u : 2u;
					const std::uint64_t identity = catalogueNumericIdentity(domain, entry.stableId);
					currentFound = currentFound || identity == currentIdentity;
					state.choiceIdentities.push_back(identity);
					state.choiceResourceDomains.push_back(domain);
					state.choiceKeys.push_back(std::to_string(entry.stableId));
					std::string label = domain == 1u ? "APP " : "UI ";
					label += entry.debugName.empty() ? "Unnamed action" : std::string(entry.debugName);
					state.choiceLabels.push_back(boundedText(std::move(label), 72u));
				}
				if (!currentFound && currentIdentity != 0u) {
					state.choiceLabels.insert(state.choiceLabels.begin() + 1u,
						"Missing action · " + std::to_string(currentStableId));
					state.choiceKeys.insert(state.choiceKeys.begin() + 1u,
						std::to_string(currentStableId));
					state.choiceResourceDomains.insert(
						state.choiceResourceDomains.begin() + 1u, currentDomain);
					state.choiceIdentities.insert(
						state.choiceIdentities.begin() + 1u, currentIdentity);
				}
				state.choiceOptions.reserve(state.choiceIdentities.size());
				for (std::size_t index = 0; index < state.choiceIdentities.size(); ++index) {
					bool reconstructable = index == 0u;
					if (index != 0u) {
						std::uint64_t stableId = 0u;
						const std::string& encoded = state.choiceKeys[index];
						const auto parsed = std::from_chars(
							encoded.data(), encoded.data() + encoded.size(), stableId);
						if (parsed.ec == std::errc{} && parsed.ptr == encoded.data() + encoded.size()) {
							const auto expectedKind = state.choiceResourceDomains[index] == 1u
								? devMode::DevActionKind::AppActionBinding
								: devMode::DevActionKind::UiActionRecipe;
							const auto entry = std::ranges::find_if(actions,
								[stableId, expectedKind](const devMode::DevActionCatalogEntry& candidate) {
									return candidate.kind == expectedKind &&
										candidate.stableId == stableId;
								});
							reconstructable = entry != actions.end() && entry->isReconstructable;
						}
					}
					state.choiceOptions.push_back(FSEL::ComboBoxOption{
						.value = state.choiceIdentities[index], .text = state.choiceLabels[index],
						.enabled = reconstructable});
				}
			}
			state.resourceSelection = currentIdentity;
			ActionCall changed{};
			if (state.app && state.interfaceState && state.editable) {
				changed = ActionCall{state.app->actions().uiActions().make(kChooseActionCall, state)};
			}
			context.uiManager.createElement(kDevCatalogueChoice, "action-choice")
				.setParameters(DevCatalogueChoiceParameters{
					.options = state.choiceOptions, .selectedValue = &state.resourceSelection,
					.onChanged = changed, .placeholder = "Search actions...",
					.enabled = state.editable})
				.setDevInternalCapture(true).draw();
			if (!state.localDiagnostic.empty()) {
				drawValueSurface(state.localDiagnostic, interface_theme::kStatusAmber);
			}
			return;
		}
		if (kind == devMode::DevEditorKind::ResourceChoice) {
			const TextureRef& texture = *static_cast<const TextureRef*>(state.editValue);
			const std::span<const devMode::DevImageCatalogEntry> images = state.app
				? state.app->devTooling().catalogues().queryImages()
				: std::span<const devMode::DevImageCatalogEntry>{};
			const std::span<const devMode::DevIconCatalogEntry> icons = state.app
				? state.app->devTooling().catalogues().queryIcons()
				: std::span<const devMode::DevIconCatalogEntry>{};
			const std::uint64_t revision = state.app
				? state.app->devTooling().catalogues().imageRevision() ^
					(state.app->devTooling().catalogues().iconRevision() << 1u) : 0u;
			if (state.choiceDomain != devMode::DevChoiceDomain::TextureResource ||
				state.choiceRevision != revision) {
				state.choiceDomain = devMode::DevChoiceDomain::TextureResource;
				state.choiceRevision = revision;
				state.choiceLabels.clear();
				state.choiceOptions.clear();
				state.choiceKeys.clear();
				state.choiceResourceDomains.clear();
				state.choiceIdentities.clear();
				const std::size_t count = images.size() + icons.size() + 1u;
				state.choiceLabels.reserve(count);
				state.choiceKeys.reserve(count);
				state.choiceResourceDomains.reserve(count);
				state.choiceIdentities.reserve(count);
				for (const devMode::DevImageCatalogEntry& image : images) {
					state.choiceKeys.emplace_back(image.key.name);
					state.choiceResourceDomains.push_back(1u);
					state.choiceIdentities.push_back(catalogueIdentity(1u, image.key.name));
					state.choiceLabels.push_back(boundedText(std::string(image.key.name), 72u));
				}
				for (const devMode::DevIconCatalogEntry& icon : icons) {
					state.choiceKeys.emplace_back(icon.iconName);
					state.choiceResourceDomains.push_back(2u);
					state.choiceIdentities.push_back(catalogueIdentity(2u, icon.iconName));
					state.choiceLabels.push_back(boundedText(std::string(icon.iconName), 72u));
				}
				state.choiceOptions.reserve(state.choiceIdentities.size());
				for (std::size_t index = 0; index < state.choiceIdentities.size(); ++index) {
					TextureRef thumbnail{};
					if (index >= images.size()) {
						const auto& icon = icons[index - images.size()];
						thumbnail.handle = icon.atlasTexture;
						thumbnail.uv0x = icon.atlasUv.x;
						thumbnail.uv0y = icon.atlasUv.y;
						thumbnail.uv1x = icon.atlasUv.x + icon.atlasUv.width;
						thumbnail.uv1y = icon.atlasUv.y + icon.atlasUv.height;
						thumbnail.sourceWidth = static_cast<std::int32_t>(icon.targetWidth);
						thumbnail.sourceHeight = static_cast<std::int32_t>(icon.targetHeight);
					}
					state.choiceOptions.push_back(FSEL::ComboBoxOption{
						.value = state.choiceIdentities[index],
						.text = state.choiceLabels[index], .icon = thumbnail});
				}
			}

			state.resourceSelection = 0u;
			for (std::size_t index = 0; index < images.size(); ++index) {
				const bool stableMatch = texture.sourceDomain == ResourceDomain::Image &&
					texture.sourceKey == images[index].key.name;
				if (!stableMatch && images[index].textureHandle != texture.handle) continue;
				state.resourceSelection = catalogueIdentity(1u, images[index].key.name);
				break;
			}
			if (state.resourceSelection == 0u) for (const auto& icon : icons) {
				const bool stableMatch = texture.sourceDomain == ResourceDomain::Icon &&
					texture.sourceKey == icon.iconName;
				if (!stableMatch && (icon.atlasTexture != texture.handle ||
					std::abs(icon.atlasUv.x - texture.uv0x) > 0.00001f ||
					std::abs(icon.atlasUv.y - texture.uv0y) > 0.00001f)) continue;
				state.resourceSelection = catalogueIdentity(2u, icon.iconName);
				break;
			}
			if (state.resourceSelection == 0u) {
				state.resourceSelection = texture.handle ? texture.handle.packed() : UINT64_MAX;
				if (std::ranges::find(state.choiceIdentities, state.resourceSelection) ==
					state.choiceIdentities.end()) {
				state.choiceLabels.insert(state.choiceLabels.begin(), texture.handle
					? "Anonymous texture · " + std::to_string(texture.handle.packed())
					: "No texture");
				state.choiceKeys.insert(state.choiceKeys.begin(), std::string{});
				state.choiceResourceDomains.insert(state.choiceResourceDomains.begin(), 0u);
				state.choiceIdentities.insert(state.choiceIdentities.begin(), state.resourceSelection);
				state.choiceOptions.clear();
				state.choiceOptions.reserve(state.choiceIdentities.size());
				for (std::size_t index = 0; index < state.choiceIdentities.size(); ++index) {
					state.choiceOptions.push_back(FSEL::ComboBoxOption{
						.value = state.choiceIdentities[index], .text = state.choiceLabels[index],
						.enabled = index != 0u});
				}
				}
			}
			if (!state.catalogLease.isValid() && state.app) {
				state.catalogLease = state.app->devTooling().catalogues().acquireInspectionLease();
			}
			ActionCall sourceChanged{};
			if (state.app && state.interfaceState && state.editable) sourceChanged = ActionCall{
				state.app->actions().uiActions().make(kChooseTextureResource, state)};
			context.uiManager.createElement(kDevCatalogueChoice, "resource-source")
					.setParameters(DevCatalogueChoiceParameters{
						.options = state.choiceOptions, .selectedValue = &state.resourceSelection,
						.onChanged = sourceChanged, .placeholder = "Search images and icons...",
						.enabled = state.editable})
					.setDevInternalCapture(true).draw();
			state.resourceFitSelection = static_cast<std::uint64_t>(texture.fitMode);
				state.resourceSamplingSelection = static_cast<std::uint64_t>(texture.samplingMode);
				state.resourceTintEnabled = texture.tintEnabled;
				ActionCall fitChanged{}, samplingChanged{}, tintChanged{};
				if (state.app && state.interfaceState && state.editable) {
					auto& actions = state.app->actions().uiActions();
					fitChanged = ActionCall{actions.make(kChooseTextureFit, state)};
					samplingChanged = ActionCall{actions.make(kChooseTextureSampling, state)};
					tintChanged = ActionCall{actions.make(kToggleTextureTint, state)};
				}
				context.uiManager.createElement(kDevCatalogueChoice, "fit-mode")
					.setParameters(DevCatalogueChoiceParameters{.options = kTextureFitOptions,
						.selectedValue = &state.resourceFitSelection, .onChanged = fitChanged,
						.placeholder = "Fit mode", .enabled = state.editable, .searchable = false})
					.setDevInternalCapture(true).draw();
				context.uiManager.createElement(kDevCatalogueChoice, "sampling-mode")
					.setParameters(DevCatalogueChoiceParameters{.options = kTextureSamplingOptions,
						.selectedValue = &state.resourceSamplingSelection, .onChanged = samplingChanged,
						.placeholder = "Sampling mode", .enabled = state.editable, .searchable = false})
					.setDevInternalCapture(true).draw();
				Clay_ElementDeclaration tintRow{};
				tintRow.layout.sizing = {
					.width = CLAY_SIZING_GROW(0), .height = CLAY_SIZING_FIXED(26)};
				tintRow.layout.layoutDirection = CLAY_LEFT_TO_RIGHT;
				tintRow.layout.childGap = 7;
				tintRow.layout.childAlignment = {
					.x = CLAY_ALIGN_X_LEFT, .y = CLAY_ALIGN_Y_CENTER};
				CLAY(context.clayID("texture-tint-row"), tintRow) {
					FSEL::SwitchParameters tint{};
					tint.isOn = state.resourceTintEnabled;
					tint.enabled = state.editable;
					tint.onToggle = tintChanged;
					context.uiManager.createElement(FSEL::kSwitch, "texture-tint")
						.setParameters(std::move(tint)).setDevInternalCapture(true).draw();
					const Clay_TextElementConfig label =
						textConfig(interface_theme::kTextSecondary, 9);
					CLAY_TEXT(context.uiManager.toClayString("Tint enabled"), CLAY_TEXT_CONFIG(label));
				}
				if (!state.localDiagnostic.empty()) {
					drawValueSurface(state.localDiagnostic, interface_theme::kStatusAmber);
				}
			return;
		}
		if (kind == devMode::DevEditorKind::SizingAxis) {
			const auto& axis = *static_cast<const Clay_SizingAxis*>(state.editValue);
			state.enumSelection = static_cast<uint64_t>(axis.type);
			if (state.semanticMode != state.enumSelection) {
				state.semanticMode = state.enumSelection;
				if (axis.type == CLAY__SIZING_TYPE_PERCENT) {
					state.semanticSliderValue =
						static_cast<double>(axis.size.percent) * 100.0;
					state.semanticValueB = 0.0f;
				} else {
					state.semanticValueA = axis.size.minMax.min;
					state.semanticValueB = axis.size.minMax.max;
				}
			}
			ActionCall changed{};
			ActionCall valuesPreview{};
			ActionCall valuesChanged{};
			ActionCall valuesCancelled{};
			if (state.app && state.interfaceState && state.editable) {
				changed = ActionCall{state.app->actions().uiActions().make(kChooseSizingAxisMode, state)};
				valuesPreview = ActionCall{
					state.app->actions().uiActions().make(kPreviewSizingAxisValues, state)};
				valuesChanged = ActionCall{
					state.app->actions().uiActions().make(kCommitSizingAxisValues, state)};
				valuesCancelled = ActionCall{
					state.app->actions().uiActions().make(kCancelSizingAxisValues, state)};
			}
			FSEL::ComboBoxParameters parameters{};
			parameters.options = kSizingAxisOptions;
			parameters.selectedValue = &state.enumSelection;
			parameters.enabled = state.editable;
			parameters.onChanged = changed;
			parameters.fontSize = kTypeControlFontSize;
			parameters.optionHeight = kTypeControlHeight;
			parameters.sizing = Clay_Sizing{
				.width = CLAY_SIZING_GROW(0), .height = CLAY_SIZING_GROW(0)};
			Clay_ElementDeclaration modeSlot{};
			modeSlot.layout.sizing = {
				.width = CLAY_SIZING_GROW(0), .height = CLAY_SIZING_FIXED(kTypeControlHeight)};
			modeSlot.clip = {.horizontal = true, .vertical = false};
			CLAY(context.clayID("sizing-mode-slot"), modeSlot) {
				context.uiManager.createElement(FSEL::kComboBox, "sizing-mode")
					.setParameters(std::move(parameters)).setDevInternalCapture(true).draw();
			}

			auto drawSizingNumber = [&](uint64_t key, std::string_view label,
				float* value, std::optional<float> maximum = std::nullopt) {
				Clay_ElementDeclaration line{};
				line.layout.sizing = {
					.width = CLAY_SIZING_GROW(0), .height = CLAY_SIZING_FIT(0)};
				line.layout.layoutDirection = CLAY_LEFT_TO_RIGHT;
				line.layout.childGap = 6;
				line.layout.childAlignment = {
					.x = CLAY_ALIGN_X_LEFT, .y = CLAY_ALIGN_Y_CENTER};
				CLAY(context.clayID(Keyed(kEditorControl, key * 4u)), line) {
					Clay_TextElementConfig labelStyle = textConfig(
						interface_theme::kTextSecondary, 9);
					CLAY(context.clayID(Keyed(kEditorControl, key * 4u + 1u)), Clay_ElementDeclaration{
						.layout = {.sizing = {.width = CLAY_SIZING_FIXED(34),
							.height = CLAY_SIZING_FIT(0)}}}) {
						CLAY_TEXT(context.uiManager.toClayString(label),
							CLAY_TEXT_CONFIG(labelStyle));
					}
					Clay_ElementDeclaration controlSlot{};
					controlSlot.layout.sizing = {
						.width = CLAY_SIZING_GROW(0), .height = CLAY_SIZING_FIXED(kTypeControlHeight)};
					controlSlot.clip = {.horizontal = true, .vertical = false};
					CLAY(context.clayID(Keyed(kEditorControl, key * 4u + 2u)), controlSlot) {
						FSEL::DragValueParameters<float> drag{};
						drag.value = value;
						drag.minimum = 0.0f;
						drag.maximum = maximum;
						drag.step = 0.5f;
						drag.pixelsPerStep = 6.0f;
						drag.dragThreshold = 4.0f;
						drag.enabled = state.editable;
						drag.readOnly = !state.editable;
						drag.edit.onChanged = valuesPreview;
						drag.edit.onCommit = valuesChanged;
						drag.edit.onCancel = valuesCancelled;
						drag.sizing = Clay_Sizing{
							.width = CLAY_SIZING_GROW(0), .height = CLAY_SIZING_GROW(0)};
						drag.fontSize = kTypeControlFontSize;
						drag.padding = kTypeControlPadding;
						drag.format.precision = 6;
						drag.format.allowScientificInput = true;
						context.uiManager.createElement(
							FSEL::kDragValueFloat, Keyed(kEditorControl, key * 4u + 3u))
							.setParameters(std::move(drag)).setDevInternalCapture(true).draw();
					}
				}
			};

			if (axis.type == CLAY__SIZING_TYPE_PERCENT) {
				Clay_ElementDeclaration line{};
				line.layout.sizing = {
					.width = CLAY_SIZING_GROW(0), .height = CLAY_SIZING_FIT(0)};
				line.layout.layoutDirection = CLAY_LEFT_TO_RIGHT;
				line.layout.childGap = 8;
				line.layout.childAlignment = {
					.x = CLAY_ALIGN_X_LEFT, .y = CLAY_ALIGN_Y_CENTER};
				line.clip = {.horizontal = true, .vertical = false};
				CLAY(context.clayID("percent-slider-line"), line) {
					Clay_ElementDeclaration sliderSlot{};
					sliderSlot.layout.sizing = {
						.width = CLAY_SIZING_GROW(0),
						.height = CLAY_SIZING_FIXED(kTypeControlHeight)};
					sliderSlot.layout.childAlignment = {
						.x = CLAY_ALIGN_X_LEFT, .y = CLAY_ALIGN_Y_CENTER};
					sliderSlot.clip = {.horizontal = true, .vertical = false};
					CLAY(context.clayID("percent-slider-slot"), sliderSlot) {
						FSEL::SliderParameters slider{};
						slider.value = &state.semanticSliderValue;
						slider.minimum = 0.0;
						slider.maximum = 100.0;
						slider.roundingStep = 1.0;
						slider.enabled = state.editable;
						slider.onChanged = valuesPreview;
						slider.onCommit = valuesChanged;
						slider.length = 150.0f;
						context.uiManager.createElement(FSEL::kSlider, "percent-slider")
							.setParameters(std::move(slider)).setDevInternalCapture(true).draw();
					}
					char percentLabel[16]{};
					std::snprintf(percentLabel, sizeof(percentLabel), "%.0f%%",
						state.semanticSliderValue);
					const Clay_TextElementConfig percentStyle = textConfig(
						interface_theme::kTextCanvas, 9, CLAY_TEXT_ALIGN_RIGHT);
					CLAY_TEXT(context.uiManager.toClayString(percentLabel),
						CLAY_TEXT_CONFIG(percentStyle));
				}
			} else if (axis.type == CLAY__SIZING_TYPE_FIXED) {
				drawSizingNumber(2u, "px", &state.semanticValueA);
			} else {
				drawSizingNumber(3u, "Min", &state.semanticValueA);
				drawSizingNumber(4u, "Max", &state.semanticValueB);
			}
			return;
		}


		if (type && (type->kind == devMode::DevTypeKind::SignedInteger ||
			type->kind == devMode::DevTypeKind::UnsignedInteger ||
			type->kind == devMode::DevTypeKind::FloatingPoint)) {
			ActionCall commit{};
			ActionCall preview{};
			if (state.app && state.interfaceState) {
				commit = ActionCall{state.app->actions().uiActions().make(kCommitEditorDraft, state)};
				preview = ActionCall{state.app->actions().uiActions().make(kPreviewEditorDraft, state)};
			}
			FSEL::TextInputParameters parameters{};
			parameters.value = &state.draft;
			parameters.syncPolicy = FSEL::TextFieldSyncPolicy::Live;
			parameters.actions.onChanged = preview;
			parameters.actions.onCommit = commit;
			parameters.enabled = state.editable;
			parameters.readOnly = !state.editable;
			parameters.placeholder = state.editable ? std::string_view{} : "Read only";
			parameters.valid = state.draftValid;
			parameters.maxBytes = field->constraint < schema->constraints.size() &&
				schema->constraints[field->constraint].hasTextMaximum
					? schema->constraints[field->constraint].textMaximum : 128u;
			parameters.sizing = Clay_Sizing{
				.width = CLAY_SIZING_GROW(0), .height = CLAY_SIZING_GROW(0)};
			parameters.fontSize = kTypeControlFontSize;
			parameters.padding = kTypeControlPadding;

			Clay_ElementDeclaration controlSlot{};
			controlSlot.layout.sizing = {
				.width = CLAY_SIZING_GROW(0), .height = CLAY_SIZING_FIXED(kTypeControlHeight)};
			controlSlot.clip = {.horizontal = true, .vertical = false};
			CLAY(context.clayID("text-control-slot"), controlSlot) {
				context.uiManager.createElement(FSEL::kTextInput, kEditorControl)
					.setParameters(std::move(parameters)).setDevInternalCapture(true).draw();
			}
			if (!state.draftValid) drawValueSurface("Invalid value", interface_theme::kStatusRed);
			return;
		}

		const bool semanticObject = type && type->kind == devMode::DevTypeKind::Object &&
			kind != devMode::DevEditorKind::ObjectGroup;
		if (semanticObject) {
			for (const devMode::DevFieldSchema& child : schema->fieldsOf(state.operationType)) {
				const void* childValue = nullptr;
				if (child.operations < schema->fieldOperations.size()) {
					const devMode::DevFieldOps* childOps = schema->fieldOperations[child.operations];
					if (childOps && childOps->constAddress) childValue = childOps->constAddress(state.editValue);
				}
				DevEditorBinding childBinding = state.binding;
				childBinding.nestedPath.push_back(field->id);
				childBinding.field = fieldIndex(*schema, child);
				context.uiManager.createElement(kDevEditor, Keyed(kSemanticChild, child.id))
					.setParameters(DevEditorParameters{.schema = context.params.schema,
						.binding = std::move(childBinding), .app = state.app,
						.interfaceState = state.interfaceState, .value = childValue,
						.showFieldName = true})
					.setDevInternalCapture(true).draw();
			}
			return;
		}

		if (type && type->kind == devMode::DevTypeKind::Sequence) {
			if (operations) drawSequenceRows(context, state, *schema, *type, *operations);
			return;
		}

		std::string reason = state.editable ? "Unsupported: no editor adapter" : "View only";
		if (field->reason == devMode::DevCapabilityReason::RawPointer) reason = "Unsupported: raw pointer";
		else if (field->reason == devMode::DevCapabilityReason::CallableType) reason = "Unsupported: callable type";
		else if (field->reason == devMode::DevCapabilityReason::NoEditAdapter) reason = "View only: no edit adapter";
		drawValueSurface(reason, interface_theme::kTextMuted);
		}();
	}
}

template <typename Context>
void drawChangeActionButton(
	Context& context,
	uint64_t key,
	std::string_view label,
	ActionCall action,
	bool enabled,
	float width) {
	FSEL::ButtonParameters button{};
	button.onActivate = action;
	button.enabled = enabled;
	button.contentMode = FSEL::ButtonContentMode::TextOnly;
	button.text = label;
	button.sizing = {
		.width = CLAY_SIZING_FIXED(width),
		.height = CLAY_SIZING_FIXED(24),
	};
	button.padding = Clay_Padding{4, 4, 0, 0};
	button.borderWidth = Clay_BorderWidth{1, 1, 1, 1, 0};
	button.cornerRadius = CLAY_CORNER_RADIUS(3);
	button.labelFontSize = 8;
	button.idleOverrides.backgroundColor = interface_theme::kDepth3Elevated;
	button.idleOverrides.labelColor = interface_theme::kTextSecondary;
	button.idleOverrides.borderColor = interface_theme::kBorderVisible;
	button.hoveredOverrides.backgroundColor = interface_theme::kHoverSurface;
	button.hoveredOverrides.labelColor = interface_theme::kTextCanvas;
	button.hoveredOverrides.borderColor = interface_theme::kAccentCurrent;
	button.pressedOverrides.backgroundColor = interface_theme::kSelectedRow;
	button.pressedOverrides.labelColor = interface_theme::kTextCanvas;
	button.pressedOverrides.borderColor = interface_theme::kAccentSeaGlass;
	button.disabledOverrides.backgroundColor = interface_theme::kDepth2Ink;
	button.disabledOverrides.labelColor = interface_theme::kTextMuted;
	button.disabledOverrides.borderColor = interface_theme::kBorderPrimary;
	context.uiManager.createElement(FSEL::kButton, Keyed(kChangeAction, key))
		.setParameters(std::move(button)).setDevInternalCapture(true).draw();
}

void DevChangesHeader::buildElement(BuildContext& context) {
	DevChangesHeaderState& state = context.state();
	state.app = context.params.app;
	state.interfaceState = context.params.interfaceState;
	state.definition = context.params.definition;
	state.window = context.params.window;
	state.instance = context.params.instance;
	state.elementName = context.params.elementName;

	bool canElevate = false;
	bool canRevert = false;
	if (state.app) {
		for (const tooling::DevOverrideApply::Record& record :
			state.app->devTooling().overrides().appliedOverrides().records()) {
			if (!recordTargetsSelection(
				record, state.definition, state.window, state.instance)) continue;
			canRevert |= record.layer == tooling::DevOverrideLayer::LiveDefinition ||
				record.layer == tooling::DevOverrideLayer::LiveInstance;
			canElevate |= record.layer == tooling::DevOverrideLayer::LiveInstance;
		}
	}
	ActionCall elevate{}, revert{};
	if (state.app && state.interfaceState) {
		auto& actions = state.app->actions().uiActions();
		elevate = ActionCall{actions.make(kElevateAllChanges, state)};
		revert = ActionCall{actions.make(kRevertAllChanges, state)};
	}

	Clay_ElementDeclaration header{};
	header.layout.sizing = {
		.width = CLAY_SIZING_GROW(0), .height = CLAY_SIZING_FIT(0)};
	header.layout.padding = Clay_Padding{7, 7, 7, 7};
	header.layout.layoutDirection = CLAY_TOP_TO_BOTTOM;
	header.layout.childGap = 6;
	header.backgroundColor = interface_theme::kDepth2Ink;
	header.cornerRadius = CLAY_CORNER_RADIUS(3);
	header.border = {.color = interface_theme::kBorderVisible,
		.width = Clay_BorderWidth{1, 1, 1, 1, 0}};
	header.clip = {.horizontal = true, .vertical = true};
	CLAY(context.clayID(), header) {
		Clay_ElementDeclaration titleRow{};
		titleRow.layout.sizing = {
			.width = CLAY_SIZING_GROW(0), .height = CLAY_SIZING_FIT(0)};
		titleRow.layout.layoutDirection = CLAY_LEFT_TO_RIGHT;
		titleRow.layout.childGap = 5;
		titleRow.layout.childAlignment = {
			.x = CLAY_ALIGN_X_LEFT, .y = CLAY_ALIGN_Y_CENTER};
		CLAY(context.clayID("legend-title-row"), titleRow) {
			CLAY_TEXT(context.uiManager.toClayString("Legend:"),
				CLAY_TEXT_CONFIG(textConfig(interface_theme::kTextCanvas, 10)));
			CLAY(context.clayID("legend-title-spacer"), {
				.layout = {.sizing = {.width = CLAY_SIZING_GROW(0),
					.height = CLAY_SIZING_FIT(0)}}}) {}
			Clay_ElementDeclaration actions{};
			actions.layout.sizing = {
				.width = CLAY_SIZING_FIT(0), .height = CLAY_SIZING_FIT(0)};
			actions.layout.layoutDirection = CLAY_LEFT_TO_RIGHT;
			actions.layout.childGap = 5;
			actions.layout.childAlignment = {
				.x = CLAY_ALIGN_X_RIGHT, .y = CLAY_ALIGN_Y_CENTER};
			CLAY(context.clayID("actions"), actions) {
				drawChangeActionButton(context, 1u, "Elevate All", elevate, canElevate, 58);
				drawChangeActionButton(context, 2u, "Revert All", revert, canRevert, 54);
			}
		}
		Clay_ElementDeclaration chain{};
		chain.layout.sizing = {
			.width = CLAY_SIZING_GROW(0), .height = CLAY_SIZING_FIT(0)};
		chain.layout.layoutDirection = CLAY_LEFT_TO_RIGHT;
		chain.layout.childGap = 2;
		chain.layout.childAlignment = {.x = CLAY_ALIGN_X_LEFT, .y = CLAY_ALIGN_Y_CENTER};
		chain.clip = {.horizontal = true, .vertical = true};
		CLAY(context.clayID("precedence"), chain) {
			struct Legend { std::string_view text; Clay_Color color; };
			const std::array<Legend, 4> layers{{
				{"Authored Code", interface_theme::kTextMuted},
				{"Baked Layer", interface_theme::kAccentSeaGlass},
				{"Live Definition", interface_theme::kAccentSignalBlue},
				{"Live Instance", interface_theme::kAccentSignalCoral},
			}};
			for (std::size_t index = 0; index < layers.size(); ++index) {
				if (index != 0u) CLAY_TEXT(
					context.uiManager.toClayString(R"(>)"),
					CLAY_TEXT_CONFIG(textConfig(interface_theme::kTextMuted, 8)));
				CLAY_TEXT(context.uiManager.toClayString(layers[index].text),
					CLAY_TEXT_CONFIG(textConfig(layers[index].color, 8)));
			}
		}
	}
}

void DevChangeCard::buildElement(BuildContext& context) {
	DevChangeCardState& state = context.state();
	state.app = context.params.app;
	state.interfaceState = context.params.interfaceState;
	state.definition = context.params.definition;
	state.window = context.params.window;
	state.instance = context.params.instance;
	state.elementName = context.params.elementName;
	state.field = context.params.field;
	state.fieldIndex = context.params.fieldIndex;
	state.fieldName = context.params.fieldName;
	state.typeName = context.params.typeName;
	state.authoredValue = context.params.authoredValue;
	state.bakedValue = context.params.bakedValue;

	const tooling::DevOverrideApply::Record* liveDefinition = nullptr;
	const tooling::DevOverrideApply::Record* liveInstance = nullptr;
	if (state.app) {
		for (const tooling::DevOverrideApply::Record& record :
			state.app->devTooling().overrides().appliedOverrides().records()) {
			if (record.field != state.field || !recordTargetsSelection(
				record, state.definition, state.window, state.instance)) continue;
			if (record.layer == tooling::DevOverrideLayer::LiveDefinition) liveDefinition = &record;
			else if (record.layer == tooling::DevOverrideLayer::LiveInstance) liveInstance = &record;
		}
	}
	const tooling::DevOverrideApply::Record* winner = liveInstance ? liveInstance : liveDefinition;
	const bool canElevate = liveInstance != nullptr;
	const bool canRevert = winner != nullptr;
	ActionCall elevate{}, revert{};
	if (state.app && state.interfaceState) {
		auto& actions = state.app->actions().uiActions();
		elevate = ActionCall{actions.make(kElevateChange, state)};
		revert = ActionCall{actions.make(kRevertChange, state)};
	}

	auto liveSummary = [&](const tooling::DevOverrideApply::Record* record) {
		if (!record || !state.app || !record->value) return std::string{};
		const devMode::DevSchemaView schema = state.app->devTooling().schemas().view();
		if (!schema) return std::string{"Value unavailable"};
		const devMode::DevFieldSchema* field = fieldAt(*schema, record->fieldIndex);
		const devMode::DevTypeSchema* type = field && schema
			? schema->type(field->valueType) : nullptr;
		return quickView(*schema, type, record->value.data(), state.app, field).primary;
	};
	struct Step { std::string_view name; std::string value; Clay_Color color; };
	std::vector<Step> steps{};
	if (!state.authoredValue.empty()) steps.push_back({"Authored Code",
		state.authoredValue, interface_theme::kTextMuted});
	if (!state.bakedValue.empty()) steps.push_back({"Baked Layer",
		state.bakedValue, interface_theme::kAccentSeaGlass});
	if (liveDefinition) steps.push_back({"Live Definition",
		liveSummary(liveDefinition), interface_theme::kAccentSignalBlue});
	if (liveInstance) steps.push_back({"Live Instance",
		liveSummary(liveInstance), interface_theme::kAccentSignalCoral});
	const Clay_Color winningColor = steps.empty()
		? interface_theme::kBorderVisible : steps.back().color;

	Clay_ElementDeclaration card{};
	card.layout.sizing = {
		.width = CLAY_SIZING_GROW(0), .height = CLAY_SIZING_FIT(0)};
	card.layout.padding = Clay_Padding{8, 8, 7, 7};
	card.layout.layoutDirection = CLAY_TOP_TO_BOTTOM;
	card.layout.childGap = 7;
	card.backgroundColor = interface_theme::kDepth2Ink;
	card.cornerRadius = CLAY_CORNER_RADIUS(3);
	card.border = {.color = winningColor, .width = Clay_BorderWidth{1, 1, 1, 1, 0}};
	card.clip = {.horizontal = true, .vertical = true};
	CLAY(context.clayID(), card) {
		Clay_ElementDeclaration title{};
		title.layout.sizing = {
			.width = CLAY_SIZING_GROW(0), .height = CLAY_SIZING_FIT(0)};
		title.layout.layoutDirection = CLAY_TOP_TO_BOTTOM;
		title.layout.childGap = 1;
		title.clip = {.horizontal = true, .vertical = true};
		CLAY(context.clayID("title"), title) {
			CLAY_TEXT(context.uiManager.toClayString(state.fieldName),
				CLAY_TEXT_CONFIG(textConfig(interface_theme::kTextCanvas, 11)));
			CLAY_TEXT(context.uiManager.toClayString(state.typeName),
				CLAY_TEXT_CONFIG(textConfig(winningColor, 8)));
		}
		Clay_ElementDeclaration chain{};
		chain.layout.sizing = {
			.width = CLAY_SIZING_GROW(0), .height = CLAY_SIZING_FIT(0)};
		chain.layout.layoutDirection = CLAY_TOP_TO_BOTTOM;
		chain.layout.childGap = 2;
		CLAY(context.clayID("chain"), chain) {
			for (std::size_t index = 0; index < steps.size(); ++index) {
				Clay_Color color = steps[index].color;
				if (index + 1u != steps.size()) color.a *= 0.42f;
				std::string line;
				if (index != 0u) line = R"(> )";
				line += steps[index].name;
				line += "  ";
				line += steps[index].value;
				Clay_TextElementConfig valueStyle = textConfig(color, 9);
				valueStyle.wrapMode = CLAY_TEXT_WRAP_WORDS;
				CLAY(context.clayID(Keyed(kChangeStep, index)), {
					.layout = {.sizing = {.width = CLAY_SIZING_GROW(0),
						.height = CLAY_SIZING_FIT(0)}}}) {
					CLAY_TEXT(context.uiManager.toClayString(line),
						CLAY_TEXT_CONFIG(valueStyle));
				}
			}
		}
		Clay_ElementDeclaration actions{};
		actions.layout.sizing = {
			.width = CLAY_SIZING_GROW(0), .height = CLAY_SIZING_FIT(0)};
		actions.layout.layoutDirection = CLAY_LEFT_TO_RIGHT;
		actions.layout.childGap = 5;
		actions.layout.childAlignment = {.x = CLAY_ALIGN_X_RIGHT, .y = CLAY_ALIGN_Y_CENTER};
		CLAY(context.clayID("actions"), actions) {
			drawChangeActionButton(context, 1u, "Elevate", elevate, canElevate, 58);
			drawChangeActionButton(context, 2u, "Revert", revert, canRevert, 56);
		}
	}
}

void DevInterfaceInspectorFields::buildElement(BuildContext& context) {
	App* app = context.params.app;
	DevInterfaceState* state = context.params.interfaceState;
	if (state &&
		state->inspectInspectorTab > static_cast<uint64_t>(DevInspectorTab::Changes)) {
		state->inspectInspectorTab = static_cast<uint64_t>(DevInspectorTab::Parameters);
	}
	const DevInspectorTab tab = state
		? static_cast<DevInspectorTab>(state->inspectInspectorTab)
		: DevInspectorTab::Parameters;
	const devMode::DevSchemaView schema = app
		? app->devTooling().schemas().view() : devMode::DevSchemaView{};
	const devMode::DevElementSchema* element = schema
		? schema->findElement(context.params.definition) : nullptr;

	devMode::DevTypeIndex rootType{};
	std::string_view emptyMessage{};
	if (element) {
		switch (tab) {
		case DevInspectorTab::Parameters:
			rootType = element->parametersType;
			emptyMessage = "This element has no parameter fields";
			break;
		case DevInspectorTab::State:
			rootType = element->stateType;
			emptyMessage = "This element does not have state";
			break;
		case DevInspectorTab::Resources:
			rootType = element->resourcesType;
			emptyMessage = "This element does not have resources";
			break;
		case DevInspectorTab::Changes:
			break;
		}
	}
	const std::span<const devMode::DevFieldSchema> fields = schema && rootType
		? schema->fieldsOf(rootType) : std::span<const devMode::DevFieldSchema>{};
	const devMode::DevTypeSchema* rootSchema = schema
		? schema->type(rootType) : nullptr;
	const devMode::DevTypeId rootOwnerType = rootSchema ? rootSchema->id : 0u;
	const tooling::DevElementCaptureSnapshot* capture = app && state
		? &app->devTooling().overrides().elementSnapshot(state->selectedWindowId)
		: nullptr;
	const tooling::DevCapturedElement* capturedElement = nullptr;
	if (capture) {
		const auto found = std::ranges::find_if(
			capture->elements,
			[&context](const tooling::DevCapturedElement& captured) {
				return captured.definition == context.params.definition &&
					captured.instance == context.params.instance;
			});
		if (found != capture->elements.end()) capturedElement = &*found;
	}
	const auto capturedValue = [&](const devMode::DevFieldSchema& field) -> const void* {
		if (!schema || !capture || !capturedElement) return nullptr;
		const devMode::DevFieldIndex wanted = fieldIndex(*schema, field);
		const std::size_t begin = capturedElement->firstField;
		const std::size_t end = std::min<std::size_t>(
			capture->fields.size(), begin + capturedElement->fieldCount);
		for (std::size_t index = begin; index < end; ++index) {
			if (capture->fields[index].field == wanted) {
				return capture->fields[index].value.data();
			}
		}
		return nullptr;
	};
	const std::vector<tooling::DevBakeDiffEntry> changes =
		app && tab == DevInspectorTab::Changes
			? app->devTooling().queryBakeDiff()
			: std::vector<tooling::DevBakeDiffEntry>{};
	struct ChangeModel {
		tooling::DevOverrideFieldKey field{};
		devMode::DevFieldIndex fieldIndex{};
		std::string fieldName{};
		std::string typeName{};
		std::string authoredValue{};
		std::string bakedValue{};
	};
	std::vector<ChangeModel> changeModels{};
	if (app && state && schema && tab == DevInspectorTab::Changes) {
		for (const tooling::DevOverrideApply::Record& record :
			app->devTooling().overrides().appliedOverrides().records()) {
			if ((record.layer != tooling::DevOverrideLayer::LiveDefinition &&
				record.layer != tooling::DevOverrideLayer::LiveInstance) ||
				!recordTargetsSelection(record, context.params.definition,
					state->selectedWindowId, context.params.instance)) continue;
			if (std::ranges::any_of(changeModels,
				[&](const ChangeModel& model) { return model.field == record.field; })) continue;
			const devMode::DevFieldSchema* field = fieldAt(*schema, record.fieldIndex);
			const devMode::DevTypeSchema* type = field ? schema->type(field->valueType) : nullptr;
			std::string name = field ? std::string(schema->string(field->displayName)) : std::string{};
			if (name.empty() && field) name = std::string(schema->string(field->name));
			if (name.empty()) name = "Changed field";
			changeModels.push_back(ChangeModel{
				.field = record.field,
				.fieldIndex = record.fieldIndex,
				.fieldName = std::move(name),
				.typeName = normalizedTypeName(*schema, type),
			});
		}
		for (const tooling::DevBakeDiffEntry& change : changes) {
			if (change.targetKind != tooling::DevBakeTargetKind::Element ||
				change.definition != context.params.definition ||
				(change.instance && change.instance != context.params.instance)) continue;
			auto model = std::ranges::find_if(changeModels,
				[&](const ChangeModel& candidate) {
					return candidate.field.field == change.fieldId;
				});
			if (model == changeModels.end()) {
				const auto found = std::ranges::find_if(schema->fields,
					[&](const devMode::DevFieldSchema& field) { return field.id == change.fieldId; });
				if (found == schema->fields.end()) continue;
				const devMode::DevTypeSchema* type = schema->type(found->valueType);
				std::string name = change.fieldPath;
				if (name.empty()) name = std::string(schema->string(found->displayName));
				if (name.empty()) name = std::string(schema->string(found->name));
				changeModels.push_back(ChangeModel{
					.field = tooling::DevOverrideFieldKey{
						.ownerType = rootOwnerType, .field = change.fieldId},
					.fieldIndex = fieldIndex(*schema, *found),
					.fieldName = std::move(name),
					.typeName = normalizedTypeName(*schema, type),
				});
				model = std::prev(changeModels.end());
			}
			if (!change.fieldPath.empty()) model->fieldName = change.fieldPath;
			if (!change.authoredValueString.empty()) {
				model->authoredValue = change.authoredValueString;
			}
			if (!change.bakedValueString.empty()) model->bakedValue = change.bakedValueString;
		}
	}

	Clay_ElementDeclaration surface{};
	surface.layout.sizing = {
		.width = CLAY_SIZING_GROW(0),
		.height = CLAY_SIZING_GROW(0),
	};
	surface.backgroundColor = interface_theme::kDepth1Panel;
	const Clay_ScrollContainerData scroll = Clay_GetScrollContainerData(context.clayID());
	surface.clip = {
		.horizontal = true,
		.vertical = true,
		.childOffset = scroll.found && scroll.scrollPosition
			? *scroll.scrollPosition : Clay_Vector2{},
	};

	CLAY(context.clayID(), surface) {
		if (!app || !state) {
			drawMessage(context, "Inspector data is unavailable");
		} else if (!schema || !element) {
			drawMessage(context, "No schema exists for this element");
		} else {
			Clay_ElementDeclaration cards{};
			cards.layout.sizing = {
				.width = CLAY_SIZING_GROW(0),
				.height = CLAY_SIZING_FIT(0),
			};
			cards.layout.padding = Clay_Padding{9, 9, 9, 9};
			cards.layout.layoutDirection = CLAY_TOP_TO_BOTTOM;
			cards.layout.childGap = 7;
			CLAY(context.clayID(kCards), cards) {
				if (tab == DevInspectorTab::Changes) {
					context.uiManager.createElement(kDevChangesHeader, kChangeHeader)
						.setParameters(DevChangesHeaderParameters{
							.app = app, .interfaceState = state,
							.definition = context.params.definition,
							.window = state->selectedWindowId,
							.instance = context.params.instance,
							.elementName = context.params.elementName,
						}).setDevInternalCapture(true).draw();
					if (changeModels.empty()) {
						drawMessage(context, "This element has no changes");
					} else {
						uint64_t changeIndex = 1u;
						for (const ChangeModel& change : changeModels) {
							context.uiManager.createElement(
								kDevChangeCard, Keyed(kEditorCard, changeIndex++))
								.setParameters(DevChangeCardParameters{
									.app = app, .interfaceState = state,
									.definition = context.params.definition,
									.window = state->selectedWindowId,
									.instance = context.params.instance,
									.elementName = context.params.elementName,
									.field = change.field,
									.fieldIndex = change.fieldIndex,
									.fieldName = change.fieldName,
									.typeName = change.typeName,
									.authoredValue = change.authoredValue,
									.bakedValue = change.bakedValue,
								}).setDevInternalCapture(true).draw();
						}
					}
				} else if (!rootType || fields.empty()) {
					drawMessage(context, emptyMessage);
				} else {
					const DevEditorRole role = tab == DevInspectorTab::Parameters
						? DevEditorRole::Parameters
						: tab == DevInspectorTab::State
							? DevEditorRole::State : DevEditorRole::Resources;
					for (const devMode::DevFieldSchema& field : fields) {
						drawCard(
							context, schema, field, role, rootOwnerType,
							tab == DevInspectorTab::Parameters
								? capturedValue(field) : nullptr);
					}
				}
			}
		}
	}
}

} // namespace FlowUi::devSystems::interface_elements

#endif
