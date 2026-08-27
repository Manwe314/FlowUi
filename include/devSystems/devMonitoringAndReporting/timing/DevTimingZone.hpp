#pragma once

#include "FlowUi/BuildConfig.hpp"

#if FLOW_UI_DEV_MODE

#include "devSystems/devMonitoringAndReporting/timing/DevTiming.hpp"

namespace FlowUi::devSystems {

class CpuTimingZone {
public:
	CpuTimingZone(
		DevTimingRecorder& recorder,
		const TimingZoneDescriptor& descriptor,
		TimingEntityRef entity = {}) noexcept;
	CpuTimingZone(
		DevTimingRecorder* recorder,
		const TimingZoneDescriptor& descriptor,
		TimingEntityRef entity = {}) noexcept;
	~CpuTimingZone() noexcept;

	CpuTimingZone(const CpuTimingZone&) = delete;
	CpuTimingZone& operator=(const CpuTimingZone&) = delete;
	CpuTimingZone(CpuTimingZone&&) = delete;
	CpuTimingZone& operator=(CpuTimingZone&&) = delete;

private:
	DevTimingRecorder* recorder_ = nullptr;
	ActiveZoneToken token_{};
};

class ManualTimingZone {
public:
	ManualTimingZone() noexcept = default;
	ManualTimingZone(
		DevTimingRecorder& recorder,
		const TimingZoneDescriptor& descriptor,
		TimingEntityRef entity = {}) noexcept;
	~ManualTimingZone() noexcept;

	ManualTimingZone(const ManualTimingZone&) = delete;
	ManualTimingZone& operator=(const ManualTimingZone&) = delete;
	ManualTimingZone(ManualTimingZone&& other) noexcept;
	ManualTimingZone& operator=(ManualTimingZone&& other) noexcept;

	void begin(
		DevTimingRecorder& recorder,
		const TimingZoneDescriptor& descriptor,
		TimingEntityRef entity = {}) noexcept;
	void end(TimingRecordFlag result = TimingRecordFlag::Completed) noexcept;
	[[nodiscard]] bool active() const noexcept { return static_cast<bool>(token_); }

private:
	void abandon() noexcept;

	DevTimingRecorder* recorder_ = nullptr;
	ActiveZoneToken token_{};
};

class ElementTimingZone {
public:
	ElementTimingZone(
		DevTimingRecorder* recorder,
		FlowDefinitionID definition,
		FlowElementID instance) noexcept;
	~ElementTimingZone() noexcept;

	ElementTimingZone(const ElementTimingZone&) = delete;
	ElementTimingZone& operator=(const ElementTimingZone&) = delete;

	void end(TimingRecordFlag result = TimingRecordFlag::Completed) noexcept;

private:
	DevTimingRecorder* recorder_ = nullptr;
	FlowDefinitionID definition_{};
	ActiveZoneToken token_{};
};


} // namespace FlowUi::devSystems

#define FLOWUI_DEV_TIMING_JOIN_IMPL(left, right) left##right
#define FLOWUI_DEV_TIMING_JOIN(left, right) FLOWUI_DEV_TIMING_JOIN_IMPL(left, right)

#define FLOWUI_DEV_TIMING_ZONE_LEVEL_IMPL(                                      \
	recorder, category, role, level, name, entity, unique)                       \
	static constexpr auto FLOWUI_DEV_TIMING_JOIN(_flowTimingDescriptor_, unique) = \
		::FlowUi::devSystems::makeTimingDescriptor(                               \
			category, role, name,                                                   \
			::FlowUi::devSystems::TimingSourceLocation::current(), level);          \
	[[maybe_unused]] ::FlowUi::devSystems::CpuTimingZone                         \
		FLOWUI_DEV_TIMING_JOIN(_flowTimingZone_, unique){                        \
			::FlowUi::devSystems::timingRecorder(recorder),                        \
			FLOWUI_DEV_TIMING_JOIN(_flowTimingDescriptor_, unique),                \
			entity}

#define FLOWUI_DEV_TIMING_ZONE_POINTER_LEVEL_IMPL(                              \
	recorder, category, role, level, name, entity, unique)                       \
	static constexpr auto FLOWUI_DEV_TIMING_JOIN(_flowTimingDescriptor_, unique) = \
		::FlowUi::devSystems::makeTimingDescriptor(                               \
			category, role, name,                                                   \
			::FlowUi::devSystems::TimingSourceLocation::current(), level);          \
	[[maybe_unused]] ::FlowUi::devSystems::CpuTimingZone                         \
		FLOWUI_DEV_TIMING_JOIN(_flowTimingZone_, unique){                        \
			recorder, FLOWUI_DEV_TIMING_JOIN(_flowTimingDescriptor_, unique), entity}

#if FLOWUI_DEV_TIMING_LEVEL >= 1
#define FLOWUI_DEV_TIMING_ZONE(recorder, category, role, name)                  \
	FLOWUI_DEV_TIMING_ZONE_LEVEL_IMPL(                                            \
		recorder, category, role, ::FlowUi::devSystems::CpuTimingLevel::Summary,   \
		name, ::FlowUi::devSystems::TimingEntityRef{}, __COUNTER__)

