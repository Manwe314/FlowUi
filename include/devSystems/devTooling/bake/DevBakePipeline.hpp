#pragma once

#include "FlowUi/BuildConfig.hpp"

#if FLOW_UI_DEV_MODE

#include <filesystem>
#include <vector>

#include "devSystems/devTooling/bake/DevBakeTypes.hpp"
#include "devSystems/devTooling/bake/DevChangesReview.hpp"

namespace FlowUi::devMode { class DevSchemaRegistry; }
namespace FlowUi::devSystems::tooling { class DevOverrideEngine; }

namespace FlowUi::devSystems::tooling {

/** Serializable reflected value, shared by bake diffs and theme exports. */
struct DevSerializedValue {
	std::string json;
	std::string cpp;
};
/** Format a reflected value without making byte-level assumptions about its type. */
[[nodiscard]] std::optional<DevSerializedValue>
serialize_dev_value(const devMode::DevSchemaGeneration& schema, devMode::DevTypeIndex type,
					const void* value) noexcept;

/** Runtime capture/coalescing side of the manifest-driven baking pipeline. */
class DevBakePipeline {
public:
	DevBakePipeline(
		devMode::DevSchemaRegistry& schemas,
		DevOverrideEngine& overrides) noexcept;

	void setManifestPath(std::filesystem::path path);
	[[nodiscard]] const std::filesystem::path& manifestPath() const noexcept {
		return manifestPath_;
	}

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
	[[nodiscard]] DevBakeStatusSnapshot queryStatus() const noexcept;
	[[nodiscard]] std::vector<DevBakeDiffEntry> queryDiff() const noexcept;
	[[nodiscard]] const DevBakeManifest& manifest() const noexcept { return manifest_; }

private:
	devMode::DevSchemaRegistry& schemas_;
	DevOverrideEngine& overrides_;
	std::filesystem::path manifestPath_{".flowui/changes/active.flowchanges"};
	DevBakeManifest manifest_{};
	DevBakeStatusSnapshot status_{};
};

} // namespace FlowUi::devSystems::tooling

#endif
