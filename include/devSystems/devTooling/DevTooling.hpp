#pragma once

/** @namespace FlowUi::devSystems::tooling
 * @brief Reflection, live editing, catalogues and immutable UI tree extension APIs.
 */

/** @file
 * @brief Public DevTooling declarations for optional development services.
 */

#include "FlowUi/Public/BuildConfig.hpp"

#if FLOW_UI_DEV_MODE

#include "devSystems/DevConfig.hpp"
#include "FlowUi/Public/identificationTypes/WindowId.hpp"
#include "devSystems/devTooling/bake/DevBakePipeline.hpp"
#include "devSystems/devTooling/catalogue/DevCatalogues.hpp"
#include "devSystems/devTooling/overlay/DevOverlayService.hpp"
#include "devSystems/devTooling/inspect/DevInspectInteractionController.hpp"
#include "devSystems/devTooling/override/DevOverrideEngine.hpp"
#include "devSystems/devTooling/schema/DevSchemaRegistry.hpp"

namespace FlowUi { class ThemeManager; }

namespace FlowUi::devSystems {

class DevTimingRecorder;
class MemorySampleSink;

/** Development-only shell for live and offline developer-tool surfaces. */
class DevTooling {
public:
	/**
	 * @brief Construct schema, override and capture services using the supplied development
	 * limits.
	 */
	explicit DevTooling(const DevToolingConfig& config = {}) noexcept;
	/**
	 * @brief Destroy owned tooling services; borrowed references returned by this object expire.
	 */
	~DevTooling();

	DevTooling(const DevTooling&) = delete;
	DevTooling& operator=(const DevTooling&) = delete;
	DevTooling(DevTooling&&) = delete;
	DevTooling& operator=(DevTooling&&) = delete;

	/** @brief Borrow the schema registry; this owner controls its lifetime. */
	[[nodiscard]] tooling::DevSchemaRegistry& schemas() noexcept { return schemas_; }
	/** @brief Borrow the schema registry; this owner controls its lifetime. */
	[[nodiscard]] const tooling::DevSchemaRegistry& schemas() const noexcept { return schemas_; }
	/** @brief Borrow the live-edit override engine; this owner controls its lifetime. */
	[[nodiscard]] tooling::DevOverrideEngine& overrides() noexcept { return overrides_; }
	/** @brief Borrow the live-edit override engine; this owner controls its lifetime. */
	[[nodiscard]] const tooling::DevOverrideEngine& overrides() const noexcept { return overrides_; }
	/**
	 * @brief Borrow manifest persistence and bake review services; this owner controls their
	 * lifetime.
	 */
	[[nodiscard]] tooling::DevBakePipeline& bakePipeline() noexcept { return bakePipeline_; }
	/**
	 * @brief Borrow manifest persistence and bake review services; this owner controls their
	 * lifetime.
	 */
	[[nodiscard]] const tooling::DevBakePipeline& bakePipeline() const noexcept {
		return bakePipeline_;
	}
	/** @brief Borrow the development overlay renderer; this owner controls its lifetime. */
	[[nodiscard]] tooling::DevOverlayService& overlays() noexcept { return overlays_; }
	/** @brief Borrow the development overlay renderer; this owner controls its lifetime. */
	[[nodiscard]] const tooling::DevOverlayService& overlays() const noexcept { return overlays_; }
	/**
	 * @brief Borrow the development resource and element catalogues; this owner controls their
	 * lifetime.
	 */
	[[nodiscard]] tooling::DevCatalogues& catalogues() noexcept { return catalogues_; }
	/**
	 * @brief Borrow the development resource and element catalogues; this owner controls their
	 * lifetime.
	 */
	[[nodiscard]] const tooling::DevCatalogues& catalogues() const noexcept { return catalogues_; }

	/** Session-owned application-window inspection controller. */
	[[nodiscard]] tooling::DevInspectInteractionController& inspect_interaction() noexcept { return inspect_interaction_; }
	/** @brief Borrow the session inspection controller without mutation. */
	[[nodiscard]] const tooling::DevInspectInteractionController& inspect_interaction() const noexcept { return inspect_interaction_; }

	/** Attachment surface for externally supplied overlays. */
	void setOverlaySelection(
		WindowId window,
		const tooling::DevOverlaySelectionSpec& selection) noexcept;
	/** @brief Remove any stored external overlay selection for window. */
	void clearOverlaySelection(WindowId window) noexcept;
	/**
	 * @brief Copy the current window selection to outSelection; return false when no selection is
	 * stored.
	 */
	[[nodiscard]] bool overlaySelection(
		WindowId window,
		tooling::DevOverlaySelectionSpec& outSelection) const noexcept;

	/** Persist the current bakeable live edits to the active .flowchanges manifest. */
	tooling::DevCommandResult bakeActiveEdits() noexcept;
	/** @brief Return an owned snapshot of manifest and compiled bake status. */
	[[nodiscard]] tooling::DevBakeStatusSnapshot queryBakeStatus() const noexcept;
	/**
	 * @brief Return owned entries describing differences between active and compiled edits.
	 */
	[[nodiscard]] std::vector<tooling::DevBakeDiffEntry> queryBakeDiff() const noexcept;

	/**
	 * @brief Apply queued edits to themes outside UI drawing; optional timing attributes the work
	 * to a recorder.
	 */
	void commitAtSafePoint(
		ThemeManager& themes,
		DevTimingRecorder* timing = nullptr) noexcept;
#if FLOWUI_DEV_MEMORY_LEVEL >= 2
	/**
	 * @brief Append current tooling capacity samples to sink without consuming producer records.
	 */
	void appendDevMemorySamples(MemorySampleSink& sink) const noexcept;
#endif

private:
	struct WindowOverlaySelection {
		WindowId window = InvalidWindowId;
		tooling::DevOverlaySelectionSpec selection{};
	};

	tooling::DevSchemaRegistry schemas_{};
	tooling::DevOverrideEngine overrides_;
	tooling::DevBakePipeline bakePipeline_;
	tooling::DevOverlayService overlays_{};
	tooling::DevInspectInteractionController inspect_interaction_{};
	tooling::DevCatalogues catalogues_{};
	std::vector<WindowOverlaySelection> overlaySelections_{};
};

} // namespace FlowUi::devSystems

#endif
