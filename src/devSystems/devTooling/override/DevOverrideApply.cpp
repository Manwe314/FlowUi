#include "devSystems/devTooling/override/DevOverrideApply.hpp"

#if FLOW_UI_DEV_MODE

#include <algorithm>
#include <utility>

#include "devSystems/devMonitoringAndReporting/timing/DevTimingZone.hpp"

namespace FlowUi::devSystems::tooling {
namespace {

std::size_t mixHash(std::uint64_t left, std::uint64_t right) noexcept {
	left ^= right + 0x9e3779b97f4a7c15ull + (left << 6u) + (left >> 2u);
	return static_cast<std::size_t>(left);
}

bool sameStoredKey(
	const DevOverrideApply::Record& record,
	const DevElementOverrideTarget& target,
	DevOverrideFieldKey field,
	DevOverrideLayer layer) noexcept {
	return record.target == target && record.field == field && record.layer == layer;
}

const devMode::DevFieldSchema* findField(
	const devMode::DevSchemaGeneration& schema,
	devMode::DevTypeIndex owner,
	devMode::DevFieldId id) noexcept {
	for (const devMode::DevFieldSchema& field : schema.fieldsOf(owner)) {
		if (field.id == id) return std::addressof(field);
	}
	return nullptr;
}

} // namespace

std::size_t DevOverrideApply::InstanceKeyHash::operator()(InstanceKey key) const noexcept {
	return mixHash(key.window, key.instance.value);
}

void DevOverrideApply::bindSchema(devMode::DevSchemaView schema) {
	schema_ = std::move(schema);
	if (!schema_) {
		for (Record& record : records_) record.schemaValid = false;
		compiled_.clear();
		return;
	}

	for (Record& record : records_) {
		record.schemaValid = false;
		record.ownerPath.clear();
		const devMode::DevElementSchema* element =
			schema_->findElement(record.target.definition);
		const devMode::DevTypeSchema* owner = schema_->findType(record.field.ownerType);
		if (!element || !owner || element->parametersType.value == 0 ||
			schema_->type(element->parametersType) != owner) continue;
		devMode::DevTypeIndex leafOwner = element->parametersType;
		bool pathValid = true;
		for (const devMode::DevFieldId pathFieldId : record.field.nestedPath) {
			const devMode::DevFieldSchema* pathField = findField(*schema_, leafOwner, pathFieldId);
			if (!pathField || pathField->operations >= schema_->fieldOperations.size()) {
				pathValid = false;
				break;
			}
			if (pathField->effectiveEdit != devMode::DevEditCapability::Editable &&
				pathField->effectiveEdit != devMode::DevEditCapability::PartiallyEditable) {
				pathValid = false;
				break;
			}
			const devMode::DevFieldOps* pathOps =
				schema_->fieldOperations[pathField->operations];
			if (!pathOps || !pathOps->mutableAddress) {
				pathValid = false;
				break;
			}
			record.ownerPath.push_back(pathOps);
			leafOwner = pathField->valueType;
		}
		if (!pathValid) continue;
		if (const devMode::DevFieldSchema* candidate =
			findField(*schema_, leafOwner, record.field.field)) {
			if (candidate->effectiveEdit != devMode::DevEditCapability::Editable) continue;
			const std::size_t index = static_cast<std::size_t>(
				candidate - schema_->fields.data());
			record.fieldIndex = devMode::DevFieldIndex{
				static_cast<std::uint32_t>(index + 1u)};
			const devMode::DevTypeSchema* valueType = schema_->type(candidate->valueType);
			record.schemaValid = valueType && valueType->id == record.value.type();
		}
	}
	rebuildCompiled();
}

void DevOverrideApply::reserveAdditional(std::size_t count) {
	records_.reserve(records_.size() + count);
}

void DevOverrideApply::set(
	const DevElementOverrideTarget& target,
	DevOverrideFieldKey field,
	devMode::DevFieldIndex fieldIndex,
	std::vector<const devMode::DevFieldOps*> ownerPath,
	DevOverrideLayer layer,
	DevOwnedValue value,
	std::uint64_t transaction) {
	std::erase_if(bakeTombstones_, [&](const Record& record) {
		return sameStoredKey(record, target, field, layer);
	});
	const auto found = std::find_if(records_.begin(), records_.end(),
		[&](const Record& record) { return sameStoredKey(record, target, field, layer); });
	if (found != records_.end()) {
		found->target = target;
		found->fieldIndex = fieldIndex;
		found->ownerPath = std::move(ownerPath);
		found->value = std::move(value);
		found->transaction = transaction;
		found->schemaValid = true;
		return;
	}
	records_.push_back(Record{
		.target = target,
		.field = std::move(field),
		.fieldIndex = fieldIndex,
		.layer = layer,
		.value = std::move(value),
		.ownerPath = std::move(ownerPath),
		.transaction = transaction,
		.schemaValid = true,
	});
}

void DevOverrideApply::clear(
	const DevElementOverrideTarget& target,
	DevOverrideFieldKey field,
	DevOverrideLayer layer) noexcept {
	for (const Record& record : records_) {
		if (sameStoredKey(record, target, field, layer)) bakeTombstones_.push_back(record);
	}
	std::erase_if(records_, [&](const Record& record) {
		return sameStoredKey(record, target, field, layer);
	});
}

void DevOverrideApply::resetDefinition(FlowDefinitionID definition) noexcept {
	for (const Record& record : records_) {
		if (record.target.definition == definition &&
			record.target.scope == DevOverrideScope::Definition) {
			bakeTombstones_.push_back(record);
		}
	}
	std::erase_if(records_, [&](const Record& record) {
		return record.target.definition == definition &&
			record.target.scope == DevOverrideScope::Definition;
	});
}

void DevOverrideApply::resetInstance(const DevElementOverrideTarget& target) noexcept {
	for (const Record& record : records_) {
		if (record.target.definition == target.definition &&
			record.target.scope == DevOverrideScope::ExactInstance &&
			record.target.window == target.window && record.target.instance == target.instance) {
			bakeTombstones_.push_back(record);
		}
	}
	std::erase_if(records_, [&](const Record& record) {
		return record.target.definition == target.definition &&
			record.target.scope == DevOverrideScope::ExactInstance &&
			record.target.window == target.window &&
			record.target.instance == target.instance;
	});
}

void DevOverrideApply::clearAll() noexcept {
	bakeTombstones_.insert(bakeTombstones_.end(), records_.begin(), records_.end());
	records_.clear();
	compiled_.clear();
}

void DevOverrideApply::rebuildCompiled() {
	compiled_.clear();
	compiled_.reserve(records_.size());
	for (std::uint32_t index = 0; index < records_.size(); ++index) {
		const Record& record = records_[index];
		if (!record.schemaValid || record.layer == DevOverrideLayer::Count) continue;
		CompiledDefinition& definition = compiled_[record.target.definition.value];
		const std::size_t layer = static_cast<std::size_t>(record.layer);
		if (record.target.scope == DevOverrideScope::Definition) {
			definition.definition[layer].push_back(index);
		} else {
			definition.instances[InstanceKey{
				.window = record.target.window,
				.instance = record.target.instance,
			}][layer].push_back(index);
		}
	}
}

void DevOverrideApply::apply(
	FlowDefinitionID definition,
	WindowId window,
	::FlowUi::detail::element::ElementInstanceKey instance,
	void* draftParameters,
	DevTimingRecorder* timing) const noexcept {
	if (!draftParameters || compiled_.empty()) return;
	const auto found = compiled_.find(definition.value);
	if (found == compiled_.end()) return;
	FLOWUI_DEV_TIMING_ZONE_BALANCED_IF(
		timing, TimingCategory::DevTool, TimingZoneRole::DevToolWork,
		"flowui.dev_override.apply");
	const CompiledDefinition& compiled = found->second;
	const LayerRecords* global = findInstanceLayers(compiled, InvalidWindowId, instance);
	const LayerRecords* local = window == InvalidWindowId
		? nullptr : findInstanceLayers(compiled, window, instance);
	for (std::size_t layer = 0; layer < DevOverrideLayerCount; ++layer) {
		applyLayer(compiled.definition[layer], draftParameters);
		if (global) applyLayer((*global)[layer], draftParameters);
		if (local) applyLayer((*local)[layer], draftParameters);
	}
}

bool DevOverrideApply::winningLayer(
	FlowDefinitionID definition,
	WindowId window,
	::FlowUi::detail::element::ElementInstanceKey instance,
	devMode::DevFieldId field,
	DevOverrideLayer& result) const noexcept {
	const auto found = compiled_.find(definition.value);
	if (found == compiled_.end()) return false;
	const CompiledDefinition& compiled = found->second;
	const LayerRecords* global = findInstanceLayers(compiled, InvalidWindowId, instance);
	const LayerRecords* local = window == InvalidWindowId
		? nullptr : findInstanceLayers(compiled, window, instance);
	bool matched = false;
	for (std::size_t layer = 0; layer < DevOverrideLayerCount; ++layer) {
		const auto inspect = [&](const std::vector<std::uint32_t>& indices) {
			for (const std::uint32_t index : indices) {
				if (index < records_.size() &&
					(records_[index].field.field == field ||
					 (!records_[index].field.nestedPath.empty() &&
					  records_[index].field.nestedPath.front() == field))) {
					result = static_cast<DevOverrideLayer>(layer);
					matched = true;
				}
			}
		};
		inspect(compiled.definition[layer]);
		if (global) inspect((*global)[layer]);
		if (local) inspect((*local)[layer]);
	}
	return matched;
}

std::size_t DevOverrideApply::memoryFootprintBytes() const noexcept {
	std::size_t bytes = records_.capacity() * sizeof(Record) +
		bakeTombstones_.capacity() * sizeof(Record) +
		compiled_.bucket_count() * sizeof(void*);
	for (const Record& record : records_) {
		bytes += record.value.heapBytes() +
			record.field.nestedPath.capacity() * sizeof(devMode::DevFieldId) +
			record.ownerPath.capacity() * sizeof(const devMode::DevFieldOps*);
	}
	for (const Record& record : bakeTombstones_) {
		bytes += record.value.heapBytes() +
			record.field.nestedPath.capacity() * sizeof(devMode::DevFieldId) +
			record.ownerPath.capacity() * sizeof(const devMode::DevFieldOps*);
	}
	for (const auto& definitionEntry : compiled_) {
		const CompiledDefinition& definition = definitionEntry.second;
		bytes += sizeof(definition) + definition.instances.bucket_count() * sizeof(void*);
		for (const auto& indices : definition.definition) {
			bytes += indices.capacity() * sizeof(std::uint32_t);
		}
		for (const auto& instanceEntry : definition.instances) {
			const LayerRecords& layers = instanceEntry.second;
			for (const auto& indices : layers) {
				bytes += indices.capacity() * sizeof(std::uint32_t);
			}
		}
	}
	return bytes;
}

void DevOverrideApply::applyLayer(
	const std::vector<std::uint32_t>& indices,
	void* draftParameters) const noexcept {
	if (!schema_) return;
	for (const std::uint32_t index : indices) {
		if (index >= records_.size()) continue;
		const Record& record = records_[index];
		if (!record.schemaValid || !record.fieldIndex || !record.value) continue;
		const std::uint32_t fieldIndex = record.fieldIndex.value - 1u;
		if (fieldIndex >= schema_->fields.size()) continue;
		const devMode::DevFieldSchema& field = schema_->fields[fieldIndex];
		if (field.operations >= schema_->fieldOperations.size()) continue;
		const devMode::DevFieldOps* operations = schema_->fieldOperations[field.operations];
		if (!operations || !operations->assignMemberFromCopy) continue;
		void* owner = draftParameters;
		for (const devMode::DevFieldOps* path : record.ownerPath) {
			owner = path && path->mutableAddress ? path->mutableAddress(owner) : nullptr;
			if (!owner) break;
		}
		if (owner && operations->assignMemberFromCopy(owner, record.value.data()) ==
			devMode::DevValueOperationStatus::Success) {
			++appliedFieldCount_;
		}
	}
}

const DevOverrideApply::LayerRecords* DevOverrideApply::findInstanceLayers(
	const CompiledDefinition& compiled,
	WindowId window,
	::FlowUi::detail::element::ElementInstanceKey instance) const noexcept {
	const auto found = compiled.instances.find(InstanceKey{
		.window = window,
		.instance = instance,
	});
	return found == compiled.instances.end() ? nullptr : std::addressof(found->second);
}

} // namespace FlowUi::devSystems::tooling

#endif
