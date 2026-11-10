#pragma once

#include "FlowUi/Public/BuildConfig.hpp"

#if FLOW_UI_DEV_MODE

#include <memory>
#include <optional>
#include <string_view>
#include <vector>

#include "devSystems/devMonitoringAndReporting/errors/DevErrorRecorder.hpp"

namespace FlowUi::devSystems {

/** Producer scope at a consumer safe point; additional recorders need their own barrier. */
struct DevErrorProducerScope {
	/** Number of events recorded by the shared fallback producer. */
	uint64_t fallback_events = 0;
	/** Number of registered thread-specific producer recorders. */
	uint32_t recorder_count = 0;
	/** Whether the current thread is the sole registered thread-specific producer. */
	bool current_thread_only = false;
};

/**
 * Owner of bounded diagnostic producers, causal records and deferred snapshots. Thread
 * attachments and borrowed snapshot providers must be released before destruction.
 */
class DevErrorMonitoring {
public:
	/**
	 * Owner of bounded diagnostic producers, causal records and deferred snapshots. Thread
	 * attachments and borrowed snapshot providers must be released before destruction.
	 */
	explicit DevErrorMonitoring(DevErrorConfig config = {});
	/** Release owned monitoring state after dependent recorders, attachments and scopes are finished. */
	~DevErrorMonitoring();
	/** Copying is disabled to preserve ownership and producer identity. */
	DevErrorMonitoring(const DevErrorMonitoring&) = delete;
	/** Copying is disabled to preserve ownership and producer identity. */
	DevErrorMonitoring& operator=(const DevErrorMonitoring&) = delete;

	/**
	 * Attach a producer to the calling thread and return its lifetime lease. The name is copied; the
	 * monitoring owner must outlive the attachment.
	 */
	[[nodiscard]] DevErrorThreadAttachment attachCurrentThread(std::string_view name);
	/**
	 * Observe a production error event and append development evidence without changing production
	 * resolution or control flow.
	 */
	void observeProductionEvent(const ErrorEventView& event) noexcept;
	/** Append a production boundary event to the specified diagnostic occurrence. */
	void recordProductionEvent(
		DevErrorOccurrenceId occurrence,
		const ErrorEventView& event,
		const DevErrorSourceDescriptor& source = {}) noexcept;
	/**
	 * Begin a diagnostic occurrence and return its identity; native text is copied into bounded
	 * record storage.
	 */
	[[nodiscard]] DevErrorOccurrenceId recordRaised(
		FlowUiError error,
		const DevErrorSourceDescriptor& source,
		DevErrorRecordFlag flags = DevErrorRecordFlag::None,
		std::string_view nativeText = {}) noexcept;
	/** Append one causal step and optional resolution to an existing occurrence. */
	void recordStep(
		DevErrorOccurrenceId occurrence,
		DevErrorStepKind kind,
		FlowUiError error = {},
		ErrorResolution resolution = ErrorResolution::None,
		const DevErrorSourceDescriptor& source = {}) noexcept;
	/** Append a bounded immediate evidence block to an occurrence. */
	void recordEvidence(
		DevErrorOccurrenceId occurrence,
		const DevErrorEvidenceBlock& evidence,
		const DevErrorSourceDescriptor& source = {}) noexcept;
	/** Append a breadcrumb with exact numeric values and optional occurrence attribution. */
	void recordBreadcrumb(
		const DevErrorBreadcrumbDescriptor& descriptor,
		uint64_t primaryValue = 0u,
		uint64_t secondaryValue = 0u,
		DevErrorOccurrenceId occurrence = 0u) noexcept;
	/**
	 * Copy registered diagnostic source descriptors; their immutable text remains borrowed from
	 * registration storage.
	 */
	[[nodiscard]] std::vector<DevErrorSourceDescriptor> sourceSnapshot() const;
	/** Copy registered breadcrumb descriptors; names remain borrowed from registration storage. */
	[[nodiscard]] std::vector<DevErrorBreadcrumbDescriptor> breadcrumbDescriptorSnapshot() const;
	/** Copy retained raw stack traces into owned storage. */
	[[nodiscard]] std::vector<DevErrorStackTrace> stackSnapshot() const;
	/**
	 * Register a borrowed nonzero source, owner and noexcept callback. The owner must remain alive
	 * until unregistered. Return false for invalid inputs or exhausted capacity.
	 */
	[[nodiscard]] bool registerSnapshotProvider(
		const DevErrorSnapshotProvider& provider) noexcept;
	/** Remove the provider matching source and owner before destroying its borrowed owner. */
	void unregisterSnapshotProvider(
		DevErrorSnapshotSourceId source,
		const void* owner) noexcept;
	/**
	 * Queue deferred capture for a nonzero occurrence/source at StackAndState detail or higher.
	 * Return false when disabled, invalid or out of capacity.
	 */
	[[nodiscard]] bool requestSnapshot(
		const DevErrorSnapshotRequest& request) noexcept;
	/**
	 * Run queued provider callbacks at a consumer safe point and retain bounded results. The caller
	 * must ensure provider owners are safe to inspect.
	 */
	void captureDeferredSnapshots() noexcept;
	/** Drain only newly produced data into reusable consumer storage. Each destination is cleared.
	 */
	void drain_snapshots_into(std::vector<DevErrorSnapshot>& destination);
	/** Drain new records in sequence order without copying retained history. */
	void drain_records_into(std::vector<DevErrorRecord>& destination);
	/** Drain new breadcrumbs in sequence order into reusable consumer storage. */
	void drain_breadcrumbs_into(std::vector<DevErrorBreadcrumb>& destination);
	/** Consume newly captured snapshots into a new owned vector. */
	[[nodiscard]] std::vector<DevErrorSnapshot> drainSnapshots();
	/** Consume newly produced diagnostic steps into a new owned vector. */
	[[nodiscard]] std::vector<DevErrorRecord> drainRecords();
	/** Consume newly produced breadcrumbs into a new owned vector. */
	[[nodiscard]] std::vector<DevErrorBreadcrumb> drainBreadcrumbs();
	/** Return cumulative capture capability and quality counters without consuming records. */
	[[nodiscard]] DevErrorQualitySnapshot qualitySnapshot() const noexcept;
	/** Copy the most recent fixed fatal capsule, or return empty when none was captured. */
	[[nodiscard]] std::optional<DevErrorFatalCapsule> fatalCapsuleSnapshot() const noexcept;
	/**
	 * Publish a fixed summary for later allocation-free fatal inspection at a synchronized safe
	 * point.
	 */
	void publishFatalSafePoint(const DevErrorFatalSafePointSummary& summary) noexcept;
	/**
	 * Apply monitoring or reporting configuration; retained storage and pending work follow the
	 * owning service policy.
	 */
	void setConfig(const DevErrorConfig& config) noexcept;
	/** Return a copy of the current effective configuration. */
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

/**
 * Record a development-only diagnostic through the active global monitoring owner. Copy native
 * text into bounded storage and preserve application control flow.
 */
void recordGlobalDevDiagnostic(
	FlowUiError error,
	const DevErrorSourceDescriptor& source,
	std::string_view nativeText = {}) noexcept;

/** Record a breadcrumb through the active global monitoring owner when one is available. */
void recordGlobalDevBreadcrumb(
	const DevErrorBreadcrumbDescriptor& descriptor,
	uint64_t primaryValue = 0u,
	uint64_t secondaryValue = 0u) noexcept;

} // namespace FlowUi::devSystems

#endif
