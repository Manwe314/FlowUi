#pragma once

#include <cstdint>
#include <string_view>

namespace FlowUi::baked {

/** Non-owning development catalogue of assignments embedded in this executable. */
struct CompiledChangeEntry {
	/** @brief Static nested member path embedded by the generator. */
	std::string_view field_path{};
	/** @brief Static complete C++ owner type name. */
	std::string_view owner_cpp_type{};
	/** @brief Static header needed to compile the assignment. */
	std::string_view source_header{};
	/** @brief Static C++ initializer emitted for this assignment. */
	std::string_view cpp_value{};
	/** @brief Static target variant name for theme changes. */
	std::string_view theme_variant{};
	/** @brief Static diagnostic label for an exact-instance change. */
	std::string_view instance_debug_label{};
	/** @brief Static serialized value retained for review comparison. */
	std::string_view value_json{};
	/** @brief Element definition identity, or zero for a theme change. */
	std::uint64_t definition_id = 0;
	/** @brief Reflected theme type identity, or zero for an element change. */
	std::uint64_t theme_type = 0;
	/** @brief Exact-instance identity when exact_instance is set. */
	std::uint64_t instance_key = 0;
	/** @brief Reflected leaf field identity. */
	std::uint64_t field_id = 0;
	/** @brief Reflected assigned-value type identity. */
	std::uint64_t value_type = 0;
	/** @brief Whether this entry describes a theme assignment. */
	bool theme = false;
	/** @brief Whether an element assignment applies only to instance_key. */
	bool exact_instance = false;
};

} // namespace FlowUi::baked
