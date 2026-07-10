#include "FlowUi/App.hpp"
#include "FlowUi/PublicStructs.hpp"
#include "FlowUi/BuildConfig.hpp"
#include "managers/FontManager.hpp"
#include "managers/ImageManager.hpp"
#if FLOWUI_INCLUDE_ICON_MANAGER
#include "managers/IconManager.hpp"
#endif
#include "managers/ViewPortManager.hpp"
#include "managers/UiManager.hpp"
#if FLOW_UI_DEV_MODE
#include "devMode/debugView.hpp"
#include "devMode/performanceDiagnostics.hpp"
#endif
#include "internal/UiTextureRegistry.hpp"
#include "internal/InputQueue.hpp"
#include "Ui/Vk_UiRenderer.hpp"
#include "Vulkan/Vk_Context.hpp"
#include "Vulkan/Vk_Frames.hpp"
#include "Vulkan/Vk_Swapchain.hpp"
#include "window/Window.hpp"

#include <array>
#include <algorithm>
#include <cctype>
#include <chrono>
#include <cstdio>
#include <filesystem>
#include <limits>
#include <string>
#include <unordered_map>
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

bool isArfontPath(const std::filesystem::path& path) {
	return !path.empty() && toLowerAscii(path.extension().string()) == ".arfont";
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

class UiTextureRegistry final : public detail::IUiTextureRegistry {
public:
	void init(VulkanContext& vk, VulkanUiRenderer& renderer, uint32_t framesInFlight) {
		destroy(vk);
		renderer_ = &renderer;
		framesInFlight_ = std::max<uint32_t>(1u, framesInFlight);
		currentFrameIndex_ = 0u;
		retiredSlotsByFrame_.assign(framesInFlight_, {});
		nextSlot_ = 1u;
		freeSlots_.clear();
		keyToSlot_.clear();
		activeBindings_.clear();

		if (renderer_->textureSlotCapacity() < 2u) {
			renderer_->reserveTextureSlots(vk, 2u);
		}
		renderer_->clearTextureSlotBinding(0u);
		renderer_->rebuildTextureDescriptors(vk.device);
	}

	void onFrameStart(VulkanContext& vk, uint32_t frameIndex) {
		if (!renderer_ || retiredSlotsByFrame_.empty()) {
			return;
		}
		currentFrameIndex_ = frameIndex % static_cast<uint32_t>(retiredSlotsByFrame_.size());
		reclaimRetiredBucket(vk, currentFrameIndex_);
	}

	void destroy(VulkanContext& vk) {
		if (renderer_ && vk.device != VK_NULL_HANDLE) {
			reclaimAllRetiredSlots(vk);
			for (const auto& [slot, _] : activeBindings_) {
				if (slot != 0u) {
					renderer_->clearTextureSlotBinding(slot);
				}
			}
			renderer_->rebuildTextureDescriptors(vk.device);
		}

		renderer_ = nullptr;
		framesInFlight_ = 1u;
		currentFrameIndex_ = 0u;
		nextSlot_ = 1u;
		freeSlots_.clear();
		keyToSlot_.clear();
		activeBindings_.clear();
		retiredSlotsByFrame_.clear();
	}

	uint32_t registerOrReplaceSlot(
		VulkanContext& vk,
		std::string_view namespacedKey,
		VkImageView imageView,
		VkSampler sampler,
		bool& inserted) override {
		if (!renderer_) {
			throw std::runtime_error("UiTextureRegistry is not initialized.");
		}
		if (namespacedKey.empty()) {
			throw std::runtime_error("UiTextureRegistry key must not be empty.");
		}
		if (imageView == VK_NULL_HANDLE || sampler == VK_NULL_HANDLE) {
			throw std::runtime_error("UiTextureRegistry received an invalid image binding.");
		}

		const std::string key(namespacedKey);
		auto keyIt = keyToSlot_.find(key);
		inserted = (keyIt == keyToSlot_.end());

		const uint32_t assignedSlot = acquireSlot(vk);
		renderer_->setTextureSlotBinding(assignedSlot, imageView, sampler);
		activeBindings_[assignedSlot] = VkDescriptorImageInfo{
			sampler,
			imageView,
			VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
		};

		if (inserted) {
			keyToSlot_.emplace(key, assignedSlot);
			return assignedSlot;
		}

		const uint32_t oldSlot = keyIt->second;
		keyIt->second = assignedSlot;
		activeBindings_.erase(oldSlot);
		retireSlot(oldSlot);
		return assignedSlot;
	}

	bool updateSlotBinding(
		std::string_view namespacedKey,
		VkImageView imageView,
		VkSampler sampler) override {
		if (!renderer_) {
			return false;
		}
		if (imageView == VK_NULL_HANDLE || sampler == VK_NULL_HANDLE) {
			throw std::runtime_error("UiTextureRegistry received an invalid image binding update.");
		}

		const auto keyIt = keyToSlot_.find(std::string(namespacedKey));
		if (keyIt == keyToSlot_.end()) {
			return false;
		}

		const uint32_t slot = keyIt->second;
		renderer_->setTextureSlotBinding(slot, imageView, sampler);
		activeBindings_[slot] = VkDescriptorImageInfo{
			sampler,
			imageView,
			VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
		};
		return true;
	}

	bool removeSlot(std::string_view namespacedKey) override {
		if (!renderer_) {
			return false;
		}
		const auto keyIt = keyToSlot_.find(std::string(namespacedKey));
		if (keyIt == keyToSlot_.end()) {
			return false;
		}

		const uint32_t removedSlot = keyIt->second;
		keyToSlot_.erase(keyIt);
		activeBindings_.erase(removedSlot);
		retireSlot(removedSlot);
		return true;
	}

	bool containsSlot(std::string_view namespacedKey) const override {
		return keyToSlot_.find(std::string(namespacedKey)) != keyToSlot_.end();
	}

private:
	uint32_t acquireSlot(VulkanContext& vk) {
		if (!freeSlots_.empty()) {
			const uint32_t slot = freeSlots_.back();
			freeSlots_.pop_back();
			return slot;
		}

		if (!renderer_) {
			throw std::runtime_error("UiTextureRegistry renderer is null.");
		}

		if (nextSlot_ >= renderer_->textureSlotCapacity()) {
			ensureSlotCapacity(vk, nextSlot_ + 1u);
		}

		return nextSlot_++;
	}

	void ensureSlotCapacity(VulkanContext& vk, uint32_t requiredCapacity) {
		if (!renderer_) {
			throw std::runtime_error("UiTextureRegistry renderer is null.");
		}
		if (requiredCapacity <= renderer_->textureSlotCapacity()) {
			return;
		}
		if (vk.device == VK_NULL_HANDLE) {
			throw std::runtime_error("UiTextureRegistry cannot grow capacity without a Vulkan device.");
		}

		vkCheck(vkDeviceWaitIdle(vk.device), "Failed to wait device idle while growing UI texture descriptor capacity.");
		reclaimAllRetiredSlots(vk);

		uint32_t newCapacity = std::max<uint32_t>(2u, renderer_->textureSlotCapacity());
		while (newCapacity < requiredCapacity) {
			newCapacity *= 2u;
		}

		renderer_->reserveTextureSlots(vk, newCapacity);
		for (const auto& [slot, binding] : activeBindings_) {
			renderer_->setTextureSlotBinding(slot, binding.imageView, binding.sampler);
		}
		renderer_->rebuildTextureDescriptors(vk.device);
	}

	void retireSlot(uint32_t slot) {
		if (!renderer_ || slot == 0u) {
			return;
		}
		if (retiredSlotsByFrame_.empty()) {
			renderer_->clearTextureSlotBinding(slot);
			freeSlots_.push_back(slot);
			return;
		}
		const uint32_t bucket = currentFrameIndex_ % static_cast<uint32_t>(retiredSlotsByFrame_.size());
		retiredSlotsByFrame_[bucket].push_back(slot);
	}

	void reclaimRetiredBucket(VulkanContext& vk, uint32_t bucketIndex) {
		if (!renderer_ || bucketIndex >= retiredSlotsByFrame_.size()) {
			return;
		}
		std::vector<uint32_t>& bucket = retiredSlotsByFrame_[bucketIndex];
		for (uint32_t slot : bucket) {
			if (slot != 0u) {
				renderer_->clearTextureSlotBinding(slot);
			}
			freeSlots_.push_back(slot);
		}
		bucket.clear();
		(void)vk;
	}

	void reclaimAllRetiredSlots(VulkanContext& vk) {
		for (uint32_t i = 0; i < retiredSlotsByFrame_.size(); ++i) {
			reclaimRetiredBucket(vk, i);
		}
	}

private:
	VulkanUiRenderer* renderer_ = nullptr;
	uint32_t framesInFlight_ = 1u;
	uint32_t currentFrameIndex_ = 0u;
	uint32_t nextSlot_ = 1u;

	std::vector<uint32_t> freeSlots_;
	std::unordered_map<std::string, uint32_t> keyToSlot_;
	std::unordered_map<uint32_t, VkDescriptorImageInfo> activeBindings_;
	std::vector<std::vector<uint32_t>> retiredSlotsByFrame_;
};

struct App::Impl {
	AppConfig config{};

	std::unique_ptr<detail::IWindowBackend> window;
	detail::InputQueue inputQueue;

	VulkanContext vk;
	Swapchain swap;
	FrameVk frames;

	UiManager ui;
	VulkanUiRenderer renderer;
	UiTextureRegistry textureRegistry;
	FontManager fonts;
	ImageManager imageManager;
	ViewPortManager viewPortManager;
#if FLOWUI_INCLUDE_ICON_MANAGER
	IconManager icons;
#endif
	FrameInput frameInputForCurrentFrame{};
	Clay_RenderCommandArray renderCommandsForCurrentFrame{};
	uint32_t uiFrameSlotCount = 1;
	std::vector<VkImageLayout> swapchainImageLayouts;
	std::chrono::steady_clock::time_point previousBeginFrameTimestamp{};
	bool hasPreviousBeginFrameTimestamp = false;
	float uiToFramebufferScaleX = 1.0f;
	float uiToFramebufferScaleY = 1.0f;

	bool framebufferResized = false;
	VkExtent2D observedFramebufferExtent{};

	explicit Impl(const AppConfig& initialConfig)
		: config(initialConfig), ui(config) {
		const uint32_t configuredFramesInFlight = std::max<uint32_t>(1u, config.vk.framesInFlight);
		uiFrameSlotCount = configuredFramesInFlight;
	}

		void init() {
			// 1) window backend
			window = detail::makeDefaultWindowBackend(config.window, &inputQueue);
			window->setInputConfig(config.window.input);
			ui.setClipboardAccessors(
				[this](std::string_view text) {
					if (window) {
						window->setClipboardText(text);
					}
				},
				[this]() -> std::string {
					if (!window) {
						return {};
					}
					return window->getClipboardText();
				});
			ui.setCursorAccessor(
				[this](CursorType cursorType) {
					if (window) {
						window->setCursorType(cursorType);
					}
				});

		// 2) instance/surface/device
		vk.createInstance(config, window->requiredInstanceExtensions());
		vk.createSurface(*window);
		vk.pickPhysicalDevice(config);
		vk.createDevice(config);

		// 3) swapchain + per-frame resources
		swap.create(config, vk, window->framebufferExtent());
		frames.create(config, vk, swap.images.size());
		swapchainImageLayouts.assign(swap.images.size(), VK_IMAGE_LAYOUT_UNDEFINED);
		observedFramebufferExtent = window->framebufferExtent();

		// 4) renderer/resources (dynamic rendering needs format)
		renderer.init(config, vk, swap.format);
		textureRegistry.init(vk, renderer, uiFrameSlotCount);
		imageManager.setRegistry(&textureRegistry);
		imageManager.init(vk, renderer, uiFrameSlotCount);
		viewPortManager.setRegistry(&textureRegistry);
		viewPortManager.init(vk, renderer, uiFrameSlotCount);
		fonts.init(vk, config.ui.fontAtlasSize);
		ui.setFontManager(&fonts);
		renderer.setFontManager(&fonts);
#if FLOWUI_INCLUDE_ICON_MANAGER
		icons.setRegistry(&textureRegistry);
		icons.init(vk, config.iconManager);
#endif

		bool defaultFontLoaded = false;
		try {
			const FontManager::FontFamilyId defaultFamilyId = fonts.createFamily(config.ui.defaultFontFamily);
			const FontManager::FontId defaultFontId = fonts.resolveFont(defaultFamilyId);
			defaultFontLoaded = fonts.getFontById(defaultFontId) != nullptr;
		} catch (const std::exception& e) {
			std::fprintf(stderr, "[FlowUi] Warning: failed loading ui.defaultFontFamily (%s)\n", e.what());
		}

		if (!defaultFontLoaded && fonts.getFontById(0) == nullptr) {
			const std::string fallbackFamilyName =
				config.ui.defaultFontFamily.name.empty() ? std::string("Default") : config.ui.defaultFontFamily.name;
			if (fonts.getFamilyId(fallbackFamilyName) == std::numeric_limits<FontManager::FontFamilyId>::max()) {
				FlowUi::FontFamilyCreateInfo fallbackFamily{};
				fallbackFamily.name = fallbackFamilyName;
				fallbackFamily.faces.clear();
				fonts.createFamily(fallbackFamily);
			}

			const std::array<std::filesystem::path, 5> fallbackCandidates = {
				std::filesystem::path(FLOWUI_SOURCE_DIR) / "assets/fonts/FacultyGlyphic-Regular.arfont",
				std::filesystem::path(FLOWUI_SOURCE_DIR) / "external/msdf-atlas-gen/artery-font-format/example.arfont",
				std::filesystem::path("assets/fonts/FacultyGlyphic-Regular.arfont"),
				std::filesystem::path("external/assets/fonts/FacultyGlyphic-Regular.arfont"),
				std::filesystem::path("external/external/msdf-atlas-gen/artery-font-format/example.arfont"),
			};

			for (const auto& fallbackPath : fallbackCandidates) {
				if (!std::filesystem::is_regular_file(fallbackPath)) {
					continue;
				}

				try {
					FlowUi::FontFaceCreateInfo fallbackFace{};
					fallbackFace.path = fallbackPath;
					fallbackFace.pixelSize = config.ui.defaultFontFamily.faces.empty()
						? 18.0f
						: config.ui.defaultFontFamily.faces.front().pixelSize;
					fonts.addFamilyFace(fallbackFamilyName, fallbackFace);
					std::fprintf(
						stderr,
						"[FlowUi] Warning: loaded fallback font because ui.defaultFontFamily was not usable: %s\n",
						fallbackPath.string().c_str());
					defaultFontLoaded = true;
					break;
				} catch (const std::exception& e) {
					std::fprintf(
						stderr,
						"[FlowUi] Warning: failed loading fallback font %s (%s)\n",
						fallbackPath.string().c_str(),
						e.what());
				}
			}
		}

		if (!defaultFontLoaded && fonts.getFontById(0) == nullptr) {
			std::fprintf(
				stderr,
				"[FlowUi] Warning: no .arfont font loaded; text rendering is disabled.\n");
		}
	}

	void pollEvents() { window->pollEvents(); }

	void beginFrame() {
		const auto now = std::chrono::steady_clock::now();
		const auto beginFrameStart = now;
		double deltaTimeSeconds = 0.0;
		if (hasPreviousBeginFrameTimestamp) {
			deltaTimeSeconds = std::chrono::duration<double>(now - previousBeginFrameTimestamp).count();
			deltaTimeSeconds = std::max(0.0, deltaTimeSeconds);
		}
		previousBeginFrameTimestamp = now;
		hasPreviousBeginFrameTimestamp = true;

#if FLOW_UI_DEV_MODE
		ui.performanceDiagnostics().beginFrame(frames.currentFrame, deltaTimeSeconds);
#endif

		const uint32_t frameSlot =
			frames.frames.empty() ? 0u : (frames.currentFrame % static_cast<uint32_t>(frames.frames.size()));

		textureRegistry.onFrameStart(vk, frameSlot);
		imageManager.onFrameStart(vk, frameSlot);
		viewPortManager.onFrameStart(vk, frameSlot);

		pollEvents();
		frameInputForCurrentFrame = inputQueue.drain(deltaTimeSeconds);

		const float inverseClampedUiScale = 1 / std::max(1.0e-6f, config.ui.uiScale);
		constexpr float kBaseScrollSensitivity = 20.0f;
		const VkExtent2D windowExtent = window->windowExtent();
		const VkExtent2D framebufferExtent = window->framebufferExtent();
		if (framebufferExtent.width != observedFramebufferExtent.width ||
			framebufferExtent.height != observedFramebufferExtent.height) {
			framebufferResized = true;
			observedFramebufferExtent = framebufferExtent;
		}
		const float logicalWindowWidth = static_cast<float>(std::max<uint32_t>(1u, windowExtent.width));
		const float logicalWindowHeight = static_cast<float>(std::max<uint32_t>(1u, windowExtent.height));
		const float framebufferWidth = static_cast<float>(std::max<uint32_t>(1u, framebufferExtent.width));
		const float framebufferHeight = static_cast<float>(std::max<uint32_t>(1u, framebufferExtent.height));
		const float layoutWidth = logicalWindowWidth * inverseClampedUiScale;
		const float layoutHeight = logicalWindowHeight * inverseClampedUiScale;
		uiToFramebufferScaleX = framebufferWidth / std::max(layoutWidth, 1.0e-6f);
		uiToFramebufferScaleY = framebufferHeight / std::max(layoutHeight, 1.0e-6f);

		FrameInput frameInputForLayout = frameInputForCurrentFrame;
		frameInputForLayout.mouseX *= inverseClampedUiScale;
		frameInputForLayout.mouseY *= inverseClampedUiScale;
		frameInputForLayout.scrollX *= inverseClampedUiScale * kBaseScrollSensitivity;
		frameInputForLayout.scrollY *= inverseClampedUiScale * kBaseScrollSensitivity;
		if (frameInputForLayout.shift && frameInputForLayout.scrollY != 0.0f) {
			frameInputForLayout.scrollX += frameInputForLayout.scrollY;
			frameInputForLayout.scrollY = 0.0f;
		}
		ui.beginFrame(frameSlot, frameInputForLayout, layoutWidth, layoutHeight);
#if FLOW_UI_DEV_MODE
		ui.performanceDiagnostics().current().beginFrameMs =
			devMode::PerformanceDiagnostics::elapsedMs(beginFrameStart);
#endif
	}

	void endFrame() {
#if FLOW_UI_DEV_MODE
		const auto endFrameStart = devMode::PerformanceDiagnostics::Clock::now();
		const auto clayStart = endFrameStart;
#endif
		renderCommandsForCurrentFrame = ui.endFrame();
#if FLOW_UI_DEV_MODE
		ui.performanceDiagnostics().current().clayLayoutMs =
			devMode::PerformanceDiagnostics::elapsedMs(clayStart);
		ui.performanceDiagnostics().current().clayCommandCount = renderCommandsForCurrentFrame.length;
		const auto prepStart = devMode::PerformanceDiagnostics::Clock::now();
#endif
		viewPortManager.prepareFrameTargets(
			renderCommandsForCurrentFrame,
			uiToFramebufferScaleX,
			uiToFramebufferScaleY);
#if FLOWUI_INCLUDE_ICON_MANAGER
		icons.prepareFrameTextures(
			renderCommandsForCurrentFrame,
			uiToFramebufferScaleX,
			uiToFramebufferScaleY);
#endif
#if FLOW_UI_DEV_MODE
		ui.performanceDiagnostics().current().resourcePrepMs =
			devMode::PerformanceDiagnostics::elapsedMs(prepStart);
		ui.performanceDiagnostics().current().endFrameMs =
			devMode::PerformanceDiagnostics::elapsedMs(endFrameStart);
#endif
	}

	void drawFrame() {
		if (frames.frames.empty() || swap.swapchain == VK_NULL_HANDLE || swap.views.empty()) {
			return;
		}
#if FLOW_UI_DEV_MODE
		const auto drawFrameStart = devMode::PerformanceDiagnostics::Clock::now();
#endif
		const VkExtent2D framebufferExtent = window ? window->framebufferExtent() : VkExtent2D{};
		if (framebufferExtent.width != observedFramebufferExtent.width ||
			framebufferExtent.height != observedFramebufferExtent.height) {
			framebufferResized = true;
			observedFramebufferExtent = framebufferExtent;
		}
		if (framebufferResized) {
			if (!recreateSwapchainIfNeeded()) {
				return;
			}
		}

		FrameVk::Frame& frame = frames.getCurrentFrame();
#if FLOW_UI_DEV_MODE
		const auto fenceWaitStart = devMode::PerformanceDiagnostics::Clock::now();
#endif
		vkCheck(
			vkWaitForFences(vk.device, 1, &frame.inFlight, VK_TRUE, UINT64_MAX),
			"Failed to wait for in-flight fence.");
#if FLOW_UI_DEV_MODE
		ui.performanceDiagnostics().current().fenceWaitMs =
			devMode::PerformanceDiagnostics::elapsedMs(fenceWaitStart);
#endif

		uint32_t swapchainImageIndex = 0;
#if FLOW_UI_DEV_MODE
		const auto acquireStart = devMode::PerformanceDiagnostics::Clock::now();
#endif
		VkResult acquireResult = vkAcquireNextImageKHR(
			vk.device,
			swap.swapchain,
			UINT64_MAX,
			frame.imageAvailable,
			VK_NULL_HANDLE,
			&swapchainImageIndex);
#if FLOW_UI_DEV_MODE
		ui.performanceDiagnostics().current().acquireMs =
			devMode::PerformanceDiagnostics::elapsedMs(acquireStart);
#endif

		if (acquireResult == VK_ERROR_OUT_OF_DATE_KHR) {
			framebufferResized = true;
			recreateSwapchainIfNeeded();
			return;
		}
		if (acquireResult != VK_SUCCESS && acquireResult != VK_SUBOPTIMAL_KHR) {
			throw std::runtime_error("Failed to acquire swapchain image.");
		}

		const bool acquiredSuboptimalSwapchain = (acquireResult == VK_SUBOPTIMAL_KHR);
#if FLOW_UI_DEV_MODE
		ui.performanceDiagnostics().current().swappedSuboptimal = acquiredSuboptimalSwapchain;
#endif
		if (swapchainImageIndex >= swap.views.size() || swapchainImageIndex >= frames.imageInFlight.size() ||
			swapchainImageIndex >= swapchainImageLayouts.size() ||
			swapchainImageIndex >= frames.renderFinishedBySwapImage.size()) {
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

		viewPortManager.remapRenderCommandsForFrame(renderCommandsForCurrentFrame, frames.currentFrame);
#if FLOW_UI_DEV_MODE
		const auto viewportRecordStart = devMode::PerformanceDiagnostics::Clock::now();
		viewPortManager.recordFramePasses(vk, frame.cmd, frames.currentFrame, &ui.performanceDiagnostics().current());
		ui.performanceDiagnostics().current().viewportRecordMs =
			devMode::PerformanceDiagnostics::elapsedMs(viewportRecordStart);
#else
		viewPortManager.recordFramePasses(vk, frame.cmd, frames.currentFrame);
#endif

#if FLOW_UI_DEV_MODE
		const auto uiRecordStart = devMode::PerformanceDiagnostics::Clock::now();
#endif
		renderer.render(
			vk,
			frame.cmd,
			renderCommandsForCurrentFrame,
			ui.inputFieldFrameOverrides(),
			swap.extent,
			swap.views[swapchainImageIndex],
			frames.currentFrame,
			uiToFramebufferScaleX,
			uiToFramebufferScaleY
#if FLOW_UI_DEV_MODE
			,
			&ui.performanceDiagnostics().current()
#endif
			);
#if FLOW_UI_DEV_MODE
		ui.performanceDiagnostics().current().uiRecordMs =
			devMode::PerformanceDiagnostics::elapsedMs(uiRecordStart);
#endif

		transitionSwapchainImageLayout(
			frame.cmd,
			swap.images[swapchainImageIndex],
			swapchainImageLayouts[swapchainImageIndex],
			VK_IMAGE_LAYOUT_PRESENT_SRC_KHR);
		swapchainImageLayouts[swapchainImageIndex] = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;

		vkCheck(vkEndCommandBuffer(frame.cmd), "Failed to end command buffer.");

		VkPipelineStageFlags waitStages[] = { VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT };
		VkSemaphore presentWaitSemaphore = frames.renderFinishedBySwapImage[swapchainImageIndex];

		VkSubmitInfo submitInfo{};
		submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
		submitInfo.waitSemaphoreCount = 1;
		submitInfo.pWaitSemaphores = &frame.imageAvailable;
		submitInfo.pWaitDstStageMask = waitStages;
		submitInfo.commandBufferCount = 1;
		submitInfo.pCommandBuffers = &frame.cmd;
		submitInfo.signalSemaphoreCount = 1;
		submitInfo.pSignalSemaphores = &presentWaitSemaphore;

#if FLOW_UI_DEV_MODE
		const auto submitStart = devMode::PerformanceDiagnostics::Clock::now();
#endif
		vkCheck(
			vkQueueSubmit(vk.graphicsQ, 1, &submitInfo, frame.inFlight),
			"Failed to submit UI command buffer.");
#if FLOW_UI_DEV_MODE
		ui.performanceDiagnostics().current().submitMs =
			devMode::PerformanceDiagnostics::elapsedMs(submitStart);
#endif

		VkPresentInfoKHR presentInfo{};
		presentInfo.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;
		presentInfo.waitSemaphoreCount = 1;
		presentInfo.pWaitSemaphores = &presentWaitSemaphore;
		presentInfo.swapchainCount = 1;
		presentInfo.pSwapchains = &swap.swapchain;
		presentInfo.pImageIndices = &swapchainImageIndex;

#if FLOW_UI_DEV_MODE
		const auto presentStart = devMode::PerformanceDiagnostics::Clock::now();
#endif
		VkResult presentResult = vkQueuePresentKHR(vk.presentQ, &presentInfo);
#if FLOW_UI_DEV_MODE
		ui.performanceDiagnostics().current().presentMs =
			devMode::PerformanceDiagnostics::elapsedMs(presentStart);
#endif

		if (presentResult == VK_ERROR_OUT_OF_DATE_KHR || presentResult == VK_SUBOPTIMAL_KHR ||
			acquiredSuboptimalSwapchain || framebufferResized) {
			framebufferResized = true;
			recreateSwapchainIfNeeded();
		} else if (presentResult != VK_SUCCESS) {
			throw std::runtime_error("Failed to present swapchain image.");
		}

		frames.advance();
#if FLOW_UI_DEV_MODE
		ui.performanceDiagnostics().current().drawFrameMs =
			devMode::PerformanceDiagnostics::elapsedMs(drawFrameStart);
		ui.performanceDiagnostics().endCompletedFrame();
#endif
	}

	bool recreateSwapchainIfNeeded() {
		if (!window || vk.device == VK_NULL_HANDLE) {
			return false;
		}
		const VkExtent2D framebufferExtent = window->framebufferExtent();
		observedFramebufferExtent = framebufferExtent;
		if (framebufferExtent.width == 0 || framebufferExtent.height == 0) {
			framebufferResized = true;
			return false;
		}

		vkCheck(vkDeviceWaitIdle(vk.device), "Failed to wait for device idle during swapchain recreation.");
		swap.recreate(config, vk, framebufferExtent);
		frames.onSwapchainRecreated(vk, swap.images.size());
		renderer.onSwapchainFormatChanged(vk, swap.format); // only if changed
		swapchainImageLayouts.assign(swap.images.size(), VK_IMAGE_LAYOUT_UNDEFINED);
		framebufferResized = false;
#if FLOW_UI_DEV_MODE
		ui.performanceDiagnostics().current().swapchainRecreated = true;
#endif
		return true;
	}

	void cleanup() {
		vkDeviceWaitIdle(vk.device);

		viewPortManager.destroy(vk);
		imageManager.destroy(vk);
#if FLOWUI_INCLUDE_ICON_MANAGER
		icons.destroy(vk);
#endif
		textureRegistry.destroy(vk);
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

bool App::shouldClose() const {
	if (!impl_ || !impl_->window) {
		return true;
	}
	return impl_->window->shouldClose();
}

void App::setShouldClose(int value) {
	if (impl_ && impl_->window) {
		impl_->window->setShouldClose(value);
	}
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

FontManager& App::fonts() {
	if (!impl_) {
		throw std::runtime_error("FlowUi::App not initialized.");
	}
	return impl_->fonts;
}

const FontManager& App::fonts() const {
	if (!impl_) {
		throw std::runtime_error("FlowUi::App not initialized.");
	}
	return impl_->fonts;
}

ImageManager& App::images() {
	if (!impl_) {
		throw std::runtime_error("FlowUi::App not initialized.");
	}
	return impl_->imageManager;
}

const ImageManager& App::images() const {
	if (!impl_) {
		throw std::runtime_error("FlowUi::App not initialized.");
	}
	return impl_->imageManager;
}

#if FLOWUI_INCLUDE_ICON_MANAGER
IconManager& App::icons() {
	if (!impl_) {
		throw std::runtime_error("FlowUi::App not initialized.");
	}
	return impl_->icons;
}

const IconManager& App::icons() const {
	if (!impl_) {
		throw std::runtime_error("FlowUi::App not initialized.");
	}
	return impl_->icons;
}
#endif

#if FLOWUI_PUBLIC_VULKAN_INTEROP
ViewPortManager& App::viewPorts() {
	if (!impl_) {
		throw std::runtime_error("FlowUi::App not initialized.");
	}
	return impl_->viewPortManager;
}

const ViewPortManager& App::viewPorts() const {
	if (!impl_) {
		throw std::runtime_error("FlowUi::App not initialized.");
	}
	return impl_->viewPortManager;
}
#endif

UiManager& App::ui() {
	if (!impl_) {
		throw std::runtime_error("FlowUi::App not initialized.");
	}
	return impl_->ui;
}

const UiManager& App::ui() const {
	if (!impl_) {
		throw std::runtime_error("FlowUi::App not initialized.");
	}
	return impl_->ui;
}

void App::setWindowTitle(std::string_view title) {
	if (impl_ && impl_->window) {
		impl_->window->setTitle(title);
	}
}

void* App::nativeWindowHandle() const {
	if (!impl_ || !impl_->window) {
		return nullptr;
	}
	return impl_->window->nativeHandle();
}

void App::setWindowInputConfig(const WindowInputConfig& config) {
	if (!impl_ || !impl_->window) {
		return;
	}
	impl_->config.window.input = config;
	impl_->window->setInputConfig(config);
}

WindowInputConfig App::windowInputConfig() const {
	if (!impl_ || !impl_->window) {
		return {};
	}
	return impl_->window->getInputConfig();
}

bool App::supportsRawMouseMotion() const {
	if (!impl_ || !impl_->window) {
		return false;
	}
	return impl_->window->supportsRawMouseMotion();
}

void App::setClipboardText(std::string_view text) {
	if (impl_ && impl_->window) {
		impl_->window->setClipboardText(text);
	}
}

std::string App::clipboardText() const {
	if (!impl_ || !impl_->window) {
		return {};
	}
	return impl_->window->getClipboardText();
}

std::pair<int, int> App::windowSize() const {
	if (!impl_ || !impl_->window) {
		return {0, 0};
	}
	VkExtent2D extent = impl_->window->windowExtent();
	return {static_cast<int>(extent.width), static_cast<int>(extent.height)};
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
#if FLOW_UI_DEV_MODE
	devMode::initializeDevFlowElementResourcesFromApp(app);
#endif
	return app;
}

} // namespace FlowUi
