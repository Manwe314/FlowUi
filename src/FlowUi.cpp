#define FLOWUI_INTERNAL_VIEWPORT_MANAGER 1
#include "FlowUi/App.hpp"
#include "FlowUi/PublicStructs.hpp"
#include "FlowUi/BuildConfig.hpp"
#include "managers/FontManager.hpp"
#include "managers/ImageManager.hpp"
#include "managers/ThemeManager.hpp"
#include "managers/ElementManager.hpp"
#include "managers/ActionManager.hpp"
#if COMPILE_FSELI
#include "FSEL/Theme.hpp"
#include "FSEL/StandardIcons.hpp"
#endif
#if FLOWUI_INCLUDE_ICON_MANAGER
#include "managers/IconManager.hpp"
#endif
#include "managers/ViewPortManager.hpp"
#include "managers/UiManager.hpp"
#include "devSystems/devMonitoringAndReporting/timing/DevTimingZone.hpp"
#include "Ui/Vk_UiRenderer.hpp"
#if FLOW_UI_DEV_MODE
#include "devSystems/devInterface/Permanents/Backend/DevInterface.hpp"
#include "devSystems/devTooling/DevTooling.hpp"
#include "devSystems/devMonitoringAndReporting/DevMonitoringAndReporting.hpp"
#include "devSystems/devMonitoringAndReporting/memory/DevMemoryProbe.hpp"
#include "devSystems/devMonitoringAndReporting/memory/DevMemorySources.hpp"
#include "devSystems/devMonitoringAndReporting/memory/DevMemory.hpp"
#include "devSystems/devMonitoringAndReporting/errors/DevError.hpp"
#include "devSystems/devMonitoringAndReporting/reporting/DevErrorReporting.hpp"
#include "devSystems/devMonitoringAndReporting/reporting/DevMemoryReporting.hpp"
#include "devSystems/devMonitoringAndReporting/reporting/DevTimingReporting.hpp"
#include "devSystems/devMonitoringAndReporting/timing/DevGpuTiming.hpp"
#include "devSystems/devMonitoringAndReporting/timing/DevTiming.hpp"
#endif
#include "internal/InputQueue.hpp"
#include "internal/StorageSystem/FlowStorageSystem.hpp"
#include "internal/ManagerStorage/FontCatalogController.hpp"
#include "Ui/Vk_UiRenderer.hpp"
#include "Vulkan/Vk_Context.hpp"
#include "Vulkan/Vk_Frames.hpp"
#include "Vulkan/Vk_Swapchain.hpp"
#include "window/Window.hpp"

#include <array>
#include <algorithm>
#include <atomic>
#include <cctype>
#include <chrono>
#include <cstdio>
#include <filesystem>
#include <exception>
#include <limits>
#include <memory>
#include <string>
#include <thread>
#include <unordered_map>
#include <vector>
#include <stdexcept>

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
	case FlowUi::ErrorEventKind::Resolved: return "resolved";
	case FlowUi::ErrorEventKind::Reported: return "reported";
	case FlowUi::ErrorEventKind::Fatal: return "fatal";
	case FlowUi::ErrorEventKind::BackendDiagnostic: return "backend";
	}
	return "unknown";
}

const char* errorResolutionName(FlowUi::ErrorResolution resolution) noexcept {
	switch (resolution) {
	case FlowUi::ErrorResolution::None: return "None";
	case FlowUi::ErrorResolution::Rejected: return "Rejected";
	case FlowUi::ErrorResolution::Ignored: return "Ignored";
	case FlowUi::ErrorResolution::Retried: return "Retried";
	case FlowUi::ErrorResolution::UsedFallback: return "UsedFallback";
	case FlowUi::ErrorResolution::Skipped: return "Skipped";
	case FlowUi::ErrorResolution::GrewCapacity: return "GrewCapacity";
	case FlowUi::ErrorResolution::EvictedAndRetried: return "EvictedAndRetried";
	case FlowUi::ErrorResolution::CanceledOperation: return "CanceledOperation";
	case FlowUi::ErrorResolution::CanceledFrame: return "CanceledFrame";
	case FlowUi::ErrorResolution::RecreatedWindow: return "RecreatedWindow";
	case FlowUi::ErrorResolution::ClosedWindow: return "ClosedWindow";
	case FlowUi::ErrorResolution::RecreatedApp: return "RecreatedApp";
	case FlowUi::ErrorResolution::Propagated: return "Propagated";
	case FlowUi::ErrorResolution::Halted: return "Halted";
	case FlowUi::ErrorResolution::Terminated: return "Terminated";
	}
	return "Unknown";
}

template <typename... Args>
void appendErrorFormat(
	char* buffer,
	std::size_t capacity,
	std::size_t& used,
	const char* format,
	Args... args) noexcept {
	if (used >= capacity - 1u) return;
	const int written = std::snprintf(buffer + used, capacity - used, format, args...);
	if (written <= 0) return;
	used = std::min<std::size_t>(
		used + static_cast<std::size_t>(written), capacity - 1u);
}

void writeDefaultErrorEvent(std::FILE* output, const FlowUi::ErrorEventView& event) noexcept {
	if (!output) return;

	char buffer[1024]{};
	const std::string_view site = FlowUi::errorSiteName(event.error.site);
	std::size_t used = 0u;
	if (event.kind == FlowUi::ErrorEventKind::BackendDiagnostic) {
		const int messageLength = static_cast<int>(
			std::min<std::size_t>(event.nativeMessage.size(), 768u));
		appendErrorFormat(
			buffer, sizeof(buffer), used,
			"[FlowUi][backend] %.*s",
			static_cast<int>(site.size()), site.data());
		if (event.error.nativeCode != 0u) {
			appendErrorFormat(
				buffer, sizeof(buffer), used, " native=%u", event.error.nativeCode);
		}
		if (event.nativeCategory != 0u) {
			appendErrorFormat(
				buffer, sizeof(buffer), used, " category=0x%x", event.nativeCategory);
		}
		if (messageLength > 0) {
			appendErrorFormat(
				buffer, sizeof(buffer), used, ": %.*s",
				messageLength, event.nativeMessage.data());
		}
	} else {
		const std::string_view code = FlowUi::errorName(event.error.code);
		const int messageLength = static_cast<int>(
			std::min<std::size_t>(event.nativeMessage.size(), 512u));
		appendErrorFormat(
			buffer, sizeof(buffer), used, "[FlowUi][%s] %.*s @ %.*s",
			errorEventKindName(event.kind),
			static_cast<int>(code.size()), code.data(),
			static_cast<int>(site.size()), site.data());
		if (event.error.subject != 0u) {
			appendErrorFormat(
				buffer, sizeof(buffer), used, " subject=0x%llx",
				static_cast<unsigned long long>(event.error.subject));
		}
		if (event.error.auxiliary != 0u) {
			appendErrorFormat(
				buffer, sizeof(buffer), used, " auxiliary=0x%llx",
				static_cast<unsigned long long>(event.error.auxiliary));
		}
		if (event.error.nativeCode != 0u) {
			appendErrorFormat(
				buffer, sizeof(buffer), used, " native=%u", event.error.nativeCode);
		}
		if (event.resolution != FlowUi::ErrorResolution::None) {
			appendErrorFormat(
				buffer, sizeof(buffer), used, " resolution=%s",
				errorResolutionName(event.resolution));
		}
		if (messageLength > 0) {
			appendErrorFormat(
				buffer, sizeof(buffer), used, ": %.*s",
				messageLength, event.nativeMessage.data());
		}
	}
	if (used < sizeof(buffer) - 1u) buffer[used++] = '\n';
	if (used > 0u) (void)std::fwrite(buffer, 1u, used, output);
	if (event.kind == FlowUi::ErrorEventKind::Fatal) {
		(void)std::fflush(output);
	}
}

class AppErrorObserver {
public:
	explicit AppErrorObserver(
		FlowUi::ErrorObserverConfig config
#if FLOW_UI_DEV_MODE
		, FlowUi::devSystems::DevErrorMonitoring* devErrors
#endif
		) noexcept
		: config_(config)
#if FLOW_UI_DEV_MODE
		, devErrors_(devErrors)
#endif
	{
		active_.store(this, std::memory_order_release);
	}

	~AppErrorObserver() {
		const AppErrorObserver* expected = this;
		(void)active_.compare_exchange_strong(
			expected, nullptr, std::memory_order_acq_rel);
	}

	AppErrorObserver(const AppErrorObserver&) = delete;
	AppErrorObserver& operator=(const AppErrorObserver&) = delete;

