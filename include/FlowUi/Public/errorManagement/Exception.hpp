#pragma once

#include "FlowUi/Public/errorManagement/ErrorValue.hpp"
#include <exception>

namespace FlowUi {

/** Typed distant-unwind carrier for a FlowUi-authored error. */
class FlowUiException final : public std::exception {
public:
	/**
	 * Copy the compact error into a typed exception without allocating.
	 * @par Example
	 * @code{.cpp}
	 * const FlowUi::FlowUiException exception(error);
	 * @endcode
	 */
	explicit FlowUiException(FlowUiError error) noexcept : error_(error) {}

	/**
	 * Borrow the stored error for the lifetime of this exception.
	 * @par Example
	 * @code{.cpp}
	 * const FlowUi::FlowUiError& error = exception.error();
	 * @endcode
	 */
	[[nodiscard]] const FlowUiError& error() const noexcept { return error_; }

	/**
	 * Return the static, null-terminated message for the stored error code.
	 * @par Example
	 * @code{.cpp}
	 * const char* message = exception.what();
	 * @endcode
	 */
	[[nodiscard]] const char* what() const noexcept override {
		// All descriptor messages are null-terminated string literals.
		return errorMessage(error_.code).data();
	}

private:
	FlowUiError error_{};
};

} // namespace FlowUi
