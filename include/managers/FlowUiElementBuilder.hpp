#pragma once

#include <cstdint>
#include <stdexcept>
#include <string>
#include <string_view>
#include <type_traits>
#include <utility>

#include "FlowUi/App.hpp"
#include "FlowUi/BuildConfig.hpp"
#include "clay.h"
#include "internal/FlowUiElementBridge.hpp"
#include "managers/ElementManager.hpp"
#include "managers/structs/FlowUiElementStructs.hpp"
#if FLOW_UI_DEV_MODE
#include "devMode/elementDevCapture.hpp"
#endif

namespace FlowUi {

/** @addtogroup flowui_element_system
 * @{
 */

/**
 * @brief builder returned by UiManager::createElement().
 *
 * ElementBuilder stores the element definition, element id, and parameter
 * values for one element invocation. The builder is normally created through
 * UiManager::createElement(); users should not need to construct it directly.
 *
 * The final call is usually draw() or construct(). draw() executes the element's
 * draw flow immediately. construct() opens and configures the element root for
 * a manual closing flow.
 *
 * @tparam Parameters Parameter struct used by the element definition.
 * @tparam State State struct used by the element definition.
 * @tparam Resources Resources struct used by the element definition.
 * @tparam DefinitionId Compile-time id used by the element definition.
 * @tparam IsDevInternal Whether the element definition is internal to FlowUi
 * dev tooling.
 *
 * @see @ref md_docs_2concepts_2element__system "Element System"
 * @see @ref md_docs_2tutorials_2custom__elements "Custom Elements"
 * @see @ref md_docs_2tutorials_2developer__mode "Developer Mode"
 */
template <
	typename Parameters = NoElementParameters,
	typename State = void,
	typename Resources = void,
	uint64_t DefinitionId = 0,
	bool IsDevInternal = false>
class ElementBuilder {
public:
	/** @brief Element definition type used by this builder. */
	using DefinitionType = ElementDefinition<Parameters, State, Resources, DefinitionId, IsDevInternal>;

	/** @brief Parameter struct stored by this builder. */
	using ParametersType = typename DefinitionType::ParametersType;

	/** @brief Build callback context type. */
	using BuildContext = typename DefinitionType::BuildContext;

	/** @brief Interaction callback context type. */
	using InteractionContext = typename DefinitionType::InteractionContext;

	/**
	 * @brief Construct an element builder.
	 *
	 * The constructor stores references and values used by draw() or construct().
	 * UiManager::createElement() is the intended public entry point because it
	 * supplies the correct UiManager, element definition, and dev-mode source
	 * location data.
	 * 
	 * Intended Usage Example:
	 * @code{.cpp}
	 *   FlowUi::UiManager& ui = app.ui();
	 * 	 ui.createElement(kExampleElement, "ExampleId").draw();
	 * @endcode	
	 *
	 * @param uiManager UI manager that owns the active frame.
	 * @param definition Element definition used for callbacks.
	 * @param elementID Flow element id string for this builder invocation.
	 * @param sourceLocation Source location captured for developer-mode
	 * inspection when FLOW_UI_DEV_MODE is enabled.
	 */
	ElementBuilder(UiManager& uiManager, const DefinitionType* definition, std::string elementID
#if FLOW_UI_DEV_MODE
		, devMode::elementCapture::SourceLocation sourceLocation = devMode::elementCapture::SourceLocation::current()
#endif
		) :
		uiManager_(uiManager),
		elementDefinition_(definition),
		elementID_(std::move(elementID)),
		captureAsDevInternal_(DefinitionType::isDevInternal)
#if FLOW_UI_DEV_MODE
		, sourceLocation_(sourceLocation)
#endif
		{}

	/**
	 * @brief Replace element parameters by copy.
	 *
	 * The copied parameters are stored in the builder and are passed by reference
	 * to interaction and build callbacks when draw() or construct() is called.
	 *
	 * @param parameters Parameter values to copy into the builder.
	 * @return Reference to this builder for fluent chaining.
	 *
	 * @code{.cpp}
	 * ui.createElement(kButton, "submit")
	 *     .setParameters(ButtonParams{
	 *         .label = "Submit",
	 *         .enabled = true,
	 *     })
	 *     .draw();
	 * @endcode
	 */
	ElementBuilder& setParameters(const ParametersType& parameters)
	{
		params_ = parameters;
		return *this;
	}

