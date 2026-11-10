#pragma once

#include <string_view>

#include "FlowUi/Public/identificationTypes/WindowId.hpp"

namespace FlowUi {

/** @brief Logical manager namespace used by ResourceKey. */
enum class ResourceDomain : unsigned char {
	/** @brief Let the receiving manager select its own domain. */
	Auto = 0,
	/** ImageManager namespace. */
	Image,
	/** IconManager namespace. */
	Icon,
	/** FontManager namespace. */
	Font,
	/** UI resource namespace. */
	Ui,
	/** Input-field resource namespace. */
	InputField,
	/** Viewport resource namespace. */
	Viewport,
	/** Developer tooling resource namespace. */
	Development,
	/** Reserved runtime resource namespace. */
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
	/** Borrowed logical name; the receiving manager interns it before returning. */
	std::string_view name{};
	/** Explicit manager namespace, or Auto to select the receiving manager. */
	ResourceDomain domain = ResourceDomain::Auto;
	/** Window attribution; zero selects the manager's natural app/window scope. */
	WindowId window = InvalidWindowId;
};

} // namespace FlowUi
