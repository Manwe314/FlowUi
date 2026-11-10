#pragma once

/** @file
 * @brief Public AgenticSnapshot declarations for optional development services.
 */
#include "FlowUi/Public/BuildConfig.hpp"
#if FLOW_UI_AGENTIC_WORKFLOW
#include "devSystems/devAgenticWorkflow/AgenticCapture.hpp"
#include "devSystems/devAgenticWorkflow/AgenticImage.hpp"
#include <span>

namespace FlowUi::devSystems::agentic {

/** One observed window frame, keyed by application identity rather than slot/count. */
struct AgenticFrameRecord {
	/** @brief Exact producer frame identity attached to this record. */
	EvidenceFrameKey key;
	/** @brief Monotonic frame start timestamp, in nanoseconds. */
	std::uint64_t begin_ns = 0;
	/** @brief Monotonic frame end timestamp, in nanoseconds. */
	std::uint64_t end_ns = 0;
	/** @brief Monotonic queue submission timestamp, in nanoseconds. */
	std::uint64_t submit_ns = 0;
	/**
	 * @brief Monotonic present-call timestamp, in nanoseconds; it does not imply scanout
	 * completion.
	 */
	std::uint64_t present_ns = 0;
	/**
	 * @brief Producer-supplied queue submission serial; independent of renderer frame slots.
	 */
	std::uint64_t submission_serial = 0;
	/** @brief Producer-supplied GPU device identity. */
	std::uint64_t device_id = 0;
	/** @brief Producer-supplied GPU queue identity. */
	std::uint64_t queue_id = 0;
	/** @brief Measured CPU frame duration, in nanoseconds. */
	std::uint64_t cpu_duration_ns = 0;
	/**
	 * @brief Measured GPU duration in nanoseconds, valid only when availability is reported.
	 */
	std::uint64_t gpu_duration_ns = 0;
	/** @brief Expected or observed UI tree generation, independent of layout revision. */
	std::uint64_t tree_generation = 0;
	/** @brief Reusable renderer frame slot; it is not a window-frame identity. */
	std::uint32_t frame_slot = 0;
	/** @brief Rendered window-frame width in physical pixels. */
	std::uint32_t width = 0;
	/** @brief Rendered window-frame height in physical pixels. */
	std::uint32_t height = 0;
	/** @brief Instrumentation activities active during this observed frame. */
	ObserverActivity observers = ObserverActivity::none;
	/** @brief Whether the frame was successfully submitted to its rendering queue. */
	bool submitted = false;
	/** @brief Whether presentation was actually reported for the observed frame. */
	bool presented = false;
	/** @brief Whether this window frame was canceled before successful completion. */
	bool canceled = false;
	/** @brief Whether the producer window belongs to built-in developer tooling. */
	bool internal_window = false;
	/** @brief Whether gpu_duration_ns contains supported measured evidence. */
	bool gpu_duration_available = false;
	/**
	 * @brief Whether this frame's cadence interval was interrupted and cannot be treated as
	 * continuous.
	 */
	bool interrupted = false;
};
/** A complete immutable artifact payload, owned before publication. */
struct AgenticArtifactPayload {
	/** @brief Complete owned immutable artifact payload bytes. */
	std::vector<std::uint8_t> bytes;
	/** @brief Stable artifact identity within the sealed capture inventory. */
	std::string artifact_id;
	/**
	 * @brief Artifact path relative to its capture root; publication validates path confinement.
	 */
	std::string path;
	/** @brief Artifact MIME type used to interpret its final bytes. */
	std::string media_type;
	/** @brief Artifact schema identity used for strict versioned validation. */
	std::string schema;
	/** @brief Version of the serialized artifact schema. */
	std::string schema_version = "1.0";
	/**
	 * @brief Schema convention used for exact integers; decimal_strings preserves 64-bit
	 * identities.
	 */
	std::string integer_encoding = "decimal_strings";
};
/** One logical cutoff, with independent producer revisions and explicit limitations. */
struct AgenticCaptureSnapshot {
	/** @brief Owned observed window frame records retained at the capture cutoff. */
	std::vector<AgenticFrameRecord> frames;
	/** @brief Owned fence-completed RGB images retained for publication. */
	std::vector<AgenticCompletedImage> images;
	/**
	 * @brief Owned immutable artifact payloads or inventory entries associated with this capture.
	 */
	std::vector<AgenticArtifactPayload> artifacts;
	/** @brief Owned coverage and loss evidence associated with this result or capture. */
	std::vector<std::pair<std::string, SectionCoverage>> coverage;
	/** @brief Explicit unsupported, missing or degraded evidence conditions. */
	std::vector<std::string> limitations;
	/**
	 * @brief Immutable application run identity, separate from job and capture identities.
	 */
	std::string run_id;
	/** @brief Immutable capture identity within its run. */
	std::string capture_id;
	/** @brief Stable queued operation identity recoverable after client disconnect. */
	std::string job_id;
	/** @brief UTC request timestamp recorded in the immutable capture envelope. */
	std::string requested_at_utc;
	/** @brief Owned allowlisted build, workload and environment evidence. */
	Json provenance = Json::make_object();
	/** @brief Owned source and resource metadata associated with this capture. */
	Json sources = Json::make_array();
	/** @brief Owned cross-section evidence correlations. */
	Json correlations = Json::make_array();
	/** @brief Owned semantic action evidence collected during the capture. */
	Json actions = Json::make_array();
	/** @brief Owned normalized capture request used to reserve and publish this snapshot. */
	AgenticCaptureRequest request;
	/** @brief Inclusive lower bound of the application-tick interval. */
	std::uint64_t first_app_tick = 0;
	/** @brief Exclusive upper bound of the application-tick interval. */
	std::uint64_t end_app_tick_exclusive = 0;
	/** @brief Inclusive monotonic measurement start timestamp, in nanoseconds. */
	std::uint64_t measurement_begin_ns = 0;
	/** @brief Monotonic measurement end timestamp, in nanoseconds. */
	std::uint64_t measurement_end_ns = 0;
	/** @brief Timing capture generation used to bind retained producer evidence. */
	std::uint64_t timing_capture_generation = 0;
	/**
	 * @brief Whether the sealed evidence explicitly contains incomplete requested coverage.
	 */
	bool partial = false;

