#pragma once
#include <algorithm>
#include <cassert>
#include <cstddef>
#include <iterator>
#include <type_traits>
#include <utility>
#include <vector>

namespace FlowUi::devSystems::detail {
/* Bounded chronological storage. Appending/evicting never moves retained payloads.
 * Slot contents survive eviction for buffer reuse; references expire on reuse/reconfiguration. */
template <class Value>
class ReportingRing {
	std::vector<Value> slots_{};
	size_t head_ = 0, count_ = 0;
	template <bool Constant>
	class Iterator {
		using Ring = std::conditional_t<Constant, const ReportingRing, ReportingRing>;
		Ring* ring_ = nullptr;
		size_t offset_ = 0;

	public:
		using iterator_category = std::forward_iterator_tag;
		using value_type = Value;
		using difference_type = std::ptrdiff_t;
		using reference = std::conditional_t<Constant, const Value&, Value&>;
		using pointer = std::conditional_t<Constant, const Value*, Value*>;
		Iterator() noexcept = default;
		Iterator(Ring& ring, size_t offset) noexcept : ring_(&ring), offset_(offset) {}
		[[nodiscard]] reference operator*() const noexcept { return (*ring_)[offset_]; }
		[[nodiscard]] pointer operator->() const noexcept { return &**this; }
		Iterator& operator++() noexcept {
			++offset_;
			return *this;
		}
		Iterator operator++(int) noexcept {
			auto previous = *this;
			++*this;
			return previous;
		}
		[[nodiscard]] bool operator==(const Iterator&) const noexcept = default;
	};

public:
	explicit ReportingRing(size_t capacity = 1) { set_capacity(capacity); }
	[[nodiscard]] size_t size() const noexcept { return count_; }
	[[nodiscard]] size_t capacity() const noexcept { return slots_.size(); }
	[[nodiscard]] bool empty() const noexcept { return !count_; }
	[[nodiscard]] Value& operator[](size_t offset) noexcept {
		return slots_[(head_ + offset) % slots_.size()];
	}
	[[nodiscard]] const Value& operator[](size_t offset) const noexcept {
		return slots_[(head_ + offset) % slots_.size()];
	}
	[[nodiscard]] Value& front() noexcept {
		assert(count_);
		return (*this)[0];
	}
	[[nodiscard]] const Value& front() const noexcept {
		assert(count_);
		return (*this)[0];
	}
	[[nodiscard]] Value& back() noexcept {
		assert(count_);
		return (*this)[count_ - 1];
	}
	[[nodiscard]] const Value& back() const noexcept {
		assert(count_);
		return (*this)[count_ - 1];
	}
	[[nodiscard]] Iterator<false> begin() noexcept { return {*this, 0}; }
	[[nodiscard]] Iterator<false> end() noexcept { return {*this, count_}; }
	[[nodiscard]] Iterator<true> begin() const noexcept { return {*this, 0}; }
	[[nodiscard]] Iterator<true> end() const noexcept { return {*this, count_}; }
	void pop_front() noexcept {
		assert(count_);
		head_ = (head_ + 1) % slots_.size();
		--count_;
	}
	/* Withdraw an uncommitted tail slot when admitting its metadata fails. */
	void pop_back() noexcept {
		assert(count_);
		--count_;
	}
	/* Admit a reusable slot; callers account for eviction before using this operation. */
	[[nodiscard]] Value& append_slot() noexcept {
		assert(count_ < slots_.size());
		return (*this)[count_++];
	}
	void push_back(Value value) {
		if (count_ == slots_.size())
			pop_front();
		append_slot() = std::move(value);
	}
	/* Explicit reconfiguration is the only operation that moves retained payloads. */
	void set_capacity(size_t capacity) {
		capacity = std::max(size_t{1}, capacity);
		if (capacity == slots_.size())
			return;
		std::vector<Value> replacement(capacity);
		const auto retained = std::min(count_, capacity);
		const auto skipped = count_ - retained;
		for (size_t offset = 0; offset < retained; ++offset)
			replacement[offset] = std::move((*this)[skipped + offset]);
		slots_.swap(replacement);
		head_ = 0;
		count_ = retained;
	}
};
} // namespace FlowUi::devSystems::detail
