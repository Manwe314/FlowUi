#pragma once

#include "FlowUi/Private/renderer/vulkan/Vk_Context.hpp"
#include "FlowUi/Private/storageSystem/vulkan/Vma.hpp"
#include "TestHarness.hpp"

#include <array>
#include <cstring>
#include <fstream>
#include <vector>

namespace FlowUi::test {

/** Sample the real uploaded icon view and straight-alpha blend after sRGB decoding. */
class IconGpuSampler {
public:
	explicit IconGpuSampler(VulkanContext& context) : context_(context) {
		try {
			VkBufferCreateInfo bufferInfo{VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO};
			bufferInfo.size = sizeof(std::array<float, 8>);
			bufferInfo.usage = VK_BUFFER_USAGE_STORAGE_BUFFER_BIT;
			VmaAllocationCreateInfo allocationInfo{};
			allocationInfo.usage = VMA_MEMORY_USAGE_AUTO;
			allocationInfo.flags =
				VMA_ALLOCATION_CREATE_HOST_ACCESS_RANDOM_BIT | VMA_ALLOCATION_CREATE_MAPPED_BIT;
			VmaAllocationInfo allocationDetails{};
			FLOWUI_CHECK(vmaCreateBuffer(context_.allocator, &bufferInfo, &allocationInfo, &buffer_,
										 &allocation_, &allocationDetails) == VK_SUCCESS);
			mapped_ = allocationDetails.pMappedData;
			const std::array bindings{
				VkDescriptorSetLayoutBinding{0, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1,
											 VK_SHADER_STAGE_COMPUTE_BIT, nullptr},
				VkDescriptorSetLayoutBinding{1, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1,
											 VK_SHADER_STAGE_COMPUTE_BIT, nullptr}};
			VkDescriptorSetLayoutCreateInfo descriptorLayout{
				VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO};
			descriptorLayout.bindingCount = static_cast<uint32_t>(bindings.size());
			descriptorLayout.pBindings = bindings.data();
			FLOWUI_CHECK(vkCreateDescriptorSetLayout(context_.device, &descriptorLayout, nullptr,
													 &setLayout_) == VK_SUCCESS);
			const std::array sizes{
				VkDescriptorPoolSize{VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1},
				VkDescriptorPoolSize{VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1}};
			VkDescriptorPoolCreateInfo poolInfo{VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO};
			poolInfo.maxSets = 1;
			poolInfo.poolSizeCount = static_cast<uint32_t>(sizes.size());
			poolInfo.pPoolSizes = sizes.data();
			FLOWUI_CHECK(vkCreateDescriptorPool(context_.device, &poolInfo, nullptr,
												&descriptorPool_) == VK_SUCCESS);
			VkDescriptorSetAllocateInfo descriptorSets{
				VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO};
			descriptorSets.descriptorPool = descriptorPool_;
			descriptorSets.descriptorSetCount = 1;
			descriptorSets.pSetLayouts = &setLayout_;
			FLOWUI_CHECK(vkAllocateDescriptorSets(context_.device, &descriptorSets,
												  &descriptorSet_) == VK_SUCCESS);
			const VkPushConstantRange coordinates{VK_SHADER_STAGE_COMPUTE_BIT, 0,
												  sizeof(std::array<uint32_t, 2>)};
			VkPipelineLayoutCreateInfo layoutInfo{VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO};
			layoutInfo.setLayoutCount = 1;
			layoutInfo.pSetLayouts = &setLayout_;
			layoutInfo.pushConstantRangeCount = 1;
			layoutInfo.pPushConstantRanges = &coordinates;
			FLOWUI_CHECK(vkCreatePipelineLayout(context_.device, &layoutInfo, nullptr,
												&pipelineLayout_) == VK_SUCCESS);
			std::ifstream input(FLOWUI_ICON_TEST_COMPUTE_SPV, std::ios::binary | std::ios::ate);
			FLOWUI_CHECK(input.good());
			const auto byteCount = input.tellg();
			FLOWUI_CHECK(byteCount > 0 && byteCount % sizeof(uint32_t) == 0);
			std::vector<uint32_t> code(static_cast<size_t>(byteCount) / sizeof(uint32_t));
			input.seekg(0);
			input.read(reinterpret_cast<char*>(code.data()), byteCount);
			FLOWUI_CHECK(input.good());
			VkShaderModuleCreateInfo shaderInfo{VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO};
			shaderInfo.codeSize = static_cast<size_t>(byteCount);
			shaderInfo.pCode = code.data();
			FLOWUI_CHECK(vkCreateShaderModule(context_.device, &shaderInfo, nullptr,
											  &shaderModule_) == VK_SUCCESS);
			VkComputePipelineCreateInfo pipelineInfo{
				VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO};
			pipelineInfo.stage = {VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO};
			pipelineInfo.stage.stage = VK_SHADER_STAGE_COMPUTE_BIT;
			pipelineInfo.stage.module = shaderModule_;
			pipelineInfo.stage.pName = "main";
			pipelineInfo.layout = pipelineLayout_;
			FLOWUI_CHECK(vkCreateComputePipelines(context_.device, VK_NULL_HANDLE, 1, &pipelineInfo,
												  nullptr, &pipeline_) == VK_SUCCESS);
			VkCommandPoolCreateInfo commandPoolInfo{VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO};
			commandPoolInfo.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
			commandPoolInfo.queueFamilyIndex = context_.graphicsQFamily;
			FLOWUI_CHECK(vkCreateCommandPool(context_.device, &commandPoolInfo, nullptr,
											 &commandPool_) == VK_SUCCESS);
			VkCommandBufferAllocateInfo commands{VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO};
			commands.commandPool = commandPool_;
			commands.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
			commands.commandBufferCount = 1;
			FLOWUI_CHECK(vkAllocateCommandBuffers(context_.device, &commands, &commandBuffer_) ==
						 VK_SUCCESS);
			VkFenceCreateInfo fenceInfo{VK_STRUCTURE_TYPE_FENCE_CREATE_INFO};
			FLOWUI_CHECK(vkCreateFence(context_.device, &fenceInfo, nullptr, &fence_) ==
						 VK_SUCCESS);
		} catch (...) {
			reset();
			throw;
		}
	}
	~IconGpuSampler() { reset(); }
	IconGpuSampler(const IconGpuSampler&) = delete;
	IconGpuSampler& operator=(const IconGpuSampler&) = delete;

