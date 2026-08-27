#include "TestHarness.hpp"

#include <fstream>
#include <iterator>
#include <string>
#include <type_traits>

#include "FlowUi/App.hpp"
#include "Vulkan/Vk_Swapchain.hpp"

namespace {

using FlowUi::App;
using FlowUi::WindowId;

void testPublicOverloads() {
	static_assert(std::is_same_v<decltype(static_cast<FlowUi::Result<WindowId> (App::*)(const FlowUi::WindowConfig&)>(&App::createWindow)),
		FlowUi::Result<WindowId> (App::*)(const FlowUi::WindowConfig&)>);
	static_assert(std::is_same_v<decltype(static_cast<FlowUi::Status (App::*)(WindowId)>(&App::beginFrame)),
		FlowUi::Status (App::*)(WindowId)>);
	static_assert(std::is_same_v<decltype(static_cast<FlowUi::Status (App::*)()>(&App::beginFrame)), FlowUi::Status (App::*)()>);
	static_assert(std::is_same_v<decltype(static_cast<FlowUi::UiManager& (App::*)(WindowId)>(&App::ui)),
		FlowUi::UiManager& (App::*)(WindowId)>);
	static_assert(!std::is_copy_constructible_v<Swapchain>);
	static_assert(std::is_nothrow_move_constructible_v<Swapchain>);
	static_assert(!std::is_copy_constructible_v<SwapchainGeneration>);
	static_assert(std::is_nothrow_move_constructible_v<SwapchainGeneration>);
	FLOWUI_CHECK(FlowUi::InvalidWindowId == 0u);
	FLOWUI_CHECK(FlowUi::MainWindowId == 1u);
}

void testFlowColorRgbAndRgbaForms() {
	constexpr Clay_Color rgb = FlowUi::Flow_Color("#12aBcD");
	static_assert(rgb.r == 0x12 && rgb.g == 0xab && rgb.b == 0xcd && rgb.a == 0xff);
	FLOWUI_CHECK(rgb.r == 0x12);
	FLOWUI_CHECK(rgb.g == 0xab);
	FLOWUI_CHECK(rgb.b == 0xcd);
	FLOWUI_CHECK(rgb.a == 0xff);

	const std::string_view runtimeHex = "#12abcd34";
	const Clay_Color rgba = FlowUi::Flow_Color(runtimeHex);
	FLOWUI_CHECK(rgba.r == 0x12);
	FLOWUI_CHECK(rgba.g == 0xab);
	FLOWUI_CHECK(rgba.b == 0xcd);
	FLOWUI_CHECK(rgba.a == 0x34);

	FLOWUI_CHECK_THROWS(FlowUi::Flow_Color(std::string_view{"#12345"}));
	FLOWUI_CHECK_THROWS(FlowUi::Flow_Color(std::string_view{"123456"}));
	FLOWUI_CHECK_THROWS(FlowUi::Flow_Color(std::string_view{"#12345g"}));
}

std::string readSource(const char* path) {
	std::ifstream input(path, std::ios::binary);
	FLOWUI_CHECK(input.good());
	return {std::istreambuf_iterator<char>(input), std::istreambuf_iterator<char>()};
}

void testLifecycleAndIdleRegressions() {
	const std::string app = readSource(FLOWUI_APP_SOURCE);
	const std::string viewport = readSource(FLOWUI_VIEWPORT_MANAGER_SOURCE);
	const std::string swapchain = readSource(FLOWUI_SWAPCHAIN_SOURCE);
	const std::string renderer = readSource(FLOWUI_UI_RENDERER_SOURCE);

	FLOWUI_CHECK(app.find("createElementWindow") == std::string::npos);
	FLOWUI_CHECK(app.find("frameSecondaryWindows") == std::string::npos);
	FLOWUI_CHECK(app.find("oldSwapchain") == std::string::npos); // old handle is encapsulated by Swapchain
	FLOWUI_CHECK(viewport.find("vkDeviceWaitIdle") == std::string::npos);
	size_t appIdleCalls = 0;
	for (size_t offset = 0; (offset = app.find("vkDeviceWaitIdle", offset)) != std::string::npos; ++offset) {
		++appIdleCalls;
	}
	FLOWUI_CHECK(appIdleCalls == 2u); // main-only compatibility resize + final shutdown
	FLOWUI_CHECK(app.find("windows.size() != 1u || window.id != mainWindowId") != std::string::npos);
	FLOWUI_CHECK(swapchain.find("createInfo.oldSwapchain = oldSwapchain") != std::string::npos);
	FLOWUI_CHECK(renderer.find("publishRendererLayout") != std::string::npos);
	FLOWUI_CHECK(renderer.find("publishRendererPipelineBundle") != std::string::npos);
	FLOWUI_CHECK(renderer.find("adoptWindowDescriptorBundle") != std::string::npos);
	FLOWUI_CHECK(renderer.find("storageSystem.trackUses(frame, rendererUses)") != std::string::npos);
}

} // namespace

int main() {
	FlowUi::test::Runner runner;
	runner.run("Phase 4 public overloads and move-only WSI types", testPublicOverloads);
	runner.run("Flow_Color accepts RGB and RGBA hex forms", testFlowColorRgbAndRgbaForms);
	runner.run("Phase 4 lifecycle, renderer, and no-idle source regressions", testLifecycleAndIdleRegressions);
	return runner.finish();
}
