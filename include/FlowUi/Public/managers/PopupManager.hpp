#pragma once

#include "FlowUi/Public/BuildConfig.hpp"

#include <cstdint>

#include "FlowUi/Public/identificationTypes/ElementID.hpp"
#include "FlowUi/Public/errorManagement/ErrorPolicy.hpp"
#include "FlowUi/Public/errorManagement/Result.hpp"
#include "FlowUi/Public/managerStructs/InputTypes.hpp"
#include "FlowUi/Public/identificationTypes/WindowId.hpp"
#include "FlowUi/Public/managerStructs/PopupTypes.hpp"

namespace FlowUi {

class UiManager;
struct FrameInput;
namespace detail::storage { class IStorageSystem; }
namespace detail::manager_storage { struct PopupManagerState; }
namespace devSystems { class MemorySampleSink; }

/**
 * @brief Window-owned placement, layering, and dismissal service for popup elements.
 *
 * Obtain this manager through UiManager::popups(). Request each open popup once
 * during UI construction and use its PopupFrame to emit the floating Clay root.
 * Placement and dismissal state participate in the window's frame commit.
 */
class PopupManager {
public:
#if FLOW_UI_DEV_MODE
	/** Whether a visible popup owns keyboard navigation in the preceding committed frame. */
	[[nodiscard]] bool has_open_popups() const noexcept;
#endif
#if FLOW_UI_DEV_MODE && FLOWUI_DEV_MEMORY_LEVEL >= 2
	/** @brief Append observation-only development memory accounting to the borrowed sink. */
	void appendDevMemorySamples(devSystems::MemorySampleSink& sink) const noexcept;
#endif
	/**
	 * @brief Submit an open popup for the active window frame.
	 * @param popupId Stable popup identity in this window; zero yields an empty frame.
	 * @param request Anchor, placement, layer, sizing, and dismissal behavior.
	 * @return Placement/visibility information, or PopupFrameInactive or a
	 * duplicate-submission error according to the configured policy. A successful
	 * frame may be hidden or measure-only; inspect it before drawing contents.
	 */
	[[nodiscard]] Result<PopupFrame> request(FlowElementID popupId, const PopupRequest& request);
	/** @brief Submit a popup addressed by a global ID; the request() frame contract applies. */
	[[nodiscard]] Result<PopupFrame> request(GlobalFlowID popupId, const PopupRequest& request);
	/** @brief Submit a popup addressed by a semantic part ID; the request() frame contract applies. */
	[[nodiscard]] Result<PopupFrame> request(FlowElementPartID popupId, const PopupRequest& request);

	/**
	 * @brief Mark a popup dismissed and record a consumable dismissal notification.
	 * @param popupId Popup identity; zero is ignored.
	 * During a build, changes commit with that frame. Outside a build they update
	 * committed state immediately. An already submitted popup becomes hidden.
	 */
	void dismiss(FlowElementID popupId);
	/** @brief Dismiss a popup addressed by a global ID. */
	void dismiss(GlobalFlowID popupId);
	/** @brief Dismiss a popup addressed by a semantic part ID. */
	void dismiss(FlowElementPartID popupId);

	/**
	 * @brief Read and clear a pending dismissal notification for popupId.
	 * @return True once per pending dismissal; false for zero, unknown, or
	 * already consumed IDs. Reading during a canceled build is rolled back.
	 */
	[[nodiscard]] bool consumeDismissed(FlowElementID popupId);
	/** @brief Consume a dismissal notification addressed by a global ID. */
	[[nodiscard]] bool consumeDismissed(GlobalFlowID popupId);
	/** @brief Consume a dismissal notification addressed by a semantic part ID. */
	[[nodiscard]] bool consumeDismissed(FlowElementPartID popupId);

private:
	friend class UiManager;

	void init(detail::storage::IStorageSystem& storage, WindowId window, const ErrorPolicy& policy);
	void destroy() noexcept;
	void beginFrame(
		const FrameInput& currentInput,
		const FrameInput& previousInput,
		float viewportWidth,
		float viewportHeight);
	void endFrame();
	void cancelFrame() noexcept;
	[[nodiscard]] bool suppressesAllPrimaryPointerInput() const;
	[[nodiscard]] uint32_t suppressedAnchorClayId() const;

	[[nodiscard]] Result<PopupFrame> requestImpl(uint64_t popupKey, const PopupRequest& request);
	void dismissImpl(uint64_t popupKey);
	[[nodiscard]] bool consumeDismissedImpl(uint64_t popupKey);

	[[nodiscard]] detail::manager_storage::PopupManagerState& state();
	[[nodiscard]] const detail::manager_storage::PopupManagerState& state() const;

	detail::storage::IStorageSystem* storage_ = nullptr;
	WindowId window_ = InvalidWindowId;
	uint64_t stateHandle_ = 0;
	PopupDuplicatePolicy duplicatePolicy_ = PopupDuplicatePolicy::FirstSubmissionWins;
	PopupMissingAnchorPolicy missingAnchorPolicy_ = PopupMissingAnchorPolicy::SkipPopup;
	PopupCapacityPolicy capacityPolicy_ = PopupCapacityPolicy::ClampLayer;
};

} // namespace FlowUi
