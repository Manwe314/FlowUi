#pragma once

#include "FlowUi/Private/inputService/InputState.hpp"
#include "FlowUi/Private/inputService/TranslatorSelection.hpp"
#include <span>
#include <vector>

namespace FlowUi::detail::input {

/** FlowUi semantic engine. Mutated only on the platform thread; workers emit through bounded sinks.
 */
class InputService final {
public:
	/** Reserve app-wide routing/capture storage and initialize the selected extra translators. */
	[[nodiscard]] Status initialize();
	/** Register stable state before attaching a native provider binding. */
	[[nodiscard]] Status registerTarget(WindowId target, InputState& state);
	/** Cancel translator/device state and remove a destination. */
	void unregisterTarget(WindowId target) noexcept;
	/** Clear poll-scoped dev press edges while preserving unconsumed text/events. */
	void beginTick() noexcept;
	/** Process native ingress, poll extra devices once, then route their ordered output. */
	[[nodiscard]] Status poll(double deltaSeconds);
	/** Refresh/consume one destination's batch and preserve held state. */
	[[nodiscard]] Result<FrameInput> frameInput(InputState& state, double deltaSeconds);
	/** Normalize an agentic replacement frame through an independent identified source.
	 * Native contributions remain intact and resume when replacement stops.
	 */
	[[nodiscard]] Result<FrameInput> overrideFrameInput(InputState& state,
														const FrameInput& selected);
	/** App-wide normalized ingress used by translators; consumed at the next poll. */
	[[nodiscard]] InputSink& sink() noexcept { return translatorSink_; }
	/** Cancel held/captured device contributions for a route. */
	void resetTarget(WindowId target) noexcept;
	/** Suspend/resume routing; disabling also cancels the route. */
	void setTargetEnabled(WindowId target, bool enabled) noexcept;
	/** Post-poll held query; no frame drain required. */
	[[nodiscard]] bool keyDown(const InputState& state, Key key) const noexcept {
		return state.keyDown(key);
	}
	/** Observe cancellation of the consumed primary gesture without changing FrameInput shape. */
	[[nodiscard]] bool primaryPointerCanceled(const InputState& state) const noexcept {
		return state.primaryPointerCanceled();
	}
	/** Ordered frame events remain valid until that state's next frame/reset. */
	[[nodiscard]] std::span<const InputEvent> events(const InputState& state) const noexcept {
		return state.events();
	}
	/** Stop translator producers before route/sink destruction; safe repeatedly. */
	void shutdown() noexcept;
	~InputService() { shutdown(); }
	InputService() = default;
	InputService(const InputService&) = delete;
	InputService& operator=(const InputService&) = delete;

private:
	struct Target {
		InputState* state = nullptr;
		WindowId id = InvalidWindowId;
		bool enabled = true;
	};
	struct Capture {
		InputDeviceId device = 0;
		InputSourceId source = 0;
		InputContactId contact = 0;
		WindowId target = InvalidWindowId;
	};
	[[nodiscard]] InputTranslatorContext context(double deltaSeconds) noexcept;
	[[nodiscard]] Status route(InputEvent event) noexcept;
	[[nodiscard]] Status drainNative(InputState& state) noexcept;
	void removeCaptures(WindowId target, InputDeviceId device = 0) noexcept;
	AdditionalInputTranslators translators_;
	InputSink translatorSink_;
	std::vector<InputEvent> ingress_;
	std::vector<Target> targets_;
	std::vector<InputRoutingTarget> routingTargets_;
	std::vector<Capture> captures_;
	std::uint64_t tick_ = 0;
	bool ready_ = false;
};

} // namespace FlowUi::detail::input
