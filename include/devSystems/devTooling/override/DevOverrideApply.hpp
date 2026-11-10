#pragma once

#include "FlowUi/Public/BuildConfig.hpp"

#if FLOW_UI_DEV_MODE

#include <array>
#include <cstddef>
#include <cstdint>
#include <unordered_map>
#include <vector>

#include "devSystems/devTooling/override/DevOverrideTypes.hpp"

namespace FlowUi::devSystems { class DevTimingRecorder; class MemorySampleSink; }

namespace FlowUi::devSystems::tooling {

/** @brief Owned layered element assignments and generation-bound application indices. Public record views are borrowed until the next mutation or schema rebind. */
class DevOverrideApply {
public:
	/** @brief Owned assignment with generation-bound member operations; exposed by records(). */
	struct Record {
		/** @brief Owned schema-typed replacement. */
		DevOwnedValue value{};
		/** @brief Scope-qualified element destination. */
		DevElementOverrideTarget target{};
		/** @brief Root type and nested member identity. */
		DevOverrideFieldKey field{};
		/** @brief Borrowed member-operation pointers pinned by the bound schema generation. */
		std::vector<const tooling::DevFieldOps*> ownerPath{};
		/** @brief Transaction that produced the assignment. */
		std::uint64_t transaction = 0;
		/** One-based index into the bound generation's fields. */
		tooling::DevFieldIndex fieldIndex{};
		/** @brief Precedence layer of this retained assignment. */
		DevOverrideLayer layer = DevOverrideLayer::LiveDefinition;
		/** @brief Whether the field path resolved against the bound schema. */
		bool schemaValid = false;
	};

	/** @brief Retain a schema generation and resolve existing field-operation paths against it. */
	void bindSchema(tooling::DevSchemaView schema);
	/** @brief Reserve room for count additional assignments before a batch mutation. */
	void reserveAdditional(std::size_t count);
	/** @brief Move an owned value and resolved member path into a scope/layer assignment; fieldIndex and ownerPath must belong to the bound schema. */
	void set(
		const DevElementOverrideTarget& target,
		DevOverrideFieldKey field,
		tooling::DevFieldIndex fieldIndex,
		std::vector<const tooling::DevFieldOps*> ownerPath,
		DevOverrideLayer layer,
		DevOwnedValue value,
		std::uint64_t transaction);
	/** @brief Remove matching target/field/layer assignments; preserve_baked_assignment suppresses a persistence removal tombstone. */
	void clear(
		const DevElementOverrideTarget& target,
		DevOverrideFieldKey field,
		DevOverrideLayer layer,
		bool preserve_baked_assignment = false) noexcept;
	/** @brief Remove retained live assignments for the selected definition and record required bake removals. */
	void resetDefinition(FlowDefinitionID definition) noexcept;
	/** @brief Remove retained assignments for the exact window-qualified instance and record required bake removals. */
	void resetInstance(const DevElementOverrideTarget& target) noexcept;
	/** @brief Clear retained element assignments and compiled indices. */
	void clearAll() noexcept;
	/** @brief Rebuild definition/instance lookup indices after assignment changes. */
	void rebuildCompiled();

	/** @brief Apply resolved layers in precedence order to correctly typed draftParameters; window/instance select exact-instance assignments. The draft is borrowed only for this call. */
	void apply(
		FlowDefinitionID definition,
		WindowId window,
		::FlowUi::detail::element::ElementInstanceKey instance,
		void* draftParameters,
		DevTimingRecorder* timing = nullptr) const noexcept;

	/** @brief Copy the highest contributing layer to result; return false when no matching assignment exists. */
	[[nodiscard]] bool winningLayer(
		FlowDefinitionID definition,
		WindowId window,
		::FlowUi::detail::element::ElementInstanceKey instance,
		tooling::DevFieldId field,
		DevOverrideLayer& result) const noexcept;
	/** @brief Borrow retained assignment records until the next mutation or schema rebind. */
	[[nodiscard]] const std::vector<Record>& records() const noexcept { return records_; }
	/** @brief Borrow pending manifest-removal records until the next mutation/reset. */
	[[nodiscard]] const std::vector<Record>& bakeTombstones() const noexcept {
		return bakeTombstones_;
	}
	/** @brief Discard acknowledged manifest-removal records. */
	void clearBakeTombstones() noexcept { bakeTombstones_.clear(); }
	/** @brief Return cumulative successfully applied reflected element assignments. */
	[[nodiscard]] std::uint64_t appliedFieldCount() const noexcept {
		return appliedFieldCount_;
	}
	/** @brief Return retained assignment values, paths, and compiled-index backing bytes. */
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

	tooling::DevSchemaView schema_{};
	std::vector<Record> records_{};
	std::vector<Record> bakeTombstones_{};
	std::unordered_map<std::uint64_t, CompiledDefinition> compiled_{};
	mutable std::uint64_t appliedFieldCount_ = 0;
};

} // namespace FlowUi::devSystems::tooling

#endif
