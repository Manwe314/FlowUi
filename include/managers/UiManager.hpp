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
#include "FlowUi/ResourceKey.hpp"
#include "internal/ElementInstanceKey.hpp"
#include "managers/InputFieldManager.hpp"
#include "managers/PopupManager.hpp"
#include "managers/ShortcutManager.hpp"
#include "managers/ThemeManager.hpp"
#include "managers/structs/ActionManagerStructs.hpp"
#include "managers/structs/FlowUiElementStructs.hpp"
#include "managers/structs/InputStructs.hpp"
#if FLOW_UI_DEV_MODE && FLOWUI_DEV_MEMORY_LEVEL >= 2
#include "devMode/elementDevCapture.hpp"
#include "devMode/devRuntime.hpp"
#include "devMode/performanceDiagnostics.hpp"
#include "internal/FlowUiElementBridge.hpp"
#endif

namespace FlowUi {
namespace devSystems { class MemorySampleSink; }

#if FLOW_UI_DEV_MODE
namespace devSystems { class DevTimingRecorder; }
#endif

struct AppWindow;

class App;
class ElementManager;
class ActionManager;
template <FlowElement Element>
class ElementBuilder;
struct FlowUiTheme;
struct FontManager;
namespace detail::storage { class IStorageSystem; struct FrameToken; }
namespace detail::manager_storage { struct UiManagerState; struct FontFrameView; }

/** @addtogroup flowui_ui_manager
 * @{
 */

/**
 * @brief Frame-scoped UI construction context and service bridge.
 *
 * UiManager owns the Clay layout context used by App during each frame. User
 * code normally reaches it through App::ui(), then uses it to create typed
 * FlowUi elements, convert Flow element ids and dynamic strings into Clay data,
 * query frame input and previous-frame interaction state, and access scoped UI
 * services such as input fields, shortcuts, clipboard helpers, cursor requests,
 * and font resolution.
 *
 * App owns the UiManager lifecycle. beginFrame(), endFrame(), renderer bridge
 * data, clipboard backend wiring, and font-manager wiring are internal to App,
 * so custom elements should treat UiManager as the frame-local authoring
 * surface rather than as a standalone runtime object.
 *
 * @code{.cpp}
 * FlowUi::UiManager& ui = app.ui();
 *
 * ui.createElement(kButton, "save")
 *     .setParameters(ButtonParams{.label = "Save"})
 *     .draw();
 * @endcode
 *
 * @see @ref md_docs_2concepts_2mental__model "Core Mental Model"
 * @see @ref md_docs_2concepts_2element__system "Element System"
 * @see @ref md_docs_2tutorials_2custom__elements "Custom Elements"
 */
class UiManager {
public:
#if FLOW_UI_DEV_MODE
	void appendDevMemorySamples(devSystems::MemorySampleSink& sink) const noexcept;
#endif
	/**
	 * @brief Store a string in the current frame arena and return a Clay string.
	 *
	 * Clay stores string data by pointer, so dynamic strings emitted during UI
	 * construction must live at least until the frame ends. This function copies
	 * the input into UiManager's active frame arena and returns a Clay_String
	 * pointing at that copy.
	 *
	 * @param s UTF-8/string data to copy into the current frame arena.
	 * @return Clay string view backed by UiManager frame storage.
	 *
	 * @throws std::runtime_error if the current frame arena does not have enough
	 * capacity. Increase UiConfig::stringArenaSize.
	 * @throws std::bad_alloc if arena storage allocation has failed earlier.
	 *
	 * @code{.cpp}
	 * CLAY_TEXT(
	 *     context.uiManager.toClayString(context.params.label),
	 *     CLAY_TEXT_CONFIG(textConfig));
	 * @endcode
	 */
	Clay_String toClayString(std::string_view s);
	Clay_String toClayString(ResourceKey key);

