#pragma once

/** @file
 * @brief Application integration declarations for the optional development services.
 */

#include "FlowUi/Public/BuildConfig.hpp"

#if FLOW_UI_DEV_MODE
#include <cstdint>
#include <span>

#include "clay.h"

struct VulkanUiRenderer;
namespace FlowUi::detail::manager_storage {
struct FontFrameView;
}
namespace FlowUi::detail::storage {
struct BindingHotRecord;
}

namespace FlowUi {

namespace devSystems {
class DevMonitoringAndReporting;
struct PerformanceCaptureSettings;
class DevTooling;

/** @brief Borrowed completed-frame replay inputs for development previews.
 *
 * The source owns none of these objects. Keep the originating renderer and
 * frame publication alive until replay finishes; do not retain views across
 * replacement of the source frame. Available only in development builds.
 */
struct DevUiReplaySource {
	/** @brief Borrowed renderer that owns the referenced replay resources. */
	const ::VulkanUiRenderer* renderer = nullptr;
	/** @brief Borrowed font bindings from the source frame publication. */
	const ::FlowUi::detail::manager_storage::FontFrameView* fontFrameView = nullptr;
	/**
	 * @brief Borrowed completed Clay render commands; a valid replay requires nonempty commands.
	 */
	const Clay_RenderCommandArray* commands = nullptr;
	/** @brief Borrowed texture records aligned with the source frame publication. */
	std::span<const ::FlowUi::detail::storage::BindingHotRecord> textureBindings{};
	/** @brief Source root bounds in layout points used to transform the replay. */
	Clay_BoundingBox sourceRootBounds{};
	/** @brief Caller identity used to distinguish replay requests. */
	uint64_t requestKey = 0u;
	/** @brief Serial identifying the captured source publication. */
	uint64_t captureSerial = 0u;
	/** @brief Source renderer slot that owns the referenced texture bindings. */
	uint32_t textureFrameSlot = 0u;
	/** @brief Multiplier from layout points to physical replay pixels. */
	float pointsToPixelsScale = 1.0f;

	/** @brief Return whether renderer, font bindings and nonempty commands are present. */
	[[nodiscard]] explicit operator bool() const noexcept {
		return renderer != nullptr && fontFrameView != nullptr && commands != nullptr &&
			   commands->length > 0;
	}
};
} // namespace devSystems

} // namespace FlowUi
#endif
