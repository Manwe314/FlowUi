#pragma once

#include <clay.h>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <memory>
#include <source_location>
#include <stdexcept>
#include <string>
#include <string_view>
#include <vector>

#include "FlowUi/Public/BuildConfig.hpp"
#include "FlowUi/Public/FlowUiTheme.hpp"
#include "FlowUi/Public/TextureRef.hpp"
#include "FlowUi/Public/configurationStructs/AppConfig.hpp"
#include "FlowUi/Public/configurationStructs/DevToolsConfig.hpp"
#include "FlowUi/Public/configurationStructs/UiConfig.hpp"
#include "FlowUi/Public/errorManagement/ErrorCodeTypes.hpp"
#include "FlowUi/Public/errorManagement/ErrorSites.hpp"
#include "FlowUi/Public/errorManagement/ErrorValue.hpp"
#include "FlowUi/Public/errorManagement/Exception.hpp"
#include "FlowUi/Public/identificationTypes/ElementID.hpp"
#include "FlowUi/Public/identificationTypes/WindowId.hpp"
#include "FlowUi/Public/managerStructs/FontTypes.hpp"
#include "FlowUi/Public/identificationTypes/ResourceKey.hpp"
#include "FlowUi/Private/elementSystem/ElementInstanceKey.hpp"
#if FLOW_UI_DEV_MODE
#include "devSystems/devTooling/override/DevOverrideEngine.hpp"
#include "devSystems/devTooling/tree/DevTreeCapture.hpp"
#include "devSystems/devTooling/schema/DevSchemaRegistry.hpp"
#endif
#include "FlowUi/Public/managers/InputFieldManager.hpp"
#include "FlowUi/Public/managers/PopupManager.hpp"
#include "FlowUi/Public/managers/ShortcutManager.hpp"
#include "FlowUi/Public/managers/ThemeManager.hpp"
#include "FlowUi/Public/managerStructs/ActionTypes.hpp"
#include "FlowUi/Public/elementAuthoring/ElementContexts.hpp"
#include "FlowUi/Public/managerStructs/InputTypes.hpp"
#if FLOW_UI_DEV_MODE
#include "FlowUi/Private/elementSystem/FlowUiElementBridge.hpp"
#endif

namespace FlowUi {
namespace devSystems { class MemorySampleSink; }

#if FLOW_UI_DEV_MODE
namespace devSystems {
class DevTimingRecorder;
namespace interface_elements {
class DevTimelineController;
class DevMemoryHistoryController;
}
} // namespace devSystems
#endif

struct AppWindow;

class App;
class ElementManager;
class ActionManager;
class ImageManager;
class IconManager;
template <FlowElement Element>
class ElementBuilder;
struct FlowUiTheme;
struct FontManager;
namespace detail::platform {
class WindowPlatformServices;
}
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
 * ui.createElement(FlowUi::kButton, FlowUi::LocalElementName{"save"})
 *     .setParameters(FlowUi::ButtonParameters{.label = "Save"})
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
	/** @brief Append development memory observations without taking ownership of the sink. */
	void appendDevMemorySamples(devSystems::MemorySampleSink& sink) const noexcept;
	/** @brief Borrow the connected mutable development schema registry, or nullptr. */
	[[nodiscard]] devSystems::tooling::DevSchemaRegistry* devSchemaRegistry() noexcept {
		return devSchemaRegistry_;
	}
	/** @brief Borrow the connected immutable development schema registry, or nullptr. */
	[[nodiscard]] const devSystems::tooling::DevSchemaRegistry* devSchemaRegistry() const noexcept {
		return devSchemaRegistry_;
	}
	/** Latest successfully completed Flow/Clay developer tree snapshot. */
	[[nodiscard]] const devSystems::tooling::DevTreeSnapshot& devTreeSnapshot() const noexcept;
	/** @brief Borrow this window's development tree capture service. */
	[[nodiscard]] devSystems::tooling::DevTreeCapture& devTreeCapture() noexcept;
	/** @brief Borrow this window's immutable development tree capture service. */
	[[nodiscard]] const devSystems::tooling::DevTreeCapture& devTreeCapture() const noexcept;
	/** Supply the authoritative App key before building a window frame. */
	void set_dev_evidence_context(uint64_t app_tick, uint64_t window_frame) noexcept {
		dev_evidence_tick_ = app_tick;
		dev_evidence_frame_ = window_frame;
	}
	/** Gate optional inspection copies while keeping layout, state and overrides active. */
	void set_dev_tree_collection(bool enabled) noexcept { dev_tree_collection_ = enabled; }
	/** @brief Return whether optional development-tree inspection copies are enabled. */
	[[nodiscard]] bool dev_tree_collection() const noexcept { return dev_tree_collection_; }
	/** @brief Borrow the connected mutable development override engine, or nullptr. */
	[[nodiscard]] devSystems::tooling::DevOverrideEngine* devOverrideEngine() noexcept {
		return devOverrideEngine_;
	}
	/** @brief Borrow the connected immutable development override engine, or nullptr. */
	[[nodiscard]] const devSystems::tooling::DevOverrideEngine* devOverrideEngine() const noexcept {
		return devOverrideEngine_;
	}

