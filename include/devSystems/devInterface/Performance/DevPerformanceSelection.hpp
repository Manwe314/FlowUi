#pragma once

#include "FlowUi/BuildConfig.hpp"
#if FLOW_UI_DEV_MODE
#include "devSystems/devMonitoringAndReporting/reporting/DevTimingReporting.hpp"
#include <array>
#include <string>

namespace FlowUi::devSystems {

inline constexpr std::array<std::string_view, static_cast<size_t>(TimingCategory::Count)>
	performance_category_names{"Lifecycle",	   "Frame", "Input", "Element", "Layout", "Prepare",
							   "Renderer CPU", "GPU",	"Wait",	 "User",	"DevTool"};

/** One scope identity shared by the window and thread trees. Zero means all of its kind. */
enum class DevPerformanceScopeKind : uint8_t { Window, Thread };

struct DevPerformanceScope {
	uint64_t id = 0;
	DevPerformanceScopeKind kind = DevPerformanceScopeKind::Window;

	[[nodiscard]] bool operator==(const DevPerformanceScope&) const noexcept = default;
};

/** Persistent view filters; these never change the recorder's capture configuration.
 * Exactly one window or thread scope is active. Zone mode adds a descriptor filter
 * to that scope. An unselected zone means all zones; an empty category mask means none.
 */
struct DevPerformanceSelection {
	std::string zone_search{};
	uint64_t selector_mode = 0;	  // 0: Scope, 1: Zone (FSEL radio binding).
	uint64_t hardware_domain = 0; // 0: Both, 1: CPU Only, 2: GPU Only.
	DevPerformanceScope selected_scope{};
	TimingZoneTypeId selected_zone = 0;
	uint32_t category_mask = (1u << static_cast<uint32_t>(TimingCategory::Count)) - 1u;
	std::array<bool, static_cast<size_t>(TimingCategory::Count)> expanded_categories{};
	bool windows_expanded = true;
	bool threads_expanded = true;

	/** Whether this descriptor and sample domain pass the active view filters. */
	[[nodiscard]] bool accepts_descriptor(const TimingZoneDescriptor& descriptor,
										  TimingSampleDomain domain) const noexcept {
		return (category_mask & timingCategoryBit(descriptor.category)) != 0 &&
			   (hardware_domain == 0 ||
				(hardware_domain == 1 && domain == TimingSampleDomain::Cpu) ||
				(hardware_domain == 2 && domain == TimingSampleDomain::Gpu)) &&
			   (selector_mode == 0 || selected_zone == 0 || selected_zone == descriptor.typeId);
	}

	/** Resolve CPU scope without copying or querying timing history. */
	[[nodiscard]] bool accepts(const CpuTimingRecord& record,
							   const TimingZoneDescriptor& descriptor) const noexcept {
		return record.typeId == descriptor.typeId &&
			   accepts_descriptor(descriptor, TimingSampleDomain::Cpu) &&
			   (selected_scope.id == 0 || (selected_scope.kind == DevPerformanceScopeKind::Window
											   ? record.frame.window == selected_scope.id
											   : record.track == selected_scope.id));
	}

	/** GPU records have no CPU thread identity; thread filters apply only to CPU samples. */
	[[nodiscard]] bool accepts(const GpuTimingRecord& record,
							   const TimingZoneDescriptor& descriptor) const noexcept {
		return record.typeId == descriptor.typeId &&
			   accepts_descriptor(descriptor, TimingSampleDomain::Gpu) &&
			   (selected_scope.kind == DevPerformanceScopeKind::Thread || selected_scope.id == 0 ||
				record.frame.window == selected_scope.id);
	}
};

/** Human-readable timing name; preserves backend descriptor identities. */
[[nodiscard]] std::string performance_zone_label(std::string_view name);
/** Case-insensitive search accepts either backend names or their display labels. */
[[nodiscard]] bool performance_zone_matches(std::string_view name, std::string_view label,
											std::string_view query) noexcept;

} // namespace FlowUi::devSystems
#endif
