#pragma once

#include "FlowUi/Private/utility/EnumFlags.hpp"

#include "devSystems/devInterface/Memory/Workbench/DevMemoryHistoryViewport.hpp"
#include "devSystems/devInterface/Performance/Workbench/DevTimelineViewport.hpp"
#define FLOWUI_INTERNAL_VIEWPORT_MANAGER 1
#include "FlowUi/Private/storageSystem/capacity/InitialMemoryCapacity.hpp"
#include "FlowUi/Private/storageSystem/capacity/MemoryCapacityProfile.hpp"
#include "FlowUi/Private/storageSystem/capacity/MemoryDemandPeaks.hpp"
#include "FlowUi/Public/App.hpp"
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
#include "FlowUi/Private/inputService/InputService.hpp"
#include "FlowUi/Private/platformProviderService/PlatformProviderService.hpp"
#include "FlowUi/Private/platformProviderService/WindowPlatformServices.hpp"
#include "FlowUi/Private/renderer/Vk_UiRenderer.hpp"
#include "FlowUi/Private/renderer/vulkan/Vk_Context.hpp"
#include "FlowUi/Private/renderer/vulkan/Vk_Frames.hpp"
#include "FlowUi/Private/renderer/vulkan/Vk_Swapchain.hpp"
#include "FlowUi/Private/resourceService/FontCatalogController.hpp"
#include "FlowUi/Private/storageSystem/FlowStorageSystem.hpp"

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

namespace FlowUi::detail::app_runtime {

class AppErrorObserver {
public:
	explicit AppErrorObserver(FlowUi::ErrorObserverConfig config
#if FLOW_UI_DEV_MODE
							  ,
							  FlowUi::devSystems::DevErrorMonitoring* devErrors
#endif
							  ) noexcept;

	~AppErrorObserver();

	AppErrorObserver(const AppErrorObserver&) = delete;
	AppErrorObserver& operator=(const AppErrorObserver&) = delete;

	void report(const FlowUi::ErrorEventView& event) const noexcept;

#if FLOW_UI_DEV_MODE
	void observeForDev(const FlowUi::ErrorEventView& event,
					   const FlowUi::devSystems::DevErrorSourceDescriptor& source) const noexcept;
#endif

	static const AppErrorObserver* active() noexcept;

private:
	FlowUi::ErrorObserverConfig config_{};
#if FLOW_UI_DEV_MODE
	FlowUi::devSystems::DevErrorMonitoring* devErrors_ = nullptr;
#endif
	static std::atomic<const AppErrorObserver*> active_;
};

} // namespace FlowUi::detail::app_runtime

namespace FlowUi {

namespace storage = detail::storage;

inline constexpr uint32_t kUiTextureDescriptorCapacity = 256u;

struct AppWindowConfig {
	WindowConfig native{};
	VulkanConfig vulkan{};
	UiConfig ui{};
	uint32_t uiTextureDescriptorCapacity = kUiTextureDescriptorCapacity;
};

/* Compose native-window overrides without changing the owning application defaults. */
WindowConfig mergeWindowConfig(const WindowConfig& base, const WindowConfigOverrides& overrides);

#if FLOW_UI_DEV_MODE
struct DevUiReplayRequest {
	std::vector<uint32_t> commandIds{};
	Clay_BoundingBox sourceRootBounds{};
	uint64_t key = 0u;
	uint32_t rootCommandId = 0u;
};

struct DevUiReplayPacket {
	std::vector<Clay_RenderCommand> commands{};
	std::vector<std::string> textByCommand{};
	std::vector<TextureRef> textureByCommand{};
	std::vector<storage::BindingHotRecord> textureBindings{};
	Clay_RenderCommandArray commandArray{};
	Clay_BoundingBox sourceRootBounds{};
	uint64_t requestKey = 0u;
	uint64_t captureSerial = 0u;
	uint32_t textureFrameSlot = 0u;

