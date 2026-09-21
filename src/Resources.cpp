#include "FlowUi/Resources.hpp"
#include <array>
#include <cstdlib>
#include <fstream>
#include <limits>

#if defined(_WIN32)
#define NOMINMAX
#include <windows.h>
#elif defined(__APPLE__)
#include <dlfcn.h>
#include <mach-o/dyld.h>
#else
#include <dlfcn.h>
#include <unistd.h>
#endif

namespace FlowUi {
namespace {

std::filesystem::path executable_directory() {
#if defined(_WIN32)
	std::vector<wchar_t> buffer(512u);
	for (;;) {
		const DWORD length =
			GetModuleFileNameW(nullptr, buffer.data(), static_cast<DWORD>(buffer.size()));
		if (!length)
			return {};
		if (length < buffer.size())
			return std::filesystem::path(std::wstring(buffer.data(), length)).parent_path();
		buffer.resize(buffer.size() * 2u);
	}
#elif defined(__APPLE__)
	uint32_t size = 0;
	_NSGetExecutablePath(nullptr, &size);
	std::vector<char> buffer(size);
	if (_NSGetExecutablePath(buffer.data(), &size) != 0)
		return {};
	return std::filesystem::weakly_canonical(buffer.data()).parent_path();
#else
	std::vector<char> buffer(512u);
	for (;;) {
		const auto length = readlink("/proc/self/exe", buffer.data(), buffer.size());
		if (length < 0)
			return {};
		if (static_cast<size_t>(length) < buffer.size())
			return std::filesystem::path(std::string(buffer.data(), length)).parent_path();
		buffer.resize(buffer.size() * 2u);
	}
#endif
}

std::filesystem::path module_directory() {
#if defined(_WIN32)
	HMODULE module{};
	if (!GetModuleHandleExW(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS |
								GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
							reinterpret_cast<LPCWSTR>(&module_directory), &module))
		return {};
	std::vector<wchar_t> buffer(512u);
	for (;;) {
		const DWORD length =
			GetModuleFileNameW(module, buffer.data(), static_cast<DWORD>(buffer.size()));
		if (!length)
			return {};
		if (length < buffer.size())
			return std::filesystem::path(std::wstring(buffer.data(), length)).parent_path();
		buffer.resize(buffer.size() * 2u);
	}
#else
	Dl_info info{};
	if (dladdr(reinterpret_cast<void*>(&module_directory), &info) && info.dli_fname)
		return std::filesystem::absolute(info.dli_fname).parent_path();
	return {};
#endif
}

void append_roots(std::vector<std::filesystem::path>& roots, std::filesystem::path directory) {
	// Ancestors cover bin/, multi-config output, and conventional lib/<triplet>/ layouts.
	for (unsigned level = 0u; level < 4u && !directory.empty(); ++level) {
		roots.emplace_back(directory / FLOWUI_RESOURCE_SUBDIRECTORY);
		roots.emplace_back(directory / "share" / FLOWUI_RESOURCE_SUBDIRECTORY);
#if defined(__APPLE__)
		roots.emplace_back(directory / "Resources" / FLOWUI_RESOURCE_SUBDIRECTORY);
#endif
		const auto parent = directory.parent_path();
		if (parent == directory)
			break;
		directory = parent;
	}
}

} // namespace

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
	try {
		std::error_code error;
		if (name.empty())
			return unexpectedError(makeError(ErrorCode::AssetPathEmpty, ErrorSite::ResourceLocate));
		if (name.is_absolute()) {
			if (std::filesystem::is_regular_file(name, error))
				return name;
			return unexpectedError(makeError(ErrorCode::AssetNotFound, ErrorSite::ResourceLocate));
		}
		if (name.has_root_name() || name.has_root_directory())
			return unexpectedError(makeError(ErrorCode::AssetPathEmpty, ErrorSite::ResourceLocate));
		const auto relative = name.lexically_normal();
		for (const auto& part : relative)
			if (part == "..")
				return unexpectedError(
					makeError(ErrorCode::AssetPathEmpty, ErrorSite::ResourceLocate));
		std::vector<std::filesystem::path> roots = config.roots;
		if (config.use_platform_defaults) {
#if defined(_WIN32)
			if (const wchar_t* root = _wgetenv(L"FLOWUI_RESOURCE_ROOT"); root && *root)
				roots.emplace_back(root);
#else
			if (const char* root = std::getenv("FLOWUI_RESOURCE_ROOT"); root && *root)
				roots.emplace_back(root);
#endif
			append_roots(roots, config.executable_directory.empty() ? executable_directory()
																	: config.executable_directory);
			if (config.executable_directory.empty())
				append_roots(roots, module_directory());
#if !defined(_WIN32)
			roots.emplace_back(std::filesystem::path("/usr/local/share") /
							   FLOWUI_RESOURCE_SUBDIRECTORY);
			roots.emplace_back(std::filesystem::path("/usr/share") / FLOWUI_RESOURCE_SUBDIRECTORY);
#endif
		}
		if (config.use_build_directory)
			roots.emplace_back(path_from_utf8(FLOWUI_RESOURCE_BUILD_DIRECTORY));
		for (const auto& root : roots) {
			const auto candidate = root / relative;
			if (std::filesystem::is_regular_file(candidate, error))
				return candidate;
		}
		return unexpectedError(makeError(ErrorCode::AssetNotFound, ErrorSite::ResourceLocate));
	} catch (const std::filesystem::filesystem_error&) {
		return unexpectedError(makeError(ErrorCode::AssetOpenFailed, ErrorSite::ResourceLocate));
	}
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