	/**
	 * @brief Replace element parameters by move.
	 *
	 * The moved parameters are stored in the builder and are passed by reference
	 * to interaction and build callbacks when draw() or construct() is called.
	 *
	 * @param parameters Parameter values to move into the builder.
	 * @return Reference to this builder for fluent chaining.
	 *
	 * @code{.cpp}
	 * ButtonParams params{};
	 * params.label = makeDynamicLabel();
	 *
	 * ui.createElement(kButton, "dynamic-button")
	 *     .setParameters(std::move(params))
	 *     .draw();
	 * @endcode
	 */
	ElementBuilder& setParameters(ParametersType&& parameters)
	{
		params_ = std::move(parameters);
		return *this;
	}

	/**
	 * @brief Mutate stored element parameters with a callable.
	 *
	 * mergeParams invokes mergeFn with ParametersType&. This is useful when the
	 * default parameter values are mostly correct and only a few fields need to
	 * be changed.
	 *
	 * @tparam MergeFn Callable type invocable with ParametersType&.
	 * @param mergeFn Callable that mutates the builder's parameter storage.
	 * @return Reference to this builder for fluent chaining.
	 *
	 * @warning mergeFn must be invocable with ParametersType&; otherwise the
	 * static_assert fails at compile time.
	 *
	 * @code{.cpp}
	 * ui.createElement(kButton, "submit")
	 *     .mergeParams([](ButtonParams& params) {
	 *         params.enabled = false;
	 *     })
	 *     .draw();
	 * @endcode
	 */
	template <typename MergeFn>
	ElementBuilder& mergeParams(MergeFn&& mergeFn)
	{
		static_assert(
			std::is_invocable_v<MergeFn, ParametersType&>,
			"FlowUi: mergeParams expects a callable that can be invoked with ParametersType&.");
		std::forward<MergeFn>(mergeFn)(params_);
		return *this;
	}

	/**
	 * @brief Replace the element id string stored by the builder.
	 *
	 * This changes the Flow element id used by later draw() or construct()
	 * calls. It is most useful when an ElementBuilder is stored in a local
	 * variable and emitted later with an id chosen after the builder was created.
	 *
	 * @param elementID Replacement Flow element id string.
	 * @return Reference to this builder for fluent chaining.
	 *
	 * @code{.cpp}
	 * auto button = ui.createElement(kButton, "pending-button")
	 *     .setParameters(ButtonParams{.label = "Open"});
	 *
	 * const std::string buttonId = isPrimary ? "toolbar/open-primary" : "toolbar/open-secondary";
	 * button.withElementID(buttonId).draw();
	 * @endcode
	 */
	ElementBuilder& withElementID(std::string_view elementID)
	{
		elementID_.assign(elementID.data(), elementID.size());
		return *this;
	}

	/**
	 * @brief Control whether this invocation is captured as internal developer UI.
	 *
	 * The value is passed to the dev capture runtime when FLOW_UI_DEV_MODE is
	 * enabled. By default it is initialized from DefinitionType::isDevInternal.
	 *
	 * @param isDevInternal true to mark this invocation as dev-internal.
	 * @return Reference to this builder for fluent chaining.
	 *
	 * @note This is mainly for FlowUi internal dev tooling. Normal user elements
	 * should not need to call this function.
	 *
	 * @code{.cpp}
	 * ui.createElement(kDevOverlay, "flowui/dev/overlay")
	 *     .setDevInternalCapture(true)
	 *     .draw();
	 * @endcode
	 */
	ElementBuilder& setDevInternalCapture(bool isDevInternal = true)
	{
		captureAsDevInternal_ = isDevInternal;
		return *this;
	}

	/**
	 * @brief Run callbacks and open a constructed Clay element.
	 *
	 * construct() executes enabled event callbacks, then runLogic, then
	 * constructElement. constructElement returns the root Clay declaration; the
	 * builder opens and configures that root using the builder's element id.
	 * 
	 * nodes created after Construct() call and before drawConstructed() will be interpreted as children of the "Constructed" element
	 *
	 * @param options Callback phases to skip for this invocation.
	 *
	 * @throws std::runtime_error if the definition pointer is null or the
	 * definition has no constructElement callback.
	 *
	 * @code{.cpp}
	 * ui.createElement(kPanel, "settings-panel")
	 *     .setParameters(PanelParams{.backgroundColor = FlowUi::Flow_Color("#202020ff")})
	 *     .construct();
	 *
	 * Clay_ElementDeclaration child{};
	 * child.layout.sizing.width = CLAY_SIZING_GROW(0);
	 * CLAY(child) {}
	 * ui.drawConstructed();
	 * @endcode
	 */
	void construct(ElementDrawOptions options = ElementDrawOptions::Default);

