#include "managers/UiManager.hpp"
#include <algorithm>
#include <cstdint>
#include <cstdio>

#if FLOW_UI_DEV_MODE
#include "devMode/debugView.hpp"
#include "devMode/registry.hpp"
#endif
#include "managers/FontManager.hpp"
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
	
	UiManager::UiManager(const FlowUi::AppConfig& appConfig)
	{
		initStringArenas(appConfig);

		const size_t minimumClayArenaCapacityBytes = static_cast<size_t>(Clay_MinMemorySize());
		const size_t configuredClayArenaCapacityBytes = appConfig.ui.clayArenaCapacityBytes;
		const size_t clayArenaCapacityBytes = (configuredClayArenaCapacityBytes == 0)
			? minimumClayArenaCapacityBytes
			: std::max(configuredClayArenaCapacityBytes, minimumClayArenaCapacityBytes);

		clayArenaMemory_ = std::make_unique<char[]>(clayArenaCapacityBytes);
		clayArena_ = Clay_CreateArenaWithCapacityAndMemory(clayArenaCapacityBytes, clayArenaMemory_.get());

		const float initialScreenWidth = static_cast<float>(std::max(1, appConfig.window.width));
		const float initialScreenHeight = static_cast<float>(std::max(1, appConfig.window.height));
		const Clay_Dimensions initialLayoutDimensions{initialScreenWidth, initialScreenHeight};

		clayContext_ = Clay_Initialize(clayArena_, initialLayoutDimensions, Clay_ErrorHandler{});
		if (!clayContext_)
		{
			throw std::runtime_error("FlowUi: Clay_Initialize failed. Increase ui.clayArenaCapacityBytes.");
		}

		const float configuredDpi = std::max(1.0f, appConfig.ui.dpi);
		pointsToPixelsScale_ = std::max(0.0f, appConfig.ui.fontScale) * (configuredDpi / kPointsPerInch);
		if (pointsToPixelsScale_ <= 0.0f) {
			pointsToPixelsScale_ = configuredDpi / kPointsPerInch;
		}
		inputFieldManager_.setConfig(appConfig.ui.inputManager);
		inputFieldManager_.setFontManager(nullptr, pointsToPixelsScale_);
#if FLOW_UI_DEV_MODE
		devToolsConfig_ = appConfig.dev;
		devPanelVisible_ = devToolsConfig_.enabled && devToolsConfig_.panelOpenByDefault;
		if (devToolsConfig_.enabled && devToolsConfig_.useShortcutManagerForPanelToggle) {
			const ShortcutChord toggleChord{
				.key = devToolsConfig_.panelToggleChord.key,
				.ctrl = devToolsConfig_.panelToggleChord.ctrl,
				.shift = devToolsConfig_.panelToggleChord.shift,
				.alt = devToolsConfig_.panelToggleChord.alt,
				.super = devToolsConfig_.panelToggleChord.super,
				.trigger = toShortcutTrigger(devToolsConfig_.panelToggleChord.trigger),
			};
			devPanelToggleShortcutId_ = shortcutManager_.registerShortcut(
				toggleChord,
				ShortcutScope::Global,
				1000,
				[this](ShortcutContext&) {
					if (!devToolsConfig_.enabled) {
						return false;
					}
					devPanelVisible_ = !devPanelVisible_;
					return true;
				});
		}
#endif

		Clay_SetCurrentContext(clayContext_);
		Clay_SetMeasureTextFunction(
			+[](Clay_StringSlice text, Clay_TextElementConfig* config, void* userData) -> Clay_Dimensions {
				const auto* uiManager = static_cast<const UiManager*>(userData);
				if (!uiManager) {
					return Clay_Dimensions{ 0.0f, 0.0f };
				}
				return uiManager->measureText(text, config);
			},
			this);
	}

		void UiManager::setFontManager(const ::FontManager* fontManager) {
			fontManager_ = fontManager;
			inputFieldManager_.setFontManager(fontManager_, pointsToPixelsScale_);
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
			if (priority < cursorPriority_) {
				return;
			}
			cursor_ = cursorType;
			cursorPriority_ = priority;
		}

		FontId UiManager::resolveFont(FontFamilyId familyId, uint32_t weight, FontStyle style) const {
			return fontManager_ ? fontManager_->resolveFont(familyId, weight, style) : 0;
		}

		FontId UiManager::resolveFont(std::string_view familyName, uint32_t weight, FontStyle style) const {
			return fontManager_ ? fontManager_->resolveFont(familyName, weight, style) : 0;
		}

		Clay_Dimensions UiManager::measureText(Clay_StringSlice text, Clay_TextElementConfig* config) const {
			if (!config || !text.chars || text.length <= 0) {
				return Clay_Dimensions{ 0.0f, 0.0f };
			}

		const FontManager::FontFaceData* fontFace = FlowUi::detail::ResolveFontFace(fontManager_, config->fontId);
		if (!fontFace) {
			const float fallbackEmPixels = static_cast<float>(std::max<uint16_t>(1u, config->fontSize)) * pointsToPixelsScale_;
			return Clay_Dimensions{
				static_cast<float>(text.length) * fallbackEmPixels * 0.5f,
				fallbackEmPixels,
			};
		}

		const FlowUi::detail::TextLayoutResult layoutResult = FlowUi::detail::LayoutTextLine(
			FlowUi::detail::TextLayoutRequest{
				.text = text,
				.fontFace = fontFace,
				.pointsToPixelsScale = pointsToPixelsScale_,
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

	void UiManager::initStringArenas(const FlowUi::AppConfig& cfg)
	{
		arenasCount_ = (cfg.vk.framesInFlight == 0) ? 1 : cfg.vk.framesInFlight;
		arenas_.resize(arenasCount_);
		
		for (auto& arena : arenas_)
		{
			arena.capacity = cfg.ui.stringArenaSize;
			arena.mem = std::unique_ptr<char[]>(new char[arena.capacity]);
			arena.offset = 0;
		}
		curArena_ = 0;
	}
	
	void UiManager::beginFrame(uint32_t frameIndex, const FrameInput& frameInput, float screenWidth, float screenHeight)
	{
		if (arenas_.empty()) {
			throw std::runtime_error("FlowUi: UiManager string arenas are not initialized.");
		}
		if (!clayContext_) {
			throw std::runtime_error("FlowUi: Clay context is not initialized.");
		}

		curArena_ = (arenasCount_ == 0) ? 0 : (frameIndex % arenasCount_);
		arenas_[curArena_].offset = 0;

		advanceFrameInteractionSnapshots();
		previousFrameInputForCurrentLayout_ = frameInputForCurrentLayout_;
		frameInputForCurrentLayout_ = frameInput;
		inputFieldManager_.beginFrame(frameInputForCurrentLayout_, previousFrameInputForCurrentLayout_);
		shortcutManager_.beginFrame(*this, frameInputForCurrentLayout_, previousFrameInputForCurrentLayout_);
		cursor_ = CursorType::Arrow;
		cursorPriority_ = 0;

		Clay_SetCurrentContext(clayContext_);

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
		constructedElementStack_.clear();
#if FLOW_UI_DEV_MODE
		devRuntime_.beginFrame();
		devRootElementOpenThisFrame_ = false;
#endif
		Clay_BeginLayout();
#if FLOW_UI_DEV_MODE
		if (devToolsConfig_.enabled && devPanelVisible_) {
			Clay_ElementDeclaration devRoot{};
			const Clay_ElementId devRootId = toClaySID("_Flow_Dev_root_");
			devRoot.layout.sizing.width = CLAY_SIZING_GROW(0);
			devRoot.layout.sizing.height = CLAY_SIZING_GROW(0);
			devRoot.layout.layoutDirection = CLAY_LEFT_TO_RIGHT;
			Clay__OpenElementWithId(devRootId);
			Clay__ConfigureOpenElement(devRoot);
			devRootElementOpenThisFrame_ = true;
		}
#endif
	}

	Clay_RenderCommandArray UiManager::endFrame()
	{
		if (!clayContext_) {
			throw std::runtime_error("FlowUi: Clay context is not initialized.");
		}

		Clay_SetCurrentContext(clayContext_);
		int32_t autoClosedConstructedElements = 0;
		while (!constructedElementStack_.empty()) {
			Clay__CloseElement();
			constructedElementStack_.pop_back();
#if FLOW_UI_DEV_MODE
			(void)devRuntime_.endCapturedElement();
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
		if (devRootElementOpenThisFrame_) {
			if (devToolsConfig_.enabled && devPanelVisible_) {
				devMode::drawDebugView(*this);
			}
			Clay__CloseElement();
			devRootElementOpenThisFrame_ = false;
		}
#endif
		Clay_RenderCommandArray renderCommands = Clay_EndLayout(static_cast<float>(frameInputForCurrentLayout_.dt));

		InteractionSnapshot interactionSnapshot;
		Clay_ElementIdArray hoveredIds = Clay_GetPointerOverIds();
		interactionSnapshot.hoveredElementIds.reserve(static_cast<size_t>(hoveredIds.length));
		for (int32_t i = 0; i < hoveredIds.length; ++i) {
			interactionSnapshot.hoveredElementIds.push_back(hoveredIds.internalArray[i]);
		}

		const bool isPrimaryPointerDown = frameInputForCurrentLayout_.mouseDown[0];
		if (isPrimaryPointerDown && !wasPrimaryPointerDownLastFrame_) {
			interactionSnapshot.pressedElementIds = interactionSnapshot.hoveredElementIds;
		} else if (isPrimaryPointerDown && wasPrimaryPointerDownLastFrame_) {
			interactionSnapshot.heldElementIds = interactionSnapshot.hoveredElementIds;
		} else if (!isPrimaryPointerDown && wasPrimaryPointerDownLastFrame_) {
			interactionSnapshot.releasedElementIds = interactionSnapshot.hoveredElementIds;
		}
		wasPrimaryPointerDownLastFrame_ = isPrimaryPointerDown;

		renderCommands = inputFieldManager_.endFrame(renderCommands);
		setCurrentInteractionSnapshot(std::move(interactionSnapshot));
		if (cursor_ != previousCursor_) {
			if (setCursorTypeAccessor_) {
				setCursorTypeAccessor_(cursor_);
			}
			previousCursor_ = cursor_;
		}
#if FLOW_UI_DEV_MODE
		devRuntime_.endFrame();
#endif
		return renderCommands;
	}
	
	char* UiManager::allocBytes(size_t nBytes, size_t align)
	{
		Arena& arena = arenas_[curArena_];
		
		size_t off = arena.offset;
		size_t aligned = (off + (align - 1)) & ~(align - 1);
		
		if (aligned + nBytes > arena.capacity)
		throw std::runtime_error("FlowUi string arena overflow: increase bytesPerArena");
		
		char* ptr = arena.mem.get() + aligned;
		arena.offset = aligned + nBytes;
		return ptr;
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

	TextureRef* UiManager::storeTexture(const TextureRef& textureRef)
	{
		char* dst = allocBytes(sizeof(TextureRef), alignof(TextureRef));
		std::memcpy(dst, &textureRef, sizeof(TextureRef));
		return reinterpret_cast<TextureRef*>(dst);
	}
	
	Clay_ElementId UiManager::toClaySID(std::string_view s) {
		return CLAY_SID(toClayString(s));
	}
	
	Clay_ElementId UiManager::toClayEID(std::string_view s) {
		return Clay_GetElementId(toClayString(s));
	}



	Clay_ElementId flowUiToClayElementId(UiManager& uiManager, std::string_view elementID) {
		return uiManager.toClayEID(elementID);
	}

	const InteractionSnapshot& flowUiPreviousInteraction(const UiManager& uiManager) {
		return uiManager.getPreviousFramesInteraction();
	}

	void flowUiPushConstructedElement(UiManager& uiManager, Clay_ElementId elementId) {
		uiManager.pushConstructedElement(elementId);
	}

#if FLOW_UI_DEV_MODE
	devMode::DevRuntime& flowUiDevRuntime(UiManager& uiManager) {
		return uiManager.devRuntime();
	}

	std::size_t flowUiDevBeginCapturedFlowElement(
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

	bool flowUiDevEndCapturedFlowElement(UiManager& uiManager) {
		return uiManager.devRuntime().endCapturedElement();
	}
#endif

	void UiManager::drawConstructed() {
		if (!clayContext_) {
			throw std::runtime_error("FlowUi: Clay context is not initialized.");
		}
		if (constructedElementStack_.empty()) {
			throw std::runtime_error("FlowUi: drawConstructed called without a matching construct call.");
		}

		Clay_SetCurrentContext(clayContext_);
		Clay__CloseElement();
		constructedElementStack_.pop_back();
#if FLOW_UI_DEV_MODE
		(void)devRuntime_.endCapturedElement();
#endif
	}

	void UiManager::pushConstructedElement(Clay_ElementId elementId) {
		constructedElementStack_.push_back(elementId);
	}

	void UiManager::setCurrentInteractionSnapshot(InteractionSnapshot snapshot) {
	    currentInteractionSnapshot_ = std::move(snapshot);
	}

	void UiManager::advanceFrameInteractionSnapshots() {
	    previousInteractionSnapshot_ = std::move(currentInteractionSnapshot_);
	    currentInteractionSnapshot_ = InteractionSnapshot{};
	}


} // namespace FlowUi
