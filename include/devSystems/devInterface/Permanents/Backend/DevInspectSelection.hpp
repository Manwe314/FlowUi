#pragma once
#include "FlowUi/BuildConfig.hpp"
#if FLOW_UI_DEV_MODE
#include "devSystems/devInterface/Permanents/Backend/DevInterfaceState.hpp"

namespace FlowUi {
class App;
}
namespace FlowUi::devSystems {
/** Apply a selector row selection and synchronize the committed overlay target. */
void apply_inspect_selection(App& app, DevInterfaceState& state, uint64_t node_kind,
							 uint64_t node_key, bool toggle) noexcept;
/** Consume controller selection revisions before building the interface panels. */
void synchronize_inspect_selection(App& app, DevInterfaceState& state);
} // namespace FlowUi::devSystems
#endif