	void clearCommands() noexcept {
		commands.clear();
		textByCommand.clear();
		textureByCommand.clear();
		textureBindings.clear();
		commandArray = {};
	}

	void rebindPayloads() noexcept {
		for (size_t commandIndex = 0u; commandIndex < commands.size(); ++commandIndex) {
			Clay_RenderCommand& command = commands[commandIndex];
			command.userData = nullptr;
			if (command.commandType == CLAY_RENDER_COMMAND_TYPE_TEXT) {
				const std::string& text = textByCommand[commandIndex];
				command.renderData.text.stringContents = Clay_StringSlice{
					.length = static_cast<int32_t>(text.size()),
					.chars = text.data(),
					.baseChars = text.data(),
				};
			} else if (command.commandType == CLAY_RENDER_COMMAND_TYPE_IMAGE) {
				command.renderData.image.imageData = &textureByCommand[commandIndex];
			}
		}
		commandArray = Clay_RenderCommandArray{
			.capacity = static_cast<int32_t>(commands.size()),
			.length = static_cast<int32_t>(commands.size()),
			.internalArray = commands.data(),
		};
	}
};
#endif

struct AppWindow {
#if FLOW_UI_AGENTIC_WORKFLOW
	devSystems::agentic::AgenticFrameRecord agentic_frame{};
#endif
#if FLOW_UI_DEV_MODE
	bool dev_suspended = false;
#endif
#if FLOW_UI_DEV_MODE && FLOWUI_PUBLIC_VULKAN_INTEROP
	std::unique_ptr<devSystems::interface_elements::DevTimelineController> timeline_controller;
	std::unique_ptr<devSystems::interface_elements::DevMemoryHistoryController>
		memory_history_controller;
#endif

#if FLOW_UI_DEV_MODE
#endif

	enum class Phase : uint8_t { Initializing, Idle, Building, Prepared, Closing };

	VulkanUiRenderer renderer;
	detail::input::InputState inputState;
	detail::platform::ActivePlatformProviderService::WindowState platformState;
	detail::platform::WindowPlatformServices platformServices;

	FrameInput frameInput{};
#if FLOW_UI_DEV_MODE
	devSystems::tooling::DevInspectPointerState inspect_pointer{};
#endif

	UiManager ui;
	AppWindowConfig config{};
	SwapchainGeneration swapchain;
#if FLOW_UI_DEV_MODE
	DevUiReplayPacket devReplayPacket{};
#endif
	detail::manager_storage::FontFrameView fontFrameView{};
	storage::FrameReadLease storageReadLease{};
#if FLOW_UI_DEV_MODE
	devSystems::tooling::DevOverlayCommandBuffer devOverlay{};
	DevUiReplayRequest devReplayRequest{};
#endif
	storage::FrameToken storageFrame{};
	ViewPortManager viewPorts;
	PreparedUiFrame preparedUi{};
	FrameVk frames;
	std::vector<SwapchainGeneration> retiredSwapchains;
#if FLOW_UI_DEV_MODE
	devSystems::ManualTimingZone frameTotalTiming{};
	devSystems::ManualTimingZone userBuildTiming{};
	devSystems::ManualTimingZone preparedGapTiming{};
#endif
	Clay_RenderCommandArray renderCommands{};
#if FLOW_UI_DEV_MODE
	devSystems::WindowFrameKey timingFrame{};
#endif
	AppWindow(WindowId windowId, AppWindowConfig windowConfig, const AppConfig& appConfig)
		: id(windowId), config(std::move(windowConfig)),
		  inputState(appConfig.errors.policy.inputTextQueueCapacity,
					 appConfig.errors.policy.inputQueueOverflow) {}
	AppWindow(const AppWindow&) = delete;
	AppWindow& operator=(const AppWindow&) = delete;
	AppWindow(AppWindow&&) = delete;
	AppWindow& operator=(AppWindow&&) = delete;

	WindowId id = InvalidWindowId;
	storage::IStorageSystem* storageSystem = nullptr;

