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
#include "managers/structs/FlowUiElementStructs.hpp"
#if FLOW_UI_DEV_MODE
#include "devMode/elementDevCapture.hpp"
#endif

namespace FlowUi {

/** @addtogroup flowui_element_system
 * @{
 */

/** @brief Fluent builder returned by UiManager::createElement(). */
template <
	typename Parameters = NoElementParameters,
	typename State = void,
	typename Resources = void,
	uint64_t DefinitionId = 0,
	bool IsDevInternal = false>
class ElementBuilder {
public:
	/** @brief Element definition type. */
	using DefinitionType = ElementDefinition<Parameters, State, Resources, DefinitionId, IsDevInternal>;
	/** @brief Resolved parameter type. */
	using ParametersType = typename DefinitionType::ParametersType;
	/** @brief Build callback context type. */
	using BuildContext = typename DefinitionType::BuildContext;
	/** @brief Interaction callback context type. */
	using InteractionContext = typename DefinitionType::InteractionContext;

	/** @brief Construct an element builder. */
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

	/** @brief Replace element parameters. */
	ElementBuilder& setParameters(const ParametersType& parameters)
	{
		params_ = parameters;
		return *this;
	}

	/** @brief Move-replace element parameters. */
	ElementBuilder& setParameters(ParametersType&& parameters)
	{
		params_ = std::move(parameters);
		return *this;
	}

	/** @brief Mutate element parameters with a callable. */
	template <typename MergeFn>
	ElementBuilder& mergeParams(MergeFn&& mergeFn)
	{
		static_assert(
			std::is_invocable_v<MergeFn, ParametersType&>,
			"FlowUi: mergeParams expects a callable that can be invoked with ParametersType&.");
		std::forward<MergeFn>(mergeFn)(params_);
		return *this;
	}

	/** @brief Replace the element id string. */
	ElementBuilder& withElementID(std::string_view elementID)
	{
		elementID_.assign(elementID.data(), elementID.size());
		return *this;
	}

	/** @brief Control whether this element is captured as internal developer UI. */
	ElementBuilder& setDevInternalCapture(bool isDevInternal = true)
	{
		captureAsDevInternal_ = isDevInternal;
		return *this;
	}

	/** @brief Construct and push a Clay element declaration. */
	void construct(ElementDrawOptions options = ElementDrawOptions::Default);
	/** @brief Run callbacks and draw the element. */
	void draw(ElementDrawOptions options = ElementDrawOptions::Default);

private:
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

	const Clay_ElementId rootElementId = detail::toClayElementId(uiManager_, elementID_);
	const uint64_t elementFlowId = toFlowId(elementID_);
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

	const Clay_ElementId rootElementId = detail::toClayElementId(uiManager_, elementID_);
	const uint64_t elementFlowId = toFlowId(elementID_);
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
