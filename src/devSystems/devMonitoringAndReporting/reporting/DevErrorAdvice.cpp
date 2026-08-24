#include "devSystems/devMonitoringAndReporting/reporting/DevErrorAdvice.hpp"

#if FLOW_UI_DEV_MODE

#include <algorithm>

#include "devSystems/devMonitoringAndReporting/reporting/DevErrorReporting.hpp"

namespace FlowUi::devSystems {
namespace {

constexpr bool hasFlag(DevErrorRecordFlag value, DevErrorRecordFlag flag) noexcept {
	return (static_cast<uint16_t>(value) & static_cast<uint16_t>(flag)) != 0u;
}

void appendEvidence(
	DevErrorAdviceResult& result,
	DevErrorAdviceEvidenceKind kind,
	uint64_t primary = 0u,
	uint64_t secondary = 0u) noexcept {
	if (result.evidenceCount >= result.evidence.size()) return;
	result.evidence[result.evidenceCount++] = DevErrorAdviceEvidenceRef{
		.kind = kind,
		.primary = primary,
		.secondary = secondary,
	};
}

bool evaluatePredicate(
	const DevErrorOccurrence& occurrence,
	const DevErrorAdviceDescriptor& descriptor,
	DevErrorAdviceResult& result) noexcept {
	switch (descriptor.predicate) {
	case DevErrorAdvicePredicate::Always:
		return true;
	case DevErrorAdvicePredicate::MemoryPressureObserved:
		if ((occurrence.memory.state != DevErrorCorrelationState::Available &&
			 occurrence.memory.state != DevErrorCorrelationState::Ambiguous) ||
			(occurrence.memory.eventSequence == 0u && occurrence.memory.checkpointTick == 0u) ||
			(occurrence.memory.operation != MemoryOperation::LogicalAllocate &&
			 occurrence.memory.operation != MemoryOperation::BackingGrow &&
			 occurrence.memory.operation != MemoryOperation::ContainerReserve &&
			 occurrence.memory.operation != MemoryOperation::ContainerReallocate &&
			 occurrence.memory.operation != MemoryOperation::ContainerRehash &&
			 occurrence.memory.operation != MemoryOperation::GpuCreate &&
			 occurrence.memory.operation != MemoryOperation::ExternalAcquire)) {
			return false;
		}
		if (occurrence.memory.eventSequence != 0u) {
			appendEvidence(result, DevErrorAdviceEvidenceKind::MemoryEvent,
				occurrence.memory.eventSequence, occurrence.memory.source);
		}
		if (occurrence.memory.checkpointTick != 0u) {
			appendEvidence(result, DevErrorAdviceEvidenceKind::MemoryCheckpoint,
				occurrence.memory.checkpointTick, occurrence.memory.storageMutationSequence);
		}
		result.confidence = occurrence.memory.state == DevErrorCorrelationState::Available &&
			(occurrence.memory.confidence == DevErrorCorrelationConfidence::Explicit ||
			 occurrence.memory.confidence == DevErrorCorrelationConfidence::SharedContext)
			? DevErrorAdviceConfidence::Strong
			: DevErrorAdviceConfidence::Possible;
		return true;
	case DevErrorAdvicePredicate::FrameContextAvailable:
		for (const DevErrorRecord& step : occurrence.steps) {
			if (!step.context.frame) continue;
			appendEvidence(result, DevErrorAdviceEvidenceKind::Frame,
				step.context.frame.window, step.context.frame.frameNumber);
			if (occurrence.timing.containingZone != 0u) {
				appendEvidence(result, DevErrorAdviceEvidenceKind::TimingZone,
					occurrence.timing.containingInvocation, occurrence.timing.containingZone);
			}
			return true;
		}
		return false;
	case DevErrorAdvicePredicate::NativeCodeAvailable:
		if (occurrence.error.nativeCode == 0u) return false;
		appendEvidence(result, DevErrorAdviceEvidenceKind::NativeCode,
			occurrence.error.nativeCode);
		return true;
	case DevErrorAdvicePredicate::FallbackApplied:
		switch (occurrence.resolution) {
		case ErrorResolution::Retried:
		case ErrorResolution::UsedFallback:
		case ErrorResolution::Skipped:
		case ErrorResolution::GrewCapacity:
		case ErrorResolution::EvictedAndRetried:
		case ErrorResolution::RecreatedWindow:
			appendEvidence(result, DevErrorAdviceEvidenceKind::Resolution,
				static_cast<uint64_t>(occurrence.resolution));
			return true;
		default: return false;
		}
	case DevErrorAdvicePredicate::OperationRejected:
		if (occurrence.resolution != ErrorResolution::Rejected &&
			occurrence.resolution != ErrorResolution::CanceledOperation &&
			occurrence.resolution != ErrorResolution::CanceledFrame) return false;
		appendEvidence(result, DevErrorAdviceEvidenceKind::Resolution,
			static_cast<uint64_t>(occurrence.resolution));
		return true;
	case DevErrorAdvicePredicate::SnapshotAvailable:
		for (const DevErrorSnapshot& snapshot : occurrence.snapshots) {
			if (snapshot.outcome != DevErrorSnapshotOutcome::Available) continue;
			appendEvidence(result, DevErrorAdviceEvidenceKind::Snapshot,
				snapshot.source, snapshot.capturedRevision);
			return true;
		}
		return false;
	case DevErrorAdvicePredicate::CaptureIncomplete: {
		constexpr DevErrorRecordFlag incompleteFlags[] = {
			DevErrorRecordFlag::CausalHistoryUnavailable,
			DevErrorRecordFlag::StepHistoryTruncated,
			DevErrorRecordFlag::BreadcrumbHistoryTruncated,
			DevErrorRecordFlag::StackUnavailable,
			DevErrorRecordFlag::StackTruncated,
			DevErrorRecordFlag::StackStorageLost,
			DevErrorRecordFlag::SnapshotUnavailable,
			DevErrorRecordFlag::SnapshotStale,
			DevErrorRecordFlag::SnapshotTruncated,
		};
		uint64_t flagBits = 0u;
		for (const DevErrorRecordFlag flag : incompleteFlags) {
			if (hasFlag(occurrence.flags, flag)) flagBits |= static_cast<uint16_t>(flag);
		}
		const bool correlationLost =
			occurrence.timing.state == DevErrorCorrelationState::Evicted ||
			occurrence.memory.state == DevErrorCorrelationState::Evicted;
		if (flagBits == 0u && !correlationLost) return false;
		appendEvidence(result, DevErrorAdviceEvidenceKind::CaptureQuality, flagBits,
			correlationLost ? 1u : 0u);
		return true;
	}
	}
	return false;
}

} // namespace

std::vector<DevErrorAdviceResult> evaluateDevErrorAdvice(
	const DevErrorOccurrence& occurrence,
	uint32_t maximumResults) {
	std::vector<DevErrorAdviceResult> results;
	if (maximumResults == 0u || occurrence.error.code == ErrorCode::None) return results;
	results.reserve(std::min<size_t>(maximumResults, devErrorAdviceCatalogue().size()));

	for (const DevErrorAdviceDescriptor& descriptor : devErrorAdviceCatalogue()) {
		if (descriptor.code != ErrorCode::None && descriptor.code != occurrence.error.code) continue;
		if (descriptor.site != ErrorSite::None && descriptor.site != occurrence.error.site) continue;
		DevErrorAdviceResult result{
			.descriptorId = descriptor.id,
			.category = descriptor.category,
			.confidence = descriptor.baseConfidence,
			.priority = descriptor.priority,
			.sourceId = occurrence.steps.empty() ? 0u : occurrence.steps.front().sourceId,
			.title = descriptor.title,
			.explanation = descriptor.explanation,
			.suggestedAction = descriptor.suggestedAction,
			.configurationKey = descriptor.configurationKey,
			.documentation = descriptor.documentation,
			.limitation = descriptor.limitation,
		};
		appendEvidence(result, DevErrorAdviceEvidenceKind::Contract,
			static_cast<uint64_t>(occurrence.error.code),
			static_cast<uint64_t>(occurrence.error.site));
		if (occurrence.error.subject != 0u) {
			appendEvidence(result, DevErrorAdviceEvidenceKind::Subject,
				occurrence.error.subject);
		}
		if (occurrence.error.auxiliary != 0u) {
			appendEvidence(result, DevErrorAdviceEvidenceKind::Auxiliary,
				occurrence.error.auxiliary);
		}
		if (!evaluatePredicate(occurrence, descriptor, result)) continue;
		results.push_back(result);
	}

	std::sort(results.begin(), results.end(), [](const auto& left, const auto& right) {
		if (left.priority != right.priority) return left.priority > right.priority;
		if (left.confidence != right.confidence) return left.confidence < right.confidence;
		return left.descriptorId < right.descriptorId;
	});
	if (results.size() > maximumResults) results.resize(maximumResults);
	return results;
}

} // namespace FlowUi::devSystems

#endif
