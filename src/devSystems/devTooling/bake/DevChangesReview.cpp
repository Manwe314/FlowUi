#include "devSystems/devTooling/bake/DevChangesReview.hpp"

#if FLOW_UI_DEV_MODE

#include <algorithm>
#include <charconv>
#include <cmath>
#include <fstream>
#include <sstream>
#include <system_error>
#include <tuple>

#include "devSystems/devTooling/bake/DevBakeInternals.hpp"
#include "devSystems/devTooling/bake/DevBakeValidation.hpp"
#include "devSystems/devTooling/bake/DevBakedChanges.hpp"
#include "devSystems/devTooling/schema/DevSchemaRegistry.hpp"
#include "managers/structs/ActionManagerStructs.hpp"

namespace FlowUi::devSystems::tooling {

namespace {

[[nodiscard]] bool live_layer(DevOverrideLayer layer) noexcept {
	return layer == DevOverrideLayer::LiveDefinition || layer == DevOverrideLayer::LiveInstance;
}

[[nodiscard]] bool equal_entry_values(const DevBakeEntry& left, const DevBakeEntry& right) {
	return bake_detail::entry_values_equal(left, right);
}

/** Policy comparisons use the same lossless float spelling as the manifest codec. */
[[nodiscard]] std::string growth_factor_text(float factor) {
	char characters[64]{};
	const auto written = std::to_chars(characters, characters + sizeof(characters), factor);
	return written.ec == std::errc{} ? std::string(characters, written.ptr)
									 : std::string("Unrepresentable growth factor");
}

[[nodiscard]] std::string
memory_selection_json(const FlowUi::detail::MemoryCapacitySelection& selection) {
	if (!FlowUi::detail::find_initial_capacity(FlowUi::memory_capacity_target_id(selection.key)))
		return selection.key + ":" + std::to_string(selection.value) + ":" + selection.policy;
	FlowUi::detail::MemoryCapacityManifest section;
	section.targets.emplace_back(selection);
	return FlowUi::detail::encode_memory_capacity_manifest(section);
}

[[nodiscard]] DevChangesDecision decision_for(const DevChangesReviewRequest& request,
											  std::string_view key) noexcept {
	for (auto decision = request.decisions.rbegin(); decision != request.decisions.rend();
		 ++decision)
		if (decision->key == key)
			return decision->decision;
	return DevChangesDecision::IncludeProposal;
}

[[nodiscard]] bool decision_resolution_reviewed(const DevChangesReviewPlan& plan,
												std::string_view key) noexcept {
	for (auto decision = plan.request.decisions.rbegin(); decision != plan.request.decisions.rend();
		 ++decision) {
		if (decision->key == key)
			return (decision->decision == DevChangesDecision::IncludeProposal ||
					decision->decision == DevChangesDecision::Remove) &&
				   decision->disk_revision == plan.disk_revision;
	}
	return false;
}

void reconcile_external_entries(DevChangesReviewPlan& plan) {
	if (!plan.request.opened_manifest)
		return;
	for (auto& row : plan.rows) {
		const bool removal_intent =
			row.decision == DevChangesDecision::Remove ||
			(row.decision == DevChangesDecision::IncludeProposal && row.proposal_removes);
		const bool proposal_intent =
			row.has_proposal && row.decision == DevChangesDecision::IncludeProposal;
		if ((!removal_intent && !proposal_intent) || row.domain == DevChangesDomain::Memory ||
			decision_resolution_reviewed(plan, row.key))
			continue;
		const auto& opened_entries = plan.request.opened_manifest->entries;
		const auto opened =
			std::find_if(opened_entries.begin(), opened_entries.end(),
						 [&](const auto& entry) { return changes_entry_key(entry) == row.key; });
		const bool had_opened = opened != opened_entries.end();
		const bool disk_changed =
			had_opened != row.has_disk ||
			(had_opened && row.disk_entry && !equal_entry_values(*opened, *row.disk_entry));
		const bool same_output = removal_intent
									 ? !row.has_disk
									 : row.disk_entry && row.proposed_entry &&
										   equal_entry_values(*row.disk_entry, *row.proposed_entry);
		if (disk_changed && !same_output) {
			row.validity = DevChangesValidity::Conflict;
			row.reason =
				"This destination changed on disk since review, including a possible deletion. " +
				std::string(removal_intent
								? "Confirm removal against this disk head, or keep the saved value."
								: "Explicitly include the proposal against this disk head, or keep "
								  "the saved value.");
		}
	}
}

[[nodiscard]] DevChangesReviewRow& find_or_add_row(DevChangesReviewPlan& plan, std::string key,
												   DevChangesDomain domain) {
	const auto found = std::find_if(plan.rows.begin(), plan.rows.end(),
									[&](const auto& row) { return row.key == key; });
	if (found != plan.rows.end())
		return *found;
	plan.rows.emplace_back();
	auto& row = plan.rows.back();
	row.key = std::move(key);
	row.domain = domain;
	row.decision = decision_for(plan.request, row.key);
	return row;
}

[[nodiscard]] DevChangesDomain entry_domain(const DevBakeEntry& entry) noexcept {
	return entry.targetKind == DevBakeTargetKind::Theme ? DevChangesDomain::Themes
														: DevChangesDomain::Elements;
}

void describe_entry(DevChangesReviewRow& row, const DevBakeEntry& entry,
					const devMode::DevSchemaGeneration* schema) {
	row.field_path = entry.fieldPath;
	row.target_name = entry.ownerCppType;
	if (schema && entry.targetKind == DevBakeTargetKind::Element) {
		if (const auto* element = schema->findElement(entry.definition))
			row.target_name = schema->string(element->displayName);
	} else if (schema && entry.targetKind == DevBakeTargetKind::Theme) {
		if (const auto* theme = schema->findTheme(entry.themeType))
			row.target_name = schema->string(theme->displayName);
	}
	if (row.target_name.empty())
		row.target_name =
			(entry.targetKind == DevBakeTargetKind::Theme ? "Theme " : "Element ") +
			std::to_string(entry.targetKind == DevBakeTargetKind::Theme ? entry.themeType
																		: entry.definition.value);
}

[[nodiscard]] bool source_header_path(std::string_view path) {
	const auto extension = std::filesystem::path(path).extension();
	return extension == ".h" || extension == ".hpp" || extension == ".hh" || extension == ".hxx" ||
		   extension == ".inl";
}

/** Resolve the legacy path and leaf identity together; names alone never imply a migration. */
[[nodiscard]] const devMode::DevFieldSchema*
resolve_saved_field(const devMode::DevSchemaGeneration& schema, DevBakeEntry& entry,
					std::string& reason) {
	devMode::DevTypeIndex owner_index{};
	if (entry.targetKind == DevBakeTargetKind::Element) {
		const auto* element = schema.findElement(entry.definition);
		if (!element) {
			reason = "Saved element is not registered in this schema; retain it for repair or "
					 "explicitly remove it.";
			return nullptr;
		}
		owner_index = element->parametersType;
	} else {
		const auto* theme = schema.findTheme(entry.themeType);
		if (!theme) {
			reason = "Saved theme is not registered in this schema; retain it for repair or "
					 "explicitly remove it.";
			return nullptr;
		}
		owner_index = theme->themeType;
	}
	const auto* root_type = schema.type(owner_index);
	if (!root_type) {
		reason = "Saved target has no parameter schema.";
		return nullptr;
	}
	const devMode::DevFieldSchema* leaf = nullptr;
	std::size_t path_start = 0;
	std::string source_header;
	while (path_start < entry.fieldPath.size()) {
		const auto path_end = entry.fieldPath.find('.', path_start);
		const auto component = std::string_view(entry.fieldPath)
								   .substr(path_start, path_end == std::string::npos
														   ? entry.fieldPath.size() - path_start
														   : path_end - path_start);
		leaf = nullptr;
		for (const auto& field : schema.fieldsOf(owner_index)) {
			if (schema.string(field.name) == component) {
				leaf = &field;
				break;
			}
		}
		if (!leaf) {
			reason =
				"Saved field path no longer resolves; choose an explicit replacement or removal.";
			return nullptr;
		}
		if (source_header.empty())
			source_header = schema.string(leaf->source.file);
		if (path_end == std::string::npos)
			break;
		owner_index = leaf->valueType;
		path_start = path_end + 1;
	}
	const auto* value_type = leaf ? schema.type(leaf->valueType) : nullptr;
	if (!leaf || leaf->id != entry.fieldId) {
		reason = "Saved nested field identity changed; no rename was inferred.";
		return nullptr;
	}
	if (!value_type || value_type->id != entry.valueType) {
		reason = "Saved field type differs from the current schema.";
		return nullptr;
	}
	const bool stable_action_field =
		value_type->id == ::FlowUi::detail::typeHash<ActionCall>() &&
		leaf->editor == devMode::DevEditorKind::ActionChoice &&
		leaf->effectiveEdit == devMode::DevEditCapability::SemanticCommand;
	if (leaf->effectiveEdit != devMode::DevEditCapability::Editable &&
		leaf->effectiveEdit != devMode::DevEditCapability::PartiallyEditable &&
		!stable_action_field) {
		reason = "Saved field is no longer writable in the current schema.";
		return nullptr;
	}
	entry.ownerCppType = schema.string(root_type->cppTypeName);
	entry.valueTypeName = schema.string(value_type->cppTypeName);
	if (source_header_path(source_header))
		entry.sourceHeader = std::move(source_header);
	return leaf;
}

[[nodiscard]] bool generator_ready(const DevBakeEntry& entry, std::string& reason) {
	if ((entry.targetKind == DevBakeTargetKind::Element &&
		 (!entry.definition ||
		  (entry.targetScope == DevOverrideScope::ExactInstance && !entry.instanceKey))) ||
		(entry.targetKind == DevBakeTargetKind::Theme &&
		 (entry.themeType == 0 || entry.themeVariant.empty()))) {
		reason = "The persisted target identity or variant is incomplete and cannot be generated.";
		return false;
	}
	if (!generator_entry_metadata_valid(entry.ownerCppType, entry.fieldPath, entry.sourceHeader,
										entry.cppValue)) {
		reason = "Assignment metadata or initializer is not accepted by the build generator.";
		return false;
	}
	if (!source_header_path(entry.sourceHeader)) {
		reason = "The reflected declaration must be in a header usable by the next build.";
		return false;
	}
	return true;
}

[[nodiscard]] std::string field_path_for(const devMode::DevSchemaGeneration* schema,
										 devMode::DevTypeIndex owner_index,
										 const DevOverrideFieldKey& key) {
	std::string path;
	const auto unresolved = [&] {
		std::string identity = "unresolved." + std::to_string(key.ownerType);
		for (const auto field_id : key.nestedPath)
			identity += "." + std::to_string(field_id);
		return identity + "." + std::to_string(key.field);
	};
	if (schema) {
		const auto append = [&](devMode::DevFieldId field_id) {
			for (const auto& field : schema->fieldsOf(owner_index)) {
				if (field.id != field_id)
					continue;
				if (!path.empty())
					path += '.';
				path += schema->string(field.name);
				owner_index = field.valueType;
				return true;
			}
			return false;
		};
		for (const auto field_id : key.nestedPath)
			if (!append(field_id))
				return unresolved();
		if (append(key.field))
			return path;
	}
	return unresolved();
}

void add_proposal(DevChangesReviewPlan& plan, DevBakeEntry entry, bool removes,
				  const devMode::DevSchemaGeneration* schema, std::string reason,
				  std::string origin) {
	auto& row = find_or_add_row(plan, changes_entry_key(entry), entry_domain(entry));
	describe_entry(row, entry, schema);
	if (row.has_proposal) {
		if (row.proposal_removes != removes ||
			(!removes && row.proposed_entry && !equal_entry_values(*row.proposed_entry, entry))) {
			row.validity = DevChangesValidity::Conflict;
			row.reason = "Different live values resolve to the same persisted destination. Choose "
						 "the saved value, omit them, or edit the sources to agree.";
			row.proposal += " | " + (removes ? std::string("Remove assignment") : entry.valueJson);
		}
		if (row.origin.find(origin) == std::string::npos)
			row.origin += "; " + origin;
		return;
	}
	row.has_proposal = true;
	row.proposal_removes = removes;
	row.proposal = removes ? "Remove assignment" : entry.valueJson;
	row.live_value = removes ? std::string{} : entry.valueJson;
	row.proposed_entry = std::move(entry);
	row.origin = std::move(origin);
	if (!reason.empty()) {
		row.validity = DevChangesValidity::Invalid;
		row.reason = std::move(reason);
	}
}

[[nodiscard]] DevBakeEntry compiled_entry(const FlowUi::baked::CompiledChangeEntry& compiled) {
	return {.targetKind = compiled.theme ? DevBakeTargetKind::Theme : DevBakeTargetKind::Element,
			.targetScope = compiled.exact_instance ? DevOverrideScope::ExactInstance
												   : DevOverrideScope::Definition,
			.definition = FlowDefinitionID{compiled.definition_id},
			.themeType = compiled.theme_type,
			.themeVariant = std::string(compiled.theme_variant),
			.instanceKey = ::FlowUi::detail::element::ElementInstanceKey{compiled.instance_key},
			.instanceDebugLabel = std::string(compiled.instance_debug_label),
			.fieldId = compiled.field_id,
			.fieldPath = std::string(compiled.field_path),
			.valueType = compiled.value_type,
			.ownerCppType = std::string(compiled.owner_cpp_type),
			.sourceHeader = std::string(compiled.source_header),
			.valueJson = std::string(compiled.value_json),
			.cppValue = std::string(compiled.cpp_value)};
}

void count_row(DevChangesReviewPlan& plan, const DevChangesReviewRow& row) {
	++plan.counts.domains[static_cast<std::size_t>(row.domain)];
	switch (row.operation) {
	case DevChangesOperation::Add:
		++plan.counts.additions;
		break;
	case DevChangesOperation::Update:
		++plan.counts.updates;
		break;
	case DevChangesOperation::Remove:
		++plan.counts.removals;
		break;
	case DevChangesOperation::Keep:
		if (row.has_disk)
			++plan.counts.retained;
		break;
	case DevChangesOperation::NoChange:
		if (row.has_disk)
			++plan.counts.retained;
		break;
	}
	if (row.has_proposal && (row.decision == DevChangesDecision::OmitProposal ||
							 row.decision == DevChangesDecision::RetainDisk))
		++plan.counts.omitted;
	if (row.blocking && row.validity == DevChangesValidity::Conflict)
		++plan.counts.conflicts;
	else if (row.blocking)
		++plan.counts.errors;
	else if (row.validity != DevChangesValidity::Ready)
		++plan.counts.warnings;
}

/** Revalidation compares values and metadata without the proposal's review-time timestamp. */
[[nodiscard]] std::string candidate_signature(DevBakeManifest candidate) {
	for (auto& entry : candidate.entries)
		entry.provenance.timestamp.clear();
	candidate.createdTimestamp.clear();
	return bake_detail::encode_manifest(candidate);
}

} // namespace

std::string changes_entry_key(const DevBakeEntry& entry) noexcept {
	try {
		const auto component = [](std::string_view value) {
			return std::to_string(value.size()) + ":" + std::string(value);
		};
		return (entry.targetKind == DevBakeTargetKind::Theme
					? "theme:" + std::to_string(entry.themeType) + ":" +
						  component(entry.themeVariant)
					: "element:" + std::to_string(entry.definition.value) + ":" +
						  (entry.targetScope == DevOverrideScope::ExactInstance
							   ? "instance:" + std::to_string(entry.instanceKey.value)
							   : "definition")) +
			   ":" + std::to_string(entry.fieldId) + ":" + component(entry.fieldPath);
	} catch (...) {
		return {};
	}
}

std::string changes_memory_key(std::string_view key) noexcept {
	try {
		return "memory:" + std::string(key);
	} catch (...) {
		return {};
	}
}

std::string_view changes_operation_name(DevChangesOperation operation) noexcept {
	switch (operation) {
	case DevChangesOperation::Keep:
		return "Saved";
	case DevChangesOperation::Add:
		return "Add";
	case DevChangesOperation::Update:
		return "Update";
	case DevChangesOperation::Remove:
		return "Remove";
	case DevChangesOperation::NoChange:
		return "Synchronized";
	}
	return "Unknown";
}

std::string_view changes_validity_name(DevChangesValidity validity) noexcept {
	switch (validity) {
	case DevChangesValidity::Ready:
		return "Ready";
	case DevChangesValidity::Unavailable:
		return "Unavailable now";
	case DevChangesValidity::Unresolved:
		return "Unresolved";
	case DevChangesValidity::Conflict:
		return "Conflict";
	case DevChangesValidity::Invalid:
		return "Invalid";
	}
	return "Unknown";
}

DevChangesReviewPlan
DevBakePipeline::query_review(const DevChangesReviewRequest& request) const noexcept {
	DevChangesReviewPlan plan;
	try {
		plan.request = request;
		plan.manifest_path = manifestPath_;
		plan.memory_revision = request.memory_revision;
		const auto schema_view = schemas_.view();
		const auto* schema = schema_view ? std::addressof(*schema_view) : nullptr;
		plan.schema_generation = schema ? schema->generation : 0;
		bool existed = false;
		std::string disk_contents;
		std::string read_diagnostic;
		DevBakeManifest disk;
		if (!bake_detail::read_manifest_snapshot(manifestPath_, disk, disk_contents, existed,
												 &read_diagnostic)) {
			plan.error = "Cannot read a valid active manifest: " + manifestPath_.string() + ". " +
						 read_diagnostic +
						 " Repair the file and refresh; pending work has been retained.";
			plan.candidate = manifest_;
			plan.counts.errors = 1;
			return plan;
		}
		plan.valid_disk = true;
		plan.disk_manifest = disk;
		plan.disk_revision =
			existed ? std::to_string(manifest_content_digest(disk_contents)) : "missing";
		plan.rebuild_required = existed ? FlowUi::baked::compiled_manifest_digest() !=
											  manifest_content_digest(disk_contents)
										: FlowUi::baked::compiled_manifest_digest() != 0;
		plan.candidate = disk;
		plan.candidate.entries.clear();
		plan.rows.reserve(disk.entries.size() + overrides_.appliedOverrides().records().size() +
						  overrides_.themeBakeRecords().size() +
						  (request.prepared_memory ? request.prepared_memory->targets.size() : 0) +
						  8);
		for (const auto& entry : disk.entries) {
			auto& row = find_or_add_row(plan, changes_entry_key(entry), entry_domain(entry));
			if (row.has_disk) {
				row.validity = DevChangesValidity::Conflict;
				row.reason = "The active manifest contains duplicate assignments to this "
							 "destination; explicitly remove or replace them.";
				continue;
			}
			row.has_disk = true;
			row.disk_entry = entry;
			row.on_disk = entry.valueJson;
			row.origin = "Saved manifest";
			describe_entry(row, entry, schema);
		}
		std::string proposal_identity;
		const auto add_element_record = [&](const DevOverrideApply::Record& record, bool removes) {
			if (!live_layer(record.layer))
				return;
			if (!removes)
				++plan.counts.live_edits;
			DevBakeDiagnostic diagnostic;
			auto entry =
				schema ? bake_detail::element_proposal(*schema, record, diagnostic) : std::nullopt;
			if (!entry) {
				devMode::DevTypeIndex root_index{};
				if (schema)
					if (const auto* element = schema->findElement(record.target.definition))
						root_index = element->parametersType;
				entry = DevBakeEntry{.targetScope = record.target.scope,
									 .definition = record.target.definition,
									 .instanceKey = record.target.instance,
									 .instanceDebugLabel = record.target.instanceDebugLabel,
									 .fieldId = record.field.field,
									 .fieldPath = field_path_for(schema, root_index, record.field),
									 .valueType = record.value.type()};
				if (diagnostic.message.empty())
					diagnostic.message =
						"Live value cannot be resolved or serialized against the current schema.";
			}
			proposal_identity +=
				changes_entry_key(*entry) + ":" + std::to_string(record.transaction) + ":" +
				std::to_string(record.target.window) + ":" + entry->valueJson + ":" +
				std::to_string(removes) + ":" + std::to_string(record.schemaValid) + ";";
			add_proposal(plan, std::move(*entry), removes, schema,
						 removes ? std::string{} : diagnostic.message,
						 "Live element · window " + std::to_string(record.target.window) +
							 " · transaction " + std::to_string(record.transaction));
		};
		for (const auto& record : overrides_.appliedOverrides().records())
			add_element_record(record, false);
		for (const auto& record : overrides_.appliedOverrides().bakeTombstones())
			add_element_record(record, true);
		const auto add_theme_record = [&](const DevOverrideEngine::ThemeBakeRecord& record,
										  bool removes) {
			if (record.preview_only)
				return;
			removes = removes || record.remove_baked;
			if (!removes)
				++plan.counts.live_edits;
			DevBakeDiagnostic diagnostic;
			auto entry =
				schema ? bake_detail::theme_proposal(*schema, record, diagnostic) : std::nullopt;
			if (!entry) {
				devMode::DevTypeIndex root_index{};
				if (schema)
					if (const auto* theme = schema->findTheme(record.target.themeType))
						root_index = theme->themeType;
				entry = DevBakeEntry{.targetKind = DevBakeTargetKind::Theme,
									 .themeType = record.target.themeType,
									 .themeVariant = record.target.variant,
									 .fieldId = record.field.field,
									 .fieldPath = field_path_for(schema, root_index, record.field),
									 .valueType = record.value.type()};
				if (diagnostic.message.empty())
					diagnostic.message = "Live theme value cannot be resolved or serialized "
										 "against the current schema.";
			}
			proposal_identity += changes_entry_key(*entry) + ":" +
								 std::to_string(record.transaction) + ":" + entry->valueJson + ":" +
								 std::to_string(removes) + ":" +
								 std::to_string(record.schemaValid) + ";";
			const auto key = changes_entry_key(*entry);
			add_proposal(plan, std::move(*entry), removes, schema,
						 removes ? std::string{} : diagnostic.message,
						 "Live theme · transaction " + std::to_string(record.transaction));
			if (record.authored && schema) {
				const auto* type = schema->findType(record.authored.type());
				if (type) {
					const auto serialized =
						serialize_dev_value(*schema,
											devMode::DevTypeIndex{static_cast<std::uint32_t>(
												type - schema->types.data())},
											record.authored.data());
					if (serialized)
						find_or_add_row(plan, key, DevChangesDomain::Themes).authored_value =
							serialized->json;
				}
			}
		};
		for (const auto& record : overrides_.themeBakeRecords())
			add_theme_record(record, false);
		for (const auto& record : overrides_.themeBakeTombstones())
			add_theme_record(record, true);
		plan.proposal_revision = manifest_content_digest(proposal_identity);
		for (const auto& compiled : FlowUi::baked::compiled_change_entries()) {
			const auto entry = compiled_entry(compiled);
			auto& row = find_or_add_row(plan, changes_entry_key(entry), entry_domain(entry));
			if (row.target_name.empty())
				describe_entry(row, entry, schema);
			row.compiled_value = entry.valueJson.empty() ? entry.cppValue : entry.valueJson;
			if (!row.has_disk && !row.has_proposal) {
				row.origin = "Compiled executable";
				row.reason =
					"Removed on disk; this compiled assignment remains active until rebuild.";
				row.operation = DevChangesOperation::NoChange;
			}
		}
		reconcile_external_entries(plan);
		// Element/theme rows are materialized from exactly the decision shown in the UI.
		for (auto& row : plan.rows) {
			const bool remove =
				row.decision == DevChangesDecision::Remove ||
				(row.decision == DevChangesDecision::IncludeProposal && row.proposal_removes);
			const bool use_proposal =
				row.has_proposal && !remove && row.decision == DevChangesDecision::IncludeProposal;
			std::optional<DevBakeEntry> next;
			if (!remove)
				next = use_proposal ? row.proposed_entry : row.disk_entry;
			if (next) {
				std::string reason;
				const auto* resolved =
					schema ? resolve_saved_field(*schema, *next, reason) : nullptr;
				if (!resolved) {
					row.validity = DevChangesValidity::Unresolved;
					row.reason =
						reason.empty()
							? "Developer schema is not available to validate this saved assignment."
							: std::move(reason);
					row.blocking = true;
				} else if (!generator_ready(*next, reason) ||
						   !bake_detail::entry_value_valid(*schema, *resolved, *next, reason)) {
					row.validity = DevChangesValidity::Invalid;
					row.reason = std::move(reason);
					row.blocking = true;
				} else if (row.validity == DevChangesValidity::Conflict &&
						   (use_proposal ||
							row.reason.starts_with("The active manifest contains duplicate"))) {
					row.blocking = true;
				} else if (row.validity == DevChangesValidity::Invalid && use_proposal) {
					row.blocking = true;
				} else {
					row.validity = DevChangesValidity::Ready;
					if (row.decision == DevChangesDecision::RetainDisk ||
						row.decision == DevChangesDecision::OmitProposal)
						row.reason = "Saved value retained; live behavior is unchanged.";
				}
				row.next_value = next->valueJson;
				const bool metadata_changed =
					row.disk_entry && (row.disk_entry->ownerCppType != next->ownerCppType ||
									   row.disk_entry->sourceHeader != next->sourceHeader ||
									   row.disk_entry->valueTypeName != next->valueTypeName);
				if (row.has_disk && row.disk_entry && !metadata_changed &&
					equal_entry_values(*row.disk_entry, *next)) {
					// Preserve saved provenance and extensions for synchronized retained overrides.
					const auto refreshed_owner = next->ownerCppType;
					const auto refreshed_header = next->sourceHeader;
					*next = *row.disk_entry;
					next->ownerCppType = refreshed_owner;
					next->sourceHeader = refreshed_header;
					row.operation = row.has_proposal ? DevChangesOperation::NoChange
													 : DevChangesOperation::Keep;
				} else {
					row.operation =
						row.has_disk ? DevChangesOperation::Update : DevChangesOperation::Add;
					if (row.disk_entry)
						next->preserved_metadata = row.disk_entry->preserved_metadata;
					if (row.disk_entry)
						next->provenance.preserved_metadata =
							row.disk_entry->provenance.preserved_metadata;
				}
				plan.candidate.entries.emplace_back(std::move(*next));
			} else if (row.has_disk) {
				row.operation = DevChangesOperation::Remove;
				if (row.validity == DevChangesValidity::Conflict &&
					row.reason.starts_with("This destination changed on disk"))
					row.blocking = true;
				else {
					row.validity = DevChangesValidity::Ready;
					row.reason = "Remove the saved assignment from the next build; current "
								 "compiled behavior is unchanged.";
					row.blocking = false;
				}
			} else {
				row.operation = DevChangesOperation::NoChange;
				row.blocking = false;
			}
		}

		const auto add_memory_target = [&](const FlowUi::detail::MemoryCapacitySelection& target,
										   bool proposal) {
			auto& row =
				find_or_add_row(plan, changes_memory_key(target.key), DevChangesDomain::Memory);
			row.field_path = target.key;
			row.target_name = target.key;
			row.origin = proposal ? "Prepared memory snapshot" : "Saved manifest";
			if (proposal) {
				if (row.has_proposal && row.proposed_memory) {
					if (memory_selection_json(*row.proposed_memory) !=
						memory_selection_json(target)) {
						row.validity = DevChangesValidity::Conflict;
						row.reason = "Different prepared values share the same application-wide "
									 "memory target.";
						row.proposal += " | " + std::to_string(target.value);
					}
					return;
				}
				row.has_proposal = true;
				row.proposed_memory = target;
				row.proposal = std::to_string(target.value);
				++plan.counts.prepared_memory;
			} else {
				row.has_disk = true;
				row.disk_memory = target;
				row.on_disk = std::to_string(target.value);
			}
		};
		if (disk.memoryCapacity)
			for (const auto& target : disk.memoryCapacity->targets)
				add_memory_target(target, false);
		if (request.prepared_memory)
			for (const auto& target : request.prepared_memory->targets)
				add_memory_target(target, true);
		const auto add_policy = [&](std::string_view key, std::string_view name,
									std::optional<std::string> saved,
									std::optional<std::string> proposed, bool clear) {
			if (!saved && !proposed && !clear)
				return;
			auto& row = find_or_add_row(plan, changes_memory_key(key), DevChangesDomain::Memory);
			row.target_name = "Shared policy";
			row.field_path = name;
			row.origin = proposed || clear ? "Prepared memory policy" : "Saved manifest";
			row.has_disk = saved.has_value();
			row.on_disk = saved.value_or("");
			row.has_proposal = proposed.has_value() || clear;
			row.proposal_removes = clear;
			row.proposal = clear ? "Clear override" : proposed.value_or("");
		};
		const auto bool_text = [](bool value) {
			return value ? std::string("true") : std::string("false");
		};
		add_policy("policy.enabled", "enabled",
				   disk.memoryCapacity ? std::optional{bool_text(disk.memoryCapacity->enabled)}
									   : std::nullopt,
				   request.prepared_memory
					   ? std::optional{bool_text(request.prepared_memory->enabled)}
					   : std::nullopt,
				   request.clear_memory_section);
		add_policy("policy.growth_factor", "growth_factor",
				   disk.memoryCapacity && disk.memoryCapacity->growth_factor
					   ? std::optional{growth_factor_text(*disk.memoryCapacity->growth_factor)}
					   : std::nullopt,
				   request.prepared_memory && request.prepared_memory->growth_factor
					   ? std::optional{growth_factor_text(*request.prepared_memory->growth_factor)}
					   : std::nullopt,
				   request.clear_growth_factor || request.clear_memory_section);
		add_policy("policy.allow_runtime_growth", "allow_runtime_growth",
				   disk.memoryCapacity && disk.memoryCapacity->allow_runtime_growth
					   ? std::optional{bool_text(*disk.memoryCapacity->allow_runtime_growth)}
					   : std::nullopt,
				   request.prepared_memory && request.prepared_memory->allow_runtime_growth
					   ? std::optional{bool_text(*request.prepared_memory->allow_runtime_growth)}
					   : std::nullopt,
				   request.clear_runtime_growth || request.clear_memory_section);
		if (const auto compiled_memory = FlowUi::baked::compiled_memory_capacity_manifest_json();
			!compiled_memory.empty()) {
			DevBakeManifest compiled;
			if (bake_detail::decode_manifest(
					"{\"manifestVersion\":2,\"bakedChanges\":[],\"memoryCapacity\":" +
						std::string(compiled_memory) + "}",
					compiled) &&
				compiled.memoryCapacity) {
				for (const auto& target : compiled.memoryCapacity->targets) {
					auto& row = find_or_add_row(plan, changes_memory_key(target.key),
												DevChangesDomain::Memory);
					if (row.target_name.empty()) {
						row.target_name = target.key;
						row.field_path = target.key;
					}
					row.compiled_value = std::to_string(target.value);
					if (!row.has_disk && !row.has_proposal)
						row.reason =
							"Removed on disk; compiled initial capacity remains until rebuild.";
				}
				const auto compiled_policy = [&](std::string_view key, std::string_view field,
												 std::string value) {
					auto& row =
						find_or_add_row(plan, changes_memory_key(key), DevChangesDomain::Memory);
					row.target_name = "Shared policy";
					row.field_path = field;
					row.compiled_value = std::move(value);
				};
				compiled_policy("policy.enabled", "enabled",
								bool_text(compiled.memoryCapacity->enabled));
				if (compiled.memoryCapacity->growth_factor)
					compiled_policy("policy.growth_factor", "growth_factor",
									growth_factor_text(*compiled.memoryCapacity->growth_factor));
				if (compiled.memoryCapacity->allow_runtime_growth)
					compiled_policy("policy.allow_runtime_growth", "allow_runtime_growth",
									bool_text(*compiled.memoryCapacity->allow_runtime_growth));
			}
		}
		if (disk.memoryCapacity || request.prepared_memory) {
			plan.candidate.memoryCapacity =
				disk.memoryCapacity.value_or(FlowUi::detail::MemoryCapacityManifest{});
			plan.candidate.memoryCapacity->targets.clear();
		}
		const auto enabled_key = changes_memory_key("policy.enabled");
		const auto enabled_decision = decision_for(request, enabled_key);
		const bool section_removal_intent =
			enabled_decision == DevChangesDecision::Remove ||
			(request.clear_memory_section &&
			 enabled_decision == DevChangesDecision::IncludeProposal);
		const bool section_removal_reviewed =
			section_removal_intent && decision_resolution_reviewed(plan, enabled_key);
		bool remove_memory_section = false;
		for (auto& row : plan.rows) {
			if (row.domain != DevChangesDomain::Memory)
				continue;
			const bool remove = section_removal_intent ||
								row.decision == DevChangesDecision::Remove ||
								(row.decision == DevChangesDecision::IncludeProposal &&
								 (row.proposal_removes || request.clear_memory_section));
			const bool proposal_intent =
				row.has_proposal && row.decision == DevChangesDecision::IncludeProposal;
			if (request.opened_manifest && (remove || proposal_intent) &&
				!decision_resolution_reviewed(plan, row.key) && !section_removal_reviewed) {
				const auto& opened_memory = request.opened_manifest->memoryCapacity;
				std::string opened_value;
				bool had_opened = false;
				if (opened_memory) {
					if (row.disk_memory || row.proposed_memory) {
						const auto opened =
							std::find_if(opened_memory->targets.begin(),
										 opened_memory->targets.end(), [&](const auto& target) {
											 return changes_memory_key(target.key) == row.key;
										 });
						if (opened != opened_memory->targets.end()) {
							had_opened = true;
							opened_value = memory_selection_json(*opened);
						}
					} else if (row.field_path == "enabled") {
						had_opened = true;
						opened_value = bool_text(opened_memory->enabled);
					} else if (row.field_path == "growth_factor" && opened_memory->growth_factor) {
						had_opened = true;
						opened_value = growth_factor_text(*opened_memory->growth_factor);
					} else if (row.field_path == "allow_runtime_growth" &&
							   opened_memory->allow_runtime_growth) {
						had_opened = true;
						opened_value = bool_text(*opened_memory->allow_runtime_growth);
					}
				}
				const auto disk_value =
					row.disk_memory ? memory_selection_json(*row.disk_memory) : row.on_disk;
				const auto proposed_value = row.proposed_memory
												? memory_selection_json(*row.proposed_memory)
												: row.proposal;
				const bool changed =
					had_opened != row.has_disk || (had_opened && disk_value != opened_value);
				const bool same_output =
					remove ? !row.has_disk : row.has_disk && disk_value == proposed_value;
				if (changed && !same_output) {
					row.validity = DevChangesValidity::Conflict;
					row.reason =
						"This memory destination changed on disk since review. " +
						std::string(
							remove
								? "Confirm removal against the new head, or keep the saved value."
								: "Explicitly include this prepared result against the new head, "
								  "or keep the saved value.");
				}
			}
			const bool use_proposal =
				row.has_proposal && !remove && row.decision == DevChangesDecision::IncludeProposal;
			if (!plan.candidate.memoryCapacity && (row.has_disk || use_proposal))
				plan.candidate.memoryCapacity.emplace();
			if (row.disk_memory || row.proposed_memory) {
				const auto& next = remove ? std::optional<FlowUi::detail::MemoryCapacitySelection>{}
								   : use_proposal ? row.proposed_memory
												  : row.disk_memory;
				if (next) {
					row.next_value = std::to_string(next->value);
					const bool equal = row.disk_memory && memory_selection_json(*row.disk_memory) ==
															  memory_selection_json(*next);
					row.operation = !row.has_disk ? DevChangesOperation::Add
									: equal ? (row.has_proposal ? DevChangesOperation::NoChange
																: DevChangesOperation::Keep)
											: DevChangesOperation::Update;
					FlowUi::detail::MemoryCapacityManifest single_target;
					single_target.targets.emplace_back(*next);
					std::string target_error;
					const bool target_valid = FlowUi::detail::validate_memory_capacity_manifest(
						single_target, target_error);
					if (!target_valid) {
						row.validity = DevChangesValidity::Invalid;
						row.blocking = true;
						row.reason = target_error;
					} else if (use_proposal && row.validity == DevChangesValidity::Conflict)
						row.blocking = true;
					// Keep invalid staged selections visible without passing unknown keys into
					// encoding.
					if (target_valid)
						plan.candidate.memoryCapacity->targets.emplace_back(*next);
				} else {
					row.operation =
						row.has_disk ? DevChangesOperation::Remove : DevChangesOperation::NoChange;
					if (row.validity == DevChangesValidity::Conflict &&
						row.reason.starts_with("This memory destination changed on disk"))
						row.blocking = true;
					else
						row.reason = "Release this override to startup fallback. Running "
									 "allocations are unchanged.";
				}
			} else {
				const bool has_next = !remove && (use_proposal || row.has_disk);
				row.next_value = has_next ? (use_proposal ? row.proposal : row.on_disk) : "";
				row.operation = !has_next		? (row.has_disk ? DevChangesOperation::Remove
																: DevChangesOperation::NoChange)
								: !row.has_disk ? DevChangesOperation::Add
								: row.next_value != row.on_disk ? DevChangesOperation::Update
								: row.has_proposal				? DevChangesOperation::NoChange
																: DevChangesOperation::Keep;
				if (row.field_path == "enabled") {
					remove_memory_section = remove && (request.clear_memory_section ||
													   row.decision == DevChangesDecision::Remove);
					if (has_next)
						plan.candidate.memoryCapacity->enabled = row.next_value == "true";
				} else if (row.field_path == "growth_factor") {
					if (!has_next && plan.candidate.memoryCapacity)
						plan.candidate.memoryCapacity->growth_factor.reset();
					else if (use_proposal)
						plan.candidate.memoryCapacity->growth_factor =
							request.prepared_memory->growth_factor;
				} else if (row.field_path == "allow_runtime_growth") {
					if (!has_next && plan.candidate.memoryCapacity)
						plan.candidate.memoryCapacity->allow_runtime_growth.reset();
					else if (use_proposal)
						plan.candidate.memoryCapacity->allow_runtime_growth =
							request.prepared_memory->allow_runtime_growth;
				}
				if (use_proposal && row.validity == DevChangesValidity::Conflict)
					row.blocking = true;
				if (use_proposal && row.field_path == "growth_factor" &&
					request.prepared_memory->growth_factor &&
					(!std::isfinite(*request.prepared_memory->growth_factor) ||
					 *request.prepared_memory->growth_factor <= 1)) {
					row.validity = DevChangesValidity::Invalid;
					row.blocking = true;
					row.reason = "Storage growth factor must be finite and greater than one.";
				}
			}
			if (remove && row.validity == DevChangesValidity::Conflict &&
				row.reason.starts_with("This memory destination changed on disk"))
				row.blocking = true;
		}
		if (remove_memory_section) {
			plan.candidate.memoryCapacity.reset();
			for (auto& row : plan.rows)
				if (row.domain == DevChangesDomain::Memory) {
					row.operation =
						row.has_disk ? DevChangesOperation::Remove : DevChangesOperation::NoChange;
					row.next_value.clear();
					const bool external_conflict =
						row.validity == DevChangesValidity::Conflict &&
						row.reason.starts_with("This memory destination changed on disk");
					row.blocking = external_conflict;
					if (!external_conflict)
						row.validity = DevChangesValidity::Ready;
					if (row.has_proposal)
						row.decision = row.has_disk ? DevChangesDecision::Remove
													: DevChangesDecision::OmitProposal;
					if (!external_conflict)
						row.reason = "The entire memory section is removed. Prepared values remain "
									 "staged; running allocations are unchanged.";
				}
		}
		if (plan.candidate.memoryCapacity) {
			std::string error;
			if (!FlowUi::detail::validate_memory_capacity_manifest(*plan.candidate.memoryCapacity,
																   error)) {
				plan.error = error;
				++plan.counts.errors;
				// Avoid encoding invalid/unregistered memory keys in the exact preview.
				plan.candidate.memoryCapacity = disk.memoryCapacity;
			}
		}
		std::sort(plan.candidate.entries.begin(), plan.candidate.entries.end(),
				  [](const auto& left, const auto& right) {
					  return changes_entry_key(left) < changes_entry_key(right);
				  });
		std::sort(plan.rows.begin(), plan.rows.end(), [](const auto& left, const auto& right) {
			return std::tie(left.domain, left.target_name, left.field_path, left.key) <
				   std::tie(right.domain, right.target_name, right.field_path, right.key);
		});
		for (const auto& row : plan.rows)
			count_row(plan, row);
		if (plan.counts.pending()) {
			plan.candidate.manifestVersion =
				plan.candidate.memoryCapacity ? 2 : disk.manifestVersion;
			if (schema)
				plan.candidate.schemaFingerprint = schema->fingerprint;
			plan.candidate.buildFingerprint = FLOWUI_BAKE_BUILD_FINGERPRINT;
			if (plan.candidate.createdTimestamp.empty())
				plan.candidate.createdTimestamp = bake_detail::utc_timestamp();
		}
		plan.candidate_json = bake_detail::encode_manifest(plan.candidate);
		plan.can_commit = plan.valid_disk && plan.error.empty() && plan.counts.conflicts == 0 &&
						  plan.counts.errors == 0 && plan.counts.pending() != 0;
		return plan;
	} catch (...) {
		plan.can_commit = false;
		plan.error = "Could not construct the Changes review; pending work has been retained.";
		return plan;
	}
}

DevCommandResult DevBakePipeline::commit_review(const DevChangesReviewPlan& plan,
												std::uint64_t current_memory_revision) noexcept {
	try {
		status_ = {};
		if (!plan.valid_disk || !plan.error.empty())
			return {.status = DevCommandStatus::BakeManifestInvalid};
		if (plan.manifest_path != manifestPath_ ||
			(current_memory_revision != 0 && current_memory_revision != plan.memory_revision)) {
			status_.bakeDiagnostics.emplace_back(DevBakeDiagnostic{
				DevBakeDiagnosticCode::ManifestInvalid,
				"Prepared memory or active manifest changed after review; refresh before baking."});
			return {.status = DevCommandStatus::SchemaMismatch};
		}
		bake_detail::ManifestWriterLock writer_lock(manifestPath_);
		if (!writer_lock.acquired()) {
			status_.bakeDiagnostics.emplace_back(
				DevBakeDiagnostic{DevBakeDiagnosticCode::ManifestWriteFailed,
								  "Another manifest writer holds " + manifestPath_.string() +
									  ".lock; retry after that writer finishes."});
			return {.status = DevCommandStatus::BakeWriteFailed};
		}
		const auto current = query_review(plan.request);
		if (!current.valid_disk || current.disk_revision != plan.disk_revision ||
			current.schema_generation != plan.schema_generation ||
			current.proposal_revision != plan.proposal_revision) {
			status_.bakeDiagnostics.emplace_back(
				DevBakeDiagnostic{DevBakeDiagnosticCode::ManifestInvalid,
								  "The disk manifest, schema or live proposals changed after "
								  "review. Refresh and review the new plan before baking."});
			return {.status = DevCommandStatus::SchemaMismatch};
		}
		if (!current.can_commit || !plan.can_commit) {
			return {.status = current.counts.pending() == 0
								  ? DevCommandStatus::NothingToBake
								  : DevCommandStatus::BakeManifestInvalid};
		}
		if (candidate_signature(current.candidate) != candidate_signature(plan.candidate) ||
			bake_detail::encode_manifest(plan.candidate) != plan.candidate_json) {
			status_.bakeDiagnostics.emplace_back(
				DevBakeDiagnostic{DevBakeDiagnosticCode::ManifestInvalid,
								  "The reviewed candidate no longer matches its validated output; "
								  "refresh before baking."});
			return {.status = DevCommandStatus::SchemaMismatch};
		}
		if (!bake_detail::write_manifest_atomically(manifestPath_, plan.candidate_json)) {
			status_.bakeDiagnostics.emplace_back(
				DevBakeDiagnostic{DevBakeDiagnosticCode::ManifestWriteFailed,
								  "Cannot atomically replace " + manifestPath_.string() +
									  "; reviewed decisions and live proposals remain available."});
			return {.status = DevCommandStatus::BakeWriteFailed};
		}
		manifest_ = plan.candidate;
		return {.status = DevCommandStatus::Applied, .applied = true};
	} catch (...) {
		return {.status = DevCommandStatus::BakeWriteFailed};
	}
}

} // namespace FlowUi::devSystems::tooling

#endif
