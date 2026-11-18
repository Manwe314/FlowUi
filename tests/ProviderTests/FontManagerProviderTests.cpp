#include "FlowUi/Private/resourceService/FontCatalogController.hpp"
#include "FlowUi/Private/storageSystem/StorageBinding.hpp"
#include "FlowUi/Public/managers/FontManager.hpp"
#include "HeadlessVulkanFixture.hpp"
#include "TestHarness.hpp"
#include <array>
#include <iostream>

namespace FlowUi::detail::font {
/** Test the private manager lifecycle against real storage and Vulkan resources. */
struct FontManagerAccess {
	static void initialize(FontManager& manager, storage::StorageBinding& binding,
						   uint32_t atlasExtent) {
		manager.init(binding, atlasExtent);
	}
	static void destroy(FontManager& manager) noexcept { manager.destroy(); }
	[[nodiscard]] static manager_storage::FontCatalogController&
	controller(FontManager& manager) noexcept {
		return *manager.controller_;
	}
};
} // namespace FlowUi::detail::font
namespace {
using namespace FlowUi;
namespace storage = FlowUi::detail::storage;
namespace font = FlowUi::detail::font;
constexpr uint64_t generousBudget = 1ull << 30u;
[[nodiscard]] std::vector<std::byte> fontBytes() {
	font::FontCpuAsset asset;
	asset.atlas.width = asset.atlas.height = 4u;
	asset.atlas.pixels.resize(64u, std::byte{128});
	font::FontCpuVariant variant;
	variant.name = "publication";
	variant.fontSizePx = 18.0f;
	variant.distanceRange = 6.0f;
	variant.emSize = 1.0f;
	variant.ascender = 0.8f;
	variant.descender = -0.2f;
	variant.lineHeight = 1.2f;
	variant.fallbackGlyphIndex = 99u;
	variant.glyphs.emplace_back(Font::GlyphData{.codepoint = 'A',
												.planeRight = 1.0f,
												.planeTop = 1.0f,
												.imageRight = 4.0f,
												.imageTop = 4.0f,
												.advanceX = 1.0f});
	variant.glyphs.emplace_back(Font::GlyphData{.codepoint = 'A',
												.planeRight = 1.0f,
												.planeTop = 1.0f,
												.imageRight = 4.0f,
												.imageTop = 4.0f,
												.advanceX = 2.0f});
	variant.glyphs.emplace_back(Font::GlyphData{.codepoint = '?',
												.planeRight = 1.0f,
												.planeTop = 1.0f,
												.imageRight = 4.0f,
												.imageTop = 4.0f,
												.advanceX = 1.0f});
	variant.kerningPairs.emplace_back(font::FontKerningPair{'A', 'V', -0.1f});
	variant.kerningPairs.emplace_back(font::FontKerningPair{'A', 'V', -0.2f});
	asset.variants.emplace_back(std::move(variant));
	font::ActiveFontMSDFProvider provider;
	return test::requireValue(provider.encodeBaked(asset));
}
struct Fixture {
	storage::ActiveStorageSystem storageSystem;
	storage::StorageBinding binding;
	FontManager manager;
	std::vector<std::byte> bytes;
	explicit Fixture(test::HeadlessVulkanFixture& vulkan, uint32_t atlasExtent = 16u)
		: storageSystem(vulkan.context()), bytes(fontBytes()) {
		storage::StorageConfig config;
		config.cpuSoftBudgetBytes = config.gpuSoftBudgetBytes = generousBudget;
		storageSystem.initialize(config);
		binding.bind(storageSystem);
		font::FontManagerAccess::initialize(manager, binding, atlasExtent);
	}
	~Fixture() {
		font::FontManagerAccess::destroy(manager);
		binding.detach();
		storageSystem.shutdown();
	}
	void constrainCpuBudget() {
		uint64_t minimum = 1u;
		uint64_t maximum = generousBudget;
		while (minimum < maximum) {
			const uint64_t candidate = minimum + (maximum - minimum) / 2u;
			try {
				storageSystem.setBudget(candidate, generousBudget);
				maximum = candidate;
			} catch (const FlowUiException& error) {
				FLOWUI_CHECK(error.error().code == ErrorCode::StorageBudgetExceeded);
				minimum = candidate + 1u;
			}
		}
		storageSystem.setBudget(minimum, generousBudget);
	}
	[[nodiscard]] FontFaceCreateInfo face() const {
		return FontFaceCreateInfo{.name = "publication", .baked_data = bytes};
	}
	[[nodiscard]] FontFamilyCreateInfo family(std::string name = "fixture") const {
		FontFamilyCreateInfo request;
		request.name = std::move(name);
		request.faces = {face()};
		return request;
	}
};
void testUploadAndMapRollback(test::HeadlessVulkanFixture& vulkan) {
	Fixture fixture(vulkan);
	auto& controller = font::FontManagerAccess::controller(fixture.manager);
	const uint64_t initialRevision = controller.catalogRevision;
	fixture.storageSystem.setBudget(generousBudget, 1u);
	const auto failed = fixture.manager.createFamily(fixture.family());
	FLOWUI_CHECK(!failed && failed.error().code == ErrorCode::StorageBudgetExceeded);
	FLOWUI_CHECK(controller.fonts.empty() && controller.fontIdByName.empty() &&
				 controller.fontIndexById.empty());
	FLOWUI_CHECK(controller.families.empty() && controller.familyIdByName.empty() &&
				 controller.nextFontId == 0u);
	FLOWUI_CHECK(controller.atlasLayerPixels.empty() &&
				 controller.catalogRevision == initialRevision);
#if FLOW_UI_DEV_MODE
	FLOWUI_CHECK(controller.dev_atlas_layers.empty());
#endif
	fixture.storageSystem.setBudget(generousBudget, generousBudget);
	const auto family = test::requireValue(fixture.manager.createFamily(fixture.family()));
	FLOWUI_CHECK(family == 0u && controller.nextFontId == 1u);
	const auto* face = fixture.manager.getFontById(0u);
	FLOWUI_CHECK(face && face->id == 0u && face->sourceAtlasWidth == 4u && face->atlasWidth == 16u);
	FLOWUI_CHECK(face->defaultVariant()->fallbackGlyphIndex == 0u);
	FLOWUI_CHECK(face->defaultVariant()->unicodeToGlyphIndex.at('A') == 0u);
	FLOWUI_CHECK(face->defaultVariant()->kerningAdvance('A', 'V') == -0.2f);
	const uint64_t revision = controller.catalogRevision;
	(void)fixture.storageSystem.intern("flowui.font.atlas.layer");
	fixture.constrainCpuBudget();
	const auto failedAdd = fixture.manager.addFamilyFace(family, fixture.face());
	FLOWUI_CHECK(!failedAdd && failedAdd.error().code == ErrorCode::StorageBudgetExceeded);
	FLOWUI_CHECK(controller.fonts.size() == 1u && controller.nextFontId == 1u);
	FLOWUI_CHECK(controller.fontIdByName.size() == 1u && controller.fontIndexById.size() == 1u);
	FLOWUI_CHECK(controller.families.front().faces.size() == 1u &&
				 controller.atlasLayerPixels.size() == 1u);
	FLOWUI_CHECK(controller.catalogRevision == revision && fixture.manager.getFontById(0u) == face);
	fixture.storageSystem.setBudget(generousBudget, generousBudget);
	FLOWUI_CHECK(test::requireValue(fixture.manager.addFamilyFace(family, fixture.face())) == 1u);
}
void testFamilyRollbackAndBorrowedPrecedence(test::HeadlessVulkanFixture& vulkan) {
	Fixture fixture(vulkan);
	auto& controller = font::FontManagerAccess::controller(fixture.manager);
	const auto family = test::requireValue(fixture.manager.createFamily(fixture.family()));
	const auto* originalFace = fixture.manager.getFontById(0u);
	const uint64_t revision = controller.catalogRevision;
	const std::array invalidBytes{std::byte{1}, std::byte{2}};
	auto request = fixture.family("failure");
	request.faces.emplace_back(FontFaceCreateInfo{.baked_data = invalidBytes});
	const auto failed = fixture.manager.createFamily(request);
	FLOWUI_CHECK(!failed);
	FLOWUI_CHECK(controller.families.size() == 1u && controller.familyIdByName.size() == 1u);
	FLOWUI_CHECK(controller.fonts.size() == 1u && controller.fontIndexById.size() == 1u &&
				 controller.fontIdByName.size() == 1u);
	FLOWUI_CHECK(controller.nextFontId == 1u && controller.atlasLayerPixels.size() == 1u);
	FLOWUI_CHECK(controller.catalogRevision == revision &&
				 fixture.manager.getFontById(0u) == originalFace);
#if FLOW_UI_DEV_MODE
	FLOWUI_CHECK(controller.dev_atlas_layers.size() == 1u);
#endif
	auto borrowed = fixture.face();
	borrowed.path = "this-file-must-not-be-read.ttf";
	FLOWUI_CHECK(test::requireValue(fixture.manager.addFamilyFace(family, borrowed)) == 1u);
	FLOWUI_CHECK(originalFace->defaultVariant()->glyphs.front().advanceX == 1.0f);
}
void testFamilyFailureAfterAtlasGrowth(test::HeadlessVulkanFixture& vulkan) {
	Fixture fixture(vulkan);
	auto& controller = font::FontManagerAccess::controller(fixture.manager);
	const auto family = test::requireValue(fixture.manager.createFamily(fixture.family()));
	const auto initialCapacity = controller.borrowedAtlas.layersCapacity;
	for (uint32_t layerIndex = 1u; layerIndex < initialCapacity; ++layerIndex)
		FLOWUI_CHECK(fixture.manager.addFamilyFace(family, fixture.face()));
	const auto* originalFace = fixture.manager.getFontById(0u);
	const auto originalImage = controller.atlasImage;
	const auto revision = controller.catalogRevision;
	const auto originalPixels = controller.atlasLayerPixels.front();
	const std::array invalidBytes{std::byte{1}, std::byte{2}};
	auto request = fixture.family("growth-failure");
	request.faces.emplace_back(FontFaceCreateInfo{.baked_data = invalidBytes});
	FLOWUI_CHECK(!fixture.manager.createFamily(request));
	FLOWUI_CHECK(controller.nextFontId == initialCapacity &&
				 controller.fonts.size() == initialCapacity);
	FLOWUI_CHECK(controller.atlasLayerPixels.size() == initialCapacity &&
				 controller.families.size() == 1u);
	FLOWUI_CHECK(fixture.manager.getFontById(0u) == originalFace &&
				 controller.atlasLayerPixels.front() == originalPixels);
	FLOWUI_CHECK(controller.atlasImage != originalImage && controller.catalogRevision > revision);
	FLOWUI_CHECK(controller.borrowedAtlas.layersUsed == initialCapacity);
	FLOWUI_CHECK(fixture.storageSystem.nativeImage(controller.atlasImage).nativeImage != 0u);
#if FLOW_UI_DEV_MODE
	FLOWUI_CHECK(controller.dev_atlas_layers.size() == initialCapacity);
	FLOWUI_CHECK(controller.dev_atlas_layers.front().texture);
#endif
	// Native atlas growth retains its new image and invalidates old inspection borrows;
	// rolling back logical faces cannot silently restore obsolete resource identities.
	FLOWUI_CHECK(test::requireValue(fixture.manager.addFamilyFace(family, fixture.face())) ==
				 initialCapacity);
}

void testRuntimeBakingPolicy(test::HeadlessVulkanFixture& vulkan) {
	Fixture fixture(vulkan, 512u);
	const auto family = test::requireValue(fixture.manager.createFamily(fixture.family()));
	FontFaceCreateInfo request;
	request.path =
		std::filesystem::path(FLOWUI_PROVIDER_ASSET_DIRECTORY) / "Inter-VariableFont_opsz,wght.ttf";
	request.name = "runtime-provider";
	request.pixelSize = 18.0f;
	const auto result = fixture.manager.addFamilyFace(family, request);
#if defined(FLOWUI_RUNTIME_FONT_BAKING)
	FLOWUI_CHECK(result && *result == 1u);
	const auto* face = fixture.manager.getFontById(*result);
	FLOWUI_CHECK(face && face->sourceAtlasWidth == 512u && face->sourceAtlasHeight == 512u);
	FLOWUI_CHECK(face->defaultVariant()->fontSizePx == 18.0f);
	FLOWUI_CHECK(face->defaultVariant()->distanceRange == 6.0f);
#else
	FLOWUI_CHECK(!result && result.error().code == ErrorCode::AssetFormatUnsupported);
	FLOWUI_CHECK(fixture.manager.getFontById(1u) == nullptr);
#endif
}

} // namespace
int main() {
	try {
		FlowUi::test::HeadlessVulkanFixture vulkan;
		FlowUi::test::Runner runner;
		runner.run("font upload and map staging failure restore IDs, records and atlas",
				   [&] { testUploadAndMapRollback(vulkan); });
		runner.run("font family rollback preserves existing face and borrowed bytes precedence",
				   [&] { testFamilyRollbackAndBorrowedPrecedence(vulkan); });
		runner.run(
			"failed family after native atlas growth preserves live faces and invalidates borrows",
			[&] { testFamilyFailureAfterAtlasGrowth(vulkan); });
		runner.run(
			"font manager runtime baking policy reaches the selected provider only when enabled",
			[&] { testRuntimeBakingPolicy(vulkan); });
		return runner.finish();
	} catch (const FlowUi::test::VulkanUnavailable& exception) {
		std::cerr << exception.what() << '\n';
#ifdef FLOWUI_TEST_REQUIRE_VULKAN_DEVICE
		return 1;
#else
		return 77;
#endif
	}
}
