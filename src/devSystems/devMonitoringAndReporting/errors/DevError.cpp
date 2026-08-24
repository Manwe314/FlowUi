#include "devSystems/devMonitoringAndReporting/errors/DevError.hpp"

#if FLOW_UI_DEV_MODE

#include <algorithm>
#include <atomic>
#include <chrono>
#include <cstring>
#include <mutex>
#include <thread>

#include "devSystems/devMonitoringAndReporting/errors/DevErrorStackProvider.hpp"

namespace FlowUi::devSystems {
namespace {

static_assert(std::atomic<uint64_t>::is_always_lock_free);
static_assert(std::atomic<DevErrorStackProvider*>::is_always_lock_free);

thread_local DevErrorRecorder* currentRecorder = nullptr;
thread_local bool recordingEvent = false;
std::atomic<DevErrorMonitoring*> activeMonitoring{nullptr};
inline constexpr auto kProductionResolvedBreadcrumb =
	makeDevErrorBreadcrumb("flowui.error.policy_resolution");
inline constexpr auto kProductionReportedBreadcrumb =
	makeDevErrorBreadcrumb("flowui.error.reported");
inline constexpr auto kProductionFatalBreadcrumb =
	makeDevErrorBreadcrumb("flowui.error.fatal");
inline constexpr auto kProductionBackendBreadcrumb =
	makeDevErrorBreadcrumb("flowui.error.backend_diagnostic");

uint64_t nowNs() noexcept {
	return static_cast<uint64_t>(std::chrono::duration_cast<std::chrono::nanoseconds>(
		std::chrono::steady_clock::now().time_since_epoch()).count());
}

void accumulate(DevErrorQualitySnapshot& destination, const DevErrorQualitySnapshot& source) {
	destination.recordedEvents += source.recordedEvents;
	destination.recordedBreadcrumbs += source.recordedBreadcrumbs;
	destination.suppressedEvents += source.suppressedEvents;
	destination.droppedEvents += source.droppedEvents;
	destination.droppedBreadcrumbs += source.droppedBreadcrumbs;
	destination.overwrittenBreadcrumbs += source.overwrittenBreadcrumbs;
	destination.recursiveEvents += source.recursiveEvents;
	destination.nativeTextTruncations += source.nativeTextTruncations;
	destination.droppedDescriptors += source.droppedDescriptors;
	destination.capturedStacks += source.capturedStacks;
	destination.deduplicatedStacks += source.deduplicatedStacks;
	destination.unavailableStacks += source.unavailableStacks;
	destination.truncatedStacks += source.truncatedStacks;
	destination.lostStacks += source.lostStacks;
	destination.requestedSnapshots += source.requestedSnapshots;
	destination.capturedSnapshots += source.capturedSnapshots;
	destination.staleSnapshots += source.staleSnapshots;
	destination.unavailableSnapshots += source.unavailableSnapshots;
	destination.truncatedSnapshots += source.truncatedSnapshots;
	destination.lostSnapshots += source.lostSnapshots;
	destination.capturedFatalCapsules += source.capturedFatalCapsules;
	destination.lostFatalCapsules += source.lostFatalCapsules;
	destination.overhead.producerCalls += source.overhead.producerCalls;
	destination.overhead.producerTimeNs += source.overhead.producerTimeNs;
	destination.overhead.recordedBytes += source.overhead.recordedBytes;
	destination.overhead.sourceRegistrationCalls += source.overhead.sourceRegistrationCalls;
	destination.overhead.sourceRegistrationTimeNs += source.overhead.sourceRegistrationTimeNs;
	destination.overhead.stackCaptureCalls += source.overhead.stackCaptureCalls;
	destination.overhead.stackCaptureTimeNs += source.overhead.stackCaptureTimeNs;
	destination.overhead.capturedStackFrames += source.overhead.capturedStackFrames;
	destination.overhead.fatalCaptureCalls += source.overhead.fatalCaptureCalls;
	destination.overhead.fatalCaptureTimeNs += source.overhead.fatalCaptureTimeNs;
}

struct ScopedAtomicDuration {
	std::atomic<uint64_t>& calls;
	std::atomic<uint64_t>& elapsed;
	uint64_t start = nowNs();
	~ScopedAtomicDuration() {
		calls.fetch_add(1u, std::memory_order_relaxed);
		elapsed.fetch_add(nowNs() - start, std::memory_order_relaxed);
	}
};

void copyNativeText(
	DevErrorRecord& record,
	std::string_view text,
	uint32_t configuredLimit,
	DevErrorRecorder& recorder) noexcept {
	if (text.empty() || configuredLimit == 0u) return;
	const size_t limit = std::min<size_t>(configuredLimit, record.nativeText.size());
	const size_t copied = std::min(limit, text.size());
	std::memcpy(record.nativeText.data(), text.data(), copied);
	record.nativeTextLength = static_cast<uint16_t>(copied);
	record.flags |= DevErrorRecordFlag::NativeTextPresent;
	if (copied < text.size()) {
		record.flags |= DevErrorRecordFlag::NativeTextTruncated;
		recorder.noteNativeTextTruncation();
	}
}

} // namespace

struct DevErrorMonitoring::Impl {
	struct AtomicFatalBreadcrumb {
		std::atomic_flag writing = ATOMIC_FLAG_INIT;
		std::atomic<uint64_t> committedSequence{0u};
		std::atomic<uint64_t> timestampNs{0u};
		std::atomic<uint64_t> descriptorId{0u};
		std::atomic<uint64_t> primaryValue{0u};
		std::atomic<uint64_t> secondaryValue{0u};
		std::atomic<DevErrorOccurrenceId> occurrence{0u};
		std::atomic<uint32_t> threadTrack{0u};
	};

	static constexpr size_t kFatalBreadcrumbJournalCapacity = 32u;

