#pragma once

#include "FlowUi/Private/ApiBoundary.hpp"

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
namespace detail::storage {
class StorageBinding;
struct FrameToken;
} // namespace detail::storage
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
 * auto uiResult = app.ui();
 * if (!uiResult) throw FlowUi::FlowUiException(uiResult.error());
 * FlowUi::UiManager& ui = uiResult->get();
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
	/**
	 * @brief Return the first latched UI error for this frame, including input-field and popup failure state. Check before committing custom frame composition.
	 *
	 * @par Example
	 * @code{.cpp}
	 * auto result = ui.frameStatus();
	 * if (!result) {
	 *     throw FlowUi::FlowUiException(result.error());
	 * }
	 * @endcode
	 *
	 * @see docs/api/ui_manager.md
	 */
	[[nodiscard]] Status frameStatus() const noexcept {
		if (frameError_) return unexpectedError(frameError_);
		if (const auto status = inputFieldManager_.frameStatus(); !status) return status;
		return popupManager_.frameStatus();
	}
	/**
	 * @brief Latch the first error for this frame so partially built output cannot be submitted. Later calls preserve the earlier error.
	 *
	 * @see docs/api/ui_manager.md
	 */
	void recordFrameError(FlowUiError error) const noexcept {
		if (!frameError_) frameError_ = error;
	}
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
	 * @brief Copy text into current-frame storage and return a Clay_String borrowing that storage. ResourceKey supplies the name text and explicit resource attribution.
	 *
	 * @par Example
	 * @code{.cpp}
	 * const std::string_view text = "Save";
	 * const Clay_String clayText = ui.toClayString(text);
	 * @endcode
	 *
	 * @throws FlowUiException On a library failure; the first failure is latched for this frame.
	 * Other operational exceptions propagate after latching frame failure. Abandon the failed frame.
	 *
	 * @see docs/api/ui_manager.md
	 */
	[[nodiscard]] Clay_String toClayString(std::string_view s);
	/**
	 * @brief Copy text into current-frame storage and return a Clay_String borrowing that storage. ResourceKey supplies the name text and explicit resource attribution.
	 *
	 * @par Example
	 * @code{.cpp}
	 * const FlowUi::ResourceKey text{.name = "Save"};
	 * const Clay_String clayText = ui.toClayString(text);
	 * @endcode
	 *
	 * @throws FlowUiException On a library failure; the first failure is latched for this frame.
	 * Other operational exceptions propagate after latching frame failure. Abandon the failed frame.
	 *
	 * @see docs/api/ui_manager.md
	 */
	[[nodiscard]] Clay_String toClayString(ResourceKey key);

	/**
	 * @brief Copy a texture reference into current-frame storage and return the pointer used by Clay image configuration. The pointee is valid for this frame only.
	 *
	 * @par Example
	 * @code{.cpp}
	 * Clay_ImageElementConfig image{};
	 * image.imageData = ui.imageData(texture);
	 * @endcode
	 *
	 * @throws FlowUiException On a library failure; the first failure is latched for this frame.
	 * Other operational exceptions propagate after latching frame failure. Abandon the failed frame.
	 *
	 * @see docs/api/ui_manager.md
	 */
	[[nodiscard]] TextureRef* imageData(TextureRef textureRef);

	/**
	 * @brief Return an input-content declaration with growing width and a height derived from the resolved font metrics. Operational failures propagate and latch frame failure; cancel the failed frame.
	 *
	 * @par Example
	 * @code{.cpp}
	 * const Clay_ElementDeclaration content = ui.inputContentElement(textConfig);
	 * @endcode
	 *
	 * @throws FlowUiException On a library failure; the first failure is latched for this frame.
	 * Other operational exceptions propagate after latching frame failure. Abandon the failed frame.
	 *
	 * @see docs/api/ui_manager.md
	 */
	[[nodiscard]] Clay_ElementDeclaration
	inputContentElement(const Clay_TextElementConfig& textConfig) const;

	/**
	 * @brief Copy the supplied string identity into current-frame storage and return its Clay string ID. ResourceKey carries explicit attribution.
	 *
	 * @par Example
	 * @code{.cpp}
	 * const std::string_view name = "toolbar/save";
	 * const Clay_ElementId clayId = ui.toClaySID(name);
	 * @endcode
	 *
	 * @throws FlowUiException On a library failure; the first failure is latched for this frame.
	 * Other operational exceptions propagate after latching frame failure. Abandon the failed frame.
	 *
	 * @see docs/api/ui_manager.md
	 */
	[[nodiscard]] Clay_ElementId toClaySID(std::string_view s);
	/**
	 * @brief Copy the supplied string identity into current-frame storage and return its Clay string ID. ResourceKey carries explicit attribution.
	 *
	 * @par Example
	 * @code{.cpp}
	 * const FlowUi::ResourceKey name{.name = "toolbar/save"};
	 * const Clay_ElementId clayId = ui.toClaySID(name);
	 * @endcode
	 *
	 * @throws FlowUiException On a library failure; the first failure is latched for this frame.
	 * Other operational exceptions propagate after latching frame failure. Abandon the failed frame.
	 *
	 * @see docs/api/ui_manager.md
	 */
	[[nodiscard]] Clay_ElementId toClaySID(ResourceKey key);

	/**
	 * @brief Bridge a strong local, global, or semantic-part identity directly into a Clay ID for this window.
	 *
	 * @par Example
	 * @code{.cpp}
	 * const Clay_ElementId clayId = ui.toClayEID(id);
	 * @endcode
	 *
	 * @throws FlowUiException On a library failure; the first failure is latched for this frame.
	 * Other operational exceptions propagate after latching frame failure. Abandon the failed frame.
	 *
	 * @see docs/api/ui_manager.md
	 */
	[[nodiscard]] Clay_ElementId toClayEID(FlowElementID id);
	/**
	 * @brief Bridge a strong local, global, or semantic-part identity directly into a Clay ID for this window.
	 *
	 * @par Example
	 * @code{.cpp}
	 * const Clay_ElementId clayId = ui.toClayEID(id);
	 * @endcode
	 *
	 * @throws FlowUiException On a library failure; the first failure is latched for this frame.
	 * Other operational exceptions propagate after latching frame failure. Abandon the failed frame.
	 *
	 * @see docs/api/ui_manager.md
	 */
	[[nodiscard]] Clay_ElementId toClayEID(GlobalFlowID id);
	/**
	 * @brief Bridge a strong local, global, or semantic-part identity directly into a Clay ID for this window.
	 *
	 * @par Example
	 * @code{.cpp}
	 * const Clay_ElementId clayId = ui.toClayEID(id);
	 * @endcode
	 *
	 * @throws FlowUiException On a library failure; the first failure is latched for this frame.
	 * Other operational exceptions propagate after latching frame failure. Abandon the failed frame.
	 *
	 * @see docs/api/ui_manager.md
	 */
	[[nodiscard]] Clay_ElementId toClayEID(FlowElementPartID id);

	/**
	 * @brief Create a direct typed builder for the active frame. Local/runtime/indexed/automatic names resolve under the captured parent; resolved, global and part IDs use their explicit address.
	 *
	 * Direct throwing operation: library validation raises FlowUiException and application parameter/callback exceptions propagate. Failure poisons the frame; abandon this builder and cancel the frame before reuse. No successful partial layout should be submitted.
	 *
	 * @par Example
	 * The complete ExampleCounter definition is provided in the element API reference.
	 * @code{.cpp}
	 * ui.createElement(counter, "counter")
	 *     .setParameters({.width = 160.0f})
	 *     .draw();
	 * @endcode
	 *
	 * @see docs/api/ui_manager.md
	 */
	template <FlowElement Element>
	[[nodiscard]] ElementBuilder<Element>
	createElement(const Element&, LocalElementName localName
#if FLOW_UI_DEV_MODE
				  ,
				  std::source_location sourceLocation = std::source_location::current()
#endif
	) try {
		return detail::requireApiValue(createElementBuilder<Element>(
			[&](FlowElementID parentId) {
				return resolveLocalElementID(parentId, Element::definitionId, localName);
			}
#if FLOW_UI_DEV_MODE
			, sourceLocation
#endif
		));
	} catch (...) {
		recordFrameError(detail::currentExceptionError(ErrorSite::UiManagerDefineElement));
		throw;
	}

	/**
	 * @brief Create a direct typed builder for the active frame. Local/runtime/indexed/automatic names resolve under the captured parent; resolved, global and part IDs use their explicit address.
	 *
	 * Direct throwing operation: library validation raises FlowUiException and application parameter/callback exceptions propagate. Failure poisons the frame; abandon this builder and cancel the frame before reuse. No successful partial layout should be submitted.
	 *
	 * @par Example
	 * The complete ExampleCounter definition is provided in the element API reference.
	 * @code{.cpp}
	 * ui.createElement(counter, FlowUi::RuntimeName("counter"))
	 *     .setParameters({.width = 160.0f})
	 *     .draw();
	 * @endcode
	 *
	 * @see docs/api/ui_manager.md
	 */
	template <FlowElement Element>
	[[nodiscard]] ElementBuilder<Element>
	createElement(const Element&, RuntimeElementName localName
#if FLOW_UI_DEV_MODE
				  ,
				  std::source_location sourceLocation = std::source_location::current()
#endif
	) try {
		return detail::requireApiValue(createElementBuilder<Element>(
			[&](FlowElementID parentId) {
				return resolveLocalElementID(parentId, Element::definitionId, localName);
			}
#if FLOW_UI_DEV_MODE
			, sourceLocation
#endif
		));
	} catch (...) {
		recordFrameError(detail::currentExceptionError(ErrorSite::UiManagerDefineElement));
		throw;
	}

	/**
	 * @brief Create a direct typed builder for the active frame. Local/runtime/indexed/automatic names resolve under the captured parent; resolved, global and part IDs use their explicit address.
	 *
	 * Direct throwing operation: library validation raises FlowUiException and application parameter/callback exceptions propagate. Failure poisons the frame; abandon this builder and cancel the frame before reuse. No successful partial layout should be submitted.
	 *
	 * @par Example
	 * The complete ExampleCounter definition is provided in the element API reference.
	 * @code{.cpp}
	 * ui.createElement(counter, FlowUi::Indexed("counter", 0))
	 *     .setParameters({.width = 160.0f})
	 *     .draw();
	 * @endcode
	 *
	 * @see docs/api/ui_manager.md
	 */
	template <FlowElement Element>
	[[nodiscard]] ElementBuilder<Element>
	createElement(const Element&, IndexedElementName indexedName
#if FLOW_UI_DEV_MODE
				  ,
				  std::source_location sourceLocation = std::source_location::current()
#endif
	) try {
		return detail::requireApiValue(createElementBuilder<Element>(
			[&](FlowElementID parentId) {
				return resolveIndexedElementID(parentId, Element::definitionId, indexedName);
			}
#if FLOW_UI_DEV_MODE
			, sourceLocation
#endif
		));
	} catch (...) {
		recordFrameError(detail::currentExceptionError(ErrorSite::UiManagerDefineElement));
		throw;
	}

	/**
	 * @brief Create a direct typed builder for the active frame. Local/runtime/indexed/automatic names resolve under the captured parent; resolved, global and part IDs use their explicit address.
	 *
	 * Direct throwing operation: library validation raises FlowUiException and application parameter/callback exceptions propagate. Failure poisons the frame; abandon this builder and cancel the frame before reuse. No successful partial layout should be submitted.
	 *
	 * @par Example
	 * The complete ExampleCounter definition is provided in the element API reference.
	 * @code{.cpp}
	 * ui.createElement(counter, FlowUi::AutoID())
	 *     .setParameters({.width = 160.0f})
	 *     .draw();
	 * @endcode
	 *
	 * @see docs/api/ui_manager.md
	 */
	template <FlowElement Element>
	[[nodiscard]] ElementBuilder<Element>
	createElement(const Element&, AutoElementName automaticName = AutoID()
#if FLOW_UI_DEV_MODE
									  ,
				  std::source_location sourceLocation = std::source_location::current()
#endif
	) try {
		return detail::requireApiValue(createElementBuilder<Element>(
			[&](FlowElementID parentId) {
				return resolveAutomaticElementID(parentId, Element::definitionId, automaticName);
			}
#if FLOW_UI_DEV_MODE
			, sourceLocation, true
#endif
		));
	} catch (...) {
		recordFrameError(detail::currentExceptionError(ErrorSite::UiManagerDefineElement));
		throw;
	}

	/**
	 * @brief Create a direct typed builder for the active frame. Local/runtime/indexed/automatic names resolve under the captured parent; resolved, global and part IDs use their explicit address.
	 *
	 * Direct throwing operation: library validation raises FlowUiException and application parameter/callback exceptions propagate. Failure poisons the frame; abandon this builder and cancel the frame before reuse. No successful partial layout should be submitted.
	 *
	 * @par Example
	 * The complete ExampleCounter definition is provided in the element API reference.
	 * @code{.cpp}
	 * ui.createElement(counter, id)
	 *     .setParameters({.width = 160.0f})
	 *     .draw();
	 * @endcode
	 *
	 * @see docs/api/ui_manager.md
	 */
	template <FlowElement Element>
	[[nodiscard]] ElementBuilder<Element>
	createElement(const Element&, GlobalFlowID globalId
#if FLOW_UI_DEV_MODE
				  ,
				  std::source_location sourceLocation = std::source_location::current()
#endif
	) try {
		return detail::requireApiValue(createElementBuilder<Element>(
			[&](FlowElementID) {
				if (!globalId)
					throw FlowUiException(makeError(ErrorCode::InvalidElementId, ErrorSite::UiManagerDefineElement));
				return normalizeGlobalElementID(globalId);
			}
#if FLOW_UI_DEV_MODE
			, sourceLocation
#endif
		));
	} catch (...) {
		recordFrameError(detail::currentExceptionError(ErrorSite::UiManagerDefineElement));
		throw;
	}

	/**
	 * @brief Create a direct typed builder for the active frame. Local/runtime/indexed/automatic names resolve under the captured parent; resolved, global and part IDs use their explicit address.
	 *
	 * Direct throwing operation: library validation raises FlowUiException and application parameter/callback exceptions propagate. Failure poisons the frame; abandon this builder and cancel the frame before reuse. No successful partial layout should be submitted.
	 *
	 * @par Example
	 * The complete ExampleCounter definition is provided in the element API reference.
	 * @code{.cpp}
	 * ui.createElement(counter, id)
	 *     .setParameters({.width = 160.0f})
	 *     .draw();
	 * @endcode
	 *
	 * @see docs/api/ui_manager.md
	 */
	template <FlowElement Element>
	[[nodiscard]] ElementBuilder<Element>
	createElement(const Element&, FlowElementID resolvedId
#if FLOW_UI_DEV_MODE
				  ,
				  std::source_location sourceLocation = std::source_location::current()
#endif
	) try {
		return detail::requireApiValue(createElementBuilder<Element>(
			[&](FlowElementID) {
				if (!resolvedId)
					throw FlowUiException(makeError(ErrorCode::InvalidElementId, ErrorSite::UiManagerDefineElement));
				return resolvedId;
			}
#if FLOW_UI_DEV_MODE
			, sourceLocation
#endif
		));
	} catch (...) {
		recordFrameError(detail::currentExceptionError(ErrorSite::UiManagerDefineElement));
		throw;
	}

	/**
	 * @brief Create a direct typed builder for the active frame. Local/runtime/indexed/automatic names resolve under the captured parent; resolved, global and part IDs use their explicit address.
	 *
	 * Direct throwing operation: library validation raises FlowUiException and application parameter/callback exceptions propagate. Failure poisons the frame; abandon this builder and cancel the frame before reuse. No successful partial layout should be submitted.
	 *
	 * @par Example
	 * The complete ExampleCounter definition is provided in the element API reference.
	 * @code{.cpp}
	 * ui.createElement(counter, id)
	 *     .setParameters({.width = 160.0f})
	 *     .draw();
	 * @endcode
	 *
	 * @see docs/api/ui_manager.md
	 */
	template <FlowElement Element>
	[[nodiscard]] ElementBuilder<Element>
	createElement(const Element&, FlowElementPartID partId
#if FLOW_UI_DEV_MODE
				  ,
				  std::source_location sourceLocation = std::source_location::current()
#endif
	) try {
		return detail::requireApiValue(createElementBuilder<Element>(
			[&](FlowElementID) {
				if (!partId)
					throw FlowUiException(makeError(ErrorCode::InvalidElementId, ErrorSite::UiManagerDefineElement));
				return normalizePartElementID(partId);
			}
#if FLOW_UI_DEV_MODE
			, sourceLocation
#endif
		));
	} catch (...) {
		recordFrameError(detail::currentExceptionError(ErrorSite::UiManagerDefineElement));
		throw;
	}

	/**
	 * @brief Close the most recent root opened by ElementBuilder::construct(), including its FlowUi scope and development capture. Each successful construct needs one close.
	 *
	 * Direct throwing operation: library validation raises FlowUiException and application parameter/callback exceptions propagate. Failure poisons the frame; abandon this builder and cancel the frame before reuse. No successful partial layout should be submitted.
	 *
	 * @par Example
	 * The complete ExampleCounter definition is provided in the element API reference.
	 * @code{.cpp}
	 * ui.createElement(counter, "panel").construct();
	 * ui.createElement(counter, "child").draw();
	 * ui.drawConstructed();
	 * @endcode
	 *
	 * @see docs/api/ui_manager.md
	 */
	void drawConstructed();

	/**
	 * @brief Borrow the previous completed frame interaction snapshot. It remains the stable source for current element pointer callbacks.
	 *
	 * @see docs/api/ui_manager.md
	 */
	[[nodiscard]] const InteractionSnapshot& getPreviousFramesInteraction() const noexcept;
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
	 * @brief Borrow this frame input in scaled FlowUi layout coordinates; do not retain this reference as an immutable historical snapshot.
	 *
	 * @see docs/api/ui_manager.md
	 */
	[[nodiscard]] const FrameInput& getCurrentFrameInput() const noexcept;

	/**
	 * @brief Borrow the previous layout-frame input, useful with current input for edge and drag calculations.
	 *
	 * @see docs/api/ui_manager.md
	 */
	[[nodiscard]] const FrameInput& getPreviousFrameInput() const noexcept;

	/**
	 * @brief Borrow this window InputFieldManager directly; the const overload exposes read-only access.
	 *
	 * @see docs/api/ui_manager.md
	 */
	[[nodiscard]] InputFieldManager& inputFields() noexcept { return inputFieldManager_; }

	/**
	 * @brief Borrow this window InputFieldManager directly; the const overload exposes read-only access.
	 *
	 * @see docs/api/ui_manager.md
	 */
	[[nodiscard]] const InputFieldManager& inputFields() const noexcept {
		return inputFieldManager_;
	}

	/**
	 * @brief Borrow this window ShortcutManager directly; the const overload exposes read-only access.
	 *
	 * @see docs/api/ui_manager.md
	 */
	[[nodiscard]] ShortcutManager& shortcuts() noexcept { return shortcutManager_; }

	/**
	 * @brief Borrow this window ShortcutManager directly; the const overload exposes read-only access.
	 *
	 * @see docs/api/ui_manager.md
	 */
	[[nodiscard]] const ShortcutManager& shortcuts() const noexcept { return shortcutManager_; }

	/**
	 * @brief Borrow this window popup placement and dismissal manager directly; the const overload exposes read-only access.
	 *
	 * @see docs/api/ui_manager.md
	 */
	[[nodiscard]] PopupManager& popups() noexcept { return popupManager_; }
	/**
	 * @brief Borrow this window popup placement and dismissal manager directly; the const overload exposes read-only access.
	 *
	 * @see docs/api/ui_manager.md
	 */
	[[nodiscard]] const PopupManager& popups() const noexcept { return popupManager_; }

	/**
	 * @brief Borrow the attached app-wide action manager directly. A detached UiManager throws FlowUiException.
	 *
	 * @see docs/api/ui_manager.md
	 */
	[[nodiscard]] ActionManager& actions();
	/**
	 * @brief Borrow the attached app-wide action manager directly. A detached UiManager throws FlowUiException.
	 *
	 * @see docs/api/ui_manager.md
	 */
	[[nodiscard]] const ActionManager& actions() const;
	/**
	 * @brief Invoke an action synchronously and return Invoked, Empty, Unbound or Disabled. Arbitrary callback exceptions propagate unchanged; application effects are not rolled back.
	 *
	 * @see docs/api/ui_manager.md

	 *
	 * @par Example
	 * @code{.cpp}
	 * const FlowUi::ActionInvocationStatus outcome = actions.invoke(call);
	 * const bool invoked = outcome == FlowUi::ActionInvocationStatus::Invoked;
	 * @endcode
	 */
	[[nodiscard]] ActionInvocationStatus invoke(ActionCall call);
