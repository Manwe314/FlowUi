#ifdef NDEBUG
#undef NDEBUG
#endif
#include "../demo/DemoElementSchemas.hpp"
#include "FlowUi/Public/BuildConfig.hpp"
#include "devSystems/devInterface/Inspect/DevValueSummary.hpp"
#include "devSystems/devTooling/override/DevOverrideEngine.hpp"
#include "devSystems/devTooling/schema/DevSchemaRegistry.hpp"
#include "FlowUi/Public/managers/ThemeManager.hpp"
#include <cassert>
#include <cstdlib>
#include <optional>
#include <string>
#include <vector>

using namespace FlowUi;
using namespace FlowUi::devSystems;
using namespace FlowUi::devSystems::interface_elements;

namespace {
struct UnregisteredValue {
	int amount = 7;
};
struct SummaryValues {
	std::optional<float> amount{};
	std::optional<Clay_Color> color{};
	std::optional<Clay_Sizing> sizing{};
	std::optional<std::string> text{};
	std::vector<int> sequence{1, 2, 3};
	UnregisteredValue unknown{};
	int* pointer = nullptr;
	FontStyle style = FontStyle::Normal;
};
FLOWUI_DEV_SCHEMA(SummaryValues, FLOWUI_DEV_FIELD(SummaryValues, amount),
				  FLOWUI_DEV_FIELD(SummaryValues, color), FLOWUI_DEV_FIELD(SummaryValues, sizing),
				  FLOWUI_DEV_FIELD(SummaryValues, text), FLOWUI_DEV_FIELD(SummaryValues, sequence),
				  FLOWUI_DEV_FIELD(SummaryValues, unknown),
				  FLOWUI_DEV_FIELD(SummaryValues, pointer), FLOWUI_DEV_FIELD(SummaryValues, style))

[[nodiscard]] const tooling::DevFieldSchema& field_named(const tooling::DevSchemaGeneration& schema,
														 const tooling::DevTypeSchema& owner,
														 std::string_view name) {
	const auto index = tooling::DevTypeIndex{static_cast<uint32_t>(&owner - schema.types.data())};
	for (const auto& field : schema.fieldsOf(index))
		if (schema.string(field.name) == name)
			return field;
	std::abort();
}
} // namespace

