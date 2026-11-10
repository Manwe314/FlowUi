#pragma once

#include "FlowUi/Public/BuildConfig.hpp"

#if FLOW_UI_DEV_MODE

#include <cstdint>
#include <string>
#include <string_view>

#include "FlowUi/Public/identificationTypes/ElementID.hpp"
#include "devSystems/DevConfig.hpp"
#include "FlowUi/Public/identificationTypes/WindowId.hpp"

namespace FlowUi::devSystems {

/** Capture category used for filtering and attributing timing records. */
enum class TimingCategory : uint8_t {
	/** Lifecycle capture category. */
	Lifecycle = 0,
	/** Frame capture category. */
	Frame,
	/** Input capture category. */
	Input,
	/** Element capture category. */
	Element,
	/** Layout capture category. */
	Layout,
	/** Prepare capture category. */
	Prepare,
	/** Renderer cpu capture category. */
	RendererCpu,
	/** Gpu capture category. */
	Gpu,
	/** Wait capture category. */
	Wait,
	/** User capture category. */
	User,
	/** Dev tool capture category. */
	DevTool,
	/** Number of timing categories; not a capture category. */
	Count,
};

/** Interpretation of a zone as active work, waiting, a gap or development overhead. */
enum class TimingZoneRole : uint8_t {
	/** Active CPU work. */
	Work = 0,
	/** CPU waiting time. */
	Wait,
	/** Uninstrumented interval between phases. */
	Gap,
	/** GPU command execution. */
	GpuWork,
	/** Development tooling overhead. */
	DevToolWork,
};

/** Kind of application entity referenced by a timing record. */
enum class TimingEntityKind : uint8_t {
	/** No flags or entity attribution. */
	None = 0,
	/** App entity attribution. */
	App,
	/** Window entity attribution. */
	Window,
	/** Element definition entity attribution. */
	ElementDefinition,
	/** Element instance entity attribution. */
	ElementInstance,
	/** Viewport entity attribution. */
	Viewport,
	/** Action entity attribution. */
	Action,
	/** Resource entity attribution. */
	Resource,
	/** Submission entity attribution. */
	Submission,
};

/** Bitmask describing completion and capture-quality conditions of CPU records. */
enum class TimingRecordFlag : uint16_t {
	/** No flags or entity attribution. */
	None = 0,
	/** Measurement completed normally. */
	Completed = 1u << 0u,
	/** Operation was cancelled before normal completion. */
	Canceled = 1u << 1u,
	/** Frame operation encountered an out-of-date surface. */
	OutOfDate = 1u << 2u,
	/** Window was minimized during the measured operation. */
	Minimized = 1u << 3u,
	/** Scope ended while an exception was active. */
	Exception = 1u << 4u,
	/** Expected measurement completion was unavailable. */
	Incomplete = 1u << 5u,
	/** Clock ordering or duration was anomalous. */
	ClockAnomaly = 1u << 6u,
	/** Capture capacity omitted some nested detail. */
	DetailTruncated = 1u << 7u,
};

/** Combine capture or quality flags into a single bitmask. */
[[nodiscard]] constexpr TimingRecordFlag operator|(
	TimingRecordFlag left,
	TimingRecordFlag right) noexcept {
	return static_cast<TimingRecordFlag>(
		static_cast<uint16_t>(left) | static_cast<uint16_t>(right));
}

/** Return the underlying integer representation of CPU timing flags. */
[[nodiscard]] constexpr uint16_t timingRecordFlags(TimingRecordFlag flag) noexcept {
	return static_cast<uint16_t>(flag);
}

/** Stable 64-bit timing descriptor identity. */
using TimingZoneTypeId = uint64_t;
/** 64-bit identity of one admitted CPU timing invocation. */
using TimingInvocationId = uint64_t;
/** 32-bit identity of an attached CPU producer track. */
using TimingTrackId = uint32_t;
/** Monotonic application polling tick identity; zero is unassigned. */
using AppTickId = uint64_t;

/** Window identity and nonzero frame sequence used to join captured evidence. */
struct WindowFrameKey {
	/** Window identity; InvalidWindowId means no window attribution. */
	WindowId window = InvalidWindowId;
	/** Nonzero frame sequence within the owning window. */
	uint64_t frameNumber = 0u;

	/** Return whether this frame, token or thread attachment is valid and active. */
	[[nodiscard]] constexpr explicit operator bool() const noexcept {
		return window != InvalidWindowId && frameNumber != 0u;
	}

	/** Compare identity fields by value. */
	auto operator<=>(const WindowFrameKey&) const = default;
};

/**
 * Compact tagged reference to an application entity; the kind defines the meaning of both numeric
 * identities.
 */
struct TimingEntityRef {
	/** Semantic kind of this record or reference. */
	TimingEntityKind kind = TimingEntityKind::None;
	/** Primary entity identity interpreted according to kind. */
	uint64_t primaryId = 0u;
	/** Secondary identity; element instances use their definition ID here. */
	uint64_t secondaryId = 0u;

