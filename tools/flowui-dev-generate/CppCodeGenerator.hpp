#pragma once

#include <filesystem>
#include <string>

#include "ManifestParser.hpp"

namespace flowui::bake_tool {

/** Generate ordered compiled assignments and immutable development metadata, writing only changed
 * files. */
[[nodiscard]] bool generateCpp(const Manifest& manifest,
							   const std::filesystem::path& outputDirectory, std::string& error);

} // namespace flowui::bake_tool
