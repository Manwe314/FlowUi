#pragma once

#include <compare>
#include <cstdint>

namespace FlowUi {

/** @brief Number of successfully committed absent frames retained by default. */
inline constexpr uint32_t DefaultElementStateGraceFrames = 5;

/** @brief Lifetime policy for state owned by one Flow element instance. */
enum class ElementStateRetention : uint8_t {
	Transient,
	WindowLifetime,
};

/** @brief Definition-level retention policy applied whenever an element is invoked. */
struct ElementStatePolicy {
	ElementStateRetention retention = ElementStateRetention::Transient;
	uint32_t graceFrames = DefaultElementStateGraceFrames;

	[[nodiscard]] static constexpr ElementStatePolicy transient(
		uint32_t expiryFrames = DefaultElementStateGraceFrames) noexcept {
		return {
			.retention = ElementStateRetention::Transient,
			.graceFrames = expiryFrames,
		};
	}

	[[nodiscard]] static constexpr ElementStatePolicy windowLifetime() noexcept {
		return {
			.retention = ElementStateRetention::WindowLifetime,
			.graceFrames = 0,
		};
	}

	auto operator<=>(const ElementStatePolicy&) const = default;
};

} // namespace FlowUi
