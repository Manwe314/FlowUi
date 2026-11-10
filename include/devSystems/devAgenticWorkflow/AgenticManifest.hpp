#pragma once

/** @file
 * @brief Public AgenticManifest declarations for optional development services.
 */
#include "FlowUi/Public/BuildConfig.hpp"
#if FLOW_UI_AGENTIC_WORKFLOW
#include "devSystems/devAgenticWorkflow/AgenticSnapshot.hpp"

namespace FlowUi::devSystems::agentic {

/** Inventory record, verified against final bytes and capture-root confinement. */
struct AgenticArtifact {
	/** @brief Stable artifact identity within the sealed capture inventory. */
	std::string artifact_id;
	/** @brief Validated artifact path relative to its capture root. */
	std::string path;
	/** @brief Artifact MIME type used to interpret its final bytes. */
	std::string media_type;
	/** @brief Artifact schema identity used for strict versioned validation. */
	std::string schema;
	/** @brief Version of the serialized artifact schema. */
	std::string schema_version;
	/**
	 * @brief Schema convention used for exact integers; decimal_strings preserves 64-bit
	 * identities.
	 */
	std::string integer_encoding;
	/** @brief Lowercase SHA-256 digest of the exact final artifact bytes. */
	std::string sha256;
	/** @brief Exact final artifact size in bytes, verified against the inventory. */
	std::uint64_t bytes = 0;
};
/** Validated sealed capture, including its preserved envelope. */
struct AgenticManifest {
	/** @brief Filesystem location of the authoritative sealed manifest. */
	std::filesystem::path manifest_path;
	/**
	 * @brief Owned validated manifest envelope, preserved alongside decoded inventory fields.
	 */
	Json document;
	/**
	 * @brief Owned immutable artifact payloads or inventory entries associated with this capture.
	 */
	std::vector<AgenticArtifact> artifacts;
	/**
	 * @brief Immutable application run identity, separate from job and capture identities.
	 */
	std::string run_id;
	/** @brief Immutable capture identity within its run. */
	std::string capture_id;
	/**
	 * @brief Content digest used to bind immutable evidence or compiled workload identity.
	 */
	std::string digest;
	/**
	 * @brief Whether the sealed evidence explicitly contains incomplete requested coverage.
	 */
	bool partial = false;
};
/** Immutable store writer and offline inventory reader. Never initializes an App or GPU. */
class AgenticStore {
public:
	/**
	 * @brief Retain store root and admission limits; construction does not create files or
	 * initialize a store.
	 */
	explicit AgenticStore(std::filesystem::path root, AgenticLimits limits = {});
	/** @brief Borrow the configured store root; valid while this store object lives. */
	[[nodiscard]] const std::filesystem::path& root() const noexcept;
	/** Explicit initialization reserves a store ID; constructors never create files. */
	[[nodiscard]] Status initialize() noexcept;
	/** Exclusively reserve a run and record immutable allowlisted provenance. */
	[[nodiscard]] Result<std::filesystem::path> reserve_run(std::string_view run_id,
															const Json& provenance) noexcept;
	/** Resolve one run by its authoritative run.json identity, independent of directory naming. */
	[[nodiscard]] Result<std::filesystem::path> resolve_run(std::string_view run_id) const noexcept;
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
/** @brief Return the lowercase SHA-256 digest of the exact supplied bytes. */
[[nodiscard]] std::string sha256(std::string_view bytes);
/** Validate inventory/schema, reference integrity, checksums and safe relative paths. */
[[nodiscard]] Result<AgenticManifest>
read_manifest(const std::filesystem::path& path, bool verify_hashes = true,
			  std::uint64_t maximum_artifact_bytes = 256u * 1024u * 1024u) noexcept;
/** Read only an explicitly inventoried artifact, under both byte and path bounds. */
[[nodiscard]] Result<std::vector<std::uint8_t>>
read_artifact(const AgenticManifest& manifest, std::string_view artifact_id,
			  std::uint64_t maximum_bytes = 1024u * 1024u) noexcept;
/**
 * @brief Read and parse only an inventoried JSON artifact under maximum_bytes and capture-root
 * path bounds.
 */
[[nodiscard]] Result<Json>
read_artifact_json(const AgenticManifest& manifest, std::string_view artifact_id,
				   std::uint64_t maximum_bytes = 16u * 1024u * 1024u) noexcept;
/**
 * @brief Return a bounded filtered artifact page; cursor must match the immutable manifest and
 * query digests.
 */
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
