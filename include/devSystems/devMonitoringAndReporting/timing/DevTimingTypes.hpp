#pragma once

#include "FlowUi/BuildConfig.hpp"

#if FLOW_UI_DEV_MODE

#include <cstdint>
#include <string_view>

#include "FlowUi/ElementID.hpp"
#include "FlowUi/WindowId.hpp"

namespace FlowUi::devSystems {

enum class CpuTimingLevel : uint8_t {
	OnlyFrameTime = 0,
	Summary = 1,
	Balanced = 2,
	Deep = 3,
};

enum class TimingCategory : uint8_t {
	Lifecycle = 0,
	Frame,
	Input,
	Element,
	Layout,
	Prepare,
	RendererCpu,
	Gpu,
	Wait,
	User,
	DevTool,
	Count,
};

enum class TimingZoneRole : uint8_t {
	Work = 0,
	Wait,
	Gap,
	GpuWork,
	DevToolWork,
};

enum class TimingEntityKind : uint8_t {
	None = 0,
	App,
	Window,
	ElementDefinition,
	ElementInstance,
	Viewport,
	Action,
	Resource,
	Submission,
};

enum class TimingRecordFlag : uint16_t {
	None = 0,
	Completed = 1u << 0u,
	Canceled = 1u << 1u,
	OutOfDate = 1u << 2u,
	Minimized = 1u << 3u,
	Exception = 1u << 4u,
	Incomplete = 1u << 5u,
	ClockAnomaly = 1u << 6u,
	DetailTruncated = 1u << 7u,
};

[[nodiscard]] constexpr TimingRecordFlag operator|(
	TimingRecordFlag left,
	TimingRecordFlag right) noexcept {
	return static_cast<TimingRecordFlag>(
		static_cast<uint16_t>(left) | static_cast<uint16_t>(right));
}

[[nodiscard]] constexpr uint16_t timingRecordFlags(TimingRecordFlag flag) noexcept {
	return static_cast<uint16_t>(flag);
}

using TimingZoneTypeId = uint64_t;
using TimingInvocationId = uint64_t;
using TimingTrackId = uint32_t;
using AppTickId = uint64_t;

struct WindowFrameKey {
	WindowId window = InvalidWindowId;
	uint64_t frameNumber = 0u;

	[[nodiscard]] constexpr explicit operator bool() const noexcept {
		return window != InvalidWindowId && frameNumber != 0u;
	}

	auto operator<=>(const WindowFrameKey&) const = default;
};

struct TimingEntityRef {
	TimingEntityKind kind = TimingEntityKind::None;
	uint64_t primaryId = 0u;
	uint64_t secondaryId = 0u;

	[[nodiscard]] static constexpr TimingEntityRef window(WindowId id) noexcept {
		return TimingEntityRef{
			.kind = TimingEntityKind::Window,
			.primaryId = static_cast<uint64_t>(id),
		};
	}

	[[nodiscard]] static constexpr TimingEntityRef definition(FlowDefinitionID id) noexcept {
		return TimingEntityRef{
			.kind = TimingEntityKind::ElementDefinition,
			.primaryId = id.value,
		};
	}

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

struct TimingSourceLocation {
	std::string_view file{};
	std::string_view function{};
	uint32_t line = 0u;
	uint32_t column = 0u;

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

struct TimingZoneDescriptor {
	TimingZoneTypeId typeId = 0u;
	std::string_view name{};
	TimingCategory category = TimingCategory::User;
	TimingZoneRole role = TimingZoneRole::Work;
	CpuTimingLevel minimumCpuLevel = CpuTimingLevel::Summary;
	TimingSourceLocation source{};
};

struct ActiveZoneToken {
	TimingInvocationId invocationId = 0u;
	uint16_t stackIndex = 0u;
	bool active = false;

	[[nodiscard]] constexpr explicit operator bool() const noexcept { return active; }
};

struct CpuTimingRecord {
	uint64_t startNs = 0u;
	uint64_t durationNs = 0u;
	uint64_t directChildNs = 0u;
	TimingInvocationId invocationId = 0u;
	TimingInvocationId parentInvocationId = 0u;
	TimingZoneTypeId typeId = 0u;
	WindowFrameKey frame{};
	AppTickId appTick = 0u;
	uint64_t primaryEntityId = 0u;
	uint64_t secondaryEntityId = 0u;
	TimingTrackId track = 0u;
	TimingEntityKind entityKind = TimingEntityKind::None;
	uint8_t depth = 0u;
	uint16_t flags = timingRecordFlags(TimingRecordFlag::Completed);

	[[nodiscard]] constexpr uint64_t exclusiveNs() const noexcept {
		return durationNs > directChildNs ? durationNs - directChildNs : 0u;
	}
};

struct DevTimingConfig {
	CpuTimingLevel cpuLevel = CpuTimingLevel::Summary;
	uint32_t enabledCategoryMask = 0xFFFFFFFFu;
	bool gpuTimingEnabled = false;
	uint32_t producerRecordCapacity = 8192u;
};

struct TimingClockCalibration {
	uint64_t minimumPairNs = 0u;
	uint64_t medianPairNs = 0u;
	uint64_t p95PairNs = 0u;
	uint32_t sampleCount = 0u;
};

struct TimingQualitySnapshot {
	uint64_t recordedZones = 0u;
	uint64_t suppressedZones = 0u;
	uint64_t droppedRecords = 0u;
	uint64_t stackOverflows = 0u;
	uint64_t misnestedZones = 0u;
	uint64_t incompleteZones = 0u;
	uint64_t clockAnomalies = 0u;
	uint64_t descriptorCollisions = 0u;
};

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

inline constexpr TimingZoneDescriptor kWindowFrameTotal = makeBuiltinTimingDescriptor(
	0x8f4f3fced0be4ca1ull,
	TimingCategory::Frame,
	TimingZoneRole::Work,
	CpuTimingLevel::OnlyFrameTime,
	"flowui.frame.total");

} // namespace timing_zones

} // namespace FlowUi::devSystems

#endif
