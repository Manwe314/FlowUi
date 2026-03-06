#include "FlowUi.hpp"
#include "flowui/PublicStructs.hpp"
#include "managers/FontManager.hpp"
#include "managers/SvgManager.hpp"
#include "Ui/UiContext.hpp"
#include "Ui/Vk_UiRenderer.hpp"
#include "Vulkan/Vk_Context.hpp"
#include "Vulkan/Vk_Frames.hpp"
#include "Vulkan/Vk_Swapchain.hpp"
#include "window/Window.hpp"

#include <algorithm>
#include <cctype>
#include <chrono>
#include <filesystem>
#include <string>
#include <vector>
#include <stdexcept>

namespace {

void vkCheck(VkResult result, const char* message) {
	if (result != VK_SUCCESS) {
		throw std::runtime_error(message);
	}
}

std::string toLowerAscii(std::string value) {
	for (char& c : value) {
		c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
	}
	return value;
}

void transitionSwapchainImageLayout(
	VkCommandBuffer commandBuffer,
	VkImage image,
	VkImageLayout oldLayout,
	VkImageLayout newLayout) {
	if (oldLayout == newLayout) {
		return;
	}

	VkPipelineStageFlags sourceStageMask = VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT;
	VkPipelineStageFlags destinationStageMask = VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT;
	VkAccessFlags sourceAccessMask = 0;
	VkAccessFlags destinationAccessMask = 0;

	if ((oldLayout == VK_IMAGE_LAYOUT_UNDEFINED || oldLayout == VK_IMAGE_LAYOUT_PRESENT_SRC_KHR) &&
		newLayout == VK_IMAGE_LAYOUT_ATTACHMENT_OPTIMAL) {
		sourceStageMask = (oldLayout == VK_IMAGE_LAYOUT_UNDEFINED)
			? VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT
			: VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
		destinationStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
		sourceAccessMask = (oldLayout == VK_IMAGE_LAYOUT_UNDEFINED) ? 0 : VK_ACCESS_MEMORY_READ_BIT;
		destinationAccessMask = VK_ACCESS_COLOR_ATTACHMENT_READ_BIT | VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
	} else if (oldLayout == VK_IMAGE_LAYOUT_ATTACHMENT_OPTIMAL && newLayout == VK_IMAGE_LAYOUT_PRESENT_SRC_KHR) {
		sourceStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
		destinationStageMask = VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT;
		sourceAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
		destinationAccessMask = VK_ACCESS_MEMORY_READ_BIT;
	} else {
		throw std::runtime_error("Unsupported swapchain image layout transition.");
	}

	VkImageMemoryBarrier imageBarrier{};
	imageBarrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
	imageBarrier.oldLayout = oldLayout;
	imageBarrier.newLayout = newLayout;
	imageBarrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
	imageBarrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
	imageBarrier.image = image;
	imageBarrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
	imageBarrier.subresourceRange.baseMipLevel = 0;
	imageBarrier.subresourceRange.levelCount = 1;
	imageBarrier.subresourceRange.baseArrayLayer = 0;
	imageBarrier.subresourceRange.layerCount = 1;
	imageBarrier.srcAccessMask = sourceAccessMask;
	imageBarrier.dstAccessMask = destinationAccessMask;

	vkCmdPipelineBarrier(
		commandBuffer,
		sourceStageMask,
		destinationStageMask,
		0,
		0,
		nullptr,
		0,
		nullptr,
		1,
		&imageBarrier);
}

} // namespace

namespace FlowUi {

struct App::Impl {
	AppConfig config{};

	std::unique_ptr<IWindowBackend> window;
	InputQueue inputQueue;

	VulkanContext vk;
	Swapchain swap;
	FrameVk frames;
	ElementRegistry elementRegistry;