	VkSurfaceKHR surface = VK_NULL_HANDLE;
	uint64_t frameNumber = 0;
	storage::SubmissionSerial lastSubmissionSerial = 0;
#if FLOW_UI_DEV_MODE
	devSystems::AppTickId timingAppTick = 0u;
#endif

	std::chrono::steady_clock::time_point previousBeginFrameTimestamp{};
	VkExtent2D observedFramebufferExtent{};
	float uiToFramebufferScaleX = 1.0f;
	float uiToFramebufferScaleY = 1.0f;
	Phase phase = Phase::Initializing;
	enum class Flag : std::uint8_t {
		storage_registered = 1u << 0u,
		has_previous_begin_frame_timestamp = 1u << 1u,
		framebuffer_resized = 1u << 2u,
	};
	detail::EnumFlags<Flag> flags{};
};

namespace detail::app_runtime {

/** Convert recoverable service errors at the existing throwing App boundary. */
inline void requireSuccess(Status status) {
	if (!status)
		throw FlowUiException(status.error());
}

/** Move a service result through the existing throwing App boundary. */
template <typename Value>
[[nodiscard]] Value requireValue(Result<Value> result) {
	if (!result)
		throw FlowUiException(result.error());
	return std::move(*result);
}

template <typename Operation>
Status runLocalOperation(Operation&& operation) {
	try {
		std::forward<Operation>(operation)();
		return {};
	} catch (const FlowUiException& exception) {
		if (exception.error().descriptor().category == ErrorCategory::Fatal) {
			detail::terminateForFatalError(exception.error());
		}
		if (exception.error().descriptor().category != ErrorCategory::Local)
			throw;
		return unexpectedError(exception.error());
	}
}

} // namespace detail::app_runtime

struct App::Impl {
#if FLOW_UI_DEV_MODE
	devSystems::DevMonitoringAndReporting devMonitoring;
	devSystems::DevTooling devTooling;
	devSystems::DevInterface devInterface{};
#if FLOW_UI_AGENTIC_WORKFLOW
	std::unique_ptr<devSystems::agentic::AgenticWorkflowService> agentic_service;
#endif
	devSystems::DevErrorThreadAttachment platformErrorAttachment;
#endif
	detail::app_runtime::AppErrorObserver errorObserver;
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

	detail::input::InputService inputService;
	detail::platform::ActivePlatformProviderService platformService;
	std::chrono::steady_clock::time_point previousInputTickTimestamp{};
	std::unordered_map<WindowId, std::unique_ptr<AppWindow>> windows;
	struct ManagedWindowEntry {
		WindowId id = InvalidWindowId;
		UiBuildCallback buildUi;
		ManagedWindowFlags flags{};
	};
	std::vector<ManagedWindowEntry> managedWindows;
	std::vector<WindowId> dispatch_order;
	enum class NativeWindowOperation : uint8_t { Close, Minimize, MaximizeRestore, Fullscreen };
	struct PendingWindowOperation {
		WindowId window_id = InvalidWindowId;
		NativeWindowOperation operation = NativeWindowOperation::Close;
	};
	std::vector<PendingWindowOperation> pending_window_operations;
	enum class DispatchFlag : std::uint8_t {
		dispatching_managed_windows = 1u << 0u,
#if FLOW_UI_AGENTIC_WORKFLOW
		advancing_agentic = 1u << 1u,
#endif
	};
	detail::EnumFlags<DispatchFlag> dispatch_flags{};
	::FlowUi::detail::MemoryDemandPeaks capacity_demand{};
	WindowId mainWindowId = MainWindowId;
	WindowId nextWindowId = MainWindowId + 1;
	WindowId activeWindowFrame = InvalidWindowId;
	enum class LifecycleFlag : std::uint8_t {
		window_ids_exhausted = 1u << 0u,
		fonts_initialized = 1u << 1u,
		images_initialized = 1u << 2u,
		icons_initialized = 1u << 3u,
		cleaned_up = 1u << 4u,
	};
	detail::EnumFlags<LifecycleFlag> lifecycle_flags{};
	std::thread::id platformThread = std::this_thread::get_id();

