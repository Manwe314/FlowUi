#pragma once

#include <cstddef>
#include <cstdint>
#include <memory>
#include <source_location>
#include <stdexcept>
#include <string_view>
#include <type_traits>
#include <utility>

#include "FlowUi/Public/App.hpp"
#include "FlowUi/Public/BuildConfig.hpp"
#include "FlowUi/Public/elementAuthoring/ElementContexts.hpp"
#include "FlowUi/Public/managers/ActionManager.hpp"
#include "FlowUi/Public/managers/ElementManager.hpp"
#include "FlowUi/Public/managers/UiManager.hpp"
#include "clay.h"
#include "devSystems/devMonitoringAndReporting/timing/DevTimingZone.hpp"
#include "devSystems/devTooling/bake/DevBakedChanges.hpp"
#if FLOW_UI_DEV_MODE
#include "FlowUi/Private/TypeOperations.hpp"
#include "FlowUi/Private/elementSystem/FlowUiElementBridge.hpp"
#endif

namespace FlowUi {

/** @addtogroup flowui_element_system
 * @{
 */

template <typename Element>
Clay_ElementId ElementBuildContext<Element>::clayID() const {
	return uiManager.toClayEID(id);
}

#if FLOW_UI_DEV_MODE
template <typename Element>
devSystems::DevTimingRecorder* ElementBuildContext<Element>::devTimingRecorder() const noexcept {
	return uiManager.devTimingRecorder();
}
#endif

template <typename Element>
FlowElementID ElementBuildContext<Element>::childID(LocalElementName name) const {
	return uiManager.resolveLocalElementID(id, ElementType::definitionId, name);
}

template <typename Element>
FlowElementID ElementBuildContext<Element>::childID(RuntimeElementName name) const {
	return uiManager.resolveLocalElementID(id, ElementType::definitionId, name);
}

template <typename Element>
FlowElementID ElementBuildContext<Element>::childID(IndexedElementName name) const {
	return uiManager.resolveIndexedElementID(id, ElementType::definitionId, name);
}

template <typename Element>
Clay_ElementId ElementBuildContext<Element>::clayID(LocalElementName name) const {
	return uiManager.toClayEID(childID(name));
}

template <typename Element>
Clay_ElementId ElementBuildContext<Element>::clayID(RuntimeElementName name) const {
	return uiManager.toClayEID(childID(name));
}

template <typename Element>
Clay_ElementId ElementBuildContext<Element>::clayID(IndexedElementName name) const {
	return uiManager.toClayEID(childID(name));
}

template <typename Element>
FlowElementPartID ElementBuildContext<Element>::part(FlowElementPart declaration) const {
	return uiManager.resolveElementPartID(ElementType::definitionId, id, declaration);
}

template <typename Element>
template <FlowElement PartElement>
ElementBuilder<PartElement> ElementBuildContext<Element>::createPart(
	const PartElement& element,
	FlowElementPart declaration) const {
	return uiManager.createElement(element, part(declaration));
}

template <typename Element>
ActionInvocationStatus ElementInteractionContext<Element>::invoke(ActionCall call) {
	return uiManager.actions().invoke(call, ActionInvocationSource{
		.kind = actionInvocationSourceKind_,
		.window = uiManager.windowId(),
		.element = id,
	});
}

template <typename Element>
ActionAvailability ElementInteractionContext<Element>::actionAvailability(
	ActionCall call) const {
	return uiManager.actions().availability(call);
}

template <typename Element>
Clay_ElementId ElementInteractionContext<Element>::clayID() const {
	return uiManager.toClayEID(id);
}

template <typename Element>
FlowElementID ElementInteractionContext<Element>::childID(LocalElementName name) const {
	return uiManager.resolveLocalElementID(id, ElementType::definitionId, name);
}

template <typename Element>
FlowElementID ElementInteractionContext<Element>::childID(RuntimeElementName name) const {
	return uiManager.resolveLocalElementID(id, ElementType::definitionId, name);
}

template <typename Element>
FlowElementID ElementInteractionContext<Element>::childID(IndexedElementName name) const {
	return uiManager.resolveIndexedElementID(id, ElementType::definitionId, name);
}

template <typename Element>
Clay_ElementId ElementInteractionContext<Element>::clayID(LocalElementName name) const {
	return uiManager.toClayEID(childID(name));
}

template <typename Element>
Clay_ElementId ElementInteractionContext<Element>::clayID(RuntimeElementName name) const {
	return uiManager.toClayEID(childID(name));
}

template <typename Element>
Clay_ElementId ElementInteractionContext<Element>::clayID(IndexedElementName name) const {
	return uiManager.toClayEID(childID(name));
}

template <typename Element>
FlowElementPartID ElementInteractionContext<Element>::part(FlowElementPart declaration) const {
	return uiManager.resolveElementPartID(ElementType::definitionId, id, declaration);
}

template <typename Element>
template <FlowElement PartElement>
ElementBuilder<PartElement> ElementInteractionContext<Element>::createPart(
	const PartElement& element,
	FlowElementPart declaration) const {
	return uiManager.createElement(element, part(declaration));
}

/**
 * @brief Builder returned by UiManager::createElement().
 *
 * ElementBuilder owns an element instance's logical id and parameter values.
 * Element behavior is selected entirely from ElementType at compile time; the
 * constexpr tag passed to createElement() is never stored.
 * Use the builder during an active frame on its owning UiManager. Borrowed
 * values inside Parameters are not deep-copied and must remain valid through
 * rendering. Builder methods do not queue work: draw() and construct() execute
 * their callbacks synchronously.
 *
 * @tparam Element Complete empty-tag element type satisfying FlowElement.
 */
template <FlowElement Element>
class ElementBuilder {
public:
	/** Complete element definition type selected at compile time. */
	using ElementType = std::remove_cvref_t<Element>;
	/** Builder-owned parameter payload, or NoElementParameters when omitted. */
	using ParametersType = ParametersOf<ElementType>;
	/** Context supplied to buildElement() and constructElement(). */
	using BuildContext = ElementBuildContext<ElementType>;
	/** Context supplied to pointer event hooks and runLogic(). */
	using InteractionContext = ElementInteractionContext<ElementType>;

