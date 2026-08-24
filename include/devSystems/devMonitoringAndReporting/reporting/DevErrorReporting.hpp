#pragma once

#include "FlowUi/BuildConfig.hpp"

#if FLOW_UI_DEV_MODE

#include <memory>
#include <optional>
#include <vector>

#include "devSystems/devMonitoringAndReporting/errors/DevErrorTypes.hpp"
#include "devSystems/devMonitoringAndReporting/reporting/DevErrorAdvice.hpp"
#include "devSystems/devMonitoringAndReporting/reporting/DevMemoryReporting.hpp"
#include "devSystems/devMonitoringAndReporting/reporting/DevTimingReporting.hpp"

namespace FlowUi::devSystems {

class DevErrorMonitoring;

using DevErrorCaptureId = uint64_t;

struct DevErrorTrigger {
	ErrorCode code = ErrorCode::None;
	ErrorSite site = ErrorSite::None;
	uint32_t nativeCode = 0u;
	bool matchNativeCode = false;
	uint32_t preOccurrenceTicks = 2u;
	uint32_t postOccurrenceTicks = 2u;
};

struct DevErrorReportingConfig {
	uint32_t retainedOccurrenceCapacity = 512u;
	uint64_t retainedByteBudget = 4u * 1024u * 1024u;
	uint32_t retainedBreadcrumbCapacity = 4096u;
	uint32_t postOccurrenceTicks = 2u;
	uint32_t maxStepsPerOccurrence = 64u;
	uint32_t maximumAdvicePerOccurrence = 4u;
	uint32_t retainedCaptureCapacity = 32u;
	uint32_t maximumPinnedTimingTicks = 16u;
	uint32_t maximumPinnedMemoryEvents = 128u;
	std::vector<DevErrorTrigger> triggers{};
};

enum class DevErrorCorrelationState : uint8_t {
	Available = 0u,
	Evicted,
	NotCaptured,
	Ambiguous,
};

enum class DevErrorCorrelationConfidence : uint8_t {
	Explicit = 0u,
	SharedContext,
	TemporalNearby,
	None,
};

struct DevErrorTimingCorrelation {
	DevErrorCorrelationState state = DevErrorCorrelationState::NotCaptured;
	DevErrorCorrelationConfidence confidence = DevErrorCorrelationConfidence::None;
	AppTickId appTick = 0u;
	WindowFrameKey frame{};
	TimingInvocationId containingInvocation = 0u;
	TimingZoneTypeId containingZone = 0u;
	uint64_t reportRevision = 0u;
};

struct DevErrorMemoryCorrelation {
	DevErrorCorrelationState state = DevErrorCorrelationState::NotCaptured;
	DevErrorCorrelationConfidence confidence = DevErrorCorrelationConfidence::None;
	MemoryEventSequence eventSequence = 0u;
	MemorySourceId source = 0u;
	MemoryOperation operation = MemoryOperation::LogicalAllocate;
	AppTickId checkpointTick = 0u;
	uint64_t storageMutationSequence = 0u;
};

enum class DevErrorOccurrenceState : uint8_t {
	Open = 0u,
	PostWindow,
	Closed,
};

struct DevErrorOccurrence {
	DevErrorOccurrenceId id = 0u;
	FlowUiError error{};
	AppTickId firstAppTick = 0u;
	AppTickId lastAppTick = 0u;
	uint64_t firstTimestampNs = 0u;
	uint64_t lastTimestampNs = 0u;
	ErrorEventKind productionKind = ErrorEventKind::Reported;
	ErrorResolution resolution = ErrorResolution::None;
	DevErrorRecordFlag flags = DevErrorRecordFlag::None;
	DevErrorOccurrenceState state = DevErrorOccurrenceState::Open;
	AppTickId closesAfterAppTick = 0u;
	DevErrorCaptureId captureId = 0u;
	DevErrorTimingCorrelation timing{};
	DevErrorMemoryCorrelation memory{};
	std::vector<DevErrorRecord> steps{};
	std::vector<DevErrorBreadcrumb> breadcrumbs{};
	std::vector<DevErrorSnapshot> snapshots{};
	std::vector<DevErrorAdviceResult> advice{};
};

enum class DevErrorTriggeredCaptureState : uint8_t {
	Collecting = 0u,
	Complete,
};

struct DevErrorTriggeredCapture {
	DevErrorCaptureId id = 0u;
	DevErrorOccurrenceId triggerOccurrence = 0u;
	AppTickId firstAppTick = 0u;
	AppTickId lastAppTick = 0u;
	DevErrorTriggeredCaptureState state = DevErrorTriggeredCaptureState::Collecting;
	bool timingHistoryEvicted = false;
	bool memoryHistoryEvicted = false;
	bool timingTruncated = false;
	bool memoryTruncated = false;
	std::vector<TimingAppTickReport> timingTicks{};
	std::vector<RetainedMemoryEvent> memoryEvents{};
};

enum class DevErrorGroupMode : uint8_t {
	Contract = 0u,
	Native,
	Source,
};

struct DevErrorGroupKey {
	ErrorCode code = ErrorCode::None;
	ErrorSite site = ErrorSite::None;
	uint32_t nativeCode = 0u;
	uint64_t sourceId = 0u;

