#pragma once

#include <cstddef>
#include <cstdint>
#include <string_view>

namespace FlowUi::detail::identity_hash {

inline constexpr uint64_t kFnvOffsetBasis = 14695981039346656037ull;
inline constexpr uint64_t kFnvPrime = 1099511628211ull;
inline constexpr uint64_t kCombineBias = 0x9e3779b97f4a7c15ull;

[[nodiscard]] constexpr uint64_t reserveZero(uint64_t value) noexcept {
	return value == 0 ? 1 : value;
}

[[nodiscard]] constexpr uint64_t avalanche64(uint64_t value) noexcept {
	value ^= value >> 30u;
	value *= 0xbf58476d1ce4e5b9ull;
	value ^= value >> 27u;
	value *= 0x94d049bb133111ebull;
	value ^= value >> 31u;
	return value;
}

[[nodiscard]] constexpr uint64_t hashBytes(std::string_view text) noexcept {
	uint64_t hash = kFnvOffsetBasis;
	for (const char character : text) {
		hash ^= static_cast<uint64_t>(static_cast<unsigned char>(character));
		hash *= kFnvPrime;
	}
	return hash;
}

[[nodiscard]] constexpr uint64_t combine(uint64_t seed, uint64_t value) noexcept {
	return avalanche64(seed ^ avalanche64(value + kCombineBias));
}

template <typename... Values>
[[nodiscard]] constexpr uint64_t compose(
	uint64_t domain,
	Values... values) noexcept {
	uint64_t hash = domain;
	((hash = combine(hash, static_cast<uint64_t>(values))), ...);
	hash = combine(hash, sizeof...(Values));
	return reserveZero(hash);
}

[[nodiscard]] constexpr uint64_t authoredNameToken(
	uint64_t domain,
	std::string_view name) noexcept {
	return name.empty() ? 0 : compose(domain, hashBytes(name));
}

template <std::size_t N>
[[nodiscard]] constexpr std::string_view literalView(const char (&name)[N]) {
	static_assert(N > 1, "FlowUi identity names must not be empty.");
	return std::string_view{name, N - 1};
}

} // namespace FlowUi::detail::identity_hash
