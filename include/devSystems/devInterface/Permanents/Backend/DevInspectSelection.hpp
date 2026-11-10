#pragma once
#include "FlowUi/Public/BuildConfig.hpp"
#if FLOW_UI_DEV_MODE
#include "devSystems/devInterface/Permanents/Backend/DevInterfaceState.hpp"

namespace FlowUi {
class App;
}
namespace FlowUi::devSystems {
/* Change the shared application-window context and discard stale inspection targets. */
void set_inspect_window(App& app, DevInterfaceState& state, WindowId window) noexcept;
/* Repair a closed application target independently of the selected interface tab. */
void synchronize_inspect_window(App& app, DevInterfaceState& state, WindowId interface_window);
/* Apply a selector row selection and synchronize the committed overlay target. */
void apply_inspect_selection(App& app, DevInterfaceState& state, uint64_t node_kind,
							 uint64_t node_key, bool toggle) noexcept;
/* Consume controller selection revisions before building the interface panels. */
void synchronize_inspect_selection(App& app, DevInterfaceState& state);
} // namespace FlowUi::devSystems
#endif
