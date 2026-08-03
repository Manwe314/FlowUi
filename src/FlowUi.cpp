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
#include "internal/UiTexturePublisher.hpp"
#include "internal/InputQueue.hpp"
#include "internal/StorageSystem/FlowStorageSystem.hpp"
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
#include <memory>
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

namespace storage = detail::storage;

inline constexpr uint32_t kUiTextureDescriptorCapacity = 256u;

//Transitional: manager-native texture publication is removed when manager GPU
// stores migrate to storage-owned images, views, samplers, and uploads.
class UiTexturePublisher final : public detail::IUiTexturePublisher {
public:
	void init(
		storage::IStorageSystem& storageSystem,
		storage::ImageViewHandle fallbackView,
		storage::SamplerHandle fallbackSampler) {
		storage_ = &storageSystem;
		fallbackNativeImageView_ = storageSystem.nativeImageView(fallbackView).nativeImageView;
		fallbackNativeSampler_ = storageSystem.nativeSampler(fallbackSampler).nativeSampler;
	}

	TextureHandle publishExternal(
		storage::ResourceDomain domain,
		std::string_view namespacedKey,
		const storage::ExternalTextureDesc& desc,
		bool& inserted) override {
		return storage().publishExternalTexture(key(domain, namespacedKey, desc.window), desc, &inserted);
	}

	TextureHandle publishFallbackAlias(
		storage::ResourceDomain domain,
		std::string_view namespacedKey,
		bool& inserted) override {
		return storage().publishExternalTexture(
			key(domain, namespacedKey, InvalidWindowId),
			storage::ExternalTextureDesc{
				.nativeImageView = fallbackNativeImageView_,
				.nativeSampler = fallbackNativeSampler_,
			},
			&inserted);
	}

	bool remove(
		storage::ResourceDomain domain,
		std::string_view namespacedKey,
		WindowId window) override {
		return storage().removeTexture(key(domain, namespacedKey, window));
	}

	bool retired(TextureHandle handle) const noexcept override {
		return !storage_ || storage_->textureRetirementComplete(handle);
	}

private:
	storage::IStorageSystem& storage() const {
		if (!storage_) throw std::runtime_error("UI texture publisher is not initialized.");
		return *storage_;
	}

	storage::ResourceKey key(
		storage::ResourceDomain domain,
		std::string_view namespacedKey,
		WindowId window) const {
		return storage::ResourceKey{
			.domain = domain,
			.name = storage().intern(namespacedKey),
			.window = window,
		};
	}

	storage::IStorageSystem* storage_ = nullptr;
	uint64_t fallbackNativeImageView_ = 0;
	uint64_t fallbackNativeSampler_ = 0;
};

struct AppWindowConfig {
	WindowConfig native{};
	VulkanConfig vulkan{};
	UiConfig ui{};
	uint32_t uiTextureDescriptorCapacity = kUiTextureDescriptorCapacity;
};

AppWindowConfig makeMainWindowConfig(const AppConfig& config) {
	AppWindowConfig result{
		.native = config.window,
		.vulkan = config.vk,
		.ui = config.ui,
	};
	result.vulkan.framesInFlight = std::max(1u, result.vulkan.framesInFlight);
	return result;
}

storage::StorageConfig makeStorageConfig(const AppConfig& config) {
	storage::StorageConfig result{};
	result.framesInFlight = std::max(1u, config.vk.framesInFlight);
	result.expectedWindows = std::max(2u, result.expectedWindows);
	result.initialInstanceBytesPerFrame = 1024ull * 1024ull;
	result.expectedBindingsPerWindow = kUiTextureDescriptorCapacity;
	return result;
}

struct AppWindow {
	AppWindow(WindowId windowId, AppWindowConfig windowConfig, const AppConfig& appConfig)
		: id(windowId), config(std::move(windowConfig)), ui(appConfig) {}

	WindowId id = InvalidWindowId;
	AppWindowConfig config{};
	storage::IStorageSystem* storageSystem = nullptr;
	bool storageRegistered = false;

	std::unique_ptr<detail::IWindowBackend> backend;
	detail::InputQueue inputQueue;
	VkSurfaceKHR surface = VK_NULL_HANDLE;
	Swapchain swapchain;
	FrameVk frames;

	UiManager ui;
	ViewPortManager viewPorts;

	VulkanUiRenderer renderer;

