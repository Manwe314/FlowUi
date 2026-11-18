#pragma once

#include <algorithm>
#include <bit>
#include <limits>

#include "FlowUi/Private/storageSystem/StorageTypes.hpp"
#include "FlowUi/Public/errorManagement/Error.hpp"

namespace FlowUi::detail::storage {

template <ErrorCode errorCode>
[[noreturn]] inline void reportStorageCalculationError() {
	constexpr FlowUiError error = makeError(errorCode, ErrorSite::StorageCalculateCapacity);
	if constexpr (error.descriptor().category == ErrorCategory::Fatal) {
		FlowUi::detail::terminateForFatalError(error);
	} else {
		throw FlowUiException(error);
	}
}

[[nodiscard]] constexpr uint64_t checkedMultiply(uint64_t leftValue, uint64_t rightValue) {
	if (leftValue != 0 && rightValue > std::numeric_limits<uint64_t>::max() / leftValue) {
		reportStorageCalculationError<ErrorCode::ArithmeticOverflow>();
	}
	return leftValue * rightValue;
}

[[nodiscard]] constexpr uint64_t checkedAdd(uint64_t leftValue, uint64_t rightValue) {
	if (rightValue > std::numeric_limits<uint64_t>::max() - leftValue) {
		reportStorageCalculationError<ErrorCode::ArithmeticOverflow>();
	}
	return leftValue + rightValue;
}

[[nodiscard]] constexpr size_t checkedSize(uint64_t value) {
	if (value > std::numeric_limits<size_t>::max()) {
		reportStorageCalculationError<ErrorCode::ArithmeticOverflow>();
	}
	return static_cast<size_t>(value);
}

[[nodiscard]] constexpr size_t alignUp(size_t value, size_t alignment) {
	if (!std::has_single_bit(alignment)) {
		reportStorageCalculationError<ErrorCode::StorageConfigurationInvalid>();
	}
	if (value > std::numeric_limits<size_t>::max() - (alignment - 1u)) {
		reportStorageCalculationError<ErrorCode::ArithmeticOverflow>();
	}
	return (value + alignment - 1u) & ~(alignment - 1u);
}

[[nodiscard]] constexpr uint32_t nextGeneration(uint32_t generation) noexcept {
	++generation;
	return generation == InvalidGeneration ? 1u : generation;
}

[[nodiscard]] constexpr uint32_t bytesPerPixel(PixelFormat format) noexcept {
	switch (format) {
	case PixelFormat::R8Unorm:
		return 1u;
	case PixelFormat::Rgba16Float:
		return 8u;
	case PixelFormat::D32Float:
		return 4u;
	case PixelFormat::Rgba8Unorm:
	case PixelFormat::Rgba8Srgb:
	case PixelFormat::Bgra8Unorm:
	case PixelFormat::Bgra8Srgb:
		return 4u;
	default:
		return 0u;
	}
}

[[nodiscard]] constexpr uint64_t estimateImageBytes(const ImageDesc& desc) {
	uint64_t bytes = 0;
	uint64_t width = desc.width;
	uint64_t height = desc.height;
	uint64_t depth = desc.depth;
	for (uint32_t mipIndex = 0; mipIndex < desc.mipLevels; ++mipIndex) {
		uint64_t mipBytes = checkedMultiply(width, height);
		mipBytes = checkedMultiply(mipBytes, depth);
		mipBytes = checkedMultiply(mipBytes, desc.layers);
		mipBytes = checkedMultiply(mipBytes, bytesPerPixel(desc.format));
		bytes = checkedAdd(bytes, mipBytes);
		width = std::max<uint64_t>(1, width / 2u);
		height = std::max<uint64_t>(1, height / 2u);
		depth = std::max<uint64_t>(1, depth / 2u);
	}
	return bytes;
}

} // namespace FlowUi::detail::storage
