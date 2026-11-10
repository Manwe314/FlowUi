#pragma once

#include "FlowUi/Public/BuildConfig.hpp"

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
class DevGpuTiming;

/**
 * Thread-attached CPU timing producer. Use it only on its attached thread and keep its DevTiming
 * owner alive until all zones and attachments are destroyed.
 */
class DevTimingRecorder {
public:
	/** Release owned monitoring state after dependent recorders, attachments and scopes are finished. */
	~DevTimingRecorder();

	/** Copying is disabled to preserve ownership and producer identity. */
	DevTimingRecorder(const DevTimingRecorder&) = delete;
	DevTimingRecorder& operator=(const DevTimingRecorder&) = delete;
	/** Moving is disabled to preserve the owner and active scope addresses. */
	DevTimingRecorder(DevTimingRecorder&&) = delete;
	DevTimingRecorder& operator=(DevTimingRecorder&&) = delete;

	/**
	 * Try to admit a CPU zone under the effective capture level, filters and stack capacity. Return
	 * an inactive token when suppressed or full. Descriptor strings must outlive the timing owner.
	 */
	[[nodiscard]] ActiveZoneToken tryBegin(
		const TimingZoneDescriptor& descriptor,
		TimingEntityRef entity = {}) noexcept;
	/** End the admitted zone with the supplied completion flags; inactive tokens are ignored. */
	void end(
		ActiveZoneToken token,
		TimingRecordFlag result = TimingRecordFlag::Completed) noexcept;
	/** End a CPU zone and add its inclusive duration to the supplied element definition aggregate. */
	void endElement(
		ActiveZoneToken token,
		FlowDefinitionID definition,
		TimingRecordFlag result = TimingRecordFlag::Completed) noexcept;

	/**
	 * Set the window frame and application tick copied into subsequently opened zones on this
	 * producer.
	 */
	void setFrameContext(WindowFrameKey frame, AppTickId appTick = 0u) noexcept;
	/** Clear window-frame attribution for subsequent producer records. */
	void clearFrameContext() noexcept;

	/** Count active published scopes in the half-open [first_tick, end_tick) tick interval. */
	[[nodiscard]] uint64_t pending_scope_count(AppTickId first_tick,
											   AppTickId end_tick) const noexcept;
	/** Return this producer track identity. */
	[[nodiscard]] TimingTrackId trackId() const noexcept;
	/** Borrow this producer track name; the owner retains its storage. */
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
	void drainElementAggregatesInto(std::vector<ElementDefinitionTimingAggregate>& output);
	[[nodiscard]] TimingQualitySnapshot qualitySnapshot() const noexcept;

	std::unique_ptr<Impl> impl_{};
};

/**
 * Move-only lease attaching one CPU timing recorder to the current thread. Destroy it on that
 * thread before its DevTiming owner.
 */
class DevTimingThreadAttachment {
public:
	/** Construct an empty inactive handle. */
	DevTimingThreadAttachment() noexcept = default;
	/** Detach the current thread recorder; its monitoring owner must still be alive. */
	~DevTimingThreadAttachment();

	/** Copying is disabled to preserve ownership and producer identity. */
	DevTimingThreadAttachment(const DevTimingThreadAttachment&) = delete;
	DevTimingThreadAttachment& operator=(const DevTimingThreadAttachment&) = delete;
	/**
	 * Transfer ownership of the thread attachment from other; the moved-from attachment becomes
	 * empty.
	 */
	DevTimingThreadAttachment(DevTimingThreadAttachment&& other) noexcept;
	/** Detach this lease, then take other's recorder; other becomes empty. */
	DevTimingThreadAttachment& operator=(DevTimingThreadAttachment&& other) noexcept;

	/** Return whether this frame, token or thread attachment is valid and active. */
	[[nodiscard]] explicit operator bool() const noexcept { return recorder_ != nullptr; }
	/**
	 * Borrow the attached producer. The attachment must be nonempty and its monitoring owner must
	 * still exist.
	 */
	[[nodiscard]] DevTimingRecorder& recorder() const noexcept { return *recorder_; }

private:
	friend class DevTiming;
	explicit DevTimingThreadAttachment(DevTimingRecorder& recorder) noexcept
		: recorder_(&recorder) {}
	void reset() noexcept;

