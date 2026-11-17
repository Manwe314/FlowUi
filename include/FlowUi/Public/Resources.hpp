#pragma once

#include "FlowUi/Public/configurationStructs/ResourceConfig.hpp"
#include "FlowUi/Public/errorManagement/Error.hpp"
#include <filesystem>
#include <string>
#include <string_view>
#include <vector>

namespace FlowUi {

/**
 * @brief Convert UTF-8 text to a native filesystem path without a Windows code-page conversion.
 * @param text UTF-8 path bytes; copied into the returned path.
 * @return Owned native path. This conversion does not check existence or resolve resources.
 * Encoding and allocation failures are returned as FlowUi errors.
 * @par Example
 * @code{.cpp}
 * std::string_view utf8 = "assets/logo.png";
 * auto result = FlowUi::path_from_utf8(utf8);
 * if (!result)
 *     throw FlowUi::FlowUiException(result.error());
 * std::filesystem::path path = std::move(*result);
 * @endcode
 */
[[nodiscard]] Result<std::filesystem::path> path_from_utf8(std::string_view text) noexcept;
/**
 * @brief Serialize a native filesystem path as UTF-8 using generic path separators.
 * @param path Native path to encode; no file access is performed.
 * @return Owned UTF-8 string.
 * Encoding and allocation failures are returned as FlowUi errors.
 * @par Example
 * @code{.cpp}
 * std::filesystem::path path{"assets/logo.png"};
 * auto result = FlowUi::path_to_utf8(path);
 * if (!result)
 *     throw FlowUi::FlowUiException(result.error());
 * std::string utf8 = std::move(*result);
 * @endcode
 */
[[nodiscard]] Result<std::string> path_to_utf8(const std::filesystem::path& path) noexcept;
/**
 * @brief Locate a packaged resource in configured search order.
 * @param name Relative resource name confined to its root, or an absolute path override.
 * @param config Explicit roots and opt-in/out controls for fallback discovery.
 * @return Located native path, or a structured discovery/path-validation error.
 * Explicit roots precede environment, platform, and build defaults. The returned
 * path owns its storage; the resource is not opened or cached by this call.
 * @par Example
 * @code{.cpp}
 * FlowUi::ResourceConfig configuration{};
 * configuration.roots.emplace_back("assets");
 * configuration.use_platform_defaults = false;
 * configuration.use_build_directory = false;
 *
 * auto result = FlowUi::locate_resource("logo.png", configuration);
 * if (!result)
 *     throw FlowUi::FlowUiException(result.error());
 * std::filesystem::path path = std::move(*result);
 * @endcode
 */
[[nodiscard]] Result<std::filesystem::path> locate_resource(const std::filesystem::path& name,
															const ResourceConfig& config = {}) noexcept;
/**
 * @brief Read an entire file as bytes through a native filesystem path.
 * @param path File to open directly, without resource-root discovery.
 * @return Owned bytes, including an empty vector for an empty file, or
 * AssetOpenFailed/AssetReadFailed when the file cannot be opened or fully read.
 * Allocation failures are returned as AllocationFailed.
 * @par Example
 * @code{.cpp}
 * std::filesystem::path path{"assets/logo.png"};
 * auto result = FlowUi::read_file_bytes(path);
 * if (!result)
 *     throw FlowUi::FlowUiException(result.error());
 * std::vector<unsigned char> bytes = std::move(*result);
 * @endcode
 */
[[nodiscard]] Result<std::vector<unsigned char>> read_file_bytes(const std::filesystem::path& path) noexcept;

} // namespace FlowUi
