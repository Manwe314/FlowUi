#include "devSystems/devInterface/Performance/DevPerformanceSelection.hpp"
#if FLOW_UI_DEV_MODE
#include <algorithm>
#include <cctype>

namespace FlowUi::devSystems {
std::string performance_zone_label(std::string_view name) {
	std::string label;
	label.reserve(name.size());
	size_t offset = 0;
	while (offset < name.size()) {
		const size_t end = name.find_first_of("._ ", offset);
		const auto word = name.substr(offset, end == std::string_view::npos ? name.size() - offset
																			: end - offset);
		if (!word.empty()) {
			if (!label.empty())
				label += ' ';
			if (word == "flowui")
				label += "FlowUi";
			else if (word == "cpu")
				label += "CPU";
			else if (word == "gpu")
				label += "GPU";
			else if (word == "ui")
				label += "UI";
			else {
				label += static_cast<char>(std::toupper(static_cast<unsigned char>(word.front())));
				label.append(word.substr(1));
			}
		}
		if (end == std::string_view::npos)
			break;
		offset = end + 1;
	}
	return label;
}

bool performance_zone_matches(std::string_view name, std::string_view label,
							  std::string_view query) noexcept {
	const auto contains = [query](std::string_view candidate) {
		return std::search(candidate.begin(), candidate.end(), query.begin(), query.end(),
						   [](unsigned char left, unsigned char right) {
							   return std::tolower(left) == std::tolower(right);
						   }) != candidate.end();
	};
	return query.empty() || contains(name) || contains(label);
}
} // namespace FlowUi::devSystems
#endif
