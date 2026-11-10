#include "HeadlessVulkanFixture.hpp"
#include "TestHarness.hpp"
#include "devSystems/devAgenticWorkflow/frameCapture/AgenticFrameCapture.hpp"
#include "FlowUi/Private/storageSystem/vulkan/Vma.hpp"

#include <algorithm>
#include <array>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <string>
#include <vector>

namespace {

namespace capture = FlowUi::devSystems::agentic::frame_capture;
using FlowUi::devSystems::agentic::AgenticCompletedImage;

struct CompletedImages {
	std::vector<AgenticCompletedImage> images;
};

FlowUi::Status accept_image(void* owner, AgenticCompletedImage&& image) noexcept {
	try {
		static_cast<CompletedImages*>(owner)->images.emplace_back(std::move(image));
		return {};
	} catch (...) {
		return FlowUi::unexpectedError(FlowUi::makeError(FlowUi::ErrorCode::AllocationFailed,
														 FlowUi::ErrorSite::AgenticImage));
	}
}

struct RenderTarget {
	VulkanContext& context;
	VkImage image{};
	VmaAllocation allocation{};
	VkImageView view{};
	VkRenderPass render_pass{};
	VkFramebuffer framebuffer{};
	VkCommandPool pool{};
	VkFence fence{};
	~RenderTarget() {
		if (fence)
			vkDestroyFence(context.device, fence, nullptr);
		if (pool)
			vkDestroyCommandPool(context.device, pool, nullptr);
		if (framebuffer)
			vkDestroyFramebuffer(context.device, framebuffer, nullptr);
		if (render_pass)
			vkDestroyRenderPass(context.device, render_pass, nullptr);
		if (view)
			vkDestroyImageView(context.device, view, nullptr);
		if (image)
			vmaDestroyImage(context.allocator, image, allocation);
	}
};

void render(VulkanContext& context, VkFormat format, VkExtent2D extent, uint64_t window,
			uint32_t slot, uint64_t window_frame, uint64_t submission_serial,
			CompletedImages& completed, const std::array<float, 4>& color, bool submit = true) {
	capture::AgenticFrameCaptureRequest request;
	request.stamp.run_id = "run_exact_identity";
	request.stamp.capture_id = "capture_exact_identity";
	request.stamp.app_tick = window_frame + 1000u;
	request.stamp.window_id = static_cast<uint32_t>(window);
	request.stamp.window_frame = window_frame;
	request.stamp.frame_slot = slot;
	request.stamp.tree_generation = window_frame + 23u;
	request.stamp.tree_available = true;
	request.completed_image = &accept_image;
	request.owner = &completed;
	FLOWUI_CHECK(capture::begin_frame(context, window, slot, request));
	RenderTarget target{context};
	VkImageCreateInfo image_info{VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO};
	image_info.imageType = VK_IMAGE_TYPE_2D;
	image_info.format = format;
	image_info.extent = {extent.width, extent.height, 1u};
	image_info.mipLevels = 1u;
	image_info.arrayLayers = 1u;
	image_info.samples = VK_SAMPLE_COUNT_1_BIT;
	image_info.usage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT |
					   VK_IMAGE_USAGE_SAMPLED_BIT;
	VmaAllocationCreateInfo allocation_info{};
	allocation_info.usage = VMA_MEMORY_USAGE_AUTO_PREFER_DEVICE;
	FLOWUI_CHECK(vmaCreateImage(context.allocator, &image_info, &allocation_info, &target.image,
								&target.allocation, nullptr) == VK_SUCCESS);
	VkImageViewCreateInfo view_info{VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO};
	view_info.image = target.image;
	view_info.viewType = VK_IMAGE_VIEW_TYPE_2D;
	view_info.format = format;
	view_info.subresourceRange = {VK_IMAGE_ASPECT_COLOR_BIT, 0u, 1u, 0u, 1u};
	FLOWUI_CHECK(vkCreateImageView(context.device, &view_info, nullptr, &target.view) ==
				 VK_SUCCESS);
	VkAttachmentDescription attachment{};
	attachment.format = format;
	attachment.samples = VK_SAMPLE_COUNT_1_BIT;
	attachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
	attachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
	attachment.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
	attachment.finalLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
	VkAttachmentReference reference{0u, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL};
	VkSubpassDescription subpass{};
	subpass.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
	subpass.colorAttachmentCount = 1u;
	subpass.pColorAttachments = &reference;
	VkRenderPassCreateInfo pass_info{VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO};
	pass_info.attachmentCount = 1u;
	pass_info.pAttachments = &attachment;
	pass_info.subpassCount = 1u;
	pass_info.pSubpasses = &subpass;
	FLOWUI_CHECK(vkCreateRenderPass(context.device, &pass_info, nullptr, &target.render_pass) ==
				 VK_SUCCESS);
	VkFramebufferCreateInfo framebuffer_info{VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO};
	framebuffer_info.renderPass = target.render_pass;
	framebuffer_info.attachmentCount = 1u;
	framebuffer_info.pAttachments = &target.view;
	framebuffer_info.width = extent.width;
	framebuffer_info.height = extent.height;
	framebuffer_info.layers = 1u;
	FLOWUI_CHECK(vkCreateFramebuffer(context.device, &framebuffer_info, nullptr,
									 &target.framebuffer) == VK_SUCCESS);
	VkCommandPoolCreateInfo pool_info{VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO};
	pool_info.queueFamilyIndex = context.graphicsQFamily;
	FLOWUI_CHECK(vkCreateCommandPool(context.device, &pool_info, nullptr, &target.pool) ==
				 VK_SUCCESS);
	VkCommandBufferAllocateInfo allocate{VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO};
	allocate.commandPool = target.pool;
	allocate.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
	allocate.commandBufferCount = 1u;
	VkCommandBuffer command_buffer{};
	FLOWUI_CHECK(vkAllocateCommandBuffers(context.device, &allocate, &command_buffer) ==
				 VK_SUCCESS);
	VkCommandBufferBeginInfo begin{VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO};
	FLOWUI_CHECK(vkBeginCommandBuffer(command_buffer, &begin) == VK_SUCCESS);
	VkClearValue clear{};
	std::copy(color.begin(), color.end(), clear.color.float32);
	VkRenderPassBeginInfo render_begin{VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO};
	render_begin.renderPass = target.render_pass;
	render_begin.framebuffer = target.framebuffer;
	render_begin.renderArea.extent = extent;
	render_begin.clearValueCount = 1u;
	render_begin.pClearValues = &clear;
	vkCmdBeginRenderPass(command_buffer, &render_begin, VK_SUBPASS_CONTENTS_INLINE);
	vkCmdEndRenderPass(command_buffer);
	const FlowUi::devSystems::agentic::AgenticImageProducerStamp producer{
		.source_image_revision = window_frame + 200u,
		.app_tick = window_frame + 1000u,
		.window_frame = window_frame,
		.available = true,
		.current_submission = true,
	};
	FLOWUI_CHECK(capture::record(context, command_buffer, target.image, extent, format,
								 VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, window, slot, "viewport",
								 "quoted\"key\nline", VK_COLOR_SPACE_MAX_ENUM_KHR, producer));
	FLOWUI_CHECK(vkEndCommandBuffer(command_buffer) == VK_SUCCESS);
	if (submit) {
		VkFenceCreateInfo fence_info{VK_STRUCTURE_TYPE_FENCE_CREATE_INFO};
		FLOWUI_CHECK(vkCreateFence(context.device, &fence_info, nullptr, &target.fence) ==
					 VK_SUCCESS);
		VkSubmitInfo submission{VK_STRUCTURE_TYPE_SUBMIT_INFO};
		submission.commandBufferCount = 1u;
		submission.pCommandBuffers = &command_buffer;
		FLOWUI_CHECK(vkQueueSubmit(context.graphicsQ, 1u, &submission, target.fence) == VK_SUCCESS);
		capture::mark_submitted(context, window, slot, submission_serial, 3u, 9u);
		FLOWUI_CHECK(vkWaitForFences(context.device, 1u, &target.fence, VK_TRUE, UINT64_MAX) ==
					 VK_SUCCESS);
	}
	// The image can be destroyed now: capture owns a separate, completed readback buffer.
}

void verify_image(const AgenticCompletedImage& image, uint32_t width, uint32_t height,
				  std::array<unsigned char, 3> expected, uint64_t window_frame,
				  uint64_t submission_serial) {
	FLOWUI_CHECK(image.width == width && image.height == height);
	FLOWUI_CHECK(image.rgb.size() == static_cast<size_t>(width) * height * 3u);
	FLOWUI_CHECK(image.run_id == "run_exact_identity");
	FLOWUI_CHECK(image.capture_id == "capture_exact_identity");
	FLOWUI_CHECK(image.window_frame == window_frame);
	FLOWUI_CHECK(image.app_tick == window_frame + 1000u);
	FLOWUI_CHECK(image.tree_generation == window_frame + 23u && image.tree_available);
	FLOWUI_CHECK(image.submission_serial == submission_serial && image.submission_available);
	FLOWUI_CHECK(image.device_id == 3u && image.queue_id == 9u);
	FLOWUI_CHECK(image.producer_available && image.producer_window_frame == window_frame);
	FLOWUI_CHECK(image.producer_submission_serial == submission_serial);
	FLOWUI_CHECK(image.source_image_revision == window_frame + 200u);
	FLOWUI_CHECK(image.viewport_key == "quoted\"key\nline");
	FLOWUI_CHECK(image.completed_monotonic_ns >= image.recorded_monotonic_ns);
	for (size_t offset = 0; offset < image.rgb.size(); ++offset)
		FLOWUI_CHECK(image.rgb[offset] == expected[offset % 3u]);
}

void test_capture(VulkanContext& context) {
	FLOWUI_CHECK(!capture::requested(context));
	FLOWUI_CHECK(capture::configure_resources(context, true));
	FLOWUI_CHECK(capture::requested(context));
	CompletedImages completed;
	completed.images.reserve(8u);
	render(context, VK_FORMAT_R8G8B8A8_UNORM, {4u, 3u}, 11u, 0u, 18u, 40u, completed,
		   {1.0f, 0.0f, 0.0f, 1.0f});
	FLOWUI_CHECK(completed.images.empty());
	FLOWUI_CHECK(capture::pending_bytes(context) == 4u * 3u * 4u);
	render(context, VK_FORMAT_B8G8R8A8_UNORM, {2u, 5u}, 11u, 1u, 20u, 42u, completed,
		   {0.0f, 1.0f, 0.0f, 1.0f});
	capture::complete_slot(context, 11u, 0u);
	FLOWUI_CHECK(completed.images.size() == 1u);
	verify_image(completed.images[0], 4u, 3u, {255u, 0u, 0u}, 18u, 40u);
	// A canceled frame and submission gap must never be inferred from the reusable slot.
	render(context, VK_FORMAT_R8G8B8A8_UNORM, {1u, 1u}, 11u, 0u, 21u, 43u, completed,
		   {1.0f, 0.0f, 0.0f, 1.0f}, false);
	capture::cancel_frame(context, 11u, 0u);
	capture::complete_slot(context, 11u, 0u);
	FLOWUI_CHECK(completed.images.size() == 1u);
	render(context, VK_FORMAT_R16G16B16A16_SFLOAT, {7u, 2u}, 11u, 0u, 25u, 48u, completed,
		   {0.0f, 0.5f, 2.0f, 1.0f});
	render(context, VK_FORMAT_R8G8B8A8_UNORM, {3u, 1u}, 22u, 0u, 1u, 49u, completed,
		   {0.0f, 0.0f, 1.0f, 1.0f});
	capture::release_window(context, 11u);
	FLOWUI_CHECK(completed.images.size() == 3u);
	verify_image(completed.images[1], 7u, 2u, {0u, 128u, 255u}, 25u, 48u);
	FLOWUI_CHECK(completed.images[1].encoding == "linear-clamped-rgb8");
	verify_image(completed.images[2], 2u, 5u, {0u, 255u, 0u}, 20u, 42u);
	capture::complete_slot(context, 22u, 0u);
	FLOWUI_CHECK(completed.images.size() == 4u);
	verify_image(completed.images[3], 3u, 1u, {0u, 0u, 255u}, 1u, 49u);
	FLOWUI_CHECK(completed.images[3].window_id == 22u);
	FLOWUI_CHECK(capture::pending_bytes(context) == 0u);
	FLOWUI_CHECK(capture::failure_count(context) == 0u);
	capture::release_device(context);
	FLOWUI_CHECK(!capture::requested(context));
	FLOWUI_CHECK(capture::configure_resources(context, true));
	render(context, VK_FORMAT_R8G8B8A8_UNORM, {1u, 1u}, 22u, 0u, 9u, 50u, completed,
		   {0.0f, 0.0f, 1.0f, 1.0f});
	FLOWUI_CHECK(capture::pending_bytes(context) != 0u);
	capture::discard_device(context);
	FLOWUI_CHECK(completed.images.size() == 4u);
	FLOWUI_CHECK(!capture::requested(context) && capture::pending_bytes(context) == 0u);
}

} // namespace

int main() {
	try {
		FlowUi::test::HeadlessVulkanFixture vulkan;
		FlowUi::test::Runner runner;
		runner.run("agent-neutral capture, frame ownership, resize, cancellation and cleanup",
				   [&] { test_capture(vulkan.context()); });
		capture::release_device(vulkan.context());
		return runner.finish();
	} catch (const FlowUi::test::VulkanUnavailable& error) {
		std::cout << "SKIP: " << error.what() << '\n';
		return 77;
	}
}