	explicit Impl(const AppConfig& initialConfig);

	~Impl();

#if FLOW_UI_DEV_MODE
	devSystems::DevTimingRecorder& timingRecorder() noexcept;

	devSystems::DevErrorRecorder& errorRecorder() noexcept;

#if FLOWUI_DEV_MEMORY_LEVEL >= 2
	static void sample_app_memory(const void* owner,
								  devSystems::MemoryProbeContext& context) noexcept;
	static void sampleElementMemory(const void* owner,
									devSystems::MemoryProbeContext& context) noexcept;
	static void sampleFontMemory(const void* owner,
								 devSystems::MemoryProbeContext& context) noexcept;
#if FLOWUI_INCLUDE_ICON_MANAGER
	static void sampleIconMemory(const void* owner,
								 devSystems::MemoryProbeContext& context) noexcept;
#endif
	static void sampleActionMemory(const void* owner,
								   devSystems::MemoryProbeContext& context) noexcept;
	static void sampleThemeMemory(const void* owner,
								  devSystems::MemoryProbeContext& context) noexcept;
	static void sampleDevToolingMemory(const void* owner,
									   devSystems::MemoryProbeContext& context) noexcept;
	static void sampleWindowMemory(const void* owner,
								   devSystems::MemoryProbeContext& context) noexcept;
#if FLOW_UI_AGENTIC_WORKFLOW
	static void sample_agentic_memory(const void* owner,
									  devSystems::MemoryProbeContext& context) noexcept;
#endif
	void registerAppMemoryProbes();
	void registerWindowMemoryProbe(AppWindow& window);
	void unregisterWindowMemoryProbe(AppWindow& window) noexcept;
	void unregisterAppMemoryProbes() noexcept;
#endif
#endif

	AppWindow& requireWindow(WindowId id);

	const AppWindow& requireWindow(WindowId id) const;

	AppWindow& mainWindow();
	const AppWindow& mainWindow() const;

	void initializeDefaultFont();

	void init(App& owner);

	void requireQuiescent() const;

	void requirePlatformThread() const;

	void initializeWindowPlatform(AppWindow& window);
	void attachPlatformServices(AppWindow& window);
	void quiesceWindowPlatform(AppWindow& window) noexcept;

	[[nodiscard]] Result<WindowOperationCapabilities>
	window_operation_capabilities(WindowId window_id) const noexcept;
	[[nodiscard]] Status request_window_operation(WindowId window_id,
												  NativeWindowOperation operation) noexcept;
	[[nodiscard]] Status execute_window_operation(WindowId window_id,
												  NativeWindowOperation operation) noexcept;
	[[nodiscard]] Status install_default_window_shortcuts(AppWindow& window);
	void drain_window_operations() noexcept;

	void destroyUnsubmittedWindow(AppWindow& window) noexcept;

	WindowId reserveWindowId();

	WindowId createWindow(const WindowConfig& nativeConfig);

	void pollEventsAndAdvanceSharedManagers();

	void pollEvents();

	void cancelStorageFrame(
		AppWindow& window
#if FLOW_UI_DEV_MODE
		,
		devSystems::TimingRecordFlag timingResult = devSystems::TimingRecordFlag::Canceled
#endif
		) noexcept;

	void completeSubmission(FrameVk::Frame& frame);

	void completeAllSubmissionsAfterIdle();

	void drainWindowGraphics(AppWindow& window);

	void collectRetiredSwapchains(AppWindow& window);

	void beginFrame(WindowId id);

	void endFrame(WindowId id);

	void drawFrame(WindowId id);

	bool recreateSwapchainIfNeeded(AppWindow& window);

	void destroyWindow(WindowId id);

	void cleanup() noexcept;
};

} // namespace FlowUi
