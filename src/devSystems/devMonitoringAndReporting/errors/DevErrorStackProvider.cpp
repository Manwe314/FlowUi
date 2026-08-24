#include "devSystems/devMonitoringAndReporting/errors/DevErrorStackProvider.hpp"

#if FLOW_UI_DEV_MODE

#if __has_include(<unwind.h>)
#include <unwind.h>
#define FLOWUI_HAS_UNWIND_STACK_CAPTURE 1
#else
#define FLOWUI_HAS_UNWIND_STACK_CAPTURE 0
#endif

namespace FlowUi::devSystems {
namespace {

constexpr uint64_t hashText(std::string_view value) noexcept {
	uint64_t hash = 14695981039346656037ull;
	for (const char character : value) {
		hash ^= static_cast<uint64_t>(static_cast<unsigned char>(character));
		hash *= 1099511628211ull;
	}
	return hash == 0u ? 1u : hash;
}

#if FLOWUI_HAS_UNWIND_STACK_CAPTURE
struct UnwindCaptureState {
	std::span<uintptr_t> destination{};
	uint32_t skip = 0u;
	uint16_t count = 0u;
	bool full = false;
};

_Unwind_Reason_Code unwindCallback(_Unwind_Context* context, void* opaque) noexcept {
	auto& state = *static_cast<UnwindCaptureState*>(opaque);
	const uintptr_t address = static_cast<uintptr_t>(_Unwind_GetIP(context));
	if (address == 0u) return _URC_NO_REASON;
	if (state.skip != 0u) {
		--state.skip;
		return _URC_NO_REASON;
	}
	if (state.count >= state.destination.size()) {
		state.full = true;
		return _URC_END_OF_STACK;
	}
	state.destination[state.count++] = address;
	return _URC_NO_REASON;
}
#endif

} // namespace

DevErrorRawStackCapture PlatformDevErrorStackProvider::capture(
	std::span<uintptr_t> destination,
	uint32_t framesToSkip) noexcept {
	DevErrorRawStackCapture result{
		.status = DevErrorStackStatus::Unavailable,
		.moduleIdentity = hashText("flowui.current_process"),
		.buildIdentity = hashText(__DATE__ " " __TIME__),
	};
#if FLOWUI_HAS_UNWIND_STACK_CAPTURE
	if (destination.empty()) return result;
	UnwindCaptureState state{.destination = destination, .skip = framesToSkip};
	(void)_Unwind_Backtrace(&unwindCallback, &state);
	result.frameCount = state.count;
	result.status = state.count == 0u
		? DevErrorStackStatus::Unavailable
		: (state.full ? DevErrorStackStatus::Truncated : DevErrorStackStatus::Available);
#else
	(void)destination;
	(void)framesToSkip;
#endif
	return result;
}

DevErrorRawStackCapture PlatformDevErrorStackProvider::captureEmergency(
	std::span<uintptr_t> destination,
	uint32_t framesToSkip) noexcept {
	return capture(destination, framesToSkip);
}

} // namespace FlowUi::devSystems

#endif
