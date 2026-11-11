#pragma once

#include <filesystem>
#include <vector>

namespace FlowUi {

/**
 * Resource lookup roots containing shaders/, fonts/, and optional application assets.
 * Explicit roots are searched before enabled discovery paths; paths are owned by the configuration.
 */
struct ResourceConfig {
	/** Caller-supplied roots searched in order before enabled default discovery paths. */
	std::vector<std::filesystem::path> roots;
	/** Executable-directory override for hosts/tests; empty uses automatic executable discovery. */
	std::filesystem::path executable_directory;
	/** Search platform installation and executable-relative resource locations. */
	bool use_platform_defaults = true;
	/** Search the configured build-tree resource directory when available. */
	bool use_build_directory = true;
};

} // namespace FlowUi
