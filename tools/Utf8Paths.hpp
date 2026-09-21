#pragma once
#include <filesystem>
#include <string>
#include <string_view>

namespace flowui::tools {
/** Convert command-line UTF-8 to a native path. */
[[nodiscard]] inline std::filesystem::path utf8_path(std::string_view text) {
	return std::filesystem::path(std::u8string(reinterpret_cast<const char8_t*>(text.data()), text.size()));
}
/** Format a native path as UTF-8 diagnostic text. */
[[nodiscard]] inline std::string utf8_text(const std::filesystem::path& path) {
	const auto text = path.generic_u8string();
	return std::string(reinterpret_cast<const char*>(text.data()), text.size());
}
} // namespace flowui::tools
