#pragma once
#include "FlowUi/BuildConfig.hpp"
#if FLOW_UI_AGENTIC_WORKFLOW
#include "devSystems/devAgenticWorkflow/AgenticManifest.hpp"

namespace FlowUi::devSystems::agentic {

/** Aggregate cadence from continuous valid timestamp segments, never reciprocal averages. */
[[nodiscard]] Result<Json>
analyze_performance(std::span<const AgenticFrameRecord> frames,
					std::optional<std::uint64_t> window_id = std::nullopt) noexcept;
/** Rank native retained CPU/GPU scopes using observed exclusive durations, preserving inclusive
 * totals, queue/track scope, quality and exact source/evidence selectors. No causality is inferred.
 */
[[nodiscard]] Result<Json>
analyze_timing_artifact(const AgenticManifest& manifest,
						std::optional<std::uint64_t> window_id = std::nullopt,
						std::uint32_t top = 10, bool include_source = false,
						std::uint64_t maximum_bytes = 32768) noexcept;
/** Losslessly reconstruct frame records from the normalized frames artifact. */
[[nodiscard]] Result<std::vector<AgenticFrameRecord>>
read_frames(const AgenticManifest& manifest) noexcept;
/** Evaluate repeated independent passes, rejecting insufficient/interfered evidence. */
[[nodiscard]] Result<Json> check_objective(std::span<const AgenticManifest> passes,
										   const AgenticObjective& objective) noexcept;
/** Compare workload/build/hardware/coverage before claiming any improvement. */
[[nodiscard]] Result<Json>
compare_captures(const AgenticManifest& baseline, const AgenticManifest& candidate,
				 const std::optional<AgenticObjective>& objective = std::nullopt) noexcept;
/** Normalize external compiler/test logs in their own domain, preserving raw locations. */
[[nodiscard]] Result<Json>
ingest_build_diagnostics(std::string_view log, std::string_view producer, std::string_view build_id,
						 std::uint64_t maximum_bytes = 1024u * 1024u) noexcept;
/** Classify retained issues without claiming unexercised paths were fixed. */
[[nodiscard]] Result<Json> diff_diagnostics(const Json& baseline, const Json& candidate,
											bool same_exercised_scenario) noexcept;

} // namespace FlowUi::devSystems::agentic

#endif // FLOW_UI_AGENTIC_WORKFLOW