	/** Create a timing entity reference for the supplied window identity. */
	[[nodiscard]] static constexpr TimingEntityRef window(WindowId id) noexcept {
		return TimingEntityRef{
			.kind = TimingEntityKind::Window,
			.primaryId = static_cast<uint64_t>(id),
		};
	}

	/** Create a timing entity reference for the supplied element definition identity. */
	[[nodiscard]] static constexpr TimingEntityRef definition(FlowDefinitionID id) noexcept {
		return TimingEntityRef{
			.kind = TimingEntityKind::ElementDefinition,
			.primaryId = id.value,
		};
	}

	/**
	 * Create a timing entity reference with the instance identity primary and definition identity
	 * secondary.
	 */
	[[nodiscard]] static constexpr TimingEntityRef element(
		FlowDefinitionID definitionId,
		FlowElementID instanceId) noexcept {
		return TimingEntityRef{
			.kind = TimingEntityKind::ElementInstance,
			.primaryId = instanceId.value,
			.secondaryId = definitionId.value,
		};
	}
};

/**
 * Borrowed source metadata for a timing descriptor. File and function storage must outlive
 * registered descriptors.
 */
struct TimingSourceLocation {
	/** Borrowed source file name; storage must outlive the descriptor registration. */
	std::string_view file{};
	/** Borrowed source function name; storage must outlive the descriptor registration. */
	std::string_view function{};
	/** Source line number; zero means unavailable. */
	uint32_t line = 0u;
	/** Source column number; zero means unavailable. */
	uint32_t column = 0u;

	/**
	 * Capture source coordinates using compiler defaults; returned strings borrow the supplied
	 * names.
	 */
	[[nodiscard]] static constexpr TimingSourceLocation current(
		const char* fileName = __builtin_FILE(),
		const char* functionName = __builtin_FUNCTION(),
		uint32_t lineNumber = __builtin_LINE(),
		uint32_t columnNumber = 0u) noexcept {
		return TimingSourceLocation{
			.file = fileName,
			.function = functionName,
			.line = lineNumber,
			.column = columnNumber,
		};
	}
};

/**
 * Immutable zone metadata. Names and source strings are borrowed and must outlive the timing
 * service that registers them.
 */
struct TimingZoneDescriptor {
	/** Stable timing descriptor identity; zero is unassigned. */
	TimingZoneTypeId typeId = 0u;
	/** Display name for this source, track or descriptor. */
	std::string_view name{};
	/** Subsystem category used for filtering and presentation. */
	TimingCategory category = TimingCategory::User;
	/** Whether the timing span represents work, waiting, a gap or tool overhead. */
	TimingZoneRole role = TimingZoneRole::Work;
	/** Lowest runtime CPU capture level that admits this descriptor. */
	CpuTimingLevel minimumCpuLevel = CpuTimingLevel::Summary;
	/** Borrowed source coordinates and text; storage must outlive registration. */
	TimingSourceLocation source{};
};

/**
 * Recorder-local token identifying an admitted CPU zone. Inactive tokens are safe to pass to
 * end().
 */
struct ActiveZoneToken {
	/** Identity of this admitted CPU timing invocation. */
	TimingInvocationId invocationId = 0u;
	/** Recorder stack position captured when the zone began. */
	uint16_t stackIndex = 0u;
	/** Whether this token or edit scope was admitted and is still active. */
	bool active = false;

	/** Return whether this frame, token or thread attachment is valid and active. */
	[[nodiscard]] constexpr explicit operator bool() const noexcept { return active; }
};

/**
 * Completed CPU timing span in the monotonic capture clock. Durations use nanoseconds; parent and
 * child attribution stays on one producer track.
 */
struct CpuTimingRecord {
	/** Start timestamp in the monotonic capture clock, in nanoseconds. */
	uint64_t startNs = 0u;
	/** Measured duration in nanoseconds. */
	uint64_t durationNs = 0u;
	/** Inclusive duration of direct children, in nanoseconds. */
	uint64_t directChildNs = 0u;
	/** Identity of this admitted CPU timing invocation. */
	TimingInvocationId invocationId = 0u;
	/** Parent CPU invocation identity; zero means a root span. */
	TimingInvocationId parentInvocationId = 0u;
	/** Stable timing descriptor identity; zero is unassigned. */
	TimingZoneTypeId typeId = 0u;
	/** Window and frame sequence associated with this evidence. */
	WindowFrameKey frame{};
	/** Application tick associated with this evidence; zero means unassigned. */
	AppTickId appTick = 0u;
	/** Primary numeric entity identity interpreted by entityKind. */
	uint64_t primaryEntityId = 0u;
	/** Secondary numeric entity identity interpreted by entityKind. */
	uint64_t secondaryEntityId = 0u;
	/** Attached CPU producer track identity. */
	TimingTrackId track = 0u;
	/** Entity tag that defines the meaning of the numeric entity identities. */
	TimingEntityKind entityKind = TimingEntityKind::None;
	/** Zero-based nesting depth within the producer stack or frame-slot plan. */
	uint8_t depth = 0u;
	/** Bitmask of completion, provenance and capture-quality conditions. */
	uint16_t flags = timingRecordFlags(TimingRecordFlag::Completed);

