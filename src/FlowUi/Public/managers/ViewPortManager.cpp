#define FLOWUI_INTERNAL_VIEWPORT_MANAGER 1
#include "FlowUi/Public/managers/ViewPortManager.hpp"
#if FLOW_UI_DEV_MODE
#include "devSystems/devMonitoringAndReporting/memory/DevContainerMemory.hpp"
#include "devSystems/devMonitoringAndReporting/memory/DevMemorySources.hpp"
#endif

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <stdexcept>
#include <string>
#include <utility>

#include "FlowUi/Private/renderer/vulkan/Vk_Context.hpp"
#include "devSystems/devAgenticWorkflow/frameCapture/AgenticFrameCapture.hpp"
#include "devSystems/devMonitoringAndReporting/timing/DevTimingZone.hpp"
#include "FlowUi/Private/storageSystem/ManagerStateAccess.hpp"
#include "FlowUi/Private/storageSystem/ResourceKeyNormalization.hpp"
#include "FlowUi/Private/renderer/ViewportStorageController.hpp"
#if FLOW_UI_DEV_MODE
#include "devSystems/devMonitoringAndReporting/timing/DevGpuTiming.hpp"
#endif

namespace {

using FlowUi::detail::manager_storage::ViewportRecordFlag;

void vkCheck(VkResult result) {
	if (result == VK_SUCCESS) return;
	FlowUi::ErrorCode code = result == VK_ERROR_DEVICE_LOST
		? FlowUi::ErrorCode::VulkanDeviceLost
		: FlowUi::ErrorCode::VulkanNativeCallFailed;
	if (result == VK_ERROR_OUT_OF_HOST_MEMORY || result == VK_ERROR_OUT_OF_DEVICE_MEMORY) {
		code = FlowUi::ErrorCode::AllocationFailed;
	}
	throw FlowUi::FlowUiException(FlowUi::makeError(
		code, FlowUi::ErrorSite::ViewportRecord, 0u, 0u,
		static_cast<std::uint32_t>(result)));
}

void transitionViewportImageLayout(
	VkCommandBuffer commandBuffer, VkImage image, VkImageLayout oldLayout, VkImageLayout newLayout) {
	if (oldLayout == newLayout || !image || !commandBuffer) return;
	VkPipelineStageFlags srcStage = VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT;
	VkPipelineStageFlags dstStage = VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT;
	VkAccessFlags srcAccess = 0, dstAccess = 0;
	if (oldLayout == VK_IMAGE_LAYOUT_UNDEFINED && newLayout == VK_IMAGE_LAYOUT_ATTACHMENT_OPTIMAL) {
		dstStage = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
		dstAccess = VK_ACCESS_COLOR_ATTACHMENT_READ_BIT | VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
	} else if (oldLayout == VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL && newLayout == VK_IMAGE_LAYOUT_ATTACHMENT_OPTIMAL) {
		srcStage = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
		dstStage = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
		srcAccess = VK_ACCESS_SHADER_READ_BIT;
		dstAccess = VK_ACCESS_COLOR_ATTACHMENT_READ_BIT | VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
	} else if (oldLayout == VK_IMAGE_LAYOUT_ATTACHMENT_OPTIMAL && newLayout == VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL) {
		srcStage = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
		dstStage = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
		srcAccess = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
		dstAccess = VK_ACCESS_SHADER_READ_BIT;
	} else if (oldLayout == VK_IMAGE_LAYOUT_ATTACHMENT_OPTIMAL &&
			   newLayout == VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL) {
		srcStage = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
		dstStage = VK_PIPELINE_STAGE_TRANSFER_BIT;
		srcAccess = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
		dstAccess = VK_ACCESS_TRANSFER_READ_BIT;
	} else if (oldLayout == VK_IMAGE_LAYOUT_UNDEFINED &&
			   newLayout == VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL) {
		dstStage = VK_PIPELINE_STAGE_TRANSFER_BIT;
		dstAccess = VK_ACCESS_TRANSFER_WRITE_BIT;
	} else if (oldLayout == VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL &&
			   newLayout == VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL) {
		srcStage = VK_PIPELINE_STAGE_TRANSFER_BIT;
		dstStage = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
		srcAccess = VK_ACCESS_TRANSFER_READ_BIT;
		dstAccess = VK_ACCESS_SHADER_READ_BIT;
	} else if (oldLayout == VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL &&
			   newLayout == VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL) {
		srcStage = VK_PIPELINE_STAGE_TRANSFER_BIT;
		dstStage = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
		srcAccess = VK_ACCESS_TRANSFER_WRITE_BIT;
		dstAccess = VK_ACCESS_SHADER_READ_BIT;
	} else {
		throw FlowUi::FlowUiException(FlowUi::makeError(FlowUi::ErrorCode::ViewportRecordingFailed, FlowUi::ErrorSite::ViewportRecord));
	}
	VkImageMemoryBarrier barrier{VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER};
	barrier.oldLayout = oldLayout;
	barrier.newLayout = newLayout;
	barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
	barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
	barrier.image = image;
	barrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
	barrier.subresourceRange.levelCount = 1;
	barrier.subresourceRange.layerCount = 1;
	barrier.srcAccessMask = srcAccess;
	barrier.dstAccessMask = dstAccess;
	vkCmdPipelineBarrier(commandBuffer, srcStage, dstStage, 0, 0, nullptr, 0, nullptr, 1, &barrier);
}

} // namespace

