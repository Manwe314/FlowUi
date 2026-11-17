#pragma once

#include "FlowUi/Private/ResultSupport.hpp"
#include "FlowUi/Public/errorManagement/ErrorValue.hpp"
#include <expected>
#include <type_traits>
#include <utility>
#include <variant>

namespace FlowUi {

#if defined(__cpp_lib_expected) && __cpp_lib_expected >= 202202L

/** Owned operation result or compact FlowUiError. Test has_value() before value()/dereference; error() requires failure. value() on failure throws std::bad_expected_access<FlowUiError>; dereference/arrow require success and error() requires failure. The common portable interface is documented in docs/api/values.md; std::expected extensions are library-version dependent.
 * @tparam T Successful result type; void represents success without a value.
 * @par Example
 * @code{.cpp}
 * const std::string title = "Inspector";
 * FlowUi::Result<std::string> copied{title};
 * FlowUi::Result<std::string> moved{std::string{"Inspector"}};
 * FlowUi::Result<std::string> failed = FlowUi::unexpectedError(FlowUi::makeError(
 *     FlowUi::ErrorCode::InvalidWindowConfiguration, FlowUi::ErrorSite::AppCreateWindow));
 * FlowUi::Result<std::string> emptyTitle{}; // Successful empty string.
 * @endcode
 */
template <typename T>
using Result = std::expected<T, FlowUiError>;

/** Result for an operation that succeeds without returning a value.
 * @par Example
 * @code{.cpp}
 * FlowUi::Status success{};
 * if (success.has_value())
 *     success.value(); // No payload.
 *
 * FlowUi::Status failed = FlowUi::unexpectedError(FlowUi::makeError(
 *     FlowUi::ErrorCode::ObjectNotInitialized, FlowUi::ErrorSite::AppInitialize));
 * if (!failed) {
 *     FlowUi::FlowUiError& error = failed.error();
 *     const FlowUi::Status& readOnly = failed;
 *     const FlowUi::FlowUiError& observedError = readOnly.error();
 * }
 * @endcode
 */
using Status = Result<void>;

/** Wrap a compact error for constructing a failed Result or Status.
 * @par Example
 * @code{.cpp}
 * const std::string title = "Inspector";
 * FlowUi::Result<std::string> copied{title};
 * FlowUi::Result<std::string> moved{std::string{"Inspector"}};
 * FlowUi::Result<std::string> failed = FlowUi::unexpectedError(FlowUi::makeError(
 *     FlowUi::ErrorCode::InvalidWindowConfiguration, FlowUi::ErrorSite::AppCreateWindow));
 * FlowUi::Result<std::string> emptyTitle{}; // Successful empty string.
 * @endcode
 */
[[nodiscard]] constexpr std::unexpected<FlowUiError> unexpectedError(FlowUiError error) noexcept {
	return std::unexpected<FlowUiError>(error);
}

#else

/** Compatibility result used when the C++ library lacks std::expected. Owns exactly one successful value or FlowUiError; check has_value() before accessing either alternative. value(), dereference/arrow and non-void error() throw std::bad_variant_access on inactive access. This fallback does not supply monadic operations, emplace, swap or std::expected-only overloads. Its value operations can throw; Result is a container, not an exception boundary.
 * @tparam T Owned successful result type.
 * @par Example
 * @code{.cpp}
 * const std::string title = "Inspector";
 * FlowUi::Result<std::string> copied{title};
 * FlowUi::Result<std::string> moved{std::string{"Inspector"}};
 * FlowUi::Result<std::string> failed = FlowUi::unexpectedError(FlowUi::makeError(
 *     FlowUi::ErrorCode::InvalidWindowConfiguration, FlowUi::ErrorSite::AppCreateWindow));
 * FlowUi::Result<std::string> emptyTitle{}; // Successful empty string.
 * @endcode
 */
template <typename T>
class Result {
public:
	/** Construct a successful result by default-constructing T.
	 * @par Example
	 * @code{.cpp}
	 * const std::string title = "Inspector";
	 * FlowUi::Result<std::string> copied{title};
	 * FlowUi::Result<std::string> moved{std::string{"Inspector"}};
	 * FlowUi::Result<std::string> failed = FlowUi::unexpectedError(FlowUi::makeError(
	 *     FlowUi::ErrorCode::InvalidWindowConfiguration, FlowUi::ErrorSite::AppCreateWindow));
	 * FlowUi::Result<std::string> emptyTitle{}; // Successful empty string.
	 * @endcode
	 */
	Result()
		requires std::is_default_constructible_v<T>
		: storage_(T{}) {}
	/** Copy a successful value.
	 * @par Example
	 * @code{.cpp}
	 * const std::string title = "Inspector";
	 * FlowUi::Result<std::string> copied{title};
	 * FlowUi::Result<std::string> moved{std::string{"Inspector"}};
	 * FlowUi::Result<std::string> failed = FlowUi::unexpectedError(FlowUi::makeError(
	 *     FlowUi::ErrorCode::InvalidWindowConfiguration, FlowUi::ErrorSite::AppCreateWindow));
	 * FlowUi::Result<std::string> emptyTitle{}; // Successful empty string.
	 * @endcode
	 */
	Result(const T& value) : storage_(value) {}
	/** Move a successful value.
	 * @par Example
	 * @code{.cpp}
	 * const std::string title = "Inspector";
	 * FlowUi::Result<std::string> copied{title};
	 * FlowUi::Result<std::string> moved{std::string{"Inspector"}};
	 * FlowUi::Result<std::string> failed = FlowUi::unexpectedError(FlowUi::makeError(
	 *     FlowUi::ErrorCode::InvalidWindowConfiguration, FlowUi::ErrorSite::AppCreateWindow));
	 * FlowUi::Result<std::string> emptyTitle{}; // Successful empty string.
	 * @endcode
	 */
	Result(T&& value) : storage_(std::move(value)) {}
	/** Construct a failure from unexpectedError().
	 * @par Example
	 * @code{.cpp}
	 * const std::string title = "Inspector";
	 * FlowUi::Result<std::string> copied{title};
	 * FlowUi::Result<std::string> moved{std::string{"Inspector"}};
	 * FlowUi::Result<std::string> failed = FlowUi::unexpectedError(FlowUi::makeError(
	 *     FlowUi::ErrorCode::InvalidWindowConfiguration, FlowUi::ErrorSite::AppCreateWindow));
	 * FlowUi::Result<std::string> emptyTitle{}; // Successful empty string.
	 * @endcode
	 */
	Result(detail::UnexpectedFlowUiError failure) noexcept : storage_(failure.error) {}