#define FLOWUI_DEV_TIMING_ZONE_ENTITY(recorder, category, role, name, entity)   \
	FLOWUI_DEV_TIMING_ZONE_LEVEL_IMPL(                                            \
		recorder, category, role, ::FlowUi::devSystems::CpuTimingLevel::Summary,   \
		name, entity, __COUNTER__)
#define FLOWUI_DEV_TIMING_ZONE_IF(recorder, category, role, name)               \
	FLOWUI_DEV_TIMING_ZONE_POINTER_LEVEL_IMPL(                                    \
		recorder, category, role, ::FlowUi::devSystems::CpuTimingLevel::Summary,   \
		name, ::FlowUi::devSystems::TimingEntityRef{}, __COUNTER__)
#else
#define FLOWUI_DEV_TIMING_ZONE(recorder, category, role, name) ((void)0)
#define FLOWUI_DEV_TIMING_ZONE_ENTITY(recorder, category, role, name, entity) ((void)0)
#define FLOWUI_DEV_TIMING_ZONE_IF(recorder, category, role, name) ((void)0)
#endif

#if FLOWUI_DEV_TIMING_LEVEL >= 2
#define FLOWUI_DEV_TIMING_ZONE_BALANCED(recorder, category, role, name)         \
	FLOWUI_DEV_TIMING_ZONE_LEVEL_IMPL(                                            \
		recorder, category, role, ::FlowUi::devSystems::CpuTimingLevel::Balanced,    \
		name, ::FlowUi::devSystems::TimingEntityRef{}, __COUNTER__)
#define FLOWUI_DEV_TIMING_ZONE_BALANCED_ENTITY(recorder, category, role, name, entity) \
	FLOWUI_DEV_TIMING_ZONE_LEVEL_IMPL(                                            \
		recorder, category, role, ::FlowUi::devSystems::CpuTimingLevel::Balanced,    \
		name, entity, __COUNTER__)
#define FLOWUI_DEV_TIMING_ZONE_BALANCED_IF(recorder, category, role, name)      \
	FLOWUI_DEV_TIMING_ZONE_POINTER_LEVEL_IMPL(                                    \
		recorder, category, role, ::FlowUi::devSystems::CpuTimingLevel::Balanced,  \
		name, ::FlowUi::devSystems::TimingEntityRef{}, __COUNTER__)
#else
#define FLOWUI_DEV_TIMING_ZONE_BALANCED(recorder, category, role, name) ((void)0)
#define FLOWUI_DEV_TIMING_ZONE_BALANCED_ENTITY(recorder, category, role, name, entity) ((void)0)
#define FLOWUI_DEV_TIMING_ZONE_BALANCED_IF(recorder, category, role, name) ((void)0)
#endif

#if FLOWUI_DEV_TIMING_LEVEL >= 3
#define FLOWUI_DEV_TIMING_ZONE_DEEP(recorder, category, role, name)             \
	FLOWUI_DEV_TIMING_ZONE_LEVEL_IMPL(                                            \
		recorder, category, role, ::FlowUi::devSystems::CpuTimingLevel::Deep,        \
		name, ::FlowUi::devSystems::TimingEntityRef{}, __COUNTER__)
#define FLOWUI_DEV_TIMING_ZONE_DEEP_ENTITY(recorder, category, role, name, entity) \
	FLOWUI_DEV_TIMING_ZONE_LEVEL_IMPL(                                            \
		recorder, category, role, ::FlowUi::devSystems::CpuTimingLevel::Deep,        \
		name, entity, __COUNTER__)
#define FLOWUI_DEV_TIMING_ZONE_DEEP_IF(recorder, category, role, name)          \
	FLOWUI_DEV_TIMING_ZONE_POINTER_LEVEL_IMPL(                                    \
		recorder, category, role, ::FlowUi::devSystems::CpuTimingLevel::Deep,      \
		name, ::FlowUi::devSystems::TimingEntityRef{}, __COUNTER__)
#else
#define FLOWUI_DEV_TIMING_ZONE_DEEP(recorder, category, role, name) ((void)0)
#define FLOWUI_DEV_TIMING_ZONE_DEEP_ENTITY(recorder, category, role, name, entity) ((void)0)
#define FLOWUI_DEV_TIMING_ZONE_DEEP_IF(recorder, category, role, name) ((void)0)
#endif

#else

#define FLOWUI_DEV_TIMING_ZONE(recorder, category, role, name) ((void)0)
#define FLOWUI_DEV_TIMING_ZONE_ENTITY(recorder, category, role, name, entity) ((void)0)
#define FLOWUI_DEV_TIMING_ZONE_IF(recorder, category, role, name) ((void)0)
#define FLOWUI_DEV_TIMING_ZONE_BALANCED(recorder, category, role, name) ((void)0)
#define FLOWUI_DEV_TIMING_ZONE_BALANCED_ENTITY(recorder, category, role, name, entity) ((void)0)
#define FLOWUI_DEV_TIMING_ZONE_BALANCED_IF(recorder, category, role, name) ((void)0)
#define FLOWUI_DEV_TIMING_ZONE_DEEP(recorder, category, role, name) ((void)0)
#define FLOWUI_DEV_TIMING_ZONE_DEEP_ENTITY(recorder, category, role, name, entity) ((void)0)
#define FLOWUI_DEV_TIMING_ZONE_DEEP_IF(recorder, category, role, name) ((void)0)

#endif
