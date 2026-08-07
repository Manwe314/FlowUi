#include "HeadlessVulkanFixture.hpp"
#include "TestHarness.hpp"

#include <iostream>
#include <stdexcept>
#include <string>

#include "FlowUi/PublicStructs.hpp"
#include "internal/StorageSystem/FlowStorageSystem.hpp"
#include "managers/ThemeManager.hpp"
#include "managers/UiManager.hpp"

namespace {

struct TestAppTheme {
	Clay_Color brandColor = { 0.2f, 0.4f, 0.8f, 1.0f };
	float cornerRadius = 8.0f;
	int spacing = 12;
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

	bool switched = themes.setActiveVariant<TestAppTheme>("light");
	FLOWUI_CHECK(switched);
	FLOWUI_CHECK(themes.getActiveTheme<TestAppTheme>().cornerRadius == 14.0f);
	FLOWUI_CHECK(themes.getActiveTheme<TestAppTheme>().spacing == 20);

	switched = themes.setActiveVariant<TestAppTheme>("dark");
	FLOWUI_CHECK(switched);
	FLOWUI_CHECK(themes.getActiveTheme<TestAppTheme>().cornerRadius == 6.0f);
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

} // namespace

int main() {
	FlowUi::test::Runner runner;
	try {
		FlowUi::test::HeadlessVulkanFixture vulkan;
		runner.run("theme registration and active retrieval", [&] { testThemeRegistrationAndAccess(vulkan); });
		runner.run("theme variant switching", [&] { testMultipleVariantsAndSwitching(vulkan); });
		runner.run("staged frame boundary theme mutations", [&] { testStagedMutations(vulkan); });
		runner.run("built-in FlowUiTheme dark/light variants", [&] { testFlowUiThemeDefaultRegistration(vulkan); });
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