	UiContext ui;
	VulkanUiRenderer renderer;
	FontManager fonts;
	IconManager icons;
	FrameInput frameInputForCurrentFrame{};
	Clay_RenderCommandArray renderCommandsForCurrentFrame{};
	uint32_t uiFrameIndex = 0;
	uint32_t uiFrameSlotCount = 1;
	std::vector<VkImageLayout> swapchainImageLayouts;
	std::chrono::steady_clock::time_point previousBeginFrameTimestamp{};
	bool hasPreviousBeginFrameTimestamp = false;

	bool framebufferResized = false;

	explicit Impl(const AppConfig& initialConfig)
		: config(initialConfig), ui(elementRegistry, config) {
		const uint32_t configuredFramesInFlight = std::max<uint32_t>(1u, config.vk.framesInFlight);
		uiFrameSlotCount = configuredFramesInFlight;
	}

	void init() {
		// 1) window backend
		window = makeDefaultWindowBackend(config.window, &inputQueue);

		// 2) instance/surface/device
		vk.createInstance(config, window->requiredInstanceExtensions());
		vk.createSurface(*window);
		vk.pickPhysicalDevice(config);
		vk.createDevice(config);

		// 3) swapchain + per-frame resources
		swap.create(config, vk);
		frames.create(config, vk, swap.images.size());
		swapchainImageLayouts.assign(swap.images.size(), VK_IMAGE_LAYOUT_UNDEFINED);

		// 4) renderer/resources (dynamic rendering needs format)
		renderer.init(config, vk, swap.format);
		fonts.init(vk, config.ui.fontAtlasSize);
		ui.setFontManager(&fonts);
		renderer.setFontManager(&fonts);
		icons.init(vk, config.ui.iconAtlasSize);

		if (!config.ui.defaultFontPath.empty()) {
			const std::filesystem::path defaultFontPath = config.ui.defaultFontPath;
			const bool isArfont = toLowerAscii(defaultFontPath.extension().string()) == ".arfont";
			if (isArfont && std::filesystem::is_regular_file(defaultFontPath)) {
				const int defaultFontId = fonts.loadFont(vk, defaultFontPath.string(), config.ui.defaultFontPx);
				if (defaultFontId < 0) {
					throw std::runtime_error("Failed to register default .arfont font: " + defaultFontPath.string());
				}
			}
		}
	}

	void pollEvents() { window->pollEvents(); }

	void beginFrame() {
		const auto now = std::chrono::steady_clock::now();
		double deltaTimeSeconds = 0.0;
		if (hasPreviousBeginFrameTimestamp) {
			deltaTimeSeconds = std::chrono::duration<double>(now - previousBeginFrameTimestamp).count();
			deltaTimeSeconds = std::max(0.0, deltaTimeSeconds);
		}
		previousBeginFrameTimestamp = now;
		hasPreviousBeginFrameTimestamp = true;

		pollEvents();
		frameInputForCurrentFrame = inputQueue.drain(deltaTimeSeconds);

		const VkExtent2D framebufferExtent = window->framebufferExtent();
		const float screenWidth = static_cast<float>(std::max<uint32_t>(1u, framebufferExtent.width));
		const float screenHeight = static_cast<float>(std::max<uint32_t>(1u, framebufferExtent.height));
		ui.beginFrame(uiFrameIndex, frameInputForCurrentFrame, screenWidth, screenHeight);

		uiFrameIndex = (uiFrameIndex + 1) % uiFrameSlotCount;
	}

	void endFrame() { renderCommandsForCurrentFrame = ui.endFrame(); }

