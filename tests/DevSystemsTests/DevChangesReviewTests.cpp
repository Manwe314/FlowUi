#ifdef NDEBUG
#undef NDEBUG
#endif

#include <algorithm>
#include <cassert>
#include <charconv>
#include <cmath>
#include <filesystem>
#include <fstream>
#include <iostream>

#include "DevChangesReviewTestSchema.hpp"
#include "devSystems/devTooling/bake/DevBakePipeline.hpp"
#include "devSystems/devTooling/override/DevOverrideEngine.hpp"
#include "managers/ThemeManager.hpp"

namespace changes_review_test {
FLOWUI_DEV_SCHEMA(SourceParameters, FLOWUI_DEV_FIELD(SourceParameters, amount))
}

namespace {

using namespace FlowUi::devSystems::tooling;
using changes_review_test::Element;

[[nodiscard]] const DevChangesReviewRow& amount_row(const DevChangesReviewPlan& plan) {
	const auto found = std::find_if(plan.rows.begin(), plan.rows.end(),
									[](const auto& row) { return row.field_path == "amount"; });
	assert(found != plan.rows.end());
	return *found;
}

[[nodiscard]] std::string read_file(const std::filesystem::path& path) {
	std::ifstream input(path, std::ios::binary);
	return std::string(std::istreambuf_iterator<char>(input), {});
}

void write_file(const std::filesystem::path& path, std::string_view contents) {
	std::ofstream output(path, std::ios::binary | std::ios::trunc);
	output << contents;
	output.close();
	assert(!output.fail());
}

void replace_once(std::string& contents, std::string_view original, std::string_view replacement) {
	const auto position = contents.find(original);
	assert(position != std::string::npos);
	contents.replace(position, original.size(), replacement);
}

struct Fixture {
	FlowUi::devMode::DevSchemaRegistry schemas{};
	DevOverrideEngine overrides{schemas};
	DevBakePipeline pipeline{schemas, overrides};
	FlowUi::ThemeManager themes{};
	std::uint64_t transaction = 0;

	explicit Fixture(const std::filesystem::path& path) {
		schemas.ensureElement<Element>();
		assert(schemas.publishPendingAtSafePoint());
		pipeline.setManifestPath(path);
	}

	[[nodiscard]] DevOverrideFieldKey field(std::string_view name) const {
		const auto schema = schemas.view();
		const auto* element = schema->findElement(Element::definitionId);
		assert(element);
		for (const auto& candidate : schema->fieldsOf(element->parametersType))
			if (schema->string(candidate.name) == name)
				return {schema->type(element->parametersType)->id, candidate.id};
		assert(!"Test fixture field was not found");
		return {};
	}

