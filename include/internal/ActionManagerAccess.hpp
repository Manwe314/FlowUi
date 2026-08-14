#pragma once

#include "managers/ActionManager.hpp"

namespace FlowUi::detail::action {

// Internal lifecycle adapter for focused manager tests. This deliberately does
// not widen ActionManager's public API.
struct ActionManagerAccess {
	static void init(
		ActionManager& manager,
		App& app,
		detail::storage::IStorageSystem& storage) {
		manager.init(app, storage);
	}

	static void destroy(ActionManager& manager) noexcept {
		manager.destroy();
	}
};

} // namespace FlowUi::detail::action
