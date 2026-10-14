#pragma once
#include "devSystems/devInterface/Memory/Analysis/DevMemorySession.hpp"
#if FLOW_UI_DEV_MODE
#include "devSystems/devInterface/Memory/Selector/DevMemorySelection.hpp"
namespace FlowUi::devSystems::interface_elements {
/** Normalize physical selector branches to one functional policy identity. */
[[nodiscard]] uint64_t memory_capacity_scope(uint64_t selection) noexcept;
/** Canonical policy owner for a registered capacity. */
[[nodiscard]] uint64_t
memory_capacity_owner(const FlowUi::detail::InitialCapacityDefinition& definition) noexcept;
/** Whether a target's functional owner belongs to a scope. */
[[nodiscard]] bool memory_capacity_contains(uint64_t scope, uint64_t owner) noexcept;
/** Friendly scope name, independent of selector row indexes. */
[[nodiscard]] std::string_view memory_capacity_scope_name(uint64_t scope) noexcept;
/** Friendly target name derived from the stable registered key. */
[[nodiscard]] std::string
memory_capacity_label(const FlowUi::detail::InitialCapacityDefinition& definition);
/** Borrow the nearest explicit policy; an empty view means the tool's default strategy. */
[[nodiscard]] std::span<const MemoryCapacityRule>
memory_capacity_policy_origin(const MemoryAnalysisState& state, uint64_t scope,
							  uint64_t target = 0) noexcept;
/** Load the nearest explicit scope recipe without modifying any prepared settings. */
void select_memory_capacity_scope(MemoryAnalysisState& state, uint64_t scope);
/** Build exact canonical previews from coherent high-water or immutable recording evidence. */
void preview_memory_capacities(MemoryAnalysisState& state, const MemoryLiveRead& live);
/** Atomically prepare this scope recipe, preserving explicit descendants and lock snapshots. */
[[nodiscard]] Status prepare_memory_capacities(MemoryAnalysisState& state);
/** Freeze or unfreeze a prepared scope rule's covered values. */
[[nodiscard]] Status lock_memory_capacity_rule(MemoryAnalysisState& state, size_t rule_index);
/** Remove a specialization and re-resolve its affected values through the inherited policy. */
[[nodiscard]] Status revert_memory_capacity_rule(MemoryAnalysisState& state, size_t rule_index,
												 const MemoryLiveRead& live);
/** Restore the last authoring operation; never changes the live application. */
void undo_memory_capacity_edit(MemoryAnalysisState& state);
} // namespace FlowUi::devSystems::interface_elements
#endif
