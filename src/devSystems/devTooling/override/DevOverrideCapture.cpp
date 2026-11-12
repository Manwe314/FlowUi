#include "devSystems/devTooling/override/DevOverrideCapture.hpp"

#if FLOW_UI_DEV_MODE

#include <algorithm>
#include <utility>

#include "devSystems/devMonitoringAndReporting/timing/DevTimingZone.hpp"
#include "devSystems/devTooling/override/DevOverrideApply.hpp"
#include "FlowUi/Public/managers/ThemeManager.hpp"

namespace FlowUi::devSystems::tooling {
namespace {

const DevElementCaptureSnapshot kEmptyElementSnapshot{};

std::size_t valueBytes(const std::vector<DevCapturedField>& fields) noexcept {
	std::size_t bytes = 0;
	for (const DevCapturedField& field : fields)
		bytes += field.value.heapBytes() + field.authoredValue.heapBytes();
	return bytes;
}

[[nodiscard]] DevValueCaptureStatus copyCapturedField(
	const tooling::DevSchemaGeneration& schema, const tooling::DevFieldSchema& field,
	const void* owner, DevOwnedValue& destination) noexcept {
	const auto* type = schema.type(field.valueType);
	if (!type || type->capture != tooling::DevCaptureCapability::Value ||
		field.operations >= schema.fieldOperations.size())
		return DevValueCaptureStatus::Unsupported;
	const auto* operations = schema.fieldOperations[field.operations];
	if (!operations || !operations->constAddress)
		return DevValueCaptureStatus::Unsupported;
	if (!owner)
		return DevValueCaptureStatus::Unavailable;
	const auto result = DevOwnedValue::copyFrom(
		schema, field.valueType, operations->constAddress(owner), destination);
	return result == tooling::DevValueOperationStatus::Success
		? DevValueCaptureStatus::Available
		: result == tooling::DevValueOperationStatus::Unsupported
			? DevValueCaptureStatus::Unsupported : DevValueCaptureStatus::CopyFailed;
}

void captureFields(const tooling::DevSchemaGeneration& schema,
	tooling::DevTypeIndex owner_type, const void* owner, bool authored,
	std::vector<DevCapturedField>& destination) {
	const auto fields = schema.fieldsOf(owner_type);
	destination.reserve(fields.size());
	for (const auto& field : fields) {
		DevCapturedField captured;
		captured.field = tooling::DevFieldIndex{
			static_cast<std::uint32_t>(&field - schema.fields.data() + 1)};
		if (authored)
			captured.authoredStatus = copyCapturedField(schema, field, owner, captured.authoredValue);
		else
			captured.captureStatus = copyCapturedField(schema, field, owner, captured.value);
		destination.emplace_back(std::move(captured));
	}
}

struct ThemeCaptureContext {
	DevThemeCaptureSnapshot* snapshot = nullptr;
	const void* overrideOwner = nullptr;
	bool (*isOverridden)(
		const void*, tooling::DevTypeId, std::string_view,
		tooling::DevFieldId, DevOverrideLayer&) noexcept = nullptr;
};

bool captureTheme(void* userData, const ThemeManager::DevThemePayloadView& view) noexcept {
	auto& context = *static_cast<ThemeCaptureContext*>(userData);
	DevThemeCaptureSnapshot& snapshot = *context.snapshot;
	if (!snapshot.schema || !view.payload) return true;
	const tooling::DevThemeSchema* theme = snapshot.schema->findTheme(view.type);
	if (!theme) return true;
	const tooling::DevTypeSchema* type = snapshot.schema->type(theme->themeType);
	if (!type || type->capture != tooling::DevCaptureCapability::Value) return true;

	try {
		DevCapturedTheme captured{
			.themeType = view.type,
			.revision = view.revision,
			.variantOffset = static_cast<std::uint32_t>(snapshot.strings.size()),
			.variantSize = static_cast<std::uint32_t>(view.variant.size()),
			.firstField = static_cast<std::uint32_t>(snapshot.fields.size()),
			.active = view.active,
		};
		snapshot.strings.insert(snapshot.strings.end(), view.variant.begin(), view.variant.end());
		for (const tooling::DevFieldSchema& field : snapshot.schema->fieldsOf(theme->themeType)) {
			if (field.operations >= snapshot.schema->fieldOperations.size()) continue;
			const tooling::DevTypeSchema* valueType = snapshot.schema->type(field.valueType);
			const tooling::DevFieldOps* fieldOps =
				snapshot.schema->fieldOperations[field.operations];
			if (!valueType || valueType->capture != tooling::DevCaptureCapability::Value ||
				!fieldOps || !fieldOps->constAddress) continue;
			const void* source = fieldOps->constAddress(view.payload);
			DevCapturedField capturedField{};
			const std::size_t index = static_cast<std::size_t>(
				std::addressof(field) - snapshot.schema->fields.data());
			capturedField.field = tooling::DevFieldIndex{
				static_cast<std::uint32_t>(index + 1u)};
			if (DevOwnedValue::copyFrom(
				*snapshot.schema, field.valueType, source, capturedField.value) !=
				tooling::DevValueOperationStatus::Success) continue;
			capturedField.captureStatus = DevValueCaptureStatus::Available;
			capturedField.authoredStatus = copyCapturedField(
				*snapshot.schema, field, view.authored, capturedField.authoredValue);
			if (context.isOverridden) {
				capturedField.overridden = context.isOverridden(
					context.overrideOwner, view.type, view.variant, field.id,
					capturedField.winningLayer);
			}
			snapshot.fields.push_back(std::move(capturedField));
		}
		captured.fieldCount = static_cast<std::uint32_t>(
			snapshot.fields.size() - captured.firstField);
		snapshot.themes.push_back(captured);
		return true;
	} catch (...) {
		return false;
	}
}

} // namespace

void DevOverrideCapture::beginWindowFrame(
	WindowId window,
	std::uint64_t frameNumber,
	tooling::DevSchemaView schema) {
	WindowBuffers& buffers = windows_[window];
	clear(buffers.building);
	buffers.building.window = window;
	buffers.building.frameNumber = frameNumber;
	buffers.building.schema = std::move(schema);
	buffers.active = true;
}

DevAuthoredElementCapture DevOverrideCapture::captureAuthoredParameters(
	FlowDefinitionID definition, tooling::DevSchemaView schema,
	const void* parameters) const noexcept {
	DevAuthoredElementCapture capture;
	if (!schema)
		return capture;
	const auto* element = schema->findElement(definition);
	if (!element)
		return capture;
	capture.schema = std::move(schema);
	try {
		captureFields(*capture.schema, element->parametersType, parameters, true, capture.fields);
		capture.status = DevValueCaptureStatus::Available;
	} catch (...) {
		capture.fields.clear();
		capture.status = DevValueCaptureStatus::CopyFailed;
	}
	return capture;
}

void DevOverrideCapture::captureElement(
	FlowDefinitionID definition,
	WindowId window,
	::FlowUi::detail::element::ElementInstanceKey instance,
	std::uint32_t flowNode,
	const void* effectiveParameters,
	const DevOverrideApply& overrides,
	DevTimingRecorder* timing,
	DevAuthoredElementCapture authored,
	const void* state,
	const void* resources,
	bool isDevInternal) noexcept {
	const auto found = windows_.find(window);
	if (found == windows_.end() || !found->second.active) return;
	DevElementCaptureSnapshot& snapshot = found->second.building;
	if (!snapshot.schema) return;
	const auto* element = snapshot.schema->findElement(definition);
	if (!element) return;
	FLOWUI_DEV_TIMING_ZONE_BALANCED_IF(
		timing, TimingCategory::DevTool, TimingZoneRole::DevToolWork,
		"flowui.dev_override.capture_element");
	const auto first_field = snapshot.fields.size();
	try {
		std::vector<DevCapturedField> parameter_fields, state_fields, resource_fields;
		captureFields(*snapshot.schema, element->parametersType, effectiveParameters, false,
			parameter_fields);
		captureFields(*snapshot.schema, element->stateType, state, false, state_fields);
		captureFields(*snapshot.schema, element->resourcesType, resources, false, resource_fields);
		const bool matching_schema = authored.schema &&
			authored.schema->generation == snapshot.schema->generation;
		for (auto& field : parameter_fields) {
			if (matching_schema) {
				const auto source = std::find_if(authored.fields.begin(), authored.fields.end(),
					[&](const DevCapturedField& candidate) { return candidate.field == field.field; });
				if (source != authored.fields.end()) {
					field.authoredValue = std::move(source->authoredValue);
					field.authoredStatus = source->authoredStatus;
				} else if (authored.status != DevValueCaptureStatus::Available)
					field.authoredStatus = authored.status;
			} else if (authored.schema) {
				field.authoredStatus = DevValueCaptureStatus::SchemaMismatch;
			}
			const auto& descriptor = snapshot.schema->fields[field.field.value - 1];
			field.overridden = overrides.winningLayer(
				definition, window, instance, descriptor.id, field.winningLayer);
		}
		DevCapturedElement captured{
			.definition = definition,
			.window = window,
			.instance = instance,
			.flowNode = flowNode,
			.firstField = static_cast<std::uint32_t>(first_field),
			.fieldCount = static_cast<std::uint32_t>(parameter_fields.size()),
			.firstStateField = static_cast<std::uint32_t>(first_field + parameter_fields.size()),
			.stateFieldCount = static_cast<std::uint32_t>(state_fields.size()),
			.firstResourceField = static_cast<std::uint32_t>(
				first_field + parameter_fields.size() + state_fields.size()),
			.resourceFieldCount = static_cast<std::uint32_t>(resource_fields.size()),
			.isDevInternal = isDevInternal,
		};
		snapshot.fields.reserve(first_field + parameter_fields.size() + state_fields.size() +
			resource_fields.size());
		for (auto* fields : {&parameter_fields, &state_fields, &resource_fields})
			for (auto& field : *fields)
				snapshot.fields.emplace_back(std::move(field));
		snapshot.elements.emplace_back(captured);
		capturedElementFieldCount_ += captured.fieldCount + captured.stateFieldCount +
			captured.resourceFieldCount;
	} catch (...) {
		snapshot.fields.resize(first_field);
	}
}

void DevOverrideCapture::endWindowFrame(WindowId window) noexcept {
	const auto found = windows_.find(window);
	if (found == windows_.end() || !found->second.active) return;
	WindowBuffers& buffers = found->second;
	buffers.building.generation = nextElementGeneration_++;
	std::swap(buffers.building, buffers.published);
	clear(buffers.building);
	buffers.active = false;
}

void DevOverrideCapture::cancelWindowFrame(WindowId window) noexcept {
	const auto found = windows_.find(window);
	if (found == windows_.end()) return;
	clear(found->second.building);
	found->second.active = false;
}

void DevOverrideCapture::release_window(WindowId window) noexcept {
	windows_.erase(window);
}

void DevOverrideCapture::captureThemes(
	const ThemeManager& themes,
	tooling::DevSchemaView schema,
	const void* overrideOwner,
	bool (*isOverridden)(
		const void*, tooling::DevTypeId, std::string_view,
		tooling::DevFieldId, DevOverrideLayer&) noexcept,
	DevTimingRecorder* timing) noexcept {
	FLOWUI_DEV_TIMING_ZONE_BALANCED_IF(
		timing, TimingCategory::DevTool, TimingZoneRole::DevToolWork,
		"flowui.dev_override.capture_themes");
	clear(buildingThemes_);
	buildingThemes_.schema = std::move(schema);
	ThemeCaptureContext context{
		.snapshot = &buildingThemes_,
		.overrideOwner = overrideOwner,
		.isOverridden = isOverridden,
	};
	(void)themes.visitDevThemePayloads(&context, &captureTheme);
	capturedThemeFieldCount_ += buildingThemes_.fields.size();
	std::sort(buildingThemes_.themes.begin(), buildingThemes_.themes.end(),
		[&](const DevCapturedTheme& left, const DevCapturedTheme& right) {
			if (left.themeType != right.themeType) return left.themeType < right.themeType;
			return buildingThemes_.variant(left) < buildingThemes_.variant(right);
		});
	buildingThemes_.generation = nextThemeGeneration_++;
	std::swap(buildingThemes_, publishedThemes_);
	clear(buildingThemes_);
}

const DevElementCaptureSnapshot& DevOverrideCapture::elements(WindowId window) const noexcept {
	const auto found = windows_.find(window);
	return found == windows_.end() ? kEmptyElementSnapshot : found->second.published;
}

void DevOverrideCapture::visitElementSnapshots(void* context,
	bool (*visitor)(void*, const DevElementCaptureSnapshot&) noexcept) const noexcept {
	if (!visitor)
		return;
	for (const auto& entry : windows_)
		if (entry.second.published.generation && !visitor(context, entry.second.published))
			return;
}

std::size_t DevOverrideCapture::memoryFootprintBytes() const noexcept {
	const auto elementBytes = [](const DevElementCaptureSnapshot& snapshot) {
		return snapshot.elements.capacity() * sizeof(DevCapturedElement) +
			snapshot.fields.capacity() * sizeof(DevCapturedField) + valueBytes(snapshot.fields);
	};
	const auto themeBytes = [](const DevThemeCaptureSnapshot& snapshot) {
		return snapshot.strings.capacity() +
			snapshot.themes.capacity() * sizeof(DevCapturedTheme) +
			snapshot.fields.capacity() * sizeof(DevCapturedField) + valueBytes(snapshot.fields);
	};
	std::size_t bytes = windows_.bucket_count() * sizeof(void*) +
		themeBytes(buildingThemes_) + themeBytes(publishedThemes_);
	for (const auto& window : windows_) {
		bytes += sizeof(window) + elementBytes(window.second.building) +
			elementBytes(window.second.published);
	}
	return bytes;
}

void DevOverrideCapture::clear(DevElementCaptureSnapshot& snapshot) noexcept {
	snapshot.window = InvalidWindowId;
	snapshot.frameNumber = 0;
	snapshot.generation = 0;
	snapshot.schema = {};
	snapshot.elements.clear();
	snapshot.fields.clear();
}

void DevOverrideCapture::clear(DevThemeCaptureSnapshot& snapshot) noexcept {
	snapshot.generation = 0;
	snapshot.schema = {};
	snapshot.strings.clear();
	snapshot.themes.clear();
	snapshot.fields.clear();
}

} // namespace FlowUi::devSystems::tooling

#endif