	/** @brief Construct an empty snapshot before bounded evidence is appended. */
	AgenticCaptureSnapshot() = default;
	/**
	 * @brief Move the complete owned capture payload without copying image or artifact bytes.
	 */
	AgenticCaptureSnapshot(AgenticCaptureSnapshot&&) noexcept = default;
	/**
	 * @brief Transfer owned snapshot storage; the moved-from object remains destructible.
	 */
	AgenticCaptureSnapshot& operator=(AgenticCaptureSnapshot&&) noexcept = default;
	AgenticCaptureSnapshot(const AgenticCaptureSnapshot&) = delete;
	AgenticCaptureSnapshot& operator=(const AgenticCaptureSnapshot&) = delete;
};
/** Encode exact frame identities with decimal strings. */
[[nodiscard]] Json to_json(const AgenticFrameRecord& frame);
/** Account for owned container capacities, nested dictionaries, images and artifact bytes. */
[[nodiscard]] Result<std::uint64_t>
snapshot_owned_bytes(const AgenticCaptureSnapshot& snapshot) noexcept;
/** Add a bounded UTF-8 artifact to an owned snapshot. */
[[nodiscard]] Status add_text_artifact(AgenticCaptureSnapshot& snapshot, std::string artifact_id,
									   std::string path, std::string media_type, std::string schema,
									   std::string text, std::string schema_version = "1.0",
									   std::string integer_encoding = "decimal_strings") noexcept;

} // namespace FlowUi::devSystems::agentic

#endif // FLOW_UI_AGENTIC_WORKFLOW
