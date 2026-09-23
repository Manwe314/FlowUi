#pragma once

#include "FlowUi/BuildConfig.hpp"

#if FLOW_UI_DEV_MODE

#include <cstdint>

#include "FlowUi/ElementID.hpp"
#include "internal/ElementInstanceKey.hpp"

namespace FlowUi::devSystems::tooling {

enum class DevOverlayModeFlags : uint32_t {
	None = 0,
	BoxModel = 1u << 0u,
	RulersAndDistance = 1u << 1u,
	TreeHierarchy = 1u << 2u,
	Typography = 1u << 3u,
	ScissorAndClip = 1u << 4u,
	RenderRunDiagnostics = 1u << 5u,
	Default = (1u << 0u) | (1u << 1u),
};

[[nodiscard]] constexpr DevOverlayModeFlags operator|(
	DevOverlayModeFlags left,
	DevOverlayModeFlags right) noexcept {
	return static_cast<DevOverlayModeFlags>(
		static_cast<uint32_t>(left) | static_cast<uint32_t>(right));
}

constexpr DevOverlayModeFlags& operator|=(
	DevOverlayModeFlags& left,
	DevOverlayModeFlags right) noexcept {
	left = left | right;
	return left;
}

[[nodiscard]] constexpr bool operator&(
	DevOverlayModeFlags left,
	DevOverlayModeFlags right) noexcept {
	return (static_cast<uint32_t>(left) & static_cast<uint32_t>(right)) != 0u;
}

[[nodiscard]] constexpr bool hasFlag(
	DevOverlayModeFlags value,
	DevOverlayModeFlags flag) noexcept {
	return value & flag;
}

enum class DevInspectTargetKind : uint8_t {
	None = 0,
	Flow = 1,
	Clay = 2,
};

struct DevOverlayTargetSpec {
	uint32_t flowNodeIndex = UINT32_MAX;
	uint32_t clayNodeIndex = UINT32_MAX;
	uint32_t clayId = 0u;
	FlowDefinitionID definition{};
	::FlowUi::detail::element::ElementInstanceKey instanceKey{};
	DevInspectTargetKind kind = DevInspectTargetKind::Flow;

	[[nodiscard]] constexpr bool isValid() const noexcept {
		if (kind == DevInspectTargetKind::Clay) {
			return clayNodeIndex != UINT32_MAX;
		}
		return flowNodeIndex != UINT32_MAX;
	}
};

struct DevOverlaySelectionSpec {
	DevOverlayTargetSpec primaryTarget{};
	DevOverlayTargetSpec secondaryTarget{};
	DevOverlayModeFlags modeFlags = DevOverlayModeFlags::Default;
	float uiScaleFactor = 1.0f;
};

} // namespace FlowUi::devSystems::tooling

#endif
