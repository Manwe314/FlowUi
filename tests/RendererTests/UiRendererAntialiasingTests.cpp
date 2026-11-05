#include "HeadlessVulkanFixture.hpp"
#include "TestHarness.hpp"
#include "Ui/Vk_UiRenderer.hpp"
#include "internal/Vma.hpp"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <span>
#include <vector>

namespace {

constexpr uint32_t target_size = 32;
constexpr VkFormat target_format = VK_FORMAT_R8G8B8A8_UNORM;
using Pixel = std::array<uint8_t, 4>;

struct ShaderDevice {
	VulkanContext context{};
	explicit ShaderDevice(const VulkanContext& source) {
		context.instance = source.instance;
		context.phys = source.phys;
		context.graphicsQFamily = source.graphicsQFamily;
		VkPhysicalDeviceVulkan12Features features{
			VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_2_FEATURES};
		VkPhysicalDeviceFeatures2 available{VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FEATURES_2};
		available.pNext = &features;
		vkGetPhysicalDeviceFeatures2(context.phys, &available);
		if (!features.shaderSampledImageArrayNonUniformIndexing) {
			throw FlowUi::test::VulkanUnavailable(
				"quad shader test requires sampled-image nonuniform indexing");
		}
		VkPhysicalDeviceProperties properties{};
		vkGetPhysicalDeviceProperties(context.phys, &properties);
		const auto& limits = properties.limits;
		if (limits.maxPerStageDescriptorSamplers < 256 ||
			limits.maxPerStageDescriptorSampledImages < 256 ||
			limits.maxDescriptorSetSamplers < 256 || limits.maxDescriptorSetSampledImages < 256) {
			throw FlowUi::test::VulkanUnavailable(
				"quad shader test requires 256 ordinary sampled-image descriptors");
		}
		features = {VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_2_FEATURES};
		features.shaderSampledImageArrayNonUniformIndexing = VK_TRUE;
		const float priority = 1;
		VkDeviceQueueCreateInfo queue{VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO};
		queue.queueFamilyIndex = context.graphicsQFamily;
		queue.queueCount = 1;
		queue.pQueuePriorities = &priority;
		VkDeviceCreateInfo info{VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO};
		info.pNext = &features;
		info.queueCreateInfoCount = 1;
		info.pQueueCreateInfos = &queue;
		FLOWUI_CHECK(vkCreateDevice(context.phys, &info, nullptr, &context.device) == VK_SUCCESS);
		vkGetDeviceQueue(context.device, context.graphicsQFamily, 0, &context.graphicsQ);
		VmaAllocatorCreateInfo allocator{};
		allocator.instance = context.instance;
		allocator.physicalDevice = context.phys;
		allocator.device = context.device;
		allocator.vulkanApiVersion = VK_API_VERSION_1_3;
		FLOWUI_CHECK(vmaCreateAllocator(&allocator, &context.allocator) == VK_SUCCESS);
	}
	~ShaderDevice() {
		if (context.allocator)
			vmaDestroyAllocator(context.allocator);
		if (context.device)
			vkDestroyDevice(context.device, nullptr);
	}
};

struct Buffer {
	VulkanContext& context;
	VkBuffer buffer{};
	VmaAllocation allocation{};
	void* mapped{};
	~Buffer() {
		if (buffer)
			vmaDestroyBuffer(context.allocator, buffer, allocation);
	}
	void create(VkDeviceSize size, VkBufferUsageFlags usage) {
		VkBufferCreateInfo info{VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO};
		info.size = size;
		info.usage = usage;
		VmaAllocationCreateInfo allocation_info{};
		allocation_info.usage = VMA_MEMORY_USAGE_AUTO;
		allocation_info.flags =
			VMA_ALLOCATION_CREATE_HOST_ACCESS_RANDOM_BIT | VMA_ALLOCATION_CREATE_MAPPED_BIT;
		VmaAllocationInfo allocation_details{};
		FLOWUI_CHECK(vmaCreateBuffer(context.allocator, &info, &allocation_info, &buffer,
									 &allocation, &allocation_details) == VK_SUCCESS);
		mapped = allocation_details.pMappedData;
	}
	void upload(const void* source, size_t size) {
		std::memcpy(mapped, source, size);
		FLOWUI_CHECK(vmaFlushAllocation(context.allocator, allocation, 0, size) == VK_SUCCESS);
	}
};

struct Image {
	VulkanContext& context;
	VkImage image{};
	VmaAllocation allocation{};
	VkImageView view{};
	~Image() {
		if (view)
			vkDestroyImageView(context.device, view, nullptr);
		if (image)
			vmaDestroyImage(context.allocator, image, allocation);
	}
	void create(uint32_t size, VkImageUsageFlags usage) {
		VkImageCreateInfo info{VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO};
		info.imageType = VK_IMAGE_TYPE_2D;
		info.format = target_format;
		info.extent = {size, size, 1};
		info.mipLevels = 1;
		info.arrayLayers = 1;
		info.samples = VK_SAMPLE_COUNT_1_BIT;
		info.tiling = VK_IMAGE_TILING_OPTIMAL;
		info.usage = usage;
		VmaAllocationCreateInfo allocation_info{};
		allocation_info.usage = VMA_MEMORY_USAGE_AUTO_PREFER_DEVICE;
		FLOWUI_CHECK(vmaCreateImage(context.allocator, &info, &allocation_info, &image, &allocation,
									nullptr) == VK_SUCCESS);
		VkImageViewCreateInfo view_info{VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO};
		view_info.image = image;
		view_info.viewType = VK_IMAGE_VIEW_TYPE_2D;
		view_info.format = target_format;
		view_info.subresourceRange = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1};
		FLOWUI_CHECK(vkCreateImageView(context.device, &view_info, nullptr, &view) == VK_SUCCESS);
	}
};

