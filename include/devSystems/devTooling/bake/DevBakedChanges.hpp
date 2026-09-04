#pragma once

#include "FlowUi/BuildConfig.hpp"

#if FLOW_UI_HAS_BAKED_CHANGES

#include <memory>
#include <string_view>
#include <type_traits>

#include "internal/TypeOperations.hpp"

#if __has_include("FlowUiBakedChanges.hpp")
#include "FlowUiBakedChanges.hpp"
#else
#include "FlowUi/ElementID.hpp"
#include "internal/ElementInstanceKey.hpp"

namespace FlowUi::baked {

inline constexpr std::uint64_t schemaFingerprint = 0x0ull;

[[nodiscard]] constexpr bool hasBakedDefinitionChanges(FlowDefinitionID) noexcept { return false; }
[[nodiscard]] constexpr bool hasBakedInstanceChanges(FlowDefinitionID) noexcept { return false; }
[[nodiscard]] constexpr bool hasBakedThemeChanges(std::uint64_t) noexcept { return false; }

inline void applyBakedParametersErased(FlowDefinitionID,
	detail::element::ElementInstanceKey, void*) noexcept {}
inline void applyBakedThemeErased(std::uint64_t, std::string_view,
	void*) noexcept {}

} // namespace FlowUi::baked
#endif

namespace FlowUi::baked {

template <typename Element, typename Parameters>
inline void applyBakedElementDefaults(
	detail::element::ElementInstanceKey instance,
	Parameters& draft) noexcept {
	using ElementType = std::remove_cvref_t<Element>;
	if constexpr (hasBakedDefinitionChanges(ElementType::definitionId) ||
		hasBakedInstanceChanges(ElementType::definitionId)) {
		applyBakedParametersErased(
			ElementType::definitionId, instance, std::addressof(draft));
	}
}

template <typename Theme>
inline void applyBakedThemeDefaults(
	std::string_view variant,
	Theme& draft) noexcept {
	using ThemeType = std::remove_cvref_t<Theme>;
	if constexpr (hasBakedThemeChanges(::FlowUi::detail::typeHash<ThemeType>())) {
		applyBakedThemeErased(
			::FlowUi::detail::typeHash<ThemeType>(), variant, std::addressof(draft));
	}
}

} // namespace FlowUi::baked

#endif
