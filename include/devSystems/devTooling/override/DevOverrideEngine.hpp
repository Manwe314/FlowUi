#pragma once

#include "FlowUi/BuildConfig.hpp"

#if FLOW_UI_DEV_MODE

#include <cstddef>
#include <cstdint>
#include <deque>
#include <mutex>
#include <string_view>
#include <type_traits>
#include <vector>

#include "devSystems/devTooling/override/DevOverrideApply.hpp"
#include "devSystems/devTooling/override/DevOverrideCapture.hpp"
#include "devSystems/devTooling/schema/DevSchemaRegistry.hpp"
#include "internal/TypeOperations.hpp"

namespace FlowUi { class ThemeManager; }
namespace FlowUi::devSystems { class DevTimingRecorder; }

namespace FlowUi::devSystems::tooling {

struct DevOverrideEngineConfig {
	std::uint32_t maximumPendingTransactions = 256;
	std::uint32_t maximumCommandsPerTransaction = 4096;
	std::uint32_t maximumPendingCommands = 16384;
};

class DevOverrideEngine {
public:
	struct ThemeBakeRecord {
		DevThemeOverrideTarget target{};
		DevOverrideFieldKey field{};
		devMode::DevFieldIndex fieldIndex{};
		DevOwnedValue original{};
		DevOwnedValue value{};
		std::vector<const devMode::DevFieldOps*> ownerPath{};
		std::uint64_t transaction = 0;
		bool schemaValid = false;
		bool dirty = false;
	};

	explicit DevOverrideEngine(
		devMode::DevSchemaRegistry& schemas,
		DevOverrideEngineConfig config = {}) noexcept;
	~DevOverrideEngine();

	DevOverrideEngine(const DevOverrideEngine&) = delete;
	DevOverrideEngine& operator=(const DevOverrideEngine&) = delete;

	[[nodiscard]] bool submit(DevChangeSet changeSet);

	template <typename T>
	devMode::DevValueOperationStatus copyValue(
		const T& source,
		DevOwnedValue& destination) const noexcept {
		using Value = std::remove_cvref_t<T>;
		const devMode::DevSchemaView schema = schemas_.view();
		if (!schema) return devMode::DevValueOperationStatus::Unsupported;
		const devMode::DevTypeSchema* type = schema->findType(::FlowUi::detail::typeHash<Value>());
		if (!type) return devMode::DevValueOperationStatus::Unsupported;
		const auto index = devMode::DevTypeIndex{
			static_cast<std::uint32_t>(type - schema->types.data())};
		return DevOwnedValue::copyFrom(*schema, index, std::addressof(source), destination);
	}

	void commitAtSafePoint(
		ThemeManager& themes,
		DevTimingRecorder* timing = nullptr) noexcept;
	void beginWindowFrame(WindowId window, std::uint64_t frameNumber);
	void endWindowFrame(WindowId window) noexcept;
	void cancelWindowFrame(WindowId window) noexcept;

	void applyElement(
		FlowDefinitionID definition,
		WindowId window,
		::FlowUi::detail::element::ElementInstanceKey instance,
		void* draftParameters,
		DevTimingRecorder* timing = nullptr) const noexcept;
	void captureElement(
		FlowDefinitionID definition,
		WindowId window,
		::FlowUi::detail::element::ElementInstanceKey instance,
		std::uint32_t flowNode,
		const void* effectiveParameters,
		DevTimingRecorder* timing = nullptr) noexcept;

	[[nodiscard]] const DevOverrideApply& appliedOverrides() const noexcept {
		return apply_;
	}
	[[nodiscard]] DevOverrideApply& appliedOverrides() noexcept { return apply_; }
	[[nodiscard]] const std::vector<ThemeBakeRecord>& themeBakeRecords() const noexcept {
		return themeRecords_;
	}
	[[nodiscard]] const std::vector<ThemeBakeRecord>& themeBakeTombstones() const noexcept {
		return themeBakeTombstones_;
	}
	void clearThemeBakeTombstones() noexcept { themeBakeTombstones_.clear(); }
	[[nodiscard]] const DevElementCaptureSnapshot& elementSnapshot(
		WindowId window) const noexcept {
		return capture_.elements(window);
	}
	[[nodiscard]] const DevThemeCaptureSnapshot& themeSnapshot() const noexcept {
		return capture_.themes();
	}
	[[nodiscard]] const std::vector<DevCommandResult>& commandResults() const noexcept {
		return results_;
	}
	[[nodiscard]] DevOverrideStats stats() const noexcept;
	[[nodiscard]] std::size_t memoryFootprintBytes() const noexcept;

private:
	struct ResolvedField {
		const devMode::DevFieldSchema* schema = nullptr;
		devMode::DevFieldIndex index{};
		std::vector<const devMode::DevFieldOps*> ownerPath{};
		DevOwnedValue originalThemeValue{};
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
		devMode::DevTypeIndex owner,
		DevOverrideFieldKey key) const noexcept;
	void applyCommand(
		DevOverrideCommand command,
		ResolvedField resolved,
		std::uint64_t transaction,
		ThemeManager& themes);
	void bindThemeRecords();
	void applyThemeRecords(ThemeManager& themes) noexcept;
	[[nodiscard]] bool themeVariantExists(
		const ThemeManager& themes,
		const DevThemeOverrideTarget& target) const noexcept;
	[[nodiscard]] bool captureOriginalThemeField(
		const ThemeManager& themes,
		const DevThemeOverrideTarget& target,
		const ResolvedField& field,
		DevOwnedValue& destination) const noexcept;
	[[nodiscard]] static bool themeFieldIsOverridden(
		const void* owner,
		devMode::DevTypeId themeType,
		std::string_view variant,
		devMode::DevFieldId field,
		DevOverrideLayer& layer) noexcept;

	devMode::DevSchemaRegistry& schemas_;
	DevOverrideEngineConfig config_{};
	devMode::DevSchemaView schema_{};
	DevOverrideApply apply_{};
	DevOverrideCapture capture_{};
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
