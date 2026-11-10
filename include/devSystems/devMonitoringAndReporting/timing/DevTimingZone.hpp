#pragma once

#include "FlowUi/Public/BuildConfig.hpp"

#if FLOW_UI_DEV_MODE

#include "devSystems/devMonitoringAndReporting/timing/DevTiming.hpp"

namespace FlowUi::devSystems {

/**
 * Nonmovable lexical CPU timing scope. The descriptor text must outlive the timing service;
 * destruction ends an admitted zone automatically.
 */
class CpuTimingZone {
public:
	/**
	 * Begin the scoped timing measurement when the recorder or context admits it; null pointers
	 * disable capture. Borrowed context and descriptor text must outlive the scope.
	 */
	CpuTimingZone(
		DevTimingRecorder& recorder,
		const TimingZoneDescriptor& descriptor,
		TimingEntityRef entity = {}) noexcept;
	/**
	 * Begin the scoped timing measurement when the recorder or context admits it; null pointers
	 * disable capture. Borrowed context and descriptor text must outlive the scope.
	 */
	CpuTimingZone(
		DevTimingRecorder* recorder,
		const TimingZoneDescriptor& descriptor,
		TimingEntityRef entity = {}) noexcept;
	/** End the admitted CPU zone as completed. */
	~CpuTimingZone() noexcept;

	/** Copying is disabled to preserve ownership and producer identity. */
	CpuTimingZone(const CpuTimingZone&) = delete;
	CpuTimingZone& operator=(const CpuTimingZone&) = delete;
	/** Moving is disabled to preserve the owner and active scope addresses. */
	CpuTimingZone(CpuTimingZone&&) = delete;
	CpuTimingZone& operator=(CpuTimingZone&&) = delete;

private:
	DevTimingRecorder* recorder_ = nullptr;
	ActiveZoneToken token_{};
};

/**
 * Move-only CPU timing scope with explicit begin/end. Destruction abandons any still-active zone
 * as incomplete.
 */
class ManualTimingZone {
public:
	/** Construct an empty inactive handle. */
	ManualTimingZone() noexcept = default;
	/** Begin or transfer an explicitly controlled CPU timing scope. */
	ManualTimingZone(
		DevTimingRecorder& recorder,
		const TimingZoneDescriptor& descriptor,
		TimingEntityRef entity = {}) noexcept;
	/** End any still-active zone as incomplete. */
	~ManualTimingZone() noexcept;

	/** Copying is disabled to preserve ownership and producer identity. */
	ManualTimingZone(const ManualTimingZone&) = delete;
	ManualTimingZone& operator=(const ManualTimingZone&) = delete;
	/** Begin or transfer an explicitly controlled CPU timing scope. */
	ManualTimingZone(ManualTimingZone&& other) noexcept;
	/** Mark this scope incomplete, then transfer other's active zone; other becomes empty. */
	ManualTimingZone& operator=(ManualTimingZone&& other) noexcept;

	/**
	 * End any existing manual scope, then try to begin a new CPU scope using borrowed descriptor
	 * metadata.
	 */
	void begin(
		DevTimingRecorder& recorder,
		const TimingZoneDescriptor& descriptor,
		TimingEntityRef entity = {}) noexcept;
	/** End the admitted zone with the supplied completion flags; inactive tokens are ignored. */
	void end(TimingRecordFlag result = TimingRecordFlag::Completed) noexcept;
	/** Return whether this manual timing scope currently owns an admitted zone. */
	[[nodiscard]] bool active() const noexcept { return static_cast<bool>(token_); }

private:
	void abandon() noexcept;

	DevTimingRecorder* recorder_ = nullptr;
	ActiveZoneToken token_{};
};

/**
 * RAII CPU timing scope that also contributes per-definition aggregate costs. A null recorder
 * disables capture.
 */
class ElementTimingZone {
public:
	/**
	 * Begin the scoped timing measurement when the recorder or context admits it; null pointers
	 * disable capture. Borrowed context and descriptor text must outlive the scope.
	 */
	ElementTimingZone(
		DevTimingRecorder* recorder,
		FlowDefinitionID definition,
		FlowElementID instance) noexcept;
	/** End any still-active zone as incomplete. */
	~ElementTimingZone() noexcept;

