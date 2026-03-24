#pragma once


#include <cstddef>
#include <cstdint>
#include <functional>
#include <memory>
#include <string_view>
#include <string>
#include <cstring>
#include <stdexcept>
#include <clay.h>
#include <vector>

#include "FlowUi/PublicStructs.hpp"
#include "managers/FlowUiElementSystem.hpp"
#include "managers/InputFieldManager.hpp"
#include "managers/ShortcutManager.hpp"
#include "window/Inputs.hpp"

struct FontManager;

namespace FlowUi {

class App;

class UiManager {
public:
	UiManager(ElementRegistry& elementRegistry, const AppConfig& appConfig);

	Clay_String toClayString(std::string_view s);
	TextureRef* storeTexture(const TextureRef& textureRef);
	Clay_ElementId toClaySID(std::string_view s);
	Clay_ElementId toClayEID(std::string_view s);
	
	ElementBuilder createElement(std::string_view elementTypeName, std::string_view instanceIdPath);
	ElementBuilder createElement(const ElementDefinition& elementDefinition, std::string_view instanceIdPath);

    const InteractionSnapshot& getPreviousFramesInteraction() const { return previousInteractionSnapshot_; }
	const FrameInput& getCurrentFrameInput() const { return frameInputForCurrentLayout_; }
	const FrameInput& getPreviousFrameInput() const { return previousFrameInputForCurrentLayout_; }
	InputFieldManager& inputFields() { return inputFieldManager_; }
	const InputFieldManager& inputFields() const { return inputFieldManager_; }
	ShortcutManager& shortcuts() { return shortcutManager_; }
	const ShortcutManager& shortcuts() const { return shortcutManager_; }
	void setClipboardText(std::string_view text) const;
	std::string clipboardText() const;
	bool hasClipboardAccess() const;
	void setClipboardAccessors(
		std::function<void(std::string_view)> setClipboardTextAccessor,
		std::function<std::string()> getClipboardTextAccessor);

    void setCurrentInteractionSnapshot(InteractionSnapshot snapshot);
    void advanceFrameInteractionSnapshots();

private:
	friend class App;

	void initStringArenas(const AppConfig& cfg);
	void beginFrame(uint32_t frameIndex, const FrameInput& frameInput, float screenWidth, float screenHeight);
	Clay_RenderCommandArray endFrame();
	void setFontManager(const ::FontManager* fontManager);
	Clay_Dimensions measureText(Clay_StringSlice text, Clay_TextElementConfig* config) const;

	struct Arena {
		std::unique_ptr<char[]> mem;
		size_t capacity = 0;
		size_t offset = 0;
	};
	
	char* allocBytes(size_t nBytes, size_t align = alignof(std::max_align_t));

private:
	
	std::vector<Arena> arenas_;
	uint32_t arenasCount_ = 0;
	uint32_t curArena_ = 0;

	std::unique_ptr<char[]> clayArenaMemory_;
	Clay_Arena clayArena_{};
	Clay_Context* clayContext_ = nullptr;
	FrameInput frameInputForCurrentLayout_{};
	FrameInput previousFrameInputForCurrentLayout_{};
	bool wasPrimaryPointerDownLastFrame_ = false;

	ElementRegistry& elementRegistry_;
	InteractionSnapshot previousInteractionSnapshot_;
    InteractionSnapshot currentInteractionSnapshot_;
	InputFieldManager inputFieldManager_{};
	ShortcutManager shortcutManager_{};
	std::function<void(std::string_view)> setClipboardTextAccessor_{};
	std::function<std::string()> getClipboardTextAccessor_{};
	const ::FontManager* fontManager_ = nullptr;
	float pointsToPixelsScale_ = 96.0f / 72.0f;

};

} //namespace FlowUi