	[[nodiscard]] friend bool operator==(
		const DevErrorGroupKey&,
		const DevErrorGroupKey&) noexcept = default;
};

struct DevErrorGroupSummary {
	DevErrorGroupKey key{};
	uint64_t occurrenceCount = 0u;
	AppTickId firstAppTick = 0u;
	AppTickId lastAppTick = 0u;
	DevErrorOccurrenceId representative = 0u;
	ErrorResolution lastResolution = ErrorResolution::None;
};

struct DevErrorOccurrenceQuery {
	std::optional<ErrorCode> code{};
	std::optional<ErrorSite> site{};
	std::optional<WindowId> window{};
	AppTickId firstAppTick = 0u;
	AppTickId lastAppTick = 0u;
	uint32_t maximumResults = 0u;
};

struct DevErrorReportingStatus {
	struct Overhead {
		uint64_t consumeCalls = 0u;
		uint64_t consumeTimeNs = 0u;
		uint64_t drainTimeNs = 0u;
		uint64_t correlationTimeNs = 0u;
		uint64_t adviceTimeNs = 0u;
		uint64_t groupingCalls = 0u;
		uint64_t groupingTimeNs = 0u;
		uint64_t drainedBytes = 0u;
	} overhead{};
	uint64_t consumedRecords = 0u;
	uint64_t consumedBreadcrumbs = 0u;
	uint64_t consumedSnapshots = 0u;
	uint64_t evictedOccurrences = 0u;
	uint64_t consumeFailures = 0u;
	uint64_t truncatedSteps = 0u;
	uint64_t truncatedBreadcrumbRanges = 0u;
	uint64_t triggeredCaptures = 0u;
	uint64_t evictedCaptures = 0u;
	uint64_t timingCorrelationEvictions = 0u;
	uint64_t memoryCorrelationEvictions = 0u;
	AppTickId lastConsumedAppTick = 0u;
	DevErrorQualitySnapshot quality{};
	uint32_t retainedOccurrences = 0u;
	uint64_t retainedBytes = 0u;
};

class DevErrorReporting {
public:
	explicit DevErrorReporting(
		DevErrorMonitoring& monitoring,
		DevErrorReportingConfig config = {},
		DevTimingReporting* timing = nullptr,
		DevMemoryReporting* memory = nullptr);
	~DevErrorReporting();
	DevErrorReporting(const DevErrorReporting&) = delete;
	DevErrorReporting& operator=(const DevErrorReporting&) = delete;

	void setConfig(const DevErrorReportingConfig& config);
	[[nodiscard]] DevErrorReportingConfig config() const;
	void consumeThrough(AppTickId appTick) noexcept;
	[[nodiscard]] DevErrorReportingStatus status() const noexcept;
	[[nodiscard]] std::vector<DevErrorOccurrence> occurrenceSnapshot() const;
	[[nodiscard]] std::optional<DevErrorOccurrence> occurrence(
		DevErrorOccurrenceId id) const;
	[[nodiscard]] std::vector<DevErrorOccurrence> occurrenceRange(
		const DevErrorOccurrenceQuery& query) const;
	[[nodiscard]] std::vector<DevErrorGroupSummary> groupSnapshot(
		DevErrorGroupMode mode = DevErrorGroupMode::Contract) const;
	[[nodiscard]] std::optional<DevErrorStackTrace> stack(DevErrorStackId id) const;
	[[nodiscard]] std::vector<DevErrorStackTrace> stackSnapshot() const;
	[[nodiscard]] std::vector<DevErrorTriggeredCapture> captureSnapshot() const;
	[[nodiscard]] std::optional<DevErrorTriggeredCapture> capture(
		DevErrorCaptureId id) const;
	[[nodiscard]] std::vector<DevErrorAdviceResult> advice(
		DevErrorOccurrenceId id) const;
	[[nodiscard]] std::span<const DevErrorAdviceDescriptor> adviceCatalogue() const noexcept;
	[[nodiscard]] std::optional<DevErrorFatalCapsule> fatalCapsule() const noexcept;

private:
	struct Impl;
	std::unique_ptr<Impl> impl_{};
};

} // namespace FlowUi::devSystems

#endif
