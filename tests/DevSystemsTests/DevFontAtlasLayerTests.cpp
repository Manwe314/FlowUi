#include "HeadlessVulkanFixture.hpp"
#include "TestHarness.hpp"

#include "FlowUi/Public/Flow.hpp"
#include "devSystems/devInterface/Catalogue/DevCatalogueModel.hpp"
#include "devSystems/devInterface/Permanents/Backend/DevInterfaceState.hpp"
#include "devSystems/devTooling/DevTooling.hpp"
#include "FlowUi/Private/resourceService/FontCatalogController.hpp"
#include "FlowUi/Private/storageSystem/FlowStorageSystem.hpp"
#include "FlowUi/Private/storageSystem/vulkan/Vma.hpp"
#include "FlowUi/Public/managers/FontManager.hpp"
#include <algorithm>
#include <array>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <limits>
#include <span>
#include <vector>

namespace {
namespace storage = FlowUi::detail::storage;
using Controller = FlowUi::detail::manager_storage::FontCatalogController;
using Layer = FlowUi::FontManager::DevAtlasLayerView;
using Pixel = std::array<uint8_t, 4>;
constexpr uint32_t atlas_extent = 8;
constexpr Pixel first_pixel{17, 91, 203, 0};
constexpr Pixel second_pixel{219, 41, 66, 0};

[[nodiscard]] uint32_t packed_pixel(const Pixel& pixel, bool opaque = false) noexcept {
	return pixel[0] | uint32_t(pixel[1]) << 8u | uint32_t(pixel[2]) << 16u |
		   uint32_t(opaque ? 255 : pixel[3]) << 24u;
}
[[nodiscard]] std::vector<uint8_t> page(const Pixel& pixel) {
	std::vector<uint8_t> pixels(atlas_extent * atlas_extent * pixel.size());
	for (size_t offset = 0; offset < pixels.size(); offset += pixel.size())
		std::copy(pixel.begin(), pixel.end(), pixels.begin() + offset);
	return pixels;
}
template <class Operation>
void expect_budget_failure(Operation&& operation, FlowUi::ErrorSite site) {
	bool caught_budget_failure = false;
	try {
		operation();
	} catch (const FlowUi::FlowUiException& exception) {
		FLOWUI_CHECK(exception.error().code == FlowUi::ErrorCode::StorageBudgetExceeded);
		FLOWUI_CHECK(exception.error().site == site);
		caught_budget_failure = true;
		std::cout << "Expected " << FlowUi::errorName(exception.error().code) << " at "
				  << FlowUi::errorSiteName(exception.error().site) << '\n';
	}
	FLOWUI_CHECK(caught_budget_failure);
}

/* Sample the actual Vulkan views, including swizzle and array-layer selection. */
class AtlasSampler {
public:
	explicit AtlasSampler(VulkanContext& context) : context_(context) {
		try {
			VkBufferCreateInfo buffer_info{VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO};
			buffer_info.size = sizeof(std::array<uint32_t, 3>);
			buffer_info.usage = VK_BUFFER_USAGE_STORAGE_BUFFER_BIT;
			VmaAllocationCreateInfo allocation_info{};
			allocation_info.usage = VMA_MEMORY_USAGE_AUTO;
			allocation_info.flags =
				VMA_ALLOCATION_CREATE_HOST_ACCESS_RANDOM_BIT | VMA_ALLOCATION_CREATE_MAPPED_BIT;
			VmaAllocationInfo allocation_details{};
			FLOWUI_CHECK(vmaCreateBuffer(context_.allocator, &buffer_info, &allocation_info,
										 &buffer_, &allocation_,
										 &allocation_details) == VK_SUCCESS);
			mapped_ = allocation_details.pMappedData;
			const std::array bindings{
				VkDescriptorSetLayoutBinding{0, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1,
											 VK_SHADER_STAGE_COMPUTE_BIT, nullptr},
				VkDescriptorSetLayoutBinding{1, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1,
											 VK_SHADER_STAGE_COMPUTE_BIT, nullptr},
				VkDescriptorSetLayoutBinding{2, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1,
											 VK_SHADER_STAGE_COMPUTE_BIT, nullptr}};
			VkDescriptorSetLayoutCreateInfo set_info{
				VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO};
			set_info.bindingCount = static_cast<uint32_t>(bindings.size());
			set_info.pBindings = bindings.data();
			FLOWUI_CHECK(vkCreateDescriptorSetLayout(context_.device, &set_info, nullptr,
													 &set_layout_) == VK_SUCCESS);
			const std::array sizes{
				VkDescriptorPoolSize{VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 2},
				VkDescriptorPoolSize{VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1}};
			VkDescriptorPoolCreateInfo pool_info{VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO};
			pool_info.maxSets = 1;
			pool_info.poolSizeCount = static_cast<uint32_t>(sizes.size());
			pool_info.pPoolSizes = sizes.data();
			FLOWUI_CHECK(vkCreateDescriptorPool(context_.device, &pool_info, nullptr,
												&descriptor_pool_) == VK_SUCCESS);
			VkDescriptorSetAllocateInfo sets{VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO};
			sets.descriptorPool = descriptor_pool_;
			sets.descriptorSetCount = 1;
			sets.pSetLayouts = &set_layout_;
			FLOWUI_CHECK(vkAllocateDescriptorSets(context_.device, &sets, &descriptor_set_) ==
						 VK_SUCCESS);
			VkPipelineLayoutCreateInfo layout_info{VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO};
			layout_info.setLayoutCount = 1;
			layout_info.pSetLayouts = &set_layout_;
			FLOWUI_CHECK(vkCreatePipelineLayout(context_.device, &layout_info, nullptr,
												&pipeline_layout_) == VK_SUCCESS);
			std::ifstream input(FLOWUI_FONT_ATLAS_TEST_COMPUTE_SPV,
								std::ios::binary | std::ios::ate);
			FLOWUI_CHECK(input.good());
			const auto byte_count = input.tellg();
			FLOWUI_CHECK(byte_count > 0 && byte_count % sizeof(uint32_t) == 0);
			std::vector<uint32_t> code(static_cast<size_t>(byte_count) / sizeof(uint32_t));
			input.seekg(0);
			input.read(reinterpret_cast<char*>(code.data()), byte_count);
			FLOWUI_CHECK(input.good());
			VkShaderModuleCreateInfo shader{VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO};
			shader.codeSize = static_cast<size_t>(byte_count);
			shader.pCode = code.data();
			FLOWUI_CHECK(vkCreateShaderModule(context_.device, &shader, nullptr, &shader_module_) ==
						 VK_SUCCESS);
			VkComputePipelineCreateInfo pipeline_info{
				VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO};
			pipeline_info.stage = {VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO};
			pipeline_info.stage.stage = VK_SHADER_STAGE_COMPUTE_BIT;
			pipeline_info.stage.module = shader_module_;
			pipeline_info.stage.pName = "main";
			pipeline_info.layout = pipeline_layout_;
			FLOWUI_CHECK(vkCreateComputePipelines(context_.device, VK_NULL_HANDLE, 1,
												  &pipeline_info, nullptr,
												  &pipeline_) == VK_SUCCESS);
			VkCommandPoolCreateInfo commands{VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO};
			commands.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
			commands.queueFamilyIndex = context_.graphicsQFamily;
			FLOWUI_CHECK(vkCreateCommandPool(context_.device, &commands, nullptr, &command_pool_) ==
						 VK_SUCCESS);
			VkCommandBufferAllocateInfo command{VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO};
			command.commandPool = command_pool_;
			command.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
			command.commandBufferCount = 1;
			FLOWUI_CHECK(vkAllocateCommandBuffers(context_.device, &command, &command_buffer_) ==
						 VK_SUCCESS);
			VkFenceCreateInfo fence_info{VK_STRUCTURE_TYPE_FENCE_CREATE_INFO};
			FLOWUI_CHECK(vkCreateFence(context_.device, &fence_info, nullptr, &fence_) ==
						 VK_SUCCESS);
		} catch (...) {
			reset();
			throw;
		}
	}
	~AtlasSampler() { reset(); }
	AtlasSampler(const AtlasSampler&) = delete;
	AtlasSampler& operator=(const AtlasSampler&) = delete;

