#pragma once

#include "FlowUi/Public/BuildConfig.hpp"

#if FLOW_UI_DEV_MODE

#include <array>
#include <cstdint>
#include <span>
#include <string_view>
#include <type_traits>

#include "FlowUi/Public/errorManagement/Error.hpp"
#include "devSystems/devMonitoringAndReporting/timing/DevTimingTypes.hpp"

namespace FlowUi::devSystems {

/** Stable diagnostic occurrence identity; zero denotes no occurrence. */
using DevErrorOccurrenceId = uint64_t;
/** Monotonic chronological diagnostic record sequence. */
using DevErrorRecordSequence = uint64_t;
/** Identity of one retained deduplicated raw stack trace. */
using DevErrorStackId = uint64_t;
/** Identity of a registered deferred snapshot provider. */
using DevErrorSnapshotSourceId = uint64_t;

class DevErrorStackProvider;

/** Kind of causal step attached to one diagnostic occurrence. */
enum class DevErrorStepKind : uint8_t {
	/** Initial error raise. */
	Raised = 0,
	/** Error crossing a recorded API boundary. */
	Boundary,
	/** Recovery or operation attempt. */
	Attempt,
	/** Recorded recovery resolution. */
	Resolution,
	/** Consequent error linked to an earlier occurrence. */
	Consequence,
	/** Native backend diagnostic event. */
	BackendDiagnostic,
	/** Evidence captured before fatal termination. */
	FatalCapture,
};

/** Bitmask describing diagnostic provenance, truncation and evidence availability. */
enum class DevErrorRecordFlag : uint16_t {
	/** No flags or entity attribution. */
	None = 0u,
	/** Production error was explicitly reported by a caller. */
	ExternallyReported = 1u << 0u,
	/** Diagnostic is development-only and does not change production control flow. */
	DevOnlyDiagnostic = 1u << 1u,
	/** Bounded native diagnostic text is present. */
	NativeTextPresent = 1u << 2u,
	/** Native diagnostic text exceeded its bounded capacity. */
	NativeTextTruncated = 1u << 3u,
	/** Source metadata was unavailable. */
	SourceUnavailable = 1u << 4u,
	/** Earlier causal history was unavailable. */
	CausalHistoryUnavailable = 1u << 5u,
	/** Diagnostic was recorded during recursive reporting. */
	Recursive = 1u << 6u,
	/** Some causal steps were omitted by retention limits. */
	StepHistoryTruncated = 1u << 7u,
	/** Some breadcrumb history was omitted by retention limits. */
	BreadcrumbHistoryTruncated = 1u << 8u,
	/** Requested raw stack capture was unavailable. */
	StackUnavailable = 1u << 9u,
	/** Raw stack exceeded the capture frame capacity. */
	StackTruncated = 1u << 10u,
	/** Captured stack could not be retained. */
	StackStorageLost = 1u << 11u,
	/** Requested subsystem snapshot was unavailable. */
	SnapshotUnavailable = 1u << 12u,
	/** Captured subsystem revision differs from the requested revision. */
	SnapshotStale = 1u << 13u,
	/** Subsystem snapshot evidence exceeded bounded capacity. */
	SnapshotTruncated = 1u << 14u,
};

/** Combine capture or quality flags into a single bitmask. */
[[nodiscard]] constexpr DevErrorRecordFlag operator|(
	DevErrorRecordFlag left,
	DevErrorRecordFlag right) noexcept {
	return static_cast<DevErrorRecordFlag>(
		static_cast<uint16_t>(left) | static_cast<uint16_t>(right));
}

/** Add capture or quality flags to the left-hand bitmask and return it. */
constexpr DevErrorRecordFlag& operator|=(
	DevErrorRecordFlag& left,
	DevErrorRecordFlag right) noexcept {
	return left = left | right;
}

/** Borrowed source text and coordinates identifying a diagnostic site. */
struct DevErrorSourceLocation {
	/** Borrowed source file name; storage must outlive the descriptor registration. */
	std::string_view file{};
	/** Borrowed source function name; storage must outlive the descriptor registration. */
	std::string_view function{};
	/** Source line number; zero means unavailable. */
	uint32_t line = 0u;
	/** Source column number; zero means unavailable. */
	uint32_t column = 0u;
};

/**
 * Stable diagnostic source identity with borrowed name and location text; strings must outlive
 * monitoring registration.
 */
struct DevErrorSourceDescriptor {
	/** Stable identity of this retained entity; zero means unassigned. */
	uint64_t id = 0u;
	/** Display name for this source, track or descriptor. */
	std::string_view name{};
	/** Borrowed source location for this descriptor. */
	DevErrorSourceLocation location{};
};

/** Stable breadcrumb identity with a borrowed display name that outlives monitoring registration. */
struct DevErrorBreadcrumbDescriptor {
	/** Stable identity of this retained entity; zero means unassigned. */
	uint64_t id = 0u;
	/** Display name for this source, track or descriptor. */
	std::string_view name{};
};

namespace detail::dev_error {
inline constexpr uint64_t kHashOffset = 14695981039346656037ull;
inline constexpr uint64_t kHashPrime = 1099511628211ull;

[[nodiscard]] constexpr uint64_t hashBytes(
	std::string_view value,
	uint64_t hash = kHashOffset) noexcept {
	for (const char character : value) {
		hash ^= static_cast<uint64_t>(static_cast<unsigned char>(character));
		hash *= kHashPrime;
	}
	return hash;
}

[[nodiscard]] constexpr uint64_t hashInteger(uint64_t value, uint64_t hash) noexcept {
	for (uint32_t byte = 0u; byte < sizeof(value); ++byte) {
		hash ^= (value >> (byte * 8u)) & 0xffu;
		hash *= kHashPrime;
	}
	return hash;
}
} // namespace detail::dev_error

/**
 * Hash a diagnostic name, source file and line into a source descriptor. All returned string
 * views borrow caller storage.
 */
[[nodiscard]] constexpr DevErrorSourceDescriptor makeDevErrorSource(
	std::string_view name,
	const char* file = __builtin_FILE(),
	const char* function = __builtin_FUNCTION(),
	uint32_t line = __builtin_LINE(),
#if __has_builtin(__builtin_COLUMN)
	uint32_t column = __builtin_COLUMN()) noexcept {
#else
	uint32_t column = 0u) noexcept {
#endif
	uint64_t hash = detail::dev_error::hashBytes(name);
	hash = detail::dev_error::hashBytes(file, hash);
	hash = detail::dev_error::hashInteger(line, hash);
	return DevErrorSourceDescriptor{
		.id = hash,
		.name = name,
		.location = {
			.file = file,
			.function = function,
			.line = line,
			.column = column,
		},
	};
}

/** Hash a breadcrumb name into a stable descriptor; the name storage must outlive registration. */
[[nodiscard]] constexpr DevErrorBreadcrumbDescriptor makeDevErrorBreadcrumb(
	std::string_view name) noexcept {
	return DevErrorBreadcrumbDescriptor{
		.id = detail::dev_error::hashBytes(name),
		.name = name,
	};
}

/** Compact application, frame, submission and entity attribution copied into diagnostic records. */
struct DevErrorContext {
	/** Application tick associated with this evidence; zero means unassigned. */
	AppTickId appTick = 0u;
	/** Window and frame sequence associated with this evidence. */
	WindowFrameKey frame{};
	/** Renderer submission sequence associated with this evidence. */
	uint64_t submissionSerial = 0u;
	/** Primary entity identity copied from the diagnostic producer context. */
	uint64_t primaryEntity = 0u;
	/** Secondary entity identity copied from the diagnostic producer context. */
	uint64_t secondaryEntity = 0u;
	/** Timing producer track associated with the diagnostic context. */
	uint32_t timingTrack = 0u;
	/** Current application or capture lifecycle phase. */
	uint16_t phase = 0u;
};

/** Semantic interpretation of one compact numeric evidence value. */
enum class DevErrorEvidenceKey : uint16_t {
	/** No flags or entity attribution. */
	None = 0u,
	/** Subject evidence payload. */
	Subject,
	/** Auxiliary evidence payload. */
	Auxiliary,
	/** Native code evidence payload. */
	NativeCode,
	/** Recorded recovery resolution. */
	Resolution,
	/** Lifecycle phase evidence payload. */
	LifecyclePhase,
	/** Configured policy evidence payload. */
	ConfiguredPolicy,
	/** Capacity evidence payload. */
	Capacity,
	/** Count evidence payload. */
	Count,
	/** Revision evidence payload. */
	Revision,
	/** Ownership flags evidence payload. */
	OwnershipFlags,
	/** Custom evidence payload. */
	Custom,
};

/** Tagged immediate evidence value with an optional repeated-value index. */
struct DevErrorEvidenceValue {
	/** Semantic evidence tag or grouping identity for this value. */
	DevErrorEvidenceKey key = DevErrorEvidenceKey::None;
	/** Index distinguishing repeated values with the same evidence key. */
	uint16_t index = 0u;
	/** Exact numeric evidence payload interpreted by key. */
	uint64_t value = 0u;
};

/** Maximum number of immediate numeric evidence values in one block. */
inline constexpr size_t kDevErrorImmediateEvidenceCapacity = 8u;

/**
 * Fixed-capacity immediate evidence collection. Overflow preserves existing values and records
 * truncation.
 */
struct DevErrorEvidenceBlock {
	/** Fixed-capacity storage; only count or valueCount entries are populated. */
	std::array<DevErrorEvidenceValue, kDevErrorImmediateEvidenceCapacity> values{};
	/** Number of populated immediate evidence values. */
	uint8_t count = 0u;
	/** Whether more evidence was offered than the fixed capacity could retain. */
	bool truncated = false;

