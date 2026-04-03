#include "devMode/debugView.hpp"

#if FLOW_UI_DEV_MODE

#include <algorithm>
#include <cerrno>
#include <charconv>
#include <cmath>
#include <cstdint>
#include <cstdlib>
#include <limits>
#include <optional>
#include <string>
#include <string_view>

#include "FlowUi/Flow.hpp"
#include "devMode/devRuntime.hpp"
#include "devMode/registry.hpp"

namespace FlowUi::devMode {
namespace {

constexpr Clay_Color kColorBg = Clay_Color{16.0f, 20.0f, 27.0f, 245.0f};
constexpr Clay_Color kColorPanel = Clay_Color{24.0f, 29.0f, 39.0f, 255.0f};
constexpr Clay_Color kColorPanelSoft = Clay_Color{29.0f, 36.0f, 48.0f, 255.0f};
constexpr Clay_Color kColorPanelSelected = Clay_Color{39.0f, 51.0f, 72.0f, 255.0f};
constexpr Clay_Color kColorAccent = Clay_Color{94.0f, 168.0f, 255.0f, 255.0f};
constexpr Clay_Color kColorText = Clay_Color{232.0f, 238.0f, 247.0f, 255.0f};
constexpr Clay_Color kColorTextMuted = Clay_Color{145.0f, 158.0f, 178.0f, 255.0f};
constexpr Clay_Color kColorDanger = Clay_Color{214.0f, 92.0f, 92.0f, 255.0f};
constexpr Clay_Color kColorInput = Clay_Color{20.0f, 24.0f, 32.0f, 255.0f};
constexpr Clay_Color kColorSuccess = Clay_Color{88.0f, 200.0f, 138.0f, 255.0f};

enum class EditorKind : uint8_t {
	Unsupported = 0,
	Bool = 1,
	SignedInt = 2,
	UnsignedInt = 3,
	Floating = 4,
	String = 5,
};

using DebugViewDefinition = ElementDefinition<
	DebugViewParams,
	DebugViewState,
	DebugViewResources,
	FLOW_DEF_ID("flowui/dev/debug-view")>;

std::string makeDefinitionButtonId(std::string_view rootId, uint64_t definitionId) {
	return std::string(rootId) + "/def/" + std::to_string(definitionId);
}

std::string makeFieldPath(std::string_view rootId, uint64_t definitionId, uint64_t fieldHash) {
	return std::string(rootId) + "/field/" + std::to_string(definitionId) + "/" + std::to_string(fieldHash);
}

std::string makeFieldResetId(std::string_view rootId, uint64_t definitionId, uint64_t fieldHash) {
	return makeFieldPath(rootId, definitionId, fieldHash) + "/reset";
}

std::string makeFieldPlusId(std::string_view rootId, uint64_t definitionId, uint64_t fieldHash) {
	return makeFieldPath(rootId, definitionId, fieldHash) + "/plus";
}

std::string makeFieldMinusId(std::string_view rootId, uint64_t definitionId, uint64_t fieldHash) {
	return makeFieldPath(rootId, definitionId, fieldHash) + "/minus";
}

std::string makeFieldToggleId(std::string_view rootId, uint64_t definitionId, uint64_t fieldHash) {
	return makeFieldPath(rootId, definitionId, fieldHash) + "/toggle";
}

std::string makeFieldInputId(std::string_view rootId, uint64_t definitionId, uint64_t fieldHash) {
	return makeFieldPath(rootId, definitionId, fieldHash) + "/input";
}

bool isPressedNow(const InteractionSnapshot& interaction, UiManager& ui, std::string_view elementId) {
	return interaction.isPressed(ui.toClayEID(elementId));
}

std::string trimCopy(std::string_view text) {
	size_t begin = 0u;
	size_t end = text.size();
	while (begin < end && (text[begin] == ' ' || text[begin] == '\t' || text[begin] == '\n' || text[begin] == '\r')) {
		++begin;
	}
	while (end > begin && (text[end - 1u] == ' ' || text[end - 1u] == '\t' || text[end - 1u] == '\n' || text[end - 1u] == '\r')) {
		--end;
	}
	return std::string(text.substr(begin, end - begin));
}

bool parseBool(std::string_view text, bool& outValue) {
	const std::string trimmed = trimCopy(text);
	if (trimmed == "1" || trimmed == "true" || trimmed == "True" || trimmed == "TRUE" || trimmed == "on" || trimmed == "ON") {
		outValue = true;
		return true;
	}
	if (trimmed == "0" || trimmed == "false" || trimmed == "False" || trimmed == "FALSE" || trimmed == "off" || trimmed == "OFF") {
		outValue = false;
		return true;
	}
	return false;
}

bool parseInt64(std::string_view text, int64_t& outValue) {
	const std::string trimmed = trimCopy(text);
	if (trimmed.empty()) {
		return false;
	}
	int64_t parsed = 0;
	const auto result = std::from_chars(trimmed.data(), trimmed.data() + trimmed.size(), parsed);
	if (result.ec != std::errc() || result.ptr != (trimmed.data() + trimmed.size())) {
		return false;
	}
	outValue = parsed;
	return true;
}

bool parseUInt64(std::string_view text, uint64_t& outValue) {
	const std::string trimmed = trimCopy(text);
	if (trimmed.empty()) {
		return false;
	}
	uint64_t parsed = 0;
	const auto result = std::from_chars(trimmed.data(), trimmed.data() + trimmed.size(), parsed);
	if (result.ec != std::errc() || result.ptr != (trimmed.data() + trimmed.size())) {
		return false;
	}
	outValue = parsed;
	return true;
}

bool parseDouble(std::string_view text, double& outValue) {
	const std::string trimmed = trimCopy(text);
	if (trimmed.empty()) {
		return false;
	}

	char* parseEnd = nullptr;
	errno = 0;
	const double parsed = std::strtod(trimmed.c_str(), &parseEnd);
	if (parseEnd == trimmed.c_str() || parseEnd != (trimmed.c_str() + trimmed.size()) || errno == ERANGE || !std::isfinite(parsed)) {
		return false;
	}
	outValue = parsed;
	return true;
}

EditorKind classifyField(const FieldDescriptor& field) {
	const uint64_t type = field.fieldTypeHash;
	if (type == typeHash<bool>()) {
		return EditorKind::Bool;
	}
	if (type == typeHash<int8_t>() || type == typeHash<int16_t>() || type == typeHash<int32_t>() || type == typeHash<int64_t>()) {
		return EditorKind::SignedInt;
	}
	if (type == typeHash<uint8_t>() || type == typeHash<uint16_t>() || type == typeHash<uint32_t>() || type == typeHash<uint64_t>()) {
		return EditorKind::UnsignedInt;
	}
	if (type == typeHash<float>() || type == typeHash<double>()) {
		return EditorKind::Floating;
	}
	if (type == typeHash<std::string>()) {
		return EditorKind::String;
	}
	return EditorKind::Unsupported;
}

std::optional<bool> getOverrideAsBool(const DevRuntime& runtime, uint64_t definitionId, uint64_t fieldHash) {
	const DevValue* value = runtime.findDefinitionParamOverride(definitionId, fieldHash);
	if (!value) {
		return std::nullopt;
	}
	if (const auto* boolValue = std::get_if<bool>(value)) {
		return *boolValue;
	}
	if (const auto* intValue = std::get_if<int64_t>(value)) {
		return (*intValue != 0);
	}
	if (const auto* floatValue = std::get_if<double>(value)) {
		return (*floatValue != 0.0);
	}
	return std::nullopt;
}

std::optional<int64_t> getOverrideAsInt64(const DevRuntime& runtime, uint64_t definitionId, uint64_t fieldHash) {
	const DevValue* value = runtime.findDefinitionParamOverride(definitionId, fieldHash);
	if (!value) {
		return std::nullopt;
	}
	if (const auto* intValue = std::get_if<int64_t>(value)) {
		return *intValue;
	}
	if (const auto* boolValue = std::get_if<bool>(value)) {
		return *boolValue ? int64_t{1} : int64_t{0};
	}
	if (const auto* floatValue = std::get_if<double>(value)) {
		if (!std::isfinite(*floatValue)) {
			return std::nullopt;
		}
		return static_cast<int64_t>(*floatValue);
	}
	return std::nullopt;
}

std::optional<double> getOverrideAsDouble(const DevRuntime& runtime, uint64_t definitionId, uint64_t fieldHash) {
	const DevValue* value = runtime.findDefinitionParamOverride(definitionId, fieldHash);
	if (!value) {
		return std::nullopt;
	}
	if (const auto* floatValue = std::get_if<double>(value)) {
		return *floatValue;
	}
	if (const auto* intValue = std::get_if<int64_t>(value)) {
		return static_cast<double>(*intValue);
	}
	if (const auto* boolValue = std::get_if<bool>(value)) {
		return *boolValue ? 1.0 : 0.0;
	}
	return std::nullopt;
}

std::optional<std::string> getOverrideAsString(const DevRuntime& runtime, uint64_t definitionId, uint64_t fieldHash) {
	const DevValue* value = runtime.findDefinitionParamOverride(definitionId, fieldHash);
	if (!value) {
		return std::nullopt;
	}
	if (const auto* stringValue = std::get_if<std::string>(value)) {
		return *stringValue;
	}
	return std::nullopt;
}

std::string defaultTextForKind(EditorKind kind, const DevRuntime& runtime, uint64_t definitionId, uint64_t fieldHash) {
	switch (kind) {
	case EditorKind::Bool: {
		const std::optional<bool> value = getOverrideAsBool(runtime, definitionId, fieldHash);
		return value.has_value() ? (*value ? "true" : "false") : "false";
	}
	case EditorKind::SignedInt: {
		const std::optional<int64_t> value = getOverrideAsInt64(runtime, definitionId, fieldHash);
		return value.has_value() ? std::to_string(*value) : "0";
	}
	case EditorKind::UnsignedInt: {
		const std::optional<int64_t> value = getOverrideAsInt64(runtime, definitionId, fieldHash);
		const int64_t unsignedLike = value.has_value() ? std::max<int64_t>(0, *value) : 0;
		return std::to_string(unsignedLike);
	}
	case EditorKind::Floating: {
		const std::optional<double> value = getOverrideAsDouble(runtime, definitionId, fieldHash);
		if (!value.has_value()) {
			return "0.0";
		}
		return std::to_string(*value);
	}
	case EditorKind::String: {
		const std::optional<std::string> value = getOverrideAsString(runtime, definitionId, fieldHash);
		return value.value_or(std::string{});
	}
	default:
		return std::string{};
	}
}

bool ensureSelection(DebugViewState& state, const std::vector<ElementDescriptor>& elements) {
	if (elements.empty()) {
		state.selectedDefinitionId = 0u;
		return false;
	}

	if (state.selectedDefinitionId == 0u) {
		state.selectedDefinitionId = elements.front().definitionId;
		return true;
	}

	for (const ElementDescriptor& element : elements) {
		if (element.definitionId == state.selectedDefinitionId) {
			return true;
		}
	}

	state.selectedDefinitionId = elements.front().definitionId;
	return true;
}

void drawSectionHeader(UiManager& ui, std::string_view label) {
	CLAY({
		.layout = {
			.sizing = {CLAY_SIZING_GROW(0), CLAY_SIZING_FIXED(28)},
			.padding = CLAY_PADDING_ALL(4),
			.childAlignment = Clay_ChildAlignment{CLAY_ALIGN_X_LEFT, CLAY_ALIGN_Y_CENTER},
		},
	}) {
		CLAY_TEXT(
			ui.toClayString(label),
			CLAY_TEXT_CONFIG({
				.textColor = kColorTextMuted,
				.fontSize = 14,
				.letterSpacing = 0,
				.wrapMode = CLAY_TEXT_WRAP_NONE,
			}));
	}
}

void drawButton(UiManager& ui, std::string_view id, std::string_view text, Clay_Color background, Clay_Color textColor, float height = 32.0f) {
	CLAY({
		.id = ui.toClaySID(id),
		.layout = {
			.sizing = {CLAY_SIZING_GROW(0), CLAY_SIZING_FIXED(height)},
			.padding = CLAY_PADDING_ALL(10),
			.childAlignment = Clay_ChildAlignment{CLAY_ALIGN_X_CENTER, CLAY_ALIGN_Y_CENTER},
		},
		.backgroundColor = background,
		.cornerRadius = CLAY_CORNER_RADIUS(8),
	}) {
		CLAY_TEXT(
			ui.toClayString(text),
			CLAY_TEXT_CONFIG({
				.textColor = textColor,
				.fontSize = 15,
				.wrapMode = CLAY_TEXT_WRAP_NONE,
			}));
	}
}

InputFieldManager::FieldQueryResult drawInputField(
	UiManager& ui,
	std::string_view inputId,
	std::string_view initialText,
	size_t maxBytes,
	EditorKind editorKind) {
	const std::string inputContentId = std::string(inputId) + "/content";
	const std::string inputTextId = std::string(inputId) + "/text";

	InputFieldManager::FieldRequest request{};
	request.fieldId = inputId;
	request.initialText = initialText;
	request.config = InputFieldManager::FieldConfig{
		.readOnly = false,
		.allowNewline = false,
		.allowArrowNavigation = true,
		.maxBytes = maxBytes,
	};
	request.textElementId = ui.toClayEID(inputTextId);
	request.contentElementId = ui.toClayEID(inputContentId);
	const InputFieldManager::FieldQueryResult query = ui.inputFields().requestField(request);

	bool parseOk = true;
	if (editorKind == EditorKind::SignedInt) {
		int64_t parsed = 0;
		parseOk = parseInt64(query.text, parsed);
	} else if (editorKind == EditorKind::UnsignedInt) {
		uint64_t parsed = 0;
		parseOk = parseUInt64(query.text, parsed);
	} else if (editorKind == EditorKind::Floating) {
		double parsed = 0.0;
		parseOk = parseDouble(query.text, parsed);
	}

	const Clay_Color borderColor = query.hasPrimaryCaret
		? kColorAccent
		: (parseOk ? Clay_Color{63.0f, 73.0f, 89.0f, 255.0f} : kColorDanger);

	CLAY({
		.id = ui.toClaySID(inputContentId),
		.layout = {
			.sizing = {CLAY_SIZING_GROW(0), CLAY_SIZING_FIXED(34)},
			.padding = CLAY_PADDING_ALL(10),
			.childAlignment = Clay_ChildAlignment{CLAY_ALIGN_X_LEFT, CLAY_ALIGN_Y_CENTER},
		},
		.backgroundColor = kColorInput,
		.cornerRadius = CLAY_CORNER_RADIUS(8),
		.border = {
			.color = borderColor,
			.width = Clay_BorderWidth{
				.left = 1,
				.right = 1,
				.top = 1,
				.bottom = 1,
				.betweenChildren = 0,
			},
		},
	}) {
		CLAY({
			.id = ui.toClaySID(inputTextId),
			.layout = {
				.sizing = {CLAY_SIZING_GROW(0), CLAY_SIZING_GROW(0)},
				.childAlignment = Clay_ChildAlignment{CLAY_ALIGN_X_LEFT, CLAY_ALIGN_Y_CENTER},
			},
		}) {
			CLAY_TEXT(
				ui.toClayString(query.text.empty() ? std::string_view{" "} : query.text),
				CLAY_TEXT_CONFIG({
					.textColor = kColorText,
					.fontSize = 15,
					.wrapMode = CLAY_TEXT_WRAP_NONE,
				}));
		}
	}

	return query;
}

void applyFieldInputOverride(
	DevRuntime& runtime,
	uint64_t definitionId,
	uint64_t fieldHash,
	EditorKind kind,
	std::string_view text) {
	switch (kind) {
	case EditorKind::Bool: {
		bool parsed = false;
		if (parseBool(text, parsed)) {
			runtime.setDefinitionParamOverride(definitionId, fieldHash, parsed);
		}
		return;
	}
	case EditorKind::SignedInt: {
		int64_t parsed = 0;
		if (parseInt64(text, parsed)) {
			runtime.setDefinitionParamOverride(definitionId, fieldHash, parsed);
		}
		return;
	}
	case EditorKind::UnsignedInt: {
		uint64_t parsed = 0;
		if (!parseUInt64(text, parsed)) {
			return;
		}
		if (parsed > static_cast<uint64_t>(std::numeric_limits<int64_t>::max())) {
			parsed = static_cast<uint64_t>(std::numeric_limits<int64_t>::max());
		}
		runtime.setDefinitionParamOverride(definitionId, fieldHash, static_cast<int64_t>(parsed));
		return;
	}
	case EditorKind::Floating: {
		double parsed = 0.0;
		if (parseDouble(text, parsed)) {
			runtime.setDefinitionParamOverride(definitionId, fieldHash, parsed);
		}
		return;
	}
	case EditorKind::String:
		runtime.setDefinitionParamOverride(definitionId, fieldHash, std::string(text));
		return;
	default:
		return;
	}
}

void runDebugViewLogic(DebugViewDefinition::InteractionContext& context) {
	DebugViewState& state = DebugViewDefinition::getOrCreateState(toFlowId(context.elementID));
	DevRegistry& registry = DevRegistry::instance();
	DevRuntime& runtime = context.uiManager.devRuntime();
	const std::vector<ElementDescriptor>& elements = registry.getElements();
	if (!ensureSelection(state, elements)) {
		return;
	}

	for (const ElementDescriptor& element : elements) {
		const std::string definitionButtonId = makeDefinitionButtonId(context.elementID, element.definitionId);
		if (isPressedNow(context.previousInteraction, context.uiManager, definitionButtonId)) {
			state.selectedDefinitionId = element.definitionId;
		}
	}

	const ElementDescriptor* selectedDefinition = registry.findElementByDefinitionId(state.selectedDefinitionId);
	if (!selectedDefinition) {
		return;
	}

	const StructDescriptor* paramsStruct = registry.findStructByTypeHash(selectedDefinition->paramsStructTypeHash);
	if (!paramsStruct) {
		return;
	}

	for (const FieldDescriptor& field : paramsStruct->fields) {
		const uint64_t fieldHash = field.fieldHash == 0u ? hashString64(field.name) : field.fieldHash;
		const std::string resetId = makeFieldResetId(context.elementID, selectedDefinition->definitionId, fieldHash);
		if (isPressedNow(context.previousInteraction, context.uiManager, resetId)) {
			(void)runtime.clearDefinitionParamOverride(selectedDefinition->definitionId, fieldHash);
			continue;
		}

		const EditorKind editorKind = classifyField(field);
		if (editorKind == EditorKind::Bool) {
			const std::string toggleId = makeFieldToggleId(context.elementID, selectedDefinition->definitionId, fieldHash);
			if (isPressedNow(context.previousInteraction, context.uiManager, toggleId)) {
				const bool current = getOverrideAsBool(runtime, selectedDefinition->definitionId, fieldHash).value_or(false);
				runtime.setDefinitionParamOverride(selectedDefinition->definitionId, fieldHash, !current);
			}
			continue;
		}

		if (editorKind == EditorKind::SignedInt || editorKind == EditorKind::UnsignedInt || editorKind == EditorKind::Floating) {
			const std::string minusId = makeFieldMinusId(context.elementID, selectedDefinition->definitionId, fieldHash);
			const std::string plusId = makeFieldPlusId(context.elementID, selectedDefinition->definitionId, fieldHash);
			const bool decrement = isPressedNow(context.previousInteraction, context.uiManager, minusId);
			const bool increment = isPressedNow(context.previousInteraction, context.uiManager, plusId);
			if (!decrement && !increment) {
				continue;
			}

			const double direction = decrement ? -1.0 : 1.0;
			if (editorKind == EditorKind::Floating) {
				const double current = getOverrideAsDouble(runtime, selectedDefinition->definitionId, fieldHash).value_or(0.0);
				runtime.setDefinitionParamOverride(selectedDefinition->definitionId, fieldHash, current + direction * 0.1);
				continue;
			}

			int64_t current = getOverrideAsInt64(runtime, selectedDefinition->definitionId, fieldHash).value_or(0);
			current += static_cast<int64_t>(direction);
			if (editorKind == EditorKind::UnsignedInt && current < 0) {
				current = 0;
			}
			runtime.setDefinitionParamOverride(selectedDefinition->definitionId, fieldHash, current);
		}
	}
}

void buildDebugView(DebugViewDefinition::BuildContext& context) {
	DebugViewState& state = DebugViewDefinition::getOrCreateState(toFlowId(context.elementID));
	DevRegistry& registry = DevRegistry::instance();
	DevRuntime& runtime = context.uiManager.devRuntime();
	const std::vector<ElementDescriptor>& elements = registry.getElements();
	const bool hasSelection = ensureSelection(state, elements);
	const ElementDescriptor* selectedDefinition = hasSelection
		? registry.findElementByDefinitionId(state.selectedDefinitionId)
		: nullptr;
	const StructDescriptor* selectedParamsStruct = selectedDefinition
		? registry.findStructByTypeHash(selectedDefinition->paramsStructTypeHash)
		: nullptr;

	CLAY({
		.id = context.uiManager.toClaySID(context.elementID),
		.layout = {
			.sizing = {CLAY_SIZING_GROW(0), CLAY_SIZING_GROW(0)},
			.layoutDirection = CLAY_TOP_TO_BOTTOM,
		},
		.backgroundColor = kColorBg,
	}) {
			CLAY({
				.layout = {
					.sizing = {CLAY_SIZING_GROW(0), CLAY_SIZING_GROW(0)},
					.padding = CLAY_PADDING_ALL(12),
					.childGap = 12,
					.layoutDirection = CLAY_LEFT_TO_RIGHT,
				},
			}) {
				CLAY({
					.layout = {
						.sizing = {CLAY_SIZING_FIXED(context.params.leftPanelWidthPx), CLAY_SIZING_GROW(0)},
						.padding = CLAY_PADDING_ALL(10),
						.childGap = 8,
						.layoutDirection = CLAY_TOP_TO_BOTTOM,
					},
					.backgroundColor = kColorPanel,
					.cornerRadius = CLAY_CORNER_RADIUS(12),
			}) {
				drawSectionHeader(context.uiManager, "Definitions");
				for (const ElementDescriptor& element : elements) {
					const bool isSelected = (element.definitionId == state.selectedDefinitionId);
					const Clay_Color itemBackground = isSelected ? kColorPanelSelected : kColorPanelSoft;
					drawButton(
						context.uiManager,
						makeDefinitionButtonId(context.elementID, element.definitionId),
						element.definitionName.empty() ? element.definitionTypeToken : element.definitionName,
						itemBackground,
						isSelected ? kColorAccent : kColorText,
						34.0f);
				}

				if (elements.empty()) {
					CLAY_TEXT(
						context.uiManager.toClayString("No registered definitions."),
						CLAY_TEXT_CONFIG({
							.textColor = kColorTextMuted,
							.fontSize = 14,
							.wrapMode = CLAY_TEXT_WRAP_WORDS,
						}));
				}
			}

				CLAY({
					.layout = {
						.sizing = {CLAY_SIZING_GROW(0), CLAY_SIZING_GROW(0)},
						.padding = CLAY_PADDING_ALL(12),
						.childGap = 10,
						.layoutDirection = CLAY_TOP_TO_BOTTOM,
					},
					.backgroundColor = kColorPanel,
					.cornerRadius = CLAY_CORNER_RADIUS(12),
			}) {
				if (!selectedDefinition) {
					CLAY_TEXT(
						context.uiManager.toClayString("Select a definition from the left panel."),
						CLAY_TEXT_CONFIG({
							.textColor = kColorTextMuted,
							.fontSize = 15,
							.wrapMode = CLAY_TEXT_WRAP_WORDS,
						}));
				} else {
						CLAY({
							.layout = {
								.sizing = {CLAY_SIZING_GROW(0), CLAY_SIZING_FIXED(56)},
								.childGap = 2,
								.layoutDirection = CLAY_TOP_TO_BOTTOM,
							},
						}) {
						CLAY_TEXT(
							context.uiManager.toClayString(selectedDefinition->definitionName),
							CLAY_TEXT_CONFIG({
								.textColor = kColorText,
								.fontSize = 20,
								.wrapMode = CLAY_TEXT_WRAP_NONE,
							}));
						CLAY_TEXT(
							context.uiManager.toClayString(selectedDefinition->definitionTypeToken),
							CLAY_TEXT_CONFIG({
								.textColor = kColorTextMuted,
								.fontSize = 13,
								.wrapMode = CLAY_TEXT_WRAP_NONE,
							}));
					}

					drawSectionHeader(context.uiManager, "Definition Param Overrides");
					if (!selectedParamsStruct || selectedParamsStruct->fields.empty()) {
						CLAY_TEXT(
							context.uiManager.toClayString("No editable primitive param fields."),
							CLAY_TEXT_CONFIG({
								.textColor = kColorTextMuted,
								.fontSize = 14,
								.wrapMode = CLAY_TEXT_WRAP_WORDS,
							}));
						} else {
							for (const FieldDescriptor& field : selectedParamsStruct->fields) {
								const uint64_t fieldHash = field.fieldHash == 0u ? hashString64(field.name) : field.fieldHash;
								const EditorKind editorKind = classifyField(field);
								const std::string resetId = makeFieldResetId(context.elementID, selectedDefinition->definitionId, fieldHash);

								CLAY({
									.layout = {
										.sizing = {CLAY_SIZING_GROW(0), CLAY_SIZING_FIXED(44)},
										.padding = CLAY_PADDING_ALL(6),
										.childGap = 8,
										.childAlignment = Clay_ChildAlignment{CLAY_ALIGN_X_LEFT, CLAY_ALIGN_Y_CENTER},
										.layoutDirection = CLAY_LEFT_TO_RIGHT,
									},
									.backgroundColor = kColorPanelSoft,
									.cornerRadius = CLAY_CORNER_RADIUS(8),
							}) {
									CLAY({
										.layout = {
											.sizing = {CLAY_SIZING_FIXED(230), CLAY_SIZING_GROW(0)},
											.childGap = 2,
											.layoutDirection = CLAY_TOP_TO_BOTTOM,
										},
									}) {
									CLAY_TEXT(
										context.uiManager.toClayString(field.name),
										CLAY_TEXT_CONFIG({
											.textColor = kColorText,
											.fontSize = 14,
											.wrapMode = CLAY_TEXT_WRAP_NONE,
										}));
									CLAY_TEXT(
										context.uiManager.toClayString(field.fieldTypeToken),
										CLAY_TEXT_CONFIG({
											.textColor = kColorTextMuted,
											.fontSize = 12,
											.wrapMode = CLAY_TEXT_WRAP_NONE,
										}));
								}

									CLAY({
										.layout = {
											.sizing = {CLAY_SIZING_GROW(0), CLAY_SIZING_GROW(0)},
											.childGap = 6,
											.childAlignment = Clay_ChildAlignment{CLAY_ALIGN_X_LEFT, CLAY_ALIGN_Y_CENTER},
											.layoutDirection = CLAY_LEFT_TO_RIGHT,
										},
									}) {
									if (editorKind == EditorKind::Bool) {
										const bool value = getOverrideAsBool(runtime, selectedDefinition->definitionId, fieldHash).value_or(false);
										drawButton(
											context.uiManager,
											makeFieldToggleId(context.elementID, selectedDefinition->definitionId, fieldHash),
											value ? "true" : "false",
											value ? kColorSuccess : Clay_Color{74.0f, 86.0f, 107.0f, 255.0f},
											kColorText,
											34.0f);
									} else if (
										editorKind == EditorKind::SignedInt ||
										editorKind == EditorKind::UnsignedInt ||
										editorKind == EditorKind::Floating ||
										editorKind == EditorKind::String) {
										if (editorKind == EditorKind::SignedInt || editorKind == EditorKind::UnsignedInt || editorKind == EditorKind::Floating) {
											drawButton(
												context.uiManager,
												makeFieldMinusId(context.elementID, selectedDefinition->definitionId, fieldHash),
												"-",
												Clay_Color{68.0f, 80.0f, 99.0f, 255.0f},
												kColorText,
												34.0f);
										}

										const std::string inputId = makeFieldInputId(context.elementID, selectedDefinition->definitionId, fieldHash);
											const std::string initialText = defaultTextForKind(
												editorKind,
												runtime,
												selectedDefinition->definitionId,
												fieldHash);

											const InputFieldManager::FieldQueryResult fieldQuery = drawInputField(
												context.uiManager,
												inputId,
												initialText,
												editorKind == EditorKind::String ? 256u : 64u,
												editorKind);

										bool queryParseOk = true;
										if (editorKind == EditorKind::SignedInt) {
											int64_t parsed = 0;
											queryParseOk = parseInt64(fieldQuery.text, parsed);
										} else if (editorKind == EditorKind::UnsignedInt) {
											uint64_t parsed = 0;
											queryParseOk = parseUInt64(fieldQuery.text, parsed);
											} else if (editorKind == EditorKind::Floating) {
												double parsed = 0.0;
												queryParseOk = parseDouble(fieldQuery.text, parsed);
											}

										if (queryParseOk) {
											applyFieldInputOverride(
												runtime,
												selectedDefinition->definitionId,
												fieldHash,
												editorKind,
												fieldQuery.text);
										}

										if (editorKind == EditorKind::SignedInt || editorKind == EditorKind::UnsignedInt || editorKind == EditorKind::Floating) {
											drawButton(
												context.uiManager,
												makeFieldPlusId(context.elementID, selectedDefinition->definitionId, fieldHash),
												"+",
												Clay_Color{68.0f, 80.0f, 99.0f, 255.0f},
												kColorText,
												34.0f);
										}
									} else {
										CLAY_TEXT(
											context.uiManager.toClayString("Unsupported type"),
											CLAY_TEXT_CONFIG({
												.textColor = kColorDanger,
												.fontSize = 13,
												.wrapMode = CLAY_TEXT_WRAP_NONE,
											}));
									}
								}

								drawButton(
									context.uiManager,
									resetId,
									"Reset",
									Clay_Color{94.0f, 66.0f, 66.0f, 255.0f},
									kColorText,
									34.0f);
							}
						}
					}
				}
			}
		}

		std::size_t selectedOverrideCount = 0u;
		if (selectedDefinition) {
			for (const auto& [key, _] : runtime.definitionParamOverrides()) {
				if (key.definitionId == selectedDefinition->definitionId) {
					++selectedOverrideCount;
				}
			}
		}

		CLAY({
			.layout = {
				.sizing = {CLAY_SIZING_GROW(0), CLAY_SIZING_FIXED(context.params.footerHeightPx)},
				.padding = CLAY_PADDING_ALL(10),
				.childAlignment = Clay_ChildAlignment{CLAY_ALIGN_X_LEFT, CLAY_ALIGN_Y_CENTER},
			},
			.backgroundColor = Clay_Color{14.0f, 18.0f, 24.0f, 255.0f},
		}) {
			const std::string footerText = selectedDefinition
				? ("Selected: " + selectedDefinition->definitionName + " | Active definition overrides: " + std::to_string(selectedOverrideCount))
				: std::string("No definition selected");
			CLAY_TEXT(
				context.uiManager.toClayString(footerText),
				CLAY_TEXT_CONFIG({
					.textColor = kColorTextMuted,
					.fontSize = 13,
					.wrapMode = CLAY_TEXT_WRAP_NONE,
				}));
		}
	}
}

inline const DebugViewDefinition kDebugViewElement = {
	nullptr,
	nullptr,
	nullptr,
	nullptr,
	+[](DebugViewDefinition::InteractionContext& context) {
		runDebugViewLogic(context);
	},
	nullptr,
	+[](DebugViewDefinition::BuildContext& context) {
		buildDebugView(context);
	},
};

} // namespace

void drawDebugView(UiManager& uiManager) {
	uiManager
		.createElement(kDebugViewElement, "flowui/dev/debug-view")
		.setParameters(DebugViewParams{})
		.draw();
}

} // namespace FlowUi::devMode

#endif