class QuadFixture {
public:
	QuadFixture(VulkanContext& context, const std::filesystem::path& shader_directory)
		: context_(context), instance_buffer_{context}, vertex_buffer_{context}, readback_{context},
		  texture_upload_{context}, target_{context}, texture_{context} {
		instance_buffer_.create(sizeof(UiInstance), VK_BUFFER_USAGE_STORAGE_BUFFER_BIT);
		constexpr std::array<float, 16> vertices{0, 0, 0, 0, 1, 0, 1, 0, 0, 1, 0, 1, 1, 1, 1, 1};
		vertex_buffer_.create(sizeof(vertices), VK_BUFFER_USAGE_VERTEX_BUFFER_BIT);
		vertex_buffer_.upload(vertices.data(), sizeof(vertices));
		readback_.create(target_size * target_size * sizeof(Pixel),
						 VK_BUFFER_USAGE_TRANSFER_DST_BIT);
		std::array<Pixel, 16> texture_pixels{};
		for (uint32_t row = 0; row < 4; ++row) {
			for (uint32_t column = 0; column < 4; ++column) {
				texture_pixels[row * 4 + column] = {static_cast<uint8_t>(column * 85),
													static_cast<uint8_t>(row * 85), 0, 255};
			}
		}
		texture_upload_.create(sizeof(texture_pixels), VK_BUFFER_USAGE_TRANSFER_SRC_BIT);
		texture_upload_.upload(texture_pixels.data(), sizeof(texture_pixels));
		target_.create(target_size,
					   VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT);
		texture_.create(4, VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT);
		create_render_pass();
		create_descriptors();
		VkPushConstantRange push_range{VK_SHADER_STAGE_VERTEX_BIT, 0, 16};
		VkPipelineLayoutCreateInfo layout_info{VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO};
		layout_info.setLayoutCount = static_cast<uint32_t>(set_layouts_.size());
		layout_info.pSetLayouts = set_layouts_.data();
		layout_info.pushConstantRangeCount = 1;
		layout_info.pPushConstantRanges = &push_range;
		FLOWUI_CHECK(vkCreatePipelineLayout(context.device, &layout_info, nullptr,
											&pipeline_layout_) == VK_SUCCESS);
		solid_pipeline_ = create_pipeline(shader_directory, "solid");
		textured_pipeline_ = create_pipeline(shader_directory, "textured");
		VkCommandPoolCreateInfo pool_info{VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO};
		pool_info.queueFamilyIndex = context.graphicsQFamily;
		pool_info.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
		FLOWUI_CHECK(vkCreateCommandPool(context.device, &pool_info, nullptr, &command_pool_) ==
					 VK_SUCCESS);
		VkCommandBufferAllocateInfo allocate{VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO};
		allocate.commandPool = command_pool_;
		allocate.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
		allocate.commandBufferCount = 1;
		FLOWUI_CHECK(vkAllocateCommandBuffers(context.device, &allocate, &command_buffer_) ==
					 VK_SUCCESS);
		upload_texture();
	}