	/** Return whether the successful alternative is active.
	 * @par Example
	 * @code{.cpp}
	 * if (!result)
	 *     return FlowUi::unexpectedError(result.error()); // Preserve the original failure.
	 * @endcode
	 */
	[[nodiscard]] bool has_value() const noexcept { return storage_.index() == 0u; }
	/** Return whether the operation succeeded.
	 * @par Example
	 * @code{.cpp}
	 * if (!result)
	 *     return FlowUi::unexpectedError(result.error()); // Preserve the original failure.
	 * @endcode
	 */
	[[nodiscard]] explicit operator bool() const noexcept { return has_value(); }
	/** Borrow the successful value; throws std::bad_variant_access on failure.
	 * @par Example
	 * @code{.cpp}
	 * FlowUi::Result<std::string> title{std::string{"Inspector"}};
	 * if (title.has_value()) {
	 *     std::string& editable = title.value();
	 *     const auto& readOnly = std::as_const(title);
	 *     const std::string& observed = readOnly.value();
	 *     std::string owned = std::move(title).value();
	 * }
	 * @endcode
	 */
	[[nodiscard]] T& value() & { return std::get<0>(storage_); }
	/** Borrow the successful value as const; throws std::bad_variant_access on failure.
	 * @par Example
	 * @code{.cpp}
	 * FlowUi::Result<std::string> title{std::string{"Inspector"}};
	 * if (title.has_value()) {
	 *     std::string& editable = title.value();
	 *     const auto& readOnly = std::as_const(title);
	 *     const std::string& observed = readOnly.value();
	 *     std::string owned = std::move(title).value();
	 * }
	 * @endcode
	 */
	[[nodiscard]] const T& value() const& { return std::get<0>(storage_); }
	/** Move from the successful value; throws std::bad_variant_access on failure.
	 * @par Example
	 * @code{.cpp}
	 * FlowUi::Result<std::string> title{std::string{"Inspector"}};
	 * if (title.has_value()) {
	 *     std::string& editable = title.value();
	 *     const auto& readOnly = std::as_const(title);
	 *     const std::string& observed = readOnly.value();
	 *     std::string owned = std::move(title).value();
	 * }
	 * @endcode
	 */
	[[nodiscard]] T&& value() && { return std::get<0>(std::move(storage_)); }
	/** Borrow the successful value; requires success.
	 * @par Example
	 * @code{.cpp}
	 * FlowUi::Result<std::string> title{std::string{"Inspector"}};
	 * if (title) {
	 *     std::string& editable = *title;
	 *     const auto& readOnly = std::as_const(title);
	 *     const std::string& observed = *readOnly;
	 *     const std::size_t length = title->size();
	 *     const std::size_t observedLength = readOnly->size();
	 * }
	 * @endcode
	 */
	[[nodiscard]] T& operator*() & { return value(); }
	/** Borrow the successful value as const; requires success.
	 * @par Example
	 * @code{.cpp}
	 * FlowUi::Result<std::string> title{std::string{"Inspector"}};
	 * if (title) {
	 *     std::string& editable = *title;
	 *     const auto& readOnly = std::as_const(title);
	 *     const std::string& observed = *readOnly;
	 *     const std::size_t length = title->size();
	 *     const std::size_t observedLength = readOnly->size();
	 * }
	 * @endcode
	 */
	[[nodiscard]] const T& operator*() const& { return value(); }
	/** Return a pointer to the successful value; requires success.
	 * @par Example
	 * @code{.cpp}
	 * FlowUi::Result<std::string> title{std::string{"Inspector"}};
	 * if (title) {
	 *     std::string& editable = *title;
	 *     const auto& readOnly = std::as_const(title);
	 *     const std::string& observed = *readOnly;
	 *     const std::size_t length = title->size();
	 *     const std::size_t observedLength = readOnly->size();
	 * }
	 * @endcode
	 */
	[[nodiscard]] T* operator->() { return &value(); }
	/** Return a const pointer to the successful value; requires success.
	 * @par Example
	 * @code{.cpp}
	 * FlowUi::Result<std::string> title{std::string{"Inspector"}};
	 * if (title) {
	 *     std::string& editable = *title;
	 *     const auto& readOnly = std::as_const(title);
	 *     const std::string& observed = *readOnly;
	 *     const std::size_t length = title->size();
	 *     const std::size_t observedLength = readOnly->size();
	 * }
	 * @endcode
	 */
	[[nodiscard]] const T* operator->() const { return &value(); }
	/** Borrow the error; throws std::bad_variant_access on success.
	 * @par Example
	 * @code{.cpp}
	 * FlowUi::Result<int> result = FlowUi::unexpectedError(FlowUi::makeError(
	 *     FlowUi::ErrorCode::InvalidWindowId, FlowUi::ErrorSite::AppAccessWindow, window));
	 * if (!result) {
	 *     FlowUi::FlowUiError& error = result.error();
	 *     const auto& readOnly = std::as_const(result);
	 *     const FlowUi::FlowUiError& observedError = readOnly.error();
	 * }
	 * @endcode
	 */
	[[nodiscard]] FlowUiError& error() & { return std::get<1>(storage_); }
	/** Borrow the error as const; throws std::bad_variant_access on success.
	 * @par Example
	 * @code{.cpp}
	 * FlowUi::Result<int> result = FlowUi::unexpectedError(FlowUi::makeError(
	 *     FlowUi::ErrorCode::InvalidWindowId, FlowUi::ErrorSite::AppAccessWindow, window));
	 * if (!result) {
	 *     FlowUi::FlowUiError& error = result.error();
	 *     const auto& readOnly = std::as_const(result);
	 *     const FlowUi::FlowUiError& observedError = readOnly.error();
	 * }
	 * @endcode
	 */
	[[nodiscard]] const FlowUiError& error() const& { return std::get<1>(storage_); }

