#pragma once

#include <string_view>


#include "FSEL/internal/SelectableSurfaceBehavior.hpp"


namespace FlowUi::FSEL {

/**
 * Per-invocation SelectableSurface configuration. Borrowed models and text must remain valid
 * through the frame; empty visual overrides inherit the active FSELTheme.
 */
struct SelectableSurfaceParameters {
	/** Invoked only for a completed click while selected is false. */
	ActionCall onSelected{};
	/** Shared layout and state appearance overrides for the selectable surface. */
	SelectableSurfaceStyle style{};
	/** Current application-owned selection state. */
	bool selected = false;
	/** Allow interaction; false also selects disabled styling. */
	bool enabled = true;
};

/** Retained interaction state used by this standard element. */
using SelectableSurfaceState = detail::selectable_surface::State;

/**
 * Construct-only selection target. The caller owns its selected value and
 * authors presentational children inside the returned surface.
 */
struct SelectableSurface {
	/** Per-invocation configuration copied into ElementBuilder. */
	using Parameters = SelectableSurfaceParameters;
	/** Per-instance state owned and retained by ElementManager. */
	using State = SelectableSurfaceState;
	/** Frame-scoped context supplied to the element build hooks. */
	using BuildContext = ElementBuildContext<SelectableSurface>;
	/** Frame-scoped context supplied to pointer and logic hooks. */
	using InteractionContext = ElementInteractionContext<SelectableSurface>;

	/** Stable definition identity used for registration, state and semantic parts. */
	static constexpr FlowDefinitionID definitionId =
		DefinitionID("FSEL.selectable-surface");
	/** Human-readable element name used by development tools. */
	static constexpr std::string_view debugName = "FSEL SelectableSurface";

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
		if (detail::selectable_surface::onReleased(
				context,
				canSelect(context))) {
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

/**
 * Reusable empty definition tag passed to UiManager::createElement(); no per-instance state is
 * stored in this object.
 */
inline constexpr SelectableSurface kSelectableSurface{};
static_assert(FlowElement<SelectableSurface>);
static_assert(ConstructibleFlowElement<SelectableSurface>);

} // namespace FlowUi::FSEL