	/**
	 * @brief Construct one pending typed element invocation.
	 *
	 * UiManager::createElement() is the intended entry point. The builder keeps
	 * direct references to the frame's UiManager and app-owned ElementManager,
	 * plus the WindowId needed to identify per-window state.
	 * @par Arguments
	 * - @p uiManager Manager for the active window frame.
	 * - @p elementManager Application-owned element data manager.
	 * - @p window Window that owns this instance's state.
	 * - @p parentId Captured parent scope used when resolving replacement names.
	 * - @p elementId Resolved nonzero identity for this invocation.
	 * @note Development builds also accept sourceLocation for the creation site and
	 * automaticIdentity to identify automatically generated identities.
	 */
	ElementBuilder(UiManager& uiManager, ElementManager& elementManager, WindowId window,
				   FlowElementID parentId, FlowElementID elementId
#if FLOW_UI_DEV_MODE
				   ,
				   std::source_location sourceLocation = std::source_location::current(),
				   bool automaticIdentity = false
#endif
				   )
		: uiManager_(uiManager), elementManager_(elementManager), window_(window),
		  parentId_(parentId), elementId_(elementId)
#if FLOW_UI_DEV_MODE
		  ,
		  captureAsDevInternal_(detail::element::isDevInternal<ElementType>()),
		  sourceLocation_(sourceLocation), automaticIdentity_(automaticIdentity)
#endif
	{}

	/** Copy parameters into the builder; borrowed members remain borrowed. @return This builder. */
	ElementBuilder& setParameters(const ParametersType& parameters) {
		params_ = parameters;
		return *this;
	}

	/** Move parameters into the builder; borrowed members remain borrowed. @return This builder. */
	ElementBuilder& setParameters(ParametersType&& parameters) {
		params_ = std::move(parameters);
		return *this;
	}

