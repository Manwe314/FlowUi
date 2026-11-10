#pragma once

#include "FlowUi/Public/BuildConfig.hpp"

#if FLOW_UI_DEV_MODE

#include <cstddef>
#include <cstdint>
#include <deque>
#include <mutex>
#include <string_view>
#include <type_traits>
#include <vector>

#include "FlowUi/Public/identificationTypes/ElementID.hpp"
#include "FlowUi/Public/identificationTypes/WindowId.hpp"
#include "devSystems/DevConfig.hpp"
#include "devSystems/devTooling/override/DevOverrideApply.hpp"
#include "devSystems/devTooling/override/DevOverrideCapture.hpp"
#include "devSystems/devTooling/schema/DevSchemaRegistry.hpp"
#include "FlowUi/Private/TypeOperations.hpp"

namespace FlowUi { class ThemeManager; }
namespace FlowUi::devSystems { class DevTimingRecorder; }

namespace FlowUi::devSystems::tooling {

/** @brief Queues owned change sets for validated safe-point application and captures their effective/authored values. Schema registry and app theme services remain caller-owned. */
class DevOverrideEngine {
public:
	/** @brief Owned theme assignment and baseline values used by live preview and persistence review. */
	struct ThemeBakeRecord {
		/** @brief Owned original values for each affected variant. */
		std::vector<std::pair<std::string, DevOwnedValue>> variant_originals{};
		/** @brief Owned pre-assignment live baseline. */
		DevOwnedValue original{};
		/** @brief Owned application-authored baseline when available. */
		DevOwnedValue authored{};
		/** @brief Owned committed replacement value. */
		DevOwnedValue value{};
		/** @brief Owned temporary preview value. */
		DevOwnedValue preview_value{};
		/** @brief Reflected theme type and named variant destination. */
		DevThemeOverrideTarget target{};
		/** @brief Root type and nested member identity. */
		DevOverrideFieldKey field{};
		/** @brief Borrowed member-operation path pinned by the active schema generation. */
		std::vector<const tooling::DevFieldOps*> ownerPath{};
		/** @brief Transaction that produced this assignment. */
		std::uint64_t transaction = 0;
		/** @brief Generation-local reflected leaf field index. */
		tooling::DevFieldIndex fieldIndex{};
		/** @brief Whether the retained field resolves against the active schema. */
		bool schemaValid = false;
		/** @brief Whether the assignment awaits application to theme storage. */
		bool dirty = false;
		/** @brief Whether the assignment belongs only to ephemeral preview. */
		bool preview_only = false;
		/** @brief Whether the next manifest should remove the compiled assignment. */
		bool remove_baked = false;
	};

	/** @brief Borrow schemas for this engine's lifetime and copy queue/capture limits from config. */
	explicit DevOverrideEngine(
		tooling::DevSchemaRegistry& schemas,
		DevOverrideEngineConfig config = {}) noexcept;
	/** @brief Release owned commands, assignments, and captures; the borrowed registry remains alive. */
	~DevOverrideEngine();

	DevOverrideEngine(const DevOverrideEngine&) = delete;
	DevOverrideEngine& operator=(const DevOverrideEngine&) = delete;

	/** @brief Move a transaction into the bounded ingress queue; return false when configured transaction/command capacities reject ingress. Validation, including empty-transaction rejection, occurs later at commitAtSafePoint(). */
	[[nodiscard]] bool submit(DevChangeSet changeSet);

	/** @brief Copy a registered typed value into destination; return Unsupported when its schema/adapter is unavailable. Destination owns the copy and retained text referents. */
	template <typename T>
	tooling::DevValueOperationStatus copyValue(
		const T& source,
		DevOwnedValue& destination) const noexcept {
		using Value = std::remove_cvref_t<T>;
		const tooling::DevSchemaView schema = schemas_.view();
		if (!schema) return tooling::DevValueOperationStatus::Unsupported;
		const tooling::DevTypeSchema* type = schema->findType(::FlowUi::detail::typeHash<Value>());
		if (!type) return tooling::DevValueOperationStatus::Unsupported;
		const auto index = tooling::DevTypeIndex{
			static_cast<std::uint32_t>(type - schema->types.data())};
		return DevOwnedValue::copyFrom(*schema, index, std::addressof(source), destination);
	}

	/** @brief Validate queued transactions and apply theme updates outside UI construction; borrowed timing optionally attributes the work. */
	void commitAtSafePoint(
		ThemeManager& themes,
		DevTimingRecorder* timing = nullptr) noexcept;
	/** @brief Begin window-local capture using the current published schema generation. */
	void beginWindowFrame(WindowId window, std::uint64_t frameNumber);
	/** @brief Publish this window's successfully captured values. */
	void endWindowFrame(WindowId window) noexcept;
	/** @brief Discard in-progress window capture and preserve prior published observations. */
	void cancelWindowFrame(WindowId window) noexcept;
	/** Release completed and in-progress observations for a retired window. */
	void release_window(WindowId window) noexcept { capture_.release_window(window); }
	/** Disable optional value snapshots; committed overrides continue to apply. */
	void set_value_collection(bool enabled) noexcept { value_collection_enabled_ = enabled; }
	/** @brief Return whether optional owned-value snapshots are enabled. */
	[[nodiscard]] bool value_collection() const noexcept { return value_collection_enabled_; }

