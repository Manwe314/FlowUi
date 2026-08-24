#pragma once

#include "FlowUi/BuildConfig.hpp"

#if FLOW_UI_DEV_MODE

#include <memory>
#include <string_view>
#include <vector>

#include "devSystems/devMonitoringAndReporting/errors/DevErrorTypes.hpp"

namespace FlowUi::devSystems {

class DevErrorMonitoring;

class DevErrorRecorder {
public:
	~DevErrorRecorder();

	DevErrorRecorder(const DevErrorRecorder&) = delete;
	DevErrorRecorder& operator=(const DevErrorRecorder&) = delete;

	void setContext(const DevErrorContext& context) noexcept;
	void clearFrameContext() noexcept;
	[[nodiscard]] DevErrorContext context() const noexcept;
	[[nodiscard]] uint32_t trackId() const noexcept;
	[[nodiscard]] std::string_view trackName() const noexcept;
	void noteSuppressed() noexcept;
	void noteRecursive() noexcept;
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

class DevErrorThreadAttachment {
public:
	DevErrorThreadAttachment() noexcept = default;
	~DevErrorThreadAttachment();
	DevErrorThreadAttachment(const DevErrorThreadAttachment&) = delete;
	DevErrorThreadAttachment& operator=(const DevErrorThreadAttachment&) = delete;
	DevErrorThreadAttachment(DevErrorThreadAttachment&& other) noexcept;
	DevErrorThreadAttachment& operator=(DevErrorThreadAttachment&& other) noexcept;
	[[nodiscard]] explicit operator bool() const noexcept { return recorder_ != nullptr; }
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
