#pragma once

#include <cstdint>
#include <string_view>

#include "FSEL/internal/SelectableSurfaceBehavior.hpp"

namespace FlowUi::FSEL {

struct RadioChoiceParameters {
	uint64_t choiceValue = 0;
	/** Borrowed application-owned selection. Null disables this choice. */
	uint64_t* selectedValue = nullptr;
	bool enabled = true;
	/** Optional notification invoked after selectedValue is updated. */
	ActionCall onSelected{};
	SelectableSurfaceStyle style{};
};

using RadioChoiceState = detail::selectable_surface::State;

/** Construct-only option bound to one value in an exclusive selection. */
struct RadioChoice {
	using Parameters = RadioChoiceParameters;
	using State = RadioChoiceState;
	using BuildContext = ElementBuildContext<RadioChoice>;
	using InteractionContext = ElementInteractionContext<RadioChoice>;

	static constexpr FlowDefinitionID definitionId =
		DefinitionID("FSEL.radio-choice");
	static constexpr std::string_view debugName = "FSEL RadioChoice";

	static void onPressed(InteractionContext& context) {
		detail::selectable_surface::onPressed(context, canSelect(context));
	}

	static void onReleased(InteractionContext& context) {
		if (!detail::selectable_surface::onReleased(
				context,
				canSelect(context))) {
			return;
		}

		*context.params.selectedValue = context.params.choiceValue;
		if (context.params.onSelected) {
			(void)context.invoke(context.params.onSelected);
		}
	}

	static void runLogic(InteractionContext& context) {
		detail::selectable_surface::runLogic(context, canSelect(context));
	}

	static Clay_ElementDeclaration constructElement(BuildContext& context) {
		return detail::selectable_surface::makeDeclaration(
			context,
			isSelected(context.params),
			isEnabled(context.params),
			context.params.style);
	}

private:
	static bool isSelected(const RadioChoiceParameters& parameters) {
		return parameters.selectedValue &&
			*parameters.selectedValue == parameters.choiceValue;
	}

	static bool isEnabled(const RadioChoiceParameters& parameters) {
		return parameters.enabled && parameters.selectedValue;
	}

	static bool canSelect(const InteractionContext& context) {
		return isEnabled(context.params) && !isSelected(context.params);
	}
};

inline constexpr RadioChoice kRadioChoice{};
static_assert(FlowElement<RadioChoice>);
static_assert(ConstructibleFlowElement<RadioChoice>);

} // namespace FlowUi::FSEL
