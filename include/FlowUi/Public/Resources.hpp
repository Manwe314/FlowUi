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
 * @throws std::filesystem::filesystem_error if the platform rejects the encoding.
 */
[[nodiscard]] std::filesystem::path path_from_utf8(std::string_view text);
/**
 * @brief Serialize a native filesystem path as UTF-8 using generic path separators.
 * @param path Native path to encode; no file access is performed.
 * @return Owned UTF-8 string.
 * @throws std::filesystem::filesystem_error if encoding conversion fails.
 */
[[nodiscard]] std::string path_to_utf8(const std::filesystem::path& path);
/**
 * @brief Locate a packaged resource in configured search order.
 * @param name Relative resource name confined to its root, or an absolute path override.
 * @param config Explicit roots and opt-in/out controls for fallback discovery.
 * @return Located native path, or a structured discovery/path-validation error.
 * Explicit roots precede environment, platform, and build defaults. The returned
 * path owns its storage; the resource is not opened or cached by this call.
 */
[[nodiscard]] Result<std::filesystem::path> locate_resource(const std::filesystem::path& name,
															const ResourceConfig& config = {});
/**
 * @brief Read an entire file as bytes through a native filesystem path.
 * @param path File to open directly, without resource-root discovery.
 * @return Owned bytes, including an empty vector for an empty file, or
 * AssetOpenFailed/AssetReadFailed when the file cannot be opened or fully read.
 * @throws std::bad_alloc if allocating the byte buffer fails.
 */
[[nodiscard]] Result<std::vector<unsigned char>> read_file_bytes(const std::filesystem::path& path);

} // namespace FlowUi