	~QuadFixture() {
		if (command_pool_)
			vkDestroyCommandPool(context_.device, command_pool_, nullptr);
		if (solid_pipeline_)
			vkDestroyPipeline(context_.device, solid_pipeline_, nullptr);
		if (textured_pipeline_)
			vkDestroyPipeline(context_.device, textured_pipeline_, nullptr);
		if (pipeline_layout_)
			vkDestroyPipelineLayout(context_.device, pipeline_layout_, nullptr);
		if (descriptor_pool_)
			vkDestroyDescriptorPool(context_.device, descriptor_pool_, nullptr);
		for (VkDescriptorSetLayout layout : set_layouts_) {
			if (layout)
				vkDestroyDescriptorSetLayout(context_.device, layout, nullptr);
		}
		if (sampler_)
			vkDestroySampler(context_.device, sampler_, nullptr);
		if (framebuffer_)
			vkDestroyFramebuffer(context_.device, framebuffer_, nullptr);
		if (render_pass_)
			vkDestroyRenderPass(context_.device, render_pass_, nullptr);
	}

	[[nodiscard]] std::span<const Pixel> render(const UiInstance& instance, bool textured = false,
												VkRect2D scissor = {{0, 0},
																	{target_size, target_size}}) {
		instance_buffer_.upload(&instance, sizeof(instance));
		begin_commands();
		VkClearValue clear{};
		VkRenderPassBeginInfo begin{VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO};
		begin.renderPass = render_pass_;
		begin.framebuffer = framebuffer_;
		begin.renderArea.extent = {target_size, target_size};
		begin.clearValueCount = 1;
		begin.pClearValues = &clear;
		vkCmdBeginRenderPass(command_buffer_, &begin, VK_SUBPASS_CONTENTS_INLINE);
		vkCmdBindPipeline(command_buffer_, VK_PIPELINE_BIND_POINT_GRAPHICS,
						  textured ? textured_pipeline_ : solid_pipeline_);
		vkCmdBindDescriptorSets(command_buffer_, VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline_layout_,
								0, static_cast<uint32_t>(descriptor_sets_.size()),
								descriptor_sets_.data(), 0, nullptr);
		VkDeviceSize offset = 0;
		vkCmdBindVertexBuffers(command_buffer_, 0, 1, &vertex_buffer_.buffer, &offset);
		vkCmdSetScissor(command_buffer_, 0, 1, &scissor);
		struct PushConstants {
			float width;
			float height;
			uint32_t first;
			uint32_t padding;
		};
		const PushConstants push{target_size, target_size, 0, 0};
		vkCmdPushConstants(command_buffer_, pipeline_layout_, VK_SHADER_STAGE_VERTEX_BIT, 0,
						   sizeof(push), &push);
		vkCmdDraw(command_buffer_, 4, 1, 0, 0);
		vkCmdEndRenderPass(command_buffer_);
		VkBufferImageCopy copy{};
		copy.imageSubresource = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 0, 1};
		copy.imageExtent = {target_size, target_size, 1};
		vkCmdCopyImageToBuffer(command_buffer_, target_.image, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
							   readback_.buffer, 1, &copy);
		VkBufferMemoryBarrier host_read{VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER};
		host_read.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
		host_read.dstAccessMask = VK_ACCESS_HOST_READ_BIT;
		host_read.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
		host_read.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
		host_read.buffer = readback_.buffer;
		host_read.size = VK_WHOLE_SIZE;
		vkCmdPipelineBarrier(command_buffer_, VK_PIPELINE_STAGE_TRANSFER_BIT,
							 VK_PIPELINE_STAGE_HOST_BIT, 0, 0, nullptr, 1, &host_read, 0, nullptr);
		submit_commands();
		FLOWUI_CHECK(vmaInvalidateAllocation(context_.allocator, readback_.allocation, 0,
											 VK_WHOLE_SIZE) == VK_SUCCESS);
		return {static_cast<const Pixel*>(readback_.mapped), target_size * target_size};
	}