	/**
	 * @brief Store a texture reference for use by Clay image render data.
	 *
	 * Clay image commands carry an opaque pointer. This helper copies TextureRef
	 * into the current frame arena and returns a pointer suitable for
	 * Clay_ImageElementConfig::imageData.
	 *
	 * @param textureRef Texture request or registered texture handle to store.
	 * @return Pointer to the frame-arena copy of textureRef.
	 *
	 * @throws std::runtime_error if the current frame arena does not have enough
	 * capacity. Increase UiConfig::stringArenaSize.
	 * 
	 * @note This function should be used as .imageData instead of raw variable address
	 *
	 * @code{.cpp}
	 * Clay_ImageElementConfig image{};
	 * image.imageData = context.uiManager.imageData(textureRef);
	 * @endcode
	 */
	TextureRef* imageData(TextureRef textureRef);

	[[deprecated("Use imageData(TextureRef); this copies Clay payload bytes, not texture ownership.")]]
	TextureRef* storeTexture(const TextureRef& textureRef);

	/**
	 * @brief Create a stable inner input-field content element declaration.
	 *
	 * This helper returns a Clay element declaration intended for the inner
	 * content node of an input field. The width grows to available space and the
	 * height is fixed to the resolved font line height for textConfig. Use it
	 * between the visual/padded outer input box and the Clay text element so an
	 * empty field keeps a stable layout area for caret rendering.
	 *
	 * @param textConfig Clay text configuration that will be used for the field text.
	 * @return Clay element declaration with grow width and fixed line-height height.
	 *
	 * @code{.cpp}
	 * CLAY(outerId, outer) {
	 *     CLAY(contentId, context.uiManager.inputContentElement(textConfig)) {
	 *         CLAY(textId, {}) {
	 *             CLAY_TEXT(
	 *                 context.uiManager.toClayString(field.text),
	 *                 CLAY_TEXT_CONFIG(textConfig));
	 *         }
	 *     }
	 * }
	 * @endcode
	 */
	Clay_ElementDeclaration inputContentElement(const Clay_TextElementConfig& textConfig) const;

	/**
	 * @brief Convert a string id to a Clay string id.
	 *
	 * This is a convenience wrapper around CLAY_SID using UiManager frame string
	 * storage. Use it when Clay's string-id path is desired for the current
	 * frame.
	 *
	 * @param s Element id string.
	 * @return Clay element id generated from s.
	 *
	 * @throws std::runtime_error if the current frame arena does not have enough
	 * capacity.
	 *
	 * @code{.cpp}
	 * const Clay_ElementId overlayId = ui.toClaySID("overlay/root");
	 * @endcode
	 */
	Clay_ElementId toClaySID(std::string_view s);
	Clay_ElementId toClaySID(ResourceKey key);

	/**
	 * @brief Deterministically bridge a strong 64-bit Flow identity to Clay.
	 *
	 * Production builds construct the numeric Clay id without allocating or
	 * hashing strings. Developer builds attach the ID's diagnostic name when it
	 * is available. Use toClaySID() for explicitly string-named Clay-only nodes.
	 *
	 * @code{.cpp}
	 * const Clay_ElementId rootId = context.uiManager.toClayEID(context.id);
	 * @endcode
	 */
	Clay_ElementId toClayEID(FlowElementID id);
	Clay_ElementId toClayEID(GlobalFlowID id);
	Clay_ElementId toClayEID(FlowElementPartID id);

	/**
	 * @brief Create a builder for a typed FlowUi element instance.
	 *
	 * createElement() is the public entry point for invoking FlowUi element
	 * definitions. The returned ElementBuilder owns the resolved element ID and parameter
	 * storage for this invocation until draw() or construct() is called.
	 *
	 * @tparam Element Empty tag type satisfying FlowElement.
	 * @param element Compile-time element tag used only for type deduction.
	 * @param localName Name unique among siblings of the same element definition.
	 * @param sourceLocation Source location captured for developer-mode
	 * inspection when FLOW_UI_DEV_MODE is enabled.
	 * @return ElementBuilder configured for the passed definition and element id.
	 *
	 * @code{.cpp}
	 * app.ui()
	 *     .createElement(kButton, "save")
	 *     .setParameters(ButtonParams{.label = "Save"})
	 *     .draw();
	 * @endcode
	 *
	 * @see @ref md_docs_2tutorials_2custom__elements "Custom Elements"
	 * @see @ref md_docs_2tutorials_2developer__mode "Developer Mode"
	 */
	template <FlowElement Element>
	ElementBuilder<Element> createElement(
		const Element&,
		LocalElementName localName
#if FLOW_UI_DEV_MODE
		, devMode::elementCapture::SourceLocation sourceLocation = devMode::elementCapture::SourceLocation::current()
#endif
		)
	{
		const FlowElementID parentId = currentFlowScope();
		return ElementBuilder<Element>(
			*this,
			elements(),
			windowId(),
			parentId,
			resolveLocalElementID(parentId, Element::definitionId, localName)
#if FLOW_UI_DEV_MODE
			, sourceLocation
#endif
		);
	}

