#pragma once

#include "FlowUi/BuildConfig.hpp"

#if FLOW_UI_DEV_MODE

#include "devSystems/devMonitoringAndReporting/reporting/DevErrorReporting.hpp"
#include <string>

namespace FlowUi::devSystems {

/** Owned registered source text, independent of recorder/catalogue storage. */
struct DevDiagnosticsSource {
	std::string name{}, file{}, function{};
	uint64_t id = 0;
	uint32_t line = 0, column = 0;
};

/** Owned breadcrumb descriptor; breadcrumb descriptors do not carry source locations. */
struct DevDiagnosticsBreadcrumbDescriptor {
	std::string name{};
	uint64_t id = 0;
};

/** Searchable context from every retained step, excluding native/evidence payloads. */
struct DevDiagnosticsStepReference {
	DevErrorContext context{};
	uint64_t source_id = 0, sequence = 0;
	uint32_t thread_track = 0;
};

/** One retained occurrence start, irrespective of its number of captured steps. */
struct DevDiagnosticsOccurrenceRow {
	std::vector<DevDiagnosticsStepReference> steps{};
	FlowUiError error{};
	uint64_t id = 0, first_app_tick = 0, last_app_tick = 0;
	uint64_t first_timestamp_ns = 0, last_timestamp_ns = 0;
	ErrorResolution resolution = ErrorResolution::None;
	DevErrorOccurrenceState state = DevErrorOccurrenceState::Open;
	DevErrorRecordFlag flags = DevErrorRecordFlag::None;
};

/** Coherent non-destructive list read. Timestamps are observations, not coverage guarantees. */
struct DevDiagnosticsSnapshot {
	std::vector<DevDiagnosticsOccurrenceRow> rows{};
	std::vector<DevDiagnosticsSource> sources{};
	std::vector<DevDiagnosticsBreadcrumbDescriptor> breadcrumb_descriptors{};
	DevErrorReportingStatus status{};
	DevErrorReportingConfig config{};
	DevErrorConfig capture_config{};
	uint64_t revision = 0;
	bool stack_provider_available = false;
};

/** Prose owned by an investigation; indexed in occurrence/advice order. */
struct DevDiagnosticsAdviceText {
	std::string title{}, explanation{}, suggested_action{}, configuration_key{}, documentation{},
		limitation{};
};

/** Complete bounded dataset shared by Freeze and both export formats. */
struct DevDiagnosticsInvestigation {
	DevDiagnosticsSnapshot snapshot{};
	std::vector<DevErrorOccurrence> occurrences{};
	std::vector<DevErrorStackTrace> stacks{};
	std::vector<DevErrorTriggeredCapture> captures{};
	std::vector<DevDiagnosticsAdviceText> advice_text{};
	std::optional<DevErrorFatalCapsule> fatal_capsule{};
	std::optional<DevErrorOccurrenceId> selected_id{};
	uint64_t owned_bytes = 0;

	DevDiagnosticsInvestigation() noexcept = default;
	DevDiagnosticsInvestigation(const DevDiagnosticsInvestigation& other);
	DevDiagnosticsInvestigation(DevDiagnosticsInvestigation&& other) noexcept;
	DevDiagnosticsInvestigation& operator=(const DevDiagnosticsInvestigation& other);
	DevDiagnosticsInvestigation& operator=(DevDiagnosticsInvestigation&& other) noexcept;
	/** Rebind captured advice views after materialization or copying owned text. */
	void bind_advice_text() noexcept;
};

/** Calculate owned vector capacities and descriptor/prose storage, without recorder pointers. */
[[nodiscard]] uint64_t
diagnostics_owned_bytes(const DevDiagnosticsInvestigation& investigation) noexcept;

/** Select one exact retained ID from an owned live/frozen dataset without consulting reporting. */
[[nodiscard]] Result<DevDiagnosticsInvestigation>
select_diagnostics_investigation(const DevDiagnosticsInvestigation& investigation,
								 DevErrorOccurrenceId occurrence_id) noexcept;

} // namespace FlowUi::devSystems

#endif