	void drawFrame() {
		if (frames.frames.empty() || swap.swapchain == VK_NULL_HANDLE || swap.views.empty()) {
			return;
		}

		FrameVk::Frame& frame = frames.getCurrantFrame();
		vkCheck(
			vkWaitForFences(vk.device, 1, &frame.inFlight, VK_TRUE, UINT64_MAX),
			"Failed to wait for in-flight fence.");

		uint32_t swapchainImageIndex = 0;
		VkResult acquireResult = vkAcquireNextImageKHR(
			vk.device,
			swap.swapchain,
			UINT64_MAX,
			frame.imageAvailable,
			VK_NULL_HANDLE,
			&swapchainImageIndex);

		if (acquireResult == VK_ERROR_OUT_OF_DATE_KHR) {
			recreateSwapchainIfNeeded();
			return;
		}
		if (acquireResult != VK_SUCCESS && acquireResult != VK_SUBOPTIMAL_KHR) {
			throw std::runtime_error("Failed to acquire swapchain image.");
		}

		const bool acquiredSuboptimalSwapchain = (acquireResult == VK_SUBOPTIMAL_KHR);
		if (swapchainImageIndex >= swap.views.size() || swapchainImageIndex >= frames.imageInFlight.size() ||
			swapchainImageIndex >= swapchainImageLayouts.size()) {
			throw std::runtime_error("Acquired swapchain image index is out of range.");
		}

		if (frames.imageInFlight[swapchainImageIndex] != VK_NULL_HANDLE) {
			vkCheck(
				vkWaitForFences(vk.device, 1, &frames.imageInFlight[swapchainImageIndex], VK_TRUE, UINT64_MAX),
				"Failed waiting for previously submitted fence for swapchain image.");
		}
		frames.imageInFlight[swapchainImageIndex] = frame.inFlight;

		vkCheck(vkResetFences(vk.device, 1, &frame.inFlight), "Failed to reset in-flight fence.");
		vkCheck(vkResetCommandPool(vk.device, frame.pool, 0), "Failed to reset command pool.");

		VkCommandBufferBeginInfo beginInfo{};
		beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
		beginInfo.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
		vkCheck(vkBeginCommandBuffer(frame.cmd, &beginInfo), "Failed to begin command buffer.");

		transitionSwapchainImageLayout(
			frame.cmd,
			swap.images[swapchainImageIndex],
			swapchainImageLayouts[swapchainImageIndex],
			VK_IMAGE_LAYOUT_ATTACHMENT_OPTIMAL);
		swapchainImageLayouts[swapchainImageIndex] = VK_IMAGE_LAYOUT_ATTACHMENT_OPTIMAL;

		renderer.render(vk, frame.cmd, renderCommandsForCurrentFrame, swap.extent, swap.views[swapchainImageIndex]);

		transitionSwapchainImageLayout(
			frame.cmd,
			swap.images[swapchainImageIndex],
			swapchainImageLayouts[swapchainImageIndex],
			VK_IMAGE_LAYOUT_PRESENT_SRC_KHR);
		swapchainImageLayouts[swapchainImageIndex] = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;

		vkCheck(vkEndCommandBuffer(frame.cmd), "Failed to end command buffer.");

		VkPipelineStageFlags waitStages[] = { VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT };

		VkSubmitInfo submitInfo{};
		submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
		submitInfo.waitSemaphoreCount = 1;
		submitInfo.pWaitSemaphores = &frame.imageAvailable;
		submitInfo.pWaitDstStageMask = waitStages;
		submitInfo.commandBufferCount = 1;
		submitInfo.pCommandBuffers = &frame.cmd;
		submitInfo.signalSemaphoreCount = 1;
		submitInfo.pSignalSemaphores = &frame.renderFinished;

		vkCheck(
			vkQueueSubmit(vk.graphicsQ, 1, &submitInfo, frame.inFlight),
			"Failed to submit UI command buffer.");

		VkPresentInfoKHR presentInfo{};
		presentInfo.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;
		presentInfo.waitSemaphoreCount = 1;
		presentInfo.pWaitSemaphores = &frame.renderFinished;
		presentInfo.swapchainCount = 1;
		presentInfo.pSwapchains = &swap.swapchain;
		presentInfo.pImageIndices = &swapchainImageIndex;

		VkResult presentResult = vkQueuePresentKHR(vk.presentQ, &presentInfo);

		if (presentResult == VK_ERROR_OUT_OF_DATE_KHR || presentResult == VK_SUBOPTIMAL_KHR ||
			acquiredSuboptimalSwapchain || framebufferResized) {
			framebufferResized = false;
			recreateSwapchainIfNeeded();
		} else if (presentResult != VK_SUCCESS) {
			throw std::runtime_error("Failed to present swapchain image.");
		}

		frames.advance();
	}