	template <FlowElement Element>
	ElementBuilder<Element> createElement(
		const Element&,
		RuntimeElementName localName
#if FLOW_UI_DEV_MODE
		, devMode::elementCapture::SourceLocation sourceLocation = devMode::elementCapture::SourceLocation::current()
#endif
		) {
		const FlowElementID parentId = currentFlowScope();
		return ElementBuilder<Element>(
			*this,
			elements(),
			windowId(),
			parentId,
			resolveLocalElementID(parentId, Element::definitionId, localName)
#if FLOW_UI_DEV_MODE
			, sourceLocation
#endif
		);
	}

	/** Create a repeated child using a positional or stable numeric key. */
	template <FlowElement Element>
	ElementBuilder<Element> createElement(
		const Element&,
		IndexedElementName indexedName
#if FLOW_UI_DEV_MODE
		, devMode::elementCapture::SourceLocation sourceLocation = devMode::elementCapture::SourceLocation::current()
#endif
		) {
		const FlowElementID parentId = currentFlowScope();
		return ElementBuilder<Element>(
			*this,
			elements(),
			windowId(),
			parentId,
			resolveIndexedElementID(parentId, Element::definitionId, indexedName)
#if FLOW_UI_DEV_MODE
			, sourceLocation
#endif
		);
	}

	/**
	 * Create a positional element identified by this callsite.
	 *
	 * Automatic IDs are appropriate only for stable static trees. A callsite
	 * executed repeatedly must use Indexed(), Keyed(), or indexedIDs().next().
	 */
	template <FlowElement Element>
	ElementBuilder<Element> createElement(
		const Element&,
		AutoElementName automaticName = AutoID()
#if FLOW_UI_DEV_MODE
		, devMode::elementCapture::SourceLocation sourceLocation = devMode::elementCapture::SourceLocation::current()
#endif
		) {
		const FlowElementID parentId = currentFlowScope();
		return ElementBuilder<Element>(
			*this,
			elements(),
			windowId(),
			parentId,
			resolveAutomaticElementID(parentId, Element::definitionId, automaticName)
#if FLOW_UI_DEV_MODE
			, sourceLocation, true
#endif
		);
	}

	/** Create an explicitly global element without applying the local parent hash. */
	template <FlowElement Element>
	ElementBuilder<Element> createElement(
		const Element&,
		GlobalFlowID globalId
#if FLOW_UI_DEV_MODE
		, devMode::elementCapture::SourceLocation sourceLocation = devMode::elementCapture::SourceLocation::current()
#endif
		) {
		if (!globalId) {
			throw FlowUiException(makeError(ErrorCode::InvalidElementId, ErrorSite::UiManagerDefineElement));
		}
		return ElementBuilder<Element>(
			*this,
			elements(),
			windowId(),
			currentFlowScope(),
			normalizeGlobalElementID(globalId)
#if FLOW_UI_DEV_MODE
			, sourceLocation
#endif
		);
	}

	template <FlowElement Element>
	ElementBuilder<Element> createElement(
		const Element&,
		FlowElementID resolvedId
#if FLOW_UI_DEV_MODE
		, devMode::elementCapture::SourceLocation sourceLocation = devMode::elementCapture::SourceLocation::current()
#endif
		) {
		if (!resolvedId) {
			throw FlowUiException(makeError(ErrorCode::InvalidElementId, ErrorSite::UiManagerDefineElement));
		}
		return ElementBuilder<Element>(
			*this,
			elements(),
			windowId(),
			currentFlowScope(),
			resolvedId
#if FLOW_UI_DEV_MODE
			, sourceLocation
#endif
		);
	}