	/**
	 * Mutate builder-owned parameters immediately using a callable.
	 * @param mergeFn Callable invoked once with ParametersType&; exceptions propagate.
	 * @return This builder for chaining.
	 */
	template <typename MergeFn>
	ElementBuilder& mergeParams(MergeFn&& mergeFn) {
		static_assert(
			std::is_invocable_v<MergeFn, ParametersType&>,
			"FlowUi: mergeParams expects a callable invocable with ParametersType&.");
		std::forward<MergeFn>(mergeFn)(params_);
		return *this;
	}

	/** Resolve a replacement local name against the parent captured at creation. */
	ElementBuilder& withID(LocalElementName name) {
		elementId_ = uiManager_.resolveLocalElementID(
			parentId_, ElementType::definitionId, name);
#if FLOW_UI_DEV_MODE
		automaticIdentity_ = false;
#endif
		return *this;
	}

	/** Resolve a runtime-authored replacement name against the captured parent. */
	ElementBuilder& withID(RuntimeElementName name) {
		elementId_ = uiManager_.resolveLocalElementID(
			parentId_, ElementType::definitionId, name);
#if FLOW_UI_DEV_MODE
		automaticIdentity_ = false;
#endif
		return *this;
	}

	/** Resolve an indexed/keyed replacement against the captured parent. */
	ElementBuilder& withID(IndexedElementName name) {
		elementId_ = uiManager_.resolveIndexedElementID(
			parentId_, ElementType::definitionId, name);
#if FLOW_UI_DEV_MODE
		automaticIdentity_ = false;
#endif
		return *this;
	}

	/** Resolve an automatic replacement against the captured parent. */
	ElementBuilder& withID(AutoElementName name) {
		elementId_ = uiManager_.resolveAutomaticElementID(
			parentId_, ElementType::definitionId, name);
#if FLOW_UI_DEV_MODE
		automaticIdentity_ = true;
#endif
		return *this;
	}

	/**
	 * Replace the builder identity with an already resolved local ID.
	 * @param id Nonzero resolved identity.
	 * @return This builder for chaining.
	 * @throws FlowUiException If id is invalid.
	 */
	ElementBuilder& withID(FlowElementID id) {
		if (!id) throw FlowUiException(makeError(ErrorCode::InvalidElementId, ErrorSite::UiManagerDefineElement));
		elementId_ = id;
#if FLOW_UI_DEV_MODE
		automaticIdentity_ = false;
#endif
		return *this;
	}

	/**
	 * Replace the builder identity with an explicitly global address.
	 * @param id Nonzero global address, normalized for this window.
	 * @return This builder for chaining.
	 * @throws FlowUiException If id is invalid.
	 */
	ElementBuilder& withID(GlobalFlowID id) {
		if (!id) throw FlowUiException(makeError(ErrorCode::InvalidElementId, ErrorSite::UiManagerDefineElement));
		elementId_ = uiManager_.normalizeGlobalElementID(id);
#if FLOW_UI_DEV_MODE
		automaticIdentity_ = false;
#endif
		return *this;
	}

	/**
	 * Replace the builder identity with a part address already bound to its owner.
	 * @param id Nonzero semantic part address.
	 * @return This builder for chaining.
	 * @throws FlowUiException If id is invalid.
	 */
	ElementBuilder& withID(FlowElementPartID id) {
		if (!id) throw FlowUiException(makeError(ErrorCode::InvalidElementId, ErrorSite::UiManagerDefineElement));
		elementId_ = uiManager_.normalizePartElementID(id);
#if FLOW_UI_DEV_MODE
		automaticIdentity_ = false;
#endif
		return *this;
	}

	/** Override whether this invocation is captured as internal developer UI. */
	ElementBuilder& setDevInternalCapture(bool isDevInternal = true) noexcept {
#if FLOW_UI_DEV_MODE
		captureAsDevInternal_ = isDevInternal;
#else
		(void)isDevInternal;
#endif
		return *this;
	}