	void recreateSwapchainIfNeeded() {
		if (!window) {
			return;
		}
		const VkExtent2D framebufferExtent = window->framebufferExtent();
		if (framebufferExtent.width == 0 || framebufferExtent.height == 0) {
			return;
		}

		vkDeviceWaitIdle(vk.device);
		swap.recreate(config, vk);
		frames.onSwapchainRecreated(swap.images.size());
		renderer.onSwapchainFormatChanged(vk, swap.format); // only if changed
		swapchainImageLayouts.assign(swap.images.size(), VK_IMAGE_LAYOUT_UNDEFINED);
	}

	void cleanup() {
		vkDeviceWaitIdle(vk.device);

		icons.destroy(vk);
		fonts.destroy(vk);
		renderer.destroy(vk);

		frames.destroy(vk);
		swap.destroy(vk);

		vk.destroy();

		window.reset();
	}
};

App::App() = default;

App::App(App&&) noexcept = default;

App& App::operator=(App&&) noexcept = default;

App::~App() {
	if (impl_) {
		impl_->cleanup();
	}
}

void App::pollEvents() {
	if (impl_) {
		impl_->pollEvents();
	}
}

bool App::shouldClose() const {
	if (!impl_ || !impl_->window) {
		return true;
	}
	return impl_->window->shouldClose();
}

void App::beginFrame() {
	if (impl_) {
		impl_->beginFrame();
	}
}

void App::endFrame() {
	if (impl_) {
		impl_->endFrame();
	}
}

void App::drawFrame() {
	if (impl_) {
		impl_->drawFrame();
	}
}

int App::loadFont(std::string_view fontPath, float pxSize) {
	if (!impl_) {
		return -1;
	}
	return impl_->fonts.loadFont(impl_->vk, fontPath, pxSize);
}

int App::loadSvgIcon(std::string_view svgPath, int pxSize) {
	(void)svgPath;
	(void)pxSize;
	return -1;
}

UiContext& App::ui() {
	if (!impl_) {
		throw std::runtime_error("FlowUi::App not initialized.");
	}
	return impl_->ui;
}

ElementRegistry& App::elementRegistry() {
	if (!impl_) {
		throw std::runtime_error("FlowUi::App not initialized.");
	}
	return impl_->elementRegistry;
}

const ElementRegistry& App::elementRegistry() const {
	if (!impl_) {
		throw std::runtime_error("FlowUi::App not initialized.");
	}
	return impl_->elementRegistry;
}

void App::registerElement(ElementDefinition definition) {
	elementRegistry().registerElement(std::move(definition));
}

void App::setWindowTitle(std::string_view title) {
	if (impl_ && impl_->window) {
		impl_->window->setTitle(title);
	}
}

std::pair<int, int> App::windowSize() const {
	if (!impl_) {
		return {0, 0};
	}
	return {impl_->config.window.width, impl_->config.window.height};
}

std::pair<int, int> App::framebufferSize() const {
	if (!impl_ || !impl_->window) {
		return {0, 0};
	}
	VkExtent2D extent = impl_->window->framebufferExtent();
	return {static_cast<int>(extent.width), static_cast<int>(extent.height)};
}

App makeApplication(const AppConfig& cfg) {
	App app;
	app.impl_ = std::make_unique<App::Impl>(cfg);
	app.impl_->init();
	return app;
}

} // namespace FlowUi
