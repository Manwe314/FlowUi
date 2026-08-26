#pragma once

#include "FlowUi/BuildConfig.hpp"

#if FLOW_UI_DEV_MODE

#include <filesystem>
#include <vector>

#include "devSystems/devTooling/bake/DevBakeTypes.hpp"

namespace FlowUi::devMode { class DevSchemaRegistry; }
namespace FlowUi::devSystems::tooling { class DevOverrideEngine; }

namespace FlowUi::devSystems::tooling {

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
