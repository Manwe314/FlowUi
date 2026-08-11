#pragma once

#include <cstddef>
#include <cstdint>
#include <string>
#include <string_view>
#include <type_traits>
#include <utility>

#include "FlowUi/App.hpp"
#include "FlowUi/BuildConfig.hpp"
#include "clay.h"
#include "managers/ElementManager.hpp"
#include "managers/UiManager.hpp"
#include "managers/structs/FlowUiElementStructs.hpp"
#if FLOW_UI_DEV_MODE
#include "devMode/elementDevCapture.hpp"
#include "internal/FlowUiElementBridge.hpp"
#endif

namespace FlowUi {

/** @addtogroup flowui_element_system
 * @{
 */

/**
 * @brief Builder returned by UiManager::createElement().
 *
 * ElementBuilder owns an element instance's logical id and parameter values.
 * Element behavior is selected entirely from ElementType at compile time; the
 * constexpr tag passed to createElement() is never stored.
 *
 * @tparam Element Complete empty-tag element type satisfying FlowElement.
 */
template <FlowElement Element>
class ElementBuilder {
public:
	using ElementType = std::remove_cvref_t<Element>;
	using ParametersType = ParametersOf<ElementType>;
	using BuildContext = ElementBuildContext<ElementType>;
	using InteractionContext = ElementInteractionContext<ElementType>;

	/**
	 * @brief Construct one pending typed element invocation.
	 *
	 * UiManager::createElement() is the intended entry point. The builder keeps
	 * direct references to the frame's UiManager and app-owned ElementManager,
	 * plus the WindowId needed to identify per-window state.
	 */
	ElementBuilder(
		UiManager& uiManager,
		ElementManager& elementManager,
		WindowId window,
		std::string elementID
#if FLOW_UI_DEV_MODE
		, devMode::elementCapture::SourceLocation sourceLocation =
			devMode::elementCapture::SourceLocation::current()
#endif
		)
		: uiManager_(uiManager),
		  elementManager_(elementManager),
		  window_(window),
		  elementID_(std::move(elementID))
#if FLOW_UI_DEV_MODE
		, captureAsDevInternal_(detail::element::isDevInternal<ElementType>()),
		  sourceLocation_(sourceLocation)
#endif
		{}

	/** Replace element parameters by copy. */
	ElementBuilder& setParameters(const ParametersType& parameters) {
		params_ = parameters;
		return *this;
	}

	/** Replace element parameters by move. */
	ElementBuilder& setParameters(ParametersType&& parameters) {
		params_ = std::move(parameters);
		return *this;
	}

	/** Mutate the builder-owned parameters with a callable. */
	template <typename MergeFn>
	ElementBuilder& mergeParams(MergeFn&& mergeFn) {
		static_assert(
			std::is_invocable_v<MergeFn, ParametersType&>,
			"FlowUi: mergeParams expects a callable invocable with ParametersType&.");
		std::forward<MergeFn>(mergeFn)(params_);
		return *this;
	}

