#include "HeadlessVulkanFixture.hpp"
#include "TestHarness.hpp"

#include <cstdint>
#include <iostream>
#include <stdexcept>
#include <string>

#include "FlowUi/Private/resourceService/ThemeStorageController.hpp"
#include "FlowUi/Private/storageSystem/StorageBinding.hpp"
#include "FlowUi/Public/FlowUiTheme.hpp"
#include "FlowUi/Public/managers/ThemeManager.hpp"
#include "FlowUi/Public/managers/UiManager.hpp"
#include "clay.h"
#if FLOW_UI_DEV_MODE
#include "devSystems/devTooling/DevTooling.hpp"
#endif

namespace {

struct TestAppTheme {
	Clay_Color brandColor = { 0.2f, 0.4f, 0.8f, 1.0f };
	float cornerRadius = 8.0f;
	int spacing = 12;
};

#if FLOW_UI_DEV_MODE
FLOWUI_DEV_SCHEMA(
	TestAppTheme,
	FLOWUI_DEV_FIELD(TestAppTheme, brandColor),
	FLOWUI_DEV_FIELD(TestAppTheme, cornerRadius),
	FLOWUI_DEV_FIELD(TestAppTheme, spacing))
#endif

struct TestEditorTheme {
	int rowHeight = 24;
};

struct alignas(64) TestOverAlignedTheme {
	int marker = 0;
};

void testFailedRegistrationLeavesNoThemeType(FlowUi::test::HeadlessVulkanFixture& vulkan) {
	for (uint32_t checkpoint = 1; checkpoint <= 4; ++checkpoint) {
		FlowUi::detail::storage::ActiveStorageSystem storage(vulkan.context());
		storage.initialize(FlowUi::detail::storage::StorageConfig{});
		FlowUi::detail::storage::StorageBinding storageBinding;
		storageBinding.bind(storage);
		FlowUi::detail::manager_storage::ThemeStorageController controller;
		controller.init(storageBinding);
		const auto variant = controller.internString("failure-retry");
		storage.setManagerFailureCountdown(checkpoint);
		FLOWUI_CHECK_THROWS(controller.registerThemeVariant<TestAppTheme>(variant, TestAppTheme{}, true));
		FLOWUI_CHECK(!controller.hasThemeType<TestAppTheme>());
		FLOWUI_CHECK(storage.resourceStats(FlowUi::detail::storage::ResourceKind::UiTheme).live == 0);
		storage.setManagerFailureCountdown(0);
		const auto recovered = controller.registerThemeVariant<TestAppTheme>(variant, TestAppTheme{}, true);
		FLOWUI_CHECK(recovered);
		FLOWUI_CHECK(controller.getActiveThemeVariant<TestAppTheme>() != nullptr);
		controller.shutdown();
	}
}

void testSealedFrameRejectsThemeMutationBeforeEffects(FlowUi::test::HeadlessVulkanFixture& vulkan) {
	FlowUi::detail::storage::ActiveStorageSystem storage(vulkan.context());
	storage.initialize(FlowUi::detail::storage::StorageConfig{});
	FlowUi::detail::storage::StorageBinding storageBinding;
	storageBinding.bind(storage);
	FlowUi::ThemeManager themes;
	themes.init(storageBinding);
	FLOWUI_CHECK(themes.registerTheme<TestEditorTheme>("first", TestEditorTheme{11}, true));
	FLOWUI_CHECK(themes.registerTheme<TestEditorTheme>("second", TestEditorTheme{22}, false));
	storage.registerWindow(7, FlowUi::detail::storage::WindowStorageDesc{});
	const auto frame = storage.beginFrame(7, FlowUi::detail::storage::FrameStorageDesc{});
	const auto lease = storage.sealFrame(frame);
	FLOWUI_CHECK(lease);
	const auto selection = themes.setActiveVariant<TestEditorTheme>("second");
	FLOWUI_CHECK(!selection);
	FLOWUI_CHECK(selection.error().code == FlowUi::ErrorCode::StorageMutationSealed);
	FLOWUI_CHECK(FlowUi::test::requireValue(themes.getActiveTheme<TestEditorTheme>()).get().rowHeight == 11);
	const auto replacement = themes.registerTheme<TestEditorTheme>("first", TestEditorTheme{33}, true);
	FLOWUI_CHECK(!replacement);
	FLOWUI_CHECK(replacement.error().code == FlowUi::ErrorCode::StorageMutationSealed);
	FLOWUI_CHECK(FlowUi::test::requireValue(themes.getTheme<TestEditorTheme>("first")).get().rowHeight == 11);
	storage.cancelFrame(frame);
	storage.unregisterWindow(7, 0);
}

void testStagedAndDevPhaseChecksPrecedeEffects(FlowUi::test::HeadlessVulkanFixture& vulkan) {
	using namespace FlowUi;
	detail::storage::ActiveStorageSystem storage(vulkan.context());
	storage.initialize({});
	FlowUi::detail::storage::StorageBinding storageBinding;
	storageBinding.bind(storage);
	detail::manager_storage::ThemeStorageController controller;
	controller.init(storageBinding);
	const auto first = controller.internString("first");
	const auto second = controller.internString("second");
	const auto firstHandle = controller.registerThemeVariant<TestEditorTheme>(first, TestEditorTheme{11}, true);
	const auto secondHandle = controller.registerThemeVariant<TestEditorTheme>(second, TestEditorTheme{22}, false);
	bool invoked = false;
	FLOWUI_CHECK(controller.queueActiveThemeMutation<TestEditorTheme>([&](TestEditorTheme& theme) {
		invoked = true;
		theme.rowHeight = 33;
	}));
	storage.registerWindow(7, detail::storage::WindowStorageDesc{});
	const auto frame = storage.beginFrame(7, detail::storage::FrameStorageDesc{});
	FLOWUI_CHECK(storage.sealFrame(frame));
	FLOWUI_CHECK_THROWS(controller.applyStagedMutations());
	FLOWUI_CHECK(!invoked);
	FLOWUI_CHECK(controller.getActiveThemeVariant<TestEditorTheme>()->rowHeight == 11);
#if FLOW_UI_DEV_MODE
	FLOWUI_CHECK(!controller.set_dev_active_variant(detail::typeHash<TestEditorTheme>(), "second"));
	devSystems::tooling::DevFieldOps field{};
	field.assignMemberFromCopy = [](void* owner, const void* source) noexcept {
		static_cast<TestEditorTheme*>(owner)->rowHeight = *static_cast<const int*>(source);
		return devSystems::tooling::DevValueOperationStatus::Success;
	};
	const int replacement = 44;
	FLOWUI_CHECK(controller.assignDevField(detail::typeHash<TestEditorTheme>(), "first", {}, field,
		&replacement) == devSystems::tooling::DevValueOperationStatus::Failed);
	FLOWUI_CHECK(controller.getActiveThemeVariant<TestEditorTheme>()->rowHeight == 11);
#endif
	storage.cancelFrame(frame);
	controller.applyStagedMutations();
	FLOWUI_CHECK(invoked);
	FLOWUI_CHECK(controller.getActiveThemeVariant<TestEditorTheme>()->rowHeight == 33);
	const auto cleanupFrame = storage.beginFrame(7, detail::storage::FrameStorageDesc{});
	FLOWUI_CHECK(storage.sealFrame(cleanupFrame));
	controller.shutdown();
	// Destructor cleanup contains the sealed-phase rejection. Storage still owns
	// both payload records and will destroy them during final owner shutdown.
	FLOWUI_CHECK(storage.managerRecordData(firstHandle, detail::storage::ResourceKind::UiTheme));
	FLOWUI_CHECK(storage.managerRecordData(secondHandle, detail::storage::ResourceKind::UiTheme));
#if FLOW_UI_DEV_MODE
	FLOWUI_CHECK(storage.resourceStats(detail::storage::ResourceKind::UiTheme).live == 2);
#endif
	storage.cancelFrame(cleanupFrame);
	storage.unregisterWindow(7, 0);
	storage.shutdown();
}

struct ThrowingAssignmentTheme {
	int marker = 0;
	ThrowingAssignmentTheme() = default;
	explicit ThrowingAssignmentTheme(int value) : marker(value) {}
	ThrowingAssignmentTheme(const ThrowingAssignmentTheme&) = default;
	ThrowingAssignmentTheme(ThrowingAssignmentTheme&&) = default;
	ThrowingAssignmentTheme& operator=(const ThrowingAssignmentTheme&) = default;
	ThrowingAssignmentTheme& operator=(ThrowingAssignmentTheme&& source) {
		marker = source.marker;
		throw 73;
	}
};

void testArbitraryThemeFailureRequiresAbandonment(FlowUi::test::HeadlessVulkanFixture& vulkan) {
	using namespace FlowUi;
	detail::storage::ActiveStorageSystem storage(vulkan.context());
	storage.initialize({});
	FlowUi::detail::storage::StorageBinding storageBinding;
	storageBinding.bind(storage);
	ThemeManager themes;
	themes.init(storageBinding);
	FLOWUI_CHECK(themes.registerTheme<ThrowingAssignmentTheme>("default", ThrowingAssignmentTheme{11}));
	FLOWUI_CHECK(themes.updateActiveTheme<ThrowingAssignmentTheme>([](ThrowingAssignmentTheme& theme) {
		theme.marker = 22;
	}));
	themes.applyStagedMutations();
	FLOWUI_CHECK(test::requireValue(themes.getActiveTheme<ThrowingAssignmentTheme>()).get().marker == 22);
	bool caughtOriginal = false;
	try {
		(void)themes.registerTheme<ThrowingAssignmentTheme>("default", ThrowingAssignmentTheme{33});
	} catch (int exception) {
		caughtOriginal = exception == 73;
	}
	FLOWUI_CHECK(caughtOriginal);
	FLOWUI_CHECK(test::requireValue(themes.getActiveTheme<ThrowingAssignmentTheme>()).get().marker == 33);
}

void testThemeRegistrationAndAccess(FlowUi::test::HeadlessVulkanFixture& vulkan) {
	FlowUi::detail::storage::ActiveStorageSystem storage(vulkan.context());
	FlowUi::detail::storage::StorageConfig config{};
	storage.initialize(config);

	FlowUi::detail::storage::StorageBinding storageBinding;
	storageBinding.bind(storage);
	FlowUi::ThemeManager themes;
	themes.init(storageBinding);

	TestAppTheme defaultTheme{
		.brandColor = { 0.1f, 0.2f, 0.3f, 1.0f },
		.cornerRadius = 4.0f,
		.spacing = 8
	};

	FLOWUI_CHECK(themes.registerTheme<TestAppTheme>("default", defaultTheme, true));

	const TestAppTheme& active = ::FlowUi::test::requireValue(themes.getActiveTheme<TestAppTheme>()).get();
	FLOWUI_CHECK(active.cornerRadius == 4.0f);
	FLOWUI_CHECK(active.spacing == 8);
	FLOWUI_CHECK(active.brandColor.r == 0.1f);

	const TestAppTheme& named = ::FlowUi::test::requireValue(themes.getTheme<TestAppTheme>("default")).get();
	FLOWUI_CHECK(named.cornerRadius == 4.0f);
}

void testMultipleVariantsAndSwitching(FlowUi::test::HeadlessVulkanFixture& vulkan) {
	FlowUi::detail::storage::ActiveStorageSystem storage(vulkan.context());
	FlowUi::detail::storage::StorageConfig config{};
	storage.initialize(config);

	FlowUi::detail::storage::StorageBinding storageBinding;
	storageBinding.bind(storage);
	FlowUi::ThemeManager themes;
	themes.init(storageBinding);

	TestAppTheme darkVariant{ .cornerRadius = 6.0f, .spacing = 10 };
	TestAppTheme lightVariant{ .cornerRadius = 14.0f, .spacing = 20 };

	FLOWUI_CHECK(themes.registerTheme<TestAppTheme>("dark", darkVariant, true));
	FLOWUI_CHECK(themes.registerTheme<TestAppTheme>("light", lightVariant, false));

	FLOWUI_CHECK(::FlowUi::test::requireValue(themes.getActiveTheme<TestAppTheme>()).get().cornerRadius == 6.0f);
	FLOWUI_CHECK(::FlowUi::test::requireValue(themes.getTheme<TestAppTheme>("light")).get().cornerRadius == 14.0f);

	auto switched = themes.setActiveVariant<TestAppTheme>("light");
	FLOWUI_CHECK(switched);
	FLOWUI_CHECK(::FlowUi::test::requireValue(themes.getActiveTheme<TestAppTheme>()).get().cornerRadius == 14.0f);
	FLOWUI_CHECK(::FlowUi::test::requireValue(themes.getActiveTheme<TestAppTheme>()).get().spacing == 20);

	switched = themes.setActiveVariant<TestAppTheme>("dark");
	FLOWUI_CHECK(switched);
	FLOWUI_CHECK(::FlowUi::test::requireValue(themes.getActiveTheme<TestAppTheme>()).get().cornerRadius == 6.0f);
}

void testDifferentThemeTypesCanShareVariantNames(FlowUi::test::HeadlessVulkanFixture& vulkan) {
	FlowUi::detail::storage::ActiveStorageSystem storage(vulkan.context());
	FlowUi::detail::storage::StorageConfig config{};
	storage.initialize(config);

	FlowUi::detail::storage::StorageBinding storageBinding;
	storageBinding.bind(storage);
	FlowUi::ThemeManager themes;
	themes.init(storageBinding);

	FLOWUI_CHECK(themes.registerTheme<TestAppTheme>(
		"default", TestAppTheme{.cornerRadius = 9.0f, .spacing = 18}, true));
	FLOWUI_CHECK(
		themes.registerTheme<TestEditorTheme>("default", TestEditorTheme{.rowHeight = 32}, true));

	FLOWUI_CHECK(::FlowUi::test::requireValue(themes.getTheme<TestAppTheme>("default")).get().cornerRadius == 9.0f);
	FLOWUI_CHECK(::FlowUi::test::requireValue(themes.getTheme<TestAppTheme>("default")).get().spacing == 18);
	FLOWUI_CHECK(::FlowUi::test::requireValue(themes.getTheme<TestEditorTheme>("default")).get().rowHeight == 32);

	FLOWUI_CHECK(themes.updateTheme<TestEditorTheme>(
		"default", [](TestEditorTheme& theme) { theme.rowHeight = 40; }));
	themes.applyStagedMutations();

	FLOWUI_CHECK(::FlowUi::test::requireValue(themes.getTheme<TestEditorTheme>("default")).get().rowHeight == 40);
	FLOWUI_CHECK(::FlowUi::test::requireValue(themes.getTheme<TestAppTheme>("default")).get().cornerRadius == 9.0f);
}

void testOverAlignedThemePayload(FlowUi::test::HeadlessVulkanFixture& vulkan) {
	FlowUi::detail::storage::ActiveStorageSystem storage(vulkan.context());
	FlowUi::detail::storage::StorageConfig config{};
	storage.initialize(config);

	FlowUi::detail::storage::StorageBinding storageBinding;
	storageBinding.bind(storage);
	FlowUi::ThemeManager themes;
	themes.init(storageBinding);
	FLOWUI_CHECK(themes.registerTheme<TestOverAlignedTheme>(
		"default", TestOverAlignedTheme{.marker = 73}, true));

	const TestOverAlignedTheme& theme = ::FlowUi::test::requireValue(themes.getActiveTheme<TestOverAlignedTheme>()).get();
	FLOWUI_CHECK(theme.marker == 73);
	FLOWUI_CHECK(reinterpret_cast<uintptr_t>(&theme) % alignof(TestOverAlignedTheme) == 0);
}

void testStagedMutations(FlowUi::test::HeadlessVulkanFixture& vulkan) {
	FlowUi::detail::storage::ActiveStorageSystem storage(vulkan.context());
	FlowUi::detail::storage::StorageConfig config{};
	storage.initialize(config);

	FlowUi::detail::storage::StorageBinding storageBinding;
	storageBinding.bind(storage);
	FlowUi::ThemeManager themes;
	themes.init(storageBinding);

	TestAppTheme initialTheme{ .cornerRadius = 5.0f, .spacing = 10 };
	FLOWUI_CHECK(themes.registerTheme<TestAppTheme>("default", initialTheme, true));

	FLOWUI_CHECK(themes.updateActiveTheme<TestAppTheme>([](TestAppTheme& t) {
		t.cornerRadius = 15.0f;
		t.spacing = 30;
	}));

	FLOWUI_CHECK(::FlowUi::test::requireValue(themes.getActiveTheme<TestAppTheme>()).get().cornerRadius == 5.0f);

	themes.applyStagedMutations();

	FLOWUI_CHECK(::FlowUi::test::requireValue(themes.getActiveTheme<TestAppTheme>()).get().cornerRadius == 15.0f);
	FLOWUI_CHECK(::FlowUi::test::requireValue(themes.getActiveTheme<TestAppTheme>()).get().spacing == 30);

	FLOWUI_CHECK(themes.updateActiveTheme<TestAppTheme>([](TestAppTheme& t) {
		t.cornerRadius = 99.0f;
		throw std::runtime_error("theme callback failure");
	}));
	FLOWUI_CHECK_THROWS(themes.applyStagedMutations());
	FLOWUI_CHECK(::FlowUi::test::requireValue(themes.getActiveTheme<TestAppTheme>()).get().cornerRadius == 99.0f);
	FLOWUI_CHECK(::FlowUi::test::requireValue(themes.getActiveTheme<TestAppTheme>()).get().spacing == 30);
}

void testFlowUiThemeDefaultRegistration(FlowUi::test::HeadlessVulkanFixture& vulkan) {
	FlowUi::detail::storage::ActiveStorageSystem storage(vulkan.context());
	FlowUi::detail::storage::StorageConfig config{};
	storage.initialize(config);

	FlowUi::detail::storage::StorageBinding storageBinding;
	storageBinding.bind(storage);
	FlowUi::ThemeManager themes;
	themes.init(storageBinding);

	FLOWUI_CHECK(
		themes.registerTheme<FlowUi::FlowUiTheme>("default", FlowUi::FlowUiTheme::dark(), true));
	FLOWUI_CHECK(
		themes.registerTheme<FlowUi::FlowUiTheme>("light", FlowUi::FlowUiTheme::light(), false));

	const auto& darkTheme = ::FlowUi::test::requireValue(themes.getActiveTheme<FlowUi::FlowUiTheme>()).get();
	FLOWUI_CHECK(darkTheme.fontSizeMedium == 14.0f);

	const auto& lightTheme = ::FlowUi::test::requireValue(themes.getTheme<FlowUi::FlowUiTheme>("light")).get();
	FLOWUI_CHECK(lightTheme.background.r == 1.0f);
	FLOWUI_CHECK(lightTheme.background.g == 1.0f);
	FLOWUI_CHECK(lightTheme.background.b == 1.0f);
}

#if FLOW_UI_DEV_MODE
void testPreBakeBaselines(FlowUi::test::HeadlessVulkanFixture& vulkan) {
	using namespace FlowUi;
	struct alignas(64) OwnedTheme {
		std::string label;
		float radius;
	};
	detail::storage::ActiveStorageSystem storage(vulkan.context());
	storage.initialize({});
	FlowUi::detail::storage::StorageBinding storageBinding;
	storageBinding.bind(storage);
	detail::manager_storage::ThemeStorageController controller;
	controller.init(storageBinding);
	const OwnedTheme authored{"authored-owned-string", 4};
	const OwnedTheme startup{"baked-owned-string", 12};
	(void)controller.registerThemeVariant<OwnedTheme>(controller.internString("default"), startup,
													  true, &authored);
	struct Context {
		bool checked = false;
		bool correct = false;
	} context;
	const auto visitor =
		[](void* data,
		   const detail::manager_storage::ThemeStorageController::DevPayloadView& view) noexcept {
			auto& context = *static_cast<Context*>(data);
			const auto* authored = static_cast<const OwnedTheme*>(view.authored);
			const auto* startup = static_cast<const OwnedTheme*>(view.startup);
			const auto* live = static_cast<const OwnedTheme*>(view.payload);
			context.checked = true;
			context.correct =
				authored && startup && live && authored->label == "authored-owned-string" &&
				authored->radius == 4 && startup->label == "baked-owned-string" &&
				startup->radius == 12 && live->radius == 12 &&
				reinterpret_cast<std::uintptr_t>(authored) % alignof(OwnedTheme) == 0 &&
				reinterpret_cast<std::uintptr_t>(startup) % alignof(OwnedTheme) == 0;
			return true;
		};
	FLOWUI_CHECK(controller.visitDevPayloads(&context, visitor));
	FLOWUI_CHECK(context.checked && context.correct);
	controller.shutdown();
}

void testDevThemeOverrideAndCapture(FlowUi::test::HeadlessVulkanFixture& vulkan) {
	using namespace FlowUi;
	using namespace FlowUi::devSystems::tooling;
	detail::storage::ActiveStorageSystem storage(vulkan.context());
	detail::storage::StorageConfig config{};
	storage.initialize(config);

	FlowUi::detail::storage::StorageBinding storageBinding;
	storageBinding.bind(storage);
	ThemeManager themes;
	themes.init(storageBinding);
	FLOWUI_CHECK(themes.registerTheme<TestAppTheme>(
		"default", TestAppTheme{.cornerRadius = 5.0f, .spacing = 12}, true));

	devSystems::DevTooling tooling;
	tooling.schemas().ensureTheme<TestAppTheme>();
	FLOWUI_CHECK(tooling.schemas().publishPendingAtSafePoint());
	const devSystems::tooling::DevSchemaView schema = tooling.schemas().view();
	const devSystems::tooling::DevTypeId type = detail::typeHash<TestAppTheme>();
	const devSystems::tooling::DevThemeSchema* themeSchema = schema->findTheme(type);
	FLOWUI_CHECK(themeSchema != nullptr);
	const devSystems::tooling::DevTypeSchema* themeType = schema->type(themeSchema->themeType);
	FLOWUI_CHECK(themeType != nullptr);
	DevOverrideFieldKey spacingField{};
	DevOverrideFieldKey redField{};
	for (const devSystems::tooling::DevFieldSchema& field :
		 schema->fieldsOf(themeSchema->themeType)) {
		if (schema->string(field.name) == "spacing") {
			spacingField = {themeType->id, field.id};
		} else if (schema->string(field.name) == "brandColor") {
			for (const devSystems::tooling::DevFieldSchema& colorField :
				 schema->fieldsOf(field.valueType)) {
				if (schema->string(colorField.name) == "r") {
					redField = {themeType->id, colorField.id, {field.id}};
					break;
				}
			}
		}
	}
	FLOWUI_CHECK(spacingField.field != 0);
	FLOWUI_CHECK(redField.field != 0);

	DevOwnedValue overrideValue;
	const int replacement = 27;
	FLOWUI_CHECK(tooling.overrides().copyValue(replacement, overrideValue) ==
				 devSystems::tooling::DevValueOperationStatus::Success);
	DevChangeSet set{.transaction = 700};
	set.commands.push_back(DevOverrideCommand{
		.kind = DevOverrideCommandKind::SetThemeField,
		.theme = {.themeType = type, .variant = "default"},
		.field = spacingField,
		.value = std::move(overrideValue),
	});
	DevOwnedValue redValue;
	const float red = 0.9f;
	FLOWUI_CHECK(tooling.overrides().copyValue(red, redValue) ==
				 devSystems::tooling::DevValueOperationStatus::Success);
	set.commands.push_back(DevOverrideCommand{
		.kind = DevOverrideCommandKind::SetThemeField,
		.theme = {.themeType = type, .variant = "default"},
		.field = redField,
		.value = std::move(redValue),
	});
	FLOWUI_CHECK(tooling.overrides().submit(std::move(set)));
	tooling.commitAtSafePoint(themes);
	FLOWUI_CHECK(::FlowUi::test::requireValue(themes.getActiveTheme<TestAppTheme>()).get().spacing == 27);
	FLOWUI_CHECK(::FlowUi::test::requireValue(themes.getActiveTheme<TestAppTheme>()).get().brandColor.r == 0.9f);
	const DevThemeCaptureSnapshot& snapshot = tooling.overrides().themeSnapshot();
	FLOWUI_CHECK(snapshot.themes.size() == 1);
	FLOWUI_CHECK(snapshot.variant(snapshot.themes.front()) == "default");
	bool capturedSpacing = false;
	for (const DevCapturedField& field : snapshot.fields) {
		const devSystems::tooling::DevFieldSchema& fieldSchema =
			snapshot.schema->fields[field.field.value - 1u];
		if (snapshot.schema->string(fieldSchema.name) != "spacing") continue;
		capturedSpacing = true;
		FLOWUI_CHECK(field.overridden);
		FLOWUI_CHECK(*static_cast<const int*>(field.value.data()) == 27);
	}
	FLOWUI_CHECK(capturedSpacing);

	DevChangeSet clear{.transaction = 701};
	clear.commands.push_back(DevOverrideCommand{
		.kind = DevOverrideCommandKind::ClearThemeField,
		.theme = {.themeType = type, .variant = "default"},
		.field = spacingField,
	});
	clear.commands.push_back(DevOverrideCommand{
		.kind = DevOverrideCommandKind::ClearThemeField,
		.theme = {.themeType = type, .variant = "default"},
		.field = redField,
	});
	FLOWUI_CHECK(tooling.overrides().submit(std::move(clear)));
	tooling.commitAtSafePoint(themes);
	FLOWUI_CHECK(::FlowUi::test::requireValue(themes.getActiveTheme<TestAppTheme>()).get().spacing == 12);
	FLOWUI_CHECK(::FlowUi::test::requireValue(themes.getActiveTheme<TestAppTheme>()).get().brandColor.r == 0.2f);
	FLOWUI_CHECK(tooling.overrides().stats().activeThemeOverrides == 0);
}
#endif

} // namespace