	/** Return inclusive duration minus direct child duration, clamped to zero. */
	[[nodiscard]] constexpr uint64_t exclusiveNs() const noexcept {
		return durationNs > directChildNs ? durationNs - directChildNs : 0u;
	}
};

/** Owned display metadata for one CPU producer track. */
struct TimingTrackDescriptor {
	/** Stable identity of this retained entity; zero means unassigned. */
	TimingTrackId id = 0u;
	/** Display name for this source, track or descriptor. */
	std::string name{};
};

/** Inclusive invocation costs accumulated for one element definition within a frame and app tick. */
struct ElementDefinitionTimingAggregate {
	/** Element definition whose invocations contributed to the aggregate. */
	FlowDefinitionID definition{};
	/** Window and frame sequence associated with this evidence. */
	WindowFrameKey frame{};
	/** Application tick associated with this evidence; zero means unassigned. */
	AppTickId appTick = 0u;
	/** Number of element invocations included in the aggregate. */
	uint64_t invocationCount = 0u;
	/** Sum of inclusive invocation durations in nanoseconds. */
	uint64_t totalInclusiveNs = 0u;
	/** Largest inclusive invocation duration in nanoseconds. */
	uint64_t maximumInclusiveNs = 0u;
	/** Number of aggregate invocations marked cancelled. */
	uint64_t canceledInvocationCount = 0u;
};

/**
 * Measured CPU clock-read pair costs in nanoseconds; describes instrumentation overhead rather
 * than application work.
 */
struct TimingClockCalibration {
	/** Minimum measured pair of CPU clock reads, in nanoseconds. */
	uint64_t minimumPairNs = 0u;
	/** Median measured pair of CPU clock reads, in nanoseconds. */
	uint64_t medianPairNs = 0u;
	/** 95th percentile pair of CPU clock reads, in nanoseconds. */
	uint64_t p95PairNs = 0u;
	/** Number of samples included in this measurement. */
	uint32_t sampleCount = 0u;
};

/** Cumulative CPU capture counters describing suppressed, lost and anomalous timing evidence. */
struct TimingQualitySnapshot {
	/** Number of recorded zones. */
	uint64_t recordedZones = 0u;
	/** Number of suppressed zones. */
	uint64_t suppressedZones = 0u;
	/** Number of dropped records. */
	uint64_t droppedRecords = 0u;
	/** Number of stack overflows. */
	uint64_t stackOverflows = 0u;
	/** Number of misnested zones. */
	uint64_t misnestedZones = 0u;
	/** Number of incomplete zones. */
	uint64_t incompleteZones = 0u;
	/** Number of clock anomalies. */
	uint64_t clockAnomalies = 0u;
	/** Number of descriptor collisions. */
	uint64_t descriptorCollisions = 0u;
	/** Cumulative timing overhead in nanoseconds. */
	uint64_t timingOverheadNs = 0u;
};

/** Return the category mask bit, or zero when the numeric category is outside 32 bits. */
[[nodiscard]] constexpr uint32_t timingCategoryBit(TimingCategory category) noexcept {
	const uint32_t index = static_cast<uint32_t>(category);
	return index < 32u ? (1u << index) : 0u;
}

namespace detail::dev_timing {

inline constexpr uint64_t kHashOffset = 14695981039346656037ull;
inline constexpr uint64_t kHashPrime = 1099511628211ull;

[[nodiscard]] consteval uint64_t hashBytes(std::string_view value, uint64_t hash = kHashOffset) {
	for (const char character : value) {
		hash ^= static_cast<uint64_t>(static_cast<unsigned char>(character));
		hash *= kHashPrime;
	}
	return hash;
}

[[nodiscard]] consteval uint64_t hashInteger(uint64_t value, uint64_t hash) {
	for (uint32_t byteIndex = 0u; byteIndex < sizeof(value); ++byteIndex) {
		hash ^= (value >> (byteIndex * 8u)) & 0xFFu;
		hash *= kHashPrime;
	}
	return hash;
}

[[nodiscard]] consteval TimingZoneTypeId makeZoneTypeId(
	TimingCategory category,
	std::string_view name,
	std::string_view file,
	uint32_t line) {
	uint64_t hash = hashBytes("FlowUi.DevTiming.Zone");
	hash = hashInteger(static_cast<uint64_t>(category), hash);
	hash = hashBytes(name, hash);
	hash = hashBytes(file, hash);
	hash = hashInteger(line, hash);
	return hash == 0u ? 1u : hash;
}

} // namespace detail::dev_timing

/**
 * Create immutable zone metadata at compile time. The identity hashes category, name, source file
 * and line; all referenced strings must outlive registration.
 */
[[nodiscard]] consteval TimingZoneDescriptor makeTimingDescriptor(
	TimingCategory category,
	TimingZoneRole role,
	std::string_view name,
	TimingSourceLocation source = TimingSourceLocation::current(),
	CpuTimingLevel minimumCpuLevel = CpuTimingLevel::Summary) {
	return TimingZoneDescriptor{
		.typeId = detail::dev_timing::makeZoneTypeId(
			category, name, source.file, source.line),
		.name = name,
		.category = category,
		.role = role,
		.minimumCpuLevel = minimumCpuLevel,
		.source = source,
	};
}

/**
 * Create metadata for an explicitly assigned built-in timing identity. Name storage must outlive
 * timing registration.
 */
[[nodiscard]] consteval TimingZoneDescriptor makeBuiltinTimingDescriptor(
	TimingZoneTypeId typeId,
	TimingCategory category,
	TimingZoneRole role,
	CpuTimingLevel minimumCpuLevel,
	std::string_view name) {
	return TimingZoneDescriptor{
		.typeId = typeId,
		.name = name,
		.category = category,
		.role = role,
		.minimumCpuLevel = minimumCpuLevel,
	};
}

namespace timing_zones {

/** Immutable built-in timing descriptor for window frame total. */
inline constexpr TimingZoneDescriptor kWindowFrameTotal = makeBuiltinTimingDescriptor(
	0x8f4f3fced0be4ca1ull,
	TimingCategory::Frame,
	TimingZoneRole::Work,
	CpuTimingLevel::OnlyFrameTime,
	"flowui.frame.total");

/** Immutable built-in timing descriptor for window frame begin. */
inline constexpr TimingZoneDescriptor kWindowFrameBegin = makeBuiltinTimingDescriptor(
	0xa3f277f0f9b44d10ull,
	TimingCategory::Frame,
	TimingZoneRole::Work,
	CpuTimingLevel::Summary,
	"flowui.frame.begin");

/** Immutable built-in timing descriptor for window frame user build. */
inline constexpr TimingZoneDescriptor kWindowFrameUserBuild = makeBuiltinTimingDescriptor(
	0x16ba65267a1d4582ull,
	TimingCategory::Frame,
	TimingZoneRole::Work,
	CpuTimingLevel::Summary,
	"flowui.frame.user_build");

/** Immutable built-in timing descriptor for window frame end. */
inline constexpr TimingZoneDescriptor kWindowFrameEnd = makeBuiltinTimingDescriptor(
	0x3c7ef860f33245cbull,
	TimingCategory::Frame,
	TimingZoneRole::Work,
	CpuTimingLevel::Summary,
	"flowui.frame.end");

/** Immutable built-in timing descriptor for window frame prepared gap. */
inline constexpr TimingZoneDescriptor kWindowFramePreparedGap = makeBuiltinTimingDescriptor(
	0x98d5c988951a46e8ull,
	TimingCategory::Frame,
	TimingZoneRole::Gap,
	CpuTimingLevel::Summary,
	"flowui.frame.prepared_gap");

/** Immutable built-in timing descriptor for window frame draw. */
inline constexpr TimingZoneDescriptor kWindowFrameDraw = makeBuiltinTimingDescriptor(
	0xb8abdb819fd44377ull,
	TimingCategory::Frame,
	TimingZoneRole::Work,
	CpuTimingLevel::Summary,
	"flowui.frame.draw");

/** Immutable built-in timing descriptor for element invoke. */
inline constexpr TimingZoneDescriptor kElementInvoke = makeBuiltinTimingDescriptor(
	0x1a80ba423d5c40efull,
	TimingCategory::Element,
	TimingZoneRole::Work,
	CpuTimingLevel::Summary,
	"flowui.element.invoke");

/** Immutable built-in timing descriptor for element constructed subtree. */
inline constexpr TimingZoneDescriptor kElementConstructedSubtree = makeBuiltinTimingDescriptor(
	0xc651715e15db4e1bull,
	TimingCategory::Element,
	TimingZoneRole::Work,
	CpuTimingLevel::Balanced,
	"flowui.element.constructed_subtree");

} // namespace timing_zones

} // namespace FlowUi::devSystems

#endif
