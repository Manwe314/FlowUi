#include "HeadlessVulkanFixture.hpp"
#include "TestHarness.hpp"

#include <cstdint>
#include <iostream>
#include <stdexcept>
#include <string>

#include "FlowUi/PublicStructs.hpp"
#include "internal/StorageSystem/FlowStorageSystem.hpp"
#include "managers/ThemeManager.hpp"
#include "managers/UiManager.hpp"
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

void testThemeRegistrationAndAccess(FlowUi::test::HeadlessVulkanFixture& vulkan) {
	FlowUi::detail::storage::FlowStorageSystem storage(vulkan.context());
	FlowUi::detail::storage::StorageConfig config{};
	storage.initialize(config);

	FlowUi::ThemeManager themes;
	themes.init(storage);

	TestAppTheme defaultTheme{
		.brandColor = { 0.1f, 0.2f, 0.3f, 1.0f },
		.cornerRadius = 4.0f,
		.spacing = 8
	};

	themes.registerTheme<TestAppTheme>("default", defaultTheme, true);

	const TestAppTheme& active = themes.getActiveTheme<TestAppTheme>();
	FLOWUI_CHECK(active.cornerRadius == 4.0f);
	FLOWUI_CHECK(active.spacing == 8);
	FLOWUI_CHECK(active.brandColor.r == 0.1f);

	const TestAppTheme& named = themes.getTheme<TestAppTheme>("default");
	FLOWUI_CHECK(named.cornerRadius == 4.0f);
}

void testMultipleVariantsAndSwitching(FlowUi::test::HeadlessVulkanFixture& vulkan) {
	FlowUi::detail::storage::FlowStorageSystem storage(vulkan.context());
	FlowUi::detail::storage::StorageConfig config{};
	storage.initialize(config);

	FlowUi::ThemeManager themes;
	themes.init(storage);

	TestAppTheme darkVariant{ .cornerRadius = 6.0f, .spacing = 10 };
	TestAppTheme lightVariant{ .cornerRadius = 14.0f, .spacing = 20 };

	themes.registerTheme<TestAppTheme>("dark", darkVariant, true);
	themes.registerTheme<TestAppTheme>("light", lightVariant, false);

	FLOWUI_CHECK(themes.getActiveTheme<TestAppTheme>().cornerRadius == 6.0f);
	FLOWUI_CHECK(themes.getTheme<TestAppTheme>("light").cornerRadius == 14.0f);

	auto switched = themes.setActiveVariant<TestAppTheme>("light");
	FLOWUI_CHECK(switched);
	FLOWUI_CHECK(themes.getActiveTheme<TestAppTheme>().cornerRadius == 14.0f);
	FLOWUI_CHECK(themes.getActiveTheme<TestAppTheme>().spacing == 20);

	switched = themes.setActiveVariant<TestAppTheme>("dark");
	FLOWUI_CHECK(switched);
	FLOWUI_CHECK(themes.getActiveTheme<TestAppTheme>().cornerRadius == 6.0f);
}

void testDifferentThemeTypesCanShareVariantNames(FlowUi::test::HeadlessVulkanFixture& vulkan) {
	FlowUi::detail::storage::FlowStorageSystem storage(vulkan.context());
	FlowUi::detail::storage::StorageConfig config{};
	storage.initialize(config);

	FlowUi::ThemeManager themes;
	themes.init(storage);

	themes.registerTheme<TestAppTheme>(
		"default",
		TestAppTheme{.cornerRadius = 9.0f, .spacing = 18},
		true);
	themes.registerTheme<TestEditorTheme>(
		"default",
		TestEditorTheme{.rowHeight = 32},
		true);

	FLOWUI_CHECK(themes.getTheme<TestAppTheme>("default").cornerRadius == 9.0f);
	FLOWUI_CHECK(themes.getTheme<TestAppTheme>("default").spacing == 18);
	FLOWUI_CHECK(themes.getTheme<TestEditorTheme>("default").rowHeight == 32);

	themes.updateTheme<TestEditorTheme>("default", [](TestEditorTheme& theme) {
		theme.rowHeight = 40;
	});
	themes.applyStagedMutations();

	FLOWUI_CHECK(themes.getTheme<TestEditorTheme>("default").rowHeight == 40);
	FLOWUI_CHECK(themes.getTheme<TestAppTheme>("default").cornerRadius == 9.0f);
}

void testOverAlignedThemePayload(FlowUi::test::HeadlessVulkanFixture& vulkan) {
	FlowUi::detail::storage::FlowStorageSystem storage(vulkan.context());
	FlowUi::detail::storage::StorageConfig config{};
	storage.initialize(config);

	FlowUi::ThemeManager themes;
	themes.init(storage);
	themes.registerTheme<TestOverAlignedTheme>(
		"default", TestOverAlignedTheme{.marker = 73}, true);

	const TestOverAlignedTheme& theme = themes.getActiveTheme<TestOverAlignedTheme>();
	FLOWUI_CHECK(theme.marker == 73);
	FLOWUI_CHECK(reinterpret_cast<uintptr_t>(&theme) % alignof(TestOverAlignedTheme) == 0);
}

