#pragma once

#include "FlowUi/Public/errorManagement/ErrorValue.hpp"
#include <expected>

namespace FlowUi {

#if !defined(__cpp_lib_expected) || __cpp_lib_expected < 202202L
namespace detail {
struct UnexpectedFlowUiError {
	FlowUiError error{};
};
} // namespace detail
#endif

} // namespace FlowUi
