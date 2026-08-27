#include "managers/UiManager.hpp"
#include <algorithm>
#include <cstdint>
#include <cstdio>

#if FLOW_UI_DEV_MODE
#include <charconv>
#include "devSystems/devMonitoringAndReporting/errors/DevError.hpp"
#include "devSystems/devMonitoringAndReporting/memory/DevContainerMemory.hpp"
#include "devSystems/devMonitoringAndReporting/memory/DevMemorySources.hpp"
#include "devSystems/devMonitoringAndReporting/timing/DevTimingZone.hpp"
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
void reportDevTreeDiagnostics(
	const FlowUi::devSystems::tooling::DevTreeCapture& capture) noexcept {
	using namespace FlowUi;
	static constexpr auto source =
		devSystems::makeDevErrorSource("flowui.dev_tree.capture_contract");
	if (capture.lastFinishFailed()) {
		devSystems::recordGlobalDevDiagnostic(
			makeError(ErrorCode::InternalInvariantBroken, ErrorSite::UiManagerEndFrame),
			source,
			"DevTreeCapture could not traverse the completed Clay forest; the prior snapshot was retained.");
		return;
	}
	for (const devSystems::tooling::DevTreeDiagnostic& diagnostic :
		capture.current().diagnostics) {
		const bool internalFailure =
			diagnostic.code == devSystems::tooling::DevTreeDiagnosticCode::FlowCaptureUnbalanced
#if FLOW_UI_DEV_CAPTURE_CLAY
			|| diagnostic.code == devSystems::tooling::DevTreeDiagnosticCode::ClayBridgeTraversalFailed
#endif
			;
		devSystems::recordGlobalDevDiagnostic(
			makeError(
				internalFailure ? ErrorCode::InternalInvariantBroken : ErrorCode::ElementDefinitionConflict,
				ErrorSite::UiManagerEndFrame,
				diagnostic.expected,
				diagnostic.observed),
			source,
			"The completed developer UI tree contains a Flow/Clay capture contract violation.");
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
			state_->devTreeCapture.appendDevMemorySamples(sink);
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
			throw FlowUiException(makeError(ErrorCode::RendererConfigurationInvalid, ErrorSite::UiManagerInitialize));
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
#endif
	}

	manager_storage::UiManagerState::~UiManagerState() noexcept {
		if (storage && clayMemory) storage->releasePersistent(clayMemory);
	}

	void UiManager::initStorage(storage::IStorageSystem& storageSystem, WindowId window, const AppConfig& config) {
		if (storage_) throw FlowUiException(makeError(ErrorCode::ObjectAlreadyInitialized, ErrorSite::UiManagerInitialize));
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
		if (!state_) throw FlowUiException(makeError(ErrorCode::ResourcePublicationFailed, ErrorSite::UiManagerInitialize));

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
			popupManager_.init(storageSystem, window, config.errors.policy);
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
			throw FlowUiException(makeError(ErrorCode::ObjectNotInitialized, ErrorSite::UiManagerAccessElements));
		}
		return *elementManager_;
	}

	const ElementManager& UiManager::elements() const {
		if (!elementManager_) {
			throw FlowUiException(makeError(ErrorCode::ObjectNotInitialized, ErrorSite::UiManagerAccessElements));
		}
		return *elementManager_;
	}

	ActionManager& UiManager::actions() {
		if (!actionManager_) {
			throw FlowUiException(makeError(ErrorCode::ObjectNotInitialized, ErrorSite::UiManagerAccessActions));
		}
		return *actionManager_;
	}

	const ActionManager& UiManager::actions() const {
		if (!actionManager_) {
			throw FlowUiException(makeError(ErrorCode::ObjectNotInitialized, ErrorSite::UiManagerAccessActions));
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
			throw FlowUiException(makeError(ErrorCode::ObjectNotInitialized, ErrorSite::UiManagerBeginFrame));
		}
		state_->activeFrame = frame;
		state_->frameArena = storage_->frameArena(frame, storage::MemoryClass::FrameTransient);
		state_->fontView = fontView;
		inputFieldManager_.setFontFrameView(fontView, state_->pointsToPixelsScale);
		if (!state_->frameArena.context) {
			throw FlowUiException(makeError(ErrorCode::FrameNotReady, ErrorSite::UiManagerBeginFrame));
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
		state_->devTreeCapture.beginFrame(
			window_, state_->devRuntime.frameCounter(), *state_->clayContext, devTimingRecorder_);
		if (devOverrideEngine_) {
			devOverrideEngine_->beginWindowFrame(
				window_, state_->devRuntime.frameCounter());
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
			throw FlowUiException(makeError(ErrorCode::ObjectNotInitialized, ErrorSite::UiManagerEndFrame));
		}

		Clay_SetCurrentContext(state_->clayContext);
		const int32_t autoClosedConstructedElements =
			static_cast<int32_t>(state_->constructedElementStack.size());
		closeConstructedToDepth(0, false, true);
		restoreFlowScope(1);
		if (autoClosedConstructedElements > 0) {
			detail::reportErrorEvent(ErrorEventView{
				.error = makeError(
					ErrorCode::FramePhaseViolation, ErrorSite::UiManagerEndFrame,
					0u, static_cast<std::uint64_t>(autoClosedConstructedElements)),
				.kind = ErrorEventKind::Resolved,
				.resolution = ErrorResolution::UsedFallback,
			});
		}
		Clay_RenderCommandArray renderCommands{};
#if FLOW_UI_DEV_MODE && FLOW_UI_DEV_CAPTURE_CLAY
		state_->devTreeCapture.noteAuthoredClayEnd();
#endif
		{
#if FLOW_UI_DEV_MODE
			FLOWUI_DEV_TIMING_ZONE_IF(
				devTimingRecorder_, devSystems::TimingCategory::Layout,
				devSystems::TimingZoneRole::Work, "flowui.layout.end");
#endif
			renderCommands = Clay_EndLayout(static_cast<float>(state_->frameInputForCurrentLayout.dt));
		}
#if FLOW_UI_DEV_MODE
		state_->devTreeCapture.finishAfterLayout();
		reportDevTreeDiagnostics(state_->devTreeCapture);
		if (devOverrideEngine_) {
			if (state_->devTreeCapture.lastFinishFailed()) {
				devOverrideEngine_->cancelWindowFrame(window_);
			} else {
				devOverrideEngine_->endWindowFrame(window_);
			}
		}
#endif

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
		if (devSchemaRegistry_) devSchemaRegistry_->publishPendingAtSafePoint();
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
			throw FlowUiException(makeError(ErrorCode::FramePhaseViolation, ErrorSite::UiManagerRender));
		}
		void* memory = state_->frameArena.allocate(nBytes, align);
		if (!memory) throw FlowUiException(makeError(ErrorCode::FrameCapacityExceeded, ErrorSite::UiManagerRender));
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
	const devSystems::tooling::DevTreeSnapshot& UiManager::devTreeSnapshot() const noexcept {
		return state_->devTreeCapture.current();
	}
	devSystems::tooling::DevTreeCapture& UiManager::devTreeCapture() noexcept {
		return state_->devTreeCapture;
	}
	const devSystems::tooling::DevTreeCapture& UiManager::devTreeCapture() const noexcept {
		return state_->devTreeCapture;
	}

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
		static constexpr auto source =
			devSystems::makeDevErrorSource("flowui.ui.clay_bridge_collision");
		devSystems::recordGlobalDevDiagnostic(
			makeError(
				ErrorCode::ElementDefinitionConflict,
				ErrorSite::UiManagerDefineElement,
				collision->first.instanceId.value,
				collision->duplicate.instanceId.value),
			source);
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
			static constexpr auto source =
				devSystems::makeDevErrorSource("flowui.ui.automatic_element_id_collision");
			devSystems::recordGlobalDevDiagnostic(
				makeError(
					ErrorCode::ElementDefinitionConflict,
					ErrorSite::UiManagerDefineElement,
					collision->instanceId.value,
					collision->duplicate.definitionId.value),
				source);
		} else if (collision) {
			static constexpr auto source =
				devSystems::makeDevErrorSource("flowui.ui.flow_root_collision");
			devSystems::recordGlobalDevDiagnostic(
				makeError(
					ErrorCode::ElementDefinitionConflict,
					ErrorSite::UiManagerDefineElement,
					collision->instanceId.value,
					collision->duplicate.definitionId.value),
				source);
		}

		if (clayCollision) {
			static constexpr auto source =
				devSystems::makeDevErrorSource("flowui.ui.clay_id_alias");
			devSystems::recordGlobalDevDiagnostic(
				makeError(
					ErrorCode::ElementDefinitionConflict,
					ErrorSite::UiManagerDefineElement,
					clayCollision->first.instanceId.value,
					clayCollision->duplicate.instanceId.value),
				source);
		}
	}

