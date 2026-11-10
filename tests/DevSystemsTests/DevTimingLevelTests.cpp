#ifdef NDEBUG
#undef NDEBUG
#endif
#include "devSystems/devMonitoringAndReporting/timing/DevTimingZone.hpp"
#include <algorithm>
#include <cassert>
#include <limits>

using namespace FlowUi;
using namespace FlowUi::devSystems;

int main() {
	DevTiming timing({.cpuLevel = CpuTimingLevel::Deep});
	assert(static_cast<unsigned>(timing.config().cpuLevel) == FLOWUI_DEV_TIMING_LEVEL);
	auto attachment = timing.attachCurrentThread("timing-level-contract");
	assert(attachment);
	auto& recorder = attachment.recorder();
	for (unsigned requested_level = 0; requested_level <= 3; ++requested_level) {
		auto configuration = timing.config();
		configuration.cpuLevel = static_cast<CpuTimingLevel>(requested_level);
		timing.setConfig(configuration);
		const unsigned effective_level =
			std::min(requested_level, unsigned(FLOWUI_DEV_TIMING_LEVEL));
		assert(static_cast<unsigned>(timing.config().cpuLevel) == effective_level);
		unsigned argument_evaluations = 0;
		const auto request_recorder = [&]() {
			++argument_evaluations;
			return &recorder;
		};
		{ CpuTimingZone frame(recorder, timing_zones::kWindowFrameTotal); }
		{
			FLOWUI_DEV_TIMING_ZONE_IF(request_recorder(), TimingCategory::User,
									  TimingZoneRole::Work, "test.summary");
		}
		{
			FLOWUI_DEV_TIMING_ZONE_BALANCED_IF(request_recorder(), TimingCategory::User,
											   TimingZoneRole::Work, "test.balanced");
		}
		{
			FLOWUI_DEV_TIMING_ZONE_DEEP_IF(request_recorder(), TimingCategory::User,
										   TimingZoneRole::Work, "test.deep");
		}
		assert(argument_evaluations == FLOWUI_DEV_TIMING_LEVEL);
		assert(timing.drainCompletedRecords().size() == 1u + effective_level);
		configuration.enabledCategoryMask = 0;
		timing.setConfig(configuration);
		{ CpuTimingZone frame(recorder, timing_zones::kWindowFrameTotal); }
		assert(timing.drainCompletedRecords().empty());
		configuration.enabledCategoryMask = 0xFFFFFFFFu;
		timing.setConfig(configuration);
	}
#if FLOWUI_DEV_TIMING_LEVEL >= 1
	for (unsigned requested_level = 1; requested_level <= 3; ++requested_level) {
		for (const bool selected : {false, true}) {
			auto configuration = timing.config();
			configuration.cpuLevel = static_cast<CpuTimingLevel>(requested_level);
			configuration.balancedElementRetentionThresholdNs =
				std::numeric_limits<uint64_t>::max();
			configuration.selectedElementInstance = selected ? FlowElementID{45} : FlowElementID{};
			timing.setConfig(configuration);
			{
				ElementTimingZone element(&recorder, FlowDefinitionID{123}, FlowElementID{45});
				element.end();
			}
			const auto effective_level = timing.config().cpuLevel;
			const bool retained = effective_level == CpuTimingLevel::Deep ||
								  (effective_level == CpuTimingLevel::Balanced && selected);
			assert(timing.drainCompletedRecords().size() == (retained ? 1u : 0u));
			const auto aggregates = timing.drainElementTimingAggregates();
			assert(aggregates.size() == 1u && aggregates.front().invocationCount == 1u);
		}
	}
#endif
}
