#pragma once

#include <array>
#include <cstdint>
#include <functional>
#include <memory>
#include <stdexcept>
#include <string>
#include <string_view>
#include <type_traits>
#include <unordered_map>
#include <unordered_set>
#include <utility>
#include <vector>

#include <clay.h>
#include <vulkan/vulkan.h>

#include "FlowUi/PublicStructs.hpp"

struct VulkanContext;
struct VulkanUiRenderer;
struct VmaAllocation_T;
struct VmaAllocator_T;

namespace FlowUi {

namespace detail {
struct IUiTextureRegistry;
} // namespace detail

class App;

struct ViewPortVulkanInterop {
	VkInstance instance = VK_NULL_HANDLE;
	VkPhysicalDevice physicalDevice = VK_NULL_HANDLE;
	VkDevice device = VK_NULL_HANDLE;
	VmaAllocator_T* allocator = nullptr;
	VkQueue graphicsQueue = VK_NULL_HANDLE;
	uint32_t graphicsQueueFamily = UINT32_MAX;
	uint32_t framesInFlight = 1u;
};

struct ViewPortRenderContext {
	VkCommandBuffer commandBuffer = VK_NULL_HANDLE;
	VkExtent2D extent{};
	VkFormat colorFormat = VK_FORMAT_UNDEFINED;
	uint32_t frameIndex = 0u;
	std::string_view key{};
	const ViewPortVulkanInterop* vulkan = nullptr;
};

struct ViewPortCreateInfo {
	VkFormat colorFormat = VK_FORMAT_R8G8B8A8_UNORM;
	std::array<float, 4> clearColor = { 0.0f, 0.0f, 0.0f, 0.0f };
	bool clearEveryFrame = true;
};

class ViewPort {
public:
	using RenderCallback = std::function<void(const ViewPortRenderContext&)>;

	std::string_view getKey() const;
	bool hasValidSize() const;
	VkExtent2D getSize() const;
	TextureRef textureRef() const;

	void setRenderCallback(RenderCallback callback);

	template <typename T, typename Fn>
	void setRenderCallback(std::shared_ptr<T> userData, Fn&& callback) {
		if (!userData) {
			throw std::runtime_error("ViewPort::setRenderCallback requires a non-null shared_ptr payload.");
		}
		using CallbackType = std::decay_t<Fn>;
		renderCallback_ = [payload = std::move(userData), typedCallback = CallbackType(std::forward<Fn>(callback))](
			const ViewPortRenderContext& context) mutable {
			typedCallback(context, *payload);
		};
	}

	void clearRenderCallback();
	bool hasRenderCallback() const;

	void setClearColor(float r, float g, float b, float a);
	std::array<float, 4> clearColor() const;
	void setClearEveryFrame(bool enabled);
	bool clearEveryFrame() const;

private:
	friend class ViewPortManager;

	std::string key_{};
	uint32_t slotId_ = 0u;
	int32_t width_ = 0;
	int32_t height_ = 0;
	VkFormat colorFormat_ = VK_FORMAT_R8G8B8A8_UNORM;
	std::array<float, 4> clearColor_ = { 0.0f, 0.0f, 0.0f, 0.0f };
	bool clearEveryFrame_ = true;
	RenderCallback renderCallback_{};
};

class ViewPortManager {
public:
	bool create(std::string_view key, const ViewPortCreateInfo& createInfo = {});
	bool remove(std::string_view key);
	bool contains(std::string_view key) const;
	ViewPort* getViewPort(std::string_view key);
	const ViewPort* getViewPort(std::string_view key) const;
	TextureRef getTexture(std::string_view key) const;
	const ViewPortVulkanInterop& getVulkanInterop() const;

private:
	friend class App;

	void setRegistry(detail::IUiTextureRegistry* registry);
	void init(VulkanContext& vk, VulkanUiRenderer& renderer, uint32_t framesInFlight);
	void onFrameStart(VulkanContext& vk, uint32_t frameIndex);
	void prepareFrameTargets(
		const Clay_RenderCommandArray& renderCommands,
		float uiToFramebufferScaleX,
		float uiToFramebufferScaleY);
	void remapRenderCommandsForFrame(Clay_RenderCommandArray& renderCommands, uint32_t frameIndex);
	void recordFramePasses(VulkanContext& vk, VkCommandBuffer primaryCommandBuffer, uint32_t frameIndex);
	void destroy(VulkanContext& vk);

	struct ViewPortImageResource {
		VkImage image = VK_NULL_HANDLE;
		VmaAllocation_T* allocation = nullptr;
		VkImageView view = VK_NULL_HANDLE;
		VkImageLayout layout = VK_IMAGE_LAYOUT_UNDEFINED;
		uint32_t width = 0u;
		uint32_t height = 0u;
	};

	struct FrameCommandResources {
		VkCommandPool pool = VK_NULL_HANDLE;
		VkCommandBuffer commandBuffer = VK_NULL_HANDLE;
	};

	struct ViewPortRecord {
		ViewPort viewport{};
		std::vector<ViewPortImageResource> imagesByFrame{};
		std::vector<FrameCommandResources> frameCommands{};
		std::vector<std::string> namespacedFrameKeys{};
		std::vector<uint32_t> slotIds{};
		uint32_t desiredWidth = 1u;
		uint32_t desiredHeight = 1u;
		bool referencedThisFrame = false;
	};

	struct SlotOwner {
		std::string key{};
		uint32_t frameSlot = 0u;
	};

	void resetFrameTracking();
	bool resizeRequired() const;
	void ensureRenderTargetSize(VulkanContext& vk, ViewPortRecord& record);
	std::vector<FrameCommandResources> createFrameCommandResources(VulkanContext& vk) const;
	void destroyFrameCommandResources(VulkanContext& vk, std::vector<FrameCommandResources>& frameResources) const;
	ViewPortImageResource createRenderTargetImage(VulkanContext& vk, uint32_t width, uint32_t height, VkFormat format) const;
	void destroyRenderTargetImages(VulkanContext& vk, std::vector<ViewPortImageResource>& images) const;
	void destroyRenderTargetImage(VulkanContext& vk, ViewPortImageResource& image) const;
	void transitionImageLayout(
		VkCommandBuffer commandBuffer,
		VkImage image,
		VkImageLayout oldLayout,
		VkImageLayout newLayout) const;
	std::string makeNamespacedKey(std::string_view key, uint32_t frameSlot) const;

private:
	detail::IUiTextureRegistry* registry_ = nullptr;
	VulkanContext* vk_ = nullptr;
	VulkanUiRenderer* renderer_ = nullptr;
	uint32_t framesInFlight_ = 1u;
	uint32_t currentFrameIndex_ = 0u;

	VkSampler sharedSampler_ = VK_NULL_HANDLE;
	ViewPortVulkanInterop interop_{};

	std::unordered_map<std::string, ViewPortRecord> viewPortsByKey_;
	std::unordered_map<uint32_t, SlotOwner> slotToOwner_;
	mutable std::unordered_set<std::string> missingTextureWarnings_;
};

} // namespace FlowUi