	FrameInput frameInput{};
	Clay_RenderCommandArray renderCommands{};
	PreparedUiFrame preparedUi{};
	std::vector<VkImageLayout> swapchainImageLayouts;
	storage::FrameToken storageFrame{};
	storage::FrameReadLease storageReadLease{};
	uint64_t frameNumber = 0;

	std::chrono::steady_clock::time_point previousBeginFrameTimestamp{};
	bool hasPreviousBeginFrameTimestamp = false;
	float uiToFramebufferScaleX = 1.0f;
	float uiToFramebufferScaleY = 1.0f;
	bool framebufferResized = false;
	VkExtent2D observedFramebufferExtent{};
};

class StorageFrameCancellationGuard {
public:
	StorageFrameCancellationGuard(storage::IStorageSystem& storageSystem, AppWindow& window)
		: storage_(&storageSystem), window_(&window) {}

	~StorageFrameCancellationGuard() {
		if (!armed_ || !window_->storageFrame) return;
		storage_->cancelFrame(window_->storageFrame);
		window_->preparedUi = {};
		window_->storageReadLease = {};
		window_->storageFrame = {};
	}

	void dismiss() noexcept { armed_ = false; }

private:
	storage::IStorageSystem* storage_ = nullptr;
	AppWindow* window_ = nullptr;
	bool armed_ = true;
};

struct App::Impl {
	AppConfig config{};
	VulkanContext vk;
	std::unique_ptr<storage::IStorageSystem> storageSystem;
	SharedUiByteResources sharedUiByteResources;
	UiTexturePublisher texturePublisher;

	FontManager fonts;
	ImageManager imageManager;
#if FLOWUI_INCLUDE_ICON_MANAGER
	IconManager icons;
#endif

	std::unordered_map<WindowId, std::unique_ptr<AppWindow>> windows;
	WindowId mainWindowId = MainWindowId;
	WindowId nextWindowId = MainWindowId + 1;
	bool fontsInitialized = false;
	bool imagesInitialized = false;
	bool iconsInitialized = false;
	bool cleanedUp = false;

	explicit Impl(const AppConfig& initialConfig) : config(initialConfig) {}
	~Impl() { cleanup(); }

	AppWindow& requireWindow(WindowId id) {
		const auto found = windows.find(id);
		if (id == InvalidWindowId || found == windows.end() || !found->second) {
			throw std::runtime_error("FlowUi window id is invalid or no longer registered.");
		}
		return *found->second;
	}

