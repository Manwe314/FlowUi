#pragma once

#include "FlowUi/Public/managers/ActionManager.hpp"

namespace FlowUi::detail::action {

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
