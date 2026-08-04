#include "TestHarness.hpp"

#include <fstream>
#include <iterator>
#include <string>
#include <type_traits>

#include "FlowUi.hpp"
#include "managers/FontManager.hpp"
#include "managers/ImageManager.hpp"
#include "managers/InputFieldManager.hpp"
#if FLOWUI_PUBLIC_VULKAN_INTEROP
#include "managers/ViewPortManager.hpp"
#endif

namespace {

std::string readSource(const char* path) {
	std::ifstream input(path, std::ios::binary);
	FLOWUI_CHECK(input.good());
	return {std::istreambuf_iterator<char>(input), std::istreambuf_iterator<char>()};
}

void testResourceKeySurface() {
	using FlowUi::ResourceKey;
	static_assert(std::is_aggregate_v<ResourceKey>);
	static_assert(std::is_trivially_copyable_v<ResourceKey>);
	constexpr ResourceKey automatic{.name = "shared"};
	static_assert(automatic.domain == FlowUi::ResourceDomain::Auto);
	static_assert(automatic.window == FlowUi::InvalidWindowId);

	using Image = FlowUi::ImageManager;
	static_assert(std::is_same_v<
		decltype(static_cast<FlowUi::TextureRef (Image::*)(ResourceKey) const>(&Image::getTexture)),
		FlowUi::TextureRef (Image::*)(ResourceKey) const>);
	using Font = FlowUi::FontManager;
	static_assert(std::is_same_v<
		decltype(static_cast<FlowUi::FontFamilyId (Font::*)(ResourceKey) const>(&Font::getFamilyId)),
		FlowUi::FontFamilyId (Font::*)(ResourceKey) const>);
	using Input = FlowUi::InputFieldManager;
	static_assert(std::is_same_v<
		decltype(static_cast<bool (Input::*)(ResourceKey)>(&Input::removeField)),
		bool (Input::*)(ResourceKey)>);
#if FLOWUI_PUBLIC_VULKAN_INTEROP
	using Viewports = FlowUi::ViewPortManager;
	static_assert(std::is_same_v<
		decltype(static_cast<bool (Viewports::*)(ResourceKey, const FlowUi::ViewPortCreateInfo&)>(&Viewports::create)),
		bool (Viewports::*)(ResourceKey, const FlowUi::ViewPortCreateInfo&)>);
#endif
}

void testManagerOwnershipGuards() {
	for (const char* path : {
		FLOWUI_IMAGE_MANAGER_SOURCE,
		FLOWUI_ICON_MANAGER_SOURCE,
		FLOWUI_FONT_MANAGER_SOURCE,
		FLOWUI_VIEWPORT_MANAGER_SOURCE,
	}) {
		const std::string source = readSource(path);
		FLOWUI_CHECK(source.find("vmaCreate") == std::string::npos);
		FLOWUI_CHECK(source.find("vmaDestroy") == std::string::npos);
		FLOWUI_CHECK(source.find("VmaAllocation") == std::string::npos);
		FLOWUI_CHECK(source.find("publishExternalTexture") == std::string::npos);
		FLOWUI_CHECK(source.find("IUiTexturePublisher") == std::string::npos);
		FLOWUI_CHECK(source.find("vkDeviceWaitIdle") == std::string::npos);
	}
	const std::string renderer = readSource(FLOWUI_UI_RENDERER_SOURCE);
	FLOWUI_CHECK(renderer.find("FontManager*") == std::string::npos);
	FLOWUI_CHECK(renderer.find("setFontManager") == std::string::npos);
	const std::string storageInterface = readSource(FLOWUI_STORAGE_INTERFACE_SOURCE);
	FLOWUI_CHECK(storageInterface.find("publishExternalTexture") == std::string::npos);
	FLOWUI_CHECK(storageInterface.find("createManagerRecord") != std::string::npos);
	FLOWUI_CHECK(storageInterface.find("managerFrameView") != std::string::npos);
	const std::string storageTypes = readSource(FLOWUI_STORAGE_TYPES_SOURCE);
	FLOWUI_CHECK(storageTypes.find("BorrowedNativeTextures") == std::string::npos);
}

} // namespace

int main() {
	FlowUi::test::Runner runner;
	runner.run("uniform ResourceKey manager surface", testResourceKeySurface);
	runner.run("manager storage ownership source guards", testManagerOwnershipGuards);
	return runner.finish();
}