	/** Replace the logical Flow/Clay root id owned by this builder. */
	ElementBuilder& withElementID(std::string_view elementID) {
		elementID_.assign(elementID.data(), elementID.size());
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
	 */
	void construct(ElementDrawOptions options = ElementDrawOptions::Default)
		requires ConstructibleFlowElement<ElementType> {
		invoke<OutputMode::Construct>(options);
	}

	/** Run the common callback pipeline and invoke ElementType::buildElement(). */
	void draw(ElementDrawOptions options = ElementDrawOptions::Default)
		requires DrawableFlowElement<ElementType> {
		invoke<OutputMode::Draw>(options);
	}

private:
	enum class OutputMode : uint8_t {
		Draw,
		Construct,
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
		bool closeOnDestruction = true;

		~DevCaptureScope() {
			if (closeOnDestruction && uiManager) {
				(void)detail::devModeBridge::endCapturedFlowElement(*uiManager);
			}
		}

		void leaveOpen() noexcept { closeOnDestruction = false; }
	};
#endif

	template <OutputMode Mode>
	void invoke(ElementDrawOptions options) {
		const FlowElementId elementFlowId = toFlowId(elementID_);

#if FLOW_UI_DEV_MODE
		// transitional: temporary dev capture bridge remains until the later dev
		// element/registry migration consolidates capture operations on UiManager.
		detail::claimFlowRootForDev(
			uiManager_,
			elementFlowId,
			ElementType::definitionId,
			elementID_,
			sourceLocation_.file_name(),
			static_cast<uint32_t>(sourceLocation_.line()),
			static_cast<uint32_t>(sourceLocation_.column()),
			sourceLocation_.function_name());
#endif

		auto invocation = detail::element::ElementInvocation<ElementType>::begin(
			elementManager_, uiManager_, window_, elementFlowId);

		Clay_ElementId rootElementId{};
		if constexpr (Mode == OutputMode::Construct || hasEventHooks) {
			rootElementId = uiManager_.toClayEID(elementID_);
		}

#if FLOW_UI_DEV_MODE
		const std::size_t captureIndex = detail::devModeBridge::beginCapturedFlowElement(
			uiManager_,
			ElementType::definitionId,
			devMode::typeHash<ElementType>(),
			devMode::typeToken<ElementType>(),
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
		DevCaptureScope devCapture{&uiManager_};
#endif

		invokeInteractionHooks(invocation, rootElementId, options);

		if constexpr (Mode == OutputMode::Draw) {
			if (elementDrawOptionsHas(options, ElementDrawOptions::SkipBuildCallback)) {
				return;
			}
		}

#if FLOW_UI_DEV_MODE
		devMode::elementCapture::applyParameterOverrides<ParametersType>(
			uiManager_,
			ElementType::definitionId,
			elementFlowId,
			elementID_,
			params_);
#endif

		BuildContext buildContext{invocation, elementID_, params_};
		if constexpr (Mode == OutputMode::Draw) {
			ElementType::buildElement(buildContext);
		} else {
			const Clay_ElementDeclaration declaration =
				ElementType::constructElement(buildContext);
			Clay__OpenElementWithId(rootElementId);
			Clay__ConfigureOpenElement(declaration);
			uiManager_.pushConstructedElement(rootElementId);
#if FLOW_UI_DEV_MODE
			devCapture.leaveOpen();
#endif
		}
	}

	void invokeInteractionHooks(
		detail::element::ElementInvocation<ElementType>& invocation,
		Clay_ElementId rootElementId,
		ElementDrawOptions options) {
		if constexpr (!hasInteractionHooks) {
			(void)invocation;
			(void)rootElementId;
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
				invocation, elementID_, params_, previousInteraction};

			if (invokeEvents) {
				if constexpr (detail::element::HasOnHoveredHook<ElementType>) {
					if (previousInteraction.isHovered(rootElementId)) {
						ElementType::onHovered(context);
					}
				}
				if constexpr (detail::element::HasOnPressedHook<ElementType>) {
					if (previousInteraction.isPressed(rootElementId)) {
						ElementType::onPressed(context);
					}
				}
				if constexpr (detail::element::HasOnHeldHook<ElementType>) {
					if (previousInteraction.isHeld(rootElementId)) {
						ElementType::onHeld(context);
					}
				}
				if constexpr (detail::element::HasOnReleasedHook<ElementType>) {
					if (previousInteraction.isReleased(rootElementId)) {
						ElementType::onReleased(context);
					}
				}
			}

			if constexpr (detail::element::HasRunLogicHook<ElementType>) {
				if (invokeLogic) ElementType::runLogic(context);
			}
		}
	}

	UiManager& uiManager_;
	ElementManager& elementManager_;
	WindowId window_ = InvalidWindowId;
	std::string elementID_{};
	ParametersType params_{};
#if FLOW_UI_DEV_MODE
	bool captureAsDevInternal_ = detail::element::isDevInternal<ElementType>();
	devMode::elementCapture::SourceLocation sourceLocation_{};
#endif
};

/** @} */

} // namespace FlowUi
