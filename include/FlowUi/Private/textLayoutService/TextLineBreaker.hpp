#pragma once

#include <string_view>
#include <vector>

#include "FlowUi/Private/textLayoutService/TextLayoutRecords.hpp"
#include "FlowUi/Public/managerStructs/InputFieldTypes.hpp"

namespace FlowUi::detail::text {

/* Greedy LTR wrapping over layout-record cluster boundaries. */
[[nodiscard]] std::vector<TextRange> breakVisualLines(
	std::string_view text,
	const TextLayoutResult& layout,
	float maximumWidth);

} // namespace FlowUi::detail::text
