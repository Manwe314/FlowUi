#include "FlowUi/Public/Resources.hpp"
#include "FlowUi/Private/resourceService/ResourceDiscovery.hpp"

#include <fstream>
#include <limits>

namespace FlowUi {

std::filesystem::path path_from_utf8(std::string_view text) {
	return std::filesystem::path(
		std::u8string(reinterpret_cast<const char8_t*>(text.data()), text.size()));
}

std::string path_to_utf8(const std::filesystem::path& path) {
	const auto value = path.generic_u8string();
	return std::string(reinterpret_cast<const char*>(value.data()), value.size());
}

Result<std::filesystem::path> locate_resource(const std::filesystem::path& name,
											  const ResourceConfig& config) {
	return detail::locate_resource(name, config);
}

Result<std::vector<unsigned char>> read_file_bytes(const std::filesystem::path& path) {
	std::ifstream stream(path, std::ios::binary | std::ios::ate);
	if (!stream)
		return unexpectedError(makeError(ErrorCode::AssetOpenFailed, ErrorSite::ResourceReadFile));
	const auto size = stream.tellg();
	if (size < 0 || static_cast<uint64_t>(size) > std::numeric_limits<size_t>::max())
		return unexpectedError(makeError(ErrorCode::AssetReadFailed, ErrorSite::ResourceReadFile));
	std::vector<unsigned char> bytes(static_cast<size_t>(size));
	stream.seekg(0);
	if (!bytes.empty())
		stream.read(reinterpret_cast<char*>(bytes.data()),
					static_cast<std::streamsize>(bytes.size()));
	if (!stream)
		return unexpectedError(makeError(ErrorCode::AssetReadFailed, ErrorSite::ResourceReadFile));
	return bytes;
}

} // namespace FlowUi