	/**
	 * Run the common callback pipeline and open this element's Clay root.
	 * Children emitted afterward remain nested until UiManager::drawConstructed().
	 * Enabled pointer hooks run before runLogic(), then constructElement().
	 * @param options Callback phases to skip; SkipBuildCallback does not skip construction.
	 * @throws FlowUiException If definition registration or managed data acquisition fails.
	 */
	void construct(ElementDrawOptions options = ElementDrawOptions::Default)
		requires ConstructibleFlowElement<ElementType> {
		invoke<OutputMode::Construct>(options);
	}

	/**
	 * Execute enabled pointer hooks, runLogic(), then buildElement() synchronously.
	 * @param options Callback phases to skip for this invocation.
	 * @throws FlowUiException If definition registration or managed data acquisition fails.
	 */
	void draw(ElementDrawOptions options = ElementDrawOptions::Default)
		requires DrawableFlowElement<ElementType> {
		invoke<OutputMode::Draw>(options);
	}

private:
	enum class OutputMode : uint8_t {
		Draw,
		Construct,
	};

	struct FlowScopeGuard {
		UiManager* uiManager = nullptr;
		size_t priorDepth = 0;
		bool active = true;

		FlowScopeGuard(UiManager& ui, FlowElementID id)
			: uiManager(&ui), priorDepth(ui.pushFlowScope(id)) {}

		~FlowScopeGuard() {
			if (active && uiManager) uiManager->restoreFlowScope(priorDepth);
		}

		void release() noexcept { active = false; }
	};

	struct ConstructedDepthGuard {
		UiManager* uiManager = nullptr;
		size_t baseline = 0;
		bool active = true;

		explicit ConstructedDepthGuard(UiManager& ui)
			: uiManager(&ui), baseline(ui.constructedElementDepth()) {}

		~ConstructedDepthGuard() {
			if (active && uiManager) uiManager->closeConstructedToDepth(baseline, true);
		}

		void release() noexcept { active = false; }
	};

	static constexpr bool hasEventHooks =
		detail::element::HasOnHoveredHook<ElementType> ||
		detail::element::HasOnPressedHook<ElementType> ||
		detail::element::HasOnHeldHook<ElementType> ||
		detail::element::HasOnReleasedHook<ElementType>;

	static constexpr bool hasInteractionHooks =
		hasEventHooks || detail::element::HasRunLogicHook<ElementType>;

#if FLOW_UI_DEV_MODE
	struct DevCaptureScope {
		UiManager* uiManager = nullptr;
		devSystems::tooling::DevTreeCapture::Token token{};
		bool closeOnDestruction = true;

		~DevCaptureScope() {
			if (closeOnDestruction && uiManager) {
				detail::dev_tooling_bridge::endCapturedFlowElement(*uiManager, token);
			}
		}

		void leaveOpen() noexcept { closeOnDestruction = false; }
	};

	struct DevInternalScopeGuard {
		UiManager* uiManager = nullptr;
		bool active = false;

		DevInternalScopeGuard(UiManager& ui, bool enter) noexcept
			: uiManager(&ui), active(enter) {
			if (active) {
				uiManager->enterDevInternalScope();
			}
		}

		~DevInternalScopeGuard() {
			if (active && uiManager) {
				uiManager->leaveDevInternalScope();
			}
		}

		void release() noexcept { active = false; }
	};
#endif