	[[nodiscard]] std::array<uint32_t, 3> sample(storage::FlowStorageSystem& resources,
												 storage::ImageViewHandle preview_view,
												 const Controller& controller) {
		const auto sampler = reinterpret_cast<VkSampler>(
			static_cast<uintptr_t>(resources.nativeSampler(controller.atlasSampler).nativeSampler));
		const VkDescriptorImageInfo preview{
			sampler,
			reinterpret_cast<VkImageView>(
				static_cast<uintptr_t>(resources.nativeImageView(preview_view).nativeImageView)),
			VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL};
		const VkDescriptorImageInfo atlas{
			sampler,
			reinterpret_cast<VkImageView>(static_cast<uintptr_t>(
				resources.nativeImageView(controller.atlasView).nativeImageView)),
			VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL};
		const VkDescriptorBufferInfo output{buffer_, 0, sizeof(std::array<uint32_t, 3>)};
		std::array<VkWriteDescriptorSet, 3> writes{};
		for (uint32_t index = 0; index < writes.size(); ++index) {
			writes[index] = {VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET};
			writes[index].dstSet = descriptor_set_;
			writes[index].dstBinding = index;
			writes[index].descriptorCount = 1;
			writes[index].descriptorType = index == 2 ? VK_DESCRIPTOR_TYPE_STORAGE_BUFFER
													  : VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
		}
		writes[0].pImageInfo = &preview;
		writes[1].pImageInfo = &atlas;
		writes[2].pBufferInfo = &output;
		vkUpdateDescriptorSets(context_.device, static_cast<uint32_t>(writes.size()), writes.data(),
							   0, nullptr);
		FLOWUI_CHECK(vkResetCommandBuffer(command_buffer_, 0) == VK_SUCCESS);
		VkCommandBufferBeginInfo begin{VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO};
		begin.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
		FLOWUI_CHECK(vkBeginCommandBuffer(command_buffer_, &begin) == VK_SUCCESS);
		VkMemoryBarrier reads{VK_STRUCTURE_TYPE_MEMORY_BARRIER};
		reads.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
		reads.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
		vkCmdPipelineBarrier(command_buffer_, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT,
							 VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, 0, 1, &reads, 0, nullptr, 0,
							 nullptr);
		vkCmdBindPipeline(command_buffer_, VK_PIPELINE_BIND_POINT_COMPUTE, pipeline_);
		vkCmdBindDescriptorSets(command_buffer_, VK_PIPELINE_BIND_POINT_COMPUTE, pipeline_layout_,
								0, 1, &descriptor_set_, 0, nullptr);
		vkCmdDispatch(command_buffer_, 1, 1, 1);
		VkMemoryBarrier host{VK_STRUCTURE_TYPE_MEMORY_BARRIER};
		host.srcAccessMask = VK_ACCESS_SHADER_WRITE_BIT;
		host.dstAccessMask = VK_ACCESS_HOST_READ_BIT;
		vkCmdPipelineBarrier(command_buffer_, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
							 VK_PIPELINE_STAGE_HOST_BIT, 0, 1, &host, 0, nullptr, 0, nullptr);
		FLOWUI_CHECK(vkEndCommandBuffer(command_buffer_) == VK_SUCCESS);
		FLOWUI_CHECK(vkResetFences(context_.device, 1, &fence_) == VK_SUCCESS);
		VkSubmitInfo submit{VK_STRUCTURE_TYPE_SUBMIT_INFO};
		submit.commandBufferCount = 1;
		submit.pCommandBuffers = &command_buffer_;
		FLOWUI_CHECK(vkQueueSubmit(context_.graphicsQ, 1, &submit, fence_) == VK_SUCCESS);
		FLOWUI_CHECK(vkWaitForFences(context_.device, 1, &fence_, VK_TRUE, 5'000'000'000) ==
					 VK_SUCCESS);
		FLOWUI_CHECK(vmaInvalidateAllocation(context_.allocator, allocation_, 0, VK_WHOLE_SIZE) ==
					 VK_SUCCESS);
		std::array<uint32_t, 3> samples{};
		std::memcpy(samples.data(), mapped_, sizeof(samples));
		return samples;
	}

private:
	void reset() noexcept {
		if (fence_)
			vkDestroyFence(context_.device, fence_, nullptr);
		if (command_pool_)
			vkDestroyCommandPool(context_.device, command_pool_, nullptr);
		if (pipeline_)
			vkDestroyPipeline(context_.device, pipeline_, nullptr);
		if (pipeline_layout_)
			vkDestroyPipelineLayout(context_.device, pipeline_layout_, nullptr);
		if (shader_module_)
			vkDestroyShaderModule(context_.device, shader_module_, nullptr);
		if (descriptor_pool_)
			vkDestroyDescriptorPool(context_.device, descriptor_pool_, nullptr);
		if (set_layout_)
			vkDestroyDescriptorSetLayout(context_.device, set_layout_, nullptr);
		if (buffer_)
			vmaDestroyBuffer(context_.allocator, buffer_, allocation_);
	}
	VulkanContext& context_;
	VkDescriptorSetLayout set_layout_{};
	VkDescriptorPool descriptor_pool_{};
	VkDescriptorSet descriptor_set_{};
	VkPipelineLayout pipeline_layout_{};
	VkShaderModule shader_module_{};
	VkPipeline pipeline_{};
	VkCommandPool command_pool_{};
	VkCommandBuffer command_buffer_{};
	VkFence fence_{};
	VkBuffer buffer_{};
	VmaAllocation allocation_{};
	void* mapped_{};
};

[[nodiscard]] storage::StorageConfig test_config() noexcept {
	storage::StorageConfig config{};
	config.initialPersistentCpuBytes = 4096;
	config.initialStringBytes = 4096;
	config.initialDecodeScratchBytes = 4096;
	config.initialUploadStagingBytes = 4096;
	config.transientBytesPerFramePerWindow = 4096;
	config.transientBytesPerWorker = 4096;
	config.initialInstanceBytesPerFrame = 4096;
	config.cpuSoftBudgetBytes = 4 * 1024 * 1024;
	config.gpuSoftBudgetBytes = 4 * 1024 * 1024;
	config.detailedTracking = true;
	return config;
}
[[nodiscard]] FlowUi::TextureHandle create_fallback(storage::FlowStorageSystem& resources) {
	const auto image = resources.createImage(
		storage::ImageDesc{.width = 1, .height = 1, .format = storage::PixelFormat::Rgba8Unorm});
	const auto view = resources.createImageView(image, storage::ImageViewDesc{});
	const auto sampler = resources.acquireSampler(storage::SamplerDesc{});
	constexpr std::array<std::byte, 4> pixel{std::byte{255}, std::byte{255}, std::byte{255},
											 std::byte{255}};
	const auto blob = resources.createBlob(pixel, 0);
	static_cast<void>(resources.enqueueUpload(
		storage::UploadRequest{.byteCount = pixel.size(),
							   .source = blob,
							   .destinationImage = image,
							   .destination = storage::UploadDestination::Image,
							   .releaseSourceWhenComplete = true}));
	resources.flushUploads();
	const auto texture = resources.createAnonymousTexture(storage::TextureViewDesc{
		.imageView = view, .sampler = sampler, .sourceWidth = 1, .sourceHeight = 1});
	resources.releaseImageView(view);
	resources.releaseImage(image);
	resources.releaseSampler(sampler);
	resources.setFallbackTexture(texture);
	return texture;
}
void check_layer_bindings(storage::FlowStorageSystem& resources, const storage::FrameToken& frame,
						  std::span<const Layer> layers) {
	const std::array textures{layers[0].texture, layers[1].texture};
	const auto bindings = resources.prepareTextureBindings(frame, textures);
	const auto* first = bindings.binding(textures[0]);
	const auto* second = bindings.binding(textures[1]);
	FLOWUI_CHECK(first && second && first->descriptorIndex != second->descriptorIndex);
	FLOWUI_CHECK(first->nativeImageView ==
				 resources.nativeImageView(layers[0].image_view).nativeImageView);
	FLOWUI_CHECK(second->nativeImageView ==
				 resources.nativeImageView(layers[1].image_view).nativeImageView);
	FLOWUI_CHECK(first->state == storage::ResourceState::Ready &&
				 second->state == storage::ResourceState::Ready);
	resources.acknowledgeTextureBindings(frame, bindings.dirtyBindings);
}
void check_layers_and_growth(FlowUi::test::HeadlessVulkanFixture& vulkan) {
	storage::FlowStorageSystem resources(vulkan.context());
	resources.initialize(test_config());
	const auto fallback = create_fallback(resources);
	constexpr FlowUi::WindowId window = 97;
	resources.registerWindow(window, storage::WindowStorageDesc{.framesInFlight = 2,
																.workerCount = 1,
																.initialTextureBindings = 8,
																.maxTextureBindings = 16,
																.transientBytesPerFrame = 8192,
																.transientBytesPerWorker = 4096});
	std::vector<Layer> retired_layers;
	retired_layers.reserve(4);
	{
		Controller controller(resources, atlas_extent);
		AtlasSampler sampler(vulkan.context());
		controller.uploadLayerTransactional(0, page(first_pixel));
		FLOWUI_CHECK(controller.dev_atlas_layers.size() == 1);
		const auto first_layer = controller.dev_atlas_layers.front();
		const auto original_image = controller.atlasImage;
		const auto original_array_view = controller.atlasView;
		controller.uploadLayerTransactional(1, page(second_pixel));
		FLOWUI_CHECK(controller.dev_atlas_layers.size() == 2);
		FLOWUI_CHECK(controller.dev_atlas_layers.front().texture == first_layer.texture);
		FLOWUI_CHECK(controller.dev_atlas_layers.front().image_view == first_layer.image_view);
		FLOWUI_CHECK(controller.atlasImage == original_image &&
					 controller.atlasView == original_array_view);
		const auto second_layer = controller.dev_atlas_layers[1];
		const auto old_frame = resources.beginFrame(
			window, storage::FrameStorageDesc{.frameSlot = 0, .frameNumber = 1});
		check_layer_bindings(resources, old_frame, controller.dev_atlas_layers);
		const auto old_submission = resources.noteSubmission(resources.sealFrame(old_frame));
		FLOWUI_CHECK(first_layer.texture != second_layer.texture);
		FLOWUI_CHECK(first_layer.image_view != second_layer.image_view);
		FLOWUI_CHECK(first_layer.layer_index == 0 && second_layer.layer_index == 1);
		FLOWUI_CHECK(resources.nativeImageView(first_layer.image_view).image == original_image);
		FLOWUI_CHECK(resources.nativeImageView(second_layer.image_view).image == original_image);
		const auto first_sample = sampler.sample(resources, first_layer.image_view, controller);
		const auto second_sample = sampler.sample(resources, second_layer.image_view, controller);
		FLOWUI_CHECK(first_sample[0] == packed_pixel(first_pixel, true));
		FLOWUI_CHECK(second_sample[0] == packed_pixel(second_pixel, true));
		FLOWUI_CHECK(first_sample[1] == packed_pixel(first_pixel));
		FLOWUI_CHECK(first_sample[2] == packed_pixel(second_pixel));
		const auto normal_view = resources.createImageView(
			original_image,
			storage::ImageViewDesc{.type = storage::ImageType::Image2D, .baseArrayLayer = 1});
		FLOWUI_CHECK(sampler.sample(resources, normal_view, controller)[0] ==
					 packed_pixel(second_pixel));
		resources.releaseImageView(normal_view);
		resources.collect();

		for (uint32_t layer = 2; layer < FlowUi::FontManager::kInitialAtlasLayerCapacity; ++layer)
			controller.uploadLayerTransactional(layer, page(first_pixel));
		retired_layers.assign(controller.dev_atlas_layers.begin(),
							  controller.dev_atlas_layers.end());
		const auto capacity_before = controller.borrowedAtlas.layersCapacity;
		FLOWUI_CHECK(resources.retainTextureForDevInspection(first_layer.texture));
		controller.uploadLayerTransactional(capacity_before, page(second_pixel));
		FLOWUI_CHECK(controller.borrowedAtlas.layersCapacity > capacity_before);
		FLOWUI_CHECK(controller.dev_atlas_layers.size() == capacity_before + 1);
		FLOWUI_CHECK(controller.atlasImage != original_image &&
					 controller.atlasView != original_array_view);
		for (size_t layer_index = 0; layer_index < retired_layers.size(); ++layer_index) {
			const auto& current = controller.dev_atlas_layers[layer_index];
			FLOWUI_CHECK(current.texture != retired_layers[layer_index].texture);
			FLOWUI_CHECK(current.image_view != retired_layers[layer_index].image_view);
			FLOWUI_CHECK(resources.nativeImageView(current.image_view).image ==
						 controller.atlasImage);
		}
		FLOWUI_CHECK(sampler.sample(resources, controller.dev_atlas_layers[0].image_view,
									controller)[0] == packed_pixel(first_pixel, true));
		FLOWUI_CHECK(sampler.sample(resources, controller.dev_atlas_layers[1].image_view,
									controller)[0] == packed_pixel(second_pixel, true));
		FLOWUI_CHECK(sampler.sample(resources, controller.dev_atlas_layers.back().image_view,
									controller)[0] == packed_pixel(second_pixel, true));
		resources.collect();
		FLOWUI_CHECK(!resources.textureRetirementComplete(retired_layers[0].texture));
		FLOWUI_CHECK(!resources.textureRetirementComplete(retired_layers[1].texture));
		const auto new_frame = resources.beginFrame(
			window, storage::FrameStorageDesc{.frameSlot = 1, .frameNumber = 2});
		check_layer_bindings(resources, new_frame, controller.dev_atlas_layers);
		const auto new_submission = resources.noteSubmission(resources.sealFrame(new_frame));
		resources.noteCompleted(old_submission);
		resources.collect();
		FLOWUI_CHECK(!resources.textureRetirementComplete(first_layer.texture));
		FLOWUI_CHECK(resources.nativeImage(original_image).nativeImage != 0);
		FLOWUI_CHECK(sampler.sample(resources, first_layer.image_view, controller)[0] ==
					 packed_pixel(first_pixel, true));
		FLOWUI_CHECK(resources.textureRetirementComplete(second_layer.texture));
		resources.releaseTextureFromDevInspection(first_layer.texture);
		resources.collect();
		for (const auto& retired : retired_layers) {
			FLOWUI_CHECK(resources.textureRetirementComplete(retired.texture));
			FLOWUI_CHECK(resources.nativeImageView(retired.image_view).nativeImageView == 0);
		}
		FLOWUI_CHECK(resources.nativeImage(original_image).nativeImage == 0);
		resources.noteCompleted(new_submission);
		resources.unregisterWindow(window, new_submission.serial);
	}
	resources.collect();
	// Only the independent fallback, retained by storage, survives controller cleanup.
	for (const auto kind : {storage::ResourceKind::GpuImage, storage::ResourceKind::ImageView,
							storage::ResourceKind::TextureView, storage::ResourceKind::Sampler})
		FLOWUI_CHECK(resources.resourceStats(kind).live == 1);
	resources.releaseAnonymousTexture(fallback);
}

void check_budget_and_partial_publication_rollback(FlowUi::test::HeadlessVulkanFixture& vulkan) {
	storage::FlowStorageSystem resources(vulkan.context());
	resources.initialize(test_config());
	{
		Controller controller(resources, atlas_extent);
		for (uint32_t layer = 0; layer < FlowUi::FontManager::kInitialAtlasLayerCapacity; ++layer)
			controller.uploadLayerTransactional(layer, page(first_pixel));
		const auto original_layers = controller.dev_atlas_layers;
		const auto original_image = controller.atlasImage;
		const auto original_array_view = controller.atlasView;
		const auto original_pixels = controller.atlasLayerPixels;
		const auto original_stats = resources.stats();
		resources.setBudget(original_stats.cpuSoftBudgetBytes,
							original_stats.gpuLiveBytes + original_stats.gpuRetiredBytes);
		std::cout << "GPU growth budget="
				  << original_stats.gpuLiveBytes + original_stats.gpuRetiredBytes
				  << " bytes; existing live=" << original_stats.gpuLiveBytes << '\n';
		expect_budget_failure([&] { controller.uploadLayerTransactional(4, page(second_pixel)); },
							  FlowUi::ErrorSite::ResourceCreateImage);
		resources.collect();
		FLOWUI_CHECK(controller.atlasImage == original_image &&
					 controller.atlasView == original_array_view);
		FLOWUI_CHECK(controller.atlasLayerPixels == original_pixels);
		FLOWUI_CHECK(controller.dev_atlas_layers.size() == original_layers.size());
		for (size_t layer_index = 0; layer_index < original_layers.size(); ++layer_index)
			FLOWUI_CHECK(controller.dev_atlas_layers[layer_index].texture ==
						 original_layers[layer_index].texture);
		FLOWUI_CHECK(resources.resourceStats(storage::ResourceKind::ImageView).live ==
					 original_layers.size() + 1);
		AtlasSampler sampler(vulkan.context());
		FLOWUI_CHECK(sampler.sample(resources, original_layers[1].image_view, controller)[0] ==
					 packed_pixel(first_pixel, true));
	}
	resources.collect();
	FLOWUI_CHECK(resources.resourceStats(storage::ResourceKind::TextureView).live == 0);

	storage::FlowStorageSystem limited(vulkan.context());
	auto config = test_config();
	limited.initialize(config);
	{
		Controller controller(limited, atlas_extent);
		controller.uploadLayerTransactional(0, page(first_pixel));
		const auto original = controller.dev_atlas_layers.front();
		// Exhaust only CPU upload bytes after the new preview view and texture are created.
		static_cast<void>(limited.intern("flowui.font.atlas.layer"));
		limited.collect();
		const auto cpu_stats = limited.stats();
		// ResourceMetadata also reports container bookkeeping outside these budgeted pools.
		const auto cpu_live_bytes =
			cpu_stats.cpu[static_cast<size_t>(storage::MemoryClass::Persistent)].liveBytes +
			cpu_stats.cpu[static_cast<size_t>(storage::MemoryClass::StringPool)].liveBytes;
		limited.setBudget(std::max<uint64_t>(1, cpu_live_bytes), config.gpuSoftBudgetBytes);
		std::cout << "CPU preview-upload budget=" << cpu_live_bytes << " bytes\n";
		expect_budget_failure([&] { controller.uploadLayerTransactional(1, page(second_pixel)); },
							  FlowUi::ErrorSite::ResourceAllocatePersistent);
		limited.collect();
		FLOWUI_CHECK(controller.atlasLayerPixels.size() == 1 &&
					 controller.dev_atlas_layers.size() == 1);
		FLOWUI_CHECK(controller.dev_atlas_layers.front().texture == original.texture);
		FLOWUI_CHECK(limited.resourceStats(storage::ResourceKind::ImageView).live == 2);
		FLOWUI_CHECK(limited.resourceStats(storage::ResourceKind::TextureView).live == 1);
		FLOWUI_CHECK(limited.resourceStats(storage::ResourceKind::ImageView).slots == 3);
		FLOWUI_CHECK(limited.resourceStats(storage::ResourceKind::ImageView).free == 1);
		FLOWUI_CHECK(limited.resourceStats(storage::ResourceKind::TextureView).slots == 2);
		FLOWUI_CHECK(limited.resourceStats(storage::ResourceKind::TextureView).free == 1);
		FLOWUI_CHECK(limited.resourceStats(storage::ResourceKind::GpuImage).live == 1);
		AtlasSampler sampler(vulkan.context());
		FLOWUI_CHECK(sampler.sample(limited, original.image_view, controller)[0] ==
					 packed_pixel(first_pixel, true));
	}
	limited.collect();
	FLOWUI_CHECK(limited.resourceStats(storage::ResourceKind::ImageView).live == 0);
	FLOWUI_CHECK(limited.resourceStats(storage::ResourceKind::TextureView).live == 0);
}

void check_public_font_catalogue() {
	FlowUi::AppConfig config{};
	config.window.title = "Font atlas catalogue layer regression";
	config.window.width = 480;
	config.window.height = 360;
	config.vk.enableValidation = true;
	auto app = FlowUi::makeApplication(config);
	const auto initial_count = app.fonts().dev_atlas_layers().size();
	FLOWUI_CHECK(initial_count > 0);
	const auto family = app.fonts().createFamily(FlowUi::FontFamilyCreateInfo{
		.name = "Atlas layer regression",
		.faces = {FlowUi::FontFaceCreateInfo{.path = FLOWUI_FONT_ATLAS_TEST_FONT_PATH,
											 .name = "Second atlas layer"}}});
	FLOWUI_CHECK(family);
	const auto layers = app.fonts().dev_atlas_layers();
	FLOWUI_CHECK(layers.size() == initial_count + 1);
	FLOWUI_CHECK(layers[0].texture != layers[1].texture &&
				 layers[0].image_view != layers[1].image_view);
	const auto summary = app.fonts().devAtlas();
	FLOWUI_CHECK(summary.texture == layers.front().texture &&
				 summary.imageView == layers.front().image_view);
	const auto atlases = app.devTooling().catalogues().queryAtlases();
	size_t font_page_count = 0;
	for (const auto& atlas : atlases) {
		if (atlas.kind != FlowUi::devSystems::tooling::DevAtlasKind::FontMsdfAtlas)
			continue;
		++font_page_count;
		FLOWUI_CHECK(atlas.pageIndex < layers.size());
		FLOWUI_CHECK(atlas.layerCount == summary.layersCapacity);
		FLOWUI_CHECK(atlas.atlasTexture == layers[atlas.pageIndex].texture);
		FLOWUI_CHECK(atlas.atlasImageView == layers[atlas.pageIndex].image_view);
		FLOWUI_CHECK(atlas.width == summary.width && atlas.height == summary.height);
	}
	FLOWUI_CHECK(font_page_count == layers.size());

	const std::vector<Layer> before_failure(layers.begin(), layers.end());
	const auto failed_family = app.fonts().createFamily(FlowUi::FontFamilyCreateInfo{
		.name = "Atlas rollback regression",
		.faces = {
			FlowUi::FontFaceCreateInfo{.path = FLOWUI_FONT_ATLAS_TEST_FONT_PATH},
			FlowUi::FontFaceCreateInfo{
				.path = std::filesystem::path(FLOWUI_FONT_ATLAS_TEST_FONT_PATH).parent_path() /
						"missing-atlas-test-font.arfont"}}});
	FLOWUI_CHECK(!failed_family);
	const auto after_failure = app.fonts().dev_atlas_layers();
	FLOWUI_CHECK(after_failure.size() == before_failure.size());
	for (size_t layer_index = 0; layer_index < before_failure.size(); ++layer_index) {
		FLOWUI_CHECK(after_failure[layer_index].texture == before_failure[layer_index].texture);
		FLOWUI_CHECK(after_failure[layer_index].image_view ==
					 before_failure[layer_index].image_view);
	}
	FLOWUI_CHECK(app.fonts().getFamilyId("Atlas rollback regression") ==
				 std::numeric_limits<FlowUi::FontFamilyId>::max());
	FlowUi::devSystems::DevInterfaceState session{};
	FlowUi::devSystems::service_catalogue(app, session, true);
	for (const auto& layer : after_failure) {
		const auto asset = std::ranges::find_if(session.catalogue.assets, [&](const auto& entry) {
			return entry.domain == FlowUi::devSystems::DevCatalogueDomain::Atlases &&
				   entry.atlas.kind == FlowUi::devSystems::tooling::DevAtlasKind::FontMsdfAtlas &&
				   entry.atlas.pageIndex == layer.layer_index;
		});
		FLOWUI_CHECK(asset != session.catalogue.assets.end());
		const auto identity = asset->identity;
		FlowUi::devSystems::select_catalogue_asset(session.catalogue, identity);
		const auto* selected = FlowUi::devSystems::selected_catalogue_asset(session.catalogue);
		FLOWUI_CHECK(selected && selected->atlas.atlasTexture == layer.texture);
		FLOWUI_CHECK(selected->atlas.atlasImageView == layer.image_view);
	}
}
} // namespace

int main(int argument_count, char** arguments) {
	FlowUi::test::Runner runner;
	if (argument_count > 1 && std::string_view(arguments[1]) == "--catalogue") {
		runner.run("native font catalogue publishes and selects each used layer; family rollback "
				   "trims previews",
				   check_public_font_catalogue);
		return runner.finish();
	}
	try {
		FlowUi::test::HeadlessVulkanFixture vulkan;
		std::cout << "Font atlas test device: " << vulkan.deviceName() << '\n';
		runner.run("distinct raw RGB layers, opaque preview alpha, unchanged array data and growth "
				   "cleanup",
				   [&] { check_layers_and_growth(vulkan); });
		runner.run("budget and partial preview publication failures roll back without leaked views",
				   [&] { check_budget_and_partial_publication_rollback(vulkan); });
	} catch (const FlowUi::test::VulkanUnavailable& error) {
		std::cerr << "Vulkan unavailable: " << error.what() << '\n';
#ifdef FLOWUI_TEST_REQUIRE_VULKAN_DEVICE
		return 1;
#else
		return 77;
#endif
	}
	return runner.finish();
}
