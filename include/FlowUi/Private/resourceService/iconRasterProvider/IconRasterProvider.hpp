#pragma once

#include <concepts>
#include <cstddef>
#include <cstdint>
#include <span>
#include <type_traits>

#include "FlowUi/Private/resourceService/iconRasterProvider/IconProviderTypes.hpp"
#include "FlowUi/Public/errorManagement/Result.hpp"

namespace FlowUi::detail::icon {

inline constexpr uint32_t iconRasterContractRevision = 1;

/** Private synchronous CPU SVG contract. Successful documents own retained input, and rasters
 * own exact-sized, top-down, tight straight-alpha RGBA8 pixels. Inputs are borrowed only for
 * the operation. One provider/document is used serially by its owning controller. */
template <typename Provider>
concept IconRasterProvider =
	std::is_nothrow_default_constructible_v<Provider> &&
	std::is_nothrow_destructible_v<Provider> && (!std::is_polymorphic_v<Provider>) &&
	requires {
		typename Provider::Document;
		requires std::movable<typename Provider::Document>;
		requires (!std::copy_constructible<typename Provider::Document>);
		requires std::is_nothrow_move_constructible_v<typename Provider::Document>;
		requires std::is_nothrow_move_assignable_v<typename Provider::Document>;
		requires std::is_nothrow_destructible_v<typename Provider::Document>;
		requires (!std::is_polymorphic_v<typename Provider::Document>);
		requires std::same_as<std::remove_cv_t<decltype(Provider::contractRevision)>, uint32_t>;
		typename std::integral_constant<uint32_t, Provider::contractRevision>;
		requires (Provider::contractRevision == iconRasterContractRevision);
	} && requires(Provider& provider, const Provider& observed,
				  const typename Provider::Document& document, std::span<const std::byte> source,
				  const IconRasterRequest& request) {
		{ provider.parse(source) } noexcept -> std::same_as<Result<typename Provider::Document>>;
		{ observed.intrinsicSize(document) } noexcept -> std::same_as<IconDimensions>;
		{ observed.contentSize(document) } noexcept -> std::same_as<IconDimensions>;
		{ provider.rasterize(document, request) } noexcept -> std::same_as<Result<Rgba8Image>>;
	};

} // namespace FlowUi::detail::icon
