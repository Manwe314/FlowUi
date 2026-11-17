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

/**
 * State lifetime for an element definition. Transient state tolerates graceFrames committed
 * absent frames and expires on the next absent frame; WindowLifetime retains it until the window
 * closes or state is explicitly erased.
 */
struct ElementStatePolicy {
	/** Retention mode; transient state is the default. */
	ElementStateRetention retention = ElementStateRetention::Transient;
	/** Absent committed frames tolerated by transient state; ignored for WindowLifetime. */
	uint32_t graceFrames = DefaultElementStateGraceFrames;

	/**
	 * @brief Create a transient policy retaining state for expiryFrames successfully committed absent frames, then expiring on the next absent frame. Canceled frames do not advance retention.
	 *
	 * @see docs/api/elements.md
	 */
	[[nodiscard]] static constexpr ElementStatePolicy transient(
		uint32_t expiryFrames = DefaultElementStateGraceFrames) noexcept {
		return {
			.retention = ElementStateRetention::Transient,
			.graceFrames = expiryFrames,
		};
	}

	/**
	 * @brief Create a policy retaining state until explicit erasure or owning-window destruction.
	 *
	 * @see docs/api/elements.md
	 */
	[[nodiscard]] static constexpr ElementStatePolicy windowLifetime() noexcept {
		return {
			.retention = ElementStateRetention::WindowLifetime,
			.graceFrames = 0,
		};
	}

	/**
	 * @brief Compare retention mode and grace period by value.
	 *
	 * @see docs/api/elements.md
	 */
	[[nodiscard]] auto operator<=>(const ElementStatePolicy&) const noexcept = default;
};

} // namespace FlowUi
