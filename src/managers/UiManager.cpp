#include "managers/UiManager.hpp"
#include <algorithm>
#include <cstdint>
#include <cstdio>

#if FLOW_UI_DEV_MODE
#include <charconv>
#include "devSystems/devMonitoringAndReporting/memory/DevContainerMemory.hpp"
#include "devSystems/devMonitoringAndReporting/memory/DevMemorySources.hpp"
#include "devSystems/devMonitoringAndReporting/timing/DevTimingZone.hpp"
#if !defined(FLOWUI_SKIP_LEGACY_DEV_ELEMENTS)
#include "devMode/debugView.hpp"
#endif
#include "devMode/registry.hpp"
#endif
#include "managers/FontManager.hpp"
#include "managers/ElementManager.hpp"
#include "managers/ActionManager.hpp"
#include "managers/ThemeManager.hpp"
#include "internal/ManagerStorage/ManagerStateAccess.hpp"
#include "internal/ManagerStorage/ResourceKeyNormalization.hpp"
#include "internal/ManagerStorage/UiManagerState.hpp"
#include "internal/Text/TextLayoutService.hpp"

namespace {

constexpr float kPointsPerInch = 72.0f;

#if FLOW_UI_DEV_MODE
FlowUi::ShortcutTrigger toShortcutTrigger(FlowUi::DevShortcutTrigger trigger) {
	switch (trigger) {
	case FlowUi::DevShortcutTrigger::Press:
		return FlowUi::ShortcutTrigger::Press;
	case FlowUi::DevShortcutTrigger::Release:
		return FlowUi::ShortcutTrigger::Release;
	case FlowUi::DevShortcutTrigger::Down:
		return FlowUi::ShortcutTrigger::Down;
	default:
		return FlowUi::ShortcutTrigger::Press;
	}
}
#endif

} // namespace

namespace FlowUi
{

	namespace manager_storage = detail::manager_storage;
	namespace key_storage = detail::managerStorage;
	namespace storage = detail::storage;

#if FLOW_UI_DEV_MODE && FLOWUI_DEV_MEMORY_LEVEL >= 2
	void UiManager::appendDevMemorySamples(devSystems::MemorySampleSink& sink) const noexcept {
		if (!state_) return;
		try {
			devSystems::DevContainerMemoryAccumulator memory{};
			memory.add(state_->constructedElementStack);
			memory.liveBytes += state_->textLayoutService.cacheBytes();
			memory.capacityBytes += state_->textLayoutService.cacheBytes();
			memory.objectCount += state_->textLayoutService.cacheEntryCount();
			memory.capacityCount += detail::text::TextLayoutService::MaxCacheEntries;
			memory.liveBytes += state_->flowRootIdTracker.retainedBytesForDev();
			memory.capacityBytes += state_->flowRootIdTracker.retainedBytesForDev();
			memory.liveBytes += state_->clayBridgeIdTracker.retainedBytesForDev();
			memory.capacityBytes += state_->clayBridgeIdTracker.retainedBytesForDev();
			devSystems::appendManagerSample(
				sink, devSystems::memory_sources::kUiLayout.id, memory, window_);
			inputFieldManager_.appendDevMemorySamples(sink);
			popupManager_.appendDevMemorySamples(sink);
			shortcutManager_.appendDevMemorySamples(sink);
		} catch (...) {}
	}
#endif

	manager_storage::UiManagerState::UiManagerState(
		storage::IStorageSystem& storageSystem,
		WindowId window,
		const AppConfig& appConfig)
		: storage(&storageSystem) {
		const size_t minimumClayArenaCapacityBytes = static_cast<size_t>(Clay_MinMemorySize());
		const size_t configuredClayArenaCapacityBytes = appConfig.ui.clayArenaCapacityBytes;
		const size_t clayArenaCapacityBytes = (configuredClayArenaCapacityBytes == 0)
			? minimumClayArenaCapacityBytes
			: std::max(configuredClayArenaCapacityBytes, minimumClayArenaCapacityBytes);

		const storage::StringId name = storageSystem.intern("flowui.ui.clay");
		clayMemory = storageSystem.allocatePersistent(
			clayArenaCapacityBytes,
			alignof(std::max_align_t),
			storage::AllocationTag{
				.memoryClass = storage::MemoryClass::WindowPersistent,
				.resourceKind = storage::ResourceKind::UiContext,
				.window = window,
				.frameSlot = storage::InvalidFrameSlot,
				.debugName = name,
			});
		clayArena = Clay_CreateArenaWithCapacityAndMemory(clayArenaCapacityBytes, clayMemory.data);

		const float initialScreenWidth = static_cast<float>(std::max(1, appConfig.window.width));
		const float initialScreenHeight = static_cast<float>(std::max(1, appConfig.window.height));
		const Clay_Dimensions initialLayoutDimensions{initialScreenWidth, initialScreenHeight};

		clayContext = Clay_Initialize(clayArena, initialLayoutDimensions, Clay_ErrorHandler{});
		if (!clayContext) {
			storageSystem.releasePersistent(clayMemory);
			clayMemory = {};
			throw std::runtime_error("FlowUi: Clay_Initialize failed. Increase ui.clayArenaCapacityBytes.");
		}

		const float configuredDpi = std::max(1.0f, appConfig.ui.dpi);
		pointsToPixelsScale = std::max(0.0f, appConfig.ui.fontScale) * (configuredDpi / kPointsPerInch);
		if (pointsToPixelsScale <= 0.0f) {
			pointsToPixelsScale = configuredDpi / kPointsPerInch;
		}
		inputManagerConfig = appConfig.ui.inputManager;
		flowScopes.reserve(32);
		constructedElementStack.reserve(16);
#if FLOW_UI_DEV_MODE
		devToolsConfig = appConfig.dev;
		devPanelVisible = devToolsConfig.enabled && devToolsConfig.panelOpenByDefault;
#endif
	}

	manager_storage::UiManagerState::~UiManagerState() noexcept {
		if (storage && clayMemory) storage->releasePersistent(clayMemory);
	}

