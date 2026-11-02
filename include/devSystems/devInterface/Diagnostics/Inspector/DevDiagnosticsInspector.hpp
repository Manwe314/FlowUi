#pragma once

#include <string_view>

#include "FlowUi/BuildConfig.hpp"

#if FLOW_UI_DEV_MODE

#include "FSEL/internal/SelectableSurfaceBehavior.hpp"
#include "devSystems/devInterface/Diagnostics/DevDiagnosticsContentParameters.hpp"
#include "devSystems/devInterface/Diagnostics/DevDiagnosticsIcons.hpp"
#include "managers/FlowUiElementBuilder.hpp"

namespace FlowUi::devSystems::interface_elements {

/** Owned clipboard payload; display wrapping never changes the copied evidence. */
struct DevDiagnosticsCopyParameters {
	std::string value{}, label{"Copy"};
	TextureRef icon{};
	App* app = nullptr;
	float width = 0;
	bool compact = false;
};

/** Copy control for exact captured properties and raw evidence. */
struct DevDiagnosticsCopy {
	using Parameters = DevDiagnosticsCopyParameters;
	using State = FSEL::detail::selectable_surface::State;
	using BuildContext = ElementBuildContext<DevDiagnosticsCopy>;
	using InteractionContext = ElementInteractionContext<DevDiagnosticsCopy>;
	static constexpr FlowDefinitionID definitionId =
		DefinitionID("flowui.dev_interface.diagnostics.copy");
	static constexpr std::string_view debugName = "Diagnostics Copy";
	static constexpr bool isDevInternal = true;
	static void onPressed(InteractionContext& context);
	static void onReleased(InteractionContext& context);
	static void onHovered(InteractionContext& context);
	static void runLogic(InteractionContext& context);
	static void buildElement(BuildContext& context);
};
inline constexpr DevDiagnosticsCopy kDevDiagnosticsCopy{};

/** Exact occurrence, context, source and bounded captured-evidence inspector. */
struct DevDiagnosticsInspector {
	using Resources = diagnostics_icons::Resources;
	struct Disclosure {
		uint64_t identity = 0;
		uint64_t revision = 0;
		bool expanded = false;
	};
	struct State {
		std::vector<Disclosure> snapshots{}, steps{};
		uint64_t occurrence_id = 0;
	};
	using Parameters = DevDiagnosticsContentParameters;
	using BuildContext = ElementBuildContext<DevDiagnosticsInspector>;

	static constexpr FlowDefinitionID definitionId =
		DefinitionID("flowui.dev_interface.diagnostics.inspector");
	static constexpr std::string_view debugName = "Diagnostics Inspector";
	static constexpr bool isDevInternal = true;

	static void buildElement(BuildContext& context);
};

inline constexpr DevDiagnosticsInspector kDevDiagnosticsInspector{};
static_assert(DrawableFlowElement<DevDiagnosticsInspector>);

} // namespace FlowUi::devSystems::interface_elements

#endif
