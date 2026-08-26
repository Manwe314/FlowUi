#pragma once

#include "FlowUi/BuildConfig.hpp"

#if FLOW_UI_HAS_BAKED_CHANGES

#include <memory>
#include <string_view>
#include <type_traits>

#include "FlowUiBakedChanges.hpp"
#include "internal/TypeOperations.hpp"

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