	/** @brief Apply retained layered assignments to correctly typed borrowed draftParameters for this definition/window/instance. */
	void applyElement(
		FlowDefinitionID definition,
		WindowId window,
		::FlowUi::detail::element::ElementInstanceKey instance,
		void* draftParameters,
		DevTimingRecorder* timing = nullptr) const noexcept;
	/** Own the current invocation's application inputs before baked/live changes. */
	[[nodiscard]] DevAuthoredElementCapture captureAuthoredParameters(
		FlowDefinitionID definition, const void* parameters) const noexcept;
	/** @brief Copy one invocation's effective parameters and optional state/resources when value collection is enabled; inputs are borrowed for the call. */
	void captureElement(
		FlowDefinitionID definition,
		WindowId window,
		::FlowUi::detail::element::ElementInstanceKey instance,
		std::uint32_t flowNode,
		const void* effectiveParameters,
		DevTimingRecorder* timing = nullptr,
		DevAuthoredElementCapture authored = {},
		const void* state = nullptr,
		const void* resources = nullptr,
		bool isDevInternal = false) noexcept;

	/** @brief Borrow the element assignment store; views expire on its next mutation or schema rebind. */
	[[nodiscard]] const DevOverrideApply& appliedOverrides() const noexcept {
		return apply_;
	}
	/** @brief Borrow the element assignment store; views expire on its next mutation or schema rebind. */
	[[nodiscard]] DevOverrideApply& appliedOverrides() noexcept { return apply_; }
	/** @brief Borrow current theme assignments and baseline values until the next safe-point mutation. */
	[[nodiscard]] const std::vector<ThemeBakeRecord>& themeBakeRecords() const noexcept {
		return themeRecords_;
	}
	/** @brief Borrow pending theme manifest-removal records until the next safe-point mutation/reset. */
	[[nodiscard]] const std::vector<ThemeBakeRecord>& themeBakeTombstones() const noexcept {
		return themeBakeTombstones_;
	}
	/** @brief Discard acknowledged theme manifest-removal records. */
	void clearThemeBakeTombstones() noexcept { themeBakeTombstones_.clear(); }
	/** @brief Borrow the latest completed value snapshot for window until its next publication. */
	[[nodiscard]] const DevElementCaptureSnapshot& elementSnapshot(
		WindowId window) const noexcept {
		return capture_.elements(window);
	}
	/** Visit published values without exposing mutable capture buffers. */
	void visitElementSnapshots(void* context,
		bool (*visitor)(void*, const DevElementCaptureSnapshot&) noexcept) const noexcept {
		if (value_collection_enabled_)
			capture_.visitElementSnapshots(context, visitor);
	}
	/** @brief Borrow the latest theme value snapshot until its next publication. */
	[[nodiscard]] const DevThemeCaptureSnapshot& themeSnapshot() const noexcept {
		return capture_.themes();
	}
	/** @brief Borrow command acknowledgements from the latest safe-point processing until the next commit. */
	[[nodiscard]] const std::vector<DevCommandResult>& commandResults() const noexcept {
		return results_;
	}
	/** @brief Copy transaction totals, active assignment counts, and memory accounting. */
	[[nodiscard]] DevOverrideStats stats() const noexcept;
	/** @brief Return retained command, override, and capture backing storage bytes. */
	[[nodiscard]] std::size_t memoryFootprintBytes() const noexcept;

private:
	struct ResolvedField {
		const tooling::DevFieldSchema* schema = nullptr;
		tooling::DevFieldIndex index{};
		std::vector<const tooling::DevFieldOps*> ownerPath{};
		DevOwnedValue originalThemeValue{};
		DevOwnedValue authoredThemeValue{};
		std::vector<std::pair<std::string, DevOwnedValue>> variant_originals{};
	};
	void syncSchema();
	[[nodiscard]] DevCommandStatus validate(
		const DevOverrideCommand& command,
		const ThemeManager& themes,
		ResolvedField& resolved) const noexcept;
	[[nodiscard]] DevCommandStatus validateValue(
		const DevOverrideCommand& command,
		const ResolvedField& resolved) const noexcept;
	[[nodiscard]] ResolvedField resolveRootField(
		tooling::DevTypeIndex owner,
		DevOverrideFieldKey key) const noexcept;
	void applyCommand(
		DevOverrideCommand command,
		ResolvedField resolved,
		std::uint64_t transaction,
		ThemeManager& themes);
	void bindThemeRecords();
	void applyThemeRecords(ThemeManager& themes) noexcept;
	// Move referenced resource names to the manager's stable pool before payload assignment.
	[[nodiscard]] bool stabilize_theme_references(ThemeManager& themes, DevOwnedValue& value,
		tooling::DevTypeIndex type) const noexcept;
	[[nodiscard]] bool themeVariantExists(
		const ThemeManager& themes,
		const DevThemeOverrideTarget& target) const noexcept;
	[[nodiscard]] bool captureOriginalThemeField(const ThemeManager& themes,
												 const DevThemeOverrideTarget& target,
												 const ResolvedField& field,
												 DevOwnedValue& destination,
												 bool authored = false) const noexcept;
	[[nodiscard]] static bool themeFieldIsOverridden(
		const void* owner,
		tooling::DevTypeId themeType,
		std::string_view variant,
		tooling::DevFieldId field,
		DevOverrideLayer& layer) noexcept;

	tooling::DevSchemaRegistry& schemas_;
	DevOverrideEngineConfig config_{};
	tooling::DevSchemaView schema_{};
	DevOverrideApply apply_{};
	DevOverrideCapture capture_{};
	bool value_collection_enabled_ = true;
	std::vector<ThemeBakeRecord> themeRecords_{};
	std::vector<ThemeBakeRecord> themeBakeTombstones_{};
	mutable std::mutex ingressMutex_{};
	std::deque<DevChangeSet> ingress_{};
	std::size_t pendingCommandCount_ = 0;
	std::vector<DevCommandResult> results_{};
	std::uint64_t committedTransactions_ = 0;
	std::uint64_t rejectedTransactions_ = 0;
};

} // namespace FlowUi::devSystems::tooling

#endif
