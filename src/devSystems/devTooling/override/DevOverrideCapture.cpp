#include "devSystems/devTooling/override/DevOverrideCapture.hpp"

#if FLOW_UI_DEV_MODE

#include <algorithm>
#include <utility>

#include "devSystems/devMonitoringAndReporting/timing/DevTimingZone.hpp"
#include "devSystems/devTooling/override/DevOverrideApply.hpp"
#include "managers/ThemeManager.hpp"

namespace FlowUi::devSystems::tooling {
namespace {

const DevElementCaptureSnapshot kEmptyElementSnapshot{};

std::size_t valueBytes(const std::vector<DevCapturedField>& fields) noexcept {
	std::size_t bytes = 0;
	for (const DevCapturedField& field : fields) bytes += field.value.heapBytes();
	return bytes;
}

struct ThemeCaptureContext {
	DevThemeCaptureSnapshot* snapshot = nullptr;
	const void* overrideOwner = nullptr;
	bool (*isOverridden)(
		const void*, devMode::DevTypeId, std::string_view,
		devMode::DevFieldId, DevOverrideLayer&) noexcept = nullptr;
};

bool captureTheme(void* userData, const ThemeManager::DevThemePayloadView& view) noexcept {
	auto& context = *static_cast<ThemeCaptureContext*>(userData);
	DevThemeCaptureSnapshot& snapshot = *context.snapshot;
	if (!snapshot.schema || !view.payload) return true;
	const devMode::DevThemeSchema* theme = snapshot.schema->findTheme(view.type);
	if (!theme) return true;
	const devMode::DevTypeSchema* type = snapshot.schema->type(theme->themeType);
	if (!type || type->capture != devMode::DevCaptureCapability::Value) return true;

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
		for (const devMode::DevFieldSchema& field : snapshot.schema->fieldsOf(theme->themeType)) {
			if (field.operations >= snapshot.schema->fieldOperations.size()) continue;
			const devMode::DevTypeSchema* valueType = snapshot.schema->type(field.valueType);
			const devMode::DevFieldOps* fieldOps =
				snapshot.schema->fieldOperations[field.operations];
			if (!valueType || valueType->capture != devMode::DevCaptureCapability::Value ||
				!fieldOps || !fieldOps->constAddress) continue;
			const void* source = fieldOps->constAddress(view.payload);
			DevCapturedField capturedField{};
			const std::size_t index = static_cast<std::size_t>(
				std::addressof(field) - snapshot.schema->fields.data());
			capturedField.field = devMode::DevFieldIndex{
				static_cast<std::uint32_t>(index + 1u)};
			if (DevOwnedValue::copyFrom(
				*snapshot.schema, field.valueType, source, capturedField.value) !=
				devMode::DevValueOperationStatus::Success) continue;
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
	devMode::DevSchemaView schema) {
	WindowBuffers& buffers = windows_[window];
	clear(buffers.building);
	buffers.building.window = window;
	buffers.building.frameNumber = frameNumber;
	buffers.building.schema = std::move(schema);
	buffers.active = true;
}

void DevOverrideCapture::captureElement(
	FlowDefinitionID definition,
	WindowId window,
	::FlowUi::detail::element::ElementInstanceKey instance,
	std::uint32_t flowNode,
	const void* effectiveParameters,
	const DevOverrideApply& overrides,
	DevTimingRecorder* timing) noexcept {
	const auto found = windows_.find(window);
	if (found == windows_.end() || !found->second.active || !effectiveParameters) return;
	DevElementCaptureSnapshot& snapshot = found->second.building;
	if (!snapshot.schema) return;
	const devMode::DevElementSchema* element = snapshot.schema->findElement(definition);
	if (!element) return;
	const devMode::DevTypeSchema* parameterType = snapshot.schema->type(element->parametersType);
	if (!parameterType || parameterType->capture != devMode::DevCaptureCapability::Value) return;
	FLOWUI_DEV_TIMING_ZONE_BALANCED_IF(
		timing, TimingCategory::DevTool, TimingZoneRole::DevToolWork,
		"flowui.dev_override.capture_element");

	try {
		DevCapturedElement captured{
			.definition = definition,
			.window = window,
			.instance = instance,
			.flowNode = flowNode,
			.firstField = static_cast<std::uint32_t>(snapshot.fields.size()),
		};
		for (const devMode::DevFieldSchema& field :
			snapshot.schema->fieldsOf(element->parametersType)) {
			if (field.operations >= snapshot.schema->fieldOperations.size()) continue;
			const devMode::DevTypeSchema* valueType = snapshot.schema->type(field.valueType);
			const devMode::DevFieldOps* fieldOps =
				snapshot.schema->fieldOperations[field.operations];
			if (!valueType || valueType->capture != devMode::DevCaptureCapability::Value ||
				!fieldOps || !fieldOps->constAddress) continue;
			DevCapturedField capturedField{};
			const std::size_t index = static_cast<std::size_t>(
				std::addressof(field) - snapshot.schema->fields.data());
			capturedField.field = devMode::DevFieldIndex{
				static_cast<std::uint32_t>(index + 1u)};
			if (DevOwnedValue::copyFrom(
				*snapshot.schema,
				field.valueType,
				fieldOps->constAddress(effectiveParameters),
				capturedField.value) != devMode::DevValueOperationStatus::Success) continue;
			capturedField.overridden = overrides.winningLayer(
				definition, window, instance, field.id, capturedField.winningLayer);
			snapshot.fields.push_back(std::move(capturedField));
		}
		captured.fieldCount = static_cast<std::uint32_t>(
			snapshot.fields.size() - captured.firstField);
		capturedElementFieldCount_ += captured.fieldCount;
		snapshot.elements.push_back(captured);
	} catch (...) {}
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

void DevOverrideCapture::captureThemes(
	const ThemeManager& themes,
	devMode::DevSchemaView schema,
	const void* overrideOwner,
	bool (*isOverridden)(
		const void*, devMode::DevTypeId, std::string_view,
		devMode::DevFieldId, DevOverrideLayer&) noexcept,
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
