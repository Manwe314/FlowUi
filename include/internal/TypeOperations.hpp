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

/** Extract a source-usable type spelling without changing the identity token. */
template <typename T>
constexpr std::string_view cppTypeName() noexcept {
#if defined(__clang__)
	constexpr std::string_view signature = __PRETTY_FUNCTION__;
	constexpr std::string_view prefix = "[T = ";
	const std::size_t first = signature.find(prefix);
	if (first == std::string_view::npos) return signature;
	const std::size_t begin = first + prefix.size();
	const std::size_t end = signature.rfind(']');
	return end == std::string_view::npos ? signature : signature.substr(begin, end - begin);
#elif defined(__GNUC__)
	constexpr std::string_view signature = __PRETTY_FUNCTION__;
	constexpr std::string_view prefix = "with T = ";
	const std::size_t first = signature.find(prefix);
	if (first == std::string_view::npos) return signature;
	const std::size_t begin = first + prefix.size();
	std::size_t end = signature.find(';', begin);
	if (end == std::string_view::npos) end = signature.rfind(']');
	return end == std::string_view::npos ? signature : signature.substr(begin, end - begin);
#elif defined(_MSC_VER)
	constexpr std::string_view signature = __FUNCSIG__;
	constexpr std::string_view prefix = "cppTypeName<";
	const std::size_t first = signature.find(prefix);
	if (first == std::string_view::npos) return signature;
	std::size_t begin = first + prefix.size();
	const std::size_t end = signature.rfind(">(void)");
	if (end == std::string_view::npos) return signature;
	if (signature.substr(begin, 7) == "struct ") begin += 7;
	else if (signature.substr(begin, 6) == "class ") begin += 6;
	else if (signature.substr(begin, 5) == "enum ") begin += 5;
	return signature.substr(begin, end - begin);
#else
	return typeToken<T>();
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