	template <class Value>
	void set(std::string_view name, Value value, FlowUi::WindowId window = 0,
			 std::uint64_t instance = 0) {
		DevOwnedValue owned;
		assert(overrides.copyValue(value, owned) ==
			   FlowUi::devMode::DevValueOperationStatus::Success);
		DevChangeSet change{.transaction = ++transaction};
		change.commands.emplace_back(DevOverrideCommand{
			.kind = DevOverrideCommandKind::SetElementField,
			.element = {.definition = Element::definitionId,
						.window = window,
						.instance = {.value = instance},
						.scope = instance ? DevOverrideScope::ExactInstance
										  : DevOverrideScope::Definition},
			.field = field(name),
			.layer = instance ? DevOverrideLayer::LiveInstance : DevOverrideLayer::LiveDefinition,
			.value = std::move(owned)});
		assert(overrides.submit(std::move(change)));
		overrides.commitAtSafePoint(themes);
		assert(!overrides.commandResults().empty() && overrides.commandResults().back().applied);
	}
};

[[nodiscard]] FlowUi::detail::MemoryCapacityManifest memory(std::uint64_t value = 128) {
	FlowUi::detail::MemoryCapacityManifest prepared;
	prepared.targets.emplace_back(FlowUi::detail::MemoryCapacitySelection{
		.evidence = {.kind = "recording", .source_name = "review acceptance", .dataset_id = 7},
		.key = "flowui.tuning.input_fields.registry_entries",
		.policy = "manual",
		.value = value});
	return prepared;
}

void verify_owned_atomic_memory(const std::filesystem::path& directory) {
	FlowUi::devMode::DevSchemaRegistry schemas;
	DevOverrideEngine overrides{schemas};
	DevBakePipeline pipeline{schemas, overrides};
	const auto path = directory / "memory.flowchanges";
	pipeline.setManifestPath(path);
	DevChangesReviewRequest request{.prepared_memory = memory(), .memory_revision = 4};
	request.prepared_memory->growth_factor = 2;
	request.prepared_memory->allow_runtime_growth = false;
	auto plan = pipeline.query_review(request);
	assert(plan.valid_disk && plan.can_commit && plan.counts.additions == 4);
	assert(!pipeline.commit_review(plan, 5).applied);
	assert(!std::filesystem::exists(path));
	assert(pipeline.commit_review(plan, 4).applied);
	assert(pipeline.manifest().memoryCapacity->targets.front().value == 128);
	const auto timestamp = std::filesystem::last_write_time(path);
	plan = pipeline.query_review(request);
	assert(!plan.can_commit && plan.counts.pending() == 0);
	assert(pipeline.commit_review(plan, 4).status == DevCommandStatus::NothingToBake);
	assert(std::filesystem::last_write_time(path) == timestamp);
	request.prepared_memory->targets.front().value = 256;
	request.clear_growth_factor = true;
	request.clear_runtime_growth = true;
	request.memory_revision = 6;
	plan = pipeline.query_review(request);
	assert(plan.can_commit && plan.counts.updates == 1 && plan.counts.removals == 2);
	assert(pipeline.commit_review(plan, 6).applied);
	assert(!pipeline.manifest().memoryCapacity->growth_factor);
	assert(!pipeline.manifest().memoryCapacity->allow_runtime_growth);
	request.prepared_memory->targets.front().key = "flowui.tuning.popups.stack_entries";
	request.decisions.emplace_back(
		DevChangesDecisionEntry{changes_memory_key("policy.enabled"), DevChangesDecision::Remove});
	plan = pipeline.query_review(request);
	assert(plan.can_commit && plan.counts.removals == 2);
	assert(plan.counts.additions == 0 && plan.counts.updates == 0 && plan.counts.omitted >= 1);
	assert(std::all_of(plan.rows.begin(), plan.rows.end(),
					   [](const auto& row) { return row.next_value.empty(); }));
	assert(pipeline.commit_review(plan).applied);
	assert(!pipeline.manifest().memoryCapacity);
}

void verify_retention_decisions_and_revision(const std::filesystem::path& directory) {
	const auto path = directory / "elements.flowchanges";
	Fixture fixture(path);
	fixture.set("amount", 3.f);
	auto plan = fixture.pipeline.query_review();
	assert(plan.can_commit && plan.counts.additions == 1);
	const auto amount_key = amount_row(plan).key;
	assert(fixture.pipeline.commit_review(plan).applied);
	assert(fixture.overrides.appliedOverrides().records().size() == 1);
	assert(fixture.pipeline.query_review().counts.pending() == 0);
	const auto original_contents = read_file(path);
	fixture.schemas.ensureElement<changes_review_test::UnrelatedElement>();
	assert(fixture.schemas.publishPendingAtSafePoint());
	fixture.overrides.commitAtSafePoint(fixture.themes);
	fixture.set("count", std::int32_t{8});
	plan = fixture.pipeline.query_review();
	assert(plan.can_commit && plan.candidate.entries.size() == 2);
	assert(fixture.pipeline.commit_review(plan).applied);
	assert(fixture.pipeline.manifest().entries.size() == 2);
	fixture.set("amount", 5.f);
	DevChangesReviewRequest omitted;
	omitted.decisions.emplace_back(
		DevChangesDecisionEntry{amount_key, DevChangesDecision::OmitProposal});
	plan = fixture.pipeline.query_review(omitted);
	assert(plan.counts.pending() == 0 && plan.counts.omitted == 1);
	assert(std::any_of(
		plan.candidate.entries.begin(), plan.candidate.entries.end(),
		[](const auto& entry) { return entry.fieldPath == "amount" && entry.valueJson == "3"; }));
	plan = fixture.pipeline.query_review();
	const auto reviewed = plan.disk_manifest;
	write_file(path, read_file(path) + "\n");
	assert(fixture.pipeline.commit_review(plan).status == DevCommandStatus::SchemaMismatch);
	assert(fixture.overrides.appliedOverrides().records().size() == 2);
	DevChangesReviewRequest refreshed{.opened_manifest = reviewed};
	plan = fixture.pipeline.query_review(refreshed);
	assert(plan.can_commit && plan.counts.conflicts == 0); // Independent formatting edit refreshes.
	assert(fixture.pipeline.commit_review(plan).applied);
	assert(read_file(path) != original_contents);
}

void verify_conflicts_and_failures(const std::filesystem::path& directory) {
	const auto path = directory / "conflicts.flowchanges";
	Fixture fixture(path);
	fixture.set("amount", 4.f, 1, 77);
	fixture.set("amount", 6.f, 2, 77);
	auto plan = fixture.pipeline.query_review();
	assert(plan.counts.conflicts == 1 && !plan.can_commit);
	assert(!std::filesystem::exists(path));
	DevChangesReviewRequest omitted;
	omitted.decisions.emplace_back(
		DevChangesDecisionEntry{amount_row(plan).key, DevChangesDecision::OmitProposal});
	plan = fixture.pipeline.query_review(omitted);
	assert(plan.counts.conflicts == 0 && plan.counts.pending() == 0);
	fixture.set("amount", 4.f, 2, 77);
	plan = fixture.pipeline.query_review();
	assert(plan.can_commit && plan.counts.additions == 1);
	std::filesystem::create_directory(path.string() + ".lock");
	assert(fixture.pipeline.commit_review(plan).status == DevCommandStatus::BakeWriteFailed);
	std::filesystem::remove(path.string() + ".lock");
	std::filesystem::create_directory(path.string() + ".tmp");
	assert(fixture.pipeline.commit_review(plan).status == DevCommandStatus::BakeWriteFailed);
	assert(!std::filesystem::exists(path));
	std::filesystem::remove(path.string() + ".tmp");
	assert(fixture.pipeline.commit_review(plan).applied);
	const auto saved = read_file(path);
	write_file(path, "{\"manifestVersion\":99,\"bakedChanges\":[]}");
	const auto invalid = fixture.pipeline.query_review();
	assert(!invalid.valid_disk && !invalid.can_commit && !invalid.error.empty());
	assert(fixture.pipeline.manifest().entries.size() == 1);
	write_file(path, saved);
}

void verify_external_reconciliation_and_unresolved(const std::filesystem::path& directory) {
	const auto path = directory / "external.flowchanges";
	Fixture fixture(path);
	fixture.set("amount", 2.f);
	auto plan = fixture.pipeline.query_review();
	assert(fixture.pipeline.commit_review(plan).applied);
	const auto opened = fixture.pipeline.query_review().disk_manifest;
	fixture.set("amount", 3.f);
	std::string external = read_file(path);
	const auto json_position = external.find("\"valueJson\": \"2\"");
	assert(json_position != std::string::npos);
	external.replace(json_position, std::string("\"valueJson\": \"2\"").size(),
					 "\"valueJson\": \"4\"");
	write_file(path, external);
	DevChangesReviewRequest request{.opened_manifest = opened};
	plan = fixture.pipeline.query_review(request);
	assert(!plan.can_commit && plan.counts.conflicts == 1);
	request.decisions.emplace_back(DevChangesDecisionEntry{
		amount_row(plan).key, DevChangesDecision::IncludeProposal, plan.disk_revision});
	plan = fixture.pipeline.query_review(request);
	assert(plan.can_commit && plan.counts.conflicts == 0);
	assert(fixture.pipeline.commit_review(plan).applied);
	// A saved type mismatch is retained visibly and can be removed without serializing its value.
	fixture.overrides.appliedOverrides().clearAll();
	fixture.overrides.appliedOverrides().clearBakeTombstones();
	external = read_file(path);
	const auto type_position = external.find("\"valueTypeId\": \"");
	assert(type_position != std::string::npos);
	const auto value_start = type_position + std::string("\"valueTypeId\": \"").size();
	external.replace(value_start, external.find('"', value_start) - value_start, "0x1");
	write_file(path, external);
	plan = fixture.pipeline.query_review();
	assert(plan.counts.errors == 1 && !plan.can_commit && amount_row(plan).disk_entry);
	request = {};
	request.decisions.emplace_back(
		DevChangesDecisionEntry{amount_row(plan).key, DevChangesDecision::Remove});
	plan = fixture.pipeline.query_review(request);
	assert(plan.can_commit && plan.counts.removals == 1);
	assert(fixture.pipeline.commit_review(plan).applied);
	assert(fixture.pipeline.manifest().entries.empty());
}

void verify_invalid_memory_omission(const std::filesystem::path& directory) {
	FlowUi::devMode::DevSchemaRegistry schemas;
	DevOverrideEngine overrides{schemas};
	DevBakePipeline pipeline{schemas, overrides};
	pipeline.setManifestPath(directory / "invalid-memory.flowchanges");
	DevChangesReviewRequest request{.prepared_memory = memory()};
	request.prepared_memory->targets.emplace_back(FlowUi::detail::MemoryCapacitySelection{
		.key = "flowui.unknown.capacity", .policy = "manual", .value = 42});
	auto plan = pipeline.query_review(request);
	assert(!plan.can_commit && plan.counts.errors == 1);
	request.decisions.emplace_back(DevChangesDecisionEntry{
		changes_memory_key("flowui.unknown.capacity"), DevChangesDecision::OmitProposal});
	plan = pipeline.query_review(request);
	assert(plan.can_commit && plan.counts.errors == 0 && plan.counts.omitted == 1);
	assert(pipeline.commit_review(plan).applied);
	assert(pipeline.manifest().memoryCapacity->targets.size() == 1);
}

void verify_provenance_and_semantic_retention(const std::filesystem::path& directory) {
	const auto path = directory / "provenance.flowchanges";
	Fixture fixture(path);
	fixture.set("amount", 6.f);
	auto plan = fixture.pipeline.query_review();
	assert(fixture.pipeline.commit_review(plan).applied);
	auto contents = read_file(path);
	const auto author_position = contents.find("\"author\": \"developer\"");
	assert(author_position != std::string::npos);
	contents.replace(author_position, std::string("\"author\": \"developer\"").size(),
					 "\"author\": \"earlier developer\", \"sourceSession\": \"prior-session\"");
	const auto value_position = contents.find("\"valueJson\": \"6\"");
	assert(value_position != std::string::npos);
	contents.replace(value_position, std::string("\"valueJson\": \"6\"").size(),
					 "\"valueJson\": \"6.000\"");
	contents.insert(contents.find("\"provenance\":"),
					"\"customEvidence\":{\"ticket\":42,\"controlNote\":\"A\\bB\\fC\"},\n      ");
	contents.insert(contents.find("\"bakedChanges\":"), "\"customEnvelope\":{\"version\":3},\n  ");
	write_file(path, contents);
	plan = fixture.pipeline.query_review();
	assert(plan.counts.pending() == 0 && !plan.can_commit);
	assert(amount_row(plan).disk_entry->provenance.author == "earlier developer");
	fixture.set("count", std::int32_t{4});
	DevChangesReviewRequest request{.prepared_memory = memory()};
	plan = fixture.pipeline.query_review(request);
	assert(plan.can_commit && plan.counts.additions == 3); // One element and two memory rows.
	assert(fixture.pipeline.commit_review(plan).applied);
	contents = read_file(path);
	assert(contents.find("earlier developer") != std::string::npos);
	assert(contents.find("prior-session") != std::string::npos);
	assert(contents.find("customEvidence") != std::string::npos);
	assert(contents.find("A\\bB\\fC") != std::string::npos);
	assert(contents.find("customEnvelope") != std::string::npos);
	assert(fixture.pipeline.manifest().entries.size() == 2);
	assert(fixture.pipeline.manifest().memoryCapacity->targets.size() == 1);
}

void verify_generated_value_and_metadata_repairs(const std::filesystem::path& directory) {
	const auto path = directory / "value-consistency.flowchanges";
	Fixture fixture(path);
	fixture.set("amount", 6.f);
	auto plan = fixture.pipeline.query_review();
	assert(fixture.pipeline.commit_review(plan).applied);
	fixture.overrides.appliedOverrides().clearAll();
	fixture.overrides.appliedOverrides().clearBakeTombstones();
	auto contents = read_file(path);
	const auto cpp_position = contents.find("\"cppValue\": \"6\"");
	assert(cpp_position != std::string::npos);
	contents.replace(cpp_position, std::string("\"cppValue\": \"6\"").size(),
					 "\"cppValue\": \"9\"");
	write_file(path, contents);
	plan = fixture.pipeline.query_review();
	assert(!plan.can_commit && amount_row(plan).validity == DevChangesValidity::Invalid);
	fixture.set("amount", 6.f);
	plan = fixture.pipeline.query_review();
	assert(plan.can_commit && plan.counts.updates == 1);
	assert(amount_row(plan).next_value == "6");
	assert(fixture.pipeline.commit_review(plan).applied);
	fixture.overrides.appliedOverrides().clearAll();
	fixture.overrides.appliedOverrides().clearBakeTombstones();
	contents = read_file(path);
	const auto header_position = contents.find("\"sourceHeader\": \"");
	assert(header_position != std::string::npos);
	const auto value_start = header_position + std::string("\"sourceHeader\": \"").size();
	contents.replace(value_start, contents.find('"', value_start) - value_start,
					 "retired/header.hpp");
	write_file(path, contents);
	plan = fixture.pipeline.query_review();
	assert(plan.can_commit && plan.counts.updates == 1);
	assert(fixture.pipeline.commit_review(plan).applied);
	assert(read_file(path).find("retired/header.hpp") == std::string::npos);
}

void verify_adjacent_growth_factors(const std::filesystem::path& directory) {
	FlowUi::devMode::DevSchemaRegistry schemas;
	DevOverrideEngine overrides{schemas};
	DevBakePipeline pipeline{schemas, overrides};
	pipeline.setManifestPath(directory / "adjacent-growth.flowchanges");
	DevChangesReviewRequest request;
	request.prepared_memory.emplace();
	request.prepared_memory->growth_factor = 1.5f;
	auto plan = pipeline.query_review(request);
	assert(plan.can_commit && pipeline.commit_review(plan).applied);
	const auto opened = pipeline.query_review().disk_manifest;
	const auto adjacent = std::nextafter(1.5f, 2.f);
	request.prepared_memory->growth_factor = adjacent;
	plan = pipeline.query_review(request);
	assert(plan.can_commit && plan.counts.updates == 1);
	assert(plan.candidate.memoryCapacity->growth_factor == adjacent);
	const auto policy = std::find_if(plan.rows.begin(), plan.rows.end(), [](const auto& row) {
		return row.field_path == "growth_factor";
	});
	assert(policy != plan.rows.end() && policy->on_disk != policy->proposal);
	assert(pipeline.commit_review(plan).applied);
	assert(pipeline.query_review(request).counts.pending() == 0);
	FlowUi::detail::MemoryCapacityManifest external;
	external.growth_factor = std::nextafter(adjacent, 2.f);
	assert(pipeline.save_memory_capacities(external).applied);
	request.opened_manifest = opened;
	plan = pipeline.query_review(request);
	assert(!plan.can_commit && plan.counts.conflicts == 1);
}

void verify_explicit_entry_removal_rebase(const std::filesystem::path& directory, bool theme) {
	const auto path = directory / (theme ? "theme-removal-rebase.flowchanges"
										 : "element-removal-rebase.flowchanges");
	Fixture fixture(path);
	fixture.set("amount", 2.f);
	auto plan = fixture.pipeline.query_review();
	assert(fixture.pipeline.commit_review(plan).applied);
	fixture.overrides.appliedOverrides().clearAll();
	fixture.overrides.appliedOverrides().clearBakeTombstones();
	if (theme) {
		fixture.schemas.ensureTheme<changes_review_test::Parameters>();
		assert(fixture.schemas.publishPendingAtSafePoint());
		fixture.overrides.commitAtSafePoint(fixture.themes);
		auto contents = read_file(path);
		replace_once(contents, "\"targetKind\": \"Element\"", "\"targetKind\": \"Theme\"");
		const auto definition_position = contents.find("\"definitionId\": \"");
		assert(definition_position != std::string::npos);
		const auto value_start = definition_position + std::string("\"definitionId\": \"").size();
		const auto value_end = contents.find('"', value_start);
		char type_hex[32]{};
		const auto written = std::to_chars(type_hex, type_hex + sizeof(type_hex),
										   fixture.field("amount").ownerType, 16);
		assert(written.ec == std::errc{});
		contents.replace(definition_position, value_end - definition_position + 1,
						 "\"themeType\": \"0x" + std::string(type_hex, written.ptr) +
							 "\", \"themeVariant\": \"default\"");
		write_file(path, contents);
	}
	plan = fixture.pipeline.query_review();
	DevChangesReviewRequest request{.opened_manifest = plan.disk_manifest};
	request.decisions.emplace_back(DevChangesDecisionEntry{
		amount_row(plan).key, DevChangesDecision::Remove, plan.disk_revision});
	plan = fixture.pipeline.query_review(request);
	assert(plan.can_commit && plan.counts.removals == 1);
	auto external = read_file(path);
	replace_once(external, "\"valueJson\": \"2\"", "\"valueJson\": \"4\"");
	replace_once(external, "\"cppValue\": \"2\"", "\"cppValue\": \"4\"");
	write_file(path, external);
	assert(fixture.pipeline.commit_review(plan).status == DevCommandStatus::SchemaMismatch);
	plan = fixture.pipeline.query_review(request);
	assert(!plan.can_commit && plan.counts.conflicts == 1 &&
		   amount_row(plan).operation == DevChangesOperation::Remove);
	request.decisions.front().disk_revision = plan.disk_revision;
	plan = fixture.pipeline.query_review(request);
	assert(plan.can_commit && plan.counts.conflicts == 0 && plan.counts.removals == 1);
	assert(fixture.pipeline.commit_review(plan).applied);
	request.opened_manifest.reset();
	assert(fixture.pipeline.query_review(request).counts.pending() == 0);
}

void verify_memory_removal_rebase(const std::filesystem::path& directory) {
	FlowUi::devMode::DevSchemaRegistry schemas;
	DevOverrideEngine overrides{schemas};
	DevBakePipeline pipeline{schemas, overrides};
	pipeline.setManifestPath(directory / "memory-removal-rebase.flowchanges");
	auto initial = memory();
	initial.growth_factor = 1.5f;
	assert(pipeline.save_memory_capacities(initial).applied);
	auto plan = pipeline.query_review();
	DevChangesReviewRequest request{.opened_manifest = plan.disk_manifest};
	request.decisions.emplace_back(
		DevChangesDecisionEntry{changes_memory_key(initial.targets.front().key),
								DevChangesDecision::Remove, plan.disk_revision});
	request.decisions.emplace_back(
		DevChangesDecisionEntry{changes_memory_key("policy.growth_factor"),
								DevChangesDecision::Remove, plan.disk_revision});
	auto external = memory(256);
	external.growth_factor = 2;
	assert(pipeline.save_memory_capacities(external).applied);
	plan = pipeline.query_review(request);
	assert(!plan.can_commit && plan.counts.conflicts == 2);
	for (auto& decision : request.decisions)
		decision.disk_revision = plan.disk_revision;
	plan = pipeline.query_review(request);
	assert(plan.can_commit && plan.counts.conflicts == 0 && plan.counts.removals == 2);
	assert(pipeline.commit_review(plan).applied);
	request.opened_manifest.reset();
	assert(pipeline.query_review(request).counts.pending() == 0);
	// Section removal must also conflict when another process introduces a new target.
	assert(pipeline.save_memory_capacities(initial).applied);
	plan = pipeline.query_review();
	request = {.opened_manifest = plan.disk_manifest, .clear_memory_section = true};
	request.decisions.emplace_back(DevChangesDecisionEntry{changes_memory_key("policy.enabled"),
														   DevChangesDecision::IncludeProposal,
														   plan.disk_revision});
	external = memory(256);
	external.targets.front().key = "flowui.tuning.popups.stack_entries";
	assert(pipeline.save_memory_capacities(external).applied);
	plan = pipeline.query_review(request);
	assert(!plan.can_commit && plan.counts.conflicts == 1);
	request.decisions.front().disk_revision = plan.disk_revision;
	plan = pipeline.query_review(request);
	assert(plan.can_commit && plan.counts.conflicts == 0 && !plan.candidate.memoryCapacity);
	assert(pipeline.commit_review(plan).applied);
	request.opened_manifest.reset();
	assert(pipeline.query_review(request).counts.pending() == 0);
}

void verify_unknown_memory_members(const std::filesystem::path& directory) {
	FlowUi::devMode::DevSchemaRegistry schemas;
	DevOverrideEngine overrides{schemas};
	DevBakePipeline pipeline{schemas, overrides};
	const auto path = directory / "unknown-memory-metadata.flowchanges";
	pipeline.setManifestPath(path);
	assert(pipeline.save_memory_capacities(memory()).applied);
	const auto original = read_file(path);
	const auto reject_member = [&](std::string_view marker, std::string_view name) {
		auto modified = original;
		const auto position = modified.find(marker);
		assert(position != std::string::npos);
		modified.insert(position, "\"" + std::string(name) + "\":42,");
		write_file(path, modified);
		const auto plan = pipeline.query_review();
		assert(!plan.valid_disk && !plan.can_commit && plan.counts.errors == 1);
		assert(plan.error.find(name) != std::string::npos);
		assert(!pipeline.commit_review(plan).applied);
		assert(read_file(path) == modified);
	};
	reject_member("\"version\":1", "futurePolicy");
	reject_member("\"key\":", "futureTargetMetadata");
	reject_member("\"kind\":", "futureEvidenceMetadata");
	write_file(path, original);
	assert(pipeline.query_review().valid_disk);
}

void verify_stable_semantic_values(const std::filesystem::path& directory) {
	Fixture fixture(directory / "semantic-values.flowchanges");
	fixture.set("action", FlowUi::ActionCall::fromStable(FlowUi::ActionCallKind::App, 55));
	auto texture = FlowUi::TextureRef::fromStable(FlowUi::ResourceDomain::Icon, "review.\"icon\"",
												  FlowUi::TextureFitMode::Contain,
												  FlowUi::TextureSamplingMode::Nearest, false);
	fixture.set("texture", texture);
	auto plan = fixture.pipeline.query_review();
	assert(plan.can_commit && plan.counts.errors == 0 && plan.counts.additions == 2);
	assert(fixture.pipeline.commit_review(plan).applied);
	assert(fixture.pipeline.query_review().counts.pending() == 0);
	texture.tintEnabled = true;
	fixture.set("texture", texture);
	plan = fixture.pipeline.query_review();
	assert(plan.can_commit && plan.counts.updates == 1);
	const auto texture_row = std::find_if(plan.rows.begin(), plan.rows.end(), [](const auto& row) {
		return row.field_path == "texture";
	});
	assert(texture_row != plan.rows.end() &&
		   texture_row->proposal.find("\"tintEnabled\":true") != std::string::npos);
	assert(fixture.pipeline.commit_review(plan).applied);
	assert(fixture.pipeline.query_review().counts.pending() == 0);
}

void verify_saved_header_with_source_reflection(const std::filesystem::path& directory) {
	FlowUi::devMode::DevSchemaRegistry schemas;
	DevOverrideEngine overrides{schemas};
	DevBakePipeline pipeline{schemas, overrides};
	schemas.ensureElement<changes_review_test::SourceElement>();
	assert(schemas.publishPendingAtSafePoint());
	const auto schema = schemas.view();
	const auto* element = schema->findElement(changes_review_test::SourceElement::definitionId);
	assert(element);
	const auto fields = schema->fieldsOf(element->parametersType);
	assert(fields.size() == 1 && schema->string(fields.front().source.file).ends_with(".cpp"));
	const auto* value_type = schema->type(fields.front().valueType);
	const auto hex = [](std::uint64_t identity) {
		char characters[32]{};
		const auto written =
			std::to_chars(characters, characters + sizeof(characters), identity, 16);
		assert(written.ec == std::errc{});
		return "0x" + std::string(characters, written.ptr);
	};
	const std::string saved_header = "tests/DevSystemsTests/DevChangesReviewTestSchema.hpp";
	const std::string contents =
		"{\"manifestVersion\":1,\"bakedChanges\":[{\"targetKind\":\"Element\",\"targetScope\":"
		"\"Definition\",\"definitionId\":\"" +
		hex(element->definitionId.value) + "\",\"fieldId\":\"" + hex(fields.front().id) +
		"\",\"fieldPath\":\"amount\",\"valueTypeId\":\"" + hex(value_type->id) +
		"\",\"valueType\":" +
		FlowUi::detail::quote_capacity_json(schema->string(value_type->cppTypeName)) +
		",\"ownerCppType\":" +
		FlowUi::detail::quote_capacity_json(
			schema->string(schema->type(element->parametersType)->cppTypeName)) +
		",\"sourceHeader\":" + FlowUi::detail::quote_capacity_json(saved_header) +
		",\"cppValue\":\"2\",\"valueJson\":\"2\"}]}";
	const auto path = directory / "source-reflection.flowchanges";
	write_file(path, contents);
	pipeline.setManifestPath(path);
	const auto plan = pipeline.query_review();
	assert(plan.valid_disk && plan.counts.errors == 0 && plan.counts.pending() == 0);
	assert(plan.candidate.entries.front().sourceHeader == saved_header);
	auto unavailable = contents;
	replace_once(unavailable, FlowUi::detail::quote_capacity_json(saved_header), "\"\"");
	write_file(path, unavailable);
	assert(pipeline.query_review().counts.errors == 1);
}

} // namespace

int main(int argument_count, char** arguments) {
	assert(argument_count == 2);
	const std::filesystem::path directory(arguments[1]);
	std::filesystem::remove_all(directory);
	std::filesystem::create_directories(directory);
	verify_owned_atomic_memory(directory);
	verify_retention_decisions_and_revision(directory);
	verify_conflicts_and_failures(directory);
	verify_external_reconciliation_and_unresolved(directory);
	verify_invalid_memory_omission(directory);
	verify_provenance_and_semantic_retention(directory);
	verify_generated_value_and_metadata_repairs(directory);
	verify_adjacent_growth_factors(directory);
	verify_explicit_entry_removal_rebase(directory, false);
	verify_explicit_entry_removal_rebase(directory, true);
	verify_memory_removal_rebase(directory);
	verify_unknown_memory_members(directory);
	verify_stable_semantic_values(directory);
	verify_saved_header_with_source_reflection(directory);
	std::cout << "Changes review retention, decisions, conflicts, revisions, atomic writes and "
				 "memory passed.\n";
}