	void UiManager::initStorage(storage::IStorageSystem& storageSystem, WindowId window, const AppConfig& config) {
		if (storage_) throw std::logic_error("UiManager is already initialized.");
		const storage::StringId name = storageSystem.intern("flowui.ui.root");
		const storage::ResourceKey key{storage::ResourceDomain::Layout, name, window};
		const storage::ManagerRecordHandle handle = manager_storage::createState<manager_storage::UiManagerState>(
			storageSystem, key, storage::ResourceKind::UiContext, name,
			std::ref(storageSystem), window, config);
		storage_ = &storageSystem;
		window_ = window;
		stateHandle_ = handle.packed();
		state_ = manager_storage::state<manager_storage::UiManagerState>(
			storage_, handle, storage::ResourceKind::UiContext);
		if (!state_) throw std::runtime_error("UiManager storage record publication failed.");

		Clay_SetCurrentContext(state_->clayContext);
		Clay_SetMeasureTextFunction(
			+[](Clay_StringSlice text, Clay_TextElementConfig* config, void* userData) -> Clay_Dimensions {
				const auto* uiManager = static_cast<const UiManager*>(userData);
				if (!uiManager) {
					return Clay_Dimensions{ 0.0f, 0.0f };
				}
				return uiManager->measureText(text, config);
			},
			this);
		try {
			inputFieldManager_.init(
				storageSystem, window, state_->inputManagerConfig,
				state_->pointsToPixelsScale, state_->textLayoutService);
			inputFieldManager_.setClipboardAccess(
				setClipboardTextAccessor_,
				getClipboardTextAccessor_);
		} catch (...) {
			destroyStorage();
			throw;
		}
		try {
			popupManager_.init(storageSystem, window);
		} catch (...) {
			destroyStorage();
			throw;
		}
		try {
			shortcutManager_.init(storageSystem, window);
			shortcutManager_.installDefaultTextShortcuts(config.ui.shortcuts.textEditing);
		} catch (...) {
			destroyStorage();
			throw;
		}
#if FLOW_UI_DEV_MODE
		if (state_->devToolsConfig.enabled && state_->devToolsConfig.useShortcutManagerForPanelToggle) {
			const ShortcutChord toggleChord{
				.key = state_->devToolsConfig.panelToggleChord.key,
				.ctrl = state_->devToolsConfig.panelToggleChord.ctrl,
				.shift = state_->devToolsConfig.panelToggleChord.shift,
				.alt = state_->devToolsConfig.panelToggleChord.alt,
				.super = state_->devToolsConfig.panelToggleChord.super,
				.trigger = toShortcutTrigger(state_->devToolsConfig.panelToggleChord.trigger),
			};
			state_->devPanelToggleShortcutId = shortcutManager_.registerShortcut(
				toggleChord, ShortcutScope::Global, 1000,
				[this](ShortcutContext&) {
					if (!state_->devToolsConfig.enabled) return false;
					state_->devPanelVisible = !state_->devPanelVisible;
					return true;
				});
		}
#endif
	}

	void UiManager::destroyStorage() noexcept {
		shortcutManager_.destroy();
		popupManager_.destroy();
		inputFieldManager_.destroy();
		if (storage_) {
			try {
				const storage::StringId name = storage_->intern("flowui.ui.root");
				(void)storage_->removeManagerRecord(
					storage::ResourceKey{storage::ResourceDomain::Layout, name, window_},
					storage::ResourceKind::UiContext);
			} catch (...) {
			}
		}
		state_ = nullptr;
		stateHandle_ = 0;
		elementManager_ = nullptr;
		actionManager_ = nullptr;
		window_ = InvalidWindowId;
		storage_ = nullptr;
	}

	ElementManager& UiManager::elements() {
		if (!elementManager_) {
			throw std::runtime_error("FlowUi: UiManager is not connected to ElementManager.");
		}
		return *elementManager_;
	}

	const ElementManager& UiManager::elements() const {
		if (!elementManager_) {
			throw std::runtime_error("FlowUi: UiManager is not connected to ElementManager.");
		}
		return *elementManager_;
	}

	ActionManager& UiManager::actions() {
		if (!actionManager_) {
			throw std::runtime_error("FlowUi: UiManager is not connected to ActionManager.");
		}
		return *actionManager_;
	}

	const ActionManager& UiManager::actions() const {
		if (!actionManager_) {
			throw std::runtime_error("FlowUi: UiManager is not connected to ActionManager.");
		}
		return *actionManager_;
	}

	ActionInvocationStatus UiManager::invoke(ActionCall call) {
		return actions().invoke(call, ActionInvocationSource{
			.kind = ActionInvocationSourceKind::UiManager,
			.window = window_,
		});
	}

		void UiManager::setClipboardText(std::string_view text) const {
			if (!setClipboardTextAccessor_) {
				return;
			}
			setClipboardTextAccessor_(text);
		}

		std::string UiManager::clipboardText() const {
			if (!getClipboardTextAccessor_) {
				return {};
			}
			return getClipboardTextAccessor_();
		}

		bool UiManager::hasClipboardAccess() const {
			return static_cast<bool>(setClipboardTextAccessor_) && static_cast<bool>(getClipboardTextAccessor_);
		}

		Clay_ElementDeclaration UiManager::inputContentElement(const Clay_TextElementConfig& textConfig) const {
			float lineHeight = static_cast<float>(std::max<uint16_t>(1u, textConfig.fontSize)) * state_->pointsToPixelsScale;
			const detail::text::TextLayoutResult& layout = state_->textLayoutService.layout(
				detail::text::TextLayoutRequest{
					.fontView = &state_->fontView,
					.fontId = static_cast<FontId>(textConfig.fontId),
					.pointsToPixelsScale = state_->pointsToPixelsScale,
					.fontSize = textConfig.fontSize,
					.letterSpacing = textConfig.letterSpacing,
				});
			if (layout.success) lineHeight = layout.lineHeight;

			Clay_ElementDeclaration declaration{};
			declaration.layout.sizing = Clay_Sizing{
				.width = CLAY_SIZING_GROW(0),
				.height = CLAY_SIZING_FIXED(std::max(1.0f, lineHeight)),
			};
			return declaration;
		}