void testStagedMutations(FlowUi::test::HeadlessVulkanFixture& vulkan) {
	FlowUi::detail::storage::FlowStorageSystem storage(vulkan.context());
	FlowUi::detail::storage::StorageConfig config{};
	storage.initialize(config);

	FlowUi::ThemeManager themes;
	themes.init(storage);

	TestAppTheme initialTheme{ .cornerRadius = 5.0f, .spacing = 10 };
	themes.registerTheme<TestAppTheme>("default", initialTheme, true);

	themes.updateActiveTheme<TestAppTheme>([](TestAppTheme& t) {
		t.cornerRadius = 15.0f;
		t.spacing = 30;
	});

	FLOWUI_CHECK(themes.getActiveTheme<TestAppTheme>().cornerRadius == 5.0f);

	themes.applyStagedMutations();

	FLOWUI_CHECK(themes.getActiveTheme<TestAppTheme>().cornerRadius == 15.0f);
	FLOWUI_CHECK(themes.getActiveTheme<TestAppTheme>().spacing == 30);

	FLOWUI_CHECK(themes.updateActiveTheme<TestAppTheme>([](TestAppTheme& t) {
		t.cornerRadius = 99.0f;
		throw std::runtime_error("theme callback failure");
	}));
	FLOWUI_CHECK_THROWS(themes.applyStagedMutations());
	FLOWUI_CHECK(themes.getActiveTheme<TestAppTheme>().cornerRadius == 15.0f);
	FLOWUI_CHECK(themes.getActiveTheme<TestAppTheme>().spacing == 30);
}

void testFlowUiThemeDefaultRegistration(FlowUi::test::HeadlessVulkanFixture& vulkan) {
	FlowUi::detail::storage::FlowStorageSystem storage(vulkan.context());
	FlowUi::detail::storage::StorageConfig config{};
	storage.initialize(config);

	FlowUi::ThemeManager themes;
	themes.init(storage);

	themes.registerTheme<FlowUi::FlowUiTheme>("default", FlowUi::FlowUiTheme::dark(), true);
	themes.registerTheme<FlowUi::FlowUiTheme>("light", FlowUi::FlowUiTheme::light(), false);

	const auto& darkTheme = themes.getActiveTheme<FlowUi::FlowUiTheme>();
	FLOWUI_CHECK(darkTheme.fontSizeMedium == 14.0f);

	const auto& lightTheme = themes.getTheme<FlowUi::FlowUiTheme>("light");
	FLOWUI_CHECK(lightTheme.background.r == 1.0f);
	FLOWUI_CHECK(lightTheme.background.g == 1.0f);
	FLOWUI_CHECK(lightTheme.background.b == 1.0f);
}

#if FLOW_UI_DEV_MODE
void testDevThemeOverrideAndCapture(FlowUi::test::HeadlessVulkanFixture& vulkan) {
	using namespace FlowUi;
	using namespace FlowUi::devSystems::tooling;
	detail::storage::FlowStorageSystem storage(vulkan.context());
	detail::storage::StorageConfig config{};
	storage.initialize(config);

	ThemeManager themes;
	themes.init(storage);
	themes.registerTheme<TestAppTheme>(
		"default", TestAppTheme{.cornerRadius = 5.0f, .spacing = 12}, true);

	devSystems::DevTooling tooling;
	tooling.schemas().ensureTheme<TestAppTheme>();
	FLOWUI_CHECK(tooling.schemas().publishPendingAtSafePoint());
	const devMode::DevSchemaView schema = tooling.schemas().view();
	const devMode::DevTypeId type = detail::typeHash<TestAppTheme>();
	const devMode::DevThemeSchema* themeSchema = schema->findTheme(type);
	FLOWUI_CHECK(themeSchema != nullptr);
	const devMode::DevTypeSchema* themeType = schema->type(themeSchema->themeType);
	FLOWUI_CHECK(themeType != nullptr);
	DevOverrideFieldKey spacingField{};
	DevOverrideFieldKey redField{};
	for (const devMode::DevFieldSchema& field : schema->fieldsOf(themeSchema->themeType)) {
		if (schema->string(field.name) == "spacing") {
			spacingField = {themeType->id, field.id};
		} else if (schema->string(field.name) == "brandColor") {
			for (const devMode::DevFieldSchema& colorField :
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
		devMode::DevValueOperationStatus::Success);
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
		devMode::DevValueOperationStatus::Success);
	set.commands.push_back(DevOverrideCommand{
		.kind = DevOverrideCommandKind::SetThemeField,
		.theme = {.themeType = type, .variant = "default"},
		.field = redField,
		.value = std::move(redValue),
	});
	FLOWUI_CHECK(tooling.overrides().submit(std::move(set)));
	tooling.commitAtSafePoint(themes);
	FLOWUI_CHECK(themes.getActiveTheme<TestAppTheme>().spacing == 27);
	FLOWUI_CHECK(themes.getActiveTheme<TestAppTheme>().brandColor.r == 0.9f);
	const DevThemeCaptureSnapshot& snapshot = tooling.overrides().themeSnapshot();
	FLOWUI_CHECK(snapshot.themes.size() == 1);
	FLOWUI_CHECK(snapshot.variant(snapshot.themes.front()) == "default");
	bool capturedSpacing = false;
	for (const DevCapturedField& field : snapshot.fields) {
		const devMode::DevFieldSchema& fieldSchema =
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
	FLOWUI_CHECK(themes.getActiveTheme<TestAppTheme>().spacing == 12);
	FLOWUI_CHECK(themes.getActiveTheme<TestAppTheme>().brandColor.r == 0.2f);
	FLOWUI_CHECK(tooling.overrides().stats().activeThemeOverrides == 0);
}
#endif

} // namespace

int main() {
	FlowUi::test::Runner runner;
	try {
		FlowUi::test::HeadlessVulkanFixture vulkan;
		runner.run("theme registration and active retrieval", [&] { testThemeRegistrationAndAccess(vulkan); });
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
