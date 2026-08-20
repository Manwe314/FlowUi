#pragma once

#include <cstdint>

#include "FlowUi/ElementID.hpp"
#include "FlowUi/WindowId.hpp"
#include "managers/structs/PopupManagerStructs.hpp"

namespace FlowUi {

class UiManager;
struct FrameInput;
namespace detail::storage { class IStorageSystem; }
namespace detail::manager_storage { struct PopupManagerState; }

class PopupManager {
public:
	[[nodiscard]] PopupFrame request(FlowElementID popupId, const PopupRequest& request);
	[[nodiscard]] PopupFrame request(GlobalFlowID popupId, const PopupRequest& request);
	[[nodiscard]] PopupFrame request(FlowElementPartID popupId, const PopupRequest& request);

	void dismiss(FlowElementID popupId);
	void dismiss(GlobalFlowID popupId);
	void dismiss(FlowElementPartID popupId);

	[[nodiscard]] bool consumeDismissed(FlowElementID popupId);
	[[nodiscard]] bool consumeDismissed(GlobalFlowID popupId);
	[[nodiscard]] bool consumeDismissed(FlowElementPartID popupId);

private:
	friend class UiManager;

	void init(detail::storage::IStorageSystem& storage, WindowId window);
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

	[[nodiscard]] PopupFrame requestImpl(uint64_t popupKey, const PopupRequest& request);
	void dismissImpl(uint64_t popupKey);
	[[nodiscard]] bool consumeDismissedImpl(uint64_t popupKey);

	[[nodiscard]] detail::manager_storage::PopupManagerState& state();
	[[nodiscard]] const detail::manager_storage::PopupManagerState& state() const;

	detail::storage::IStorageSystem* storage_ = nullptr;
	WindowId window_ = InvalidWindowId;
	uint64_t stateHandle_ = 0;
};

} // namespace FlowUi