		void UiManager::setClipboardAccessors(
			std::function<void(std::string_view)> setClipboardTextAccessor,
			std::function<std::string()> getClipboardTextAccessor) {
			setClipboardTextAccessor_ = std::move(setClipboardTextAccessor);
			getClipboardTextAccessor_ = std::move(getClipboardTextAccessor);
			if (state_) {
				inputFieldManager_.setClipboardAccess(
					setClipboardTextAccessor_,
					getClipboardTextAccessor_);
			}
		}

		void UiManager::setCursorAccessor(std::function<void(CursorType)> setCursorTypeAccessor) {
			setCursorTypeAccessor_ = std::move(setCursorTypeAccessor);
		}

		void UiManager::requestCursor(CursorType cursorType, uint8_t priority) {
			if (priority < state_->cursorPriority) {
				return;
			}
			state_->cursor = cursorType;
			state_->cursorPriority = priority;
		}

		FontId UiManager::resolveFont(FontFamilyId familyId, uint32_t weight, FontStyle style) const {
			return state_->fontView.resolve(familyId, weight, style);
		}

		FontId UiManager::resolveFont(std::string_view familyName, uint32_t weight, FontStyle style) const {
			return state_->fontView.resolve(familyName, weight, style);
		}

		Clay_Dimensions UiManager::measureText(Clay_StringSlice text, Clay_TextElementConfig* config) const {
			if (!config || !text.chars || text.length <= 0) {
				return Clay_Dimensions{ 0.0f, 0.0f };
			}

		const FlowUi::Font::FontFaceData* fontFace = detail::text::resolveFontFace(&state_->fontView, config->fontId);
		if (!fontFace) {
			const float fallbackEmPixels = static_cast<float>(std::max<uint16_t>(1u, config->fontSize)) * state_->pointsToPixelsScale;
			return Clay_Dimensions{
				static_cast<float>(text.length) * fallbackEmPixels * 0.5f,
				fallbackEmPixels,
			};
		}

		const detail::text::TextLayoutResult& layoutResult = state_->textLayoutService.layout(
			detail::text::TextLayoutRequest{
				.text = std::string_view(text.chars, static_cast<size_t>(text.length)),
				.fontView = &state_->fontView,
				.fontId = static_cast<FontId>(config->fontId),
				.pointsToPixelsScale = state_->pointsToPixelsScale,
				.fontSize = config->fontSize,
				.letterSpacing = config->letterSpacing,
				.includeGlyphGeometry = false,
			});

		if (!layoutResult.success) {
			return Clay_Dimensions{ 0.0f, 0.0f };
		}

		return Clay_Dimensions{ layoutResult.measuredWidth, layoutResult.lineHeight };
	}

	void UiManager::beginFrame(
		const storage::FrameToken& frame,
		const FrameInput& frameInput,
		const manager_storage::FontFrameView& fontView,
		float screenWidth,
		float screenHeight)
	{
#if FLOW_UI_DEV_MODE
		FLOWUI_DEV_TIMING_ZONE_IF(
			devTimingRecorder_, devSystems::TimingCategory::Frame,
			devSystems::TimingZoneRole::Work, "flowui.ui.begin_frame");
#endif
		if (!state_->clayContext) {
			throw std::runtime_error("FlowUi: Clay context is not initialized.");
		}
		state_->activeFrame = frame;
		state_->frameArena = storage_->frameArena(frame, storage::MemoryClass::FrameTransient);
		state_->fontView = fontView;
		inputFieldManager_.setFontFrameView(fontView, state_->pointsToPixelsScale);
		if (!state_->frameArena.context) {
			throw std::runtime_error("FlowUi: window frame arena is unavailable.");
		}

		{
#if FLOW_UI_DEV_MODE
			FLOWUI_DEV_TIMING_ZONE_BALANCED_IF(
				devTimingRecorder_, devSystems::TimingCategory::Input,
				devSystems::TimingZoneRole::Work, "flowui.ui.interaction_advance");
#endif
			advanceFrameInteractionSnapshots();
		}
		state_->previousFrameInputForCurrentLayout = state_->frameInputForCurrentLayout;
		state_->frameInputForCurrentLayout = frameInput;
		Clay_SetCurrentContext(state_->clayContext);

		const float clampedScreenWidth = std::max(1.0f, screenWidth);
		const float clampedScreenHeight = std::max(1.0f, screenHeight);
		Clay_SetLayoutDimensions(Clay_Dimensions{clampedScreenWidth, clampedScreenHeight});
		Clay_SetPointerState(
			Clay_Vector2{frameInput.mouseX, frameInput.mouseY},
			frameInput.mouseDown[0]);
		{
#if FLOW_UI_DEV_MODE
			FLOWUI_DEV_TIMING_ZONE_BALANCED_IF(
				devTimingRecorder_, devSystems::TimingCategory::Input,
				devSystems::TimingZoneRole::Work, "flowui.popup.begin_frame");
#endif
			popupManager_.beginFrame(
				state_->frameInputForCurrentLayout,
				state_->previousFrameInputForCurrentLayout,
				clampedScreenWidth,
				clampedScreenHeight);
		}
		if (popupManager_.suppressesAllPrimaryPointerInput()) {
			state_->frameInputForCurrentLayout.mouseDown[0] = false;
			Clay_SetPointerState(
				Clay_Vector2{frameInput.mouseX, frameInput.mouseY},
				false);
		}
		{
#if FLOW_UI_DEV_MODE
			FLOWUI_DEV_TIMING_ZONE_BALANCED_IF(
				devTimingRecorder_, devSystems::TimingCategory::Input,
				devSystems::TimingZoneRole::Work, "flowui.input_field.begin_frame");
#endif
			inputFieldManager_.beginFrame(
				state_->frameInputForCurrentLayout,
				state_->previousFrameInputForCurrentLayout,
				popupManager_.suppressedAnchorClayId());
		}
		{
#if FLOW_UI_DEV_MODE
			FLOWUI_DEV_TIMING_ZONE_BALANCED_IF(
				devTimingRecorder_, devSystems::TimingCategory::Input,
				devSystems::TimingZoneRole::Work, "flowui.shortcut.begin_frame");
#endif
			shortcutManager_.beginFrame(
				*this, state_->frameInputForCurrentLayout, state_->previousFrameInputForCurrentLayout);
		}
		state_->cursor = CursorType::Arrow;
		state_->cursorPriority = 0;
		{
#if FLOW_UI_DEV_MODE
			FLOWUI_DEV_TIMING_ZONE_BALANCED_IF(
				devTimingRecorder_, devSystems::TimingCategory::Layout,
				devSystems::TimingZoneRole::Work, "flowui.layout.scroll_update");
#endif
			Clay_UpdateScrollContainers(
				false,
				Clay_Vector2{frameInput.scrollX, frameInput.scrollY},
				static_cast<float>(frameInput.dt));
		}
		state_->constructedElementStack.clear();
		state_->flowScopes.beginFrame();
#if FLOW_UI_DEV_MODE
		state_->flowRootIdTracker.beginFrame();
		state_->clayBridgeIdTracker.beginFrame();
		state_->devRuntime.beginFrame();
		state_->devRootElementOpenThisFrame = false;
#endif
		{
#if FLOW_UI_DEV_MODE
			FLOWUI_DEV_TIMING_ZONE_IF(
				devTimingRecorder_, devSystems::TimingCategory::Layout,
				devSystems::TimingZoneRole::Work, "flowui.layout.begin");
#endif
			Clay_BeginLayout();
		}
#if FLOW_UI_DEV_MODE
		if (state_->devToolsConfig.enabled && state_->devPanelVisible) {
			Clay_ElementDeclaration devRoot{};
			const Clay_ElementId devRootId = toClaySID("_Flow_Dev_root_");
			devRoot.layout.sizing.width = CLAY_SIZING_GROW(0);
			devRoot.layout.sizing.height = CLAY_SIZING_GROW(0);
			devRoot.layout.layoutDirection = CLAY_LEFT_TO_RIGHT;
			Clay__OpenElementWithId(devRootId);
			Clay__ConfigureOpenElement(devRoot);
			state_->devRootElementOpenThisFrame = true;
		}
#endif
	}

