#pragma once

#include "FlowUi/Public/BuildConfig.hpp"

#if FLOW_UI_DEV_MODE

#include <memory>
#include <utility>
#include "FlowUi/Public/errorManagement/Error.hpp"
#include "FlowUi/Public/configurationStructs/DevToolsConfig.hpp"
#include "FlowUi/Public/errorManagement/Result.hpp"
#include "FlowUi/Public/identificationTypes/WindowId.hpp"
#include "FlowUi/Public/managerStructs/ShortcutTypes.hpp"

namespace FlowUi {
class App;
class UiManager;
namespace detail::storage { class IStorageSystem; }
}

namespace FlowUi::devSystems {
struct DevInterfaceState;

/* Owns the dedicated developer-interface window and its toggle shortcut. */
class DevInterface {
public:
	DevInterface() noexcept;
	~DevInterface();

	DevInterface(const DevInterface&) = delete;
	DevInterface& operator=(const DevInterface&) = delete;
	DevInterface(DevInterface&&) = delete;
	DevInterface& operator=(DevInterface&&) = delete;

	[[nodiscard]] Status initialize(UiManager& mainUi, const DevToolsConfig& config);
	/* Reserve the panel chord in each window shortcut dispatcher. */
	[[nodiscard]] Status attachWindow(UiManager& ui);
	[[nodiscard]] Status synchronize(App& app);
	/* Release borrowed backend inspection leases before App destroys its storage system. */
	void release_inspection_resources(::FlowUi::detail::storage::IStorageSystem* storage) noexcept;
	/* Queue a manual toggle for the next platform safe point. */
	void request_toggle() noexcept { toggleRequested_ = true; }
	/* Consume one post-arm native press edge. */
	[[nodiscard]] bool take_start_request() noexcept {
		return std::exchange(start_requested_, false);
	}

	/* True while the retained developer window is suspended without UI dispatch. */
	[[nodiscard]] bool suspended() const noexcept { return suspended_; }
	[[nodiscard]] WindowId windowId() const noexcept { return windowId_; }

private:
	std::unique_ptr<DevInterfaceState> session_{};
	bool start_requested_ = false;
	bool suspended_ = false, toggle_down_ = false;
	bool enabled_ = false;
	bool shortcutEnabled_ = false;
	bool toggleRequested_ = false;
	DevShortcutChord toggleChord_{};
	WindowId windowId_ = InvalidWindowId;
};

} // namespace FlowUi::devSystems

#endif
