#include "devSystems/devInterface/Memory/Workbench/DevMemoryHistoryViewport.hpp"
#include "devSystems/devInterface/Performance/Workbench/DevTimelineViewport.hpp"
#define FLOWUI_INTERNAL_VIEWPORT_MANAGER 1
#include "FlowUi/Private/storageSystem/capacity/InitialMemoryCapacity.hpp"
#include "FlowUi/Private/storageSystem/capacity/MemoryCapacityProfile.hpp"
#include "FlowUi/Private/storageSystem/capacity/MemoryDemandPeaks.hpp"
#include "FlowUi/Public/App.hpp"
#include "FlowUi/Public/Resources.hpp"
#if FLOW_UI_DEV_MODE
#include "devSystems/devMonitoringAndReporting/memory/DevContainerMemory.hpp"
#endif
#include "FlowUi/Public/BuildConfig.hpp"
#include "FlowUi/Public/FlowUiTheme.hpp"
#include "FlowUi/Public/TextureRef.hpp"
#include "FlowUi/Public/configurationStructs/AppConfig.hpp"
#include "FlowUi/Public/errorManagement/Error.hpp"
#include "FlowUi/Public/managers/ActionManager.hpp"
#include "FlowUi/Public/managers/ElementManager.hpp"
#include "FlowUi/Public/managers/FontManager.hpp"
#include "FlowUi/Public/managers/ImageManager.hpp"
#include "FlowUi/Public/managers/ThemeManager.hpp"
#include "devSystems/devTooling/bake/DevBakedChanges.hpp"
#if COMPILE_FSELI
#include "FSEL/StandardIcons.hpp"
#include "FSEL/Theme.hpp"
#endif
#if FLOWUI_INCLUDE_ICON_MANAGER
#include "FlowUi/Public/managers/IconManager.hpp"
#endif
#include "FlowUi/Private/renderer/Vk_UiRenderer.hpp"
#include "FlowUi/Public/managers/UiManager.hpp"
#include "FlowUi/Public/managers/ViewPortManager.hpp"
#include "devSystems/devMonitoringAndReporting/timing/DevTimingZone.hpp"
#if FLOW_UI_AGENTIC_WORKFLOW
#include "devSystems/devAgenticWorkflow/AgenticBuildInfo.hpp"
#include "devSystems/devAgenticWorkflow/AgenticService.hpp"
#include "devSystems/devAgenticWorkflow/frameCapture/AgenticFrameCapture.hpp"
#endif
#if FLOW_UI_DEV_MODE
#include "devSystems/devInterface/Permanents/Backend/DevInterface.hpp"
#include "devSystems/devMonitoringAndReporting/DevMonitoringAndReporting.hpp"
#include "devSystems/devMonitoringAndReporting/errors/DevError.hpp"
#include "devSystems/devMonitoringAndReporting/memory/DevMemory.hpp"
#include "devSystems/devMonitoringAndReporting/memory/DevMemoryProbe.hpp"
#include "devSystems/devMonitoringAndReporting/memory/DevMemorySources.hpp"
#include "devSystems/devMonitoringAndReporting/reporting/DevErrorReporting.hpp"
#include "devSystems/devMonitoringAndReporting/reporting/DevMemoryReporting.hpp"
#include "devSystems/devMonitoringAndReporting/reporting/DevPerformanceCapture.hpp"
#include "devSystems/devMonitoringAndReporting/reporting/DevTimingReporting.hpp"
#include "devSystems/devMonitoringAndReporting/timing/DevGpuTiming.hpp"
#include "devSystems/devMonitoringAndReporting/timing/DevTiming.hpp"
#include "devSystems/devTooling/DevTooling.hpp"
#endif
#include "FlowUi/Private/inputService/InputQueue.hpp"
#include "FlowUi/Private/renderer/Vk_UiRenderer.hpp"
#include "FlowUi/Private/renderer/vulkan/Vk_Context.hpp"
#include "FlowUi/Private/renderer/vulkan/Vk_Frames.hpp"
#include "FlowUi/Private/renderer/vulkan/Vk_Swapchain.hpp"
#include "FlowUi/Private/resourceService/FontCatalogController.hpp"
#include "FlowUi/Private/storageSystem/FlowStorageSystem.hpp"
#include "FlowUi/Private/windowService/Window.hpp"

#include <algorithm>
#include <array>
#include <atomic>
#include <cctype>
#include <chrono>
#include <cstdio>
#include <exception>
#include <filesystem>
#include <limits>
#include <memory>
#include <stdexcept>
#include <string>
#include <thread>
#include <unordered_map>
#include <unordered_set>
#include <vector>

#include "FlowUi/Private/AppRuntime.hpp"
#include "FlowUi/Public/defaults/FontDefaults.hpp"
#include "FlowUi/Public/defaults/TextureDefaults.hpp"

