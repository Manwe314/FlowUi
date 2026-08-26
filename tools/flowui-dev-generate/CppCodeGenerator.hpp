#pragma once

#include <filesystem>
#include <string>

#include "ManifestParser.hpp"

namespace flowui::bake_tool {

bool generateCpp(
	const Manifest& manifest,
	const std::filesystem::path& outputDirectory,
	std::string& error);

} // namespace flowui::bake_tool
