#pragma once

#include "FlowUi/Error.hpp"
#include <filesystem>
#include <string>
#include <string_view>
#include <vector>

namespace FlowUi {

/** Resource roots contain shaders/, fonts/, and optional application assets. */
struct ResourceConfig {
	std::vector<std::filesystem::path> roots;
	/** Override executable discovery, for embedding hosts and tests. */
	std::filesystem::path executable_directory;
	bool use_platform_defaults = true;
	bool use_build_directory = true;
};

/** Convert UTF-8 text to a native filesystem path without a Windows code-page conversion. */
[[nodiscard]] std::filesystem::path path_from_utf8(std::string_view text);
/** Serialize a native filesystem path as UTF-8. */
[[nodiscard]] std::string path_to_utf8(const std::filesystem::path& path);
/** Locate a packaged resource. Explicit roots precede environment, platform and build defaults.
 * Relative resource names must stay within their root. Absolute paths are explicit overrides.
 */
[[nodiscard]] Result<std::filesystem::path> locate_resource(const std::filesystem::path& name,
															const ResourceConfig& config = {});
/** Read binary data through a native path. */
[[nodiscard]] Result<std::vector<unsigned char>> read_file_bytes(const std::filesystem::path& path);

} // namespace FlowUi
