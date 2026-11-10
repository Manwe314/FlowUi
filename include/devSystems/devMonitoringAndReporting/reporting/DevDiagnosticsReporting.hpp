#pragma once

#include "FlowUi/Public/BuildConfig.hpp"

#if FLOW_UI_DEV_MODE

#include "devSystems/devMonitoringAndReporting/reporting/DevErrorReporting.hpp"
#include <string>

namespace FlowUi::devSystems {

/** Owned registered source text, independent of recorder/catalogue storage. */
struct DevDiagnosticsSource {
	/** @var DevDiagnosticsSource::name
	 * Owned name text, independent of recorder storage.
	 */
	/** @var DevDiagnosticsSource::file
	 * Owned file text, independent of recorder storage.
	 */
	/** @var DevDiagnosticsSource::function
	 * Owned function text, independent of recorder storage.
	 */
	std::string name{}, file{}, function{};
	/** Stable identity of this retained entity; zero means unassigned. */
	uint64_t id = 0;
	/** @var DevDiagnosticsSource::line
	 * Source line number; zero means unavailable.
	 */
	/** @var DevDiagnosticsSource::column
	 * Source column number; zero means unavailable.
	 */
	uint32_t line = 0, column = 0;
};

/** Owned breadcrumb descriptor; breadcrumb descriptors do not carry source locations. */
struct DevDiagnosticsBreadcrumbDescriptor {
	/** Display name for this source, track or descriptor. */
	std::string name{};
	/** Stable identity of this retained entity; zero means unassigned. */
	uint64_t id = 0;
};

/** Searchable context from every retained step, excluding native/evidence payloads. */
struct DevDiagnosticsStepReference {
	/** Producer context copied when this evidence was captured. */
	DevErrorContext context{};
	/** @var DevDiagnosticsStepReference::source_id
	 * Registered source identity referenced by this diagnostic step.
	 */
	/** @var DevDiagnosticsStepReference::sequence
	 * Monotonic capture sequence used to order retained records.
	 */
	uint64_t source_id = 0, sequence = 0;
	/** Producer thread track recorded with this diagnostic step. */
	uint32_t thread_track = 0;
};

/** One retained occurrence start, irrespective of its number of captured steps. */
struct DevDiagnosticsOccurrenceRow {
	/** Owned retained causal steps or their compact searchable references. */
	std::vector<DevDiagnosticsStepReference> steps{};
	/** Production error contract and compact payload captured for this occurrence. */
	FlowUiError error{};
	/** @var DevDiagnosticsOccurrenceRow::id
	 * Stable identity of this retained entity; zero means unassigned.
	 */
	/** @var DevDiagnosticsOccurrenceRow::first_app_tick
	 * First tick admitted to the capture or occurrence.
	 */
	/** @var DevDiagnosticsOccurrenceRow::last_app_tick
	 * Last application tick associated with the occurrence.
	 */
	uint64_t id = 0, first_app_tick = 0, last_app_tick = 0;
	/** @var DevDiagnosticsOccurrenceRow::first_timestamp_ns
	 * First observed occurrence timestamp in nanoseconds.
	 */
	/** @var DevDiagnosticsOccurrenceRow::last_timestamp_ns
	 * Last observed occurrence timestamp in nanoseconds.
	 */
	uint64_t first_timestamp_ns = 0, last_timestamp_ns = 0;
	/** Resolution applied or reported for the error contract. */
	ErrorResolution resolution = ErrorResolution::None;
	/** Retention, correlation or collection state of this record. */
	DevErrorOccurrenceState state = DevErrorOccurrenceState::Open;
	/** Bitmask of completion, provenance and capture-quality conditions. */
	DevErrorRecordFlag flags = DevErrorRecordFlag::None;
};

/** Coherent non-destructive list read. Timestamps are observations, not coverage guarantees. */
struct DevDiagnosticsSnapshot {
	/** Owned chronological diagnostic occurrence rows. */
	std::vector<DevDiagnosticsOccurrenceRow> rows{};
	/** Owned diagnostic source metadata independent of recorder storage. */
	std::vector<DevDiagnosticsSource> sources{};
	/** Owned breadcrumb names and identities. */
	std::vector<DevDiagnosticsBreadcrumbDescriptor> breadcrumb_descriptors{};
	/** Availability or quality state of this capture result. */
	DevErrorReportingStatus status{};
	/** Reporting retention configuration observed with this dataset. */
	DevErrorReportingConfig config{};
	/** Diagnostic producer capture configuration observed with this dataset. */
	DevErrorConfig capture_config{};
	/** Revision identifying the observed metadata, report or safe-point state. */
	uint64_t revision = 0;
	/** Whether a stack provider was available when the dataset was materialized. */
	bool stack_provider_available = false;
};

/** Prose owned by an investigation; indexed in occurrence/advice order. */
struct DevDiagnosticsAdviceText {
	/** @var DevDiagnosticsAdviceText::title
	 * Owned title prose; advice views are rebound to this storage.
	 */
	/** @var DevDiagnosticsAdviceText::explanation
	 * Owned explanation prose; advice views are rebound to this storage.
	 */
	/** @var DevDiagnosticsAdviceText::suggested_action
	 * Owned suggested action prose; advice views are rebound to this storage.
	 */
	/** @var DevDiagnosticsAdviceText::configuration_key
	 * Owned configuration key prose; advice views are rebound to this storage.
	 */
	/** @var DevDiagnosticsAdviceText::documentation
	 * Owned documentation prose; advice views are rebound to this storage.
	 */
	/** @var DevDiagnosticsAdviceText::limitation
	 * Owned evidence limitation prose; advice views are rebound to this storage.
	 */
	std::string title{}, explanation{}, suggested_action{}, configuration_key{}, documentation{},
		limitation{};
};

/** Complete bounded dataset shared by Freeze and both export formats. */
struct DevDiagnosticsInvestigation {
	/** Owned list view and metadata captured consistently with the investigation. */
	DevDiagnosticsSnapshot snapshot{};
	/** Owned retained diagnostic occurrences and their full bounded evidence. */
	std::vector<DevErrorOccurrence> occurrences{};
	/** Owned raw stack traces referenced by the retained occurrences. */
	std::vector<DevErrorStackTrace> stacks{};
	/** Owned error-triggered evidence captures. */
	std::vector<DevErrorTriggeredCapture> captures{};
	/** Owned advice prose used to rebind occurrence advice views after copying. */
	std::vector<DevDiagnosticsAdviceText> advice_text{};
	/** Optional owned allocation-free evidence from a fatal boundary. */
	std::optional<DevErrorFatalCapsule> fatal_capsule{};
	/** Optional exact retained occurrence selected for the investigation. */
	std::optional<DevErrorOccurrenceId> selected_id{};
	/** Estimated owned storage footprint in bytes. */
	uint64_t owned_bytes = 0;

