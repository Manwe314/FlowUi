#pragma once

#include "FlowUi/BuildConfig.hpp"

#if FLOW_UI_DEV_MODE

#include "FlowUi/Error.hpp"
#include "FlowUi/PublicStructs.hpp"
#include "FlowUi/WindowId.hpp"
#include "managers/structs/ShortcutManagerStructs.hpp"

namespace FlowUi {
class App;
class UiManager;
}

namespace FlowUi::devSystems {

/** Owns the dedicated developer-interface window and its toggle shortcut. */
class DevInterface {
public:
	DevInterface() noexcept = default;
	~DevInterface() = default;

	DevInterface(const DevInterface&) = delete;
	DevInterface& operator=(const DevInterface&) = delete;
	DevInterface(DevInterface&&) = delete;
	DevInterface& operator=(DevInterface&&) = delete;

	[[nodiscard]] Status initialize(UiManager& mainUi, const DevToolsConfig& config);
	[[nodiscard]] Status attachWindow(UiManager& ui);
	[[nodiscard]] Status synchronize(App& app);

	[[nodiscard]] WindowId windowId() const noexcept { return windowId_; }

private:
	bool enabled_ = false;
	bool shortcutEnabled_ = false;
	bool toggleRequested_ = false;
	DevShortcutChord toggleChord_{};
	WindowId windowId_ = InvalidWindowId;
};

} // namespace FlowUi::devSystems

#endif