	template <OutputMode Mode>
	void invoke(ElementDrawOptions options) {
		FlowScopeGuard flowScope{uiManager_, elementId_};

#if FLOW_UI_DEV_MODE
		if (uiManager_.isDevPassivePreview())
			options = options | ElementDrawOptions::SkipEventCallbacks |
					  ElementDrawOptions::SkipLogicCallback;
		if constexpr (!detail::element::isDevInternal<ElementType>())
			if (auto* registry = uiManager_.devSchemaRegistry())
				registry->template registerElementPreview<ElementType>();
		// Route header-only builder capture through the internal runtime bridge.
		detail::claimFlowRootForDev(
			uiManager_,
			elementId_,
			ElementType::definitionId,
			sourceLocation_.file_name(),
			static_cast<uint32_t>(sourceLocation_.line()),
			static_cast<uint32_t>(sourceLocation_.column()),
			sourceLocation_.function_name(),
			automaticIdentity_);
#endif

#if FLOW_UI_DEV_MODE && FLOWUI_DEV_TIMING_LEVEL >= 1
		devSystems::ElementTimingZone invocationTiming(
			uiManager_.devTimingRecorder(), ElementType::definitionId, elementId_);
#endif
		auto invocation = [&]() {
#if FLOW_UI_DEV_MODE
			FLOWUI_DEV_TIMING_ZONE_DEEP_IF(
				uiManager_.devTimingRecorder(), devSystems::TimingCategory::Element,
				devSystems::TimingZoneRole::Work, "flowui.element.registration_state");
#endif
			return detail::element::ElementInvocation<ElementType>::begin(
				elementManager_, uiManager_, window_, elementId_);
		}();

#if FLOW_UI_DEV_MODE
		const bool isDevInternal =
			captureAsDevInternal_ || uiManager_.isDevInternalScope();
		DevInternalScopeGuard devInternalGuard{uiManager_, isDevInternal};
		auto* overrides = uiManager_.devOverrideEngine();
		auto authored_parameters = !isDevInternal && overrides
			? overrides->captureAuthoredParameters(ElementType::definitionId, std::addressof(params_))
			: devSystems::tooling::DevAuthoredElementCapture{};
#endif

#if FLOW_UI_HAS_BAKED_CHANGES
#if FLOW_UI_DEV_MODE
		if (!isDevInternal)
#endif
			baked::applyBakedElementDefaults<ElementType>(
				detail::element::toInstanceKey(elementId_), params_);
#endif

#if FLOW_UI_DEV_MODE
		if (!isDevInternal && overrides) {
			overrides->applyElement(ElementType::definitionId, window_,
				detail::element::toInstanceKey(elementId_), std::addressof(params_),
				uiManager_.devTimingRecorder());
		}
#endif

		Clay_ElementId rootElementId{};
		if constexpr (Mode == OutputMode::Construct) {
			rootElementId = uiManager_.toClayEID(elementId_);
		}

		ConstructedDepthGuard constructedDepth{uiManager_};

		{
#if FLOW_UI_DEV_MODE
			FLOWUI_DEV_TIMING_ZONE_BALANCED_IF(
				uiManager_.devTimingRecorder(), devSystems::TimingCategory::Element,
				devSystems::TimingZoneRole::Work, "flowui.element.interaction_hooks");
#endif
			invokeInteractionHooks(invocation, elementId_, options);
		}

		if constexpr (Mode == OutputMode::Draw) {
			if (elementDrawOptionsHas(options, ElementDrawOptions::SkipBuildCallback)) {
#if FLOW_UI_DEV_MODE && FLOWUI_DEV_TIMING_LEVEL >= 1
				invocationTiming.end();
#endif
				return;
			}
		}
		BuildContext buildContext{invocation, params_};
#if FLOW_UI_DEV_MODE
		const auto treeToken = detail::dev_tooling_bridge::beginCapturedFlowElement(
			uiManager_, ElementType::definitionId, detail::element::debugName<ElementType>(),
			detail::typeToken<ElementType>(), elementId_, isDevInternal,
			Mode == OutputMode::Construct, sourceLocation_.file_name(),
			static_cast<uint32_t>(sourceLocation_.line()),
			static_cast<uint32_t>(sourceLocation_.column()), sourceLocation_.function_name());
		DevCaptureScope devCapture{&uiManager_, treeToken};
#endif
		if constexpr (Mode == OutputMode::Draw) {
			{
#if FLOW_UI_DEV_MODE
				FLOWUI_DEV_TIMING_ZONE_BALANCED_IF(
					uiManager_.devTimingRecorder(), devSystems::TimingCategory::Element,
					devSystems::TimingZoneRole::Work, "flowui.element.build_callback");
#endif
				ElementType::buildElement(buildContext);
			}
#if FLOW_UI_DEV_MODE
			captureEffectiveParameters(treeToken, invocation, std::move(authored_parameters));
#endif
		} else {
			Clay_ElementDeclaration declaration{};
			{
#if FLOW_UI_DEV_MODE
				FLOWUI_DEV_TIMING_ZONE_BALANCED_IF(
					uiManager_.devTimingRecorder(), devSystems::TimingCategory::Element,
					devSystems::TimingZoneRole::Work, "flowui.element.construct_callback");
#endif
				declaration = ElementType::constructElement(buildContext);
			}
#if FLOW_UI_DEV_MODE
			captureEffectiveParameters(treeToken, invocation, std::move(authored_parameters));
#endif
			if (uiManager_.constructedElementDepth() != constructedDepth.baseline) {
				detail::terminateForFatalError(makeError(ErrorCode::InternalInvariantBroken, ErrorSite::ElementInvoke));
			}
#if FLOW_UI_DEV_MODE && FLOWUI_DEV_TIMING_LEVEL >= 1
			invocationTiming.end();
#endif
			uiManager_.retainConstructedElement(
				rootElementId, elementId_, flowScope.priorDepth
#if FLOW_UI_DEV_MODE
				, treeToken
				, isDevInternal
#endif
#if FLOW_UI_DEV_MODE && FLOWUI_DEV_TIMING_LEVEL >= 2
				, ElementType::definitionId
#endif
			);
			Clay__OpenElementWithId(rootElementId);
			Clay__ConfigureOpenElement(declaration);
			constructedDepth.release();
			flowScope.release();
#if FLOW_UI_DEV_MODE
			devCapture.leaveOpen();
#endif
		}
#if FLOW_UI_DEV_MODE && FLOWUI_DEV_TIMING_LEVEL >= 1
		if constexpr (Mode == OutputMode::Draw) invocationTiming.end();
#endif
	}

#if FLOW_UI_DEV_MODE
	void captureEffectiveParameters(
		devSystems::tooling::DevTreeCapture::Token treeToken,
		const detail::element::ElementInvocation<ElementType>& invocation,
		devSystems::tooling::DevAuthoredElementCapture authored_parameters) noexcept {
		if (!treeToken || treeToken.node == devSystems::tooling::InvalidFlowNode) return;
		if (auto* overrides = uiManager_.devOverrideEngine()) {
			overrides->captureElement(
				ElementType::definitionId,
				window_,
				detail::element::toInstanceKey(elementId_),
				treeToken.node,
				std::addressof(params_),
				uiManager_.devTimingRecorder(), std::move(authored_parameters),
				invocation.capturedStatePayload(), invocation.capturedResourcesPayload(),
				uiManager_.isDevInternalScope());
		}
	}
#endif

