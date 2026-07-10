#include "managers/ViewPortManager.hpp"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <stdexcept>
#include <string>
#include <utility>

#include "internal/UiTextureRegistry.hpp"
#include "Ui/Vk_UiRenderer.hpp"
#include "Vulkan/Vk_Context.hpp"
#include "internal/Vma.hpp"
#if FLOW_UI_DEV_MODE
#include "devMode/performanceDiagnostics.hpp"
#endif

namespace {

void vkCheck(VkResult result, const char* message) {
	if (result != VK_SUCCESS) {
		throw std::runtime_error(message);
	}
}

} // namespace

namespace FlowUi {

std::string_view ViewPort::getKey() const {
	return key_;
}

bool ViewPort::hasValidSize() const {
	return width_ > 0 && height_ > 0;
}

VkExtent2D ViewPort::getSize() const {
	return {
		static_cast<uint32_t>(std::max<int32_t>(0, width_)),
		static_cast<uint32_t>(std::max<int32_t>(0, height_)),
	};
}

TextureRef ViewPort::textureRef() const {
	TextureRef result{};
	result.id = slotId_;
	result.sourceWidth = width_;
	result.sourceHeight = height_;
	return result;
}

void ViewPort::setRenderCallback(RenderCallback callback) {
	renderCallback_ = std::move(callback);
}

void ViewPort::clearRenderCallback() {
	renderCallback_ = {};
}

bool ViewPort::hasRenderCallback() const {
	return static_cast<bool>(renderCallback_);
}

void ViewPort::setClearColor(float r, float g, float b, float a) {
	clearColor_ = { r, g, b, a };
}

std::array<float, 4> ViewPort::clearColor() const {
	return clearColor_;
}

void ViewPort::setClearEveryFrame(bool enabled) {
	clearEveryFrame_ = enabled;
}

bool ViewPort::clearEveryFrame() const {
	return clearEveryFrame_;
}

void ViewPortManager::setRegistry(detail::IUiTextureRegistry* registry) {
	registry_ = registry;
}

void ViewPortManager::init(VulkanContext& vk, VulkanUiRenderer& renderer, uint32_t framesInFlight) {
	detail::IUiTextureRegistry* const preservedRegistry = registry_;
	destroy(vk);
	registry_ = preservedRegistry;
	if (vk.device == VK_NULL_HANDLE || vk.allocator == nullptr) {
		throw std::runtime_error("ViewPortManager init requires a valid Vulkan device + allocator.");
	}
	if (vk.graphicsQFamily == UINT32_MAX || vk.graphicsQ == VK_NULL_HANDLE) {
		throw std::runtime_error("ViewPortManager requires a valid graphics queue family.");
	}

	vk_ = &vk;
	renderer_ = &renderer;
	framesInFlight_ = std::max<uint32_t>(1u, framesInFlight);
	currentFrameIndex_ = 0u;
	viewPortsByKey_.clear();
	slotToOwner_.clear();
	missingTextureWarnings_.clear();

	interop_ = ViewPortVulkanInterop{
		.instance = vk.instance,
		.physicalDevice = vk.phys,
		.device = vk.device,
		.allocator = vk.allocator,
		.graphicsQueue = vk.graphicsQ,
		.graphicsQueueFamily = vk.graphicsQFamily,
		.framesInFlight = framesInFlight_,
	};

	VkSamplerCreateInfo samplerInfo{};
	samplerInfo.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
	samplerInfo.magFilter = VK_FILTER_LINEAR;
	samplerInfo.minFilter = VK_FILTER_LINEAR;
	samplerInfo.mipmapMode = VK_SAMPLER_MIPMAP_MODE_LINEAR;
	samplerInfo.addressModeU = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
	samplerInfo.addressModeV = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
	samplerInfo.addressModeW = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
	samplerInfo.minLod = 0.0f;
	samplerInfo.maxLod = 0.0f;
	samplerInfo.maxAnisotropy = 1.0f;
	samplerInfo.anisotropyEnable = VK_FALSE;
	samplerInfo.compareEnable = VK_FALSE;
	samplerInfo.borderColor = VK_BORDER_COLOR_FLOAT_TRANSPARENT_BLACK;
	samplerInfo.unnormalizedCoordinates = VK_FALSE;
	vkCheck(vkCreateSampler(vk.device, &samplerInfo, nullptr, &sharedSampler_), "Failed to create viewport sampler.");
}

bool ViewPortManager::create(std::string_view key, const ViewPortCreateInfo& createInfo) {
	if (!vk_ || vk_->device == VK_NULL_HANDLE || vk_->allocator == nullptr || sharedSampler_ == VK_NULL_HANDLE) {
		throw std::runtime_error("ViewPortManager is not initialized.");
	}
	if (!registry_) {
		throw std::runtime_error("ViewPortManager registry backend is not set.");
	}
	if (key.empty()) {
		throw std::runtime_error("ViewPortManager key must not be empty.");
	}

	const std::string keyString(key);
	if (viewPortsByKey_.find(keyString) != viewPortsByKey_.end()) {
		return false;
	}

	ViewPortRecord record{};
	record.viewport.key_ = keyString;
	record.viewport.colorFormat_ =
		(createInfo.colorFormat == VK_FORMAT_UNDEFINED) ? VK_FORMAT_R8G8B8A8_UNORM : createInfo.colorFormat;
	record.viewport.clearColor_ = createInfo.clearColor;
	record.viewport.clearEveryFrame_ = createInfo.clearEveryFrame;
	record.viewport.width_ = 1;
	record.viewport.height_ = 1;
	record.desiredWidth = 1u;
	record.desiredHeight = 1u;
	record.referencedThisFrame = false;

	try {
		record.frameCommands = createFrameCommandResources(*vk_);
		record.imagesByFrame.reserve(framesInFlight_);
		record.namespacedFrameKeys.reserve(framesInFlight_);
		record.slotIds.reserve(framesInFlight_);

		for (uint32_t frameSlot = 0; frameSlot < framesInFlight_; ++frameSlot) {
			record.imagesByFrame.push_back(createRenderTargetImage(*vk_, 1u, 1u, record.viewport.colorFormat_));

			const std::string namespacedKey = makeNamespacedKey(key, frameSlot);
			bool inserted = false;
			const uint32_t slotId = registry_->registerOrReplaceSlot(
				*vk_,
				namespacedKey,
				record.imagesByFrame.back().view,
				sharedSampler_,
				inserted);
			if (!inserted) {
				throw std::runtime_error("ViewPortManager internal namespaced key collision.");
			}

			record.namespacedFrameKeys.push_back(namespacedKey);
			record.slotIds.push_back(slotId);
		}

		if (record.slotIds.empty()) {
			throw std::runtime_error("ViewPortManager failed to create per-frame viewport slot IDs.");
		}

		const uint32_t activeSlot = currentFrameIndex_ % static_cast<uint32_t>(record.slotIds.size());
		record.viewport.slotId_ = record.slotIds[activeSlot];
		record.viewport.width_ = static_cast<int32_t>(record.imagesByFrame[activeSlot].width);
		record.viewport.height_ = static_cast<int32_t>(record.imagesByFrame[activeSlot].height);

		ViewPortRecord& insertedRecord = viewPortsByKey_.emplace(keyString, std::move(record)).first->second;
		for (uint32_t frameSlot = 0; frameSlot < insertedRecord.slotIds.size(); ++frameSlot) {
			slotToOwner_[insertedRecord.slotIds[frameSlot]] = SlotOwner{ keyString, frameSlot };
		}
		missingTextureWarnings_.erase(keyString);
		return true;
	} catch (...) {
		for (const std::string& namespacedKey : record.namespacedFrameKeys) {
			const bool removed = registry_->removeSlot(namespacedKey);
			(void)removed;
		}
		destroyFrameCommandResources(*vk_, record.frameCommands);
		destroyRenderTargetImages(*vk_, record.imagesByFrame);
		throw;
	}
}

bool ViewPortManager::remove(std::string_view key) {
	if (!vk_ || vk_->device == VK_NULL_HANDLE) {
		throw std::runtime_error("ViewPortManager is not initialized.");
	}
	if (!registry_) {
		throw std::runtime_error("ViewPortManager registry backend is not set.");
	}

	const std::string keyString(key);
	const auto recordIt = viewPortsByKey_.find(keyString);
	if (recordIt == viewPortsByKey_.end()) {
		return false;
	}

	vkCheck(vkDeviceWaitIdle(vk_->device), "Failed to wait idle while removing viewport.");
	for (const std::string& namespacedKey : recordIt->second.namespacedFrameKeys) {
		const bool removed = registry_->removeSlot(namespacedKey);
		(void)removed;
	}
	for (uint32_t slotId : recordIt->second.slotIds) {
		slotToOwner_.erase(slotId);
	}

	destroyFrameCommandResources(*vk_, recordIt->second.frameCommands);
	destroyRenderTargetImages(*vk_, recordIt->second.imagesByFrame);
	viewPortsByKey_.erase(recordIt);
	missingTextureWarnings_.erase(keyString);
	return true;
}

bool ViewPortManager::contains(std::string_view key) const {
	return viewPortsByKey_.find(std::string(key)) != viewPortsByKey_.end();
}

ViewPort* ViewPortManager::getViewPort(std::string_view key) {
	const auto it = viewPortsByKey_.find(std::string(key));
	if (it == viewPortsByKey_.end()) {
		return nullptr;
	}
	return &it->second.viewport;
}

const ViewPort* ViewPortManager::getViewPort(std::string_view key) const {
	const auto it = viewPortsByKey_.find(std::string(key));
	if (it == viewPortsByKey_.end()) {
		return nullptr;
	}
	return &it->second.viewport;
}

TextureRef ViewPortManager::getTexture(std::string_view key) const {
	TextureRef result{};

	const std::string keyString(key);
	const auto it = viewPortsByKey_.find(keyString);
	if (it == viewPortsByKey_.end()) {
		if (missingTextureWarnings_.find(keyString) == missingTextureWarnings_.end()) {
			std::fprintf(stderr, "[FlowUi] Warning: viewport key '%s' was not found, using fallback texture id 0.\n", keyString.c_str());
			missingTextureWarnings_.insert(keyString);
		}
		result.id = 0u;
		return result;
	}

	const ViewPortRecord& record = it->second;
	if (record.slotIds.empty() || record.imagesByFrame.empty()) {
		result.id = 0u;
		return result;
	}

	const uint32_t frameSlot = currentFrameIndex_ % static_cast<uint32_t>(record.slotIds.size());
	result.id = record.slotIds[frameSlot];
	result.sourceWidth = static_cast<int32_t>(record.imagesByFrame[frameSlot].width);
	result.sourceHeight = static_cast<int32_t>(record.imagesByFrame[frameSlot].height);
	return result;
}

const ViewPortVulkanInterop& ViewPortManager::getVulkanInterop() const {
	if (!vk_ || vk_->device == VK_NULL_HANDLE) {
		throw std::runtime_error("ViewPortManager is not initialized.");
	}
	return interop_;
}

void ViewPortManager::onFrameStart(VulkanContext& vk, uint32_t frameIndex) {
	(void)vk;
	currentFrameIndex_ = frameIndex % std::max<uint32_t>(1u, framesInFlight_);
	for (auto& [_, record] : viewPortsByKey_) {
		if (record.slotIds.empty() || record.imagesByFrame.empty()) {
			continue;
		}
		const uint32_t frameSlot = currentFrameIndex_ % static_cast<uint32_t>(record.slotIds.size());
		record.viewport.slotId_ = record.slotIds[frameSlot];
		record.viewport.width_ = static_cast<int32_t>(record.imagesByFrame[frameSlot].width);
		record.viewport.height_ = static_cast<int32_t>(record.imagesByFrame[frameSlot].height);
	}
}

void ViewPortManager::resetFrameTracking() {
	for (auto& [_, record] : viewPortsByKey_) {
		record.referencedThisFrame = false;
		record.desiredWidth = 1u;
		record.desiredHeight = 1u;
	}
}

void ViewPortManager::prepareFrameTargets(
	const Clay_RenderCommandArray& renderCommands,
	float uiToFramebufferScaleX,
	float uiToFramebufferScaleY) {
	resetFrameTracking();

	const float clampedScaleX = std::max(uiToFramebufferScaleX, 1.0e-6f);
	const float clampedScaleY = std::max(uiToFramebufferScaleY, 1.0e-6f);

	for (int32_t i = 0; i < renderCommands.length; ++i) {
		const Clay_RenderCommand& command = renderCommands.internalArray[i];
		if (command.commandType != CLAY_RENDER_COMMAND_TYPE_IMAGE) {
			continue;
		}

		const auto* textureRef = reinterpret_cast<const TextureRef*>(command.renderData.image.imageData);
		const uint32_t slotId = textureRef ? textureRef->id : 0u;
		if (slotId == 0u) {
			continue;
		}

		const auto slotOwnerIt = slotToOwner_.find(slotId);
		if (slotOwnerIt == slotToOwner_.end()) {
			continue;
		}

		auto recordIt = viewPortsByKey_.find(slotOwnerIt->second.key);
		if (recordIt == viewPortsByKey_.end()) {
			continue;
		}

		ViewPortRecord& record = recordIt->second;
		record.referencedThisFrame = true;

		const float scaledWidth = std::max(0.0f, command.boundingBox.width * clampedScaleX);
		const float scaledHeight = std::max(0.0f, command.boundingBox.height * clampedScaleY);
		const uint32_t desiredWidth = std::max<uint32_t>(1u, static_cast<uint32_t>(std::ceil(scaledWidth)));
		const uint32_t desiredHeight = std::max<uint32_t>(1u, static_cast<uint32_t>(std::ceil(scaledHeight)));

		record.desiredWidth = std::max(record.desiredWidth, desiredWidth);
		record.desiredHeight = std::max(record.desiredHeight, desiredHeight);
	}
}

void ViewPortManager::remapRenderCommandsForFrame(Clay_RenderCommandArray& renderCommands, uint32_t frameIndex) {
	const uint32_t frameSlot = frameIndex % std::max<uint32_t>(1u, framesInFlight_);

	for (int32_t i = 0; i < renderCommands.length; ++i) {
		Clay_RenderCommand& command = renderCommands.internalArray[i];
		if (command.commandType != CLAY_RENDER_COMMAND_TYPE_IMAGE) {
			continue;
		}

		auto* textureRef = reinterpret_cast<TextureRef*>(command.renderData.image.imageData);
		if (!textureRef || textureRef->id == 0u) {
			continue;
		}

		const auto slotOwnerIt = slotToOwner_.find(textureRef->id);
		if (slotOwnerIt == slotToOwner_.end()) {
			continue;
		}

		auto recordIt = viewPortsByKey_.find(slotOwnerIt->second.key);
		if (recordIt == viewPortsByKey_.end()) {
			continue;
		}

		ViewPortRecord& record = recordIt->second;
		if (record.slotIds.empty() || record.imagesByFrame.empty()) {
			continue;
		}

		const uint32_t resolvedSlot = frameSlot % static_cast<uint32_t>(record.slotIds.size());
		textureRef->id = record.slotIds[resolvedSlot];
		textureRef->sourceWidth = static_cast<int32_t>(record.imagesByFrame[resolvedSlot].width);
		textureRef->sourceHeight = static_cast<int32_t>(record.imagesByFrame[resolvedSlot].height);
	}
}

bool ViewPortManager::resizeRequired() const {
	for (const auto& [_, record] : viewPortsByKey_) {
		if (!record.referencedThisFrame) {
			continue;
		}
		for (const ViewPortImageResource& image : record.imagesByFrame) {
			if (record.desiredWidth != image.width || record.desiredHeight != image.height) {
				return true;
			}
		}
	}
	return false;
}

void ViewPortManager::ensureRenderTargetSize(VulkanContext& vk, ViewPortRecord& record) {
	const uint32_t clampedWidth = std::max<uint32_t>(1u, record.desiredWidth);
	const uint32_t clampedHeight = std::max<uint32_t>(1u, record.desiredHeight);

	for (uint32_t frameSlot = 0; frameSlot < record.imagesByFrame.size(); ++frameSlot) {
		ViewPortImageResource& image = record.imagesByFrame[frameSlot];
		if (image.width == clampedWidth && image.height == clampedHeight) {
			continue;
		}

		ViewPortImageResource resizedImage = createRenderTargetImage(vk, clampedWidth, clampedHeight, record.viewport.colorFormat_);
		try {
			const bool updated = registry_ && registry_->updateSlotBinding(
				record.namespacedFrameKeys[frameSlot],
				resizedImage.view,
				sharedSampler_);
			if (!updated) {
				throw std::runtime_error("ViewPortManager failed to update texture slot binding after resize.");
			}
		} catch (...) {
			destroyRenderTargetImage(vk, resizedImage);
			throw;
		}

		destroyRenderTargetImage(vk, image);
		image = resizedImage;
	}

	if (!record.slotIds.empty() && !record.imagesByFrame.empty()) {
		const uint32_t frameSlot = currentFrameIndex_ % static_cast<uint32_t>(record.slotIds.size());
		record.viewport.slotId_ = record.slotIds[frameSlot];
		record.viewport.width_ = static_cast<int32_t>(record.imagesByFrame[frameSlot].width);
		record.viewport.height_ = static_cast<int32_t>(record.imagesByFrame[frameSlot].height);
	}
}

void ViewPortManager::recordFramePasses(
	VulkanContext& vk,
	VkCommandBuffer primaryCommandBuffer,
	uint32_t frameIndex
#if FLOW_UI_DEV_MODE
	,
	devMode::FrameDiagnostics* diagnostics
#endif
	) {
	if (primaryCommandBuffer == VK_NULL_HANDLE || !vk_ || vk.device == VK_NULL_HANDLE) {
		return;
	}

	if (resizeRequired()) {
		vkCheck(vkDeviceWaitIdle(vk.device), "Failed to wait idle while resizing viewport render targets.");
	}

	const uint32_t frameSlot = frameIndex % std::max<uint32_t>(1u, framesInFlight_);

	for (auto& [_, record] : viewPortsByKey_) {
		if (!record.referencedThisFrame) {
			continue;
		}
#if FLOW_UI_DEV_MODE
		const auto viewportRecordStart = devMode::PerformanceDiagnostics::Clock::now();
		devMode::ViewPortFrameDiagnostics* viewportDiagnostics = nullptr;
		if (diagnostics) {
			auto& entry = diagnostics->viewports[record.viewport.key_];
			entry.key = record.viewport.key_;
			entry.width = record.desiredWidth;
			entry.height = record.desiredHeight;
			entry.hadCallback = static_cast<bool>(record.viewport.renderCallback_);
			viewportDiagnostics = &entry;

			diagnostics->referencedViewportCount += 1u;
			diagnostics->viewportPixelArea +=
				static_cast<uint64_t>(record.desiredWidth) *
				static_cast<uint64_t>(record.desiredHeight);

			bool viewportWillResize = false;
			for (const ViewPortImageResource& image : record.imagesByFrame) {
				if (image.width != record.desiredWidth || image.height != record.desiredHeight) {
					viewportWillResize = true;
					break;
				}
			}
			if (viewportWillResize) {
				entry.resized = true;
				diagnostics->resizedViewportCount += 1u;
			}
		}
#endif
		if (record.frameCommands.empty() || frameSlot >= record.frameCommands.size()) {
			throw std::runtime_error("ViewPortManager frame command resources are missing.");
		}
		if (record.imagesByFrame.empty() || frameSlot >= record.imagesByFrame.size()) {
			throw std::runtime_error("ViewPortManager per-frame viewport images are missing.");
		}
		if (record.slotIds.empty() || frameSlot >= record.slotIds.size()) {
			throw std::runtime_error("ViewPortManager per-frame viewport slot IDs are missing.");
		}

		ensureRenderTargetSize(vk, record);
		record.viewport.slotId_ = record.slotIds[frameSlot];

		FrameCommandResources& frameResources = record.frameCommands[frameSlot];
		ViewPortImageResource& image = record.imagesByFrame[frameSlot];

		vkCheck(
			vkResetCommandPool(vk.device, frameResources.pool, 0),
			"Failed to reset viewport secondary command pool.");

		VkCommandBufferInheritanceRenderingInfo inheritanceRenderingInfo{};
		inheritanceRenderingInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_INHERITANCE_RENDERING_INFO;
		inheritanceRenderingInfo.colorAttachmentCount = 1;
		inheritanceRenderingInfo.pColorAttachmentFormats = &record.viewport.colorFormat_;
		inheritanceRenderingInfo.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;

		VkCommandBufferInheritanceInfo inheritanceInfo{};
		inheritanceInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_INHERITANCE_INFO;
		inheritanceInfo.pNext = &inheritanceRenderingInfo;

		VkCommandBufferBeginInfo beginInfo{};
		beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
		beginInfo.flags =
			VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT |
			VK_COMMAND_BUFFER_USAGE_RENDER_PASS_CONTINUE_BIT;
		beginInfo.pInheritanceInfo = &inheritanceInfo;
		vkCheck(
			vkBeginCommandBuffer(frameResources.commandBuffer, &beginInfo),
			"Failed to begin viewport secondary command buffer.");

		if (record.viewport.renderCallback_) {
			const ViewPortRenderContext context{
				.commandBuffer = frameResources.commandBuffer,
				.extent = { image.width, image.height },
				.colorFormat = record.viewport.colorFormat_,
				.frameIndex = frameSlot,
				.key = record.viewport.key_,
				.vulkan = &interop_,
			};
#if FLOW_UI_DEV_MODE
			const auto callbackStart = devMode::PerformanceDiagnostics::Clock::now();
#endif
			record.viewport.renderCallback_(context);
#if FLOW_UI_DEV_MODE
			if (viewportDiagnostics) {
				viewportDiagnostics->callbackCpuMs =
					devMode::PerformanceDiagnostics::elapsedMs(callbackStart);
			}
#endif
		}

		vkCheck(vkEndCommandBuffer(frameResources.commandBuffer), "Failed to end viewport secondary command buffer.");

		const VkImageLayout previousLayout = image.layout;
		transitionImageLayout(
			primaryCommandBuffer,
			image.image,
			image.layout,
			VK_IMAGE_LAYOUT_ATTACHMENT_OPTIMAL);
		image.layout = VK_IMAGE_LAYOUT_ATTACHMENT_OPTIMAL;

		const bool shouldClear = record.viewport.clearEveryFrame_ || previousLayout == VK_IMAGE_LAYOUT_UNDEFINED;
		VkRenderingAttachmentInfo colorAttachment{};
		colorAttachment.sType = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO;
		colorAttachment.imageView = image.view;
		colorAttachment.imageLayout = VK_IMAGE_LAYOUT_ATTACHMENT_OPTIMAL;
		colorAttachment.loadOp = shouldClear ? VK_ATTACHMENT_LOAD_OP_CLEAR : VK_ATTACHMENT_LOAD_OP_LOAD;
		colorAttachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
		colorAttachment.clearValue.color.float32[0] = record.viewport.clearColor_[0];
		colorAttachment.clearValue.color.float32[1] = record.viewport.clearColor_[1];
		colorAttachment.clearValue.color.float32[2] = record.viewport.clearColor_[2];
		colorAttachment.clearValue.color.float32[3] = record.viewport.clearColor_[3];

		VkRenderingInfo renderingInfo{};
		renderingInfo.sType = VK_STRUCTURE_TYPE_RENDERING_INFO;
		renderingInfo.flags = VK_RENDERING_CONTENTS_SECONDARY_COMMAND_BUFFERS_BIT;
		renderingInfo.renderArea.offset = { 0, 0 };
		renderingInfo.renderArea.extent = { image.width, image.height };
		renderingInfo.layerCount = 1;
		renderingInfo.colorAttachmentCount = 1;
		renderingInfo.pColorAttachments = &colorAttachment;

		vkCmdBeginRendering(primaryCommandBuffer, &renderingInfo);
		if (record.viewport.renderCallback_) {
			vkCmdExecuteCommands(primaryCommandBuffer, 1, &frameResources.commandBuffer);
		}
		vkCmdEndRendering(primaryCommandBuffer);

		transitionImageLayout(
			primaryCommandBuffer,
			image.image,
			image.layout,
			VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
		image.layout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
#if FLOW_UI_DEV_MODE
		if (viewportDiagnostics) {
			viewportDiagnostics->recordCpuMs =
				devMode::PerformanceDiagnostics::elapsedMs(viewportRecordStart);
		}
#endif
	}
}

void ViewPortManager::destroy(VulkanContext& vk) {
	if (vk.device != VK_NULL_HANDLE) {
		vkDeviceWaitIdle(vk.device);
	}

	for (auto& [_, record] : viewPortsByKey_) {
		if (registry_) {
			for (const std::string& namespacedKey : record.namespacedFrameKeys) {
				const bool removed = registry_->removeSlot(namespacedKey);
				(void)removed;
			}
		}
		destroyFrameCommandResources(vk, record.frameCommands);
		destroyRenderTargetImages(vk, record.imagesByFrame);
	}
	viewPortsByKey_.clear();
	slotToOwner_.clear();
	missingTextureWarnings_.clear();

	if (sharedSampler_ != VK_NULL_HANDLE && vk.device != VK_NULL_HANDLE) {
		vkDestroySampler(vk.device, sharedSampler_, nullptr);
	}
	sharedSampler_ = VK_NULL_HANDLE;

	registry_ = nullptr;
	vk_ = nullptr;
	renderer_ = nullptr;
	framesInFlight_ = 1u;
	currentFrameIndex_ = 0u;
	interop_ = {};
}

std::vector<ViewPortManager::FrameCommandResources> ViewPortManager::createFrameCommandResources(VulkanContext& vk) const {
	std::vector<FrameCommandResources> resources(framesInFlight_);

	try {
		for (FrameCommandResources& entry : resources) {
			VkCommandPoolCreateInfo poolInfo{};
			poolInfo.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
			poolInfo.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
			poolInfo.queueFamilyIndex = vk.graphicsQFamily;
			vkCheck(vkCreateCommandPool(vk.device, &poolInfo, nullptr, &entry.pool), "Failed to create viewport command pool.");

			VkCommandBufferAllocateInfo allocInfo{};
			allocInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
			allocInfo.commandPool = entry.pool;
			allocInfo.level = VK_COMMAND_BUFFER_LEVEL_SECONDARY;
			allocInfo.commandBufferCount = 1;
			vkCheck(
				vkAllocateCommandBuffers(vk.device, &allocInfo, &entry.commandBuffer),
				"Failed to allocate viewport secondary command buffer.");
		}
	} catch (...) {
		for (FrameCommandResources& entry : resources) {
			if (entry.pool != VK_NULL_HANDLE) {
				vkDestroyCommandPool(vk.device, entry.pool, nullptr);
			}
		}
		throw;
	}

	return resources;
}

void ViewPortManager::destroyFrameCommandResources(
	VulkanContext& vk,
	std::vector<FrameCommandResources>& frameResources) const {
	for (FrameCommandResources& entry : frameResources) {
		if (entry.pool != VK_NULL_HANDLE && vk.device != VK_NULL_HANDLE) {
			vkDestroyCommandPool(vk.device, entry.pool, nullptr);
		}
		entry.pool = VK_NULL_HANDLE;
		entry.commandBuffer = VK_NULL_HANDLE;
	}
	frameResources.clear();
}

ViewPortManager::ViewPortImageResource ViewPortManager::createRenderTargetImage(
	VulkanContext& vk,
	uint32_t width,
	uint32_t height,
	VkFormat format) const {
	if (width == 0 || height == 0 || format == VK_FORMAT_UNDEFINED) {
		throw std::runtime_error("ViewPortManager cannot create a zero-sized or undefined-format render target.");
	}

	ViewPortImageResource result{};

	VkImageCreateInfo imageInfo{};
	imageInfo.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
	imageInfo.imageType = VK_IMAGE_TYPE_2D;
	imageInfo.extent = { width, height, 1 };
	imageInfo.mipLevels = 1;
	imageInfo.arrayLayers = 1;
	imageInfo.format = format;
	imageInfo.tiling = VK_IMAGE_TILING_OPTIMAL;
	imageInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
	imageInfo.usage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT;
	imageInfo.samples = VK_SAMPLE_COUNT_1_BIT;
	imageInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

	VmaAllocationCreateInfo allocInfo{};
	allocInfo.usage = VMA_MEMORY_USAGE_AUTO_PREFER_DEVICE;

	vkCheck(
		vmaCreateImage(
			vk.allocator,
			&imageInfo,
			&allocInfo,
			&result.image,
			&result.allocation,
			nullptr),
		"Failed to create viewport render target image.");

	VkImageViewCreateInfo viewInfo{};
	viewInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
	viewInfo.image = result.image;
	viewInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
	viewInfo.format = format;
	viewInfo.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
	viewInfo.subresourceRange.baseMipLevel = 0;
	viewInfo.subresourceRange.levelCount = 1;
	viewInfo.subresourceRange.baseArrayLayer = 0;
	viewInfo.subresourceRange.layerCount = 1;
	vkCheck(vkCreateImageView(vk.device, &viewInfo, nullptr, &result.view), "Failed to create viewport render target view.");

	result.layout = VK_IMAGE_LAYOUT_UNDEFINED;
	result.width = width;
	result.height = height;
	return result;
}

void ViewPortManager::destroyRenderTargetImages(VulkanContext& vk, std::vector<ViewPortImageResource>& images) const {
	for (ViewPortImageResource& image : images) {
		destroyRenderTargetImage(vk, image);
	}
	images.clear();
}

void ViewPortManager::destroyRenderTargetImage(VulkanContext& vk, ViewPortImageResource& image) const {
	if (image.view != VK_NULL_HANDLE && vk.device != VK_NULL_HANDLE) {
		vkDestroyImageView(vk.device, image.view, nullptr);
	}
	image.view = VK_NULL_HANDLE;

	if (image.image != VK_NULL_HANDLE && vk.allocator != nullptr) {
		vmaDestroyImage(vk.allocator, image.image, image.allocation);
	}
	image.image = VK_NULL_HANDLE;
	image.allocation = nullptr;
	image.layout = VK_IMAGE_LAYOUT_UNDEFINED;
	image.width = 0u;
	image.height = 0u;
}

void ViewPortManager::transitionImageLayout(
	VkCommandBuffer commandBuffer,
	VkImage image,
	VkImageLayout oldLayout,
	VkImageLayout newLayout) const {
	if (oldLayout == newLayout || image == VK_NULL_HANDLE || commandBuffer == VK_NULL_HANDLE) {
		return;
	}

	VkPipelineStageFlags srcStage = VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT;
	VkPipelineStageFlags dstStage = VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT;
	VkAccessFlags srcAccess = 0u;
	VkAccessFlags dstAccess = 0u;

	if (oldLayout == VK_IMAGE_LAYOUT_UNDEFINED && newLayout == VK_IMAGE_LAYOUT_ATTACHMENT_OPTIMAL) {
		srcStage = VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT;
		dstStage = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
		srcAccess = 0u;
		dstAccess = VK_ACCESS_COLOR_ATTACHMENT_READ_BIT | VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
	} else if (
		oldLayout == VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL &&
		newLayout == VK_IMAGE_LAYOUT_ATTACHMENT_OPTIMAL) {
		srcStage = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
		dstStage = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
		srcAccess = VK_ACCESS_SHADER_READ_BIT;
		dstAccess = VK_ACCESS_COLOR_ATTACHMENT_READ_BIT | VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
	} else if (
		oldLayout == VK_IMAGE_LAYOUT_ATTACHMENT_OPTIMAL &&
		newLayout == VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL) {
		srcStage = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
		dstStage = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
		srcAccess = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
		dstAccess = VK_ACCESS_SHADER_READ_BIT;
	} else {
		throw std::runtime_error("ViewPortManager encountered unsupported image layout transition.");
	}

	VkImageMemoryBarrier barrier{};
	barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
	barrier.oldLayout = oldLayout;
	barrier.newLayout = newLayout;
	barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
	barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
	barrier.image = image;
	barrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
	barrier.subresourceRange.baseMipLevel = 0;
	barrier.subresourceRange.levelCount = 1;
	barrier.subresourceRange.baseArrayLayer = 0;
	barrier.subresourceRange.layerCount = 1;
	barrier.srcAccessMask = srcAccess;
	barrier.dstAccessMask = dstAccess;

	vkCmdPipelineBarrier(
		commandBuffer,
		srcStage,
		dstStage,
		0,
		0,
		nullptr,
		0,
		nullptr,
		1,
		&barrier);
}

std::string ViewPortManager::makeNamespacedKey(std::string_view key, uint32_t frameSlot) const {
	return "vp:" + std::string(key) + ":f" + std::to_string(frameSlot);
}

} // namespace FlowUi
