#pragma once
#include "internal/MemoryDemandPeaks.hpp"

#include <algorithm>
#include <array>
#include <cstdint>
#include <functional>
#include <string>
#include <unordered_map>
#include <vector>

#include <vulkan/vulkan.h>

#include "internal/StorageSystem/IStorageSystem.hpp"
#include "managers/ViewPortManager.hpp"

struct VulkanContext;

namespace FlowUi::detail::manager_storage {

struct ViewportFacadeState {
	storage::IStorageSystem* storage = nullptr;
	WindowId window = InvalidWindowId;
	std::string key{};
	TextureHandle texture{};
	int32_t width = 0;
	int32_t height = 0;
	VkFormat colorFormat = VK_FORMAT_R8G8B8A8_UNORM;
	std::array<float, 4> clearColor{0.0f, 0.0f, 0.0f, 0.0f};
	bool clearEveryFrame = true;
	std::function<void(const ViewPortRenderContext&)> renderCallback{};
};

struct ViewportImageResource {
#if FLOW_UI_AGENTIC_WORKFLOW
	uint64_t agentic_producer_app_tick = 0;
	uint64_t agentic_producer_window_frame = 0;
	uint64_t agentic_producer_submission_serial = 0;
	uint64_t agentic_content_revision = 0;
	uint64_t agentic_pending_app_tick = 0;
	uint64_t agentic_pending_window_frame = 0;
	uint64_t agentic_pending_revision = 0;
#endif
	storage::ImageHandle image{};
	storage::ImageViewHandle view{};
	VkImage nativeImage = VK_NULL_HANDLE;
	VkImageView nativeView = VK_NULL_HANDLE;
	VkImageLayout layout = VK_IMAGE_LAYOUT_UNDEFINED;
	uint32_t width = 0;
	uint32_t height = 0;
};

struct ViewportFrameCommands {
	VkCommandPool pool = VK_NULL_HANDLE;
	VkCommandBuffer commandBuffer = VK_NULL_HANDLE;
};

struct ViewportTargetGeneration {
	uint64_t generation = 0;
	std::vector<ViewportImageResource> images{};
	std::vector<ViewportFrameCommands> commands{};
	std::vector<TextureHandle> textures{};
};

#if FLOW_UI_DEV_MODE
/** One immutable observation, recorded in its source window's ordinary graphics submission. */
struct ViewportDevCapture {
	ViewportImageResource image{};
	TextureHandle texture{};
	uint64_t source_generation = 0, source_frame = 0, submission = 0;
	uint32_t frame_slot = 0, format = 0;
	bool recorded = false;
};
#endif

struct ViewportRecord {
	ViewportFacadeState state{};
	ViewPort* facadeAddress = nullptr;
	ViewportTargetGeneration active{};
#if FLOW_UI_DEV_MODE
	ViewportDevCapture pending_capture{}, published_capture{};
	bool capture_requested = false, capture_failed = false, capture_release_requested = false;
#endif
#if FLOW_UI_AGENTIC_WORKFLOW
	uint64_t agentic_next_content_revision = 1;
#endif
	uint32_t desiredWidth = 1;
	uint32_t desiredHeight = 1;
	uint32_t resizeHysteresisPixels = 4u;
	bool referencedThisFrame = false;
};

struct RetiredViewportGeneration {
	ViewportTargetGeneration targets{};
};
struct ViewportTextureOwner {
	std::string key{};
	uint32_t frameSlot = 0;
};

class ViewportStorageController {
public:
	ViewportStorageController(storage::IStorageSystem& storageSystem, VulkanContext& context,
							  WindowId owningWindow, uint32_t frameCount);
	~ViewportStorageController() noexcept;

	[[nodiscard]] ViewportTargetGeneration buildTargets(uint32_t width, uint32_t height,
														VkFormat format);
	void discardUnpublished(ViewportTargetGeneration&& generation) noexcept;
	void reserveRetirement();
	void retireTargets(ViewportTargetGeneration&& generation);
	void collectRetired();
#if FLOW_UI_DEV_MODE
	[[nodiscard]] bool supports_dev_capture(VkFormat format) const noexcept;
	[[nodiscard]] ViewportDevCapture create_dev_capture(uint32_t width, uint32_t height,
														VkFormat format);
	/** Queue retirement without allocation; returns false when the bounded retirement queue is
	 * full. */
	[[nodiscard]] bool retire_dev_capture(ViewportDevCapture& capture) noexcept;
	void discard_dev_capture(ViewportDevCapture& capture) noexcept;
	void collect_dev_captures() noexcept;
	[[nodiscard]] uint64_t dev_capture_bytes() const noexcept;
#endif

	::FlowUi::detail::MemoryDemandPeaks capacity_demand{};
	storage::IStorageSystem* storage = nullptr;
	VulkanContext* vk = nullptr;
	WindowId window = InvalidWindowId;
	uint32_t framesInFlight = 1;
	uint32_t currentFrameIndex = 0;
	uint64_t nextGeneration = 1;
	storage::SamplerHandle sampler{};
	ViewPortVulkanInterop interop{};
	std::unordered_map<std::string, ViewportRecord> records{};
	std::unordered_map<std::string, ViewPort> facades{};
	std::unordered_map<uint64_t, ViewportTextureOwner> textureOwners{};
	std::vector<RetiredViewportGeneration> retired{};
#if FLOW_UI_DEV_MODE
	std::vector<ViewportDevCapture> retired_dev_captures{};
#endif

private:
	[[nodiscard]] std::vector<ViewportFrameCommands> createCommands() const;
	void destroyCommands(std::vector<ViewportFrameCommands>& commands) const noexcept;
	[[nodiscard]] ViewportImageResource createImage(uint32_t width, uint32_t height,
													VkFormat format) const;
	void destroyImages(std::vector<ViewportImageResource>& images) const noexcept;
};

} // namespace FlowUi::detail::manager_storage
