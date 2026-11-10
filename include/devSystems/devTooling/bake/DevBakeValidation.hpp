#pragma once

#include <array>
#include <cstdint>
#include <string_view>

namespace FlowUi::devSystems::tooling {

/** Restrict generated member access to complete C++ identifier segments. */
[[nodiscard]] constexpr bool generator_field_path_valid(std::string_view path) noexcept {
	bool segment_start = true;
	for (const unsigned char character : path) {
		if (character == '.') {
			if (segment_start)
				return false;
			segment_start = true;
			continue;
		}
		const bool identifier_start = (character >= 'a' && character <= 'z') ||
									  (character >= 'A' && character <= 'Z') || character == '_';
		if (!identifier_start && (segment_start || character < '0' || character > '9'))
			return false;
		segment_start = false;
	}
	return !segment_start;
}

/** Restrict emitted owner names to the registered C++ type vocabulary. */
[[nodiscard]] constexpr bool generator_cpp_type_valid(std::string_view type) noexcept {
	if (type.empty())
		return false;
	for (const unsigned char character : type) {
		const bool identifier_character =
			(character >= 'a' && character <= 'z') || (character >= 'A' && character <= 'Z') ||
			(character >= '0' && character <= '9') || character == '_';
		if (!identifier_character &&
			std::string_view(":<>, *").find(static_cast<char>(character)) == std::string_view::npos)
			return false;
	}
	return true;
}

/** Header names must fit one preprocessor include directive. */
[[nodiscard]] constexpr bool generator_source_header_valid(std::string_view header) noexcept {
	if (header.empty())
		return false;
	for (const unsigned char character : header)
		if (character < 32 || character == '"')
			return false;
	return true;
}

/** Exact file-content revision shared by the runtime review and generated executable. */
[[nodiscard]] constexpr std::uint64_t manifest_content_digest(std::string_view contents) noexcept {
	std::uint64_t digest = 14695981039346656037ull;
	for (const unsigned char character : contents) {
		digest ^= character;
		digest *= 1099511628211ull;
	}
	return digest;
}

/** Accept the initializer vocabulary emitted by runtime serialization, including quoted resource
 * keys. */
[[nodiscard]] constexpr bool generator_cpp_expression_valid(std::string_view expression) noexcept {
	if (expression.empty())
		return false;
	std::array<char, 128> delimiters{};
	std::size_t delimiter_count = 0;
	bool quoted = false;
	bool escaped = false;
	bool has_token = false;
	const auto identifier_character = [](unsigned char character) constexpr noexcept {
		return (character >= 'a' && character <= 'z') || (character >= 'A' && character <= 'Z') ||
			   (character >= '0' && character <= '9') || character == '_';
	};
	for (std::size_t character_index = 0; character_index < expression.size(); ++character_index) {
		const unsigned char character = expression[character_index];
		if (quoted) {
			if (character < 32)
				return false;
			if (escaped) {
				escaped = false;
				continue;
			}
			if (character == '\\')
				escaped = true;
			else if (character == '"')
				quoted = false;
			continue;
		}
		if (character == '"') {
			quoted = true;
			has_token = true;
			continue;
		}
		// Older generated initializers reference the removed no-op sampling type.
		// Reject them before generation; the same text inside a resource key remains valid.
		constexpr std::string_view obsolete_type = "FlowUi::TextureSamplingMode";
		const std::size_t type_end = character_index + obsolete_type.size();
		if (expression.substr(character_index).starts_with(obsolete_type) &&
			(character_index == 0 || !identifier_character(expression[character_index - 1])) &&
			(type_end == expression.size() || !identifier_character(expression[type_end])))
			return false;
		if (character == '{' || character == '(') {
			if (delimiter_count == delimiters.size())
				return false;
			delimiters[delimiter_count++] = static_cast<char>(character);
		} else if (character == '}' || character == ')') {
			if (delimiter_count == 0 ||
				delimiters[--delimiter_count] != (character == '}' ? '{' : '('))
				return false;
		}
		if (!identifier_character(character) &&
			std::string_view(":<>, \t\n\r.+-={}()").find(static_cast<char>(character)) ==
				std::string_view::npos)
			return false;
		if (identifier_character(character))
			has_token = true;
	}
	return has_token && !quoted && !escaped && delimiter_count == 0;
}

/** Runtime readiness and the host generator use the same metadata acceptance rules. */
[[nodiscard]] constexpr bool generator_entry_metadata_valid(std::string_view owner_type,
															std::string_view field_path,
															std::string_view source_header,
															std::string_view expression) noexcept {
	return generator_cpp_type_valid(owner_type) && generator_field_path_valid(field_path) &&
		   generator_source_header_valid(source_header) &&
		   generator_cpp_expression_valid(expression);
}

} // namespace FlowUi::devSystems::tooling
