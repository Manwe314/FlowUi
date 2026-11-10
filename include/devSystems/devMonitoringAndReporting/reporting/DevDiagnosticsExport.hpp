#pragma once

#include "FlowUi/Public/BuildConfig.hpp"

#if FLOW_UI_DEV_MODE

#include "devSystems/devMonitoringAndReporting/reporting/DevDiagnosticsReporting.hpp"
#include <filesystem>

namespace FlowUi::devSystems {

/** View definitions accompanying the exact owned evidence set. */
struct DevDiagnosticsExportMetadata {
	/** Owned human-readable description of the exported investigation filter. */
	std::string filter_description{};
	/** Exact diagnostic grouping key represented by the export. */
	DevErrorGroupKey group_key{};
	/** Grouping rule represented by group_key. */
	DevErrorGroupMode group_mode = DevErrorGroupMode::Contract;
	/** Whether the exported dataset is a frozen investigation. */
	bool frozen = false;
};

/** Serialize a versioned JSON bundle; integer values use exact decimal conversion. */
[[nodiscard]] Result<std::string>
/**
 * Serialize a versioned JSON bundle preserving exact integer evidence. Return serialization
 * failures through Result.
 */
serialize_diagnostics_json(const DevDiagnosticsInvestigation& investigation,
						   const DevDiagnosticsExportMetadata& metadata = {}) noexcept;

/** Generate readable advice/history plus the complete structured evidence appendix. */
[[nodiscard]] Result<std::string>
/**
 * Serialize readable diagnostic advice and history with a structured evidence appendix; return
 * failures through Result.
 */
serialize_diagnostics_markdown(const DevDiagnosticsInvestigation& investigation,
							   const DevDiagnosticsExportMetadata& metadata = {}) noexcept;

/** Write path_prefix.json and path_prefix.md; success requires both completed files.
 * Existing destinations are rejected. The caller runs this command outside drawing callbacks.
 */
[[nodiscard]] Status
/**
 * Write JSON and Markdown files beside path_prefix. Reject existing destinations; success
 * requires both files. Call outside drawing callbacks.
 */
write_diagnostics_export(const DevDiagnosticsInvestigation& investigation,
						 const std::filesystem::path& path_prefix,
						 const DevDiagnosticsExportMetadata& metadata = {}) noexcept;

} // namespace FlowUi::devSystems

#endif
