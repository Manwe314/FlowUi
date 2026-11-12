#pragma once

#include "FlowUi/Public/errorManagement/Result.hpp"
#include "FlowUi/Public/input/InputEvents.hpp"
#include <atomic>
#include <cmath>
#include <cstddef>
#include <span>
#include <vector>

namespace FlowUi::detail::input {
class InputState;
class InputService;
} // namespace FlowUi::detail::input

namespace FlowUi {

/** Bounded ingress for native/translator events. Emission is thread safe and never allocates.
 * Worker producers must stop before their sink is destroyed. A failed emission reports overflow;
 * the input engine cancels affected held state before consuming subsequent input.
 */
class InputSink final {
public:
	/** Reserve all ingress storage before producers are attached. */
	explicit InputSink(std::size_t capacity = 4096) {
		pending_.reserve(capacity);
		capacity_ = pending_.capacity();
	}
	InputSink(const InputSink&) = delete;
	InputSink& operator=(const InputSink&) = delete;
	/** Queue a checked FlowUi key transition. */
	[[nodiscard]] Status key(const KeyEvent& event) noexcept {
		if (!keySlot(event.key) || event.modifiers >= 16)
			return invalid();
		return emit(event);
	}
	/** Queue committed Unicode, rejecting surrogate/non-scalar values. */
	[[nodiscard]] Status text(const TextInputEvent& event) noexcept {
		if (event.codepoint > 0x10ffff || (event.codepoint >= 0xd800 && event.codepoint <= 0xdfff))
			return invalid();
		return emit(event);
	}
	/** Queue finite pointer coordinates in window space. */
	[[nodiscard]] Status pointerMove(const PointerMoveEvent& event) noexcept {
		if (!std::isfinite(event.x) || !std::isfinite(event.y) ||
			static_cast<unsigned>(event.kind) > static_cast<unsigned>(PointerKind::Virtual))
			return invalid();
		return emit(event);
	}
	/** Queue one of the five supported compatibility buttons. */
	[[nodiscard]] Status pointerButton(const PointerButtonEvent& event) noexcept {
		if (static_cast<unsigned>(event.button) >= 5 ||
			static_cast<unsigned>(event.kind) > static_cast<unsigned>(PointerKind::Virtual) ||
			static_cast<unsigned>(event.phase) > static_cast<unsigned>(InputPhase::Cancel))
			return invalid();
		return emit(event);
	}
	/** Queue finite scroll deltas. */
	[[nodiscard]] Status scroll(const ScrollInputEvent& event) noexcept {
		if (!std::isfinite(event.x) || !std::isfinite(event.y))
			return invalid();
		return emit(event);
	}
	/** Queue observed focus. */
	[[nodiscard]] Status focus(const FocusInputEvent& event) noexcept { return emit(event); }
	/** Cancel device/source contributions. */
	[[nodiscard]] Status reset(const DeviceResetEvent& event) noexcept { return emit(event); }
	/** Queue a device connection transition. */
	[[nodiscard]] Status connection(const DeviceConnectionEvent& event) noexcept {
		return emit(event);
	}
	/** Queue a logical UI action, consumed through the compatibility navigation projection. */
	[[nodiscard]] Status navigation(const NavigationInputEvent& event) noexcept {
		if (!std::isfinite(event.magnitude) ||
			static_cast<unsigned>(event.action) > static_cast<unsigned>(NavigationAction::Cancel) ||
			static_cast<unsigned>(event.phase) > static_cast<unsigned>(InputPhase::Cancel))
			return invalid();
		return emit(event);
	}
	/** Maximum unread event count; fixed after construction. */
	[[nodiscard]] std::size_t capacity() const noexcept { return capacity_; }

private:
	friend class detail::input::InputState;
	friend class detail::input::InputService;
	class Guard {
	public:
		explicit Guard(std::atomic_flag& lock) noexcept : lock_(lock) {
			while (lock_.test_and_set(std::memory_order_acquire)) {
				std::atomic_signal_fence(std::memory_order_seq_cst);
			}
		}
		~Guard() { lock_.clear(std::memory_order_release); }

	private:
		std::atomic_flag& lock_;
	};
	[[nodiscard]] static Status invalid() noexcept {
		return unexpectedError(makeError(ErrorCode::InputRangeInvalid, ErrorSite::InputQueueEvent));
	}
	template <class Event>
	[[nodiscard]] Status emit(const Event& event) noexcept {
		Guard guard(lock_);
		if (pending_.size() == pending_.capacity()) {
			overflow_ = true;
			return unexpectedError(
				makeError(ErrorCode::PlatformInputQueueOverflow, ErrorSite::InputQueueEvent));
		}
		pending_.emplace_back(event);
		return {};
	}
	void take(std::vector<InputEvent>& destination, bool& overflow) noexcept {
		Guard guard(lock_);
		destination.clear();
		pending_.swap(destination);
		overflow = overflow_;
		overflow_ = false;
	}
	void clear() noexcept {
		Guard guard(lock_);
		pending_.clear();
		overflow_ = false;
	}
	std::vector<InputEvent> pending_;
	std::size_t capacity_ = 0;
	std::atomic_flag lock_ = ATOMIC_FLAG_INIT;
	bool overflow_ = false;
};

} // namespace FlowUi