namespace FlowUi {
#if FLOW_UI_DEV_MODE
void ViewPortManager::visitDevViewports(void* target, DevViewportVisitor visitor) const {
	if (!controller_ || !visitor)
		return;
	for (const auto& [key, record] : controller_->records) {
		const auto& pending = record.pending_capture;
		const auto& capture =
			pending.submission && pending.submission <= storage_->completedSerial()
				? pending
				: record.published_capture;
		const bool available = capture.texture &&
			!record.flags.has_flag(ViewportRecordFlag::capture_release_requested);
		const DevViewportView view{
			.key = key,
			.capture_texture = available ? capture.texture : TextureHandle{},
			.generation = record.active.generation,
			.captured_frame = available ? capture.source_frame : 0,
			.captured_generation = available ? capture.source_generation : 0,
			.captured_submission = available ? capture.submission : 0,
			.width = static_cast<uint32_t>(std::max(0, record.state.width)),
			.height = static_cast<uint32_t>(std::max(0, record.state.height)),
			.format = static_cast<uint32_t>(record.state.colorFormat),
			.captured_width = available ? capture.image.width : 0,
			.captured_height = available ? capture.image.height : 0,
			.captured_format = available ? capture.format : 0,
			.has_callback = static_cast<bool>(record.state.renderCallback),
			.clear_every_frame = record.state.clearEveryFrame,
			.capture_supported = controller_->supports_dev_capture(record.state.colorFormat),
			.capture_pending =
				record.flags.has_flag(ViewportRecordFlag::capture_requested) ||
				(pending.texture &&
				 (!pending.submission || pending.submission > storage_->completedSerial())),
			.capture_failed = record.flags.has_flag(ViewportRecordFlag::capture_failed)};
		if (!visitor(target, view))
			break;
	}
}

Status ViewPortManager::request_dev_capture(std::string_view key) noexcept {
	if (!controller_ || !storage_)
		return unexpectedError(
			makeError(ErrorCode::ObjectNotInitialized, ErrorSite::ViewportPrepare));
	try {
		controller_->collect_dev_captures();
		const auto found = controller_->records.find(std::string(key));
		if (found == controller_->records.end())
			return unexpectedError(
				makeError(ErrorCode::ViewportNotFound, ErrorSite::ViewportPrepare));
		auto& record = found->second;
		if (!controller_->supports_dev_capture(record.state.colorFormat)) {
			record.flags.set_flag(ViewportRecordFlag::capture_failed, true);
			return unexpectedError(
				makeError(ErrorCode::ViewportConfigurationInvalid, ErrorSite::ViewportPrepare));
		}
		const auto active_count = std::ranges::count_if(controller_->records, [](const auto& item) {
			const auto& candidate = item.second;
			return candidate.flags.has_flag(ViewportRecordFlag::capture_requested) ||
				   candidate.pending_capture.texture ||
				   candidate.published_capture.texture;
		});
		if (active_count >= 16 && !record.flags.has_flag(ViewportRecordFlag::capture_requested) &&
			!record.pending_capture.texture &&
			!record.published_capture.texture)
			return unexpectedError(
				makeError(ErrorCode::StorageCapacityExceeded, ErrorSite::ViewportPrepare));
		if (controller_->retired_dev_captures.capacity() < 64)
			controller_->retired_dev_captures.reserve(64);
		record.flags.set_flag(ViewportRecordFlag::capture_release_requested, false);
		record.flags.set_flag(ViewportRecordFlag::capture_failed, false);
		if (!record.pending_capture.texture)
			record.flags.set_flag(ViewportRecordFlag::capture_requested, true);
		return {};
	} catch (const FlowUiException& exception) {
		return unexpectedError(exception.error());
	} catch (...) {
		return unexpectedError(makeError(ErrorCode::AllocationFailed, ErrorSite::ViewportPrepare));
	}
}

void ViewPortManager::release_dev_capture(std::string_view key) noexcept {
	if (!controller_)
		return;
	try {
		const auto found = controller_->records.find(std::string(key));
		if (found == controller_->records.end())
			return;
		found->second.flags.set_flag(ViewportRecordFlag::capture_requested, false);
		found->second.flags.set_flag(ViewportRecordFlag::capture_release_requested, true);
		controller_->collect_dev_captures();
	} catch (...) {}
}

Result<TextureHandle> ViewPortManager::retain_dev_capture(std::string_view key,
														  TextureHandle expected_capture) noexcept {
	if (!controller_ || !storage_)
		return unexpectedError(
			makeError(ErrorCode::ObjectNotInitialized, ErrorSite::ViewportPublish));
	try {
		const auto found = controller_->records.find(std::string(key));
		if (found == controller_->records.end())
			return unexpectedError(
				makeError(ErrorCode::ViewportNotFound, ErrorSite::ViewportPublish));
		const auto& record = found->second;
		const auto& pending = record.pending_capture;
		const auto& capture =
			pending.texture == expected_capture ? pending : record.published_capture;
		if (!capture.texture || capture.texture != expected_capture || !capture.submission ||
			capture.submission > storage_->completedSerial())
			return unexpectedError(
				makeError(ErrorCode::ResourceHandleInvalid, ErrorSite::ViewportPublish));
		return storage_->createAnonymousTexture(detail::storage::TextureViewDesc{
			.imageView = capture.image.view,
			.sampler = controller_->sampler,
			.sourceWidth = static_cast<int32_t>(capture.image.width),
			.sourceHeight = static_cast<int32_t>(capture.image.height)});
	} catch (const FlowUiException& exception) {
		return unexpectedError(exception.error());
	} catch (...) {
		return unexpectedError(makeError(ErrorCode::AllocationFailed, ErrorSite::ViewportPublish));
	}
}

void ViewPortManager::prepare_dev_captures(const detail::storage::FrameToken& frame) noexcept {
	if (!controller_)
		return;
	for (auto& [key, record] : controller_->records) {
		if (!record.flags.has_flag(ViewportRecordFlag::capture_requested) ||
			!record.flags.has_flag(ViewportRecordFlag::referenced_this_frame) ||
			record.pending_capture.texture)
			continue;
		try {
			const auto& source = record.active.images[controller_->currentFrameIndex];
			auto capture = controller_->create_dev_capture(source.width, source.height,
														   record.state.colorFormat);
			capture.source_generation = record.active.generation;
			capture.source_frame = frame.frameNumber;
			capture.frame_slot = frame.frameSlot;
			try {
				storage_->trackUse(frame, capture.image.image);
			} catch (...) {
				controller_->discard_dev_capture(capture);
				throw;
			}
			record.pending_capture = capture;
			record.flags.set_flag(ViewportRecordFlag::capture_requested, false);
		} catch (...) {
			record.flags.set_flag(ViewportRecordFlag::capture_failed, true);
			record.flags.set_flag(ViewportRecordFlag::capture_requested, false);
		}
	}
}

void ViewPortManager::mark_dev_captures_submitted(uint64_t serial) noexcept {
	if (!controller_)
		return;
	for (auto& [key, record] : controller_->records) {
		auto& pending = record.pending_capture;
		if (pending.recorded && !pending.submission &&
			pending.frame_slot == controller_->currentFrameIndex)
			pending.submission = serial;
	}
}
void ViewPortManager::collect_dev_captures() noexcept {
	if (controller_)
		controller_->collect_dev_captures();
}
#endif
#if FLOW_UI_DEV_MODE && FLOWUI_DEV_MEMORY_LEVEL >= 2
void ViewPortManager::appendDevMemorySamples(devSystems::MemorySampleSink& sink) const noexcept {
	if (!controller_) return;
	try {
		devSystems::DevContainerMemoryAccumulator memory{};
		memory.addNodeContainer(controller_->records);
		memory.addNodeContainer(controller_->facades);
		memory.addNodeContainer(controller_->textureOwners);
		memory.add(controller_->retired);
		devSystems::append_capacity_demand(
			sink, FlowUi::detail::InitialCapacityTarget::viewports_records, controller_->records,
			controller_->capacity_demand, windowId_);
		devSystems::append_capacity_demand(
			sink, FlowUi::detail::InitialCapacityTarget::viewports_facades, controller_->facades,
			controller_->capacity_demand, windowId_);
		devSystems::append_capacity_demand(
			sink, FlowUi::detail::InitialCapacityTarget::viewports_texture_owners,
			controller_->textureOwners, controller_->capacity_demand, windowId_);
		devSystems::append_capacity_demand(
			sink, FlowUi::detail::InitialCapacityTarget::viewports_retired, controller_->retired,
			controller_->capacity_demand, windowId_);

		for (const auto& [_, record] : controller_->records) {
			memory.add(record.state.key);
			memory.add(record.active.images);
			memory.add(record.active.commands);
			memory.add(record.active.textures);
		}
		devSystems::appendManagerSample(sink, devSystems::memory_sources::kViewports.id, memory, windowId_);
	} catch (...) {}
}
#endif

namespace manager_storage = detail::manager_storage;
namespace key_storage = detail::managerStorage;
namespace storage = detail::storage;

static void ensureRenderTargetSize(
	manager_storage::ViewportStorageController& controller,
	storage::IStorageSystem& storageSystem,
	WindowId window,
	manager_storage::ViewportRecord& record);

static storage::ResourceKey viewportKey(
	storage::IStorageSystem& storageSystem, ResourceKey key, WindowId window) {
	return key_storage::normalizeResourceKey(
		storageSystem, key, ResourceDomain::Viewport,
		key_storage::ResourceScope::WindowLocal, window);
}

std::string_view ViewPort::getKey() const noexcept {
	return state_ ? std::string_view(state_->key) : std::string_view{};
}
bool ViewPort::hasValidSize() const noexcept {
	return state_ && state_->width > 0 && state_->height > 0;
}
VkExtent2D ViewPort::getSize() const noexcept {
	return state_ ? VkExtent2D{
		static_cast<uint32_t>(std::max(0, state_->width)),
		static_cast<uint32_t>(std::max(0, state_->height))} : VkExtent2D{};
}
TextureRef ViewPort::textureRef() const noexcept {
	return state_ ? TextureRef{
		.handle = state_->texture, .sourceWidth = state_->width, .sourceHeight = state_->height} : TextureRef{};
}
void ViewPort::setRenderCallback(RenderCallback callback) {
	if (!state_) throw FlowUiException(makeError(ErrorCode::ViewportDetached, ErrorSite::ViewportConfigure));
	state_->renderCallback = std::move(callback);
	state_->storage->noteManagerMutation(state_->window);
}
void ViewPort::clearRenderCallback() { setRenderCallback({}); }
bool ViewPort::hasRenderCallback() const noexcept {
	return state_ && static_cast<bool>(state_->renderCallback);
}
void ViewPort::setClearColor(float r, float g, float b, float a) {
	if (!state_) throw FlowUiException(makeError(ErrorCode::ViewportDetached, ErrorSite::ViewportConfigure));
	state_->clearColor = {r, g, b, a};
	state_->storage->noteManagerMutation(state_->window);
}
std::array<float, 4> ViewPort::clearColor() const noexcept {
	return state_ ? state_->clearColor : std::array<float, 4>{0, 0, 0, 0};
}
void ViewPort::setClearEveryFrame(bool enabled) {
	if (!state_) throw FlowUiException(makeError(ErrorCode::ViewportDetached, ErrorSite::ViewportConfigure));
	state_->clearEveryFrame = enabled;
	state_->storage->noteManagerMutation(state_->window);
}
bool ViewPort::clearEveryFrame() const noexcept {
	return !state_ || state_->clearEveryFrame;
}

void ViewPortManager::init(
	storage::IStorageSystem& storageSystem,
	VulkanContext& vk,
	WindowId window,
	uint32_t framesInFlight,
	MissingVisualPolicy missingPolicy) {
	destroyDrained(vk);
	if (!window || vk.device == VK_NULL_HANDLE || vk.graphicsQFamily == UINT32_MAX || vk.graphicsQ == VK_NULL_HANDLE) {
		throw FlowUiException(makeError(
			!window ? ErrorCode::InvalidWindowId : ErrorCode::ObjectNotInitialized,
			ErrorSite::ViewportManagerInitialize,
			window));
	}
	const storage::StringId name = storageSystem.intern("flowui.viewport.controller");
	const storage::ResourceKey key{storage::ResourceDomain::Viewport, name, window};
	const storage::ManagerRecordHandle handle = manager_storage::createState<manager_storage::ViewportStorageController>(
		storageSystem, key, storage::ResourceKind::Viewport, name,
		std::ref(storageSystem), std::ref(vk), window, framesInFlight);
	storage_ = &storageSystem;
	missingPolicy_ = missingPolicy;
	windowId_ = window;
	controllerHandle_ = handle.packed();
	controller_ = manager_storage::state<manager_storage::ViewportStorageController>(
		storage_, handle, storage::ResourceKind::Viewport);
	if (!controller_) {
		destroyDrained(vk);
		throw FlowUiException(makeError(ErrorCode::ResourcePublicationFailed, ErrorSite::ViewportManagerInitialize));
	}
}

Result<bool> ViewPortManager::create(std::string_view key, const ViewPortCreateInfo& createInfo) {
	return create(ResourceKey{.name = key}, createInfo);
}

Result<bool> ViewPortManager::create(ResourceKey key, const ViewPortCreateInfo& createInfo) {
	if (!controller_ || !storage_) return unexpectedError(makeError(ErrorCode::ObjectNotInitialized, ErrorSite::ViewportCreate));
	try {
		(void)viewportKey(*storage_, key, windowId_);
	} catch (const FlowUiException& exception) {
		return unexpectedError(exception.error());
	}
	const std::string keyString(key.name);
	if (controller_->records.contains(keyString)) {
		return unexpectedError(makeError(ErrorCode::ViewportKeyCollision, ErrorSite::ViewportCreate));
	}
	const VkFormat format = createInfo.colorFormat == VK_FORMAT_UNDEFINED
		? VK_FORMAT_R8G8B8A8_UNORM : createInfo.colorFormat;
	manager_storage::ViewportTargetGeneration targets{};
	try {
		targets = controller_->buildTargets(1, 1, format);
	} catch (const std::bad_alloc&) {
		throw;
	} catch (const FlowUiException& exception) {
		if (exception.error().descriptor().category == ErrorCategory::Fatal) {
			detail::terminateForFatalError(exception.error());
		}
		return unexpectedError(exception.error());
	} catch (...) {
		return unexpectedError(makeError(ErrorCode::ViewportConfigurationInvalid, ErrorSite::ViewportCreate));
	}
	bool facadeInserted = false;
	try {
		auto [facadeIt, inserted] = controller_->facades.emplace(keyString, ViewPort{});
		if (!inserted) throw FlowUiException(makeError(ErrorCode::InternalInvariantBroken, ErrorSite::ViewportCreate));
		facadeInserted = true;
		manager_storage::ViewportRecord record{};
		record.state = manager_storage::ViewportFacadeState{
			.storage = storage_, .window = windowId_, .key = keyString,
			.width = 1, .height = 1, .colorFormat = format,
			.clearColor = createInfo.clearColor, .clearEveryFrame = createInfo.clearEveryFrame,
		};
		record.resizeHysteresisPixels = createInfo.resizeHysteresisPixels;
		record.active = std::move(targets);
		auto [recordIt, recordInserted] = controller_->records.emplace(keyString, std::move(record));
		if (!recordInserted) throw FlowUiException(makeError(ErrorCode::InternalInvariantBroken, ErrorSite::ViewportCreate));
		facadeIt->second.state_ = &recordIt->second.state;
		recordIt->second.facadeAddress = &facadeIt->second;
		const uint32_t activeSlot = controller_->currentFrameIndex % recordIt->second.active.textures.size();
		recordIt->second.state.texture = recordIt->second.active.textures[activeSlot];
		for (uint32_t slot = 0; slot < recordIt->second.active.textures.size(); ++slot) {
			controller_->textureOwners.emplace(
				recordIt->second.active.textures[slot].packed(),
				manager_storage::ViewportTextureOwner{keyString, slot});
		}
		storage_->clearDiagnosticMark(viewportKey(*storage_, key, windowId_), 1u);
		storage_->noteManagerMutation(windowId_);
		return true;
	} catch (const std::bad_alloc&) {
		if (facadeInserted) controller_->facades.erase(keyString);
		if (const auto it = controller_->records.find(keyString); it != controller_->records.end()) {
			targets = std::move(it->second.active);
			controller_->records.erase(it);
		}
		for (TextureHandle texture : targets.textures) controller_->textureOwners.erase(texture.packed());
		controller_->discardUnpublished(std::move(targets));
		throw;
	} catch (const FlowUiException& exception) {
		if (facadeInserted) controller_->facades.erase(keyString);
		if (const auto it = controller_->records.find(keyString); it != controller_->records.end()) {
			targets = std::move(it->second.active);
			controller_->records.erase(it);
		}
		for (TextureHandle texture : targets.textures) controller_->textureOwners.erase(texture.packed());
		controller_->discardUnpublished(std::move(targets));
		if (exception.error().descriptor().category == ErrorCategory::Fatal) {
			detail::terminateForFatalError(exception.error());
		}
		return unexpectedError(exception.error());
	} catch (...) {
		if (facadeInserted) controller_->facades.erase(keyString);
		if (const auto it = controller_->records.find(keyString); it != controller_->records.end()) {
			targets = std::move(it->second.active);
			controller_->records.erase(it);
		}
		for (TextureHandle texture : targets.textures) controller_->textureOwners.erase(texture.packed());
		controller_->discardUnpublished(std::move(targets));
		return unexpectedError(makeError(ErrorCode::ResourcePublicationFailed, ErrorSite::ViewportCreate));
	}
}

Result<bool> ViewPortManager::remove(std::string_view key) { return remove(ResourceKey{.name = key}); }
Result<bool> ViewPortManager::remove(ResourceKey key) {
	if (!controller_ || !storage_) return unexpectedError(makeError(ErrorCode::ObjectNotInitialized, ErrorSite::ViewportDestroy));
	try {
		(void)viewportKey(*storage_, key, windowId_);
	} catch (const FlowUiException& exception) {
		return unexpectedError(exception.error());
	}
	const std::string keyString(key.name);
	const auto it = controller_->records.find(keyString);
	if (it == controller_->records.end()) return false;
#if FLOW_UI_DEV_MODE
	if (it->second.pending_capture.texture && !it->second.pending_capture.submission)
		return unexpectedError(
			makeError(ErrorCode::FramePhaseViolation, ErrorSite::ViewportDestroy));
	if (!controller_->retire_dev_capture(it->second.pending_capture) ||
		!controller_->retire_dev_capture(it->second.published_capture))
		return unexpectedError(
			makeError(ErrorCode::StorageCapacityExceeded, ErrorSite::ViewportDestroy));
#endif
	controller_->reserveRetirement();
	if (it->second.facadeAddress) it->second.facadeAddress->state_ = nullptr;
	controller_->retireTargets(std::move(it->second.active));
	controller_->records.erase(it);
	controller_->facades.erase(keyString);
	storage_->noteManagerMutation(windowId_);
	return true;
}

bool ViewPortManager::contains(std::string_view key) const { return contains(ResourceKey{.name = key}); }
bool ViewPortManager::contains(ResourceKey key) const {
	if (!controller_ || !storage_) return false;
	(void)viewportKey(*storage_, key, windowId_);
	return controller_->records.contains(std::string(key.name));
}
ViewPort* ViewPortManager::getViewPort(std::string_view key) { return getViewPort(ResourceKey{.name = key}); }
ViewPort* ViewPortManager::getViewPort(ResourceKey key) {
	if (!controller_ || !storage_) return nullptr;
	(void)viewportKey(*storage_, key, windowId_);
	const auto it = controller_->facades.find(std::string(key.name));
	return it == controller_->facades.end() ? nullptr : &it->second;
}
const ViewPort* ViewPortManager::getViewPort(std::string_view key) const {
	return getViewPort(ResourceKey{.name = key});
}
const ViewPort* ViewPortManager::getViewPort(ResourceKey key) const {
	if (!controller_ || !storage_) return nullptr;
	(void)viewportKey(*storage_, key, windowId_);
	const auto it = controller_->facades.find(std::string(key.name));
	return it == controller_->facades.end() ? nullptr : &it->second;
}
TextureRef ViewPortManager::getTexture(std::string_view key) const {
	return getTexture(ResourceKey{.name = key});
}
TextureRef ViewPortManager::getTexture(ResourceKey key) const {
	if (!controller_ || !storage_) return {};
	const storage::ResourceKey normalized = viewportKey(*storage_, key, windowId_);
	const auto it = controller_->records.find(std::string(key.name));
	if (it == controller_->records.end()) {
		if (storage_->markDiagnosticOnce(normalized, 1u)) {
			detail::reportErrorEvent(ErrorEventView{
				.error = makeError(
					ErrorCode::ViewportNotFound, ErrorSite::ViewportLookup,
					normalized.name),
				.kind = ErrorEventKind::Resolved,
				.resolution = missingPolicy_ == MissingVisualPolicy::SkipVisual
					? ErrorResolution::Skipped
					: ErrorResolution::UsedFallback,
			});
		}
		return TextureRef{
			.skipIfUnavailable = missingPolicy_ == MissingVisualPolicy::SkipVisual,
		};
	}
	return it->second.facadeAddress ? it->second.facadeAddress->textureRef() : TextureRef{};
}

const ViewPortVulkanInterop& ViewPortManager::getVulkanInterop() const {
	if (!controller_) throw FlowUiException(makeError(ErrorCode::ObjectNotInitialized, ErrorSite::ViewportLookup));
	return controller_->interop;
}

void ViewPortManager::onFrameStart(VulkanContext&, uint32_t frameIndex) {
	if (!controller_) return;
#if FLOW_UI_DEV_MODE
	for (auto& [key, record] : controller_->records)
		if (record.pending_capture.texture && !record.pending_capture.submission)
			controller_->discard_dev_capture(record.pending_capture);
	controller_->collect_dev_captures();
#endif
	controller_->currentFrameIndex = frameIndex % controller_->framesInFlight;
	controller_->collectRetired();
	for (auto& [_, record] : controller_->records) {
		if (record.active.textures.empty()) continue;
		const uint32_t slot = controller_->currentFrameIndex % record.active.textures.size();
		record.state.texture = record.active.textures[slot];
		record.state.width = static_cast<int32_t>(record.active.images[slot].width);
		record.state.height = static_cast<int32_t>(record.active.images[slot].height);
	}
}

void ViewPortManager::resetFrameTracking() {
	for (auto& [_, record] : controller_->records) {
		record.flags.set_flag(ViewportRecordFlag::referenced_this_frame, false);
		record.desiredWidth = 1;
		record.desiredHeight = 1;
	}
}

void ViewPortManager::prepareFrameTargets(
	Clay_RenderCommandArray& commands, float scaleX, float scaleY) {
	if (!controller_) throw FlowUiException(makeError(ErrorCode::ObjectNotInitialized, ErrorSite::ViewportPrepare));
	resetFrameTracking();
	const float sx = std::max(scaleX, 1.0e-6f);
	const float sy = std::max(scaleY, 1.0e-6f);

	struct ReferencedCommand {
		TextureRef* texture = nullptr;
		manager_storage::ViewportRecord* viewport = nullptr;
	};
	std::vector<ReferencedCommand> referencedCommands{};
	referencedCommands.reserve(static_cast<size_t>(std::max(0, commands.length)));

	for (int32_t i = 0; i < commands.length; ++i) {
		Clay_RenderCommand& command = commands.internalArray[i];
		if (command.commandType != CLAY_RENDER_COMMAND_TYPE_IMAGE) continue;
		auto* ref = reinterpret_cast<TextureRef*>(command.renderData.image.imageData);
		if (!ref || !ref->handle) continue;
		const auto owner = controller_->textureOwners.find(ref->handle.packed());
		if (owner == controller_->textureOwners.end()) continue;
		const auto record = controller_->records.find(owner->second.key);
		if (record == controller_->records.end()) continue;

		if (!record->second.flags.has_flag(ViewportRecordFlag::referenced_this_frame)) {
			record->second.flags.set_flag(ViewportRecordFlag::referenced_this_frame, true);
			record->second.desiredWidth = 0;
			record->second.desiredHeight = 0;
		}

		constexpr uint32_t kSizeQuantStep = 8u;
		auto quantize = [](uint32_t v) -> uint32_t {
			return ((v + kSizeQuantStep - 1u) / kSizeQuantStep) * kSizeQuantStep;
		};
		const uint32_t rawWidth = std::max(1u, static_cast<uint32_t>(std::ceil(std::max(0.0f, command.boundingBox.width * sx))));
		const uint32_t rawHeight = std::max(1u, static_cast<uint32_t>(std::ceil(std::max(0.0f, command.boundingBox.height * sy))));
		record->second.desiredWidth = std::max(record->second.desiredWidth, quantize(rawWidth));
		record->second.desiredHeight = std::max(record->second.desiredHeight, quantize(rawHeight));

		referencedCommands.emplace_back(ReferencedCommand{ref, &record->second});
	}

	for (auto& [_, record] : controller_->records) {
		if (!record.flags.has_flag(ViewportRecordFlag::referenced_this_frame))
			continue;
		if (!record.active.images.empty()) {
			constexpr uint32_t kDefaultHysteresisPixels = 4u;
			const uint32_t hysteresis = std::max(record.resizeHysteresisPixels, kDefaultHysteresisPixels);
			const uint32_t currentWidth = record.active.images.front().width;
			const uint32_t currentHeight = record.active.images.front().height;
			const uint32_t widthDiff = currentWidth > record.desiredWidth
				? currentWidth - record.desiredWidth : record.desiredWidth - currentWidth;
			const uint32_t heightDiff = currentHeight > record.desiredHeight
				? currentHeight - record.desiredHeight : record.desiredHeight - currentHeight;
			if (widthDiff <= hysteresis) record.desiredWidth = currentWidth;
			if (heightDiff <= hysteresis) record.desiredHeight = currentHeight;
		}
		ensureRenderTargetSize(*controller_, *storage_, windowId_, record);
	}

	const uint32_t frameSlot = controller_->currentFrameIndex % controller_->framesInFlight;
	for (const ReferencedCommand& command : referencedCommands) {
		if (!command.texture || !command.viewport || command.viewport->active.textures.empty()) continue;
		const uint32_t slot = frameSlot % command.viewport->active.textures.size();
		command.texture->handle = command.viewport->active.textures[slot];
		command.texture->sourceWidth = static_cast<int32_t>(command.viewport->active.images[slot].width);
		command.texture->sourceHeight = static_cast<int32_t>(command.viewport->active.images[slot].height);
	}
}

void ViewPortManager::remapRenderCommandsForFrame(Clay_RenderCommandArray& commands, uint32_t frameIndex) {
	if (!controller_) return;
	const uint32_t frameSlot = frameIndex % controller_->framesInFlight;
	for (int32_t i = 0; i < commands.length; ++i) {
		Clay_RenderCommand& command = commands.internalArray[i];
		if (command.commandType != CLAY_RENDER_COMMAND_TYPE_IMAGE) continue;
		auto* ref = reinterpret_cast<TextureRef*>(command.renderData.image.imageData);
		if (!ref || !ref->handle) continue;
		const auto owner = controller_->textureOwners.find(ref->handle.packed());
		if (owner == controller_->textureOwners.end()) continue;
		const auto record = controller_->records.find(owner->second.key);
		if (record == controller_->records.end() || record->second.active.textures.empty()) continue;
		const uint32_t slot = frameSlot % record->second.active.textures.size();
		ref->handle = record->second.active.textures[slot];
		ref->sourceWidth = static_cast<int32_t>(record->second.active.images[slot].width);
		ref->sourceHeight = static_cast<int32_t>(record->second.active.images[slot].height);
	}
}

bool ViewPortManager::resizeRequired() const {
	if (!controller_) return false;
	for (const auto& [_, record] : controller_->records) {
		if (!record.flags.has_flag(ViewportRecordFlag::referenced_this_frame))
			continue;
		if (record.active.images.empty() || record.active.images.front().width != record.desiredWidth ||
			record.active.images.front().height != record.desiredHeight) return true;
	}
	return false;
}

static void ensureRenderTargetSize(
	manager_storage::ViewportStorageController& controller,
	storage::IStorageSystem& storageSystem,
	WindowId window,
	manager_storage::ViewportRecord& record) {
	const uint32_t width = std::max(1u, record.desiredWidth);
	const uint32_t height = std::max(1u, record.desiredHeight);
	if (!record.active.images.empty() && record.active.images.front().width == width &&
		record.active.images.front().height == height) return;
	manager_storage::ViewportTargetGeneration candidate = controller.buildTargets(width, height, record.state.colorFormat);
	controller.reserveRetirement();
	manager_storage::ViewportTargetGeneration previous = std::move(record.active);
	record.active = std::move(candidate);
	for (uint32_t slot = 0; slot < record.active.textures.size(); ++slot) {
		controller.textureOwners.emplace(record.active.textures[slot].packed(),
			manager_storage::ViewportTextureOwner{record.state.key, slot});
	}
	const uint32_t slot = controller.currentFrameIndex % record.active.textures.size();
	record.state.texture = record.active.textures[slot];
	record.state.width = static_cast<int32_t>(width);
	record.state.height = static_cast<int32_t>(height);
	controller.retireTargets(std::move(previous));
	storageSystem.noteManagerMutation(window);
}

#if FLOW_UI_AGENTIC_WORKFLOW
void ViewPortManager::set_agentic_frame_context(uint64_t app_tick, uint64_t window_frame) noexcept {
	if (!controller_ || !controller_->vk ||
		!devSystems::agentic::frame_capture::requested(*controller_->vk))
		return;
	agentic_app_tick_ = app_tick;
	agentic_window_frame_ = window_frame;
	for (auto& entry : controller_->records)
		for (auto& image : entry.second.active.images)
			if (image.agentic_pending_window_frame != window_frame) {
				image.agentic_pending_app_tick = 0;
				image.agentic_pending_window_frame = 0;
				image.agentic_pending_revision = 0;
			}
}

void ViewPortManager::mark_agentic_submitted(uint64_t submission_serial) noexcept {
	if (!controller_ || !agentic_window_frame_)
		return;
	for (auto& entry : controller_->records)
		for (auto& image : entry.second.active.images)
			if (image.agentic_pending_revision &&
				image.agentic_pending_window_frame == agentic_window_frame_) {
				image.agentic_producer_app_tick = image.agentic_pending_app_tick;
				image.agentic_producer_window_frame = image.agentic_pending_window_frame;
				image.agentic_producer_submission_serial = submission_serial;
				image.agentic_content_revision = image.agentic_pending_revision;
				image.agentic_pending_revision = 0;
			}
}
#endif

void ViewPortManager::recordFramePasses(
	VulkanContext& vk, VkCommandBuffer primary, uint32_t frameIndex
#if FLOW_UI_DEV_MODE
	, devSystems::DevTimingRecorder* timingRecorder,
	devSystems::GpuTimingCommandContext* gpuTiming
#endif
) {
#if FLOW_UI_DEV_MODE
	FLOWUI_DEV_TIMING_ZONE_IF(
		timingRecorder, devSystems::TimingCategory::RendererCpu,
		devSystems::TimingZoneRole::Work, "flowui.viewport.record_all");
#endif
	if (!controller_ || !primary || !vk.device) return;
	const uint32_t slot = frameIndex % controller_->framesInFlight;
	for (auto& [key, record] : controller_->records) {
		if (!record.flags.has_flag(ViewportRecordFlag::referenced_this_frame)) continue;
#if FLOW_UI_DEV_MODE
		devSystems::GpuCommandTimingZone gpuZone(
			gpuTiming, primary, devSystems::gpu_timing_zones::kViewportPass,
			devSystems::TimingEntityRef{
				.kind = devSystems::TimingEntityKind::Viewport,
				.primaryId = std::hash<std::string_view>{}(key),
			});
#endif
		FLOWUI_DEV_TIMING_ZONE_BALANCED_IF(
			timingRecorder, devSystems::TimingCategory::RendererCpu,
			devSystems::TimingZoneRole::Work, "flowui.viewport.record");
		if (slot >= record.active.commands.size() || slot >= record.active.images.size()) {
			detail::terminateForFatalError(makeError(
				ErrorCode::ViewportGenerationIncomplete, ErrorSite::ViewportRecord,
				std::hash<std::string_view>{}(key)));
		}
		manager_storage::ViewportFrameCommands& frame = record.active.commands[slot];
		manager_storage::ViewportImageResource& image = record.active.images[slot];
		vkCheck(vkResetCommandPool(vk.device, frame.pool, 0));
		VkCommandBufferInheritanceRenderingInfo inheritanceRendering{VK_STRUCTURE_TYPE_COMMAND_BUFFER_INHERITANCE_RENDERING_INFO};
		inheritanceRendering.colorAttachmentCount = 1;
		inheritanceRendering.pColorAttachmentFormats = &record.state.colorFormat;
		inheritanceRendering.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;
		VkCommandBufferInheritanceInfo inheritance{VK_STRUCTURE_TYPE_COMMAND_BUFFER_INHERITANCE_INFO};
		inheritance.pNext = &inheritanceRendering;
		VkCommandBufferBeginInfo begin{VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO};
		begin.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT | VK_COMMAND_BUFFER_USAGE_RENDER_PASS_CONTINUE_BIT;
		begin.pInheritanceInfo = &inheritance;
		vkCheck(vkBeginCommandBuffer(frame.commandBuffer, &begin));
		if (record.state.renderCallback) {
			const ViewPortRenderContext context{
				.commandBuffer = frame.commandBuffer, .extent = {image.width, image.height},
				.colorFormat = record.state.colorFormat, .frameIndex = slot,
				.key = record.state.key, .vulkan = &controller_->interop,
			};
			FLOWUI_DEV_TIMING_ZONE_BALANCED_IF(
				timingRecorder, devSystems::TimingCategory::User,
				devSystems::TimingZoneRole::Work, "flowui.viewport.callback");
			record.state.renderCallback(context);
		}
		vkCheck(vkEndCommandBuffer(frame.commandBuffer));
		const VkImageLayout previousLayout = image.layout;
#if FLOW_UI_AGENTIC_WORKFLOW
		// LOAD without a callback preserves a prior producer's content even though the
		// viewport is sampled by this window frame. Commit new identities on submit only.
		if (agentic_window_frame_ &&
			(record.state.clearEveryFrame || previousLayout == VK_IMAGE_LAYOUT_UNDEFINED ||
			 record.state.renderCallback)) {
			image.agentic_pending_app_tick = agentic_app_tick_;
			image.agentic_pending_window_frame = agentic_window_frame_;
			if (record.agentic_next_content_revision != UINT64_MAX)
				image.agentic_pending_revision = record.agentic_next_content_revision++;
			else {
				image.agentic_pending_revision = 0;
				image.agentic_content_revision = 0;
				image.agentic_producer_app_tick = 0;
				image.agentic_producer_window_frame = 0;
				image.agentic_producer_submission_serial = 0;
			}
		}
#endif
		transitionViewportImageLayout(primary, image.nativeImage, image.layout, VK_IMAGE_LAYOUT_ATTACHMENT_OPTIMAL);
		image.layout = VK_IMAGE_LAYOUT_ATTACHMENT_OPTIMAL;
		VkRenderingAttachmentInfo attachment{VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO};
		attachment.imageView = image.nativeView;
		attachment.imageLayout = VK_IMAGE_LAYOUT_ATTACHMENT_OPTIMAL;
		attachment.loadOp = record.state.clearEveryFrame || previousLayout == VK_IMAGE_LAYOUT_UNDEFINED
			? VK_ATTACHMENT_LOAD_OP_CLEAR : VK_ATTACHMENT_LOAD_OP_LOAD;
		attachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
		std::copy(record.state.clearColor.begin(), record.state.clearColor.end(), attachment.clearValue.color.float32);
		VkRenderingInfo rendering{VK_STRUCTURE_TYPE_RENDERING_INFO};
		rendering.flags = VK_RENDERING_CONTENTS_SECONDARY_COMMAND_BUFFERS_BIT;
		rendering.renderArea.extent = {image.width, image.height}; rendering.layerCount = 1;
		rendering.colorAttachmentCount = 1; rendering.pColorAttachments = &attachment;
		vkCmdBeginRendering(primary, &rendering);
		if (record.state.renderCallback) vkCmdExecuteCommands(primary, 1, &frame.commandBuffer);
		vkCmdEndRendering(primary);
#if FLOW_UI_DEV_MODE
		auto& capture = record.pending_capture;
		if (capture.texture && !capture.recorded && !capture.submission &&
			capture.frame_slot == slot && capture.source_generation == record.active.generation) {
			transitionViewportImageLayout(primary, image.nativeImage, image.layout,
										  VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL);
			image.layout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
			transitionViewportImageLayout(primary, capture.image.nativeImage,
										  VK_IMAGE_LAYOUT_UNDEFINED,
										  VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL);
			VkImageCopy region{};
			region.srcSubresource = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 0, 1};
			region.dstSubresource = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 0, 1};
			region.extent = {image.width, image.height, 1};
			vkCmdCopyImage(primary, image.nativeImage, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
						   capture.image.nativeImage, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1,
						   &region);
			transitionViewportImageLayout(primary, capture.image.nativeImage,
										  VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
										  VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
			capture.image.layout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
			capture.recorded = true;
		}
#endif
		transitionViewportImageLayout(primary, image.nativeImage, image.layout, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
		image.layout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
#if FLOW_UI_AGENTIC_WORKFLOW
		const devSystems::agentic::AgenticImageProducerStamp producer{
			.source_image_revision = image.agentic_pending_revision
										 ? image.agentic_pending_revision
										 : image.agentic_content_revision,
			.app_tick = image.agentic_pending_revision ? image.agentic_pending_app_tick
													   : image.agentic_producer_app_tick,
			.window_frame = image.agentic_pending_revision ? image.agentic_pending_window_frame
														   : image.agentic_producer_window_frame,
			.submission_serial =
				image.agentic_pending_revision ? 0u : image.agentic_producer_submission_serial,
			.available =
				image.agentic_pending_revision != 0u || image.agentic_content_revision != 0u,
			.current_submission = image.agentic_pending_revision != 0u,
		};
		// The producer counts failures for the service's visual coverage report.
		static_cast<void>(devSystems::agentic::frame_capture::record(vk, primary, image.nativeImage, {image.width, image.height},
			record.state.colorFormat, image.layout, windowId_, slot, "viewport", key, VK_COLOR_SPACE_MAX_ENUM_KHR, producer));
#endif
	}
}

void ViewPortManager::destroyDrained(VulkanContext&) {
	if (storage_ && windowId_) {
		try {
			const storage::StringId name = storage_->intern("flowui.viewport.controller");
			(void)storage_->removeManagerRecord(
				storage::ResourceKey{storage::ResourceDomain::Viewport, name, windowId_},
				storage::ResourceKind::Viewport);
		} catch (...) {}
	}
	controller_ = nullptr; controllerHandle_ = 0; windowId_ = InvalidWindowId; storage_ = nullptr;
	missingPolicy_ = MissingVisualPolicy::UseFallbackTexture;
}

} // namespace FlowUi
