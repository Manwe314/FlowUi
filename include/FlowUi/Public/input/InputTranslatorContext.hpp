#pragma once

#include "FlowUi/Public/identificationTypes/WindowId.hpp"
#include <cstddef>
#include <cstdint>
#include <span>

namespace FlowUi {

/** A semantic destination available during translator polling. Contains no native state. */
struct InputRoutingTarget {
	WindowId window = InvalidWindowId;
	bool focused = false;
	bool enabled = true;
};

/** Borrowed app-tick context; targets are valid only during the translator call. */
struct InputTranslatorContext {
	std::span<const InputRoutingTarget> targets{};
	double deltaTimeSeconds = 0.0;
	std::uint64_t tick = 0;
	std::size_t eventCapacity = 4096;

	/** Return the first enabled focused destination, or InvalidWindowId. */
	[[nodiscard]] WindowId focusedTarget() const noexcept {
		for (const auto& target : targets)
			if (target.enabled && target.focused)
				return target.window;
		return InvalidWindowId;
	}
};

} // namespace FlowUi
