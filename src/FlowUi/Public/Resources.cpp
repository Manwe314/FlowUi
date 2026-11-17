#include "FlowUi/Public/Resources.hpp"
#include "FlowUi/Private/ApiBoundary.hpp"
#include "FlowUi/Private/resourceService/ResourceDiscovery.hpp"

#include <fstream>
#include <limits>

namespace FlowUi {

Result<std::filesystem::path> path_from_utf8(std::string_view text) noexcept try {
	return std::filesystem::path(
		std::u8string(reinterpret_cast<const char8_t*>(text.data()), text.size()));
} catch (const std::filesystem::filesystem_error&) {
	return unexpectedError(makeError(ErrorCode::AssetOpenFailed, ErrorSite::ResourceReadFile));
} catch (...) {
	return unexpectedError(detail::currentExceptionError(ErrorSite::ResourceReadFile));
}

Result<std::string> path_to_utf8(const std::filesystem::path& path) noexcept try {
	const auto value = path.generic_u8string();
	return std::string(reinterpret_cast<const char*>(value.data()), value.size());
} catch (const std::filesystem::filesystem_error&) {
	return unexpectedError(makeError(ErrorCode::AssetOpenFailed, ErrorSite::ResourceReadFile));
} catch (...) {
	return unexpectedError(detail::currentExceptionError(ErrorSite::ResourceReadFile));
}

Result<std::filesystem::path> locate_resource(const std::filesystem::path& name,
											  const ResourceConfig& config) noexcept try {
	return detail::locate_resource(name, config);
} catch (const std::filesystem::filesystem_error&) {
	return unexpectedError(makeError(ErrorCode::AssetOpenFailed, ErrorSite::ResourceLocate));
} catch (...) {
	return unexpectedError(detail::currentExceptionError(ErrorSite::ResourceLocate));
}

Result<std::vector<unsigned char>> read_file_bytes(const std::filesystem::path& path) noexcept try {
	std::error_code pathError;
	if (std::filesystem::is_directory(path, pathError) || pathError)
		return unexpectedError(makeError(ErrorCode::AssetOpenFailed, ErrorSite::ResourceReadFile));
	std::ifstream stream(path, std::ios::binary | std::ios::ate);
	if (!stream)
		return unexpectedError(makeError(ErrorCode::AssetOpenFailed, ErrorSite::ResourceReadFile));
	const auto size = stream.tellg();
	if (size < 0 || static_cast<uint64_t>(size) > std::numeric_limits<size_t>::max() ||
		static_cast<uint64_t>(size) >
			static_cast<uint64_t>(std::numeric_limits<std::streamsize>::max()))
		return unexpectedError(makeError(ErrorCode::AssetReadFailed, ErrorSite::ResourceReadFile));
	std::vector<unsigned char> bytes;
	if (static_cast<uint64_t>(size) > bytes.max_size())
		return unexpectedError(makeError(ErrorCode::AssetReadFailed, ErrorSite::ResourceReadFile));
	bytes.resize(static_cast<size_t>(size));
	stream.seekg(0);
	if (!bytes.empty())
		stream.read(reinterpret_cast<char*>(bytes.data()),
					static_cast<std::streamsize>(bytes.size()));
	if (!stream)
		return unexpectedError(makeError(ErrorCode::AssetReadFailed, ErrorSite::ResourceReadFile));
	return bytes;
} catch (const std::filesystem::filesystem_error&) {
	return unexpectedError(makeError(ErrorCode::AssetOpenFailed, ErrorSite::ResourceReadFile));
} catch (...) {
	return unexpectedError(detail::currentExceptionError(ErrorSite::ResourceReadFile));
}

} // namespace FlowUi
