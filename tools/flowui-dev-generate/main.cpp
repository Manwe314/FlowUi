#include <cctype>
#include <charconv>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <optional>
#include <string>
#include <string_view>

#include "CppCodeGenerator.hpp"
#include "ManifestParser.hpp"
#include "../Utf8Main.hpp"

namespace {

void usage() {
	std::cerr << "usage: flowui-dev-generate --manifest <file> --output-dir <dir> [--schema-header "
				 "<file>]\n"
			  << "       flowui-dev-generate --manifest <file> --inspect\n"
			  << "       flowui-dev-generate --manifest <file> --remove-entry <zero-based index>\n"
			  << "Inspection/removal works even when saved C++ metadata is no longer compilable.\n"
			  << "Build a repair executable with -DFLOW_UI_ENABLE_BAKED_CHANGES=OFF; the active "
				 "file is preserved.\n";
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
	std::optional<std::size_t> removal_index;
	bool inspect = false;
	for (int index = 1; index < argc; ++index) {
		const std::string_view argument(argv[index]);
		if ((argument == "--manifest" || argument == "--output-dir" ||
			 argument == "--schema-header" || argument == "--remove-entry") &&
			index + 1 >= argc) {
			usage(); return 2;
		}
		if (argument == "--manifest") manifestPath = flowui::tools::utf8_path(argv[++index]);
		else if (argument == "--output-dir") outputDirectory = flowui::tools::utf8_path(argv[++index]);
		else if (argument == "--schema-header") schemaHeader = flowui::tools::utf8_path(argv[++index]);
		else if (argument == "--inspect")
			inspect = true;
		else if (argument == "--remove-entry") {
			const std::string_view index_text(argv[++index]);
			std::size_t selected_index = 0;
			const auto parsed = std::from_chars(
				index_text.data(), index_text.data() + index_text.size(), selected_index);
			if (parsed.ec != std::errc{} || parsed.ptr != index_text.data() + index_text.size()) {
				std::cerr << "--remove-entry requires a nonnegative decimal index\n";
				return 2;
			}
			removal_index = selected_index;
		} else {
			std::cerr << "unknown argument: " << argument << '\n';
			usage();
			return 2;
		}
	}
	if (manifestPath.empty() || (outputDirectory.empty() && !inspect && !removal_index) ||
		(!outputDirectory.empty() && (inspect || removal_index)) || (inspect && removal_index)) {
		usage();
		return 2;
	}

	flowui::bake_tool::Manifest manifest;
	std::string error;
	if (inspect) {
		std::string description;
		if (!flowui::bake_tool::inspect_manifest(manifestPath, description, error)) {
			std::cerr << "flowui-dev-generate: " << error << '\n';
			return 1;
		}
		std::cout << description;
		return 0;
	}
	if (removal_index) {
		if (!flowui::bake_tool::remove_manifest_entry(manifestPath, *removal_index, error)) {
			std::cerr << "flowui-dev-generate: " << error << '\n';
			return 1;
		}
		std::cout << "Removed bakedChanges[" << *removal_index
				  << "]; manifest updated, rebuild required\n";
		return 0;
	}
	if (!flowui::bake_tool::parseManifest(manifestPath, manifest, error)) {
		std::cerr << "flowui-dev-generate: " << error << '\n'; return 1;
	}
	if (!schemaHeader.empty()) {
		std::uint64_t expected = 0;
		if (!schemaHeaderFingerprint(schemaHeader, expected)) {
			std::cerr << "flowui-dev-generate: schema header has no FLOWUI_DEV_SCHEMA_FINGERPRINT\n"; return 1;
		}
		if (expected != manifest.schemaFingerprint) {
			std::cerr << "flowui-dev-generate: schema fingerprint changed; preserving "
					  << manifest.entries.size()
					  << " saved entries. Generated compilation checks their current C++ targets. "
					  << "Use --manifest <file> --inspect and explicit --remove-entry <index> to "
						 "repair obsolete entries.\n";
		}
	}
	if (!flowui::bake_tool::generateCpp(manifest, outputDirectory, error)) {
		std::cerr << "flowui-dev-generate: " << error << '\n'; return 1;
	}
	return 0;
}