	Clay_RenderCommandArray UiManager::endFrame()
	{
#if FLOW_UI_DEV_MODE
		FLOWUI_DEV_TIMING_ZONE_IF(
			devTimingRecorder_, devSystems::TimingCategory::Frame,
			devSystems::TimingZoneRole::Work, "flowui.ui.end_frame");
#endif
		if (!state_->clayContext) {
			throw std::runtime_error("FlowUi: Clay context is not initialized.");
		}

		Clay_SetCurrentContext(state_->clayContext);
		const int32_t autoClosedConstructedElements =
			static_cast<int32_t>(state_->constructedElementStack.size());
		closeConstructedToDepth(0, false);
		restoreFlowScope(1);
		if (autoClosedConstructedElements > 0) {
			std::fprintf(
				stderr,
				"[FlowUi] Warning: auto-closed %d constructed element(s). Call ui.drawConstructed() for each ui.createElement(...).construct().\n",
				autoClosedConstructedElements);
		}
#if FLOW_UI_DEV_MODE
		if (state_->devRootElementOpenThisFrame) {
			if (state_->devToolsConfig.enabled && state_->devPanelVisible) {
#if !defined(FLOWUI_SKIP_LEGACY_DEV_ELEMENTS)
				{
					FLOWUI_DEV_TIMING_ZONE_BALANCED_IF(
						devTimingRecorder_, devSystems::TimingCategory::DevTool,
						devSystems::TimingZoneRole::DevToolWork, "flowui.dev_tool.build");
					devMode::drawDebugView(*this);
				}
#endif
			}
			Clay__CloseElement();
			state_->devRootElementOpenThisFrame = false;
		}
#endif
		Clay_RenderCommandArray renderCommands{};
		{
#if FLOW_UI_DEV_MODE
			FLOWUI_DEV_TIMING_ZONE_IF(
				devTimingRecorder_, devSystems::TimingCategory::Layout,
				devSystems::TimingZoneRole::Work, "flowui.layout.end");
#endif
			renderCommands = Clay_EndLayout(static_cast<float>(state_->frameInputForCurrentLayout.dt));
		}

		{
#if FLOW_UI_DEV_MODE
			FLOWUI_DEV_TIMING_ZONE_BALANCED_IF(
				devTimingRecorder_, devSystems::TimingCategory::Input,
				devSystems::TimingZoneRole::Work, "flowui.input.interaction_snapshot");
#endif
			InteractionSnapshot& interactionSnapshot = state_->currentInteractionSnapshot;
			Clay_ElementIdArray hoveredIds = Clay_GetPointerOverIds();
			interactionSnapshot.hoveredElementIds.reserve(static_cast<size_t>(hoveredIds.length));
			for (int32_t i = 0; i < hoveredIds.length; ++i) {
				interactionSnapshot.hoveredElementIds.push_back(hoveredIds.internalArray[i].id);
			}

			const bool isPrimaryPointerDown = state_->frameInputForCurrentLayout.mouseDown[0];
			if (isPrimaryPointerDown && !state_->wasPrimaryPointerDownLastFrame) {
				interactionSnapshot.pressedElementIds = interactionSnapshot.hoveredElementIds;
				const uint32_t suppressedAnchor = popupManager_.suppressedAnchorClayId();
				if (suppressedAnchor != 0) {
					auto& pressed = interactionSnapshot.pressedElementIds;
					pressed.erase(
						std::remove(pressed.begin(), pressed.end(), suppressedAnchor),
						pressed.end());
				}
			} else if (isPrimaryPointerDown && state_->wasPrimaryPointerDownLastFrame) {
				interactionSnapshot.heldElementIds = interactionSnapshot.hoveredElementIds;
			} else if (!isPrimaryPointerDown && state_->wasPrimaryPointerDownLastFrame) {
				interactionSnapshot.releasedElementIds = interactionSnapshot.hoveredElementIds;
			}
			state_->wasPrimaryPointerDownLastFrame = isPrimaryPointerDown;
		}

		{
#if FLOW_UI_DEV_MODE
			FLOWUI_DEV_TIMING_ZONE_BALANCED_IF(
				devTimingRecorder_, devSystems::TimingCategory::Input,
				devSystems::TimingZoneRole::Work, "flowui.input_field.end_frame");
#endif
			renderCommands = inputFieldManager_.endFrame(renderCommands);
		}
		storage_->noteManagerMutation(window_);
		if (state_->cursor != state_->previousCursor) {
			if (setCursorTypeAccessor_) {
				setCursorTypeAccessor_(state_->cursor);
			}
			state_->previousCursor = state_->cursor;
		}
#if FLOW_UI_DEV_MODE
		state_->devRuntime.endFrame();
		state_->flowRootIdTracker.discardFrame();
		state_->clayBridgeIdTracker.discardFrame();
#endif
		{
#if FLOW_UI_DEV_MODE
			FLOWUI_DEV_TIMING_ZONE_BALANCED_IF(
				devTimingRecorder_, devSystems::TimingCategory::Input,
				devSystems::TimingZoneRole::Work, "flowui.popup.end_frame");
#endif
			popupManager_.endFrame();
		}
		return renderCommands;
	}
	
