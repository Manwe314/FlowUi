#pragma once

#include <concepts>
#include <cstddef>
#include <cstring>
#include <memory>
#include <tuple>
#include <type_traits>
#include <utility>

#include "managers/structs/ActionManagerStructs.hpp"

namespace FlowUi::detail::ui_action {

template <typename Parameter>
struct StoredArgument {
	using Bare = std::remove_reference_t<Parameter>;
	using Type = std::conditional_t<
		std::is_lvalue_reference_v<Parameter>,
		std::add_pointer_t<Bare>,
		std::remove_cv_t<Parameter>>;
};

template <typename Parameter>
using StoredArgumentT = typename StoredArgument<Parameter>::Type;

template <typename Parameter, typename Argument>
consteval bool argumentCanBind() {
	if constexpr (std::is_rvalue_reference_v<Parameter>) {
		return false;
	} else if constexpr (std::is_lvalue_reference_v<Parameter>) {
		if constexpr (!std::is_lvalue_reference_v<Argument>) return false;
		using Expected = std::remove_reference_t<Parameter>;
		using Provided = std::remove_reference_t<Argument>;
		if constexpr (std::is_const_v<Expected>) {
			return std::same_as<std::remove_const_t<Expected>, std::remove_const_t<Provided>>;
		} else {
			return std::same_as<Expected, Provided> && !std::is_const_v<Provided>;
		}
	} else if constexpr (std::is_pointer_v<Parameter>) {
		return std::is_convertible_v<Argument, Parameter>;
	} else {
		using Stored = StoredArgumentT<Parameter>;
		return std::is_constructible_v<Stored, Argument> &&
			std::is_trivially_copyable_v<Stored> &&
			std::is_trivially_destructible_v<Stored>;
	}
}

template <typename Parameter, typename Argument>
[[nodiscard]] StoredArgumentT<Parameter> storeArgument(Argument&& argument) {
	if constexpr (std::is_lvalue_reference_v<Parameter>) {
		return std::addressof(argument);
	} else {
		return static_cast<StoredArgumentT<Parameter>>(std::forward<Argument>(argument));
	}
}

template <typename Parameters, std::size_t Index, std::size_t... Prior>
consteval std::size_t storedOffsetImpl(std::index_sequence<Prior...>) {
	return (std::size_t{0} + ... + sizeof(StoredArgumentT<std::tuple_element_t<Prior, Parameters>>));
}

template <typename Parameters, std::size_t Index>
inline constexpr std::size_t StoredOffset =
	storedOffsetImpl<Parameters, Index>(std::make_index_sequence<Index>{});

template <typename Parameters, std::size_t... Index>
consteval std::size_t storedBytesImpl(std::index_sequence<Index...>) {
	return (std::size_t{0} + ... + sizeof(StoredArgumentT<std::tuple_element_t<Index, Parameters>>));
}

template <typename Parameters>
inline constexpr std::size_t StoredBytes =
	storedBytesImpl<Parameters>(std::make_index_sequence<std::tuple_size_v<Parameters>>{});

template <typename Stored>
[[nodiscard]] Stored readStored(const std::byte* bytes) {
	Stored result{};
	std::memcpy(std::addressof(result), bytes, sizeof(Stored));
	return result;
}

template <typename Parameter>
[[nodiscard]] decltype(auto) decodeArgument(const std::byte* bytes) {
	using Stored = StoredArgumentT<Parameter>;
	if constexpr (std::is_lvalue_reference_v<Parameter>) {
		Stored pointer = readStored<Stored>(bytes);
		return static_cast<Parameter>(*pointer);
	} else {
		return readStored<Stored>(bytes);
	}
}

template <typename Operation, typename Parameters, std::size_t... Index>
void invokePacked(const std::byte* payload, std::index_sequence<Index...>) {
	Operation{}(
		decodeArgument<std::tuple_element_t<Index, Parameters>>(
			payload + StoredOffset<Parameters, Index>)...);
}

template <typename Operation>
void invokeRecipe(const std::byte* payload) {
	using Traits = CallableSignature<decltype(&Operation::operator())>;
	using Parameters = typename Traits::Arguments;
	invokePacked<Operation, Parameters>(
		payload, std::make_index_sequence<std::tuple_size_v<Parameters>>{});
}

template <typename Parameters, typename ArgumentsTuple, std::size_t... Index>
consteval bool argumentsCanBind(std::index_sequence<Index...>) {
	return (argumentCanBind<
		std::tuple_element_t<Index, Parameters>,
		std::tuple_element_t<Index, ArgumentsTuple>>() && ...);
}

template <typename Parameters, typename ArgumentsTuple, std::size_t... Index>
void packArguments(
	std::byte* destination,
	ArgumentsTuple&& arguments,
	std::index_sequence<Index...>) {
	(([&] {
		using Parameter = std::tuple_element_t<Index, Parameters>;
		using Stored = StoredArgumentT<Parameter>;
		Stored value = storeArgument<Parameter>(
			std::get<Index>(std::forward<ArgumentsTuple>(arguments)));
		std::memcpy(
			destination + StoredOffset<Parameters, Index>,
			std::addressof(value),
			sizeof(Stored));
	}()), ...);
}

} // namespace FlowUi::detail::ui_action
