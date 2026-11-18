#pragma once
#include "DeterministicTextLayoutProvider.hpp"
namespace FlowUi::test {
[[nodiscard]] uint32_t registeredTextSourceSignature() noexcept;
/** Real source-registered replacement; the independently built function participates in each
 * operation. */
class RegisteredTextProvider : public DeterministicTextLayoutProvider {
public:
	[[nodiscard]] Result<detail::text::TextLayoutResult>
	layout(const detail::text::TextProviderInput& input, const Font::FontFaceData& face) noexcept {
		if (registeredTextSourceSignature() != 0xF103u)
			return unexpectedError(makeError(ErrorCode::AssetPayloadInvalid, ErrorSite::FontParse));
		return DeterministicTextLayoutProvider::layout(input, face);
	}
};
} // namespace FlowUi::test
