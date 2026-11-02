#pragma once

#include "FlowUi/BuildConfig.hpp"

#include "FlowUi/PublicStructs.hpp"
#if FLOW_UI_DEV_MODE
#include "devSystems/devTooling/bake/DevBakedChangesMetadata.hpp"
#endif
#include <span>
#include <string_view>

#if FLOW_UI_DEV_MODE && (!FLOW_UI_HAS_BAKED_CHANGES || !__has_include("FlowUiBakedChanges.hpp"))
namespace FlowUi::baked {
/** Exact manifest revision compiled into this executable, or zero without baked changes. */
[[nodiscard]] constexpr std::uint64_t compiled_manifest_digest() noexcept {
	return 0;
}
/** Development-only catalogue of compiled assignments. */
[[nodiscard]] inline std::span<const CompiledChangeEntry> compiled_change_entries() noexcept {
	return {};
}
/** Saved memory payload, including evidence and disabled settings. */
[[nodiscard]] constexpr std::string_view compiled_memory_capacity_manifest_json() noexcept {
	return {};
}
} // namespace FlowUi::baked
#endif

#if !FLOW_UI_HAS_BAKED_CHANGES || !__has_include("FlowUiBakedChanges.hpp")
namespace FlowUi::baked {
/** No compiled capacity payload in this build. */
[[nodiscard]] constexpr bool has_baked_memory_capacity() noexcept {
	return false;
}
[[nodiscard]] inline std::span<const MemoryCapacitySetting>
baked_memory_capacity_settings() noexcept {
	return {};
}
} // namespace FlowUi::baked
#endif

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
