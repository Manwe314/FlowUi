#pragma once

#include <cstdint>
#include <string_view>
#include <type_traits>

namespace FlowUi::detail {

template <typename T>
constexpr std::string_view typeToken() noexcept {
#if defined(__clang__) || defined(__GNUC__)
	return __PRETTY_FUNCTION__;
#elif defined(_MSC_VER)
	return __FUNCSIG__;
#else
	return "FlowUi::detail::typeToken<unknown>()";
#endif
}

constexpr uint64_t hashString64(std::string_view text) noexcept {
	uint64_t hash = 14695981039346656037ull;
	for (const char c : text) {
		hash ^= static_cast<uint64_t>(static_cast<unsigned char>(c));
		hash *= 1099511628211ull;
	}
	return (hash == 0ull) ? 1ull : hash;
}

template <typename T>
constexpr uint64_t typeHash() noexcept {
	using CleanT = std::remove_cvref_t<T>;
	return hashString64(typeToken<CleanT>());
}

} // namespace FlowUi::detail
