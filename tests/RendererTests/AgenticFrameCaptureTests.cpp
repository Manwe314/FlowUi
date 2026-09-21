#include "HeadlessVulkanFixture.hpp"
#include "TestHarness.hpp"
#include "internal/AgenticDebug/FrameCapture.hpp"
#include "internal/Vma.hpp"

#include <algorithm>
#include <array>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <string>
#include <vector>

namespace {

namespace capture = FlowUi::agentic_debug;

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
			uint32_t slot, const std::array<float, 4>& color, bool submit = true) {
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
	capture::record(context, command_buffer, target.image, extent, format,
					VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, window, slot, "viewport",
					"quoted\"key\nline");
	FLOWUI_CHECK(vkEndCommandBuffer(command_buffer) == VK_SUCCESS);
	if (submit) {
		VkFenceCreateInfo fence_info{VK_STRUCTURE_TYPE_FENCE_CREATE_INFO};
		FLOWUI_CHECK(vkCreateFence(context.device, &fence_info, nullptr, &target.fence) ==
					 VK_SUCCESS);
		VkSubmitInfo submission{VK_STRUCTURE_TYPE_SUBMIT_INFO};
		submission.commandBufferCount = 1u;
		submission.pCommandBuffers = &command_buffer;
		FLOWUI_CHECK(vkQueueSubmit(context.graphicsQ, 1u, &submission, target.fence) == VK_SUCCESS);
		capture::mark_submitted(context, window, slot);
		FLOWUI_CHECK(vkWaitForFences(context.device, 1u, &target.fence, VK_TRUE, UINT64_MAX) ==
					 VK_SUCCESS);
	}
	// The image can be destroyed now: capture owns a separate, completed readback buffer.
}

void verify_image(const std::filesystem::path& path, uint32_t width, uint32_t height,
				  std::array<unsigned char, 3> expected) {
	std::ifstream input(path, std::ios::binary);
	std::string magic;
	uint32_t actual_width{}, actual_height{}, maximum{};
	input >> magic >> actual_width >> actual_height >> maximum;
	FLOWUI_CHECK(input && magic == "P6" && actual_width == width && actual_height == height &&
				 maximum == 255u);
	input.get();
	std::vector<unsigned char> pixels{std::istreambuf_iterator<char>(input),
									  std::istreambuf_iterator<char>()};
	FLOWUI_CHECK(pixels.size() == static_cast<size_t>(width) * height * 3u);
	for (size_t offset = 0u; offset < pixels.size(); ++offset)
		FLOWUI_CHECK(pixels[offset] == expected[offset % 3u]);
}

void test_capture(VulkanContext& context) {
	FLOWUI_CHECK(capture::requested());
	const std::filesystem::path root(std::getenv("FLOWUI_AGENTIC_DEBUG_DIR"));
	std::vector<std::filesystem::path> previous;
	if (std::filesystem::exists(root)) {
		for (const auto& entry : std::filesystem::directory_iterator(root))
			previous.emplace_back(entry.path());
	}
	render(context, VK_FORMAT_R8G8B8A8_UNORM, {4u, 3u}, 11u, 0u, {1.0f, 0.0f, 0.0f, 1.0f});
	std::filesystem::path session;
	for (const auto& entry : std::filesystem::directory_iterator(root)) {
		if (std::find(previous.begin(), previous.end(), entry.path()) == previous.end())
			session = entry.path();
	}
	FLOWUI_CHECK(!session.empty());
	FLOWUI_CHECK(!std::filesystem::exists(session / "frames.jsonl"));
	render(context, VK_FORMAT_B8G8R8A8_UNORM, {2u, 5u}, 11u, 1u, {0.0f, 1.0f, 0.0f, 1.0f});
	capture::complete_slot(context, 11u, 0u);
	verify_image(session / "frame-0.ppm", 4u, 3u, {255u, 0u, 0u});
	FLOWUI_CHECK(!std::filesystem::exists(session / "frame-1.ppm"));
	// Reuse slot zero with a resized target while slot one's capture remains pending.
	render(context, VK_FORMAT_R16G16B16A16_SFLOAT, {7u, 2u}, 11u, 0u, {0.0f, 0.5f, 2.0f, 1.0f});
	render(context, VK_FORMAT_R8G8B8A8_UNORM, {3u, 1u}, 22u, 0u, {0.0f, 0.0f, 1.0f, 1.0f});
	capture::release_window(context, 11u);
	verify_image(session / "frame-1.ppm", 2u, 5u, {0u, 255u, 0u});
	verify_image(session / "frame-2.ppm", 7u, 2u, {0u, 128u, 255u});
	FLOWUI_CHECK(!std::filesystem::exists(session / "frame-3.ppm"));
	capture::complete_slot(context, 22u, 0u);
	verify_image(session / "frame-3.ppm", 3u, 1u, {0u, 0u, 255u});
	render(context, VK_FORMAT_R8G8B8A8_UNORM, {1u, 1u}, 22u, 0u, {1.0f, 0.0f, 0.0f, 1.0f}, false);
	capture::complete_slot(context, 22u, 0u);
	FLOWUI_CHECK(!std::filesystem::exists(session / "frame-4.ppm"));
	capture::release_device(context);
	std::ifstream manifest(session / "frames.jsonl");
	std::string line;
	uint32_t records{};
	while (std::getline(manifest, line)) {
		FLOWUI_CHECK(line.find("\"schema\":1") != std::string::npos);
		FLOWUI_CHECK(line.find("quoted\\\"key\\u000aline") != std::string::npos);
		++records;
	}
	FLOWUI_CHECK(records == 4u);
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
