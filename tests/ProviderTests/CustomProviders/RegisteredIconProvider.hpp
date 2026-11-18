#pragma once
#include "DistinctIconRasterProvider.hpp"
namespace FlowUi::test {
[[nodiscard]] uint32_t registeredIconSourceSignature() noexcept;
/** Real source-registered replacement; the independently built function participates in each
 * operation. */
class RegisteredIconProvider : public DistinctIconRasterProvider {
public:
	[[nodiscard]] Result<DistinctIconRasterProvider::Document>
	parse(std::span<const std::byte> bytes) noexcept {
		if (registeredIconSourceSignature() != 0xF103u)
			return unexpectedError(
				makeError(ErrorCode::AssetPayloadInvalid, ErrorSite::IconParseSource));
		return DistinctIconRasterProvider::parse(bytes);
	}
};
} // namespace FlowUi::test
