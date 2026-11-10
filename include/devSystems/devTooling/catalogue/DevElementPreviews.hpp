#pragma once
#include "FlowUi/Public/BuildConfig.hpp"
#if FLOW_UI_DEV_MODE
#include "FlowUi/Public/elementAuthoring/ElementBuilder.hpp"
#include "FlowUi/Public/elementAuthoring/ElementSet.hpp"
namespace FlowUi::devSystems::tooling {
/** Declare a named typed set without preparing resources or executing element builders.
 * Admission is an author contract: effective defaults must build without app side effects
 * or floating/pop-up roots. Unknown types discovered at runtime remain unadmitted. */
template <FlowElement... Elements>
void register_element_catalogue(DevSchemaRegistry& registry, std::string_view name,
								ElementSet<Elements...>, bool admit_defaults = false) {
	(registry.template ensureElement<Elements>(), ...);
	(registry.template registerElementPreview<Elements>(name, admit_defaults), ...);
}
/** Support the schema catalogue's existing pack representation through the same path. */
template <FlowElement... Elements>
void register_element_catalogue(DevSchemaRegistry& registry, std::string_view name,
								DevElementCatalogue<Elements...>, bool admit_defaults = false) {
	register_element_catalogue(registry, name, ElementSet<Elements...>{}, admit_defaults);
}
} // namespace FlowUi::devSystems::tooling
#endif
