#pragma once

#include <concepts>
#include <cstdint>
#include <type_traits>

#include "FlowUi/Private/textLayoutService/TextProviderTypes.hpp"
#include "FlowUi/Public/errorManagement/Result.hpp"

namespace FlowUi::detail::text {

inline constexpr uint32_t textLayoutContractRevision = 1;

/** Concrete synchronous CPU layout contract; core owns caches and atlas mapping. */
template <typename Provider>
concept TextLayoutServiceConcept =
	std::is_nothrow_default_constructible_v<Provider> && std::is_nothrow_destructible_v<Provider> &&
	(!std::is_polymorphic_v<Provider>) &&
	requires(Provider& provider, const TextProviderInput& input, const Font::FontFaceData& face) {
		requires std::same_as<std::remove_cv_t<decltype(Provider::contractRevision)>, uint32_t>;
		typename std::integral_constant<uint32_t, Provider::contractRevision>;
		requires(Provider::contractRevision == textLayoutContractRevision);
		{ provider.layout(input, face) } noexcept -> std::same_as<Result<TextLayoutResult>>;
	};

} // namespace FlowUi::detail::text
