#pragma once

#include "FlowUi/Private/ApiBoundary.hpp"

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
namespace detail::storage {
class StorageBinding;
}
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
	/**
	 * @brief Read a caught failure requiring cancellation of the popup frame.
	 *
	 * UiManager checks the latch during frame completion.
	 *
	 * @return Success means no popup frame error has been latched; an error requires cancellation of the surrounding frame.
	 * Recoverable initialization, validation, allocation, and storage failures are returned as FlowUiError; no exception escapes this noexcept operation.
	 *
	 * @code{.cpp}
	 * auto result = manager.frameStatus();
	 * if (!result) {
	 *     throw FlowUi::FlowUiException(result.error());
	 * }
	 * @endcode
	 */
	[[nodiscard]] Status frameStatus() const noexcept {
		return frameError_ ? Status{unexpectedError(frameError_)} : Status{};
	}
#if FLOW_UI_DEV_MODE
	/** Whether a visible popup owns keyboard navigation in the preceding committed frame. */
	[[nodiscard]] bool has_open_popups() const noexcept;
#endif
#if FLOW_UI_DEV_MODE && FLOWUI_DEV_MEMORY_LEVEL >= 2
	/** @brief Append observation-only development memory accounting to the borrowed sink. */
	void appendDevMemorySamples(devSystems::MemorySampleSink& sink) const noexcept;
