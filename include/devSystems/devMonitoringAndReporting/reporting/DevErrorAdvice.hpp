#pragma once

#include "FlowUi/BuildConfig.hpp"

#if FLOW_UI_DEV_MODE

#include <array>
#include <cstdint>
#include <span>
#include <string_view>
#include <vector>

#include "FlowUi/Error.hpp"

namespace FlowUi::devSystems {

struct DevErrorOccurrence;

using DevErrorAdviceId = uint64_t;

enum class DevErrorAdviceCategory : uint8_t {
	Capacity = 0u,
	Lifecycle,
	Resource,
	Popup,
	Viewport,
	Asset,
	Renderer,
	Vulkan,
	CaptureQuality,
};

enum class DevErrorAdviceConfidence : uint8_t {
	Certain = 0u,
	Strong,
	Possible,
	InsufficientEvidence,
};

enum class DevErrorAdvicePredicate : uint8_t {
	Always = 0u,
	MemoryPressureObserved,
	FrameContextAvailable,
	NativeCodeAvailable,
	FallbackApplied,
	OperationRejected,
	SnapshotAvailable,
	CaptureIncomplete,
};

enum class DevErrorAdviceEvidenceKind : uint8_t {
	Contract = 0u,
	Subject,
	Auxiliary,
	Resolution,
	Frame,
	TimingZone,
	MemoryEvent,
	MemoryCheckpoint,
	NativeCode,
	Snapshot,
	CaptureQuality,
};

struct DevErrorAdviceEvidenceRef {
	DevErrorAdviceEvidenceKind kind = DevErrorAdviceEvidenceKind::Contract;
	uint64_t primary = 0u;
	uint64_t secondary = 0u;
};

struct DevErrorAdviceDescriptor {
	DevErrorAdviceId id = 0u;
	ErrorCode code = ErrorCode::None;
	ErrorSite site = ErrorSite::None;
	DevErrorAdviceCategory category = DevErrorAdviceCategory::Resource;
	DevErrorAdvicePredicate predicate = DevErrorAdvicePredicate::Always;
	uint16_t priority = 0u;
	DevErrorAdviceConfidence baseConfidence = DevErrorAdviceConfidence::Possible;
	std::string_view title{};
	std::string_view explanation{};
	std::string_view suggestedAction{};
	std::string_view configurationKey{};
	std::string_view documentation{};
	std::string_view limitation{};
};

struct DevErrorNoGuidanceAnnotation {
	ErrorCode first = ErrorCode::None;
	ErrorCode last = ErrorCode::None;
	std::string_view rationale{};
};

inline constexpr size_t kDevErrorAdviceEvidenceCapacity = 8u;

struct DevErrorAdviceResult {
	DevErrorAdviceId descriptorId = 0u;
	DevErrorAdviceCategory category = DevErrorAdviceCategory::Resource;
	DevErrorAdviceConfidence confidence = DevErrorAdviceConfidence::Possible;
	uint16_t priority = 0u;
	uint64_t sourceId = 0u;
	std::string_view title{};
	std::string_view explanation{};
	std::string_view suggestedAction{};
	std::string_view configurationKey{};
	std::string_view documentation{};
	std::string_view limitation{};
	uint8_t evidenceCount = 0u;
	std::array<DevErrorAdviceEvidenceRef, kDevErrorAdviceEvidenceCapacity> evidence{};
};

/** Static human-facing catalogue. All advice prose is defined in its .cpp file. */
[[nodiscard]] std::span<const DevErrorAdviceDescriptor> devErrorAdviceCatalogue() noexcept;
[[nodiscard]] std::span<const DevErrorNoGuidanceAnnotation>
devErrorNoGuidanceAnnotations() noexcept;
[[nodiscard]] bool devErrorAdviceCatalogueCovers(ErrorCode code) noexcept;

/** Pure evaluation over one immutable occurrence; never calls managers or mutates App state. */
[[nodiscard]] std::vector<DevErrorAdviceResult> evaluateDevErrorAdvice(
	const DevErrorOccurrence& occurrence,
	uint32_t maximumResults);

} // namespace FlowUi::devSystems

#endif
