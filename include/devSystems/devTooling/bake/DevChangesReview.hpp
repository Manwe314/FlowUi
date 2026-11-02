#pragma once

#include "FlowUi/BuildConfig.hpp"

#if FLOW_UI_DEV_MODE

#include <array>
#include <filesystem>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

#include "devSystems/devTooling/bake/DevBakeTypes.hpp"

namespace FlowUi::devSystems::tooling {

enum class DevChangesDomain : std::uint8_t { Elements, Themes, Memory };
enum class DevChangesOperation : std::uint8_t { Keep, Add, Update, Remove, NoChange };
enum class DevChangesValidity : std::uint8_t { Ready, Unavailable, Unresolved, Conflict, Invalid };
enum class DevChangesDecision : std::uint8_t { IncludeProposal, OmitProposal, RetainDisk, Remove };

/** A manifest decision survives sorting, filtering and schema refresh through its owned key. */
struct DevChangesDecisionEntry {
	std::string key{};
	DevChangesDecision decision = DevChangesDecision::IncludeProposal;
	/** Disk head explicitly reviewed when confirming a proposal or removal after a conflict. */
	std::string disk_revision{};
};

/** Owned adapter snapshot. Missing optional controls retain disk values unless explicitly cleared.
 */
struct DevChangesReviewRequest {
	std::vector<DevChangesDecisionEntry> decisions{};
	std::optional<FlowUi::detail::MemoryCapacityManifest> prepared_memory{};
	/** Earlier owned disk baseline used to expose external same-destination edits on refresh. */
	std::optional<DevBakeManifest> opened_manifest{};
	std::uint64_t memory_revision = 0;
	bool clear_memory_section = false;
	bool clear_growth_factor = false;
	bool clear_runtime_growth = false;
};

/** One persisted destination, with independent operation, validity and decision dimensions. */
struct DevChangesReviewRow {
	std::optional<DevBakeEntry> disk_entry{}, proposed_entry{};
	std::optional<FlowUi::detail::MemoryCapacitySelection> disk_memory{}, proposed_memory{};
	std::string key{}, target_name{}, field_path{}, on_disk{}, proposal{}, next_value{};
	std::string reason{}, origin{}, authored_value{}, compiled_value{}, live_value{};
	DevChangesDomain domain = DevChangesDomain::Elements;
	DevChangesOperation operation = DevChangesOperation::Keep;
	DevChangesValidity validity = DevChangesValidity::Ready;
	DevChangesDecision decision = DevChangesDecision::IncludeProposal;
	bool has_disk = false;
	bool has_proposal = false;
	bool proposal_removes = false;
	bool blocking = false;
};

/** Counts derive from the exact candidate output, independent of visible UI filters. */
struct DevChangesReviewCounts {
	std::array<std::size_t, 3> domains{};
	std::size_t additions = 0, updates = 0, removals = 0, retained = 0;
	std::size_t omitted = 0, conflicts = 0, errors = 0, warnings = 0;
	std::size_t live_edits = 0, prepared_memory = 0;
	/** Number of persisted destinations changed by this plan. */
	[[nodiscard]] std::size_t pending() const noexcept { return additions + updates + removals; }
};

/** Frozen, owned review and exact candidate. Commit revalidates its disk/schema/proposal revisions.
 */
struct DevChangesReviewPlan {
	DevBakeManifest candidate{};
	DevBakeManifest disk_manifest{};
	DevChangesReviewRequest request{};
	std::vector<DevChangesReviewRow> rows{};
	std::filesystem::path manifest_path{};
	std::string disk_revision{}, candidate_json{}, error{};
	DevChangesReviewCounts counts{};
	std::uint64_t schema_generation = 0, proposal_revision = 0, memory_revision = 0;
	bool valid_disk = false;
	bool can_commit = false;
	bool rebuild_required = false;
};

/** Canonical identity includes scope and complete nested path, never a transient row index. */
[[nodiscard]] std::string changes_entry_key(const DevBakeEntry& entry) noexcept;
/** Registered memory capacity identity; shared controls use the reserved policy prefix. */
[[nodiscard]] std::string changes_memory_key(std::string_view key) noexcept;
/** Human-readable operation for shared Changes presentation. */
[[nodiscard]] std::string_view changes_operation_name(DevChangesOperation operation) noexcept;
/** Human-readable validation state for shared Changes presentation. */
[[nodiscard]] std::string_view changes_validity_name(DevChangesValidity validity) noexcept;

} // namespace FlowUi::devSystems::tooling

#endif