	char* UiManager::allocBytes(size_t nBytes, size_t align)
	{
		if (!state_ || !state_->activeFrame) {
			throw std::logic_error("FlowUi frame storage is not active.");
		}
		void* memory = state_->frameArena.allocate(nBytes, align);
		if (!memory) throw std::runtime_error("FlowUi window frame arena allocation failed.");
		return static_cast<char*>(memory);
	}
	
	Clay_String UiManager::toClayString(std::string_view s)
	{
		const size_t len = s.size();
		char* dst = allocBytes(len + 1, alignof(char));
		std::memcpy(dst, s.data(), len);
		dst[len] = '\0';
		
		Clay_String out;
		out.isStaticallyAllocated = false;
		out.length = (int)len;
		out.chars = dst;
		return out;
	}

	std::string_view UiManager::normalizeUiResourceName(ResourceKey key) const {
		const storage::ResourceKey normalized = key_storage::normalizeResourceKey(
			*storage_, key, ResourceDomain::Ui,
			key_storage::ResourceScope::WindowLocal, window_);
		return storage_->string(normalized.name);
	}

	Clay_String UiManager::toClayString(ResourceKey key) {
		return toClayString(normalizeUiResourceName(key));
	}

	TextureRef* UiManager::imageData(TextureRef textureRef)
	{
		char* dst = allocBytes(sizeof(TextureRef), alignof(TextureRef));
		std::memcpy(dst, &textureRef, sizeof(TextureRef));
		return reinterpret_cast<TextureRef*>(dst);
	}

	TextureRef* UiManager::storeTexture(const TextureRef& textureRef)
	{
		return imageData(textureRef);
	}
	
	Clay_ElementId UiManager::toClaySID(std::string_view s) {
		return CLAY_SID(toClayString(s));
	}

	Clay_ElementId UiManager::toClaySID(ResourceKey key) {
		return toClaySID(normalizeUiResourceName(key));
	}
	
	Clay_ElementId UiManager::toClayEID(FlowElementID id) {
		const uint32_t clayId = FlowIDToClayID(id);
#if FLOW_UI_DEV_MODE
		claimClayBridgeForDev(detail::element::toInstanceKey(id), id.debugName);
#endif
		return Clay_ElementId{
			.id = clayId,
			.offset = 0,
			.baseId = clayId,
#if FLOW_UI_DEV_MODE
			.stringId = id.debugName.empty() ? Clay_String{} : toClayString(id.debugName),
#else
			.stringId = {},
#endif
		};
	}

	Clay_ElementId UiManager::toClayEID(GlobalFlowID id) {
		const uint32_t clayId = FlowIDToClayID(id);
#if FLOW_UI_DEV_MODE
		claimClayBridgeForDev(detail::element::toInstanceKey(id), id.debugName);
#endif
		return Clay_ElementId{
			.id = clayId,
			.offset = 0,
			.baseId = clayId,
#if FLOW_UI_DEV_MODE
			.stringId = id.debugName.empty() ? Clay_String{} : toClayString(id.debugName),
#else
			.stringId = {},
#endif
		};
	}

	Clay_ElementId UiManager::toClayEID(FlowElementPartID id) {
		const uint32_t clayId = FlowIDToClayID(id);
#if FLOW_UI_DEV_MODE
		claimClayBridgeForDev(detail::element::toInstanceKey(id), id.debugName);
#endif
		return Clay_ElementId{
			.id = clayId,
			.offset = 0,
			.baseId = clayId,
#if FLOW_UI_DEV_MODE
			.stringId = id.debugName.empty() ? Clay_String{} : toClayString(id.debugName),
#else
			.stringId = {},
#endif
		};
	}

	const InteractionSnapshot& UiManager::getPreviousFramesInteraction() const {
		return state_->previousInteractionSnapshot;
	}

	const FrameInput& UiManager::getCurrentFrameInput() const {
		return state_->frameInputForCurrentLayout;
	}

	const FrameInput& UiManager::getPreviousFrameInput() const {
		return state_->previousFrameInputForCurrentLayout;
	}

#if FLOW_UI_DEV_MODE
	devMode::DevRuntime& UiManager::devRuntime() { return state_->devRuntime; }
	const devMode::DevRuntime& UiManager::devRuntime() const { return state_->devRuntime; }
	DevToolsConfig& UiManager::devToolsConfig() { return state_->devToolsConfig; }
	const DevToolsConfig& UiManager::devToolsConfig() const { return state_->devToolsConfig; }
	devMode::PerformanceDiagnostics& UiManager::performanceDiagnostics() { return state_->performanceDiagnostics; }
	const devMode::PerformanceDiagnostics& UiManager::performanceDiagnostics() const {
		return state_->performanceDiagnostics;
	}

