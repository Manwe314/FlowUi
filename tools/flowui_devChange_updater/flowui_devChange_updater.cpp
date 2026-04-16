#include <algorithm>
#include <cctype>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <limits>
#include <optional>
#include <sstream>
#include <string>
#include <string_view>
#include <unordered_map>
#include <utility>
#include <vector>

namespace {

struct JsonValue {
	enum class Kind : uint8_t {
		Null,
		Bool,
		Number,
		String,
		Array,
		Object,
	};

	Kind kind = Kind::Null;
	bool boolValue = false;
	std::string text{};
	std::vector<JsonValue> array{};
	std::vector<std::pair<std::string, JsonValue>> object{};
};

class JsonParser {
public:
	explicit JsonParser(std::string source) : source_(std::move(source)) {}

	bool parse(JsonValue& outRoot, std::string& outError) {
		index_ = 0u;
		if (!parseValue(outRoot, outError)) {
			return false;
		}
		skipWhitespace();
		if (!atEnd()) {
			outError = "Unexpected trailing characters after JSON root.";
			return false;
		}
		return true;
	}

private:
	bool parseValue(JsonValue& outValue, std::string& outError) {
		skipWhitespace();
		if (atEnd()) {
			outError = "Unexpected end of JSON input.";
			return false;
		}

		const char c = current();
		if (c == '{') {
			return parseObject(outValue, outError);
		}
		if (c == '[') {
			return parseArray(outValue, outError);
		}
		if (c == '"') {
			outValue.kind = JsonValue::Kind::String;
			return parseString(outValue.text, outError);
		}
		if (c == '-' || std::isdigit(static_cast<unsigned char>(c)) != 0) {
			outValue.kind = JsonValue::Kind::Number;
			return parseNumber(outValue.text, outError);
		}
		if (matchKeyword("true")) {
			outValue.kind = JsonValue::Kind::Bool;
			outValue.boolValue = true;
			return true;
		}
		if (matchKeyword("false")) {
			outValue.kind = JsonValue::Kind::Bool;
			outValue.boolValue = false;
			return true;
		}
		if (matchKeyword("null")) {
			outValue.kind = JsonValue::Kind::Null;
			return true;
		}

		outError = "Unexpected token while parsing JSON value.";
		return false;
	}

	bool parseObject(JsonValue& outValue, std::string& outError) {
		if (current() != '{') {
			outError = "Expected '{' while parsing object.";
			return false;
		}
		++index_;
		outValue.kind = JsonValue::Kind::Object;
		outValue.object.clear();

		skipWhitespace();
		if (!atEnd() && current() == '}') {
			++index_;
			return true;
		}

		while (!atEnd()) {
			skipWhitespace();
			if (atEnd() || current() != '"') {
				outError = "Expected object key string.";
				return false;
			}
			std::string key{};
			if (!parseString(key, outError)) {
				return false;
			}
			skipWhitespace();
			if (atEnd() || current() != ':') {
				outError = "Expected ':' after object key.";
				return false;
			}
			++index_;

			JsonValue value{};
			if (!parseValue(value, outError)) {
				return false;
			}
			outValue.object.emplace_back(std::move(key), std::move(value));

			skipWhitespace();
			if (!atEnd() && current() == ',') {
				++index_;
				continue;
			}
			if (!atEnd() && current() == '}') {
				++index_;
				return true;
			}
			outError = "Expected ',' or '}' while parsing object.";
			return false;
		}

		outError = "Unterminated JSON object.";
		return false;
	}

	bool parseArray(JsonValue& outValue, std::string& outError) {
		if (current() != '[') {
			outError = "Expected '[' while parsing array.";
			return false;
		}
		++index_;
		outValue.kind = JsonValue::Kind::Array;
		outValue.array.clear();

		skipWhitespace();
		if (!atEnd() && current() == ']') {
			++index_;
			return true;
		}

		while (!atEnd()) {
			JsonValue item{};
			if (!parseValue(item, outError)) {
				return false;
			}
			outValue.array.push_back(std::move(item));

			skipWhitespace();
			if (!atEnd() && current() == ',') {
				++index_;
				continue;
			}
			if (!atEnd() && current() == ']') {
				++index_;
				return true;
			}
			outError = "Expected ',' or ']' while parsing array.";
			return false;
		}

		outError = "Unterminated JSON array.";
		return false;
	}

	bool parseString(std::string& outText, std::string& outError) {
		if (current() != '"') {
			outError = "Expected string opening quote.";
			return false;
		}
		++index_;
		outText.clear();

		while (!atEnd()) {
			const char c = current();
			++index_;

			if (c == '"') {
				return true;
			}
			if (c == '\\') {
				if (atEnd()) {
					outError = "Invalid trailing escape in JSON string.";
					return false;
				}
				const char e = current();
				++index_;
				switch (e) {
				case '"':
				case '\\':
				case '/':
					outText.push_back(e);
					break;
				case 'b':
					outText.push_back('\b');
					break;
				case 'f':
					outText.push_back('\f');
					break;
				case 'n':
					outText.push_back('\n');
					break;
				case 'r':
					outText.push_back('\r');
					break;
				case 't':
					outText.push_back('\t');
					break;
				case 'u':
				{
					// Keep strict but simple: decode only ASCII range; otherwise placeholder.
					if (index_ + 4u > source_.size()) {
						outError = "Invalid unicode escape in JSON string.";
						return false;
					}
					unsigned value = 0u;
					for (int i = 0; i < 4; ++i) {
						const char h = source_[index_ + static_cast<std::size_t>(i)];
						value <<= 4u;
						if (h >= '0' && h <= '9') value += static_cast<unsigned>(h - '0');
						else if (h >= 'a' && h <= 'f') value += static_cast<unsigned>(10 + (h - 'a'));
						else if (h >= 'A' && h <= 'F') value += static_cast<unsigned>(10 + (h - 'A'));
						else {
							outError = "Invalid hex digit in unicode escape.";
							return false;
						}
					}
					index_ += 4u;
					if (value <= 0x7Fu) {
						outText.push_back(static_cast<char>(value));
					} else {
						outText.push_back('?');
					}
					break;
				}
				default:
					outError = "Unsupported escape in JSON string.";
					return false;
				}
				continue;
			}
			outText.push_back(c);
		}

		outError = "Unterminated JSON string.";
		return false;
	}

