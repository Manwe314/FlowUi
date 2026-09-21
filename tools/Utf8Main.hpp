#pragma once
#include "Utf8Paths.hpp"
#include <filesystem>
#include <string>
#include <string_view>
#include <vector>

#if defined(_WIN32)
int flowui_tool_main(int argc, char** argv);
int wmain(int argc, wchar_t** argv) {
	std::vector<std::string> arguments;
	std::vector<char*> pointers;
	arguments.reserve(static_cast<size_t>(argc));
	pointers.reserve(static_cast<size_t>(argc) + 1u);
	for (int index = 0; index < argc; ++index) {
		const auto utf8 = std::filesystem::path(argv[index]).u8string();
		arguments.emplace_back(reinterpret_cast<const char*>(utf8.data()), utf8.size());
	}
	for (auto& argument : arguments)
		pointers.emplace_back(argument.data());
	pointers.emplace_back(nullptr);
	return flowui_tool_main(argc, pointers.data());
}
#define main flowui_tool_main
#endif
