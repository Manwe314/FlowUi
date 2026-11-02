#pragma once
#include "internal/MemoryCapacityManifest.hpp"
#include <optional>

#include <cstdint>
#include <filesystem>
#include <string>
#include <string_view>
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
	std::string value_json{};
	std::uint64_t value_type = 0;
	std::size_t source_index = 0;
	std::size_t source_line = 1;
};

struct Manifest {
	std::optional<FlowUi::detail::MemoryCapacityManifest> memoryCapacity{};
	std::uint32_t version = 0;
	std::uint64_t schemaFingerprint = 0;
	std::uint64_t content_digest = 0;
	std::string source_path{};
	std::vector<Entry> entries{};
};

/** Decode a supported manifest and validate every saved generator assignment without dropping
 * entries. */
[[nodiscard]] bool parseManifest(const std::filesystem::path& path, Manifest& output,
								 std::string& error);

/** Inspect raw saved identities without requiring compilable generator metadata. */
[[nodiscard]] bool inspect_manifest(const std::filesystem::path& path, std::string& output,
									std::string& error);
/** Remove one indexed saved entry, preserving all other JSON and checking the source revision. */
[[nodiscard]] bool remove_manifest_entry(const std::filesystem::path& path, std::size_t entry_index,
										 std::string& error);

} // namespace flowui::bake_tool