	bool parseNumber(std::string& outText, std::string& outError) {
		const std::size_t start = index_;

		if (current() == '-') {
			++index_;
		}
		if (atEnd()) {
			outError = "Invalid number.";
			return false;
		}

		if (current() == '0') {
			++index_;
		} else if (std::isdigit(static_cast<unsigned char>(current())) != 0) {
			while (!atEnd() && std::isdigit(static_cast<unsigned char>(current())) != 0) {
				++index_;
			}
		} else {
			outError = "Invalid number digits.";
			return false;
		}

		if (!atEnd() && current() == '.') {
			++index_;
			if (atEnd() || std::isdigit(static_cast<unsigned char>(current())) == 0) {
				outError = "Invalid fractional part in number.";
				return false;
			}
			while (!atEnd() && std::isdigit(static_cast<unsigned char>(current())) != 0) {
				++index_;
			}
		}

		if (!atEnd() && (current() == 'e' || current() == 'E')) {
			++index_;
			if (!atEnd() && (current() == '+' || current() == '-')) {
				++index_;
			}
			if (atEnd() || std::isdigit(static_cast<unsigned char>(current())) == 0) {
				outError = "Invalid exponent in number.";
				return false;
			}
			while (!atEnd() && std::isdigit(static_cast<unsigned char>(current())) != 0) {
				++index_;
			}
		}

		outText.assign(source_.substr(start, index_ - start));
		return true;
	}

	bool matchKeyword(std::string_view keyword) {
		if (index_ + keyword.size() > source_.size()) {
			return false;
		}
		if (source_.compare(index_, keyword.size(), keyword) != 0) {
			return false;
		}
		index_ += keyword.size();
		return true;
	}

	void skipWhitespace() {
		while (!atEnd() && std::isspace(static_cast<unsigned char>(current())) != 0) {
			++index_;
		}
	}

	bool atEnd() const {
		return index_ >= source_.size();
	}

