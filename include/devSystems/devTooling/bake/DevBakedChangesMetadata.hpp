#pragma once

#include <cstdint>
#include <string_view>

namespace FlowUi::baked {

/** Non-owning development catalogue of assignments embedded in this executable. */
struct CompiledChangeEntry {
	std::string_view field_path{};
	std::string_view owner_cpp_type{};
	std::string_view source_header{};
	std::string_view cpp_value{};
	std::string_view theme_variant{};
	std::string_view instance_debug_label{};
	std::string_view value_json{};
	std::uint64_t definition_id = 0;
	std::uint64_t theme_type = 0;
	std::uint64_t instance_key = 0;
	std::uint64_t field_id = 0;
	std::uint64_t value_type = 0;
	bool theme = false;
	bool exact_instance = false;
};

} // namespace FlowUi::baked