int main() {
	tooling::DevSchemaRegistry registry;
	registry.ensureStruct<SummaryValues>();
	registry.ensureElement<FSEL::Button>();
	registry.ensureElement<FSEL::ComboBox>();
	registry.ensureElement<FSEL::TextInput>();
	registry.ensureElement<FSEL::TextArea>();
	registry.ensureElement<FSEL::NumberInput<float>>();
	registry.ensureElement<FSEL::DragValue<float>>();
	assert(registry.publishPendingAtSafePoint());
	const auto schema = registry.view();
	const auto* owner = schema->findType(::FlowUi::detail::typeHash<SummaryValues>());
	assert(owner);
	SummaryValues values;
	const auto summary = [&](std::string_view name, const void* value,
							 DevSummaryAvailability availability =
								 DevSummaryAvailability::NotCaptured) {
		const auto& field = field_named(*schema, *owner, name);
		return format_value_summary(*schema, schema->type(field.valueType), value, nullptr, &field,
									availability);
	};
	assert(summary("amount", &values.amount).primary == "Unset / inherits");
	values.amount = 12.5f;
	assert(summary("amount", &values.amount).primary == "12.5");
	values.color = Clay_Color{18, 52, 86, 255};
	const auto color = summary("color", &values.color);
	assert(color.primary == "#123456FF" && color.swatch.has_value());
	values.sizing = Clay_Sizing{CLAY_SIZING_FIXED(123), CLAY_SIZING_FIXED(45)};
	const auto sizing = summary("sizing", &values.sizing);
	assert(sizing.detail.find("123") != std::string::npos &&
		   sizing.detail.find("45") != std::string::npos);
	values.text = std::string(200, 'z') + "\nfull detail";
	const auto text = summary("text", &values.text);
	assert(text.primary.size() <= 96 && text.detail.find("\\nfull detail") != std::string::npos);
	assert(summary("sequence", &values.sequence).primary == "3 items");
	assert(summary("unknown", nullptr).primary.starts_with("Unregistered"));
	const auto owned_unknown = summary("unknown", &values.unknown);
	assert(owned_unknown.primary.starts_with("Unregistered"));
	assert(owned_unknown.status == DevQuickStatus::Warning);
	assert(owned_unknown.detail.find("UnregisteredValue") != std::string::npos);
	assert(owned_unknown.detail.find("4 bytes") != std::string::npos);
	assert(summary("pointer", nullptr).primary == "Pointer · metadata only");
	assert(summary("amount", nullptr).primary == "Value not captured");
	assert(summary("amount", nullptr, DevSummaryAvailability::CopyFailed).primary ==
		   "Value copy failed");
	assert(summary("amount", nullptr, DevSummaryAvailability::CollectionDisabled).primary ==
		   "Value collection disabled");
	assert(summary("amount", nullptr, DevSummaryAvailability::SchemaChanged)
			   .primary.starts_with("Schema changed"));
	values.style = static_cast<FontStyle>(73);
	assert(summary("style", &values.style).primary == "Unknown · 73");
	for (const auto definition :
		 {FSEL::Button::definitionId, FSEL::ComboBox::definitionId, FSEL::TextInput::definitionId,
		  FSEL::TextArea::definitionId, FSEL::NumberInput<float>::definitionId,
		  FSEL::DragValue<float>::definitionId}) {
		const auto* element = schema->findElement(definition);
		assert(element);
		const auto* parameters = schema->type(element->parametersType);
		assert(parameters);
		const auto& font = field_named(*schema, *parameters,
									   definition == FSEL::Button::definitionId ? "labelFontFamily"
																				: "fontFamily");
		assert(font.editor == tooling::DevEditorKind::FontChoice);
		assert(font.choiceDomain == tooling::DevChoiceDomain::FontFamily);
	}
	// Consume owned State/Resources ranges exactly as the read-only Inspector tabs do.
	tooling::DevOverrideEngine engine(registry);
	FSEL::ComboBoxParameters parameters;
	FSEL::ComboBoxState control_state;
	control_state.open = true;
	FSEL::ComboBoxResources resources;
	resources.openIcon.sourceKey = "test/open-icon";
	engine.beginWindowFrame(1, 41);
	auto authored = engine.captureAuthoredParameters(FSEL::ComboBox::definitionId, &parameters);
	engine.captureElement(FSEL::ComboBox::definitionId, 1,
						  ::FlowUi::detail::element::ElementInstanceKey{99}, 0, &parameters,
						  nullptr, std::move(authored), &control_state, &resources);
	engine.endWindowFrame(1);
	control_state.open = false;
	resources.openIcon.sourceKey = "changed-after-capture";
	const auto& snapshot = engine.elementSnapshot(1);
	assert(snapshot.frameNumber == 41 && snapshot.elements.size() == 1);
	const auto& captured = snapshot.elements.front();
	assert(captured.stateFieldCount > 0 && captured.resourceFieldCount == 2);
	assert(captured.firstResourceField == captured.firstStateField + captured.stateFieldCount);
	// Field capabilities describe an interned C++ member independently of its UI role.
	// Read-only State/Resources behavior is supplied by each element's role policy,
	// and the override engine only accepts parameter-owner field keys.
	const auto* captured_element = snapshot.schema->findElement(FSEL::ComboBox::definitionId);
	assert(captured_element);
	assert(captured_element->statePolicy.edit == tooling::DevEditCapability::ViewOnly);
	assert(captured_element->resourcesPolicy.edit == tooling::DevEditCapability::ViewOnly);

	const auto find_role_summary = [&](std::uint32_t first, std::uint32_t count,
									   std::string_view name) {
		assert(first + count <= snapshot.fields.size());
		for (std::size_t index = first; index < first + count; ++index) {
			const auto& value = snapshot.fields[index];
			const auto& field = snapshot.schema->fields[value.field.value - 1];
			if (snapshot.schema->string(field.name) != name)
				continue;
			assert(value.captureStatus == tooling::DevValueCaptureStatus::Available);

			return format_value_summary(*snapshot.schema, snapshot.schema->type(field.valueType),
										value.value.data(), nullptr, &field)
				.primary;
		}
		std::abort();
	};
	assert(find_role_summary(captured.firstStateField, captured.stateFieldCount, "open") == "On");
	assert(find_role_summary(captured.firstResourceField, captured.resourceFieldCount,
							 "openIcon") == "test/open-icon");
	const auto* state_type = snapshot.schema->type(captured_element->stateType);
	assert(state_type);
	const auto& state_open = field_named(*snapshot.schema, *state_type, "open");
	tooling::DevOverrideCommand invalid_state_edit;
	invalid_state_edit.element = {.definition = FSEL::ComboBox::definitionId,
								  .window = 1,
								  .instance = ::FlowUi::detail::element::ElementInstanceKey{99},
								  .scope = tooling::DevOverrideScope::ExactInstance};
	invalid_state_edit.field = {.ownerType = state_type->id, .field = state_open.id};
	invalid_state_edit.layer = tooling::DevOverrideLayer::LiveInstance;
	assert(engine.copyValue(false, invalid_state_edit.value) ==
		   tooling::DevValueOperationStatus::Success);
	tooling::DevChangeSet attempted_edit;
	attempted_edit.transaction = 1;
	attempted_edit.commands.reserve(1);
	attempted_edit.commands.emplace_back(std::move(invalid_state_edit));
	assert(engine.submit(std::move(attempted_edit)));
	ThemeManager themes;
	engine.commitAtSafePoint(themes);
	assert(!engine.commandResults().empty());
	assert(!engine.commandResults().back().applied &&
		   engine.commandResults().back().status == tooling::DevCommandStatus::FieldUnavailable);
}
