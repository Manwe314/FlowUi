#include "devSystems/devTooling/override/DevOverrideEngine.hpp"

#if FLOW_UI_DEV_MODE

#include <algorithm>
#include <cmath>
#include <limits>
#include <utility>

#include "devSystems/devMonitoringAndReporting/timing/DevTimingZone.hpp"
#include "managers/ThemeManager.hpp"

namespace FlowUi::devSystems::tooling {
namespace {

bool isSetCommand(DevOverrideCommandKind kind) noexcept {
	return kind == DevOverrideCommandKind::SetElementField ||
		kind == DevOverrideCommandKind::BeginBatchDrag ||
		kind == DevOverrideCommandKind::UpdateBatchDrag ||
		kind == DevOverrideCommandKind::EndBatchDrag ||
		kind == DevOverrideCommandKind::SetThemeField;
}

bool isElementFieldCommand(DevOverrideCommandKind kind) noexcept {
	return kind == DevOverrideCommandKind::SetElementField ||
		kind == DevOverrideCommandKind::ClearElementField ||
		kind == DevOverrideCommandKind::BeginBatchDrag ||
		kind == DevOverrideCommandKind::UpdateBatchDrag ||
		kind == DevOverrideCommandKind::EndBatchDrag;
}

bool layerMatchesScope(DevOverrideLayer layer, DevOverrideScope scope) noexcept {
	if (layer == DevOverrideLayer::EphemeralPreview) return true;
	if (scope == DevOverrideScope::Definition) {
		return layer == DevOverrideLayer::BakedDefinition ||
			layer == DevOverrideLayer::LiveDefinition;
	}
	return layer == DevOverrideLayer::BakedInstance ||
		layer == DevOverrideLayer::LiveInstance;
}

} // namespace

DevOverrideEngine::DevOverrideEngine(
	devMode::DevSchemaRegistry& schemas,
	DevOverrideEngineConfig config) noexcept
	: schemas_(schemas), config_(config) {}

DevOverrideEngine::~DevOverrideEngine() = default;

bool DevOverrideEngine::submit(DevChangeSet changeSet) {
	std::lock_guard lock(ingressMutex_);
	if (ingress_.size() >= config_.maximumPendingTransactions ||
		changeSet.commands.size() > config_.maximumCommandsPerTransaction ||
		changeSet.commands.size() > config_.maximumPendingCommands -
			std::min<std::size_t>(pendingCommandCount_, config_.maximumPendingCommands)) {
		return false;
	}
	pendingCommandCount_ += changeSet.commands.size();
	ingress_.push_back(std::move(changeSet));
	return true;
}

void DevOverrideEngine::commitAtSafePoint(ThemeManager& themes,
										  DevTimingRecorder* timing) noexcept {
	FLOWUI_DEV_TIMING_ZONE_BALANCED_IF(timing, TimingCategory::DevTool, TimingZoneRole::DevToolWork,
									   "flowui.dev_override.commit");
	try {
		(void)schemas_.publishPendingAtSafePoint();
		syncSchema();
		std::deque<DevChangeSet> pending;
		{
			std::lock_guard lock(ingressMutex_);
			pending.swap(ingress_);
			pendingCommandCount_ = 0;
		}
		results_.clear();
		for (DevChangeSet& transaction : pending) {
			try {
				if (transaction.commands.empty()) {
					results_.push_back({
						.transaction = transaction.transaction,
						.status = DevCommandStatus::EmptyTransaction,
					});
					++rejectedTransactions_;
					continue;
				}
				std::vector<ResolvedField> resolved(transaction.commands.size());
				DevCommandStatus rejected = DevCommandStatus::Applied;
				std::uint32_t rejectedIndex = 0;
				for (std::uint32_t index = 0; index < transaction.commands.size(); ++index) {
					resolved[index] = {};
					const DevCommandStatus status =
						validate(transaction.commands[index], themes, resolved[index]);
					DevCommandStatus preparation_status = status;
					auto& command = transaction.commands[index];
					auto& field = resolved[index];
					if (preparation_status == DevCommandStatus::Applied &&
						command.theme.themeType != 0 && field.schema) {
						const auto prepare_value = [&](DevOwnedValue& value) {
							return !value || stabilize_theme_references(themes, value,
																		field.schema->valueType);
						};
						bool prepared = prepare_value(command.value) &&
										prepare_value(field.originalThemeValue) &&
										prepare_value(field.authoredThemeValue);
						for (auto& original : field.variant_originals)
							prepared = prepared && prepare_value(original.second);
						if (command.kind == DevOverrideCommandKind::ClearThemeField) {
							const auto retained =
								std::find_if(themeRecords_.begin(), themeRecords_.end(),
											 [&](const auto& record) {
												 return record.target == command.theme &&
														record.field == command.field;
											 });
							if (retained != themeRecords_.end()) {
								prepared = prepared && prepare_value(retained->original) &&
										   prepare_value(retained->value) &&
										   prepare_value(retained->preview_value);
								for (auto& original : retained->variant_originals)
									prepared = prepared && prepare_value(original.second);
							}
						}
						if (!prepared)
							preparation_status = DevCommandStatus::ValueCopyFailed;
					}
					if (preparation_status != DevCommandStatus::Applied) {
						rejected = preparation_status;
						rejectedIndex = index;
						break;
					}
				}
				if (rejected != DevCommandStatus::Applied) {
					results_.push_back({
						.transaction = transaction.transaction,
						.command = rejectedIndex,
						.status = rejected,
					});
					++rejectedTransactions_;
					continue;
				}

				std::size_t additionalElementRecords = 0;
				std::size_t additionalThemeRecords = 0;
				for (const DevOverrideCommand& command : transaction.commands) {
					additionalElementRecords +=
						isElementFieldCommand(command.kind) && isSetCommand(command.kind);
					additionalThemeRecords += command.kind == DevOverrideCommandKind::SetThemeField;
				}
				apply_.reserveAdditional(additionalElementRecords);
				themeRecords_.reserve(themeRecords_.size() + additionalThemeRecords);
				for (std::uint32_t index = 0; index < transaction.commands.size(); ++index) {
					applyCommand(std::move(transaction.commands[index]), std::move(resolved[index]),
								 transaction.transaction, themes);
				}
				apply_.rebuildCompiled();
				applyThemeRecords(themes);
				const bool theme_assignment_pending =
					std::ranges::any_of(themeRecords_, [&](const auto& record) {
						return record.transaction == transaction.transaction && record.dirty;
					});
				if (theme_assignment_pending) {
					++rejectedTransactions_;
					results_.push_back({.transaction = transaction.transaction,
										.status = DevCommandStatus::ValueCopyFailed});
					continue;
				}
				results_.push_back({
					.transaction = transaction.transaction,
					.status = DevCommandStatus::Applied,
					.applied = true,
				});
				++committedTransactions_;
			} catch (...) {
				++rejectedTransactions_;
				results_.push_back({.transaction = transaction.transaction,
									.status = DevCommandStatus::InternalFailure});
			}
		}
		applyThemeRecords(themes);
		if (value_collection_enabled_)
			capture_.captureThemes(themes, schema_, this, &themeFieldIsOverridden, timing);
	} catch (...) {
		++rejectedTransactions_;
		results_.push_back({.status = DevCommandStatus::InternalFailure});
	}
}

void DevOverrideEngine::beginWindowFrame(WindowId window, std::uint64_t frameNumber) {
	if (!value_collection_enabled_)
		return;
	syncSchema();
	capture_.beginWindowFrame(window, frameNumber, schema_);
}

void DevOverrideEngine::endWindowFrame(WindowId window) noexcept {
	if (!value_collection_enabled_)
		return;
	capture_.endWindowFrame(window);
}

void DevOverrideEngine::cancelWindowFrame(WindowId window) noexcept {
	capture_.cancelWindowFrame(window);
}

void DevOverrideEngine::applyElement(FlowDefinitionID definition, WindowId window,
									 ::FlowUi::detail::element::ElementInstanceKey instance,
									 void* draftParameters,
									 DevTimingRecorder* timing) const noexcept {
	apply_.apply(definition, window, instance, draftParameters, timing);
}

void DevOverrideEngine::captureElement(
	FlowDefinitionID definition,
	WindowId window,
	::FlowUi::detail::element::ElementInstanceKey instance,
	std::uint32_t flowNode,
	const void* effectiveParameters,
	DevTimingRecorder* timing) noexcept {
	if (!value_collection_enabled_)
		return;
	capture_.captureElement(
		definition, window, instance, flowNode, effectiveParameters, apply_, timing);
}

DevOverrideStats DevOverrideEngine::stats() const noexcept {
	DevOverrideStats result{
		.committedTransactions = committedTransactions_,
		.rejectedTransactions = rejectedTransactions_,
		.appliedElementFields = apply_.appliedFieldCount(),
		.capturedElementFields = capture_.capturedElementFieldCount(),
		.capturedThemeFields = capture_.capturedThemeFieldCount(),
		.activeElementOverrides = static_cast<std::uint32_t>(apply_.records().size()),
		.activeThemeOverrides = static_cast<std::uint32_t>(themeRecords_.size()),
		.memoryFootprintBytes = memoryFootprintBytes(),
	};
	return result;
}

std::size_t DevOverrideEngine::memoryFootprintBytes() const noexcept {
	std::size_t bytes = apply_.memoryFootprintBytes() + capture_.memoryFootprintBytes() +
		themeRecords_.capacity() * sizeof(ThemeBakeRecord) +
		themeBakeTombstones_.capacity() * sizeof(ThemeBakeRecord) +
		results_.capacity() * sizeof(DevCommandResult);
	for (const ThemeBakeRecord& record : themeRecords_) {
		bytes += record.target.variant.capacity() + record.original.heapBytes() +
			record.value.heapBytes() +
			record.field.nestedPath.capacity() * sizeof(devMode::DevFieldId) +
			record.ownerPath.capacity() * sizeof(const devMode::DevFieldOps*);
	}
	for (const ThemeBakeRecord& record : themeBakeTombstones_) {
		bytes += record.target.variant.capacity() + record.original.heapBytes() +
			record.value.heapBytes() +
			record.field.nestedPath.capacity() * sizeof(devMode::DevFieldId) +
			record.ownerPath.capacity() * sizeof(const devMode::DevFieldOps*);
	}
	std::lock_guard lock(ingressMutex_);
	bytes += ingress_.size() * sizeof(DevChangeSet);
	for (const DevChangeSet& changeSet : ingress_) {
		bytes += changeSet.commands.capacity() * sizeof(DevOverrideCommand);
		for (const DevOverrideCommand& command : changeSet.commands) {
			bytes += command.theme.variant.capacity() + command.value.heapBytes() +
				command.field.nestedPath.capacity() * sizeof(devMode::DevFieldId);
		}
	}
	return bytes;
}

void DevOverrideEngine::syncSchema() {
	const devMode::DevSchemaView current = schemas_.view();
	if (schema_ && current && schema_->generation == current->generation) return;
	schema_ = current;
	apply_.bindSchema(schema_);
	bindThemeRecords();
}

DevCommandStatus DevOverrideEngine::validate(
	const DevOverrideCommand& command,
	const ThemeManager& themes,
	ResolvedField& resolved) const noexcept {
	if (!schema_) return DevCommandStatus::SchemaUnavailable;
	if (command.kind == DevOverrideCommandKind::ClearAllElements) {
		return DevCommandStatus::Applied;
	}
	if (command.kind == DevOverrideCommandKind::ResetDefinition) {
		return command.element.definition && schema_->findElement(command.element.definition)
			? DevCommandStatus::Applied : DevCommandStatus::InvalidTarget;
	}
	if (command.kind == DevOverrideCommandKind::ResetInstance) {
		return command.element.definition && command.element.instance &&
			command.element.scope == DevOverrideScope::ExactInstance &&
			schema_->findElement(command.element.definition)
			? DevCommandStatus::Applied : DevCommandStatus::InvalidTarget;
	}

	if (command.kind == DevOverrideCommandKind::SetThemeField ||
		command.kind == DevOverrideCommandKind::ClearThemeField || command.theme.themeType != 0) {
		const devMode::DevThemeSchema* theme = schema_->findTheme(command.theme.themeType);
		if (!theme || command.theme.variant.empty()) return DevCommandStatus::InvalidTarget;
		resolved = resolveRootField(theme->themeType, command.field);
		if (!resolved.schema) return DevCommandStatus::FieldUnavailable;
		if (!themeVariantExists(themes, command.theme)) {
			return DevCommandStatus::ThemeVariantUnavailable;
		}
		if (command.kind == DevOverrideCommandKind::ResetThemeField) {
			if (command.theme.variant == "*" ||
				resolved.schema->effectiveEdit != devMode::DevEditCapability::Editable)
				return DevCommandStatus::InvalidTarget;
			return captureOriginalThemeField(themes, command.theme, resolved,
											 resolved.originalThemeValue, true)
					   ? DevCommandStatus::Applied
					   : DevCommandStatus::ValueCopyFailed;
		}
		if (!isSetCommand(command.kind))
			return DevCommandStatus::Applied;
		const DevCommandStatus valueStatus = validateValue(command, resolved);
		if (valueStatus != DevCommandStatus::Applied) return valueStatus;
		const bool alreadyRetained = std::any_of(
			themeRecords_.begin(), themeRecords_.end(), [&](const ThemeBakeRecord& record) {
				return record.target == command.theme && record.field == command.field;
			});
		if (!alreadyRetained && command.theme.variant != "*" &&
			!captureOriginalThemeField(themes, command.theme, resolved,
									   resolved.originalThemeValue)) {
			return DevCommandStatus::ValueCopyFailed;
		}
		if (!alreadyRetained && command.theme.variant == "*") {
			struct Context {
				std::vector<std::string> variants;
				devMode::DevTypeId type;
			} context{{}, command.theme.themeType};
			const auto visitor = [](void* data,
									const ThemeManager::DevThemePayloadView& view) noexcept {
				auto& context = *static_cast<Context*>(data);
				if (view.type == context.type) {
					try {
						context.variants.emplace_back(view.variant);
					} catch (...) {
						return false;
					}
				}
				return true;
			};
			(void)themes.visitDevThemePayloads(&context, visitor);
			try {
				resolved.variant_originals.reserve(context.variants.size());
				for (const auto& variant : context.variants) {
					DevOwnedValue original;
					if (!captureOriginalThemeField(themes, {command.theme.themeType, variant},
												   resolved, original))
						return DevCommandStatus::ValueCopyFailed;
					resolved.variant_originals.emplace_back(variant, std::move(original));
				}
				if (resolved.variant_originals.empty())
					return DevCommandStatus::ThemeVariantUnavailable;
				resolved.originalThemeValue = resolved.variant_originals.front().second;
			} catch (...) {
				return DevCommandStatus::ValueCopyFailed;
			}
		}
		if (!alreadyRetained && !captureOriginalThemeField(themes, command.theme, resolved,
														   resolved.authoredThemeValue, true))
			return DevCommandStatus::ValueCopyFailed;
		return DevCommandStatus::Applied;
	}

	if (!isElementFieldCommand(command.kind))
		return DevCommandStatus::InvalidTarget;
	const devMode::DevElementSchema* element = schema_->findElement(command.element.definition);
	if (!element) return DevCommandStatus::InvalidTarget;
	if (command.element.scope == DevOverrideScope::ExactInstance && !command.element.instance) {
		return DevCommandStatus::InvalidTarget;
	}
	const DevOverrideLayer effectiveLayer =
		(command.kind == DevOverrideCommandKind::BeginBatchDrag ||
		 command.kind == DevOverrideCommandKind::UpdateBatchDrag)
			? DevOverrideLayer::EphemeralPreview : command.layer;
	if (!layerMatchesScope(effectiveLayer, command.element.scope)) {
		return DevCommandStatus::InvalidTarget;
	}
	if (effectiveLayer == DevOverrideLayer::BakedInstance &&
		!command.element.bakeable) return DevCommandStatus::InvalidTarget;
	resolved = resolveRootField(element->parametersType, command.field);
	if (!resolved.schema) return DevCommandStatus::FieldUnavailable;
	return isSetCommand(command.kind) ? validateValue(command, resolved)
		: DevCommandStatus::Applied;
}

DevCommandStatus DevOverrideEngine::validateValue(const DevOverrideCommand& command,
												  const ResolvedField& resolved) const noexcept {
	if (!resolved.schema || !command.value)
		return DevCommandStatus::TypeMismatch;
	const devMode::DevTypeSchema* type = schema_->type(resolved.schema->valueType);
	const bool action_choice =
		type && type->id == ::FlowUi::detail::typeHash<ActionCall>() &&
		resolved.schema->editor == devMode::DevEditorKind::ActionChoice &&
		resolved.schema->effectiveEdit == devMode::DevEditCapability::SemanticCommand;
	const bool resource_choice =
		type && type->id == ::FlowUi::detail::typeHash<TextureRef>() &&
		resolved.schema->editor == devMode::DevEditorKind::ResourceChoice &&
		resolved.schema->effectiveEdit == devMode::DevEditCapability::PartiallyEditable;
	if (resolved.schema->effectiveEdit != devMode::DevEditCapability::Editable && !action_choice &&
		!resource_choice) {
		return DevCommandStatus::FieldReadOnly;
	}
	if (!type || type->id != command.value.type())
		return DevCommandStatus::TypeMismatch;
	if (action_choice) {
		const auto& action = *static_cast<const ActionCall*>(command.value.data());
		const auto reference = action.stableReference();
		if (action && (!reference.id || (reference.kind != ActionCallKind::App &&
										 reference.kind != ActionCallKind::Ui)))
			return DevCommandStatus::ConstraintRejected;
	}
	if (resource_choice) {
		const auto& texture = *static_cast<const TextureRef*>(command.value.data());
		if ((texture.sourceKey.empty() && texture.handle) ||
			(!texture.sourceKey.empty() && texture.sourceDomain != ResourceDomain::Image &&
			 texture.sourceDomain != ResourceDomain::Icon) ||
			static_cast<unsigned>(texture.fitMode) > static_cast<unsigned>(TextureFitMode::None) ||
			static_cast<unsigned>(texture.samplingMode) >
				static_cast<unsigned>(TextureSamplingMode::Nearest))
			return DevCommandStatus::ConstraintRejected;
	}
	if (resolved.schema->constraint != 0) {
		if (resolved.schema->constraint >= schema_->constraints.size()) {
			return DevCommandStatus::SchemaMismatch;
		}
		const devMode::DevConstraintRecord& constraint =
			schema_->constraints[resolved.schema->constraint];
		if (constraint.hasTextMaximum) {
			if (resolved.schema->valueType.value >= schema_->typeOperations.size()) {
				return DevCommandStatus::SchemaMismatch;
			}
			const devMode::DevTypeOps* operations =
				schema_->typeOperations[resolved.schema->valueType.value];
			if (!operations || !operations->textView ||
				operations->textView(command.value.data()).size() > constraint.textMaximum) {
				return DevCommandStatus::ConstraintRejected;
			}
		}
		if (constraint.numeric.hasMinimum || constraint.numeric.hasMaximum ||
			constraint.numeric.hasStep) {
			const devMode::DevTypeOps* operations =
				resolved.schema->valueType.value < schema_->typeOperations.size()
					? schema_->typeOperations[resolved.schema->valueType.value] : nullptr;
			long double value = 0.0L;
			if (!operations || !operations->numericValue ||
				!operations->numericValue(command.value.data(), value) ||
				!std::isfinite(value)) return DevCommandStatus::ConstraintRejected;
			if (constraint.numeric.hasMinimum &&
				value < static_cast<long double>(constraint.numeric.minimum)) {
				return DevCommandStatus::ConstraintRejected;
			}
			if (constraint.numeric.hasMaximum &&
				value > static_cast<long double>(constraint.numeric.maximum)) {
				return DevCommandStatus::ConstraintRejected;
			}
			if (constraint.numeric.hasStep) {
				const long double step = static_cast<long double>(constraint.numeric.step);
				const long double origin = constraint.numeric.hasMinimum
					? static_cast<long double>(constraint.numeric.minimum) : 0.0L;
				if (!(step > 0.0L) || !std::isfinite(step)) {
					return DevCommandStatus::SchemaMismatch;
				}
				const long double steps = (value - origin) / step;
				const long double nearest = std::round(steps);
				const long double tolerance = 1.0e-9L *
					std::max(1.0L, std::abs(steps));
				if (std::abs(steps - nearest) > tolerance) {
					return DevCommandStatus::ConstraintRejected;
				}
			}
		}
	}
	if (type->kind == devMode::DevTypeKind::Enumeration &&
		type->enumeration.values.count != 0) {
		const devMode::DevTypeOps* operations =
			resolved.schema->valueType.value < schema_->typeOperations.size()
				? schema_->typeOperations[resolved.schema->valueType.value] : nullptr;
		long double value = 0.0L;
		if (!operations || !operations->numericValue ||
			!operations->numericValue(command.value.data(), value)) {
			return DevCommandStatus::ConstraintRejected;
		}
		bool found = false;
		for (std::uint32_t offset = 0; offset < type->enumeration.values.count; ++offset) {
			const std::uint64_t bits = schema_->enumValues[
				type->enumeration.values.first + offset].bits;
			if (value == static_cast<long double>(bits) ||
				(type->enumeration.isSigned &&
				 value == static_cast<long double>(static_cast<std::int64_t>(bits)))) {
				found = true;
				break;
			}
		}
		if (!found) return DevCommandStatus::ConstraintRejected;
	}
	return DevCommandStatus::Applied;
}

DevOverrideEngine::ResolvedField DevOverrideEngine::resolveRootField(
	devMode::DevTypeIndex owner,
	DevOverrideFieldKey key) const noexcept {
	try {
		const devMode::DevTypeSchema* ownerType = schema_ ? schema_->type(owner) : nullptr;
		if (!ownerType || ownerType->id != key.ownerType) return {};
		ResolvedField result{};
		for (const devMode::DevFieldId pathFieldId : key.nestedPath) {
			const devMode::DevFieldSchema* pathField = nullptr;
			for (const devMode::DevFieldSchema& candidate : schema_->fieldsOf(owner)) {
				if (candidate.id == pathFieldId) {
					pathField = std::addressof(candidate);
					break;
				}
			}
			if (!pathField || pathField->operations >= schema_->fieldOperations.size()) return {};
			if (pathField->effectiveEdit != devMode::DevEditCapability::Editable &&
				pathField->effectiveEdit != devMode::DevEditCapability::PartiallyEditable) return {};
			const devMode::DevFieldOps* operations =
				schema_->fieldOperations[pathField->operations];
			if (!operations || !operations->constAddress || !operations->mutableAddress) return {};
			result.ownerPath.push_back(operations);
			owner = pathField->valueType;
		}
		for (const devMode::DevFieldSchema& field : schema_->fieldsOf(owner)) {
			if (field.id != key.field) continue;
			const std::size_t index = static_cast<std::size_t>(
				std::addressof(field) - schema_->fields.data());
			result.schema = std::addressof(field);
			result.index = devMode::DevFieldIndex{static_cast<std::uint32_t>(index + 1u)};
			return result;
		}
		return {};
	} catch (...) {
		return {};
	}
}

void DevOverrideEngine::applyCommand(
	DevOverrideCommand command,
	ResolvedField resolved,
	std::uint64_t transaction,
	ThemeManager& themes) {
	if (command.kind == DevOverrideCommandKind::ResetThemeField) {
		const auto target = command.theme;
		const auto field = command.field;
		command.value = resolved.originalThemeValue;
		resolved.authoredThemeValue = resolved.originalThemeValue;
		command.kind = DevOverrideCommandKind::SetThemeField;
		(void)captureOriginalThemeField(themes, target, resolved, resolved.originalThemeValue);
		applyCommand(std::move(command), std::move(resolved), transaction, themes);
		for (auto& record : themeRecords_)
			if (record.target == target && record.field == field) {
				record.remove_baked = true;
				themeBakeTombstones_.push_back(record);
				break;
			}
		return;
	}
	if (command.theme.themeType != 0 && command.kind != DevOverrideCommandKind::SetThemeField &&
		command.kind != DevOverrideCommandKind::ClearThemeField) {
		const bool preview = command.kind == DevOverrideCommandKind::BeginBatchDrag ||
							 command.kind == DevOverrideCommandKind::UpdateBatchDrag;
		auto found = std::ranges::find_if(themeRecords_, [&](const auto& record) {
			return record.target == command.theme && record.field == command.field;
		});
		if (preview && found != themeRecords_.end()) {
			found->preview_value = std::move(command.value);
			found->dirty = true;
			return;
		}
		command.kind = DevOverrideCommandKind::SetThemeField;
		if (preview) {
			command.layer = DevOverrideLayer::EphemeralPreview;
			DevOwnedValue preview_value = std::move(command.value);
			command.value = resolved.originalThemeValue;
			const auto target = command.theme;
			const auto field = command.field;
			applyCommand(std::move(command), std::move(resolved), transaction, themes);
			found = std::ranges::find_if(themeRecords_, [&](const auto& record) {
				return record.target == target && record.field == field;
			});
			if (found != themeRecords_.end()) {
				found->preview_value = std::move(preview_value);
				found->preview_only = true;
			}
			return;
		}
	}
	switch (command.kind) {
	case DevOverrideCommandKind::SetElementField:
		apply_.set(command.element, command.field, resolved.index,
			std::move(resolved.ownerPath), command.layer,
			std::move(command.value), transaction);
		break;
	case DevOverrideCommandKind::ClearElementField:
		apply_.clear(command.element, command.field, command.layer,
			command.preserve_baked_assignment);
		break;
	case DevOverrideCommandKind::ResetDefinition:
		apply_.resetDefinition(command.element.definition);
		break;
	case DevOverrideCommandKind::ResetInstance:
		apply_.resetInstance(command.element);
		break;
	case DevOverrideCommandKind::ClearAllElements:
		apply_.clearAll();
		break;
	case DevOverrideCommandKind::BeginBatchDrag:
	case DevOverrideCommandKind::UpdateBatchDrag:
		apply_.set(command.element, command.field, resolved.index, std::move(resolved.ownerPath),
			DevOverrideLayer::EphemeralPreview, std::move(command.value), transaction);
		break;
	case DevOverrideCommandKind::EndBatchDrag:
		apply_.clear(command.element, command.field, DevOverrideLayer::EphemeralPreview);
		apply_.set(command.element, command.field, resolved.index,
			std::move(resolved.ownerPath), command.layer,
			std::move(command.value), transaction);
		break;
	case DevOverrideCommandKind::SetThemeField: {
		if (command.layer != DevOverrideLayer::EphemeralPreview)
			std::erase_if(themeBakeTombstones_, [&](const ThemeBakeRecord& record) {
				return record.target == command.theme && record.field == command.field;
			});
		auto found = std::find_if(themeRecords_.begin(), themeRecords_.end(),
			[&](const ThemeBakeRecord& record) {
				return record.target == command.theme && record.field == command.field;
			});
		if (found == themeRecords_.end()) {
			if (!resolved.originalThemeValue) throw std::bad_alloc{};
			themeRecords_.push_back(ThemeBakeRecord{
				.variant_originals = std::move(resolved.variant_originals),
				.original = std::move(resolved.originalThemeValue),
				.authored = std::move(resolved.authoredThemeValue),
				.value = std::move(command.value),
				.target = std::move(command.theme),
				.field = std::move(command.field),
				.ownerPath = std::move(resolved.ownerPath),
				.transaction = transaction,
				.fieldIndex = resolved.index,
				.schemaValid = true,
				.dirty = true,
			});
		} else {
			found->fieldIndex = resolved.index;
			found->ownerPath = std::move(resolved.ownerPath);
			found->value = std::move(command.value);
			found->preview_value.reset();
			found->preview_only = false;
			found->remove_baked = false;
			found->transaction = transaction;
			found->schemaValid = true;
			found->dirty = true;
		}
		break;
	}
	case DevOverrideCommandKind::ClearThemeField: {
		if (command.preserve_baked_assignment)
			std::erase_if(themeBakeTombstones_, [&](const ThemeBakeRecord& record) {
				return record.target == command.theme && record.field == command.field;
			});
		const auto found = std::find_if(
			themeRecords_.begin(), themeRecords_.end(), [&](const ThemeBakeRecord& record) {
				return record.target == command.theme && record.field == command.field;
			});
		if (found != themeRecords_.end() && command.layer == DevOverrideLayer::EphemeralPreview &&
			!found->preview_only) {
			found->preview_value.reset();
			found->dirty = true;
			break;
		}
		if (found != themeRecords_.end() && found->schemaValid && found->fieldIndex) {
			if (!found->preview_only && !command.preserve_baked_assignment)
				themeBakeTombstones_.push_back(*found);
			const std::uint32_t index = found->fieldIndex.value - 1u;
			if (index < schema_->fields.size()) {
				const devMode::DevFieldSchema& field = schema_->fields[index];
				if (field.operations < schema_->fieldOperations.size()) {
					const devMode::DevFieldOps* operations =
						schema_->fieldOperations[field.operations];
					if (operations) {
						if (found->target.variant == "*") {
							for (auto& [variant, original] : found->variant_originals) {
								if (!stabilize_theme_references(themes, original, field.valueType))
									throw std::bad_alloc{};
								(void)themes.assignDevThemeField(found->target.themeType, variant,
																 found->ownerPath, *operations,
																 original.data());
							}
						} else {
							if (!stabilize_theme_references(themes, found->original, field.valueType))
								throw std::bad_alloc{};
							(void)themes.assignDevThemeField(
								found->target.themeType, found->target.variant, found->ownerPath,
								*operations, found->original.data());
						}
					}
				}
			}
			themeRecords_.erase(found);
			for (auto& record : themeRecords_)
				record.dirty = true;
		}
		break;
	}
	}
}

void DevOverrideEngine::bindThemeRecords() {
	if (!schema_) {
	for (ThemeBakeRecord& record : themeRecords_) record.schemaValid = false;
		return;
	}
	for (ThemeBakeRecord& record : themeRecords_) {
		record.schemaValid = false;
		const devMode::DevThemeSchema* theme = schema_->findTheme(record.target.themeType);
		if (!theme) continue;
		const ResolvedField resolved = resolveRootField(theme->themeType, record.field);
		if (!resolved.schema) continue;
		const devMode::DevTypeSchema* valueType = schema_->type(resolved.schema->valueType);
		if (!valueType || valueType->id != record.value.type() ||
			valueType->id != record.original.type()) continue;
		record.fieldIndex = resolved.index;
		record.ownerPath = resolved.ownerPath;
		record.schemaValid = true;
	}
}

void DevOverrideEngine::applyThemeRecords(ThemeManager& themes) noexcept {
	if (!schema_) return;
	// Newly registered or cloned variants inherit retained general edits at this safe point.
	for (auto& record : themeRecords_) {
		if (record.target.variant != "*" || !record.schemaValid || !record.fieldIndex)
			continue;
		struct Context {
			ThemeBakeRecord* record;
			const devMode::DevSchemaGeneration* schema;
		} context{&record, schema_.operator->()};
		const auto visitor = [](void* data,
								const ThemeManager::DevThemePayloadView& view) noexcept {
			auto& context = *static_cast<Context*>(data);
			auto& record = *context.record;
			if (view.type != record.target.themeType ||
				std::ranges::any_of(record.variant_originals, [&](const auto& original) {
					return original.first == view.variant;
				}))
				return true;
			try {
				const auto& field = context.schema->fields[record.fieldIndex.value - 1];
				const auto* operations = context.schema->fieldOperations[field.operations];
				const void* owner = view.startup;
				for (const auto* path : record.ownerPath)
					owner = owner && path->constAddress ? path->constAddress(owner) : nullptr;
				if (!owner || !operations || !operations->constAddress)
					return false;
				DevOwnedValue original;
				if (DevOwnedValue::copyFrom(*context.schema, field.valueType,
											operations->constAddress(owner),
											original) != devMode::DevValueOperationStatus::Success)
					return false;
				record.variant_originals.emplace_back(std::string(view.variant),
													  std::move(original));
				record.dirty = true;
				return true;
			} catch (...) {
				return false;
			}
		};
		(void)themes.visitDevThemePayloads(&context, visitor);
	}
	if (std::ranges::none_of(themeRecords_, [](const auto& record) { return record.dirty; }))
		return;
	// General tokens form the base; specific variants always win regardless of edit order.
	for (bool general : {true, false})
		for (ThemeBakeRecord& record : themeRecords_) {
			if ((record.target.variant == "*") != general)
				continue;
			if (!record.schemaValid || !record.fieldIndex || !record.value)
				continue;
			const std::uint32_t index = record.fieldIndex.value - 1u;
			if (index >= schema_->fields.size())
				continue;
			const devMode::DevFieldSchema& field = schema_->fields[index];
			if (field.operations >= schema_->fieldOperations.size())
				continue;
			const devMode::DevFieldOps* operations = schema_->fieldOperations[field.operations];
			if (!operations)
				continue;
			auto& assigned_value = record.preview_value ? record.preview_value : record.value;
			if (!stabilize_theme_references(themes, assigned_value, field.valueType))
				continue;
			if (themes.assignDevThemeField(
					record.target.themeType, record.target.variant, record.ownerPath, *operations,
					assigned_value.data()) ==
				devMode::DevValueOperationStatus::Success) {
				record.dirty = false;
			}
		}
}

bool DevOverrideEngine::stabilize_theme_references(ThemeManager& themes, DevOwnedValue& value,
	devMode::DevTypeIndex type) const noexcept {
	if (!schema_ || !value)
		return false;
	const auto stabilize = [&](auto&& self, devMode::DevTypeIndex current_type,
		void* destination, uint32_t depth) -> bool {
		const auto* descriptor = schema_->type(current_type);
		if (!descriptor || !destination || depth > 16)
			return false;
		if (descriptor->id == ::FlowUi::detail::typeHash<TextureRef>()) {
			auto& texture = *static_cast<TextureRef*>(destination);
			if (texture.sourceKey.empty())
				return true;
			const auto retained = themes.retain_dev_resource_key(texture.sourceKey);
			if (!retained)
				return false;
			texture.sourceKey = *retained;
			return true;
		}
		if (descriptor->kind == devMode::DevTypeKind::Object) {
			for (const auto& member : schema_->fieldsOf(current_type)) {
				if (member.operations >= schema_->fieldOperations.size())
					return false;
				const auto* access = schema_->fieldOperations[member.operations];
				if (access && access->mutableAddress &&
					!self(self, member.valueType, access->mutableAddress(destination), depth + 1))
					return false;
			}
		} else if (descriptor->kind == devMode::DevTypeKind::Optional ||
			descriptor->kind == devMode::DevTypeKind::Sequence) {
			if (current_type.value >= schema_->typeOperations.size())
				return false;
			const auto* access = schema_->typeOperations[current_type.value];
			if (!access)
				return false;
			if (descriptor->kind == devMode::DevTypeKind::Optional) {
				if (access->optionalHasValue && access->optionalHasValue(destination))
					return access->optionalMutableValueAddress && self(self, descriptor->elementType,
						access->optionalMutableValueAddress(destination), depth + 1);
			} else if (access->sequenceSize && access->sequenceMutableElementAddress) {
				const auto count = access->sequenceSize(destination);
				for (std::size_t index = 0; index < count; ++index)
					if (!self(self, descriptor->elementType,
						access->sequenceMutableElementAddress(destination, index), depth + 1))
						return false;
			}
		}
		return true;
	};
	return stabilize(stabilize, type, value.data(), 0);
}

bool DevOverrideEngine::themeVariantExists(
	const ThemeManager& themes,
	const DevThemeOverrideTarget& target) const noexcept {
	struct Context { const DevThemeOverrideTarget* target; bool found = false; } context{&target};
	const auto visitor = [](void* userData, const ThemeManager::DevThemePayloadView& view) noexcept {
		auto& state = *static_cast<Context*>(userData);
		if (view.type == state.target->themeType &&
			(state.target->variant == "*" || view.variant == state.target->variant)) {
			state.found = true;
			return false;
		}
		return true;
	};
	(void)themes.visitDevThemePayloads(&context, visitor);
	return context.found;
}

bool DevOverrideEngine::captureOriginalThemeField(const ThemeManager& themes,
												  const DevThemeOverrideTarget& target,
												  const ResolvedField& field,
												  DevOwnedValue& destination,
												  bool authored) const noexcept {
	struct Context {
		const DevThemeOverrideTarget* target = nullptr;
		const ResolvedField* field = nullptr;
		const devMode::DevSchemaGeneration* schema = nullptr;
		DevOwnedValue* destination = nullptr;
		bool copied = false;
		bool authored = false;
	} context{&target, &field, schema_.operator->(), &destination, false, authored};
	const auto visitor = [](void* userData, const ThemeManager::DevThemePayloadView& view) noexcept {
		auto& state = *static_cast<Context*>(userData);
		if (view.type != state.target->themeType ||
			(state.target->variant == "*" ? !view.active : view.variant != state.target->variant)) {
			return true;
		}
		const devMode::DevFieldSchema& schemaField = *state.field->schema;
		if (schemaField.operations >= state.schema->fieldOperations.size()) return false;
		const devMode::DevFieldOps* operations =
			state.schema->fieldOperations[schemaField.operations];
		if (!operations || !operations->constAddress) return false;
		const void* owner =
			state.authored ? view.authored : (view.startup ? view.startup : view.payload);
		if (!owner)
			return false;
		for (const devMode::DevFieldOps* path : state.field->ownerPath) {
			owner = path && path->constAddress ? path->constAddress(owner) : nullptr;
			if (!owner) return false;
		}
		state.copied = DevOwnedValue::copyFrom(
			*state.schema, schemaField.valueType,
			operations->constAddress(owner), *state.destination) ==
			devMode::DevValueOperationStatus::Success;
		return false;
	};
	(void)themes.visitDevThemePayloads(&context, visitor);
	return context.copied;
}

bool DevOverrideEngine::themeFieldIsOverridden(
	const void* owner,
	devMode::DevTypeId themeType,
	std::string_view variant,
	devMode::DevFieldId field,
	DevOverrideLayer& layer) noexcept {
	const auto& engine = *static_cast<const DevOverrideEngine*>(owner);
	for (const ThemeBakeRecord& record : engine.themeRecords_) {
		if (record.schemaValid && record.target.themeType == themeType &&
			(record.target.variant == variant || record.target.variant == "*") &&
			(record.field.field == field ||
			 (!record.field.nestedPath.empty() && record.field.nestedPath.front() == field))) {
			layer = DevOverrideLayer::LiveDefinition;
			return true;
		}
	}
	return false;
}

} // namespace FlowUi::devSystems::tooling

#endif