#endif
	/**
	 * @brief Submit an open popup for the active window UI build.
	 *
	 * Request each popup once per active build. Emit the floating root when visible or measureOnly, using its floating configuration. Placement/dismissal participate in commit; canceling the build rolls them back. Default unknown-size policy uses a hidden measurement frame.
	 *
	 * @return Success contains PopupFrame, which may be hidden or measureOnly; a zero ID yields an empty frame. Inactive frame or configured duplicate/placement failures return errors.
	 * Recoverable initialization, validation, allocation, and storage failures are returned as FlowUiError; no exception escapes this noexcept operation.
	 *
	 * @code{.cpp}
	 * FlowUi::PopupRequest request{};
	 * request.anchor = FlowUi::PopupAnchor::parent();
	 * request.expectedSize = Clay_Dimensions{240.0f, 160.0f};
	 * auto result = manager.request(fieldId, request);
	 * if (!result) {
	 *     throw FlowUi::FlowUiException(result.error());
	 * }
	 * const auto popup = *result;
	 * @endcode
	 */
	[[nodiscard]] Result<PopupFrame> request(FlowElementID popupId, const PopupRequest& request) noexcept;
	/**
	 * @brief Submit an open popup for the active window UI build.
	 *
	 * Request each popup once per active build. Emit the floating root when visible or measureOnly, using its floating configuration. Placement/dismissal participate in commit; canceling the build rolls them back. Default unknown-size policy uses a hidden measurement frame.
	 *
	 * @return Success contains PopupFrame, which may be hidden or measureOnly; a zero ID yields an empty frame. Inactive frame or configured duplicate/placement failures return errors.
	 * Recoverable initialization, validation, allocation, and storage failures are returned as FlowUiError; no exception escapes this noexcept operation.
	 *
	 * @code{.cpp}
	 * FlowUi::PopupRequest request{};
	 * request.anchor = FlowUi::PopupAnchor::parent();
	 * request.expectedSize = Clay_Dimensions{240.0f, 160.0f};
	 * auto result = manager.request(fieldId, request);
	 * if (!result) {
	 *     throw FlowUi::FlowUiException(result.error());
	 * }
	 * const auto popup = *result;
	 * @endcode
	 */
	[[nodiscard]] Result<PopupFrame> request(GlobalFlowID popupId, const PopupRequest& request) noexcept;
	/**
	 * @brief Submit an open popup for the active window UI build.
	 *
	 * Request each popup once per active build. Emit the floating root when visible or measureOnly, using its floating configuration. Placement/dismissal participate in commit; canceling the build rolls them back. Default unknown-size policy uses a hidden measurement frame.
	 *
	 * @return Success contains PopupFrame, which may be hidden or measureOnly; a zero ID yields an empty frame. Inactive frame or configured duplicate/placement failures return errors.
	 * Recoverable initialization, validation, allocation, and storage failures are returned as FlowUiError; no exception escapes this noexcept operation.
	 *
	 * @code{.cpp}
	 * FlowUi::PopupRequest request{};
	 * request.anchor = FlowUi::PopupAnchor::parent();
	 * request.expectedSize = Clay_Dimensions{240.0f, 160.0f};
	 * auto result = manager.request(fieldId, request);
	 * if (!result) {
	 *     throw FlowUi::FlowUiException(result.error());
	 * }
	 * const auto popup = *result;
	 * @endcode
	 */
	[[nodiscard]] Result<PopupFrame> request(FlowElementPartID popupId, const PopupRequest& request) noexcept;

	/**
	 * @brief Dismiss a popup and record a consumable notification.
	 *
	 * During a build the change commits with the frame and is rolled back on cancel. Outside a build it commits immediately. An already submitted popup becomes hidden.
	 *
	 * @post The dismissal request has completed; zero IDs are ignored.
	 * @throws FlowUiException for library validation/storage failures. Allocation and other operation exceptions propagate unchanged. Every exception latches frame failure; abandon the current frame.
	 *
	 * @code{.cpp}
	 * manager.dismiss(fieldId);
	 * @endcode
	 */
	void dismiss(FlowElementID popupId);
	/**
	 * @brief Dismiss a popup and record a consumable notification.
	 *
	 * During a build the change commits with the frame and is rolled back on cancel. Outside a build it commits immediately. An already submitted popup becomes hidden.
	 *
	 * @post The dismissal request has completed; zero IDs are ignored.
	 * @throws FlowUiException for library validation/storage failures. Allocation and other operation exceptions propagate unchanged. Every exception latches frame failure; abandon the current frame.
	 *
	 * @code{.cpp}
	 * manager.dismiss(fieldId);
	 * @endcode
	 */
	void dismiss(GlobalFlowID popupId);
	/**
	 * @brief Dismiss a popup and record a consumable notification.
	 *
	 * During a build the change commits with the frame and is rolled back on cancel. Outside a build it commits immediately. An already submitted popup becomes hidden.
	 *
	 * @post The dismissal request has completed; zero IDs are ignored.
	 * @throws FlowUiException for library validation/storage failures. Allocation and other operation exceptions propagate unchanged. Every exception latches frame failure; abandon the current frame.
	 *
	 * @code{.cpp}
	 * manager.dismiss(fieldId);
	 * @endcode
	 */
	void dismiss(FlowElementPartID popupId);

	/**
	 * @brief Read and consume one pending popup dismissal.
	 *
	 * Consumption during a canceled build is rolled back.
	 *
	 * @return true once per notification, or false for zero, unknown, or already-consumed IDs.
	 * @throws FlowUiException for library validation/storage failures. Allocation and other operation exceptions propagate unchanged. Every exception latches frame failure; abandon the current frame.
	 *
	 * @code{.cpp}
	 * const auto dismissed = manager.consumeDismissed(fieldId);
	 * @endcode
	 */
	[[nodiscard]] bool consumeDismissed(FlowElementID popupId);
	/**
	 * @brief Read and consume one pending popup dismissal.
	 *
	 * Consumption during a canceled build is rolled back.
	 *
	 * @return true once per notification, or false for zero, unknown, or already-consumed IDs.
	 * @throws FlowUiException for library validation/storage failures. Allocation and other operation exceptions propagate unchanged. Every exception latches frame failure; abandon the current frame.
	 *
	 * @code{.cpp}
	 * const auto dismissed = manager.consumeDismissed(fieldId);
	 * @endcode
	 */
	[[nodiscard]] bool consumeDismissed(GlobalFlowID popupId);
	/**
	 * @brief Read and consume one pending popup dismissal.
	 *
	 * Consumption during a canceled build is rolled back.
	 *
	 * @return true once per notification, or false for zero, unknown, or already-consumed IDs.
	 * @throws FlowUiException for library validation/storage failures. Allocation and other operation exceptions propagate unchanged. Every exception latches frame failure; abandon the current frame.
	 *
	 * @code{.cpp}
	 * const auto dismissed = manager.consumeDismissed(fieldId);
	 * @endcode
	 */
	[[nodiscard]] bool consumeDismissed(FlowElementPartID popupId);

private:
	mutable FlowUiError frameError_{};
	friend class UiManager;

	void init(detail::storage::StorageBinding& storage, WindowId window, const ErrorPolicy& policy);
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

	detail::storage::StorageBinding* storage_ = nullptr;
	WindowId window_ = InvalidWindowId;
	uint64_t stateHandle_ = 0;
	PopupDuplicatePolicy duplicatePolicy_ = PopupDuplicatePolicy::FirstSubmissionWins;
	PopupMissingAnchorPolicy missingAnchorPolicy_ = PopupMissingAnchorPolicy::SkipPopup;
	PopupCapacityPolicy capacityPolicy_ = PopupCapacityPolicy::ClampLayer;
};

} // namespace FlowUi
