#pragma once

#include "FlowUi/Private/ResultSupport.hpp"
#include "FlowUi/Public/errorManagement/ErrorValue.hpp"
#include <expected>
#include <type_traits>
#include <utility>
#include <variant>

namespace FlowUi {

#if defined(__cpp_lib_expected) && __cpp_lib_expected >= 202202L

/** Owned operation result or compact FlowUiError. Test has_value() before value()/dereference; error() requires failure. Accessing the inactive alternative is invalid.
 * @tparam T Successful result type; void represents success without a value. */
template <typename T>
using Result = std::expected<T, FlowUiError>;

/** Result for an operation that succeeds without returning a value. */
using Status = Result<void>;

/** Wrap a compact error for constructing a failed Result or Status. */
[[nodiscard]] constexpr std::unexpected<FlowUiError> unexpectedError(FlowUiError error) noexcept {
	return std::unexpected<FlowUiError>(error);
}

#else

/** Compatibility result used when the C++ library lacks std::expected. Owns exactly one successful value or FlowUiError; check has_value() before accessing either alternative. Inactive access throws std::bad_variant_access.
 * @tparam T Owned successful result type. */
template <typename T>
class Result {
public:
	/** Construct a successful result by default-constructing T. */
	Result()
		requires std::is_default_constructible_v<T>
		: storage_(T{}) {}
	/** Copy a successful value. */
	Result(const T& value) : storage_(value) {}
	/** Move a successful value. */
	Result(T&& value) : storage_(std::move(value)) {}
	/** Construct a failure from unexpectedError(). */
	Result(detail::UnexpectedFlowUiError failure) : storage_(failure.error) {}

	/** Return whether the successful alternative is active. */
	[[nodiscard]] bool has_value() const noexcept { return storage_.index() == 0u; }
	/** Return whether the operation succeeded. */
	[[nodiscard]] explicit operator bool() const noexcept { return has_value(); }
	/** Borrow the successful value; throws std::bad_variant_access on failure. */
	[[nodiscard]] T& value() & { return std::get<0>(storage_); }
	/** Borrow the successful value as const; throws std::bad_variant_access on failure. */
	[[nodiscard]] const T& value() const& { return std::get<0>(storage_); }
	/** Move from the successful value; throws std::bad_variant_access on failure. */
	[[nodiscard]] T&& value() && { return std::get<0>(std::move(storage_)); }
	/** Borrow the successful value; requires success. */
	[[nodiscard]] T& operator*() & { return value(); }
	/** Borrow the successful value as const; requires success. */
	[[nodiscard]] const T& operator*() const& { return value(); }
	/** Return a pointer to the successful value; requires success. */
	[[nodiscard]] T* operator->() { return &value(); }
	/** Return a const pointer to the successful value; requires success. */
	[[nodiscard]] const T* operator->() const { return &value(); }
	/** Borrow the error; throws std::bad_variant_access on success. */
	[[nodiscard]] FlowUiError& error() & { return std::get<1>(storage_); }
	/** Borrow the error as const; throws std::bad_variant_access on success. */
	[[nodiscard]] const FlowUiError& error() const& { return std::get<1>(storage_); }

	template <typename U>
	/** Copy the successful value, or convert fallback to T on failure. */
	[[nodiscard]] T value_or(U&& fallback) const& {
		return has_value() ? std::get<0>(storage_) : static_cast<T>(std::forward<U>(fallback));
	}

private:
	std::variant<T, FlowUiError> storage_;
};

/** Compatibility result for no-value operations; default construction represents success. */
template <>
class Result<void> {
public:
	/** Construct a successful no-value result. */
	Result() = default;
	/** Construct a failed no-value result from unexpectedError(). */
	Result(detail::UnexpectedFlowUiError failure) : error_(failure.error), hasValue_(false) {}
	/** Return whether the no-value operation succeeded. */
	[[nodiscard]] bool has_value() const noexcept { return hasValue_; }
	/** Return whether the operation succeeded. */
	[[nodiscard]] explicit operator bool() const noexcept { return has_value(); }
	/** Validate success; throws std::bad_variant_access on failure. */
	void value() const {
		if (!hasValue_)
			throw std::bad_variant_access{};
	}
	/** Borrow the stored error; meaningful only on failure. */
	[[nodiscard]] FlowUiError& error() & { return error_; }
	/** Borrow the stored error as const; meaningful only on failure. */
	[[nodiscard]] const FlowUiError& error() const& { return error_; }

private:
	FlowUiError error_{};
	bool hasValue_ = true;
};

/** Result for an operation that succeeds without returning a value. */
using Status = Result<void>;

/** Wrap a compact error for constructing a failed compatibility Result or Status. */
[[nodiscard]] constexpr detail::UnexpectedFlowUiError unexpectedError(FlowUiError error) noexcept {
	return detail::UnexpectedFlowUiError{error};
}

#endif

} // namespace FlowUi