	/** Create an element at a semantic part address already bound to its owner. */
	template <FlowElement Element>
	ElementBuilder<Element> createElement(
		const Element&,
		FlowElementPartID partId
#if FLOW_UI_DEV_MODE
		, devMode::elementCapture::SourceLocation sourceLocation = devMode::elementCapture::SourceLocation::current()
#endif
		) {
		if (!partId) {
			throw FlowUiException(makeError(ErrorCode::InvalidElementId, ErrorSite::UiManagerDefineElement));
		}
		return ElementBuilder<Element>(
			*this,
			elements(),
			windowId(),
			currentFlowScope(),
			normalizePartElementID(partId)
#if FLOW_UI_DEV_MODE
			, sourceLocation
#endif
		);
	}

	/**
	 * @brief Close the current element opened by ElementBuilder::construct().
	 *
	 * Call this after emitting child Clay nodes for an element opened through
	 * createElement(...).construct(). drawConstructed() closes that constructed
	 * root and ends any active dev-mode capture for it.
	 *
	 * @throws std::runtime_error if there is no active constructed element or if
	 * the Clay context is not initialized.
	 *
	 * @code{.cpp}
	 * ui.createElement(kPanel, "settings").construct();
	 * CLAY(ui.toClaySID("body"), {}) {}
	 * ui.drawConstructed();
	 * @endcode
	 */
	void drawConstructed();

	/**
	 * @brief Return the previous completed frame's interaction snapshot.
	 *
	 * Element callbacks use the previous snapshot while constructing the current
	 * frame so hover, press, hold, and release queries are stable.
	 *
	 * @return Interaction data collected when the previous frame ended.
	 *
	 * @code{.cpp}
	 * if (context.uiManager.getPreviousFramesInteraction().isHovered(rootId)) {
	 *     context.uiManager.requestCursor(FlowUi::CursorType::PointingHand);
	 * }
	 * @endcode
	 */
    const InteractionSnapshot& getPreviousFramesInteraction() const;

	/**
	 * @brief Return input for the current layout frame.
	 *
	 * Coordinates are in FlowUi layout space after App has applied UI scaling.
	 *
	 * @return Current frame input used by UiManager during this layout pass.
	 *
	 * @code{.cpp}
	 * const FrameInput& input = context.uiManager.getCurrentFrameInput();
	 * const float mouseX = input.mouseX;
	 * @endcode
	 */
	const FrameInput& getCurrentFrameInput() const;

	/**
	 * @brief Return input for the previous layout frame.
	 *
	 * Use this with getCurrentFrameInput() for custom edge detection or drag
	 * calculations.
	 *
	 * @return Previous frame input in FlowUi layout space.
	 *
	 * @code{.cpp}
	 * const bool pressed =
	 *     context.uiManager.getCurrentFrameInput().mouseDown[0] &&
	 *     !context.uiManager.getPreviousFrameInput().mouseDown[0];
	 * @endcode
	 */
	const FrameInput& getPreviousFrameInput() const;

	/**
	 * @brief Access the input field manager.
	 *
	 * @return Mutable InputFieldManager owned by this UiManager.
	 *
	 * @code{.cpp}
	 * context.uiManager.inputFields().requestCaret(
	 *     fieldId,
	 *     FlowUi::CaretRequestKind::SetPrimary);
	 * @endcode
	 */
	InputFieldManager& inputFields() { return inputFieldManager_; }

	/**
	 * @brief Access the input field manager.
	 *
	 * @return Immutable InputFieldManager owned by this UiManager.
	 */
	const InputFieldManager& inputFields() const { return inputFieldManager_; }

	/**
	 * @brief Access the shortcut manager.
	 *
	 * @return Mutable ShortcutManager owned by this UiManager.
	 *
	 * @code{.cpp}
	 * const FlowUi::ShortcutId id = app.ui().shortcuts().registerShortcut(
	 *     chord,
	 *     FlowUi::ShortcutScope::Global,
	 *     0,
	 *     callback);
	 * @endcode
	 */
	ShortcutManager& shortcuts() { return shortcutManager_; }

	/**
	 * @brief Access the shortcut manager.
	 *
	 * @return Immutable ShortcutManager owned by this UiManager.
	 */
	const ShortcutManager& shortcuts() const { return shortcutManager_; }