	void UiManager::claimClayBridgeForDev(
		detail::element::ElementInstanceKey instanceId,
		std::string_view debugName) {
		if (!state_ || !instanceId) return;
		const manager_storage::ClayBridgeCollisionForDev* collision =
			state_->clayBridgeIdTracker.claim(
				instanceId,
				detail::element_id::toClayValue(instanceId.value),
				manager_storage::FlowRootClaimSourceForDev{
					.debugPath = std::string(debugName),
					.fileName = "<direct typed Clay bridge>",
				});
		if (!collision) return;
		std::fprintf(
			stderr,
			"[FlowUi] Warning: Clay ID %u aliases distinct Flow IDs %llu ('%s') and %llu ('%s').\n",
			collision->clayId,
			static_cast<unsigned long long>(collision->first.instanceId.value),
			collision->first.source.debugPath.c_str(),
			static_cast<unsigned long long>(collision->duplicate.instanceId.value),
			collision->duplicate.source.debugPath.c_str());
	}
#endif



namespace detail {

#if FLOW_UI_DEV_MODE
	// Header-only builders enter the frame-local diagnostic tracker here.
	void claimFlowRootForDev(
		UiManager& uiManager,
		FlowElementID elementId,
		FlowDefinitionID definitionId,
		std::string_view fileName,
		uint32_t line,
		uint32_t column,
		std::string_view functionName,
		bool automaticIdentity) {
		const manager_storage::FlowRootClaimSourceForDev source{
			.definitionId = definitionId,
			.debugPath = std::string(elementId.debugName),
			.fileName = std::string(fileName),
			.functionName = std::string(functionName),
			.line = line,
			.column = column,
			.automaticIdentity = automaticIdentity,
		};
		const manager_storage::FlowRootCollisionForDev* collision =
			uiManager.state_->flowRootIdTracker.claim(
				detail::element::toInstanceKey(elementId),
				source);
		const manager_storage::ClayBridgeCollisionForDev* clayCollision =
			uiManager.state_->clayBridgeIdTracker.claim(
				detail::element::toInstanceKey(elementId),
				detail::element_id::toClayValue(elementId.value),
				source);

		if (collision && collision->first.automaticIdentity &&
			collision->duplicate.automaticIdentity) {
			std::fprintf(
				stderr,
				"[FlowUi] Warning: automatic element ID %llu was used more than once in one window frame. A loop or repeated callsite must use FlowUi::Indexed(), FlowUi::Keyed(), or context.indexedIDs().next(). First: %s:%u:%u. Duplicate: %s:%u:%u.\n",
				static_cast<unsigned long long>(collision->instanceId.value),
				collision->first.fileName.c_str(),
				collision->first.line,
				collision->first.column,
				collision->duplicate.fileName.c_str(),
				collision->duplicate.line,
				collision->duplicate.column);
		} else if (collision) {
			std::fprintf(
				stderr,
				"[FlowUi] Warning: duplicate Flow root id %llu. First: definition %llu, '%s' at %s:%u:%u. Duplicate: definition %llu, '%s' at %s:%u:%u.\n",
				static_cast<unsigned long long>(collision->instanceId.value),
				static_cast<unsigned long long>(collision->first.definitionId.value),
				collision->first.debugPath.c_str(),
				collision->first.fileName.c_str(),
				collision->first.line,
				collision->first.column,
				static_cast<unsigned long long>(collision->duplicate.definitionId.value),
				collision->duplicate.debugPath.c_str(),
				collision->duplicate.fileName.c_str(),
				collision->duplicate.line,
				collision->duplicate.column);
		}

		if (clayCollision) {
			std::fprintf(
				stderr,
				"[FlowUi] Warning: Clay ID %u aliases distinct Flow IDs. First: %llu, definition %llu, '%s' at %s:%u:%u. Duplicate: %llu, definition %llu, '%s' at %s:%u:%u.\n",
				clayCollision->clayId,
				static_cast<unsigned long long>(clayCollision->first.instanceId.value),
				static_cast<unsigned long long>(clayCollision->first.source.definitionId.value),
				clayCollision->first.source.debugPath.c_str(),
				clayCollision->first.source.fileName.c_str(),
				clayCollision->first.source.line,
				clayCollision->first.source.column,
				static_cast<unsigned long long>(clayCollision->duplicate.instanceId.value),
				static_cast<unsigned long long>(clayCollision->duplicate.source.definitionId.value),
				clayCollision->duplicate.source.debugPath.c_str(),
				clayCollision->duplicate.source.fileName.c_str(),
				clayCollision->duplicate.source.line,
				clayCollision->duplicate.source.column);
		}
	}

namespace devModeBridge {

	std::size_t beginCapturedFlowElement(
		UiManager& uiManager,
		FlowDefinitionID definitionId,
		uint64_t definitionTypeHash,
		std::string_view definitionTypeToken,
		FlowElementID elementId,
		bool isInternalToDevMode) {
		if (isInternalToDevMode && uiManager.devToolsConfig().excludeInternalDevElementsFromCapture) {
			return devMode::DevRuntime::kInvalidCaptureIndex;
		}

		devMode::DevRuntime& runtime = uiManager.devRuntime();
		const std::size_t captureIndex = runtime.beginCapturedFlowElement(
			definitionId,
			definitionTypeHash,
			elementId,
			{},
			definitionTypeToken,
			isInternalToDevMode);

		if (captureIndex == devMode::DevRuntime::kInvalidCaptureIndex) {
			return captureIndex;
		}

		const devMode::DevRegistry& registry = devMode::DevRegistry::instance();
		const devMode::ElementDescriptor* descriptor = registry.findElementByDefinitionId(definitionId);
		const bool hasRegisteredDefinition = descriptor != nullptr;
		const bool hasRegisteredParamsStruct =
			(descriptor != nullptr) && (registry.findStructByTypeHash(descriptor->paramsStructTypeHash) != nullptr);
		const bool hasRegisteredStateStruct =
			(descriptor != nullptr) && (registry.findStructByTypeHash(descriptor->stateStructTypeHash) != nullptr);
		const bool hasRegisteredResourcesStruct =
			(descriptor != nullptr) && (registry.findStructByTypeHash(descriptor->resourcesStructTypeHash) != nullptr);

		runtime.setCapturedElementRegistrationMetadata(
			captureIndex,
			hasRegisteredDefinition,
			hasRegisteredParamsStruct,
			hasRegisteredStateStruct,
			hasRegisteredResourcesStruct,
			descriptor ? descriptor->definitionName : std::string_view{},
			descriptor ? descriptor->definitionTypeToken : definitionTypeToken);

		runtime.setCapturedElementAuthoringKeys(
			captureIndex,
			elementId.debugName,
			descriptor ? descriptor->definitionName : std::string_view{});
		return captureIndex;
	}

