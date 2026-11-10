#pragma once

#include <cstdint>
#include <string_view>

#include "FSEL/internal/SelectableSurfaceBehavior.hpp"

namespace FlowUi::FSEL {

/**
 * Per-invocation RadioChoice configuration. Borrowed models and text must remain valid through
 * the frame; empty visual overrides inherit the active FSELTheme.
 */
struct RadioChoiceParameters {
	/** Optional notification invoked after selectedValue is updated. */
	ActionCall onSelected{};
	/** Value written to selectedValue when this choice is selected. */
	uint64_t choiceValue = 0;
	/** Borrowed application-owned selection. Null disables this choice. */
	uint64_t* selectedValue = nullptr;
	/** Shared layout and state appearance overrides for the selectable surface. */
	SelectableSurfaceStyle style{};
	/** Allow interaction; false also selects disabled styling. */
	bool enabled = true;
};

/** Retained interaction state used by this standard element. */
using RadioChoiceState = detail::selectable_surface::State;

/** Construct-only option bound to one value in an exclusive selection. */
struct RadioChoice {
	/** Per-invocation configuration copied into ElementBuilder. */
	using Parameters = RadioChoiceParameters;
	/** Per-instance state owned and retained by ElementManager. */
	using State = RadioChoiceState;
	/** Frame-scoped context supplied to the element build hooks. */
	using BuildContext = ElementBuildContext<RadioChoice>;
	/** Frame-scoped context supplied to pointer and logic hooks. */
	using InteractionContext = ElementInteractionContext<RadioChoice>;

	/** Stable definition identity used for registration, state and semantic parts. */
	static constexpr FlowDefinitionID definitionId =
		DefinitionID("FSEL.radio-choice");
	/** Human-readable element name used by development tools. */
	static constexpr std::string_view debugName = "FSEL RadioChoice";

	/**
	 * ElementBuilder press hook; begins the enabled control interaction. @param context Borrowed
	 * context valid for this callback only.
	 */
	static void onPressed(InteractionContext& context) {
		detail::selectable_surface::onPressed(context, canSelect(context));
	}

	/**
	 * ElementBuilder release hook; completes or cancels the pending interaction. @param context
	 * Borrowed context valid for this callback only.
	 */
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

	/**
	 * ElementBuilder logic hook; reconciles retained interaction state with the current input.
	 * @param context Borrowed context valid for this callback only.
	 */
	static void runLogic(InteractionContext& context) {
		detail::selectable_surface::runLogic(context, canSelect(context));
	}

	/**
	 * ElementBuilder construct hook; returns the themed root declaration for caller-authored
	 * children. @param context Borrowed context valid for this callback only. @return Root Clay
	 * declaration; the builder opens it for child composition.
	 */
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

/**
 * Reusable empty definition tag passed to UiManager::createElement(); no per-instance state is
 * stored in this object.
 */
inline constexpr RadioChoice kRadioChoice{};
static_assert(FlowElement<RadioChoice>);
static_assert(ConstructibleFlowElement<RadioChoice>);

} // namespace FlowUi::FSEL