	/** Copying is disabled to preserve ownership and producer identity. */
	ElementTimingZone(const ElementTimingZone&) = delete;
	ElementTimingZone& operator=(const ElementTimingZone&) = delete;

	/** End the admitted zone with the supplied completion flags; inactive tokens are ignored. */
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
/**
 * Open a lexical summary-level CPU zone on a recorder or valid thread attachment. The name
 * must be compile-time text whose storage outlives registration. Disabled build levels do not
 * evaluate macro arguments.
 */
#define FLOWUI_DEV_TIMING_ZONE(recorder, category, role, name)                  \
	FLOWUI_DEV_TIMING_ZONE_LEVEL_IMPL(                                            \
		recorder, category, role, ::FlowUi::devSystems::CpuTimingLevel::Summary,   \
		name, ::FlowUi::devSystems::TimingEntityRef{}, __COUNTER__)

/**
 * Open a summary-level CPU zone attributed to the supplied TimingEntityRef. Disabled build
 * levels do not evaluate macro arguments.
 */
#define FLOWUI_DEV_TIMING_ZONE_ENTITY(recorder, category, role, name, entity)   \
	FLOWUI_DEV_TIMING_ZONE_LEVEL_IMPL(                                            \
		recorder, category, role, ::FlowUi::devSystems::CpuTimingLevel::Summary,   \
		name, entity, __COUNTER__)
/**
 * Open a summary-level CPU zone when the recorder pointer is nonnull. Disabled build levels
 * do not evaluate macro arguments.
 */
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
/**
 * Open a lexical balanced-level CPU zone when that detail is compiled and enabled. Disabled
 * build levels do not evaluate macro arguments.
 */
#define FLOWUI_DEV_TIMING_ZONE_BALANCED(recorder, category, role, name)         \
	FLOWUI_DEV_TIMING_ZONE_LEVEL_IMPL(                                            \
		recorder, category, role, ::FlowUi::devSystems::CpuTimingLevel::Balanced,    \
		name, ::FlowUi::devSystems::TimingEntityRef{}, __COUNTER__)
/**
 * Open a balanced-level CPU zone attributed to the supplied TimingEntityRef. Disabled build
 * levels do not evaluate macro arguments.
 */
#define FLOWUI_DEV_TIMING_ZONE_BALANCED_ENTITY(recorder, category, role, name, entity) \
	FLOWUI_DEV_TIMING_ZONE_LEVEL_IMPL(                                            \
		recorder, category, role, ::FlowUi::devSystems::CpuTimingLevel::Balanced,    \
		name, entity, __COUNTER__)
/**
 * Open a balanced-level CPU zone when the recorder pointer is nonnull. Disabled build levels
 * do not evaluate macro arguments.
 */
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
/**
 * Open a lexical deep-level CPU zone when that detail is compiled and enabled. Disabled build
 * levels do not evaluate macro arguments.
 */
#define FLOWUI_DEV_TIMING_ZONE_DEEP(recorder, category, role, name)             \
	FLOWUI_DEV_TIMING_ZONE_LEVEL_IMPL(                                            \
		recorder, category, role, ::FlowUi::devSystems::CpuTimingLevel::Deep,        \
		name, ::FlowUi::devSystems::TimingEntityRef{}, __COUNTER__)
/**
 * Open a deep-level CPU zone attributed to the supplied TimingEntityRef. Disabled build
 * levels do not evaluate macro arguments.
 */
#define FLOWUI_DEV_TIMING_ZONE_DEEP_ENTITY(recorder, category, role, name, entity) \
	FLOWUI_DEV_TIMING_ZONE_LEVEL_IMPL(                                            \
		recorder, category, role, ::FlowUi::devSystems::CpuTimingLevel::Deep,        \
		name, entity, __COUNTER__)
/**
 * Open a deep-level CPU zone when the recorder pointer is nonnull. Disabled build levels do
 * not evaluate macro arguments.
 */
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