	DevTimingRecorder* recorder_ = nullptr;
};

/**
 * Owner of per-thread CPU timing producers, descriptor metadata and capture configuration. Attach
 * each producing thread before opening timing zones.
 */
class DevTiming {
public:
	/**
	 * Owner of per-thread CPU timing producers, descriptor metadata and capture configuration.
	 * Attach each producing thread before opening timing zones.
	 */
	explicit DevTiming(DevTimingConfig config = {});
	/** Release owned monitoring state after dependent recorders, attachments and scopes are finished. */
	~DevTiming();

	/** Copying is disabled to preserve ownership and producer identity. */
	DevTiming(const DevTiming&) = delete;
	DevTiming& operator=(const DevTiming&) = delete;
	/** Moving is disabled to preserve the owner and active scope addresses. */
	DevTiming(DevTiming&&) = delete;
	DevTiming& operator=(DevTiming&&) = delete;

	/**
	 * Attach a producer to the calling thread and return its lifetime lease. The name is copied; the
	 * monitoring owner must outlive the attachment.
	 */
	[[nodiscard]] DevTimingThreadAttachment attachCurrentThread(std::string_view trackName);
	/** Append newly completed records to destination and consume them from producer queues. */
	void drain_completed_records_into(std::vector<CpuTimingRecord>& destination);
	/** Consume newly completed producer records into a new owned vector. */
	[[nodiscard]] std::vector<CpuTimingRecord> drainCompletedRecords();
	/** Drain and merge element aggregates into reusable caller storage. */
	void drain_element_aggregates_into(std::vector<ElementDefinitionTimingAggregate>& destination);
	/** Consume newly completed per-definition aggregates into a new owned vector. */
	[[nodiscard]] std::vector<ElementDefinitionTimingAggregate> drainElementTimingAggregates();
	/** Registration revision used to avoid repeated metadata copies. */
	[[nodiscard]] uint64_t metadata_revision() const noexcept;
	/**
	 * Copy registered timing descriptor metadata. Descriptor string views remain borrowed from their
	 * original static storage.
	 */
	[[nodiscard]] std::vector<TimingZoneDescriptor> descriptorSnapshot() const;
	/** Copy registered CPU producer track metadata into owned strings. */
	[[nodiscard]] std::vector<TimingTrackDescriptor> trackSnapshot() const;
	/** Return cumulative capture capability and quality counters without consuming records. */
	[[nodiscard]] TimingQualitySnapshot qualitySnapshot() const;

	/**
	 * Apply monitoring or reporting configuration; retained storage and pending work follow the
	 * owning service policy.
	 */
	void setConfig(const DevTimingConfig& config) noexcept;
	/** Return a copy of the current effective configuration. */
	[[nodiscard]] DevTimingConfig config() const noexcept;
	/** Borrow CPU clock-read overhead calibration for the lifetime of the timing owner. */
	[[nodiscard]] const TimingClockCalibration& clockCalibration() const noexcept;

	/** Count worker scopes in the half-open [first_tick, end_tick) tick interval. */
	[[nodiscard]] uint64_t pending_scope_count(AppTickId first_tick,
											   AppTickId end_tick) const noexcept;
	/** Read the monotonic capture clock used by CPU zones and tick boundaries. */
	[[nodiscard]] uint64_t nowNs() const noexcept;

private:
	friend class DevTimingRecorder;
	friend class DevGpuTiming;
	struct Impl;

	[[nodiscard]] uint64_t configGeneration() const noexcept;
	[[nodiscard]] DevTimingConfig recorderConfig(uint64_t& generation) const noexcept;
	void registerDescriptor(const TimingZoneDescriptor& descriptor) noexcept;

	std::unique_ptr<Impl> impl_{};
};

/** Borrow the recorder itself or the recorder held by a nonempty thread attachment. */
[[nodiscard]] inline DevTimingRecorder& timingRecorder(DevTimingRecorder& recorder) noexcept {
	return recorder;
}

/** Borrow the recorder itself or the recorder held by a nonempty thread attachment. */
[[nodiscard]] inline DevTimingRecorder& timingRecorder(
	DevTimingThreadAttachment& attachment) noexcept {
	return attachment.recorder();
}

} // namespace FlowUi::devSystems

#endif
