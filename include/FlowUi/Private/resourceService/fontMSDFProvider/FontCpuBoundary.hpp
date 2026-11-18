#pragma once
#include "FlowUi/Public/errorManagement/Exception.hpp"
#include "FlowUi/Public/errorManagement/Result.hpp"
#include <exception>
#include <new>
#include <utility>
namespace FlowUi::detail::font {
/** Standalone CPU result boundary; fatal errors terminate without an App observer dependency. */
template <typename Operation>
[[nodiscard]] auto fontCpuResult(ErrorSite site, Operation&& operation) noexcept
	-> decltype(operation()) {
	using Return = decltype(operation());
	try {
		return std::forward<Operation>(operation)();
	} catch (const FlowUiException& exception) {
		if (exception.error().descriptor().category == ErrorCategory::Fatal)
			std::terminate();
		return Return(unexpectedError(exception.error()));
	} catch (const std::bad_alloc&) {
		return Return(unexpectedError(makeError(ErrorCode::AllocationFailed, site)));
	} catch (...) {
		return Return(unexpectedError(makeError(ErrorCode::ForeignExceptionObserved, site)));
	}
}
} // namespace FlowUi::detail::font