	bool endCapturedFlowElement(UiManager& uiManager) {
		return uiManager.devRuntime().endCapturedElement();
	}

} // namespace devModeBridge
#endif

} // namespace detail

#if FLOW_UI_DEV_MODE
namespace devMode::elementCapture {

	DevRuntime& runtime(UiManager& uiManager) {
		return uiManager.devRuntime();
	}

} // namespace devMode::elementCapture
#endif

	void UiManager::drawConstructed() {
		if (!state_->clayContext) {
			throw std::runtime_error("FlowUi: Clay context is not initialized.");
		}
		if (state_->constructedElementStack.empty()) {
			throw std::runtime_error("FlowUi: drawConstructed called without a matching construct call.");
		}

		Clay_SetCurrentContext(state_->clayContext);
		closeConstructedToDepth(state_->constructedElementStack.size() - 1, false);
	}

	FlowElementID UiManager::currentFlowScope() const noexcept {
		return state_ ? state_->flowScopes.current() : RootFlowScopeID;
	}

#if FLOW_UI_DEV_MODE
	std::string_view UiManager::joinFlowDebugPath(
		std::string_view parent,
		std::string_view child) {
		if (child.empty()) return {};
		const size_t separatorBytes = parent.empty() ? 0 : 1;
		const size_t totalBytes = parent.size() + separatorBytes + child.size();
		char* joined = allocBytes(totalBytes, alignof(char));
		size_t offset = 0;
		if (!parent.empty()) {
			std::memcpy(joined, parent.data(), parent.size());
			offset = parent.size();
			joined[offset++] = '/';
		}
		std::memcpy(joined + offset, child.data(), child.size());
		return std::string_view(joined, totalBytes);
	}

	std::string_view UiManager::indexedFlowDebugName(IndexedElementName name) {
		char indexBuffer[32]{};
		const auto [indexEnd, error] = std::to_chars(
			indexBuffer, indexBuffer + sizeof(indexBuffer), name.index);
		if (error != std::errc{}) {
			throw std::runtime_error("FlowUi could not format an indexed element debug name.");
		}
		const size_t indexBytes = static_cast<size_t>(indexEnd - indexBuffer);
		const size_t totalBytes = name.debugName.size() + indexBytes + 2;
		char* formatted = allocBytes(totalBytes, alignof(char));
		size_t offset = 0;
		std::memcpy(formatted, name.debugName.data(), name.debugName.size());
		offset += name.debugName.size();
		formatted[offset++] = '[';
		std::memcpy(formatted + offset, indexBuffer, indexBytes);
		offset += indexBytes;
		formatted[offset] = ']';
		return std::string_view(formatted, totalBytes);
	}

	std::string_view UiManager::automaticFlowDebugName(AutoElementName name) {
		char lineBuffer[16]{};
		char columnBuffer[16]{};
		const auto [lineEnd, lineError] = std::to_chars(
			lineBuffer, lineBuffer + sizeof(lineBuffer), name.line);
		const auto [columnEnd, columnError] = std::to_chars(
			columnBuffer, columnBuffer + sizeof(columnBuffer), name.column);
		if (lineError != std::errc{} || columnError != std::errc{}) {
			throw std::runtime_error("FlowUi could not format an automatic element debug name.");
		}
		constexpr std::string_view prefix = "@auto/";
		const size_t lineBytes = static_cast<size_t>(lineEnd - lineBuffer);
		const size_t columnBytes = static_cast<size_t>(columnEnd - columnBuffer);
		const size_t totalBytes = prefix.size() + name.fileName.size() +
			lineBytes + columnBytes + 2;
		char* formatted = allocBytes(totalBytes, alignof(char));
		size_t offset = 0;
		std::memcpy(formatted + offset, prefix.data(), prefix.size());
		offset += prefix.size();
		std::memcpy(formatted + offset, name.fileName.data(), name.fileName.size());
		offset += name.fileName.size();
		formatted[offset++] = ':';
		std::memcpy(formatted + offset, lineBuffer, lineBytes);
		offset += lineBytes;
		formatted[offset++] = ':';
		std::memcpy(formatted + offset, columnBuffer, columnBytes);
		return std::string_view(formatted, totalBytes);
	}

	std::string_view UiManager::globalFlowDebugName(GlobalFlowID id) {
		constexpr std::string_view prefix = "@global/";
		const size_t totalBytes = prefix.size() + id.debugName.size();
		char* formatted = allocBytes(totalBytes, alignof(char));
		std::memcpy(formatted, prefix.data(), prefix.size());
		std::memcpy(formatted + prefix.size(), id.debugName.data(), id.debugName.size());
		return std::string_view(formatted, totalBytes);
	}
#endif

	FlowElementID UiManager::resolveLocalElementID(
		FlowElementID parent,
		FlowDefinitionID definition,
		LocalElementName name) {
		if (!parent || !definition || !name) {
			throw std::invalid_argument("FlowUi local element IDs require valid parent, definition, and name values.");
		}
		return detail::element_id::resolveLocal(
			parent,
			definition,
			name.token
#if FLOW_UI_DEV_MODE
			, joinFlowDebugPath(parent.debugName, name.debugName)
#endif
		);
	}

	FlowElementID UiManager::resolveLocalElementID(
		FlowElementID parent,
		FlowDefinitionID definition,
		RuntimeElementName name) {
		if (!parent || !definition || !name) {
			throw std::invalid_argument("FlowUi runtime element IDs require valid parent, definition, and name values.");
		}
		return detail::element_id::resolveLocal(
			parent,
			definition,
			name.token
#if FLOW_UI_DEV_MODE
			, joinFlowDebugPath(parent.debugName, name.debugName)
#endif
		);
	}

