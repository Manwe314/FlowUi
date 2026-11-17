#pragma once

#include "FlowUi/Public/identificationTypes/WindowId.hpp"
#include <cstddef>
#include <cstdint>
#include <span>

namespace FlowUi {

/** A semantic destination available during translator polling. Contains no native state. */
struct InputRoutingTarget {
	/** Live semantic routing destination. */
	WindowId window = InvalidWindowId;
	/** Whether the host currently observes this route as focused. */
	bool focused = false;
	/** Whether this destination accepts translated input. */
	bool enabled = true;
};

/** Borrowed app-tick context; targets are valid only during the translator call. */
struct InputTranslatorContext {
	/** Borrowed ordered routing destinations; valid only during this call. */
	std::span<const InputRoutingTarget> targets{};
	/** Elapsed application-tick duration in seconds. */
	double deltaTimeSeconds = 0.0;
	/** Application tick identity for deterministic sampling. */
	std::uint64_t tick = 0;
	/** Configured bounded input-event capacity; not a count of free slots. */
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