namespace devModeBridge {

	devSystems::tooling::DevTreeCapture::Token beginCapturedFlowElement(
		UiManager& uiManager,
		FlowDefinitionID definitionId,
		uint64_t definitionTypeHash,
		std::string_view definitionTypeToken,
		FlowElementID elementId,
		bool isInternalToDevMode,
		bool constructed,
		std::string_view sourceFile,
		uint32_t sourceLine,
		uint32_t sourceColumn,
		std::string_view sourceFunction) {
		(void)definitionTypeHash;
		const devMode::DevRegistry& registry = devMode::DevRegistry::instance();
		const devMode::ElementDescriptor* descriptor = registry.findElementByDefinitionId(definitionId);
		return uiManager.devTreeCapture().beginFlow({
			.definition = definitionId,
			.instance = elementId,
			.definitionName = descriptor ? descriptor->definitionName : std::string_view{},
			.definitionTypeToken = descriptor ? descriptor->definitionTypeToken : definitionTypeToken,
			.sourceFile = sourceFile,
			.sourceFunction = sourceFunction,
			.sourceLine = sourceLine,
			.sourceColumn = sourceColumn,
			.constructed = constructed,
			.internalDev = isInternalToDevMode,
			.suppress = isInternalToDevMode &&
				uiManager.devToolsConfig().excludeInternalDevElementsFromCapture,
		});
	}

