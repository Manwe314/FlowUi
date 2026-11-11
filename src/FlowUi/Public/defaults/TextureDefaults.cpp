#include "FlowUi/Public/defaults/TextureDefaults.hpp"

#include <array>

namespace FlowUi::defaults {
namespace {

constexpr std::array<std::byte, 16> missing_texture_pixels{
	std::byte{128}, std::byte{0}, std::byte{255}, std::byte{255},
	std::byte{0}, std::byte{255}, std::byte{255}, std::byte{255},
	std::byte{0}, std::byte{255}, std::byte{255}, std::byte{255},
	std::byte{128}, std::byte{0}, std::byte{255}, std::byte{255},
};

} // namespace

TexturePixels missing_texture() noexcept {
	return {.bytes = missing_texture_pixels, .width = 2, .height = 2};
}

} // namespace FlowUi::defaults
