#include "Ui/UiContext.hpp"
#include <algorithm>
#include <cstdint>

#include "managers/FontManager.hpp"

namespace {

constexpr float kDefaultDpi = 96.0f;
constexpr float kPointsPerInch = 72.0f;

bool DecodeNextUtf8Codepoint(const Clay_StringSlice& stringSlice, int& byteOffset, uint32_t& outCodepoint) {
	if (!stringSlice.chars || byteOffset >= stringSlice.length) {
		return false;
	}

	const auto* bytes = reinterpret_cast<const uint8_t*>(stringSlice.chars);
	const uint8_t first = bytes[byteOffset];

	if (first < 0x80u) {
		outCodepoint = first;
		byteOffset += 1;
		return true;
	}

	auto continuation = [&](int index) -> uint8_t {
		return (index < stringSlice.length) ? bytes[index] : 0u;
	};

	if ((first & 0xE0u) == 0xC0u && byteOffset + 1 < stringSlice.length) {
		const uint8_t c1 = continuation(byteOffset + 1);
		if ((c1 & 0xC0u) == 0x80u) {
			outCodepoint = ((first & 0x1Fu) << 6u) | (c1 & 0x3Fu);
			byteOffset += 2;
			return true;
		}
	} else if ((first & 0xF0u) == 0xE0u && byteOffset + 2 < stringSlice.length) {
		const uint8_t c1 = continuation(byteOffset + 1);
		const uint8_t c2 = continuation(byteOffset + 2);
		if ((c1 & 0xC0u) == 0x80u && (c2 & 0xC0u) == 0x80u) {
			outCodepoint = ((first & 0x0Fu) << 12u) | ((c1 & 0x3Fu) << 6u) | (c2 & 0x3Fu);
			byteOffset += 3;
			return true;
		}
	} else if ((first & 0xF8u) == 0xF0u && byteOffset + 3 < stringSlice.length) {
		const uint8_t c1 = continuation(byteOffset + 1);
		const uint8_t c2 = continuation(byteOffset + 2);
		const uint8_t c3 = continuation(byteOffset + 3);
		if ((c1 & 0xC0u) == 0x80u && (c2 & 0xC0u) == 0x80u && (c3 & 0xC0u) == 0x80u) {
			outCodepoint = ((first & 0x07u) << 18u) | ((c1 & 0x3Fu) << 12u) | ((c2 & 0x3Fu) << 6u) | (c3 & 0x3Fu);
			byteOffset += 4;
			return true;
		}
	}

	outCodepoint = 0xFFFDu;
	byteOffset += 1;
	return true;
}

const FontManager::FontFaceData* ResolveFontFace(const FontManager* fontManager, uint16_t fontId) {
	if (!fontManager) {
		return nullptr;
	}

	const FontManager::FontFaceData* face = fontManager->getFontById(static_cast<int>(fontId));
	if (!face) {
		face = fontManager->getFontById(0);
	}
	return face;
}

Clay_Dimensions ClayMeasureTextCallback(Clay_StringSlice text, Clay_TextElementConfig* config, void* userData) {
	const auto* uiContext = static_cast<const FlowUi::UiContext*>(userData);
	if (!uiContext) {
		return Clay_Dimensions{ 0.0f, 0.0f };
	}
	return uiContext->measureText(text, config);
}

} // namespace

namespace FlowUi
{
	
	UiContext::UiContext(ElementRegistry& elementRegistry, const FlowUi::AppConfig& appConfig)
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

		pointsToPixelsScale_ = std::max(0.0f, appConfig.ui.fontScale) * (kDefaultDpi / kPointsPerInch);
		if (pointsToPixelsScale_ <= 0.0f) {
			pointsToPixelsScale_ = kDefaultDpi / kPointsPerInch;
		}