private:
	void create_render_pass() {
		VkAttachmentDescription attachment{};
		attachment.format = target_format;
		attachment.samples = VK_SAMPLE_COUNT_1_BIT;
		attachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
		attachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
		attachment.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
		attachment.finalLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
		VkAttachmentReference reference{0, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL};
		VkSubpassDescription subpass{};
		subpass.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
		subpass.colorAttachmentCount = 1;
		subpass.pColorAttachments = &reference;
		VkSubpassDependency dependency{};
		dependency.srcSubpass = 0;
		dependency.dstSubpass = VK_SUBPASS_EXTERNAL;
		dependency.srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
		dependency.dstStageMask = VK_PIPELINE_STAGE_TRANSFER_BIT;
		dependency.srcAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
		dependency.dstAccessMask = VK_ACCESS_TRANSFER_READ_BIT;
		VkRenderPassCreateInfo info{VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO};
		info.attachmentCount = 1;
		info.pAttachments = &attachment;
		info.subpassCount = 1;
		info.pSubpasses = &subpass;
		info.dependencyCount = 1;
		info.pDependencies = &dependency;
		FLOWUI_CHECK(vkCreateRenderPass(context_.device, &info, nullptr, &render_pass_) ==
					 VK_SUCCESS);
		VkFramebufferCreateInfo framebuffer_info{VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO};
		framebuffer_info.renderPass = render_pass_;
		framebuffer_info.attachmentCount = 1;
		framebuffer_info.pAttachments = &target_.view;
		framebuffer_info.width = target_size;
		framebuffer_info.height = target_size;
		framebuffer_info.layers = 1;
		FLOWUI_CHECK(vkCreateFramebuffer(context_.device, &framebuffer_info, nullptr,
										 &framebuffer_) == VK_SUCCESS);
	}

	void create_descriptors() {
		VkDescriptorSetLayoutBinding storage_binding{0, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1,
													 VK_SHADER_STAGE_VERTEX_BIT, nullptr};
		VkDescriptorSetLayoutCreateInfo info{VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO};
		info.bindingCount = 1;
		info.pBindings = &storage_binding;
		FLOWUI_CHECK(vkCreateDescriptorSetLayout(context_.device, &info, nullptr,
												 &set_layouts_[0]) == VK_SUCCESS);
		VkDescriptorSetLayoutBinding texture_binding{1, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
													 256, VK_SHADER_STAGE_FRAGMENT_BIT, nullptr};
		info.pBindings = &texture_binding;
		FLOWUI_CHECK(vkCreateDescriptorSetLayout(context_.device, &info, nullptr,
												 &set_layouts_[1]) == VK_SUCCESS);
		std::array<VkDescriptorPoolSize, 2> pool_sizes{
			{{VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1},
			 {VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 256}}};
		VkDescriptorPoolCreateInfo pool_info{VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO};
		pool_info.maxSets = 2;
		pool_info.poolSizeCount = static_cast<uint32_t>(pool_sizes.size());
		pool_info.pPoolSizes = pool_sizes.data();
		FLOWUI_CHECK(vkCreateDescriptorPool(context_.device, &pool_info, nullptr,
											&descriptor_pool_) == VK_SUCCESS);
		VkDescriptorSetAllocateInfo allocate{VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO};
		allocate.descriptorPool = descriptor_pool_;
		allocate.descriptorSetCount = static_cast<uint32_t>(set_layouts_.size());
		allocate.pSetLayouts = set_layouts_.data();
		FLOWUI_CHECK(vkAllocateDescriptorSets(context_.device, &allocate,
											  descriptor_sets_.data()) == VK_SUCCESS);
		VkSamplerCreateInfo sampler_info{VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO};
		sampler_info.magFilter = VK_FILTER_LINEAR;
		sampler_info.minFilter = VK_FILTER_LINEAR;
		sampler_info.addressModeU = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
		sampler_info.addressModeV = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
		sampler_info.addressModeW = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
		FLOWUI_CHECK(vkCreateSampler(context_.device, &sampler_info, nullptr, &sampler_) ==
					 VK_SUCCESS);
		VkDescriptorBufferInfo buffer_info{instance_buffer_.buffer, 0, sizeof(UiInstance)};
		std::array<VkDescriptorImageInfo, 256> image_infos{};
		image_infos.fill({sampler_, texture_.view, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL});
		std::array<VkWriteDescriptorSet, 2> writes{};
		writes[0] = {VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET};
		writes[0].dstSet = descriptor_sets_[0];
		writes[0].dstBinding = 0;
		writes[0].descriptorCount = 1;
		writes[0].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
		writes[0].pBufferInfo = &buffer_info;
		writes[1] = {VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET};
		writes[1].dstSet = descriptor_sets_[1];
		writes[1].dstBinding = 1;
		writes[1].descriptorCount = static_cast<uint32_t>(image_infos.size());
		writes[1].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
		writes[1].pImageInfo = image_infos.data();
		vkUpdateDescriptorSets(context_.device, static_cast<uint32_t>(writes.size()), writes.data(),
							   0, nullptr);
	}

	[[nodiscard]] VkShaderModule load_shader(const std::filesystem::path& path) const {
		std::ifstream stream(path, std::ios::binary | std::ios::ate);
		FLOWUI_CHECK(stream.good());
		const size_t size = static_cast<size_t>(stream.tellg());
		FLOWUI_CHECK(size % sizeof(uint32_t) == 0);
		std::vector<uint32_t> code(size / sizeof(uint32_t));
		stream.seekg(0);
		stream.read(reinterpret_cast<char*>(code.data()), static_cast<std::streamsize>(size));
		FLOWUI_CHECK(stream.good());
		VkShaderModuleCreateInfo info{VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO};
		info.codeSize = size;
		info.pCode = code.data();
		VkShaderModule module{};
		FLOWUI_CHECK(vkCreateShaderModule(context_.device, &info, nullptr, &module) == VK_SUCCESS);
		return module;
	}

	[[nodiscard]] VkPipeline create_pipeline(const std::filesystem::path& directory,
											 std::string_view kind) const {
		const std::string prefix = "flowui_ui_" + std::string(kind);
		VkShaderModule vertex = load_shader(directory / (prefix + ".vert.spv"));
		VkShaderModule fragment = load_shader(directory / (prefix + ".frag.spv"));
		std::array<VkPipelineShaderStageCreateInfo, 2> stages{};
		stages[0] = {VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO};
		stages[0].stage = VK_SHADER_STAGE_VERTEX_BIT;
		stages[0].module = vertex;
		stages[0].pName = "main";
		stages[1] = {VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO};
		stages[1].stage = VK_SHADER_STAGE_FRAGMENT_BIT;
		stages[1].module = fragment;
		stages[1].pName = "main";
		VkVertexInputBindingDescription binding{0, 4 * sizeof(float), VK_VERTEX_INPUT_RATE_VERTEX};
		std::array<VkVertexInputAttributeDescription, 2> attributes{
			{{0, 0, VK_FORMAT_R32G32_SFLOAT, 0},
			 {1, 0, VK_FORMAT_R32G32_SFLOAT, 2 * sizeof(float)}}};
		VkPipelineVertexInputStateCreateInfo vertex_input{
			VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO};
		vertex_input.vertexBindingDescriptionCount = 1;
		vertex_input.pVertexBindingDescriptions = &binding;
		vertex_input.vertexAttributeDescriptionCount = static_cast<uint32_t>(attributes.size());
		vertex_input.pVertexAttributeDescriptions = attributes.data();
		VkPipelineInputAssemblyStateCreateInfo assembly{
			VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO};
		assembly.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_STRIP;
		VkViewport viewport{0, 0, target_size, target_size, 0, 1};
		VkPipelineViewportStateCreateInfo viewport_info{
			VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO};
		viewport_info.viewportCount = 1;
		viewport_info.pViewports = &viewport;
		viewport_info.scissorCount = 1;
		VkPipelineRasterizationStateCreateInfo raster{
			VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO};
		raster.polygonMode = VK_POLYGON_MODE_FILL;
		raster.cullMode = VK_CULL_MODE_NONE;
		raster.lineWidth = 1;
		VkPipelineMultisampleStateCreateInfo multisample{
			VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO};
		multisample.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;
		VkPipelineColorBlendAttachmentState attachment{};
		attachment.colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT |
									VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;
		VkPipelineColorBlendStateCreateInfo blend{
			VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO};
		blend.attachmentCount = 1;
		blend.pAttachments = &attachment;
		VkDynamicState dynamic_state = VK_DYNAMIC_STATE_SCISSOR;
		VkPipelineDynamicStateCreateInfo dynamic{
			VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO};
		dynamic.dynamicStateCount = 1;
		dynamic.pDynamicStates = &dynamic_state;
		VkGraphicsPipelineCreateInfo info{VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO};
		info.stageCount = static_cast<uint32_t>(stages.size());
		info.pStages = stages.data();
		info.pVertexInputState = &vertex_input;
		info.pInputAssemblyState = &assembly;
		info.pViewportState = &viewport_info;
		info.pRasterizationState = &raster;
		info.pMultisampleState = &multisample;
		info.pColorBlendState = &blend;
		info.pDynamicState = &dynamic;
		info.layout = pipeline_layout_;
		info.renderPass = render_pass_;
		VkPipeline pipeline{};
		const VkResult result = vkCreateGraphicsPipelines(context_.device, VK_NULL_HANDLE, 1, &info,
														  nullptr, &pipeline);
		vkDestroyShaderModule(context_.device, fragment, nullptr);
		vkDestroyShaderModule(context_.device, vertex, nullptr);
		FLOWUI_CHECK(result == VK_SUCCESS);
		return pipeline;
	}

	void begin_commands() {
		FLOWUI_CHECK(vkResetCommandBuffer(command_buffer_, 0) == VK_SUCCESS);
		VkCommandBufferBeginInfo begin{VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO};
		begin.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
		FLOWUI_CHECK(vkBeginCommandBuffer(command_buffer_, &begin) == VK_SUCCESS);
	}
	void submit_commands() {
		FLOWUI_CHECK(vkEndCommandBuffer(command_buffer_) == VK_SUCCESS);
		VkSubmitInfo submit{VK_STRUCTURE_TYPE_SUBMIT_INFO};
		submit.commandBufferCount = 1;
		submit.pCommandBuffers = &command_buffer_;
		FLOWUI_CHECK(vkQueueSubmit(context_.graphicsQ, 1, &submit, VK_NULL_HANDLE) == VK_SUCCESS);
		FLOWUI_CHECK(vkQueueWaitIdle(context_.graphicsQ) == VK_SUCCESS);
	}
	void upload_texture() {
		begin_commands();
		VkImageMemoryBarrier barrier{VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER};
		barrier.dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
		barrier.oldLayout = VK_IMAGE_LAYOUT_UNDEFINED;
		barrier.newLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
		barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
		barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
		barrier.image = texture_.image;
		barrier.subresourceRange = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1};
		vkCmdPipelineBarrier(command_buffer_, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,
							 VK_PIPELINE_STAGE_TRANSFER_BIT, 0, 0, nullptr, 0, nullptr, 1,
							 &barrier);
		VkBufferImageCopy copy{};
		copy.imageSubresource = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 0, 1};
		copy.imageExtent = {4, 4, 1};
		vkCmdCopyBufferToImage(command_buffer_, texture_upload_.buffer, texture_.image,
							   VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &copy);
		barrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
		barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
		barrier.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
		barrier.newLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
		vkCmdPipelineBarrier(command_buffer_, VK_PIPELINE_STAGE_TRANSFER_BIT,
							 VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT, 0, 0, nullptr, 0, nullptr, 1,
							 &barrier);
		submit_commands();
	}

	VulkanContext& context_;
	Buffer instance_buffer_;
	Buffer vertex_buffer_;
	Buffer readback_;
	Buffer texture_upload_;
	Image target_;
	Image texture_;
	std::array<VkDescriptorSetLayout, 2> set_layouts_{};
	std::array<VkDescriptorSet, 2> descriptor_sets_{};
	VkRenderPass render_pass_{};
	VkFramebuffer framebuffer_{};
	VkDescriptorPool descriptor_pool_{};
	VkSampler sampler_{};
	VkPipelineLayout pipeline_layout_{};
	VkPipeline solid_pipeline_{};
	VkPipeline textured_pipeline_{};
	VkCommandPool command_pool_{};
	VkCommandBuffer command_buffer_{};
};