	void report(const FlowUi::ErrorEventView& event) const noexcept {
		switch (config_.mode) {
		case FlowUi::ErrorReportingMode::SinkAndDefault:
			writeDefaultErrorEvent(config_.output, event);
			config_.sink.notify(event);
			break;
		case FlowUi::ErrorReportingMode::SinkOrDefault:
			if (config_.sink) config_.sink.notify(event);
			else writeDefaultErrorEvent(config_.output, event);
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
	void observeForDev(
		const FlowUi::ErrorEventView& event,
		const FlowUi::devSystems::DevErrorSourceDescriptor& source) const noexcept {
		if (devErrors_) devErrors_->recordProductionEvent(0u, event, source);
	}
#endif

	static const AppErrorObserver* active() noexcept {
		return active_.load(std::memory_order_acquire);
	}

private:
	FlowUi::ErrorObserverConfig config_{};
#if FLOW_UI_DEV_MODE
	FlowUi::devSystems::DevErrorMonitoring* devErrors_ = nullptr;
#endif
	static std::atomic<const AppErrorObserver*> active_;
};

std::atomic<const AppErrorObserver*> AppErrorObserver::active_{nullptr};
thread_local bool reportingErrorEvent = false;

void vkCheck(
	VkResult result,
	FlowUi::ErrorSite site,
	FlowUi::ErrorCode code = FlowUi::ErrorCode::VulkanNativeCallFailed) {
	if (result != VK_SUCCESS) {
		if (result == VK_ERROR_DEVICE_LOST) code = FlowUi::ErrorCode::VulkanDeviceLost;
		if (result == VK_ERROR_OUT_OF_HOST_MEMORY || result == VK_ERROR_OUT_OF_DEVICE_MEMORY) {
			code = FlowUi::ErrorCode::AllocationFailed;
		}
		throw FlowUi::FlowUiException(FlowUi::makeError(
			code, site, 0u, 0u,
			static_cast<std::uint32_t>(result)));
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
		throw FlowUi::FlowUiException(FlowUi::makeError(FlowUi::ErrorCode::RenderCommandInvalid, FlowUi::ErrorSite::AppDrawFrame));
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
		throw FlowUi::FlowUiException(FlowUi::makeError(FlowUi::ErrorCode::WindowPresentationUnsupported, FlowUi::ErrorSite::AppCreateWindow));
	}
}

} // namespace

namespace FlowUi {

namespace detail {

#if FLOW_UI_DEV_MODE
void reportErrorEvent(
	const ErrorEventView& event,
	const char* file,
	const char* function,
	std::uint32_t line,
	std::uint32_t column) noexcept {
#else
void reportErrorEvent(const ErrorEventView& event) noexcept {
#endif
	const AppErrorObserver* observer = AppErrorObserver::active();
#if FLOW_UI_DEV_MODE
	if (observer) {
		observer->observeForDev(
			event,
			devSystems::makeDevErrorSource(
				"flowui.production.error", file, function, line, column));
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

namespace {

template <typename Operation>
Status runLocalOperation(Operation&& operation) {
	try {
		std::forward<Operation>(operation)();
		return {};
	} catch (const FlowUiException& exception) {
		if (exception.error().descriptor().category == ErrorCategory::Fatal) {
			detail::terminateForFatalError(exception.error());
		}
		if (exception.error().descriptor().category != ErrorCategory::Local) throw;
		return unexpectedError(exception.error());
	}
}

} // namespace

namespace storage = detail::storage;

inline constexpr uint32_t kUiTextureDescriptorCapacity = 256u;

struct AppWindowConfig {
	WindowConfig native{};
	VulkanConfig vulkan{};
	UiConfig ui{};
	uint32_t uiTextureDescriptorCapacity = kUiTextureDescriptorCapacity;
};

WindowConfig mergeWindowConfig(
	const WindowConfig& base,
	const WindowConfigOverrides& overrides) {
	WindowConfig result = base;
	if (overrides.width) result.width = *overrides.width;
	if (overrides.height) result.height = *overrides.height;
	if (overrides.title) result.title = *overrides.title;
	if (overrides.resizable) result.resizable = *overrides.resizable;
	if (overrides.decorated) result.decorated = *overrides.decorated;
	if (overrides.maximized) result.maximized = *overrides.maximized;
	if (overrides.fullscreen) result.fullscreen = *overrides.fullscreen;
	if (overrides.highDPI) result.highDPI = *overrides.highDPI;
	if (overrides.input) result.input = *overrides.input;
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
	result.allowTransientGrowth = result.allowRuntimeGrowth &&
		config.errors.policy.transientCapacity == StorageCapacityPolicy::GrowWithinBudget;
	result.allowPersistentGrowth = result.allowRuntimeGrowth &&
		config.errors.policy.persistentCapacity == StorageCapacityPolicy::GrowWithinBudget;
	return result;
}

storage::WindowStorageDesc makeWindowStorageDesc(
	storage::IStorageSystem& storageSystem,
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

struct AppWindow {
	AppWindow(WindowId windowId, AppWindowConfig windowConfig, const AppConfig& appConfig)
		: id(windowId),
		  config(std::move(windowConfig)),
		  inputQueue(appConfig.errors.policy.inputTextQueueCapacity, appConfig.errors.policy.inputQueueOverflow) {}

	WindowId id = InvalidWindowId;
	AppWindowConfig config{};
	storage::IStorageSystem* storageSystem = nullptr;
	bool storageRegistered = false;

	std::unique_ptr<detail::IWindowBackend> backend;
	detail::InputQueue inputQueue;
	VkSurfaceKHR surface = VK_NULL_HANDLE;
	SwapchainGeneration swapchain;
	std::vector<SwapchainGeneration> retiredSwapchains;
	FrameVk frames;

	UiManager ui;
	ViewPortManager viewPorts;

	VulkanUiRenderer renderer;

	FrameInput frameInput{};
	Clay_RenderCommandArray renderCommands{};
	PreparedUiFrame preparedUi{};
	storage::FrameToken storageFrame{};
	storage::FrameReadLease storageReadLease{};
	detail::manager_storage::FontFrameView fontFrameView{};
	uint64_t frameNumber = 0;
	storage::SubmissionSerial lastSubmissionSerial = 0;

#if FLOW_UI_DEV_MODE
	devSystems::WindowFrameKey timingFrame{};
	devSystems::AppTickId timingAppTick = 0u;
	devSystems::ManualTimingZone frameTotalTiming{};
	devSystems::ManualTimingZone userBuildTiming{};
	devSystems::ManualTimingZone preparedGapTiming{};
	devSystems::tooling::DevOverlayCommandBuffer devOverlay{};
#endif

	std::chrono::steady_clock::time_point previousBeginFrameTimestamp{};
	bool hasPreviousBeginFrameTimestamp = false;
	float uiToFramebufferScaleX = 1.0f;
	float uiToFramebufferScaleY = 1.0f;
	bool framebufferResized = false;
	VkExtent2D observedFramebufferExtent{};

	enum class Phase : uint8_t { Idle, Building, Prepared, Closing };
	Phase phase = Phase::Idle;
};

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
	WindowFrameExitGuard(
		storage::IStorageSystem& storageSystem,
		AppWindow& window,
		WindowId& activeWindow
#if FLOW_UI_DEV_MODE
		, devSystems::DevTimingRecorder& timingRecorder,
		devSystems::DevErrorRecorder& errorRecorder
#endif
		) noexcept
		: storage_(&storageSystem), window_(&window), activeWindow_(&activeWindow)
#if FLOW_UI_DEV_MODE
		, timingRecorder_(&timingRecorder), errorRecorder_(&errorRecorder),
		  uncaughtExceptions_(std::uncaught_exceptions())
#endif
		{}

#if FLOW_UI_DEV_MODE
	void setTimingResult(devSystems::TimingRecordFlag result) noexcept { timingResult_ = result; }
#endif

	~WindowFrameExitGuard() {
		if (window_->storageFrame) {
			try { storage_->cancelFrame(window_->storageFrame); } catch (...) {}
		}
		window_->preparedUi = {};
		window_->storageReadLease = {};
		window_->storageFrame = {};
		if (window_->phase != AppWindow::Phase::Closing) window_->phase = AppWindow::Phase::Idle;
		if (*activeWindow_ == window_->id) *activeWindow_ = InvalidWindowId;
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
		devSystems::recordGlobalDevBreadcrumb(
			completed ? kDevFrameComplete : kDevFrameCancel,
			window_->id,
			window_->frameNumber);
		errorRecorder_->clearFrameContext();
#endif
	}

private:
	storage::IStorageSystem* storage_;
	AppWindow* window_;
	WindowId* activeWindow_;
#if FLOW_UI_DEV_MODE
	devSystems::DevTimingRecorder* timingRecorder_ = nullptr;
	devSystems::DevErrorRecorder* errorRecorder_ = nullptr;
	devSystems::TimingRecordFlag timingResult_ = devSystems::TimingRecordFlag::Canceled;
	int uncaughtExceptions_ = 0;
#endif
};

struct App::Impl {
#if FLOW_UI_DEV_MODE
	devSystems::DevMonitoringAndReporting devMonitoring;
	devSystems::DevTooling devTooling;
	devSystems::DevInterface devInterface{};
	devSystems::DevErrorThreadAttachment platformErrorAttachment;
#endif
	AppErrorObserver errorObserver;
#if FLOW_UI_DEV_MODE
	devSystems::DevTimingThreadAttachment platformTimingAttachment;
	devSystems::AppTickId appTick = 0u;
#endif
	AppConfig config{};
	VulkanContext vk;
	std::unique_ptr<storage::IStorageSystem> storageSystem;
	storage::StorageConfig storageConfig{};
	SharedUiByteResources sharedUiByteResources;

	FontManager fonts;
	ImageManager imageManager;
	ThemeManager themeManager;
	ElementManager elementManager;
	ActionManager actionManager;
#if FLOWUI_INCLUDE_ICON_MANAGER
	IconManager icons;
#endif

	std::unordered_map<WindowId, std::unique_ptr<AppWindow>> windows;
	struct ManagedWindowEntry {
		WindowId id = InvalidWindowId;
		UiBuildCallback buildUi;
		ManagedWindowFlags flags{};
	};
	std::vector<ManagedWindowEntry> managedWindows;
	WindowId mainWindowId = MainWindowId;
	WindowId nextWindowId = MainWindowId + 1;
	WindowId activeWindowFrame = InvalidWindowId;
	bool windowIdsExhausted = false;
	bool fontsInitialized = false;
	bool imagesInitialized = false;
	bool iconsInitialized = false;
	bool cleanedUp = false;
	std::thread::id platformThread = std::this_thread::get_id();

	explicit Impl(const AppConfig& initialConfig)
#if FLOW_UI_DEV_MODE
		: devMonitoring(initialConfig.dev.monitoring),
		  devTooling(initialConfig.dev.tooling),
		  platformErrorAttachment(devMonitoring.errors().attachCurrentThread("flowui.platform")),
		  errorObserver(initialConfig.errors.observer, &devMonitoring.errors()),
		  platformTimingAttachment(devMonitoring.timing().attachCurrentThread("flowui.platform")),
		  config(initialConfig) {}
#else
		: errorObserver(initialConfig.errors.observer), config(initialConfig) {}
#endif
	~Impl() { cleanup(); }

#if FLOW_UI_DEV_MODE
	devSystems::DevTimingRecorder& timingRecorder() noexcept {
		return platformTimingAttachment.recorder();
	}

	devSystems::DevErrorRecorder& errorRecorder() noexcept {
		return platformErrorAttachment.recorder();
	}

#if FLOWUI_DEV_MEMORY_LEVEL >= 2
	static void sampleElementMemory(const void* owner, devSystems::MemoryProbeContext& context) noexcept {
		static_cast<const ElementManager*>(owner)->appendDevMemorySamples(context.sink);
	}
	static void sampleFontMemory(const void* owner, devSystems::MemoryProbeContext& context) noexcept {
		static_cast<const FontManager*>(owner)->appendDevMemorySamples(context.sink);
	}
#if FLOWUI_INCLUDE_ICON_MANAGER
	static void sampleIconMemory(const void* owner, devSystems::MemoryProbeContext& context) noexcept {
		static_cast<const IconManager*>(owner)->appendDevMemorySamples(context.sink);
	}
#endif
	static void sampleActionMemory(const void* owner, devSystems::MemoryProbeContext& context) noexcept {
		static_cast<const ActionManager*>(owner)->appendDevMemorySamples(context.sink);
	}
	static void sampleThemeMemory(const void* owner, devSystems::MemoryProbeContext& context) noexcept {
		static_cast<const ThemeManager*>(owner)->appendDevMemorySamples(context.sink);
	}
	static void sampleDevToolingMemory(
		const void* owner,
		devSystems::MemoryProbeContext& context) noexcept {
		static_cast<const devSystems::DevTooling*>(owner)->appendDevMemorySamples(context.sink);
	}
	static void sampleWindowMemory(const void* owner, devSystems::MemoryProbeContext& context) noexcept {
		const auto& window = *static_cast<const AppWindow*>(owner);
		window.ui.appendDevMemorySamples(context.sink);
		window.viewPorts.appendDevMemorySamples(context.sink);
		window.renderer.appendDevMemorySamples(context.sink, &window.preparedUi);
	}
	void registerAppMemoryProbes() {
		auto& memory = devMonitoring.memory();
		(void)memory.registerProbe({devSystems::memory_sources::kElements.id, &elementManager, &sampleElementMemory});
		(void)memory.registerProbe({devSystems::memory_sources::kFonts.id, &fonts, &sampleFontMemory});
#if FLOWUI_INCLUDE_ICON_MANAGER
		(void)memory.registerProbe({devSystems::memory_sources::kIcons.id, &icons, &sampleIconMemory});
#endif
		(void)memory.registerProbe({devSystems::memory_sources::kActions.id, &actionManager, &sampleActionMemory});
		(void)memory.registerProbe({devSystems::memory_sources::kThemes.id, &themeManager, &sampleThemeMemory});
		(void)memory.registerProbe({
			devSystems::memory_sources::kDevTooling.id, &devTooling, &sampleDevToolingMemory});
	}
	void registerWindowMemoryProbe(AppWindow& window) {
		(void)devMonitoring.memory().registerProbe({
			devSystems::memory_sources::kUiLayout.id, &window, &sampleWindowMemory});
	}
	void unregisterWindowMemoryProbe(AppWindow& window) noexcept {
		devMonitoring.memory().unregisterProbe(devSystems::memory_sources::kUiLayout.id, &window);
	}
	void unregisterAppMemoryProbes() noexcept {
		auto& memory = devMonitoring.memory();
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

	AppWindow& requireWindow(WindowId id) {
		const auto found = windows.find(id);
		if (id == InvalidWindowId || found == windows.end() || !found->second) {
			throw FlowUiException(makeError(ErrorCode::InvalidWindowId, ErrorSite::AppAccessWindow, id));
		}
		return *found->second;
	}

	const AppWindow& requireWindow(WindowId id) const {
		const auto found = windows.find(id);
		if (id == InvalidWindowId || found == windows.end() || !found->second) {
			throw FlowUiException(makeError(ErrorCode::InvalidWindowId, ErrorSite::AppAccessWindow, id));
		}
		return *found->second;
	}

	AppWindow& mainWindow() { return requireWindow(mainWindowId); }
	const AppWindow& mainWindow() const { return requireWindow(mainWindowId); }

	void initializeDefaultFont() {
		bool defaultFontLoaded = false;
		{
			auto created = fonts.createFamily(config.ui.defaultFontFamily);
			if (created) {
				const FontManager::FontId defaultFontId = fonts.resolveFont(*created);
				defaultFontLoaded = fonts.getFontById(defaultFontId) != nullptr;
			}
		}
		if (!defaultFontLoaded && fonts.getFontById(0) == nullptr &&
			config.errors.policy.defaultFont == DefaultFontFailurePolicy::FailAppImmediately) {
			throw FlowUiException(makeError(ErrorCode::DefaultFontUnavailable, ErrorSite::FontManagerInitialize));
		}

		if (!defaultFontLoaded && fonts.getFontById(0) == nullptr) {
			const std::string fallbackFamilyName =
				config.ui.defaultFontFamily.name.empty() ? std::string("Default") : config.ui.defaultFontFamily.name;
			if (fonts.getFamilyId(fallbackFamilyName) == std::numeric_limits<FontManager::FontFamilyId>::max()) {
				FontFamilyCreateInfo fallbackFamily{};
				fallbackFamily.name = fallbackFamilyName;
				fallbackFamily.faces.clear();
				(void)fonts.createFamily(fallbackFamily);
			}

			const std::array<std::filesystem::path, 5> fallbackCandidates = {
				std::filesystem::path(FLOWUI_SOURCE_DIR) / "assets/fonts/FacultyGlyphic-Regular.arfont",
				std::filesystem::path(FLOWUI_SOURCE_DIR) / "external/msdf-atlas-gen/artery-font-format/example.arfont",
				std::filesystem::path("assets/fonts/FacultyGlyphic-Regular.arfont"),
				std::filesystem::path("external/assets/fonts/FacultyGlyphic-Regular.arfont"),
				std::filesystem::path("external/external/msdf-atlas-gen/artery-font-format/example.arfont"),
			};

			for (const auto& fallbackPath : fallbackCandidates) {
				if (!std::filesystem::is_regular_file(fallbackPath)) continue;
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
						.error = makeError(
							ErrorCode::DefaultFontUnavailable,
							ErrorSite::FontManagerInitialize),
						.kind = ErrorEventKind::Resolved,
						.resolution = ErrorResolution::UsedFallback,
					});
					defaultFontLoaded = true;
					break;
			}
		}

		if (!defaultFontLoaded && fonts.getFontById(0) == nullptr) {
			if (config.errors.policy.defaultFont == DefaultFontFailurePolicy::TryFallbackThenFailApp) {
				throw FlowUiException(makeError(ErrorCode::DefaultFontUnavailable, ErrorSite::FontManagerInitialize));
			}
			detail::reportErrorEvent(ErrorEventView{
				.error = makeError(
					ErrorCode::DefaultFontUnavailable,
					ErrorSite::FontManagerInitialize),
				.kind = ErrorEventKind::Resolved,
				.resolution = ErrorResolution::Skipped,
			});
		}
	}

	void init(App& owner) {
#if FLOW_UI_DEV_MODE
		FLOWUI_DEV_TIMING_ZONE(
			timingRecorder(), devSystems::TimingCategory::Lifecycle,
			devSystems::TimingZoneRole::Work, "flowui.app.init");
#endif
		auto main = std::make_unique<AppWindow>(MainWindowId, makeMainWindowConfig(config), config);
		AppWindow* const mainPointer = main.get();
		windows.emplace(MainWindowId, std::move(main));

		mainPointer->backend = detail::makeDefaultWindowBackend(mainPointer->config.native, &mainPointer->inputQueue);
		mainPointer->backend->setInputConfig(mainPointer->config.native.input);
		mainPointer->ui.setClipboardAccessors(
			[mainPointer](std::string_view text) {
				if (mainPointer->backend) mainPointer->backend->setClipboardText(text);
			},
			[mainPointer]() -> std::string {
				return mainPointer->backend ? mainPointer->backend->getClipboardText() : std::string{};
			});
		mainPointer->ui.setCursorAccessor([mainPointer](CursorType cursorType) {
			if (mainPointer->backend) mainPointer->backend->setCursorType(cursorType);
		});

		vk.createInstance(config, mainPointer->backend->requiredInstanceExtensions());
		mainPointer->surface = vk.createSurface(*mainPointer->backend);
		vk.pickPhysicalDevice(config, mainPointer->surface);
		if (!vk.supportsPresentation(mainPointer->surface)) {
			throw FlowUiException(makeError(ErrorCode::WindowPresentationUnsupported, ErrorSite::AppInitialize));
		}
#if FLOW_UI_DEV_MODE
		vk.devGpuTimingRequested = devMonitoring.timing().config().gpuTimingEnabled;
#if FLOWUI_DEV_MEMORY_LEVEL >= 1
		vk.devGpuMemoryRequested = devMonitoring.memory().config().gpuMemory;
#endif
#endif
		vk.createDevice(config);
#if FLOW_UI_DEV_MODE
		devMonitoring.gpuTiming().initialize(vk);
#if FLOWUI_DEV_MEMORY_LEVEL >= 1
		devMonitoring.memory().initializeEnvironmentProbes(vk);
#endif
#endif

		storageSystem = std::make_unique<storage::FlowStorageSystem>(vk);
		storageConfig = makeStorageConfig(config);
		storageSystem->initialize(storageConfig);
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
		themeManager.registerTheme<FlowUiTheme>("default", FlowUiTheme::dark(), true);
#if COMPILE_FSELI
		themeManager.registerTheme<FSEL::FSELTheme>("default", FSEL::FSELTheme{}, true);
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
		mainPointer->storageRegistered = true;
		elementManager.registerWindow(mainPointer->id);
		mainPointer->ui.initStorage(
			*storageSystem, mainPointer->id, makeUiManagerConfig(config, mainPointer->config));
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
		initSharedUiByteResources(*storageSystem, sharedUiByteResources);
		const storage::TextureHandle fallbackTexture = storageSystem->publishTexture(
			storage::ResourceKey{
				.domain = storage::ResourceDomain::Internal,
				.name = storageSystem->intern("flowui.ui.fallback"),
				.window = InvalidWindowId,
			},
			storage::TextureViewDesc{
				.imageView = sharedUiByteResources.placeholderUiView,
				.sampler = sharedUiByteResources.linearSampler,
				.sourceWidth = 1,
				.sourceHeight = 1,
			});
		storageSystem->setFallbackTexture(fallbackTexture);

		mainPointer->swapchain.create(
			mainPointer->config.native,
			mainPointer->config.vulkan,
			vk,
			mainPointer->surface,
			mainPointer->backend->framebufferExtent());
		mainPointer->frames.create(mainPointer->config.vulkan.framesInFlight, vk);
#if FLOW_UI_DEV_MODE
		devMonitoring.timingReporting().noteFramesInFlight(mainPointer->config.vulkan.framesInFlight);
#endif
		mainPointer->observedFramebufferExtent = mainPointer->backend->framebufferExtent();

		mainPointer->renderer.init(
			mainPointer->config.vulkan,
			mainPointer->config.ui,
			vk,
			mainPointer->swapchain.swapchain.format,
			*storageSystem,
			mainPointer->id,
			sharedUiByteResources,
			storageConfig.initialInstanceBytesPerFrame,
			mainPointer->config.uiTextureDescriptorCapacity,
			config.errors.policy.transientCapacity == StorageCapacityPolicy::GrowWithinBudget);
		imagesInitialized = true;
		imageManager.init(*storageSystem, config.errors.policy.missingImage);
		mainPointer->viewPorts.init(
			*storageSystem, vk, mainPointer->id, mainPointer->config.vulkan.framesInFlight,
			config.errors.policy.missingViewport);
		fontsInitialized = true;
		fonts.init(*storageSystem, config.ui.fontAtlasSize);
#if FLOWUI_INCLUDE_ICON_MANAGER
		iconsInitialized = true;
		icons.init(
			*storageSystem, config.iconManager,
			config.errors.policy.iconGeneration, config.errors.policy.descriptorCapacity);
#if COMPILE_FSELI
		FSEL::standard_icons::registerStandardIcons(icons);
#endif
#endif
#if FLOW_UI_DEV_MODE
		devTooling.catalogues().bindManagers(
			storageSystem.get(), &themeManager, &actionManager, &fonts,
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
		if (!devInterfaceStatus) throw FlowUiException(devInterfaceStatus.error());
#endif
	}

	void requireQuiescent() const {
		if (activeWindowFrame != InvalidWindowId) {
			throw FlowUiException(makeError(
				ErrorCode::FrameAlreadyActive, ErrorSite::AppRequireQuiescent, activeWindowFrame));
		}
	}

	void requirePlatformThread() const {
		if (std::this_thread::get_id() != platformThread) {
			throw FlowUiException(makeError(ErrorCode::WrongThread, ErrorSite::AppRequirePlatformThread));
		}
	}

	void attachBackendAccessors(AppWindow& window) {
		AppWindow* const stable = &window;
		window.ui.setClipboardAccessors(
			[stable](std::string_view text) {
				if (stable->backend) stable->backend->setClipboardText(text);
			},
			[stable]() -> std::string {
				return stable->backend ? stable->backend->getClipboardText() : std::string{};
			});
		window.ui.setCursorAccessor([stable](CursorType cursorType) {
			if (stable->backend) stable->backend->setCursorType(cursorType);
		});
	}

	void destroyUnsubmittedWindow(AppWindow& window) noexcept {
		if (window.backend) window.backend->detachCallbacks();
		try { window.viewPorts.destroyDrained(vk); } catch (...) {}
		if (storageSystem) {
			try { window.renderer.destroy(vk, *storageSystem, window.lastSubmissionSerial); } catch (...) {}
		}
		window.frames.destroy(vk);
		for (SwapchainGeneration& generation : window.retiredSwapchains) generation.destroy(vk);
		window.retiredSwapchains.clear();
		window.swapchain.destroy(vk);
		elementManager.destroyWindow(window.id);
		window.ui.destroyStorage();
		if (storageSystem && window.storageRegistered) {
			try { storageSystem->unregisterWindow(window.id, window.lastSubmissionSerial); } catch (...) {}
			window.storageRegistered = false;
			try { storageSystem->collect(); } catch (...) {}
		}
		if (window.surface != VK_NULL_HANDLE && vk.instance != VK_NULL_HANDLE) {
			vkDestroySurfaceKHR(vk.instance, window.surface, nullptr);
			window.surface = VK_NULL_HANDLE;
		}
		window.backend.reset();
	}

	WindowId reserveWindowId() {
		if (windowIdsExhausted) {
			detail::terminateForFatalError(makeError(ErrorCode::WindowIdSpaceExhausted, ErrorSite::AppCreateWindow));
		}
		const WindowId id = nextWindowId;
		if (id == InvalidWindowId || id == MainWindowId) {
			detail::terminateForFatalError(makeError(ErrorCode::WindowIdSpaceExhausted, ErrorSite::AppCreateWindow));
		}
		if (id == std::numeric_limits<WindowId>::max()) {
			windowIdsExhausted = true;
		} else {
			++nextWindowId;
		}
		return id;
	}

	WindowId createWindow(const WindowConfig& nativeConfig) {
		requirePlatformThread();
		requireQuiescent();
		if (nativeConfig.width <= 0 || nativeConfig.height <= 0) {
			throw FlowUiException(makeError(ErrorCode::InvalidWindowConfiguration, ErrorSite::AppCreateWindow));
		}
		const WindowId id = reserveWindowId();
#if FLOW_UI_DEV_MODE
		FLOWUI_DEV_TIMING_ZONE_ENTITY(
			timingRecorder(), devSystems::TimingCategory::Lifecycle,
			devSystems::TimingZoneRole::Work, "flowui.window.create",
			devSystems::TimingEntityRef::window(id));
#endif
		auto pending = std::make_unique<AppWindow>(id, makeWindowConfig(config, nativeConfig), config);
		try {
			pending->backend = detail::makeDefaultWindowBackend(pending->config.native, &pending->inputQueue);
			pending->backend->setInputConfig(pending->config.native.input);
			attachBackendAccessors(*pending);
			pending->surface = vk.createSurface(*pending->backend);
			if (!vk.supportsPresentation(pending->surface)) {
				throw FlowUiException(makeError(
					ErrorCode::WindowPresentationUnsupported, ErrorSite::AppCreateWindow, id));
			}
			validateSecondarySurface(vk, pending->surface);
			if (!vk.supportsExactPresentCompletion()) {
				throw FlowUiException(makeError(
					ErrorCode::WindowPresentationUnsupported, ErrorSite::AppCreateWindow, id));
			}

			pending->storageSystem = storageSystem.get();
			storageSystem->registerWindow(
				id, makeWindowStorageDesc(*storageSystem, storageConfig, pending->config));
			pending->storageRegistered = true;
			elementManager.registerWindow(id);
			pending->ui.initStorage(
				*storageSystem, id, makeUiManagerConfig(config, pending->config));
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
#if FLOW_UI_DEV_MODE
			Status devInterfaceStatus = devInterface.attachWindow(pending->ui);
			if (!devInterfaceStatus) throw FlowUiException(devInterfaceStatus.error());
#endif
			pending->swapchain.create(
				pending->config.native,
				pending->config.vulkan,
				vk,
				pending->surface,
				pending->backend->framebufferExtent());
			pending->frames.create(pending->config.vulkan.framesInFlight, vk);
#if FLOW_UI_DEV_MODE
			devMonitoring.timingReporting().noteFramesInFlight(pending->config.vulkan.framesInFlight);
#endif
			pending->observedFramebufferExtent = pending->backend->framebufferExtent();
			pending->renderer.init(
				pending->config.vulkan,
				pending->config.ui,
				vk,
				pending->swapchain.swapchain.format,
				*storageSystem,
				id,
				sharedUiByteResources,
				storageConfig.initialInstanceBytesPerFrame,
				pending->config.uiTextureDescriptorCapacity,
				config.errors.policy.transientCapacity == StorageCapacityPolicy::GrowWithinBudget);
			pending->viewPorts.init(
				*storageSystem, vk, id, pending->config.vulkan.framesInFlight,
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
			if (pending) destroyUnsubmittedWindow(*pending);
			throw;
		}
	}

	void pollEventsAndAdvanceSharedManagers() {
		requirePlatformThread();
		requireQuiescent();
#if FLOW_UI_DEV_MODE
		{
			FLOWUI_DEV_TIMING_ZONE(
				timingRecorder(), devSystems::TimingCategory::DevTool,
				devSystems::TimingZoneRole::DevToolWork, "flowui.dev.reporting.consume");
			devMonitoring.timingReporting().consumeThrough(appTick);
			devMonitoring.memoryReporting().consume(appTick);
			devMonitoring.errorReporting().consumeThrough(appTick);
		}
		if (appTick == std::numeric_limits<devSystems::AppTickId>::max()) {
			detail::terminateForFatalError(makeError(ErrorCode::AppTickSpaceExhausted, ErrorSite::AppPollEvents));
		}
		++appTick;
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
		FLOWUI_DEV_TIMING_ZONE(
			timingRecorder(), devSystems::TimingCategory::Lifecycle,
			devSystems::TimingZoneRole::Work, "flowui.app.poll_events");
#endif
		detail::pollWindowSystemEvents();
		themeManager.applyStagedMutations();
#if FLOW_UI_DEV_MODE
		devTooling.commitAtSafePoint(themeManager, &timingRecorder());
#endif
		if (storageSystem) storageSystem->collect();
#if FLOWUI_INCLUDE_ICON_MANAGER
		if (iconsInitialized) icons.beginAppTick();
#endif
	}

	void pollEvents() {
		pollEventsAndAdvanceSharedManagers();
	}

	void cancelStorageFrame(
		AppWindow& window
#if FLOW_UI_DEV_MODE
		, devSystems::TimingRecordFlag timingResult = devSystems::TimingRecordFlag::Canceled
#endif
		) noexcept {
		if (window.storageFrame) {
#if FLOW_UI_DEV_MODE
			devSystems::recordGlobalDevBreadcrumb(
				kDevStorageFrameCancel, window.id, window.storageFrame.epoch);
#endif
			elementManager.cancelWindowFrame(window.id, window.storageFrame.epoch);
			if (storageSystem) storageSystem->cancelFrame(window.storageFrame);
		}
		window.ui.cancelFrameState();
		window.preparedUi = {};
		window.storageReadLease = {};
		window.storageFrame = {};
		if (window.phase != AppWindow::Phase::Closing) window.phase = AppWindow::Phase::Idle;
		if (activeWindowFrame == window.id) activeWindowFrame = InvalidWindowId;
#if FLOW_UI_DEV_MODE
		devSystems::recordGlobalDevBreadcrumb(
			kDevFrameCancel, window.id, window.frameNumber);
		window.preparedGapTiming.end(timingResult);
		window.userBuildTiming.end(timingResult);
		window.frameTotalTiming.end(timingResult);
		window.timingFrame = {};
		window.timingAppTick = 0u;
		timingRecorder().clearFrameContext();
		errorRecorder().clearFrameContext();
#endif
	}

	void completeSubmission(FrameVk::Frame& frame) {
		if (!storageSystem || !frame.storageSubmission) return;
		storageSystem->noteCompleted(frame.storageSubmission);
		frame.storageSubmission = {};
	}

	void completeAllSubmissionsAfterIdle() {
		if (!storageSystem) return;
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

	void drainWindowGraphics(AppWindow& window) {
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

	void collectRetiredSwapchains(AppWindow& window) {
		for (SwapchainGeneration& generation : window.retiredSwapchains) {
			generation.waitForPresentCompletion(vk);
			generation.destroy(vk);
		}
		window.retiredSwapchains.clear();
	}

	void beginFrame(WindowId id) {
		requirePlatformThread();
		AppWindow& window = requireWindow(id);
		if (window.phase != AppWindow::Phase::Idle) {
			throw FlowUiException(makeError(ErrorCode::FramePhaseViolation, ErrorSite::AppBeginFrame, id));
		}
		if (activeWindowFrame != InvalidWindowId) {
			throw FlowUiException(makeError(
				ErrorCode::FrameAlreadyActive, ErrorSite::AppBeginFrame, activeWindowFrame));
		}
		if (!window.backend || window.frames.frames.empty()) {
			throw FlowUiException(makeError(ErrorCode::FrameNotReady, ErrorSite::AppBeginFrame, id));
		}
		if (window.frameNumber == std::numeric_limits<uint64_t>::max()) {
			detail::terminateForFatalError(
				makeError(ErrorCode::FrameNumberSpaceExhausted, ErrorSite::AppBeginFrame, id));
		}
		const uint64_t nextFrameNumber = window.frameNumber + 1u;

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
		devSystems::recordGlobalDevBreadcrumb(
			kDevFrameBegin, window.id, nextFrameNumber);
		window.frameTotalTiming.begin(
			timingRecorder(), devSystems::timing_zones::kWindowFrameTotal,
			devSystems::TimingEntityRef::window(window.id));
#endif
		try {
#if FLOW_UI_DEV_MODE
			devSystems::ManualTimingZone beginTiming(
				timingRecorder(), devSystems::timing_zones::kWindowFrameBegin,
				devSystems::TimingEntityRef::window(window.id));
#endif
			{
#if FLOW_UI_DEV_MODE
				FLOWUI_DEV_TIMING_ZONE_BALANCED(
					timingRecorder(), devSystems::TimingCategory::Input,
					devSystems::TimingZoneRole::Work, "flowui.frame.input_refresh");
#endif
				window.backend->refreshInputSnapshot();
			}
			const auto now = std::chrono::steady_clock::now();
			double deltaTimeSeconds = 0.0;
			if (window.hasPreviousBeginFrameTimestamp) {
				deltaTimeSeconds = std::max(
					0.0, std::chrono::duration<double>(now - window.previousBeginFrameTimestamp).count());
			}
			window.previousBeginFrameTimestamp = now;
			window.hasPreviousBeginFrameTimestamp = true;
#if FLOW_UI_DEV_MODE
			window.ui.performanceDiagnostics().beginFrame(nextFrameNumber, deltaTimeSeconds);
#endif

			FrameVk::Frame& frame = window.frames.getCurrentFrame();
			{
#if FLOW_UI_DEV_MODE
				FLOWUI_DEV_TIMING_ZONE(
					timingRecorder(), devSystems::TimingCategory::Wait,
					devSystems::TimingZoneRole::Wait, "flowui.wait.frame_slot_fence");
#endif
				vkCheck(vkWaitForFences(vk.device, 1, &frame.inFlight, VK_TRUE, UINT64_MAX),
					ErrorSite::AppAcquireFrame);
			}
#if FLOW_UI_DEV_MODE
			devMonitoring.gpuTiming().resolveCompleted(vk, frame.gpuTiming);
#endif
			{
#if FLOW_UI_DEV_MODE
				FLOWUI_DEV_TIMING_ZONE_BALANCED(
					timingRecorder(), devSystems::TimingCategory::Prepare,
					devSystems::TimingZoneRole::Work, "flowui.storage.complete_collect");
#endif
				completeSubmission(frame);
				storageSystem->collect();
			}

			const uint32_t frameSlot = window.frames.currentFrame;
			{
#if FLOW_UI_DEV_MODE
				FLOWUI_DEV_TIMING_ZONE_BALANCED(
					timingRecorder(), devSystems::TimingCategory::Prepare,
					devSystems::TimingZoneRole::Work, "flowui.viewport.frame_start");
#endif
				window.viewPorts.onFrameStart(vk, frameSlot);
			}

			{
#if FLOW_UI_DEV_MODE
				FLOWUI_DEV_TIMING_ZONE_BALANCED(
					timingRecorder(), devSystems::TimingCategory::Prepare,
					devSystems::TimingZoneRole::Work, "flowui.storage.begin_frame");
#endif
				window.storageFrame = storageSystem->beginFrame(window.id, {
					.frameSlot = frameSlot,
					.frameNumber = nextFrameNumber,
				});
#if FLOW_UI_DEV_MODE
				devSystems::recordGlobalDevBreadcrumb(
					kDevStorageFrameBegin, window.id, window.storageFrame.epoch);
#endif
				window.fontFrameView = fonts.frameView(window.storageFrame);
			}
			window.frameNumber = nextFrameNumber;
			activeWindowFrame = window.id;
			window.phase = AppWindow::Phase::Building;

			{
#if FLOW_UI_DEV_MODE
				FLOWUI_DEV_TIMING_ZONE_BALANCED(
					timingRecorder(), devSystems::TimingCategory::Element,
					devSystems::TimingZoneRole::Work, "flowui.element.begin_window_frame");
#endif
				elementManager.beginWindowFrame(window.id, window.storageFrame.epoch);
			}
			{
#if FLOW_UI_DEV_MODE
				FLOWUI_DEV_TIMING_ZONE_BALANCED(
					timingRecorder(), devSystems::TimingCategory::Input,
					devSystems::TimingZoneRole::Work, "flowui.input.drain");
#endif
				window.frameInput = window.inputQueue.drain(deltaTimeSeconds);
				if (window.frameInput.droppedTextInputCount != 0u) {
					detail::reportErrorEvent(ErrorEventView{
						.error = makeError(
							ErrorCode::PlatformInputQueueOverflow,
							ErrorSite::InputQueueEvent,
							window.id,
							window.frameInput.droppedTextInputCount),
						.kind = ErrorEventKind::Resolved,
						.resolution = ErrorResolution::Ignored,
					});
				}
			}
			const float inverseClampedUiScale = 1 / std::max(1.0e-6f, window.config.ui.uiScale);
			constexpr float kBaseScrollSensitivity = 20.0f;
			const VkExtent2D windowExtent = window.backend->windowExtent();
			const VkExtent2D framebufferExtent = window.backend->framebufferExtent();
			if (framebufferExtent.width != window.observedFramebufferExtent.width ||
				framebufferExtent.height != window.observedFramebufferExtent.height) {
				window.framebufferResized = true;
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
			window.ui.beginFrame(
				window.storageFrame, layoutInput, window.fontFrameView, layoutWidth, layoutHeight);
#if FLOW_UI_DEV_MODE
			beginTiming.end();
			window.userBuildTiming.begin(
				timingRecorder(), devSystems::timing_zones::kWindowFrameUserBuild,
				devSystems::TimingEntityRef::window(window.id));
#endif
		} catch (...) {
			cancelStorageFrame(window
#if FLOW_UI_DEV_MODE
				, devSystems::TimingRecordFlag::Exception
#endif
			);
			throw;
		}
	}

	void endFrame(WindowId id) {
		requirePlatformThread();
		AppWindow& window = requireWindow(id);
		if (activeWindowFrame != id || window.phase != AppWindow::Phase::Building || !window.storageFrame) {
			throw FlowUiException(makeError(ErrorCode::FramePhaseViolation, ErrorSite::AppEndFrame, id));
		}
#if FLOW_UI_DEV_MODE
		window.userBuildTiming.end();
#endif
		try {
#if FLOW_UI_DEV_MODE
			devSystems::ManualTimingZone endTiming(
				timingRecorder(), devSystems::timing_zones::kWindowFrameEnd,
				devSystems::TimingEntityRef::window(window.id));
#endif
			window.renderCommands = window.ui.endFrame();
			{
#if FLOW_UI_DEV_MODE
				FLOWUI_DEV_TIMING_ZONE_BALANCED(
					timingRecorder(), devSystems::TimingCategory::Prepare,
					devSystems::TimingZoneRole::Work, "flowui.viewport.prepare_targets");
#endif
				window.viewPorts.prepareFrameTargets(
					window.renderCommands, window.uiToFramebufferScaleX, window.uiToFramebufferScaleY);
			}
#if FLOWUI_INCLUDE_ICON_MANAGER
			{
#if FLOW_UI_DEV_MODE
				FLOWUI_DEV_TIMING_ZONE_BALANCED(
					timingRecorder(), devSystems::TimingCategory::Prepare,
					devSystems::TimingZoneRole::Work, "flowui.icon.prepare_textures");
#endif
				icons.prepareFrameTextures(
					window.renderCommands, window.uiToFramebufferScaleX, window.uiToFramebufferScaleY);
			}
#endif
			window.viewPorts.remapRenderCommandsForFrame(window.renderCommands, window.frames.currentFrame);
			storage::ArenaView textureArena{};
			std::span<storage::TextureHandle> gathered{};
			size_t gatheredCount = 0;
			{
#if FLOW_UI_DEV_MODE
				FLOWUI_DEV_TIMING_ZONE_BALANCED(
					timingRecorder(), devSystems::TimingCategory::Prepare,
					devSystems::TimingZoneRole::Work, "flowui.texture.gather_bindings");
#endif
				textureArena = storageSystem->frameArena(
					window.storageFrame, storage::MemoryClass::FrameTransient);
				gathered = textureArena.allocateArray<storage::TextureHandle>(
					static_cast<size_t>(std::max(0, window.renderCommands.length)));
				for (int32_t commandIndex = 0; commandIndex < window.renderCommands.length; ++commandIndex) {
					const Clay_RenderCommand& command = window.renderCommands.internalArray[commandIndex];
					if (command.commandType != CLAY_RENDER_COMMAND_TYPE_IMAGE) continue;
					const auto* texture = static_cast<const TextureRef*>(command.renderData.image.imageData);
					if (!texture || !texture->handle) continue;
					bool duplicate = false;
					for (size_t i = 0; i < gatheredCount; ++i) {
						if (gathered[i] == texture->handle) {
							duplicate = true;
							break;
						}
					}
					if (!duplicate) gathered[gatheredCount++] = texture->handle;
				}
			}
			storage::PreparedTextureBindings preparedBindings{};
			{
#if FLOW_UI_DEV_MODE
				FLOWUI_DEV_TIMING_ZONE_BALANCED(
					timingRecorder(), devSystems::TimingCategory::Prepare,
					devSystems::TimingZoneRole::Work, "flowui.texture.prepare_bindings");
#endif
				preparedBindings = storageSystem->prepareTextureBindings(
					window.storageFrame, gathered.first(gatheredCount));
				window.renderer.applyTextureBindings(
					vk.device, window.frames.currentFrame, preparedBindings);
				storageSystem->acknowledgeTextureBindings(
					window.storageFrame, preparedBindings.dirtyBindings);
			}
#if FLOW_UI_DEV_MODE
			window.devOverlay.clear();
			devSystems::tooling::DevOverlaySelectionSpec overlaySelection{};
			if (devTooling.overlaySelection(window.id, overlaySelection)) {
				const float scaleX = std::max(window.uiToFramebufferScaleX, 1.0e-6f);
				const float scaleY = std::max(window.uiToFramebufferScaleY, 1.0e-6f);
				devTooling.overlays().generateOverlayCommands(
					overlaySelection,
					window.ui.devTreeSnapshot(),
					static_cast<float>(window.swapchain.swapchain.extent.width) / scaleX,
					static_cast<float>(window.swapchain.swapchain.extent.height) / scaleY,
					window.devOverlay,
					&window.renderCommands);
				const float configuredDpi = std::max(1.0f, window.config.ui.dpi);
				float pointsToPixelsScale = std::max(0.0f, window.config.ui.fontScale) * (configuredDpi / 72.0f);
				if (pointsToPixelsScale <= 0.0f) pointsToPixelsScale = configuredDpi / 72.0f;
				devTooling.overlays().buildUiRendererInstances(
					window.devOverlay,
					window.fontFrameView,
					pointsToPixelsScale,
					scaleX,
					scaleY,
					static_cast<float>(window.swapchain.swapchain.extent.width),
					static_cast<float>(window.swapchain.swapchain.extent.height));
			}
#endif
			window.preparedUi = window.renderer.prepareFrame(
				vk,
				*storageSystem,
				window.storageFrame,
				preparedBindings,
				window.renderCommands,
				window.ui.inputFieldFrameOverrides(),
				window.fontFrameView,
				window.swapchain.swapchain.extent,
				window.uiToFramebufferScaleX,
				window.uiToFramebufferScaleY
#if FLOW_UI_DEV_MODE
				, &window.devOverlay, &timingRecorder()
#endif
				);
			{
#if FLOW_UI_DEV_MODE
				FLOWUI_DEV_TIMING_ZONE_BALANCED(
					timingRecorder(), devSystems::TimingCategory::Prepare,
					devSystems::TimingZoneRole::Work, "flowui.storage.seal_frame");
#endif
				window.storageReadLease = storageSystem->sealFrame(window.storageFrame);
#if FLOW_UI_DEV_MODE
				devSystems::recordGlobalDevBreadcrumb(
					kDevStorageFrameSeal, window.id, window.storageFrame.epoch);
#endif
			}
			{
#if FLOW_UI_DEV_MODE
				FLOWUI_DEV_TIMING_ZONE_BALANCED(
					timingRecorder(), devSystems::TimingCategory::Element,
					devSystems::TimingZoneRole::Work, "flowui.element.commit_window_frame");
#endif
				elementManager.commitWindowFrame(window.id, window.storageFrame.epoch);
			}
			window.phase = AppWindow::Phase::Prepared;
#if FLOW_UI_DEV_MODE
			endTiming.end();
			window.preparedGapTiming.begin(
				timingRecorder(), devSystems::timing_zones::kWindowFramePreparedGap,
				devSystems::TimingEntityRef::window(window.id));
#endif
		} catch (...) {
			cancelStorageFrame(window
#if FLOW_UI_DEV_MODE
				, devSystems::TimingRecordFlag::Exception
#endif
			);
			throw;
		}
	}

	void drawFrame(WindowId id) {
		requirePlatformThread();
		AppWindow& window = requireWindow(id);
		if (activeWindowFrame != id || window.phase != AppWindow::Phase::Prepared) {
			throw FlowUiException(makeError(ErrorCode::FramePhaseViolation, ErrorSite::AppDrawFrame, id));
		}
#if FLOW_UI_DEV_MODE
		window.preparedGapTiming.end();
#endif
		WindowFrameExitGuard frameExit(
			*storageSystem, window, activeWindowFrame
#if FLOW_UI_DEV_MODE
			, timingRecorder(), errorRecorder()
#endif
		);
#if FLOW_UI_DEV_MODE
		devSystems::ManualTimingZone drawTiming(
			timingRecorder(), devSystems::timing_zones::kWindowFrameDraw,
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
			FLOWUI_DEV_TIMING_ZONE_BALANCED(
				timingRecorder(), devSystems::TimingCategory::RendererCpu,
				devSystems::TimingZoneRole::Work, "flowui.swapchain.resize_check");
#endif
			const VkExtent2D framebufferExtent =
				window.backend ? window.backend->framebufferExtent() : VkExtent2D{};
			if (framebufferExtent.width != window.observedFramebufferExtent.width ||
				framebufferExtent.height != window.observedFramebufferExtent.height) {
				window.framebufferResized = true;
				window.observedFramebufferExtent = framebufferExtent;
			}
		}
		if (window.framebufferResized) {
#if FLOW_UI_DEV_MODE
			const auto result = devSystems::TimingRecordFlag::Canceled |
				devSystems::TimingRecordFlag::OutOfDate;
			drawTiming.end(result);
			frameExit.setTimingResult(result);
#endif
			cancelStorageFrame(window
#if FLOW_UI_DEV_MODE
				, result
#endif
			);
			const bool recreated = recreateSwapchainIfNeeded(window);
			detail::reportErrorEvent(ErrorEventView{
				.error = makeError(
					ErrorCode::SwapchainOutOfDate, ErrorSite::AppRecreateWindow,
					id),
				.kind = ErrorEventKind::Resolved,
				.resolution = recreated
					? ErrorResolution::RecreatedWindow
					: ErrorResolution::Skipped,
			});
			return;
		}

		FrameVk::Frame& frame = window.frames.getCurrentFrame();
		uint32_t swapchainImageIndex = 0;
		VkResult acquireResult = VK_SUCCESS;
		{
#if FLOW_UI_DEV_MODE
			FLOWUI_DEV_TIMING_ZONE(
				timingRecorder(), devSystems::TimingCategory::Wait,
				devSystems::TimingZoneRole::Wait, "flowui.wait.acquire_image");
#endif
			acquireResult = vkAcquireNextImageKHR(
				vk.device, window.swapchain.swapchain.swapchain, UINT64_MAX, frame.imageAvailable,
				VK_NULL_HANDLE, &swapchainImageIndex);
		}
		if (acquireResult == VK_ERROR_OUT_OF_DATE_KHR) {
			window.framebufferResized = true;
#if FLOW_UI_DEV_MODE
			const auto result = devSystems::TimingRecordFlag::Canceled |
				devSystems::TimingRecordFlag::OutOfDate;
			drawTiming.end(result);
			frameExit.setTimingResult(result);
#endif
			cancelStorageFrame(window
#if FLOW_UI_DEV_MODE
				, result
#endif
			);
			const bool recreated = recreateSwapchainIfNeeded(window);
			detail::reportErrorEvent(ErrorEventView{
				.error = makeError(
					ErrorCode::SwapchainOutOfDate, ErrorSite::AppAcquireFrame,
					id, 0u, static_cast<std::uint32_t>(acquireResult)),
				.kind = ErrorEventKind::Resolved,
				.resolution = recreated
					? ErrorResolution::RecreatedWindow
					: ErrorResolution::Skipped,
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
			detail::terminateForFatalError(
				makeError(ErrorCode::SwapchainImageInvalid, ErrorSite::AppAcquireFrame, id,
					swapchainImageIndex));
		}
		if (window.swapchain.imageInFlight[swapchainImageIndex] != VK_NULL_HANDLE) {
			FLOWUI_DEV_TIMING_ZONE(
				timingRecorder(), devSystems::TimingCategory::Wait,
				devSystems::TimingZoneRole::Wait, "flowui.wait.swapchain_image_fence");
			vkCheck(vkWaitForFences(vk.device, 1, &window.swapchain.imageInFlight[swapchainImageIndex],
				VK_TRUE, UINT64_MAX),
				ErrorSite::AppAcquireFrame);
		}
		window.swapchain.imageInFlight[swapchainImageIndex] = frame.inFlight;

		{
#if FLOW_UI_DEV_MODE
			FLOWUI_DEV_TIMING_ZONE_BALANCED(
				timingRecorder(), devSystems::TimingCategory::RendererCpu,
				devSystems::TimingZoneRole::Work, "flowui.renderer.command_buffer_begin");
#endif
			vkCheck(vkResetCommandPool(vk.device, frame.pool, 0),
				ErrorSite::AppDrawFrame);
			VkCommandBufferBeginInfo beginInfo{};
			beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
			beginInfo.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
			vkCheck(vkBeginCommandBuffer(frame.cmd, &beginInfo),
				ErrorSite::AppDrawFrame);
		}

#if FLOW_UI_DEV_MODE
		devSystems::GpuTimingCommandContext gpuTiming =
			devMonitoring.gpuTiming().beginFrameRecording(
				vk, frame.gpuTiming, frame.cmd, window.timingFrame, window.timingAppTick);
#endif

		transitionSwapchainImageLayout(frame.cmd, window.swapchain.swapchain.images[swapchainImageIndex],
			window.swapchain.layouts[swapchainImageIndex], VK_IMAGE_LAYOUT_ATTACHMENT_OPTIMAL);
		window.swapchain.layouts[swapchainImageIndex] = VK_IMAGE_LAYOUT_ATTACHMENT_OPTIMAL;

		window.viewPorts.recordFramePasses(
			vk, frame.cmd, window.frames.currentFrame
#if FLOW_UI_DEV_MODE
			, &timingRecorder(), &gpuTiming
#endif
		);
		window.renderer.recordPreparedFrame(
			vk,
			frame.cmd,
			window.swapchain.swapchain.extent,
			window.swapchain.swapchain.views[swapchainImageIndex],
			window.frames.currentFrame,
			window.preparedUi
#if FLOW_UI_DEV_MODE
			, &timingRecorder(), &gpuTiming
#endif
		);

		{
#if FLOW_UI_DEV_MODE
			FLOWUI_DEV_TIMING_ZONE_BALANCED(
				timingRecorder(), devSystems::TimingCategory::RendererCpu,
				devSystems::TimingZoneRole::Work, "flowui.renderer.command_buffer_end");
#endif
			transitionSwapchainImageLayout(frame.cmd, window.swapchain.swapchain.images[swapchainImageIndex],
				window.swapchain.layouts[swapchainImageIndex], VK_IMAGE_LAYOUT_PRESENT_SRC_KHR);
			window.swapchain.layouts[swapchainImageIndex] = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;
#if FLOW_UI_DEV_MODE
			devMonitoring.gpuTiming().endFrameRecording(gpuTiming, frame.cmd);
#endif
			vkCheck(vkEndCommandBuffer(frame.cmd),
				ErrorSite::AppDrawFrame);
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

		vkCheck(vkResetFences(vk.device, 1, &frame.inFlight),
			ErrorSite::AppSubmitFrame);
		{
#if FLOW_UI_DEV_MODE
			FLOWUI_DEV_TIMING_ZONE(
				timingRecorder(), devSystems::TimingCategory::RendererCpu,
				devSystems::TimingZoneRole::Work, "flowui.renderer.queue_submit");
#endif
			vkCheck(vkQueueSubmit(vk.graphicsQ, 1, &submitInfo, frame.inFlight),
				ErrorSite::AppSubmitFrame);
			frame.storageSubmission = storageSystem->noteSubmission(window.storageReadLease);
#if FLOW_UI_DEV_MODE
			devMonitoring.gpuTiming().markSubmitted(
				frame.gpuTiming, frame.storageSubmission.serial);
#endif
		}
		window.lastSubmissionSerial = std::max(window.lastSubmissionSerial, frame.storageSubmission.serial);
		window.swapchain.lastGraphicsUse = std::max(window.swapchain.lastGraphicsUse, frame.storageSubmission.serial);
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
			vkCheck(vkResetFences(vk.device, 1, &presentFence),
				ErrorSite::AppPresent);
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
			FLOWUI_DEV_TIMING_ZONE(
				timingRecorder(), devSystems::TimingCategory::Wait,
				devSystems::TimingZoneRole::Wait, "flowui.wait.present_call");
#endif
			presentResult = vkQueuePresentKHR(vk.presentQ, &presentInfo);
		}
		if (presentId != 0 && (presentResult == VK_SUCCESS || presentResult == VK_SUBOPTIMAL_KHR)) {
			window.swapchain.lastPresentId = presentId;
		}
		if (vk.wsiRetirementMode == WsiRetirementMode::PresentFence &&
			(presentResult == VK_SUCCESS || presentResult == VK_SUBOPTIMAL_KHR)) {
			window.swapchain.presentPending[swapchainImageIndex] = 1u;
		}
		if (presentResult == VK_ERROR_OUT_OF_DATE_KHR || presentResult == VK_SUBOPTIMAL_KHR ||
			acquiredSuboptimalSwapchain || window.framebufferResized) {
			window.framebufferResized = true;
			const bool recreated = recreateSwapchainIfNeeded(window);
			const VkResult observedResult = presentResult != VK_SUCCESS
				? presentResult
				: acquireResult;
			detail::reportErrorEvent(ErrorEventView{
				.error = makeError(
					ErrorCode::SwapchainOutOfDate, ErrorSite::AppPresent,
					id, 0u, static_cast<std::uint32_t>(observedResult)),
				.kind = ErrorEventKind::Resolved,
				.resolution = recreated
					? ErrorResolution::RecreatedWindow
					: ErrorResolution::Skipped,
			});
		} else if (presentResult != VK_SUCCESS) {
			vkCheck(presentResult,
				ErrorSite::AppPresent, ErrorCode::PresentationFailed);
		}

		window.frames.advance();
#if FLOW_UI_DEV_MODE
		window.ui.performanceDiagnostics().endCompletedFrame();
		drawTiming.end();
		window.frameTotalTiming.end();
		window.timingFrame = {};
		window.timingAppTick = 0u;
		timingRecorder().clearFrameContext();
		frameExit.setTimingResult(devSystems::TimingRecordFlag::Completed);
#endif
	}

	bool recreateSwapchainIfNeeded(AppWindow& window) {
		if (!window.backend || vk.device == VK_NULL_HANDLE || window.surface == VK_NULL_HANDLE) return false;
		const VkExtent2D framebufferExtent = window.backend->framebufferExtent();
		window.observedFramebufferExtent = framebufferExtent;
		if (framebufferExtent.width == 0 || framebufferExtent.height == 0) {
			window.framebufferResized = true;
			return false;
		}
#if FLOW_UI_DEV_MODE
		FLOWUI_DEV_TIMING_ZONE_ENTITY(
			timingRecorder(), devSystems::TimingCategory::Lifecycle,
			devSystems::TimingZoneRole::Work, "flowui.window.recreate_swapchain",
			devSystems::TimingEntityRef::window(window.id));
#endif

		if (!vk.supportsExactPresentCompletion()) {
			if (windows.size() != 1u || window.id != mainWindowId) {
				throw FlowUiException(makeError(
					ErrorCode::WindowPresentationUnsupported, ErrorSite::AppRecreateWindow,
					window.id));
			}
			vkCheck(vkDeviceWaitIdle(vk.device),
				ErrorSite::AppRecreateWindow);
			completeAllSubmissionsAfterIdle();
			SwapchainGeneration replacement{};
			try {
				replacement.create(
					window.config.native,
					window.config.vulkan,
					vk,
					window.surface,
					framebufferExtent,
					window.swapchain.swapchain.swapchain);
			} catch (...) {
				if (window.backend) window.backend->setShouldClose(1);
				throw;
			}
			try {
				window.renderer.onSwapchainFormatChanged(
					vk, replacement.swapchain.format, window.lastSubmissionSerial);
			} catch (...) {
				replacement.destroy(vk);
				if (window.backend) window.backend->setShouldClose(1);
				throw;
			}
			window.swapchain.destroy(vk);
			window.swapchain = std::move(replacement);
		} else {
			drainWindowGraphics(window);
			SwapchainGeneration replacement{};
			try {
				replacement.create(
					window.config.native,
					window.config.vulkan,
					vk,
					window.surface,
					framebufferExtent,
					window.swapchain.swapchain.swapchain);
			} catch (...) {
				if (window.backend) window.backend->setShouldClose(1);
				throw;
			}
			try {
				window.renderer.onSwapchainFormatChanged(
					vk, replacement.swapchain.format, window.lastSubmissionSerial);
				window.retiredSwapchains.reserve(window.retiredSwapchains.size() + 1u);
			} catch (...) {
				replacement.destroy(vk);
				if (window.backend) window.backend->setShouldClose(1);
				throw;
			}
			window.retiredSwapchains.push_back(std::move(window.swapchain));
			window.swapchain = std::move(replacement);
			collectRetiredSwapchains(window);
			if (!window.storageFrame) storageSystem->collect();
		}
		window.framebufferResized = false;
		return true;
	}

	void destroyWindow(WindowId id) {
		requirePlatformThread();
		if (id == mainWindowId) {
			throw FlowUiException(makeError(
				ErrorCode::MainWindowDestructionForbidden, ErrorSite::AppDestroyWindow, id));
		}
		AppWindow& window = requireWindow(id);
#if FLOW_UI_DEV_MODE && FLOWUI_DEV_MEMORY_LEVEL >= 2
		unregisterWindowMemoryProbe(window);
#endif
		if (activeWindowFrame != InvalidWindowId && activeWindowFrame != id) {
			throw FlowUiException(makeError(
				ErrorCode::FrameAlreadyActive, ErrorSite::AppDestroyWindow, activeWindowFrame));
		}
		if (activeWindowFrame == id) cancelStorageFrame(window);
#if FLOW_UI_DEV_MODE
		FLOWUI_DEV_TIMING_ZONE_ENTITY(
			timingRecorder(), devSystems::TimingCategory::Lifecycle,
			devSystems::TimingZoneRole::Work, "flowui.window.destroy",
			devSystems::TimingEntityRef::window(id));
#endif
		window.phase = AppWindow::Phase::Closing;
		if (window.backend) window.backend->detachCallbacks();

		drainWindowGraphics(window);
		collectRetiredSwapchains(window);
		window.swapchain.waitForPresentCompletion(vk);
		window.renderer.destroy(vk, *storageSystem, window.lastSubmissionSerial);
		window.viewPorts.destroyDrained(vk);
		window.frames.destroy(vk);
		window.swapchain.destroy(vk);
		elementManager.destroyWindow(id);
		window.ui.destroyStorage();
		if (window.storageRegistered) {
			storageSystem->unregisterWindow(id, window.lastSubmissionSerial);
			window.storageRegistered = false;
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
		std::erase_if(managedWindows, [id](const ManagedWindowEntry& entry) {
			return entry.id == id;
		});
		activeWindowFrame = InvalidWindowId;
	}

	void cleanup() noexcept {
		if (cleanedUp) return;
		cleanedUp = true;
		for (auto& [_, window] : windows) cancelStorageFrame(*window);
#if FLOW_UI_DEV_MODE
		devSystems::ManualTimingZone shutdownTiming(
			timingRecorder(), devSystems::makeBuiltinTimingDescriptor(
				0xd8d6bc916b0f4a41ull, devSystems::TimingCategory::Lifecycle,
				devSystems::TimingZoneRole::Work, devSystems::CpuTimingLevel::Summary,
				"flowui.app.shutdown"));
#endif
		if (vk.device != VK_NULL_HANDLE) {
			(void)vkDeviceWaitIdle(vk.device);
			try { completeAllSubmissionsAfterIdle(); } catch (...) {}
		}
		for (auto& [_, window] : windows) {
#if FLOW_UI_DEV_MODE && FLOWUI_DEV_MEMORY_LEVEL >= 2
			unregisterWindowMemoryProbe(*window);
#endif
			elementManager.destroyWindow(window->id);
		}
#if FLOW_UI_DEV_MODE && FLOWUI_DEV_MEMORY_LEVEL >= 2
		unregisterAppMemoryProbes();
#endif
		elementManager.destroy();
		actionManager.destroy();
		themeManager.destroy();

		if (imagesInitialized) imageManager.destroy();
#if FLOWUI_INCLUDE_ICON_MANAGER
		if (iconsInitialized) icons.destroy();
#endif
		if (fontsInitialized) fonts.destroy();

		for (auto& [_, window] : windows) {
			window->viewPorts.destroyDrained(vk);
			if (storageSystem) {
				try { window->renderer.destroy(vk, *storageSystem, window->lastSubmissionSerial); } catch (...) {}
			}
			window->frames.destroy(vk);
			for (SwapchainGeneration& generation : window->retiredSwapchains) generation.destroy(vk);
			window->retiredSwapchains.clear();
			window->swapchain.destroy(vk);
			window->ui.destroyStorage();
			if (storageSystem && window->storageRegistered) {
				try { storageSystem->unregisterWindow(window->id, storageSystem->completedSerial()); } catch (...) {}
				window->storageRegistered = false;
			}
			if (window->surface != VK_NULL_HANDLE && vk.instance != VK_NULL_HANDLE) {
				vkDestroySurfaceKHR(vk.instance, window->surface, nullptr);
				window->surface = VK_NULL_HANDLE;
			}
			window->backend.reset();
		}
		if (storageSystem) {
			destroySharedUiByteResources(*storageSystem, sharedUiByteResources);
			try { storageSystem->collect(); } catch (...) {}
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
		shutdownTiming.end();
		devMonitoring.timingReporting().consumeThrough(appTick);
		devMonitoring.memoryReporting().consume(appTick);
		devMonitoring.errorReporting().consumeThrough(appTick);
#endif
	}
};

App::App() = default;

App::App(App&& other) noexcept
	: impl_(std::move(other.impl_)) {
	if (impl_) {
		impl_->elementManager.rebindOwner(*this);
		impl_->actionManager.rebindOwner(*this);
	}
}

App& App::operator=(App&& other) noexcept {
	if (this == &other) return *this;
	impl_ = std::move(other.impl_);
	if (impl_) {
		impl_->elementManager.rebindOwner(*this);
		impl_->actionManager.rebindOwner(*this);
	}
	return *this;
}

App::~App() = default;

WindowId App::mainWindowId() const noexcept {
	return impl_ ? impl_->mainWindowId : MainWindowId;
}

void App::reportError(FlowUiError error) const noexcept {
	if (!impl_) return;
	detail::reportErrorEvent(ErrorEventView{
		.error = error,
		.kind = ErrorEventKind::Reported,
	});
}

Result<WindowId> App::createWindow(const WindowConfig& config) {
	if (!impl_) return unexpectedError(makeError(ErrorCode::AppUnavailable, ErrorSite::AppCreateWindow));
	try {
		return impl_->createWindow(config);
	} catch (const FlowUiException& exception) {
		if (exception.error().descriptor().category == ErrorCategory::Fatal) {
			detail::terminateForFatalError(exception.error());
		}
		return unexpectedError(exception.error());
	}
}

Result<WindowId> App::createWindowLikeMain() {
	return createWindow(WindowConfigOverrides{});
}

Result<WindowId> App::createWindow(const WindowConfigOverrides& overrides) {
	if (!impl_) return unexpectedError(makeError(ErrorCode::AppUnavailable, ErrorSite::AppCreateWindow));
	WindowConfig config = mergeWindowConfig(impl_->config.window, overrides);
	if (!overrides.title) {
		config.title += " - Window " + std::to_string(impl_->nextWindowId);
	}
	return createWindow(config);
}

Result<WindowId> App::createWindow(std::string_view title, int width, int height) {
	WindowConfigOverrides overrides{};
	overrides.title = std::string(title);
	if (width != 0) overrides.width = width;
	if (height != 0) overrides.height = height;
	return createWindow(overrides);
}

Result<WindowId> App::createWindow(
	const WindowConfigOverrides& overrides,
	UiBuildCallback buildUi,
	ManagedWindowFlags flags) {
	if (!buildUi) {
		return unexpectedError(makeError(
			ErrorCode::InvalidWindowConfiguration, ErrorSite::AppCreateWindow));
	}
	auto created = createWindow(overrides);
	if (!created) return created;
	Status registered = setWindowUiCallback(*created, std::move(buildUi), flags);
	if (!registered) {
		(void)destroyWindow(*created);
		return unexpectedError(registered.error());
	}
	return created;
}

Status App::setWindowUiCallback(
	WindowId id,
	UiBuildCallback buildUi,
	ManagedWindowFlags flags) {
	if (!impl_) return unexpectedError(makeError(ErrorCode::AppUnavailable, ErrorSite::AppFrameDispatch));
	return runLocalOperation([&] {
		impl_->requirePlatformThread();
		impl_->requireQuiescent();
		(void)impl_->requireWindow(id);
		if (id == impl_->mainWindowId || !buildUi) {
			throw FlowUiException(makeError(
				ErrorCode::InvalidWindowConfiguration, ErrorSite::AppFrameDispatch, id));
		}
		const auto found = std::find_if(
			impl_->managedWindows.begin(), impl_->managedWindows.end(),
			[id](const Impl::ManagedWindowEntry& entry) { return entry.id == id; });
		if (found != impl_->managedWindows.end()) {
			found->buildUi = std::move(buildUi);
			found->flags = flags;
			return;
		}
		impl_->managedWindows.push_back(Impl::ManagedWindowEntry{
			.id = id,
			.buildUi = std::move(buildUi),
			.flags = flags,
		});
	});
}

void App::removeWindowUiCallback(WindowId id) {
	if (!impl_) return;
	std::erase_if(impl_->managedWindows, [id](const Impl::ManagedWindowEntry& entry) {
		return entry.id == id;
	});
}

Status App::dispatchManagedWindows() {
	if (!impl_) return unexpectedError(makeError(ErrorCode::AppUnavailable, ErrorSite::AppFrameDispatch));
	try {
		impl_->requirePlatformThread();
		impl_->requireQuiescent();
	} catch (const FlowUiException& exception) {
		if (exception.error().descriptor().category == ErrorCategory::Fatal) {
			detail::terminateForFatalError(exception.error());
		}
		if (exception.error().descriptor().category != ErrorCategory::Local) throw;
		return unexpectedError(exception.error());
	}

	std::vector<WindowId> dispatchOrder;
	dispatchOrder.reserve(impl_->managedWindows.size());
	for (const Impl::ManagedWindowEntry& entry : impl_->managedWindows) {
		dispatchOrder.push_back(entry.id);
	}

	FlowUiError firstError{};
	const auto rememberError = [&](FlowUiError error) {
		if (!firstError) firstError = error;
	};
	const auto rememberException = [&](const FlowUiException& exception) {
		const FlowUiError error = exception.error();
		if (error.descriptor().category == ErrorCategory::Fatal) {
			detail::terminateForFatalError(error);
		}
		if (error.descriptor().category != ErrorCategory::Local) throw exception;
		rememberError(error);
	};

	for (WindowId id : dispatchOrder) {
		const auto managed = std::find_if(
			impl_->managedWindows.begin(), impl_->managedWindows.end(),
			[id](const Impl::ManagedWindowEntry& entry) { return entry.id == id; });
		if (managed == impl_->managedWindows.end()) continue;
		UiBuildCallback buildUi = managed->buildUi;
		const ManagedWindowFlags flags = managed->flags;
		const auto windowIt = impl_->windows.find(id);
		if (windowIt == impl_->windows.end()) {
			removeWindowUiCallback(id);
			continue;
		}
		if (!windowIt->second->backend || windowIt->second->backend->shouldClose()) {
			if (flags.autoDestroyOnClose) {
				try {
					impl_->destroyWindow(id);
				} catch (const FlowUiException& exception) {
					rememberException(exception);
				}
			}
			continue;
		}

		try {
			impl_->beginFrame(id);
		} catch (const FlowUiException& exception) {
			rememberException(exception);
			continue;
		}

		try {
			buildUi(windowIt->second->ui, id);
		} catch (...) {
			const auto current = impl_->windows.find(id);
			if (current != impl_->windows.end()) impl_->cancelStorageFrame(*current->second);
			const FlowUiError error = makeError(
				ErrorCode::UiBuildCallbackFailed, ErrorSite::AppFrameDispatch, id);
			reportError(error);
			rememberError(error);
			continue;
		}

		try {
			impl_->endFrame(id);
			impl_->drawFrame(id);
		} catch (const FlowUiException& exception) {
			rememberException(exception);
		}
	}

	if (firstError) return unexpectedError(firstError);
	return {};
}

Status App::destroyWindow(WindowId id) {
	if (!impl_) return unexpectedError(makeError(ErrorCode::AppUnavailable, ErrorSite::AppDestroyWindow));
	return runLocalOperation([&] { impl_->destroyWindow(id); });
}

bool App::hasWindow(WindowId id) const noexcept {
	return impl_ && id != InvalidWindowId && impl_->windows.contains(id);
}

Status App::pollEvents() {
	if (!impl_) return unexpectedError(makeError(ErrorCode::AppUnavailable, ErrorSite::AppAccessWindow));
	return runLocalOperation([&] { impl_->pollEventsAndAdvanceSharedManagers(); });
}

bool App::shouldClose() const {
	if (!impl_) return true;
	return shouldClose(impl_->mainWindowId);
}

bool App::shouldClose(WindowId id) const {
	if (!impl_) throw FlowUiException(makeError(ErrorCode::AppUnavailable, ErrorSite::AppAccessWindow));
	const AppWindow& window = impl_->requireWindow(id);
	return !window.backend || window.backend->shouldClose();
}

void App::setShouldClose(int value) {
	if (!impl_) return;
	setShouldClose(impl_->mainWindowId, value);
}

void App::setShouldClose(WindowId id, int value) {
	if (!impl_) throw FlowUiException(makeError(ErrorCode::AppUnavailable, ErrorSite::AppAccessWindow));
	AppWindow& window = impl_->requireWindow(id);
	if (window.backend) window.backend->setShouldClose(value);
}

Status App::beginFrame() {
	if (!impl_) return unexpectedError(makeError(ErrorCode::AppUnavailable, ErrorSite::AppBeginFrame));
	return runLocalOperation([&] {
		impl_->pollEventsAndAdvanceSharedManagers();
		impl_->beginFrame(impl_->mainWindowId);
	});
}

Status App::beginFrame(WindowId id) {
	if (!impl_) return unexpectedError(makeError(ErrorCode::AppUnavailable, ErrorSite::AppBeginFrame));
	return runLocalOperation([&] { impl_->beginFrame(id); });
}

Status App::endFrame() {
	if (!impl_) return unexpectedError(makeError(ErrorCode::AppUnavailable, ErrorSite::AppEndFrame));
	return runLocalOperation([&] { impl_->endFrame(impl_->mainWindowId); });
}

Status App::endFrame(WindowId id) {
	if (!impl_) return unexpectedError(makeError(ErrorCode::AppUnavailable, ErrorSite::AppEndFrame));
	return runLocalOperation([&] { impl_->endFrame(id); });
}

Status App::drawFrame() {
	if (!impl_) return unexpectedError(makeError(ErrorCode::AppUnavailable, ErrorSite::AppDrawFrame));
	Status mainStatus = drawFrame(impl_->mainWindowId);
	if (!mainStatus) return mainStatus;
#if FLOW_UI_DEV_MODE
	Status devInterfaceStatus = impl_->devInterface.synchronize(*this);
	if (!devInterfaceStatus) return devInterfaceStatus;
#endif
	return dispatchManagedWindows();
}

Status App::drawFrame(WindowId id) {
	if (!impl_) return unexpectedError(makeError(ErrorCode::AppUnavailable, ErrorSite::AppDrawFrame));
	return runLocalOperation([&] { impl_->drawFrame(id); });
}

FontManager& App::fonts() {
	if (!impl_) {
		throw FlowUiException(makeError(ErrorCode::AppUnavailable, ErrorSite::AppAccessFonts));
	}
	return impl_->fonts;
}

const FontManager& App::fonts() const {
	if (!impl_) {
		throw FlowUiException(makeError(ErrorCode::AppUnavailable, ErrorSite::AppAccessFonts));
	}
	return impl_->fonts;
}

ImageManager& App::images() {
	if (!impl_) {
		throw FlowUiException(makeError(ErrorCode::AppUnavailable, ErrorSite::AppAccessImages));
	}
	return impl_->imageManager;
}

const ImageManager& App::images() const {
	if (!impl_) {
		throw FlowUiException(makeError(ErrorCode::AppUnavailable, ErrorSite::AppAccessImages));
	}
	return impl_->imageManager;
}

ThemeManager& App::themes() {
	if (!impl_) {
		throw FlowUiException(makeError(ErrorCode::AppUnavailable, ErrorSite::AppAccessThemes));
	}
	return impl_->themeManager;
}

const ThemeManager& App::themes() const {
	if (!impl_) {
		throw FlowUiException(makeError(ErrorCode::AppUnavailable, ErrorSite::AppAccessThemes));
	}
	return impl_->themeManager;
}

ElementManager& App::elements() {
	if (!impl_) throw FlowUiException(makeError(ErrorCode::AppUnavailable, ErrorSite::AppAccessElements));
	return impl_->elementManager;
}

const ElementManager& App::elements() const {
	if (!impl_) throw FlowUiException(makeError(ErrorCode::AppUnavailable, ErrorSite::AppAccessElements));
	return impl_->elementManager;
}

ActionManager& App::actions() {
	if (!impl_) throw FlowUiException(makeError(ErrorCode::AppUnavailable, ErrorSite::AppAccessActions));
	return impl_->actionManager;
}

const ActionManager& App::actions() const {
	if (!impl_) throw FlowUiException(makeError(ErrorCode::AppUnavailable, ErrorSite::AppAccessActions));
	return impl_->actionManager;
}

#if FLOW_UI_DEV_MODE
devSystems::DevMonitoringAndReporting& App::devMonitoring() {
	if (!impl_) throw FlowUiException(makeError(ErrorCode::AppUnavailable, ErrorSite::AppAccessDevMonitoring));
	return impl_->devMonitoring;
}

const devSystems::DevMonitoringAndReporting& App::devMonitoring() const {
	if (!impl_) throw FlowUiException(makeError(ErrorCode::AppUnavailable, ErrorSite::AppAccessDevMonitoring));
	return impl_->devMonitoring;
}

devSystems::DevTooling& App::devTooling() {
	if (!impl_) throw FlowUiException(makeError(ErrorCode::AppUnavailable, ErrorSite::AppAccessDevMonitoring));
	return impl_->devTooling;
}

const devSystems::DevTooling& App::devTooling() const {
	if (!impl_) throw FlowUiException(makeError(ErrorCode::AppUnavailable, ErrorSite::AppAccessDevMonitoring));
	return impl_->devTooling;
}

std::vector<DevWindowInfo> App::devWindowSnapshot() const {
	if (!impl_) return {};
	std::vector<DevWindowInfo> result;
	result.reserve(impl_->windows.size());
	for (const auto& [id, window] : impl_->windows) {
		if (!window) continue;
		result.push_back(DevWindowInfo{
			.id = id,
			.title = window->config.native.title,
			.framesInFlight = window->config.vulkan.framesInFlight,
		});
	}
	std::ranges::sort(result, {}, &DevWindowInfo::id);
	return result;
}

devSystems::DevUiReplaySource App::devUiReplaySource(WindowId id) noexcept {
	if (!impl_) return {};
	const auto found = impl_->windows.find(id);
	if (found == impl_->windows.end() || !found->second) return {};
	AppWindow& window = *found->second;
	const float configuredDpi = std::max(1.0f, window.config.ui.dpi);
	float pointsToPixelsScale =
		std::max(0.0f, window.config.ui.fontScale) * (configuredDpi / 72.0f);
	if (pointsToPixelsScale <= 0.0f) pointsToPixelsScale = configuredDpi / 72.0f;
	return devSystems::DevUiReplaySource{
		.renderer = &window.renderer,
		.prepared = &window.preparedUi,
		.fontFrameView = &window.fontFrameView,
		.renderCommands = &window.renderCommands,
		.extentWidth = window.swapchain.swapchain.extent.width,
		.extentHeight = window.swapchain.swapchain.extent.height,
		.pointsToPixelsScale = pointsToPixelsScale,
		.uiToFramebufferScaleX = window.uiToFramebufferScaleX,
		.uiToFramebufferScaleY = window.uiToFramebufferScaleY,
	};
}
#endif

#if FLOWUI_INCLUDE_ICON_MANAGER
IconManager& App::icons() {
	if (!impl_) {
		throw FlowUiException(makeError(ErrorCode::AppUnavailable, ErrorSite::AppAccessIcons));
	}
	return impl_->icons;
}

const IconManager& App::icons() const {
	if (!impl_) {
		throw FlowUiException(makeError(ErrorCode::AppUnavailable, ErrorSite::AppAccessIcons));
	}
	return impl_->icons;
}
#endif

#if FLOWUI_PUBLIC_VULKAN_INTEROP
ViewPortManager& App::viewPorts() {
	if (!impl_) {
		throw FlowUiException(makeError(ErrorCode::AppUnavailable, ErrorSite::AppAccessViewports));
	}
	return impl_->mainWindow().viewPorts;
}

ViewPortManager& App::viewPorts(WindowId id) {
	if (!impl_) throw FlowUiException(makeError(ErrorCode::AppUnavailable, ErrorSite::AppAccessViewports));
	return impl_->requireWindow(id).viewPorts;
}

const ViewPortManager& App::viewPorts() const {
	if (!impl_) {
		throw FlowUiException(makeError(ErrorCode::AppUnavailable, ErrorSite::AppAccessViewports));
	}
	return impl_->mainWindow().viewPorts;
}

const ViewPortManager& App::viewPorts(WindowId id) const {
	if (!impl_) throw FlowUiException(makeError(ErrorCode::AppUnavailable, ErrorSite::AppAccessViewports));
	return impl_->requireWindow(id).viewPorts;
}
#endif

UiManager& App::ui() {
	if (!impl_) {
		throw FlowUiException(makeError(ErrorCode::AppUnavailable, ErrorSite::AppAccessUi));
	}
	return impl_->mainWindow().ui;
}

UiManager& App::ui(WindowId id) {
	if (!impl_) throw FlowUiException(makeError(ErrorCode::AppUnavailable, ErrorSite::AppAccessUi));
	return impl_->requireWindow(id).ui;
}

const UiManager& App::ui() const {
	if (!impl_) {
		throw FlowUiException(makeError(ErrorCode::AppUnavailable, ErrorSite::AppAccessUi));
	}
	return impl_->mainWindow().ui;
}

const UiManager& App::ui(WindowId id) const {
	if (!impl_) throw FlowUiException(makeError(ErrorCode::AppUnavailable, ErrorSite::AppAccessUi));
	return impl_->requireWindow(id).ui;
}

void App::setWindowTitle(std::string_view title) {
	if (!impl_) return;
	setWindowTitle(impl_->mainWindowId, title);
}

void App::setWindowTitle(WindowId id, std::string_view title) {
	if (!impl_) throw FlowUiException(makeError(ErrorCode::AppUnavailable, ErrorSite::WindowSetTitle));
	AppWindow& window = impl_->requireWindow(id);
	window.config.native.title.assign(title);
	if (window.backend) window.backend->setTitle(title);
}

void* App::nativeWindowHandle() const {
	if (!impl_) return nullptr;
	return nativeWindowHandle(impl_->mainWindowId);
}

void* App::nativeWindowHandle(WindowId id) const {
	if (!impl_) throw FlowUiException(makeError(ErrorCode::AppUnavailable, ErrorSite::AppAccessWindow));
	const AppWindow& window = impl_->requireWindow(id);
	return window.backend ? window.backend->nativeHandle() : nullptr;
}

void App::setWindowInputConfig(const WindowInputConfig& config) {
	if (!impl_) return;
	setWindowInputConfig(impl_->mainWindowId, config);
}

void App::setWindowInputConfig(WindowId id, const WindowInputConfig& config) {
	if (!impl_) throw FlowUiException(makeError(ErrorCode::AppUnavailable, ErrorSite::AppAccessWindow));
	AppWindow& window = impl_->requireWindow(id);
	window.config.native.input = config;
	if (window.backend) window.backend->setInputConfig(config);
}

WindowInputConfig App::windowInputConfig() const {
	if (!impl_) return {};
	return windowInputConfig(impl_->mainWindowId);
}

WindowInputConfig App::windowInputConfig(WindowId id) const {
	if (!impl_) throw FlowUiException(makeError(ErrorCode::AppUnavailable, ErrorSite::AppAccessWindow));
	const AppWindow& window = impl_->requireWindow(id);
	return window.backend ? window.backend->getInputConfig() : WindowInputConfig{};
}

bool App::supportsRawMouseMotion() const {
	if (!impl_) return false;
	return supportsRawMouseMotion(impl_->mainWindowId);
}

bool App::supportsRawMouseMotion(WindowId id) const {
	if (!impl_) throw FlowUiException(makeError(ErrorCode::AppUnavailable, ErrorSite::AppAccessWindow));
	const AppWindow& window = impl_->requireWindow(id);
	return window.backend && window.backend->supportsRawMouseMotion();
}

void App::setClipboardText(std::string_view text) {
	if (!impl_) return;
	setClipboardText(impl_->mainWindowId, text);
}

void App::setClipboardText(WindowId id, std::string_view text) {
	if (!impl_) throw FlowUiException(makeError(ErrorCode::AppUnavailable, ErrorSite::WindowWriteClipboard));
	AppWindow& window = impl_->requireWindow(id);
	if (window.backend) window.backend->setClipboardText(text);
}

std::string App::clipboardText() const {
	if (!impl_) return {};
	return clipboardText(impl_->mainWindowId);
}

std::string App::clipboardText(WindowId id) const {
	if (!impl_) throw FlowUiException(makeError(ErrorCode::AppUnavailable, ErrorSite::WindowReadClipboard));
	const AppWindow& window = impl_->requireWindow(id);
	return window.backend ? window.backend->getClipboardText() : std::string{};
}

std::pair<int, int> App::windowSize() const {
	if (!impl_) return {0, 0};
	return windowSize(impl_->mainWindowId);
}

std::pair<int, int> App::windowSize(WindowId id) const {
	if (!impl_) throw FlowUiException(makeError(ErrorCode::AppUnavailable, ErrorSite::AppAccessWindow));
	const AppWindow& window = impl_->requireWindow(id);
	if (!window.backend) return {0, 0};
	const VkExtent2D extent = window.backend->windowExtent();
	return {static_cast<int>(extent.width), static_cast<int>(extent.height)};
}

std::pair<int, int> App::framebufferSize() const {
	if (!impl_) return {0, 0};
	return framebufferSize(impl_->mainWindowId);
}

std::pair<int, int> App::framebufferSize(WindowId id) const {
	if (!impl_) throw FlowUiException(makeError(ErrorCode::AppUnavailable, ErrorSite::AppAccessWindow));
	const AppWindow& window = impl_->requireWindow(id);
	if (!window.backend) return {0, 0};
	const VkExtent2D extent = window.backend->framebufferExtent();
	return {static_cast<int>(extent.width), static_cast<int>(extent.height)};
}

App makeApplication(const AppConfig& cfg) {
	App app;
	app.impl_ = std::make_unique<App::Impl>(cfg);
#if FLOW_UI_DEV_MODE
	FLOWUI_DEV_TIMING_ZONE(
		app.impl_->timingRecorder(), devSystems::TimingCategory::Lifecycle,
		devSystems::TimingZoneRole::Work, "flowui.app.make");
#endif
	app.impl_->init(app);
	return app;
}

} // namespace FlowUi
