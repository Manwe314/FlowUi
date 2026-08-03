#pragma once

#include <compare>
#include <cstdint>

namespace FlowUi {

/** App-level, generation-checked identity for a UI texture resource. */
struct TextureHandle {
	uint32_t index = 0;
	uint32_t generation = 0;

	[[nodiscard]] constexpr explicit operator bool() const noexcept {
		return index != 0 && generation != 0;
	}

	[[nodiscard]] constexpr uint64_t packed() const noexcept {
		return (static_cast<uint64_t>(generation) << 32u) | index;
	}

	[[nodiscard]] static constexpr TextureHandle fromPacked(uint64_t value) noexcept {
		return TextureHandle{
			.index = static_cast<uint32_t>(value),
			.generation = static_cast<uint32_t>(value >> 32u),
		};
	}

	auto operator<=>(const TextureHandle&) const = default;
};

inline constexpr TextureHandle InvalidTextureHandle{};

} // namespace FlowUi
