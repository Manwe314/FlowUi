#include "FlowUi/Private/resourceService/IconManagerAccess.hpp"
#include "HeadlessVulkanFixture.hpp"
#include "IconGpuSampler.hpp"
#include "TestHarness.hpp"
#include <cmath>

#include <cstdlib>
#include <iostream>
#include <limits>

namespace {
namespace storage = FlowUi::detail::storage;
namespace icon = FlowUi::detail::icon;
using Access = icon::IconManagerAccess;
constexpr std::string_view landscapeSource =
	R"(<svg xmlns="http://www.w3.org/2000/svg" width="12" height="6"><rect width="12" height="6" fill="#ff8040" fill-opacity="0.5"/></svg>)";

struct ManagerFixture {
	storage::ActiveStorageSystem storageSystem;
	storage::StorageBinding binding;
	FlowUi::IconManager manager;

	explicit ManagerFixture(FlowUi::test::HeadlessVulkanFixture& context)
		: storageSystem(context.context()) {
		storage::StorageConfig config{};
		config.initialPersistentCpuBytes = 4096;
		config.cpuSoftBudgetBytes = 32u << 20u;
		config.gpuSoftBudgetBytes = 64u << 20u;
		storageSystem.initialize(config);
		binding.bind(storageSystem);
		FlowUi::IconManagerConfig icons{};
		icons.atlasSize = 64;
		icons.atlasPadding = 1;
		icons.maxAtlasPages = 1;
		icons.sizeBucketStep = 8;
		Access::initialize(manager, binding, icons);
	}
	~ManagerFixture() {
		Access::destroy(manager);
		binding.detach();
		storageSystem.shutdown();
	}

