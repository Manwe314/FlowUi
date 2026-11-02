#ifdef NDEBUG
#undef NDEBUG
#endif
#include "HeadlessVulkanFixture.hpp"
#include "devSystems/devTooling/bake/DevChangesRuntime.hpp"
#include "internal/StorageSystem/FlowStorageSystem.hpp"
#include "managers/ActionManager.hpp"
#include "managers/ThemeManager.hpp"
#include <algorithm>
#include <cassert>
#include <iostream>

using namespace FlowUi;
using namespace FlowUi::devSystems::tooling;

namespace changes_runtime_test {
struct Theme {
	ActionCall action{};
	TextureRef texture = TextureRef::fromStable(ResourceDomain::Icon, "owned.fixture");
	float radius = 4;
};
FLOWUI_DEV_SCHEMA(Theme, FLOWUI_DEV_FIELD(Theme, action), FLOWUI_DEV_FIELD(Theme, texture),
				  FLOWUI_DEV_FIELD(Theme, radius))
struct TexturePair {
	TextureRef first{}, second{};
};
FLOWUI_DEV_SCHEMA(TexturePair, FLOWUI_DEV_FIELD(TexturePair, first),
				  FLOWUI_DEV_FIELD(TexturePair, second))

struct Fixture {
	detail::storage::FlowStorageSystem storage;
	devMode::DevSchemaRegistry registry{};
	DevOverrideEngine engine{registry};
	ThemeManager themes{};
	devMode::DevSchemaView schema{};
	std::uint64_t transaction = 0;

