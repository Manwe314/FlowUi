#include "FlowUi/Resources.hpp"
#include <chrono>
#include <fstream>
#include <iostream>

int main() {
	using namespace FlowUi;
	const auto temporary =
		std::filesystem::temp_directory_path() /
		("flowui-resources-" +
		 std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
	struct Cleanup {
		std::filesystem::path directory;
		~Cleanup() {
			std::error_code error;
			std::filesystem::remove_all(directory, error);
		}
	} cleanup{temporary};
	const auto unicode_name = std::filesystem::path(u8"fonts/日本語-é.bin");
	const auto first = temporary / "first";
	const auto second = temporary / "second";
	for (const auto& root : {first, second}) {
		std::filesystem::create_directories(root / "fonts");
		std::ofstream(root / unicode_name, std::ios::binary) << "font-data";
	}
	ResourceConfig config{
		.roots = {first, second}, .use_platform_defaults = false, .use_build_directory = false};
	int failures = 0;
	const auto check = [&failures](bool condition, const char* message) {
		if (!condition) {
			std::cerr << message << '\n';
			++failures;
		}
	};
	const auto found = locate_resource(unicode_name, config);
	check(found && *found == first / unicode_name, "Explicit resource root precedence failed");
	check(path_from_utf8(path_to_utf8(unicode_name)) == unicode_name,
		  "UTF-8 path round trip failed");
	const auto bytes = read_file_bytes(first / unicode_name);
	check(bytes && std::string(bytes->begin(), bytes->end()) == "font-data",
		  "Native Unicode file loading failed");
	check(!locate_resource("../escape", config), "Parent traversal accepted");
	check(!locate_resource("missing", config), "Missing resource accepted");
	check(!read_file_bytes(first / "missing"), "Missing file accepted");
	check(locate_resource(first / unicode_name, config).has_value(),
		  "Absolute resource override failed");
	config.roots.clear();
	config.use_platform_defaults = true;
	config.executable_directory = temporary / "installed" / "bin";
	const auto packaged = temporary / "installed" / "share" / FLOWUI_TEST_RESOURCE_SUBDIRECTORY / "fonts";
	std::filesystem::create_directories(packaged);
	std::ofstream(packaged / "test.bin") << "packaged";
	check(locate_resource("fonts/test.bin", config).has_value(),
		  "Relocatable platform resource discovery failed");
	check(locate_resource("shaders/flowui_ui_solid.vert.spv").has_value(),
		  "Automatic shader staging failed");
	check(locate_resource("fonts/Inter.arfont").has_value(),
		  "Automatic default font staging failed");
	return failures == 0 ? 0 : 1;
}
