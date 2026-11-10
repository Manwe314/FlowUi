#pragma once

#include "FlowUi/Public/BuildConfig.hpp"

#if FLOW_UI_DEV_MODE

#include <span>

#include "devSystems/devMonitoringAndReporting/errors/DevErrorTypes.hpp"

namespace FlowUi::devSystems {

/** Capture result for raw program counters written into caller-owned storage. */
struct DevErrorRawStackCapture {
	/** Availability or quality state of this capture result. */
	DevErrorStackStatus status = DevErrorStackStatus::Unavailable;
	/** Number of valid raw program counters in frames or the caller destination. */
	uint16_t frameCount = 0u;
	/** Module identity needed to interpret captured program counters. */
	uint64_t moduleIdentity = 0u;
	/** Build identity needed to interpret captured program counters. */
	uint64_t buildIdentity = 0u;
};

/**
 * Raw stack capture interface. Implementations report unavailable capture through the result and
 * must not throw across this boundary.
 */
class DevErrorStackProvider {
public:
	/** Destroy the provider through the capture interface. */
	virtual ~DevErrorStackProvider() = default;
	/**
	 * Capture raw program counters into destination and report the valid prefix and availability.
	 * Skip the requested number of leading frames; no exceptions cross the provider boundary.
	 */
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
	/**
	 * Capture raw program counters into destination and report the valid prefix and availability.
	 * Skip the requested number of leading frames; no exceptions cross the provider boundary.
	 */
	[[nodiscard]] DevErrorRawStackCapture capture(
		std::span<uintptr_t> destination,
		uint32_t framesToSkip) noexcept override;
	/**
	 * Attempt allocation-free raw-PC capture without blocking or acquiring application locks;
	 * unsupported providers return unavailable.
	 */
	[[nodiscard]] DevErrorRawStackCapture captureEmergency(
		std::span<uintptr_t> destination,
		uint32_t framesToSkip) noexcept override;
};

} // namespace FlowUi::devSystems

#endif
