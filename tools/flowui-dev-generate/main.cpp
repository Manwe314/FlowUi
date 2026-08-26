#include <charconv>
#include <cctype>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <string>
#include <string_view>

#include "CppCodeGenerator.hpp"
#include "ManifestParser.hpp"

namespace {

void usage() {
	std::cerr << "usage: flowui-dev-generate --manifest <file> --output-dir <dir> [--schema-header <file>]\n";
}

bool schemaHeaderFingerprint(const std::filesystem::path& path, std::uint64_t& fingerprint) {
	std::ifstream input(path);
	if (!input) return false;
	const std::string contents((std::istreambuf_iterator<char>(input)), {});
	const std::string marker = "FLOWUI_DEV_SCHEMA_FINGERPRINT";
	const std::size_t found = contents.find(marker);
	if (found == std::string::npos) return false;
	const std::size_t hex = contents.find("0x", found + marker.size());
	if (hex == std::string::npos) return false;
	std::size_t end = hex + 2;
	while (end < contents.size() && std::isxdigit(static_cast<unsigned char>(contents[end]))) ++end;
	const auto parsed = std::from_chars(contents.data() + hex + 2, contents.data() + end, fingerprint, 16);
	return parsed.ec == std::errc{};
}

} // namespace

int main(int argc, char** argv) {
	std::filesystem::path manifestPath;
	std::filesystem::path outputDirectory;
	std::filesystem::path schemaHeader;
	for (int index = 1; index < argc; ++index) {
		const std::string_view argument(argv[index]);
		if ((argument == "--manifest" || argument == "--output-dir" || argument == "--schema-header") && index + 1 >= argc) {
			usage(); return 2;
		}
		if (argument == "--manifest") manifestPath = argv[++index];
		else if (argument == "--output-dir") outputDirectory = argv[++index];
		else if (argument == "--schema-header") schemaHeader = argv[++index];
		else { std::cerr << "unknown argument: " << argument << '\n'; usage(); return 2; }
	}
	if (manifestPath.empty() || outputDirectory.empty()) { usage(); return 2; }

	flowui::bake_tool::Manifest manifest;
	std::string error;
	if (!flowui::bake_tool::parseManifest(manifestPath, manifest, error)) {
		std::cerr << "flowui-dev-generate: " << error << '\n'; return 1;
	}
	if (!schemaHeader.empty()) {
		std::uint64_t expected = 0;
		if (!schemaHeaderFingerprint(schemaHeader, expected)) {
			std::cerr << "flowui-dev-generate: schema header has no FLOWUI_DEV_SCHEMA_FINGERPRINT\n"; return 1;
		}
		if (expected != manifest.schemaFingerprint) {
			std::cerr << "flowui-dev-generate: stale manifest ignored (schema fingerprint mismatch)\n";
			manifest.entries.clear();
		}
	}
	if (!flowui::bake_tool::generateCpp(manifest, outputDirectory, error)) {
		std::cerr << "flowui-dev-generate: " << error << '\n'; return 1;
	}
	return 0;
}
