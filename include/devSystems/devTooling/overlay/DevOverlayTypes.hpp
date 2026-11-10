#pragma once

#include "FlowUi/Public/BuildConfig.hpp"

#if FLOW_UI_DEV_MODE

#include <cstdint>

#include "FlowUi/Public/identificationTypes/ElementID.hpp"
#include "FlowUi/Private/elementSystem/ElementInstanceKey.hpp"

namespace FlowUi::devSystems::tooling {

/** @brief Bit flags selecting visual inspection surfaces. */
enum class DevOverlayModeFlags : uint32_t {
	/** @brief Generate no overlay surfaces. */
	None = 0,
	/** @brief Show element bounds and box-model information. */
	BoxModel = 1u << 0u,
	/** @brief Show rulers and target separation. */
	RulersAndDistance = 1u << 1u,
	/** @brief Show ownership hierarchy relationships. */
	TreeHierarchy = 1u << 2u,
	/** @brief Show text metrics and layout annotations. */
	Typography = 1u << 3u,
	/** @brief Show clip/scissor relationships. */
	ScissorAndClip = 1u << 4u,
	/** @brief Show renderer run diagnostics. */
	RenderRunDiagnostics = 1u << 5u,
	/** @brief Enable box-model and distance surfaces. */
	Default = (1u << 0u) | (1u << 1u),
};

/** @brief Combine enabled flags without mutating either input. */
[[nodiscard]] constexpr DevOverlayModeFlags operator|(
	DevOverlayModeFlags left,
	DevOverlayModeFlags right) noexcept {
	return static_cast<DevOverlayModeFlags>(
		static_cast<uint32_t>(left) | static_cast<uint32_t>(right));
}

/** @brief Add right-hand flags to left and return left. */
constexpr DevOverlayModeFlags& operator|=(
	DevOverlayModeFlags& left,
	DevOverlayModeFlags right) noexcept {
	left = left | right;
	return left;
}

/** @brief Return whether the two flag sets share any bit. */
[[nodiscard]] constexpr bool operator&(
	DevOverlayModeFlags left,
	DevOverlayModeFlags right) noexcept {
	return (static_cast<uint32_t>(left) & static_cast<uint32_t>(right)) != 0u;
}

/** @brief Return whether value contains any bit in flag; combined flags use an any-bit test. */
[[nodiscard]] constexpr bool hasFlag(
	DevOverlayModeFlags value,
	DevOverlayModeFlags flag) noexcept {
	return value & flag;
}

/** @brief Identity domain of an inspection target. */
enum class DevInspectTargetKind : uint8_t {
	/** @brief No target identity. */
	None = 0,
	/** @brief Typed Flow invocation identity. */
	Flow = 1,
	/** @brief Raw Clay layout identity. */
	Clay = 2,
};

/** @brief Frame-local resolved Flow or Clay target indices for an inspection overlay. */
struct DevOverlayTargetSpec {
	/** @brief Resolved Flow node index, or UINT32_MAX when absent. */
	uint32_t flowNodeIndex = UINT32_MAX;
	/** @brief Resolved Clay node index, or UINT32_MAX when absent. */
	uint32_t clayNodeIndex = UINT32_MAX;
	/** @brief Numeric Clay ID for renderer correlation. */
	uint32_t clayId = 0u;
	/** @brief Resolved Flow definition identity. */
	FlowDefinitionID definition{};
	/** @brief Resolved Flow instance identity. */
	::FlowUi::detail::element::ElementInstanceKey instanceKey{};
	/** @brief Whether the target resolves a Flow or raw Clay node. */
	DevInspectTargetKind kind = DevInspectTargetKind::Flow;

	/** @brief Return whether the index for the selected Flow/Clay target kind is present; validate it against its snapshot before dereferencing. */
	[[nodiscard]] constexpr bool isValid() const noexcept {
		if (kind == DevInspectTargetKind::Clay) {
			return clayNodeIndex != UINT32_MAX;
		}
		return flowNodeIndex != UINT32_MAX;
	}
};

/** @brief Copied renderer selection with optional comparison target and enabled surfaces. */
struct DevOverlaySelectionSpec {
	/** @brief Main inspected target for the supplied snapshot. */
	DevOverlayTargetSpec primaryTarget{};
	/** @brief Optional comparison target for distance/ruler overlays. */
	DevOverlayTargetSpec secondaryTarget{};
	/** @brief Overlay surfaces to generate. */
	DevOverlayModeFlags modeFlags = DevOverlayModeFlags::Default;
	/** @brief Scale applied to overlay annotations. */
	float uiScaleFactor = 1.0f;
};

} // namespace FlowUi::devSystems::tooling

#endif
