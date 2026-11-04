#pragma once
#include "FlowUi/BuildConfig.hpp"
#if FLOW_UI_AGENTIC_WORKFLOW
#include "devSystems/devAgenticWorkflow/AgenticCapture.hpp"
#include "devSystems/devAgenticWorkflow/AgenticImage.hpp"
#include <span>

namespace FlowUi::devSystems::agentic {

/** One observed window frame, keyed by application identity rather than slot/count. */
struct AgenticFrameRecord {
	EvidenceFrameKey key;
	std::uint64_t begin_ns = 0;
	std::uint64_t end_ns = 0;
	std::uint64_t submit_ns = 0;
	std::uint64_t present_ns = 0;
	std::uint64_t submission_serial = 0;
	std::uint64_t device_id = 0;
	std::uint64_t queue_id = 0;
	std::uint64_t cpu_duration_ns = 0;
	std::uint64_t gpu_duration_ns = 0;
	std::uint64_t tree_generation = 0;
	std::uint32_t frame_slot = 0;
	std::uint32_t width = 0;
	std::uint32_t height = 0;
	ObserverActivity observers = ObserverActivity::none;
	bool submitted = false;
	bool presented = false;
	bool canceled = false;
	bool internal_window = false;
	bool gpu_duration_available = false;
	bool interrupted = false;
};
/** A complete immutable artifact payload, owned before publication. */
struct AgenticArtifactPayload {
	std::vector<std::uint8_t> bytes;
	std::string artifact_id;
	std::string path;
	std::string media_type;
	std::string schema;
	std::string schema_version = "1.0";
	std::string integer_encoding = "decimal_strings";
};
/** One logical cutoff, with independent producer revisions and explicit limitations. */
struct AgenticCaptureSnapshot {
	std::vector<AgenticFrameRecord> frames;
	std::vector<AgenticCompletedImage> images;
	std::vector<AgenticArtifactPayload> artifacts;
	std::vector<std::pair<std::string, SectionCoverage>> coverage;
	std::vector<std::string> limitations;
	std::string run_id;
	std::string capture_id;
	std::string job_id;
	std::string requested_at_utc;
	Json provenance = Json::make_object();
	Json sources = Json::make_array();
	Json correlations = Json::make_array();
	Json actions = Json::make_array();
	AgenticCaptureRequest request;
	std::uint64_t first_app_tick = 0;
	std::uint64_t end_app_tick_exclusive = 0;
	std::uint64_t measurement_begin_ns = 0;
	std::uint64_t measurement_end_ns = 0;
	std::uint64_t timing_capture_generation = 0;
	bool partial = false;

	AgenticCaptureSnapshot() = default;
	AgenticCaptureSnapshot(AgenticCaptureSnapshot&&) noexcept = default;
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
