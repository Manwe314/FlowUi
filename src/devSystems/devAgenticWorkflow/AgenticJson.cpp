#include "devSystems/devAgenticWorkflow/AgenticJson.hpp"
#include "devSystems/devAgenticWorkflow/AgenticTypes.hpp"

#include <algorithm>
#include <charconv>
#include <cmath>
#include <limits>
#include <stdexcept>

namespace FlowUi::devSystems::agentic {
namespace {
class Parser {
public:
	explicit Parser(std::string_view input) noexcept : input_(input) {}
	[[nodiscard]] bool parse(Json& output) {
		whitespace();
		return value(output, 0) && (whitespace(), position_ == input_.size());
	}

private:
	std::string_view input_;
	std::size_t position_ = 0;
	void whitespace() noexcept {
		while (position_ < input_.size() &&
			   (input_[position_] == ' ' || input_[position_] == '\t' ||
				input_[position_] == '\r' || input_[position_] == '\n'))
			++position_;
	}
	[[nodiscard]] bool take(char expected) noexcept {
		whitespace();
		if (position_ >= input_.size() || input_[position_] != expected)
			return false;
		++position_;
		return true;
	}
	[[nodiscard]] bool hex_quad(std::uint32_t& output) noexcept {
		if (input_.size() - position_ < 4)
			return false;
		output = 0;
		for (unsigned digit_index = 0; digit_index < 4; ++digit_index) {
			const auto digit = input_[position_++];
			const unsigned value =
				digit >= '0' && digit <= '9'   ? static_cast<unsigned>(digit - '0')
				: digit >= 'a' && digit <= 'f' ? static_cast<unsigned>(digit - 'a' + 10)
				: digit >= 'A' && digit <= 'F' ? static_cast<unsigned>(digit - 'A' + 10)
											   : 16u;
			if (value == 16u)
				return false;
			output = output * 16u + value;
		}
		return true;
	}
	static void append_utf8(std::string& output, std::uint32_t codepoint) {
		if (codepoint <= 0x7f)
			output.push_back(static_cast<char>(codepoint));
		else if (codepoint <= 0x7ff) {
			output.push_back(static_cast<char>(0xc0u | (codepoint >> 6u)));
			output.push_back(static_cast<char>(0x80u | (codepoint & 0x3fu)));
		} else if (codepoint <= 0xffff) {
			output.push_back(static_cast<char>(0xe0u | (codepoint >> 12u)));
			output.push_back(static_cast<char>(0x80u | ((codepoint >> 6u) & 0x3fu)));
			output.push_back(static_cast<char>(0x80u | (codepoint & 0x3fu)));
		} else {
			output.push_back(static_cast<char>(0xf0u | (codepoint >> 18u)));
			output.push_back(static_cast<char>(0x80u | ((codepoint >> 12u) & 0x3fu)));
			output.push_back(static_cast<char>(0x80u | ((codepoint >> 6u) & 0x3fu)));
			output.push_back(static_cast<char>(0x80u | (codepoint & 0x3fu)));
		}
	}
	[[nodiscard]] bool string(std::string& output) {
		if (position_ >= input_.size() || input_[position_++] != '"')
			return false;
		while (position_ < input_.size()) {
			const auto character = static_cast<unsigned char>(input_[position_++]);
			if (character == '"')
				return true;
			if (character < 0x20u)
				return false;
			if (character == '\\') {
				if (position_ == input_.size())
					return false;
				switch (input_[position_++]) {
				case '"':
					output.push_back('"');
					break;
				case '\\':
					output.push_back('\\');
					break;
				case '/':
					output.push_back('/');
					break;
				case 'b':
					output.push_back('\b');
					break;
				case 'f':
					output.push_back('\f');
					break;
				case 'n':
					output.push_back('\n');
					break;
				case 'r':
					output.push_back('\r');
					break;
				case 't':
					output.push_back('\t');
					break;
				case 'u': {
					std::uint32_t codepoint = 0;
					if (!hex_quad(codepoint))
						return false;
					if (codepoint >= 0xd800u && codepoint <= 0xdbffu) {
						if (input_.size() - position_ < 2 || input_[position_++] != '\\' ||
							input_[position_++] != 'u')
							return false;
						std::uint32_t low = 0;
						if (!hex_quad(low) || low < 0xdc00u || low > 0xdfffu)
							return false;
						codepoint = 0x10000u + ((codepoint - 0xd800u) << 10u) + low - 0xdc00u;
					} else if (codepoint >= 0xdc00u && codepoint <= 0xdfffu)
						return false;
					append_utf8(output, codepoint);
					break;
				}
				default:
					return false;
				}
			} else if (character < 0x80u)
				output.push_back(static_cast<char>(character));
			else {
				const unsigned continuation_count = character >= 0xc2u && character <= 0xdfu   ? 1u
													: character >= 0xe0u && character <= 0xefu ? 2u
													: character >= 0xf0u && character <= 0xf4u ? 3u
																							   : 0u;
				if (continuation_count == 0 || input_.size() - position_ < continuation_count)
					return false;
				std::uint32_t codepoint = character & (0x7fu >> continuation_count);
				for (unsigned continuation_index = 0; continuation_index < continuation_count;
					 ++continuation_index) {
					const auto next = static_cast<unsigned char>(input_[position_++]);
					if ((next & 0xc0u) != 0x80u)
						return false;
					codepoint = (codepoint << 6u) | (next & 0x3fu);
				}
				if ((continuation_count == 1 && codepoint < 0x80u) ||
					(continuation_count == 2 && codepoint < 0x800u) ||
					(continuation_count == 3 && codepoint < 0x10000u) || codepoint > 0x10ffffu ||
					(codepoint >= 0xd800u && codepoint <= 0xdfffu))
					return false;
				append_utf8(output, codepoint);
			}
		}
		return false;
	}
	[[nodiscard]] bool value(Json& output, unsigned depth) {
		if (depth > 64)
			return false;
		whitespace();
		if (position_ == input_.size())
			return false;
		const char first = input_[position_];
		if (first == '"') {
			output.kind = Json::Kind::string;
			return string(output.text);
		}
		if (first == '{') {
			++position_;
			output.kind = Json::Kind::object;
			if (take('}'))
				return true;
			do {
				whitespace();
				std::string key;
				if (!string(key) || !take(':') || output.find(key))
					return false;
				Json member;
				if (!value(member, depth + 1))
					return false;
				output.object.emplace_back(std::move(key), std::move(member));
				if (take('}'))
					return true;
			} while (take(','));
			return false;
		}
		if (first == '[') {
			++position_;
			output.kind = Json::Kind::array;
			if (take(']'))
				return true;
			do {
				Json member;
				if (!value(member, depth + 1))
					return false;
				output.array.emplace_back(std::move(member));
				if (take(']'))
					return true;
			} while (take(','));
			return false;
		}
		if (input_.substr(position_, 4) == "null") {
			position_ += 4;
			return true;
		}
		if (input_.substr(position_, 4) == "true") {
			position_ += 4;
			output = Json(true);
			return true;
		}
		if (input_.substr(position_, 5) == "false") {
			position_ += 5;
			output = Json(false);
			return true;
		}
		const std::size_t begin = position_;
		if (first == '-')
			++position_;
		if (position_ == input_.size())
			return false;
		if (input_[position_] == '0')
			++position_;
		else {
			if (input_[position_] < '1' || input_[position_] > '9')
				return false;
			while (position_ < input_.size() && input_[position_] >= '0' &&
				   input_[position_] <= '9')
				++position_;
		}
		if (position_ < input_.size() && input_[position_] == '.') {
			++position_;
			const auto digits_begin = position_;
			while (position_ < input_.size() && input_[position_] >= '0' &&
				   input_[position_] <= '9')
				++position_;
			if (digits_begin == position_)
				return false;
		}
		if (position_ < input_.size() && (input_[position_] == 'e' || input_[position_] == 'E')) {
			++position_;
			if (position_ < input_.size() && (input_[position_] == '+' || input_[position_] == '-'))
				++position_;
			const auto digits_begin = position_;
			while (position_ < input_.size() && input_[position_] >= '0' &&
				   input_[position_] <= '9')
				++position_;
			if (digits_begin == position_)
				return false;
		}
		output.kind = Json::Kind::number;
		output.text.assign(input_.substr(begin, position_ - begin));
		return true;
	}
};
void escaped(std::string& output, std::string_view input) {
	constexpr char hex[] = "0123456789abcdef";
	output.push_back('"');
	for (const auto character : input) {
		switch (character) {
		case '"':
			output += "\\\"";
			break;
		case '\\':
			output += "\\\\";
			break;
		case '\b':
			output += "\\b";
			break;
		case '\f':
			output += "\\f";
			break;
		case '\n':
			output += "\\n";
			break;
		case '\r':
			output += "\\r";
			break;
		case '\t':
			output += "\\t";
			break;
		default:
			if (static_cast<unsigned char>(character) < 0x20u) {
				output += "\\u00";
				output.push_back(hex[(static_cast<unsigned char>(character) >> 4u)]);
				output.push_back(hex[static_cast<unsigned char>(character) & 0xfu]);
			} else
				output.push_back(character);
		}
	}
	output.push_back('"');
}
void serialize(std::string& output, const Json& value) {
	switch (value.kind) {
	case Json::Kind::null:
		output += "null";
		break;
	case Json::Kind::boolean:
		output += value.boolean ? "true" : "false";
		break;
	case Json::Kind::number:
		output += value.text;
		break;
	case Json::Kind::string:
		escaped(output, value.text);
		break;
	case Json::Kind::array: {
		output.push_back('[');
		bool first = true;
		for (const auto& member : value.array) {
			if (!first)
				output.push_back(',');
			first = false;
			serialize(output, member);
		}
		output.push_back(']');
		break;
	}
	case Json::Kind::object: {
		std::vector<const std::pair<std::string, Json>*> members;
		members.reserve(value.object.size());
		for (const auto& member : value.object)
			members.emplace_back(&member);
		std::sort(members.begin(), members.end(), [](const auto* first, const auto* second) {
			return first->first < second->first;
		});
		output.push_back('{');
		bool first = true;
		for (const auto* member : members) {
			if (!first)
				output.push_back(',');
			first = false;
			escaped(output, member->first);
			output.push_back(':');
			serialize(output, member->second);
		}
		output.push_back('}');
		break;
	}
	}
}
} // namespace
Json::Json(double value) : kind(Kind::number) {
	if (!std::isfinite(value)) {
		kind = Kind::null;
		return;
	}
	char buffer[64];
	const auto converted =
		std::to_chars(buffer, buffer + sizeof(buffer), value, std::chars_format::general,
					  std::numeric_limits<double>::max_digits10);
	text.assign(buffer, converted.ptr);
}
Json Json::make_object(std::initializer_list<std::pair<std::string, Json>> fields) {
	Json output;
	output.kind = Kind::object;
	output.object.assign(fields);
	return output;
}
Json Json::make_array(std::initializer_list<Json> values) {
	Json output;
	output.kind = Kind::array;
	output.array.assign(values);
	return output;
}
Json Json::number(std::uint64_t value) {
	Json output;
	output.kind = Kind::number;
	output.text = std::to_string(value);
	return output;
}
const Json* Json::find(std::string_view key) const noexcept {
	for (const auto& member : object)
		if (member.first == key)
			return &member.second;
	return nullptr;
}
Json* Json::find(std::string_view key) noexcept {
	for (auto& member : object)
		if (member.first == key)
			return &member.second;
	return nullptr;
}
void Json::set(std::string key, Json value) {
	if (auto* member = find(key))
		*member = std::move(value);
	else {
		kind = Kind::object;
		object.emplace_back(std::move(key), std::move(value));
	}
}
std::string_view Json::string_or(std::string_view fallback) const noexcept {
	return kind == Kind::string ? std::string_view(text) : fallback;
}
double Json::number_or(double fallback) const noexcept {
	if (kind != Kind::number)
		return fallback;
	double output = 0;
	const auto parsed = std::from_chars(text.data(), text.data() + text.size(), output);
	return parsed.ec == std::errc{} && parsed.ptr == text.data() + text.size() &&
				   std::isfinite(output)
			   ? output
			   : fallback;
}
std::uint64_t Json::uint64_or(std::uint64_t fallback) const noexcept {
	if (kind != Kind::number && kind != Kind::string)
		return fallback;
	std::uint64_t output = 0;
	const auto parsed = std::from_chars(text.data(), text.data() + text.size(), output);
	return parsed.ec == std::errc{} && parsed.ptr == text.data() + text.size() ? output : fallback;
}
bool Json::bool_or(bool fallback) const noexcept {
	return kind == Kind::boolean ? boolean : fallback;
}
Result<Json> parse_json(std::string_view input, std::size_t maximum_bytes) noexcept {
	try {
		if (input.size() > maximum_bytes)
			return unexpectedError(agentic_error(AgenticError::budget_exhausted));
		Json output;
		if (!Parser(input).parse(output))
			return unexpectedError(agentic_error(AgenticError::invalid_request));
		return output;
	} catch (...) {
		return unexpectedError(makeError(ErrorCode::AllocationFailed, ErrorSite::AgenticCommand));
	}
}
std::string write_json(const Json& value) {
	std::string output;
	output.reserve(256);
	serialize(output, value);
	return output;
}
Json decimal(std::uint64_t value) {
	return Json(std::to_string(value));
}
Result<std::uint64_t> parse_decimal(const Json& value) noexcept {
	if (value.kind != Json::Kind::string || value.text.empty() ||
		(value.text.size() > 1 && value.text.front() == '0'))
		return unexpectedError(agentic_error(AgenticError::invalid_request));
	std::uint64_t output = 0;
	const auto parsed =
		std::from_chars(value.text.data(), value.text.data() + value.text.size(), output);
	if (parsed.ec != std::errc{} || parsed.ptr != value.text.data() + value.text.size())
		return unexpectedError(agentic_error(AgenticError::invalid_request));
	return output;
}
} // namespace FlowUi::devSystems::agentic
