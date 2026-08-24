#include <algorithm>
#include <chrono>
#include <iostream>
#include <string_view>

#include "FlowUi/Error.hpp"
#include "devSystems/devMonitoringAndReporting/errors/DevError.hpp"
#include "devSystems/devMonitoringAndReporting/errors/DevErrorStackProvider.hpp"
#include "devSystems/devMonitoringAndReporting/memory/DevMemory.hpp"
#include "devSystems/devMonitoringAndReporting/reporting/DevMemoryReporting.hpp"
#include "devSystems/devMonitoringAndReporting/reporting/DevTimingReporting.hpp"
#include "devSystems/devMonitoringAndReporting/timing/DevGpuTiming.hpp"
#include "devSystems/devMonitoringAndReporting/timing/DevTiming.hpp"
#include "devSystems/devMonitoringAndReporting/reporting/DevErrorReporting.hpp"

namespace {

bool expect(bool condition, std::string_view message) {
	if (condition) return true;
	std::cerr << "Dev error monitoring test failed: " << message << '\n';
	return false;
}

class TestStackProvider final : public FlowUi::devSystems::DevErrorStackProvider {
public:
	enum class Mode { Available, Truncated, Unavailable };
	Mode mode = Mode::Available;
	uintptr_t base = 0x1000u;

	FlowUi::devSystems::DevErrorRawStackCapture capture(
		std::span<uintptr_t> destination,
		uint32_t) noexcept override {
		using namespace FlowUi::devSystems;
		if (mode == Mode::Unavailable) return {};
		if (!destination.empty()) destination[0] = base;
		if (destination.size() > 1u) destination[1] = base + 0x10u;
		return DevErrorRawStackCapture{
			.status = mode == Mode::Truncated
				? DevErrorStackStatus::Truncated : DevErrorStackStatus::Available,
			.frameCount = static_cast<uint16_t>(std::min<size_t>(2u, destination.size())),
			.moduleIdentity = 41u,
			.buildIdentity = 42u,
		};
	}

	FlowUi::devSystems::DevErrorRawStackCapture captureEmergency(
		std::span<uintptr_t> destination,
		uint32_t framesToSkip) noexcept override {
		return capture(destination, framesToSkip);
	}
};

struct SnapshotState {
	uint64_t revision = 0u;
};

bool captureSnapshot(
	const void* owner,
	const FlowUi::devSystems::DevErrorSnapshotRequest& request,
	FlowUi::devSystems::DevErrorSnapshotSink& sink) noexcept {
	using namespace FlowUi::devSystems;
	const auto& state = *static_cast<const SnapshotState*>(owner);
	sink.setCapturedRevision(state.revision);
	if (request.expectedRevision != state.revision) {
		sink.setOutcome(DevErrorSnapshotOutcome::StaleRevision);
		return false;
	}
	const size_t count = request.context.phase != 0u
		? kDevErrorSnapshotValueCapacity + 1u : 2u;
	for (size_t index = 0u; index < count; ++index) {
		(void)sink.append(DevErrorEvidenceKey::Custom, index, static_cast<uint16_t>(index));
	}
	return true;
}

} // namespace