	static DevErrorConfig normalize(DevErrorConfig config) noexcept {
		config.level = static_cast<DevErrorCaptureLevel>(std::min(
			static_cast<uint8_t>(config.level),
			static_cast<uint8_t>(compiledDevErrorLevel())));
		config.producerRecordCapacity = std::max(1u, config.producerRecordCapacity);
		config.breadcrumbCapacity = std::max(1u, config.breadcrumbCapacity);
		config.recentBreadcrumbCount = std::max(1u, config.recentBreadcrumbCount);
		config.nativeTextLimit = std::min<uint32_t>(
			config.nativeTextLimit, static_cast<uint32_t>(kDevErrorNativeTextCapacity));
		config.threadRecorderCapacity = std::max(1u, config.threadRecorderCapacity);
		config.sourceDescriptorCapacity = std::max(1u, config.sourceDescriptorCapacity);
		config.breadcrumbDescriptorCapacity = std::max(1u, config.breadcrumbDescriptorCapacity);
		config.stackTraceCapacity = std::max(1u, config.stackTraceCapacity);
		config.maximumStackFrames = std::clamp<uint32_t>(
			config.maximumStackFrames, 1u, kDevErrorMaximumStackFrames);
		config.snapshotProviderCapacity = std::max(1u, config.snapshotProviderCapacity);
		config.pendingSnapshotCapacity = std::max(1u, config.pendingSnapshotCapacity);
		config.retainedSnapshotCapacity = std::max(1u, config.retainedSnapshotCapacity);
		return config;
	}

	DevErrorStackId captureStack(DevErrorRecordFlag& flags) noexcept {
		ScopedAtomicDuration duration{stackCaptureCalls, stackCaptureTimeNs};
		DevErrorStackProvider* provider = nullptr;
		uint32_t maximumFrames = 0u;
		{
			std::scoped_lock lock(mutex);
			provider = monitoringConfig.stackProvider
				? monitoringConfig.stackProvider : &platformStackProvider;
			maximumFrames = monitoringConfig.maximumStackFrames;
		}
		std::array<uintptr_t, kDevErrorMaximumStackFrames> scratch{};
		const DevErrorRawStackCapture captured = provider->capture(
			std::span<uintptr_t>(scratch.data(), maximumFrames), 3u);
		if (captured.status == DevErrorStackStatus::Unavailable || captured.frameCount == 0u) {
			flags |= DevErrorRecordFlag::StackUnavailable;
			unavailableStacks.fetch_add(1u, std::memory_order_relaxed);
			return 0u;
		}
		capturedStackFrames.fetch_add(captured.frameCount, std::memory_order_relaxed);
		if (captured.status == DevErrorStackStatus::Truncated) {
			flags |= DevErrorRecordFlag::StackTruncated;
			truncatedStacks.fetch_add(1u, std::memory_order_relaxed);
		}
		uint64_t hash = detail::dev_error::kHashOffset;
		for (uint16_t index = 0u; index < captured.frameCount; ++index) {
			hash = detail::dev_error::hashInteger(scratch[index], hash);
		}
		hash = detail::dev_error::hashInteger(captured.moduleIdentity, hash);
		hash = detail::dev_error::hashInteger(captured.buildIdentity, hash);

		try {
			std::scoped_lock lock(mutex);
			for (uint32_t index = 0u; index < stackCount; ++index) {
				const DevErrorStackTrace& existing = stacks[index];
				if (existing.hash != hash || existing.frameCount != captured.frameCount ||
					existing.moduleIdentity != captured.moduleIdentity ||
					existing.buildIdentity != captured.buildIdentity) continue;
				if (std::equal(
					existing.frames.begin(), existing.frames.begin() + existing.frameCount,
					scratch.begin())) {
					deduplicatedStacks.fetch_add(1u, std::memory_order_relaxed);
					return existing.id;
				}
			}
			if (stackCount >= stacks.size()) {
				flags |= DevErrorRecordFlag::StackStorageLost;
				lostStacks.fetch_add(1u, std::memory_order_relaxed);
				return 0u;
			}
			DevErrorStackTrace& destination = stacks[stackCount++];
			destination.id = nextStackId.fetch_add(1u, std::memory_order_relaxed);
			destination.hash = hash;
			destination.moduleIdentity = captured.moduleIdentity;
			destination.buildIdentity = captured.buildIdentity;
			destination.status = captured.status;
			destination.frameCount = captured.frameCount;
			std::copy_n(scratch.begin(), captured.frameCount, destination.frames.begin());
			capturedStacks.fetch_add(1u, std::memory_order_relaxed);
			return destination.id;
		} catch (...) {
			flags |= DevErrorRecordFlag::StackStorageLost;
			lostStacks.fetch_add(1u, std::memory_order_relaxed);
			return 0u;
		}
	}

	void rememberBreadcrumb(const DevErrorBreadcrumb& breadcrumb) noexcept {
		AtomicFatalBreadcrumb& slot = fatalBreadcrumbJournal[
			breadcrumb.sequence % fatalBreadcrumbJournal.size()];
		if (slot.writing.test_and_set(std::memory_order_acquire)) return;
		slot.committedSequence.store(0u, std::memory_order_relaxed);
		slot.timestampNs.store(breadcrumb.timestampNs, std::memory_order_relaxed);
		slot.descriptorId.store(breadcrumb.descriptorId, std::memory_order_relaxed);
		slot.primaryValue.store(breadcrumb.primaryValue, std::memory_order_relaxed);
		slot.secondaryValue.store(breadcrumb.secondaryValue, std::memory_order_relaxed);
		slot.occurrence.store(breadcrumb.occurrence, std::memory_order_relaxed);
		slot.threadTrack.store(breadcrumb.threadTrack, std::memory_order_relaxed);
		slot.committedSequence.store(breadcrumb.sequence, std::memory_order_release);
		slot.writing.clear(std::memory_order_release);
	}

	bool readSafePoint(DevErrorFatalSafePointSummary& result) const noexcept {
		for (uint32_t attempt = 0u; attempt < 3u; ++attempt) {
			const uint64_t before = safePointSequence.load(std::memory_order_acquire);
			if (before == 0u || (before & 1u) != 0u) continue;
			result = DevErrorFatalSafePointSummary{
				.revision = safePointRevision.load(std::memory_order_relaxed),
				.appTick = safePointAppTick.load(std::memory_order_relaxed),
				.timingMutationSequence = safePointTimingMutation.load(std::memory_order_relaxed),
				.timingDroppedRecords = safePointTimingDropped.load(std::memory_order_relaxed),
				.memoryGeneration = safePointMemoryGeneration.load(std::memory_order_relaxed),
				.memoryMutationSequence = safePointMemoryMutation.load(std::memory_order_relaxed),
				.memoryDroppedOperations = safePointMemoryDropped.load(std::memory_order_relaxed),
				.errorRecordedEvents = safePointErrorRecorded.load(std::memory_order_relaxed),
				.errorDroppedEvents = safePointErrorDropped.load(std::memory_order_relaxed),
			};
			if (before == safePointSequence.load(std::memory_order_acquire)) return true;
		}
		return false;
	}

