#include "FlowUi/Private/inputService/InputService.hpp"
#include <algorithm>
#include <limits>
#include <new>
#include <stdexcept>
#include <type_traits>

namespace FlowUi::detail::input {

InputTranslatorContext InputService::context(double deltaSeconds) noexcept {
	routingTargets_.clear();
	for (const auto& target : targets_)
		routingTargets_.emplace_back(
			InputRoutingTarget{target.id, target.state->focused(), target.enabled});
	return {.targets = routingTargets_,
			.deltaTimeSeconds = deltaSeconds,
			.tick = tick_,
			.eventCapacity = translatorSink_.capacity()};
}

Status InputService::initialize() {
	if (ready_)
		return unexpectedError(
			makeError(ErrorCode::ObjectAlreadyInitialized, ErrorSite::InputManagerInitialize));
	try {
		targets_.reserve(128);
		routingTargets_.reserve(128);
		captures_.reserve(128);
		ingress_.reserve(translatorSink_.capacity());
		const auto status = translators_.initialize(context(0));
		if (!status)
			return status;
		ready_ = true;
		return {};
	} catch (const std::bad_alloc&) {
		translators_.shutdown();
		return unexpectedError(
			makeError(ErrorCode::AllocationFailed, ErrorSite::InputManagerInitialize));
	}
}

Status InputService::registerTarget(WindowId target, InputState& state) {
	if (!ready_)
		return unexpectedError(
			makeError(ErrorCode::ObjectNotInitialized, ErrorSite::InputManagerInitialize));
	if (target == InvalidWindowId ||
		std::any_of(targets_.begin(), targets_.end(), [&](const Target& existing) {
			return existing.id == target || existing.state == &state;
		}))
		return unexpectedError(
			makeError(ErrorCode::InvalidWindowId, ErrorSite::InputQueueEvent, target));
	if (targets_.size() == targets_.capacity() || targets_.size() == routingTargets_.capacity()) {
		const auto maximumCapacity = std::min(targets_.max_size(), routingTargets_.max_size());
		if (targets_.size() == maximumCapacity)
			return unexpectedError(
				makeError(ErrorCode::ArithmeticOverflow, ErrorSite::InputQueueEvent, target));
		const auto currentCapacity = std::max(targets_.capacity(), routingTargets_.capacity());
		const auto nextCapacity =
			currentCapacity > targets_.size() ? currentCapacity
			: currentCapacity > maximumCapacity / 2
				? maximumCapacity
				: std::min(maximumCapacity, std::max<std::size_t>(128, currentCapacity * 2));
		try {
			targets_.reserve(nextCapacity);
			routingTargets_.reserve(nextCapacity);
		} catch (const std::bad_alloc&) {
			return unexpectedError(
				makeError(ErrorCode::AllocationFailed, ErrorSite::InputQueueEvent, target));
		} catch (const std::length_error&) {
			return unexpectedError(
				makeError(ErrorCode::ArithmeticOverflow, ErrorSite::InputQueueEvent, target));
		}
	}
	state.bindTarget(target);
	targets_.emplace_back(Target{&state, target, true});
	return {};
}

void InputService::removeCaptures(WindowId target, InputDeviceId device) noexcept {
	std::erase_if(captures_, [&](const Capture& capture) {
		return (target == InvalidWindowId || capture.target == target) &&
			   (device == 0 || capture.device == device);
	});
}

void InputService::resetTarget(WindowId target) noexcept {
	translators_.resetTarget(target, translatorSink_);
	for (auto& entry : targets_)
		if (entry.id == target)
			entry.state->reset();
	removeCaptures(target);
	// Translator cancellation events are drained on the next poll; unrelated routes remain queued.
}

void InputService::unregisterTarget(WindowId target) noexcept {
	resetTarget(target);
	std::erase_if(targets_, [&](const Target& entry) { return entry.id == target; });
}

void InputService::setTargetEnabled(WindowId target, bool enabled) noexcept {
	for (auto& entry : targets_) {
		if (entry.id != target)
			continue;
		if (!enabled && entry.enabled)
			resetTarget(target);
		entry.enabled = enabled;
		return;
	}
}

void InputService::beginTick() noexcept {
	++tick_;
#if FLOW_UI_DEV_MODE
	for (auto& target : targets_)
		target.state->queue().dev_shortcut_presses.fill(0);
#endif
}

Status InputService::route(InputEvent event) noexcept {
	WindowId targetId = InvalidWindowId;
	InputDeviceId device = 0;
	InputSourceId source = 0;
	InputContactId contact = 0;
	bool contactEvent = false;
	bool releaseCapture = false;
	bool acquireCapture = false;
	bool cancelDevice = false;
	std::visit(
		[&](const auto& value) {
			using Event = std::decay_t<decltype(value)>;
			targetId = value.target;
			if constexpr (!std::is_same_v<Event, FocusInputEvent>) {
				device = value.device;
				source = value.source;
			}
			if constexpr (std::is_same_v<Event, PointerMoveEvent> ||
						  std::is_same_v<Event, PointerButtonEvent>) {
				contact = value.contact;
				contactEvent = value.kind == PointerKind::Touch || value.kind == PointerKind::Pen;
			}
			if constexpr (std::is_same_v<Event, PointerButtonEvent>) {
				acquireCapture = contactEvent && value.pressed && value.phase != InputPhase::Cancel;
				releaseCapture =
					contactEvent && (!value.pressed || value.phase == InputPhase::Cancel);
			}
			if constexpr (std::is_same_v<Event, DeviceResetEvent>)
				cancelDevice = true;
			if constexpr (std::is_same_v<Event, DeviceConnectionEvent>)
				cancelDevice = !value.connected;
		},
		event);
	if (cancelDevice) {
		for (auto& target : targets_) {
			if (targetId != InvalidWindowId && targetId != target.id)
				continue;
			auto routed = event;
			std::visit([&](auto& value) { value.target = target.id; }, routed);
			const auto status = target.state->accept(std::move(routed));
			if (!status)
				return status;
		}
		removeCaptures(targetId, device);
		return {};
	}
	if (contactEvent) {
		const auto capture =
			std::find_if(captures_.begin(), captures_.end(), [&](const Capture& entry) {
				return entry.device == device && entry.source == source && entry.contact == contact;
			});
		if (capture != captures_.end())
			targetId = capture->target;
	}
	if (targetId == InvalidWindowId) {
		const auto focused =
			std::find_if(targets_.begin(), targets_.end(), [](const Target& entry) {
				return entry.enabled && entry.state->focused();
			});
		if (focused == targets_.end())
			return {};
		targetId = focused->id;
	}
	const auto target = std::find_if(targets_.begin(), targets_.end(),
									 [&](const Target& entry) { return entry.id == targetId; });
	if (target == targets_.end() || !target->enabled)
		return {};
	if (acquireCapture && !target->state->focused())
		return {};
	if (acquireCapture) {
		const auto existing =
			std::find_if(captures_.begin(), captures_.end(), [&](const Capture& entry) {
				return entry.device == device && entry.source == source && entry.contact == contact;
			});
		if (existing == captures_.end()) {
			if (captures_.size() == captures_.capacity()) {
				resetTarget(targetId);
				return unexpectedError(makeError(ErrorCode::PlatformInputQueueOverflow,
												 ErrorSite::InputQueueEvent, targetId));
			}
			captures_.emplace_back(Capture{device, source, contact, targetId});
		}
	}
	std::visit([&](auto& value) { value.target = targetId; }, event);
	const auto status = target->state->accept(std::move(event));
	if (!target->state->focused())
		removeCaptures(targetId);
	if (releaseCapture)
		std::erase_if(captures_, [&](const Capture& entry) {
			return entry.device == device && entry.source == source && entry.contact == contact;
		});
	return status;
}

Status InputService::drainNative(InputState& state) noexcept {
	bool overflow = false;
	state.sink_.take(state.ingress_, overflow);
	if (overflow) {
		state.pendingPrimaryPointerCanceled_ =
			state.pendingPrimaryPointerCanceled_ || state.lastConsumedPrimaryPointerDown_;
		state.cancel();
		state.ingress_.clear();
		state.pendingEvents_.clear();
		if (state.pendingEvents_.capacity() != 0)
			state.pendingEvents_.emplace_back(DeviceResetEvent{.target = state.target_});
		removeCaptures(state.target_);
		return unexpectedError(makeError(ErrorCode::PlatformInputQueueOverflow,
										 ErrorSite::InputQueueEvent, state.target_));
	}
	for (auto event : state.ingress_) {
		std::visit(
			[&](auto& value) {
				if (value.target == InvalidWindowId)
					value.target = state.target_;
			},
			event);
		const auto status = route(std::move(event));
		if (!status) {
			state.ingress_.clear();
			return status;
		}
	}
	state.ingress_.clear();
	return {};
}

Status InputService::poll(double deltaSeconds) {
	if (!ready_)
		return unexpectedError(
			makeError(ErrorCode::ObjectNotInitialized, ErrorSite::InputBeginFrame));
	for (auto& target : targets_) {
		if (!target.enabled) {
			target.state->reset();
			continue;
		}
		const auto status = drainNative(*target.state);
		if (!status) {
			removeCaptures(target.id);
			return status;
		}
		if (!target.state->focused())
			removeCaptures(target.id);
	}
	const auto status = translators_.poll(context(deltaSeconds), translatorSink_);
	if (!status) {
		for (auto& target : targets_) {
			translators_.resetTarget(target.id, translatorSink_);
			target.state->cancel();
		}
		translatorSink_.clear();
		captures_.clear();
		return status;
	}
	bool overflow = false;
	translatorSink_.take(ingress_, overflow);
	if (overflow) {
		for (auto& target : targets_) {
			translators_.resetTarget(target.id, translatorSink_);
			target.state->cancel();
		}
		translatorSink_.clear();
		captures_.clear();
		ingress_.clear();
		return unexpectedError(
			makeError(ErrorCode::PlatformInputQueueOverflow, ErrorSite::InputQueueEvent));
	}
	for (const auto& event : ingress_) {
		const auto routeStatus = route(event);
		if (!routeStatus) {
			ingress_.clear();
			return routeStatus;
		}
	}
	ingress_.clear();
	return {};
}

Result<FrameInput> InputService::frameInput(InputState& state, double deltaSeconds) {
	if (!ready_)
		return unexpectedError(
			makeError(ErrorCode::ObjectNotInitialized, ErrorSite::InputBeginFrame));
	const auto status = drainNative(state);
	if (!status)
		return unexpectedError(status.error());
	try {
		return state.consume(deltaSeconds);
	} catch (const std::bad_alloc&) {
		return unexpectedError(makeError(ErrorCode::AllocationFailed, ErrorSite::InputBeginFrame));
	}
}

Result<FrameInput> InputService::overrideFrameInput(InputState& state, const FrameInput& selected) {
	if (!ready_)
		return unexpectedError(
			makeError(ErrorCode::ObjectNotInitialized, ErrorSite::InputBeginFrame));
	try {
		// Agentic input is already bounded by its configured protocol limit. Reserve its complete
		// validated batch before emission rather than applying the native text-queue size to
		// replay.
		if (selected.text.size() > std::numeric_limits<std::size_t>::max() - 1024)
			return unexpectedError(
				makeError(ErrorCode::ArithmeticOverflow, ErrorSite::InputQueueEvent));
		const auto batchCapacity = std::max(state.sink_.capacity(), selected.text.size() + 1024);
		if (!state.syntheticState_ || state.syntheticState_->sink_.capacity() < batchCapacity) {
			state.syntheticState_ = std::make_unique<InputState>(
				std::max(state.queue_.textCapacity_, selected.text.size()),
				state.queue_.overflowPolicy_, batchCapacity);
			state.syntheticState_->bindTarget(state.target_);
		}
		auto& synthetic = *state.syntheticState_;
		if (synthetic.queue_.textCapacity_ < selected.text.size()) {
			synthetic.queue_.queuedTextInput_.reserve(selected.text.size());
			synthetic.queue_.textCapacity_ = selected.text.size();
		}
		if (state.syntheticFrameSerial_ + 1 != state.frameSerial_)
			synthetic.reset();
		state.syntheticFrameSerial_ = state.frameSerial_;
		constexpr InputDeviceId syntheticDevice = 0xffffffffffffffffULL;
		// Replacement input is eligible independently of OS focus. Its observed focus is projected
		// after normalization so recording/replay can retain the native focus observation.
		Status status = synthetic.sink().focus({.target = state.target_, .focused = true});
		auto emit = [&](Status next) {
			if (status && !next)
				status = next;
		};
		for (std::size_t slot = 0; slot < selected.keyDown.size(); ++slot) {
			const auto key = keyFromCode(static_cast<int>(slot));
			if (!key)
				continue;
			emit(synthetic.sink().key({.device = syntheticDevice,
									   .target = state.target_,
									   .key = *key,
									   .pressed = selected.keyDown[slot]}));
		}
		emit(synthetic.sink().pointerMove({.device = syntheticDevice,
										   .target = state.target_,
										   .kind = PointerKind::Virtual,
										   .x = selected.mouseX,
										   .y = selected.mouseY,
										   .inside = selected.pointerInside}));
		for (std::size_t button = 0; button < selected.mouseDown.size(); ++button)
			emit(synthetic.sink().pointerButton({.device = syntheticDevice,
												 .target = state.target_,
												 .kind = PointerKind::Virtual,
												 .button = static_cast<PointerButton>(button),
												 .pressed = selected.mouseDown[button]}));
		emit(synthetic.sink().scroll({.device = syntheticDevice,
									  .target = state.target_,
									  .x = selected.scrollX,
									  .y = selected.scrollY}));
		for (const char32_t codepoint : selected.text)
			emit(synthetic.sink().text(
				{.device = syntheticDevice, .target = state.target_, .codepoint = codepoint}));
		if (!status) {
			synthetic.reset();
			return unexpectedError(status.error());
		}
		status = synthetic.processPending();
		if (!status)
			return unexpectedError(status.error());
		auto projected = synthetic.consume(selected.dt);
		projected.windowFocused = selected.windowFocused;
		projected.droppedTextInputCount += selected.droppedTextInputCount;
		state.syntheticFrame_ = true;
		return projected;
	} catch (const std::bad_alloc&) {
		return unexpectedError(makeError(ErrorCode::AllocationFailed, ErrorSite::InputBeginFrame));
	}
}

void InputService::shutdown() noexcept {
	if (!ready_)
		return;
	for (const auto& target : targets_)
		translators_.resetTarget(target.id, translatorSink_);
	translators_.shutdown();
	for (auto& target : targets_)
		target.state->reset();
	targets_.clear();
	routingTargets_.clear();
	captures_.clear();
	ingress_.clear();
	translatorSink_.clear();
	ready_ = false;
}

} // namespace FlowUi::detail::input