	/** @brief Access this window's mutable popup placement and dismissal service. */
	PopupManager& popups() { return popupManager_; }
	/** @brief Access this window's immutable popup placement and dismissal service. */
	const PopupManager& popups() const { return popupManager_; }

	/** @brief Access the app-wide ActionManager attached to this UI. */
	[[nodiscard]] ActionManager& actions();
	/** @brief Access the immutable app-wide ActionManager attached to this UI. */
	[[nodiscard]] const ActionManager& actions() const;
	/** @brief Invoke an action from this window's UI context. */
	ActionInvocationStatus invoke(ActionCall call);
#if FLOW_UI_DEV_MODE
	/**
	 * @brief Access the developer runtime.
	 *
	 * @return Mutable dev-mode runtime for the active UiManager.
	 */
	devMode::DevRuntime& devRuntime();

	/**
	 * @brief Access the developer runtime.
	 *
	 * @return Immutable dev-mode runtime for the active UiManager.
	 */
	const devMode::DevRuntime& devRuntime() const;

	/**
	 * @brief Access developer tooling config.
	 *
	 * @return Mutable developer tooling configuration.
	 */
	DevToolsConfig& devToolsConfig();

	/**
	 * @brief Access developer tooling config.
	 *
	 * @return Immutable developer tooling configuration.
	 */
	const DevToolsConfig& devToolsConfig() const;

	/**
	 * @brief Access developer-mode performance diagnostics.
	 *
	 * @return Mutable rolling performance diagnostics for the active UiManager.
	 */
	devMode::PerformanceDiagnostics& performanceDiagnostics();

	/**
	 * @brief Access developer-mode performance diagnostics.
	 *
	 * @return Immutable rolling performance diagnostics for the active UiManager.
	 */
	const devMode::PerformanceDiagnostics& performanceDiagnostics() const;
#endif

	/**
	 * @brief Set clipboard text through the configured clipboard accessor.
	 *
	 * If no clipboard setter is installed, this function does nothing.
	 *
	 * @param text Text to copy to the system or configured clipboard.
	 *
	 * @throws Any exception thrown by the configured clipboard setter.
	 *
	 * @code{.cpp}
	 * context.uiManager.setClipboardText("Copied text");
	 * @endcode
	 */
	void setClipboardText(std::string_view text) const;

	/**
	 * @brief Read clipboard text through the configured clipboard accessor.
	 *
	 * @return Clipboard text, or an empty string when no getter is installed.
	 *
	 * @throws Any exception thrown by the configured clipboard getter.
	 *
	 * @code{.cpp}
	 * const std::string pasted = context.uiManager.clipboardText();
	 * @endcode
	 */
	std::string clipboardText() const;

	/**
	 * @brief Return whether clipboard accessors are installed.
	 *
	 * @retval true Both clipboard setter and getter callbacks are installed.
	 * @retval false At least one clipboard callback is missing.
	 *
	 * @code{.cpp}
	 * if (context.uiManager.hasClipboardAccess()) {
	 *     context.uiManager.setClipboardText(selectedText);
	 * }
	 * @endcode
	 */
	bool hasClipboardAccess() const;

	/**
	 * @brief Request a cursor shape for the current frame.
	 *
	 * Cursor requests are reset at the beginning of each frame. When multiple
	 * requests are made in one frame, the request with the highest priority wins;
	 * equal priority requests may replace earlier requests.
	 *
	 * @param cursorType Cursor shape requested by UI code.
	 * @param priority Ordering priority for this frame's cursor request.
	 *
	 * @code{.cpp}
	 * context.uiManager.requestCursor(FlowUi::CursorType::IBeam, 10);
	 * @endcode
	 */
	void requestCursor(CursorType cursorType, uint8_t priority = 0);

	/**
	 * @brief Resolve a concrete Clay font id for a family/style request.
	 *
	 * This forwards to FlowUi::FontManager when the App-owned font manager is connected
	 * to UiManager. It is useful in element callbacks that receive UiManager but
	 * not FlowUi::FontManager directly.
	 *
	 * @param familyId Existing logical font family id.
	 * @param weight Requested CSS-style font weight.
	 * @param style Requested font style.
	 * @return Concrete FontId for Clay text, or 0 if no font manager is attached
	 * or the family cannot be resolved.
	 *
	 * @code{.cpp}
	 * textConfig.fontId = context.uiManager.resolveFont(bodyFamily, 700);
	 * @endcode
	 */
	FontId resolveFont(FontFamilyId familyId, uint32_t weight = 400, FontStyle style = FontStyle::Normal) const;

