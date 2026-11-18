#pragma once
#include <cstddef>
#include <cstdint>
#include <initializer_list>
#include <iterator>
#include <string_view>
namespace FlowUi::detail::font {
struct FontBakeProfile {
	double pixelSize;
	double pixelRange;
	double angleThreshold;
	double miterLimit;
	uint32_t spacing;
};
inline constexpr FontBakeProfile runtimeFontBakeProfile{18.0, 6.0, 3.0, 1.0, 2u};
inline constexpr FontBakeProfile offlineFontBakeProfile{64.0, 8.0, 3.0, 1.0, 2u};
inline constexpr unsigned long long fontColoringMultiplier = 6364136223846793005ull;
inline constexpr unsigned long long fontColoringIncrement = 1442695040888963407ull;
inline constexpr uint32_t offlineUiCodepoints[] = {
	0x2013u, // EN DASH
	0x2014u, // EM DASH
	0x2018u, // LEFT SINGLE QUOTATION MARK
	0x2019u, // RIGHT SINGLE QUOTATION MARK
	0x201Cu, // LEFT DOUBLE QUOTATION MARK
	0x201Du, // RIGHT DOUBLE QUOTATION MARK
	0x2022u, // BULLET
	0x2026u, // HORIZONTAL ELLIPSIS
	0x20ACu, // EURO SIGN
	0x2122u, // TRADE MARK SIGN
	0x2190u, // LEFTWARDS ARROW
	0x2191u, // UPWARDS ARROW
	0x2192u, // RIGHTWARDS ARROW
	0x2193u, // DOWNWARDS ARROW
	0x2194u, // LEFT RIGHT ARROW
	0x2195u, // UP DOWN ARROW
	0x2212u, // MINUS SIGN
	0x2260u, // NOT EQUAL TO
	0x2264u, // LESS-THAN OR EQUAL TO
	0x2265u, // GREATER-THAN OR EQUAL TO
	0x25A1u, // WHITE SQUARE
	0x25B2u, // BLACK UP-POINTING TRIANGLE
	0x25B6u, // BLACK RIGHT-POINTING TRIANGLE
	0x25BCu, // BLACK DOWN-POINTING TRIANGLE
	0x25C0u, // BLACK LEFT-POINTING TRIANGLE
	0x25C6u, // BLACK DIAMOND
	0x25CBu, // WHITE CIRCLE
	0x25CFu, // BLACK CIRCLE
	0x2713u, // CHECK MARK
	0x2717u, // BALLOT X
	0xFFFDu, // REPLACEMENT CHARACTER
};
/** Detect file includes without mistaking quoted character literals for directives. */
[[nodiscard]] constexpr bool hasExternalCharsetIncludes(std::string_view text) noexcept {
	char quote = 0;
	bool escaped = false;
	for (size_t index = 0; index < text.size(); ++index) {
		const char character = text[index];
		if (quote != 0) {
			if (escaped)
				escaped = false;
			else if (character == '\\')
				escaped = true;
			else if (character == quote)
				quote = 0;
		} else if (character == '\'' || character == '"')
			quote = character;
		else if (character == '@' && text.substr(index).starts_with("@include"))
			return true;
	}
	return false;
}
static_assert(hasExternalCharsetIncludes("@include \"file\""));
static_assert(!hasExternalCharsetIncludes("\"@include\""));
[[nodiscard]] consteval bool validateFontBakeProfiles() noexcept {
	for (const auto profile : {runtimeFontBakeProfile, offlineFontBakeProfile}) {
		if (!(profile.pixelSize > 0.0 && profile.pixelRange > 0.0 &&
			  profile.angleThreshold >= 0.0 && profile.miterLimit >= 0.0) ||
			profile.spacing != 2u)
			return false;
	}
	for (size_t index = 0; index < std::size(offlineUiCodepoints); ++index) {
		if (offlineUiCodepoints[index] > 0x10ffffu ||
			(offlineUiCodepoints[index] >= 0xd800u && offlineUiCodepoints[index] <= 0xdfffu))
			return false;
		for (size_t previous = 0; previous < index; ++previous)
			if (offlineUiCodepoints[previous] == offlineUiCodepoints[index])
				return false;
	}
	return true;
}
static_assert(validateFontBakeProfiles());
} // namespace FlowUi::detail::font
