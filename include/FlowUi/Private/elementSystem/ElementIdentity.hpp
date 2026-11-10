#pragma once

#include "FlowUi/Private/IdentityHash.hpp"
#include "FlowUi/Public/identificationTypes/ElementIDTypes.hpp"
#include <concepts>
#include <cstdint>
#include <string_view>
#include <type_traits>

/* Internal identity domains and typed resolvers shared by the supported factories. */
namespace FlowUi::detail::element_id {

// These constants are part of the stable FlowUi identity format. Changing one
// intentionally changes the corresponding identity domain.
inline constexpr uint64_t kDefinitionDomain = 0x3d8f6a19c247e5b1ull;
inline constexpr uint64_t kRootFlowScopeDomain = 0x57c31e8ab604d92full;
inline constexpr uint64_t kLocalElementDomain = 0xa40bf1d7629c3e85ull;
inline constexpr uint64_t kIndexedNameDomain = 0x6e21c9b4f83a507dull;
inline constexpr uint64_t kAutoElementDomain = 0xd17a4e835bc2609full;
inline constexpr uint64_t kGlobalElementDomain = 0x8c53f02ab971d64eull;
inline constexpr uint64_t kPartDeclarationDomain = 0x25e6a1c94f0b73d8ull;
inline constexpr uint64_t kPartInstanceDomain = 0xf4097bc15e2a86d3ull;
inline constexpr uint64_t kClayBridgeDomain = 0x71c8e534ad09f26bull;

template <typename Element>
concept HasIdentityDefinition = requires {
	requires std::same_as<
		std::remove_cv_t<decltype(std::remove_cvref_t<Element>::definitionId)>,
		FlowDefinitionID>;
	requires (std::remove_cvref_t<Element>::definitionId.value != 0);
};

template <typename Element>
[[nodiscard]] constexpr uint64_t definitionValue() noexcept {
	using ElementType = std::remove_cvref_t<Element>;
	static_assert(
		HasIdentityDefinition<ElementType>,
		"FlowUi identity factories require a nonzero FlowDefinitionID definitionId.");
	return ElementType::definitionId.value;
}

[[nodiscard]] constexpr FlowElementID resolveLocal(
	FlowElementID parent,
	FlowDefinitionID definition,
	uint64_t nameToken
#if FLOW_UI_DEV_MODE
	, std::string_view debugName = {}
#endif
	) noexcept {
	if (!parent || !definition || nameToken == 0) return {};
	return FlowElementID{
		.value = detail::identity_hash::compose(
			kLocalElementDomain, parent.value, definition.value, nameToken),
#if FLOW_UI_DEV_MODE
		.debugName = debugName,
#endif
	};
}

[[nodiscard]] constexpr FlowElementID resolveAutomatic(
	FlowElementID parent,
	FlowDefinitionID definition,
	uint64_t callsiteToken
#if FLOW_UI_DEV_MODE
	, std::string_view debugName = {}
#endif
	) noexcept {
	if (!parent || !definition || callsiteToken == 0) return {};
	return FlowElementID{
		.value = detail::identity_hash::compose(
			kAutoElementDomain, parent.value, definition.value, callsiteToken),
#if FLOW_UI_DEV_MODE
		.debugName = debugName,
#endif
	};
}


/* Internal numeric implementation of the deterministic Clay bridge. */
[[nodiscard]] constexpr uint32_t toClayValue(uint64_t flowValue) noexcept {
	if (flowValue == 0) return 0;
	const uint64_t mixed = detail::identity_hash::avalanche64(
		flowValue ^ kClayBridgeDomain);
	const uint32_t folded = static_cast<uint32_t>(mixed) ^
		static_cast<uint32_t>(mixed >> 32);
	return folded == 0 ? 1u : folded;
}

} // namespace FlowUi::detail::element_id
