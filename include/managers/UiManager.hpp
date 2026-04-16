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

#include "FlowUi/BuildConfig.hpp"
#include "FlowUi/PublicStructs.hpp"
#include "managers/FlowUiElementSystem.hpp"
#include "managers/InputFieldManager.hpp"
#include "managers/ShortcutManager.hpp"
#include "window/Inputs.hpp"
#if FLOW_UI_DEV_MODE
#include "devMode/devRuntime.hpp"
#endif

struct FontManager;

namespace FlowUi {

class App;

class UiManager {
public:
	UiManager(const AppConfig& appConfig);

	Clay_String toClayString(std::string_view s);
	TextureRef* storeTexture(const TextureRef& textureRef);
	Clay_ElementId toClaySID(std::string_view s);
	Clay_ElementId toClayEID(std::string_view s);

	template <typename Parameters, typename State, typename Resources, uint64_t DefinitionId, bool IsDevInternal>
	ElementBuilder<Parameters, State, Resources, DefinitionId, IsDevInternal> createElement(
		const ElementDefinition<Parameters, State, Resources, DefinitionId, IsDevInternal>& elementDefinition,
		std::string_view elementID
#if FLOW_UI_DEV_MODE
		, devCaptureDetail::DevSourceLocation sourceLocation = devCaptureDetail::DevSourceLocation::current()
#endif
		)
	{
		return ElementBuilder<Parameters, State, Resources, DefinitionId, IsDevInternal>(
			*this,
			&elementDefinition,
			std::string(elementID)
#if FLOW_UI_DEV_MODE
			, sourceLocation
#endif
		);
	}
	void drawConstructed();

    const InteractionSnapshot& getPreviousFramesInteraction() const { return previousInteractionSnapshot_; }
	const FrameInput& getCurrentFrameInput() const { return frameInputForCurrentLayout_; }
	const FrameInput& getPreviousFrameInput() const { return previousFrameInputForCurrentLayout_; }
	InputFieldManager& inputFields() { return inputFieldManager_; }
	const InputFieldManager& inputFields() const { return inputFieldManager_; }
	ShortcutManager& shortcuts() { return shortcutManager_; }
	const ShortcutManager& shortcuts() const { return shortcutManager_; }
#if FLOW_UI_DEV_MODE
	devMode::DevRuntime& devRuntime() { return devRuntime_; }
	const devMode::DevRuntime& devRuntime() const { return devRuntime_; }
	DevToolsConfig& devToolsConfig() { return devToolsConfig_; }
	const DevToolsConfig& devToolsConfig() const { return devToolsConfig_; }
#endif
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
	template <typename Parameters, typename State, typename Resources, uint64_t DefinitionId, bool IsDevInternal>
	friend class ElementBuilder;
	friend void flowUiPushConstructedElement(UiManager& uiManager, Clay_ElementId elementId);

	void initStringArenas(const AppConfig& cfg);
	void beginFrame(uint32_t frameIndex, const FrameInput& frameInput, float screenWidth, float screenHeight);
	Clay_RenderCommandArray endFrame();
	void setFontManager(const ::FontManager* fontManager);
	Clay_Dimensions measureText(Clay_StringSlice text, Clay_TextElementConfig* config) const;
	void pushConstructedElement(Clay_ElementId elementId);

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

	InteractionSnapshot previousInteractionSnapshot_;
	InteractionSnapshot currentInteractionSnapshot_;
	std::vector<Clay_ElementId> constructedElementStack_;
	InputFieldManager inputFieldManager_{};
	ShortcutManager shortcutManager_{};
#if FLOW_UI_DEV_MODE
	devMode::DevRuntime devRuntime_{};
	DevToolsConfig devToolsConfig_{};
	bool devPanelVisible_ = false;
	bool devRootElementOpenThisFrame_ = false;
	ShortcutId devPanelToggleShortcutId_ = 0u;
#endif
	std::function<void(std::string_view)> setClipboardTextAccessor_{};
	std::function<std::string()> getClipboardTextAccessor_{};
	const ::FontManager* fontManager_ = nullptr;
	float pointsToPixelsScale_ = 96.0f / 72.0f;

};

} //namespace FlowUi