	/**
	 * @brief Run callbacks and emit this element's draw flow.
	 *
	 * draw() executes enabled event callbacks, then runLogic, then buildElement
	 * unless ElementDrawOptions::SkipBuildCallback is set. buildElement owns the
	 * Clay emission for this element.
	 *
	 * @param options Callback phases to skip for this invocation.
	 *
	 * @throws std::runtime_error if the definition pointer is null or the
	 * definition has no buildElement callback.
	 *
	 * @code{.cpp}
	 * ui.createElement(kButton, "submit")
	 *     .setParameters(ButtonParams{.label = "Submit"})
	 *     .draw();
	 * @endcode
	 */
	void draw(ElementDrawOptions options = ElementDrawOptions::Default);

private:
	// transitional: temporary builder bridge registers the current ElementDefinition specialization until the concept-based builder carries a normalized descriptor directly.
	void ensureDefinitionRegistered()
	{
		detail::ensureElementDefinitionRegistered(
			uiManager_, elementDescriptor<DefinitionType>);
	}

	UiManager& uiManager_;
	const DefinitionType* elementDefinition_;
	std::string elementID_;
	ParametersType params_{};
	bool captureAsDevInternal_ = false;
#if FLOW_UI_DEV_MODE
	devMode::elementCapture::SourceLocation sourceLocation_{};
#endif
};

template <typename Parameters, typename State, typename Resources, uint64_t DefinitionId, bool IsDevInternal>
void ElementBuilder<Parameters, State, Resources, DefinitionId, IsDevInternal>::construct(ElementDrawOptions options)
{
	if (!elementDefinition_ || !elementDefinition_->constructElement) {
		throw std::runtime_error("FlowUi: elementDefinition is null or missing constructElement callback.");
	}
#if FLOW_UI_DEV_MODE
	const uint64_t elementFlowId = toFlowId(elementID_);
	// transitional: temporary dev-only invocation hook claims the current builder root until normalized concept dispatch owns the claim directly.
	detail::claimFlowRootForDev(
		uiManager_,
		elementFlowId,
		DefinitionType::definitionId,
		elementID_,
		sourceLocation_.file_name(),
		static_cast<uint32_t>(sourceLocation_.line()),
		static_cast<uint32_t>(sourceLocation_.column()),
		sourceLocation_.function_name());
#endif
	// transitional: temporary invocation hook auto-registers the current builder's definition until normalized concept dispatch owns registration.
	ensureDefinitionRegistered();

	const Clay_ElementId rootElementId = detail::toClayElementId(uiManager_, elementID_);
#if FLOW_UI_DEV_MODE
	const std::size_t captureIndex = detail::devModeBridge::beginCapturedFlowElement(
		uiManager_,
		DefinitionType::definitionId,
		devMode::typeHash<DefinitionType>(),
		devMode::typeToken<DefinitionType>(),
		elementID_,
		elementFlowId,
		captureAsDevInternal_);
	if (captureIndex != devMode::DevRuntime::kInvalidCaptureIndex) {
		(void)devMode::elementCapture::runtime(uiManager_).setCapturedElementSource(
			captureIndex,
			sourceLocation_.file_name(),
			static_cast<uint32_t>(sourceLocation_.line()),
			static_cast<uint32_t>(sourceLocation_.column()),
			sourceLocation_.function_name());
	}

	struct DevCaptureRollback {
		UiManager& uiManager;
		bool enabled = true;
		~DevCaptureRollback() {
			if (enabled) {
				(void)detail::devModeBridge::endCapturedFlowElement(uiManager);
			}
		}
	} devCaptureRollback{uiManager_, true};
#endif

	if (!elementDrawOptionsHas(options, ElementDrawOptions::SkipEventCallbacks)) {
		const InteractionSnapshot& previousInteraction = detail::previousInteraction(uiManager_);

		InteractionContext eventContext{
			uiManager_,
			elementID_,
			params_,
			previousInteraction
		};

		if (elementDefinition_->onHovered && previousInteraction.isHovered(rootElementId)) {
			elementDefinition_->onHovered(eventContext);
		}
		if (elementDefinition_->onPressed && previousInteraction.isPressed(rootElementId)) {
			elementDefinition_->onPressed(eventContext);
		}
		if (elementDefinition_->onHeld && previousInteraction.isHeld(rootElementId)) {
			elementDefinition_->onHeld(eventContext);
		}
		if (elementDefinition_->onReleased && previousInteraction.isReleased(rootElementId)) {
			elementDefinition_->onReleased(eventContext);
		}
	}

	if (!elementDrawOptionsHas(options, ElementDrawOptions::SkipLogicCallback) && elementDefinition_->runLogic) {
		const InteractionSnapshot& previousInteraction = detail::previousInteraction(uiManager_);
		InteractionContext logicContext{
			uiManager_,
			elementID_,
			params_,
			previousInteraction
		};
		elementDefinition_->runLogic(logicContext);
	}

#if FLOW_UI_DEV_MODE
	devMode::elementCapture::applyParameterOverrides<ParametersType>(
		uiManager_,
		DefinitionType::definitionId,
		elementFlowId,
		elementID_,
		params_);
#endif

	BuildContext buildContext{
		uiManager_,
		elementID_,
		params_
	};

	Clay_ElementDeclaration declaration = elementDefinition_->constructElement(buildContext);
	Clay__OpenElementWithId(rootElementId);
	Clay__ConfigureOpenElement(declaration);
	detail::pushConstructedElement(uiManager_, rootElementId);
#if FLOW_UI_DEV_MODE
	devCaptureRollback.enabled = false;
#endif
}

template <typename Parameters, typename State, typename Resources, uint64_t DefinitionId, bool IsDevInternal>
void ElementBuilder<Parameters, State, Resources, DefinitionId, IsDevInternal>::draw(ElementDrawOptions options)
{
	if (!elementDefinition_ || !elementDefinition_->buildElement) {
		throw std::runtime_error("FlowUi: elementDefinition is null or missing buildElement callback.");
	}
#if FLOW_UI_DEV_MODE
	const uint64_t elementFlowId = toFlowId(elementID_);
	// transitional: temporary dev-only invocation hook claims the current builder root until normalized concept dispatch owns the claim directly.
	detail::claimFlowRootForDev(
		uiManager_,
		elementFlowId,
		DefinitionType::definitionId,
		elementID_,
		sourceLocation_.file_name(),
		static_cast<uint32_t>(sourceLocation_.line()),
		static_cast<uint32_t>(sourceLocation_.column()),
		sourceLocation_.function_name());
#endif
	// transitional: temporary invocation hook auto-registers the current builder's definition until normalized concept dispatch owns registration.
	ensureDefinitionRegistered();

	const Clay_ElementId rootElementId = detail::toClayElementId(uiManager_, elementID_);
#if FLOW_UI_DEV_MODE
	const std::size_t captureIndex = detail::devModeBridge::beginCapturedFlowElement(
		uiManager_,
		DefinitionType::definitionId,
		devMode::typeHash<DefinitionType>(),
		devMode::typeToken<DefinitionType>(),
		elementID_,
		elementFlowId,
		captureAsDevInternal_);
	if (captureIndex != devMode::DevRuntime::kInvalidCaptureIndex) {
		(void)devMode::elementCapture::runtime(uiManager_).setCapturedElementSource(
			captureIndex,
			sourceLocation_.file_name(),
			static_cast<uint32_t>(sourceLocation_.line()),
			static_cast<uint32_t>(sourceLocation_.column()),
			sourceLocation_.function_name());
	}

	struct DevCaptureCloseOnExit {
		UiManager& uiManager;
		~DevCaptureCloseOnExit() {
			(void)detail::devModeBridge::endCapturedFlowElement(uiManager);
		}
	} devCaptureCloseOnExit{uiManager_};
#endif

	if (!elementDrawOptionsHas(options, ElementDrawOptions::SkipEventCallbacks)) {
		const InteractionSnapshot& previousInteraction = detail::previousInteraction(uiManager_);

		InteractionContext eventContext{
			uiManager_,
			elementID_,
			params_,
			previousInteraction
		};

		if (elementDefinition_->onHovered && previousInteraction.isHovered(rootElementId)) {
			elementDefinition_->onHovered(eventContext);
		}
		if (elementDefinition_->onPressed && previousInteraction.isPressed(rootElementId)) {
			elementDefinition_->onPressed(eventContext);
		}
		if (elementDefinition_->onHeld && previousInteraction.isHeld(rootElementId)) {
			elementDefinition_->onHeld(eventContext);
		}
		if (elementDefinition_->onReleased && previousInteraction.isReleased(rootElementId)) {
			elementDefinition_->onReleased(eventContext);
		}
	}

	if (!elementDrawOptionsHas(options, ElementDrawOptions::SkipLogicCallback) && elementDefinition_->runLogic) {
		const InteractionSnapshot& previousInteraction = detail::previousInteraction(uiManager_);
		InteractionContext logicContext{
			uiManager_,
			elementID_,
			params_,
			previousInteraction
		};
		elementDefinition_->runLogic(logicContext);
	}

	if (!elementDrawOptionsHas(options, ElementDrawOptions::SkipBuildCallback)) {
#if FLOW_UI_DEV_MODE
		devMode::elementCapture::applyParameterOverrides<ParametersType>(
			uiManager_,
			DefinitionType::definitionId,
			elementFlowId,
			elementID_,
			params_);
#endif
		BuildContext buildContext{
			uiManager_,
			elementID_,
			params_
		};
		elementDefinition_->buildElement(buildContext);
	}
}

/** @} */

} // namespace FlowUi
