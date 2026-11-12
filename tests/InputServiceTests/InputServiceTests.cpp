#include "FlowUi/Private/inputService/InputService.hpp"
#include "TestHarness.hpp"
#include <atomic>
#include <limits>
#include <memory>
#include <thread>

namespace {
using namespace FlowUi;
using namespace FlowUi::detail::input;

void contributionAndCadence() {
	InputState state(4);
	InputService service;
	FLOWUI_CHECK(service.initialize());
	FLOWUI_CHECK(service.registerTarget(MainWindowId, state));
	FLOWUI_CHECK(state.sink().key({.device = 10, .key = Key::A, .pressed = true}));
	FLOWUI_CHECK(state.sink().key({.device = 20, .key = Key::A, .pressed = true}));
	FLOWUI_CHECK(state.sink().text({.codepoint = U'λ'}));
	service.beginTick();
	FLOWUI_CHECK(service.poll(.01));
	FLOWUI_CHECK(service.keyDown(state, Key::A));
	FLOWUI_CHECK(state.sink().key({.device = 10, .key = Key::A, .pressed = false}));
	service.beginTick();
	FLOWUI_CHECK(service.poll(.01));
	FLOWUI_CHECK(service.keyDown(state, Key::A));
	auto frame = service.frameInput(state, .02);
	FLOWUI_CHECK(frame && frame->text == std::vector<char32_t>{U'λ'});
	FLOWUI_CHECK(frame->keyDown[keyCode(Key::A)]);
	FLOWUI_CHECK(state.events().size() == 4);
	auto second = service.frameInput(state, .01);
	FLOWUI_CHECK(second && second->text.empty() && state.events().empty());
	FLOWUI_CHECK(second->keyDown[keyCode(Key::A)]);
	FLOWUI_CHECK(state.sink().connection({.device = 20, .connected = false}));
	FLOWUI_CHECK(service.poll(.01));
	FLOWUI_CHECK(!service.keyDown(state, Key::A));
	service.shutdown();
}

void orderedEdgesAndFocus() {
	InputState state;
	InputService service;
	FLOWUI_CHECK(service.initialize());
	FLOWUI_CHECK(service.registerTarget(MainWindowId, state));
	FLOWUI_CHECK(state.sink().key({.key = Key::LeftShift, .pressed = true, .modifiers = 2}));
	FLOWUI_CHECK(state.sink().key({.key = Key::B, .pressed = true, .modifiers = 2}));
	FLOWUI_CHECK(state.sink().key({.key = Key::B, .pressed = false}));
	FLOWUI_CHECK(service.poll(0));
#if FLOW_UI_DEV_MODE
	FLOWUI_CHECK(state.queue().dev_shortcut_presses[keyCode(Key::B)] == (1u << 2));
#endif
	auto frame = service.frameInput(state, .01);
	FLOWUI_CHECK(frame && frame->shift && !frame->keyDown[keyCode(Key::B)]);
	FLOWUI_CHECK(state.events().size() == 3);
	FLOWUI_CHECK(std::get<KeyEvent>(state.events()[1]).pressed);
	FLOWUI_CHECK(!std::get<KeyEvent>(state.events()[2]).pressed);
	FLOWUI_CHECK(state.sink().focus({.focused = false}));
	FLOWUI_CHECK(service.poll(0));
	FLOWUI_CHECK(!service.keyDown(state, Key::LeftShift));
	FLOWUI_CHECK(state.sink().key({.key = Key::LeftShift, .pressed = true}));
	FLOWUI_CHECK(service.poll(0));
	FLOWUI_CHECK(!service.keyDown(state, Key::LeftShift));
	service.shutdown();
}

void boundedTextAndOverflow() {
	InputState state(2, InputQueueOverflowPolicy::DropOldest, 16);
	InputService service;
	FLOWUI_CHECK(service.initialize());
	FLOWUI_CHECK(service.registerTarget(MainWindowId, state));
	for (char32_t character : {U'a', U'b', U'c'})
		FLOWUI_CHECK(state.sink().text({.codepoint = character}));
	FLOWUI_CHECK(service.poll(0));
	auto frame = service.frameInput(state, 0);
	FLOWUI_CHECK(frame && frame->text == std::vector<char32_t>{U'b', U'c'});
	FLOWUI_CHECK(frame->droppedTextInputCount == 1);
	FLOWUI_CHECK(state.sink().key({.key = Key::A, .pressed = true}));
	FLOWUI_CHECK(service.poll(0));
	FLOWUI_CHECK(service.keyDown(state, Key::A));
	for (int i = 0; i < 16; ++i)
		FLOWUI_CHECK(state.sink().text({.codepoint = U'x'}));
	FLOWUI_CHECK(!state.sink().text({.codepoint = U'y'}));
	const auto overflow = service.poll(0);
	FLOWUI_CHECK(!overflow && overflow.error().code == ErrorCode::PlatformInputQueueOverflow);
	FLOWUI_CHECK(!service.keyDown(state, Key::A));
	FLOWUI_CHECK(!state.sink().key({.key = static_cast<Key>(511), .pressed = true}));
	FLOWUI_CHECK(!state.sink().text({.codepoint = 0xd800}));
	FLOWUI_CHECK(!state.sink().pointerMove({.x = std::numeric_limits<float>::infinity()}));
	service.shutdown();
}

void routeDisableAndNavigation() {
	InputState first;
	InputState second;
	InputService service;
	FLOWUI_CHECK(service.initialize());
	FLOWUI_CHECK(service.registerTarget(1, first));
	FLOWUI_CHECK(service.registerTarget(2, second));
	FLOWUI_CHECK(first.sink().navigation({.device = 99, .action = NavigationAction::Activate}));
	FLOWUI_CHECK(
		first.sink().pointerMove({.device = 99, .kind = PointerKind::Virtual, .x = 25, .y = 30}));
	FLOWUI_CHECK(
		first.sink().pointerButton({.device = 99, .kind = PointerKind::Virtual, .pressed = true}));
	FLOWUI_CHECK(service.poll(0));
	auto frame = service.frameInput(first, .01);
	FLOWUI_CHECK(frame && frame->keyDown[keyCode(Key::Enter)] && frame->mouseDown[0]);
	FLOWUI_CHECK(frame->mouseX == 25 && frame->mouseY == 30);
	auto other = service.frameInput(second, .01);
	FLOWUI_CHECK(other && !other->mouseDown[0] && !other->keyDown[keyCode(Key::Enter)]);
	service.setTargetEnabled(1, false);
	FLOWUI_CHECK(!service.keyDown(first, Key::Enter));
	FLOWUI_CHECK(first.events().empty());
	service.unregisterTarget(1);
	service.shutdown();
}
void pointerArbitrationAndContactLifetimes() {
	InputState first;
	InputState second;
	InputService service;
	FLOWUI_CHECK(service.initialize());
	FLOWUI_CHECK(service.registerTarget(1, first));
	FLOWUI_CHECK(service.registerTarget(2, second));
	FLOWUI_CHECK(first.sink().pointerMove({.device = NativeInputDevice, .x = 0, .y = 0}));
	FLOWUI_CHECK(service.poll(0));
	FLOWUI_CHECK(service.sink().pointerMove(
		{.device = 77, .target = 1, .kind = PointerKind::Virtual, .x = 25, .y = 30}));
	FLOWUI_CHECK(service.poll(0));
	FLOWUI_CHECK(first.sink().pointerMove({.device = NativeInputDevice, .x = 0, .y = 0}));
	auto frame = service.frameInput(first, 0);
	FLOWUI_CHECK(frame && frame->mouseX == 25 && frame->mouseY == 30);
	for (InputContactId contact = 1; contact < 300; ++contact) {
		FLOWUI_CHECK(service.sink().pointerMove({.device = 80,
												 .target = 1,
												 .contact = contact,
												 .kind = PointerKind::Touch,
												 .x = 10,
												 .y = 20}));
		FLOWUI_CHECK(service.sink().pointerButton({.device = 80,
												   .target = 1,
												   .contact = contact,
												   .kind = PointerKind::Touch,
												   .pressed = true}));
		FLOWUI_CHECK(service.poll(0));
		FLOWUI_CHECK(service.sink().pointerMove({.device = 80,
												 .target = 2,
												 .contact = contact,
												 .kind = PointerKind::Touch,
												 .x = 35,
												 .y = 40}));
		FLOWUI_CHECK(service.sink().pointerButton({.device = 80,
												   .target = 2,
												   .contact = contact,
												   .kind = PointerKind::Touch,
												   .pressed = false}));
		FLOWUI_CHECK(service.poll(0));
		frame = service.frameInput(first, 0);
		FLOWUI_CHECK(frame && !frame->mouseDown[0] && frame->mouseX == 35);
		auto other = service.frameInput(second, 0);
		FLOWUI_CHECK(other && other->mouseX == 0);
	}
	service.shutdown();
}

void syntheticReplacementKeepsNativeSource() {
	InputState state;
	InputService service;
	FLOWUI_CHECK(service.initialize());
	FLOWUI_CHECK(service.registerTarget(1, state));
	FLOWUI_CHECK(state.sink().key({.key = Key::A, .pressed = true}));
	FLOWUI_CHECK(service.poll(0));
	FLOWUI_CHECK(service.frameInput(state, .1));
	FrameInput selected{};
	selected.dt = .1;
	selected.keyDown[keyCode(Key::B)] = true;
	selected.mouseX = 45;
	selected.mouseY = 50;
	selected.mouseDown[0] = true;
	selected.pointerInside = true;
	selected.windowFocused = false;
	selected.text = {U'λ'};
	const auto replacement = service.overrideFrameInput(state, selected);
	FLOWUI_CHECK(replacement && !replacement->keyDown[keyCode(Key::A)]);
	FLOWUI_CHECK(replacement->keyDown[keyCode(Key::B)] && replacement->mouseDown[0]);
	FLOWUI_CHECK(replacement->mouseX == 45 && replacement->text == selected.text);
	FLOWUI_CHECK(!replacement->windowFocused);
	FLOWUI_CHECK(!state.events().empty());
	const auto native = service.frameInput(state, .1);
	FLOWUI_CHECK(native && native->keyDown[keyCode(Key::A)] && !native->keyDown[keyCode(Key::B)]);
	service.shutdown();
}

void concurrentWorkerIngress() {
	InputState state;
	InputService service;
	FLOWUI_CHECK(service.initialize());
	FLOWUI_CHECK(service.registerTarget(1, state));
	std::atomic_bool emissionsSucceeded{true};
	auto produce = [&](InputDeviceId device) {
		for (std::size_t sample = 0; sample < 100; ++sample) {
			const auto pressed =
				state.sink().key({.device = device, .key = Key::A, .pressed = true});
			const auto text = state.sink().text({.device = device, .codepoint = U'x'});
			const auto released =
				state.sink().key({.device = device, .key = Key::A, .pressed = false});
			if (!pressed || !text || !released)
				emissionsSucceeded.store(false);
		}
	};
	std::thread first(produce, 10);
	std::thread second(produce, 20);
	first.join();
	second.join();
	FLOWUI_CHECK(emissionsSucceeded.load());
	FLOWUI_CHECK(service.poll(0));
	auto frame = service.frameInput(state, .01);
	FLOWUI_CHECK(frame && frame->text.size() == 200 && !frame->keyDown[keyCode(Key::A)]);
	FLOWUI_CHECK(state.events().size() == 600);
	std::array<bool, 2> held{};
	for (const auto& event : state.events()) {
		const auto* key = std::get_if<KeyEvent>(&event);
		if (!key)
			continue;
		const std::size_t deviceIndex = key->device == 10 ? 0 : 1;
		FLOWUI_CHECK(held[deviceIndex] != key->pressed);
		held[deviceIndex] = key->pressed;
	}
	FLOWUI_CHECK(!held[0] && !held[1]);
	service.shutdown();
}

struct TranslatorMetrics {
	std::size_t acquired = 0;
	std::size_t released = 0;
	std::size_t polls = 0;
	std::size_t resets = 0;
};

template <std::size_t Identity, bool FailAcquisition = false>
class SampleTranslator {
public:
	inline static TranslatorMetrics metrics{};
	[[nodiscard]] Status initialize(const InputTranslatorContext& context) noexcept {
		if (context.eventCapacity == 0)
			return unexpectedError(makeError(ErrorCode::PlatformInputQueueOverflow,
											 ErrorSite::InputManagerInitialize));
		++metrics.acquired;
		active_ = true;
		if constexpr (FailAcquisition) {
			shutdown();
			return unexpectedError(makeError(ErrorCode::PlatformInitializationFailed,
											 ErrorSite::InputManagerInitialize));
		}
		return {};
	}
	[[nodiscard]] Status poll(const InputTranslatorContext& context, InputSink& sink) noexcept {
		if (!active_)
			return unexpectedError(
				makeError(ErrorCode::ObjectNotInitialized, ErrorSite::InputBeginFrame));
		++metrics.polls;
		return sink.key({.device = Identity + 100,
						 .target = context.focusedTarget(),
						 .key = Key::A,
						 .pressed = true});
	}
	void resetTarget(WindowId target, InputSink& sink) noexcept {
		++metrics.resets;
		const auto status = sink.reset({.device = Identity + 100, .target = target});
		resetFailed_ = !status;
	}
	void shutdown() noexcept {
		if (active_) {
			++metrics.released;
			active_ = false;
		}
	}
	[[nodiscard]] bool resetFailed() const noexcept { return resetFailed_; }

private:
	bool active_ = false;
	bool resetFailed_ = false;
};

void translatorPackLifecycle() {
	using First = SampleTranslator<0>;
	using Second = SampleTranslator<1>;
	using Failing = SampleTranslator<2, true>;
	First::metrics = {};
	Second::metrics = {};
	Failing::metrics = {};
	const std::array<InputRoutingTarget, 1> targets{{{1, true, true}}};
	InputTranslatorContext context{.targets = targets};
	{
		InputTranslatorPack<First, Failing, Second> pack;
		const auto status = pack.initialize(context);
		FLOWUI_CHECK(!status);
		FLOWUI_CHECK(First::metrics.acquired == 1 && First::metrics.released == 1);
		FLOWUI_CHECK(Failing::metrics.acquired == 1 && Failing::metrics.released == 1);
		FLOWUI_CHECK(Second::metrics.acquired == 0);
		pack.shutdown();
		FLOWUI_CHECK(First::metrics.released == 1);
	}
	First::metrics = {};
	InputState state;
	InputService service;
	FLOWUI_CHECK(service.initialize());
	FLOWUI_CHECK(service.registerTarget(1, state));
	{
		InputTranslatorPack<First, Second> pack;
		FLOWUI_CHECK(pack.initialize(context));
		FLOWUI_CHECK(pack.poll(context, service.sink()));
		FLOWUI_CHECK(service.poll(.01));
		FLOWUI_CHECK(service.keyDown(state, Key::A));
		FLOWUI_CHECK(First::metrics.polls == 1 && Second::metrics.polls == 1);
		pack.resetTarget(1, service.sink());
		FLOWUI_CHECK(!std::get<0>(pack.translators()).resetFailed());
		FLOWUI_CHECK(service.poll(.01));
		FLOWUI_CHECK(!service.keyDown(state, Key::A));
		pack.shutdown();
		pack.shutdown();
		FLOWUI_CHECK(First::metrics.released == 1 && Second::metrics.released == 1);
	}
	service.shutdown();
}

void targetRegistrationGrowsOutsidePolling() {
	std::vector<std::unique_ptr<InputState>> states;
	states.reserve(160);
	InputService service;
	FLOWUI_CHECK(service.initialize());
	for (WindowId target = 1; target <= 160; ++target) {
		states.emplace_back(
			std::make_unique<InputState>(1, InputQueueOverflowPolicy::DropNewest, 8));
		FLOWUI_CHECK(service.registerTarget(target, *states.back()));
	}
	FLOWUI_CHECK(service.sink().key({.device = 77, .target = 160, .key = Key::A, .pressed = true}));
	FLOWUI_CHECK(service.poll(.01));
	FLOWUI_CHECK(service.keyDown(*states.back(), Key::A));
	FLOWUI_CHECK(!service.keyDown(*states.front(), Key::A));
	FLOWUI_CHECK(!service.registerTarget(160, *states.front()));
	service.unregisterTarget(160);
	FLOWUI_CHECK(!service.keyDown(*states.back(), Key::A));
	service.shutdown();
}

void cancellationIsSeparateFromOrdinaryRelease() {
	InputState state;
	InputService service;
	FLOWUI_CHECK(service.initialize());
	FLOWUI_CHECK(service.registerTarget(1, state));
	auto consume = [&]() {
		FLOWUI_CHECK(service.poll(.01));
		const auto frame = service.frameInput(state, .01);
		FLOWUI_CHECK(frame);
		return *frame;
	};
	FLOWUI_CHECK(state.sink().pointerButton({.device = 10, .pressed = true}));
	FLOWUI_CHECK(consume().mouseDown[0]);
	FLOWUI_CHECK(!service.primaryPointerCanceled(state));
	FLOWUI_CHECK(state.sink().pointerButton({.device = 20, .pressed = true}));
	FLOWUI_CHECK(state.sink().connection({.device = 10, .connected = false}));
	FLOWUI_CHECK(consume().mouseDown[0]);
	FLOWUI_CHECK(!service.primaryPointerCanceled(state));
	FLOWUI_CHECK(state.sink().pointerButton({.device = 20, .pressed = false}));
	FLOWUI_CHECK(!consume().mouseDown[0]);
	FLOWUI_CHECK(!service.primaryPointerCanceled(state));
	FLOWUI_CHECK(state.sink().pointerButton({.device = 10, .pressed = true}));
	FLOWUI_CHECK(consume().mouseDown[0]);
	FLOWUI_CHECK(state.sink().connection({.device = 10, .connected = false}));
	FLOWUI_CHECK(!consume().mouseDown[0]);
	FLOWUI_CHECK(service.primaryPointerCanceled(state));
	FLOWUI_CHECK(!consume().mouseDown[0]);
	FLOWUI_CHECK(!service.primaryPointerCanceled(state));
	FLOWUI_CHECK(state.sink().pointerButton({.device = 10, .pressed = true}));
	FLOWUI_CHECK(consume().mouseDown[0]);
	FLOWUI_CHECK(state.sink().focus({.focused = false}));
	FLOWUI_CHECK(!consume().mouseDown[0]);
	FLOWUI_CHECK(service.primaryPointerCanceled(state));
	FLOWUI_CHECK(state.sink().focus({.focused = true}));
	FLOWUI_CHECK(
		state.sink().pointerButton({.device = 10, .kind = PointerKind::Touch, .pressed = true}));
	FLOWUI_CHECK(consume().mouseDown[0]);
	FLOWUI_CHECK(state.sink().pointerButton(
		{.device = 10, .kind = PointerKind::Touch, .phase = InputPhase::Cancel}));
	FLOWUI_CHECK(!consume().mouseDown[0]);
	FLOWUI_CHECK(service.primaryPointerCanceled(state));
	FLOWUI_CHECK(state.sink().pointerButton({.device = 10, .pressed = true}));
	FLOWUI_CHECK(consume().mouseDown[0]);
	service.resetTarget(1);
	FLOWUI_CHECK(!consume().mouseDown[0]);
	FLOWUI_CHECK(service.primaryPointerCanceled(state));
	FLOWUI_CHECK(state.sink().pointerButton({.device = 10, .pressed = true}));
	FLOWUI_CHECK(consume().mouseDown[0]);
	FLOWUI_CHECK(state.sink().connection({.device = 10, .connected = false}));
	FLOWUI_CHECK(state.sink().pointerButton({.device = 20, .pressed = true}));
	FLOWUI_CHECK(consume().mouseDown[0]);
	FLOWUI_CHECK(service.primaryPointerCanceled(state));
	// Suspension cancels the displayed capture even if release was queued without a frame.
	FLOWUI_CHECK(state.sink().pointerButton({.device = 20, .pressed = false}));
	FLOWUI_CHECK(service.poll(.01));
	service.resetTarget(1);
	FLOWUI_CHECK(!consume().mouseDown[0]);
	FLOWUI_CHECK(service.primaryPointerCanceled(state));
	FLOWUI_CHECK(state.sink().pointerButton({.device = 10, .pressed = true}));
	FLOWUI_CHECK(consume().mouseDown[0]);
	FLOWUI_CHECK(state.sink().pointerButton({.device = 10, .pressed = false}));
	FLOWUI_CHECK(service.poll(.01));
	for (std::size_t queued = 0; queued < state.sink().capacity(); ++queued)
		FLOWUI_CHECK(state.sink().text({.codepoint = U'x'}));
	FLOWUI_CHECK(!state.sink().text({.codepoint = U'y'}));
	FLOWUI_CHECK(!service.poll(.01));
	FLOWUI_CHECK(service.frameInput(state, .01));
	FLOWUI_CHECK(service.primaryPointerCanceled(state));
	service.shutdown();
}

} // namespace

int main() {
	FlowUi::test::Runner runner;
	runner.run("source contributions survive independent release and repeated polls",
			   contributionAndCadence);
	runner.run("ordered edges and focus cancellation", orderedEdgesAndFocus);
	runner.run("bounded Unicode text and ingress overflow cancellation", boundedTextAndOverflow);
	runner.run("per-window reset and logical navigation projection", routeDisableAndNavigation);
	runner.run("virtual pointer survives native refresh and touch capture handles many lifetimes",
			   pointerArbitrationAndContactLifetimes);
	runner.run("agentic replacement uses an independent semantic source",
			   syntheticReplacementKeepsNativeSource);
	runner.run("concurrent workers preserve ordered per-source bounded ingress",
			   concurrentWorkerIngress);
	runner.run("additional translator acquisition rollback and resource shutdown",
			   translatorPackLifecycle);
	runner.run("stable nonmovable target registration grows beyond initial reservation",
			   targetRegistrationGrowsOutsidePolling);
	runner.run("pointer cancellation differs from release and preserves other held sources",
			   cancellationIsSeparateFromOrdinaryRelease);
	return runner.finish();
}
