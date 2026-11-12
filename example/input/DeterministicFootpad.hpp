#pragma once

#include "FlowUi/Public/input/InputTranslation.hpp"
#include <array>
#include <cstddef>

namespace FlowUi::examples {

/** A deterministic foot switch sample, standing in for one completed SDK acquisition. */
struct FootpadSample {
	float pointerX = 25;
	float pointerY = 30;
	bool pressed = false;
	bool connected = true;
};

/** Complete device translator example. Replace samples with SDK reads or bounded worker ingress.
 * Two idle ticks establish Clay layout, then a full click activates the existing FSEL button.
 * A second held press is cancelled by disconnection. Native keyboard/mouse remain independent.
 */
class DeterministicFootpad final {
public:
	/** Reset deterministic sample acquisition. The target list can be empty during app setup. */
	[[nodiscard]] Status initialize(const InputTranslatorContext& context) noexcept {
		if (ready_)
			return unexpectedError(
				makeError(ErrorCode::ObjectAlreadyInitialized, ErrorSite::InputManagerInitialize));
		if (context.eventCapacity < 4)
			return unexpectedError(makeError(ErrorCode::PlatformInputQueueOverflow,
											 ErrorSite::InputManagerInitialize));
		sampleIndex_ = 0;
		target_ = InvalidWindowId;
		ready_ = true;
		return {};
	}
	/** Acquire one sample, establish pointer position, and emit only actual switch transitions. */
	[[nodiscard]] Status poll(const InputTranslatorContext& context, InputSink& sink) noexcept {
		if (!ready_)
			return unexpectedError(
				makeError(ErrorCode::ObjectNotInitialized, ErrorSite::InputBeginFrame));
		if (pendingReset_ != InvalidWindowId) {
			const auto status = sink.reset({.device = device_, .target = pendingReset_});
			if (!status)
				return status;
			pendingReset_ = InvalidWindowId;
		}
		if (sampleIndex_ == samples_.size())
			return {};
		if (target_ == InvalidWindowId)
			target_ = context.focusedTarget();
		if (target_ == InvalidWindowId)
			return {};
		const auto& sample = samples_[sampleIndex_];
		if (!sample.connected) {
			const auto status =
				sink.connection({.device = device_, .target = target_, .connected = false});
			if (!status)
				return status;
			pressed_ = false;
			target_ = InvalidWindowId;
			++sampleIndex_;
			return {};
		}
		auto status = sink.pointerMove({.device = device_,
										.target = target_,
										.kind = PointerKind::Virtual,
										.x = sample.pointerX,
										.y = sample.pointerY,
										.inside = true});
		if (!status)
			return status;
		if (sample.pressed != pressed_) {
			status = sink.pointerButton({.device = device_,
										 .target = target_,
										 .kind = PointerKind::Virtual,
										 .button = PointerButton::Primary,
										 .pressed = sample.pressed});
			if (!status)
				return status;
			pressed_ = sample.pressed;
		}
		++sampleIndex_;
		return {};
	}
	/** Release SDK-side capture when the owning route is suspended or removed. */
	void resetTarget(WindowId target, InputSink& sink) noexcept {
		if (target_ != target)
			return;
		const auto status = sink.reset({.device = device_, .target = target});
		if (!status)
			pendingReset_ = target;
		target_ = InvalidWindowId;
		pressed_ = false;
	}
	/** Stop acquisition and clear every borrowed target identity; safe repeatedly. */
	void shutdown() noexcept {
		ready_ = false;
		pressed_ = false;
		target_ = InvalidWindowId;
		pendingReset_ = InvalidWindowId;
		sampleIndex_ = 0;
	}

private:
	static constexpr InputDeviceId device_ = 0xf001;
	static constexpr std::array<FootpadSample, 9> samples_{{{},
															{},
															{.pressed = true},
															{.pressed = true},
															{},
															{},
															{.pressed = true},
															{.connected = false},
															{.connected = false}}};
	WindowId target_ = InvalidWindowId;
	WindowId pendingReset_ = InvalidWindowId;
	std::size_t sampleIndex_ = 0;
	bool ready_ = false;
	bool pressed_ = false;
};

using DeterministicFootpadPack = InputTranslatorPack<DeterministicFootpad>;
static_assert(InputTranslatorConcept<DeterministicFootpad>);

} // namespace FlowUi::examples
