#pragma once

#include "FlowUi/Public/BuildConfig.hpp"

#if FLOW_UI_DEV_MODE

#include <array>
#include <filesystem>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

#include "devSystems/devTooling/bake/DevBakeTypes.hpp"

namespace FlowUi::devSystems::tooling {

/** @brief Independent persisted destination domain in a Changes review. */
enum class DevChangesDomain : std::uint8_t {
	/** @brief Element parameter assignments. */
	Elements,
	/** @brief Named theme assignments. */
	Themes,
	/** @brief Memory capacities and shared growth policy. */
	Memory
};
/** @brief Effect of a review decision on the exact candidate manifest. */
enum class DevChangesOperation : std::uint8_t {
	/** @brief Retain the saved destination. */
	Keep,
	/** @brief Create a destination absent on disk. */
	Add,
	/** @brief Replace the saved destination value. */
	Update,
	/** @brief Delete the saved destination. */
	Remove,
	/** @brief Produce no persisted difference. */
	NoChange
};
/** @brief Resolution and conflict state independent of review decisions. */
enum class DevChangesValidity : std::uint8_t {
	/** @brief Destination is resolved and valid. */
	Ready,
	/** @brief Required observation or source is unavailable. */
	Unavailable,
	/** @brief Destination cannot be resolved against current metadata. */
	Unresolved,
	/** @brief Disk and proposed edits require an explicit review decision. */
	Conflict,
	/** @brief Candidate value or metadata failed validation. */
	Invalid
};
/** @brief User-selected treatment of one persisted destination. */
enum class DevChangesDecision : std::uint8_t {
	/** @brief Use the current live/staged proposal. */
	IncludeProposal,
	/** @brief Exclude the proposal from this review candidate. */
	OmitProposal,
	/** @brief Keep the current on-disk assignment. */
	RetainDisk,
	/** @brief Remove the destination from the next manifest. */
	Remove
};

/** A manifest decision survives sorting, filtering and schema refresh through its owned key. */
struct DevChangesDecisionEntry {
	/** @brief Owned canonical destination key, independent of row position. */
	std::string key{};
	/** @brief User-selected treatment of this destination in the next manifest. */
	DevChangesDecision decision = DevChangesDecision::IncludeProposal;
	/** Disk head explicitly reviewed when confirming a proposal or removal after a conflict. */
	std::string disk_revision{};
};

/** Owned adapter snapshot. Missing optional controls retain disk values unless explicitly cleared.
 */
struct DevChangesReviewRequest {
	/** @brief Owned per-destination decisions to apply when building the candidate. */
	std::vector<DevChangesDecisionEntry> decisions{};
	/** @brief Optional staged capacity selection payload. */
	std::optional<FlowUi::detail::MemoryCapacityManifest> prepared_memory{};
	/** Earlier owned disk baseline used to expose external same-destination edits on refresh. */
	std::optional<DevBakeManifest> opened_manifest{};
	/** @brief Revision of the staged memory selection that this request reviewed. */
	std::uint64_t memory_revision = 0;
	/** @brief Explicitly remove all persisted memory settings. */
	bool clear_memory_section = false;
	/** @brief Explicitly remove the saved growth-factor control. */
	bool clear_growth_factor = false;
	/** @brief Explicitly remove the saved runtime-growth control. */
	bool clear_runtime_growth = false;
};

/** One persisted destination, with independent operation, validity and decision dimensions. */
struct DevChangesReviewRow {
	/** @var std::optional<DevBakeEntry> disk_entry
	 * @brief Owned existing reflected assignment, when present.
	 */
	/** @var std::optional<DevBakeEntry> proposed_entry
	 * @brief Owned proposed reflected assignment, when present.
	 */
	std::optional<DevBakeEntry> disk_entry{}, proposed_entry{};
	/** @var std::optional<FlowUi::detail::MemoryCapacitySelection> disk_memory
	 * @brief Owned existing memory selection, when present.
	 */
	/** @var std::optional<FlowUi::detail::MemoryCapacitySelection> proposed_memory
	 * @brief Owned staged memory selection, when present.
	 */
	std::optional<FlowUi::detail::MemoryCapacitySelection> disk_memory{}, proposed_memory{};
	/** @var std::string key
	 * @brief Canonical owned destination identity.
	 */
	/** @var std::string target_name
	 * @brief Owned target display label.
	 */
	/** @var std::string field_path
	 * @brief Owned complete nested member or capacity path.
	 */
	/** @var std::string on_disk
	 * @brief Formatted current disk value.
	 */
	/** @var std::string proposal
	 * @brief Formatted live/staged proposed value.
	 */
	/** @var std::string next_value
	 * @brief Formatted value produced by the current decision.
	 */
	std::string key{}, target_name{}, field_path{}, on_disk{}, proposal{}, next_value{};
	/** @var std::string reason
	 * @brief Owned validation/conflict explanation.
	 */
	/** @var std::string origin
	 * @brief Owned provenance/source explanation.
	 */
	/** @var std::string authored_value
	 * @brief Formatted application-authored baseline.
	 */
	/** @var std::string compiled_value
	 * @brief Formatted value embedded in the executable.
	 */
	/** @var std::string live_value
	 * @brief Formatted current effective value.
	 */
	std::string reason{}, origin{}, authored_value{}, compiled_value{}, live_value{};
	/** @brief Element, theme, or memory destination domain. */
	DevChangesDomain domain = DevChangesDomain::Elements;
	/** @brief Change that the current decision produces in the candidate. */
	DevChangesOperation operation = DevChangesOperation::Keep;
	/** @brief Independent resolution/conflict/validation state. */
	DevChangesValidity validity = DevChangesValidity::Ready;
	/** @brief Decision currently applied to this destination. */
	DevChangesDecision decision = DevChangesDecision::IncludeProposal;
	/** @brief Whether the destination exists in the on-disk manifest. */
	bool has_disk = false;
	/** @brief Whether a live or staged proposal exists. */
	bool has_proposal = false;
	/** @brief Whether the proposal requests removal of the destination. */
	bool proposal_removes = false;
	/** @brief Whether this row prevents committing the current candidate. */
	bool blocking = false;
};

/** Counts derive from the exact candidate output, independent of visible UI filters. */
struct DevChangesReviewCounts {
	/** @brief Candidate row counts indexed by DevChangesDomain. */
	std::array<std::size_t, 3> domains{};
	/** @var std::size_t additions
	 * @brief Number of new persisted destinations.
	 */
	/** @var std::size_t updates
	 * @brief Number of changed persisted destinations.
	 */
	/** @var std::size_t removals
	 * @brief Number of removed persisted destinations.
	 */
	/** @var std::size_t retained
	 * @brief Number of destinations retained from disk.
	 */
	std::size_t additions = 0, updates = 0, removals = 0, retained = 0;
	/** @var std::size_t omitted
	 * @brief Number of proposals omitted by review decisions.
	 */
	/** @var std::size_t conflicts
	 * @brief Number of conflicting destinations.
	 */
	/** @var std::size_t errors
	 * @brief Number of validation errors.
	 */
	/** @var std::size_t warnings
	 * @brief Number of nonblocking warnings.
	 */
	std::size_t omitted = 0, conflicts = 0, errors = 0, warnings = 0;
	/** @var std::size_t live_edits
	 * @brief Number of considered live edits.
	 */
	/** @var std::size_t prepared_memory
	 * @brief Number of staged memory selections.
	 */
	std::size_t live_edits = 0, prepared_memory = 0;
	/** Number of persisted destinations changed by this plan. */
	[[nodiscard]] std::size_t pending() const noexcept { return additions + updates + removals; }
};

/** Frozen, owned review and exact candidate. Commit revalidates its disk/schema/proposal revisions.
 */
struct DevChangesReviewPlan {
	/** @brief Owned exact next manifest produced by this plan. */
	DevBakeManifest candidate{};
	/** @brief Owned manifest read while computing the plan. */
	DevBakeManifest disk_manifest{};
	/** @brief Owned review decisions and staged selection inputs. */
	DevChangesReviewRequest request{};
	/** @brief Owned rows across every domain, before UI filtering. */
	std::vector<DevChangesReviewRow> rows{};
	/** @brief Native path whose exact revision must still match at commit. */
	std::filesystem::path manifest_path{};
	/** @var std::string disk_revision
	 * @brief Exact disk-content revision reviewed by this plan.
	 */
	/** @var std::string candidate_json
	 * @brief Owned exact serialized candidate contents.
	 */
	/** @var std::string error
	 * @brief Owned plan-level diagnostic when review failed.
	 */
	std::string disk_revision{}, candidate_json{}, error{};
	/** @brief Candidate-wide change, conflict, and diagnostic totals. */
	DevChangesReviewCounts counts{};
	/** @var std::uint64_t schema_generation
	 * @brief Schema publication used to prepare this plan.
	 */
	/** @var std::uint64_t proposal_revision
	 * @brief Live proposal revision reviewed by this plan.
	 */
	/** @var std::uint64_t memory_revision
	 * @brief Staged memory revision reviewed by this plan.
	 */
	std::uint64_t schema_generation = 0, proposal_revision = 0, memory_revision = 0;
	/** @brief Whether the current disk manifest was successfully read and validated. */
	bool valid_disk = false;
	/** @brief Whether all required inputs resolved and no blocking row remains. */
	bool can_commit = false;
	/** @brief Whether candidate changes require regenerating/rebuilding compiled defaults. */
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
