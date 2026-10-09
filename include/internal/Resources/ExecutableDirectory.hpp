#pragma once
#include <filesystem>
namespace FlowUi::detail {
/** Discover the running executable's directory; return an empty path on failure. */
[[nodiscard]] std::filesystem::path executable_directory() noexcept;
} // namespace FlowUi::detail
