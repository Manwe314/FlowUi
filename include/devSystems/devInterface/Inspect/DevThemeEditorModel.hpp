#pragma once
#include "FlowUi/BuildConfig.hpp"
#if FLOW_UI_DEV_MODE
#include "devSystems/devTooling/override/DevOverrideTypes.hpp"
#include "managers/ThemeManager.hpp"
#include <span>
#include <string>
#include <vector>

namespace FlowUi {
class App;
}
namespace FlowUi::devSystems::interface_elements::theme_editor {
using ThemeView = ThemeManager::DevThemePayloadView;
struct ThemeLeaf {
	tooling::DevOverrideFieldKey key{};
	std::string path{};
	const devMode::DevFieldSchema* field = nullptr;
	const void* live = nullptr;
	const void* authored = nullptr;
	const void* startup = nullptr;
};

/** Semantic comparison distinguishes unsupported values from equality. */
enum class ValueComparison : uint8_t { Equal, Different, Unavailable };
/** Compare reflected values without comparing object representation or padding. */
[[nodiscard]] ValueComparison compare_values(const devMode::DevSchemaGeneration& schema,
											 devMode::DevTypeIndex type, const void* left,
											 const void* right) noexcept;

/** Cached presentation and baseline differences for one reflected token. */
struct ThemeTokenValue {
	std::string authored_text{}, startup_text{}, live_text{};
	std::string authored_display{}, startup_display{}, live_display{}, live_cpp{};
	bool changed = false;
	bool live_edit = false;
	bool startup_edit = false;
	bool comparison_available = true;
	bool exportable = false;
};
/** One variant's metadata and revision-dependent display/export results. */
struct ThemePresentation {
	ThemeView source{};
	std::string variant{}, type_name{}, code{};
	std::vector<ThemeLeaf> fields{};
	std::vector<ThemeTokenValue> values{};
	std::vector<std::size_t> changed_indices{};
	std::size_t assignment_count = 0, omitted_count = 0;
	bool source_usable = false;
};
/** Session-owned model shared by all three theme editor surfaces. */
class ThemeEditorModel {
public:
	/** Refresh views, rebuilding only variants whose schema, identity or revision changed. */
	[[nodiscard]] Status refresh(App& app) noexcept;
	/** Current catalogue views, valid until the next refresh or theme mutation. */
	[[nodiscard]] std::span<const ThemeView> catalogue() const noexcept { return catalogue_; }
	/** Find revision-dependent presentation for a view in the current catalogue. */
	[[nodiscard]] const ThemePresentation* find(const ThemeView& theme) const noexcept;
	/** Whether variants disagree on a top-level token or group. */
	[[nodiscard]] bool mixed(devMode::DevTypeId type, devMode::DevFieldId group) const noexcept;
	/** Monotonic presentation generation, including active-variant changes. */
	[[nodiscard]] uint64_t generation() const noexcept { return generation_; }
	/** Cumulative metadata/value rebuild counters for performance regression checks. */
	[[nodiscard]] uint64_t leaf_rebuild_count() const noexcept { return leaf_rebuild_count_; }
	[[nodiscard]] uint64_t value_refresh_count() const noexcept { return value_refresh_count_; }
	/** Presentation formatting calls; excludes unsupported comparator fallback serialization. */
	[[nodiscard]] uint64_t serialization_count() const noexcept { return serialization_count_; }

private:
	struct MixedGroup {
		devMode::DevTypeId type = 0;
		devMode::DevFieldId group = 0;
	};
	std::vector<ThemePresentation> presentations_{};
	std::vector<ThemeView> catalogue_{}, pending_catalogue_{};
	std::vector<MixedGroup> mixed_groups_{};
	devMode::DevSchemaView schema_{};
	uint64_t generation_ = 0, leaf_rebuild_count_ = 0, value_refresh_count_ = 0;
	uint64_t serialization_count_ = 0;
};
} // namespace FlowUi::devSystems::interface_elements::theme_editor
#endif