	void writeFatalCapsule(
		const ErrorEventView& event,
		const DevErrorSourceDescriptor& source,
		DevErrorRecorder& recorder) noexcept {
		ScopedAtomicDuration duration{fatalCaptureCalls, fatalCaptureTimeNs};
		uint8_t expected = 0u;
		if (!fatalCapsuleState.compare_exchange_strong(
			expected, 1u, std::memory_order_acq_rel)) {
			lostFatalCapsules.fetch_add(1u, std::memory_order_relaxed);
			return;
		}

		DevErrorFatalCapsule capsule{};
		capsule.byteSize = sizeof(DevErrorFatalCapsule);
		capsule.error = event.error;
		capsule.occurrence = nextOccurrence.fetch_add(1u, std::memory_order_relaxed);
		capsule.timestampNs = nowNs();
		capsule.context = recorder.emergencyContext();
		capsule.sourceId = source.id;
		capsule.threadTrack = recorder.trackId();
		capsule.nativeCategory = event.nativeCategory;
		capsule.resolution = event.resolution;
		capsule.productionInspection = event.inspection;
		capsule.captureLevel = static_cast<DevErrorCaptureLevel>(
			captureLevel.load(std::memory_order_relaxed));
		capsule.availableInputs = DevErrorFatalCapabilityInput::EmergencyCapsule |
			DevErrorFatalCapabilityInput::CurrentContext;
		(void)capsule.evidence.append(DevErrorEvidenceKey::Subject, event.error.subject);
		(void)capsule.evidence.append(DevErrorEvidenceKey::Auxiliary, event.error.auxiliary);
		(void)capsule.evidence.append(DevErrorEvidenceKey::NativeCode, event.error.nativeCode);
		(void)capsule.evidence.append(
			DevErrorEvidenceKey::Resolution, static_cast<uint64_t>(event.resolution));

		if (!event.nativeMessage.empty()) {
			const size_t copied = std::min(event.nativeMessage.size(), capsule.nativeText.size());
			std::memcpy(capsule.nativeText.data(), event.nativeMessage.data(), copied);
			capsule.nativeTextLength = static_cast<uint16_t>(copied);
			capsule.flags |= DevErrorRecordFlag::NativeTextPresent;
			if (copied < event.nativeMessage.size()) {
				capsule.flags |= DevErrorRecordFlag::NativeTextTruncated;
			}
		}

		if (readSafePoint(capsule.safePoint)) {
			capsule.availableInputs |= DevErrorFatalCapabilityInput::SafePointSummary;
		}

		const uint64_t newest = nextBreadcrumbSequence.load(std::memory_order_acquire) - 1u;
		const uint64_t first = newest >= kDevErrorFatalBreadcrumbCapacity
			? newest - kDevErrorFatalBreadcrumbCapacity + 1u : 1u;
		for (uint64_t sequence = first; sequence <= newest && sequence != 0u; ++sequence) {
			const AtomicFatalBreadcrumb& slot =
				fatalBreadcrumbJournal[sequence % fatalBreadcrumbJournal.size()];
			if (slot.committedSequence.load(std::memory_order_acquire) != sequence) continue;
			DevErrorFatalBreadcrumb breadcrumb{
				.sequence = sequence,
				.timestampNs = slot.timestampNs.load(std::memory_order_relaxed),
				.descriptorId = slot.descriptorId.load(std::memory_order_relaxed),
				.primaryValue = slot.primaryValue.load(std::memory_order_relaxed),
				.secondaryValue = slot.secondaryValue.load(std::memory_order_relaxed),
				.occurrence = slot.occurrence.load(std::memory_order_relaxed),
				.threadTrack = slot.threadTrack.load(std::memory_order_relaxed),
			};
			if (slot.committedSequence.load(std::memory_order_acquire) != sequence) continue;
			capsule.breadcrumbs[capsule.breadcrumbCount++] = breadcrumb;
		}
		if (capsule.breadcrumbCount != 0u) {
			capsule.availableInputs |= DevErrorFatalCapabilityInput::RecentBreadcrumbs;
		}

		if (capsule.captureLevel >= DevErrorCaptureLevel::StackAndState) {
			DevErrorStackProvider* provider = emergencyStackProvider.load(std::memory_order_acquire);
			const DevErrorRawStackCapture stack = provider
				? provider->captureEmergency(capsule.stackFrames, 3u)
				: DevErrorRawStackCapture{};
			capsule.stackStatus = stack.status;
			capsule.stackFrameCount = static_cast<uint16_t>(std::min<size_t>(
				stack.frameCount, capsule.stackFrames.size()));
			capsule.moduleIdentity = stack.moduleIdentity;
			capsule.buildIdentity = stack.buildIdentity;
			if (capsule.stackFrameCount != 0u) {
				capsule.availableInputs |= DevErrorFatalCapabilityInput::RawStack;
			}
			if (stack.status == DevErrorStackStatus::Unavailable) {
				capsule.flags |= DevErrorRecordFlag::StackUnavailable;
			} else if (stack.status == DevErrorStackStatus::Truncated) {
				capsule.flags |= DevErrorRecordFlag::StackTruncated;
			}
		}

		fatalCapsule = capsule;
		capturedFatalCapsules.fetch_add(1u, std::memory_order_relaxed);
		fatalCapsuleState.store(2u, std::memory_order_release);
	}

