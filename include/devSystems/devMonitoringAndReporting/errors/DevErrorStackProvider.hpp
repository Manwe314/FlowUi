#pragma once

#include "FlowUi/BuildConfig.hpp"

#if FLOW_UI_DEV_MODE

#include <span>

#include "devSystems/devMonitoringAndReporting/errors/DevErrorTypes.hpp"

namespace FlowUi::devSystems {

struct DevErrorRawStackCapture {
	DevErrorStackStatus status = DevErrorStackStatus::Unavailable;
	uint16_t frameCount = 0u;
	uint64_t moduleIdentity = 0u;
	uint64_t buildIdentity = 0u;
};

class DevErrorStackProvider {
public:
	virtual ~DevErrorStackProvider() = default;
	[[nodiscard]] virtual DevErrorRawStackCapture capture(
		std::span<uintptr_t> destination,
		uint32_t framesToSkip) noexcept = 0;
	/** Opt-in path which must not allocate, block, or acquire application locks. */
	[[nodiscard]] virtual DevErrorRawStackCapture captureEmergency(
		std::span<uintptr_t>, uint32_t) noexcept { return {}; }
};

/** Process-local raw-PC provider. Unsupported platforms report Unavailable. */
class PlatformDevErrorStackProvider final : public DevErrorStackProvider {
public:
	[[nodiscard]] DevErrorRawStackCapture capture(
		std::span<uintptr_t> destination,
		uint32_t framesToSkip) noexcept override;
	[[nodiscard]] DevErrorRawStackCapture captureEmergency(
		std::span<uintptr_t> destination,
		uint32_t framesToSkip) noexcept override;
};

} // namespace FlowUi::devSystems

#endif
