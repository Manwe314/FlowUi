#pragma once

#include "internal/MemoryDemandPeaks.hpp"
#include <cstddef>
#include <stdexcept>
#include <vector>

#include "FlowUi/ElementID.hpp"
#include "FlowUi/Error.hpp"

namespace FlowUi::detail::manager_storage {

/** Per-window semantic element ancestry for the active FlowUi frame. */
class FlowScopeStack {
public:
	/** Bind the application monitoring gate before use. */
	void bind_memory_demand(const ::FlowUi::detail::MemoryDemandControl* control) noexcept {
		capacity_demand_.bind(control);
	}
	void reserve(size_t capacity) { scopes_.reserve(capacity); }

	void beginFrame() {
		scopes_.clear();
		scopes_.push_back(RootFlowScopeID);
	}

	void cancelFrame() noexcept { scopes_.clear(); }

	[[nodiscard]] FlowElementID current() const noexcept {
		return scopes_.empty() ? RootFlowScopeID : scopes_.back();
	}

	[[nodiscard]] size_t depth() const noexcept { return scopes_.size(); }
	[[nodiscard]] uint64_t consume_demand() const noexcept {
		return capacity_demand_.consume(InitialCapacityTarget::ui_flow_scope_depth, scopes_.size());
	}
	[[nodiscard]] size_t capacity() const noexcept { return scopes_.capacity(); }

	[[nodiscard]] size_t push(FlowElementID id) {
		if (!id) throw FlowUiException(makeError(ErrorCode::InvalidElementId, ErrorSite::UiManagerOpenElement));
		const size_t priorDepth = scopes_.size();
		scopes_.push_back(id);
		FLOWUI_MEMORY_DEMAND_RECORD(capacity_demand_, InitialCapacityTarget::ui_flow_scope_depth,
									scopes_.size());
		return priorDepth;
	}

	void restore(size_t depth) noexcept {
		if (depth <= scopes_.size()) scopes_.resize(depth);
	}

private:
	std::vector<FlowElementID> scopes_{};
	::FlowUi::detail::MemoryDemandPeaks capacity_demand_{};
};

} // namespace FlowUi::detail::manager_storage