	mutable std::mutex mutex{};
	DevErrorConfig monitoringConfig{};
	std::atomic<uint8_t> captureLevel{0u};
	std::atomic<uint32_t> nativeTextLimit{0u};
	std::atomic<DevErrorStackProvider*> emergencyStackProvider{nullptr};
	std::vector<std::unique_ptr<DevErrorRecorder>> recorders{};
	std::unique_ptr<DevErrorRecorder> fallback{};
	std::atomic<uint32_t> nextTrack{1u};
	std::atomic<DevErrorOccurrenceId> nextOccurrence{1u};
	std::atomic<DevErrorRecordSequence> nextRecordSequence{1u};
	std::atomic<uint64_t> nextBreadcrumbSequence{1u};
	std::vector<DevErrorSourceDescriptor> sources{};
	std::vector<DevErrorBreadcrumbDescriptor> breadcrumbDescriptors{};
	PlatformDevErrorStackProvider platformStackProvider{};
	std::vector<DevErrorStackTrace> stacks{};
	uint32_t stackCount = 0u;
	std::atomic<DevErrorStackId> nextStackId{1u};
	std::vector<DevErrorSnapshotProvider> snapshotProviders{};
	std::vector<DevErrorSnapshotRequest> pendingSnapshots{};
	std::vector<DevErrorSnapshot> completedSnapshots{};
	std::atomic<uint64_t> droppedDescriptors{0u};
	std::atomic<uint64_t> capturedStacks{0u};
	std::atomic<uint64_t> deduplicatedStacks{0u};
	std::atomic<uint64_t> unavailableStacks{0u};
	std::atomic<uint64_t> truncatedStacks{0u};
	std::atomic<uint64_t> lostStacks{0u};
	std::atomic<uint64_t> requestedSnapshots{0u};
	std::atomic<uint64_t> capturedSnapshots{0u};
	std::atomic<uint64_t> staleSnapshots{0u};
	std::atomic<uint64_t> unavailableSnapshots{0u};
	std::atomic<uint64_t> truncatedSnapshots{0u};
	std::atomic<uint64_t> lostSnapshots{0u};
	std::array<AtomicFatalBreadcrumb, kFatalBreadcrumbJournalCapacity> fatalBreadcrumbJournal{};
	std::atomic<uint8_t> fatalCapsuleState{0u};
	DevErrorFatalCapsule fatalCapsule{};
	std::atomic<uint64_t> capturedFatalCapsules{0u};
	std::atomic<uint64_t> lostFatalCapsules{0u};
	std::atomic<uint64_t> producerCalls{0u};
	std::atomic<uint64_t> producerTimeNs{0u};
	std::atomic<uint64_t> sourceRegistrationCalls{0u};
	std::atomic<uint64_t> sourceRegistrationTimeNs{0u};
	std::atomic<uint64_t> stackCaptureCalls{0u};
	std::atomic<uint64_t> stackCaptureTimeNs{0u};
	std::atomic<uint64_t> capturedStackFrames{0u};
	std::atomic<uint64_t> fatalCaptureCalls{0u};
	std::atomic<uint64_t> fatalCaptureTimeNs{0u};
	std::atomic<uint64_t> safePointSequence{0u};
	std::atomic<uint64_t> safePointRevision{0u};
	std::atomic<AppTickId> safePointAppTick{0u};
	std::atomic<uint64_t> safePointTimingMutation{0u};
	std::atomic<uint64_t> safePointTimingDropped{0u};
	std::atomic<uint64_t> safePointMemoryGeneration{0u};
	std::atomic<uint64_t> safePointMemoryMutation{0u};
	std::atomic<uint64_t> safePointMemoryDropped{0u};
	std::atomic<uint64_t> safePointErrorRecorded{0u};
	std::atomic<uint64_t> safePointErrorDropped{0u};
};

DevErrorMonitoring::DevErrorMonitoring(DevErrorConfig config) {
	struct Bootstrap {
		static std::unique_ptr<Impl> make(DevErrorMonitoring& owner, DevErrorConfig initial) {
			auto impl = std::unique_ptr<Impl>(new Impl{});
			impl->monitoringConfig = Impl::normalize(initial);
			impl->captureLevel.store(
				static_cast<uint8_t>(impl->monitoringConfig.level), std::memory_order_relaxed);
			impl->nativeTextLimit.store(
				impl->monitoringConfig.nativeTextLimit, std::memory_order_relaxed);
			impl->emergencyStackProvider.store(
				impl->monitoringConfig.stackProvider
					? impl->monitoringConfig.stackProvider : &impl->platformStackProvider,
				std::memory_order_relaxed);
			impl->recorders.reserve(impl->monitoringConfig.threadRecorderCapacity);
			impl->sources.reserve(impl->monitoringConfig.sourceDescriptorCapacity);
			impl->breadcrumbDescriptors.reserve(
				impl->monitoringConfig.breadcrumbDescriptorCapacity);
			impl->stacks.resize(impl->monitoringConfig.stackTraceCapacity);
			impl->snapshotProviders.reserve(impl->monitoringConfig.snapshotProviderCapacity);
			impl->pendingSnapshots.reserve(impl->monitoringConfig.pendingSnapshotCapacity);
			impl->completedSnapshots.reserve(impl->monitoringConfig.retainedSnapshotCapacity);
			impl->fallback = std::unique_ptr<DevErrorRecorder>(new DevErrorRecorder(
				owner, 0u, "flowui.unattached", impl->monitoringConfig));
			return impl;
		}
	};
	impl_ = Bootstrap::make(*this, config);
	activeMonitoring.store(this, std::memory_order_release);
}

DevErrorMonitoring::~DevErrorMonitoring() {
	DevErrorMonitoring* expected = this;
	(void)activeMonitoring.compare_exchange_strong(
		expected, nullptr, std::memory_order_acq_rel);
}

DevErrorThreadAttachment DevErrorMonitoring::attachCurrentThread(std::string_view name) {
	std::scoped_lock lock(impl_->mutex);
	if (impl_->recorders.size() >= impl_->monitoringConfig.threadRecorderCapacity) {
		currentRecorder = impl_->fallback.get();
		return DevErrorThreadAttachment(*impl_->fallback);
	}
	auto recorder = std::unique_ptr<DevErrorRecorder>(new DevErrorRecorder(
		*this,
		impl_->nextTrack.fetch_add(1u, std::memory_order_relaxed),
		name,
		impl_->monitoringConfig));
	DevErrorRecorder* pointer = recorder.get();
	impl_->recorders.push_back(std::move(recorder));
	currentRecorder = pointer;
	return DevErrorThreadAttachment(*pointer);
}

void DevErrorMonitoring::detach(DevErrorRecorder& recorder) noexcept {
	if (currentRecorder == &recorder) currentRecorder = nullptr;
}

void DevErrorMonitoring::observeProductionEvent(const ErrorEventView& event) noexcept {
	recordProductionEvent(0u, event);
}

void DevErrorMonitoring::recordProductionEvent(
	DevErrorOccurrenceId occurrence,
	const ErrorEventView& event,
	const DevErrorSourceDescriptor& source) noexcept {
	DevErrorRecorder* recorder = currentRecorder ? currentRecorder : impl_->fallback.get();
	ScopedAtomicDuration duration{impl_->producerCalls, impl_->producerTimeNs};
	if (impl_->captureLevel.load(std::memory_order_relaxed) ==
		static_cast<uint8_t>(DevErrorCaptureLevel::Disabled)) {
		recorder->noteSuppressed();
		return;
	}
	if (event.kind == ErrorEventKind::Fatal) {
		impl_->writeFatalCapsule(event, source, *recorder);
		return;
	}
	if (recordingEvent) {
		recorder->noteRecursive();
		return;
	}
	struct Guard {
		Guard() noexcept { recordingEvent = true; }
		~Guard() { recordingEvent = false; }
	} guard;
	const DevErrorBreadcrumbDescriptor* eventBreadcrumb = &kProductionReportedBreadcrumb;
	switch (event.kind) {
	case ErrorEventKind::Resolved: eventBreadcrumb = &kProductionResolvedBreadcrumb; break;
	case ErrorEventKind::Reported: eventBreadcrumb = &kProductionReportedBreadcrumb; break;
	case ErrorEventKind::Fatal: eventBreadcrumb = &kProductionFatalBreadcrumb; break;
	case ErrorEventKind::BackendDiagnostic: eventBreadcrumb = &kProductionBackendBreadcrumb; break;
	}
	recordBreadcrumb(
		*eventBreadcrumb,
		static_cast<uint64_t>(event.resolution),
		event.error.subject,
		occurrence);
	try {
		if (source.id != 0u) {
			ScopedAtomicDuration sourceDuration{
				impl_->sourceRegistrationCalls, impl_->sourceRegistrationTimeNs};
			std::scoped_lock lock(impl_->mutex);
			const auto found = std::find_if(impl_->sources.begin(), impl_->sources.end(),
				[&](const auto& value) { return value.id == source.id; });
			if (found == impl_->sources.end()) {
				if (impl_->sources.size() < impl_->monitoringConfig.sourceDescriptorCapacity) {
					impl_->sources.push_back(source);
				} else {
					impl_->droppedDescriptors.fetch_add(1u, std::memory_order_relaxed);
				}
			}
		}
	} catch (...) {
		recorder->noteSuppressed();
	}

	DevErrorRecord record{};
	record.sequence = impl_->nextRecordSequence.fetch_add(1u, std::memory_order_relaxed);
	record.timestampNs = nowNs();
	record.occurrence = occurrence != 0u
		? occurrence
		: impl_->nextOccurrence.fetch_add(1u, std::memory_order_relaxed);
	record.error = event.error;
	record.sourceId = source.id;
	record.productionKind = event.kind;
	record.resolution = event.resolution;
	record.nativeCategory = event.nativeCategory;
	(void)record.evidence.append(DevErrorEvidenceKey::Subject, event.error.subject);
	(void)record.evidence.append(DevErrorEvidenceKey::Auxiliary, event.error.auxiliary);
	(void)record.evidence.append(DevErrorEvidenceKey::NativeCode, event.error.nativeCode);
	(void)record.evidence.append(
		DevErrorEvidenceKey::Resolution, static_cast<uint64_t>(event.resolution));
	switch (event.kind) {
	case ErrorEventKind::Resolved: record.kind = DevErrorStepKind::Resolution; break;
	case ErrorEventKind::Reported:
		record.kind = DevErrorStepKind::Raised;
		record.flags |= DevErrorRecordFlag::ExternallyReported;
		break;
	case ErrorEventKind::Fatal: record.kind = DevErrorStepKind::FatalCapture; break;
	case ErrorEventKind::BackendDiagnostic: record.kind = DevErrorStepKind::BackendDiagnostic; break;
	}
	if (impl_->captureLevel.load(std::memory_order_relaxed) >=
		static_cast<uint8_t>(DevErrorCaptureLevel::StackAndState) &&
		event.kind != ErrorEventKind::BackendDiagnostic) {
		record.stackId = impl_->captureStack(record.flags);
	}

	copyNativeText(
		record, event.nativeMessage,
		impl_->nativeTextLimit.load(std::memory_order_relaxed), *recorder);
	(void)recorder->tryRecord(record);
}

DevErrorOccurrenceId DevErrorMonitoring::recordRaised(
	FlowUiError error,
	const DevErrorSourceDescriptor& source,
	DevErrorRecordFlag flags,
	std::string_view nativeText) noexcept {
	DevErrorRecorder* recorder = currentRecorder ? currentRecorder : impl_->fallback.get();
	ScopedAtomicDuration duration{impl_->producerCalls, impl_->producerTimeNs};
	if (impl_->captureLevel.load(std::memory_order_relaxed) ==
		static_cast<uint8_t>(DevErrorCaptureLevel::Disabled)) {
		recorder->noteSuppressed();
		return 0u;
	}
	const DevErrorOccurrenceId occurrence =
		impl_->nextOccurrence.fetch_add(1u, std::memory_order_relaxed);
	try {
		if (source.id != 0u) {
			ScopedAtomicDuration sourceDuration{
				impl_->sourceRegistrationCalls, impl_->sourceRegistrationTimeNs};
			std::scoped_lock lock(impl_->mutex);
			const auto found = std::find_if(impl_->sources.begin(), impl_->sources.end(),
				[&](const auto& value) { return value.id == source.id; });
			if (found == impl_->sources.end()) {
				if (impl_->sources.size() < impl_->monitoringConfig.sourceDescriptorCapacity) {
					impl_->sources.push_back(source);
				} else {
					impl_->droppedDescriptors.fetch_add(1u, std::memory_order_relaxed);
				}
			}
		}
		DevErrorRecord record{
			.sequence = impl_->nextRecordSequence.fetch_add(1u, std::memory_order_relaxed),
			.timestampNs = nowNs(),
			.occurrence = occurrence,
			.error = error,
			.sourceId = source.id,
			.kind = DevErrorStepKind::Raised,
			.productionKind = ErrorEventKind::Reported,
			.flags = flags,
		};
		(void)record.evidence.append(DevErrorEvidenceKey::Subject, error.subject);
		(void)record.evidence.append(DevErrorEvidenceKey::Auxiliary, error.auxiliary);
		(void)record.evidence.append(DevErrorEvidenceKey::NativeCode, error.nativeCode);
		if (impl_->captureLevel.load(std::memory_order_relaxed) >=
			static_cast<uint8_t>(DevErrorCaptureLevel::StackAndState)) {
			record.stackId = impl_->captureStack(record.flags);
		}
		copyNativeText(
			record, nativeText,
			impl_->nativeTextLimit.load(std::memory_order_relaxed), *recorder);
		(void)recorder->tryRecord(record);
	} catch (...) {
		recorder->noteSuppressed();
	}
	return occurrence;
}

void DevErrorMonitoring::recordEvidence(
	DevErrorOccurrenceId occurrence,
	const DevErrorEvidenceBlock& evidence,
	const DevErrorSourceDescriptor& source) noexcept {
	if (occurrence == 0u || impl_->captureLevel.load(std::memory_order_relaxed) <
		static_cast<uint8_t>(DevErrorCaptureLevel::StackAndState)) return;
	ScopedAtomicDuration duration{impl_->producerCalls, impl_->producerTimeNs};
	DevErrorRecorder* recorder = currentRecorder ? currentRecorder : impl_->fallback.get();
	try {
		if (source.id != 0u) {
			ScopedAtomicDuration sourceDuration{
				impl_->sourceRegistrationCalls, impl_->sourceRegistrationTimeNs};
			std::scoped_lock lock(impl_->mutex);
			const auto found = std::find_if(impl_->sources.begin(), impl_->sources.end(),
				[&](const auto& value) { return value.id == source.id; });
			if (found == impl_->sources.end() &&
				impl_->sources.size() < impl_->monitoringConfig.sourceDescriptorCapacity) {
				impl_->sources.push_back(source);
			}
		}
		(void)recorder->tryRecord(DevErrorRecord{
			.sequence = impl_->nextRecordSequence.fetch_add(1u, std::memory_order_relaxed),
			.timestampNs = nowNs(),
			.occurrence = occurrence,
			.sourceId = source.id,
			.kind = DevErrorStepKind::Boundary,
			.evidence = evidence,
		});
	} catch (...) {
		recorder->noteSuppressed();
	}
}

void DevErrorMonitoring::recordStep(
	DevErrorOccurrenceId occurrence,
	DevErrorStepKind kind,
	FlowUiError error,
	ErrorResolution resolution,
	const DevErrorSourceDescriptor& source) noexcept {
	if (occurrence == 0u || impl_->captureLevel.load(std::memory_order_relaxed) <
		static_cast<uint8_t>(DevErrorCaptureLevel::Causal)) return;
	ScopedAtomicDuration duration{impl_->producerCalls, impl_->producerTimeNs};
	DevErrorRecorder* recorder = currentRecorder ? currentRecorder : impl_->fallback.get();
	try {
		if (source.id != 0u) {
			ScopedAtomicDuration sourceDuration{
				impl_->sourceRegistrationCalls, impl_->sourceRegistrationTimeNs};
			std::scoped_lock lock(impl_->mutex);
			const auto found = std::find_if(impl_->sources.begin(), impl_->sources.end(),
				[&](const auto& value) { return value.id == source.id; });
			if (found == impl_->sources.end()) {
				if (impl_->sources.size() < impl_->monitoringConfig.sourceDescriptorCapacity) {
					impl_->sources.push_back(source);
				} else {
					impl_->droppedDescriptors.fetch_add(1u, std::memory_order_relaxed);
				}
			}
		}
		(void)recorder->tryRecord(DevErrorRecord{
			.sequence = impl_->nextRecordSequence.fetch_add(1u, std::memory_order_relaxed),
			.timestampNs = nowNs(),
			.occurrence = occurrence,
			.error = error,
			.sourceId = source.id,
			.kind = kind,
			.productionKind = ErrorEventKind::Reported,
			.resolution = resolution,
		});
	} catch (...) {
		recorder->noteSuppressed();
	}
}

void DevErrorMonitoring::recordBreadcrumb(
	const DevErrorBreadcrumbDescriptor& descriptor,
	uint64_t primaryValue,
	uint64_t secondaryValue,
	DevErrorOccurrenceId occurrence) noexcept {
	if (impl_->captureLevel.load(std::memory_order_relaxed) <
		static_cast<uint8_t>(DevErrorCaptureLevel::Causal) || descriptor.id == 0u) return;
	ScopedAtomicDuration duration{impl_->producerCalls, impl_->producerTimeNs};
	DevErrorRecorder* recorder = currentRecorder ? currentRecorder : impl_->fallback.get();
	const uint64_t breadcrumbSequence =
		impl_->nextBreadcrumbSequence.fetch_add(1u, std::memory_order_relaxed);
	DevErrorBreadcrumb breadcrumb{
		.sequence = breadcrumbSequence,
		.timestampNs = nowNs(),
		.descriptorId = descriptor.id,
		.occurrence = occurrence,
		.context = recorder->emergencyContext(),
		.primaryValue = primaryValue,
		.secondaryValue = secondaryValue,
		.threadTrack = recorder->trackId(),
	};
	impl_->rememberBreadcrumb(breadcrumb);
	try {
		{
			std::scoped_lock lock(impl_->mutex);
			const auto found = std::find_if(
				impl_->breadcrumbDescriptors.begin(), impl_->breadcrumbDescriptors.end(),
				[&](const auto& value) { return value.id == descriptor.id; });
			if (found == impl_->breadcrumbDescriptors.end()) {
				if (impl_->breadcrumbDescriptors.size() <
					impl_->monitoringConfig.breadcrumbDescriptorCapacity) {
					impl_->breadcrumbDescriptors.push_back(descriptor);
				} else {
					impl_->droppedDescriptors.fetch_add(1u, std::memory_order_relaxed);
				}
			}
		}
		(void)recorder->tryBreadcrumb(breadcrumb);
	} catch (...) {
		recorder->noteSuppressed();
	}
}

std::vector<DevErrorSourceDescriptor> DevErrorMonitoring::sourceSnapshot() const {
	std::vector<DevErrorSourceDescriptor> result;
	std::scoped_lock lock(impl_->mutex);
	result = impl_->sources;
	std::sort(result.begin(), result.end(), [](const auto& left, const auto& right) {
		return left.id < right.id;
	});
	return result;
}

std::vector<DevErrorBreadcrumbDescriptor>
DevErrorMonitoring::breadcrumbDescriptorSnapshot() const {
	std::vector<DevErrorBreadcrumbDescriptor> result;
	std::scoped_lock lock(impl_->mutex);
	result = impl_->breadcrumbDescriptors;
	std::sort(result.begin(), result.end(), [](const auto& left, const auto& right) {
		return left.id < right.id;
	});
	return result;
}

std::vector<DevErrorStackTrace> DevErrorMonitoring::stackSnapshot() const {
	std::scoped_lock lock(impl_->mutex);
	return std::vector<DevErrorStackTrace>(
		impl_->stacks.begin(), impl_->stacks.begin() + impl_->stackCount);
}

bool DevErrorMonitoring::registerSnapshotProvider(
	const DevErrorSnapshotProvider& provider) noexcept {
	if (provider.source == 0u || provider.owner == nullptr || provider.capture == nullptr) {
		return false;
	}
	try {
		std::scoped_lock lock(impl_->mutex);
		const auto found = std::find_if(
			impl_->snapshotProviders.begin(), impl_->snapshotProviders.end(),
			[&](const auto& value) {
				return value.source == provider.source && value.owner == provider.owner;
			});
		if (found != impl_->snapshotProviders.end()) return true;
		if (impl_->snapshotProviders.size() >=
			impl_->monitoringConfig.snapshotProviderCapacity) {
			impl_->lostSnapshots.fetch_add(1u, std::memory_order_relaxed);
			return false;
		}
		impl_->snapshotProviders.push_back(provider);
		return true;
	} catch (...) {
		impl_->lostSnapshots.fetch_add(1u, std::memory_order_relaxed);
		return false;
	}
}

void DevErrorMonitoring::unregisterSnapshotProvider(
	DevErrorSnapshotSourceId source,
	const void* owner) noexcept {
	try {
		std::scoped_lock lock(impl_->mutex);
		const auto found = std::find_if(
			impl_->snapshotProviders.begin(), impl_->snapshotProviders.end(),
			[&](const auto& value) { return value.source == source && value.owner == owner; });
		if (found != impl_->snapshotProviders.end()) impl_->snapshotProviders.erase(found);
	} catch (...) {}
}

bool DevErrorMonitoring::requestSnapshot(const DevErrorSnapshotRequest& request) noexcept {
	if (request.occurrence == 0u || request.source == 0u ||
		impl_->captureLevel.load(std::memory_order_relaxed) <
			static_cast<uint8_t>(DevErrorCaptureLevel::StackAndState)) return false;
	try {
		std::scoped_lock lock(impl_->mutex);
		if (impl_->pendingSnapshots.size() >= impl_->monitoringConfig.pendingSnapshotCapacity) {
			impl_->lostSnapshots.fetch_add(1u, std::memory_order_relaxed);
			return false;
		}
		impl_->pendingSnapshots.push_back(request);
		impl_->requestedSnapshots.fetch_add(1u, std::memory_order_relaxed);
		return true;
	} catch (...) {
		impl_->lostSnapshots.fetch_add(1u, std::memory_order_relaxed);
		return false;
	}
}

void DevErrorMonitoring::captureDeferredSnapshots() noexcept {
	for (;;) {
		DevErrorSnapshotRequest request{};
		DevErrorSnapshotProvider provider{};
		try {
			std::scoped_lock lock(impl_->mutex);
			if (impl_->pendingSnapshots.empty()) break;
			request = impl_->pendingSnapshots.front();
			impl_->pendingSnapshots.erase(impl_->pendingSnapshots.begin());
			const auto found = std::find_if(
				impl_->snapshotProviders.begin(), impl_->snapshotProviders.end(),
				[&](const auto& value) { return value.source == request.source; });
			if (found != impl_->snapshotProviders.end()) provider = *found;
		} catch (...) {
			impl_->lostSnapshots.fetch_add(1u, std::memory_order_relaxed);
			break;
		}

		DevErrorSnapshot snapshot{
			.occurrence = request.occurrence,
			.source = request.source,
			.expectedRevision = request.expectedRevision,
			.outcome = DevErrorSnapshotOutcome::Unavailable,
		};
		if (provider.capture != nullptr) {
			DevErrorSnapshotSink sink(snapshot);
			sink.setOutcome(DevErrorSnapshotOutcome::Available);
			if (!provider.capture(provider.owner, request, sink) &&
				snapshot.outcome == DevErrorSnapshotOutcome::Available) {
				snapshot.outcome = DevErrorSnapshotOutcome::Unavailable;
			}
		}

		switch (snapshot.outcome) {
		case DevErrorSnapshotOutcome::Available:
			impl_->capturedSnapshots.fetch_add(1u, std::memory_order_relaxed); break;
		case DevErrorSnapshotOutcome::StaleRevision:
			impl_->staleSnapshots.fetch_add(1u, std::memory_order_relaxed); break;
		case DevErrorSnapshotOutcome::Truncated:
			impl_->truncatedSnapshots.fetch_add(1u, std::memory_order_relaxed); break;
		case DevErrorSnapshotOutcome::Unavailable:
			impl_->unavailableSnapshots.fetch_add(1u, std::memory_order_relaxed); break;
		case DevErrorSnapshotOutcome::StorageLost:
			impl_->lostSnapshots.fetch_add(1u, std::memory_order_relaxed); break;
		}

		try {
			std::scoped_lock lock(impl_->mutex);
			if (impl_->completedSnapshots.size() >=
				impl_->monitoringConfig.retainedSnapshotCapacity) {
				impl_->lostSnapshots.fetch_add(1u, std::memory_order_relaxed);
				continue;
			}
			impl_->completedSnapshots.push_back(snapshot);
		} catch (...) {
			impl_->lostSnapshots.fetch_add(1u, std::memory_order_relaxed);
		}
	}
}

std::vector<DevErrorSnapshot> DevErrorMonitoring::drainSnapshots() {
	std::vector<DevErrorSnapshot> result;
	std::scoped_lock lock(impl_->mutex);
	result.reserve(impl_->completedSnapshots.size());
	result.insert(
		result.end(), impl_->completedSnapshots.begin(), impl_->completedSnapshots.end());
	impl_->completedSnapshots.clear();
	return result;
}

std::vector<DevErrorRecord> DevErrorMonitoring::drainRecords() {
	std::vector<DevErrorRecord> result;
	std::scoped_lock lock(impl_->mutex);
	impl_->fallback->drainRecordsInto(result);
	for (const auto& recorder : impl_->recorders) recorder->drainRecordsInto(result);
	std::sort(result.begin(), result.end(), [](const auto& left, const auto& right) {
		return left.sequence < right.sequence;
	});
	return result;
}

std::vector<DevErrorBreadcrumb> DevErrorMonitoring::drainBreadcrumbs() {
	std::vector<DevErrorBreadcrumb> result;
	std::scoped_lock lock(impl_->mutex);
	impl_->fallback->drainBreadcrumbsInto(result);
	for (const auto& recorder : impl_->recorders) recorder->drainBreadcrumbsInto(result);
	std::sort(result.begin(), result.end(), [](const auto& left, const auto& right) {
		return left.sequence < right.sequence;
	});
	return result;
}

DevErrorQualitySnapshot DevErrorMonitoring::qualitySnapshot() const noexcept {
	DevErrorQualitySnapshot result{};
	try {
		std::scoped_lock lock(impl_->mutex);
		accumulate(result, impl_->fallback->qualitySnapshot());
		for (const auto& recorder : impl_->recorders) {
			accumulate(result, recorder->qualitySnapshot());
		}
		result.droppedDescriptors = impl_->droppedDescriptors.load(std::memory_order_relaxed);
		result.capturedStacks = impl_->capturedStacks.load(std::memory_order_relaxed);
		result.deduplicatedStacks = impl_->deduplicatedStacks.load(std::memory_order_relaxed);
		result.unavailableStacks = impl_->unavailableStacks.load(std::memory_order_relaxed);
		result.truncatedStacks = impl_->truncatedStacks.load(std::memory_order_relaxed);
		result.lostStacks = impl_->lostStacks.load(std::memory_order_relaxed);
		result.requestedSnapshots = impl_->requestedSnapshots.load(std::memory_order_relaxed);
		result.capturedSnapshots = impl_->capturedSnapshots.load(std::memory_order_relaxed);
		result.staleSnapshots = impl_->staleSnapshots.load(std::memory_order_relaxed);
		result.unavailableSnapshots = impl_->unavailableSnapshots.load(std::memory_order_relaxed);
		result.truncatedSnapshots = impl_->truncatedSnapshots.load(std::memory_order_relaxed);
		result.lostSnapshots = impl_->lostSnapshots.load(std::memory_order_relaxed);
		result.capturedFatalCapsules =
			impl_->capturedFatalCapsules.load(std::memory_order_relaxed);
		result.lostFatalCapsules = impl_->lostFatalCapsules.load(std::memory_order_relaxed);
		result.overhead.producerCalls = impl_->producerCalls.load(std::memory_order_relaxed);
		result.overhead.producerTimeNs = impl_->producerTimeNs.load(std::memory_order_relaxed);
		result.overhead.sourceRegistrationCalls =
			impl_->sourceRegistrationCalls.load(std::memory_order_relaxed);
		result.overhead.sourceRegistrationTimeNs =
			impl_->sourceRegistrationTimeNs.load(std::memory_order_relaxed);
		result.overhead.stackCaptureCalls =
			impl_->stackCaptureCalls.load(std::memory_order_relaxed);
		result.overhead.stackCaptureTimeNs =
			impl_->stackCaptureTimeNs.load(std::memory_order_relaxed);
		result.overhead.capturedStackFrames =
			impl_->capturedStackFrames.load(std::memory_order_relaxed);
		result.overhead.fatalCaptureCalls =
			impl_->fatalCaptureCalls.load(std::memory_order_relaxed);
		result.overhead.fatalCaptureTimeNs =
			impl_->fatalCaptureTimeNs.load(std::memory_order_relaxed);
	} catch (...) {
	}
	return result;
}

void DevErrorMonitoring::setConfig(const DevErrorConfig& config) noexcept {
	try {
		std::scoped_lock lock(impl_->mutex);
		const DevErrorConfig normalized = Impl::normalize(config);
		// Slab/ring capacities are construction-time bounds. Runtime updates may
		// alter capture detail and the non-owning stack backend, never resize a
		// producer-owned allocation on an error path.
		impl_->monitoringConfig.level = normalized.level;
		impl_->monitoringConfig.nativeTextLimit = normalized.nativeTextLimit;
		impl_->monitoringConfig.maximumStackFrames = normalized.maximumStackFrames;
		impl_->monitoringConfig.stackProvider = normalized.stackProvider;
		impl_->captureLevel.store(
			static_cast<uint8_t>(impl_->monitoringConfig.level), std::memory_order_relaxed);
		impl_->nativeTextLimit.store(
			impl_->monitoringConfig.nativeTextLimit, std::memory_order_relaxed);
		impl_->emergencyStackProvider.store(
			impl_->monitoringConfig.stackProvider
				? impl_->monitoringConfig.stackProvider : &impl_->platformStackProvider,
			std::memory_order_release);
	} catch (...) {
	}
}

DevErrorConfig DevErrorMonitoring::config() const noexcept {
	try {
		std::scoped_lock lock(impl_->mutex);
		return impl_->monitoringConfig;
	} catch (...) {
		return {};
	}
}

std::optional<DevErrorFatalCapsule> DevErrorMonitoring::fatalCapsuleSnapshot() const noexcept {
	if (impl_->fatalCapsuleState.load(std::memory_order_acquire) != 2u) return std::nullopt;
	return impl_->fatalCapsule;
}

void DevErrorMonitoring::publishFatalSafePoint(
	const DevErrorFatalSafePointSummary& summary) noexcept {
	impl_->safePointSequence.fetch_add(1u, std::memory_order_acq_rel);
	impl_->safePointRevision.store(summary.revision, std::memory_order_relaxed);
	impl_->safePointAppTick.store(summary.appTick, std::memory_order_relaxed);
	impl_->safePointTimingMutation.store(
		summary.timingMutationSequence, std::memory_order_relaxed);
	impl_->safePointTimingDropped.store(summary.timingDroppedRecords, std::memory_order_relaxed);
	impl_->safePointMemoryGeneration.store(summary.memoryGeneration, std::memory_order_relaxed);
	impl_->safePointMemoryMutation.store(
		summary.memoryMutationSequence, std::memory_order_relaxed);
	impl_->safePointMemoryDropped.store(summary.memoryDroppedOperations, std::memory_order_relaxed);
	impl_->safePointErrorRecorded.store(summary.errorRecordedEvents, std::memory_order_relaxed);
	impl_->safePointErrorDropped.store(summary.errorDroppedEvents, std::memory_order_relaxed);
	impl_->safePointSequence.fetch_add(1u, std::memory_order_release);
}

void recordGlobalDevDiagnostic(
	FlowUiError error,
	const DevErrorSourceDescriptor& source,
	std::string_view nativeText) noexcept {
	if (DevErrorMonitoring* monitoring = activeMonitoring.load(std::memory_order_acquire)) {
		(void)monitoring->recordRaised(
			error, source, DevErrorRecordFlag::DevOnlyDiagnostic, nativeText);
	}
}

void recordGlobalDevBreadcrumb(
	const DevErrorBreadcrumbDescriptor& descriptor,
	uint64_t primaryValue,
	uint64_t secondaryValue) noexcept {
	if (DevErrorMonitoring* monitoring = activeMonitoring.load(std::memory_order_acquire)) {
		monitoring->recordBreadcrumb(descriptor, primaryValue, secondaryValue);
	}
}

} // namespace FlowUi::devSystems

#endif