	/**
	 * @brief Resolve a concrete Clay font id for a named family/style request.
	 *
	 * This forwards to FlowUi::FontManager when available and returns 0 when the family
	 * name cannot be resolved.
	 *
	 * @param familyName Existing logical font family name string.
	 * @param weight Requested CSS-style font weight.
	 * @param style Requested font style.
	 * @return Concrete FontId for Clay text, or 0 if no font manager is attached
	 * or the family cannot be resolved.
	 *
	 * @code{.cpp}
	 * textConfig.fontId = context.uiManager.resolveFont(
	 *     "Body",
	 *     400,
	 *     FlowUi::FontStyle::Normal);
	 * @endcode
	 */
	FontId resolveFont(std::string_view familyName, uint32_t weight = 400, FontStyle style = FontStyle::Normal) const;

	/**
	 * @brief Access the active variant for theme type T.
	 *
	 * @tparam T Theme struct type.
	 * @return Const reference to active theme instance of type T.
	 * @throws std::runtime_error if ThemeManager is not connected or theme is unregistered.
	 */
	template <typename T>
	[[nodiscard]] const T& theme() const {
		return appThemes().template getActiveTheme<T>();
	}

	/**
	 * @brief Access a specific named variant for theme type T.
	 *
	 * @tparam T Theme struct type.
	 * @param variantName Name of the theme variant.
	 * @return Const reference to named theme instance of type T.
	 * @throws std::runtime_error if ThemeManager is not connected or variant is unregistered.
	 */
	template <typename T>
	[[nodiscard]] const T& theme(std::string_view variantName) const {
		return appThemes().template getTheme<T>(variantName);
	}

	/**
	 * @brief Convenience shortcut for active built-in FlowUiTheme.
	 *
	 * @return Const reference to active FlowUiTheme.
	 */
	[[nodiscard]] const FlowUiTheme& flowTheme() const;

	/** @brief Access the app-owned element state/resource manager. */
	[[nodiscard]] ElementManager& elements();

	/** @brief Access the app-owned element state/resource manager. */
	[[nodiscard]] const ElementManager& elements() const;

	/** @brief Return the window whose frame this UI manager builds. */
	[[nodiscard]] WindowId windowId() const noexcept { return window_; }

private:
	friend class App;
	friend class ElementManager;
	friend class ActionManager;
	friend struct AppWindow;
	template <FlowElement Element>
	friend class ElementBuilder;
	template <typename Element>
	friend struct ElementBuildContext;
	template <typename Element>
	friend struct ElementInteractionContext;
#if FLOW_UI_DEV_MODE
	// Internal builder bridge records frame-local identity diagnostics.
	friend void detail::claimFlowRootForDev(
		UiManager& uiManager,
		FlowElementID elementId,
		FlowDefinitionID definitionId,
		std::string_view fileName,
		uint32_t line,
		uint32_t column,
		std::string_view functionName,
		bool automaticIdentity);
	void claimClayBridgeForDev(
		detail::element::ElementInstanceKey instanceId,
		std::string_view debugName);
#endif

	UiManager() = default;
	void initStorage(detail::storage::IStorageSystem& storage, WindowId window, const AppConfig& config);
	void destroyStorage() noexcept;
	void setThemeManager(const ThemeManager* themeManager) noexcept { themeManager_ = themeManager; }
	void setElementManager(ElementManager* elementManager) noexcept { elementManager_ = elementManager; }
	void setActionManager(ActionManager* actionManager) noexcept { actionManager_ = actionManager; }
#if FLOW_UI_DEV_MODE
	void setDevTimingRecorder(devSystems::DevTimingRecorder* recorder) noexcept {
		devTimingRecorder_ = recorder;
	}
	[[nodiscard]] devSystems::DevTimingRecorder* devTimingRecorder() const noexcept {
		return devTimingRecorder_;
	}
#endif
	const ThemeManager& appThemes() const;