	void constrainCpuBudget() {
		uint64_t low = 0;
		uint64_t high = 32u << 20u;
		while (low < high) {
			const uint64_t candidate = low + (high - low) / 2u;
			try {
				storageSystem.setBudget(candidate, 64u << 20u);
				high = candidate;
			} catch (const FlowUi::FlowUiException& failure) {
				FLOWUI_CHECK(failure.error().code == FlowUi::ErrorCode::StorageBudgetExceeded);
				low = candidate + 1;
			}
		}
		storageSystem.setBudget(low, 64u << 20u);
	}
	void restoreCpuBudget() { storageSystem.setBudget(32u << 20u, 64u << 20u); }
};

void testRegistrationCacheAndRemoval(FlowUi::test::HeadlessVulkanFixture& context) {
	ManagerFixture fixture{context};
	FLOWUI_CHECK(
		FlowUi::test::requireValue(fixture.manager.registerSvg("translucent", landscapeSource)));
	FLOWUI_CHECK(
		!FlowUi::test::requireValue(fixture.manager.registerSvg("translucent", landscapeSource)));
	FLOWUI_CHECK(!FlowUi::test::requireValue(fixture.manager.registerSvg("translucent", "")));
	const auto& controller = Access::controller(fixture.manager);
	FLOWUI_CHECK(controller.documentsByKey.size() == 1);
	const auto source = controller.documentsByKey.at("translucent").source;
	FLOWUI_CHECK(fixture.storageSystem.readBlob(source).size() == landscapeSource.size());
	const auto first = Access::variant(fixture.manager, "translucent", 24, 24);
	FLOWUI_CHECK(first.sourceWidth == 24 && first.sourceHeight == 12);
	FLOWUI_CHECK(first.contentRect.w == 24 && first.contentRect.h == 12);
	FLOWUI_CHECK(first.paddedRect.w == 26 && first.paddedRect.h == 14);
	FLOWUI_CHECK(first.uv0x > 0 && first.uv0y > 0);
	FLOWUI_CHECK(Access::variant(fixture.manager, "translucent", 20, 20).texture == first.texture);
	const auto exact = Access::variant(fixture.manager, "translucent", 20, 20, true);
	FLOWUI_CHECK(exact.texture != first.texture);
	FLOWUI_CHECK(controller.variantsByKeyAndSize.size() == 2);
	FLOWUI_CHECK(FlowUi::test::requireValue(fixture.manager.remove("translucent")));
	FLOWUI_CHECK(controller.documentsByKey.empty() && controller.variantsByKeyAndSize.empty());
	fixture.storageSystem.collect();
	Access::beginTick(fixture.manager);
	FLOWUI_CHECK(controller.retiredRegions.empty());
	FLOWUI_CHECK(fixture.storageSystem.readBlob(source).empty());
	FLOWUI_CHECK(!FlowUi::test::requireValue(fixture.manager.remove("translucent")));
}

void testSourceAndUploadRollback(FlowUi::test::HeadlessVulkanFixture& context) {
	ManagerFixture fixture{context};
	(void)fixture.storageSystem.intern("retry");
	(void)fixture.storageSystem.intern("flowui.icon.atlas.region");
	fixture.constrainCpuBudget();
	const auto failedRegistration = fixture.manager.registerSvg("retry", landscapeSource);
	FLOWUI_CHECK(!failedRegistration);
	FLOWUI_CHECK(Access::controller(fixture.manager).documentsByKey.empty());
	fixture.restoreCpuBudget();
	FLOWUI_CHECK(FlowUi::test::requireValue(fixture.manager.registerSvg("retry", landscapeSource)));
	const auto& controller = Access::controller(fixture.manager);
	const uint64_t areaBefore = controller.atlasPages.front().usedArea;
	fixture.constrainCpuBudget();
	FLOWUI_CHECK_THROWS(Access::variant(fixture.manager, "retry", 24, 24));
	FLOWUI_CHECK(controller.variantsByKeyAndSize.empty());
	FLOWUI_CHECK(controller.atlasPages.front().usedArea == areaBefore);
	fixture.restoreCpuBudget();
	FLOWUI_CHECK(Access::variant(fixture.manager, "retry", 24, 24).texture);
}

void testUploadedPixelsAndBlending(FlowUi::test::HeadlessVulkanFixture& context) {
	ManagerFixture fixture{context};
	FLOWUI_CHECK(
		FlowUi::test::requireValue(fixture.manager.registerSvg("pixels", landscapeSource)));
	const auto cpu = Access::raster(fixture.manager, "pixels", 24, 24);
	const auto variant = Access::variant(fixture.manager, "pixels", 24, 24);
	const auto& controller = Access::controller(fixture.manager);
	const auto nativeView =
		fixture.storageSystem.nativeImageView(controller.atlasPages[variant.pageIndex].view);
	const auto nativeSampler = fixture.storageSystem.nativeSampler(controller.atlasSampler);
	FlowUi::test::IconGpuSampler readback{context.context()};
	const auto imageView =
		reinterpret_cast<VkImageView>(static_cast<uintptr_t>(nativeView.nativeImageView));
	const auto sampler =
		reinterpret_cast<VkSampler>(static_cast<uintptr_t>(nativeSampler.nativeSampler));
	const auto actual =
		readback.sample(imageView, sampler, variant.contentRect.x + variant.sourceWidth / 2,
						variant.contentRect.y + variant.sourceHeight / 2);
	const size_t pixelOffset =
		(static_cast<size_t>(cpu.image.height / 2) * cpu.image.width + cpu.image.width / 2) * 4;
	const float alpha =
		static_cast<float>(std::to_integer<uint8_t>(cpu.image.pixels[pixelOffset + 3])) / 255.0f;
	FLOWUI_CHECK(std::abs(actual[3] - alpha) < 0.0001f);
	for (size_t channelIndex = 0; channelIndex < 3; ++channelIndex) {
		const float srgb = static_cast<float>(std::to_integer<uint8_t>(
							   cpu.image.pixels[pixelOffset + channelIndex])) /
						   255.0f;
		const float linear =
			srgb <= 0.04045f ? srgb / 12.92f : std::pow((srgb + 0.055f) / 1.055f, 2.4f);

		// Device sRGB decoding may use a finite-precision approximation.
		constexpr float sampleTolerance = 1.0f / 255.0f;
		FLOWUI_CHECK(std::abs(actual[channelIndex] - linear) < sampleTolerance);
		FLOWUI_CHECK(std::abs(actual[channelIndex + 4] - (linear * alpha + 1.0f - alpha)) <
					 sampleTolerance);
	}
	const auto padding =
		readback.sample(imageView, sampler, variant.paddedRect.x, variant.paddedRect.y);
	FLOWUI_CHECK(padding[0] == 0 && padding[1] == 0 && padding[2] == 0 && padding[3] == 0);
	FLOWUI_CHECK(padding[4] == 1 && padding[5] == 1 && padding[6] == 1 && padding[7] == 1);
}

void testSubmittedRegionRetirement(FlowUi::test::HeadlessVulkanFixture& context) {
	ManagerFixture fixture{context};
	FLOWUI_CHECK(FlowUi::test::requireValue(fixture.manager.registerSvg("held", landscapeSource)));
	const auto variant = Access::variant(fixture.manager, "held", 24, 24);
	const auto& controller = Access::controller(fixture.manager);
	const uint64_t occupiedArea = controller.atlasPages.front().usedArea;
	const auto fallback = fixture.storageSystem.createAnonymousTexture(
		storage::TextureViewDesc{.imageView = controller.atlasPages.front().view,
								 .sampler = controller.atlasSampler,
								 .sourceWidth = 64,
								 .sourceHeight = 64});
	fixture.storageSystem.setFallbackTexture(fallback);
	constexpr FlowUi::WindowId window = 17;
	fixture.storageSystem.registerWindow(window,
										 storage::WindowStorageDesc{.framesInFlight = 1,
																	.workerCount = 1,
																	.initialTextureBindings = 8,
																	.maxTextureBindings = 16});
	const auto frame = fixture.storageSystem.beginFrame(window, storage::FrameStorageDesc{});
	const std::array textures{variant.texture};
	const auto bindings = fixture.storageSystem.prepareTextureBindings(frame, textures);
	fixture.storageSystem.acknowledgeTextureBindings(frame, bindings.dirtyBindings);
	const auto lease = fixture.storageSystem.sealFrame(frame);
	const auto view = fixture.storageSystem.nativeImageView(controller.atlasPages.front().view);
	const auto nativeSampler = fixture.storageSystem.nativeSampler(controller.atlasSampler);
	FlowUi::test::IconGpuSampler readback{context.context()};
	const auto sampled = readback.sample(
		reinterpret_cast<VkImageView>(static_cast<uintptr_t>(view.nativeImageView)),
		reinterpret_cast<VkSampler>(static_cast<uintptr_t>(nativeSampler.nativeSampler)),
		variant.contentRect.x, variant.contentRect.y);
	FLOWUI_CHECK(sampled[3] > 0);
	const auto submission = fixture.storageSystem.noteSubmission(lease);
	FLOWUI_CHECK(FlowUi::test::requireValue(fixture.manager.remove("held")));
	fixture.storageSystem.collect();
	Access::beginTick(fixture.manager);
	FLOWUI_CHECK(!controller.retiredRegions.empty());
	FLOWUI_CHECK(controller.atlasPages.front().usedArea == occupiedArea);
	FLOWUI_CHECK(!fixture.storageSystem.textureRetirementComplete(variant.texture));
	fixture.storageSystem.noteCompleted(submission);
	fixture.storageSystem.collect();
	Access::beginTick(fixture.manager);
	FLOWUI_CHECK(controller.retiredRegions.empty());
	FLOWUI_CHECK(controller.atlasPages.front().usedArea == 0);
	fixture.storageSystem.unregisterWindow(window, submission.serial);
	fixture.storageSystem.releaseAnonymousTexture(fallback);
}

void testRenderCommandValidation(FlowUi::test::HeadlessVulkanFixture& context) {
	ManagerFixture fixture{context};
	FLOWUI_CHECK(
		FlowUi::test::requireValue(fixture.manager.registerSvg("render", landscapeSource)));
	auto reference = FlowUi::test::requireValue(fixture.manager.textureRef("render"));
	Clay_RenderCommand command{};
	command.boundingBox.width = 24;
	command.boundingBox.height = 24;
	command.commandType = CLAY_RENDER_COMMAND_TYPE_IMAGE;
	command.renderData.image.imageData = &reference;
	Clay_RenderCommandArray commands{.capacity = 1, .length = 1, .internalArray = &command};
	Access::prepare(fixture.manager, commands);
	FLOWUI_CHECK(reference.handle && reference.sourceWidth == 24 && reference.sourceHeight == 12);
	reference = FlowUi::test::requireValue(fixture.manager.textureRef("render"));
	command.boundingBox.width = 100'000'000.0f;
	command.boundingBox.height = 100'000'000.0f;
	FLOWUI_CHECK_THROWS(Access::prepare(fixture.manager, commands));
	command.boundingBox.width = std::numeric_limits<float>::infinity();
	FLOWUI_CHECK_THROWS(Access::prepare(fixture.manager, commands));
}
template <typename Provider>
void registerProviderOutputChecks(FlowUi::test::Runner& runner,
								  FlowUi::test::HeadlessVulkanFixture& context) {
	if constexpr (requires { Provider::corruptRaster.store(true); }) {
		runner.run(
			"invalid selected-provider pixels cannot reserve or publish an atlas region", [&] {
				ManagerFixture fixture{context};
				FLOWUI_CHECK(FlowUi::test::requireValue(
					fixture.manager.registerSvg("invalid-output", landscapeSource)));
				const auto& controller = Access::controller(fixture.manager);
				const auto before = controller.atlasPages.front().usedArea;
				struct RasterReset {
					~RasterReset() noexcept { Provider::corruptRaster.store(false); }
				} reset;
				Provider::corruptRaster.store(true);
				FLOWUI_CHECK_THROWS(Access::variant(fixture.manager, "invalid-output", 24, 24));
				FLOWUI_CHECK(controller.variantsByKeyAndSize.empty());
				FLOWUI_CHECK(controller.atlasPages.front().usedArea == before);
				Provider::corruptRaster.store(false);
				FLOWUI_CHECK(Access::variant(fixture.manager, "invalid-output", 24, 24).texture);
			});
	}
}

} // namespace

int main() {
	try {
		FlowUi::test::HeadlessVulkanFixture context;
		FlowUi::test::Runner runner;
		runner.run("icon source ownership, atlas fit, size reuse and retirement",
				   [&] { testRegistrationCacheAndRemoval(context); });
		runner.run("icon source and upload failures roll back before retry",
				   [&] { testSourceAndUploadRollback(context); });
		runner.run("actual uploaded GPU icon pixels, sRGB sampling and straight-alpha blending",
				   [&] { testUploadedPixelsAndBlending(context); });
		runner.run("real icon render-command path and dimension validation",
				   [&] { testRenderCommandValidation(context); });
		runner.run("submitted icon regions remain held until completion",
				   [&] { testSubmittedRegionRetirement(context); });
		registerProviderOutputChecks<icon::ActiveIconRasterProvider>(runner, context);
		return runner.finish();
	} catch (const FlowUi::test::VulkanUnavailable& error) {
		std::cerr << error.what() << '\n';
#if FLOWUI_TEST_REQUIRE_VULKAN_DEVICE
		return 1;
#else
		return 77;
#endif
	}
}
