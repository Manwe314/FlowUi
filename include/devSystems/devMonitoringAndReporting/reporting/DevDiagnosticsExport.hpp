#pragma once

#include "FlowUi/BuildConfig.hpp"

#if FLOW_UI_DEV_MODE

#include "devSystems/devMonitoringAndReporting/reporting/DevDiagnosticsReporting.hpp"
#include <filesystem>

namespace FlowUi::devSystems {

/** View definitions accompanying the exact owned evidence set. */
struct DevDiagnosticsExportMetadata {
	std::string filter_description{};
	DevErrorGroupKey group_key{};
	DevErrorGroupMode group_mode = DevErrorGroupMode::Contract;
	bool frozen = false;
};

/** Serialize a versioned JSON bundle; integer values use exact decimal conversion. */
[[nodiscard]] Result<std::string>
serialize_diagnostics_json(const DevDiagnosticsInvestigation& investigation,
						   const DevDiagnosticsExportMetadata& metadata = {}) noexcept;

/** Generate readable advice/history plus the complete structured evidence appendix. */
[[nodiscard]] Result<std::string>
serialize_diagnostics_markdown(const DevDiagnosticsInvestigation& investigation,
							   const DevDiagnosticsExportMetadata& metadata = {}) noexcept;

/** Write path_prefix.json and path_prefix.md; success requires both completed files.
 * Existing destinations are rejected. The caller runs this command outside drawing callbacks.
 */
[[nodiscard]] Status
write_diagnostics_export(const DevDiagnosticsInvestigation& investigation,
						 const std::filesystem::path& path_prefix,
						 const DevDiagnosticsExportMetadata& metadata = {}) noexcept;

} // namespace FlowUi::devSystems

#endif