int main() {
	FlowUi::test::Runner runner;
	try {
		FlowUi::test::HeadlessVulkanFixture vulkan;
		runner.run("theme registration and active retrieval", [&] { testThemeRegistrationAndAccess(vulkan); });
		runner.run("failed theme registration rolls back type and permits retry", [&] {
			testFailedRegistrationLeavesNoThemeType(vulkan);
		});
		runner.run("sealed frame rejects theme selection and replacement without changes", [&] {
			testSealedFrameRejectsThemeMutationBeforeEffects(vulkan);
		});
		runner.run("staged and developer phase checks preserve effects and pending work", [&] {
			testStagedAndDevPhaseChecksPrecedeEffects(vulkan);
		});
		runner.run("arbitrary theme assignment propagates with possible effects", [&] {
			testArbitraryThemeFailureRequiresAbandonment(vulkan);
		});
		runner.run("theme variant switching", [&] { testMultipleVariantsAndSwitching(vulkan); });
		runner.run("different theme types can share variant names", [&] {
			testDifferentThemeTypesCanShareVariantNames(vulkan);
		});
		runner.run("over-aligned theme payload storage", [&] {
			testOverAlignedThemePayload(vulkan);
		});
		runner.run("staged frame boundary theme mutations", [&] { testStagedMutations(vulkan); });
		runner.run("built-in FlowUiTheme dark/light variants", [&] { testFlowUiThemeDefaultRegistration(vulkan); });
#if FLOW_UI_DEV_MODE
		runner.run("typed pre-bake theme baselines", [&] { testPreBakeBaselines(vulkan); });
		runner.run("developer theme override and capture", [&] {
			testDevThemeOverrideAndCapture(vulkan);
		});
#endif
		return runner.finish();
	} catch (const FlowUi::test::VulkanUnavailable& error) {
#ifdef FLOWUI_TEST_REQUIRE_VULKAN_DEVICE
		std::cerr << "FAIL: " << error.what() << '\n';
		return 1;
#else
		std::cout << "SKIP: " << error.what() << '\n';
		return 77;
#endif
	}
}
