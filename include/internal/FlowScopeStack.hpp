#pragma once

#include <cstddef>
#include <stdexcept>
#include <vector>

#include "FlowUi/ElementID.hpp"

namespace FlowUi::detail::manager_storage {

/** Per-window semantic element ancestry for the active FlowUi frame. */
class FlowScopeStack {
public:
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

	[[nodiscard]] size_t push(FlowElementID id) {
		if (!id) throw std::invalid_argument("FlowUi cannot enter an invalid element scope.");
		const size_t priorDepth = scopes_.size();
		scopes_.push_back(id);
		return priorDepth;
	}

	void restore(size_t depth) noexcept {
		if (depth <= scopes_.size()) scopes_.resize(depth);
	}

private:
	std::vector<FlowElementID> scopes_{};
};

} // namespace FlowUi::detail::manager_storage
