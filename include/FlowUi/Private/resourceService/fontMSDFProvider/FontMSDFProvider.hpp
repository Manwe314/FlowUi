#pragma once
#include "FlowUi/Private/resourceService/fontMSDFProvider/FontProviderTypes.hpp"
#include "FlowUi/Public/errorManagement/Result.hpp"
#include <concepts>
#include <type_traits>
namespace FlowUi::detail::font {
inline constexpr uint32_t fontMsdfContractRevision = 1;

template <typename Provider>
concept FontMSDFProvider =
	std::is_nothrow_default_constructible_v<Provider> && std::is_nothrow_destructible_v<Provider> &&
	(!std::is_polymorphic_v<Provider>) &&
	requires(Provider& provider, std::span<const std::byte> bakedBytes,
			 const FontBakeRequest& request, const FontCpuAsset& asset) {
		requires std::same_as<std::remove_cv_t<decltype(Provider::contractRevision)>, uint32_t>;
		typename std::integral_constant<uint32_t, Provider::contractRevision>;
		requires(Provider::contractRevision == fontMsdfContractRevision);
		requires std::same_as<std::remove_cv_t<decltype(Provider::supportsBaking)>, bool>;
		requires std::same_as<std::remove_cv_t<decltype(Provider::supportsEncoding)>, bool>;
		typename std::bool_constant<Provider::supportsBaking>;
		typename std::bool_constant<Provider::supportsEncoding>;
		{ provider.loadBaked(bakedBytes) } noexcept -> std::same_as<Result<FontCpuAsset>>;
		{ provider.bake(request) } noexcept -> std::same_as<Result<FontBakeOutput>>;
		{ provider.encodeBaked(asset) } noexcept -> std::same_as<Result<std::vector<std::byte>>>;
	};
} // namespace FlowUi::detail::font
