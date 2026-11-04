#pragma once

#include "FlowUi/BuildConfig.hpp"

#if FLOW_UI_DEV_MODE

#include <memory>
#include <optional>
#include <string_view>
#include <vector>

#include "devSystems/devMonitoringAndReporting/errors/DevErrorRecorder.hpp"

namespace FlowUi::devSystems {

/** Producer scope at a consumer safe point; additional recorders need their own barrier. */
struct DevErrorProducerScope {
	uint64_t fallback_events = 0;
	uint32_t recorder_count = 0;
	bool current_thread_only = false;
};

class DevErrorMonitoring {
public:
	explicit DevErrorMonitoring(DevErrorConfig config = {});
	~DevErrorMonitoring();
	DevErrorMonitoring(const DevErrorMonitoring&) = delete;
	DevErrorMonitoring& operator=(const DevErrorMonitoring&) = delete;

	[[nodiscard]] DevErrorThreadAttachment attachCurrentThread(std::string_view name);
	void observeProductionEvent(const ErrorEventView& event) noexcept;
	void recordProductionEvent(
		DevErrorOccurrenceId occurrence,
		const ErrorEventView& event,
		const DevErrorSourceDescriptor& source = {}) noexcept;
	[[nodiscard]] DevErrorOccurrenceId recordRaised(
		FlowUiError error,
		const DevErrorSourceDescriptor& source,
		DevErrorRecordFlag flags = DevErrorRecordFlag::None,
		std::string_view nativeText = {}) noexcept;
	void recordStep(
		DevErrorOccurrenceId occurrence,
		DevErrorStepKind kind,
		FlowUiError error = {},
		ErrorResolution resolution = ErrorResolution::None,
		const DevErrorSourceDescriptor& source = {}) noexcept;
	void recordEvidence(
		DevErrorOccurrenceId occurrence,
		const DevErrorEvidenceBlock& evidence,
		const DevErrorSourceDescriptor& source = {}) noexcept;
	void recordBreadcrumb(
		const DevErrorBreadcrumbDescriptor& descriptor,
		uint64_t primaryValue = 0u,
		uint64_t secondaryValue = 0u,
		DevErrorOccurrenceId occurrence = 0u) noexcept;
	[[nodiscard]] std::vector<DevErrorSourceDescriptor> sourceSnapshot() const;
	[[nodiscard]] std::vector<DevErrorBreadcrumbDescriptor> breadcrumbDescriptorSnapshot() const;
	[[nodiscard]] std::vector<DevErrorStackTrace> stackSnapshot() const;
	[[nodiscard]] bool registerSnapshotProvider(
		const DevErrorSnapshotProvider& provider) noexcept;
	void unregisterSnapshotProvider(
		DevErrorSnapshotSourceId source,
		const void* owner) noexcept;
	[[nodiscard]] bool requestSnapshot(
		const DevErrorSnapshotRequest& request) noexcept;
	void captureDeferredSnapshots() noexcept;
	/** Drain only newly produced data into reusable consumer storage. Each destination is cleared.
	 */
	void drain_snapshots_into(std::vector<DevErrorSnapshot>& destination);
	/** Drain new records in sequence order without copying retained history. */
	void drain_records_into(std::vector<DevErrorRecord>& destination);
	/** Drain new breadcrumbs in sequence order into reusable consumer storage. */
	void drain_breadcrumbs_into(std::vector<DevErrorBreadcrumb>& destination);
	[[nodiscard]] std::vector<DevErrorSnapshot> drainSnapshots();
	[[nodiscard]] std::vector<DevErrorRecord> drainRecords();
	[[nodiscard]] std::vector<DevErrorBreadcrumb> drainBreadcrumbs();
	[[nodiscard]] DevErrorQualitySnapshot qualitySnapshot() const noexcept;
	[[nodiscard]] std::optional<DevErrorFatalCapsule> fatalCapsuleSnapshot() const noexcept;
	void publishFatalSafePoint(const DevErrorFatalSafePointSummary& summary) noexcept;
	void setConfig(const DevErrorConfig& config) noexcept;
	[[nodiscard]] DevErrorConfig config() const noexcept;
	/** Identify when the current thread is the sole registered producer, without draining it. */
	[[nodiscard]] DevErrorProducerScope producer_scope() const noexcept;

private:
	friend class DevErrorRecorder;
	friend class DevErrorThreadAttachment;
	struct Impl;
	void detach(DevErrorRecorder& recorder) noexcept;
	std::unique_ptr<Impl> impl_{};
};

void recordGlobalDevDiagnostic(
	FlowUiError error,
	const DevErrorSourceDescriptor& source,
	std::string_view nativeText = {}) noexcept;

void recordGlobalDevBreadcrumb(
	const DevErrorBreadcrumbDescriptor& descriptor,
	uint64_t primaryValue = 0u,
	uint64_t secondaryValue = 0u) noexcept;

} // namespace FlowUi::devSystems

#endif