	void invokeInteractionHooks(
		detail::element::ElementInvocation<ElementType>& invocation,
		FlowElementID elementId,
		ElementDrawOptions options) {
		if constexpr (!hasInteractionHooks) {
			(void)invocation;
			(void)elementId;
			(void)options;
			return;
		} else {
			bool invokeEvents = false;
			bool invokeLogic = false;
			if constexpr (hasEventHooks) {
				invokeEvents = !elementDrawOptionsHas(
					options, ElementDrawOptions::SkipEventCallbacks);
			}
			if constexpr (detail::element::HasRunLogicHook<ElementType>) {
				invokeLogic = !elementDrawOptionsHas(
					options, ElementDrawOptions::SkipLogicCallback);
			}
			if (!invokeEvents && !invokeLogic) return;

			const InteractionSnapshot& previousInteraction =
				uiManager_.getPreviousFramesInteraction();
			InteractionContext context{
				invocation, params_, previousInteraction};

			if (invokeEvents) {
				if constexpr (detail::element::HasOnHoveredHook<ElementType>) {
					if (previousInteraction.isHovered(elementId)) {
						FLOWUI_DEV_TIMING_ZONE_DEEP_IF(
							uiManager_.devTimingRecorder(), devSystems::TimingCategory::Element,
							devSystems::TimingZoneRole::Work, "flowui.element.on_hovered");
						context.setActionInvocationSourceKind(
							ActionInvocationSourceKind::ElementHovered);
						ElementType::onHovered(context);
					}
				}
				if constexpr (detail::element::HasOnPressedHook<ElementType>) {
					if (previousInteraction.isPressed(elementId)) {
						FLOWUI_DEV_TIMING_ZONE_DEEP_IF(
							uiManager_.devTimingRecorder(), devSystems::TimingCategory::Element,
							devSystems::TimingZoneRole::Work, "flowui.element.on_pressed");
						context.setActionInvocationSourceKind(
							ActionInvocationSourceKind::ElementPressed);
						ElementType::onPressed(context);
					}
				}
				if constexpr (detail::element::HasOnHeldHook<ElementType>) {
					if (previousInteraction.isHeld(elementId)) {
						FLOWUI_DEV_TIMING_ZONE_DEEP_IF(
							uiManager_.devTimingRecorder(), devSystems::TimingCategory::Element,
							devSystems::TimingZoneRole::Work, "flowui.element.on_held");
						context.setActionInvocationSourceKind(
							ActionInvocationSourceKind::ElementHeld);
						ElementType::onHeld(context);
					}
				}
				if constexpr (detail::element::HasOnReleasedHook<ElementType>) {
					if (previousInteraction.isReleased(elementId)) {
						FLOWUI_DEV_TIMING_ZONE_DEEP_IF(
							uiManager_.devTimingRecorder(), devSystems::TimingCategory::Element,
							devSystems::TimingZoneRole::Work, "flowui.element.on_released");
						context.setActionInvocationSourceKind(
							ActionInvocationSourceKind::ElementReleased);
						ElementType::onReleased(context);
					}
				}
			}

			if constexpr (detail::element::HasRunLogicHook<ElementType>) {
				if (invokeLogic) {
					FLOWUI_DEV_TIMING_ZONE_DEEP_IF(
						uiManager_.devTimingRecorder(), devSystems::TimingCategory::Element,
						devSystems::TimingZoneRole::Work, "flowui.element.run_logic");
					context.setActionInvocationSourceKind(
						ActionInvocationSourceKind::ElementLogic);
					ElementType::runLogic(context);
				}
			}
		}
	}