	/** Construct an empty owned investigation dataset. */
	DevDiagnosticsInvestigation() noexcept = default;
	/**
	 * Copy or move owned evidence and ensure descriptor or advice views refer to this dataset's
	 * owned text.
	 */
	DevDiagnosticsInvestigation(const DevDiagnosticsInvestigation& other);
	/**
	 * Copy or move owned evidence and ensure descriptor or advice views refer to this dataset's
	 * owned text.
	 */
	DevDiagnosticsInvestigation(DevDiagnosticsInvestigation&& other) noexcept;
	/** Assign owned state, transferring moves or rebinding borrowed views after a copy as required. */
	DevDiagnosticsInvestigation& operator=(const DevDiagnosticsInvestigation& other);
	/** Assign owned state, transferring moves or rebinding borrowed views after a copy as required. */
	DevDiagnosticsInvestigation& operator=(DevDiagnosticsInvestigation&& other) noexcept;
	/** Rebind captured advice views after materialization or copying owned text. */
	void bind_advice_text() noexcept;
};

/** Calculate owned vector capacities and descriptor/prose storage, without recorder pointers. */
[[nodiscard]] uint64_t
/** Estimate owned vector capacities and descriptor/prose bytes without counting recorder pointers. */
diagnostics_owned_bytes(const DevDiagnosticsInvestigation& investigation) noexcept;

/** Select one exact retained ID from an owned live/frozen dataset without consulting reporting. */
[[nodiscard]] Result<DevDiagnosticsInvestigation>
/**
 * Create an owned investigation for one exact retained identity from an owned dataset. Return a
 * Result error if that identity is unavailable.
 */
select_diagnostics_investigation(const DevDiagnosticsInvestigation& investigation,
								 DevErrorOccurrenceId occurrence_id) noexcept;

} // namespace FlowUi::devSystems

#endif
