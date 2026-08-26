#pragma once

#include "FlowUi/BuildConfig.hpp"

#if FLOW_UI_DEV_MODE

#include "devSystems/devTooling/override/DevOverrideEngine.hpp"
#include "devSystems/devTooling/schema/DevSchemaRegistry.hpp"

namespace FlowUi { class ThemeManager; }

namespace FlowUi::devSystems {

class DevTimingRecorder;
class MemorySampleSink;

/** Development-only shell for live and offline developer-tool surfaces. */
class DevTooling {
public:
	DevTooling() noexcept;
	~DevTooling();

	DevTooling(const DevTooling&) = delete;
	DevTooling& operator=(const DevTooling&) = delete;
	DevTooling(DevTooling&&) = delete;
	DevTooling& operator=(DevTooling&&) = delete;

	[[nodiscard]] devMode::DevSchemaRegistry& schemas() noexcept { return schemas_; }
	[[nodiscard]] const devMode::DevSchemaRegistry& schemas() const noexcept { return schemas_; }
	[[nodiscard]] tooling::DevOverrideEngine& overrides() noexcept { return overrides_; }
	[[nodiscard]] const tooling::DevOverrideEngine& overrides() const noexcept { return overrides_; }

	void commitAtSafePoint(
		ThemeManager& themes,
		DevTimingRecorder* timing = nullptr) noexcept;
#if FLOWUI_DEV_MEMORY_LEVEL >= 2
	void appendDevMemorySamples(MemorySampleSink& sink) const noexcept;
#endif

private:
	devMode::DevSchemaRegistry schemas_{};
	tooling::DevOverrideEngine overrides_;
};

} // namespace FlowUi::devSystems

#endif
