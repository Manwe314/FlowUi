#pragma once

#include <compare>
#include <cstdint>

namespace FlowUi {

/** @brief Number of successfully committed absent frames retained by default. */
inline constexpr uint32_t DefaultElementStateGraceFrames = 5;

/** @brief Lifetime policy for state owned by one Flow element instance. */
enum class ElementStateRetention : uint8_t {
	/** Reclaim state after it is absent for the configured committed frame grace period. */
	Transient,
	/** Retain state until its owning window is destroyed or state is explicitly erased. */
	WindowLifetime,
};

/** @brief Definition-level retention policy applied whenever an element is invoked. */
struct ElementStatePolicy {
	/** Retention mode; transient state is the default. */
	ElementStateRetention retention = ElementStateRetention::Transient;
	/** Successfully committed absent frames tolerated by transient state. */
	uint32_t graceFrames = DefaultElementStateGraceFrames;

	/**
	 * Create a policy for state that expires after a period of absence.
	 * @param expiryFrames Number of committed absent frames to retain.
	 * @return Transient retention policy with the requested grace period.
	 */
	[[nodiscard]] static constexpr ElementStatePolicy transient(
		uint32_t expiryFrames = DefaultElementStateGraceFrames) noexcept {
		return {
			.retention = ElementStateRetention::Transient,
			.graceFrames = expiryFrames,
		};
	}

	/** Return a policy that retains state for the owning window's lifetime. */
	[[nodiscard]] static constexpr ElementStatePolicy windowLifetime() noexcept {
		return {
			.retention = ElementStateRetention::WindowLifetime,
			.graceFrames = 0,
		};
	}

	/** Compare retention mode and grace period by value. */
	[[nodiscard]] auto operator<=>(const ElementStatePolicy&) const noexcept = default;
};

} // namespace FlowUi
