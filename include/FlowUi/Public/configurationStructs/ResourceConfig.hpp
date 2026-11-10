#pragma once

#include <filesystem>
#include <vector>

namespace FlowUi {

/** Resource roots contain shaders/, fonts/, and optional application assets. */
struct ResourceConfig {
	/** Caller-supplied roots searched in order before enabled default discovery paths. */
	std::vector<std::filesystem::path> roots;
	/** Override executable discovery, for embedding hosts and tests. */
	std::filesystem::path executable_directory;
	/** Search platform installation and executable-relative resource locations. */
	bool use_platform_defaults = true;
	/** Search the configured build-tree resource directory when available. */
	bool use_build_directory = true;
};

} // namespace FlowUi