	FlowElementID UiManager::resolveIndexedElementID(
		FlowElementID parent,
		FlowDefinitionID definition,
		IndexedElementName name) {
		if (!parent || !definition || !name) {
			throw std::invalid_argument("FlowUi indexed element IDs require valid parent, definition, and name values.");
		}
		return detail::element_id::resolveLocal(
			parent,
			definition,
			name.token
#if FLOW_UI_DEV_MODE
			, joinFlowDebugPath(parent.debugName, indexedFlowDebugName(name))
#endif
		);
	}

	FlowElementID UiManager::resolveAutomaticElementID(
		FlowElementID parent,
		FlowDefinitionID definition,
		AutoElementName name) {
		if (!parent || !definition || !name) {
			throw std::invalid_argument("FlowUi automatic element IDs require valid parent, definition, and callsite values.");
		}
		return detail::element_id::resolveAutomatic(
			parent,
			definition,
			name.token
#if FLOW_UI_DEV_MODE
			, joinFlowDebugPath(parent.debugName, automaticFlowDebugName(name))
#endif
		);
	}

	FlowElementID UiManager::normalizeGlobalElementID(GlobalFlowID id) {
		if (!id) throw std::invalid_argument("FlowUi requires a valid global element ID.");
		return FlowElementID{
			.value = id.value,
#if FLOW_UI_DEV_MODE
			.debugName = globalFlowDebugName(id),
#endif
		};
	}

	FlowElementID UiManager::normalizePartElementID(FlowElementPartID id) const noexcept {
		return FlowElementID{
			.value = id.value,
#if FLOW_UI_DEV_MODE
			.debugName = id.debugName,
#endif
		};
	}

	FlowElementPartID UiManager::resolveElementPartID(
		FlowDefinitionID ownerDefinition,
		FlowElementID owner,
		FlowElementPart part) {
		if (!ownerDefinition || !owner || !part) {
			throw std::invalid_argument(
				"FlowUi semantic parts require valid owner definition, owner ID, and declaration values.");
		}
		return FlowElementPartID{
			.value = detail::identity_hash::compose(
				detail::element_id::kPartInstanceDomain,
				ownerDefinition.value,
				owner.value,
				part.token),
#if FLOW_UI_DEV_MODE
			.debugName = joinFlowDebugPath(owner.debugName, part.debugName),
#endif
		};
	}

	size_t UiManager::pushFlowScope(FlowElementID id) {
		if (!state_ || !state_->activeFrame || !id) {
			throw std::logic_error("FlowUi cannot enter an element scope outside an active frame.");
		}
		return state_->flowScopes.push(id);
	}

	void UiManager::restoreFlowScope(size_t depth) noexcept {
		if (state_) state_->flowScopes.restore(depth);
	}

	size_t UiManager::constructedElementDepth() const noexcept {
		return state_ ? state_->constructedElementStack.size() : 0;
	}

	void UiManager::closeConstructedToDepth(size_t depth, bool warn) noexcept {
		if (!state_ || depth >= state_->constructedElementStack.size()) return;
		const size_t closedCount = state_->constructedElementStack.size() - depth;
		while (state_->constructedElementStack.size() > depth) {
			manager_storage::ConstructedElementFrame frame =
				std::move(state_->constructedElementStack.back());
			Clay__CloseElement();
			state_->constructedElementStack.pop_back();
			restoreFlowScope(frame.priorFlowScopeDepth);
#if FLOW_UI_DEV_MODE
			(void)state_->devRuntime.endCapturedElement();
#endif
#if FLOW_UI_DEV_MODE && FLOWUI_DEV_TIMING_LEVEL >= 2
			frame.subtreeTiming.end();
#endif
		}
#if FLOW_UI_DEV_MODE
		if (warn) {
			std::fprintf(
				stderr,
				"[FlowUi] Warning: auto-closed %zu constructed element(s) left open by a draw callback.\n",
				closedCount);
		}
#else
		(void)warn;
		(void)closedCount;
#endif
	}

	void UiManager::retainConstructedElement(
		Clay_ElementId clayId,
		FlowElementID flowId,
		size_t priorFlowScopeDepth
#if FLOW_UI_DEV_MODE && FLOWUI_DEV_TIMING_LEVEL >= 2
		, FlowDefinitionID definitionId
#endif
		) {
		manager_storage::ConstructedElementFrame frame{
			.clayId = clayId,
			.flowId = flowId,
			.priorFlowScopeDepth = priorFlowScopeDepth,
		};
#if FLOW_UI_DEV_MODE && FLOWUI_DEV_TIMING_LEVEL >= 2
		if (devTimingRecorder_) {
			frame.subtreeTiming.begin(
				*devTimingRecorder_, devSystems::timing_zones::kElementConstructedSubtree,
				devSystems::TimingEntityRef::element(definitionId, flowId));
		}
#endif
		state_->constructedElementStack.push_back(std::move(frame));
	}

	void UiManager::advanceFrameInteractionSnapshots() {
		std::swap(state_->previousInteractionSnapshot, state_->currentInteractionSnapshot);
		state_->currentInteractionSnapshot.hoveredElementIds.clear();
		state_->currentInteractionSnapshot.pressedElementIds.clear();
		state_->currentInteractionSnapshot.heldElementIds.clear();
		state_->currentInteractionSnapshot.releasedElementIds.clear();
	}

	void UiManager::cancelFrameState() noexcept {
		if (!state_) return;
#if FLOW_UI_DEV_MODE && FLOWUI_DEV_TIMING_LEVEL >= 2
		for (auto& frame : state_->constructedElementStack) {
			frame.subtreeTiming.end(devSystems::TimingRecordFlag::Canceled);
		}
#endif
		state_->constructedElementStack.clear();
		state_->flowScopes.cancelFrame();
		popupManager_.cancelFrame();
		state_->frameArena = {};
		state_->activeFrame = {};
#if FLOW_UI_DEV_MODE
		state_->flowRootIdTracker.discardFrame();
		state_->clayBridgeIdTracker.discardFrame();
#endif
	}

	const ThemeManager& UiManager::appThemes() const {
		if (!themeManager_) {
			throw std::runtime_error("UiManager: ThemeManager is not connected.");
		}
		return *themeManager_;
	}

	const FlowUiTheme& UiManager::flowTheme() const {
		return theme<FlowUiTheme>();
	}

} // namespace FlowUi
