#pragma once

#include "FlowUi/FontResources.hpp"
#include "internal/Text/TextLayoutRecords.hpp"

namespace FlowUi::detail::text {

class CurrentTextShaper {
public:
	[[nodiscard]] TextLayoutResult shape(
		const TextLayoutRequest& request,
		const Font::FontFaceData& fontFace) const;
};

} // namespace FlowUi::detail::text