	const AppWindow& requireWindow(WindowId id) const {
		const auto found = windows.find(id);
		if (id == InvalidWindowId || found == windows.end() || !found->second) {
			throw std::runtime_error("FlowUi window id is invalid or no longer registered.");
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

	void init() {
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
		vk.createDevice(config);

		storageSystem = std::make_unique<storage::FlowStorageSystem>(vk);
		const storage::StorageConfig storageConfig = makeStorageConfig(config);
		storageSystem->initialize(storageConfig);
		mainPointer->storageSystem = storageSystem.get();
		storage::WindowStorageDesc windowStorage{};
		windowStorage.framesInFlight = mainPointer->config.vulkan.framesInFlight;
		windowStorage.workerCount = storageConfig.expectedWorkerCount;
		windowStorage.initialTextureBindings = mainPointer->config.uiTextureDescriptorCapacity;
		windowStorage.maxTextureBindings = mainPointer->config.uiTextureDescriptorCapacity;
		windowStorage.transientBytesPerFrame = storageConfig.transientBytesPerFramePerWindow;
		windowStorage.transientBytesPerWorker = storageConfig.transientBytesPerWorker;
		windowStorage.debugName = storageSystem->intern(mainPointer->config.native.title);
		storageSystem->registerWindow(mainPointer->id, windowStorage);
		mainPointer->storageRegistered = true;
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
		texturePublisher.init(
			*storageSystem,
			sharedUiByteResources.placeholderUiView,
			sharedUiByteResources.linearSampler);

		mainPointer->swapchain.create(
			mainPointer->config.native,
			mainPointer->config.vulkan,
			vk,
			mainPointer->surface,
			mainPointer->backend->framebufferExtent());
		mainPointer->frames.create(
			mainPointer->config.vulkan.framesInFlight, vk, mainPointer->swapchain.images.size());
		mainPointer->swapchainImageLayouts.assign(
			mainPointer->swapchain.images.size(), VK_IMAGE_LAYOUT_UNDEFINED);
		mainPointer->observedFramebufferExtent = mainPointer->backend->framebufferExtent();

		mainPointer->renderer.init(
			config,
			vk,
			mainPointer->swapchain.format,
			*storageSystem,
			mainPointer->id,
			sharedUiByteResources,
			storageConfig.initialInstanceBytesPerFrame,
			mainPointer->config.uiTextureDescriptorCapacity);
		//Transitional: managers publish borrowed native resources until their VMA
		// stores migrate to storage ownership.
		imageManager.setTexturePublisher(&texturePublisher);
		imagesInitialized = true;
		imageManager.init(vk, mainPointer->config.vulkan.framesInFlight);
		mainPointer->viewPorts.setTexturePublisher(&texturePublisher, mainPointer->id);
		mainPointer->viewPorts.init(vk, mainPointer->config.vulkan.framesInFlight);
		fontsInitialized = true;
		fonts.init(vk, config.ui.fontAtlasSize);
		mainPointer->ui.setFontManager(&fonts);
		mainPointer->renderer.setFontManager(&fonts);
#if FLOWUI_INCLUDE_ICON_MANAGER
		icons.setTexturePublisher(&texturePublisher);
		iconsInitialized = true;
		icons.init(vk, config.iconManager);
#endif
		initializeDefaultFont();
	}

	void pollEvents() {
		detail::pollWindowSystemEvents();
	}

	void cancelStorageFrame(AppWindow& window) noexcept {
		if (storageSystem && window.storageFrame) storageSystem->cancelFrame(window.storageFrame);
		window.preparedUi = {};
		window.storageReadLease = {};
		window.storageFrame = {};
	}

	void completeSubmission(FrameVk::Frame& frame) {
		if (!storageSystem || !frame.storageSubmission) return;
		storageSystem->noteCompleted(frame.storageSubmission);
		frame.storageSubmission = {};
	}

	void completeAllSubmissionsAfterIdle() {
		if (!storageSystem) return;
		for (auto& [_, window] : windows) {
			for (FrameVk::Frame& frame : window->frames.frames) completeSubmission(frame);
		}
		storageSystem->collect();
	}

	void beginFrame(WindowId id) {
		AppWindow& window = requireWindow(id);
		if (!window.backend || window.frames.frames.empty()) {
			throw std::runtime_error("FlowUi window is not ready to begin a frame.");
		}

		window.backend->refreshInputSnapshot();
		const auto now = std::chrono::steady_clock::now();
		const auto beginFrameStart = now;
		double deltaTimeSeconds = 0.0;
		if (window.hasPreviousBeginFrameTimestamp) {
			deltaTimeSeconds = std::max(
				0.0, std::chrono::duration<double>(now - window.previousBeginFrameTimestamp).count());
		}
		window.previousBeginFrameTimestamp = now;
		window.hasPreviousBeginFrameTimestamp = true;

#if FLOW_UI_DEV_MODE
		window.ui.performanceDiagnostics().beginFrame(window.frames.currentFrame, deltaTimeSeconds);
		const auto fenceWaitStart = devMode::PerformanceDiagnostics::Clock::now();
#endif
		FrameVk::Frame& frame = window.frames.getCurrentFrame();
		vkCheck(vkWaitForFences(vk.device, 1, &frame.inFlight, VK_TRUE, UINT64_MAX),
			"Failed to wait for in-flight fence.");
#if FLOW_UI_DEV_MODE
		window.ui.performanceDiagnostics().current().fenceWaitMs =
			devMode::PerformanceDiagnostics::elapsedMs(fenceWaitStart);
#endif
		completeSubmission(frame);
		storageSystem->collect();

		const uint32_t frameSlot = window.frames.currentFrame;
		//Transitional: shared managers still advance on the main window frame cadence.
		imageManager.onFrameStart(vk, frameSlot);
		window.viewPorts.onFrameStart(vk, frameSlot);

		if (window.frameNumber == std::numeric_limits<uint64_t>::max()) {
			throw std::runtime_error("FlowUi window frame number space exhausted.");
		}
		const uint64_t nextFrameNumber = window.frameNumber + 1u;
		window.storageFrame = storageSystem->beginFrame(window.id, {
			.frameSlot = frameSlot,
			.frameNumber = nextFrameNumber,
		});
		window.frameNumber = nextFrameNumber;

		try {
			window.frameInput = window.inputQueue.drain(deltaTimeSeconds);
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
			window.ui.beginFrame(frameSlot, layoutInput, layoutWidth, layoutHeight);
#if FLOW_UI_DEV_MODE
			window.ui.performanceDiagnostics().current().beginFrameMs =
				devMode::PerformanceDiagnostics::elapsedMs(beginFrameStart);
#endif
		} catch (...) {
			cancelStorageFrame(window);
			throw;
		}
	}

	void endFrame(WindowId id) {
		AppWindow& window = requireWindow(id);
		if (!window.storageFrame) throw std::runtime_error("FlowUi window has no active storage frame.");
		try {
#if FLOW_UI_DEV_MODE
			const auto endFrameStart = devMode::PerformanceDiagnostics::Clock::now();
			const auto clayStart = endFrameStart;
#endif
			window.renderCommands = window.ui.endFrame();
#if FLOW_UI_DEV_MODE
			window.ui.performanceDiagnostics().current().clayLayoutMs =
				devMode::PerformanceDiagnostics::elapsedMs(clayStart);
			window.ui.performanceDiagnostics().current().clayCommandCount = window.renderCommands.length;
			const auto prepStart = devMode::PerformanceDiagnostics::Clock::now();
#endif
			window.viewPorts.prepareFrameTargets(
				window.renderCommands, window.uiToFramebufferScaleX, window.uiToFramebufferScaleY);
#if FLOWUI_INCLUDE_ICON_MANAGER
			//Transitional: icon texture preparation still uses the main window's legacy binding registry.
			icons.prepareFrameTextures(
				window.renderCommands, window.uiToFramebufferScaleX, window.uiToFramebufferScaleY);
#endif
			//Transitional: viewport and manager textures still carry renderer-local
			// slots. Phase 3 resolves logical TextureHandles per AppWindow instead.
			window.viewPorts.remapRenderCommandsForFrame(window.renderCommands, window.frames.currentFrame);
			storage::ArenaView textureArena = storageSystem->frameArena(
				window.storageFrame, storage::MemoryClass::FrameTransient);
			std::span<storage::TextureHandle> gathered = textureArena.allocateArray<storage::TextureHandle>(
				static_cast<size_t>(std::max(0, window.renderCommands.length)));
			size_t gatheredCount = 0;
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
			const storage::PreparedTextureBindings preparedBindings = storageSystem->prepareTextureBindings(
				window.storageFrame, gathered.first(gatheredCount));
			window.renderer.applyTextureBindings(
				vk.device, window.frames.currentFrame, preparedBindings);
			storageSystem->acknowledgeTextureBindings(
				window.storageFrame, preparedBindings.dirtyBindings);
			window.preparedUi = window.renderer.prepareFrame(
				vk,
				*storageSystem,
				window.storageFrame,
				preparedBindings,
				window.renderCommands,
				window.ui.inputFieldFrameOverrides(),
				window.swapchain.extent,
				window.uiToFramebufferScaleX,
				window.uiToFramebufferScaleY
#if FLOW_UI_DEV_MODE
				, &window.ui.performanceDiagnostics().current()
#endif
				);
#if FLOW_UI_DEV_MODE
			window.ui.performanceDiagnostics().current().resourcePrepMs =
				devMode::PerformanceDiagnostics::elapsedMs(prepStart);
			window.ui.performanceDiagnostics().current().endFrameMs =
				devMode::PerformanceDiagnostics::elapsedMs(endFrameStart);
#endif
			window.storageReadLease = storageSystem->sealFrame(window.storageFrame);
		} catch (...) {
			cancelStorageFrame(window);
			throw;
		}
	}

	void drawFrame(WindowId id) {
		AppWindow& window = requireWindow(id);
		StorageFrameCancellationGuard storageGuard(*storageSystem, window);
		if (window.frames.frames.empty() || window.swapchain.swapchain == VK_NULL_HANDLE ||
			window.swapchain.views.empty()) return;
		if (!window.storageReadLease) {
			throw std::runtime_error("FlowUi window frame must be ended before it is drawn.");
		}
		if (window.preparedUi.epoch == 0 ||
			window.preparedUi.epoch != window.storageReadLease.frame.epoch) {
			throw std::runtime_error("FlowUi prepared UI data is stale for the sealed storage frame.");
		}
#if FLOW_UI_DEV_MODE
		const auto drawFrameStart = devMode::PerformanceDiagnostics::Clock::now();
#endif
		const VkExtent2D framebufferExtent = window.backend ? window.backend->framebufferExtent() : VkExtent2D{};
		if (framebufferExtent.width != window.observedFramebufferExtent.width ||
			framebufferExtent.height != window.observedFramebufferExtent.height) {
			window.framebufferResized = true;
			window.observedFramebufferExtent = framebufferExtent;
		}
		if (window.framebufferResized && !recreateSwapchainIfNeeded(window)) return;

		FrameVk::Frame& frame = window.frames.getCurrentFrame();
		uint32_t swapchainImageIndex = 0;
#if FLOW_UI_DEV_MODE
		const auto acquireStart = devMode::PerformanceDiagnostics::Clock::now();
#endif
		const VkResult acquireResult = vkAcquireNextImageKHR(
			vk.device, window.swapchain.swapchain, UINT64_MAX, frame.imageAvailable,
			VK_NULL_HANDLE, &swapchainImageIndex);
#if FLOW_UI_DEV_MODE
		window.ui.performanceDiagnostics().current().acquireMs =
			devMode::PerformanceDiagnostics::elapsedMs(acquireStart);
#endif
		if (acquireResult == VK_ERROR_OUT_OF_DATE_KHR) {
			window.framebufferResized = true;
			recreateSwapchainIfNeeded(window);
			return;
		}
		if (acquireResult != VK_SUCCESS && acquireResult != VK_SUBOPTIMAL_KHR) {
			throw std::runtime_error("Failed to acquire swapchain image.");
		}

		const bool acquiredSuboptimalSwapchain = acquireResult == VK_SUBOPTIMAL_KHR;
#if FLOW_UI_DEV_MODE
		window.ui.performanceDiagnostics().current().swappedSuboptimal = acquiredSuboptimalSwapchain;
#endif
		if (swapchainImageIndex >= window.swapchain.views.size() ||
			swapchainImageIndex >= window.frames.imageInFlight.size() ||
			swapchainImageIndex >= window.swapchainImageLayouts.size() ||
			swapchainImageIndex >= window.frames.renderFinishedBySwapImage.size()) {
			throw std::runtime_error("Acquired swapchain image index is out of range.");
		}
		if (window.frames.imageInFlight[swapchainImageIndex] != VK_NULL_HANDLE) {
			vkCheck(vkWaitForFences(vk.device, 1, &window.frames.imageInFlight[swapchainImageIndex],
				VK_TRUE, UINT64_MAX), "Failed waiting for previously submitted fence for swapchain image.");
		}
		window.frames.imageInFlight[swapchainImageIndex] = frame.inFlight;

		vkCheck(vkResetCommandPool(vk.device, frame.pool, 0), "Failed to reset command pool.");
		VkCommandBufferBeginInfo beginInfo{};
		beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
		beginInfo.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
		vkCheck(vkBeginCommandBuffer(frame.cmd, &beginInfo), "Failed to begin command buffer.");

		transitionSwapchainImageLayout(frame.cmd, window.swapchain.images[swapchainImageIndex],
			window.swapchainImageLayouts[swapchainImageIndex], VK_IMAGE_LAYOUT_ATTACHMENT_OPTIMAL);
		window.swapchainImageLayouts[swapchainImageIndex] = VK_IMAGE_LAYOUT_ATTACHMENT_OPTIMAL;

#if FLOW_UI_DEV_MODE
		const auto viewportRecordStart = devMode::PerformanceDiagnostics::Clock::now();
		window.viewPorts.recordFramePasses(
			vk, frame.cmd, window.frames.currentFrame, &window.ui.performanceDiagnostics().current());
		window.ui.performanceDiagnostics().current().viewportRecordMs =
			devMode::PerformanceDiagnostics::elapsedMs(viewportRecordStart);
		const auto uiRecordStart = devMode::PerformanceDiagnostics::Clock::now();
#else
		window.viewPorts.recordFramePasses(vk, frame.cmd, window.frames.currentFrame);
#endif
			window.renderer.recordPreparedFrame(
				vk,
				frame.cmd,
				window.swapchain.extent,
				window.swapchain.views[swapchainImageIndex],
				window.frames.currentFrame,
				window.preparedUi
#if FLOW_UI_DEV_MODE
				, &window.ui.performanceDiagnostics().current()
#endif
			);
#if FLOW_UI_DEV_MODE
		window.ui.performanceDiagnostics().current().uiRecordMs =
			devMode::PerformanceDiagnostics::elapsedMs(uiRecordStart);
#endif

		transitionSwapchainImageLayout(frame.cmd, window.swapchain.images[swapchainImageIndex],
			window.swapchainImageLayouts[swapchainImageIndex], VK_IMAGE_LAYOUT_PRESENT_SRC_KHR);
		window.swapchainImageLayouts[swapchainImageIndex] = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;
		vkCheck(vkEndCommandBuffer(frame.cmd), "Failed to end command buffer.");

		VkPipelineStageFlags waitStages[] = {VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT};
		VkSemaphore presentWaitSemaphore = window.frames.renderFinishedBySwapImage[swapchainImageIndex];
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
#if FLOW_UI_DEV_MODE
		const auto submitStart = devMode::PerformanceDiagnostics::Clock::now();
#endif
		vkCheck(vkQueueSubmit(vk.graphicsQ, 1, &submitInfo, frame.inFlight),
			"Failed to submit UI command buffer.");
			frame.storageSubmission = storageSystem->noteSubmission(window.storageReadLease);
			window.preparedUi = {};
		window.storageReadLease = {};
		window.storageFrame = {};
		storageGuard.dismiss();
#if FLOW_UI_DEV_MODE
		window.ui.performanceDiagnostics().current().submitMs =
			devMode::PerformanceDiagnostics::elapsedMs(submitStart);
#endif

		VkPresentInfoKHR presentInfo{};
		presentInfo.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;
		presentInfo.waitSemaphoreCount = 1;
		presentInfo.pWaitSemaphores = &presentWaitSemaphore;
		presentInfo.swapchainCount = 1;
		presentInfo.pSwapchains = &window.swapchain.swapchain;
		presentInfo.pImageIndices = &swapchainImageIndex;
#if FLOW_UI_DEV_MODE
		const auto presentStart = devMode::PerformanceDiagnostics::Clock::now();
#endif
		const VkResult presentResult = vkQueuePresentKHR(vk.presentQ, &presentInfo);
#if FLOW_UI_DEV_MODE
		window.ui.performanceDiagnostics().current().presentMs =
			devMode::PerformanceDiagnostics::elapsedMs(presentStart);
#endif
		if (presentResult == VK_ERROR_OUT_OF_DATE_KHR || presentResult == VK_SUBOPTIMAL_KHR ||
			acquiredSuboptimalSwapchain || window.framebufferResized) {
			window.framebufferResized = true;
			recreateSwapchainIfNeeded(window);
		} else if (presentResult != VK_SUCCESS) {
			throw std::runtime_error("Failed to present swapchain image.");
		}

		window.frames.advance();
#if FLOW_UI_DEV_MODE
		window.ui.performanceDiagnostics().current().drawFrameMs =
			devMode::PerformanceDiagnostics::elapsedMs(drawFrameStart);
		window.ui.performanceDiagnostics().endCompletedFrame();
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

		//Transitional: Phase 4 replaces this app-wide idle with per-window serial retirement.
		vkCheck(vkDeviceWaitIdle(vk.device), "Failed to wait for device idle during swapchain recreation.");
		completeAllSubmissionsAfterIdle();
		window.swapchain.recreate(
			window.config.native, window.config.vulkan, vk, window.surface, framebufferExtent);
		window.frames.onSwapchainRecreated(vk, window.swapchain.images.size());
		window.renderer.onSwapchainFormatChanged(vk, window.swapchain.format);
		window.swapchainImageLayouts.assign(window.swapchain.images.size(), VK_IMAGE_LAYOUT_UNDEFINED);
		window.framebufferResized = false;
#if FLOW_UI_DEV_MODE
		window.ui.performanceDiagnostics().current().swapchainRecreated = true;
#endif
		return true;
	}

	void cleanup() noexcept {
		if (cleanedUp) return;
		cleanedUp = true;
		for (auto& [_, window] : windows) cancelStorageFrame(*window);
		if (vk.device != VK_NULL_HANDLE) {
			(void)vkDeviceWaitIdle(vk.device);
			try { completeAllSubmissionsAfterIdle(); } catch (...) {}
		}

		if (imagesInitialized) imageManager.destroy(vk);
#if FLOWUI_INCLUDE_ICON_MANAGER
		if (iconsInitialized) icons.destroy(vk);
#endif
		if (fontsInitialized) fonts.destroy(vk);

		for (auto& [_, window] : windows) {
			window->viewPorts.destroy(vk);
			if (storageSystem) {
				try { window->renderer.destroy(vk, *storageSystem); } catch (...) {}
			}
			window->frames.destroy(vk);
			window->swapchain.destroy(vk);
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
			storageSystem->shutdown();
			storageSystem.reset();
		}
		windows.clear();
		vk.destroy();
	}
};

App::App() = default;

App::App(App&&) noexcept = default;

App& App::operator=(App&&) noexcept = default;

App::~App() = default;

WindowId App::mainWindowId() const noexcept {
	return impl_ ? impl_->mainWindowId : MainWindowId;
}

bool App::shouldClose() const {
	if (!impl_) return true;
	const AppWindow& window = impl_->mainWindow();
	return !window.backend || window.backend->shouldClose();
}

void App::setShouldClose(int value) {
	if (!impl_) return;
	AppWindow& window = impl_->mainWindow();
	if (window.backend) window.backend->setShouldClose(value);
}

void App::beginFrame() {
	if (impl_) {
		impl_->pollEvents();
		impl_->beginFrame(impl_->mainWindowId);
	}
}

void App::endFrame() {
	if (impl_) {
		impl_->endFrame(impl_->mainWindowId);
	}
}

void App::drawFrame() {
	if (impl_) {
		impl_->drawFrame(impl_->mainWindowId);
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
	return impl_->mainWindow().viewPorts;
}

const ViewPortManager& App::viewPorts() const {
	if (!impl_) {
		throw std::runtime_error("FlowUi::App not initialized.");
	}
	return impl_->mainWindow().viewPorts;
}
#endif

UiManager& App::ui() {
	if (!impl_) {
		throw std::runtime_error("FlowUi::App not initialized.");
	}
	return impl_->mainWindow().ui;
}

const UiManager& App::ui() const {
	if (!impl_) {
		throw std::runtime_error("FlowUi::App not initialized.");
	}
	return impl_->mainWindow().ui;
}

void App::setWindowTitle(std::string_view title) {
	if (!impl_) return;
	AppWindow& window = impl_->mainWindow();
	window.config.native.title.assign(title);
	if (window.backend) window.backend->setTitle(title);
}

void* App::nativeWindowHandle() const {
	if (!impl_) return nullptr;
	const AppWindow& window = impl_->mainWindow();
	return window.backend ? window.backend->nativeHandle() : nullptr;
}

void App::setWindowInputConfig(const WindowInputConfig& config) {
	if (!impl_) return;
	AppWindow& window = impl_->mainWindow();
	window.config.native.input = config;
	if (window.backend) window.backend->setInputConfig(config);
}

WindowInputConfig App::windowInputConfig() const {
	if (!impl_) return {};
	const AppWindow& window = impl_->mainWindow();
	return window.backend ? window.backend->getInputConfig() : WindowInputConfig{};
}

bool App::supportsRawMouseMotion() const {
	if (!impl_) return false;
	const AppWindow& window = impl_->mainWindow();
	return window.backend && window.backend->supportsRawMouseMotion();
}

void App::setClipboardText(std::string_view text) {
	if (!impl_) return;
	AppWindow& window = impl_->mainWindow();
	if (window.backend) window.backend->setClipboardText(text);
}

std::string App::clipboardText() const {
	if (!impl_) return {};
	const AppWindow& window = impl_->mainWindow();
	return window.backend ? window.backend->getClipboardText() : std::string{};
}

std::pair<int, int> App::windowSize() const {
	if (!impl_) return {0, 0};
	const AppWindow& window = impl_->mainWindow();
	if (!window.backend) return {0, 0};
	const VkExtent2D extent = window.backend->windowExtent();
	return {static_cast<int>(extent.width), static_cast<int>(extent.height)};
}

std::pair<int, int> App::framebufferSize() const {
	if (!impl_) return {0, 0};
	const AppWindow& window = impl_->mainWindow();
	if (!window.backend) return {0, 0};
	const VkExtent2D extent = window.backend->framebufferExtent();
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
