#include "managers/UiManager.hpp"
#include <algorithm>
#include <cstdint>
#include <cstdio>

#if FLOW_UI_DEV_MODE
#include "devMode/debugView.hpp"
#include "devMode/registry.hpp"
#endif
#include "managers/FontManager.hpp"
#include "internal/ManagerStorage/ManagerStateAccess.hpp"
#include "internal/ManagerStorage/ResourceKeyNormalization.hpp"
#include "internal/ManagerStorage/UiManagerState.hpp"
#include "internal/TextLayoutEngine.hpp"

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
				state_->pointsToPixelsScale);
		} catch (...) {
			destroyStorage();
			throw;
		}
		try {
			shortcutManager_.init(storageSystem, window);
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
		window_ = InvalidWindowId;
		storage_ = nullptr;
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

			const FlowUi::Font::FontFaceData* fontFace = FlowUi::detail::ResolveFontFace(&state_->fontView, textConfig.fontId);
			const FlowUi::Font::FontVariantData* variant = fontFace ? fontFace->defaultVariant() : nullptr;
			if (variant && variant->emSize > 0.0f) {
				const float emPixels = textConfig.fontSize > 0
					? static_cast<float>(textConfig.fontSize) * state_->pointsToPixelsScale
					: variant->fontSizePx;
				if (emPixels > 0.0f) {
					lineHeight = variant->lineHeight * (emPixels / variant->emSize);
				}
			}

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

		const FlowUi::Font::FontFaceData* fontFace = FlowUi::detail::ResolveFontFace(&state_->fontView, config->fontId);
		if (!fontFace) {
			const float fallbackEmPixels = static_cast<float>(std::max<uint16_t>(1u, config->fontSize)) * state_->pointsToPixelsScale;
			return Clay_Dimensions{
				static_cast<float>(text.length) * fallbackEmPixels * 0.5f,
				fallbackEmPixels,
			};
		}

		const FlowUi::detail::TextLayoutResult layoutResult = FlowUi::detail::LayoutTextLine(
			FlowUi::detail::TextLayoutRequest{
				.text = text,
				.fontFace = fontFace,
				.pointsToPixelsScale = state_->pointsToPixelsScale,
				.fontSize = config->fontSize,
				.letterSpacing = config->letterSpacing,
				.lineOriginX = 0.0f,
				.lineOriginY = 0.0f,
				.emitGlyphQuads = false,
			},
			[](const FlowUi::detail::TextLayoutGlyphQuad&) {});

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

		advanceFrameInteractionSnapshots();
		state_->previousFrameInputForCurrentLayout = state_->frameInputForCurrentLayout;
		state_->frameInputForCurrentLayout = frameInput;
		inputFieldManager_.beginFrame(state_->frameInputForCurrentLayout, state_->previousFrameInputForCurrentLayout);
		shortcutManager_.beginFrame(*this, state_->frameInputForCurrentLayout, state_->previousFrameInputForCurrentLayout);
		state_->cursor = CursorType::Arrow;
		state_->cursorPriority = 0;

		Clay_SetCurrentContext(state_->clayContext);

		const float clampedScreenWidth = std::max(1.0f, screenWidth);
		const float clampedScreenHeight = std::max(1.0f, screenHeight);
		Clay_SetLayoutDimensions(Clay_Dimensions{clampedScreenWidth, clampedScreenHeight});
		Clay_SetPointerState(
			Clay_Vector2{frameInput.mouseX, frameInput.mouseY},
			frameInput.mouseDown[0]);
		Clay_UpdateScrollContainers(
			false,
			Clay_Vector2{frameInput.scrollX, frameInput.scrollY},
			static_cast<float>(frameInput.dt));
		state_->constructedElementStack.clear();
#if FLOW_UI_DEV_MODE
		state_->devRuntime.beginFrame();
		state_->devRootElementOpenThisFrame = false;
#endif
		Clay_BeginLayout();
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
		if (!state_->clayContext) {
			throw std::runtime_error("FlowUi: Clay context is not initialized.");
		}

		Clay_SetCurrentContext(state_->clayContext);
		int32_t autoClosedConstructedElements = 0;
		while (!state_->constructedElementStack.empty()) {
			Clay__CloseElement();
			state_->constructedElementStack.pop_back();
#if FLOW_UI_DEV_MODE
			(void)state_->devRuntime.endCapturedElement();
#endif
			++autoClosedConstructedElements;
		}
		if (autoClosedConstructedElements > 0) {
			std::fprintf(
				stderr,
				"[FlowUi] Warning: auto-closed %d constructed element(s). Call ui.drawConstructed() for each ui.createElement(...).construct().\n",
				autoClosedConstructedElements);
		}
#if FLOW_UI_DEV_MODE
		if (state_->devRootElementOpenThisFrame) {
			if (state_->devToolsConfig.enabled && state_->devPanelVisible) {
				devMode::drawDebugView(*this);
			}
			Clay__CloseElement();
			state_->devRootElementOpenThisFrame = false;
		}
#endif
		Clay_RenderCommandArray renderCommands = Clay_EndLayout(static_cast<float>(state_->frameInputForCurrentLayout.dt));

		InteractionSnapshot& interactionSnapshot = state_->currentInteractionSnapshot;
		Clay_ElementIdArray hoveredIds = Clay_GetPointerOverIds();
		interactionSnapshot.hoveredElementIds.reserve(static_cast<size_t>(hoveredIds.length));
		for (int32_t i = 0; i < hoveredIds.length; ++i) {
			interactionSnapshot.hoveredElementIds.push_back(hoveredIds.internalArray[i]);
		}

		const bool isPrimaryPointerDown = state_->frameInputForCurrentLayout.mouseDown[0];
		if (isPrimaryPointerDown && !state_->wasPrimaryPointerDownLastFrame) {
			interactionSnapshot.pressedElementIds = interactionSnapshot.hoveredElementIds;
		} else if (isPrimaryPointerDown && state_->wasPrimaryPointerDownLastFrame) {
			interactionSnapshot.heldElementIds = interactionSnapshot.hoveredElementIds;
		} else if (!isPrimaryPointerDown && state_->wasPrimaryPointerDownLastFrame) {
			interactionSnapshot.releasedElementIds = interactionSnapshot.hoveredElementIds;
		}
		state_->wasPrimaryPointerDownLastFrame = isPrimaryPointerDown;

		renderCommands = inputFieldManager_.endFrame(renderCommands);
		storage_->noteManagerMutation(window_);
		if (state_->cursor != state_->previousCursor) {
			if (setCursorTypeAccessor_) {
				setCursorTypeAccessor_(state_->cursor);
			}
			state_->previousCursor = state_->cursor;
		}
#if FLOW_UI_DEV_MODE
		state_->devRuntime.endFrame();
#endif
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
	
	Clay_ElementId UiManager::toClayEID(std::string_view s) {
		return Clay_GetElementId(toClayString(s));
	}

	Clay_ElementId UiManager::toClayEID(ResourceKey key) {
		return toClayEID(normalizeUiResourceName(key));
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
#endif



namespace detail {

	Clay_ElementId toClayElementId(UiManager& uiManager, std::string_view elementID) {
		return uiManager.toClayEID(elementID);
	}

	const InteractionSnapshot& previousInteraction(const UiManager& uiManager) {
		return uiManager.getPreviousFramesInteraction();
	}

	void pushConstructedElement(UiManager& uiManager, Clay_ElementId elementId) {
		uiManager.pushConstructedElement(elementId);
	}

#if FLOW_UI_DEV_MODE
namespace devModeBridge {

	std::size_t beginCapturedFlowElement(
		UiManager& uiManager,
		uint64_t definitionId,
		uint64_t definitionTypeHash,
		std::string_view definitionTypeToken,
		std::string_view elementID,
		uint64_t flowId,
		bool isInternalToDevMode) {
		if (isInternalToDevMode && uiManager.devToolsConfig().excludeInternalDevElementsFromCapture) {
			return devMode::DevRuntime::kInvalidCaptureIndex;
		}

		devMode::DevRuntime& runtime = uiManager.devRuntime();
		const std::size_t captureIndex = runtime.beginCapturedFlowElement(
			definitionId,
			definitionTypeHash,
			flowId,
			elementID,
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
			elementID,
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
		Clay__CloseElement();
		state_->constructedElementStack.pop_back();
#if FLOW_UI_DEV_MODE
		(void)state_->devRuntime.endCapturedElement();
#endif
	}

	void UiManager::pushConstructedElement(Clay_ElementId elementId) {
		state_->constructedElementStack.push_back(elementId);
	}

	void UiManager::advanceFrameInteractionSnapshots() {
		std::swap(state_->previousInteractionSnapshot, state_->currentInteractionSnapshot);
		state_->currentInteractionSnapshot.hoveredElementIds.clear();
		state_->currentInteractionSnapshot.pressedElementIds.clear();
		state_->currentInteractionSnapshot.heldElementIds.clear();
		state_->currentInteractionSnapshot.releasedElementIds.clear();
	}


} // namespace FlowUi