	char current() const {
		return source_[index_];
	}

private:
	std::string source_{};
	std::size_t index_ = 0u;
};

const JsonValue* findObjectField(const JsonValue& object, std::string_view key) {
	if (object.kind != JsonValue::Kind::Object) {
		return nullptr;
	}
	for (const auto& [fieldName, fieldValue] : object.object) {
		if (fieldName == key) {
			return &fieldValue;
		}
	}
	return nullptr;
}

std::string trimCopy(std::string_view text) {
	std::size_t begin = 0u;
	std::size_t end = text.size();
	while (begin < end && std::isspace(static_cast<unsigned char>(text[begin])) != 0) {
		++begin;
	}
	while (end > begin && std::isspace(static_cast<unsigned char>(text[end - 1u])) != 0) {
		--end;
	}
	return std::string(text.substr(begin, end - begin));
}

bool jsonString(const JsonValue& value, std::string& out) {
	if (value.kind != JsonValue::Kind::String) {
		return false;
	}
	out = value.text;
	return true;
}

bool jsonUInt64(const JsonValue& value, uint64_t& out) {
	if (value.kind != JsonValue::Kind::Number) {
		return false;
	}
	try {
		std::size_t consumed = 0u;
		const unsigned long long parsed = std::stoull(value.text, &consumed, 10);
		if (consumed != value.text.size()) {
			return false;
		}
		out = static_cast<uint64_t>(parsed);
		return true;
	} catch (...) {
		return false;
	}
}

bool jsonUInt32(const JsonValue& value, uint32_t& out) {
	uint64_t tmp = 0u;
	if (!jsonUInt64(value, tmp) || tmp > static_cast<uint64_t>(std::numeric_limits<uint32_t>::max())) {
		return false;
	}
	out = static_cast<uint32_t>(tmp);
	return true;
}

std::string jsonEscape(std::string_view text) {
	std::string out{};
	out.reserve(text.size() + 8u);
	for (char c : text) {
		switch (c) {
		case '\"': out += "\\\""; break;
		case '\\': out += "\\\\"; break;
		case '\n': out += "\\n"; break;
		case '\r': out += "\\r"; break;
		case '\t': out += "\\t"; break;
		default:
			if (static_cast<unsigned char>(c) < 0x20u) {
				out += "\\u00";
				static constexpr char kHex[] = "0123456789abcdef";
				out.push_back(kHex[(static_cast<unsigned char>(c) >> 4u) & 0x0Fu]);
				out.push_back(kHex[static_cast<unsigned char>(c) & 0x0Fu]);
			} else {
				out.push_back(c);
			}
		}
	}
	return out;
}

std::string cppStringLiteral(std::string_view text) {
	return std::string("\"") + jsonEscape(text) + "\"";
}

struct ParsedChange {
	uint64_t fieldHash = 0u;
	std::string fieldName{};
	uint64_t fieldTypeHash = 0u;
	std::string jsonKind{};
	std::string valueLexeme{};
	std::string cppValue{};
};

struct ParsedDefinitionTarget {
	uint64_t definitionId = 0u;
	std::string definitionName{};
	std::vector<ParsedChange> changes{};
};

struct ParsedInstanceTarget {
	uint64_t definitionId = 0u;
	std::string definitionName{};
	uint64_t flowId = 0u;
	std::string elementId{};
	std::string sourceFile{};
	uint32_t sourceLine = 0u;
	std::vector<ParsedChange> changes{};
};

struct UnresolvedEntry {
	std::string scope{};
	uint64_t definitionId = 0u;
	uint64_t flowId = 0u;
	std::string elementId{};
	std::string sourceFile{};
	uint32_t sourceLine = 0u;
	uint64_t fieldHash = 0u;
	std::string fieldName{};
	std::string reason{};
};

struct OutputModel {
	std::string schema = "flowui.dev.export.v1";
	std::string kind = "params";
	std::vector<ParsedDefinitionTarget> definitions{};
	std::vector<ParsedInstanceTarget> instances{};
	std::vector<UnresolvedEntry> unresolved{};
};

bool parseChange(const JsonValue& value, ParsedChange& outChange, std::string& outError) {
	if (value.kind != JsonValue::Kind::Object) {
		outError = "Change entry is not an object.";
		return false;
	}
	const JsonValue* fieldHashValue = findObjectField(value, "fieldHash");
	const JsonValue* fieldNameValue = findObjectField(value, "fieldName");
	const JsonValue* fieldTypeHashValue = findObjectField(value, "fieldTypeHash");
	const JsonValue* wrappedValue = findObjectField(value, "value");
	if (!fieldHashValue || !fieldNameValue || !fieldTypeHashValue || !wrappedValue) {
		outError = "Missing change fields.";
		return false;
	}
	if (!jsonUInt64(*fieldHashValue, outChange.fieldHash)) {
		outError = "Invalid change.fieldHash.";
		return false;
	}
	if (!jsonString(*fieldNameValue, outChange.fieldName)) {
		outError = "Invalid change.fieldName.";
		return false;
	}
	if (!jsonUInt64(*fieldTypeHashValue, outChange.fieldTypeHash)) {
		outError = "Invalid change.fieldTypeHash.";
		return false;
	}
	if (wrappedValue->kind != JsonValue::Kind::Object) {
		outError = "Invalid change.value wrapper.";
		return false;
	}
	const JsonValue* kindValue = findObjectField(*wrappedValue, "kind");
	const JsonValue* rawValue = findObjectField(*wrappedValue, "value");
	if (!kindValue || !rawValue || !jsonString(*kindValue, outChange.jsonKind)) {
		outError = "Invalid change.value.kind.";
		return false;
	}

	if (outChange.jsonKind == "bool") {
		if (rawValue->kind != JsonValue::Kind::Bool) {
			outError = "bool change has non-bool value.";
			return false;
		}
		outChange.cppValue = rawValue->boolValue ? "true" : "false";
		outChange.valueLexeme = outChange.cppValue;
		return true;
	}
	if (outChange.jsonKind == "int64") {
		if (rawValue->kind != JsonValue::Kind::Number) {
			outError = "int64 change has non-number value.";
			return false;
		}
		outChange.cppValue = rawValue->text;
		outChange.valueLexeme = rawValue->text;
		return true;
	}
	if (outChange.jsonKind == "double") {
		if (rawValue->kind != JsonValue::Kind::Number) {
			outError = "double change has non-number value.";
			return false;
		}
		outChange.cppValue = rawValue->text;
		outChange.valueLexeme = rawValue->text;
		return true;
	}
	if (outChange.jsonKind == "string") {
		if (rawValue->kind != JsonValue::Kind::String) {
			outError = "string change has non-string value.";
			return false;
		}
		outChange.cppValue = cppStringLiteral(rawValue->text);
		outChange.valueLexeme = rawValue->text;
		return true;
	}
	if (outChange.jsonKind == "null") {
		outError = "null values are not patchable in V1.";
		return false;
	}

	outError = "Unsupported change.value.kind: " + outChange.jsonKind;
	return false;
}

bool parseInputModel(const JsonValue& root, OutputModel& outModel, std::string& outError) {
	if (root.kind != JsonValue::Kind::Object) {
		outError = "Root JSON value must be an object.";
		return false;
	}

	if (const JsonValue* schemaValue = findObjectField(root, "schema")) {
		(void)jsonString(*schemaValue, outModel.schema);
	}
	if (const JsonValue* kindValue = findObjectField(root, "kind")) {
		(void)jsonString(*kindValue, outModel.kind);
	}

	const JsonValue* definitions = findObjectField(root, "definitions");
	if (!definitions || definitions->kind != JsonValue::Kind::Array) {
		outError = "Missing or invalid root.definitions.";
		return false;
	}
	const JsonValue* instances = findObjectField(root, "instances");
	if (!instances || instances->kind != JsonValue::Kind::Array) {
		outError = "Missing or invalid root.instances.";
		return false;
	}

	for (const JsonValue& item : definitions->array) {
		if (item.kind != JsonValue::Kind::Object) {
			outError = "Definition entry is not an object.";
			return false;
		}
		ParsedDefinitionTarget target{};
		const JsonValue* definitionIdValue = findObjectField(item, "definitionId");
		const JsonValue* definitionNameValue = findObjectField(item, "definitionName");
		const JsonValue* changesValue = findObjectField(item, "changes");
		if (!definitionIdValue || !definitionNameValue || !changesValue) {
			outError = "Definition entry is missing required fields.";
			return false;
		}
		if (!jsonUInt64(*definitionIdValue, target.definitionId) || !jsonString(*definitionNameValue, target.definitionName)) {
			outError = "Invalid definition entry fields.";
			return false;
		}
		if (changesValue->kind != JsonValue::Kind::Array) {
			outError = "Definition changes is not an array.";
			return false;
		}
		for (const JsonValue& changeValue : changesValue->array) {
			ParsedChange change{};
			if (!parseChange(changeValue, change, outError)) {
				return false;
			}
			target.changes.push_back(std::move(change));
		}
		outModel.definitions.push_back(std::move(target));
	}

	for (const JsonValue& item : instances->array) {
		if (item.kind != JsonValue::Kind::Object) {
			outError = "Instance entry is not an object.";
			return false;
		}
		ParsedInstanceTarget target{};
		const JsonValue* definitionIdValue = findObjectField(item, "definitionId");
		const JsonValue* definitionNameValue = findObjectField(item, "definitionName");
		const JsonValue* flowIdValue = findObjectField(item, "flowId");
		const JsonValue* elementIdValue = findObjectField(item, "elementId");
		const JsonValue* sourceFileValue = findObjectField(item, "sourceFile");
		const JsonValue* sourceLineValue = findObjectField(item, "sourceLine");
		const JsonValue* changesValue = findObjectField(item, "changes");
		if (!definitionIdValue || !definitionNameValue || !flowIdValue || !elementIdValue ||
			!sourceFileValue || !sourceLineValue || !changesValue) {
			outError = "Instance entry is missing required fields.";
			return false;
		}
		if (!jsonUInt64(*definitionIdValue, target.definitionId) ||
			!jsonString(*definitionNameValue, target.definitionName) ||
			!jsonUInt64(*flowIdValue, target.flowId) ||
			!jsonString(*elementIdValue, target.elementId) ||
			!jsonString(*sourceFileValue, target.sourceFile) ||
			!jsonUInt32(*sourceLineValue, target.sourceLine)) {
			outError = "Invalid instance entry fields.";
			return false;
		}
		if (changesValue->kind != JsonValue::Kind::Array) {
			outError = "Instance changes is not an array.";
			return false;
		}
		for (const JsonValue& changeValue : changesValue->array) {
			ParsedChange change{};
			if (!parseChange(changeValue, change, outError)) {
				return false;
			}
			target.changes.push_back(std::move(change));
		}
		outModel.instances.push_back(std::move(target));
	}

	return true;
}

struct ScanState {
	bool inString = false;
	bool inChar = false;
	bool inLineComment = false;
	bool inBlockComment = false;
	bool escaped = false;
};

void scanAdvance(ScanState& state, const std::string& text, std::size_t index) {
	const char c = text[index];
	const char n = (index + 1u < text.size()) ? text[index + 1u] : '\0';

	if (state.inLineComment) {
		if (c == '\n') {
			state.inLineComment = false;
		}
		return;
	}
	if (state.inBlockComment) {
		if (c == '*' && n == '/') {
			state.inBlockComment = false;
		}
		return;
	}
	if (state.inString) {
		if (state.escaped) {
			state.escaped = false;
			return;
		}
		if (c == '\\') {
			state.escaped = true;
			return;
		}
		if (c == '"') {
			state.inString = false;
		}
		return;
	}
	if (state.inChar) {
		if (state.escaped) {
			state.escaped = false;
			return;
		}
		if (c == '\\') {
			state.escaped = true;
			return;
		}
		if (c == '\'') {
			state.inChar = false;
		}
		return;
	}

	if (c == '/' && n == '/') {
		state.inLineComment = true;
		return;
	}
	if (c == '/' && n == '*') {
		state.inBlockComment = true;
		return;
	}
	if (c == '"') {
		state.inString = true;
		return;
	}
	if (c == '\'') {
		state.inChar = true;
		return;
	}
}

bool isCodePosition(const ScanState& state) {
	return !(state.inString || state.inChar || state.inLineComment || state.inBlockComment);
}

std::size_t findMatchingBracket(const std::string& text, std::size_t openPos, char openChar, char closeChar) {
	if (openPos >= text.size() || text[openPos] != openChar) {
		return std::string::npos;
	}
	ScanState state{};
	int depth = 0;
	for (std::size_t i = openPos; i < text.size(); ++i) {
		if (isCodePosition(state)) {
			if (text[i] == openChar) {
				++depth;
			} else if (text[i] == closeChar) {
				--depth;
				if (depth == 0) {
					return i;
				}
			}
		}
		scanAdvance(state, text, i);
	}
	return std::string::npos;
}

std::size_t findOutside(const std::string& text, std::string_view needle, std::size_t begin, std::size_t endExclusive) {
	if (needle.empty() || begin >= text.size()) {
		return std::string::npos;
	}
	endExclusive = std::min(endExclusive, text.size());
	ScanState state{};
	for (std::size_t i = begin; i + needle.size() <= endExclusive; ++i) {
		if (isCodePosition(state) && text.compare(i, needle.size(), needle) == 0) {
			return i;
		}
		scanAdvance(state, text, i);
	}
	return std::string::npos;
}

std::size_t findCharOutside(const std::string& text, char needle, std::size_t begin) {
	if (begin >= text.size()) {
		return std::string::npos;
	}
	ScanState state{};
	for (std::size_t i = begin; i < text.size(); ++i) {
		if (isCodePosition(state) && text[i] == needle) {
			return i;
		}
		scanAdvance(state, text, i);
	}
	return std::string::npos;
}

std::vector<std::size_t> lineOffsets(const std::string& text) {
	std::vector<std::size_t> offsets{};
	offsets.reserve(256u);
	offsets.push_back(0u);
	for (std::size_t i = 0; i < text.size(); ++i) {
		if (text[i] == '\n') {
			offsets.push_back(i + 1u);
		}
	}
	return offsets;
}

std::size_t lineToOffset(const std::vector<std::size_t>& offsets, uint32_t line) {
	if (line == 0u) {
		return std::string::npos;
	}
	const std::size_t index = static_cast<std::size_t>(line - 1u);
	if (index >= offsets.size()) {
		return std::string::npos;
	}
	return offsets[index];
}

std::size_t lineEndOffset(const std::string& text, const std::vector<std::size_t>& offsets, uint32_t line) {
	if (line == 0u) {
		return std::string::npos;
	}
	const std::size_t idx = static_cast<std::size_t>(line - 1u);
	if (idx >= offsets.size()) {
		return std::string::npos;
	}
	if (idx + 1u < offsets.size()) {
		return offsets[idx + 1u] - 1u;
	}
	return text.size();
}

std::size_t lineStartForOffset(const std::string& text, std::size_t offset) {
	if (offset > text.size()) {
		return std::string::npos;
	}
	std::size_t i = offset;
	while (i > 0u && text[i - 1u] != '\n') {
		--i;
	}
	return i;
}

std::string leadingWhitespaceAt(const std::string& text, std::size_t offset) {
	const std::size_t start = lineStartForOffset(text, offset);
	if (start == std::string::npos) {
		return {};
	}
	std::size_t i = start;
	while (i < text.size() && (text[i] == ' ' || text[i] == '\t')) {
		++i;
	}
	return std::string(text.substr(start, i - start));
}

std::string buildSetParametersBlock(
	const std::string& callIndent,
	const std::unordered_map<std::string, std::string>& fieldValueByName) {
	std::vector<std::pair<std::string, std::string>> fields{};
	fields.reserve(fieldValueByName.size());
	for (const auto& kv : fieldValueByName) {
		fields.emplace_back(kv.first, kv.second);
	}
	std::sort(
		fields.begin(),
		fields.end(),
		[](const auto& a, const auto& b) { return a.first < b.first; });

	std::string out{};
	out += "\n" + callIndent + ".setParameters({";
	for (std::size_t i = 0; i < fields.size(); ++i) {
		out += "\n" + callIndent + "    ." + fields[i].first + " = " + fields[i].second;
		if (i + 1u < fields.size()) {
			out += ",";
		}
	}
	out += "\n" + callIndent + "})";
	return out;
}

std::string buildMergeParamsBlock(
	const std::string& callIndent,
	const std::unordered_map<std::string, std::string>& fieldValueByName,
	bool prependVariableComment) {
	std::vector<std::pair<std::string, std::string>> fields{};
	fields.reserve(fieldValueByName.size());
	for (const auto& kv : fieldValueByName) {
		fields.emplace_back(kv.first, kv.second);
	}
	std::sort(
		fields.begin(),
		fields.end(),
		[](const auto& a, const auto& b) { return a.first < b.first; });

	std::string out{};
	if (prependVariableComment) {
		out += "\n" + callIndent + "/* V1 cant Update parameters made with variables */";
	}
	out += "\n" + callIndent + ".mergeParams([](auto& params) {";
	for (const auto& [fieldName, fieldValue] : fields) {
		out += "\n" + callIndent + "    params." + fieldName + " = " + fieldValue + ";";
	}
	out += "\n" + callIndent + "})";
	return out;
}

bool findOuterBraces(std::string_view arg, std::size_t& outOpen, std::size_t& outClose) {
	outOpen = std::string::npos;
	outClose = std::string::npos;
	ScanState state{};
	int depth = 0;
	for (std::size_t i = 0; i < arg.size(); ++i) {
		if (isCodePosition(state)) {
			if (arg[i] == '{') {
				if (outOpen == std::string::npos) {
					outOpen = i;
				}
				++depth;
			} else if (arg[i] == '}') {
				--depth;
				if (depth == 0 && outOpen != std::string::npos) {
					outClose = i;
					break;
				}
			}
		}

		const char c = arg[i];
		const char n = (i + 1u < arg.size()) ? arg[i + 1u] : '\0';
		if (state.inLineComment) {
			if (c == '\n') state.inLineComment = false;
			continue;
		}
		if (state.inBlockComment) {
			if (c == '*' && n == '/') state.inBlockComment = false;
			continue;
		}
		if (state.inString) {
			if (state.escaped) state.escaped = false;
			else if (c == '\\') state.escaped = true;
			else if (c == '"') state.inString = false;
			continue;
		}
		if (state.inChar) {
			if (state.escaped) state.escaped = false;
			else if (c == '\\') state.escaped = true;
			else if (c == '\'') state.inChar = false;
			continue;
		}
		if (c == '/' && n == '/') { state.inLineComment = true; continue; }
		if (c == '/' && n == '*') { state.inBlockComment = true; continue; }
		if (c == '"') { state.inString = true; continue; }
		if (c == '\'') { state.inChar = true; continue; }
	}
	if (outOpen == std::string::npos || outClose == std::string::npos) {
		return false;
	}
	// ensure no code tokens after closing brace except whitespace.
	for (std::size_t i = outClose + 1u; i < arg.size(); ++i) {
		if (std::isspace(static_cast<unsigned char>(arg[i])) == 0) {
			// suffix is allowed (for templates etc). still considered brace-based.
			break;
		}
	}
	return true;
}

std::vector<std::string> splitTopLevelCommaSeparated(std::string_view text) {
	std::vector<std::string> segments{};
	std::size_t segmentStart = 0u;
	ScanState state{};
	int braceDepth = 0;
	int parenDepth = 0;
	int bracketDepth = 0;
	for (std::size_t i = 0; i < text.size(); ++i) {
		if (isCodePosition(state)) {
			const char c = text[i];
			if (c == '{') ++braceDepth;
			else if (c == '}') --braceDepth;
			else if (c == '(') ++parenDepth;
			else if (c == ')') --parenDepth;
			else if (c == '[') ++bracketDepth;
			else if (c == ']') --bracketDepth;
			else if (c == ',' && braceDepth == 0 && parenDepth == 0 && bracketDepth == 0) {
				segments.push_back(trimCopy(text.substr(segmentStart, i - segmentStart)));
				segmentStart = i + 1u;
			}
		}
		// local scan for string/comment states
		const char c = text[i];
		const char n = (i + 1u < text.size()) ? text[i + 1u] : '\0';
		if (state.inLineComment) {
			if (c == '\n') state.inLineComment = false;
			continue;
		}
		if (state.inBlockComment) {
			if (c == '*' && n == '/') state.inBlockComment = false;
			continue;
		}
		if (state.inString) {
			if (state.escaped) state.escaped = false;
			else if (c == '\\') state.escaped = true;
			else if (c == '"') state.inString = false;
			continue;
		}
		if (state.inChar) {
			if (state.escaped) state.escaped = false;
			else if (c == '\\') state.escaped = true;
			else if (c == '\'') state.inChar = false;
			continue;
		}
		if (c == '/' && n == '/') { state.inLineComment = true; continue; }
		if (c == '/' && n == '*') { state.inBlockComment = true; continue; }
		if (c == '"') { state.inString = true; continue; }
		if (c == '\'') { state.inChar = true; continue; }
	}
	if (segmentStart <= text.size()) {
		segments.push_back(trimCopy(text.substr(segmentStart)));
	}
	return segments;
}

bool parseDesignatedAssignments(
	std::string_view body,
	std::unordered_map<std::string, std::string>& outValues,
	std::vector<std::string>& outOrder) {
	outValues.clear();
	outOrder.clear();
	const std::vector<std::string> segments = splitTopLevelCommaSeparated(body);
	for (const std::string& rawSegment : segments) {
		const std::string segment = trimCopy(rawSegment);
		if (segment.empty()) {
			continue;
		}
		if (segment[0] != '.') {
			return false;
		}
		const std::size_t eqPos = segment.find('=');
		if (eqPos == std::string::npos) {
			return false;
		}
		const std::string lhs = trimCopy(std::string_view(segment).substr(0u, eqPos));
		const std::string rhs = trimCopy(std::string_view(segment).substr(eqPos + 1u));
		if (lhs.size() < 2u || lhs[0] != '.') {
			return false;
		}
		const std::string fieldName = lhs.substr(1u);
		if (fieldName.empty()) {
			return false;
		}
		if (outValues.find(fieldName) == outValues.end()) {
			outOrder.push_back(fieldName);
		}
		outValues[fieldName] = rhs;
	}
	return true;
}

void replaceRange(std::string& text, std::size_t begin, std::size_t endExclusive, std::string_view replacement) {
	text.replace(begin, endExclusive - begin, replacement);
}

struct PatchResult {
	bool patched = false;
	std::vector<UnresolvedEntry> unresolved{};
};

PatchResult patchInstanceTarget(std::string& content, const ParsedInstanceTarget& target) {
	PatchResult result{};

	const std::vector<std::size_t> lines = lineOffsets(content);
	const std::size_t lineStart = lineToOffset(lines, target.sourceLine);
	const std::size_t lineEnd = lineEndOffset(content, lines, target.sourceLine);
	if (lineStart == std::string::npos || lineEnd == std::string::npos || lineStart >= lineEnd) {
		for (const ParsedChange& change : target.changes) {
			result.unresolved.push_back(UnresolvedEntry{
				.scope = "instance",
				.definitionId = target.definitionId,
				.flowId = target.flowId,
				.elementId = target.elementId,
				.sourceFile = target.sourceFile,
				.sourceLine = target.sourceLine,
				.fieldHash = change.fieldHash,
				.fieldName = change.fieldName,
				.reason = "Source line is outside file bounds.",
			});
		}
		return result;
	}

	const std::string_view lineText(content.data() + lineStart, lineEnd - lineStart);
	const std::size_t createLocalPos = lineText.find(".createElement");
	if (createLocalPos == std::string::npos) {
		for (const ParsedChange& change : target.changes) {
			result.unresolved.push_back(UnresolvedEntry{
				.scope = "instance",
				.definitionId = target.definitionId,
				.flowId = target.flowId,
				.elementId = target.elementId,
				.sourceFile = target.sourceFile,
				.sourceLine = target.sourceLine,
				.fieldHash = change.fieldHash,
				.fieldName = change.fieldName,
				.reason = "Could not find .createElement on source line.",
			});
		}
		return result;
	}

	const std::size_t createPos = lineStart + createLocalPos;
	const std::size_t createNamePos = createPos + std::string(".createElement").size();
	const std::size_t createOpenParen = content.find('(', createNamePos);
	if (createOpenParen == std::string::npos) {
		for (const ParsedChange& change : target.changes) {
			result.unresolved.push_back(UnresolvedEntry{
				.scope = "instance",
				.definitionId = target.definitionId,
				.flowId = target.flowId,
				.elementId = target.elementId,
				.sourceFile = target.sourceFile,
				.sourceLine = target.sourceLine,
				.fieldHash = change.fieldHash,
				.fieldName = change.fieldName,
				.reason = "Malformed .createElement call.",
			});
		}
		return result;
	}
	const std::size_t createCloseParen = findMatchingBracket(content, createOpenParen, '(', ')');
	if (createCloseParen == std::string::npos) {
		for (const ParsedChange& change : target.changes) {
			result.unresolved.push_back(UnresolvedEntry{
				.scope = "instance",
				.definitionId = target.definitionId,
				.flowId = target.flowId,
				.elementId = target.elementId,
				.sourceFile = target.sourceFile,
				.sourceLine = target.sourceLine,
				.fieldHash = change.fieldHash,
				.fieldName = change.fieldName,
				.reason = "Unbalanced parentheses in .createElement call.",
			});
		}
		return result;
	}

	const std::size_t afterCreate = createCloseParen + 1u;
	const std::size_t chainSemicolon = findCharOutside(content, ';', afterCreate);
	const std::size_t searchEnd = (chainSemicolon == std::string::npos) ? content.size() : chainSemicolon;
	const std::size_t drawPos = findOutside(content, ".draw", afterCreate, searchEnd);
	const std::size_t setParametersPos = [&]() -> std::size_t {
		const std::size_t first = findOutside(content, ".setParameters", afterCreate, searchEnd);
		if (first != std::string::npos) return first;
		return findOutside(content, ".setParams", afterCreate, searchEnd);
	}();

	std::unordered_map<std::string, std::string> fieldValues{};
	for (const ParsedChange& change : target.changes) {
		fieldValues[change.fieldName] = change.cppValue;
	}
	if (fieldValues.empty()) {
		return result;
	}

	// No setParameters call: insert one after createElement(...) call.
	if (setParametersPos == std::string::npos) {
		std::string indent = {};
		if (drawPos != std::string::npos) {
			indent = leadingWhitespaceAt(content, drawPos);
		}
		if (indent.empty()) {
			indent = leadingWhitespaceAt(content, createPos);
			indent += "\t";
		}
		const std::string snippet = buildSetParametersBlock(indent, fieldValues);
		replaceRange(content, afterCreate, afterCreate, snippet);
		result.patched = true;
		return result;
	}

	const std::size_t setNameEnd = content.find('(', setParametersPos);
	if (setNameEnd == std::string::npos) {
		for (const ParsedChange& change : target.changes) {
			result.unresolved.push_back(UnresolvedEntry{
				.scope = "instance",
				.definitionId = target.definitionId,
				.flowId = target.flowId,
				.elementId = target.elementId,
				.sourceFile = target.sourceFile,
				.sourceLine = target.sourceLine,
				.fieldHash = change.fieldHash,
				.fieldName = change.fieldName,
				.reason = "Malformed .setParameters call.",
			});
		}
		return result;
	}
	const std::size_t setClose = findMatchingBracket(content, setNameEnd, '(', ')');
	if (setClose == std::string::npos) {
		for (const ParsedChange& change : target.changes) {
			result.unresolved.push_back(UnresolvedEntry{
				.scope = "instance",
				.definitionId = target.definitionId,
				.flowId = target.flowId,
				.elementId = target.elementId,
				.sourceFile = target.sourceFile,
				.sourceLine = target.sourceLine,
				.fieldHash = change.fieldHash,
				.fieldName = change.fieldName,
				.reason = "Unbalanced parentheses in .setParameters call.",
			});
		}
		return result;
	}

	const std::size_t argStart = setNameEnd + 1u;
	const std::size_t argEnd = setClose;
	const std::string argText = content.substr(argStart, argEnd - argStart);
	const std::string argTrimmed = trimCopy(argText);

	std::string setIndent = leadingWhitespaceAt(content, setParametersPos);
	if (setIndent.empty()) {
		setIndent = leadingWhitespaceAt(content, createPos);
		setIndent += "\t";
	}

	std::size_t braceOpen = std::string::npos;
	std::size_t braceClose = std::string::npos;
	const bool hasBraces = findOuterBraces(argTrimmed, braceOpen, braceClose);

	// Variable/expression argument: append mergeParams patching call.
	if (!hasBraces) {
		const std::string snippet = buildMergeParamsBlock(setIndent, fieldValues, true);
		replaceRange(content, setClose + 1u, setClose + 1u, snippet);
		result.patched = true;
		return result;
	}

	// Brace-based argument: update/add designated fields.
	const std::string prefix = argTrimmed.substr(0u, braceOpen);
	const std::string body = argTrimmed.substr(braceOpen + 1u, braceClose - braceOpen - 1u);
	const std::string suffix = argTrimmed.substr(braceClose + 1u);

	std::unordered_map<std::string, std::string> parsedValues{};
	std::vector<std::string> fieldOrder{};
	if (!parseDesignatedAssignments(body, parsedValues, fieldOrder)) {
		const std::string snippet = buildMergeParamsBlock(setIndent, fieldValues, false);
		replaceRange(content, setClose + 1u, setClose + 1u, snippet);
		result.patched = true;
		return result;
	}

	for (const auto& [fieldName, value] : fieldValues) {
		if (parsedValues.find(fieldName) == parsedValues.end()) {
			fieldOrder.push_back(fieldName);
		}
		parsedValues[fieldName] = value;
	}

	std::string rebuiltBody{};
	for (std::size_t i = 0; i < fieldOrder.size(); ++i) {
		const std::string& name = fieldOrder[i];
		rebuiltBody += "\n" + setIndent + "    ." + name + " = " + parsedValues[name];
		if (i + 1u < fieldOrder.size()) {
			rebuiltBody += ",";
		}
	}
	std::string rebuiltArg = prefix + "{";
	rebuiltArg += rebuiltBody;
	rebuiltArg += "\n" + setIndent + "}";
	rebuiltArg += suffix;

	replaceRange(content, argStart, argEnd, rebuiltArg);
	result.patched = true;
	return result;
}

bool readFileText(const std::filesystem::path& path, std::string& outText, std::string& outError) {
	std::ifstream file(path, std::ios::binary);
	if (!file.is_open()) {
		outError = "Failed to open file: " + path.string();
		return false;
	}
	std::ostringstream stream{};
	stream << file.rdbuf();
	if (!file.good() && !file.eof()) {
		outError = "Failed to read file: " + path.string();
		return false;
	}
	outText = stream.str();
	return true;
}

bool writeFileText(const std::filesystem::path& path, std::string_view text, std::string& outError) {
	const std::filesystem::path parent = path.parent_path();
	if (!parent.empty()) {
		std::error_code ec{};
		std::filesystem::create_directories(parent, ec);
		if (ec) {
			outError = "Failed to create output directory: " + parent.string();
			return false;
		}
	}
	std::ofstream file(path, std::ios::binary | std::ios::trunc);
	if (!file.is_open()) {
		outError = "Failed to open file for writing: " + path.string();
		return false;
	}
	file.write(text.data(), static_cast<std::streamsize>(text.size()));
	if (!file.good()) {
		outError = "Failed to write file: " + path.string();
		return false;
	}
	return true;
}

std::filesystem::path defaultOutputPath(const std::filesystem::path& inputPath) {
	const std::filesystem::path parent = inputPath.parent_path();
	const std::string stem = inputPath.stem().string();
	const std::string ext = inputPath.extension().string();
	return parent / (stem + "_output" + ext);
}

std::string buildOutputJson(const OutputModel& model, std::size_t patchedInstanceCount, std::size_t patchedFileCount) {
	std::string out{};
	out.reserve(8192u);

	auto appendIndent = [&](int depth) {
		out.append(static_cast<std::size_t>(std::max(0, depth) * 2), ' ');
	};
	auto appendString = [&](std::string_view text) {
		out += "\"";
		out += jsonEscape(text);
		out += "\"";
	};

	out += "{\n";
	appendIndent(1); out += "\"schema\":"; appendString(model.schema); out += ",\n";
	appendIndent(1); out += "\"kind\":"; appendString(model.kind); out += ",\n";
	appendIndent(1); out += "\"stats\": {\n";
	appendIndent(2); out += "\"patchedInstanceTargets\":" + std::to_string(patchedInstanceCount) + ",\n";
	appendIndent(2); out += "\"patchedFiles\":" + std::to_string(patchedFileCount) + ",\n";
	appendIndent(2); out += "\"unresolvedCount\":" + std::to_string(model.unresolved.size()) + "\n";
	appendIndent(1); out += "},\n";

	appendIndent(1); out += "\"definitions\": [\n";
	for (std::size_t i = 0; i < model.definitions.size(); ++i) {
		const ParsedDefinitionTarget& def = model.definitions[i];
		appendIndent(2); out += "{\n";
		appendIndent(3); out += "\"definitionId\":" + std::to_string(def.definitionId) + ",\n";
		appendIndent(3); out += "\"definitionName\":"; appendString(def.definitionName); out += ",\n";
		appendIndent(3); out += "\"changes\": [\n";
		for (std::size_t j = 0; j < def.changes.size(); ++j) {
			const ParsedChange& ch = def.changes[j];
			appendIndent(4); out += "{";
			out += "\"fieldHash\":" + std::to_string(ch.fieldHash) + ",";
			out += "\"fieldName\":"; appendString(ch.fieldName); out += ",";
			out += "\"fieldTypeHash\":" + std::to_string(ch.fieldTypeHash) + ",";
			out += "\"valueKind\":"; appendString(ch.jsonKind); out += ",";
			out += "\"value\":"; appendString(ch.valueLexeme);
			out += "}";
			out += (j + 1u < def.changes.size()) ? ",\n" : "\n";
		}
		appendIndent(3); out += "]\n";
		appendIndent(2); out += "}";
		out += (i + 1u < model.definitions.size()) ? ",\n" : "\n";
	}
	appendIndent(1); out += "],\n";

	appendIndent(1); out += "\"instances\": [\n";
	for (std::size_t i = 0; i < model.instances.size(); ++i) {
		const ParsedInstanceTarget& in = model.instances[i];
		appendIndent(2); out += "{\n";
		appendIndent(3); out += "\"definitionId\":" + std::to_string(in.definitionId) + ",\n";
		appendIndent(3); out += "\"definitionName\":"; appendString(in.definitionName); out += ",\n";
		appendIndent(3); out += "\"flowId\":" + std::to_string(in.flowId) + ",\n";
		appendIndent(3); out += "\"elementId\":"; appendString(in.elementId); out += ",\n";
		appendIndent(3); out += "\"sourceFile\":"; appendString(in.sourceFile); out += ",\n";
		appendIndent(3); out += "\"sourceLine\":" + std::to_string(in.sourceLine) + ",\n";
		appendIndent(3); out += "\"changes\": [\n";
		for (std::size_t j = 0; j < in.changes.size(); ++j) {
			const ParsedChange& ch = in.changes[j];
			appendIndent(4); out += "{";
			out += "\"fieldHash\":" + std::to_string(ch.fieldHash) + ",";
			out += "\"fieldName\":"; appendString(ch.fieldName); out += ",";
			out += "\"fieldTypeHash\":" + std::to_string(ch.fieldTypeHash) + ",";
			out += "\"valueKind\":"; appendString(ch.jsonKind); out += ",";
			out += "\"value\":"; appendString(ch.valueLexeme);
			out += "}";
			out += (j + 1u < in.changes.size()) ? ",\n" : "\n";
		}
		appendIndent(3); out += "]\n";
		appendIndent(2); out += "}";
		out += (i + 1u < model.instances.size()) ? ",\n" : "\n";
	}
	appendIndent(1); out += "],\n";

	appendIndent(1); out += "\"unresolved\": [\n";
	for (std::size_t i = 0; i < model.unresolved.size(); ++i) {
		const UnresolvedEntry& unr = model.unresolved[i];
		appendIndent(2); out += "{\n";
		appendIndent(3); out += "\"scope\":"; appendString(unr.scope); out += ",\n";
		appendIndent(3); out += "\"definitionId\":" + std::to_string(unr.definitionId) + ",\n";
		appendIndent(3); out += "\"flowId\":" + std::to_string(unr.flowId) + ",\n";
		appendIndent(3); out += "\"elementId\":"; appendString(unr.elementId); out += ",\n";
		appendIndent(3); out += "\"sourceFile\":"; appendString(unr.sourceFile); out += ",\n";
		appendIndent(3); out += "\"sourceLine\":" + std::to_string(unr.sourceLine) + ",\n";
		appendIndent(3); out += "\"fieldHash\":" + std::to_string(unr.fieldHash) + ",\n";
		appendIndent(3); out += "\"fieldName\":"; appendString(unr.fieldName); out += ",\n";
		appendIndent(3); out += "\"reason\":"; appendString(unr.reason); out += "\n";
		appendIndent(2); out += "}";
		out += (i + 1u < model.unresolved.size()) ? ",\n" : "\n";
	}
	appendIndent(1); out += "]\n";
	out += "}\n";
	return out;
}

} // namespace

