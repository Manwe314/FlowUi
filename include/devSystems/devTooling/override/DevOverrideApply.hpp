#pragma once

#include "FlowUi/BuildConfig.hpp"

#if FLOW_UI_DEV_MODE

#include <array>
#include <cstddef>
#include <cstdint>
#include <unordered_map>
#include <vector>

#include "devSystems/devTooling/override/DevOverrideTypes.hpp"

namespace FlowUi::devSystems { class DevTimingRecorder; class MemorySampleSink; }

namespace FlowUi::devSystems::tooling {

class DevOverrideApply {
public:
	struct Record {
		DevElementOverrideTarget target{};
		DevOverrideFieldKey field{};
		/** One-based index into the bound generation's fields. */
		devMode::DevFieldIndex fieldIndex{};
		DevOverrideLayer layer = DevOverrideLayer::LiveDefinition;
		DevOwnedValue value{};
		std::vector<const devMode::DevFieldOps*> ownerPath{};
		std::uint64_t transaction = 0;
		bool schemaValid = false;
	};

	void bindSchema(devMode::DevSchemaView schema);
	void reserveAdditional(std::size_t count);
	void set(
		const DevElementOverrideTarget& target,
		DevOverrideFieldKey field,
		devMode::DevFieldIndex fieldIndex,
		std::vector<const devMode::DevFieldOps*> ownerPath,
		DevOverrideLayer layer,
		DevOwnedValue value,
		std::uint64_t transaction);
	void clear(
		const DevElementOverrideTarget& target,
		DevOverrideFieldKey field,
		DevOverrideLayer layer) noexcept;
	void resetDefinition(FlowDefinitionID definition) noexcept;
	void resetInstance(const DevElementOverrideTarget& target) noexcept;
	void clearAll() noexcept;
	void rebuildCompiled();

	void apply(
		FlowDefinitionID definition,
		WindowId window,
		::FlowUi::detail::element::ElementInstanceKey instance,
		void* draftParameters,
		DevTimingRecorder* timing = nullptr) const noexcept;

	[[nodiscard]] bool winningLayer(
		FlowDefinitionID definition,
		WindowId window,
		::FlowUi::detail::element::ElementInstanceKey instance,
		devMode::DevFieldId field,
		DevOverrideLayer& result) const noexcept;
	[[nodiscard]] const std::vector<Record>& records() const noexcept { return records_; }
	[[nodiscard]] std::uint64_t appliedFieldCount() const noexcept {
		return appliedFieldCount_;
	}
	[[nodiscard]] std::size_t memoryFootprintBytes() const noexcept;

private:
	struct InstanceKey {
		WindowId window = InvalidWindowId;
		::FlowUi::detail::element::ElementInstanceKey instance{};
		friend bool operator==(InstanceKey, InstanceKey) noexcept = default;
	};
	struct InstanceKeyHash {
		std::size_t operator()(InstanceKey key) const noexcept;
	};
	using LayerRecords = std::array<std::vector<std::uint32_t>, DevOverrideLayerCount>;
	struct CompiledDefinition {
		LayerRecords definition{};
		std::unordered_map<InstanceKey, LayerRecords, InstanceKeyHash> instances{};
	};

	void applyLayer(
		const std::vector<std::uint32_t>& indices,
		void* draftParameters) const noexcept;
	[[nodiscard]] const LayerRecords* findInstanceLayers(
		const CompiledDefinition& compiled,
		WindowId window,
		::FlowUi::detail::element::ElementInstanceKey instance) const noexcept;

	devMode::DevSchemaView schema_{};
	std::vector<Record> records_{};
	std::unordered_map<std::uint64_t, CompiledDefinition> compiled_{};
	mutable std::uint64_t appliedFieldCount_ = 0;
};

} // namespace FlowUi::devSystems::tooling

#endif