	[[nodiscard]] std::array<float, 8> sample(VkImageView view, VkSampler sampler, uint32_t pixelX,
											  uint32_t pixelY) {
		const VkDescriptorImageInfo imageInfo{sampler, view,
											  VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL};
		const VkDescriptorBufferInfo bufferInfo{buffer_, 0, sizeof(std::array<float, 8>)};
		std::array<VkWriteDescriptorSet, 2> writes{};
		for (uint32_t index = 0; index < writes.size(); ++index) {
			writes[index] = {VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET};
			writes[index].dstSet = descriptorSet_;
			writes[index].dstBinding = index;
			writes[index].descriptorCount = 1;
			writes[index].descriptorType = index == 0 ? VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER
													  : VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
		}
		writes[0].pImageInfo = &imageInfo;
		writes[1].pBufferInfo = &bufferInfo;
		vkUpdateDescriptorSets(context_.device, static_cast<uint32_t>(writes.size()), writes.data(),
							   0, nullptr);
		FLOWUI_CHECK(vkResetCommandBuffer(commandBuffer_, 0) == VK_SUCCESS);
		VkCommandBufferBeginInfo beginInfo{VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO};
		beginInfo.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
		FLOWUI_CHECK(vkBeginCommandBuffer(commandBuffer_, &beginInfo) == VK_SUCCESS);
		VkMemoryBarrier reads{VK_STRUCTURE_TYPE_MEMORY_BARRIER};
		reads.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
		reads.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
		vkCmdPipelineBarrier(commandBuffer_, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT,
							 VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, 0, 1, &reads, 0, nullptr, 0,
							 nullptr);
		vkCmdBindPipeline(commandBuffer_, VK_PIPELINE_BIND_POINT_COMPUTE, pipeline_);
		vkCmdBindDescriptorSets(commandBuffer_, VK_PIPELINE_BIND_POINT_COMPUTE, pipelineLayout_, 0,
								1, &descriptorSet_, 0, nullptr);
		const std::array coordinates{pixelX, pixelY};
		vkCmdPushConstants(commandBuffer_, pipelineLayout_, VK_SHADER_STAGE_COMPUTE_BIT, 0,
						   sizeof(coordinates), coordinates.data());
		vkCmdDispatch(commandBuffer_, 1, 1, 1);
		VkMemoryBarrier host{VK_STRUCTURE_TYPE_MEMORY_BARRIER};
		host.srcAccessMask = VK_ACCESS_SHADER_WRITE_BIT;
		host.dstAccessMask = VK_ACCESS_HOST_READ_BIT;
		vkCmdPipelineBarrier(commandBuffer_, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
							 VK_PIPELINE_STAGE_HOST_BIT, 0, 1, &host, 0, nullptr, 0, nullptr);
		FLOWUI_CHECK(vkEndCommandBuffer(commandBuffer_) == VK_SUCCESS);
		FLOWUI_CHECK(vkResetFences(context_.device, 1, &fence_) == VK_SUCCESS);
		VkSubmitInfo submit{VK_STRUCTURE_TYPE_SUBMIT_INFO};
		submit.commandBufferCount = 1;
		submit.pCommandBuffers = &commandBuffer_;
		FLOWUI_CHECK(vkQueueSubmit(context_.graphicsQ, 1, &submit, fence_) == VK_SUCCESS);
		FLOWUI_CHECK(vkWaitForFences(context_.device, 1, &fence_, VK_TRUE, 5'000'000'000) ==
					 VK_SUCCESS);
		FLOWUI_CHECK(vmaInvalidateAllocation(context_.allocator, allocation_, 0, VK_WHOLE_SIZE) ==
					 VK_SUCCESS);
		std::array<float, 8> samples{};
		std::memcpy(samples.data(), mapped_, sizeof(samples));
		return samples;
	}

private:
	void reset() noexcept {
		if (fence_)
			vkDestroyFence(context_.device, fence_, nullptr);
		if (commandPool_)
			vkDestroyCommandPool(context_.device, commandPool_, nullptr);
		if (pipeline_)
			vkDestroyPipeline(context_.device, pipeline_, nullptr);
		if (pipelineLayout_)
			vkDestroyPipelineLayout(context_.device, pipelineLayout_, nullptr);
		if (shaderModule_)
			vkDestroyShaderModule(context_.device, shaderModule_, nullptr);
		if (descriptorPool_)
			vkDestroyDescriptorPool(context_.device, descriptorPool_, nullptr);
		if (setLayout_)
			vkDestroyDescriptorSetLayout(context_.device, setLayout_, nullptr);
		if (buffer_)
			vmaDestroyBuffer(context_.allocator, buffer_, allocation_);
	}
	VulkanContext& context_;
	VkDescriptorSetLayout setLayout_{};
	VkDescriptorPool descriptorPool_{};
	VkDescriptorSet descriptorSet_{};
	VkPipelineLayout pipelineLayout_{};
	VkShaderModule shaderModule_{};
	VkPipeline pipeline_{};
	VkCommandPool commandPool_{};
	VkCommandBuffer commandBuffer_{};
	VkFence fence_{};
	VkBuffer buffer_{};
	VmaAllocation allocation_{};
	void* mapped_{};
};

} // namespace FlowUi::test
