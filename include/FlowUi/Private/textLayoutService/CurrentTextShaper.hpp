#pragma once

#include "FlowUi/Public/managerStructs/FontTypes.hpp"
#include "FlowUi/Private/textLayoutService/TextLayoutRecords.hpp"

namespace FlowUi::detail::text {

class CurrentTextShaper {
public:
	[[nodiscard]] TextLayoutResult shape(
		const TextLayoutRequest& request,
		const Font::FontFaceData& fontFace) const;
};

} // namespace FlowUi::detail::text
