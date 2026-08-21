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
#if FLOW_UI_DEV_MODE
#include "devMode/debugView.hpp"
#include "devSystems/devMonitoringAndReporting/DevMonitoringAndReporting.hpp"
#include "devSystems/devMonitoringAndReporting/memory/DevMemoryProbe.hpp"
#include "devSystems/devMonitoringAndReporting/memory/DevMemorySources.hpp"
#include "devSystems/devMonitoringAndReporting/memory/DevMemory.hpp"
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

void validateSecondarySurface(VulkanContext& vk, VkSurfaceKHR surface) {
	VkSurfaceCapabilitiesKHR capabilities{};
	vkCheck(vkGetPhysicalDeviceSurfaceCapabilitiesKHR(vk.phys, surface, &capabilities),
		"Failed to query secondary-window surface capabilities.");
	uint32_t formatCount = 0;
	vkCheck(vkGetPhysicalDeviceSurfaceFormatsKHR(vk.phys, surface, &formatCount, nullptr),
		"Failed to query secondary-window surface formats.");
	uint32_t presentModeCount = 0;
	vkCheck(vkGetPhysicalDeviceSurfacePresentModesKHR(vk.phys, surface, &presentModeCount, nullptr),
		"Failed to query secondary-window present modes.");
	if (formatCount == 0 || presentModeCount == 0) {
		throw std::runtime_error("Secondary-window surface has no usable swapchain formats or present modes.");
	}
}

} // namespace

namespace FlowUi {

namespace storage = detail::storage;

inline constexpr uint32_t kUiTextureDescriptorCapacity = 256u;

struct AppWindowConfig {
	WindowConfig native{};
	VulkanConfig vulkan{};
	UiConfig ui{};
	uint32_t uiTextureDescriptorCapacity = kUiTextureDescriptorCapacity;
};

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
		  config(std::move(windowConfig)) { (void)appConfig; }

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
		, devSystems::DevTimingRecorder& timingRecorder
#endif
		) noexcept
		: storage_(&storageSystem), window_(&window), activeWindow_(&activeWindow)
#if FLOW_UI_DEV_MODE
		, timingRecorder_(&timingRecorder), uncaughtExceptions_(std::uncaught_exceptions())
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
#endif
	}

private:
	storage::IStorageSystem* storage_;
	AppWindow* window_;
	WindowId* activeWindow_;
#if FLOW_UI_DEV_MODE
	devSystems::DevTimingRecorder* timingRecorder_ = nullptr;
	devSystems::TimingRecordFlag timingResult_ = devSystems::TimingRecordFlag::Canceled;
	int uncaughtExceptions_ = 0;
#endif
};

struct App::Impl {
#if FLOW_UI_DEV_MODE
	devSystems::DevMonitoringAndReporting devMonitoring{};
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
		: platformTimingAttachment(devMonitoring.timing().attachCurrentThread("flowui.platform")),
		  config(initialConfig) {}
#else
		: config(initialConfig) {}
#endif
	~Impl() { cleanup(); }

#if FLOW_UI_DEV_MODE
	devSystems::DevTimingRecorder& timingRecorder() noexcept {
		return platformTimingAttachment.recorder();
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
			throw std::invalid_argument("FlowUi window id is invalid or no longer registered.");
		}
		return *found->second;
	}

	const AppWindow& requireWindow(WindowId id) const {
		const auto found = windows.find(id);
		if (id == InvalidWindowId || found == windows.end() || !found->second) {
			throw std::invalid_argument("FlowUi window id is invalid or no longer registered.");
		}
		return *found->second;
	}

	AppWindow& mainWindow() { return requireWindow(mainWindowId); }
	const AppWindow& mainWindow() const { return requireWindow(mainWindowId); }

	void initializeDefaultFont() {
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
				FontFamilyCreateInfo fallbackFamily{};
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
				if (!std::filesystem::is_regular_file(fallbackPath)) continue;
				try {
					FontFaceCreateInfo fallbackFace{};
					fallbackFace.path = fallbackPath;
					fallbackFace.pixelSize = config.ui.defaultFontFamily.faces.empty()
						? 18.0f
						: config.ui.defaultFontFamily.faces.front().pixelSize;
					fonts.addFamilyFace(fallbackFamilyName, fallbackFace);
					std::fprintf(stderr,
						"[FlowUi] Warning: loaded fallback font because ui.defaultFontFamily was not usable: %s\n",
						fallbackPath.string().c_str());
					defaultFontLoaded = true;
					break;
				} catch (const std::exception& e) {
					std::fprintf(stderr, "[FlowUi] Warning: failed loading fallback font %s (%s)\n",
						fallbackPath.string().c_str(), e.what());
				}
			}
		}

		if (!defaultFontLoaded && fonts.getFontById(0) == nullptr) {
			std::fprintf(stderr, "[FlowUi] Warning: no .arfont font loaded; text rendering is disabled.\n");
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
			throw std::runtime_error("Selected Vulkan present queue does not support the main window surface.");
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
		themeManager.registerTheme<FlowUiTheme>("default", FlowUiTheme::dark(), true);
#if COMPILE_FSELI
		themeManager.registerTheme<FSEL::FSELTheme>("default", FSEL::FSELTheme{}, true);
#endif
		actionManager.init(owner, *storageSystem);
		elementManager.init(owner, *storageSystem);
#if FLOW_UI_DEV_MODE
		elementManager.setDevTimingRecorder(&timingRecorder());
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
#endif
		mainPointer->ui.setThemeManager(&themeManager);
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
			mainPointer->config.uiTextureDescriptorCapacity);
		imagesInitialized = true;
		imageManager.init(*storageSystem);
		mainPointer->viewPorts.init(
			*storageSystem, vk, mainPointer->id, mainPointer->config.vulkan.framesInFlight);
		fontsInitialized = true;
		fonts.init(*storageSystem, config.ui.fontAtlasSize);
