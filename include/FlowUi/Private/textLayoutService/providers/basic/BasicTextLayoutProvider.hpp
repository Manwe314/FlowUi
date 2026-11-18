#pragma once

#include "FlowUi/Private/textLayoutService/TextLayoutServiceConcept.hpp"

namespace FlowUi::detail::text {

/** The existing codepoint/kerning LTR algorithm, with owned CPU layout output. */
class BasicTextLayoutProvider {
public:
	static constexpr uint32_t contractRevision = textLayoutContractRevision;

	/** Compute hard lines, glyph placement, source clusters and carets synchronously. */
	[[nodiscard]] Result<TextLayoutResult> layout(const TextProviderInput& input,
												  const Font::FontFaceData& fontFace) noexcept;
};

static_assert(TextLayoutServiceConcept<BasicTextLayoutProvider>);

} // namespace FlowUi::detail::text