#if FLOW_UI_DEV_MODE
	/**
	 * @brief Access developer tooling config.
	 *
	 * @return Mutable developer tooling configuration.
	 */
	[[nodiscard]] DevToolsConfig& devToolsConfig() noexcept;

	/**
	 * @brief Access developer tooling config.
	 *
	 * @return Immutable developer tooling configuration.
	 */
	[[nodiscard]] const DevToolsConfig& devToolsConfig() const noexcept;
#endif

	/**
	 * @brief Copy text to the attached platform clipboard. Detached, unsupported, and failed platform operations are returned as errors; check the Status.
	 *
	 * @par Example
	 * @code{.cpp}
	 * auto result = ui.setClipboardText("Copied text");
	 * if (!result) {
	 *     throw FlowUi::FlowUiException(result.error());
	 * }
	 * @endcode
	 *
	 * @see docs/api/ui_manager.md
	 */
	[[nodiscard]] Status setClipboardText(std::string_view text) const noexcept;

	/**
	 * @brief Return an owned copy of clipboard text. An empty successful string is valid text; detached/platform failures are errors.
	 *
	 * @par Example
	 * @code{.cpp}
	 * auto result = ui.clipboardText();
	 * if (!result) {
	 *     throw FlowUi::FlowUiException(result.error());
	 * }
	 * const std::string pasted = std::move(*result);
	 * @endcode
	 *
	 * @see docs/api/ui_manager.md
	 */
	[[nodiscard]] Result<std::string> clipboardText() const noexcept;

	/**
	 * @brief Return whether the attached window currently supports immediate clipboard reads and writes; detached managers return false.
	 *
	 * @see docs/api/ui_manager.md
	 */
	[[nodiscard]] bool hasClipboardAccess() const noexcept;

	/**
	 * @brief Request this frame cursor shape. Highest priority wins; a later equal-priority request may replace the earlier one. Requests reset at frame start.
	 *
	 * @see docs/api/ui_manager.md
	 */
	void requestCursor(CursorType cursorType, uint8_t priority = 0) noexcept;

	/**
	 * @brief Return the resolved concrete font ID from this attached manager's frame view. Zero represents an unresolved font fallback; this lookup does not allocate.
	 *
	 * @pre This UiManager remains initialized and attached to its owning window. The frame view is populated during frame preparation.
	 *
	 * @par Example
	 * @code{.cpp}
	 * const FlowUi::FontId font = ui.resolveFont(family, 400, FlowUi::FontStyle::Normal);
	 * @endcode
	 *
	 * @see docs/api/ui_manager.md
	 */
	[[nodiscard]] FontId resolveFont(FontFamilyId familyId, uint32_t weight = 400,
									 FontStyle style = FontStyle::Normal) const noexcept;

	/**
	 * @brief Return the resolved concrete font ID from this attached manager's frame view. Zero represents an unresolved font fallback; this lookup does not allocate.
	 *
	 * @pre This UiManager remains initialized and attached to its owning window. The frame view is populated during frame preparation.
	 *
	 * @par Example
	 * @code{.cpp}
	 * const FlowUi::FontId font = ui.resolveFont(family, 400, FlowUi::FontStyle::Normal);
	 * @endcode
	 *
	 * @see docs/api/ui_manager.md
	 */
	[[nodiscard]] FontId resolveFont(std::string_view familyName, uint32_t weight = 400,
									 FontStyle style = FontStyle::Normal) const noexcept;

	/**
	 * @brief Borrow the active or explicitly named registered theme through Result<reference_wrapper<const T>>. Active lookup respects a development local theme scope; named lookup bypasses it.
	 *
	 * Check the Result before result->get(). The wrapper and extracted reference do not own the payload. Reacquire after theme registry changes; a development override also limits the borrow to its enclosing scope.
	 *
	 * @par Example
	 * @code{.cpp}
	 * auto result = ui.theme<FlowUi::FlowUiTheme>();
	 * if (!result) {
	 *     throw FlowUi::FlowUiException(result.error());
	 * }
	 * const FlowUi::FlowUiTheme& theme = result->get();
	 * const float fontSize = theme.fontSizeMedium;
	 * @endcode
	 *
	 * @see docs/api/ui_manager.md
	 */
	template <typename T>
	[[nodiscard]] Result<std::reference_wrapper<const T>> theme() const noexcept try {
#if FLOW_UI_DEV_MODE
		for (auto* scope = dev_theme_scope_; scope; scope = scope->previous_)
			if (scope->payload_.type == detail::typeHash<T>() &&
				scope->payload_.payloadSize == sizeof(T))
				return std::cref(*static_cast<const T*>(scope->payload_.payload));
#endif
		return appThemes().template getActiveTheme<T>();
	} catch (...) {
		const auto error = detail::currentExceptionError(ErrorSite::UiManagerAccessTheme);
		return unexpectedError(error);
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
	 * @brief Borrow the active or explicitly named registered theme through Result<reference_wrapper<const T>>. Active lookup respects a development local theme scope; named lookup bypasses it.
	 *
	 * Check the Result before result->get(). The wrapper and extracted reference do not own the payload. Reacquire after theme registry changes; a development override also limits the borrow to its enclosing scope.
	 *
	 * @par Example
	 * @code{.cpp}
	 * auto result = ui.theme<FlowUi::FlowUiTheme>("light");
	 * if (!result) {
	 *     throw FlowUi::FlowUiException(result.error());
	 * }
	 * const FlowUi::FlowUiTheme& theme = result->get();
	 * const float fontSize = theme.fontSizeMedium;
	 * @endcode
	 *
	 * @see docs/api/ui_manager.md
	 */
	template <typename T>
	[[nodiscard]] Result<std::reference_wrapper<const T>> theme(std::string_view variantName) const noexcept try {
		return appThemes().template getTheme<T>(variantName);
	} catch (...) {
		const auto error = detail::currentExceptionError(ErrorSite::UiManagerAccessTheme);
		return unexpectedError(error);
	}

	/**
	 * @brief Borrow the active built-in FlowUiTheme through Result<reference_wrapper<const FlowUiTheme>>, including a development local theme scope.
	 *
	 * Check the Result before result->get(). The wrapper and extracted reference do not own the payload. Reacquire after theme registry changes; a development override also limits the borrow to its enclosing scope.
	 *
	 * @par Example
	 * @code{.cpp}
	 * auto result = ui.flowTheme();
	 * if (!result) {
	 *     throw FlowUi::FlowUiException(result.error());
	 * }
	 * const FlowUi::FlowUiTheme& theme = result->get();
	 * const float fontSize = theme.fontSizeMedium;
	 * @endcode
	 *
	 * @see docs/api/ui_manager.md
	 */
	[[nodiscard]] Result<std::reference_wrapper<const FlowUiTheme>> flowTheme() const noexcept;

	/**
	 * @brief Borrow the attached app-wide element manager directly. A detached UiManager throws FlowUiException.
	 *
	 * @see docs/api/ui_manager.md
	 */
	[[nodiscard]] ElementManager& elements();

	/**
	 * @brief Borrow the attached app-wide element manager directly. A detached UiManager throws FlowUiException.
	 *
	 * @see docs/api/ui_manager.md
	 */
	[[nodiscard]] const ElementManager& elements() const;

	/**
	 * @brief Return the window whose frame is built by this manager.
	 *
	 * @see docs/api/ui_manager.md
	 */
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
	/** Prepare library identity/manager access before constructing arbitrary application parameters. */
	template <FlowElement Element, typename ResolveIdentity>
	[[nodiscard]] Result<ElementBuilder<Element>> createElementBuilder(
		ResolveIdentity&& resolveIdentity
#if FLOW_UI_DEV_MODE
		, std::source_location sourceLocation, bool automaticIdentity = false
#endif
	) {
		auto elementManager = detail::catchResult<Result<std::reference_wrapper<ElementManager>>>(
			ErrorSite::UiManagerAccessElements, [&] { return std::ref(elements()); });
		if (!elementManager) {
			recordFrameError(elementManager.error());
			return unexpectedError(elementManager.error());
		}
		const FlowElementID parentId = currentFlowScope();
		auto resolvedId = detail::catchResult<Result<FlowElementID>>(ErrorSite::UiManagerDefineElement, [&] {
			return std::forward<ResolveIdentity>(resolveIdentity)(parentId);
		});
		if (!resolvedId) {
			recordFrameError(resolvedId.error());
			return unexpectedError(resolvedId.error());
		}
		try {
			return ElementBuilder<Element>(*this, elementManager->get(), windowId(), parentId, *resolvedId
#if FLOW_UI_DEV_MODE
				, sourceLocation, automaticIdentity
#endif
			);
		} catch (...) {
			recordFrameError(detail::currentExceptionError(ErrorSite::UiManagerDefineElement));
			throw;
		}
	}

	mutable FlowUiError frameError_{};
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
	void initStorage(detail::storage::StorageBinding& storage, WindowId window,
					 const AppConfig& config);
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
	detail::storage::StorageBinding* storage_ = nullptr;
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
