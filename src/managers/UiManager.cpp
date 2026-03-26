#include "managers/UiManager.hpp"
#include <algorithm>
#include <cstdint>
#include <cstdio>

#include "managers/FontManager.hpp"
#include "internal/TextLayoutEngine.hpp"

namespace {

constexpr float kPointsPerInch = 72.0f;

} // namespace

namespace FlowUi
{
	
	UiManager::UiManager(ElementRegistry& elementRegistry, const FlowUi::AppConfig& appConfig)
		: elementRegistry_(elementRegistry)
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

		Clay_SetCurrentContext(clayContext_);

		const float clampedScreenWidth = std::max(1.0f, screenWidth);
		const float clampedScreenHeight = std::max(1.0f, screenHeight);
		Clay_SetLayoutDimensions(Clay_Dimensions{clampedScreenWidth, clampedScreenHeight});
		Clay_SetPointerState(
			Clay_Vector2{frameInput.mouseX, frameInput.mouseY},
			frameInput.mouseDown[0]);
		Clay_UpdateScrollContainers(
			true,
			Clay_Vector2{frameInput.scrollX, frameInput.scrollY},
			static_cast<float>(frameInput.dt));
		constructedElementStack_.clear();
		Clay_BeginLayout();
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
			++autoClosedConstructedElements;
		}
		if (autoClosedConstructedElements > 0) {
			std::fprintf(
				stderr,
				"[FlowUi] Warning: auto-closed %d constructed element(s). Call ui.drawConstructed() for each ui.createElement(...).construct().\n",
				autoClosedConstructedElements);
		}
		Clay_RenderCommandArray renderCommands = Clay_EndLayout();

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

	ElementBuilder UiManager::createElement(std::string_view elementTypeName, std::string_view instanceIdPath) {
    	const ElementDefinition* definition = elementRegistry_.findElement(elementTypeName);
    	if (!definition) {
    	    throw std::runtime_error("FlowUi: createElement called with unregistered element type.");
    	}
	
    	return ElementBuilder(*this, definition, std::string(instanceIdPath));
	}

	ElementBuilder UiManager::createElement(const ElementDefinition& elementDefinition, std::string_view instanceIdPath) {
		return ElementBuilder(*this, &elementDefinition, std::string(instanceIdPath));
	}

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