[[nodiscard]] UiInstance rectangle() noexcept {
	UiInstance instance{};
	instance.x = 8.75f;
	instance.y = 8.75f;
	instance.w = 12.5f;
	instance.h = 12.5f;
	instance.colorRGBA = 0xFFFFFFFFu;
	return instance;
}

void check_alpha(std::span<const Pixel> pixels, uint32_t column, uint32_t row, int expected) {
	const int actual = pixels[row * target_size + column][3];
	if (std::abs(actual - expected) > 1) {
		throw FlowUi::test::CheckFailure(
			"alpha at (" + std::to_string(column) + ", " + std::to_string(row) + "): expected " +
			std::to_string(expected) + ", actual " + std::to_string(actual));
	}
}

void check_outer_feather(QuadFixture& fixture, bool textured, bool border, bool rounded) {
	UiInstance instance = rectangle();
	if (border) {
		instance.solidMode = 1;
		instance.borderL = instance.borderT = instance.borderR = instance.borderB = 2;
	}
	if (rounded)
		instance.r0 = instance.r1 = instance.r2 = instance.r3 = 3;
	const auto pixels = fixture.render(instance, textured);
	std::cout << "Exterior alpha (left, right, top, bottom): "
			  << static_cast<int>(pixels[15 * target_size + 8][3]) << ", "
			  << static_cast<int>(pixels[15 * target_size + 21][3]) << ", "
			  << static_cast<int>(pixels[8 * target_size + 15][3]) << ", "
			  << static_cast<int>(pixels[21 * target_size + 15][3]) << '\n';
	check_alpha(pixels, 8, 15, 64);
	check_alpha(pixels, 21, 15, 64);
	check_alpha(pixels, 15, 8, 64);
	check_alpha(pixels, 15, 21, 64);
	check_alpha(pixels, 7, 15, 0);
	check_alpha(pixels, 22, 15, 0);
	check_alpha(pixels, 15, 7, 0);
	check_alpha(pixels, 15, 22, 0);
	check_alpha(pixels, 9, 15, 255);
	check_alpha(pixels, 15, 15, border ? 0 : 255);
	if (rounded)
		check_alpha(pixels, 8, 8, 0);
}