		Clay_SetCurrentContext(clayContext_);
		Clay_SetMeasureTextFunction(ClayMeasureTextCallback, this);
	}

	void UiContext::setFontManager(const ::FontManager* fontManager) {
		fontManager_ = fontManager;
	}

	Clay_Dimensions UiContext::measureText(Clay_StringSlice text, Clay_TextElementConfig* config) const {
		if (!config || !text.chars || text.length <= 0) {
			return Clay_Dimensions{ 0.0f, 0.0f };
		}

		const FontManager::FontFaceData* fontFace = ResolveFontFace(fontManager_, config->fontId);
		if (!fontFace) {
			const float fallbackEmPixels = static_cast<float>(std::max<uint16_t>(1u, config->fontSize)) * pointsToPixelsScale_;
			return Clay_Dimensions{
				static_cast<float>(text.length) * fallbackEmPixels * 0.5f,
				fallbackEmPixels,
			};
		}

		const FontManager::FontVariantData* variant = fontFace->defaultVariant();
		if (!variant || variant->glyphs.empty()) {
			return Clay_Dimensions{ 0.0f, 0.0f };
		}

		float emPixels = variant->fontSizePx;
		if (config->fontSize > 0) {
			emPixels = static_cast<float>(config->fontSize) * pointsToPixelsScale_;
		}
		if (emPixels <= 0.0f) {
			return Clay_Dimensions{ 0.0f, 0.0f };
		}

		const float emToPixels = emPixels / std::max(variant->emSize, 1.0e-6f);
		const float letterSpacingPx = static_cast<float>(config->letterSpacing);
		float penX = 0.0f;
		float minX = 0.0f;
		float maxX = 0.0f;

		uint32_t previousCodepoint = 0;
		bool hasPreviousCodepoint = false;
		int byteOffset = 0;
		while (byteOffset < text.length) {
			uint32_t codepoint = 0;
			if (!DecodeNextUtf8Codepoint(text, byteOffset, codepoint)) {
				break;
			}

			if (codepoint == '\n') {
				hasPreviousCodepoint = false;
				continue;
			}

			if (hasPreviousCodepoint) {
				penX += variant->kerningAdvance(previousCodepoint, codepoint) * emToPixels;
			}

			uint32_t glyphIndex = variant->fallbackGlyphIndex;
			const auto glyphIt = variant->unicodeToGlyphIndex.find(codepoint);
			if (glyphIt != variant->unicodeToGlyphIndex.end()) {
				glyphIndex = glyphIt->second;
			}
			if (glyphIndex >= variant->glyphs.size()) {
				if (variant->glyphs.empty()) {
					continue;
				}
				glyphIndex = 0;
			}

			const FontManager::GlyphData& glyph = variant->glyphs[glyphIndex];
			minX = std::min(minX, penX + glyph.planeLeft * emToPixels);
			maxX = std::max(maxX, penX + glyph.planeRight * emToPixels);

			penX += glyph.advanceX * emToPixels + letterSpacingPx;
			maxX = std::max(maxX, penX);

			previousCodepoint = codepoint;
			hasPreviousCodepoint = true;
		}

		const float measuredWidth = std::max(0.0f, maxX - minX);
		float naturalLineHeight = variant->lineHeight * emToPixels;
		if (naturalLineHeight <= 0.0f) {
			naturalLineHeight = emPixels;
		}

		return Clay_Dimensions{ measuredWidth, naturalLineHeight };
	}

	void UiContext::initStringArenas(const FlowUi::AppConfig& cfg)
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
	
	void UiContext::beginFrame(uint32_t frameIndex, const FrameInput& frameInput, float screenWidth, float screenHeight)
	{
		if (arenas_.empty()) {
			throw std::runtime_error("FlowUi: UiContext string arenas are not initialized.");
		}
		if (!clayContext_) {
			throw std::runtime_error("FlowUi: Clay context is not initialized.");
		}

		curArena_ = (arenasCount_ == 0) ? 0 : (frameIndex % arenasCount_);
		arenas_[curArena_].offset = 0;

		advanceFrameInteractionSnapshots();
		frameInputForCurrentLayout_ = frameInput;

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
		Clay_BeginLayout();
	}

	Clay_RenderCommandArray UiContext::endFrame()
	{
		if (!clayContext_) {
			throw std::runtime_error("FlowUi: Clay context is not initialized.");
		}

		Clay_SetCurrentContext(clayContext_);
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

		setCurrentInteractionSnapshot(std::move(interactionSnapshot));
		return renderCommands;
	}
	
	char* UiContext::allocBytes(size_t nBytes, size_t align)
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
	
	Clay_String UiContext::str(std::string_view s)
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
	
	Clay_ElementId UiContext::sid(std::string_view s) {
		return CLAY_SID(str(s));
	}
	
	Clay_ElementId UiContext::eid(std::string_view s) {
		return Clay_GetElementId(str(s));
	}

	ElementBuilder UiContext::createElement(std::string_view elementTypeName, std::string_view instanceIdPath) {
    	const ElementDefinition* definition = elementRegistry_.findElement(elementTypeName);
    	if (!definition) {
    	    throw std::runtime_error("FlowUi: createElement called with unregistered element type.");
    	}
	
    	return ElementBuilder(*this, definition, std::string(instanceIdPath));
	}

	void UiContext::setCurrentInteractionSnapshot(InteractionSnapshot snapshot) {
	    currentInteractionSnapshot_ = std::move(snapshot);
	}

	void UiContext::advanceFrameInteractionSnapshots() {
	    previousInteractionSnapshot_ = std::move(currentInteractionSnapshot_);
	    currentInteractionSnapshot_ = InteractionSnapshot{};
	}


} // namespace FlowUi