	/**
	 * Append one tagged numeric value. Return false and mark truncation when the fixed evidence
	 * capacity is exhausted.
	 */
	bool append(DevErrorEvidenceKey key, uint64_t value, uint16_t index = 0u) noexcept {
		if (count >= values.size()) {
			truncated = true;
			return false;
		}
		values[count++] = DevErrorEvidenceValue{.key = key, .index = index, .value = value};
		return true;
	}
};

/** Availability and truncation state of captured raw stack frames. */
enum class DevErrorStackStatus : uint8_t {
	/** Capture was not requested by the active detail level. */
	NotRequested = 0u,
	/** Requested evidence is present. */
	Available,
	/** Requested evidence could not be captured. */
	Unavailable,
	/** Evidence is present but some values exceeded bounded capacity. */
	Truncated,
	/** Evidence was captured but could not be retained. */
	StorageLost,
};

/** Maximum raw program counters retained in a diagnostic stack trace. */
inline constexpr size_t kDevErrorMaximumStackFrames = 48u;

/** Owned bounded raw-PC trace with module and build identity for later symbolization. */
struct DevErrorStackTrace {
	/** Stable identity of this retained entity; zero means unassigned. */
	DevErrorStackId id = 0u;
	/** Hash used to deduplicate raw stack contents. */
	uint64_t hash = 0u;
	/** Module identity needed to interpret captured program counters. */
	uint64_t moduleIdentity = 0u;
	/** Build identity needed to interpret captured program counters. */
	uint64_t buildIdentity = 0u;
	/** Availability or quality state of this capture result. */
	DevErrorStackStatus status = DevErrorStackStatus::NotRequested;
	/** Number of valid raw program counters in frames or the caller destination. */
	uint16_t frameCount = 0u;
	/** Owned raw program counter array; only frameCount entries are valid. */
	std::array<uintptr_t, kDevErrorMaximumStackFrames> frames{};
};

/** Availability and revision quality of a deferred subsystem snapshot. */
enum class DevErrorSnapshotOutcome : uint8_t {
	/** Requested evidence is present. */
	Available = 0u,
	/** Requested evidence could not be captured. */
	Unavailable,
	/** Provider observed a different subsystem revision than requested. */
	StaleRevision,
	/** Evidence is present but some values exceeded bounded capacity. */
	Truncated,
	/** Evidence was captured but could not be retained. */
	StorageLost,
};

/** Deferred request for one occurrence and snapshot source at an expected subsystem revision. */
struct DevErrorSnapshotRequest {
	/** Diagnostic occurrence identity; zero means no explicit occurrence. */
	DevErrorOccurrenceId occurrence = 0u;
	/** Snapshot source or evidence identity associated with this record. */
	DevErrorSnapshotSourceId source = 0u;
	/** Subsystem revision requested by the snapshot consumer. */
	uint64_t expectedRevision = 0u;
	/** Producer context copied when this evidence was captured. */
	DevErrorContext context{};
};

/** Maximum numeric values retained in one subsystem snapshot. */
inline constexpr size_t kDevErrorSnapshotValueCapacity = 24u;

/**
 * Owned bounded subsystem evidence captured at a safe point, with requested and observed revision
 * identities.
 */
struct DevErrorSnapshot {
	/** Diagnostic occurrence identity; zero means no explicit occurrence. */
	DevErrorOccurrenceId occurrence = 0u;
	/** Snapshot source or evidence identity associated with this record. */
	DevErrorSnapshotSourceId source = 0u;
	/** Subsystem revision requested by the snapshot consumer. */
	uint64_t expectedRevision = 0u;
	/** Subsystem revision actually observed by the snapshot provider. */
	uint64_t capturedRevision = 0u;
	/** Availability and revision result reported by the snapshot provider. */
	DevErrorSnapshotOutcome outcome = DevErrorSnapshotOutcome::Unavailable;
	/** Number of populated snapshot values. */
	uint8_t valueCount = 0u;
	/** Fixed-capacity storage; only count or valueCount entries are populated. */
	std::array<DevErrorEvidenceValue, kDevErrorSnapshotValueCapacity> values{};
};

/**
 * Borrowed writer into one bounded snapshot. The destination must outlive this sink and its
 * provider callback.
 */
class DevErrorSnapshotSink {
public:
	/** Bind a writer to the borrowed snapshot destination for the duration of provider capture. */
	explicit DevErrorSnapshotSink(DevErrorSnapshot& snapshot) noexcept : snapshot_(&snapshot) {}
	/**
	 * Append one tagged numeric value. Return false and mark truncation when the fixed evidence
	 * capacity is exhausted.
	 */
	bool append(DevErrorEvidenceKey key, uint64_t value, uint16_t index = 0u) noexcept {
		if (snapshot_->valueCount >= snapshot_->values.size()) {
			snapshot_->outcome = DevErrorSnapshotOutcome::Truncated;
			return false;
		}
		snapshot_->values[snapshot_->valueCount++] =
			DevErrorEvidenceValue{.key = key, .index = index, .value = value};
		return true;
	}
	/** Publish the actual subsystem revision observed by the snapshot callback. */
	void setCapturedRevision(uint64_t revision) noexcept { snapshot_->capturedRevision = revision; }
	/** Set the snapshot availability or revision outcome reported by the provider. */
	void setOutcome(DevErrorSnapshotOutcome outcome) noexcept { snapshot_->outcome = outcome; }

private:
	DevErrorSnapshot* snapshot_ = nullptr;
};

/**
 * Noexcept callback that fills a borrowed snapshot sink at a safe point; the opaque owner and
 * request remain valid only during the call.
 */
using DevErrorSnapshotCapture = bool (*)(
	const void*, const DevErrorSnapshotRequest&, DevErrorSnapshotSink&) noexcept;

/**
 * Borrowed subsystem snapshot callback and owner. Unregister the provider before destroying its
 * owner.
 */
struct DevErrorSnapshotProvider {
	/** Snapshot source or evidence identity associated with this record. */
	DevErrorSnapshotSourceId source = 0u;
	/** Borrowed callback owner; unregister this provider before destroying it. */
	const void* owner = nullptr;
	/** Borrowed noexcept snapshot callback; its owner must remain valid until unregistered. */
	DevErrorSnapshotCapture capture = nullptr;
};

/**
 * One owned chronological diagnostic step, including compact causal context and bounded native
 * text.
 */
struct DevErrorRecord {
	/** Monotonic capture sequence used to order retained records. */
	DevErrorRecordSequence sequence = 0u;
	/** Timestamp in the monotonic capture clock, in nanoseconds. */
	uint64_t timestampNs = 0u;
	/** Diagnostic occurrence identity; zero means no explicit occurrence. */
	DevErrorOccurrenceId occurrence = 0u;
	/** Preceding diagnostic occurrence recorded as the causal parent; zero means none. */
	DevErrorOccurrenceId cause = 0u;
	/** Production error contract and compact payload captured for this occurrence. */
	FlowUiError error{};
	/** Producer context copied when this evidence was captured. */
	DevErrorContext context{};
	/** Registered diagnostic source identity; zero means unavailable. */
	uint64_t sourceId = 0u;
	/** Retained raw stack identity; zero means no captured stack. */
	DevErrorStackId stackId = 0u;
	/** Beginning sequence of the associated breadcrumb range. */
	uint64_t breadcrumbBegin = 0u;
	/** Ending sequence of the associated breadcrumb range. */
	uint64_t breadcrumbEnd = 0u;
	/** Producer thread track identity associated with the evidence. */
	uint32_t threadTrack = 0u;
	/** Native backend category associated with native error evidence. */
	uint32_t nativeCategory = 0u;
	/** Semantic kind of this record or reference. */
	DevErrorStepKind kind = DevErrorStepKind::Raised;
	/** Production error event kind represented by this diagnostic step. */
	ErrorEventKind productionKind = ErrorEventKind::Reported;
	/** Resolution applied or reported for the error contract. */
	ErrorResolution resolution = ErrorResolution::None;
	/** Bitmask of completion, provenance and capture-quality conditions. */
	DevErrorRecordFlag flags = DevErrorRecordFlag::None;
	/** Number of valid bytes in nativeText. */
	uint16_t nativeTextLength = 0u;
	/** Bounded numeric evidence supporting this record or advice result. */
	DevErrorEvidenceBlock evidence{};
	/** Owned bounded native diagnostic bytes; nativeTextLength gives the valid prefix. */
	std::array<char, kDevErrorNativeTextCapacity> nativeText{};
};

/**
 * One bounded chronological breadcrumb linking numeric values to an optional diagnostic
 * occurrence.
 */
struct DevErrorBreadcrumb {
	/** Monotonic capture sequence used to order retained records. */
	uint64_t sequence = 0u;
	/** Timestamp in the monotonic capture clock, in nanoseconds. */
	uint64_t timestampNs = 0u;
	/** Registered breadcrumb or advice descriptor identity. */
	uint64_t descriptorId = 0u;
	/** Diagnostic occurrence identity; zero means no explicit occurrence. */
	DevErrorOccurrenceId occurrence = 0u;
	/** Producer context copied when this evidence was captured. */
	DevErrorContext context{};
	/** First exact numeric value attached to the breadcrumb. */
	uint64_t primaryValue = 0u;
	/** Second exact numeric value attached to the breadcrumb. */
	uint64_t secondaryValue = 0u;
	/** Producer thread track identity associated with the evidence. */
	uint32_t threadTrack = 0u;
};

/** Bitmask of evidence and runtime capabilities considered available during fatal capture. */
enum class DevErrorFatalCapabilityInput : uint32_t {
	/** No flags or entity attribution. */
	None = 0u,
	/** Emergency capsule is available or trusted for fatal capture. */
	EmergencyCapsule = 1u << 0u,
	/** Current context is available or trusted for fatal capture. */
	CurrentContext = 1u << 1u,
	/** Recent breadcrumbs is available or trusted for fatal capture. */
	RecentBreadcrumbs = 1u << 2u,
	/** Raw stack is available or trusted for fatal capture. */
	RawStack = 1u << 3u,
	/** Safe point summary is available or trusted for fatal capture. */
	SafePointSummary = 1u << 4u,
	/** Immutable snapshot is available or trusted for fatal capture. */
	ImmutableSnapshot = 1u << 5u,
	/** Diagnostic thread safe is available or trusted for fatal capture. */
	DiagnosticThreadSafe = 1u << 6u,
	/** Allocator trusted is available or trusted for fatal capture. */
	AllocatorTrusted = 1u << 7u,
	/** Subsystem locks trusted is available or trusted for fatal capture. */
	SubsystemLocksTrusted = 1u << 8u,
};

/** Combine capture or quality flags into a single bitmask. */
[[nodiscard]] constexpr DevErrorFatalCapabilityInput operator|(
	DevErrorFatalCapabilityInput left,
	DevErrorFatalCapabilityInput right) noexcept {
	return static_cast<DevErrorFatalCapabilityInput>(
		static_cast<uint32_t>(left) | static_cast<uint32_t>(right));
}

/** Add capture or quality flags to the left-hand bitmask and return it. */
constexpr DevErrorFatalCapabilityInput& operator|=(
	DevErrorFatalCapabilityInput& left,
	DevErrorFatalCapabilityInput right) noexcept {
	return left = left | right;
}

/** Return whether inputs contains any bit selected by input; None always returns false. */
[[nodiscard]] constexpr bool hasDevErrorFatalCapabilityInput(
	DevErrorFatalCapabilityInput inputs,
	DevErrorFatalCapabilityInput input) noexcept {
	return (static_cast<uint32_t>(inputs) & static_cast<uint32_t>(input)) != 0u;
}

/**
 * Last published allocation-free summary of monitoring state, reusable when live subsystem
 * inspection is unsafe.
 */
struct DevErrorFatalSafePointSummary {
	/** Revision identifying the observed metadata, report or safe-point state. */
	uint64_t revision = 0u;
	/** Application tick associated with this evidence; zero means unassigned. */
	AppTickId appTick = 0u;
	/** Last timing history mutation sequence observed at the safe point. */
	uint64_t timingMutationSequence = 0u;
	/** Timing records dropped as observed at the safe point. */
	uint64_t timingDroppedRecords = 0u;
	/** Memory history generation observed at the safe point. */
	uint64_t memoryGeneration = 0u;
	/** Memory operation sequence observed at the safe point. */
	uint64_t memoryMutationSequence = 0u;
	/** Dropped memory operations observed at the safe point. */
	uint64_t memoryDroppedOperations = 0u;
	/** Diagnostic events recorded as observed at the safe point. */
	uint64_t errorRecordedEvents = 0u;
	/** Diagnostic events dropped as observed at the safe point. */
	uint64_t errorDroppedEvents = 0u;
};

/** Compact breadcrumb representation stored inside a fixed fatal capsule. */
struct DevErrorFatalBreadcrumb {
	/** Monotonic capture sequence used to order retained records. */
	uint64_t sequence = 0u;
	/** Timestamp in the monotonic capture clock, in nanoseconds. */
	uint64_t timestampNs = 0u;
	/** Registered breadcrumb or advice descriptor identity. */
	uint64_t descriptorId = 0u;
	/** First exact numeric value attached to the breadcrumb. */
	uint64_t primaryValue = 0u;
	/** Second exact numeric value attached to the breadcrumb. */
	uint64_t secondaryValue = 0u;
	/** Diagnostic occurrence identity; zero means no explicit occurrence. */
	DevErrorOccurrenceId occurrence = 0u;
	/** Producer thread track identity associated with the evidence. */
	uint32_t threadTrack = 0u;
};

/** Version of the fixed fatal capsule representation. */
inline constexpr uint32_t kDevErrorFatalCapsuleLayoutVersion = 1u;
/** Maximum raw program counters retained in a fatal capsule. */
inline constexpr size_t kDevErrorFatalStackCapacity = 24u;
/** Maximum recent breadcrumbs retained in a fatal capsule. */
inline constexpr size_t kDevErrorFatalBreadcrumbCapacity = 8u;

/** Fixed, allocation-free evidence written before production termination. */
struct DevErrorFatalCapsule {
	/** Owned bounded breadcrumb evidence; only breadcrumbCount entries are valid for fatal capsules. */
	std::array<DevErrorFatalBreadcrumb, kDevErrorFatalBreadcrumbCapacity> breadcrumbs{};
	/** Fixed-capacity raw program counter storage; only stackFrameCount entries are valid. */
	std::array<uintptr_t, kDevErrorFatalStackCapacity> stackFrames{};
	/** Bounded numeric evidence supporting this record or advice result. */
	DevErrorEvidenceBlock evidence{};
	/** Most recently published summary safe to inspect without live subsystem access. */
	DevErrorFatalSafePointSummary safePoint{};
	/** Producer context copied when this evidence was captured. */
	DevErrorContext context{};
	/** Production error contract and compact payload captured for this occurrence. */
	FlowUiError error{};
	/** Diagnostic occurrence identity; zero means no explicit occurrence. */
	DevErrorOccurrenceId occurrence = 0u;
	/** Timestamp in the monotonic capture clock, in nanoseconds. */
	uint64_t timestampNs = 0u;
	/** Registered diagnostic source identity; zero means unavailable. */
	uint64_t sourceId = 0u;
	/** Module identity needed to interpret captured program counters. */
	uint64_t moduleIdentity = 0u;
	/** Build identity needed to interpret captured program counters. */
	uint64_t buildIdentity = 0u;
	/** Version of the fixed fatal capsule layout. */
	uint32_t layoutVersion = kDevErrorFatalCapsuleLayoutVersion;
	/** Size of the fatal capsule representation in bytes. */
	uint32_t byteSize = 0u;
	/** Producer thread track identity associated with the evidence. */
	uint32_t threadTrack = 0u;
	/** Native backend category associated with native error evidence. */
	uint32_t nativeCategory = 0u;
	/** Resolution applied or reported for the error contract. */
	ErrorResolution resolution = ErrorResolution::None;
	/** Production inspection capabilities reported at the fatal boundary. */
	FatalInspectionCapability productionInspection = FatalInspectionCapability::None;
	/** Effective diagnostic capture detail level. */
	DevErrorCaptureLevel captureLevel = DevErrorCaptureLevel::Disabled;
	/** Availability and truncation state of the fatal raw stack. */
	DevErrorStackStatus stackStatus = DevErrorStackStatus::NotRequested;
	/** Bitmask of completion, provenance and capture-quality conditions. */
	DevErrorRecordFlag flags = DevErrorRecordFlag::None;
	/** Bitmask of evidence and runtime inputs admitted for fatal capture. */
	DevErrorFatalCapabilityInput availableInputs = DevErrorFatalCapabilityInput::None;
	/** Number of valid entries in stackFrames. */
	uint16_t stackFrameCount = 0u;
	/** Number of valid bytes in nativeText. */
	uint16_t nativeTextLength = 0u;
	/** Owned bounded native diagnostic bytes; nativeTextLength gives the valid prefix. */
	std::array<char, kDevErrorNativeTextCapacity> nativeText{};
	/** Number of valid entries in breadcrumbs. */
	uint8_t breadcrumbCount = 0u;
};

/** Cumulative instrumentation costs and operation counts for diagnostic capture. */
struct DevErrorOverheadSnapshot {
	/** Number of producer calls. */
	uint64_t producerCalls = 0u;
	/** Cumulative producer time in nanoseconds. */
	uint64_t producerTimeNs = 0u;
	/** Bytes of recorded bytes. */
	uint64_t recordedBytes = 0u;
	/** Number of source registration calls. */
	uint64_t sourceRegistrationCalls = 0u;
	/** Cumulative source registration time in nanoseconds. */
	uint64_t sourceRegistrationTimeNs = 0u;
	/** Number of stack capture calls. */
	uint64_t stackCaptureCalls = 0u;
	/** Cumulative stack capture time in nanoseconds. */
	uint64_t stackCaptureTimeNs = 0u;
	/** Number of captured stack frames. */
	uint64_t capturedStackFrames = 0u;
	/** Number of fatal capture calls. */
	uint64_t fatalCaptureCalls = 0u;
	/** Cumulative fatal capture time in nanoseconds. */
	uint64_t fatalCaptureTimeNs = 0u;
};

/** Cumulative diagnostic capture, truncation and evidence-loss counters. */
struct DevErrorQualitySnapshot {
	/** Number of recorded events. */
	uint64_t recordedEvents = 0u;
	/** Number of recorded breadcrumbs. */
	uint64_t recordedBreadcrumbs = 0u;
	/** Number of suppressed events. */
	uint64_t suppressedEvents = 0u;
	/** Number of dropped events. */
	uint64_t droppedEvents = 0u;
	/** Number of dropped breadcrumbs. */
	uint64_t droppedBreadcrumbs = 0u;
	/** Number of overwritten breadcrumbs. */
	uint64_t overwrittenBreadcrumbs = 0u;
	/** Number of recursive events. */
	uint64_t recursiveEvents = 0u;
	/** Number of native text truncations. */
	uint64_t nativeTextTruncations = 0u;
	/** Number of dropped descriptors. */
	uint64_t droppedDescriptors = 0u;
	/** Number of captured stacks. */
	uint64_t capturedStacks = 0u;
	/** Number of deduplicated stacks. */
	uint64_t deduplicatedStacks = 0u;
	/** Number of unavailable stacks. */
	uint64_t unavailableStacks = 0u;
	/** Number of truncated stacks. */
	uint64_t truncatedStacks = 0u;
	/** Number of lost stacks. */
	uint64_t lostStacks = 0u;
	/** Number of requested snapshots. */
	uint64_t requestedSnapshots = 0u;
	/** Number of captured snapshots. */
	uint64_t capturedSnapshots = 0u;
	/** Number of stale snapshots. */
	uint64_t staleSnapshots = 0u;
	/** Number of unavailable snapshots. */
	uint64_t unavailableSnapshots = 0u;
	/** Number of truncated snapshots. */
	uint64_t truncatedSnapshots = 0u;
	/** Number of lost snapshots. */
	uint64_t lostSnapshots = 0u;
	/** Number of captured fatal capsules. */
	uint64_t capturedFatalCapsules = 0u;
	/** Number of lost fatal capsules. */
	uint64_t lostFatalCapsules = 0u;
	/** Cumulative instrumentation costs and operation counts. */
	DevErrorOverheadSnapshot overhead{};
};

static_assert(std::is_trivially_copyable_v<DevErrorFatalCapsule>);

/** Return the maximum diagnostic detail level compiled into this build. */
[[nodiscard]] constexpr DevErrorCaptureLevel compiledDevErrorLevel() noexcept {
#if FLOWUI_DEV_ERROR_LEVEL <= 0
	return DevErrorCaptureLevel::Disabled;
#elif FLOWUI_DEV_ERROR_LEVEL == 1
	return DevErrorCaptureLevel::Summary;
#elif FLOWUI_DEV_ERROR_LEVEL == 2
	return DevErrorCaptureLevel::Causal;
#elif FLOWUI_DEV_ERROR_LEVEL == 3
	return DevErrorCaptureLevel::StackAndState;
#else
	return DevErrorCaptureLevel::Deep;
#endif
}

} // namespace FlowUi::devSystems

#endif