	/**
	 * @brief Check whether UI construction is currently inside an active dev-internal element scope.
	 *
	 * When true, child elements (even standard library controls like FSEL::Button)
	 * are protected from developer override mutations and captured as internal dev elements.
	 */
	[[nodiscard]] bool isDevInternalScope() const noexcept {
		return devInternalScopeDepth_ > 0;
	}

	/** Enter an internal dev element scope, shielding descendants from overrides. */
	void enterDevInternalScope() noexcept {
		++devInternalScopeDepth_;
	}

	/** Leave an internal dev element scope. */
	void leaveDevInternalScope() noexcept {
		if (devInternalScopeDepth_ > 0) {
			--devInternalScopeDepth_;
		}
	}
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
	 * @throws FlowUiException with FramePhaseViolation outside an active frame,
	 * or FrameCapacityExceeded when the configured frame arena is exhausted.
	 * @throws std::bad_alloc if arena storage allocation has failed earlier.
	 *
	 * @code{.cpp}
	 * CLAY_TEXT(
	 *     context.uiManager.toClayString(context.params.label),
	 *     CLAY_TEXT_CONFIG(textConfig));
	 * @endcode
	 */
	[[nodiscard]] Clay_String toClayString(std::string_view s);
	/** @brief Normalize a window-local Ui resource key and copy its name to the active frame arena. */
	[[nodiscard]] Clay_String toClayString(ResourceKey key);

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
	 * @throws FlowUiException with FramePhaseViolation outside an active frame,
	 * or FrameCapacityExceeded when the configured frame arena is exhausted.
	 * 
	 * @note This function should be used as .imageData instead of raw variable address
	 *
	 * @code{.cpp}
	 * Clay_ImageElementConfig image{};
	 * image.imageData = context.uiManager.imageData(textureRef);
	 * @endcode
	 */
	[[nodiscard]] TextureRef* imageData(TextureRef textureRef);

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
	 *                 context.uiManager.toClayString(field.text.copy()),
	 *                 CLAY_TEXT_CONFIG(textConfig));
	 *         }
	 *     }
	 * }
	 * @endcode
	 */
	[[nodiscard]] Clay_ElementDeclaration
	inputContentElement(const Clay_TextElementConfig& textConfig) const;

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
	 * @throws FlowUiException with FramePhaseViolation outside a frame or
	 * FrameCapacityExceeded when the frame arena is exhausted.
	 *
	 * @code{.cpp}
	 * const Clay_ElementId overlayId = ui.toClaySID("overlay/root");
	 * @endcode
	 */
	[[nodiscard]] Clay_ElementId toClaySID(std::string_view s);
	/** @brief Normalize a window-local Ui resource key and hash its frame-copied name into a Clay ID. */
	[[nodiscard]] Clay_ElementId toClaySID(ResourceKey key);

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
	[[nodiscard]] Clay_ElementId toClayEID(FlowElementID id);
	/** @brief Bridge a globally resolved identity to Clay without changing its Flow address. */
	[[nodiscard]] Clay_ElementId toClayEID(GlobalFlowID id);
	/** @brief Bridge a owner-bound semantic part identity to Clay without changing its Flow address. */
	[[nodiscard]] Clay_ElementId toClayEID(FlowElementPartID id);

	/**
	 * @brief Create a builder for a typed FlowUi element instance.
	 *
	 * createElement() is the public entry point for invoking FlowUi element
	 * definitions. The returned ElementBuilder owns the resolved element ID and parameter
	 * storage for this invocation until draw() or construct() is called.
	 *
	 * @tparam Element Empty tag type satisfying FlowElement.
	 * @par Arguments
	 * - @p localName Name unique among siblings of the same element definition.
	 * @note Development builds also accept sourceLocation for inspection metadata.
	 * @return ElementBuilder configured for the passed definition and element id.
	 *
	 * @code{.cpp}
	 * app.ui()
	 *     .createElement(FlowUi::kButton, FlowUi::LocalElementName{"save"})
	 *     .setParameters(FlowUi::ButtonParameters{.label = "Save"})
	 *     .draw();
	 * @endcode
	 *
	 * @see @ref md_docs_2tutorials_2custom__elements "Custom Elements"
	 * @see @ref md_docs_2tutorials_2developer__mode "Developer Mode"
	 */
	template <FlowElement Element>
	[[nodiscard]] ElementBuilder<Element>
	createElement(const Element&, LocalElementName localName
#if FLOW_UI_DEV_MODE
				  ,
				  std::source_location sourceLocation = std::source_location::current()
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

	/** @brief Create a local child from a runtime name; the name is borrowed for this invocation. */
	template <FlowElement Element>
	[[nodiscard]] ElementBuilder<Element>
	createElement(const Element&, RuntimeElementName localName
#if FLOW_UI_DEV_MODE
				  ,
				  std::source_location sourceLocation = std::source_location::current()
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
	[[nodiscard]] ElementBuilder<Element>
	createElement(const Element&, IndexedElementName indexedName
#if FLOW_UI_DEV_MODE
				  ,
				  std::source_location sourceLocation = std::source_location::current()
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
	[[nodiscard]] ElementBuilder<Element>
	createElement(const Element&, AutoElementName automaticName = AutoID()
#if FLOW_UI_DEV_MODE
									  ,
				  std::source_location sourceLocation = std::source_location::current()
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
	[[nodiscard]] ElementBuilder<Element>
	createElement(const Element&, GlobalFlowID globalId
#if FLOW_UI_DEV_MODE
				  ,
				  std::source_location sourceLocation = std::source_location::current()
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

	/** @brief Create a builder using an already resolved nonzero Flow identity without rehashing. */
	template <FlowElement Element>
	[[nodiscard]] ElementBuilder<Element>
	createElement(const Element&, FlowElementID resolvedId
#if FLOW_UI_DEV_MODE
				  ,
				  std::source_location sourceLocation = std::source_location::current()
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
	[[nodiscard]] ElementBuilder<Element>
	createElement(const Element&, FlowElementPartID partId
#if FLOW_UI_DEV_MODE
				  ,
				  std::source_location sourceLocation = std::source_location::current()
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
	 * @throws FlowUiException if no constructed element or active Clay context is available.
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
	[[nodiscard]] const InteractionSnapshot& getPreviousFramesInteraction() const;
#if FLOW_UI_AGENTIC_WORKFLOW
	/** Observe the just-completed layout; valid only until this window begins its next frame. */
	[[nodiscard]] const InteractionSnapshot& dev_completed_interaction() const noexcept;
	/** Layout-space window extent used by the most recent build, without switching Clay contexts. */
	[[nodiscard]] Clay_Dimensions dev_layout_dimensions() const noexcept;
	/** Last completed UI identity, independent of optional development-tree collection. */
	[[nodiscard]] std::uint64_t dev_completed_window_frame() const noexcept;
	/** @brief Return the app tick owning the most recently completed window UI frame. */
	[[nodiscard]] std::uint64_t dev_completed_app_tick() const noexcept;
#endif

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
	[[nodiscard]] const FrameInput& getCurrentFrameInput() const;

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
	[[nodiscard]] const FrameInput& getPreviousFrameInput() const;

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
	[[nodiscard]] InputFieldManager& inputFields() noexcept { return inputFieldManager_; }

	/**
	 * @brief Access the input field manager.
	 *
	 * @return Immutable InputFieldManager owned by this UiManager.
	 */
	[[nodiscard]] const InputFieldManager& inputFields() const noexcept {
		return inputFieldManager_;
	}

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
	[[nodiscard]] ShortcutManager& shortcuts() noexcept { return shortcutManager_; }

	/**
	 * @brief Access the shortcut manager.
	 *
	 * @return Immutable ShortcutManager owned by this UiManager.
	 */
	[[nodiscard]] const ShortcutManager& shortcuts() const noexcept { return shortcutManager_; }

	/** @brief Access this window's mutable popup placement and dismissal service. */
	[[nodiscard]] PopupManager& popups() noexcept { return popupManager_; }
	/** @brief Access this window's immutable popup placement and dismissal service. */
	[[nodiscard]] const PopupManager& popups() const noexcept { return popupManager_; }

	/** @brief Access the app-wide ActionManager attached to this UI. */
	[[nodiscard]] ActionManager& actions();
	/** @brief Access the immutable app-wide ActionManager attached to this UI. */
	[[nodiscard]] const ActionManager& actions() const;
	/** @brief Invoke an action from this window's UI context. */
	[[nodiscard]] ActionInvocationStatus invoke(ActionCall call);
#if FLOW_UI_DEV_MODE
	/**
	 * @brief Access developer tooling config.
	 *
	 * @return Mutable developer tooling configuration.
	 */
	[[nodiscard]] DevToolsConfig& devToolsConfig();

	/**
	 * @brief Access developer tooling config.
	 *
	 * @return Immutable developer tooling configuration.
	 */
	[[nodiscard]] const DevToolsConfig& devToolsConfig() const;
#endif

	/**
	 * @brief Set clipboard text through the attached platform clipboard service.
	 *
	 * Detached managers ignore clipboard writes; service failures are reported.
	 *
	 * @param text Text to copy to the system or configured clipboard.
	 *
	 * @code{.cpp}
	 * context.uiManager.setClipboardText("Copied text");
	 * @endcode
	 */
	void setClipboardText(std::string_view text) const;

	/**
	 * @brief Read clipboard text through the attached platform clipboard service.
	 *
	 * @return Clipboard text, or an empty string when detached or a read fails.
	 *
	 * @code{.cpp}
	 * const std::string pasted = context.uiManager.clipboardText();
	 * @endcode
	 */
	[[nodiscard]] std::string clipboardText() const;

	/**
	 * @brief Return whether the attached window provides immediate clipboard access.
	 *
	 * @retval true The attached window supports clipboard reads and writes.
	 * @retval false The service is detached or clipboard access is unsupported.
	 *
	 * @code{.cpp}
	 * if (context.uiManager.hasClipboardAccess()) {
	 *     context.uiManager.setClipboardText(selectedText);
	 * }
	 * @endcode
	 */
	[[nodiscard]] bool hasClipboardAccess() const;

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
	[[nodiscard]] FontId resolveFont(FontFamilyId familyId, uint32_t weight = 400,
									 FontStyle style = FontStyle::Normal) const;

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
	[[nodiscard]] FontId resolveFont(std::string_view familyName, uint32_t weight = 400,
									 FontStyle style = FontStyle::Normal) const;

	/**
	 * @brief Access the active variant for theme type T.
	 *
	 * @tparam T Theme struct type.
	 * @return Const reference to active theme instance of type T.
	 * @throws FlowUiException if ThemeManager is unavailable or no active theme exists.
	 * @note A development DevThemeScope may substitute a borrowed local payload;
	 * references must not outlive that scope or a theme registry mutation.
	 */
	template <typename T>
	[[nodiscard]] const T& theme() const {
#if FLOW_UI_DEV_MODE
		for (auto* scope = dev_theme_scope_; scope; scope = scope->previous_)
			if (scope->payload_.type == detail::typeHash<T>() &&
				scope->payload_.payloadSize == sizeof(T))
				return *static_cast<const T*>(scope->payload_.payload);
#endif
		return appThemes().template getActiveTheme<T>();
	}

#if FLOW_UI_DEV_MODE
	/** Frame-local variant resolution for nested builders; active application variants stay intact.
	 */
	class DevThemeScope {
	public:
		/** @brief Borrow payload for nested theme resolution until this scope is destroyed. */
		DevThemeScope(UiManager& ui, ThemeManager::DevThemePayloadView payload) noexcept
			: ui_(ui), payload_(payload), previous_(ui.dev_theme_scope_) {
			if (payload_.payload)
				ui_.dev_theme_scope_ = this;
		}
		/** @brief Restore the previous local theme resolver. */
		~DevThemeScope() { ui_.dev_theme_scope_ = previous_; }
		DevThemeScope(const DevThemeScope&) = delete;
		DevThemeScope& operator=(const DevThemeScope&) = delete;

	private:
		friend class UiManager;
		UiManager& ui_;
		ThemeManager::DevThemePayloadView payload_{};
		DevThemeScope* previous_ = nullptr;
	};
	/** Propagate passive callback suppression to every nested typed builder. */
	class DevPassivePreviewScope {
	public:
		/** @brief Suppress typed interaction callbacks and protect nested preview nodes from overrides. */
		explicit DevPassivePreviewScope(UiManager& ui) noexcept : ui_(ui) {
			++ui_.dev_passive_preview_depth_;
			ui_.enterDevInternalScope();
		}
		/** @brief Restore the enclosing preview suppression and internal-scope depth. */
		~DevPassivePreviewScope() {
			ui_.leaveDevInternalScope();
			--ui_.dev_passive_preview_depth_;
		}
		DevPassivePreviewScope(const DevPassivePreviewScope&) = delete;
		DevPassivePreviewScope& operator=(const DevPassivePreviewScope&) = delete;

	private:
		UiManager& ui_;
	};
	/** Whether all nested element callbacks must remain passive. */
	[[nodiscard]] bool isDevPassivePreview() const noexcept {
		return dev_passive_preview_depth_ != 0;
	}
#endif

	/**
	 * @brief Access a specific named variant for theme type T.
	 *
	 * @tparam T Theme struct type.
	 * @param variantName Name of the theme variant.
	 * @return Const reference to named theme instance of type T.
	 * @throws FlowUiException if ThemeManager is unavailable or the variant is absent.
	 * @note This explicit variant lookup bypasses development local scopes. The
	 * reference is borrowed; reacquire it after theme registry mutation.
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

#if FLOW_UI_DEV_MODE
	/** Internal window-owned timeline renderer; absent before runtime initialization. */
	[[nodiscard]] devSystems::interface_elements::DevMemoryHistoryController*
	memory_history_controller() const noexcept {
		return memory_history_controller_;
	}
	/** Window-owned Performance timeline controller. */
	[[nodiscard]] devSystems::interface_elements::DevTimelineController*
	timeline_controller() const noexcept {
		return timeline_controller_;
	}
#endif
private:
#if FLOW_UI_DEV_MODE
	devSystems::interface_elements::DevTimelineController* timeline_controller_ = nullptr;
	devSystems::interface_elements::DevMemoryHistoryController* memory_history_controller_ =
		nullptr;
#endif
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
	void setImageManager(ImageManager* imageManager) noexcept { imageManager_ = imageManager; }
	void setIconManager(IconManager* iconManager) noexcept { iconManager_ = iconManager; }
#if FLOW_UI_DEV_MODE
	void setDevTimingRecorder(devSystems::DevTimingRecorder* recorder) noexcept {
		devTimingRecorder_ = recorder;
	}
	void setDevSchemaRegistry(devSystems::tooling::DevSchemaRegistry* registry) noexcept {
		devSchemaRegistry_ = registry;
	}
	void setDevOverrideEngine(
		devSystems::tooling::DevOverrideEngine* engine) noexcept {
		devOverrideEngine_ = engine;
	}
	[[nodiscard]] devSystems::DevTimingRecorder* devTimingRecorder() const noexcept {
		return devTimingRecorder_;
	}
#endif
	const ThemeManager& appThemes() const;

	void beginFrame(const detail::storage::FrameToken& frame, const FrameInput& frameInput,
					const detail::manager_storage::FontFrameView& fontView, float screenWidth,
					float screenHeight, bool primaryPointerCanceled = false);
	Clay_RenderCommandArray endFrame();
	const detail::InputFieldFrameOverrides& inputFieldFrameOverrides() const { return inputFieldManager_.frameOverrides(); }
	/** Bind the borrowed native clipboard/cursor bridge, including late storage creation. */
	void setPlatformServices(detail::platform::WindowPlatformServices* services) noexcept;
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
	void closeConstructedToDepth(
		size_t depth, bool warn, bool autoClosedAtFrameEnd = false) noexcept;
	void retainConstructedElement(
		Clay_ElementId clayId,
		FlowElementID flowId,
		size_t priorFlowScopeDepth
#if FLOW_UI_DEV_MODE
		, devSystems::tooling::DevTreeCapture::Token treeToken
		, bool isDevInternal = false
#endif
#if FLOW_UI_DEV_MODE && FLOWUI_DEV_TIMING_LEVEL >= 2
		, FlowDefinitionID definitionId = {}
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
	detail::platform::WindowPlatformServices* platformServices_ = nullptr;
	detail::storage::IStorageSystem* storage_ = nullptr;
	const ThemeManager* themeManager_ = nullptr;
	ElementManager* elementManager_ = nullptr;
	ActionManager* actionManager_ = nullptr;
	ImageManager* imageManager_ = nullptr;
	IconManager* iconManager_ = nullptr;
#if FLOW_UI_DEV_MODE
	devSystems::DevTimingRecorder* devTimingRecorder_ = nullptr;
	devSystems::tooling::DevSchemaRegistry* devSchemaRegistry_ = nullptr;
	devSystems::tooling::DevOverrideEngine* devOverrideEngine_ = nullptr;
	uint32_t devInternalScopeDepth_ = 0;
	uint32_t dev_passive_preview_depth_ = 0;
	DevThemeScope* dev_theme_scope_ = nullptr;
	uint64_t dev_evidence_tick_ = 0, dev_evidence_frame_ = 0;
	bool dev_tree_collection_ = true;
#endif
	WindowId window_ = InvalidWindowId;
	uint64_t stateHandle_ = 0;
	detail::manager_storage::UiManagerState* state_ = nullptr;

};

/** @} */

} //namespace FlowUi

#include "FlowUi/Public/elementAuthoring/ElementBuilder.hpp"