	UiManager& uiManager_;
	ElementManager& elementManager_;
	WindowId window_ = InvalidWindowId;
	FlowElementID parentId_{};
	FlowElementID elementId_{};
	ParametersType params_{};
#if FLOW_UI_DEV_MODE
	bool captureAsDevInternal_ = detail::element::isDevInternal<ElementType>();
	std::source_location sourceLocation_{};
	bool automaticIdentity_ = false;
#endif
};

/** @} */

} // namespace FlowUi

#if FLOW_UI_DEV_MODE
namespace FlowUi::devSystems::tooling {
template <FlowElement Element>
void DevSchemaRegistry::registerElementPreview(std::string_view set_name, bool admitted) {
	const auto found =
		std::ranges::find(element_previews_, Element::definitionId, &ElementPreview::definition);
	if (found != element_previews_.end()) {
		if (admitted && !found->admitted) {
			found->admitted = true;
			++preview_revision_;
		}
		if (!set_name.empty() && found->set_name != set_name) {
			found->set_name = set_name;
			++preview_revision_;
		}
		return;
	}
	element_previews_.emplace_back(ElementPreview{
		std::string(set_name), Element::definitionId,
		+[](UiManager& ui, uint64_t specimen, bool construct) {
			const UiManager::DevPassivePreviewScope passive(ui);
			const auto identity = FlowElementID{specimen};
			ElementBuilder<Element> builder(ui, ui.elements(), ui.windowId(), {}, identity);
			builder.setDevInternalCapture(true);
			if constexpr (ConstructibleFlowElement<Element>) {
				if (construct || !DrawableFlowElement<Element>) {
					builder.construct();
					ui.drawConstructed();
					return;
				}
			}
			if constexpr (DrawableFlowElement<Element>)
				builder.draw();
		},
		admitted, DrawableFlowElement<Element>, ConstructibleFlowElement<Element>});
	++preview_revision_;
}
} // namespace FlowUi::devSystems::tooling
#endif