int main() {
	using namespace FlowUi;
	using namespace FlowUi::devSystems;

	DevErrorMonitoring monitoring(DevErrorConfig{
		.level = DevErrorCaptureLevel::Causal,
		.producerRecordCapacity = 32u,
		.breadcrumbCapacity = 16u,
		.recentBreadcrumbCount = 4u,
		.nativeTextLimit = 12u,
	});
	auto attachment = monitoring.attachCurrentThread("test.platform");
	attachment.recorder().setContext(DevErrorContext{
		.appTick = 7u,
		.frame = WindowFrameKey{4u, 12u},
		.primaryEntity = 4u,
		.timingTrack = 3u,
	});

	static constexpr auto breadcrumb = makeDevErrorBreadcrumb("test.operation.begin");
	static constexpr auto source = makeDevErrorSource("test.capacity.raise");
	monitoring.recordBreadcrumb(breadcrumb, 11u, 12u);
	const FlowUiError error = makeError(
		ErrorCode::StorageCapacityExceeded,
		ErrorSite::ResourceAllocatePersistent,
		99u,
		100u,
		17u);
	const DevErrorOccurrenceId occurrence = monitoring.recordRaised(
		error, source, DevErrorRecordFlag::None, "native-text-that-is-long");
	monitoring.recordStep(
		occurrence, DevErrorStepKind::Attempt, error, ErrorResolution::None, source);
	monitoring.recordProductionEvent(
		occurrence,
		ErrorEventView{
			.error = error,
			.kind = ErrorEventKind::Resolved,
			.resolution = ErrorResolution::EvictedAndRetried,
		},
		source);

	DevErrorReporting reporting(monitoring, DevErrorReportingConfig{
		.retainedOccurrenceCapacity = 8u,
		.retainedByteBudget = 64u * 1024u,
		.retainedBreadcrumbCapacity = 16u,
		.postOccurrenceTicks = 2u,
		.maxStepsPerOccurrence = 8u,
	});
	reporting.consumeThrough(7u);

	bool passed = true;
	const auto retained = reporting.occurrence(occurrence);
	passed &= expect(retained.has_value(), "explicit occurrence was retained");
	if (retained) {
		passed &= expect(retained->steps.size() == 3u, "raised/attempt/resolution joined");
		passed &= expect(retained->breadcrumbs.size() == 2u, "breadcrumb range was rebuilt");
		passed &= expect(
			retained->resolution == ErrorResolution::EvictedAndRetried,
			"production resolution was retained");
		passed &= expect(
			retained->state == DevErrorOccurrenceState::PostWindow,
			"resolved occurrence entered post window");
		passed &= expect(
			retained->steps.front().nativeTextLength == 12u,
			"native text obeyed configured bound");
		const auto fallback = std::find_if(
			retained->advice.begin(), retained->advice.end(),
			[](const auto& result) { return result.descriptorId == 0x1801u; });
		passed &= expect(fallback != retained->advice.end() &&
			fallback->title == "FlowUi applied a fallback or skip policy",
			"configured fallback resolution produced catalogue advice");
	}
	passed &= expect(retained && reporting.advice(occurrence).size() == retained->advice.size(),
		"advice query exposes the occurrence's immutable results");

	const auto groups = reporting.groupSnapshot(DevErrorGroupMode::Native);
	passed &= expect(groups.size() == 1u && groups.front().occurrenceCount == 1u,
		"native grouping produced one group");
	const auto range = reporting.occurrenceRange(DevErrorOccurrenceQuery{
		.code = ErrorCode::StorageCapacityExceeded,
		.window = WindowId{4u},
	});
	passed &= expect(range.size() == 1u, "code/window query found occurrence");

	reporting.consumeThrough(9u);
	const auto closed = reporting.occurrence(occurrence);
	passed &= expect(
		closed && closed->state == DevErrorOccurrenceState::Closed,
		"post window closed at configured tick");

	monitoring.observeProductionEvent(ErrorEventView{
		.error = error,
		.kind = ErrorEventKind::Reported,
	});
	reporting.consumeThrough(10u);
	passed &= expect(
		reporting.groupSnapshot().front().occurrenceCount == 2u,
		"contract grouping counts repeated errors");
	passed &= expect(!monitoring.sourceSnapshot().empty(), "source descriptor registered");
	passed &= expect(
		reporting.status().quality.nativeTextTruncations == 1u,
		"native text truncation is visible in quality status");

	reporting.setConfig(DevErrorReportingConfig{
		.retainedOccurrenceCapacity = 1u,
		.retainedByteBudget = 64u * 1024u,
	});
	passed &= expect(
		reporting.status().retainedOccurrences == 1u &&
			reporting.status().evictedOccurrences >= 1u,
		"count budget evicts oldest occurrences");

	TestStackProvider stackProvider;
	DevErrorMonitoring deepMonitoring(DevErrorConfig{
		.level = DevErrorCaptureLevel::Deep,
		.producerRecordCapacity = 32u,
		.breadcrumbCapacity = 8u,
		.recentBreadcrumbCount = 2u,
		.nativeTextLimit = 16u,
		.threadRecorderCapacity = 2u,
		.sourceDescriptorCapacity = 8u,
		.breadcrumbDescriptorCapacity = 8u,
		.stackTraceCapacity = 2u,
		.maximumStackFrames = 4u,
		.snapshotProviderCapacity = 2u,
		.pendingSnapshotCapacity = 4u,
		.retainedSnapshotCapacity = 4u,
		.stackProvider = &stackProvider,
	});
	auto deepAttachment = deepMonitoring.attachCurrentThread("test.deep");
	deepAttachment.recorder().setContext(DevErrorContext{.appTick = 20u});
	const DevErrorOccurrenceId firstStackOccurrence =
		deepMonitoring.recordRaised(error, source);
	const DevErrorOccurrenceId duplicateStackOccurrence =
		deepMonitoring.recordRaised(error, source);
	stackProvider.mode = TestStackProvider::Mode::Truncated;
	stackProvider.base = 0x2000u;
	const DevErrorOccurrenceId truncatedStackOccurrence =
		deepMonitoring.recordRaised(error, source);
	stackProvider.mode = TestStackProvider::Mode::Unavailable;
	const DevErrorOccurrenceId unavailableStackOccurrence =
		deepMonitoring.recordRaised(error, source);
	stackProvider.mode = TestStackProvider::Mode::Available;
	stackProvider.base = 0x3000u;
	const DevErrorOccurrenceId lostStackOccurrence =
		deepMonitoring.recordRaised(error, source);

	SnapshotState snapshotState{.revision = 5u};
	constexpr DevErrorSnapshotSourceId snapshotSource = 77u;
	passed &= expect(deepMonitoring.registerSnapshotProvider(DevErrorSnapshotProvider{
		.source = snapshotSource,
		.owner = &snapshotState,
		.capture = &captureSnapshot,
	}), "snapshot provider registered");
	passed &= expect(deepMonitoring.requestSnapshot(DevErrorSnapshotRequest{
		.occurrence = firstStackOccurrence,
		.source = snapshotSource,
		.expectedRevision = 5u,
	}), "available snapshot requested");
	passed &= expect(deepMonitoring.requestSnapshot(DevErrorSnapshotRequest{
		.occurrence = duplicateStackOccurrence,
		.source = snapshotSource,
		.expectedRevision = 4u,
	}), "stale snapshot requested");
	passed &= expect(deepMonitoring.requestSnapshot(DevErrorSnapshotRequest{
		.occurrence = truncatedStackOccurrence,
		.source = snapshotSource,
		.expectedRevision = 5u,
		.context = DevErrorContext{.phase = 1u},
	}), "truncated snapshot requested");
	passed &= expect(deepMonitoring.requestSnapshot(DevErrorSnapshotRequest{
		.occurrence = unavailableStackOccurrence,
		.source = 88u,
		.expectedRevision = 1u,
	}), "unavailable-provider snapshot requested");
	passed &= expect(!deepMonitoring.requestSnapshot(DevErrorSnapshotRequest{
		.occurrence = lostStackOccurrence,
		.source = snapshotSource,
		.expectedRevision = 5u,
	}), "snapshot request storage loss was bounded");

	DevErrorReporting deepReporting(deepMonitoring);
	deepReporting.consumeThrough(20u);
	const auto firstDeep = deepReporting.occurrence(firstStackOccurrence);
	const auto duplicateDeep = deepReporting.occurrence(duplicateStackOccurrence);
	const auto truncatedDeep = deepReporting.occurrence(truncatedStackOccurrence);
	const auto unavailableDeep = deepReporting.occurrence(unavailableStackOccurrence);
	const auto lostDeep = deepReporting.occurrence(lostStackOccurrence);
	passed &= expect(firstDeep && duplicateDeep &&
		firstDeep->steps.front().stackId == duplicateDeep->steps.front().stackId,
		"identical raw stacks deduplicated");
	passed &= expect(deepReporting.stackSnapshot().size() == 2u,
		"stack slab retained its configured capacity");
	passed &= expect(truncatedDeep && truncatedDeep->steps.front().stackId != 0u,
		"truncated stack remained usable");
	passed &= expect(unavailableDeep && unavailableDeep->steps.front().stackId == 0u,
		"unsupported stack capture remained non-fatal");
	passed &= expect(unavailableDeep && std::any_of(
		unavailableDeep->advice.begin(), unavailableDeep->advice.end(),
		[](const auto& result) { return result.descriptorId == 0x1802u; }),
		"incomplete capture produced explicit capture-quality advice");
	passed &= expect(lostDeep && lostDeep->steps.front().stackId == 0u,
		"stack storage loss remained non-fatal");
	passed &= expect(firstDeep && firstDeep->snapshots.size() == 1u &&
		firstDeep->snapshots.front().outcome == DevErrorSnapshotOutcome::Available,
		"safe-point snapshot captured");
	passed &= expect(duplicateDeep && duplicateDeep->snapshots.front().outcome ==
		DevErrorSnapshotOutcome::StaleRevision,
		"stale revision was explicit");
	passed &= expect(truncatedDeep && truncatedDeep->snapshots.front().outcome ==
		DevErrorSnapshotOutcome::Truncated,
		"snapshot truncation was explicit");
	passed &= expect(unavailableDeep && unavailableDeep->snapshots.front().outcome ==
		DevErrorSnapshotOutcome::Unavailable,
		"missing snapshot provider was explicit");
	passed &= expect(deepReporting.status().quality.lostStacks == 1u,
		"stack storage loss was counted");
	passed &= expect(deepReporting.status().quality.lostSnapshots >= 1u,
		"snapshot storage loss was counted");

	DevTiming timing;
	DevGpuTiming gpuTiming(timing);
	DevTimingReporting timingReporting(timing, gpuTiming);
	timingReporting.setConfig(TimingReportingConfig{
		.retainedAppTickCapacity = 1u,
		.minimumFramesInFlightMultiplier = 1u,
		.rollingSampleCapacity = 32u,
	});
	auto timingAttachment = timing.attachCurrentThread("test.correlation");
	const WindowFrameKey correlationFrame{9u, 3u};
	timingAttachment.recorder().setFrameContext(correlationFrame, 30u);
	const ActiveZoneToken timingToken = timingAttachment.recorder().tryBegin(
		timing_zones::kWindowFrameTotal, TimingEntityRef::window(9u));

	DevMemory memory(DevMemoryConfig{
		.level = MemoryMonitoringLevel::SubsystemCapacity,
		.producerEventCapacity = 32u,
		.gpuMemory = false,
		.processMemory = false,
	});
	DevMemoryReporting memoryReporting(memory, MemoryReportingConfig{
		.segmentCapacity = 32u,
		.eventByteCapacity = sizeof(RetainedMemoryEvent),
		.managerSampleEveryTicks = 8u,
		.quantileWindowSegments = 32u,
		.retainLifetimeEvents = true,
	});
	memory.recorder().setAppTickContext(30u);
	const uint64_t memoryTimestamp = static_cast<uint64_t>(
		std::chrono::duration_cast<std::chrono::nanoseconds>(
			std::chrono::steady_clock::now().time_since_epoch()).count());
	passed &= expect(memory.recorder().tryRecord(MemoryOperationRecord{
		.source = 122u,
		.operation = MemoryOperation::BackingGrow,
		.detailLevel = MemoryMonitoringLevel::StorageSummary,
		.appTick = 29u,
		.frame = WindowFrameKey{9u, 2u},
		.timestampNs = memoryTimestamp - 1u,
		.bytesBefore = 32u,
		.bytesAfter = 64u,
		.bytesChanged = 32u,
	}), "older memory correlation event recorded");
	passed &= expect(memory.recorder().tryRecord(MemoryOperationRecord{
		.source = 123u,
		.operation = MemoryOperation::BackingGrow,
		.detailLevel = MemoryMonitoringLevel::StorageSummary,
		.appTick = 30u,
		.frame = correlationFrame,
		.timestampNs = memoryTimestamp,
		.bytesBefore = 64u,
		.bytesAfter = 128u,
		.bytesChanged = 64u,
	}), "memory correlation event recorded");

	DevErrorMonitoring correlatedMonitoring(DevErrorConfig{
		.level = DevErrorCaptureLevel::Causal,
		.producerRecordCapacity = 16u,
		.breadcrumbCapacity = 8u,
	});
	auto correlatedAttachment = correlatedMonitoring.attachCurrentThread("test.error.correlation");
	correlatedAttachment.recorder().setContext(DevErrorContext{
		.appTick = 30u,
		.frame = correlationFrame,
		.primaryEntity = 9u,
		.timingTrack = timingAttachment.recorder().trackId(),
	});
	const DevErrorOccurrenceId correlatedOccurrence =
		correlatedMonitoring.recordRaised(error, source);
	timingAttachment.recorder().end(timingToken);
	timingReporting.consumeThrough(30u);
	memoryReporting.consume(30u);
	correlatedAttachment.recorder().setContext(DevErrorContext{
		.appTick = 29u,
		.frame = WindowFrameKey{9u, 2u},
		.primaryEntity = 9u,
		.timingTrack = timingAttachment.recorder().trackId(),
	});
	const DevErrorOccurrenceId evictedCorrelationOccurrence =
		correlatedMonitoring.recordRaised(error, source);

	DevErrorReporting correlatedReporting(
		correlatedMonitoring,
		DevErrorReportingConfig{
			.retainedOccurrenceCapacity = 8u,
			.retainedByteBudget = 256u * 1024u,
			.retainedBreadcrumbCapacity = 16u,
			.postOccurrenceTicks = 1u,
			.maxStepsPerOccurrence = 8u,
			.retainedCaptureCapacity = 2u,
			.maximumPinnedTimingTicks = 4u,
			.maximumPinnedMemoryEvents = 8u,
			.triggers = {DevErrorTrigger{
				.code = ErrorCode::StorageCapacityExceeded,
				.site = ErrorSite::ResourceAllocatePersistent,
				.preOccurrenceTicks = 0u,
				.postOccurrenceTicks = 1u,
			}},
		},
		&timingReporting,
		&memoryReporting);
	correlatedReporting.consumeThrough(30u);
	const auto correlated = correlatedReporting.occurrence(correlatedOccurrence);
	passed &= expect(correlated &&
		correlated->timing.state == DevErrorCorrelationState::Available &&
		correlated->timing.confidence == DevErrorCorrelationConfidence::SharedContext &&
		correlated->timing.containingZone != 0u,
		"containing timing zone was correlated by shared context");
	passed &= expect(correlated &&
		correlated->memory.state == DevErrorCorrelationState::Available &&
		correlated->memory.confidence == DevErrorCorrelationConfidence::SharedContext &&
		correlated->memory.eventSequence != 0u,
		"memory event was correlated by frame context");
	passed &= expect(correlated && !correlated->advice.empty() &&
		correlated->advice.front().descriptorId == 0x1001u &&
		correlated->advice.front().confidence == DevErrorAdviceConfidence::Strong,
		"site-specific memory evidence ranked above code-wide advice");
	if (correlated && !correlated->advice.empty()) {
		const DevErrorAdviceResult& result = correlated->advice.front();
		passed &= expect(
			result.title == "Captured memory growth preceded the capacity failure" &&
			result.suggestedAction ==
				"Inspect the pinned memory event and checkpoint before changing the configured budget or retention policy.",
			"advice wording matches the reviewed catalogue snapshot");
		passed &= expect(std::any_of(
			result.evidence.begin(), result.evidence.begin() + result.evidenceCount,
			[](const auto& evidence) {
				return evidence.kind == DevErrorAdviceEvidenceKind::MemoryEvent;
			}), "ranked advice cites the memory event it used");
	}
	passed &= expect(correlated && correlated->captureId != 0u,
		"matching code/site trigger opened umbrella capture");
	const DevErrorCaptureId correlatedCaptureId = correlated ? correlated->captureId : 0u;
	const auto activeCapture = correlatedReporting.capture(correlatedCaptureId);
	passed &= expect(activeCapture && activeCapture->timingTicks.size() == 1u &&
		activeCapture->memoryEvents.size() == 1u,
		"trigger pinned bounded timing and memory history");
	const auto evictedCorrelation =
		correlatedReporting.occurrence(evictedCorrelationOccurrence);
	passed &= expect(evictedCorrelation &&
		evictedCorrelation->timing.state == DevErrorCorrelationState::Evicted &&
		evictedCorrelation->memory.state == DevErrorCorrelationState::Evicted,
		"evicted timing and memory history was explicit");

	timingReporting.consumeThrough(31u);
	memoryReporting.consume(31u);
	correlatedReporting.consumeThrough(31u);
	const auto completedCapture = correlatedReporting.capture(correlatedCaptureId);
	passed &= expect(completedCapture &&
		completedCapture->state == DevErrorTriggeredCaptureState::Complete,
		"triggered post range closed at its configured tick");

	const auto catalogue = correlatedReporting.adviceCatalogue();
	std::vector<DevErrorAdviceId> adviceIds;
	for (const DevErrorAdviceDescriptor& descriptor : catalogue) {
		passed &= expect(descriptor.id != 0u &&
			std::find(adviceIds.begin(), adviceIds.end(), descriptor.id) == adviceIds.end(),
			"advice descriptor IDs are stable and unique");
		adviceIds.push_back(descriptor.id);
		passed &= expect(!descriptor.title.empty() && !descriptor.explanation.empty() &&
			!descriptor.suggestedAction.empty() && !descriptor.documentation.empty() &&
			!descriptor.limitation.empty(),
			"every advice descriptor has complete human-facing catalogue text");
	}
	DevErrorOccurrence releaseEvidence{
		.id = 999u,
		.error = error,
		.memory = DevErrorMemoryCorrelation{
			.state = DevErrorCorrelationState::Available,
			.confidence = DevErrorCorrelationConfidence::SharedContext,
			.eventSequence = 55u,
			.operation = MemoryOperation::LogicalRelease,
		},
	};
	const auto releaseAdvice = evaluateDevErrorAdvice(releaseEvidence, 4u);
	passed &= expect(std::none_of(
		releaseAdvice.begin(), releaseAdvice.end(),
		[](const auto& result) { return result.descriptorId == 0x1001u; }),
		"release evidence does not masquerade as memory pressure");
	const auto expectCoveredRange = [&](ErrorCode first, ErrorCode last) {
		for (uint16_t code = static_cast<uint16_t>(first);
			code <= static_cast<uint16_t>(last); ++code) {
			passed &= expect(devErrorAdviceCatalogueCovers(static_cast<ErrorCode>(code)),
				"every ErrorCode has guidance or an explicit no-guidance annotation");
		}
	};
	expectCoveredRange(ErrorCode::InternalInvariantBroken, ErrorCode::ForeignExceptionObserved);
	expectCoveredRange(ErrorCode::AppUnavailable, ErrorCode::AppTickSpaceExhausted);
	expectCoveredRange(ErrorCode::PlatformInitializationFailed, ErrorCode::PlatformInputQueueOverflow);
	expectCoveredRange(ErrorCode::InvalidWindowId, ErrorCode::WindowRecreationFailed);
	expectCoveredRange(ErrorCode::FrameAlreadyActive, ErrorCode::FrameCapacityExceeded);
	expectCoveredRange(ErrorCode::VulkanVersionUnsupported, ErrorCode::PresentIdSpaceExhausted);
	expectCoveredRange(ErrorCode::ShaderUnavailable, ErrorCode::RendererNativeResourceInvalid);
	expectCoveredRange(ErrorCode::StorageConfigurationInvalid, ErrorCode::StorageGenerationExhausted);
	expectCoveredRange(ErrorCode::InvalidResourceKey, ErrorCode::ResourceGenerationExhausted);
	expectCoveredRange(ErrorCode::AssetPathEmpty, ErrorCode::AssetDecodeFailed);
	expectCoveredRange(ErrorCode::FontFamilyAlreadyExists, ErrorCode::DefaultFontUnavailable);
	expectCoveredRange(ErrorCode::ImageDecodeFailed, ErrorCode::ImagePublicationFailed);
	expectCoveredRange(ErrorCode::IconSourceInvalid, ErrorCode::IconKeyCollision);
	expectCoveredRange(ErrorCode::InvalidElementId, ErrorCode::ElementStorageStale);
	expectCoveredRange(ErrorCode::InvalidActionId, ErrorCode::ActionPublicationConflict);
	expectCoveredRange(ErrorCode::ThemeTypeNotFound, ErrorCode::ThemeMutationFailed);
	expectCoveredRange(ErrorCode::InputFieldUnavailable, ErrorCode::InputSizeLimitExceeded);
	expectCoveredRange(ErrorCode::PopupFrameInactive, ErrorCode::PopupCapacityExceeded);
	expectCoveredRange(ErrorCode::ShortcutInvalid, ErrorCode::ShortcutNotFound);
	expectCoveredRange(ErrorCode::ViewportDetached, ErrorCode::ViewportRecordingFailed);

	for (uint8_t levelValue = static_cast<uint8_t>(DevErrorCaptureLevel::Disabled);
		levelValue <= static_cast<uint8_t>(DevErrorCaptureLevel::Deep); ++levelValue) {
		TestStackProvider levelStackProvider;
		DevErrorMonitoring levelMonitoring(DevErrorConfig{
			.level = static_cast<DevErrorCaptureLevel>(levelValue),
			.producerRecordCapacity = 1u,
			.breadcrumbCapacity = 2u,
			.stackTraceCapacity = 2u,
			.maximumStackFrames = 4u,
			.stackProvider = &levelStackProvider,
		});
		auto levelAttachment = levelMonitoring.attachCurrentThread("test.capture.level");
		levelAttachment.recorder().setContext(DevErrorContext{.appTick = 50u});
		levelMonitoring.recordProductionEvent(0u, ErrorEventView{
			.error = error,
			.kind = ErrorEventKind::Reported,
		}, source);
		levelMonitoring.recordProductionEvent(0u, ErrorEventView{
			.error = error,
			.kind = ErrorEventKind::Reported,
		}, source);
		const auto levelRecords = levelMonitoring.drainRecords();
		const auto levelBreadcrumbs = levelMonitoring.drainBreadcrumbs();
		const DevErrorQualitySnapshot levelQuality = levelMonitoring.qualitySnapshot();
		const uint64_t expectedProducerOperations =
			levelValue >= static_cast<uint8_t>(DevErrorCaptureLevel::Causal) ? 4u : 2u;
		passed &= expect(levelQuality.overhead.producerCalls == expectedProducerOperations,
			"producer overhead counts record and breadcrumb work at every capture level");
		if (levelValue == static_cast<uint8_t>(DevErrorCaptureLevel::Disabled)) {
			passed &= expect(levelRecords.empty() && levelBreadcrumbs.empty() &&
				levelQuality.suppressedEvents == 2u,
				"disabled level measures suppression without retaining sidecar data");
			continue;
		}
		passed &= expect(levelRecords.size() == 1u && levelQuality.droppedEvents == 1u &&
			levelQuality.overhead.recordedBytes >= sizeof(DevErrorRecord),
			"bounded saturation and retained bytes are measured at enabled levels");
		if (levelValue == static_cast<uint8_t>(DevErrorCaptureLevel::Summary)) {
			passed &= expect(levelBreadcrumbs.empty() && levelRecords.front().stackId == 0u,
				"summary level avoids causal and stack evidence");
		} else {
			passed &= expect(!levelBreadcrumbs.empty(),
				"causal and deeper levels retain semantic breadcrumbs");
		}
		if (levelValue >= static_cast<uint8_t>(DevErrorCaptureLevel::StackAndState)) {
			passed &= expect(levelRecords.front().stackId != 0u &&
				levelQuality.overhead.stackCaptureCalls == 2u &&
				levelQuality.overhead.capturedStackFrames != 0u,
				"stack levels measure raw-stack capture work even when record storage fills");
		}

		DevErrorReporting levelReporting(levelMonitoring);
		levelReporting.consumeThrough(50u);
		const auto levelReportingStatus = levelReporting.status();
		passed &= expect(levelReportingStatus.overhead.consumeCalls == 1u,
			"reporting overhead counts safe-point consumption at every enabled level");
	}

	TestStackProvider fatalStackProvider;
	DevErrorMonitoring fatalMonitoring(DevErrorConfig{
		.level = DevErrorCaptureLevel::Deep,
		.producerRecordCapacity = 1u,
		.breadcrumbCapacity = 2u,
		.stackTraceCapacity = 1u,
		.maximumStackFrames = 4u,
		.stackProvider = &fatalStackProvider,
	});
	auto fatalAttachment = fatalMonitoring.attachCurrentThread("test.fatal");
	fatalAttachment.recorder().setContext(DevErrorContext{
		.appTick = 60u,
		.frame = WindowFrameKey{11u, 7u},
		.submissionSerial = 91u,
		.primaryEntity = 44u,
	});
	fatalMonitoring.recordBreadcrumb(breadcrumb, 201u, 202u);
	fatalMonitoring.publishFatalSafePoint(DevErrorFatalSafePointSummary{
		.revision = 3u,
		.appTick = 59u,
		.timingMutationSequence = 12u,
		.timingDroppedRecords = 1u,
		.memoryGeneration = 5u,
		.memoryMutationSequence = 19u,
		.memoryDroppedOperations = 2u,
		.errorRecordedEvents = 8u,
		.errorDroppedEvents = 1u,
	});
	const FlowUiError fatalError = makeError(
		ErrorCode::InternalInvariantBroken, ErrorSite::AppPollEvents, 77u, 88u, 99u);
	fatalMonitoring.recordProductionEvent(0u, ErrorEventView{
		.error = fatalError,
		.kind = ErrorEventKind::Fatal,
		.resolution = ErrorResolution::Terminated,
		.inspection = FatalInspectionCapability::TerminalOnly,
		.nativeMessage = "fatal-native-evidence",
	}, source);
	const auto fatalCapsule = fatalMonitoring.fatalCapsuleSnapshot();
	passed &= expect(fatalCapsule &&
		fatalCapsule->layoutVersion == kDevErrorFatalCapsuleLayoutVersion &&
		fatalCapsule->byteSize == sizeof(DevErrorFatalCapsule) &&
		fatalCapsule->error.code == ErrorCode::InternalInvariantBroken &&
		fatalCapsule->context.appTick == 60u && fatalCapsule->context.frame.window == 11u,
		"fatal capsule preserves its versioned fixed record and lock-free context");
	passed &= expect(fatalCapsule && fatalCapsule->safePoint.revision == 3u &&
		fatalCapsule->breadcrumbCount == 1u && fatalCapsule->stackFrameCount != 0u &&
		hasDevErrorFatalCapabilityInput(
			fatalCapsule->availableInputs, DevErrorFatalCapabilityInput::SafePointSummary) &&
		hasDevErrorFatalCapabilityInput(
			fatalCapsule->availableInputs, DevErrorFatalCapabilityInput::RecentBreadcrumbs) &&
		hasDevErrorFatalCapabilityInput(
			fatalCapsule->availableInputs, DevErrorFatalCapabilityInput::RawStack),
		"fatal capsule contains only prepublished history and emergency-safe raw evidence");
	passed &= expect(fatalCapsule &&
		fatalCapsule->productionInspection == FatalInspectionCapability::TerminalOnly &&
		!hasDevErrorFatalCapabilityInput(
			fatalCapsule->availableInputs, DevErrorFatalCapabilityInput::DiagnosticThreadSafe) &&
		!hasDevErrorFatalCapabilityInput(
			fatalCapsule->availableInputs, DevErrorFatalCapabilityInput::AllocatorTrusted) &&
		!hasDevErrorFatalCapabilityInput(
			fatalCapsule->availableInputs, DevErrorFatalCapabilityInput::SubsystemLocksTrusted),
		"freeze trust inputs remain explicitly unavailable and continuation stays prohibited");
	passed &= expect(fatalMonitoring.drainRecords().empty(),
		"fatal capture bypasses ordinary lock-taking recorder storage");
	fatalMonitoring.recordProductionEvent(0u, ErrorEventView{
		.error = makeError(ErrorCode::StorageFrameProtocolViolation, ErrorSite::StorageValidateFrame),
		.kind = ErrorEventKind::Fatal,
		.resolution = ErrorResolution::Terminated,
		.inspection = FatalInspectionCapability::TerminalOnly,
	});
	const auto preservedFatalCapsule = fatalMonitoring.fatalCapsuleSnapshot();
	const auto fatalQuality = fatalMonitoring.qualitySnapshot();
	passed &= expect(preservedFatalCapsule &&
		preservedFatalCapsule->error.code == ErrorCode::InternalInvariantBroken &&
		fatalQuality.capturedFatalCapsules == 1u && fatalQuality.lostFatalCapsules == 1u &&
		fatalQuality.overhead.fatalCaptureCalls == 2u,
		"first fatal capsule wins and later fatal loss remains measurable");
	DevErrorReporting fatalReporting(fatalMonitoring);
	passed &= expect(fatalReporting.fatalCapsule().has_value(),
		"reporting exposes the capsule without draining or mutating it");
	passed &= expect(sizeof(DevErrorFatalCapsule) <= 2048u,
		"fatal capsule remains a small fixed emergency record");

	return passed ? 0 : 1;
}
