#pragma once

#include "FlowUi/Public/BuildConfig.hpp"

#if FLOW_UI_DEV_MODE

#include <filesystem>
#include <vector>

#include "devSystems/devTooling/bake/DevBakeTypes.hpp"
#include "devSystems/devTooling/bake/DevChangesReview.hpp"

namespace FlowUi::devSystems::tooling {

class DevSchemaRegistry;
class DevOverrideEngine;

/** Resolve a current authored field, rejecting differing values within definition scope. */
[[nodiscard]] DevValueCaptureStatus query_authored_element_value(
	const DevOverrideEngine& overrides, const tooling::DevSchemaGeneration& schema,
	const DevElementOverrideTarget& target, const DevOverrideFieldKey& field,
	DevOwnedValue& destination) noexcept;
/** Resolve a saved element field path against the current authored observations. */
[[nodiscard]] DevValueCaptureStatus query_authored_element_value(
	const DevOverrideEngine& overrides, const tooling::DevSchemaGeneration& schema,
	const DevBakeEntry& entry, WindowId window, DevOwnedValue& destination) noexcept;

/** Serializable reflected value, shared by bake diffs and theme exports. */
struct DevSerializedValue {
	/** @brief Owned reflected JSON representation. */
	std::string json;
	/** @brief Owned validated C++ initializer representation. */
	std::string cpp;
};
/** Format a reflected value without making byte-level assumptions about its type. */
[[nodiscard]] std::optional<DevSerializedValue>
serialize_dev_value(const tooling::DevSchemaGeneration& schema, tooling::DevTypeIndex type,
					const void* value) noexcept;

/** Runtime capture/coalescing side of the manifest-driven baking pipeline. */
class DevBakePipeline {
public:
	/** @brief Borrow the registry and override engine for the pipeline lifetime; no manifest is written by construction. */
	DevBakePipeline(
		tooling::DevSchemaRegistry& schemas,
		DevOverrideEngine& overrides) noexcept;

	/** @brief Replace the owned destination path for subsequent reads/reviews/commits; this does not write the file. */
	void setManifestPath(std::filesystem::path path);
	/** @brief Borrow the current native manifest path until the next path change or destruction. */
	[[nodiscard]] const std::filesystem::path& manifestPath() const noexcept {
		return manifestPath_;
	}

	/** @brief Capture currently bakeable live edits and persist their manifest; return an explicit command result. */
	DevCommandResult bake() noexcept;
	/** Build one owned review snapshot for all domains without changing live values or disk. */
	[[nodiscard]] DevChangesReviewPlan
	query_review(const DevChangesReviewRequest& request = {}) const noexcept;
	/** Commit the reviewed revision once. A supplied current memory revision guards staged edits.
	 */
	[[nodiscard]] DevCommandResult
	commit_review(const DevChangesReviewPlan& plan,
				  std::uint64_t current_memory_revision = 0) noexcept;
	/** Upsert selected capacities without requiring an element schema; review export does not
	 * activate. */
	[[nodiscard]] DevCommandResult
	save_memory_capacities(const FlowUi::detail::MemoryCapacityManifest& selections,
						   std::span<const std::string_view> removed = {},
						   const std::filesystem::path& review_path = {}) noexcept;
	/** Remove all memory settings while retaining element/theme entries. */
	[[nodiscard]] DevCommandResult reset_memory_capacities() noexcept;
	/** @brief Return an owned current eligibility/status snapshot without changing live values. */
	[[nodiscard]] DevBakeStatusSnapshot queryStatus() const noexcept;
	/** @brief Return owned authored/live/compiled comparison rows without changing live values. */
	[[nodiscard]] std::vector<DevBakeDiffEntry> queryDiff() const noexcept;
	/** @brief Borrow the pipeline's retained manifest until the next bake/review commit or owner destruction. */
	[[nodiscard]] const DevBakeManifest& manifest() const noexcept { return manifest_; }

private:
	tooling::DevSchemaRegistry& schemas_;
	DevOverrideEngine& overrides_;
	std::filesystem::path manifestPath_{".flowui/changes/active.flowchanges"};
	DevBakeManifest manifest_{};
	DevBakeStatusSnapshot status_{};
};

} // namespace FlowUi::devSystems::tooling

#endif