	void endCapturedFlowElement(
		UiManager& uiManager,
		devSystems::tooling::DevTreeCapture::Token token,
		bool autoClosed) noexcept {
		uiManager.devTreeCapture().endFlow(token, autoClosed);
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
			throw FlowUiException(makeError(ErrorCode::ObjectNotInitialized, ErrorSite::UiManagerInvokeElement));
		}
		if (state_->constructedElementStack.empty()) {
			throw FlowUiException(makeError(ErrorCode::FramePhaseViolation, ErrorSite::UiManagerInvokeElement));
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
			detail::terminateForFatalError(makeError(ErrorCode::InternalInvariantBroken, ErrorSite::UiManagerDefineElement));
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
			detail::terminateForFatalError(makeError(ErrorCode::InternalInvariantBroken, ErrorSite::UiManagerDefineElement));
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
			throw FlowUiException(makeError(ErrorCode::InvalidElementId, ErrorSite::UiManagerDefineElement));
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
			throw FlowUiException(makeError(ErrorCode::InvalidElementId, ErrorSite::UiManagerDefineElement));
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
			throw FlowUiException(makeError(ErrorCode::InvalidElementId, ErrorSite::UiManagerDefineElement));
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
			throw FlowUiException(makeError(ErrorCode::InvalidElementId, ErrorSite::UiManagerDefineElement));
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
		if (!id) throw FlowUiException(makeError(ErrorCode::InvalidElementId, ErrorSite::UiManagerDefineElement));
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
			throw FlowUiException(makeError(ErrorCode::InvalidElementId, ErrorSite::UiManagerDefineElement));
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
			throw FlowUiException(makeError(
				!id ? ErrorCode::InvalidElementId : ErrorCode::FramePhaseViolation,
				ErrorSite::UiManagerOpenElement));
		}
		return state_->flowScopes.push(id);
	}

	void UiManager::restoreFlowScope(size_t depth) noexcept {
		if (state_) state_->flowScopes.restore(depth);
	}

	size_t UiManager::constructedElementDepth() const noexcept {
		return state_ ? state_->constructedElementStack.size() : 0;
	}

	void UiManager::closeConstructedToDepth(
		size_t depth, bool warn, bool autoClosedAtFrameEnd) noexcept {
		if (!state_ || depth >= state_->constructedElementStack.size()) return;
		const size_t closedCount = state_->constructedElementStack.size() - depth;
		while (state_->constructedElementStack.size() > depth) {
			manager_storage::ConstructedElementFrame frame =
				std::move(state_->constructedElementStack.back());
			Clay__CloseElement();
			state_->constructedElementStack.pop_back();
			restoreFlowScope(frame.priorFlowScopeDepth);
#if FLOW_UI_DEV_MODE
			state_->devTreeCapture.endFlow(frame.treeToken, warn || autoClosedAtFrameEnd);
#endif
#if FLOW_UI_DEV_MODE && FLOWUI_DEV_TIMING_LEVEL >= 2
			frame.subtreeTiming.end();
#endif
		}
#if FLOW_UI_DEV_MODE
		if (warn) {
			static constexpr auto source =
				devSystems::makeDevErrorSource("flowui.ui.auto_closed_elements");
			devSystems::recordGlobalDevDiagnostic(
				makeError(
					ErrorCode::FramePhaseViolation,
					ErrorSite::UiManagerCloseElement,
					0u,
					closedCount),
				source);
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
#if FLOW_UI_DEV_MODE
		, devSystems::tooling::DevTreeCapture::Token treeToken
#endif
#if FLOW_UI_DEV_MODE && FLOWUI_DEV_TIMING_LEVEL >= 2
		, FlowDefinitionID definitionId
#endif
		) {
		manager_storage::ConstructedElementFrame frame{
			.clayId = clayId,
			.flowId = flowId,
			.priorFlowScopeDepth = priorFlowScopeDepth,
#if FLOW_UI_DEV_MODE
			.treeToken = treeToken,
#endif
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
		state_->devTreeCapture.cancelFrame();
		if (devOverrideEngine_) devOverrideEngine_->cancelWindowFrame(window_);
		state_->flowRootIdTracker.discardFrame();
		state_->clayBridgeIdTracker.discardFrame();
#endif
	}

	const ThemeManager& UiManager::appThemes() const {
		if (!themeManager_) {
			throw FlowUiException(makeError(ErrorCode::ObjectNotInitialized, ErrorSite::UiManagerAccessTheme));
		}
		return *themeManager_;
	}

	const FlowUiTheme& UiManager::flowTheme() const {
		return theme<FlowUiTheme>();
	}

} // namespace FlowUi