int main(int argc, char** argv) {
	if (argc < 2 || argc > 3) {
		std::cerr << "Usage: flowui_devChange_updater <input.json> [output.json]\n";
		return 1;
	}

	const std::filesystem::path inputPath = argv[1];
	const std::filesystem::path outputPath = (argc >= 3) ? std::filesystem::path(argv[2]) : defaultOutputPath(inputPath);

	std::string inputText{};
	std::string ioError{};
	if (!readFileText(inputPath, inputText, ioError)) {
		std::cerr << ioError << "\n";
		return 1;
	}

	JsonParser parser(std::move(inputText));
	JsonValue root{};
	std::string parseError{};
	if (!parser.parse(root, parseError)) {
		std::cerr << "Failed to parse JSON: " << parseError << "\n";
		return 1;
	}

	OutputModel model{};
	std::string modelError{};
	if (!parseInputModel(root, model, modelError)) {
		std::cerr << "Invalid export JSON schema: " << modelError << "\n";
		return 1;
	}

	// V1: definitions are unresolved by design.
	for (const ParsedDefinitionTarget& definition : model.definitions) {
		for (const ParsedChange& change : definition.changes) {
			model.unresolved.push_back(UnresolvedEntry{
				.scope = "definition",
				.definitionId = definition.definitionId,
				.flowId = 0u,
				.elementId = {},
				.sourceFile = {},
				.sourceLine = 0u,
				.fieldHash = change.fieldHash,
				.fieldName = change.fieldName,
				.reason = "V1 does not support Definition changes.",
			});
		}
	}

	std::unordered_map<std::string, std::vector<std::size_t>> targetIndexesByFile{};
	for (std::size_t i = 0; i < model.instances.size(); ++i) {
		targetIndexesByFile[model.instances[i].sourceFile].push_back(i);
	}

	std::size_t patchedInstanceTargets = 0u;
	std::size_t patchedFiles = 0u;

	for (auto& [sourceFile, indexes] : targetIndexesByFile) {
		if (!std::filesystem::path(sourceFile).is_absolute()) {
			for (std::size_t idx : indexes) {
				const ParsedInstanceTarget& target = model.instances[idx];
				for (const ParsedChange& change : target.changes) {
					model.unresolved.push_back(UnresolvedEntry{
						.scope = "instance",
						.definitionId = target.definitionId,
						.flowId = target.flowId,
						.elementId = target.elementId,
						.sourceFile = target.sourceFile,
						.sourceLine = target.sourceLine,
						.fieldHash = change.fieldHash,
						.fieldName = change.fieldName,
						.reason = "V1 cant resolve relative filepaths.",
					});
				}
			}
			continue;
		}

		const std::filesystem::path sourcePath = sourceFile;
		std::string content{};
		std::string readError{};
		if (!readFileText(sourcePath, content, readError)) {
			for (std::size_t idx : indexes) {
				const ParsedInstanceTarget& target = model.instances[idx];
				for (const ParsedChange& change : target.changes) {
					model.unresolved.push_back(UnresolvedEntry{
						.scope = "instance",
						.definitionId = target.definitionId,
						.flowId = target.flowId,
						.elementId = target.elementId,
						.sourceFile = target.sourceFile,
						.sourceLine = target.sourceLine,
						.fieldHash = change.fieldHash,
						.fieldName = change.fieldName,
						.reason = "Failed to open source file.",
					});
				}
			}
			continue;
		}

		std::sort(
			indexes.begin(),
			indexes.end(),
			[&model](std::size_t lhs, std::size_t rhs) {
				return model.instances[lhs].sourceLine > model.instances[rhs].sourceLine;
			});

		bool fileChanged = false;
		for (std::size_t idx : indexes) {
			const ParsedInstanceTarget& target = model.instances[idx];
			PatchResult patch = patchInstanceTarget(content, target);
			if (patch.patched) {
				++patchedInstanceTargets;
				fileChanged = true;
			}
			for (const UnresolvedEntry& unresolved : patch.unresolved) {
				model.unresolved.push_back(unresolved);
			}
		}

		if (fileChanged) {
			std::string writeError{};
			if (!writeFileText(sourcePath, content, writeError)) {
				for (std::size_t idx : indexes) {
					const ParsedInstanceTarget& target = model.instances[idx];
					for (const ParsedChange& change : target.changes) {
						model.unresolved.push_back(UnresolvedEntry{
							.scope = "instance",
							.definitionId = target.definitionId,
							.flowId = target.flowId,
							.elementId = target.elementId,
							.sourceFile = target.sourceFile,
							.sourceLine = target.sourceLine,
							.fieldHash = change.fieldHash,
							.fieldName = change.fieldName,
							.reason = "Patched in memory but failed to write source file.",
						});
					}
				}
			} else {
				++patchedFiles;
			}
		}
	}

	const std::string outputJson = buildOutputJson(model, patchedInstanceTargets, patchedFiles);
	std::string outputError{};
	if (!writeFileText(outputPath, outputJson, outputError)) {
		std::cerr << outputError << "\n";
		return 1;
	}

	std::cout
		<< "flowui_devChange_updater: patched instance targets=" << patchedInstanceTargets
		<< ", patched files=" << patchedFiles
		<< ", unresolved=" << model.unresolved.size()
		<< ", output='" << outputPath.string() << "'\n";
	return 0;
}
