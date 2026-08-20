#pragma once

#include "FlowUi/BuildConfig.hpp"

#if FLOW_UI_DEV_MODE

#include <cstdint>
#include <memory>
#include <string_view>
#include <vector>

#include "devSystems/devMonitoringAndReporting/timing/DevTimingTypes.hpp"

namespace FlowUi::devSystems {

class CpuTimingZone;
class ManualTimingZone;
class DevTiming;

class DevTimingRecorder {
public:
	~DevTimingRecorder();

	DevTimingRecorder(const DevTimingRecorder&) = delete;
	DevTimingRecorder& operator=(const DevTimingRecorder&) = delete;
	DevTimingRecorder(DevTimingRecorder&&) = delete;
	DevTimingRecorder& operator=(DevTimingRecorder&&) = delete;

	[[nodiscard]] ActiveZoneToken tryBegin(
		const TimingZoneDescriptor& descriptor,
		TimingEntityRef entity = {}) noexcept;
	void end(
		ActiveZoneToken token,
		TimingRecordFlag result = TimingRecordFlag::Completed) noexcept;

	void setFrameContext(WindowFrameKey frame, AppTickId appTick = 0u) noexcept;
	void clearFrameContext() noexcept;

	[[nodiscard]] TimingTrackId trackId() const noexcept;
	[[nodiscard]] std::string_view trackName() const noexcept;

private:
	friend class DevTiming;
	friend class DevTimingThreadAttachment;

	struct Impl;
	explicit DevTimingRecorder(
		DevTiming& owner,
		TimingTrackId track,
		std::string_view trackName,
		uint32_t recordCapacity);
	void detachCurrentThread() noexcept;
	void drainInto(std::vector<CpuTimingRecord>& output);
	[[nodiscard]] TimingQualitySnapshot qualitySnapshot() const noexcept;

	std::unique_ptr<Impl> impl_{};
};

class DevTimingThreadAttachment {
public:
	DevTimingThreadAttachment() noexcept = default;
	~DevTimingThreadAttachment();

	DevTimingThreadAttachment(const DevTimingThreadAttachment&) = delete;
	DevTimingThreadAttachment& operator=(const DevTimingThreadAttachment&) = delete;
	DevTimingThreadAttachment(DevTimingThreadAttachment&& other) noexcept;
	DevTimingThreadAttachment& operator=(DevTimingThreadAttachment&& other) noexcept;

	[[nodiscard]] explicit operator bool() const noexcept { return recorder_ != nullptr; }
	[[nodiscard]] DevTimingRecorder& recorder() const noexcept { return *recorder_; }

private:
	friend class DevTiming;
	explicit DevTimingThreadAttachment(DevTimingRecorder& recorder) noexcept
		: recorder_(&recorder) {}
	void reset() noexcept;

	DevTimingRecorder* recorder_ = nullptr;
};

class DevTiming {
public:
	explicit DevTiming(DevTimingConfig config = {});
	~DevTiming();

	DevTiming(const DevTiming&) = delete;
	DevTiming& operator=(const DevTiming&) = delete;
	DevTiming(DevTiming&&) = delete;
	DevTiming& operator=(DevTiming&&) = delete;

	[[nodiscard]] DevTimingThreadAttachment attachCurrentThread(std::string_view trackName);
	[[nodiscard]] std::vector<CpuTimingRecord> drainCompletedRecords();
	[[nodiscard]] std::vector<TimingZoneDescriptor> descriptorSnapshot() const;
	[[nodiscard]] TimingQualitySnapshot qualitySnapshot() const;

	void setConfig(const DevTimingConfig& config) noexcept;
	[[nodiscard]] DevTimingConfig config() const noexcept;
	[[nodiscard]] const TimingClockCalibration& clockCalibration() const noexcept;

private:
	friend class DevTimingRecorder;
	struct Impl;

	[[nodiscard]] uint64_t nowNs() const noexcept;
	[[nodiscard]] uint64_t configGeneration() const noexcept;
	[[nodiscard]] DevTimingConfig recorderConfig(uint64_t& generation) const noexcept;
	void registerDescriptor(const TimingZoneDescriptor& descriptor) noexcept;

	std::unique_ptr<Impl> impl_{};
};

[[nodiscard]] inline DevTimingRecorder& timingRecorder(DevTimingRecorder& recorder) noexcept {
	return recorder;
}

[[nodiscard]] inline DevTimingRecorder& timingRecorder(
	DevTimingThreadAttachment& attachment) noexcept {
	return attachment.recorder();
}

} // namespace FlowUi::devSystems

#endif
