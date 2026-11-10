#pragma once

/** @file
 * @brief Public DevMonitoringAndReporting declarations for optional development services.
 */

#include "FlowUi/Public/BuildConfig.hpp"

#if FLOW_UI_DEV_MODE

#include <memory>

#include "devSystems/DevConfig.hpp"

namespace FlowUi::devSystems {

class DevPerformanceCapture;
class DevTiming;
class DevGpuTiming;
class DevTimingReporting;
class DevMemory;
class DevMemoryReporting;
class DevErrorMonitoring;
class DevErrorReporting;

/** Development-only owner for monitoring collection and derived reporting. */
class DevMonitoringAndReporting {
public:
	/**
	 * @brief Construct owned monitoring and reporting services using the supplied capacities and
	 * policies.
	 */
	explicit DevMonitoringAndReporting(const DevMonitoringConfig& config = {});
	/**
	 * @brief Destroy the reporting services and their producer owners; all borrowed service
	 * references expire.
	 */
	~DevMonitoringAndReporting();

	DevMonitoringAndReporting(const DevMonitoringAndReporting&) = delete;
	DevMonitoringAndReporting& operator=(const DevMonitoringAndReporting&) = delete;
	DevMonitoringAndReporting(DevMonitoringAndReporting&&) = delete;
	DevMonitoringAndReporting& operator=(DevMonitoringAndReporting&&) = delete;

	/** App-lifetime Performance capture controller. */
	[[nodiscard]] DevPerformanceCapture& performance_capture() noexcept;
	/**
	 * @brief Borrow CPU timing collection; the reference remains valid while this owner lives.
	 */
	[[nodiscard]] DevTiming& timing() noexcept;
	/**
	 * @brief Borrow CPU timing collection; the reference remains valid while this owner lives.
	 */
	[[nodiscard]] const DevTiming& timing() const noexcept;
	/**
	 * @brief Borrow GPU timing collection; the reference remains valid while this owner lives.
	 */
	[[nodiscard]] DevGpuTiming& gpuTiming() noexcept;
	/**
	 * @brief Borrow GPU timing collection; the reference remains valid while this owner lives.
	 */
	[[nodiscard]] const DevGpuTiming& gpuTiming() const noexcept;
	/**
	 * @brief Borrow derived timing history; the reference remains valid while this owner lives.
	 */
	[[nodiscard]] DevTimingReporting& timingReporting() noexcept;
	/**
	 * @brief Borrow derived timing history; the reference remains valid while this owner lives.
	 */
	[[nodiscard]] const DevTimingReporting& timingReporting() const noexcept;
	/**
	 * @brief Borrow memory collection; the reference remains valid while this owner lives.
	 */
	[[nodiscard]] DevMemory& memory() noexcept;
	/**
	 * @brief Borrow memory collection; the reference remains valid while this owner lives.
	 */
	[[nodiscard]] const DevMemory& memory() const noexcept;
	/**
	 * @brief Borrow derived memory history; the reference remains valid while this owner lives.
	 */
	[[nodiscard]] DevMemoryReporting& memoryReporting() noexcept;
	/**
	 * @brief Borrow derived memory history; the reference remains valid while this owner lives.
	 */
	[[nodiscard]] const DevMemoryReporting& memoryReporting() const noexcept;
	/**
	 * @brief Borrow error evidence collection; the reference remains valid while this owner
	 * lives.
	 */
	[[nodiscard]] DevErrorMonitoring& errors() noexcept;
	/**
	 * @brief Borrow error evidence collection; the reference remains valid while this owner
	 * lives.
	 */
	[[nodiscard]] const DevErrorMonitoring& errors() const noexcept;
	/**
	 * @brief Borrow retained error investigations; the reference remains valid while this owner
	 * lives.
	 */
	[[nodiscard]] DevErrorReporting& errorReporting() noexcept;
	/**
	 * @brief Borrow retained error investigations; the reference remains valid while this owner
	 * lives.
	 */
	[[nodiscard]] const DevErrorReporting& errorReporting() const noexcept;

private:
	std::unique_ptr<DevPerformanceCapture> performance_capture_{};
	std::unique_ptr<DevTiming> timing_{};
	std::unique_ptr<DevGpuTiming> gpuTiming_{};
	std::unique_ptr<DevTimingReporting> timingReporting_{};
	std::unique_ptr<DevMemory> memory_{};
	std::unique_ptr<DevMemoryReporting> memoryReporting_{};
	std::unique_ptr<DevErrorMonitoring> errors_{};
	std::unique_ptr<DevErrorReporting> errorReporting_{};
};

} // namespace FlowUi::devSystems

#endif
