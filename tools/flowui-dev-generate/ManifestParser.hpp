#pragma once

#include <cstdint>
#include <filesystem>
#include <string>
#include <vector>

namespace flowui::bake_tool {

enum class TargetKind { Element, Theme };
enum class TargetScope { Definition, ExactInstance };

struct Entry {
	TargetKind targetKind = TargetKind::Element;
	TargetScope targetScope = TargetScope::Definition;
	std::uint64_t definitionId = 0;
	std::uint64_t themeType = 0;
	std::string themeVariant{};
	std::uint64_t instanceKey = 0;
	std::string instanceDebugLabel{};
	std::uint64_t fieldId = 0;
	std::string fieldPath{};
	std::string ownerCppType{};
	std::string sourceHeader{};
	std::string cppValue{};
};

struct Manifest {
	std::uint32_t version = 0;
	std::uint64_t schemaFingerprint = 0;
	std::vector<Entry> entries{};
};

bool parseManifest(
	const std::filesystem::path& path,
	Manifest& output,
	std::string& error);

} // namespace flowui::bake_tool
