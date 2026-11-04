#include "command_line.hpp"
#include "devSystems/devAgenticWorkflow/AgenticTypes.hpp"

#include <iostream>
#include <vector>

int main(int argument_count, char** argument_values) {
	using namespace FlowUi::devSystems::agentic;
	try {
#ifdef _WIN32
		if (argument_count == 3 &&
			std::string_view(argument_values[1]) == "--flowui-launch-supervisor") {
			const auto status =
				run_agentic_launch_supervisor(std::filesystem::u8path(argument_values[2]));
			return status ? 0 : 4;
		}
#endif
		std::vector<std::string_view> arguments;
		arguments.reserve(static_cast<std::size_t>(argument_count));
		for (int index = 1; index < argument_count; ++index)
			arguments.emplace_back(argument_values[index]);
		auto command_line = parse_command_line(arguments);
		if (!command_line) {
			AgenticCommandRequest request;
			request.operation = "invalid";
			auto identifier = make_identifier("request");
			if (!identifier) {
				std::cerr << "flowui-agent: unable to allocate a request identity\n";
				return 4;
			}
			request.request_id = std::move(*identifier);
			std::cout << write_json(command_failure(request, 2, "invalid_arguments",
													command_line.error().message()))
					  << '\n';
			return 2;
		}
		if (command_line->help) {
			command_line->request.operation = "help";
			std::cout << write_json(command_success(command_line->request, command_line_help()))
					  << '\n';
			return 0;
		}
		auto selected = select_live_instance(*command_line);
		if (!selected) {
			std::cout << write_json(command_failure(command_line->request, 3,
													"instance_unavailable",
													selected.error().message()))
					  << '\n';
			return 3;
		}
		auto response = execute_agentic_command(command_line->context, command_line->request);
		std::cout << write_json(response.envelope) << '\n';
		return response.exit_code;
	} catch (...) {
		std::cerr << "flowui-agent: unable to create a bounded command response\n";
		return 4;
	}
}
