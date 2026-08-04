#pragma once

#include <string_view>

#include "FlowUi/WindowId.hpp"

namespace FlowUi {

/** @brief Logical manager namespace used by ResourceKey. */
enum class ResourceDomain : unsigned char {
	/** @brief Let the receiving manager select its own domain. */
	Auto = 0,
	Image,
	Icon,
	Font,
	Ui,
	InputField,
	Viewport,
	Development,
	Internal,
};

/**
 * @brief Human-readable logical identity accepted by FlowUi managers.
 *
 * ResourceKey is a non-owning call-site value. Managers intern name before the
 * call returns. `ResourceDomain::Auto` selects the receiving manager's domain.
 * A zero window selects app scope for app-shared managers and the manager's
 * owning window for window-local managers.
 */
struct ResourceKey {
	std::string_view name{};
	ResourceDomain domain = ResourceDomain::Auto;
	WindowId window = InvalidWindowId;
};

} // namespace FlowUi