	void beginFrame(
		const detail::storage::FrameToken& frame,
		const FrameInput& frameInput,
		const detail::manager_storage::FontFrameView& fontView,
		float screenWidth,
		float screenHeight);
	Clay_RenderCommandArray endFrame();
	const detail::InputFieldFrameOverrides& inputFieldFrameOverrides() const { return inputFieldManager_.frameOverrides(); }
	void setCursorAccessor(std::function<void(CursorType)> setCursorTypeAccessor);
	void setClipboardAccessors(
		std::function<void(std::string_view)> setClipboardTextAccessor,
		std::function<std::string()> getClipboardTextAccessor);
	void advanceFrameInteractionSnapshots();
	void cancelFrameState() noexcept;
	Clay_Dimensions measureText(Clay_StringSlice text, Clay_TextElementConfig* config) const;
	[[nodiscard]] FlowElementID currentFlowScope() const noexcept;
	[[nodiscard]] FlowElementID resolveLocalElementID(
		FlowElementID parent,
		FlowDefinitionID definition,
		LocalElementName name);
	[[nodiscard]] FlowElementID resolveLocalElementID(
		FlowElementID parent,
		FlowDefinitionID definition,
		RuntimeElementName name);
	[[nodiscard]] FlowElementID resolveIndexedElementID(
		FlowElementID parent,
		FlowDefinitionID definition,
		IndexedElementName name);
	[[nodiscard]] FlowElementID resolveAutomaticElementID(
		FlowElementID parent,
		FlowDefinitionID definition,
		AutoElementName name);
	[[nodiscard]] FlowElementID normalizeGlobalElementID(GlobalFlowID id);
	[[nodiscard]] FlowElementID normalizePartElementID(FlowElementPartID id) const noexcept;
	[[nodiscard]] FlowElementPartID resolveElementPartID(
		FlowDefinitionID ownerDefinition,
		FlowElementID owner,
		FlowElementPart part);
	[[nodiscard]] size_t pushFlowScope(FlowElementID id);
	void restoreFlowScope(size_t depth) noexcept;
	[[nodiscard]] size_t constructedElementDepth() const noexcept;
	void closeConstructedToDepth(size_t depth, bool warn) noexcept;
	void retainConstructedElement(
		Clay_ElementId clayId,
		FlowElementID flowId,
		size_t priorFlowScopeDepth
#if FLOW_UI_DEV_MODE && FLOWUI_DEV_TIMING_LEVEL >= 2
		, FlowDefinitionID definitionId
#endif
		);
#if FLOW_UI_DEV_MODE
	[[nodiscard]] std::string_view joinFlowDebugPath(
		std::string_view parent,
		std::string_view child);
	[[nodiscard]] std::string_view indexedFlowDebugName(IndexedElementName name);
	[[nodiscard]] std::string_view automaticFlowDebugName(AutoElementName name);
	[[nodiscard]] std::string_view globalFlowDebugName(GlobalFlowID id);
#endif

	char* allocBytes(size_t nBytes, size_t align = alignof(std::max_align_t));
	std::string_view normalizeUiResourceName(ResourceKey key) const;

private:
	
	InputFieldManager inputFieldManager_{};
	PopupManager popupManager_{};
	ShortcutManager shortcutManager_{};
	std::function<void(std::string_view)> setClipboardTextAccessor_{};
	std::function<std::string()> getClipboardTextAccessor_{};
	std::function<void(CursorType)> setCursorTypeAccessor_{};
	detail::storage::IStorageSystem* storage_ = nullptr;
	const ThemeManager* themeManager_ = nullptr;
	ElementManager* elementManager_ = nullptr;
	ActionManager* actionManager_ = nullptr;
#if FLOW_UI_DEV_MODE
	devSystems::DevTimingRecorder* devTimingRecorder_ = nullptr;
#endif
	WindowId window_ = InvalidWindowId;
	uint64_t stateHandle_ = 0;
	detail::manager_storage::UiManagerState* state_ = nullptr;

};

/** @} */

} //namespace FlowUi

#include "managers/FlowUiElementBuilder.hpp"