void check_texture_mapping(QuadFixture& fixture, bool flipped) {
	UiInstance instance = rectangle();
	instance.w = instance.h = 12;
	if (flipped) {
		instance.uv0x = instance.uv0y = 1;
		instance.uv1x = instance.uv1y = 0;
	}
	const auto pixels = fixture.render(instance, true);
	for (uint32_t row = 10; row <= 18; row += 4) {
		for (uint32_t column = 10; column <= 18; column += 4) {
			float horizontal = (static_cast<float>(column) + 0.5f - instance.x) / instance.w;
			float vertical = (static_cast<float>(row) + 0.5f - instance.y) / instance.h;
			if (flipped) {
				horizontal = 1 - horizontal;
				vertical = 1 - vertical;
			}
			const int expected_red =
				static_cast<int>(std::lround(std::clamp(horizontal * 4 - 0.5f, 0.0f, 3.0f) * 85));
			const int expected_green =
				static_cast<int>(std::lround(std::clamp(vertical * 4 - 0.5f, 0.0f, 3.0f) * 85));
			FLOWUI_CHECK(std::abs(static_cast<int>(pixels[row * target_size + column][0]) -
								  expected_red) <= 1);
			FLOWUI_CHECK(std::abs(static_cast<int>(pixels[row * target_size + column][1]) -
								  expected_green) <= 1);
		}
	}
}

