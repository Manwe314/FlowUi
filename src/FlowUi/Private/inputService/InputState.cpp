#include "FlowUi/Private/inputService/InputState.hpp"
#include <algorithm>
#include <type_traits>

namespace FlowUi::detail::input {
namespace {
[[nodiscard]] Status overflowStatus(WindowId target) noexcept {
	return unexpectedError(
		makeError(ErrorCode::PlatformInputQueueOverflow, ErrorSite::InputQueueEvent, target));
}
[[nodiscard]] Key navigationKey(NavigationAction action) noexcept {
	switch (action) {
	case NavigationAction::Left:
		return Key::Left;
	case NavigationAction::Right:
		return Key::Right;
	case NavigationAction::Up:
		return Key::Up;
	case NavigationAction::Down:
		return Key::Down;
	case NavigationAction::Next:
	case NavigationAction::Previous:
		return Key::Tab;
	case NavigationAction::Activate:
		return Key::Enter;
	case NavigationAction::Cancel:
		return Key::Escape;
	}
	return Key::Escape;
}
} // namespace

InputState::InputState(std::size_t textCapacity, InputQueueOverflowPolicy overflowPolicy,
					   std::size_t eventCapacity)
	: queue_(textCapacity, overflowPolicy), sink_(eventCapacity) {
	ingress_.reserve(eventCapacity);
	pendingEvents_.reserve(eventCapacity);
	frameEvents_.reserve(eventCapacity);
	contributions_.reserve(128);
}

bool InputState::keyDown(Key key) const noexcept {
	const auto slot = keySlot(key);
	return slot && queue_.queuedKeysDown_[*slot];
}

InputState::Contribution* InputState::contribution(InputDeviceId device, InputSourceId source,
												   InputContactId contact,
												   PointerKind kind) noexcept {
	const auto found = std::find_if(
		contributions_.begin(), contributions_.end(), [&](const Contribution& candidate) {
			return candidate.device == device && candidate.source == source &&
				   candidate.contact == contact && candidate.kind == kind;
		});
	if (found != contributions_.end())
		return &*found;
	if (contributions_.size() == contributions_.capacity())
		return nullptr;
	Contribution added{};
	added.device = device;
	added.source = source;
	added.contact = contact;
	added.kind = kind;
	if (kind == PointerKind::Touch || kind == PointerKind::Pen)
		added.primary = std::none_of(contributions_.begin(), contributions_.end(),
									 [](const Contribution& candidate) {
										 return (candidate.kind == PointerKind::Touch ||
												 candidate.kind == PointerKind::Pen) &&
												candidate.primary && candidate.buttons[0];
									 });
	contributions_.emplace_back(added);
	return &contributions_.back();
}

void InputState::rebuildProjection() noexcept {
	queue_.clearKeyboardState();
	queue_.clearMouseButtonsState();
	for (const auto& source : contributions_) {
		for (std::size_t keyIndex = 0; keyIndex < source.keys.size(); ++keyIndex)
			if (source.keys[keyIndex])
				queue_.pushKey(static_cast<int>(keyIndex), true);
		if (source.primary)
			for (std::size_t buttonIndex = 0; buttonIndex < source.buttons.size(); ++buttonIndex)
				if (source.buttons[buttonIndex])
					queue_.pushMouseButton(static_cast<int>(buttonIndex), true);
	}
}

void InputState::cancel(InputDeviceId device, InputSourceId source) noexcept {
	const bool canceledPrimaryHeld = std::any_of(
		contributions_.begin(), contributions_.end(), [&](const Contribution& contribution) {
			return contribution.primary && contribution.buttons[0] &&
				   (device == 0 || (contribution.device == device &&
									(source == 0 || contribution.source == source)));
		});
	std::erase_if(contributions_, [&](const Contribution& contribution) {
		return device == 0 ||
			   (contribution.device == device && (source == 0 || contribution.source == source));
	});
	rebuildProjection();
	if (canceledPrimaryHeld && std::none_of(contributions_.begin(), contributions_.end(),
											[](const Contribution& contribution) {
												return contribution.primary &&
													   contribution.buttons[0];
											}))
		pendingPrimaryPointerCanceled_ = true;
}

void InputState::reset() noexcept {
	pendingPrimaryPointerCanceled_ =
		pendingPrimaryPointerCanceled_ || lastConsumedPrimaryPointerDown_;
	lastConsumedPrimaryPointerDown_ = false;
	cancel();
	sink_.clear();
	if (syntheticState_)
		syntheticState_->reset();
	contributions_.clear();
	ingress_.clear();
	pendingEvents_.clear();
	frameEvents_.clear();
	frameSerial_ = 0;
	syntheticFrameSerial_ = 0;
	syntheticFrame_ = false;
	framePrimaryPointerCanceled_ = false;
	queue_.clearKeyboardState();
	queue_.clearMouseButtonsState();
	queue_.queuedTextInput_.clear();
	queue_.queuedScrollX_ = 0;
	queue_.queuedScrollY_ = 0;
#if FLOW_UI_DEV_MODE
	queue_.dev_shortcut_presses.fill(0);
#endif
}

Status InputState::accept(InputEvent event) noexcept {
	bool changed = true;
	Status status{};
	std::visit(
		[&](auto& value) {
			using Event = std::decay_t<decltype(value)>;
			if (value.target != InvalidWindowId && value.target != target_) {
				status = unexpectedError(makeError(ErrorCode::InvalidWindowId,
												   ErrorSite::InputQueueEvent, value.target));
				return;
			}
			value.target = target_;
			if constexpr (std::is_same_v<Event, FocusInputEvent>) {
				changed = focused_ != value.focused;
				focused_ = value.focused;
				queue_.setWindowFocused(value.focused);
				if (!value.focused)
					cancel();
			} else if constexpr (std::is_same_v<Event, DeviceResetEvent>) {
				cancel(value.device, value.source);
			} else if constexpr (std::is_same_v<Event, DeviceConnectionEvent>) {
				if (!value.connected)
					cancel(value.device, value.source);
			} else if constexpr (std::is_same_v<Event, TextInputEvent>) {
				if (!focused_) {
					changed = false;
					return;
				}
				queue_.pushChar(value.codepoint);
			} else if constexpr (std::is_same_v<Event, ScrollInputEvent>) {
				if (!focused_) {
					changed = false;
					return;
				}
				queue_.pushScroll(value.x, value.y);
				changed = value.x != 0 || value.y != 0;
			} else if constexpr (std::is_same_v<Event, KeyEvent>) {
				if (!focused_ && value.pressed) {
					changed = false;
					return;
				}
				auto* source = contribution(value.device, value.source);
				if (!source) {
					status = overflowStatus(target_);
					return;
				}
				const auto slot = keySlot(value.key);
				if (!slot) {
					status = unexpectedError(
						makeError(ErrorCode::InputRangeInvalid, ErrorSite::InputQueueEvent));
					return;
				}
				changed = source->keys[*slot] != value.pressed;
				source->keys[*slot] = value.pressed;
#if FLOW_UI_DEV_MODE
				if (changed && value.pressed)
					queue_.note_dev_shortcut_press(keyCode(value.key), value.modifiers);
#endif
				if (changed)
					rebuildProjection();
			} else if constexpr (std::is_same_v<Event, NavigationInputEvent>) {
				if (!focused_ && value.phase != InputPhase::End &&
					value.phase != InputPhase::Cancel) {
					changed = false;
					return;
				}
				auto* source = contribution(value.device, value.source);
				if (!source) {
					status = overflowStatus(target_);
					return;
				}
				const bool down =
					value.phase == InputPhase::Begin || value.phase == InputPhase::Update;
				const auto slot = static_cast<std::size_t>(navigationKey(value.action));
				changed = source->keys[slot] != down;
				source->keys[slot] = down;
				if (value.action == NavigationAction::Previous)
					source->keys[static_cast<std::size_t>(Key::LeftShift)] = down;
				if (changed)
					rebuildProjection();
			} else if constexpr (std::is_same_v<Event, PointerMoveEvent>) {
				auto* source = contribution(value.device, value.source, value.contact, value.kind);
				if (!source) {
					status = overflowStatus(target_);
					return;
				}
				changed = !source->pointerValid || source->pointerX != value.x ||
						  source->pointerY != value.y || source->pointerInside != value.inside;
				source->pointerX = value.x;
				source->pointerY = value.y;
				source->pointerInside = value.inside;
				source->pointerValid = true;
				const bool anotherContactCaptured =
					std::any_of(contributions_.begin(), contributions_.end(),
								[&](const Contribution& candidate) {
									return &candidate != source && candidate.primary &&
										   candidate.buttons[0] &&
										   (candidate.kind == PointerKind::Touch ||
											candidate.kind == PointerKind::Pen);
								});
				if (source->primary && changed && !anotherContactCaptured) {
					queue_.setMousePos(value.x, value.y);
					queue_.setPointerInside(value.inside);
				}
			} else if constexpr (std::is_same_v<Event, PointerButtonEvent>) {
				if (!focused_ && value.pressed) {
					changed = false;
					return;
				}
				auto* source = contribution(value.device, value.source, value.contact, value.kind);
				if (!source) {
					status = overflowStatus(target_);
					return;
				}
				const auto button = static_cast<std::size_t>(value.button);
				const bool pressed = value.pressed && value.phase != InputPhase::Cancel;
				const bool canceledPrimaryHeld = value.phase == InputPhase::Cancel && button == 0 &&
												 source->primary && source->buttons[0];
				if (pressed && !source->buttons[button] &&
					(value.kind == PointerKind::Touch || value.kind == PointerKind::Pen)) {
					source->primary =
						std::none_of(contributions_.begin(), contributions_.end(),
									 [&](const Contribution& candidate) {
										 return &candidate != source && candidate.primary &&
												candidate.buttons[0] &&
												(candidate.kind == PointerKind::Touch ||
												 candidate.kind == PointerKind::Pen);
									 });
				}
				changed = source->buttons[button] != pressed;
				source->buttons[button] = pressed;
				if (!pressed &&
					(value.kind == PointerKind::Touch || value.kind == PointerKind::Pen))
					std::erase_if(contributions_, [&](const Contribution& candidate) {
						return candidate.device == value.device &&
							   candidate.source == value.source &&
							   candidate.contact == value.contact && candidate.kind == value.kind;
					});
				if (changed)
					rebuildProjection();
				if (canceledPrimaryHeld &&
					std::none_of(contributions_.begin(), contributions_.end(),
								 [](const Contribution& contribution) {
									 return contribution.primary && contribution.buttons[0];
								 }))
					pendingPrimaryPointerCanceled_ = true;
			}
		},
		event);
	if (!status || (changed && pendingEvents_.size() == pendingEvents_.capacity())) {
		pendingPrimaryPointerCanceled_ =
			pendingPrimaryPointerCanceled_ || lastConsumedPrimaryPointerDown_;
		cancel();
		pendingEvents_.clear();
		if (pendingEvents_.capacity() != 0)
			pendingEvents_.emplace_back(DeviceResetEvent{.target = target_});
		return status ? overflowStatus(target_) : status;
	}
	if (changed)
		pendingEvents_.emplace_back(std::move(event));
	return {};
}

Status InputState::processPending() noexcept {
	bool overflow = false;
	sink_.take(ingress_, overflow);
	if (overflow) {
		pendingPrimaryPointerCanceled_ =
			pendingPrimaryPointerCanceled_ || lastConsumedPrimaryPointerDown_;
		cancel();
		ingress_.clear();
		pendingEvents_.clear();
		if (pendingEvents_.capacity() != 0)
			pendingEvents_.emplace_back(DeviceResetEvent{.target = target_});
		return overflowStatus(target_);
	}
	for (const auto& event : ingress_) {
		const auto status = accept(event);
		if (!status) {
			ingress_.clear();
			return status;
		}
	}
	ingress_.clear();
	return {};
}

FrameInput InputState::consume(double deltaSeconds) {
	auto frame = queue_.drain(deltaSeconds);
	lastConsumedPrimaryPointerDown_ = frame.mouseDown[0];
	++frameSerial_;
	syntheticFrame_ = false;
	framePrimaryPointerCanceled_ = pendingPrimaryPointerCanceled_;
	pendingPrimaryPointerCanceled_ = false;
	frameEvents_.clear();
	frameEvents_.swap(pendingEvents_);
	return frame;
}

} // namespace FlowUi::detail::input