#if FLOWUI_INCLUDE_ICON_MANAGER
		iconsInitialized = true;
		icons.init(*storageSystem, config.iconManager);
#if COMPILE_FSELI
		FSEL::standard_icons::registerStandardIcons(icons);
#endif
#endif
#if FLOW_UI_DEV_MODE && FLOWUI_DEV_MEMORY_LEVEL >= 2
		registerAppMemoryProbes();
		registerWindowMemoryProbe(*mainPointer);
#endif
		initializeDefaultFont();
	}

	void requireQuiescent(const char* operation) const {
		if (activeWindowFrame != InvalidWindowId) {
			throw std::logic_error(std::string(operation) +
				" requires the current window frame triplet to finish first.");
		}
	}

	void requirePlatformThread(const char* operation) const {
		if (std::this_thread::get_id() != platformThread) {
			throw std::logic_error(std::string(operation) + " must run on the FlowUi platform thread.");
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
		if (windowIdsExhausted) throw std::overflow_error("FlowUi WindowId space is exhausted.");
		const WindowId id = nextWindowId;
		if (id == InvalidWindowId || id == MainWindowId) {
			throw std::overflow_error("FlowUi WindowId space is exhausted.");
		}
		if (id == std::numeric_limits<WindowId>::max()) {
			windowIdsExhausted = true;
		} else {
			++nextWindowId;
		}
		return id;
	}

	WindowId createWindow(const WindowConfig& nativeConfig) {
		requirePlatformThread("FlowUi::App::createWindow");
		requireQuiescent("FlowUi::App::createWindow");
		if (nativeConfig.width <= 0 || nativeConfig.height <= 0) {
			throw std::invalid_argument("Secondary FlowUi windows require positive width and height.");
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
				throw std::runtime_error(
					"Selected Vulkan present queue family does not support the secondary-window surface.");
			}
			validateSecondarySurface(vk, pending->surface);
			if (!vk.supportsExactPresentCompletion()) {
				throw std::runtime_error(
					"Independent secondary-window retirement requires VK_EXT_swapchain_maintenance1 "
					"present fences or VK_KHR_present_id plus VK_KHR_present_wait.");
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
#endif
			pending->ui.setThemeManager(&themeManager);
			actionManager.attachTo(pending->ui);
			elementManager.attachTo(pending->ui);
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
				pending->config.uiTextureDescriptorCapacity);
			pending->viewPorts.init(*storageSystem, vk, id, pending->config.vulkan.framesInFlight);

			auto [entry, inserted] = windows.try_emplace(id);
			if (!inserted) throw std::runtime_error("FlowUi WindowId registry collision.");
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
		requirePlatformThread("FlowUi::App::pollEvents");
		requireQuiescent("FlowUi::App::pollEvents");
#if FLOW_UI_DEV_MODE
		{
			FLOWUI_DEV_TIMING_ZONE(
				timingRecorder(), devSystems::TimingCategory::DevTool,
				devSystems::TimingZoneRole::DevToolWork, "flowui.dev.reporting.consume");
			devMonitoring.timingReporting().consumeThrough(appTick);
			devMonitoring.memoryReporting().consume(appTick);
		}
		if (appTick == std::numeric_limits<devSystems::AppTickId>::max()) {
			throw std::runtime_error("FlowUi application timing tick space exhausted.");
		}
		++appTick;
#if FLOWUI_DEV_MEMORY_LEVEL >= 1
		devMonitoring.memory().recorder().setAppTickContext(appTick);
		devMonitoring.memory().advanceGpuFrameIndex(static_cast<uint32_t>(appTick));
#endif
		timingRecorder().setFrameContext({}, appTick);
		TimingContextExitGuard timingContextExit(timingRecorder());
		FLOWUI_DEV_TIMING_ZONE(
			timingRecorder(), devSystems::TimingCategory::Lifecycle,
			devSystems::TimingZoneRole::Work, "flowui.app.poll_events");
#endif
		detail::pollWindowSystemEvents();
		themeManager.applyStagedMutations();
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
		window.preparedGapTiming.end(timingResult);
		window.userBuildTiming.end(timingResult);
		window.frameTotalTiming.end(timingResult);
		window.timingFrame = {};
		window.timingAppTick = 0u;
		timingRecorder().clearFrameContext();
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
				"Failed to drain a window frame fence.");
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
		requirePlatformThread("FlowUi::App::beginFrame");
		AppWindow& window = requireWindow(id);
		if (window.phase != AppWindow::Phase::Idle) {
			throw std::logic_error("FlowUi window beginFrame() requires the Idle lifecycle phase.");
		}
		if (activeWindowFrame != InvalidWindowId) {
			throw std::logic_error(
				"FlowUi permits only one active window frame triplet at a time.");
		}
		if (!window.backend || window.frames.frames.empty()) {
			throw std::runtime_error("FlowUi window is not ready to begin a frame.");
		}
		if (window.frameNumber == std::numeric_limits<uint64_t>::max()) {
			throw std::runtime_error("FlowUi window frame number space exhausted.");
		}
		const uint64_t nextFrameNumber = window.frameNumber + 1u;

#if FLOW_UI_DEV_MODE
		window.timingFrame = devSystems::WindowFrameKey{window.id, nextFrameNumber};
		window.timingAppTick = appTick;
		timingRecorder().setFrameContext(window.timingFrame, window.timingAppTick);
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

			FrameVk::Frame& frame = window.frames.getCurrentFrame();
			{
#if FLOW_UI_DEV_MODE
				FLOWUI_DEV_TIMING_ZONE(
					timingRecorder(), devSystems::TimingCategory::Wait,
					devSystems::TimingZoneRole::Wait, "flowui.wait.frame_slot_fence");
#endif
				vkCheck(vkWaitForFences(vk.device, 1, &frame.inFlight, VK_TRUE, UINT64_MAX),
					"Failed to wait for in-flight fence.");
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
		requirePlatformThread("FlowUi::App::endFrame");
		AppWindow& window = requireWindow(id);
		if (activeWindowFrame != id || window.phase != AppWindow::Phase::Building || !window.storageFrame) {
			throw std::logic_error("FlowUi window endFrame() requires its Building lifecycle phase.");
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
				, &timingRecorder()
#endif
				);
			{
#if FLOW_UI_DEV_MODE
				FLOWUI_DEV_TIMING_ZONE_BALANCED(
					timingRecorder(), devSystems::TimingCategory::Prepare,
					devSystems::TimingZoneRole::Work, "flowui.storage.seal_frame");
#endif
				window.storageReadLease = storageSystem->sealFrame(window.storageFrame);
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
		requirePlatformThread("FlowUi::App::drawFrame");
		AppWindow& window = requireWindow(id);
		if (activeWindowFrame != id || window.phase != AppWindow::Phase::Prepared) {
			throw std::logic_error("FlowUi window drawFrame() requires its Prepared lifecycle phase.");
		}
#if FLOW_UI_DEV_MODE
		window.preparedGapTiming.end();
#endif
		WindowFrameExitGuard frameExit(
			*storageSystem, window, activeWindowFrame
#if FLOW_UI_DEV_MODE
			, timingRecorder()
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
			throw std::runtime_error("FlowUi window frame must be ended before it is drawn.");
		}
		if (window.preparedUi.epoch == 0 ||
			window.preparedUi.epoch != window.storageReadLease.frame.epoch) {
			throw std::runtime_error("FlowUi prepared UI data is stale for the sealed storage frame.");
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
			(void)recreateSwapchainIfNeeded(window);
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
			(void)recreateSwapchainIfNeeded(window);
			return;
		}
		if (acquireResult != VK_SUCCESS && acquireResult != VK_SUBOPTIMAL_KHR) {
			throw std::runtime_error("Failed to acquire swapchain image.");
		}

		const bool acquiredSuboptimalSwapchain = acquireResult == VK_SUBOPTIMAL_KHR;
		if (swapchainImageIndex >= window.swapchain.swapchain.views.size() ||
			swapchainImageIndex >= window.swapchain.imageInFlight.size() ||
			swapchainImageIndex >= window.swapchain.layouts.size() ||
			swapchainImageIndex >= window.swapchain.renderFinished.size()) {
			throw std::runtime_error("Acquired swapchain image index is out of range.");
		}
		if (window.swapchain.imageInFlight[swapchainImageIndex] != VK_NULL_HANDLE) {
			FLOWUI_DEV_TIMING_ZONE(
				timingRecorder(), devSystems::TimingCategory::Wait,
				devSystems::TimingZoneRole::Wait, "flowui.wait.swapchain_image_fence");
			vkCheck(vkWaitForFences(vk.device, 1, &window.swapchain.imageInFlight[swapchainImageIndex],
				VK_TRUE, UINT64_MAX), "Failed waiting for previously submitted fence for swapchain image.");
		}
		window.swapchain.imageInFlight[swapchainImageIndex] = frame.inFlight;

		{
#if FLOW_UI_DEV_MODE
			FLOWUI_DEV_TIMING_ZONE_BALANCED(
				timingRecorder(), devSystems::TimingCategory::RendererCpu,
				devSystems::TimingZoneRole::Work, "flowui.renderer.command_buffer_begin");
#endif
			vkCheck(vkResetCommandPool(vk.device, frame.pool, 0), "Failed to reset command pool.");
			VkCommandBufferBeginInfo beginInfo{};
			beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
			beginInfo.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
			vkCheck(vkBeginCommandBuffer(frame.cmd, &beginInfo), "Failed to begin command buffer.");
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
			vkCheck(vkEndCommandBuffer(frame.cmd), "Failed to end command buffer.");
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

		vkCheck(vkResetFences(vk.device, 1, &frame.inFlight), "Failed to reset in-flight fence.");
		{
#if FLOW_UI_DEV_MODE
			FLOWUI_DEV_TIMING_ZONE(
				timingRecorder(), devSystems::TimingCategory::RendererCpu,
				devSystems::TimingZoneRole::Work, "flowui.renderer.queue_submit");
#endif
			vkCheck(vkQueueSubmit(vk.graphicsQ, 1, &submitInfo, frame.inFlight),
				"Failed to submit UI command buffer.");
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
					"Failed waiting to reuse a swapchain present fence.");
				window.swapchain.presentPending[swapchainImageIndex] = 0u;
			}
			vkCheck(vkResetFences(vk.device, 1, &presentFence),
				"Failed to reset a swapchain present fence.");
			presentFenceInfo.sType = VK_STRUCTURE_TYPE_SWAPCHAIN_PRESENT_FENCE_INFO_EXT;
			presentFenceInfo.swapchainCount = 1;
			presentFenceInfo.pFences = &presentFence;
			presentInfo.pNext = &presentFenceInfo;
		} else if (vk.wsiRetirementMode == WsiRetirementMode::PresentWait) {
			if (window.swapchain.lastPresentId == std::numeric_limits<uint64_t>::max()) {
				throw std::runtime_error("FlowUi swapchain present-id space is exhausted.");
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
			recreateSwapchainIfNeeded(window);
		} else if (presentResult != VK_SUCCESS) {
			throw std::runtime_error("Failed to present swapchain image.");
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
				throw std::runtime_error(
					"The legacy device-idle swapchain fallback is restricted to a main-only application.");
			}
			vkCheck(vkDeviceWaitIdle(vk.device),
				"Failed to wait for device idle during main-only compatibility resize.");
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
		requirePlatformThread("FlowUi::App::destroyWindow");
		if (id == mainWindowId) {
			throw std::invalid_argument("The semantic main FlowUi window cannot be destroyed explicitly.");
		}
		AppWindow& window = requireWindow(id);
#if FLOW_UI_DEV_MODE && FLOWUI_DEV_MEMORY_LEVEL >= 2
		unregisterWindowMemoryProbe(window);
#endif
		if (activeWindowFrame != InvalidWindowId && activeWindowFrame != id) {
			throw std::logic_error(
				"Cannot destroy a window while another window frame triplet is active.");
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
		windows.erase(id);
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

WindowId App::createWindow(const WindowConfig& config) {
	if (!impl_) throw std::runtime_error("FlowUi::App not initialized.");
	return impl_->createWindow(config);
}

void App::destroyWindow(WindowId id) {
	if (!impl_) throw std::runtime_error("FlowUi::App not initialized.");
	impl_->destroyWindow(id);
}

bool App::hasWindow(WindowId id) const noexcept {
	return impl_ && id != InvalidWindowId && impl_->windows.contains(id);
}

void App::pollEvents() {
	if (impl_) impl_->pollEventsAndAdvanceSharedManagers();
}

bool App::shouldClose() const {
	if (!impl_) return true;
	return shouldClose(impl_->mainWindowId);
}

bool App::shouldClose(WindowId id) const {
	if (!impl_) throw std::runtime_error("FlowUi::App not initialized.");
	const AppWindow& window = impl_->requireWindow(id);
	return !window.backend || window.backend->shouldClose();
}

void App::setShouldClose(int value) {
	if (!impl_) return;
	setShouldClose(impl_->mainWindowId, value);
}

void App::setShouldClose(WindowId id, int value) {
	if (!impl_) throw std::runtime_error("FlowUi::App not initialized.");
	AppWindow& window = impl_->requireWindow(id);
	if (window.backend) window.backend->setShouldClose(value);
}

void App::beginFrame() {
	if (impl_) {
		impl_->pollEventsAndAdvanceSharedManagers();
		impl_->beginFrame(impl_->mainWindowId);
	}
}

void App::beginFrame(WindowId id) {
	if (!impl_) throw std::runtime_error("FlowUi::App not initialized.");
	impl_->beginFrame(id);
}

void App::endFrame() {
	if (impl_) {
		impl_->endFrame(impl_->mainWindowId);
	}
}

void App::endFrame(WindowId id) {
	if (!impl_) throw std::runtime_error("FlowUi::App not initialized.");
	impl_->endFrame(id);
}

void App::drawFrame() {
	if (impl_) {
		impl_->drawFrame(impl_->mainWindowId);
	}
}

void App::drawFrame(WindowId id) {
	if (!impl_) throw std::runtime_error("FlowUi::App not initialized.");
	impl_->drawFrame(id);
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

ThemeManager& App::themes() {
	if (!impl_) {
		throw std::runtime_error("FlowUi::App not initialized.");
	}
	return impl_->themeManager;
}

const ThemeManager& App::themes() const {
	if (!impl_) {
		throw std::runtime_error("FlowUi::App not initialized.");
	}
	return impl_->themeManager;
}

ElementManager& App::elements() {
	if (!impl_) throw std::runtime_error("FlowUi::App not initialized.");
	return impl_->elementManager;
}

const ElementManager& App::elements() const {
	if (!impl_) throw std::runtime_error("FlowUi::App not initialized.");
	return impl_->elementManager;
}

ActionManager& App::actions() {
	if (!impl_) throw std::runtime_error("FlowUi::App not initialized.");
	return impl_->actionManager;
}

const ActionManager& App::actions() const {
	if (!impl_) throw std::runtime_error("FlowUi::App not initialized.");
	return impl_->actionManager;
}

#if FLOW_UI_DEV_MODE
devSystems::DevMonitoringAndReporting& App::devMonitoring() {
	if (!impl_) throw std::runtime_error("FlowUi::App not initialized.");
	return impl_->devMonitoring;
}

const devSystems::DevMonitoringAndReporting& App::devMonitoring() const {
	if (!impl_) throw std::runtime_error("FlowUi::App not initialized.");
	return impl_->devMonitoring;
}
#endif

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
	return impl_->mainWindow().viewPorts;
}

ViewPortManager& App::viewPorts(WindowId id) {
	if (!impl_) throw std::runtime_error("FlowUi::App not initialized.");
	return impl_->requireWindow(id).viewPorts;
}

const ViewPortManager& App::viewPorts() const {
	if (!impl_) {
		throw std::runtime_error("FlowUi::App not initialized.");
	}
	return impl_->mainWindow().viewPorts;
}

const ViewPortManager& App::viewPorts(WindowId id) const {
	if (!impl_) throw std::runtime_error("FlowUi::App not initialized.");
	return impl_->requireWindow(id).viewPorts;
}
#endif

UiManager& App::ui() {
	if (!impl_) {
		throw std::runtime_error("FlowUi::App not initialized.");
	}
	return impl_->mainWindow().ui;
}

UiManager& App::ui(WindowId id) {
	if (!impl_) throw std::runtime_error("FlowUi::App not initialized.");
	return impl_->requireWindow(id).ui;
}

const UiManager& App::ui() const {
	if (!impl_) {
		throw std::runtime_error("FlowUi::App not initialized.");
	}
	return impl_->mainWindow().ui;
}

const UiManager& App::ui(WindowId id) const {
	if (!impl_) throw std::runtime_error("FlowUi::App not initialized.");
	return impl_->requireWindow(id).ui;
}

void App::setWindowTitle(std::string_view title) {
	if (!impl_) return;
	setWindowTitle(impl_->mainWindowId, title);
}

void App::setWindowTitle(WindowId id, std::string_view title) {
	if (!impl_) throw std::runtime_error("FlowUi::App not initialized.");
	AppWindow& window = impl_->requireWindow(id);
	window.config.native.title.assign(title);
	if (window.backend) window.backend->setTitle(title);
}

void* App::nativeWindowHandle() const {
	if (!impl_) return nullptr;
	return nativeWindowHandle(impl_->mainWindowId);
}

void* App::nativeWindowHandle(WindowId id) const {
	if (!impl_) throw std::runtime_error("FlowUi::App not initialized.");
	const AppWindow& window = impl_->requireWindow(id);
	return window.backend ? window.backend->nativeHandle() : nullptr;
}

void App::setWindowInputConfig(const WindowInputConfig& config) {
	if (!impl_) return;
	setWindowInputConfig(impl_->mainWindowId, config);
}

void App::setWindowInputConfig(WindowId id, const WindowInputConfig& config) {
	if (!impl_) throw std::runtime_error("FlowUi::App not initialized.");
	AppWindow& window = impl_->requireWindow(id);
	window.config.native.input = config;
	if (window.backend) window.backend->setInputConfig(config);
}

WindowInputConfig App::windowInputConfig() const {
	if (!impl_) return {};
	return windowInputConfig(impl_->mainWindowId);
}

WindowInputConfig App::windowInputConfig(WindowId id) const {
	if (!impl_) throw std::runtime_error("FlowUi::App not initialized.");
	const AppWindow& window = impl_->requireWindow(id);
	return window.backend ? window.backend->getInputConfig() : WindowInputConfig{};
}

bool App::supportsRawMouseMotion() const {
	if (!impl_) return false;
	return supportsRawMouseMotion(impl_->mainWindowId);
}

bool App::supportsRawMouseMotion(WindowId id) const {
	if (!impl_) throw std::runtime_error("FlowUi::App not initialized.");
	const AppWindow& window = impl_->requireWindow(id);
	return window.backend && window.backend->supportsRawMouseMotion();
}

void App::setClipboardText(std::string_view text) {
	if (!impl_) return;
	setClipboardText(impl_->mainWindowId, text);
}

void App::setClipboardText(WindowId id, std::string_view text) {
	if (!impl_) throw std::runtime_error("FlowUi::App not initialized.");
	AppWindow& window = impl_->requireWindow(id);
	if (window.backend) window.backend->setClipboardText(text);
}

std::string App::clipboardText() const {
	if (!impl_) return {};
	return clipboardText(impl_->mainWindowId);
}

std::string App::clipboardText(WindowId id) const {
	if (!impl_) throw std::runtime_error("FlowUi::App not initialized.");
	const AppWindow& window = impl_->requireWindow(id);
	return window.backend ? window.backend->getClipboardText() : std::string{};
}

std::pair<int, int> App::windowSize() const {
	if (!impl_) return {0, 0};
	return windowSize(impl_->mainWindowId);
}

std::pair<int, int> App::windowSize(WindowId id) const {
	if (!impl_) throw std::runtime_error("FlowUi::App not initialized.");
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
	if (!impl_) throw std::runtime_error("FlowUi::App not initialized.");
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
#if FLOW_UI_DEV_MODE && !defined(FLOWUI_SKIP_LEGACY_DEV_ELEMENTS)
	devMode::initializeDevFlowElementResourcesFromApp(app);
#endif
	return app;
}

} // namespace FlowUi