	template <typename U>
	/** Copy the successful value, or convert fallback to T on failure.
	 * @par Example
	 * @code{.cpp}
	 * const std::string title = result.value_or(std::string{"Untitled"});
	 * @endcode
	 */
	[[nodiscard]] T value_or(U&& fallback) const& {
		return has_value() ? std::get<0>(storage_) : static_cast<T>(std::forward<U>(fallback));
	}

private:
	std::variant<T, FlowUiError> storage_;
};

/** Compatibility result for no-value operations; default construction represents success.
 * @par Example
 * @code{.cpp}
 * FlowUi::Status success{};
 * if (success.has_value())
 *     success.value(); // No payload.
 *
 * FlowUi::Status failed = FlowUi::unexpectedError(FlowUi::makeError(
 *     FlowUi::ErrorCode::ObjectNotInitialized, FlowUi::ErrorSite::AppInitialize));
 * if (!failed) {
 *     FlowUi::FlowUiError& error = failed.error();
 *     const FlowUi::Status& readOnly = failed;
 *     const FlowUi::FlowUiError& observedError = readOnly.error();
 * }
 * @endcode
 */
template <>
class Result<void> {
public:
	/** Construct a successful no-value result.
	 * @par Example
	 * @code{.cpp}
	 * FlowUi::Status success{};
	 * if (success.has_value())
	 *     success.value(); // No payload.
	 *
	 * FlowUi::Status failed = FlowUi::unexpectedError(FlowUi::makeError(
	 *     FlowUi::ErrorCode::ObjectNotInitialized, FlowUi::ErrorSite::AppInitialize));
	 * if (!failed) {
	 *     FlowUi::FlowUiError& error = failed.error();
	 *     const FlowUi::Status& readOnly = failed;
	 *     const FlowUi::FlowUiError& observedError = readOnly.error();
	 * }
	 * @endcode
	 */
	Result() noexcept = default;
	/** Construct a failed no-value result from unexpectedError().
	 * @par Example
	 * @code{.cpp}
	 * FlowUi::Status success{};
	 * if (success.has_value())
	 *     success.value(); // No payload.
	 *
	 * FlowUi::Status failed = FlowUi::unexpectedError(FlowUi::makeError(
	 *     FlowUi::ErrorCode::ObjectNotInitialized, FlowUi::ErrorSite::AppInitialize));
	 * if (!failed) {
	 *     FlowUi::FlowUiError& error = failed.error();
	 *     const FlowUi::Status& readOnly = failed;
	 *     const FlowUi::FlowUiError& observedError = readOnly.error();
	 * }
	 * @endcode
	 */
	Result(detail::UnexpectedFlowUiError failure) noexcept : error_(failure.error), hasValue_(false) {}
	/** Return whether the no-value operation succeeded.
	 * @par Example
	 * @code{.cpp}
	 * FlowUi::Status success{};
	 * if (success.has_value())
	 *     success.value(); // No payload.
	 *
	 * FlowUi::Status failed = FlowUi::unexpectedError(FlowUi::makeError(
	 *     FlowUi::ErrorCode::ObjectNotInitialized, FlowUi::ErrorSite::AppInitialize));
	 * if (!failed) {
	 *     FlowUi::FlowUiError& error = failed.error();
	 *     const FlowUi::Status& readOnly = failed;
	 *     const FlowUi::FlowUiError& observedError = readOnly.error();
	 * }
	 * @endcode
	 */
	[[nodiscard]] bool has_value() const noexcept { return hasValue_; }
	/** Return whether the operation succeeded.
	 * @par Example
	 * @code{.cpp}
	 * FlowUi::Status success{};
	 * if (success.has_value())
	 *     success.value(); // No payload.
	 *
	 * FlowUi::Status failed = FlowUi::unexpectedError(FlowUi::makeError(
	 *     FlowUi::ErrorCode::ObjectNotInitialized, FlowUi::ErrorSite::AppInitialize));
	 * if (!failed) {
	 *     FlowUi::FlowUiError& error = failed.error();
	 *     const FlowUi::Status& readOnly = failed;
	 *     const FlowUi::FlowUiError& observedError = readOnly.error();
	 * }
	 * @endcode
	 */
	[[nodiscard]] explicit operator bool() const noexcept { return has_value(); }
	/** Validate success; throws std::bad_variant_access on failure.
	 * @par Example
	 * @code{.cpp}
	 * FlowUi::Status success{};
	 * if (success.has_value())
	 *     success.value(); // No payload.
	 *
	 * FlowUi::Status failed = FlowUi::unexpectedError(FlowUi::makeError(
	 *     FlowUi::ErrorCode::ObjectNotInitialized, FlowUi::ErrorSite::AppInitialize));
	 * if (!failed) {
	 *     FlowUi::FlowUiError& error = failed.error();
	 *     const FlowUi::Status& readOnly = failed;
	 *     const FlowUi::FlowUiError& observedError = readOnly.error();
	 * }
	 * @endcode
	 */
	void value() const {
		if (!hasValue_)
			throw std::bad_variant_access{};
	}
	/** Borrow the stored error; meaningful only on failure.
	 * @par Example
	 * @code{.cpp}
	 * FlowUi::Status success{};
	 * if (success.has_value())
	 *     success.value(); // No payload.
	 *
	 * FlowUi::Status failed = FlowUi::unexpectedError(FlowUi::makeError(
	 *     FlowUi::ErrorCode::ObjectNotInitialized, FlowUi::ErrorSite::AppInitialize));
	 * if (!failed) {
	 *     FlowUi::FlowUiError& error = failed.error();
	 *     const FlowUi::Status& readOnly = failed;
	 *     const FlowUi::FlowUiError& observedError = readOnly.error();
	 * }
	 * @endcode
	 */
	[[nodiscard]] FlowUiError& error() & noexcept { return error_; }
	/** Borrow the stored error as const; meaningful only on failure.
	 * @par Example
	 * @code{.cpp}
	 * FlowUi::Status success{};
	 * if (success.has_value())
	 *     success.value(); // No payload.
	 *
	 * FlowUi::Status failed = FlowUi::unexpectedError(FlowUi::makeError(
	 *     FlowUi::ErrorCode::ObjectNotInitialized, FlowUi::ErrorSite::AppInitialize));
	 * if (!failed) {
	 *     FlowUi::FlowUiError& error = failed.error();
	 *     const FlowUi::Status& readOnly = failed;
	 *     const FlowUi::FlowUiError& observedError = readOnly.error();
	 * }
	 * @endcode
	 */
	[[nodiscard]] const FlowUiError& error() const& noexcept { return error_; }

private:
	FlowUiError error_{};
	bool hasValue_ = true;
};

/** Result for an operation that succeeds without returning a value.
 * @par Example
 * @code{.cpp}
 * FlowUi::Status success{};
 * if (success.has_value())
 *     success.value(); // No payload.
 *
 * FlowUi::Status failed = FlowUi::unexpectedError(FlowUi::makeError(
 *     FlowUi::ErrorCode::ObjectNotInitialized, FlowUi::ErrorSite::AppInitialize));
 * if (!failed) {
 *     FlowUi::FlowUiError& error = failed.error();
 *     const FlowUi::Status& readOnly = failed;
 *     const FlowUi::FlowUiError& observedError = readOnly.error();
 * }
 * @endcode
 */
using Status = Result<void>;

/** Wrap a compact error for constructing a failed compatibility Result or Status.
 * @par Example
 * @code{.cpp}
 * const std::string title = "Inspector";
 * FlowUi::Result<std::string> copied{title};
 * FlowUi::Result<std::string> moved{std::string{"Inspector"}};
 * FlowUi::Result<std::string> failed = FlowUi::unexpectedError(FlowUi::makeError(
 *     FlowUi::ErrorCode::InvalidWindowConfiguration, FlowUi::ErrorSite::AppCreateWindow));
 * FlowUi::Result<std::string> emptyTitle{}; // Successful empty string.
 * @endcode
 */
[[nodiscard]] constexpr detail::UnexpectedFlowUiError unexpectedError(FlowUiError error) noexcept {
	return detail::UnexpectedFlowUiError{error};
}

#endif

} // namespace FlowUi
