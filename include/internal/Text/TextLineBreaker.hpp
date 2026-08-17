#pragma once

#include <string_view>
#include <vector>

#include "internal/Text/TextLayoutRecords.hpp"
#include "managers/structs/InputFieldManagerStructs.hpp"

namespace FlowUi::detail::text {

/** Greedy LTR wrapping over layout-record cluster boundaries. */
[[nodiscard]] std::vector<TextRange> breakVisualLines(
	std::string_view text,
	const TextLayoutResult& layout,
	float maximumWidth);

} // namespace FlowUi::detail::text
