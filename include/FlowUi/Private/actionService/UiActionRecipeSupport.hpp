#pragma once

#include "FlowUi/Private/IdentityHash.hpp"
#include <cstdint>
#include <string_view>
#include <tuple>
#include <type_traits>

/* Internal callable traits and stable recipe hashing used by public UI actions. */
namespace FlowUi::detail::ui_action {

constexpr uint64_t kUiRecipeDomain = 0x98d473f25b1ca60full;

[[nodiscard]] constexpr uint64_t recipeHash(std::string_view text) noexcept {
	return detail::identity_hash::reserveZero(detail::identity_hash::combine(
		kUiRecipeDomain,
		detail::identity_hash::hashBytes(text)));
}

template <typename T>
[[nodiscard]] consteval std::string_view callableTypeName() {
#if defined(__clang__) || defined(__GNUC__)
	return __PRETTY_FUNCTION__;
#elif defined(_MSC_VER)
	return __FUNCSIG__;
#else
	return "FlowUi::UiAction<unknown>";
#endif
}

template <typename T>
[[nodiscard]] consteval uint64_t callableTypeId() {
	return recipeHash(callableTypeName<T>());
}

template <typename Signature>
struct CallableSignature;

template <typename Return, typename Class, typename... Args>
struct CallableSignature<Return (Class::*)(Args...) const> {
	using Result = Return;
	using Arguments = std::tuple<Args...>;
};

template <typename Return, typename Class, typename... Args>
struct CallableSignature<Return (Class::*)(Args...)> {
	using Result = Return;
	using Arguments = std::tuple<Args...>;
};

template <typename Return, typename Class, typename... Args>
struct CallableSignature<Return (Class::*)(Args...) const noexcept>
	: CallableSignature<Return (Class::*)(Args...) const> {};

template <typename Return, typename Class, typename... Args>
struct CallableSignature<Return (Class::*)(Args...) noexcept>
	: CallableSignature<Return (Class::*)(Args...)> {};

template <typename Fn>
concept ConcreteRecipeCallable = requires {
	&Fn::operator();
	typename CallableSignature<decltype(&Fn::operator())>::Result;
	typename CallableSignature<decltype(&Fn::operator())>::Arguments;
};

} // namespace FlowUi::detail::ui_action
