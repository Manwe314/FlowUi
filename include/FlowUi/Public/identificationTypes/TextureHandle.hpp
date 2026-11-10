#pragma once

#include <compare>
#include <cstdint>

namespace FlowUi {

/** App-level, generation-checked identity for a UI texture resource. */
struct TextureHandle {
	/** Opaque resource slot; zero is invalid. */
	uint32_t index = 0;
	/** Slot generation guarding against reuse; zero is invalid. */
	uint32_t generation = 0;

	/** Return whether both fields are nonzero; the owning manager validates liveness. */
	[[nodiscard]] constexpr explicit operator bool() const noexcept {
		return index != 0 && generation != 0;
	}

	/** Encode generation in the high 32 bits and index in the low 32 bits. */
	[[nodiscard]] constexpr uint64_t packed() const noexcept {
		return (static_cast<uint64_t>(generation) << 32u) | index;
	}

	/** Decode packed fields without validating resource liveness or ownership. */
	[[nodiscard]] static constexpr TextureHandle fromPacked(uint64_t value) noexcept {
		return TextureHandle{
			.index = static_cast<uint32_t>(value),
			.generation = static_cast<uint32_t>(value >> 32u),
		};
	}

	/** Compare slot index and generation lexicographically. */
	[[nodiscard]] auto operator<=>(const TextureHandle&) const noexcept = default;
};

/** Sentinel for an absent texture resource. */
inline constexpr TextureHandle InvalidTextureHandle{};

} // namespace FlowUi