	explicit Fixture(VulkanContext& context) : storage(context) {
		storage.initialize({});
		themes.init(storage);
		assert(themes.registerTheme<Theme>("dark", Theme{}));
		assert(themes.registerTheme<Theme>("light", Theme{.radius = 8}, false));
		registry.ensureTheme<Theme>();
		registry.ensureStruct<TexturePair>();
		assert(registry.publishPendingAtSafePoint());
		schema = registry.view();
	}
	[[nodiscard]] DevChangesReviewRow row(std::string_view name, std::string value_json,
										  std::string variant = "dark") const {
		const auto* registered = schema->findTheme(detail::typeHash<Theme>());
		assert(registered);
		const auto fields = schema->fieldsOf(registered->themeType);
		const auto found = std::ranges::find_if(
			fields, [&](const auto& field) { return schema->string(field.name) == name; });
		assert(found != fields.end());
		DevBakeEntry entry;
		entry.targetKind = DevBakeTargetKind::Theme;
		entry.themeType = detail::typeHash<Theme>();
		entry.themeVariant = std::move(variant);
		entry.fieldId = found->id;
		entry.fieldPath = name;
		entry.valueType = schema->type(found->valueType)->id;
		entry.valueJson = std::move(value_json);
		DevChangesReviewRow result;
		result.domain = DevChangesDomain::Themes;
		result.disk_entry = entry;
		result.has_disk = true;
		return result;
	}
	[[nodiscard]] DevChangesRuntimeEdit prepare(const DevChangesReviewRow& row,
												DevChangesRuntimeAction action) const noexcept {
		return prepare_changes_runtime_edit(*schema, engine, themes, row, InvalidWindowId, action);
	}
	void commit(std::vector<DevOverrideCommand> commands) {
		assert(engine.submit({.transaction = ++transaction, .commands = std::move(commands)}));
		engine.commitAtSafePoint(themes);
		assert(!engine.commandResults().empty());
		if (!engine.commandResults().front().applied)
			std::cerr << "Rejected transaction " << transaction << ": status "
					  << static_cast<unsigned>(engine.commandResults().front().status) << '\n';
		assert(engine.commandResults().front().applied);
	}
	void reject(DevOverrideCommand command, DevCommandStatus status) {
		assert(engine.submit({.transaction = ++transaction, .commands = {std::move(command)}}));
		engine.commitAtSafePoint(themes);
		if (engine.commandResults().empty() || engine.commandResults().front().applied)
			std::cerr << "Unexpected acceptance of transaction " << transaction << " kind "
					  << static_cast<unsigned>(command.kind) << " expected status "
					  << static_cast<unsigned>(status) << '\n';
		assert(!engine.commandResults().empty() && !engine.commandResults().front().applied);
		assert(engine.commandResults().front().status == status);
	}
};

void verify_theme_decisions(Fixture& fixture) {
	const auto row = fixture.row("radius", "9");
	auto incompatible_saved = row;
	incompatible_saved.proposed_entry = incompatible_saved.disk_entry;
	incompatible_saved.disk_entry->valueType = 0;
	assert(!fixture.prepare(incompatible_saved, DevChangesRuntimeAction::ApplySaved).available);
	auto edit = fixture.prepare(row, DevChangesRuntimeAction::ApplySaved);
	assert(edit.available && edit.forward.front().preserve_baked_assignment);
	fixture.commit(edit.forward);
	assert(fixture.themes.getTheme<Theme>("dark").radius == 9);
	assert(fixture.themes.getTheme<Theme>("light").radius == 8);
	edit = fixture.prepare(row, DevChangesRuntimeAction::DiscardLive);
	assert(edit.available && edit.forward.front().kind == DevOverrideCommandKind::ClearThemeField);
	fixture.commit(edit.forward);
	assert(fixture.themes.getTheme<Theme>("dark").radius == 4);
	assert(fixture.engine.themeBakeRecords().empty());
	assert(fixture.engine.themeBakeTombstones().empty());
	fixture.commit(edit.inverse);
	assert(fixture.themes.getTheme<Theme>("dark").radius == 9);
	edit = fixture.prepare(row, DevChangesRuntimeAction::ResetAuthored);
	assert(edit.available);
	fixture.commit(edit.forward);
	assert(fixture.themes.getTheme<Theme>("dark").radius == 4);
	fixture.commit(edit.inverse);
	assert(fixture.themes.getTheme<Theme>("dark").radius == 9);
	fixture.commit(fixture.prepare(row, DevChangesRuntimeAction::DiscardLive).forward);
	const auto broad = fixture.row("radius", "12", "*");
	edit = fixture.prepare(broad, DevChangesRuntimeAction::ApplySaved);
	assert(edit.available);
	fixture.commit(edit.forward);
	assert(fixture.themes.getTheme<Theme>("dark").radius == 12);
	assert(fixture.themes.getTheme<Theme>("light").radius == 12);
	assert(!fixture.prepare(broad, DevChangesRuntimeAction::ResetAuthored).available);
	edit = fixture.prepare(broad, DevChangesRuntimeAction::DiscardLive);
	assert(edit.available);
	fixture.commit(edit.forward);
	assert(fixture.themes.getTheme<Theme>("dark").radius == 4);
	assert(fixture.themes.getTheme<Theme>("light").radius == 8);
	assert(fixture.engine.themeBakeTombstones().empty());
	fixture.commit(edit.inverse);
	assert(fixture.themes.getTheme<Theme>("dark").radius == 12);
	assert(fixture.themes.getTheme<Theme>("light").radius == 12);
}

void verify_stable_actions_and_resource_diagnostics(Fixture& fixture) {
	for (const auto kind : {ActionCallKind::App, ActionCallKind::Ui}) {
		const auto row =
			fixture.row("action", "{\"id\":18446744073709551615,\"kind\":" +
									  std::to_string(static_cast<unsigned>(kind)) + "}");
		const auto edit = fixture.prepare(row, DevChangesRuntimeAction::ApplySaved);
		assert(edit.available);
		fixture.commit(edit.forward);
		const auto reference = fixture.themes.getTheme<Theme>("dark").action.stableReference();
		assert(reference.kind == kind && reference.id == UINT64_MAX);
		fixture.commit(edit.inverse);
	}
	const auto clear = fixture.prepare(fixture.row("action", "{\"kind\":0,\"id\":0}"),
									   DevChangesRuntimeAction::ApplySaved);
	assert(clear.available);
	fixture.commit(clear.forward);
	assert(!fixture.themes.getTheme<Theme>("dark").action);
	for (const auto malformed :
		 {"{\"kind\":3,\"id\":1}", "{\"kind\":1,\"id\":0}",
		  "{\"kind\":1,\"id\":18446744073709551616}", "{\"kind\":1,\"id\":1,\"id\":2}",
		  "{\"kind\":1,\"id\":1.5}", "{\"kind\":1}"})
		assert(
			!fixture.prepare(fixture.row("action", malformed), DevChangesRuntimeAction::ApplySaved)
				 .available);
	const auto legacy_texture =
		fixture.prepare(fixture.row("texture", "{\"domain\":2,\"key\":\"saved.key\"}"),
						DevChangesRuntimeAction::ApplySaved);
	assert(!legacy_texture.available &&
		   legacy_texture.error.find("legacy resource metadata") != std::string::npos);
	const std::string texture_json =
		"{\"domain\":" + std::to_string(static_cast<unsigned>(ResourceDomain::Icon)) +
		",\"key\":\"saved.owned.\\\"key\",\"fitMode\":2,\"samplingMode\":1,\"tintEnabled\":true}";
	auto texture_row = fixture.row("texture", texture_json);
	auto texture_edit = fixture.prepare(texture_row, DevChangesRuntimeAction::ApplySaved);
	assert(texture_edit.available);
	auto copied_edit = texture_edit;
	texture_edit = {};
	fixture.commit(copied_edit.forward);
	const auto applied_texture = fixture.themes.getTheme<Theme>("dark").texture;
	assert(applied_texture.sourceKey == "saved.owned.\"key" &&
		   applied_texture.fitMode == TextureFitMode::Cover &&
		   applied_texture.samplingMode == TextureSamplingMode::Nearest &&
		   applied_texture.tintEnabled);
	fixture.commit(copied_edit.inverse);
	fixture.commit(copied_edit.forward);
	for (const auto malformed :
		 {"{\"domain\":0,\"key\":\"x\",\"fitMode\":2,\"samplingMode\":1,\"tintEnabled\":true}",
		  "{\"domain\":2,\"key\":\"x\",\"fitMode\":4,\"samplingMode\":1,\"tintEnabled\":true}",
		  "{\"domain\":2,\"key\":\"x\",\"fitMode\":2,\"samplingMode\":2,\"tintEnabled\":true}"})
		assert(
			!fixture.prepare(fixture.row("texture", malformed), DevChangesRuntimeAction::ApplySaved)
				 .available);
	DevOverrideCommand retained_texture;
	retained_texture.kind = DevOverrideCommandKind::SetThemeField;
	retained_texture.theme = {.themeType = detail::typeHash<Theme>(), .variant = "dark"};
	retained_texture.field = {detail::typeHash<Theme>(), texture_row.disk_entry->fieldId};
	const auto stable_texture = TextureRef::fromStable(ResourceDomain::Icon, "live.fixture");
	assert(fixture.engine.copyValue(stable_texture, retained_texture.value) ==
		   devMode::DevValueOperationStatus::Success);
	fixture.commit({retained_texture});
	texture_row.proposed_entry = texture_row.disk_entry;
	texture_row.has_proposal = true;
	const auto retained_edit = fixture.prepare(texture_row, DevChangesRuntimeAction::ApplyProposal);
	assert(retained_edit.available);
	fixture.commit(retained_edit.forward);
	assert(fixture.themes.getTheme<Theme>("dark").texture.sourceKey == "live.fixture");
	for (const auto invalid_fit : {true, false}) {
		auto invalid_texture = stable_texture;
		if (invalid_fit)
			invalid_texture.fitMode = static_cast<TextureFitMode>(99);
		else
			invalid_texture.samplingMode = static_cast<TextureSamplingMode>(99);
		assert(fixture.engine.copyValue(invalid_texture, retained_texture.value) ==
			   devMode::DevValueOperationStatus::Success);
		fixture.reject(retained_texture, DevCommandStatus::ConstraintRejected);
	}
	const auto* texture_type = fixture.schema->findType(detail::typeHash<TextureRef>());
	assert(texture_type);
	const auto texture_fields = fixture.schema->fieldsOf(devMode::DevTypeIndex{
		static_cast<std::uint32_t>(texture_type - fixture.schema->types.data())});
	const auto handle = std::ranges::find_if(texture_fields, [&](const auto& field) {
		return fixture.schema->string(field.name) == "handle";
	});
	assert(handle != texture_fields.end());
	retained_texture.field.nestedPath = {texture_row.disk_entry->fieldId};
	retained_texture.field.field = handle->id;
	assert(fixture.engine.copyValue(TextureHandle{}, retained_texture.value) ==
		   devMode::DevValueOperationStatus::Success);
	fixture.reject(retained_texture, DevCommandStatus::FieldReadOnly);
	ActionManager actions;
	bool bound_target = false;
	constexpr auto toggle =
		UiAction("changes.runtime.bound.toggle", [](bool& target) { target = !target; });
	const ActionCall bound_action{actions.uiActions().make(toggle, bound_target)};
	assert(bound_action && bound_action.stableReference().kind == ActionCallKind::None);
	DevOverrideCommand unstable_action;
	unstable_action.kind = DevOverrideCommandKind::SetThemeField;
	unstable_action.theme = {.themeType = detail::typeHash<Theme>(), .variant = "dark"};
	unstable_action.field = {detail::typeHash<Theme>(),
							 fixture.row("action", "").disk_entry->fieldId};
	assert(fixture.engine.copyValue(bound_action, unstable_action.value) ==
		   devMode::DevValueOperationStatus::Success);
	fixture.reject(unstable_action, DevCommandStatus::ConstraintRejected);
}

void verify_owned_resource_copies(Fixture& fixture) {
	DevOwnedValue original;
	{
		const std::string first_key = "a short key";
		const std::string second_key(200, 'z');
		TexturePair pair{TextureRef::fromStable(ResourceDomain::Image, first_key),
						 TextureRef::fromStable(ResourceDomain::Icon, second_key)};
		assert(fixture.engine.copyValue(pair, original) ==
			   devMode::DevValueOperationStatus::Success);
	}
	auto copied = original;
	DevOwnedValue moved = std::move(original);
	original.reset();
	DevOwnedValue captured;
	const auto* type = fixture.schema->findType(detail::typeHash<TexturePair>());
	assert(type);
	assert(
		DevOwnedValue::copyFrom(
			*fixture.schema,
			devMode::DevTypeIndex{static_cast<std::uint32_t>(type - fixture.schema->types.data())},
			moved.data(), captured) == devMode::DevValueOperationStatus::Success);
	moved.reset();
	for (const auto* value : {&copied, &captured}) {
		const auto& pair = *static_cast<const TexturePair*>(value->data());
		assert(pair.first.sourceKey == "a short key");
		assert(pair.second.sourceKey == std::string(200, 'z'));
	}
	auto modified = captured;
	auto& modified_pair = *static_cast<TexturePair*>(modified.data());
	modified_pair.first = {};
	modified_pair.second.sourceKey = "a replacement key from a typed editor";
	auto copied_modified = modified;
	modified.reset();
	const auto& copied_pair = *static_cast<const TexturePair*>(copied_modified.data());
	assert(copied_pair.first.sourceKey.empty());
	assert(copied_pair.second.sourceKey == "a replacement key from a typed editor");
	std::string_view outside;
	assert(!captured.retain_text_reference(outside, "outside object"));
}

void verify_resource_baseline_lifetimes(Fixture& fixture) {
	fixture.commit(
		fixture.prepare(fixture.row("texture", ""), DevChangesRuntimeAction::DiscardLive).forward);
	const auto texture_json = [](std::string_view key) {
		return "{\"domain\":" + std::to_string(static_cast<unsigned>(ResourceDomain::Icon)) +
			   ",\"key\":\"" + std::string(key) +
			   "\",\"fitMode\":1,\"samplingMode\":0,\"tintEnabled\":false}";
	};
	const std::string broad_key = "broad.saved.resource.key.with.owned.heap.storage";
	const auto broad = fixture.row("texture", texture_json(broad_key), "*");
	fixture.commit(fixture.prepare(broad, DevChangesRuntimeAction::ApplySaved).forward);
	assert(fixture.themes.clone_dev_variant(detail::typeHash<Theme>(), "dark", "cloned-resource"));
	const auto named =
		fixture.row("texture", texture_json("named.saved.resource.key.with.owned.heap.storage"));
	fixture.commit(fixture.prepare(named, DevChangesRuntimeAction::ApplySaved).forward);
	fixture.commit(fixture.prepare(broad, DevChangesRuntimeAction::DiscardLive).forward);
	assert(fixture.themes.getTheme<Theme>("cloned-resource").texture.sourceKey == broad_key);
	auto undo_named = fixture.prepare(named, DevChangesRuntimeAction::DiscardLive);
	fixture.commit(undo_named.forward);
	assert(fixture.themes.getTheme<Theme>("dark").texture.sourceKey == "owned.fixture");
	fixture.commit(undo_named.inverse);
	assert(fixture.themes.getTheme<Theme>("dark").texture.sourceKey ==
		   "named.saved.resource.key.with.owned.heap.storage");
	undo_named = {};
	fixture.commit(fixture.prepare(named, DevChangesRuntimeAction::DiscardLive).forward);
	assert(fixture.themes.getTheme<Theme>("dark").texture.sourceKey == "owned.fixture");
}

void verify_resource_budget_failure_acknowledgement(Fixture& fixture) {
	const auto statistics = fixture.storage.stats();
	std::uint64_t live_cpu_bytes = 0;
	for (const auto& memory : statistics.cpu)
		live_cpu_bytes += memory.liveBytes;
	const auto rejection_budget = live_cpu_bytes + 8;
	fixture.storage.setBudget(rejection_budget, statistics.gpuSoftBudgetBytes);
	// Metadata statistics include heap-owned registries; exceed the entire pool budget so the
	// real string allocation must fail independently of those accounting details.
	const std::string unavailable_key(static_cast<std::size_t>(rejection_budget + 1), 'r');
	const auto original_key = std::string(fixture.themes.getTheme<Theme>("dark").texture.sourceKey);
	const auto row = fixture.row(
		"texture", "{\"domain\":" + std::to_string(static_cast<unsigned>(ResourceDomain::Icon)) +
					   ",\"key\":\"" + unavailable_key +
					   "\","
					   "\"fitMode\":1,\"samplingMode\":0,\"tintEnabled\":false}");
	const auto draft = fixture.prepare(row, DevChangesRuntimeAction::ApplySaved);
	assert(draft.available);
	for (const auto kind :
		 {DevOverrideCommandKind::SetThemeField, DevOverrideCommandKind::BeginBatchDrag,
		  DevOverrideCommandKind::UpdateBatchDrag, DevOverrideCommandKind::EndBatchDrag}) {
		auto command = draft.forward.front();
		command.kind = kind;
		fixture.reject(std::move(command), DevCommandStatus::ValueCopyFailed);
		assert(fixture.engine.commandResults().front().transaction == fixture.transaction);
		assert(fixture.themes.getTheme<Theme>("dark").texture.sourceKey == original_key);
	}
	const auto rejected_transaction = ++fixture.transaction;
	assert(fixture.engine.submit({.transaction = rejected_transaction, .commands = draft.forward}));
	const auto valid_transaction = ++fixture.transaction;
	const auto valid =
		fixture.prepare(fixture.row("radius", "7"), DevChangesRuntimeAction::ApplySaved);
	assert(valid.available);
	assert(fixture.engine.submit({.transaction = valid_transaction, .commands = valid.forward}));
	fixture.engine.commitAtSafePoint(fixture.themes);
	assert(fixture.engine.commandResults().size() == 2);
	assert(fixture.engine.commandResults()[0].transaction == rejected_transaction &&
		   !fixture.engine.commandResults()[0].applied);
	assert(fixture.engine.commandResults()[1].transaction == valid_transaction &&
		   fixture.engine.commandResults()[1].applied);
	assert(fixture.themes.getTheme<Theme>("dark").radius == 7);
	fixture.storage.setBudget(statistics.cpuSoftBudgetBytes, statistics.gpuSoftBudgetBytes);
}
} // namespace changes_runtime_test

int main() {
	try {
		test::HeadlessVulkanFixture context;
		changes_runtime_test::Fixture fixture(context.context());
		changes_runtime_test::verify_theme_decisions(fixture);
		changes_runtime_test::verify_stable_actions_and_resource_diagnostics(fixture);
		changes_runtime_test::verify_owned_resource_copies(fixture);
		changes_runtime_test::verify_resource_baseline_lifetimes(fixture);
		changes_runtime_test::verify_resource_budget_failure_acknowledgement(fixture);
		std::cout << "Theme discard/reset/undo, stable action decoding, and resource ownership "
					 "diagnostics passed\n";
		return 0;
	} catch (const test::VulkanUnavailable& error) {
		std::cout << error.what() << '\n';
		return 77;
	}
}
