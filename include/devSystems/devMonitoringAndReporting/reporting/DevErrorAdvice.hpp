#pragma once

#include "FlowUi/Public/BuildConfig.hpp"

#if FLOW_UI_DEV_MODE

#include <array>
#include <cstdint>
#include <span>
#include <string_view>
#include <vector>

#include "FlowUi/Public/errorManagement/Error.hpp"

namespace FlowUi::devSystems {

struct DevErrorOccurrence;

/** Stable identity of one static diagnostic advice rule. */
using DevErrorAdviceId = uint64_t;

/** Subsystem category used to organize diagnostic guidance. */
enum class DevErrorAdviceCategory : uint8_t {
	/** Capacity guidance or supporting evidence. */
	Capacity = 0u,
	/** Lifecycle guidance or supporting evidence. */
	Lifecycle,
	/** Resource guidance or supporting evidence. */
	Resource,
	/** Popup guidance or supporting evidence. */
	Popup,
	/** Viewport guidance or supporting evidence. */
	Viewport,
	/** Asset guidance or supporting evidence. */
	Asset,
	/** Renderer guidance or supporting evidence. */
	Renderer,
	/** Vulkan guidance or supporting evidence. */
	Vulkan,
	/** Capture quality guidance or supporting evidence. */
	CaptureQuality,
};

/** Strength of the evidence supporting a diagnostic suggestion. */
enum class DevErrorAdviceConfidence : uint8_t {
	/** Captured evidence establishes the suggestion directly. */
	Certain = 0u,
	/** Captured evidence strongly supports the suggestion. */
	Strong,
	/** Suggestion is plausible for the observed contract. */
	Possible,
	/** Captured evidence cannot support a stronger conclusion. */
	InsufficientEvidence,
};

/** Evidence condition that must hold for an advice descriptor to apply. */
enum class DevErrorAdvicePredicate : uint8_t {
	/** Apply without an additional evidence predicate. */
	Always = 0u,
	/** Require memory pressure observed. */
	MemoryPressureObserved,
	/** Require frame context available. */
	FrameContextAvailable,
	/** Require native code available. */
	NativeCodeAvailable,
	/** Require fallback applied. */
	FallbackApplied,
	/** Require operation rejected. */
	OperationRejected,
	/** Require snapshot available. */
	SnapshotAvailable,
	/** Require capture incomplete. */
	CaptureIncomplete,
};

/** Kind of captured fact supporting an advice result. */
enum class DevErrorAdviceEvidenceKind : uint8_t {
	/** Group by error code and site. */
	Contract = 0u,
	/** Subject guidance or supporting evidence. */
	Subject,
	/** Auxiliary guidance or supporting evidence. */
	Auxiliary,
	/** Recorded recovery resolution. */
	Resolution,
	/** Frame guidance or supporting evidence. */
	Frame,
	/** Timing zone guidance or supporting evidence. */
	TimingZone,
	/** Memory event guidance or supporting evidence. */
	MemoryEvent,
	/** Memory checkpoint guidance or supporting evidence. */
	MemoryCheckpoint,
	/** Native code guidance or supporting evidence. */
	NativeCode,
	/** Snapshot guidance or supporting evidence. */
	Snapshot,
	/** Capture quality guidance or supporting evidence. */
	CaptureQuality,
};

/** Compact reference to one fact used to justify guidance. */
struct DevErrorAdviceEvidenceRef {
	/** Semantic kind of this record or reference. */
	DevErrorAdviceEvidenceKind kind = DevErrorAdviceEvidenceKind::Contract;
	/** Primary evidence identity interpreted by kind. */
	uint64_t primary = 0u;
	/** Secondary evidence identity interpreted by kind. */
	uint64_t secondary = 0u;
};

/** Static advice rule with borrowed immutable prose, matching contract and evidence predicate. */
struct DevErrorAdviceDescriptor {
	/** Stable identity of this retained entity; zero means unassigned. */
	DevErrorAdviceId id = 0u;
	/** Production error code matched by this rule, query or group. */
	ErrorCode code = ErrorCode::None;
	/** Production error site matched by this rule, query or group. */
	ErrorSite site = ErrorSite::None;
	/** Subsystem category used for filtering and presentation. */
	DevErrorAdviceCategory category = DevErrorAdviceCategory::Resource;
	/** Evidence condition required for the advice rule to apply. */
	DevErrorAdvicePredicate predicate = DevErrorAdvicePredicate::Always;
	/** Advice ordering priority; interpreted by the catalogue evaluator. */
	uint16_t priority = 0u;
	/** Initial confidence before evaluating captured evidence. */
	DevErrorAdviceConfidence baseConfidence = DevErrorAdviceConfidence::Possible;
	/** Human-readable guidance title. */
	std::string_view title{};
	/** Borrowed explanation of the diagnostic guidance. */
	std::string_view explanation{};
	/** Borrowed human-readable suggested action; storage must outlive this view. */
	std::string_view suggestedAction{};
	/** Borrowed configuration key relevant to the suggestion. */
	std::string_view configurationKey{};
	/** Borrowed documentation reference relevant to the suggestion. */
	std::string_view documentation{};
	/** Borrowed explanation of evidence or confidence limitations. */
	std::string_view limitation{};
};

/** Inclusive error-code range intentionally lacking advice, with a borrowed rationale. */
struct DevErrorNoGuidanceAnnotation {
	/** First error code in the inclusive intentionally unguided range. */
	ErrorCode first = ErrorCode::None;
	/** Last error code in the inclusive intentionally unguided range. */
	ErrorCode last = ErrorCode::None;
	/** Borrowed reason that the error-code range intentionally has no advice. */
	std::string_view rationale{};
};

/** Maximum supporting evidence references retained per advice result. */
inline constexpr size_t kDevErrorAdviceEvidenceCapacity = 8u;

/**
 * Evaluated guidance with bounded evidence references. Prose views refer to the catalogue or an
 * owning investigation.
 */
struct DevErrorAdviceResult {
	/** Bounded numeric evidence supporting this record or advice result. */
	std::array<DevErrorAdviceEvidenceRef, kDevErrorAdviceEvidenceCapacity> evidence{};
	/** Human-readable guidance title. */
	std::string_view title{};
	/** Borrowed explanation of the diagnostic guidance. */
	std::string_view explanation{};
	/** Borrowed human-readable suggested action; storage must outlive this view. */
	std::string_view suggestedAction{};
	/** Borrowed configuration key relevant to the suggestion. */
	std::string_view configurationKey{};
	/** Borrowed documentation reference relevant to the suggestion. */
	std::string_view documentation{};
	/** Borrowed explanation of evidence or confidence limitations. */
	std::string_view limitation{};
	/** Registered breadcrumb or advice descriptor identity. */
	DevErrorAdviceId descriptorId = 0u;
	/** Registered diagnostic source identity; zero means unavailable. */
	uint64_t sourceId = 0u;
	/** Subsystem category used for filtering and presentation. */
	DevErrorAdviceCategory category = DevErrorAdviceCategory::Resource;
	/** Strength or basis of the available supporting evidence. */
	DevErrorAdviceConfidence confidence = DevErrorAdviceConfidence::Possible;
	/** Advice ordering priority; interpreted by the catalogue evaluator. */
	uint16_t priority = 0u;
	/** Number of populated references in evidence. */
	uint8_t evidenceCount = 0u;
};

/** Static human-facing catalogue. All advice prose is defined in its .cpp file. */
[[nodiscard]] std::span<const DevErrorAdviceDescriptor> devErrorAdviceCatalogue() noexcept;
[[nodiscard]] std::span<const DevErrorNoGuidanceAnnotation>
/** Borrow static inclusive error-code ranges that intentionally have no guidance. */
devErrorNoGuidanceAnnotations() noexcept;
/** Return whether the catalogue or an explicit no-guidance annotation covers the error code. */
[[nodiscard]] bool devErrorAdviceCatalogueCovers(ErrorCode code) noexcept;

/** Pure evaluation over one immutable occurrence; never calls managers or mutates App state. */
[[nodiscard]] std::vector<DevErrorAdviceResult> evaluateDevErrorAdvice(
	const DevErrorOccurrence& occurrence,
	uint32_t maximumResults);

} // namespace FlowUi::devSystems

#endif
