#pragma once

#include <filesystem>

#include "FlowUi/Public/configurationStructs/ResourceConfig.hpp"
#include "FlowUi/Public/errorManagement/Error.hpp"

namespace FlowUi::detail {

/* Resolve a packaged resource using the application's existing lookup policy. */
[[nodiscard]] Result<std::filesystem::path> locate_resource(const std::filesystem::path& name,
															const ResourceConfig& config);

} // namespace FlowUi::detail
