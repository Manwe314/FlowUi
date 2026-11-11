#pragma once

#include <concepts>
#include <cstdint>
#include <initializer_list>
#include <type_traits>

namespace FlowUi::detail {

/** Stores one domain of explicitly assigned byte-sized flag masks. */
template <typename Flag>
	requires std::is_enum_v<Flag> &&
			 std::same_as<std::underlying_type_t<Flag>, std::uint8_t>
class EnumFlags {
public:
	constexpr EnumFlags() noexcept = default;
	explicit constexpr EnumFlags(Flag flag) noexcept : bits_(static_cast<std::uint8_t>(flag)) {}
	constexpr EnumFlags(std::initializer_list<Flag> flags) noexcept {
		for (const Flag flag : flags)
			set_flag(flag);
	}

	[[nodiscard]] constexpr bool has_flag(Flag flag) const noexcept {
		return (bits_ & static_cast<std::uint8_t>(flag)) != 0;
	}
	constexpr void set_flag(Flag flag, bool enabled = true) noexcept {
		const auto mask = static_cast<std::uint8_t>(flag);
		bits_ = enabled ? static_cast<std::uint8_t>(bits_ | mask)
						: static_cast<std::uint8_t>(bits_ & static_cast<std::uint8_t>(~mask));
	}
	constexpr void clear_flag(Flag flag) noexcept { set_flag(flag, false); }

private:
	std::uint8_t bits_ = 0;
};

} // namespace FlowUi::detail