namespace {

#if FLOW_UI_DEV_MODE
inline constexpr auto kDevFrameBegin =
	FlowUi::devSystems::makeDevErrorBreadcrumb("flowui.frame.begin");
inline constexpr auto kDevFrameComplete =
	FlowUi::devSystems::makeDevErrorBreadcrumb("flowui.frame.complete");
inline constexpr auto kDevFrameCancel =
	FlowUi::devSystems::makeDevErrorBreadcrumb("flowui.frame.cancel");
inline constexpr auto kDevStorageFrameBegin =
	FlowUi::devSystems::makeDevErrorBreadcrumb("flowui.storage.frame.begin");
inline constexpr auto kDevStorageFrameSeal =
	FlowUi::devSystems::makeDevErrorBreadcrumb("flowui.storage.frame.seal");
inline constexpr auto kDevStorageFrameCancel =
	FlowUi::devSystems::makeDevErrorBreadcrumb("flowui.storage.frame.cancel");
#endif

const char* errorEventKindName(FlowUi::ErrorEventKind kind) noexcept {
	switch (kind) {
	case FlowUi::ErrorEventKind::Resolved:
		return "resolved";
	case FlowUi::ErrorEventKind::Reported:
		return "reported";
	case FlowUi::ErrorEventKind::Fatal:
		return "fatal";
	case FlowUi::ErrorEventKind::BackendDiagnostic:
		return "backend";
	}
	return "unknown";
}

const char* errorResolutionName(FlowUi::ErrorResolution resolution) noexcept {
	switch (resolution) {
	case FlowUi::ErrorResolution::None:
		return "None";
	case FlowUi::ErrorResolution::Rejected:
		return "Rejected";
	case FlowUi::ErrorResolution::Ignored:
		return "Ignored";
	case FlowUi::ErrorResolution::Retried:
		return "Retried";
	case FlowUi::ErrorResolution::UsedFallback:
		return "UsedFallback";
	case FlowUi::ErrorResolution::Skipped:
		return "Skipped";
	case FlowUi::ErrorResolution::GrewCapacity:
		return "GrewCapacity";
	case FlowUi::ErrorResolution::EvictedAndRetried:
		return "EvictedAndRetried";
	case FlowUi::ErrorResolution::CanceledOperation:
		return "CanceledOperation";
	case FlowUi::ErrorResolution::CanceledFrame:
		return "CanceledFrame";
	case FlowUi::ErrorResolution::RecreatedWindow:
		return "RecreatedWindow";
	case FlowUi::ErrorResolution::ClosedWindow:
		return "ClosedWindow";
	case FlowUi::ErrorResolution::RecreatedApp:
		return "RecreatedApp";
	case FlowUi::ErrorResolution::Propagated:
		return "Propagated";
	case FlowUi::ErrorResolution::Halted:
		return "Halted";
	case FlowUi::ErrorResolution::Terminated:
		return "Terminated";
	}
	return "Unknown";
}

template <typename... Args>
void appendErrorFormat(char* buffer, std::size_t capacity, std::size_t& used, const char* format,
					   Args... args) noexcept {
	if (used >= capacity - 1u)
		return;
	const int written = std::snprintf(buffer + used, capacity - used, format, args...);
	if (written <= 0)
		return;
	used = std::min<std::size_t>(used + static_cast<std::size_t>(written), capacity - 1u);
}

void writeDefaultErrorEvent(std::FILE* output, const FlowUi::ErrorEventView& event) noexcept {
	if (!output)
		return;

	char buffer[1024]{};
	const std::string_view site = FlowUi::errorSiteName(event.error.site);
	std::size_t used = 0u;
	if (event.kind == FlowUi::ErrorEventKind::BackendDiagnostic) {
		const int messageLength =
			static_cast<int>(std::min<std::size_t>(event.nativeMessage.size(), 768u));
		appendErrorFormat(buffer, sizeof(buffer), used, "[FlowUi][backend] %.*s",
						  static_cast<int>(site.size()), site.data());
		if (event.error.nativeCode != 0u) {
			appendErrorFormat(buffer, sizeof(buffer), used, " native=%u", event.error.nativeCode);
		}
		if (event.nativeCategory != 0u) {
			appendErrorFormat(buffer, sizeof(buffer), used, " category=0x%x", event.nativeCategory);
		}
		if (messageLength > 0) {
			appendErrorFormat(buffer, sizeof(buffer), used, ": %.*s", messageLength,
							  event.nativeMessage.data());
		}
	} else {
		const std::string_view code = FlowUi::errorName(event.error.code);
		const int messageLength =
			static_cast<int>(std::min<std::size_t>(event.nativeMessage.size(), 512u));
		appendErrorFormat(buffer, sizeof(buffer), used, "[FlowUi][%s] %.*s @ %.*s",
						  errorEventKindName(event.kind), static_cast<int>(code.size()),
						  code.data(), static_cast<int>(site.size()), site.data());
		if (event.error.subject != 0u) {
			appendErrorFormat(buffer, sizeof(buffer), used, " subject=0x%llx",
							  static_cast<unsigned long long>(event.error.subject));
		}
		if (event.error.auxiliary != 0u) {
			appendErrorFormat(buffer, sizeof(buffer), used, " auxiliary=0x%llx",
							  static_cast<unsigned long long>(event.error.auxiliary));
		}
		if (event.error.nativeCode != 0u) {
			appendErrorFormat(buffer, sizeof(buffer), used, " native=%u", event.error.nativeCode);
		}
		if (event.resolution != FlowUi::ErrorResolution::None) {
			appendErrorFormat(buffer, sizeof(buffer), used, " resolution=%s",
							  errorResolutionName(event.resolution));
		}
		if (messageLength > 0) {
			appendErrorFormat(buffer, sizeof(buffer), used, ": %.*s", messageLength,
							  event.nativeMessage.data());
		}
	}
	if (used < sizeof(buffer) - 1u)
		buffer[used++] = '\n';
	if (used > 0u)
		(void)std::fwrite(buffer, 1u, used, output);
	if (event.kind == FlowUi::ErrorEventKind::Fatal) {
		(void)std::fflush(output);
	}
}

thread_local bool reportingErrorEvent = false;

void vkCheck(VkResult result, FlowUi::ErrorSite site,
			 FlowUi::ErrorCode code = FlowUi::ErrorCode::VulkanNativeCallFailed) {
	if (result != VK_SUCCESS) {
		if (result == VK_ERROR_DEVICE_LOST)
			code = FlowUi::ErrorCode::VulkanDeviceLost;
		if (result == VK_ERROR_OUT_OF_HOST_MEMORY || result == VK_ERROR_OUT_OF_DEVICE_MEMORY) {
			code = FlowUi::ErrorCode::AllocationFailed;
		}
		throw FlowUi::FlowUiException(
			FlowUi::makeError(code, site, 0u, 0u, static_cast<std::uint32_t>(result)));
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

void transitionSwapchainImageLayout(VkCommandBuffer commandBuffer, VkImage image,
									VkImageLayout oldLayout, VkImageLayout newLayout) {
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
		destinationAccessMask =
			VK_ACCESS_COLOR_ATTACHMENT_READ_BIT | VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
	} else if (oldLayout == VK_IMAGE_LAYOUT_ATTACHMENT_OPTIMAL &&
			   newLayout == VK_IMAGE_LAYOUT_PRESENT_SRC_KHR) {
		sourceStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
		destinationStageMask = VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT;
		sourceAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
		destinationAccessMask = VK_ACCESS_MEMORY_READ_BIT;
	} else {
		throw FlowUi::FlowUiException(FlowUi::makeError(FlowUi::ErrorCode::RenderCommandInvalid,
														FlowUi::ErrorSite::AppDrawFrame));
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

	vkCmdPipelineBarrier(commandBuffer, sourceStageMask, destinationStageMask, 0, 0, nullptr, 0,
						 nullptr, 1, &imageBarrier);
}

void validateSecondarySurface(VulkanContext& vk, VkSurfaceKHR surface) {
	VkSurfaceCapabilitiesKHR capabilities{};
	vkCheck(vkGetPhysicalDeviceSurfaceCapabilitiesKHR(vk.phys, surface, &capabilities),
			FlowUi::ErrorSite::AppCreateWindow);
	uint32_t formatCount = 0;
	vkCheck(vkGetPhysicalDeviceSurfaceFormatsKHR(vk.phys, surface, &formatCount, nullptr),
			FlowUi::ErrorSite::AppCreateWindow);
	uint32_t presentModeCount = 0;
	vkCheck(vkGetPhysicalDeviceSurfacePresentModesKHR(vk.phys, surface, &presentModeCount, nullptr),
			FlowUi::ErrorSite::AppCreateWindow);
	if (formatCount == 0 || presentModeCount == 0) {
		throw FlowUi::FlowUiException(FlowUi::makeError(
			FlowUi::ErrorCode::WindowPresentationUnsupported, FlowUi::ErrorSite::AppCreateWindow));
	}
}

} // namespace

namespace FlowUi::detail::app_runtime {

AppErrorObserver::AppErrorObserver(FlowUi::ErrorObserverConfig config
#if FLOW_UI_DEV_MODE
								   ,
								   FlowUi::devSystems::DevErrorMonitoring* devErrors
#endif
								   ) noexcept
	: config_(config)
#if FLOW_UI_DEV_MODE
	  ,
	  devErrors_(devErrors)
#endif
{
	active_.store(this, std::memory_order_release);
}

AppErrorObserver::~AppErrorObserver() {
	const AppErrorObserver* expected = this;
	(void)active_.compare_exchange_strong(expected, nullptr, std::memory_order_acq_rel);
}

void AppErrorObserver::report(const FlowUi::ErrorEventView& event) const noexcept {
	switch (config_.mode) {
	case FlowUi::ErrorReportingMode::SinkAndDefault:
		writeDefaultErrorEvent(config_.output, event);
		config_.sink.notify(event);
		break;
	case FlowUi::ErrorReportingMode::SinkOrDefault:
		if (config_.sink)
			config_.sink.notify(event);
		else
			writeDefaultErrorEvent(config_.output, event);
		break;
	case FlowUi::ErrorReportingMode::SinkOnly:
		config_.sink.notify(event);
		break;
	case FlowUi::ErrorReportingMode::DefaultOnly:
		writeDefaultErrorEvent(config_.output, event);
		break;
	}
}

#if FLOW_UI_DEV_MODE
void AppErrorObserver::observeForDev(
	const FlowUi::ErrorEventView& event,
	const FlowUi::devSystems::DevErrorSourceDescriptor& source) const noexcept {
	if (devErrors_)
		devErrors_->recordProductionEvent(0u, event, source);
}
#endif

const AppErrorObserver* AppErrorObserver::active() noexcept {
	return active_.load(std::memory_order_acquire);
}

std::atomic<const AppErrorObserver*> AppErrorObserver::active_{nullptr};

} // namespace FlowUi::detail::app_runtime

namespace FlowUi {

namespace detail {

#if FLOW_UI_DEV_MODE
void reportErrorEvent(const ErrorEventView& event, const char* file, const char* function,
					  std::uint32_t line, std::uint32_t column) noexcept {
#else
void reportErrorEvent(const ErrorEventView& event) noexcept {
#endif
	const auto* observer = ::FlowUi::detail::app_runtime::AppErrorObserver::active();
#if FLOW_UI_DEV_MODE
	if (observer) {
		observer->observeForDev(event,
								devSystems::makeDevErrorSource("flowui.production.error", file,
															   function, line, column));
	}
#endif
	if (reportingErrorEvent) {
		writeDefaultErrorEvent(stderr, event);
		return;
	}

	struct ReportingGuard {
		ReportingGuard() noexcept { reportingErrorEvent = true; }
		~ReportingGuard() { reportingErrorEvent = false; }
	} guard;

	if (observer) {
		observer->report(event);
	} else {
		writeDefaultErrorEvent(stderr, event);
	}
}

} // namespace detail

WindowConfig mergeWindowConfig(const WindowConfig& base, const WindowConfigOverrides& overrides) {
	WindowConfig result = base;
	if (overrides.width)
		result.width = *overrides.width;
	if (overrides.height)
		result.height = *overrides.height;
	if (overrides.title)
		result.title = *overrides.title;
	if (overrides.resizable)
		result.resizable = *overrides.resizable;
	if (overrides.decorated)
		result.decorated = *overrides.decorated;
	if (overrides.maximized)
		result.maximized = *overrides.maximized;
	if (overrides.fullscreen)
		result.fullscreen = *overrides.fullscreen;
	if (overrides.highDPI)
		result.highDPI = *overrides.highDPI;
	if (overrides.input)
		result.input = *overrides.input;
	return result;
}

AppWindowConfig makeWindowConfig(const AppConfig& config, const WindowConfig& native) {
	AppWindowConfig result{
		.native = native,
		.vulkan = config.vk,
		.ui = config.ui,
	};
	result.vulkan.framesInFlight = std::max(1u, result.vulkan.framesInFlight);
	return result;
}

AppWindowConfig makeMainWindowConfig(const AppConfig& config) {
	return makeWindowConfig(config, config.window);
}

storage::StorageConfig makeStorageConfig(const AppConfig& config) {
	storage::StorageConfig result{};
	result.framesInFlight = std::max(1u, config.vk.framesInFlight);
	result.expectedWindows = std::max(2u, result.expectedWindows);
	result.initialInstanceBytesPerFrame = 1024ull * 1024ull;
	result.expectedBindingsPerWindow = kUiTextureDescriptorCapacity;
	(void)storage::applyMemoryCapacityProfile(result, config.memoryCapacityProfile);
	result.allowTransientGrowth =
		result.allowRuntimeGrowth &&
		config.errors.policy.transientCapacity == StorageCapacityPolicy::GrowWithinBudget;
	result.allowPersistentGrowth =
		result.allowRuntimeGrowth &&
		config.errors.policy.persistentCapacity == StorageCapacityPolicy::GrowWithinBudget;
	return result;
}

storage::WindowStorageDesc makeWindowStorageDesc(storage::IStorageSystem& storageSystem,
												 const storage::StorageConfig& storageConfig,
												 const AppWindowConfig& config) {
	return storage::WindowStorageDesc{
		.framesInFlight = config.vulkan.framesInFlight,
		.workerCount = storageConfig.expectedWorkerCount,
		.initialTextureBindings = config.uiTextureDescriptorCapacity,
		.maxTextureBindings = config.uiTextureDescriptorCapacity,
		.transientBytesPerFrame = storageConfig.transientBytesPerFramePerWindow,
		.transientBytesPerWorker = storageConfig.transientBytesPerWorker,
		.debugName = storageSystem.intern(config.native.title),
	};
}

AppConfig makeUiManagerConfig(const AppConfig& appDefaults, const AppWindowConfig& window) {
	AppConfig result = appDefaults;
	result.window = window.native;
	result.vk = window.vulkan;
	result.ui = window.ui;
	return result;
}

#if FLOW_UI_DEV_MODE

void captureDevUiReplayPacket(const DevUiReplayRequest& request,
							  const Clay_RenderCommandArray& source,
							  const storage::PreparedTextureBindings& preparedBindings,
							  uint32_t textureFrameSlot, DevUiReplayPacket& packet) {
	packet.clearCommands();
	packet.sourceRootBounds = request.sourceRootBounds;
	packet.requestKey = request.key;
	packet.textureFrameSlot = textureFrameSlot;
	++packet.captureSerial;
	if (request.commandIds.empty() || source.length <= 0 || !source.internalArray)
		return;

	if (request.rootCommandId != 0u) {
		for (int32_t commandIndex = 0; commandIndex < source.length; ++commandIndex) {
			const Clay_RenderCommand& command = source.internalArray[commandIndex];
			if (command.id == request.rootCommandId &&
				(command.commandType == CLAY_RENDER_COMMAND_TYPE_RECTANGLE ||
				 command.commandType == CLAY_RENDER_COMMAND_TYPE_BORDER ||
				 command.commandType == CLAY_RENDER_COMMAND_TYPE_TEXT ||
				 command.commandType == CLAY_RENDER_COMMAND_TYPE_IMAGE)) {
				packet.sourceRootBounds = command.boundingBox;
				break;
			}
		}
	}

	const std::unordered_set<uint32_t> requestedIds(request.commandIds.begin(),
													request.commandIds.end());
	std::vector<uint8_t> selected(static_cast<size_t>(source.length), 0u);
	std::vector<int32_t> clipStack{};
	clipStack.reserve(8u);
	for (int32_t commandIndex = 0; commandIndex < source.length; ++commandIndex) {
		const Clay_RenderCommand& command = source.internalArray[commandIndex];
		if (command.commandType == CLAY_RENDER_COMMAND_TYPE_SCISSOR_START) {
			clipStack.push_back(commandIndex);
			continue;
		}
		if (command.commandType == CLAY_RENDER_COMMAND_TYPE_SCISSOR_END) {
			if (!clipStack.empty()) {
				const int32_t startIndex = clipStack.back();
				clipStack.pop_back();
				if (selected[static_cast<size_t>(startIndex)] != 0u) {
					selected[static_cast<size_t>(commandIndex)] = 1u;
				}
			}
			continue;
		}
		const bool drawable = command.commandType == CLAY_RENDER_COMMAND_TYPE_RECTANGLE ||
							  command.commandType == CLAY_RENDER_COMMAND_TYPE_BORDER ||
							  command.commandType == CLAY_RENDER_COMMAND_TYPE_TEXT ||
							  command.commandType == CLAY_RENDER_COMMAND_TYPE_IMAGE;
		if (!drawable || !requestedIds.contains(command.id))
			continue;
		selected[static_cast<size_t>(commandIndex)] = 1u;
		for (const int32_t clipIndex : clipStack) {
			selected[static_cast<size_t>(clipIndex)] = 1u;
		}
	}

	size_t selectedCount = 0u;
	for (const uint8_t isSelected : selected)
		selectedCount += isSelected != 0u ? 1u : 0u;
	packet.commands.reserve(selectedCount);
	for (int32_t commandIndex = 0; commandIndex < source.length; ++commandIndex) {
		if (selected[static_cast<size_t>(commandIndex)] != 0u) {
			packet.commands.push_back(source.internalArray[commandIndex]);
		}
	}
	packet.textByCommand.resize(packet.commands.size());
	packet.textureByCommand.resize(packet.commands.size());
	uint32_t maximumTextureIndex = 0u;
	for (size_t commandIndex = 0u; commandIndex < packet.commands.size(); ++commandIndex) {
		Clay_RenderCommand& command = packet.commands[commandIndex];
		if (command.commandType == CLAY_RENDER_COMMAND_TYPE_TEXT) {
			const Clay_StringSlice text = command.renderData.text.stringContents;
			if (text.chars && text.length > 0) {
				packet.textByCommand[commandIndex].assign(text.chars,
														  static_cast<size_t>(text.length));
			}
		} else if (command.commandType == CLAY_RENDER_COMMAND_TYPE_IMAGE &&
				   command.renderData.image.imageData) {
			packet.textureByCommand[commandIndex] =
				*static_cast<const TextureRef*>(command.renderData.image.imageData);
			maximumTextureIndex =
				std::max(maximumTextureIndex, packet.textureByCommand[commandIndex].handle.index);
		}
	}
	if (!preparedBindings.bindingsByTextureIndex.empty()) {
		const size_t bindingCount = std::min(preparedBindings.bindingsByTextureIndex.size(),
											 static_cast<size_t>(maximumTextureIndex) + 1u);
		packet.textureBindings.assign(preparedBindings.bindingsByTextureIndex.begin(),
									  preparedBindings.bindingsByTextureIndex.begin() +
										  static_cast<std::ptrdiff_t>(bindingCount));
	}
	packet.rebindPayloads();
}
#endif

#if FLOW_UI_DEV_MODE
class TimingContextExitGuard {
public:
	explicit TimingContextExitGuard(devSystems::DevTimingRecorder& recorder) noexcept
		: recorder_(&recorder) {}
	~TimingContextExitGuard() { recorder_->clearFrameContext(); }

	TimingContextExitGuard(const TimingContextExitGuard&) = delete;
	TimingContextExitGuard& operator=(const TimingContextExitGuard&) = delete;

private:
	devSystems::DevTimingRecorder* recorder_;
};
#endif

class WindowFrameExitGuard {
public:
	WindowFrameExitGuard(storage::IStorageSystem& storageSystem, AppWindow& window,
						 WindowId& activeWindow
#if FLOW_UI_DEV_MODE
						 ,
						 devSystems::DevTimingRecorder& timingRecorder,
						 devSystems::DevErrorRecorder& errorRecorder
#endif
#if FLOW_UI_AGENTIC_WORKFLOW
						 ,
						 devSystems::agentic::AgenticWorkflowService* agentic_service,
						 VulkanContext* agentic_context
#endif
						 ) noexcept
		: storage_(&storageSystem), window_(&window), activeWindow_(&activeWindow)
#if FLOW_UI_DEV_MODE
		  ,
		  timingRecorder_(&timingRecorder), errorRecorder_(&errorRecorder),
		  uncaughtExceptions_(std::uncaught_exceptions())
#endif
#if FLOW_UI_AGENTIC_WORKFLOW
		  ,
		  agentic_service_(agentic_service), agentic_context_(agentic_context)
#endif
	{
	}

#if FLOW_UI_DEV_MODE
	void setTimingResult(devSystems::TimingRecordFlag result) noexcept { timingResult_ = result; }
#endif

	~WindowFrameExitGuard() {
#if FLOW_UI_AGENTIC_WORKFLOW
		if (agentic_service_ && window_->agentic_frame.key.window_frame) {
			if (agentic_context_ && !window_->agentic_frame.submitted)
				devSystems::agentic::frame_capture::cancel_frame(*agentic_context_, window_->id,
																 window_->agentic_frame.frame_slot);
			window_->agentic_frame.end_ns = devSystems::agentic::monotonic_nanoseconds();
			window_->agentic_frame.cpu_duration_ns =
				window_->agentic_frame.end_ns - window_->agentic_frame.begin_ns;
			window_->agentic_frame.canceled = !window_->agentic_frame.presented;
			agentic_service_->observe_frame(std::move(window_->agentic_frame));
			window_->agentic_frame = {};
		}
#endif
		if (window_->storageFrame) {
			try {
				storage_->cancelFrame(window_->storageFrame);
			} catch (...) {}
		}
		window_->preparedUi = {};
		window_->storageReadLease = {};
		window_->storageFrame = {};
		if (window_->phase != AppWindow::Phase::Closing)
			window_->phase = AppWindow::Phase::Idle;
		if (*activeWindow_ == window_->id)
			*activeWindow_ = InvalidWindowId;
#if FLOW_UI_DEV_MODE
		const devSystems::TimingRecordFlag finalTimingResult =
			std::uncaught_exceptions() > uncaughtExceptions_
				? devSystems::TimingRecordFlag::Exception
				: timingResult_;
		window_->preparedGapTiming.end(finalTimingResult);
		window_->userBuildTiming.end(finalTimingResult);
		window_->frameTotalTiming.end(finalTimingResult);
		window_->timingFrame = {};
		window_->timingAppTick = 0u;
		timingRecorder_->clearFrameContext();
		const bool completed = finalTimingResult == devSystems::TimingRecordFlag::Completed;
		devSystems::recordGlobalDevBreadcrumb(completed ? kDevFrameComplete : kDevFrameCancel,
											  window_->id, window_->frameNumber);
		errorRecorder_->clearFrameContext();
#endif
	}

private:
	storage::IStorageSystem* storage_;
#if FLOW_UI_AGENTIC_WORKFLOW
	devSystems::agentic::AgenticWorkflowService* agentic_service_ = nullptr;
	VulkanContext* agentic_context_ = nullptr;
#endif
	AppWindow* window_;
	WindowId* activeWindow_;
#if FLOW_UI_DEV_MODE
	devSystems::DevTimingRecorder* timingRecorder_ = nullptr;
	devSystems::DevErrorRecorder* errorRecorder_ = nullptr;
	devSystems::TimingRecordFlag timingResult_ = devSystems::TimingRecordFlag::Canceled;
	int uncaughtExceptions_ = 0;
#endif
};

App::Impl::Impl(const AppConfig& initialConfig)
#if FLOW_UI_DEV_MODE
	: devMonitoring(initialConfig.dev.monitoring), devTooling(initialConfig.dev.tooling),
	  platformErrorAttachment(devMonitoring.errors().attachCurrentThread("flowui.platform")),
	  errorObserver(initialConfig.errors.observer, &devMonitoring.errors()),
	  platformTimingAttachment(devMonitoring.timing().attachCurrentThread("flowui.platform")),
	  config(initialConfig) {
	devMonitoring.memoryReporting().set_capacity_baseline(initialConfig.memoryCapacityProfile);
}
#else
	: errorObserver(initialConfig.errors.observer), config(initialConfig) {
}
#endif

App::Impl::~Impl() {
	cleanup();
}

#if FLOW_UI_DEV_MODE
devSystems::DevTimingRecorder& App::Impl::timingRecorder() noexcept {
	return platformTimingAttachment.recorder();
}
#endif

#if FLOW_UI_DEV_MODE
devSystems::DevErrorRecorder& App::Impl::errorRecorder() noexcept {
	return platformErrorAttachment.recorder();
}
#endif

#if FLOW_UI_DEV_MODE
#if FLOWUI_DEV_MEMORY_LEVEL >= 2
void App::Impl::sample_app_memory(const void* owner,
								  devSystems::MemoryProbeContext& context) noexcept {
	const auto& app = *static_cast<const Impl*>(owner);
	devSystems::append_capacity_demand(context.sink,
									   FlowUi::detail::InitialCapacityTarget::app_windows,
									   app.windows, app.capacity_demand);
	devSystems::append_capacity_demand(context.sink,
									   FlowUi::detail::InitialCapacityTarget::app_managed,
									   app.managedWindows, app.capacity_demand);
	devSystems::append_capacity_demand(context.sink,
									   FlowUi::detail::InitialCapacityTarget::app_dispatch,
									   app.dispatch_order, app.capacity_demand);
}
#endif
#endif

#if FLOW_UI_DEV_MODE
#if FLOWUI_DEV_MEMORY_LEVEL >= 2
void App::Impl::sampleElementMemory(const void* owner,
									devSystems::MemoryProbeContext& context) noexcept {
	static_cast<const ElementManager*>(owner)->appendDevMemorySamples(context.sink);
}
#endif
#endif

#if FLOW_UI_DEV_MODE
#if FLOWUI_DEV_MEMORY_LEVEL >= 2
void App::Impl::sampleFontMemory(const void* owner,
								 devSystems::MemoryProbeContext& context) noexcept {
	static_cast<const FontManager*>(owner)->appendDevMemorySamples(context.sink);
}
#endif
#endif

#if FLOW_UI_DEV_MODE
#if FLOWUI_DEV_MEMORY_LEVEL >= 2
#if FLOWUI_INCLUDE_ICON_MANAGER
void App::Impl::sampleIconMemory(const void* owner,
								 devSystems::MemoryProbeContext& context) noexcept {
	static_cast<const IconManager*>(owner)->appendDevMemorySamples(context.sink);
}
#endif
#endif
#endif

#if FLOW_UI_DEV_MODE
#if FLOWUI_DEV_MEMORY_LEVEL >= 2
void App::Impl::sampleActionMemory(const void* owner,
								   devSystems::MemoryProbeContext& context) noexcept {
	static_cast<const ActionManager*>(owner)->appendDevMemorySamples(context.sink);
}
#endif
#endif

#if FLOW_UI_DEV_MODE
#if FLOWUI_DEV_MEMORY_LEVEL >= 2
void App::Impl::sampleThemeMemory(const void* owner,
								  devSystems::MemoryProbeContext& context) noexcept {
	static_cast<const ThemeManager*>(owner)->appendDevMemorySamples(context.sink);
}
#endif
#endif

#if FLOW_UI_DEV_MODE
#if FLOWUI_DEV_MEMORY_LEVEL >= 2
void App::Impl::sampleDevToolingMemory(const void* owner,
									   devSystems::MemoryProbeContext& context) noexcept {
	static_cast<const devSystems::DevTooling*>(owner)->appendDevMemorySamples(context.sink);
}
#endif
#endif

#if FLOW_UI_DEV_MODE
#if FLOWUI_DEV_MEMORY_LEVEL >= 2
void App::Impl::sampleWindowMemory(const void* owner,
								   devSystems::MemoryProbeContext& context) noexcept {
	const auto& window = *static_cast<const AppWindow*>(owner);
	window.ui.appendDevMemorySamples(context.sink);
	window.viewPorts.appendDevMemorySamples(context.sink);
	window.renderer.appendDevMemorySamples(context.sink, &window.preparedUi);
}
#endif
#endif

#if FLOW_UI_DEV_MODE
#if FLOWUI_DEV_MEMORY_LEVEL >= 2
#if FLOW_UI_AGENTIC_WORKFLOW
void App::Impl::sample_agentic_memory(const void* owner,
									  devSystems::MemoryProbeContext& context) noexcept {
	const auto& application = *static_cast<const Impl*>(owner);
	const auto cpu_bytes =
		application.agentic_service ? application.agentic_service->owned_memory_bytes() : 0;
	context.sink.append(devSystems::MemoryValueSample{
		.source = devSystems::memory_sources::kAgenticCpu.id,
		.backingAllocatedBytes = cpu_bytes,
		.flags = devSystems::MemorySampleFlag::Estimate,
		.value_fields = static_cast<uint16_t>(devSystems::MemoryValueField::Backing)});
	context.sink.append(devSystems::MemoryValueSample{
		.source = devSystems::memory_sources::kAgenticReadback.id,
		.backingAllocatedBytes = devSystems::agentic::frame_capture::pending_bytes(application.vk),
		.flags = devSystems::MemorySampleFlag::OverlapsProcessResident,
		.value_fields = static_cast<uint16_t>(devSystems::MemoryValueField::Backing)});
}
#endif
#endif
#endif

#if FLOW_UI_DEV_MODE
#if FLOWUI_DEV_MEMORY_LEVEL >= 2
void App::Impl::registerAppMemoryProbes() {
	auto& memory = devMonitoring.memory();
#if FLOW_UI_AGENTIC_WORKFLOW
	if (!memory.registerSource(devSystems::memory_sources::kAgenticCpu) ||
		!memory.registerSource(devSystems::memory_sources::kAgenticReadback) ||
		!memory.registerProbe(
			{devSystems::memory_sources::kAgenticCpu.id, this, &sample_agentic_memory}))
		detail::reportErrorEvent(ErrorEventView{
			.error = makeError(ErrorCode::AgenticBudgetExhausted, ErrorSite::AgenticConfigure),
			.kind = ErrorEventKind::Reported});
#endif
	(void)memory.registerProbe(
		{FlowUi::detail::initial_capacity_definitions
			 [static_cast<size_t>(FlowUi::detail::InitialCapacityTarget::app_windows)]
				 .id(),
		 this, &sample_app_memory});
	(void)memory.registerProbe(
		{devSystems::memory_sources::kElements.id, &elementManager, &sampleElementMemory});
	(void)memory.registerProbe({devSystems::memory_sources::kFonts.id, &fonts, &sampleFontMemory});
#if FLOWUI_INCLUDE_ICON_MANAGER
	(void)memory.registerProbe({devSystems::memory_sources::kIcons.id, &icons, &sampleIconMemory});
#endif
	(void)memory.registerProbe(
		{devSystems::memory_sources::kActions.id, &actionManager, &sampleActionMemory});
	(void)memory.registerProbe(
		{devSystems::memory_sources::kThemes.id, &themeManager, &sampleThemeMemory});
	(void)memory.registerProbe(
		{devSystems::memory_sources::kDevTooling.id, &devTooling, &sampleDevToolingMemory});
}
#endif
#endif

#if FLOW_UI_DEV_MODE
#if FLOWUI_DEV_MEMORY_LEVEL >= 2
void App::Impl::registerWindowMemoryProbe(AppWindow& window) {
	(void)devMonitoring.memory().registerProbe(
		{devSystems::memory_sources::kUiLayout.id, &window, &sampleWindowMemory});
}
#endif
#endif

#if FLOW_UI_DEV_MODE
#if FLOWUI_DEV_MEMORY_LEVEL >= 2
void App::Impl::unregisterWindowMemoryProbe(AppWindow& window) noexcept {
	devMonitoring.memory().unregisterProbe(devSystems::memory_sources::kUiLayout.id, &window);
}
#endif
#endif

#if FLOW_UI_DEV_MODE
#if FLOWUI_DEV_MEMORY_LEVEL >= 2
void App::Impl::unregisterAppMemoryProbes() noexcept {
	auto& memory = devMonitoring.memory();
#if FLOW_UI_AGENTIC_WORKFLOW
	memory.unregisterProbe(devSystems::memory_sources::kAgenticCpu.id, this);
#endif
	memory.unregisterProbe(devSystems::memory_sources::kElements.id, &elementManager);
	memory.unregisterProbe(devSystems::memory_sources::kFonts.id, &fonts);
#if FLOWUI_INCLUDE_ICON_MANAGER
	memory.unregisterProbe(devSystems::memory_sources::kIcons.id, &icons);
#endif
	memory.unregisterProbe(devSystems::memory_sources::kActions.id, &actionManager);
	memory.unregisterProbe(devSystems::memory_sources::kThemes.id, &themeManager);
}
#endif
#endif

AppWindow& App::Impl::requireWindow(WindowId id) {
	const auto found = windows.find(id);
	if (id == InvalidWindowId || found == windows.end() || !found->second) {
		throw FlowUiException(
			makeError(ErrorCode::InvalidWindowId, ErrorSite::AppAccessWindow, id));
	}
	return *found->second;
}

const AppWindow& App::Impl::requireWindow(WindowId id) const {
	const auto found = windows.find(id);
	if (id == InvalidWindowId || found == windows.end() || !found->second) {
		throw FlowUiException(
			makeError(ErrorCode::InvalidWindowId, ErrorSite::AppAccessWindow, id));
	}
	return *found->second;
}

AppWindow& App::Impl::mainWindow() {
	return requireWindow(mainWindowId);
}

const AppWindow& App::Impl::mainWindow() const {
	return requireWindow(mainWindowId);
}

void App::Impl::initializeDefaultFont() {
	bool defaultFontLoaded = false;
	{
		auto default_family = config.ui.defaultFontFamily;
		for (auto& face : default_family.faces) {
			if (face.baked_data.empty() &&
				(face.path == "assets/fonts/Inter.arfont" || face.path.empty())) {
				const auto path = locate_resource("fonts/Inter.arfont", config.resources);
				face.path = path ? *path : std::filesystem::path{};
			}
		}
		auto created = fonts.createFamily(default_family);
		if (created) {
			const FontManager::FontId defaultFontId = fonts.resolveFont(*created);
			defaultFontLoaded = fonts.getFontById(defaultFontId) != nullptr;
		}
	}
	if (!defaultFontLoaded && fonts.getFontById(0) == nullptr &&
		config.errors.policy.defaultFont == DefaultFontFailurePolicy::FailAppImmediately) {
		throw FlowUiException(
			makeError(ErrorCode::DefaultFontUnavailable, ErrorSite::FontManagerInitialize));
	}

	if (!defaultFontLoaded && fonts.getFontById(0) == nullptr) {
		const std::string fallbackFamilyName = config.ui.defaultFontFamily.name.empty()
												   ? std::string("Default")
												   : config.ui.defaultFontFamily.name;
		if (fonts.getFamilyId(fallbackFamilyName) ==
			std::numeric_limits<FontManager::FontFamilyId>::max()) {
			FontFamilyCreateInfo fallbackFamily{};
			fallbackFamily.name = fallbackFamilyName;
			fallbackFamily.faces.clear();
			(void)fonts.createFamily(fallbackFamily);
		}

		const auto packaged_font = locate_resource("fonts/Inter.arfont", config.resources);
		const std::array<std::filesystem::path, 1> fallbackCandidates{
			packaged_font ? *packaged_font : std::filesystem::path{}};

		for (const auto& fallbackPath : fallbackCandidates) {
			if (!std::filesystem::is_regular_file(fallbackPath))
				continue;
			FontFaceCreateInfo fallbackFace{};
			fallbackFace.path = fallbackPath;
			fallbackFace.pixelSize = config.ui.defaultFontFamily.faces.empty()
										 ? 18.0f
										 : config.ui.defaultFontFamily.faces.front().pixelSize;
			auto added = fonts.addFamilyFace(fallbackFamilyName, fallbackFace);
			if (!added) {
				continue;
			}
			detail::reportErrorEvent(ErrorEventView{
				.error =
					makeError(ErrorCode::DefaultFontUnavailable, ErrorSite::FontManagerInitialize),
				.kind = ErrorEventKind::Resolved,
				.resolution = ErrorResolution::UsedFallback,
			});
			defaultFontLoaded = true;
			break;
		}
		if (!defaultFontLoaded) {
			const auto added = fonts.addFamilyFace(fallbackFamilyName, defaults::missing_font());
			if (added) {
				defaultFontLoaded = true;
				detail::reportErrorEvent(ErrorEventView{
					.error = makeError(ErrorCode::DefaultFontUnavailable,
									   ErrorSite::FontManagerInitialize),
					.kind = ErrorEventKind::Resolved,
					.resolution = ErrorResolution::UsedFallback,
				});
			}
		}
	}

	if (!defaultFontLoaded && fonts.getFontById(0) == nullptr) {
		if (config.errors.policy.defaultFont == DefaultFontFailurePolicy::TryFallbackThenFailApp) {
			throw FlowUiException(
				makeError(ErrorCode::DefaultFontUnavailable, ErrorSite::FontManagerInitialize));
		}
		detail::reportErrorEvent(ErrorEventView{
			.error = makeError(ErrorCode::DefaultFontUnavailable, ErrorSite::FontManagerInitialize),
			.kind = ErrorEventKind::Resolved,
			.resolution = ErrorResolution::Skipped,
		});
	}
}

void App::Impl::init(App& owner) {
#if FLOW_UI_DEV_MODE
	FLOWUI_DEV_TIMING_ZONE(timingRecorder(), devSystems::TimingCategory::Lifecycle,
						   devSystems::TimingZoneRole::Work, "flowui.app.init");
#endif
	auto main = std::make_unique<AppWindow>(MainWindowId, makeMainWindowConfig(config), config);
	AppWindow* const mainPointer = main.get();
	windows.emplace(MainWindowId, std::move(main));

	mainPointer->backend =
		detail::makeDefaultWindowBackend(mainPointer->config.native, &mainPointer->inputQueue);
	mainPointer->backend->setInputConfig(mainPointer->config.native.input);
	mainPointer->ui.setClipboardAccessors(
		[mainPointer](std::string_view text) {
			if (mainPointer->backend)
				mainPointer->backend->setClipboardText(text);
		},
		[mainPointer]() -> std::string {
			return mainPointer->backend ? mainPointer->backend->getClipboardText() : std::string{};
		});
	mainPointer->ui.setCursorAccessor([mainPointer](CursorType cursorType) {
		if (mainPointer->backend)
			mainPointer->backend->setCursorType(cursorType);
	});

	vk.createInstance(config, mainPointer->backend->requiredInstanceExtensions());
	mainPointer->surface = vk.createSurface(*mainPointer->backend);
	vk.pickPhysicalDevice(config, mainPointer->surface);
	if (!vk.supportsPresentation(mainPointer->surface)) {
		throw FlowUiException(
			makeError(ErrorCode::WindowPresentationUnsupported, ErrorSite::AppInitialize));
	}
#if FLOW_UI_DEV_MODE
	vk.flags.set_flag(VulkanContext::Flag::dev_gpu_timing_requested,
		devMonitoring.timing().config().gpuTimingEnabled);
#if FLOWUI_DEV_MEMORY_LEVEL >= 1
	vk.flags.set_flag(VulkanContext::Flag::dev_gpu_memory_requested,
		devMonitoring.memory().config().gpuMemory);
#endif
#endif
	vk.createDevice(config);
#if FLOW_UI_AGENTIC_WORKFLOW
	auto agentic_configuration = config.agentic_workflow;
	VkPhysicalDeviceProperties agentic_device_properties{};
	vkGetPhysicalDeviceProperties(vk.phys, &agentic_device_properties);
	agentic_configuration.provenance.set("flowui_source_digest",
										 devSystems::agentic::build_info::source_digest);
	agentic_configuration.provenance.set("knowledge_pack_digest",
										 devSystems::agentic::build_info::knowledge_digest);
	agentic_configuration.provenance.set("flowui_working_tree_dirty",
										 devSystems::agentic::build_info::working_tree_dirty);
	agentic_configuration.provenance.set(
		"hardware",
		devSystems::agentic::Json::make_object(
			{{"device_name", agentic_device_properties.deviceName},
			 {"vendor_id", devSystems::agentic::Json::number(agentic_device_properties.vendorID)},
			 {"device_id", devSystems::agentic::Json::number(agentic_device_properties.deviceID)},
			 {"driver_version",
			  devSystems::agentic::Json::number(agentic_device_properties.driverVersion)}}));
	agentic_configuration.provenance.set("build_profile",
										 devSystems::agentic::Json::make_object(
											 {{"compiler", FLOWUI_AGENTIC_COMPILER},
											  {"configuration", FLOWUI_AGENTIC_BUILD_TYPE},
											  {"flags", FLOWUI_AGENTIC_BUILD_FLAGS},
											  {"flowui_version", FLOWUI_AGENTIC_LIBRARY_VERSION},
											  {"flowui_revision", FLOWUI_AGENTIC_LIBRARY_REVISION},
											  {"validation", config.vk.enableValidation}}));
	agentic_configuration.provenance.set(
		"window_configuration",
		devSystems::agentic::Json::make_object(
			{{"width", devSystems::agentic::Json::number(config.window.width)},
			 {"height", devSystems::agentic::Json::number(config.window.height)},
			 {"present_mode",
			  devSystems::agentic::Json::number(static_cast<uint64_t>(config.vk.presentMode))},
			 {"frame_slots", devSystems::agentic::Json::number(config.vk.framesInFlight)},
			 {"layout_dpi", devSystems::agentic::Json(static_cast<double>(config.ui.dpi))},
			 {"font_scale", devSystems::agentic::Json(static_cast<double>(config.ui.fontScale))}}));
	agentic_service = std::make_unique<devSystems::agentic::AgenticWorkflowService>(
		devSystems::agentic::AgenticDependencies{&owner, &devMonitoring, &devTooling, &themeManager,
												 &devInterface, &config.resources,
												 &timingRecorder()});
	const auto agentic_configured = agentic_service->configure(agentic_configuration);
	if (!agentic_configured)
		throw FlowUiException(agentic_configured.error());
	const auto agentic_resources = devSystems::agentic::frame_capture::configure_resources(
		vk, agentic_service->visual_capture_enabled());
	if (!agentic_resources)
		throw FlowUiException(agentic_resources.error());
#endif
#if FLOW_UI_DEV_MODE
	devMonitoring.gpuTiming().initialize(vk);
#if FLOWUI_DEV_MEMORY_LEVEL >= 1
	devMonitoring.memory().initializeEnvironmentProbes(vk);
#endif
#endif

	storageSystem = std::make_unique<storage::FlowStorageSystem>(vk);
	storageConfig = makeStorageConfig(config);
#if FLOW_UI_DEV_MODE && FLOWUI_DEV_MEMORY_LEVEL >= 2
	storageConfig.memory_demand_control = devMonitoring.memory().demand_control();
	capacity_demand.bind(storageConfig.memory_demand_control);
#endif
	storageSystem->initialize(storageConfig);
	pending_window_operations.reserve(16u);
	detail::reserve_initial_capacity(
		windows,
		storageSystem->initial_memory_capacity(FlowUi::detail::InitialCapacityTarget::app_windows));
	detail::reserve_initial_capacity(
		managedWindows,
		storageSystem->initial_memory_capacity(FlowUi::detail::InitialCapacityTarget::app_managed));
	detail::reserve_initial_capacity(dispatch_order,
									 storageSystem->initial_memory_capacity(
										 FlowUi::detail::InitialCapacityTarget::app_dispatch));
#if FLOW_UI_DEV_MODE && FLOWUI_DEV_MEMORY_LEVEL >= 1
	devMonitoring.memory().setStorageSystem(storageSystem.get());
#endif
#if FLOW_UI_DEV_MODE && FLOWUI_DEV_MEMORY_LEVEL >= 2
	fonts.setDevMemoryRecorder(&devMonitoring.memory().recorder());
	imageManager.setDevMemoryRecorder(&devMonitoring.memory().recorder());
#if FLOWUI_INCLUDE_ICON_MANAGER
	icons.setDevMemoryRecorder(&devMonitoring.memory().recorder());
#endif
#endif
	themeManager.init(*storageSystem);
#if FLOW_UI_DEV_MODE
	themeManager.setDevSchemaRegistry(&devTooling.schemas());
#endif
	const auto default_theme_status =
		themeManager.registerTheme<FlowUiTheme>("default", FlowUiTheme::dark(), true);
	if (!default_theme_status)
		throw FlowUiException(default_theme_status.error());
#if COMPILE_FSELI
	const auto standard_theme_status =
		themeManager.registerTheme<FSEL::FSELTheme>("default", FSEL::FSELTheme{}, true);
	if (!standard_theme_status)
		throw FlowUiException(standard_theme_status.error());
#endif
	actionManager.init(owner, *storageSystem);
	elementManager.init(owner, *storageSystem);
#if FLOW_UI_DEV_MODE
	elementManager.setDevTimingRecorder(&timingRecorder());
	elementManager.setDevSchemaRegistry(&devTooling.schemas());
#endif
	mainPointer->storageSystem = storageSystem.get();
	storageSystem->registerWindow(
		mainPointer->id, makeWindowStorageDesc(*storageSystem, storageConfig, mainPointer->config));
	mainPointer->flags.set_flag(AppWindow::Flag::storage_registered, true);
	elementManager.registerWindow(mainPointer->id);
	mainPointer->ui.initStorage(*storageSystem, mainPointer->id,
								makeUiManagerConfig(config, mainPointer->config));
#if FLOW_UI_DEV_MODE
	mainPointer->ui.setDevTimingRecorder(&timingRecorder());
	mainPointer->ui.setDevSchemaRegistry(&devTooling.schemas());
	mainPointer->ui.setDevOverrideEngine(&devTooling.overrides());
#endif
	mainPointer->ui.setThemeManager(&themeManager);
	mainPointer->ui.setImageManager(&imageManager);
#if FLOWUI_INCLUDE_ICON_MANAGER
	mainPointer->ui.setIconManager(&icons);
#endif
	actionManager.attachTo(mainPointer->ui);
	elementManager.attachTo(mainPointer->ui);
	const auto window_shortcut_status = install_default_window_shortcuts(*mainPointer);
	if (!window_shortcut_status)
		throw FlowUiException(window_shortcut_status.error());
	initSharedUiByteResources(*storageSystem, sharedUiByteResources);
	const storage::TextureHandle fallbackTexture = storageSystem->publishTexture(
		storage::ResourceKey{
			.domain = storage::ResourceDomain::Internal,
			.name = storageSystem->intern("flowui.ui.fallback"),
			.window = InvalidWindowId,
		},
		storage::TextureViewDesc{
			.imageView = sharedUiByteResources.placeholderUiView,
			.sampler = sharedUiByteResources.missing_texture_sampler,
			.sourceWidth = static_cast<int32_t>(defaults::missing_texture().width),
			.sourceHeight = static_cast<int32_t>(defaults::missing_texture().height),
		});
	storageSystem->setFallbackTexture(fallbackTexture);

	mainPointer->swapchain.create(mainPointer->config.native, mainPointer->config.vulkan, vk,
								  mainPointer->surface, mainPointer->backend->framebufferExtent());
	mainPointer->frames.create(mainPointer->config.vulkan.framesInFlight, vk);
#if FLOW_UI_DEV_MODE
	devMonitoring.timingReporting().noteFramesInFlight(mainPointer->config.vulkan.framesInFlight);
#endif
	mainPointer->observedFramebufferExtent = mainPointer->backend->framebufferExtent();

	mainPointer->renderer.init(
		mainPointer->config.vulkan, mainPointer->config.ui, vk,
		mainPointer->swapchain.swapchain.format, *storageSystem, mainPointer->id,
		sharedUiByteResources, storageConfig.initialInstanceBytesPerFrame,
		mainPointer->config.uiTextureDescriptorCapacity,
		config.errors.policy.transientCapacity == StorageCapacityPolicy::GrowWithinBudget,
		config.resources);
	lifecycle_flags.set_flag(LifecycleFlag::images_initialized, true);
	imageManager.init(*storageSystem, config.errors.policy.missingImage);
	mainPointer->viewPorts.init(*storageSystem, vk, mainPointer->id,
								mainPointer->config.vulkan.framesInFlight,
								config.errors.policy.missingViewport);
	lifecycle_flags.set_flag(LifecycleFlag::fonts_initialized, true);
	fonts.init(*storageSystem, config.ui.fontAtlasSize);
#if FLOWUI_INCLUDE_ICON_MANAGER
	lifecycle_flags.set_flag(LifecycleFlag::icons_initialized, true);
	icons.init(*storageSystem, config.iconManager, config.errors.policy.iconGeneration,
			   config.errors.policy.descriptorCapacity);
#if COMPILE_FSELI
	FSEL::standard_icons::registerStandardIcons(icons);
#endif
#endif
#if FLOW_UI_DEV_MODE
	devTooling.catalogues().bindManagers(storageSystem.get(), &themeManager, &actionManager, &fonts,
#if FLOWUI_INCLUDE_ICON_MANAGER
										 &icons,
#else
										 nullptr,
#endif
										 &imageManager, &devTooling.schemas());
	devTooling.catalogues().setDevTimingRecorder(&timingRecorder());
#endif
#if FLOW_UI_DEV_MODE && FLOWUI_DEV_MEMORY_LEVEL >= 2
	registerAppMemoryProbes();
	registerWindowMemoryProbe(*mainPointer);
#endif
	initializeDefaultFont();
#if FLOW_UI_DEV_MODE
	Status devInterfaceStatus = devInterface.initialize(mainPointer->ui, config.dev);
	if (!devInterfaceStatus)
		throw FlowUiException(devInterfaceStatus.error());
	if (config.dev.enabled && config.dev.capture_timing_on_startup)
		devMonitoring.performance_capture().request_startup_capture();
#endif
}

void App::Impl::requireQuiescent() const {
	if (activeWindowFrame != InvalidWindowId) {
		throw FlowUiException(makeError(ErrorCode::FrameAlreadyActive,
										ErrorSite::AppRequireQuiescent, activeWindowFrame));
	}
}

void App::Impl::requirePlatformThread() const {
	if (std::this_thread::get_id() != platformThread) {
		throw FlowUiException(
			makeError(ErrorCode::WrongThread, ErrorSite::AppRequirePlatformThread));
	}
}

void App::Impl::attachBackendAccessors(AppWindow& window) {
	AppWindow* const stable = &window;
	window.ui.setClipboardAccessors(
		[stable](std::string_view text) {
			if (stable->backend)
				stable->backend->setClipboardText(text);
		},
		[stable]() -> std::string {
			return stable->backend ? stable->backend->getClipboardText() : std::string{};
		});
	window.ui.setCursorAccessor([stable](CursorType cursorType) {
		if (stable->backend)
			stable->backend->setCursorType(cursorType);
	});
}

Result<WindowOperationCapabilities>
App::Impl::window_operation_capabilities(WindowId window_id) const noexcept {
	if (std::this_thread::get_id() != platformThread)
		return unexpectedError(
			makeError(ErrorCode::WrongThread, ErrorSite::AppRequirePlatformThread));
	const auto window_entry = windows.find(window_id);
	if (window_entry == windows.end() || !window_entry->second->backend)
		return unexpectedError(
			makeError(ErrorCode::InvalidWindowId, ErrorSite::AppAccessWindow, window_id));
	return window_entry->second->backend->window_operation_capabilities();
}

Status App::Impl::execute_window_operation(WindowId window_id,
										   NativeWindowOperation operation) noexcept {
	const auto window_entry = windows.find(window_id);
	if (window_entry == windows.end() || !window_entry->second->backend)
		return unexpectedError(
			makeError(ErrorCode::InvalidWindowId, ErrorSite::AppAccessWindow, window_id));
	auto& backend = *window_entry->second->backend;
	Status status = unexpectedError(
		makeError(ErrorCode::WindowOperationUnsupported, ErrorSite::AppAccessWindow, window_id));
	switch (operation) {
	case NativeWindowOperation::Close:
		status = backend.request_window_close();
		break;
	case NativeWindowOperation::Minimize:
		status = backend.minimize_window();
		break;
	case NativeWindowOperation::MaximizeRestore:
		status = backend.toggle_window_maximized();
		break;
	case NativeWindowOperation::Fullscreen:
		status = backend.toggle_window_fullscreen();
		break;
	}
	if (!status) {
		auto error = status.error();
		if (error.subjectKind() == ErrorSubjectKind::Window)
			error.subject = window_id;
		return unexpectedError(error);
	}
	return {};
}

Status App::Impl::request_window_operation(WindowId window_id,
										   NativeWindowOperation operation) noexcept {
	const auto capabilities = window_operation_capabilities(window_id);
	if (!capabilities)
		return unexpectedError(capabilities.error());
	const bool supported = operation == NativeWindowOperation::Close ? capabilities->close_window
						   : operation == NativeWindowOperation::Minimize ? capabilities->minimize
						   : operation == NativeWindowOperation::MaximizeRestore
							   ? capabilities->maximize_restore
							   : capabilities->toggle_fullscreen;
	if (!supported)
		return unexpectedError(makeError(ErrorCode::WindowOperationUnsupported,
										 ErrorSite::AppAccessWindow, window_id));
	if (operation == NativeWindowOperation::Close || activeWindowFrame == InvalidWindowId)
		return execute_window_operation(window_id, operation);
	try {
		pending_window_operations.emplace_back(PendingWindowOperation{window_id, operation});
		return {};
	} catch (const std::exception&) {
		return unexpectedError(
			makeError(ErrorCode::AllocationFailed, ErrorSite::AppAccessWindow, window_id));
	}
}

void App::Impl::drain_window_operations() noexcept {
	while (!pending_window_operations.empty() && activeWindowFrame == InvalidWindowId) {
		// Error observers can destroy a window and cancel its remaining requests.
		// Keep no queue references alive across execution or error reporting.
		const auto pending = pending_window_operations.front();
		pending_window_operations.erase(pending_window_operations.begin());
		const auto status = execute_window_operation(pending.window_id, pending.operation);
		if (!status)
			detail::reportErrorEvent(
				ErrorEventView{.error = status.error(), .kind = ErrorEventKind::Reported});
	}
}

Status App::Impl::install_default_window_shortcuts(AppWindow& window) {
	const auto& settings = window.config.ui.shortcuts.window;
	if (!settings.enabled)
		return {};
	struct PlannedShortcut {
		ShortcutChord chord{};
		NativeWindowOperation operation = NativeWindowOperation::Close;
		bool supported = false;
	};
	std::array<PlannedShortcut, 4> plan{};
	size_t plan_size = 0u;
	const auto capabilities = window.backend->window_operation_capabilities();
	const auto append = [&](const DefaultWindowShortcutBinding& binding,
							NativeWindowOperation operation, bool supported,
							std::optional<ShortcutChord> platform_chord) {
		if (binding.enabled) {
			const auto chord = binding.chord ? binding.chord : platform_chord;
			if (chord)
				plan[plan_size++] = PlannedShortcut{*chord, operation, supported};
		}
	};
#if defined(__APPLE__)
	append(settings.close_window, NativeWindowOperation::Close, capabilities.close_window,
		   ShortcutChord{.key = GLFW_KEY_W, .super = true});
	append(settings.minimize, NativeWindowOperation::Minimize, capabilities.minimize,
		   ShortcutChord{.key = GLFW_KEY_M, .super = true});
	const ShortcutChord fullscreen_chord{.key = GLFW_KEY_F, .ctrl = true, .super = true};
#else
	append(settings.close_window, NativeWindowOperation::Close, capabilities.close_window,
		   std::nullopt);
	append(settings.minimize, NativeWindowOperation::Minimize, capabilities.minimize, std::nullopt);
	const ShortcutChord fullscreen_chord{.key = GLFW_KEY_F11};
#endif
	append(settings.maximize_restore, NativeWindowOperation::MaximizeRestore,
		   capabilities.maximize_restore, std::nullopt);
	append(settings.toggle_fullscreen, NativeWindowOperation::Fullscreen,
		   capabilities.toggle_fullscreen, fullscreen_chord);
	const auto valid_key = [](int key) noexcept {
		return key == GLFW_KEY_SPACE || key == GLFW_KEY_APOSTROPHE ||
			   (key >= GLFW_KEY_COMMA && key <= GLFW_KEY_9) || key == GLFW_KEY_SEMICOLON ||
			   key == GLFW_KEY_EQUAL || (key >= GLFW_KEY_A && key <= GLFW_KEY_RIGHT_BRACKET) ||
			   key == GLFW_KEY_GRAVE_ACCENT || key == GLFW_KEY_WORLD_1 || key == GLFW_KEY_WORLD_2 ||
			   (key >= GLFW_KEY_ESCAPE && key <= GLFW_KEY_END) ||
			   (key >= GLFW_KEY_CAPS_LOCK && key <= GLFW_KEY_PAUSE) ||
			   (key >= GLFW_KEY_F1 && key <= GLFW_KEY_F25) ||
			   (key >= GLFW_KEY_KP_0 && key <= GLFW_KEY_KP_EQUAL) ||
			   (key >= GLFW_KEY_LEFT_SHIFT && key <= GLFW_KEY_MENU);
	};
	for (size_t candidate_index = 0u; candidate_index < plan_size; ++candidate_index) {
		const auto& chord = plan[candidate_index].chord;
		if (!valid_key(chord.key) || chord.trigger != ShortcutTrigger::Press)
			return unexpectedError(
				makeError(ErrorCode::ShortcutInvalid, ErrorSite::ShortcutRegister));
		for (size_t previous_index = 0u; previous_index < candidate_index; ++previous_index) {
			const auto& previous = plan[previous_index].chord;
			if (chord.key == previous.key && chord.ctrl == previous.ctrl &&
				chord.shift == previous.shift && chord.alt == previous.alt &&
				chord.super == previous.super)
				return unexpectedError(
					makeError(ErrorCode::ShortcutInvalid, ErrorSite::ShortcutRegister));
		}
	}
	std::array<ShortcutId, 4> installed{};
	size_t installed_count = 0u;
	const auto rollback = [&] {
		for (size_t installed_index = 0u; installed_index < installed_count; ++installed_index)
			if (!window.ui.shortcuts().unregisterShortcut(installed[installed_index]))
				detail::reportErrorEvent(ErrorEventView{
					.error = makeError(ErrorCode::ShortcutNotFound, ErrorSite::ShortcutRemove,
									   installed[installed_index]),
					.kind = ErrorEventKind::Reported});
	};
	try {
		for (size_t plan_index = 0u; plan_index < plan_size; ++plan_index) {
			const auto& entry = plan[plan_index];
			if (!entry.supported)
				continue;
			const auto registered = window.ui.shortcuts().registerShortcut(
				entry.chord, ShortcutScope::Global, settings.priority,
				[this, operation = entry.operation](ShortcutContext& context) noexcept {
					const auto capabilities = window_operation_capabilities(context.ui.windowId());
					if (!capabilities && capabilities.error().code == ErrorCode::InvalidWindowId)
						return false;
					if (capabilities) {
						const bool supported =
							operation == NativeWindowOperation::Close ? capabilities->close_window
							: operation == NativeWindowOperation::Minimize ? capabilities->minimize
							: operation == NativeWindowOperation::MaximizeRestore
								? capabilities->maximize_restore
								: capabilities->toggle_fullscreen;
						if (!supported)
							return false;
					}
					const auto status = request_window_operation(context.ui.windowId(), operation);
					if (status)
						return true;
					detail::reportErrorEvent(
						ErrorEventView{.error = status.error(), .kind = ErrorEventKind::Reported});
					return true;
				});
			if (!registered) {
				rollback();
				return unexpectedError(registered.error());
			}
			installed[installed_count++] = *registered;
		}
	} catch (...) {
		rollback();
		throw;
	}
	return {};
}

void App::Impl::destroyUnsubmittedWindow(AppWindow& window) noexcept {
	if (window.backend)
		window.backend->detachCallbacks();
	try {
		window.viewPorts.destroyDrained(vk);
	} catch (...) {}
	if (storageSystem) {
		try {
			window.renderer.destroy(vk, *storageSystem, window.lastSubmissionSerial);
		} catch (...) {}
	}
	window.frames.destroy(vk);
	for (SwapchainGeneration& generation : window.retiredSwapchains)
		generation.destroy(vk);
	window.retiredSwapchains.clear();
	window.swapchain.destroy(vk);
	elementManager.destroyWindow(window.id);
	window.ui.destroyStorage();
	if (storageSystem && window.flags.has_flag(AppWindow::Flag::storage_registered)) {
		try {
			storageSystem->unregisterWindow(window.id, window.lastSubmissionSerial);
		} catch (...) {}
		window.flags.set_flag(AppWindow::Flag::storage_registered, false);
		try {
			storageSystem->collect();
		} catch (...) {}
	}
	if (window.surface != VK_NULL_HANDLE && vk.instance != VK_NULL_HANDLE) {
		vkDestroySurfaceKHR(vk.instance, window.surface, nullptr);
		window.surface = VK_NULL_HANDLE;
	}
	window.backend.reset();
}

WindowId App::Impl::reserveWindowId() {
	if (lifecycle_flags.has_flag(LifecycleFlag::window_ids_exhausted)) {
		detail::terminateForFatalError(
			makeError(ErrorCode::WindowIdSpaceExhausted, ErrorSite::AppCreateWindow));
	}
	const WindowId id = nextWindowId;
	if (id == InvalidWindowId || id == MainWindowId) {
		detail::terminateForFatalError(
			makeError(ErrorCode::WindowIdSpaceExhausted, ErrorSite::AppCreateWindow));
	}
	if (id == std::numeric_limits<WindowId>::max()) {
		lifecycle_flags.set_flag(LifecycleFlag::window_ids_exhausted, true);
	} else {
		++nextWindowId;
	}
	return id;
}

WindowId App::Impl::createWindow(const WindowConfig& nativeConfig) {
	requirePlatformThread();
	requireQuiescent();
	if (nativeConfig.width <= 0 || nativeConfig.height <= 0) {
		throw FlowUiException(
			makeError(ErrorCode::InvalidWindowConfiguration, ErrorSite::AppCreateWindow));
	}
	const WindowId id = reserveWindowId();
#if FLOW_UI_DEV_MODE
	FLOWUI_DEV_TIMING_ZONE_ENTITY(timingRecorder(), devSystems::TimingCategory::Lifecycle,
								  devSystems::TimingZoneRole::Work, "flowui.window.create",
								  devSystems::TimingEntityRef::window(id));
#endif
	auto pending = std::make_unique<AppWindow>(id, makeWindowConfig(config, nativeConfig), config);
	try {
		pending->backend =
			detail::makeDefaultWindowBackend(pending->config.native, &pending->inputQueue);
		pending->backend->setInputConfig(pending->config.native.input);
		attachBackendAccessors(*pending);
		pending->surface = vk.createSurface(*pending->backend);
		if (!vk.supportsPresentation(pending->surface)) {
			throw FlowUiException(makeError(ErrorCode::WindowPresentationUnsupported,
											ErrorSite::AppCreateWindow, id));
		}
		validateSecondarySurface(vk, pending->surface);
		if (!vk.supportsExactPresentCompletion()) {
			throw FlowUiException(makeError(ErrorCode::WindowPresentationUnsupported,
											ErrorSite::AppCreateWindow, id));
		}

		pending->storageSystem = storageSystem.get();
		storageSystem->registerWindow(
			id, makeWindowStorageDesc(*storageSystem, storageConfig, pending->config));
		pending->flags.set_flag(AppWindow::Flag::storage_registered, true);
		elementManager.registerWindow(id);
		pending->ui.initStorage(*storageSystem, id, makeUiManagerConfig(config, pending->config));
#if FLOW_UI_DEV_MODE
		pending->ui.setDevTimingRecorder(&timingRecorder());
		pending->ui.setDevSchemaRegistry(&devTooling.schemas());
		pending->ui.setDevOverrideEngine(&devTooling.overrides());
#endif
		pending->ui.setThemeManager(&themeManager);
		pending->ui.setImageManager(&imageManager);
#if FLOWUI_INCLUDE_ICON_MANAGER
		pending->ui.setIconManager(&icons);
#endif
		actionManager.attachTo(pending->ui);
		elementManager.attachTo(pending->ui);
		const auto window_shortcut_status = install_default_window_shortcuts(*pending);
		if (!window_shortcut_status)
			throw FlowUiException(window_shortcut_status.error());
#if FLOW_UI_DEV_MODE

#endif
		pending->swapchain.create(pending->config.native, pending->config.vulkan, vk,
								  pending->surface, pending->backend->framebufferExtent());
		pending->frames.create(pending->config.vulkan.framesInFlight, vk);
#if FLOW_UI_DEV_MODE
		devMonitoring.timingReporting().noteFramesInFlight(pending->config.vulkan.framesInFlight);
#endif
		pending->observedFramebufferExtent = pending->backend->framebufferExtent();
#if FLOW_UI_DEV_MODE
		Status dev_interface_status = devInterface.attachWindow(pending->ui);
		if (!dev_interface_status)
			throw FlowUiException(dev_interface_status.error());
#endif
		pending->renderer.init(
			pending->config.vulkan, pending->config.ui, vk, pending->swapchain.swapchain.format,
			*storageSystem, id, sharedUiByteResources, storageConfig.initialInstanceBytesPerFrame,
			pending->config.uiTextureDescriptorCapacity,
			config.errors.policy.transientCapacity == StorageCapacityPolicy::GrowWithinBudget,
			config.resources);
		pending->viewPorts.init(*storageSystem, vk, id, pending->config.vulkan.framesInFlight,
								config.errors.policy.missingViewport);

		auto [entry, inserted] = windows.try_emplace(id);
		if (!inserted) {
			detail::terminateForFatalError(
				makeError(ErrorCode::WindowRegistryCollision, ErrorSite::AppCreateWindow, id));
		}
		entry->second = std::move(pending);
#if FLOW_UI_DEV_MODE && FLOWUI_DEV_MEMORY_LEVEL >= 2
		registerWindowMemoryProbe(*entry->second);
#endif
		return id;
	} catch (...) {
		if (pending)
			destroyUnsubmittedWindow(*pending);
		throw;
	}
}

void App::Impl::pollEventsAndAdvanceSharedManagers() {
	requirePlatformThread();
#if FLOW_UI_AGENTIC_WORKFLOW
	if (dispatch_flags.has_flag(DispatchFlag::advancing_agentic))
		throw FlowUiException(makeError(ErrorCode::AgenticBusy, ErrorSite::AgenticScenario));
#endif
	requireQuiescent();
	drain_window_operations();
#if FLOW_UI_DEV_MODE
	{
		// Reporting closes the previous cadence interval, after its window frames.
		// Frame teardown clears the recorder context, so restore the owning tick.
		timingRecorder().setFrameContext({}, appTick);
		TimingContextExitGuard reporting_context_exit(timingRecorder());
		FLOWUI_DEV_TIMING_ZONE(timingRecorder(), devSystems::TimingCategory::DevTool,
							   devSystems::TimingZoneRole::DevToolWork,
							   "flowui.dev.reporting.consume");
		{
			FLOWUI_DEV_TIMING_ZONE(timingRecorder(), devSystems::TimingCategory::DevTool,
								   devSystems::TimingZoneRole::DevToolWork,
								   "flowui.dev.reporting.timing");
			devMonitoring.timingReporting().consumeThrough(appTick);
		}
		{
			FLOWUI_DEV_TIMING_ZONE(timingRecorder(), devSystems::TimingCategory::DevTool,
								   devSystems::TimingZoneRole::DevToolWork,
								   "flowui.dev.reporting.memory");
			devMonitoring.memoryReporting().consume(appTick);
		}
		{
			FLOWUI_DEV_TIMING_ZONE(timingRecorder(), devSystems::TimingCategory::DevTool,
								   devSystems::TimingZoneRole::DevToolWork,
								   "flowui.dev.reporting.errors");
			devMonitoring.errorReporting().consumeThrough(appTick);
		}
	}
	if (appTick == std::numeric_limits<devSystems::AppTickId>::max()) {
		detail::terminateForFatalError(
			makeError(ErrorCode::AppTickSpaceExhausted, ErrorSite::AppPollEvents));
	}
	++appTick;
	devMonitoring.timingReporting().note_tick_boundary(appTick, devMonitoring.timing().nowNs());
#if FLOWUI_DEV_MEMORY_LEVEL >= 1
	devMonitoring.memory().recorder().setAppTickContext(appTick);
	devMonitoring.memory().advanceGpuFrameIndex(static_cast<uint32_t>(appTick));
#endif
	timingRecorder().setFrameContext({}, appTick);
	errorRecorder().setContext(devSystems::DevErrorContext{
		.appTick = appTick,
		.timingTrack = timingRecorder().trackId(),
	});
	TimingContextExitGuard timingContextExit(timingRecorder());
	FLOWUI_DEV_TIMING_ZONE(timingRecorder(), devSystems::TimingCategory::Lifecycle,
						   devSystems::TimingZoneRole::Work, "flowui.app.poll_events");
#endif
#if FLOW_UI_DEV_MODE
	for (auto& [window_id, window] : windows)
		window->inputQueue.dev_shortcut_presses.fill(0);
#endif
	detail::pollWindowSystemEvents();
	themeManager.applyStagedMutations();
#if FLOW_UI_DEV_MODE
	devTooling.commitAtSafePoint(themeManager, &timingRecorder());
	for (auto& [window_id, window] : windows)
		window->viewPorts.collect_dev_captures();
#endif
	if (storageSystem)
		storageSystem->collect();
#if FLOWUI_INCLUDE_ICON_MANAGER
	if (lifecycle_flags.has_flag(LifecycleFlag::icons_initialized))
		icons.beginAppTick();
#endif
}

void App::Impl::pollEvents() {
	pollEventsAndAdvanceSharedManagers();
}

void App::Impl::cancelStorageFrame(AppWindow& window
#if FLOW_UI_DEV_MODE
								   ,
								   devSystems::TimingRecordFlag timingResult
#endif
								   ) noexcept {
	if (window.storageFrame) {
#if FLOW_UI_DEV_MODE
		devSystems::recordGlobalDevBreadcrumb(kDevStorageFrameCancel, window.id,
											  window.storageFrame.epoch);
#endif
		elementManager.cancelWindowFrame(window.id, window.storageFrame.epoch);
		if (storageSystem)
			storageSystem->cancelFrame(window.storageFrame);
	}
#if FLOW_UI_AGENTIC_WORKFLOW
	devSystems::agentic::frame_capture::cancel_frame(vk, window.id, window.frames.currentFrame);
	if (agentic_service && window.agentic_frame.key.window_frame) {
		window.agentic_frame.canceled = true;
		window.agentic_frame.end_ns = devSystems::agentic::monotonic_nanoseconds();
		agentic_service->observe_frame(std::move(window.agentic_frame));
		window.agentic_frame = {};
	}
#endif
	window.ui.cancelFrameState();
	window.preparedUi = {};
	window.storageReadLease = {};
	window.storageFrame = {};
	if (window.phase != AppWindow::Phase::Closing)
		window.phase = AppWindow::Phase::Idle;
	if (activeWindowFrame == window.id)
		activeWindowFrame = InvalidWindowId;
#if FLOW_UI_DEV_MODE
	devSystems::recordGlobalDevBreadcrumb(kDevFrameCancel, window.id, window.frameNumber);
	window.preparedGapTiming.end(timingResult);
	window.userBuildTiming.end(timingResult);
	window.frameTotalTiming.end(timingResult);
	window.timingFrame = {};
	window.timingAppTick = 0u;
	timingRecorder().clearFrameContext();
	errorRecorder().clearFrameContext();
#endif
}

void App::Impl::completeSubmission(FrameVk::Frame& frame) {
	if (!storageSystem || !frame.storageSubmission)
		return;
	storageSystem->noteCompleted(frame.storageSubmission);
	frame.storageSubmission = {};
}

void App::Impl::completeAllSubmissionsAfterIdle() {
	if (!storageSystem)
		return;
	for (auto& [_, window] : windows) {
		for (FrameVk::Frame& frame : window->frames.frames) {
#if FLOW_UI_DEV_MODE
			devMonitoring.gpuTiming().resolveCompleted(vk, frame.gpuTiming);
#endif
			completeSubmission(frame);
		}
	}
	storageSystem->collect();
}

void App::Impl::drainWindowGraphics(AppWindow& window) {
	for (FrameVk::Frame& frame : window.frames.frames) {
		vkCheck(vkWaitForFences(vk.device, 1, &frame.inFlight, VK_TRUE, UINT64_MAX),
				ErrorSite::AppSubmitFrame);
#if FLOW_UI_DEV_MODE
		devMonitoring.gpuTiming().resolveCompleted(vk, frame.gpuTiming);
#endif
		completeSubmission(frame);
	}
	if (storageSystem && !window.storageFrame) {
		storageSystem->collect();
	}
}

void App::Impl::collectRetiredSwapchains(AppWindow& window) {
	for (SwapchainGeneration& generation : window.retiredSwapchains) {
		generation.waitForPresentCompletion(vk);
		generation.destroy(vk);
	}
	window.retiredSwapchains.clear();
}

void App::Impl::beginFrame(WindowId id) {
	requirePlatformThread();
#if FLOW_UI_AGENTIC_WORKFLOW
	if (dispatch_flags.has_flag(DispatchFlag::advancing_agentic))
		throw FlowUiException(makeError(ErrorCode::AgenticBusy, ErrorSite::AgenticScenario));
#endif
	AppWindow& window = requireWindow(id);
	if (window.phase != AppWindow::Phase::Idle) {
		throw FlowUiException(
			makeError(ErrorCode::FramePhaseViolation, ErrorSite::AppBeginFrame, id));
	}
	if (activeWindowFrame != InvalidWindowId) {
		throw FlowUiException(
			makeError(ErrorCode::FrameAlreadyActive, ErrorSite::AppBeginFrame, activeWindowFrame));
	}
	if (!window.backend || window.frames.frames.empty()) {
		throw FlowUiException(makeError(ErrorCode::FrameNotReady, ErrorSite::AppBeginFrame, id));
	}
	if (window.frameNumber == std::numeric_limits<uint64_t>::max()) {
		detail::terminateForFatalError(
			makeError(ErrorCode::FrameNumberSpaceExhausted, ErrorSite::AppBeginFrame, id));
	}
	const uint64_t nextFrameNumber = window.frameNumber + 1u;
#if FLOW_UI_AGENTIC_WORKFLOW
	window.frameNumber = nextFrameNumber;
	window.agentic_frame = {};
	window.agentic_frame.key = {{}, appTick, nextFrameNumber, window.id};
	window.agentic_frame.begin_ns = devSystems::agentic::monotonic_nanoseconds();
	window.agentic_frame.frame_slot = window.frames.currentFrame;
	window.agentic_frame.width = window.swapchain.swapchain.extent.width;
	window.agentic_frame.height = window.swapchain.swapchain.extent.height;
	window.agentic_frame.internal_window = window.id == devInterface.windowId();
	window.agentic_frame.interrupted = window.flags.has_flag(AppWindow::Flag::framebuffer_resized);
	if (auto* native = static_cast<GLFWwindow*>(window.backend->nativeHandle()))
		window.agentic_frame.interrupted = window.agentic_frame.interrupted ||
										   glfwGetWindowAttrib(native, GLFW_VISIBLE) != GLFW_TRUE ||
										   glfwGetWindowAttrib(native, GLFW_ICONIFIED) == GLFW_TRUE;
#endif

#if FLOW_UI_DEV_MODE
	window.timingFrame = devSystems::WindowFrameKey{window.id, nextFrameNumber};
	window.timingAppTick = appTick;
	timingRecorder().setFrameContext(window.timingFrame, window.timingAppTick);
	errorRecorder().setContext(devSystems::DevErrorContext{
		.appTick = window.timingAppTick,
		.frame = window.timingFrame,
		.primaryEntity = window.id,
		.timingTrack = timingRecorder().trackId(),
	});
	devSystems::recordGlobalDevBreadcrumb(kDevFrameBegin, window.id, nextFrameNumber);
	window.frameTotalTiming.begin(timingRecorder(), devSystems::timing_zones::kWindowFrameTotal,
								  devSystems::TimingEntityRef::window(window.id));
#endif
	try {
#if FLOW_UI_DEV_MODE
		devSystems::ManualTimingZone beginTiming(timingRecorder(),
												 devSystems::timing_zones::kWindowFrameBegin,
												 devSystems::TimingEntityRef::window(window.id));
#endif
		{
#if FLOW_UI_DEV_MODE
			FLOWUI_DEV_TIMING_ZONE_BALANCED(timingRecorder(), devSystems::TimingCategory::Input,
											devSystems::TimingZoneRole::Work,
											"flowui.frame.input_refresh");
#endif
			window.backend->refreshInputSnapshot();
		}
		const auto now = std::chrono::steady_clock::now();
		double deltaTimeSeconds = 0.0;
		if (window.flags.has_flag(AppWindow::Flag::has_previous_begin_frame_timestamp)) {
			deltaTimeSeconds = std::max(
				0.0,
				std::chrono::duration<double>(now - window.previousBeginFrameTimestamp).count());
		}
		window.previousBeginFrameTimestamp = now;
		window.flags.set_flag(AppWindow::Flag::has_previous_begin_frame_timestamp, true);

		FrameVk::Frame& frame = window.frames.getCurrentFrame();
		{
#if FLOW_UI_DEV_MODE
			FLOWUI_DEV_TIMING_ZONE(timingRecorder(), devSystems::TimingCategory::Wait,
								   devSystems::TimingZoneRole::Wait,
								   "flowui.wait.frame_slot_fence");
#endif
			vkCheck(vkWaitForFences(vk.device, 1, &frame.inFlight, VK_TRUE, UINT64_MAX),
					ErrorSite::AppAcquireFrame);
		}
#if FLOW_UI_DEV_MODE
		devMonitoring.gpuTiming().resolveCompleted(vk, frame.gpuTiming);
#endif
		{
#if FLOW_UI_DEV_MODE
			FLOWUI_DEV_TIMING_ZONE_BALANCED(timingRecorder(), devSystems::TimingCategory::Prepare,
											devSystems::TimingZoneRole::Work,
											"flowui.storage.complete_collect");
#endif
			completeSubmission(frame);
			storageSystem->collect();
		}

#if FLOW_UI_DEV_MODE && FLOWUI_PUBLIC_VULKAN_INTEROP
		if (!window.memory_history_controller)
			window.memory_history_controller =
				std::make_unique<devSystems::interface_elements::DevMemoryHistoryController>();
		window.ui.memory_history_controller_ = window.memory_history_controller.get();
		window.memory_history_controller->begin_frame(window.viewPorts, window.renderer,
													  storageSystem->completedSerial());
		if (!window.timeline_controller)
			window.timeline_controller =
				std::make_unique<devSystems::interface_elements::DevTimelineController>();
		window.ui.timeline_controller_ = window.timeline_controller.get();
		window.timeline_controller->begin_frame(window.viewPorts, window.renderer,
												storageSystem->completedSerial());
#endif
		const uint32_t frameSlot = window.frames.currentFrame;
#if FLOW_UI_AGENTIC_WORKFLOW
		devSystems::agentic::frame_capture::complete_slot(vk, window.id, frameSlot);
#endif
		{
#if FLOW_UI_DEV_MODE
			FLOWUI_DEV_TIMING_ZONE_BALANCED(timingRecorder(), devSystems::TimingCategory::Prepare,
											devSystems::TimingZoneRole::Work,
											"flowui.viewport.frame_start");
#endif
			window.viewPorts.onFrameStart(vk, frameSlot);
		}

		{
#if FLOW_UI_DEV_MODE
			FLOWUI_DEV_TIMING_ZONE_BALANCED(timingRecorder(), devSystems::TimingCategory::Prepare,
											devSystems::TimingZoneRole::Work,
											"flowui.storage.begin_frame");
#endif
			window.storageFrame =
				storageSystem->beginFrame(window.id, {
														 .frameSlot = frameSlot,
														 .frameNumber = nextFrameNumber,
													 });
#if FLOW_UI_DEV_MODE
			devSystems::recordGlobalDevBreadcrumb(kDevStorageFrameBegin, window.id,
												  window.storageFrame.epoch);
#endif
			window.fontFrameView = fonts.frameView(window.storageFrame);
		}
		window.frameNumber = nextFrameNumber;
		activeWindowFrame = window.id;
		window.phase = AppWindow::Phase::Building;

		{
#if FLOW_UI_DEV_MODE
			FLOWUI_DEV_TIMING_ZONE_BALANCED(timingRecorder(), devSystems::TimingCategory::Element,
											devSystems::TimingZoneRole::Work,
											"flowui.element.begin_window_frame");
#endif
			elementManager.beginWindowFrame(window.id, window.storageFrame.epoch);
		}
		{
#if FLOW_UI_DEV_MODE
			FLOWUI_DEV_TIMING_ZONE_BALANCED(timingRecorder(), devSystems::TimingCategory::Input,
											devSystems::TimingZoneRole::Work, "flowui.input.drain");
#endif
			window.frameInput = window.inputQueue.drain(deltaTimeSeconds);
			if (window.frameInput.droppedTextInputCount != 0u) {
				detail::reportErrorEvent(ErrorEventView{
					.error =
						makeError(ErrorCode::PlatformInputQueueOverflow, ErrorSite::InputQueueEvent,
								  window.id, window.frameInput.droppedTextInputCount),
					.kind = ErrorEventKind::Resolved,
					.resolution = ErrorResolution::Ignored,
				});
			}
		}
		const float inverseClampedUiScale = 1 / std::max(1.0e-6f, window.config.ui.uiScale);
#if FLOW_UI_AGENTIC_WORKFLOW
		if (agentic_service) {
			auto selected = agentic_service->select_frame_input(
				window.agentic_frame.key, window.frameInput, window.config.ui.uiScale);
			if (!selected)
				throw FlowUiException(selected.error());
		}
#endif
		constexpr float kBaseScrollSensitivity = 20.0f;
		const VkExtent2D windowExtent = window.backend->windowExtent();
		const VkExtent2D framebufferExtent = window.backend->framebufferExtent();
		if (framebufferExtent.width != window.observedFramebufferExtent.width ||
			framebufferExtent.height != window.observedFramebufferExtent.height) {
			window.flags.set_flag(AppWindow::Flag::framebuffer_resized, true);
			window.observedFramebufferExtent = framebufferExtent;
		}
		const float logicalWidth = static_cast<float>(std::max(1u, windowExtent.width));
		const float logicalHeight = static_cast<float>(std::max(1u, windowExtent.height));
		const float framebufferWidth = static_cast<float>(std::max(1u, framebufferExtent.width));
		const float framebufferHeight = static_cast<float>(std::max(1u, framebufferExtent.height));
		const float layoutWidth = logicalWidth * inverseClampedUiScale;
		const float layoutHeight = logicalHeight * inverseClampedUiScale;
		window.uiToFramebufferScaleX = framebufferWidth / std::max(layoutWidth, 1.0e-6f);
		window.uiToFramebufferScaleY = framebufferHeight / std::max(layoutHeight, 1.0e-6f);

		FrameInput layoutInput = window.frameInput;
		layoutInput.mouseX *= inverseClampedUiScale;
		layoutInput.mouseY *= inverseClampedUiScale;
		layoutInput.scrollX *= inverseClampedUiScale * kBaseScrollSensitivity;
		layoutInput.scrollY *= inverseClampedUiScale * kBaseScrollSensitivity;
		if (layoutInput.shift && layoutInput.scrollY != 0.0f) {
			layoutInput.scrollX += layoutInput.scrollY;
			layoutInput.scrollY = 0.0f;
		}
#if FLOW_UI_DEV_MODE
		devTooling.inspect_interaction().set_interface_window(devInterface.windowId());
		if (!devMonitoring.performance_capture().interface_work_suspended()
#if FLOW_UI_AGENTIC_WORKFLOW
			&& !(agentic_service && agentic_service->interaction_controls_window(window.id))
#endif
		)
			devTooling.inspect_interaction().filter_input(window.id, window.ui.devTreeSnapshot(),
														  layoutInput, window.inspect_pointer);
#endif
#if FLOW_UI_DEV_MODE
		window.ui.set_dev_evidence_context(appTick, nextFrameNumber);
#if FLOW_UI_AGENTIC_WORKFLOW
		if (agentic_service)
			agentic_service->prepare_window_collection(window.id);
#endif
#endif
		window.ui.beginFrame(window.storageFrame, layoutInput, window.fontFrameView, layoutWidth,
							 layoutHeight);
#if FLOW_UI_DEV_MODE
		if (!devMonitoring.performance_capture().interface_work_suspended() &&
#if FLOW_UI_AGENTIC_WORKFLOW
			!(agentic_service && agentic_service->interaction_controls_window(window.id)) &&
#endif
			devTooling.inspect_interaction().picking() &&
			devTooling.inspect_interaction().eligible_window(window.id) &&
			layoutInput.pointerInside)
			window.ui.requestCursor(CursorType::Crosshair, 255);
#endif
#if FLOW_UI_DEV_MODE
		beginTiming.end();
		window.userBuildTiming.begin(timingRecorder(),
									 devSystems::timing_zones::kWindowFrameUserBuild,
									 devSystems::TimingEntityRef::window(window.id));
#endif
	} catch (...) {
		cancelStorageFrame(window
#if FLOW_UI_DEV_MODE
						   ,
						   devSystems::TimingRecordFlag::Exception
#endif
		);
		throw;
	}
}

void App::Impl::endFrame(WindowId id) {
	requirePlatformThread();
	AppWindow& window = requireWindow(id);
	if (activeWindowFrame != id || window.phase != AppWindow::Phase::Building ||
		!window.storageFrame) {
		throw FlowUiException(
			makeError(ErrorCode::FramePhaseViolation, ErrorSite::AppEndFrame, id));
	}
#if FLOW_UI_DEV_MODE
	window.userBuildTiming.end();
#endif
	try {
#if FLOW_UI_DEV_MODE
		devSystems::ManualTimingZone endTiming(timingRecorder(),
											   devSystems::timing_zones::kWindowFrameEnd,
											   devSystems::TimingEntityRef::window(window.id));
#endif
		window.renderCommands = window.ui.endFrame();
		{
#if FLOW_UI_DEV_MODE
			FLOWUI_DEV_TIMING_ZONE_BALANCED(timingRecorder(), devSystems::TimingCategory::Prepare,
											devSystems::TimingZoneRole::Work,
											"flowui.viewport.prepare_targets");
#endif
			window.viewPorts.prepareFrameTargets(
				window.renderCommands, window.uiToFramebufferScaleX, window.uiToFramebufferScaleY);
#if FLOW_UI_DEV_MODE
			window.viewPorts.prepare_dev_captures(window.storageFrame);
#endif
		}
#if FLOWUI_INCLUDE_ICON_MANAGER
		{
#if FLOW_UI_DEV_MODE
			FLOWUI_DEV_TIMING_ZONE_BALANCED(timingRecorder(), devSystems::TimingCategory::Prepare,
											devSystems::TimingZoneRole::Work,
											"flowui.icon.prepare_textures");
#endif
			icons.prepareFrameTextures(window.renderCommands, window.uiToFramebufferScaleX,
									   window.uiToFramebufferScaleY);
		}
#endif
		window.viewPorts.remapRenderCommandsForFrame(window.renderCommands,
													 window.frames.currentFrame);
		storage::ArenaView textureArena{};
		std::span<storage::TextureHandle> gathered{};
		size_t gatheredCount = 0;
		{
#if FLOW_UI_DEV_MODE
			FLOWUI_DEV_TIMING_ZONE_BALANCED(timingRecorder(), devSystems::TimingCategory::Prepare,
											devSystems::TimingZoneRole::Work,
											"flowui.texture.gather_bindings");
#endif
			textureArena = storageSystem->frameArena(window.storageFrame,
													 storage::MemoryClass::FrameTransient);
			gathered = textureArena.allocateArray<storage::TextureHandle>(
				static_cast<size_t>(std::max(0, window.renderCommands.length)));
			for (int32_t commandIndex = 0; commandIndex < window.renderCommands.length;
				 ++commandIndex) {
				const Clay_RenderCommand& command =
					window.renderCommands.internalArray[commandIndex];
				if (command.commandType != CLAY_RENDER_COMMAND_TYPE_IMAGE)
					continue;
				const auto* texture =
					static_cast<const TextureRef*>(command.renderData.image.imageData);
				if (!texture || !texture->handle)
					continue;
				bool duplicate = false;
				for (size_t i = 0; i < gatheredCount; ++i) {
					if (gathered[i] == texture->handle) {
						duplicate = true;
						break;
					}
				}
				if (!duplicate)
					gathered[gatheredCount++] = texture->handle;
			}
		}
		storage::PreparedTextureBindings preparedBindings{};
		{
#if FLOW_UI_DEV_MODE
			FLOWUI_DEV_TIMING_ZONE_BALANCED(timingRecorder(), devSystems::TimingCategory::Prepare,
											devSystems::TimingZoneRole::Work,
											"flowui.texture.prepare_bindings");
#endif
			preparedBindings = storageSystem->prepareTextureBindings(window.storageFrame,
																	 gathered.first(gatheredCount));
			window.renderer.applyTextureBindings(vk.device, window.frames.currentFrame,
												 preparedBindings);
			storageSystem->acknowledgeTextureBindings(window.storageFrame,
													  preparedBindings.dirtyBindings);
		}
#if FLOW_UI_DEV_MODE
		if (!devMonitoring.performance_capture().interface_work_suspended() &&
			!window.devReplayRequest.commandIds.empty()) {
			captureDevUiReplayPacket(window.devReplayRequest, window.renderCommands,
									 preparedBindings, window.storageFrame.frameSlot,
									 window.devReplayPacket);
			window.devReplayRequest = {};
		}
		window.devOverlay.clear();
		devSystems::tooling::DevOverlaySelectionSpec overlaySelection{};
		if (!devMonitoring.performance_capture().interface_work_suspended()
#if FLOW_UI_AGENTIC_WORKFLOW
			&& !(agentic_service && agentic_service->interaction_controls_window(window.id))
#endif
		)
			devTooling.inspect_interaction().finish_frame(window.ui.devTreeSnapshot(),
														  window.inspect_pointer);
		if (!devMonitoring.performance_capture().interface_work_suspended() &&
#if FLOW_UI_AGENTIC_WORKFLOW
			!(agentic_service && agentic_service->interaction_controls_window(window.id)) &&
#endif
			devTooling.inspect_interaction().eligible_window(window.id) &&
			(devTooling.inspect_interaction().overlay_selection(window.ui.devTreeSnapshot(),
																overlaySelection) ||
			 devTooling.overlaySelection(window.id, overlaySelection))) {
			const float scaleX = std::max(window.uiToFramebufferScaleX, 1.0e-6f);
			const float scaleY = std::max(window.uiToFramebufferScaleY, 1.0e-6f);
			devTooling.overlays().generateOverlayCommands(
				overlaySelection, window.ui.devTreeSnapshot(),
				static_cast<float>(window.swapchain.swapchain.extent.width) / scaleX,
				static_cast<float>(window.swapchain.swapchain.extent.height) / scaleY,
				window.devOverlay, &window.renderCommands);
			const float configuredDpi = std::max(1.0f, window.config.ui.dpi);
			float pointsToPixelsScale =
				std::max(0.0f, window.config.ui.fontScale) * (configuredDpi / 72.0f);
			if (pointsToPixelsScale <= 0.0f)
				pointsToPixelsScale = configuredDpi / 72.0f;
			devTooling.overlays().buildUiRendererInstances(
				window.devOverlay, window.fontFrameView, pointsToPixelsScale, scaleX, scaleY,
				static_cast<float>(window.swapchain.swapchain.extent.width),
				static_cast<float>(window.swapchain.swapchain.extent.height));
		}
#endif
#if FLOW_UI_DEV_MODE && FLOWUI_PUBLIC_VULKAN_INTEROP
		if (window.memory_history_controller)
			window.memory_history_controller->prepare(window.fontFrameView,
													  std::max(0.01f, window.config.ui.fontScale) *
														  std::max(1.0f, window.config.ui.dpi) /
														  72.0f,
													  window.frames.currentFrame);
		if (window.timeline_controller)
			window.timeline_controller->prepare(window.fontFrameView,
												std::max(0.01f, window.config.ui.fontScale) *
													std::max(1.0f, window.config.ui.dpi) / 72.0f,
												window.frames.currentFrame);
#endif
		window.preparedUi = window.renderer.prepareFrame(
			vk, *storageSystem, window.storageFrame, preparedBindings, window.renderCommands,
			window.ui.inputFieldFrameOverrides(), window.fontFrameView,
			window.swapchain.swapchain.extent, window.uiToFramebufferScaleX,
			window.uiToFramebufferScaleY
#if FLOW_UI_DEV_MODE
			,
			&window.devOverlay, &timingRecorder()
#endif
		);
		{
#if FLOW_UI_DEV_MODE
			FLOWUI_DEV_TIMING_ZONE_BALANCED(timingRecorder(), devSystems::TimingCategory::Prepare,
											devSystems::TimingZoneRole::Work,
											"flowui.storage.seal_frame");
#endif
			window.storageReadLease = storageSystem->sealFrame(window.storageFrame);
#if FLOW_UI_DEV_MODE
			devSystems::recordGlobalDevBreadcrumb(kDevStorageFrameSeal, window.id,
												  window.storageFrame.epoch);
#endif
		}
		{
#if FLOW_UI_DEV_MODE
			FLOWUI_DEV_TIMING_ZONE_BALANCED(timingRecorder(), devSystems::TimingCategory::Element,
											devSystems::TimingZoneRole::Work,
											"flowui.element.commit_window_frame");
#endif
			elementManager.commitWindowFrame(window.id, window.storageFrame.epoch);
		}
		window.phase = AppWindow::Phase::Prepared;
#if FLOW_UI_AGENTIC_WORKFLOW
		if (agentic_service)
			agentic_service->observe_interaction(window.agentic_frame.key);
#endif
#if FLOW_UI_DEV_MODE
		endTiming.end();
		window.preparedGapTiming.begin(timingRecorder(),
									   devSystems::timing_zones::kWindowFramePreparedGap,
									   devSystems::TimingEntityRef::window(window.id));
#endif
	} catch (...) {
		cancelStorageFrame(window
#if FLOW_UI_DEV_MODE
						   ,
						   devSystems::TimingRecordFlag::Exception
#endif
		);
		throw;
	}
}

void App::Impl::drawFrame(WindowId id) {
	requirePlatformThread();
	AppWindow& window = requireWindow(id);
	if (activeWindowFrame != id || window.phase != AppWindow::Phase::Prepared) {
		throw FlowUiException(
			makeError(ErrorCode::FramePhaseViolation, ErrorSite::AppDrawFrame, id));
	}
#if FLOW_UI_DEV_MODE
	window.preparedGapTiming.end();
#endif
	WindowFrameExitGuard frameExit(*storageSystem, window, activeWindowFrame
#if FLOW_UI_DEV_MODE
								   ,
								   timingRecorder(), errorRecorder()
#endif
#if FLOW_UI_AGENTIC_WORKFLOW
														 ,
								   agentic_service.get(), &vk
#endif
	);
#if FLOW_UI_DEV_MODE
	devSystems::ManualTimingZone drawTiming(timingRecorder(),
											devSystems::timing_zones::kWindowFrameDraw,
											devSystems::TimingEntityRef::window(window.id));
#endif
	if (window.frames.frames.empty() || window.swapchain.swapchain.swapchain == VK_NULL_HANDLE ||
		window.swapchain.swapchain.views.empty()) {
#if FLOW_UI_DEV_MODE
		drawTiming.end(devSystems::TimingRecordFlag::Minimized);
		frameExit.setTimingResult(devSystems::TimingRecordFlag::Minimized);
#endif
		return;
	}
	if (!window.storageReadLease) {
		detail::terminateForFatalError(
			makeError(ErrorCode::PreparedFrameStale, ErrorSite::AppDrawFrame, id));
	}
	if (window.preparedUi.epoch == 0 ||
		window.preparedUi.epoch != window.storageReadLease.frame.epoch) {
		detail::terminateForFatalError(
			makeError(ErrorCode::PreparedFrameStale, ErrorSite::AppDrawFrame, id));
	}
	{
#if FLOW_UI_DEV_MODE
		FLOWUI_DEV_TIMING_ZONE_BALANCED(timingRecorder(), devSystems::TimingCategory::RendererCpu,
										devSystems::TimingZoneRole::Work,
										"flowui.swapchain.resize_check");
#endif
		const VkExtent2D framebufferExtent =
			window.backend ? window.backend->framebufferExtent() : VkExtent2D{};
		if (framebufferExtent.width != window.observedFramebufferExtent.width ||
			framebufferExtent.height != window.observedFramebufferExtent.height) {
			window.flags.set_flag(AppWindow::Flag::framebuffer_resized, true);
			window.observedFramebufferExtent = framebufferExtent;
		}
	}
	if (window.flags.has_flag(AppWindow::Flag::framebuffer_resized)) {
#if FLOW_UI_DEV_MODE
		const auto result =
			devSystems::TimingRecordFlag::Canceled | devSystems::TimingRecordFlag::OutOfDate;
		drawTiming.end(result);
		frameExit.setTimingResult(result);
#endif
		cancelStorageFrame(window
#if FLOW_UI_DEV_MODE
						   ,
						   result
#endif
		);
		const bool recreated = recreateSwapchainIfNeeded(window);
		detail::reportErrorEvent(ErrorEventView{
			.error = makeError(ErrorCode::SwapchainOutOfDate, ErrorSite::AppRecreateWindow, id),
			.kind = ErrorEventKind::Resolved,
			.resolution = recreated ? ErrorResolution::RecreatedWindow : ErrorResolution::Skipped,
		});
		return;
	}

	FrameVk::Frame& frame = window.frames.getCurrentFrame();
	uint32_t swapchainImageIndex = 0;
	VkResult acquireResult = VK_SUCCESS;
	{
#if FLOW_UI_DEV_MODE
		FLOWUI_DEV_TIMING_ZONE(timingRecorder(), devSystems::TimingCategory::Wait,
							   devSystems::TimingZoneRole::Wait, "flowui.wait.acquire_image");
#endif
		acquireResult =
			vkAcquireNextImageKHR(vk.device, window.swapchain.swapchain.swapchain, UINT64_MAX,
								  frame.imageAvailable, VK_NULL_HANDLE, &swapchainImageIndex);
	}
	if (acquireResult == VK_ERROR_OUT_OF_DATE_KHR) {
		window.flags.set_flag(AppWindow::Flag::framebuffer_resized, true);
#if FLOW_UI_DEV_MODE
		const auto result =
			devSystems::TimingRecordFlag::Canceled | devSystems::TimingRecordFlag::OutOfDate;
		drawTiming.end(result);
		frameExit.setTimingResult(result);
#endif
		cancelStorageFrame(window
#if FLOW_UI_DEV_MODE
						   ,
						   result
#endif
		);
		const bool recreated = recreateSwapchainIfNeeded(window);
		detail::reportErrorEvent(ErrorEventView{
			.error = makeError(ErrorCode::SwapchainOutOfDate, ErrorSite::AppAcquireFrame, id, 0u,
							   static_cast<std::uint32_t>(acquireResult)),
			.kind = ErrorEventKind::Resolved,
			.resolution = recreated ? ErrorResolution::RecreatedWindow : ErrorResolution::Skipped,
		});
		return;
	}
	if (acquireResult != VK_SUCCESS && acquireResult != VK_SUBOPTIMAL_KHR) {
		vkCheck(acquireResult, ErrorSite::AppAcquireFrame);
	}

	const bool acquiredSuboptimalSwapchain = acquireResult == VK_SUBOPTIMAL_KHR;
	if (swapchainImageIndex >= window.swapchain.swapchain.views.size() ||
		swapchainImageIndex >= window.swapchain.imageInFlight.size() ||
		swapchainImageIndex >= window.swapchain.layouts.size() ||
		swapchainImageIndex >= window.swapchain.renderFinished.size()) {
		detail::terminateForFatalError(makeError(
			ErrorCode::SwapchainImageInvalid, ErrorSite::AppAcquireFrame, id, swapchainImageIndex));
	}
	if (window.swapchain.imageInFlight[swapchainImageIndex] != VK_NULL_HANDLE) {
		FLOWUI_DEV_TIMING_ZONE(timingRecorder(), devSystems::TimingCategory::Wait,
							   devSystems::TimingZoneRole::Wait,
							   "flowui.wait.swapchain_image_fence");
		vkCheck(vkWaitForFences(vk.device, 1, &window.swapchain.imageInFlight[swapchainImageIndex],
								VK_TRUE, UINT64_MAX),
				ErrorSite::AppAcquireFrame);
	}
	window.swapchain.imageInFlight[swapchainImageIndex] = frame.inFlight;

	{
#if FLOW_UI_DEV_MODE
		FLOWUI_DEV_TIMING_ZONE_BALANCED(timingRecorder(), devSystems::TimingCategory::RendererCpu,
										devSystems::TimingZoneRole::Work,
										"flowui.renderer.command_buffer_begin");
#endif
		vkCheck(vkResetCommandPool(vk.device, frame.pool, 0), ErrorSite::AppDrawFrame);
		VkCommandBufferBeginInfo beginInfo{};
		beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
		beginInfo.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
		vkCheck(vkBeginCommandBuffer(frame.cmd, &beginInfo), ErrorSite::AppDrawFrame);
	}

#if FLOW_UI_DEV_MODE
	devSystems::GpuTimingCommandContext gpuTiming = devMonitoring.gpuTiming().beginFrameRecording(
		vk, frame.gpuTiming, frame.cmd, window.timingFrame, window.timingAppTick);
#endif

	transitionSwapchainImageLayout(
		frame.cmd, window.swapchain.swapchain.images[swapchainImageIndex],
		window.swapchain.layouts[swapchainImageIndex], VK_IMAGE_LAYOUT_ATTACHMENT_OPTIMAL);
	window.swapchain.layouts[swapchainImageIndex] = VK_IMAGE_LAYOUT_ATTACHMENT_OPTIMAL;

#if FLOW_UI_AGENTIC_WORKFLOW
	window.viewPorts.set_agentic_frame_context(window.timingAppTick, window.frameNumber);
	window.agentic_frame.tree_generation =
		window.ui.devTreeSnapshot().frameNumber == window.frameNumber
			? window.ui.devTreeSnapshot().generation
			: 0;
	if (agentic_service) {
		agentic_service->observe_tree(window.id);
		auto image_request = agentic_service->image_request(window.agentic_frame);
		image_request.stamp.logical_to_physical_scale_x = window.uiToFramebufferScaleX;
		image_request.stamp.logical_to_physical_scale_y = window.uiToFramebufferScaleY;
		if (image_request.completed_image) {
			window.agentic_frame.observers = window.agentic_frame.observers |
											 devSystems::agentic::ObserverActivity::image_readback;
			const auto selected = devSystems::agentic::frame_capture::begin_frame(
				vk, window.id, window.frames.currentFrame, image_request);
			if (!selected)
				window.agentic_frame.interrupted = true;
		}
	}
#endif
	window.viewPorts.recordFramePasses(vk, frame.cmd, window.frames.currentFrame
#if FLOW_UI_DEV_MODE
									   ,
									   &timingRecorder(), &gpuTiming
#endif
	);
	window.renderer.recordPreparedFrame(vk, frame.cmd, window.swapchain.swapchain.extent,
										window.swapchain.swapchain.views[swapchainImageIndex],
										window.frames.currentFrame, window.preparedUi
#if FLOW_UI_DEV_MODE
										,
										&timingRecorder(), &gpuTiming
#endif
	);
#if FLOW_UI_AGENTIC_WORKFLOW
	if (devSystems::agentic::frame_capture::requested(vk) &&
		devSystems::agentic::frame_capture::supports_surface(vk, window.surface)) {
		const auto readback = devSystems::agentic::frame_capture::record(
			vk, frame.cmd, window.swapchain.swapchain.images[swapchainImageIndex],
			window.swapchain.swapchain.extent, window.swapchain.swapchain.format,
			VK_IMAGE_LAYOUT_ATTACHMENT_OPTIMAL, window.id, window.frames.currentFrame, "window");
		if (!readback)
			window.agentic_frame.interrupted = true;
	}
#endif

	{
#if FLOW_UI_DEV_MODE
		FLOWUI_DEV_TIMING_ZONE_BALANCED(timingRecorder(), devSystems::TimingCategory::RendererCpu,
										devSystems::TimingZoneRole::Work,
										"flowui.renderer.command_buffer_end");
#endif
		transitionSwapchainImageLayout(
			frame.cmd, window.swapchain.swapchain.images[swapchainImageIndex],
			window.swapchain.layouts[swapchainImageIndex], VK_IMAGE_LAYOUT_PRESENT_SRC_KHR);
		window.swapchain.layouts[swapchainImageIndex] = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;
#if FLOW_UI_DEV_MODE
		devMonitoring.gpuTiming().endFrameRecording(gpuTiming, frame.cmd);
#endif
		vkCheck(vkEndCommandBuffer(frame.cmd), ErrorSite::AppDrawFrame);
	}

	VkPipelineStageFlags waitStages[] = {VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT};
	VkSemaphore presentWaitSemaphore = window.swapchain.renderFinished[swapchainImageIndex];
	VkSubmitInfo submitInfo{};
	submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
	submitInfo.waitSemaphoreCount = 1;
	submitInfo.pWaitSemaphores = &frame.imageAvailable;
	submitInfo.pWaitDstStageMask = waitStages;
	submitInfo.commandBufferCount = 1;
	submitInfo.pCommandBuffers = &frame.cmd;
	submitInfo.signalSemaphoreCount = 1;
	submitInfo.pSignalSemaphores = &presentWaitSemaphore;

	vkCheck(vkResetFences(vk.device, 1, &frame.inFlight), ErrorSite::AppSubmitFrame);
	{
#if FLOW_UI_DEV_MODE
		FLOWUI_DEV_TIMING_ZONE(timingRecorder(), devSystems::TimingCategory::RendererCpu,
							   devSystems::TimingZoneRole::Work, "flowui.renderer.queue_submit");
#endif
		vkCheck(vkQueueSubmit(vk.graphicsQ, 1, &submitInfo, frame.inFlight),
				ErrorSite::AppSubmitFrame);
		frame.storageSubmission = storageSystem->noteSubmission(window.storageReadLease);
#if FLOW_UI_DEV_MODE
		window.viewPorts.mark_dev_captures_submitted(frame.storageSubmission.serial);
#endif
#if FLOW_UI_AGENTIC_WORKFLOW
		window.viewPorts.mark_agentic_submitted(frame.storageSubmission.serial);
		window.agentic_frame.submitted = true;
		window.agentic_frame.submission_serial = frame.storageSubmission.serial;
		window.agentic_frame.submit_ns = devSystems::agentic::monotonic_nanoseconds();
		window.agentic_frame.device_id = 1;
		window.agentic_frame.queue_id = 1;
		devSystems::agentic::frame_capture::mark_submitted(
			vk, window.id, window.frames.currentFrame, frame.storageSubmission.serial,
			window.agentic_frame.device_id, window.agentic_frame.queue_id);
#endif
#if FLOW_UI_DEV_MODE
		devMonitoring.gpuTiming().markSubmitted(frame.gpuTiming, frame.storageSubmission.serial);
#endif
	}
#if FLOW_UI_DEV_MODE && FLOWUI_PUBLIC_VULKAN_INTEROP
	if (window.memory_history_controller)
		window.memory_history_controller->submitted(frame.storageSubmission.serial);
	if (window.timeline_controller)
		window.timeline_controller->submitted(frame.storageSubmission.serial);
#endif
	window.lastSubmissionSerial =
		std::max(window.lastSubmissionSerial, frame.storageSubmission.serial);
	window.swapchain.lastGraphicsUse =
		std::max(window.swapchain.lastGraphicsUse, frame.storageSubmission.serial);
	window.preparedUi = {};
	window.storageReadLease = {};
	window.storageFrame = {};

	VkPresentInfoKHR presentInfo{};
	presentInfo.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;
	presentInfo.waitSemaphoreCount = 1;
	presentInfo.pWaitSemaphores = &presentWaitSemaphore;
	presentInfo.swapchainCount = 1;
	presentInfo.pSwapchains = &window.swapchain.swapchain.swapchain;
	presentInfo.pImageIndices = &swapchainImageIndex;
	VkSwapchainPresentFenceInfoEXT presentFenceInfo{};
	VkPresentIdKHR presentIdInfo{};
	uint64_t presentId = 0;
	if (vk.wsiRetirementMode == WsiRetirementMode::PresentFence) {
		VkFence& presentFence = window.swapchain.presentComplete[swapchainImageIndex];
		if (window.swapchain.presentPending[swapchainImageIndex] != 0u) {
			vkCheck(vkWaitForFences(vk.device, 1, &presentFence, VK_TRUE, UINT64_MAX),
					ErrorSite::AppPresent);
			window.swapchain.presentPending[swapchainImageIndex] = 0u;
		}
		vkCheck(vkResetFences(vk.device, 1, &presentFence), ErrorSite::AppPresent);
		presentFenceInfo.sType = VK_STRUCTURE_TYPE_SWAPCHAIN_PRESENT_FENCE_INFO_EXT;
		presentFenceInfo.swapchainCount = 1;
		presentFenceInfo.pFences = &presentFence;
		presentInfo.pNext = &presentFenceInfo;
	} else if (vk.wsiRetirementMode == WsiRetirementMode::PresentWait) {
		if (window.swapchain.lastPresentId == std::numeric_limits<uint64_t>::max()) {
			detail::terminateForFatalError(
				makeError(ErrorCode::PresentIdSpaceExhausted, ErrorSite::AppPresent, id));
		}
		presentId = window.swapchain.lastPresentId + 1u;
		presentIdInfo.sType = VK_STRUCTURE_TYPE_PRESENT_ID_KHR;
		presentIdInfo.swapchainCount = 1;
		presentIdInfo.pPresentIds = &presentId;
		presentInfo.pNext = &presentIdInfo;
	}
	VkResult presentResult = VK_SUCCESS;
	{
#if FLOW_UI_DEV_MODE
		FLOWUI_DEV_TIMING_ZONE(timingRecorder(), devSystems::TimingCategory::Wait,
							   devSystems::TimingZoneRole::Wait, "flowui.wait.present_call");
#endif
		presentResult = vkQueuePresentKHR(vk.presentQ, &presentInfo);
#if FLOW_UI_AGENTIC_WORKFLOW
		window.agentic_frame.present_ns = devSystems::agentic::monotonic_nanoseconds();
		window.agentic_frame.presented =
			presentResult == VK_SUCCESS || presentResult == VK_SUBOPTIMAL_KHR;
		window.agentic_frame.interrupted =
			window.agentic_frame.interrupted || presentResult != VK_SUCCESS;
#endif
	}
	if (presentId != 0 && (presentResult == VK_SUCCESS || presentResult == VK_SUBOPTIMAL_KHR)) {
		window.swapchain.lastPresentId = presentId;
	}
	if (vk.wsiRetirementMode == WsiRetirementMode::PresentFence &&
		(presentResult == VK_SUCCESS || presentResult == VK_SUBOPTIMAL_KHR)) {
		window.swapchain.presentPending[swapchainImageIndex] = 1u;
	}
	if (presentResult == VK_ERROR_OUT_OF_DATE_KHR || presentResult == VK_SUBOPTIMAL_KHR ||
		acquiredSuboptimalSwapchain || window.flags.has_flag(AppWindow::Flag::framebuffer_resized)) {
		window.flags.set_flag(AppWindow::Flag::framebuffer_resized, true);
		const bool recreated = recreateSwapchainIfNeeded(window);
		const VkResult observedResult = presentResult != VK_SUCCESS ? presentResult : acquireResult;
		detail::reportErrorEvent(ErrorEventView{
			.error = makeError(ErrorCode::SwapchainOutOfDate, ErrorSite::AppPresent, id, 0u,
							   static_cast<std::uint32_t>(observedResult)),
			.kind = ErrorEventKind::Resolved,
			.resolution = recreated ? ErrorResolution::RecreatedWindow : ErrorResolution::Skipped,
		});
	} else if (presentResult != VK_SUCCESS) {
		vkCheck(presentResult, ErrorSite::AppPresent, ErrorCode::PresentationFailed);
	}

	window.frames.advance();
#if FLOW_UI_DEV_MODE
	drawTiming.end();
	window.frameTotalTiming.end();
	window.timingFrame = {};
	window.timingAppTick = 0u;
	timingRecorder().clearFrameContext();
	frameExit.setTimingResult(devSystems::TimingRecordFlag::Completed);
#endif
}

bool App::Impl::recreateSwapchainIfNeeded(AppWindow& window) {
	if (!window.backend || vk.device == VK_NULL_HANDLE || window.surface == VK_NULL_HANDLE)
		return false;
	const VkExtent2D framebufferExtent = window.backend->framebufferExtent();
	window.observedFramebufferExtent = framebufferExtent;
	if (framebufferExtent.width == 0 || framebufferExtent.height == 0) {
		window.flags.set_flag(AppWindow::Flag::framebuffer_resized, true);
		return false;
	}
#if FLOW_UI_DEV_MODE
	FLOWUI_DEV_TIMING_ZONE_ENTITY(
		timingRecorder(), devSystems::TimingCategory::Lifecycle, devSystems::TimingZoneRole::Work,
		"flowui.window.recreate_swapchain", devSystems::TimingEntityRef::window(window.id));
#endif

	if (!vk.supportsExactPresentCompletion()) {
		if (windows.size() != 1u || window.id != mainWindowId) {
			throw FlowUiException(makeError(ErrorCode::WindowPresentationUnsupported,
											ErrorSite::AppRecreateWindow, window.id));
		}
		vkCheck(vkDeviceWaitIdle(vk.device), ErrorSite::AppRecreateWindow);
		completeAllSubmissionsAfterIdle();
		SwapchainGeneration replacement{};
		try {
			replacement.create(window.config.native, window.config.vulkan, vk, window.surface,
							   framebufferExtent, window.swapchain.swapchain.swapchain);
		} catch (...) {
			if (window.backend)
				window.backend->setShouldClose(1);
			throw;
		}
		try {
			window.renderer.onSwapchainFormatChanged(vk, replacement.swapchain.format,
													 window.lastSubmissionSerial);
		} catch (...) {
			replacement.destroy(vk);
			if (window.backend)
				window.backend->setShouldClose(1);
			throw;
		}
		window.swapchain.destroy(vk);
		window.swapchain = std::move(replacement);
	} else {
		drainWindowGraphics(window);
		SwapchainGeneration replacement{};
		try {
			replacement.create(window.config.native, window.config.vulkan, vk, window.surface,
							   framebufferExtent, window.swapchain.swapchain.swapchain);
		} catch (...) {
			if (window.backend)
				window.backend->setShouldClose(1);
			throw;
		}
		try {
			window.renderer.onSwapchainFormatChanged(vk, replacement.swapchain.format,
													 window.lastSubmissionSerial);
			window.retiredSwapchains.reserve(window.retiredSwapchains.size() + 1u);
		} catch (...) {
			replacement.destroy(vk);
			if (window.backend)
				window.backend->setShouldClose(1);
			throw;
		}
		window.retiredSwapchains.push_back(std::move(window.swapchain));
		window.swapchain = std::move(replacement);
		collectRetiredSwapchains(window);
		if (!window.storageFrame)
			storageSystem->collect();
	}
	window.flags.set_flag(AppWindow::Flag::framebuffer_resized, false);
	return true;
}

void App::Impl::destroyWindow(WindowId id) {
	requirePlatformThread();
	if (id == mainWindowId) {
		throw FlowUiException(
			makeError(ErrorCode::MainWindowDestructionForbidden, ErrorSite::AppDestroyWindow, id));
	}
	AppWindow& window = requireWindow(id);
#if FLOW_UI_DEV_MODE && FLOWUI_DEV_MEMORY_LEVEL >= 2
	unregisterWindowMemoryProbe(window);
#endif
	if (activeWindowFrame != InvalidWindowId && activeWindowFrame != id) {
		throw FlowUiException(makeError(ErrorCode::FrameAlreadyActive, ErrorSite::AppDestroyWindow,
										activeWindowFrame));
	}
	if (activeWindowFrame == id)
		cancelStorageFrame(window);
	std::erase_if(pending_window_operations,
				  [id](const PendingWindowOperation& pending) { return pending.window_id == id; });
#if FLOW_UI_DEV_MODE
	FLOWUI_DEV_TIMING_ZONE_ENTITY(timingRecorder(), devSystems::TimingCategory::Lifecycle,
								  devSystems::TimingZoneRole::Work, "flowui.window.destroy",
								  devSystems::TimingEntityRef::window(id));
#endif
#if FLOW_UI_DEV_MODE
	devTooling.inspect_interaction().remove_window(id);
	devTooling.clearOverlaySelection(id);
	devTooling.overrides().release_window(id);
#endif
	window.phase = AppWindow::Phase::Closing;
	if (window.backend)
		window.backend->detachCallbacks();

	drainWindowGraphics(window);
	collectRetiredSwapchains(window);
	window.swapchain.waitForPresentCompletion(vk);
#if FLOW_UI_AGENTIC_WORKFLOW
	devSystems::agentic::frame_capture::release_window(vk, id);
#endif
#if FLOW_UI_DEV_MODE && FLOWUI_PUBLIC_VULKAN_INTEROP
	if (window.memory_history_controller)
		window.memory_history_controller->destroy_drained();
	if (window.timeline_controller)
		window.timeline_controller->destroy_drained();
#endif
	window.renderer.destroy(vk, *storageSystem, window.lastSubmissionSerial);
	window.viewPorts.destroyDrained(vk);
	window.frames.destroy(vk);
	window.swapchain.destroy(vk);
	elementManager.destroyWindow(id);
	window.ui.destroyStorage();
	if (window.flags.has_flag(AppWindow::Flag::storage_registered)) {
		storageSystem->unregisterWindow(id, window.lastSubmissionSerial);
		window.flags.set_flag(AppWindow::Flag::storage_registered, false);
	}
	storageSystem->collect();
	if (window.surface != VK_NULL_HANDLE) {
		vkDestroySurfaceKHR(vk.instance, window.surface, nullptr);
		window.surface = VK_NULL_HANDLE;
	}
	window.backend.reset();
#if FLOW_UI_DEV_MODE
	devTooling.clearOverlaySelection(id);
#endif
	windows.erase(id);
	std::erase_if(managedWindows, [id](const ManagedWindowEntry& entry) { return entry.id == id; });
	activeWindowFrame = InvalidWindowId;
}

void App::Impl::cleanup() noexcept {
	if (lifecycle_flags.has_flag(LifecycleFlag::cleaned_up))
		return;
	lifecycle_flags.set_flag(LifecycleFlag::cleaned_up, true);
#if FLOW_UI_AGENTIC_WORKFLOW
	if (agentic_service)
		agentic_service->stop_admission();
#endif
	for (auto& [_, window] : windows)
		cancelStorageFrame(*window);
#if FLOW_UI_DEV_MODE
	devSystems::ManualTimingZone shutdownTiming(
		timingRecorder(), devSystems::makeBuiltinTimingDescriptor(
							  0xd8d6bc916b0f4a41ull, devSystems::TimingCategory::Lifecycle,
							  devSystems::TimingZoneRole::Work, devSystems::CpuTimingLevel::Summary,
							  "flowui.app.shutdown"));
#endif
#if FLOW_UI_AGENTIC_WORKFLOW
	bool agentic_device_lost = false;
#endif
	if (vk.device != VK_NULL_HANDLE) {
		const auto drain_result = vkDeviceWaitIdle(vk.device);
		if (drain_result == VK_SUCCESS)
			try {
				completeAllSubmissionsAfterIdle();
			} catch (...) {}
#if FLOW_UI_AGENTIC_WORKFLOW
		agentic_device_lost = drain_result != VK_SUCCESS;
		if (agentic_device_lost)
			devSystems::agentic::frame_capture::discard_device(vk);
		else
			devSystems::agentic::frame_capture::release_device(vk);
#endif
	}
#if FLOW_UI_AGENTIC_WORKFLOW
	if (agentic_service) {
		const auto agentic_stopped = agentic_service->shutdown(
			{.publish_partial = true, .device_lost = agentic_device_lost});
		if (!agentic_stopped)
			std::fputs("FlowUi agentic publication incomplete during teardown\n", stderr);
		agentic_service.reset();
	}
#endif
	for (auto& [_, window] : windows) {
#if FLOW_UI_DEV_MODE && FLOWUI_DEV_MEMORY_LEVEL >= 2
		unregisterWindowMemoryProbe(*window);
#endif
		elementManager.destroyWindow(window->id);
	}
#if FLOW_UI_DEV_MODE && FLOWUI_DEV_MEMORY_LEVEL >= 2
	unregisterAppMemoryProbes();
#endif
#if FLOW_UI_DEV_MODE
	devInterface.release_inspection_resources(storageSystem.get());
#endif
	elementManager.destroy();
	actionManager.destroy();
	themeManager.destroy();

	if (lifecycle_flags.has_flag(LifecycleFlag::images_initialized))
		imageManager.destroy();
#if FLOWUI_INCLUDE_ICON_MANAGER
	if (lifecycle_flags.has_flag(LifecycleFlag::icons_initialized))
		icons.destroy();
#endif
	if (lifecycle_flags.has_flag(LifecycleFlag::fonts_initialized))
		fonts.destroy();

	for (auto& [_, window] : windows) {
#if FLOW_UI_DEV_MODE && FLOWUI_PUBLIC_VULKAN_INTEROP
		if (window->memory_history_controller)
			window->memory_history_controller->destroy_drained();
		if (window->timeline_controller)
			window->timeline_controller->destroy_drained();
#endif
		window->viewPorts.destroyDrained(vk);
		if (storageSystem) {
			try {
				window->renderer.destroy(vk, *storageSystem, window->lastSubmissionSerial);
			} catch (...) {}
		}
		window->frames.destroy(vk);
		for (SwapchainGeneration& generation : window->retiredSwapchains)
			generation.destroy(vk);
		window->retiredSwapchains.clear();
		window->swapchain.destroy(vk);
		window->ui.destroyStorage();
		if (storageSystem && window->flags.has_flag(AppWindow::Flag::storage_registered)) {
			try {
				storageSystem->unregisterWindow(window->id, storageSystem->completedSerial());
			} catch (...) {}
			window->flags.set_flag(AppWindow::Flag::storage_registered, false);
		}
		if (window->surface != VK_NULL_HANDLE && vk.instance != VK_NULL_HANDLE) {
			vkDestroySurfaceKHR(vk.instance, window->surface, nullptr);
			window->surface = VK_NULL_HANDLE;
		}
		window->backend.reset();
	}
	if (storageSystem) {
		destroySharedUiByteResources(*storageSystem, sharedUiByteResources);
		try {
			storageSystem->collect();
		} catch (...) {}
	}

	if (storageSystem) {
#if FLOW_UI_DEV_MODE && FLOWUI_DEV_MEMORY_LEVEL >= 1
		devMonitoring.memoryReporting().requestEnvironmentCheckpoint();
		devMonitoring.memoryReporting().consume(appTick);
		devMonitoring.memory().setStorageSystem(nullptr);
#endif
		storageSystem->shutdown();
		storageSystem.reset();
	}
	windows.clear();
	activeWindowFrame = InvalidWindowId;
#if FLOW_UI_DEV_MODE && FLOWUI_DEV_MEMORY_LEVEL >= 1
	devMonitoring.memory().detachEnvironmentProbes();
#endif
	vk.destroy();
#if FLOW_UI_DEV_MODE
	devMonitoring.performance_capture().shutdown(appTick, devMonitoring.timing().nowNs());
	shutdownTiming.end();
	devMonitoring.timingReporting().consumeThrough(appTick);
	devMonitoring.memoryReporting().consume(appTick);
	devMonitoring.errorReporting().consumeThrough(appTick);
#endif
}

} // namespace FlowUi
