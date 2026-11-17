#pragma once

#include "FlowUi/Public/errorManagement/Result.hpp"
#include "FlowUi/Private/ApiBoundary.hpp"
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
 * Worker producers must stop before their sink is destroyed. Emission errors distinguish invalid
 * input, an uninitialized sink and queue overflow. After overflow, the input engine cancels affected
 * held state before consuming subsequent input. Success means queue acceptance, not delivery.
 */
class InputSink final {
public:
	/** Construct a detached sink; initialize before attaching producers. */
	InputSink() noexcept = default;
	/** Reserve bounded ingress storage before producers are attached. Zero capacity, repeated
	 * initialization and allocation failure return errors. Actual capacity may exceed the request.
	 * @par Example
	 * @code{.cpp}
	 * FlowUi::InputSink sink;
	 * const auto status = sink.initialize(1024);
	 * if (!status)
	 *     throw FlowUi::FlowUiException(status.error());
	 * @endcode
	 */
	[[nodiscard]] Status initialize(std::size_t capacity = 4096) noexcept try {
		if (capacity_ != 0)
			return unexpectedError(makeError(ErrorCode::ObjectAlreadyInitialized, ErrorSite::InputQueueEvent));
		if (capacity == 0)
			return unexpectedError(makeError(ErrorCode::PlatformInputQueueOverflow, ErrorSite::InputQueueEvent));
		pending_.reserve(capacity);
		capacity_ = pending_.capacity();
		return {};
	} catch (...) {
		return unexpectedError(detail::currentExceptionError(ErrorSite::InputQueueEvent));
	}
	/** Copying event ingress is disabled; keep one sink owned by its host. */
	InputSink(const InputSink&) = delete;
	/** Copy assignment is disabled; producers retain the original sink reference. */
	InputSink& operator=(const InputSink&) = delete;
	/** Queue a checked FlowUi key transition.
	 * @par Example
	 * @code{.cpp}
	 * const FlowUi::KeyEvent event{
	 *     .device = 42,
	 *     .target = window,
	 *     .key = FlowUi::Key::A,
	 *     .pressed = true
	 * };
	 * const auto status = sink.key(event);
	 * if (!status)
	 *     throw FlowUi::FlowUiException(status.error());
	 * @endcode
	 */
	[[nodiscard]] Status key(const KeyEvent& event) noexcept {
		if (!keySlot(event.key) || event.modifiers >= 16)
			return invalid();
		return emit(event);
	}
	/** Queue committed Unicode, rejecting surrogate/non-scalar values.
	 * @par Example
	 * @code{.cpp}
	 * const FlowUi::TextInputEvent event{
	 *     .device = 42,
	 *     .target = window,
	 *     .codepoint = U'λ'
	 * };
	 * const auto status = sink.text(event);
	 * if (!status)
	 *     throw FlowUi::FlowUiException(status.error());
	 * @endcode
	 */
	[[nodiscard]] Status text(const TextInputEvent& event) noexcept {
		if (event.codepoint > 0x10ffff || (event.codepoint >= 0xd800 && event.codepoint <= 0xdfff))
			return invalid();
		return emit(event);
	}
	/** Queue finite pointer coordinates in window space.
	 * @par Example
	 * @code{.cpp}
	 * const FlowUi::PointerMoveEvent event{
	 *     .device = 42,
	 *     .target = window,
	 *     .kind = FlowUi::PointerKind::Virtual,
	 *     .x = 80.0f,
	 *     .y = 32.0f
	 * };
	 * const auto status = sink.pointerMove(event);
	 * if (!status)
	 *     throw FlowUi::FlowUiException(status.error());
	 * @endcode
	 */
	[[nodiscard]] Status pointerMove(const PointerMoveEvent& event) noexcept {
		if (!std::isfinite(event.x) || !std::isfinite(event.y) ||
			static_cast<unsigned>(event.kind) > static_cast<unsigned>(PointerKind::Virtual))
			return invalid();
		return emit(event);
	}
	/** Queue one of the five supported compatibility buttons.
	 * @par Example
	 * @code{.cpp}
	 * const FlowUi::PointerButtonEvent event{
	 *     .device = 42,
	 *     .target = window,
	 *     .kind = FlowUi::PointerKind::Virtual,
	 *     .button = FlowUi::PointerButton::Primary,
	 *     .pressed = true,
	 *     .phase = FlowUi::InputPhase::Begin
	 * };
	 * const auto status = sink.pointerButton(event);
	 * if (!status)
	 *     throw FlowUi::FlowUiException(status.error());
	 * @endcode
	 */
	[[nodiscard]] Status pointerButton(const PointerButtonEvent& event) noexcept {
		if (static_cast<unsigned>(event.button) >= 5 ||
			static_cast<unsigned>(event.kind) > static_cast<unsigned>(PointerKind::Virtual) ||
			static_cast<unsigned>(event.phase) > static_cast<unsigned>(InputPhase::Cancel))
			return invalid();
		return emit(event);
	}
	/** Queue finite scroll deltas.
	 * @par Example
	 * @code{.cpp}
	 * const FlowUi::ScrollInputEvent event{
	 *     .device = 42,
	 *     .target = window,
	 *     .y = 1.0f
	 * };
	 * const auto status = sink.scroll(event);
	 * if (!status)
	 *     throw FlowUi::FlowUiException(status.error());
	 * @endcode
	 */
	[[nodiscard]] Status scroll(const ScrollInputEvent& event) noexcept {
		if (!std::isfinite(event.x) || !std::isfinite(event.y))
			return invalid();
		return emit(event);
	}
	/** Queue observed focus.
	 * @par Example
	 * @code{.cpp}
	 * const FlowUi::FocusInputEvent event{
	 *     .target = window,
	 *     .focused = false
	 * };
	 * const auto status = sink.focus(event);
	 * if (!status)
	 *     throw FlowUi::FlowUiException(status.error());
	 * @endcode
	 */
	[[nodiscard]] Status focus(const FocusInputEvent& event) noexcept { return emit(event); }
	/** Cancel device/source contributions.
	 * @par Example
	 * @code{.cpp}
	 * const FlowUi::DeviceResetEvent event{
	 *     .device = 42,
	 *     .target = window
	 * };
	 * const auto status = sink.reset(event);
	 * if (!status)
	 *     throw FlowUi::FlowUiException(status.error());
	 * @endcode
	 */
	[[nodiscard]] Status reset(const DeviceResetEvent& event) noexcept { return emit(event); }
	/** Queue a device connection transition.
	 * @par Example
	 * @code{.cpp}
	 * const FlowUi::DeviceConnectionEvent event{
	 *     .device = 42,
	 *     .target = window,
	 *     .connected = false
	 * };
	 * const auto status = sink.connection(event);
	 * if (!status)
	 *     throw FlowUi::FlowUiException(status.error());
	 * @endcode
	 */
	[[nodiscard]] Status connection(const DeviceConnectionEvent& event) noexcept {
		return emit(event);
	}
	/** Queue a logical UI action, consumed through the compatibility navigation projection.
	 * @par Example
	 * @code{.cpp}
	 * const FlowUi::NavigationInputEvent event{
	 *     .device = 42,
	 *     .target = window,
	 *     .action = FlowUi::NavigationAction::Activate,
	 *     .phase = FlowUi::InputPhase::Begin,
	 *     .magnitude = 1.0f
	 * };
	 * const auto status = sink.navigation(event);
	 * if (!status)
	 *     throw FlowUi::FlowUiException(status.error());
	 * @endcode
	 */
	[[nodiscard]] Status navigation(const NavigationInputEvent& event) noexcept {
		if (!std::isfinite(event.magnitude) ||
			static_cast<unsigned>(event.action) > static_cast<unsigned>(NavigationAction::Cancel) ||
			static_cast<unsigned>(event.phase) > static_cast<unsigned>(InputPhase::Cancel))
			return invalid();
		return emit(event);
	}
	/** Maximum unread event count; fixed after initialization. */
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
		if (capacity_ == 0)
			return unexpectedError(makeError(ErrorCode::ObjectNotInitialized, ErrorSite::InputQueueEvent));
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
