#pragma once

#include "devSystems/devTooling/bake/DevBakePipeline.hpp"
#include "devSystems/devTooling/override/DevOverrideEngine.hpp"

#if FLOW_UI_DEV_MODE
namespace FlowUi::devSystems::tooling::bake_detail {
/* Directory creation coordinates all runtime and repair writers before check-and-replace. */
class ManifestWriterLock {
public:
	explicit ManifestWriterLock(const std::filesystem::path& manifest) : path_(manifest) {
		path_ += ".lock";
		std::error_code error;
		if (!manifest.parent_path().empty())
			std::filesystem::create_directories(manifest.parent_path(), error);
		if (!error)
			acquired_ = std::filesystem::create_directory(path_, error);
	}
	ManifestWriterLock(const ManifestWriterLock&) = delete;
	ManifestWriterLock& operator=(const ManifestWriterLock&) = delete;
	~ManifestWriterLock() {
		if (acquired_) {
			std::error_code error;
			std::filesystem::remove(path_, error);
		}
	}
	[[nodiscard]] bool acquired() const noexcept { return acquired_; }

private:
	std::filesystem::path path_;
	bool acquired_ = false;
};

/* Shared codec and domain adapters used by the runtime and owned review plans. */
[[nodiscard]] bool decode_manifest(std::string_view contents, DevBakeManifest& manifest);
[[nodiscard]] bool read_manifest_snapshot(const std::filesystem::path& path,
										  DevBakeManifest& manifest, std::string& contents,
										  bool& existed, std::string* diagnostic = nullptr);
[[nodiscard]] std::string encode_manifest(const DevBakeManifest& manifest);
[[nodiscard]] bool entry_values_equal(const DevBakeEntry& left, const DevBakeEntry& right);
[[nodiscard]] bool entry_value_valid(const tooling::DevSchemaGeneration& schema,
									 const tooling::DevFieldSchema& field,
									 const DevBakeEntry& entry, std::string& reason);
[[nodiscard]] bool entry_cpp_value_consistent(const tooling::DevSchemaGeneration& schema,
											  const tooling::DevFieldSchema& field,
											  const DevBakeEntry& entry);
[[nodiscard]] std::string utc_timestamp();
[[nodiscard]] bool write_manifest_atomically(const std::filesystem::path& destination,
											 std::string_view contents);
[[nodiscard]] std::optional<DevBakeEntry>
element_proposal(const tooling::DevSchemaGeneration& schema, const DevOverrideApply::Record& record,
				 DevBakeDiagnostic& diagnostic);
[[nodiscard]] std::optional<DevBakeEntry>
theme_proposal(const tooling::DevSchemaGeneration& schema,
			   const DevOverrideEngine::ThemeBakeRecord& record, DevBakeDiagnostic& diagnostic);
} // namespace FlowUi::devSystems::tooling::bake_detail
#endif
