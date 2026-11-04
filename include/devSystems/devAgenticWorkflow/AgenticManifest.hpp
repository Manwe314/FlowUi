#pragma once
#include "FlowUi/BuildConfig.hpp"
#if FLOW_UI_AGENTIC_WORKFLOW
#include "devSystems/devAgenticWorkflow/AgenticSnapshot.hpp"

namespace FlowUi::devSystems::agentic {

/** Inventory record, verified against final bytes and capture-root confinement. */
struct AgenticArtifact {
	std::string artifact_id;
	std::string path;
	std::string media_type;
	std::string schema;
	std::string schema_version;
	std::string integer_encoding;
	std::string sha256;
	std::uint64_t bytes = 0;
};
/** Validated sealed capture, including its preserved envelope. */
struct AgenticManifest {
	std::filesystem::path manifest_path;
	Json document;
	std::vector<AgenticArtifact> artifacts;
	std::string run_id;
	std::string capture_id;
	std::string digest;
	bool partial = false;
};
/** Immutable store writer and offline inventory reader. Never initializes an App or GPU. */
class AgenticStore {
public:
	explicit AgenticStore(std::filesystem::path root, AgenticLimits limits = {});
	[[nodiscard]] const std::filesystem::path& root() const noexcept;
	/** Explicit initialization reserves a store ID; constructors never create files. */
	[[nodiscard]] Status initialize() noexcept;
	/** Exclusively reserve a run and record immutable allowlisted provenance. */
	[[nodiscard]] Result<std::filesystem::path> reserve_run(std::string_view run_id,
															const Json& provenance) noexcept;
	/** Stage artifacts, validate, write manifest last, then atomically publish. */
	[[nodiscard]] Result<AgenticManifest> publish(AgenticCaptureSnapshot snapshot) noexcept;
	/** Resolve exactly one sealed capture by ID or an explicit portable manifest. */
	[[nodiscard]] Result<AgenticManifest> resolve_capture(std::string_view capture_id_or_path,
														  bool verify_hashes = true) const noexcept;
	/** Discover valid final manifests; a stale catalogue is never authoritative. */
	[[nodiscard]] Result<Json> list_captures() const noexcept;
	/** Rebuild the index from sealed inventories under an exclusive short store lock. */
	[[nodiscard]] Status repair_index() noexcept;
	/** Select/remove whole captures, preserving pinned/referenced evidence. */
	[[nodiscard]] Result<Json> prune(std::uint64_t older_than_seconds,
									 bool dry_run = true) noexcept;

private:
	std::filesystem::path root_;
	AgenticLimits limits_;
};
/** SHA-256 of exact final bytes; lowercase hexadecimal. */
[[nodiscard]] std::string sha256(std::span<const std::uint8_t> bytes);
[[nodiscard]] std::string sha256(std::string_view bytes);
/** Validate inventory/schema, reference integrity, checksums and safe relative paths. */
[[nodiscard]] Result<AgenticManifest>
read_manifest(const std::filesystem::path& path, bool verify_hashes = true,
			  std::uint64_t maximum_artifact_bytes = 256u * 1024u * 1024u) noexcept;
/** Read only an explicitly inventoried artifact, under both byte and path bounds. */
[[nodiscard]] Result<std::vector<std::uint8_t>>
read_artifact(const AgenticManifest& manifest, std::string_view artifact_id,
			  std::uint64_t maximum_bytes = 1024u * 1024u) noexcept;
[[nodiscard]] Result<Json>
read_artifact_json(const AgenticManifest& manifest, std::string_view artifact_id,
				   std::uint64_t maximum_bytes = 16u * 1024u * 1024u) noexcept;
[[nodiscard]] Result<AgenticPage>
query_artifact(const AgenticManifest& manifest, std::string_view artifact_id,
			   const Json& filter = Json::make_object(), std::uint32_t limit = 100,
			   std::uint64_t maximum_bytes = 65536, std::string_view cursor = {}) noexcept;
/** Atomically publish a derived immutable comparison under its run. */
[[nodiscard]] Result<std::filesystem::path> publish_comparison(const AgenticManifest& baseline,
															   const AgenticManifest& candidate,
															   const Json& comparison) noexcept;
/** Strict, lossless version-1 schema validation used before writing and after reading. */
[[nodiscard]] Status validate_agentic_document(const Json& document,
											   std::string_view expected_schema) noexcept;

} // namespace FlowUi::devSystems::agentic

#endif // FLOW_UI_AGENTIC_WORKFLOW
