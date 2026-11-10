#pragma once

#include "FlowUi/Public/BuildConfig.hpp"

#if FLOW_UI_DEV_MODE

#include <memory>
#include <string_view>
#include <vector>

#include "devSystems/devMonitoringAndReporting/errors/DevErrorTypes.hpp"

namespace FlowUi::devSystems {

class DevErrorMonitoring;

/**
 * Thread-local error context and producer counters managed by DevErrorMonitoring. Use the
 * attached recorder on its producer thread.
 */
class DevErrorRecorder {
public:
	/** Release owned monitoring state after dependent recorders, attachments and scopes are finished. */
	~DevErrorRecorder();

	/** Copying is disabled to preserve ownership and producer identity. */
	DevErrorRecorder(const DevErrorRecorder&) = delete;
	/** Copying is disabled to preserve ownership and producer identity. */
	DevErrorRecorder& operator=(const DevErrorRecorder&) = delete;

	/**
	 * Copy application, frame and entity attribution used by subsequent diagnostic records on this
	 * producer.
	 */
	void setContext(const DevErrorContext& context) noexcept;
	/** Clear window-frame attribution for subsequent producer records. */
	void clearFrameContext() noexcept;
	/** Return a copy of this producer's current diagnostic attribution. */
	[[nodiscard]] DevErrorContext context() const noexcept;
	/** Return this producer track identity. */
	[[nodiscard]] uint32_t trackId() const noexcept;
	/** Borrow this producer track name; the owner retains its storage. */
	[[nodiscard]] std::string_view trackName() const noexcept;
	/** Increment the producer counter for a diagnostic suppressed by capture policy. */
	void noteSuppressed() noexcept;
	/** Increment the producer counter for a recursive diagnostic call. */
	void noteRecursive() noexcept;
	/** Increment the producer counter for bounded native text truncation. */
	void noteNativeTextTruncation() noexcept;

private:
	friend class DevErrorMonitoring;
	friend class DevErrorThreadAttachment;
	struct Impl;
	explicit DevErrorRecorder(
		DevErrorMonitoring& owner,
		uint32_t track,
		std::string_view name,
		const DevErrorConfig& config);
	[[nodiscard]] bool tryRecord(DevErrorRecord record) noexcept;
	[[nodiscard]] bool tryBreadcrumb(DevErrorBreadcrumb breadcrumb) noexcept;
	void drainRecordsInto(std::vector<DevErrorRecord>& destination);
	void drainBreadcrumbsInto(std::vector<DevErrorBreadcrumb>& destination);
	[[nodiscard]] DevErrorQualitySnapshot qualitySnapshot() const noexcept;
	[[nodiscard]] DevErrorContext emergencyContext() const noexcept;

	std::unique_ptr<Impl> impl_{};
};

/**
 * Move-only attachment of the current thread to an error recorder. Destroy it before its
 * monitoring owner.
 */
class DevErrorThreadAttachment {
public:
	/** Construct an empty inactive handle. */
	DevErrorThreadAttachment() noexcept = default;
	/** Detach the current thread recorder; its monitoring owner must still be alive. */
	~DevErrorThreadAttachment();
	/** Copying is disabled to preserve ownership and producer identity. */
	DevErrorThreadAttachment(const DevErrorThreadAttachment&) = delete;
	/** Copying is disabled to preserve ownership and producer identity. */
	DevErrorThreadAttachment& operator=(const DevErrorThreadAttachment&) = delete;
	/**
	 * Transfer ownership of the thread attachment from other; the moved-from attachment becomes
	 * empty.
	 */
	DevErrorThreadAttachment(DevErrorThreadAttachment&& other) noexcept;
	/** Assign owned state, transferring moves or rebinding borrowed views after a copy as required. */
	DevErrorThreadAttachment& operator=(DevErrorThreadAttachment&& other) noexcept;
	/** Return whether this frame, token or thread attachment is valid and active. */
	[[nodiscard]] explicit operator bool() const noexcept { return recorder_ != nullptr; }
	/**
	 * Borrow the attached producer. The attachment must be nonempty and its monitoring owner must
	 * still exist.
	 */
	[[nodiscard]] DevErrorRecorder& recorder() const noexcept { return *recorder_; }

private:
	friend class DevErrorMonitoring;
	explicit DevErrorThreadAttachment(DevErrorRecorder& recorder) noexcept
		: recorder_(&recorder) {}
	void reset() noexcept;
	DevErrorRecorder* recorder_ = nullptr;
};

} // namespace FlowUi::devSystems

#endif
