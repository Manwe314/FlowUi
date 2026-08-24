#pragma once

#include <algorithm>
#include <bit>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <new>
#include <stdexcept>
#include <type_traits>
#include <utility>

#include "FlowUi/Error.hpp"

namespace FlowUi::detail::storage {

struct AlignedRecordLayout {
	size_t headerBytes = 0;
	size_t payloadOffset = 0;
	size_t payloadBytes = 0;
	size_t payloadAlignment = 0;
	size_t allocationBytes = 0;
	size_t allocationAlignment = 0;

	[[nodiscard]] constexpr explicit operator bool() const noexcept {
		return headerBytes != 0 && payloadBytes != 0 && allocationBytes != 0;
	}

	[[nodiscard]] void* payload(void* allocation) const noexcept {
		return allocation
			? static_cast<void*>(static_cast<std::byte*>(allocation) + payloadOffset)
			: nullptr;
	}

	[[nodiscard]] const void* payload(const void* allocation) const noexcept {
		return allocation
			? static_cast<const void*>(static_cast<const std::byte*>(allocation) + payloadOffset)
			: nullptr;
	}
};

[[nodiscard]] inline AlignedRecordLayout makeAlignedRecordLayout(
	size_t headerBytes,
	size_t headerAlignment,
	size_t payloadBytes,
	size_t payloadAlignment) {
	if (headerBytes == 0 || payloadBytes == 0) {
		throw FlowUiException(makeError(ErrorCode::StorageConfigurationInvalid, ErrorSite::ResourceCreatePersistentRecord));
	}
	if (!std::has_single_bit(headerAlignment) || !std::has_single_bit(payloadAlignment)) {
		throw FlowUiException(makeError(ErrorCode::StorageConfigurationInvalid, ErrorSite::ResourceCreatePersistentRecord));
	}
	if (headerBytes > std::numeric_limits<size_t>::max() - (payloadAlignment - 1u)) {
		throw FlowUiException(makeError(ErrorCode::ArithmeticOverflow, ErrorSite::ResourceCreatePersistentRecord));
	}
	const size_t payloadOffset =
		(headerBytes + payloadAlignment - 1u) & ~(payloadAlignment - 1u);
	if (payloadBytes > std::numeric_limits<size_t>::max() - payloadOffset) {
		throw FlowUiException(makeError(ErrorCode::ArithmeticOverflow, ErrorSite::ResourceCreatePersistentRecord));
	}
	return AlignedRecordLayout{
		.headerBytes = headerBytes,
		.payloadOffset = payloadOffset,
		.payloadBytes = payloadBytes,
		.payloadAlignment = payloadAlignment,
		.allocationBytes = payloadOffset + payloadBytes,
		.allocationAlignment = std::max(headerAlignment, payloadAlignment),
	};
}

template <typename Header, typename Payload>
[[nodiscard]] inline AlignedRecordLayout makeAlignedRecordLayout() {
	return makeAlignedRecordLayout(
		sizeof(Header), alignof(Header), sizeof(Payload), alignof(Payload));
}

template <typename Header, typename Payload, typename... PayloadArgs>
Header* constructAlignedRecord(
	void* allocation,
	const AlignedRecordLayout& layout,
	PayloadArgs&&... payloadArgs) {
	static_assert(std::is_nothrow_destructible_v<Header>);
	static_assert(std::is_nothrow_destructible_v<Payload>);
	if (!allocation || layout.headerBytes < sizeof(Header) ||
		layout.payloadBytes < sizeof(Payload) ||
		layout.payloadAlignment < alignof(Payload) ||
		reinterpret_cast<uintptr_t>(allocation) % alignof(Header) != 0 ||
		reinterpret_cast<uintptr_t>(layout.payload(allocation)) % alignof(Payload) != 0) {
		detail::terminateForFatalError(makeError(ErrorCode::InternalInvariantBroken, ErrorSite::ResourceCreatePersistentRecord));
	}

	Header* header = ::new (allocation) Header();
	try {
		::new (layout.payload(allocation)) Payload(std::forward<PayloadArgs>(payloadArgs)...);
	} catch (...) {
		header->~Header();
		throw;
	}
	return header;
}

template <typename Header, typename Payload>
void destroyAlignedRecord(void* allocation, const AlignedRecordLayout& layout) noexcept {
	if (!allocation) return;
	static_cast<Payload*>(layout.payload(allocation))->~Payload();
	static_cast<Header*>(allocation)->~Header();
}

} // namespace FlowUi::detail::storage
