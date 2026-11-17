#pragma once

#include "FlowUi/Private/ErrorReporting.hpp"
#include "FlowUi/Public/errorManagement/Exception.hpp"
#include "FlowUi/Public/errorManagement/Result.hpp"

#include <new>
#include <utility>

namespace FlowUi::detail {

/** Resume internal unwinding when a nested public operation failed. */
inline void requireApiSuccess(Status status) {
	if (!status)
		throw FlowUiException(status.error());
}

/** Unwrap a nested public operation inside a library-owned exception boundary. */
template <typename Value>
[[nodiscard]] Value requireApiValue(Result<Value> result) {
	if (!result)
		throw FlowUiException(result.error());
	return std::move(*result);
}

/** Translate the active exception at an API boundary without allocating.
 * Call only while handling an exception. FlowUi errors retain their original attribution.
 * Exception type does not establish ownership: application code may throw FlowUiException too.
 * Library invariant failures terminate at their proven origin, never by guessing here.
 */
[[nodiscard]] inline FlowUiError currentExceptionError(ErrorSite site) noexcept {
	try {
		throw;
	} catch (const FlowUiException& exception) {
		return exception.error();
	} catch (const std::bad_alloc&) {
		return makeError(ErrorCode::AllocationFailed, site);
	} catch (...) {
		return makeError(ErrorCode::ForeignExceptionObserved, site);
	}
}

/** Invoke a fallible API operation and contain exceptions at its result boundary. */
template <typename ResultType, typename Operation>
[[nodiscard]] ResultType catchResult(ErrorSite site, Operation&& operation) noexcept {
	try {
		return std::forward<Operation>(operation)();
	} catch (...) {
		return unexpectedError(currentExceptionError(site));
	}
}

} // namespace FlowUi::detail
