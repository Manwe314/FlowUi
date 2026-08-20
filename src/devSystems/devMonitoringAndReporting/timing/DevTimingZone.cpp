#include "devSystems/devMonitoringAndReporting/timing/DevTimingZone.hpp"

#if FLOW_UI_DEV_MODE

#include <utility>

namespace FlowUi::devSystems {

CpuTimingZone::CpuTimingZone(
	DevTimingRecorder& recorder,
	const TimingZoneDescriptor& descriptor,
	TimingEntityRef entity) noexcept
	: recorder_(&recorder),
	  token_(recorder.tryBegin(descriptor, entity)) {}

CpuTimingZone::~CpuTimingZone() noexcept {
	if (recorder_ && token_) recorder_->end(token_);
}

ManualTimingZone::ManualTimingZone(
	DevTimingRecorder& recorder,
	const TimingZoneDescriptor& descriptor,
	TimingEntityRef entity) noexcept {
	begin(recorder, descriptor, entity);
}

ManualTimingZone::~ManualTimingZone() noexcept { abandon(); }

ManualTimingZone::ManualTimingZone(ManualTimingZone&& other) noexcept
	: recorder_(std::exchange(other.recorder_, nullptr)),
	  token_(std::exchange(other.token_, {})) {}

ManualTimingZone& ManualTimingZone::operator=(ManualTimingZone&& other) noexcept {
	if (this == &other) return *this;
	abandon();
	recorder_ = std::exchange(other.recorder_, nullptr);
	token_ = std::exchange(other.token_, {});
	return *this;
}

void ManualTimingZone::begin(
	DevTimingRecorder& recorder,
	const TimingZoneDescriptor& descriptor,
	TimingEntityRef entity) noexcept {
	abandon();
	recorder_ = &recorder;
	token_ = recorder.tryBegin(descriptor, entity);
}

void ManualTimingZone::end(TimingRecordFlag result) noexcept {
	if (recorder_ && token_) recorder_->end(token_, result);
	recorder_ = nullptr;
	token_ = {};
}

void ManualTimingZone::abandon() noexcept {
	if (recorder_ && token_) recorder_->end(token_, TimingRecordFlag::Incomplete);
	recorder_ = nullptr;
	token_ = {};
}

} // namespace FlowUi::devSystems

#endif
