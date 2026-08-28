#pragma once

#include "FlowUi/BuildConfig.hpp"

#if FLOW_UI_DEV_MODE

#include <array>
#include <cstdint>
#include <span>
#include <string_view>
#include <type_traits>

#include "FlowUi/Error.hpp"
#include "devSystems/devMonitoringAndReporting/timing/DevTimingTypes.hpp"

namespace FlowUi::devSystems {

using DevErrorOccurrenceId = uint64_t;
using DevErrorRecordSequence = uint64_t;
using DevErrorStackId = uint64_t;
using DevErrorSnapshotSourceId = uint64_t;

class DevErrorStackProvider;

enum class DevErrorStepKind : uint8_t {
	Raised = 0,
	Boundary,
	Attempt,
	Resolution,
	Consequence,
	BackendDiagnostic,
	FatalCapture,
};

enum class DevErrorRecordFlag : uint16_t {
	None = 0u,
	ExternallyReported = 1u << 0u,
	DevOnlyDiagnostic = 1u << 1u,
	NativeTextPresent = 1u << 2u,
	NativeTextTruncated = 1u << 3u,
	SourceUnavailable = 1u << 4u,
	CausalHistoryUnavailable = 1u << 5u,
	Recursive = 1u << 6u,
	StepHistoryTruncated = 1u << 7u,
	BreadcrumbHistoryTruncated = 1u << 8u,
	StackUnavailable = 1u << 9u,
	StackTruncated = 1u << 10u,
	StackStorageLost = 1u << 11u,
	SnapshotUnavailable = 1u << 12u,
	SnapshotStale = 1u << 13u,
	SnapshotTruncated = 1u << 14u,
};

[[nodiscard]] constexpr DevErrorRecordFlag operator|(
	DevErrorRecordFlag left,
	DevErrorRecordFlag right) noexcept {
	return static_cast<DevErrorRecordFlag>(
		static_cast<uint16_t>(left) | static_cast<uint16_t>(right));
}

constexpr DevErrorRecordFlag& operator|=(
	DevErrorRecordFlag& left,
	DevErrorRecordFlag right) noexcept {
	return left = left | right;
}

struct DevErrorSourceLocation {
	std::string_view file{};
	std::string_view function{};
	uint32_t line = 0u;
	uint32_t column = 0u;
};

struct DevErrorSourceDescriptor {
	uint64_t id = 0u;
	std::string_view name{};
	DevErrorSourceLocation location{};
};

struct DevErrorBreadcrumbDescriptor {
	uint64_t id = 0u;
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

[[nodiscard]] constexpr DevErrorSourceDescriptor makeDevErrorSource(
	std::string_view name,
	const char* file = __builtin_FILE(),
	const char* function = __builtin_FUNCTION(),
	uint32_t line = __builtin_LINE(),
	uint32_t column = __builtin_COLUMN()) noexcept {
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

[[nodiscard]] constexpr DevErrorBreadcrumbDescriptor makeDevErrorBreadcrumb(
	std::string_view name) noexcept {
	return DevErrorBreadcrumbDescriptor{
		.id = detail::dev_error::hashBytes(name),
		.name = name,
	};
}

struct DevErrorContext {
	AppTickId appTick = 0u;
	WindowFrameKey frame{};
	uint64_t submissionSerial = 0u;
	uint64_t primaryEntity = 0u;
	uint64_t secondaryEntity = 0u;
	uint32_t timingTrack = 0u;
	uint16_t phase = 0u;
};

enum class DevErrorEvidenceKey : uint16_t {
	None = 0u,
	Subject,
	Auxiliary,
	NativeCode,
	Resolution,
	LifecyclePhase,
	ConfiguredPolicy,
	Capacity,
	Count,
	Revision,
	OwnershipFlags,
	Custom,
};

struct DevErrorEvidenceValue {
	DevErrorEvidenceKey key = DevErrorEvidenceKey::None;
	uint16_t index = 0u;
	uint64_t value = 0u;
};

inline constexpr size_t kDevErrorImmediateEvidenceCapacity = 8u;

struct DevErrorEvidenceBlock {
	std::array<DevErrorEvidenceValue, kDevErrorImmediateEvidenceCapacity> values{};
	uint8_t count = 0u;
	bool truncated = false;

	bool append(DevErrorEvidenceKey key, uint64_t value, uint16_t index = 0u) noexcept {
		if (count >= values.size()) {
			truncated = true;
			return false;
		}
		values[count++] = DevErrorEvidenceValue{.key = key, .index = index, .value = value};
		return true;
	}
};

enum class DevErrorStackStatus : uint8_t {
	NotRequested = 0u,
	Available,
	Unavailable,
	Truncated,
	StorageLost,
};

inline constexpr size_t kDevErrorMaximumStackFrames = 48u;

struct DevErrorStackTrace {
	DevErrorStackId id = 0u;
	uint64_t hash = 0u;
	uint64_t moduleIdentity = 0u;
	uint64_t buildIdentity = 0u;
	DevErrorStackStatus status = DevErrorStackStatus::NotRequested;
	uint16_t frameCount = 0u;
	std::array<uintptr_t, kDevErrorMaximumStackFrames> frames{};
};

enum class DevErrorSnapshotOutcome : uint8_t {
	Available = 0u,
	Unavailable,
	StaleRevision,
	Truncated,
	StorageLost,
};

struct DevErrorSnapshotRequest {
	DevErrorOccurrenceId occurrence = 0u;
	DevErrorSnapshotSourceId source = 0u;
	uint64_t expectedRevision = 0u;
	DevErrorContext context{};
};

inline constexpr size_t kDevErrorSnapshotValueCapacity = 24u;

struct DevErrorSnapshot {
	DevErrorOccurrenceId occurrence = 0u;
	DevErrorSnapshotSourceId source = 0u;
	uint64_t expectedRevision = 0u;
	uint64_t capturedRevision = 0u;
	DevErrorSnapshotOutcome outcome = DevErrorSnapshotOutcome::Unavailable;
	uint8_t valueCount = 0u;
	std::array<DevErrorEvidenceValue, kDevErrorSnapshotValueCapacity> values{};
};

class DevErrorSnapshotSink {
public:
	explicit DevErrorSnapshotSink(DevErrorSnapshot& snapshot) noexcept : snapshot_(&snapshot) {}
	bool append(DevErrorEvidenceKey key, uint64_t value, uint16_t index = 0u) noexcept {
		if (snapshot_->valueCount >= snapshot_->values.size()) {
			snapshot_->outcome = DevErrorSnapshotOutcome::Truncated;
			return false;
		}
		snapshot_->values[snapshot_->valueCount++] =
			DevErrorEvidenceValue{.key = key, .index = index, .value = value};
		return true;
	}
	void setCapturedRevision(uint64_t revision) noexcept { snapshot_->capturedRevision = revision; }
	void setOutcome(DevErrorSnapshotOutcome outcome) noexcept { snapshot_->outcome = outcome; }

private:
	DevErrorSnapshot* snapshot_ = nullptr;
};

using DevErrorSnapshotCapture = bool (*)(
	const void*, const DevErrorSnapshotRequest&, DevErrorSnapshotSink&) noexcept;

struct DevErrorSnapshotProvider {
	DevErrorSnapshotSourceId source = 0u;
	const void* owner = nullptr;
	DevErrorSnapshotCapture capture = nullptr;
};

struct DevErrorRecord {
	DevErrorRecordSequence sequence = 0u;
	uint64_t timestampNs = 0u;
	DevErrorOccurrenceId occurrence = 0u;
	DevErrorOccurrenceId cause = 0u;
	FlowUiError error{};
	DevErrorContext context{};
	uint64_t sourceId = 0u;
	DevErrorStackId stackId = 0u;
	uint64_t breadcrumbBegin = 0u;
	uint64_t breadcrumbEnd = 0u;
	uint32_t threadTrack = 0u;
	uint32_t nativeCategory = 0u;
	DevErrorStepKind kind = DevErrorStepKind::Raised;
	ErrorEventKind productionKind = ErrorEventKind::Reported;
	ErrorResolution resolution = ErrorResolution::None;
	DevErrorRecordFlag flags = DevErrorRecordFlag::None;
	uint16_t nativeTextLength = 0u;
	DevErrorEvidenceBlock evidence{};
	std::array<char, kDevErrorNativeTextCapacity> nativeText{};
};

struct DevErrorBreadcrumb {
	uint64_t sequence = 0u;
	uint64_t timestampNs = 0u;
	uint64_t descriptorId = 0u;
	DevErrorOccurrenceId occurrence = 0u;
	DevErrorContext context{};
	uint64_t primaryValue = 0u;
	uint64_t secondaryValue = 0u;
	uint32_t threadTrack = 0u;
};

enum class DevErrorFatalCapabilityInput : uint32_t {
	None = 0u,
	EmergencyCapsule = 1u << 0u,
	CurrentContext = 1u << 1u,
	RecentBreadcrumbs = 1u << 2u,
	RawStack = 1u << 3u,
	SafePointSummary = 1u << 4u,
	ImmutableSnapshot = 1u << 5u,
	DiagnosticThreadSafe = 1u << 6u,
	AllocatorTrusted = 1u << 7u,
	SubsystemLocksTrusted = 1u << 8u,
};

[[nodiscard]] constexpr DevErrorFatalCapabilityInput operator|(
	DevErrorFatalCapabilityInput left,
	DevErrorFatalCapabilityInput right) noexcept {
	return static_cast<DevErrorFatalCapabilityInput>(
		static_cast<uint32_t>(left) | static_cast<uint32_t>(right));
}

constexpr DevErrorFatalCapabilityInput& operator|=(
	DevErrorFatalCapabilityInput& left,
	DevErrorFatalCapabilityInput right) noexcept {
	return left = left | right;
}

[[nodiscard]] constexpr bool hasDevErrorFatalCapabilityInput(
	DevErrorFatalCapabilityInput inputs,
	DevErrorFatalCapabilityInput input) noexcept {
	return (static_cast<uint32_t>(inputs) & static_cast<uint32_t>(input)) != 0u;
}

struct DevErrorFatalSafePointSummary {
	uint64_t revision = 0u;
	AppTickId appTick = 0u;
	uint64_t timingMutationSequence = 0u;
	uint64_t timingDroppedRecords = 0u;
	uint64_t memoryGeneration = 0u;
	uint64_t memoryMutationSequence = 0u;
	uint64_t memoryDroppedOperations = 0u;
	uint64_t errorRecordedEvents = 0u;
	uint64_t errorDroppedEvents = 0u;
};

struct DevErrorFatalBreadcrumb {
	uint64_t sequence = 0u;
	uint64_t timestampNs = 0u;
	uint64_t descriptorId = 0u;
	uint64_t primaryValue = 0u;
	uint64_t secondaryValue = 0u;
	DevErrorOccurrenceId occurrence = 0u;
	uint32_t threadTrack = 0u;
};

inline constexpr uint32_t kDevErrorFatalCapsuleLayoutVersion = 1u;
inline constexpr size_t kDevErrorFatalStackCapacity = 24u;
inline constexpr size_t kDevErrorFatalBreadcrumbCapacity = 8u;

/** Fixed, allocation-free evidence written before production termination. */
struct DevErrorFatalCapsule {
	uint32_t layoutVersion = kDevErrorFatalCapsuleLayoutVersion;
	uint32_t byteSize = 0u;
	FlowUiError error{};
	DevErrorOccurrenceId occurrence = 0u;
	uint64_t timestampNs = 0u;
	DevErrorContext context{};
	uint64_t sourceId = 0u;
	uint64_t moduleIdentity = 0u;
	uint64_t buildIdentity = 0u;
	uint32_t threadTrack = 0u;
	uint32_t nativeCategory = 0u;
	ErrorResolution resolution = ErrorResolution::None;
	FatalInspectionCapability productionInspection = FatalInspectionCapability::None;
	DevErrorCaptureLevel captureLevel = DevErrorCaptureLevel::Disabled;
	DevErrorStackStatus stackStatus = DevErrorStackStatus::NotRequested;
	DevErrorRecordFlag flags = DevErrorRecordFlag::None;
	DevErrorFatalCapabilityInput availableInputs = DevErrorFatalCapabilityInput::None;
	uint16_t stackFrameCount = 0u;
	uint8_t breadcrumbCount = 0u;
	uint16_t nativeTextLength = 0u;
	DevErrorEvidenceBlock evidence{};
	DevErrorFatalSafePointSummary safePoint{};
	std::array<uintptr_t, kDevErrorFatalStackCapacity> stackFrames{};
	std::array<DevErrorFatalBreadcrumb, kDevErrorFatalBreadcrumbCapacity> breadcrumbs{};
	std::array<char, kDevErrorNativeTextCapacity> nativeText{};
};

struct DevErrorOverheadSnapshot {
	uint64_t producerCalls = 0u;
	uint64_t producerTimeNs = 0u;
	uint64_t recordedBytes = 0u;
	uint64_t sourceRegistrationCalls = 0u;
	uint64_t sourceRegistrationTimeNs = 0u;
	uint64_t stackCaptureCalls = 0u;
	uint64_t stackCaptureTimeNs = 0u;
	uint64_t capturedStackFrames = 0u;
	uint64_t fatalCaptureCalls = 0u;
	uint64_t fatalCaptureTimeNs = 0u;
};

struct DevErrorQualitySnapshot {
	uint64_t recordedEvents = 0u;
	uint64_t recordedBreadcrumbs = 0u;
	uint64_t suppressedEvents = 0u;
	uint64_t droppedEvents = 0u;
	uint64_t droppedBreadcrumbs = 0u;
	uint64_t overwrittenBreadcrumbs = 0u;
	uint64_t recursiveEvents = 0u;
	uint64_t nativeTextTruncations = 0u;
	uint64_t droppedDescriptors = 0u;
	uint64_t capturedStacks = 0u;
	uint64_t deduplicatedStacks = 0u;
	uint64_t unavailableStacks = 0u;
	uint64_t truncatedStacks = 0u;
	uint64_t lostStacks = 0u;
	uint64_t requestedSnapshots = 0u;
	uint64_t capturedSnapshots = 0u;
	uint64_t staleSnapshots = 0u;
	uint64_t unavailableSnapshots = 0u;
	uint64_t truncatedSnapshots = 0u;
	uint64_t lostSnapshots = 0u;
	uint64_t capturedFatalCapsules = 0u;
	uint64_t lostFatalCapsules = 0u;
	DevErrorOverheadSnapshot overhead{};
};

static_assert(std::is_trivially_copyable_v<DevErrorFatalCapsule>);

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
