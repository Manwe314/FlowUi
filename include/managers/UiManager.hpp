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
#include "internal/FlowUiElementBridge.hpp"
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

/** @addtogroup flowui_ui_manager
 * @{
 */

/** @brief Bridges FlowUi application code with Clay layout and per-frame UI services. */
class UiManager {
public:
	/** @brief Construct a UI manager from app configuration. */
	UiManager(const AppConfig& appConfig);

	/** @brief Store a string in the current frame arena and return a Clay string. */
	Clay_String toClayString(std::string_view s);
	/** @brief Store a texture reference for use by Clay render data. */
	TextureRef* storeTexture(const TextureRef& textureRef);
	/** @brief Convert a string id to a Clay string id. */
	Clay_ElementId toClaySID(std::string_view s);
	/** @brief Convert an element id string to a Clay element id. */
	Clay_ElementId toClayEID(std::string_view s);

	/** @brief Create a builder for a typed FlowUi element instance. */
	template <typename Parameters, typename State, typename Resources, uint64_t DefinitionId, bool IsDevInternal>
	ElementBuilder<Parameters, State, Resources, DefinitionId, IsDevInternal> createElement(
		const ElementDefinition<Parameters, State, Resources, DefinitionId, IsDevInternal>& elementDefinition,
		std::string_view elementID
#if FLOW_UI_DEV_MODE
		, devMode::elementCapture::SourceLocation sourceLocation = devMode::elementCapture::SourceLocation::current()
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
	/** @brief Draw the current constructed element stack. */
	void drawConstructed();

	/** @brief Return the previous frame's interaction snapshot. */
    const InteractionSnapshot& getPreviousFramesInteraction() const { return previousInteractionSnapshot_; }
	/** @brief Return input for the current layout frame. */
	const FrameInput& getCurrentFrameInput() const { return frameInputForCurrentLayout_; }
	/** @brief Return input for the previous layout frame. */
	const FrameInput& getPreviousFrameInput() const { return previousFrameInputForCurrentLayout_; }
	/** @brief Access the input field manager. */
	InputFieldManager& inputFields() { return inputFieldManager_; }
	/** @brief Access the input field manager. */
	const InputFieldManager& inputFields() const { return inputFieldManager_; }
	/** @brief Access the shortcut manager. */
	ShortcutManager& shortcuts() { return shortcutManager_; }
	/** @brief Access the shortcut manager. */
	const ShortcutManager& shortcuts() const { return shortcutManager_; }
#if FLOW_UI_DEV_MODE
	/** @brief Access the developer runtime. */
	devMode::DevRuntime& devRuntime() { return devRuntime_; }
	/** @brief Access the developer runtime. */
	const devMode::DevRuntime& devRuntime() const { return devRuntime_; }
	/** @brief Access developer tooling config. */
	DevToolsConfig& devToolsConfig() { return devToolsConfig_; }
	/** @brief Access developer tooling config. */
	const DevToolsConfig& devToolsConfig() const { return devToolsConfig_; }
#endif
	/** @brief Set clipboard text through configured clipboard accessors. */
	void setClipboardText(std::string_view text) const;
	/** @brief Read clipboard text through configured clipboard accessors. */
	std::string clipboardText() const;
	/** @brief Return true if clipboard accessors are installed. */
	bool hasClipboardAccess() const;
	/** @brief Request a cursor shape for the current frame. */
	void requestCursor(CursorType cursorType, uint8_t priority = 0);
	/** @brief Resolve a concrete Clay font id for a family/style request. */
	FontId resolveFont(FontFamilyId familyId, uint32_t weight = 400, FontStyle style = FontStyle::Normal) const;
	/** @brief Resolve a concrete Clay font id for a named family/style request. */
	FontId resolveFont(std::string_view familyName, uint32_t weight = 400, FontStyle style = FontStyle::Normal) const;
	/** @brief Set clipboard access callbacks. */
	void setClipboardAccessors(
		std::function<void(std::string_view)> setClipboardTextAccessor,
		std::function<std::string()> getClipboardTextAccessor);

	/** @brief Set the current interaction snapshot. */
    void setCurrentInteractionSnapshot(InteractionSnapshot snapshot);
	/** @brief Advance stored interaction snapshots by one frame. */
    void advanceFrameInteractionSnapshots();

private:
	friend class App;
	template <typename Parameters, typename State, typename Resources, uint64_t DefinitionId, bool IsDevInternal>
	friend class ElementBuilder;
	friend void detail::pushConstructedElement(UiManager& uiManager, Clay_ElementId elementId);

	void initStringArenas(const AppConfig& cfg);
	void beginFrame(uint32_t frameIndex, const FrameInput& frameInput, float screenWidth, float screenHeight);
	Clay_RenderCommandArray endFrame();
	void setFontManager(const ::FontManager* fontManager);
	void setCursorAccessor(std::function<void(CursorType)> setCursorTypeAccessor);
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
	std::function<void(CursorType)> setCursorTypeAccessor_{};
	CursorType cursor_ = CursorType::Arrow;
	CursorType previousCursor_ = CursorType::Arrow;
	uint8_t cursorPriority_ = 0;
	const ::FontManager* fontManager_ = nullptr;
	float pointsToPixelsScale_ = 96.0f / 72.0f;

};

/** @} */

} //namespace FlowUi
