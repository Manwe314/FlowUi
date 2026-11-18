#pragma once
#include "DistinctFontMsdfProvider.hpp"
namespace FlowUi::test {
[[nodiscard]] uint32_t registeredFontSourceSignature() noexcept;
/** Real source-registered replacement; the independently built function participates in each
 * operation. */
class RegisteredFontProvider : public DistinctFontMsdfProvider {
public:
	[[nodiscard]] Result<detail::font::FontCpuAsset>
	loadBaked(std::span<const std::byte> bytes) noexcept {
		if (registeredFontSourceSignature() != 0xF103u)
			return unexpectedError(makeError(ErrorCode::AssetPayloadInvalid, ErrorSite::FontParse));
		return DistinctFontMsdfProvider::loadBaked(bytes);
	}
};
} // namespace FlowUi::test
