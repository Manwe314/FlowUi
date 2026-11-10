#pragma once

/** @file
 * @brief Public AgenticVerification declarations for optional development services.
 */
#include "FlowUi/Public/BuildConfig.hpp"
#if FLOW_UI_AGENTIC_WORKFLOW

#include "devSystems/devAgenticWorkflow/AgenticJson.hpp"

namespace FlowUi::devSystems::agentic {

/** Validate a finite suite and resolve all defaults without acquiring runtime control. */
[[nodiscard]] Result<Json> normalize_verification_request(const Json& request) noexcept;
/** Hash normalized execution identity, excluding retry identity and response preferences. */
[[nodiscard]] std::string verification_request_digest(const Json& normalized_request);
/** Page case details with a cursor bound to immutable report identity and a hard byte bound. */
[[nodiscard]] Result<Json> verification_report_page(const Json& report, const Json& arguments,
													std::string_view detail = "all") noexcept;
/** Count required assertions and checkpoints independently of command transport success. */
[[nodiscard]] Json verification_assertion_coverage(const Json& case_reports);
/** A definite mismatch fails; incomplete required observations or cleanup are inconclusive. */
[[nodiscard]] std::string_view verification_report_verdict(const Json& coverage,
														   bool execution_complete,
														   bool cleanup_complete) noexcept;

} // namespace FlowUi::devSystems::agentic

#endif
