#pragma once

#include "FlowUi/Private/inputService/InputQueue.hpp"
#include "FlowUi/Public/input/InputSink.hpp"
#include <array>
#include <memory>
#include <span>
#include <vector>

namespace FlowUi::detail::input {

/** Stable per-window semantic state. Native bindings borrow its sink until quiescence. */
class InputState final {
public:
	explicit InputState(
		std::size_t textCapacity = 4096,
		InputQueueOverflowPolicy overflowPolicy = InputQueueOverflowPolicy::DropNewest,
		std::size_t eventCapacity = 4096);
	InputState(const InputState&) = delete;
	InputState& operator=(const InputState&) = delete;
	/** Bound native event destination. */
	[[nodiscard]] InputSink& sink() noexcept { return sink_; }
	/** Compatibility accumulator, including independent development press-edge state. */
	[[nodiscard]] InputQueue& queue() noexcept { return queue_; }
	[[nodiscard]] const InputQueue& queue() const noexcept { return queue_; }
	/** Read-only ordered frame batch, valid until next frameInput/reset/unregister. */
	[[nodiscard]] std::span<const InputEvent> events() const noexcept {
		return syntheticFrame_ && syntheticState_ ? syntheticState_->events()
												  : std::span<const InputEvent>(frameEvents_);
	}
	/** Observe normalized held state without consuming an event batch. */
	[[nodiscard]] bool keyDown(Key key) const noexcept;
	/** Current host focus after processing the latest native input. */
	[[nodiscard]] bool focused() const noexcept { return focused_; }
	/** Whether the consumed frame cancelled its primary gesture instead of releasing normally. */
	[[nodiscard]] bool primaryPointerCanceled() const noexcept {
		return syntheticFrame_ && syntheticState_ ? syntheticState_->primaryPointerCanceled()
												  : framePrimaryPointerCanceled_;
	}
	/** Process native ingress; duplicate sampled states create no extra edges. */
	[[nodiscard]] Status processPending() noexcept;
	/** Cancel all held sources and discard unconsumed batches/ingress. */
	void reset() noexcept;
	/** Identify the final stable AppWindow address's semantic destination. */
	void bindTarget(WindowId target) noexcept { target_ = target; }

private:
	friend class InputService;
	struct Contribution {
		std::array<bool, FrameInput::kKeyboardKeyCount> keys{};
		std::array<bool, FrameInput::kMouseButtonCount> buttons{};
		InputDeviceId device = 0;
		InputSourceId source = 0;
		InputContactId contact = 0;
		float pointerX = 0;
		float pointerY = 0;
		PointerKind kind = PointerKind::Mouse;
		bool primary = true;
		bool pointerInside = false;
		bool pointerValid = false;
	};
	[[nodiscard]] Contribution* contribution(InputDeviceId device, InputSourceId source,
											 InputContactId contact = 0,
											 PointerKind kind = PointerKind::Mouse) noexcept;
	[[nodiscard]] Status accept(InputEvent event) noexcept;
	void cancel(InputDeviceId device = 0, InputSourceId source = 0) noexcept;
	void rebuildProjection() noexcept;
	[[nodiscard]] FrameInput consume(double deltaSeconds);

	InputQueue queue_;
	InputSink sink_;
	std::vector<InputEvent> ingress_;
	std::vector<InputEvent> pendingEvents_;
	std::vector<InputEvent> frameEvents_;
	std::vector<Contribution> contributions_;
	std::unique_ptr<InputState> syntheticState_;
	WindowId target_ = InvalidWindowId;
	std::uint64_t frameSerial_ = 0;
	std::uint64_t syntheticFrameSerial_ = 0;
	bool focused_ = true;
	bool syntheticFrame_ = false;
	bool pendingPrimaryPointerCanceled_ = false;
	bool framePrimaryPointerCanceled_ = false;
	bool lastConsumedPrimaryPointerDown_ = false;
};

} // namespace FlowUi::detail::input