void check_cropped_texture(QuadFixture& fixture) {
	UiInstance instance = rectangle();
	instance.w = instance.h = 4.5f;
	instance.uv0x = instance.uv0y = 0.375f;
	instance.uv1x = instance.uv1y = 0.625f;
	const auto pixels = fixture.render(instance, true);
	check_alpha(pixels, 8, 10, 64);
	check_alpha(pixels, 13, 10, 64);
	FLOWUI_CHECK(pixels[10 * target_size + 8][0] == 85);
	FLOWUI_CHECK(pixels[10 * target_size + 13][0] == 170);
	FLOWUI_CHECK(pixels[8 * target_size + 10][1] == 85);
	FLOWUI_CHECK(pixels[13 * target_size + 10][1] == 170);
}

} // namespace

int main(int argument_count, char** arguments) {
	try {
		FlowUi::test::HeadlessVulkanFixture device;
		ShaderDevice shader_device(device.context());
		std::cout << "Vulkan device: " << device.deviceName() << '\n';
		const std::filesystem::path shader_directory =
			argument_count > 1 ? arguments[1] : FLOWUI_ANTIALIASING_SHADER_DIRECTORY;
		QuadFixture fixture(shader_device.context, shader_directory);
		FlowUi::test::Runner runner;
		runner.run("solid outer feather on all four edges",
				   [&] { check_outer_feather(fixture, false, false, false); });
		runner.run("border outer feather and hollow interior",
				   [&] { check_outer_feather(fixture, false, true, false); });
		runner.run("rounded solid preserves feather and shape",
				   [&] { check_outer_feather(fixture, false, false, true); });
		runner.run("rounded border preserves feather and shape",
				   [&] { check_outer_feather(fixture, false, true, true); });
		runner.run("textured outer feather on all four edges",
				   [&] { check_outer_feather(fixture, true, false, false); });
		runner.run("rounded textured outer feather",
				   [&] { check_outer_feather(fixture, true, false, true); });
		runner.run("texture coordinates retain original mapping",
				   [&] { check_texture_mapping(fixture, false); });
		runner.run("flipped texture coordinates retain mapping",
				   [&] { check_texture_mapping(fixture, true); });
		runner.run("feather samples cropped texture edge", [&] { check_cropped_texture(fixture); });
		runner.run("explicit scissor still clips feather", [&] {
			const auto pixels = fixture.render(rectangle(), false, {{9, 9}, {12, 12}});
			check_alpha(pixels, 8, 15, 0);
			check_alpha(pixels, 21, 15, 0);
			check_alpha(pixels, 9, 15, 255);
		});
		runner.run("degenerate quads remain invisible", [&] {
			for (bool zero_width : {false, true}) {
				UiInstance instance = rectangle();
				if (zero_width)
					instance.w = 0;
				else
					instance.h = 0;
				const auto pixels = fixture.render(instance);
				FLOWUI_CHECK(std::all_of(pixels.begin(), pixels.end(),
										 [](const Pixel& pixel) { return pixel[3] == 0; }));
			}
		});
		return runner.finish();
	} catch (const FlowUi::test::VulkanUnavailable& error) {
		std::cerr << "Vulkan unavailable: " << error.what() << '\n';
#if FLOWUI_TEST_REQUIRE_VULKAN_DEVICE
		return 1;
#else
		return 77;
#endif
	} catch (const std::exception& error) {
		std::cerr << "[FAIL] antialiasing fixture: " << error.what() << '\n';
		return 1;
	}
}
