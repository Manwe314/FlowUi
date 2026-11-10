#pragma once

#include "devSystems/devAgenticWorkflow/AgenticCommands.hpp"

#include <span>

namespace FlowUi::devSystems::agentic {

/* Parsed CLI state; JSON is the default and stdout contains exactly one envelope. */
struct AgenticCommandLine {
	AgenticCommandContext context;
	AgenticCommandRequest request;
	bool help = false;
};

/* Decode explicit arguments without evaluating a shell or modifying client configuration. */
[[nodiscard]] Result<AgenticCommandLine>
parse_command_line(std::span<const std::string_view> arguments) noexcept;
/* Install an authenticated selected-run handler when an instance was explicitly selected. */
[[nodiscard]] Status select_live_instance(AgenticCommandLine& command_line) noexcept;
/* Compact CLI orientation, also used for --help inside the JSON result. */
[[nodiscard]] Json command_line_help();

} // namespace FlowUi::devSystems::agentic
