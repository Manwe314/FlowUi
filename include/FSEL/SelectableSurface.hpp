#pragma once

#include <string_view>


#include "FSEL/internal/SelectableSurfaceBehavior.hpp"


namespace FlowUi::FSEL {

struct SelectableSurfaceParameters {
	/** Current application-owned selection state. */
	bool selected = false;
	bool enabled = true;
	/** Invoked only for a completed click while selected is false. */
	ActionCall onSelected{};
	SelectableSurfaceStyle style{};
};

using SelectableSurfaceState = detail::selectable_surface::State;

/**
 * Construct-only selection target. The caller owns its selected value and
 * authors presentational children inside the returned surface.
 */
struct SelectableSurface {
	using Parameters = SelectableSurfaceParameters;
	using State = SelectableSurfaceState;
	using BuildContext = ElementBuildContext<SelectableSurface>;
	using InteractionContext = ElementInteractionContext<SelectableSurface>;

	static constexpr FlowDefinitionID definitionId =
		DefinitionID("FSEL.selectable-surface");
	static constexpr std::string_view debugName = "FSEL SelectableSurface";

	static void onPressed(InteractionContext& context) {
		detail::selectable_surface::onPressed(context, canSelect(context));
	}

	static void onReleased(InteractionContext& context) {
		if (detail::selectable_surface::onReleased(
				context,
				canSelect(context))) {
			(void)context.invoke(context.params.onSelected);
		}
	}

	static void runLogic(InteractionContext& context) {
		detail::selectable_surface::runLogic(context, canSelect(context));
	}

	static Clay_ElementDeclaration constructElement(BuildContext& context) {
		return detail::selectable_surface::makeDeclaration(
			context,
			context.params.selected,
			isEnabled(context),
			context.params.style);
	}

private:
	static bool isEnabled(const InteractionContext& context) {
		return context.params.enabled &&
			context.actionAvailability(context.params.onSelected).enabled;
	}

	static bool isEnabled(const BuildContext& context) {
		return context.params.enabled &&
			context.uiManager.actions()
				.availability(context.params.onSelected).enabled;
	}

	static bool canSelect(const InteractionContext& context) {
		return !context.params.selected && isEnabled(context);
	}
};

inline constexpr SelectableSurface kSelectableSurface{};
static_assert(FlowElement<SelectableSurface>);
static_assert(ConstructibleFlowElement<SelectableSurface>);

} // namespace FlowUi::FSEL
