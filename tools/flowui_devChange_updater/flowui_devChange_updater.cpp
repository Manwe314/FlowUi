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
#include <unordered_set>
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

bool jsonBool(const JsonValue& value, bool& out) {
	if (value.kind != JsonValue::Kind::Bool) {
		return false;
	}
	out = value.boolValue;
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

struct AggregateFieldInfo {
	std::string_view jsonName{};
	std::string_view cppName{};
};

struct AggregateTypeInfo {
	std::string_view typeName{};
	std::vector<AggregateFieldInfo> fields{};
};

std::string makeFloatLiteral(std::string_view numberLexeme) {
	std::string literal{numberLexeme};
	if (literal.find_first_of(".eE") == std::string::npos) {
		literal += ".0";
	}
	literal += "f";
	return literal;
}

bool parseJsonFloatLiteral(
	const JsonValue* numericValue,
	std::string_view valueKind,
	std::string_view componentName,
	std::string& outLiteral,
	std::string& outLexeme,
	std::string& outError) {
	if (!numericValue || numericValue->kind != JsonValue::Kind::Number) {
		outError = std::string(valueKind) + " change has missing/invalid component: " + std::string(componentName);
		return false;
	}

	double numeric = 0.0;
	try {
		std::size_t consumed = 0u;
		numeric = std::stod(numericValue->text, &consumed);
		if (consumed != numericValue->text.size()) {
			outError = std::string(valueKind) + " component has invalid numeric lexeme: " + std::string(componentName);
			return false;
		}
	} catch (...) {
		outError = std::string(valueKind) + " component is not parseable as number: " + std::string(componentName);
		return false;
	}

	if (numeric < -static_cast<double>(std::numeric_limits<float>::max()) ||
		numeric > static_cast<double>(std::numeric_limits<float>::max())) {
		outError = std::string(valueKind) + " component is out of float range: " + std::string(componentName);
		return false;
	}

	outLexeme = numericValue->text;
	outLiteral = makeFloatLiteral(numericValue->text);
	return true;
}

bool parseJsonUInt16Literal(
	const JsonValue* numericValue,
	std::string_view valueKind,
	std::string_view componentName,
	std::string& outLiteral,
	std::string& outLexeme,
	std::string& outError) {
	if (!numericValue || numericValue->kind != JsonValue::Kind::Number) {
		outError = std::string(valueKind) + " change has missing/invalid component: " + std::string(componentName);
		return false;
	}

	long long numeric = 0;
	try {
		std::size_t consumed = 0u;
		numeric = std::stoll(numericValue->text, &consumed, 10);
		if (consumed != numericValue->text.size()) {
			outError = std::string(valueKind) + " component has invalid integer lexeme: " + std::string(componentName);
			return false;
		}
	} catch (...) {
		outError = std::string(valueKind) + " component is not parseable as integer: " + std::string(componentName);
		return false;
	}

	if (numeric < 0 || numeric > static_cast<long long>(std::numeric_limits<uint16_t>::max())) {
		outError = std::string(valueKind) + " component is out of uint16_t range: " + std::string(componentName);
		return false;
	}

	outLiteral = std::to_string(numeric);
	outLexeme = outLiteral;
	return true;
}

std::string buildDesignatedAggregate(
	std::string_view typeName,
	const std::vector<AggregateFieldInfo>& fields,
	const std::vector<std::string>& literals) {
	std::string out{typeName};
	out += "{";
	for (std::size_t i = 0u; i < fields.size(); ++i) {
		if (i > 0u) {
			out += ", ";
		}
		out += ".";
		out += fields[i].cppName;
		out += " = ";
		out += literals[i];
	}
	out += "}";
	return out;
}

std::string buildAggregateLexeme(
	std::string_view typeName,
	const std::vector<AggregateFieldInfo>& fields,
	const std::vector<std::string>& lexemes) {
	std::string out{typeName};
	out += "{";
	for (std::size_t i = 0u; i < fields.size(); ++i) {
		if (i > 0u) {
			out += ", ";
		}
		out += fields[i].jsonName;
		out += "=";
		out += lexemes[i];
	}
	out += "}";
	return out;
}

bool parseTypedAggregateChange(
	const JsonValue& rawValue,
	std::string_view valueKind,
	const std::vector<AggregateTypeInfo>& typeInfos,
	bool parseFloatComponents,
	std::string& outCppValue,
	std::string& outValueLexeme,
	std::string& outError) {
	if (rawValue.kind != JsonValue::Kind::Object) {
		outError = std::string(valueKind) + " change has non-object value.";
		return false;
	}

	const JsonValue* typeValue = findObjectField(rawValue, "type");
	if (!typeValue || typeValue->kind != JsonValue::Kind::String) {
		outError = std::string(valueKind) + " change requires string type.";
		return false;
	}

	std::string typeName{};
	(void)jsonString(*typeValue, typeName);
	const AggregateTypeInfo* typeInfo = nullptr;
	for (const AggregateTypeInfo& candidate : typeInfos) {
		if (candidate.typeName == typeName) {
			typeInfo = &candidate;
			break;
		}
	}
	if (typeInfo == nullptr) {
		outError = std::string(valueKind) + " change has unsupported type: " + typeName;
		return false;
	}

	std::vector<std::string> literals{};
	std::vector<std::string> lexemes{};
	literals.reserve(typeInfo->fields.size());
	lexemes.reserve(typeInfo->fields.size());
	for (const AggregateFieldInfo& field : typeInfo->fields) {
		std::string literal{};
		std::string lexeme{};
		const JsonValue* component = findObjectField(rawValue, field.jsonName);
		const bool parsed = parseFloatComponents
			? parseJsonFloatLiteral(component, valueKind, field.jsonName, literal, lexeme, outError)
			: parseJsonUInt16Literal(component, valueKind, field.jsonName, literal, lexeme, outError);
		if (!parsed) {
			return false;
		}
		literals.push_back(std::move(literal));
		lexemes.push_back(std::move(lexeme));
	}

	outCppValue = buildDesignatedAggregate(typeInfo->typeName, typeInfo->fields, literals);
	outValueLexeme = buildAggregateLexeme(typeInfo->typeName, typeInfo->fields, lexemes);
	return true;
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
	uint64_t paramsStructTypeHash = 0u;
	std::string paramsStructName{};
	bool hasSourceMetadata = false;
	std::string sourceFile{};
	uint32_t sourceLine = 0u;
	uint32_t sourceColumn = 0u;
	std::string sourceFunction{};
	std::vector<ParsedChange> changes{};
};

struct ParsedInstanceTarget {
	uint64_t definitionId = 0u;
	std::string definitionName{};
	uint64_t flowId = 0u;
	std::string elementId{};
	std::string sourceFile{};
	uint32_t sourceLine = 0u;
	uint32_t sourceColumn = 0u;
	std::string sourceFunction{};
	uint64_t sourceLocationHash = 0u;
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
	if (outChange.jsonKind == "enum1") {
		if (rawValue->kind != JsonValue::Kind::Object) {
			outError = "enum1 change has non-object value.";
			return false;
		}

		const JsonValue* numericValue = findObjectField(*rawValue, "numeric");
		const JsonValue* nameValue = findObjectField(*rawValue, "name");
		if (!numericValue || !nameValue) {
			outError = "enum1 change is missing numeric/name.";
			return false;
		}

		uint64_t numeric = 0u;
		if (!jsonUInt64(*numericValue, numeric) || numeric > static_cast<uint64_t>(std::numeric_limits<uint8_t>::max())) {
			outError = "enum1 change has invalid numeric value.";
			return false;
		}

		if (nameValue->kind != JsonValue::Kind::String) {
			outError = "enum1 change requires symbolic enum name for patching.";
			return false;
		}

		std::string enumName{};
		if (!jsonString(*nameValue, enumName) || enumName.empty()) {
			outError = "enum1 change has invalid enum name.";
			return false;
		}

		outChange.cppValue = enumName;
		outChange.valueLexeme = enumName + " (" + std::to_string(numeric) + ")";
		return true;
	}
	if (outChange.jsonKind == "enum2") {
		if (rawValue->kind != JsonValue::Kind::Object) {
			outError = "enum2 change has non-object value.";
			return false;
		}

		const JsonValue* typeValue = findObjectField(*rawValue, "type");
		if (!typeValue || typeValue->kind != JsonValue::Kind::String) {
			outError = "enum2 change requires string type.";
			return false;
		}

		auto parseEnum2Component = [&](
			const JsonValue* objectValue,
			const char* objectName,
			std::string& outName,
			uint8_t& outNumeric) -> bool {
			if (!objectValue || objectValue->kind != JsonValue::Kind::Object) {
				outError = std::string("enum2 change has missing/invalid component: ") + objectName;
				return false;
			}

			const JsonValue* numericValue = findObjectField(*objectValue, "numeric");
			const JsonValue* nameValue = findObjectField(*objectValue, "name");
			if (!numericValue || !nameValue) {
				outError = std::string("enum2 component is missing numeric/name: ") + objectName;
				return false;
			}

			uint64_t numeric = 0u;
			if (!jsonUInt64(*numericValue, numeric) || numeric > static_cast<uint64_t>(std::numeric_limits<uint8_t>::max())) {
				outError = std::string("enum2 component has invalid numeric value: ") + objectName;
				return false;
			}

			if (nameValue->kind != JsonValue::Kind::String) {
				outError = std::string("enum2 component requires symbolic name: ") + objectName;
				return false;
			}

			std::string parsedName{};
			if (!jsonString(*nameValue, parsedName) || parsedName.empty()) {
				outError = std::string("enum2 component has invalid enum name: ") + objectName;
				return false;
			}

			outName = std::move(parsedName);
			outNumeric = static_cast<uint8_t>(numeric);
			return true;
		};

		std::string enum2Type{};
		(void)jsonString(*typeValue, enum2Type);
		if (enum2Type == "Clay_ChildAlignment") {
			std::string xName{};
			std::string yName{};
			uint8_t xNumeric = 0u;
			uint8_t yNumeric = 0u;
			if (
				!parseEnum2Component(findObjectField(*rawValue, "x"), "x", xName, xNumeric) ||
				!parseEnum2Component(findObjectField(*rawValue, "y"), "y", yName, yNumeric))
			{
				return false;
			}

			outChange.cppValue =
				"Clay_ChildAlignment{.x = " + xName + ", .y = " + yName + "}";
			outChange.valueLexeme =
				"Clay_ChildAlignment{x=" + xName + " (" + std::to_string(xNumeric) +
				"), y=" + yName + " (" + std::to_string(yNumeric) + ")}";
			return true;
		}
		if (enum2Type == "Clay_FloatingAttachPoints") {
			std::string elementName{};
			std::string parentName{};
			uint8_t elementNumeric = 0u;
			uint8_t parentNumeric = 0u;
			if (
				!parseEnum2Component(
					findObjectField(*rawValue, "element"),
					"element",
					elementName,
					elementNumeric) ||
				!parseEnum2Component(
					findObjectField(*rawValue, "parent"),
					"parent",
					parentName,
					parentNumeric))
			{
				return false;
			}

			outChange.cppValue =
				"Clay_FloatingAttachPoints{.element = " + elementName + ", .parent = " + parentName + "}";
			outChange.valueLexeme =
				"Clay_FloatingAttachPoints{element=" + elementName + " (" + std::to_string(elementNumeric) +
				"), parent=" + parentName + " (" + std::to_string(parentNumeric) + ")}";
			return true;
		}

		outError = "enum2 change has unsupported type: " + enum2Type;
		return false;
	}
	if (outChange.jsonKind == "float2") {
		static const std::vector<AggregateTypeInfo> kFloat2TypeInfos{
			{"Clay_Vector2", {{"x", "x"}, {"y", "y"}}},
			{"Clay_Dimensions", {{"width", "width"}, {"height", "height"}}},
			{"Clay_SizingMinMax", {{"min", "min"}, {"max", "max"}}},
		};
		if (!parseTypedAggregateChange(
			*rawValue,
			"float2",
			kFloat2TypeInfos,
			true,
			outChange.cppValue,
			outChange.valueLexeme,
			outError))
		{
			return false;
		}
		return true;
	}
	if (outChange.jsonKind == "float4") {
		static const std::vector<AggregateTypeInfo> kFloat4TypeInfos{
			{"Clay_Color", {{"r", "r"}, {"g", "g"}, {"b", "b"}, {"a", "a"}}},
			{"Clay_CornerRadius", {{"topLeft", "topLeft"}, {"topRight", "topRight"}, {"bottomLeft", "bottomLeft"}, {"bottomRight", "bottomRight"}}},
		};
		if (!parseTypedAggregateChange(
			*rawValue,
			"float4",
			kFloat4TypeInfos,
			true,
			outChange.cppValue,
			outChange.valueLexeme,
			outError))
		{
			return false;
		}
		return true;
	}
	if (outChange.jsonKind == "tagged_union") {
		if (rawValue->kind != JsonValue::Kind::Object) {
			outError = "tagged_union change has non-object value.";
			return false;
		}

		const JsonValue* typeValue = findObjectField(*rawValue, "type");
		if (!typeValue || typeValue->kind != JsonValue::Kind::String) {
			outError = "tagged_union change requires string type.";
			return false;
		}

		auto parseTaggedUnionFloat = [&](
			const JsonValue* numericValue,
			const char* componentName,
			std::string& outLiteral,
			std::string& outLexeme) -> bool {
			if (!numericValue || numericValue->kind != JsonValue::Kind::Number) {
				outError = std::string("tagged_union change has missing/invalid component: ") + componentName;
				return false;
			}

			double numeric = 0.0;
			try {
				std::size_t consumed = 0u;
				numeric = std::stod(numericValue->text, &consumed);
				if (consumed != numericValue->text.size()) {
					outError = std::string("tagged_union component has invalid numeric lexeme: ") + componentName;
					return false;
				}
			} catch (...) {
				outError = std::string("tagged_union component is not parseable as number: ") + componentName;
				return false;
			}

			if (numeric < -static_cast<double>(std::numeric_limits<float>::max()) ||
				numeric > static_cast<double>(std::numeric_limits<float>::max())) {
				outError = std::string("tagged_union component is out of float range: ") + componentName;
				return false;
			}

			outLexeme = numericValue->text;
			outLiteral = numericValue->text;
			if (outLiteral.find_first_of(".eE") == std::string::npos) {
				outLiteral += ".0";
			}
			outLiteral += "f";
			return true;
		};

		auto parseTag = [&](
			const JsonValue* tagValue,
			std::string& outTagName,
			uint8_t& outTagNumeric) -> bool {
			if (!tagValue || tagValue->kind != JsonValue::Kind::Object) {
				outError = "tagged_union change has missing/invalid tag.";
				return false;
			}

			const JsonValue* numericValue = findObjectField(*tagValue, "numeric");
			const JsonValue* nameValue = findObjectField(*tagValue, "name");
			if (!numericValue || !nameValue) {
				outError = "tagged_union tag is missing numeric/name.";
				return false;
			}

			uint64_t numeric = 0u;
			if (!jsonUInt64(*numericValue, numeric) || numeric > static_cast<uint64_t>(std::numeric_limits<uint8_t>::max())) {
				outError = "tagged_union tag has invalid numeric value.";
				return false;
			}

			if (nameValue->kind != JsonValue::Kind::String) {
				outError = "tagged_union tag requires symbolic name for patching.";
				return false;
			}

			std::string parsedName{};
			if (!jsonString(*nameValue, parsedName) || parsedName.empty()) {
				outError = "tagged_union tag has invalid enum name.";
				return false;
			}

			outTagName = std::move(parsedName);
			outTagNumeric = static_cast<uint8_t>(numeric);
			return true;
		};

		std::string taggedType{};
		(void)jsonString(*typeValue, taggedType);
		if (taggedType == "Clay_SizingAxis") {
			std::string tagName{};
			uint8_t tagNumeric = 0u;
			if (!parseTag(findObjectField(*rawValue, "tag"), tagName, tagNumeric)) {
				return false;
			}

			if (tagName == "CLAY__SIZING_TYPE_PERCENT") {
				std::string percentLiteral{};
				std::string percentLexeme{};
				if (!parseTaggedUnionFloat(
					findObjectField(*rawValue, "percent"),
					"percent",
					percentLiteral,
					percentLexeme))
				{
					return false;
				}

				outChange.cppValue =
					"Clay_SizingAxis{.size = {.percent = " + percentLiteral + "}, .type = " + tagName + "}";
				outChange.valueLexeme =
					"Clay_SizingAxis{type=" + tagName + " (" + std::to_string(tagNumeric) +
					"), percent=" + percentLexeme + "}";
				return true;
			}

			if (
				tagName == "CLAY__SIZING_TYPE_FIT" ||
				tagName == "CLAY__SIZING_TYPE_GROW" ||
				tagName == "CLAY__SIZING_TYPE_FIXED")
			{
				const JsonValue* minMaxValue = findObjectField(*rawValue, "minMax");
				if (!minMaxValue || minMaxValue->kind != JsonValue::Kind::Object) {
					outError = "tagged_union sizing axis requires minMax object for non-percent tags.";
					return false;
				}

				std::string minLiteral{};
				std::string maxLiteral{};
				std::string minLexeme{};
				std::string maxLexeme{};
				if (
					!parseTaggedUnionFloat(
						findObjectField(*minMaxValue, "min"),
						"min",
						minLiteral,
						minLexeme) ||
					!parseTaggedUnionFloat(
						findObjectField(*minMaxValue, "max"),
						"max",
						maxLiteral,
						maxLexeme))
				{
					return false;
				}

				outChange.cppValue =
					"Clay_SizingAxis{.size = {.minMax = Clay_SizingMinMax{.min = " + minLiteral +
					", .max = " + maxLiteral + "}}, .type = " + tagName + "}";
				outChange.valueLexeme =
					"Clay_SizingAxis{type=" + tagName + " (" + std::to_string(tagNumeric) +
					"), minMax={min=" + minLexeme + ", max=" + maxLexeme + "}}";
				return true;
			}

			outError = "tagged_union sizing axis has unsupported tag name: " + tagName;
			return false;
		}

		outError = "tagged_union change has unsupported type: " + taggedType;
		return false;
	}
	if (outChange.jsonKind == "composite_struct") {
		if (rawValue->kind != JsonValue::Kind::Object) {
			outError = "composite_struct change has non-object value.";
			return false;
		}

		const JsonValue* typeValue = findObjectField(*rawValue, "type");
		if (!typeValue || typeValue->kind != JsonValue::Kind::String) {
			outError = "composite_struct change requires string type.";
			return false;
		}

		auto parseFloatComponent = [&](
			const JsonValue* numericValue,
			const char* componentName,
			std::string& outLiteral,
			std::string& outLexeme) -> bool {
			if (!numericValue || numericValue->kind != JsonValue::Kind::Number) {
				outError = std::string("composite_struct has missing/invalid float component: ") + componentName;
				return false;
			}

			double numeric = 0.0;
			try {
				std::size_t consumed = 0u;
				numeric = std::stod(numericValue->text, &consumed);
				if (consumed != numericValue->text.size()) {
					outError = std::string("composite_struct float has invalid numeric lexeme: ") + componentName;
					return false;
				}
			} catch (...) {
				outError = std::string("composite_struct float is not parseable: ") + componentName;
				return false;
			}

			if (numeric < -static_cast<double>(std::numeric_limits<float>::max()) ||
				numeric > static_cast<double>(std::numeric_limits<float>::max())) {
				outError = std::string("composite_struct float is out of range: ") + componentName;
				return false;
			}

			outLexeme = numericValue->text;
			outLiteral = numericValue->text;
			if (outLiteral.find_first_of(".eE") == std::string::npos) {
				outLiteral += ".0";
			}
			outLiteral += "f";
			return true;
		};

		auto parseUnsignedInteger = [&](
			const JsonValue* numericValue,
			const char* componentName,
			uint64_t maxValue,
			std::string& outLexeme) -> bool {
			if (!numericValue || numericValue->kind != JsonValue::Kind::Number) {
				outError = std::string("composite_struct has missing/invalid integer component: ") + componentName;
				return false;
			}

			unsigned long long numeric = 0;
			try {
				std::size_t consumed = 0u;
				numeric = std::stoull(numericValue->text, &consumed, 10);
				if (consumed != numericValue->text.size()) {
					outError = std::string("composite_struct integer has invalid lexeme: ") + componentName;
					return false;
				}
			} catch (...) {
				outError = std::string("composite_struct integer is not parseable: ") + componentName;
				return false;
			}

			if (numeric > maxValue) {
				outError = std::string("composite_struct integer is out of range: ") + componentName;
				return false;
			}

			outLexeme = std::to_string(numeric);
			return true;
		};

		auto parseSignedInteger = [&](
			const JsonValue* numericValue,
			const char* componentName,
			long long minValue,
			long long maxValue,
			std::string& outLexeme) -> bool {
			if (!numericValue || numericValue->kind != JsonValue::Kind::Number) {
				outError = std::string("composite_struct has missing/invalid signed integer component: ") + componentName;
				return false;
			}

			long long numeric = 0;
			try {
				std::size_t consumed = 0u;
				numeric = std::stoll(numericValue->text, &consumed, 10);
				if (consumed != numericValue->text.size()) {
					outError = std::string("composite_struct signed integer has invalid lexeme: ") + componentName;
					return false;
				}
			} catch (...) {
				outError = std::string("composite_struct signed integer is not parseable: ") + componentName;
				return false;
			}

			if (numeric < minValue || numeric > maxValue) {
				outError = std::string("composite_struct signed integer is out of range: ") + componentName;
				return false;
			}

			outLexeme = std::to_string(numeric);
			return true;
		};

		auto parseEnumPayload = [&](
			const JsonValue* enumValue,
			const char* componentName,
			std::string& outName,
			uint8_t& outNumeric) -> bool {
			if (!enumValue || enumValue->kind != JsonValue::Kind::Object) {
				outError = std::string("composite_struct enum has missing/invalid component: ") + componentName;
				return false;
			}

			const JsonValue* numericValue = findObjectField(*enumValue, "numeric");
			const JsonValue* nameValue = findObjectField(*enumValue, "name");
			if (!numericValue || !nameValue) {
				outError = std::string("composite_struct enum payload missing numeric/name: ") + componentName;
				return false;
			}

			uint64_t numeric = 0u;
			if (!jsonUInt64(*numericValue, numeric) || numeric > static_cast<uint64_t>(std::numeric_limits<uint8_t>::max())) {
				outError = std::string("composite_struct enum numeric out of range: ") + componentName;
				return false;
			}

			if (nameValue->kind != JsonValue::Kind::String) {
				outError = std::string("composite_struct enum requires symbolic name: ") + componentName;
				return false;
			}

			std::string parsedName{};
			if (!jsonString(*nameValue, parsedName) || parsedName.empty()) {
				outError = std::string("composite_struct enum has invalid name: ") + componentName;
				return false;
			}

			outName = std::move(parsedName);
			outNumeric = static_cast<uint8_t>(numeric);
			return true;
		};

		auto pointerExprFromLexeme = [](const std::string& lexeme) -> std::string {
			if (lexeme == "0") {
				return "nullptr";
			}
			return "reinterpret_cast<void*>(static_cast<uintptr_t>(" + lexeme + "ull))";
		};

		auto parseColorObject = [&](
			const JsonValue* colorValue,
			const char* componentName,
			std::string& outCpp,
			std::string& outLexeme) -> bool {
			if (!colorValue || colorValue->kind != JsonValue::Kind::Object) {
				outError = std::string("composite_struct color has missing/invalid object: ") + componentName;
				return false;
			}

			std::string rLiteral{}, gLiteral{}, bLiteral{}, aLiteral{};
			std::string rLexeme{}, gLexeme{}, bLexeme{}, aLexeme{};
			if (
				!parseFloatComponent(findObjectField(*colorValue, "r"), "r", rLiteral, rLexeme) ||
				!parseFloatComponent(findObjectField(*colorValue, "g"), "g", gLiteral, gLexeme) ||
				!parseFloatComponent(findObjectField(*colorValue, "b"), "b", bLiteral, bLexeme) ||
				!parseFloatComponent(findObjectField(*colorValue, "a"), "a", aLiteral, aLexeme))
			{
				return false;
			}

			outCpp = "Clay_Color{.r = " + rLiteral + ", .g = " + gLiteral + ", .b = " + bLiteral + ", .a = " + aLiteral + "}";
			outLexeme = "Clay_Color{r=" + rLexeme + ", g=" + gLexeme + ", b=" + bLexeme + ", a=" + aLexeme + "}";
			return true;
		};

		auto parseCornerRadiusObject = [&](
			const JsonValue* radiusValue,
			const char* componentName,
			std::string& outCpp,
			std::string& outLexeme) -> bool {
			if (!radiusValue || radiusValue->kind != JsonValue::Kind::Object) {
				outError = std::string("composite_struct corner radius has missing/invalid object: ") + componentName;
				return false;
			}

			std::string topLeftLiteral{}, topRightLiteral{}, bottomLeftLiteral{}, bottomRightLiteral{};
			std::string topLeftLexeme{}, topRightLexeme{}, bottomLeftLexeme{}, bottomRightLexeme{};
			if (
				!parseFloatComponent(findObjectField(*radiusValue, "topLeft"), "topLeft", topLeftLiteral, topLeftLexeme) ||
				!parseFloatComponent(findObjectField(*radiusValue, "topRight"), "topRight", topRightLiteral, topRightLexeme) ||
				!parseFloatComponent(findObjectField(*radiusValue, "bottomLeft"), "bottomLeft", bottomLeftLiteral, bottomLeftLexeme) ||
				!parseFloatComponent(findObjectField(*radiusValue, "bottomRight"), "bottomRight", bottomRightLiteral, bottomRightLexeme))
			{
				return false;
			}

			outCpp =
				"Clay_CornerRadius{.topLeft = " + topLeftLiteral + ", .topRight = " + topRightLiteral +
				", .bottomLeft = " + bottomLeftLiteral + ", .bottomRight = " + bottomRightLiteral + "}";
			outLexeme =
				"Clay_CornerRadius{topLeft=" + topLeftLexeme + ", topRight=" + topRightLexeme +
				", bottomLeft=" + bottomLeftLexeme + ", bottomRight=" + bottomRightLexeme + "}";
			return true;
		};

		auto parseVector2Object = [&](
			const JsonValue* vecValue,
			const char* componentName,
			std::string& outCpp,
			std::string& outLexeme) -> bool {
			if (!vecValue || vecValue->kind != JsonValue::Kind::Object) {
				outError = std::string("composite_struct vector2 has missing/invalid object: ") + componentName;
				return false;
			}

			std::string xLiteral{}, yLiteral{};
			std::string xLexeme{}, yLexeme{};
			if (
				!parseFloatComponent(findObjectField(*vecValue, "x"), "x", xLiteral, xLexeme) ||
				!parseFloatComponent(findObjectField(*vecValue, "y"), "y", yLiteral, yLexeme))
			{
				return false;
			}

			outCpp = "Clay_Vector2{.x = " + xLiteral + ", .y = " + yLiteral + "}";
			outLexeme = "Clay_Vector2{x=" + xLexeme + ", y=" + yLexeme + "}";
			return true;
		};

		auto parseDimensionsObject = [&](
			const JsonValue* dimValue,
			const char* componentName,
			std::string& outCpp,
			std::string& outLexeme) -> bool {
			if (!dimValue || dimValue->kind != JsonValue::Kind::Object) {
				outError = std::string("composite_struct dimensions has missing/invalid object: ") + componentName;
				return false;
			}

			std::string widthLiteral{}, heightLiteral{};
			std::string widthLexeme{}, heightLexeme{};
			if (
				!parseFloatComponent(findObjectField(*dimValue, "width"), "width", widthLiteral, widthLexeme) ||
				!parseFloatComponent(findObjectField(*dimValue, "height"), "height", heightLiteral, heightLexeme))
			{
				return false;
			}

			outCpp = "Clay_Dimensions{.width = " + widthLiteral + ", .height = " + heightLiteral + "}";
			outLexeme = "Clay_Dimensions{width=" + widthLexeme + ", height=" + heightLexeme + "}";
			return true;
		};

		auto parsePaddingObject = [&](
			const JsonValue* paddingValue,
			const char* componentName,
			std::string& outCpp,
			std::string& outLexeme) -> bool {
			if (!paddingValue || paddingValue->kind != JsonValue::Kind::Object) {
				outError = std::string("composite_struct padding has missing/invalid object: ") + componentName;
				return false;
			}

			std::string leftLexeme{}, rightLexeme{}, topLexeme{}, bottomLexeme{};
			if (
				!parseUnsignedInteger(findObjectField(*paddingValue, "left"), "left", static_cast<uint64_t>(std::numeric_limits<uint16_t>::max()), leftLexeme) ||
				!parseUnsignedInteger(findObjectField(*paddingValue, "right"), "right", static_cast<uint64_t>(std::numeric_limits<uint16_t>::max()), rightLexeme) ||
				!parseUnsignedInteger(findObjectField(*paddingValue, "top"), "top", static_cast<uint64_t>(std::numeric_limits<uint16_t>::max()), topLexeme) ||
				!parseUnsignedInteger(findObjectField(*paddingValue, "bottom"), "bottom", static_cast<uint64_t>(std::numeric_limits<uint16_t>::max()), bottomLexeme))
			{
				return false;
			}

			outCpp =
				"Clay_Padding{.left = " + leftLexeme + ", .right = " + rightLexeme +
				", .top = " + topLexeme + ", .bottom = " + bottomLexeme + "}";
			outLexeme =
				"Clay_Padding{left=" + leftLexeme + ", right=" + rightLexeme +
				", top=" + topLexeme + ", bottom=" + bottomLexeme + "}";
			return true;
		};

		auto parseBorderWidthObject = [&](
			const JsonValue* widthValue,
			const char* componentName,
			std::string& outCpp,
			std::string& outLexeme) -> bool {
			if (!widthValue || widthValue->kind != JsonValue::Kind::Object) {
				outError = std::string("composite_struct border width has missing/invalid object: ") + componentName;
				return false;
			}

			std::string leftLexeme{}, rightLexeme{}, topLexeme{}, bottomLexeme{}, betweenLexeme{};
			if (
				!parseUnsignedInteger(findObjectField(*widthValue, "left"), "left", static_cast<uint64_t>(std::numeric_limits<uint16_t>::max()), leftLexeme) ||
				!parseUnsignedInteger(findObjectField(*widthValue, "right"), "right", static_cast<uint64_t>(std::numeric_limits<uint16_t>::max()), rightLexeme) ||
				!parseUnsignedInteger(findObjectField(*widthValue, "top"), "top", static_cast<uint64_t>(std::numeric_limits<uint16_t>::max()), topLexeme) ||
				!parseUnsignedInteger(findObjectField(*widthValue, "bottom"), "bottom", static_cast<uint64_t>(std::numeric_limits<uint16_t>::max()), bottomLexeme) ||
				!parseUnsignedInteger(findObjectField(*widthValue, "betweenChildren"), "betweenChildren", static_cast<uint64_t>(std::numeric_limits<uint16_t>::max()), betweenLexeme))
			{
				return false;
			}

			outCpp =
				"Clay_BorderWidth{.left = " + leftLexeme + ", .right = " + rightLexeme +
				", .top = " + topLexeme + ", .bottom = " + bottomLexeme +
				", .betweenChildren = " + betweenLexeme + "}";
			outLexeme =
				"Clay_BorderWidth{left=" + leftLexeme + ", right=" + rightLexeme +
				", top=" + topLexeme + ", bottom=" + bottomLexeme +
				", betweenChildren=" + betweenLexeme + "}";
			return true;
		};

		auto parseSizingAxisObject = [&](
			const JsonValue* axisValue,
			const char* componentName,
			std::string& outCpp,
			std::string& outLexeme) -> bool {
			if (!axisValue || axisValue->kind != JsonValue::Kind::Object) {
				outError = std::string("composite_struct sizing axis has missing/invalid object: ") + componentName;
				return false;
			}

			std::string tagName{};
			uint8_t tagNumeric = 0u;
			if (!parseEnumPayload(findObjectField(*axisValue, "tag"), "tag", tagName, tagNumeric)) {
				return false;
			}

			if (tagName == "CLAY__SIZING_TYPE_PERCENT") {
				std::string percentLiteral{};
				std::string percentLexeme{};
				if (!parseFloatComponent(findObjectField(*axisValue, "percent"), "percent", percentLiteral, percentLexeme)) {
					return false;
				}

				outCpp = "Clay_SizingAxis{.size = {.percent = " + percentLiteral + "}, .type = " + tagName + "}";
				outLexeme =
					"Clay_SizingAxis{type=" + tagName + " (" + std::to_string(tagNumeric) +
					"), percent=" + percentLexeme + "}";
				return true;
			}

			if (
				tagName == "CLAY__SIZING_TYPE_FIT" ||
				tagName == "CLAY__SIZING_TYPE_GROW" ||
				tagName == "CLAY__SIZING_TYPE_FIXED")
			{
				const JsonValue* minMaxValue = findObjectField(*axisValue, "minMax");
				if (!minMaxValue || minMaxValue->kind != JsonValue::Kind::Object) {
					outError = "composite_struct sizing axis requires minMax object for non-percent tags.";
					return false;
				}

				std::string minLiteral{}, maxLiteral{};
				std::string minLexeme{}, maxLexeme{};
				if (
					!parseFloatComponent(findObjectField(*minMaxValue, "min"), "min", minLiteral, minLexeme) ||
					!parseFloatComponent(findObjectField(*minMaxValue, "max"), "max", maxLiteral, maxLexeme))
				{
					return false;
				}

				outCpp =
					"Clay_SizingAxis{.size = {.minMax = Clay_SizingMinMax{.min = " + minLiteral +
					", .max = " + maxLiteral + "}}, .type = " + tagName + "}";
				outLexeme =
					"Clay_SizingAxis{type=" + tagName + " (" + std::to_string(tagNumeric) +
					"), minMax={min=" + minLexeme + ", max=" + maxLexeme + "}}";
				return true;
			}

			outError = "composite_struct sizing axis has unsupported tag name: " + tagName;
			return false;
		};

		auto parseSizingObject = [&](
			const JsonValue* sizingValue,
			const char* componentName,
			std::string& outCpp,
			std::string& outLexeme) -> bool {
			if (!sizingValue || sizingValue->kind != JsonValue::Kind::Object) {
				outError = std::string("composite_struct sizing has missing/invalid object: ") + componentName;
				return false;
			}

			std::string widthCpp{}, heightCpp{};
			std::string widthLexeme{}, heightLexeme{};
			if (
				!parseSizingAxisObject(findObjectField(*sizingValue, "width"), "width", widthCpp, widthLexeme) ||
				!parseSizingAxisObject(findObjectField(*sizingValue, "height"), "height", heightCpp, heightLexeme))
			{
				return false;
			}

			outCpp = "Clay_Sizing{.width = " + widthCpp + ", .height = " + heightCpp + "}";
			outLexeme = "Clay_Sizing{width=" + widthLexeme + ", height=" + heightLexeme + "}";
			return true;
		};

		auto parseLayoutConfigObject = [&](
			const JsonValue* layoutValue,
			const char* componentName,
			std::string& outCpp,
			std::string& outLexeme) -> bool {
			if (!layoutValue || layoutValue->kind != JsonValue::Kind::Object) {
				outError = std::string("composite_struct layout config has missing/invalid object: ") + componentName;
				return false;
			}

			std::string sizingCpp{}, sizingLexeme{};
			if (!parseSizingObject(findObjectField(*layoutValue, "sizing"), "sizing", sizingCpp, sizingLexeme)) {
				return false;
			}

			std::string paddingCpp{}, paddingLexeme{};
			if (!parsePaddingObject(findObjectField(*layoutValue, "padding"), "padding", paddingCpp, paddingLexeme)) {
				return false;
			}

			std::string childGapLexeme{};
			if (!parseUnsignedInteger(
				findObjectField(*layoutValue, "childGap"),
				"childGap",
				static_cast<uint64_t>(std::numeric_limits<uint16_t>::max()),
				childGapLexeme))
			{
				return false;
			}

			const JsonValue* childAlignmentValue = findObjectField(*layoutValue, "childAlignment");
			if (!childAlignmentValue || childAlignmentValue->kind != JsonValue::Kind::Object) {
				outError = "composite_struct layout config requires childAlignment object.";
				return false;
			}
			std::string alignXName{}, alignYName{};
			uint8_t alignXNumeric = 0u;
			uint8_t alignYNumeric = 0u;
			if (
				!parseEnumPayload(findObjectField(*childAlignmentValue, "x"), "childAlignment.x", alignXName, alignXNumeric) ||
				!parseEnumPayload(findObjectField(*childAlignmentValue, "y"), "childAlignment.y", alignYName, alignYNumeric))
			{
				return false;
			}
			const std::string childAlignmentCpp =
				"Clay_ChildAlignment{.x = " + alignXName + ", .y = " + alignYName + "}";

			std::string layoutDirectionName{};
			uint8_t layoutDirectionNumeric = 0u;
			if (!parseEnumPayload(
				findObjectField(*layoutValue, "layoutDirection"),
				"layoutDirection",
				layoutDirectionName,
				layoutDirectionNumeric))
			{
				return false;
			}

			outCpp =
				"Clay_LayoutConfig{.sizing = " + sizingCpp +
				", .padding = " + paddingCpp +
				", .childGap = " + childGapLexeme +
				", .childAlignment = " + childAlignmentCpp +
				", .layoutDirection = " + layoutDirectionName + "}";
			outLexeme =
				"Clay_LayoutConfig{layoutDirection=" + layoutDirectionName +
				" (" + std::to_string(layoutDirectionNumeric) + "), childGap=" + childGapLexeme + "}";
			return true;
		};

		auto parseFloatingObject = [&](
			const JsonValue* floatingValue,
			const char* componentName,
			std::string& outCpp,
			std::string& outLexeme) -> bool {
			if (!floatingValue || floatingValue->kind != JsonValue::Kind::Object) {
				outError = std::string("composite_struct floating config has missing/invalid object: ") + componentName;
				return false;
			}

			std::string offsetCpp{}, offsetLexeme{};
			if (!parseVector2Object(findObjectField(*floatingValue, "offset"), "offset", offsetCpp, offsetLexeme)) {
				return false;
			}
			std::string expandCpp{}, expandLexeme{};
			if (!parseDimensionsObject(findObjectField(*floatingValue, "expand"), "expand", expandCpp, expandLexeme)) {
				return false;
			}

			std::string parentIdLexeme{};
			if (!parseUnsignedInteger(
				findObjectField(*floatingValue, "parentId"),
				"parentId",
				static_cast<uint64_t>(std::numeric_limits<uint32_t>::max()),
				parentIdLexeme))
			{
				return false;
			}

			std::string zIndexLexeme{};
			if (!parseSignedInteger(
				findObjectField(*floatingValue, "zIndex"),
				"zIndex",
				static_cast<long long>(std::numeric_limits<int16_t>::min()),
				static_cast<long long>(std::numeric_limits<int16_t>::max()),
				zIndexLexeme))
			{
				return false;
			}

			const JsonValue* attachPointsValue = findObjectField(*floatingValue, "attachPoints");
			if (!attachPointsValue || attachPointsValue->kind != JsonValue::Kind::Object) {
				outError = "composite_struct floating config requires attachPoints object.";
				return false;
			}
			std::string attachElementName{}, attachParentName{};
			uint8_t attachElementNumeric = 0u;
			uint8_t attachParentNumeric = 0u;
			if (
				!parseEnumPayload(findObjectField(*attachPointsValue, "element"), "attachPoints.element", attachElementName, attachElementNumeric) ||
				!parseEnumPayload(findObjectField(*attachPointsValue, "parent"), "attachPoints.parent", attachParentName, attachParentNumeric))
			{
				return false;
			}
			const std::string attachPointsCpp =
				"Clay_FloatingAttachPoints{.element = " + attachElementName + ", .parent = " + attachParentName + "}";

			std::string pointerCaptureModeName{};
			uint8_t pointerCaptureModeNumeric = 0u;
			if (!parseEnumPayload(
				findObjectField(*floatingValue, "pointerCaptureMode"),
				"pointerCaptureMode",
				pointerCaptureModeName,
				pointerCaptureModeNumeric))
			{
				return false;
			}

			std::string attachToName{};
			uint8_t attachToNumeric = 0u;
			if (!parseEnumPayload(
				findObjectField(*floatingValue, "attachTo"),
				"attachTo",
				attachToName,
				attachToNumeric))
			{
				return false;
			}

			std::string clipToName{};
			uint8_t clipToNumeric = 0u;
			if (!parseEnumPayload(
				findObjectField(*floatingValue, "clipTo"),
				"clipTo",
				clipToName,
				clipToNumeric))
			{
				return false;
			}

			outCpp =
				"Clay_FloatingElementConfig{.offset = " + offsetCpp +
				", .expand = " + expandCpp +
				", .parentId = " + parentIdLexeme +
				", .zIndex = " + zIndexLexeme +
				", .attachPoints = " + attachPointsCpp +
				", .pointerCaptureMode = " + pointerCaptureModeName +
				", .attachTo = " + attachToName +
				", .clipTo = " + clipToName + "}";
			outLexeme =
				"Clay_FloatingElementConfig{attachTo=" + attachToName +
				" (" + std::to_string(attachToNumeric) + "), clipTo=" + clipToName +
				" (" + std::to_string(clipToNumeric) + "), zIndex=" + zIndexLexeme + "}";
			return true;
		};

		auto parseClipObject = [&](
			const JsonValue* clipValue,
			const char* componentName,
			std::string& outCpp,
			std::string& outLexeme) -> bool {
			if (!clipValue || clipValue->kind != JsonValue::Kind::Object) {
				outError = std::string("composite_struct clip config has missing/invalid object: ") + componentName;
				return false;
			}

			const JsonValue* horizontalValue = findObjectField(*clipValue, "horizontal");
			const JsonValue* verticalValue = findObjectField(*clipValue, "vertical");
			if (!horizontalValue || horizontalValue->kind != JsonValue::Kind::Bool ||
				!verticalValue || verticalValue->kind != JsonValue::Kind::Bool)
			{
				outError = "composite_struct clip config requires boolean horizontal/vertical.";
				return false;
			}

			std::string childOffsetCpp{}, childOffsetLexeme{};
			if (!parseVector2Object(findObjectField(*clipValue, "childOffset"), "childOffset", childOffsetCpp, childOffsetLexeme)) {
				return false;
			}

			const std::string horizontalLiteral = horizontalValue->boolValue ? "true" : "false";
			const std::string verticalLiteral = verticalValue->boolValue ? "true" : "false";
			outCpp =
				"Clay_ClipElementConfig{.horizontal = " + horizontalLiteral +
				", .vertical = " + verticalLiteral +
				", .childOffset = " + childOffsetCpp + "}";
			outLexeme =
				"Clay_ClipElementConfig{horizontal=" + horizontalLiteral +
				", vertical=" + verticalLiteral + ", childOffset=" + childOffsetLexeme + "}";
			return true;
		};

		auto parseBorderObject = [&](
			const JsonValue* borderValue,
			const char* componentName,
			std::string& outCpp,
			std::string& outLexeme) -> bool {
			if (!borderValue || borderValue->kind != JsonValue::Kind::Object) {
				outError = std::string("composite_struct border config has missing/invalid object: ") + componentName;
				return false;
			}

			std::string colorCpp{}, colorLexeme{};
			if (!parseColorObject(findObjectField(*borderValue, "color"), "color", colorCpp, colorLexeme)) {
				return false;
			}

			std::string widthCpp{}, widthLexeme{};
			if (!parseBorderWidthObject(findObjectField(*borderValue, "width"), "width", widthCpp, widthLexeme)) {
				return false;
			}

			outCpp = "Clay_BorderElementConfig{.color = " + colorCpp + ", .width = " + widthCpp + "}";
			outLexeme = "Clay_BorderElementConfig{color=" + colorLexeme + ", width=" + widthLexeme + "}";
			return true;
		};

		auto parseElementIdObject = [&](
			const JsonValue* idValue,
			const char* componentName,
			std::string& outCpp,
			std::string& outLexeme) -> bool {
			if (!idValue || idValue->kind != JsonValue::Kind::Object) {
				outError = std::string("composite_struct element id has missing/invalid object: ") + componentName;
				return false;
			}

			std::string idLexeme{}, offsetLexeme{}, baseIdLexeme{};
			if (
				!parseUnsignedInteger(findObjectField(*idValue, "id"), "id", static_cast<uint64_t>(std::numeric_limits<uint32_t>::max()), idLexeme) ||
				!parseUnsignedInteger(findObjectField(*idValue, "offset"), "offset", static_cast<uint64_t>(std::numeric_limits<uint32_t>::max()), offsetLexeme) ||
				!parseUnsignedInteger(findObjectField(*idValue, "baseId"), "baseId", static_cast<uint64_t>(std::numeric_limits<uint32_t>::max()), baseIdLexeme))
			{
				return false;
			}

			const JsonValue* stringIdValue = findObjectField(*idValue, "stringId");
			if (!stringIdValue || stringIdValue->kind != JsonValue::Kind::String) {
				outError = "composite_struct element id requires stringId string.";
				return false;
			}
			const std::string stringLiteral = cppStringLiteral(stringIdValue->text);
			const std::string stringLength = std::to_string(stringIdValue->text.size());

			const JsonValue* staticAllocatedValue = findObjectField(*idValue, "isStaticallyAllocated");
			if (!staticAllocatedValue || staticAllocatedValue->kind != JsonValue::Kind::Bool) {
				outError = "composite_struct element id requires isStaticallyAllocated bool.";
				return false;
			}
			const std::string staticAllocatedLiteral = staticAllocatedValue->boolValue ? "true" : "false";
			const std::string charsLiteral = stringIdValue->text.empty() ? "nullptr" : stringLiteral;

			outCpp =
				"Clay_ElementId{.id = " + idLexeme +
				", .offset = " + offsetLexeme +
				", .baseId = " + baseIdLexeme +
				", .stringId = Clay_String{.isStaticallyAllocated = " + staticAllocatedLiteral +
				", .length = " + stringLength +
				", .chars = " + charsLiteral + "}}";
			outLexeme =
				"Clay_ElementId{id=" + idLexeme + ", offset=" + offsetLexeme + ", baseId=" + baseIdLexeme + "}";
			return true;
		};

		std::string compositeType{};
		(void)jsonString(*typeValue, compositeType);
		if (compositeType == "Clay_Sizing") {
			std::string sizingCpp{};
			std::string sizingLexeme{};
			if (!parseSizingObject(findObjectField(*rawValue, "sizing"), "sizing", sizingCpp, sizingLexeme)) {
				return false;
			}
			outChange.cppValue = sizingCpp;
			outChange.valueLexeme = sizingLexeme;
			return true;
		}
		if (compositeType == "Clay_LayoutConfig") {
			std::string layoutCpp{};
			std::string layoutLexeme{};
			if (!parseLayoutConfigObject(findObjectField(*rawValue, "layout"), "layout", layoutCpp, layoutLexeme)) {
				return false;
			}
			outChange.cppValue = layoutCpp;
			outChange.valueLexeme = layoutLexeme;
			return true;
		}
		if (compositeType == "Clay_TextElementConfig") {
			const JsonValue* textValue = findObjectField(*rawValue, "text");
			if (!textValue || textValue->kind != JsonValue::Kind::Object) {
				outError = "composite_struct text config requires text object.";
				return false;
			}

			std::string userDataLexeme{};
			if (!parseUnsignedInteger(
				findObjectField(*textValue, "userData"),
				"userData",
				std::numeric_limits<uint64_t>::max(),
				userDataLexeme))
			{
				return false;
			}

			std::string textColorCpp{}, textColorLexeme{};
			if (!parseColorObject(findObjectField(*textValue, "textColor"), "textColor", textColorCpp, textColorLexeme)) {
				return false;
			}

			std::string fontIdLexeme{}, fontSizeLexeme{}, letterSpacingLexeme{}, lineHeightLexeme{};
			if (
				!parseUnsignedInteger(findObjectField(*textValue, "fontId"), "fontId", static_cast<uint64_t>(std::numeric_limits<uint16_t>::max()), fontIdLexeme) ||
				!parseUnsignedInteger(findObjectField(*textValue, "fontSize"), "fontSize", static_cast<uint64_t>(std::numeric_limits<uint16_t>::max()), fontSizeLexeme) ||
				!parseUnsignedInteger(findObjectField(*textValue, "letterSpacing"), "letterSpacing", static_cast<uint64_t>(std::numeric_limits<uint16_t>::max()), letterSpacingLexeme) ||
				!parseUnsignedInteger(findObjectField(*textValue, "lineHeight"), "lineHeight", static_cast<uint64_t>(std::numeric_limits<uint16_t>::max()), lineHeightLexeme))
			{
				return false;
			}

			std::string wrapModeName{};
			uint8_t wrapModeNumeric = 0u;
			if (!parseEnumPayload(findObjectField(*textValue, "wrapMode"), "wrapMode", wrapModeName, wrapModeNumeric)) {
				return false;
			}

			std::string textAlignmentName{};
			uint8_t textAlignmentNumeric = 0u;
			if (!parseEnumPayload(findObjectField(*textValue, "textAlignment"), "textAlignment", textAlignmentName, textAlignmentNumeric)) {
				return false;
			}

			outChange.cppValue =
				"Clay_TextElementConfig{.userData = " + pointerExprFromLexeme(userDataLexeme) +
				", .textColor = " + textColorCpp +
				", .fontId = " + fontIdLexeme +
				", .fontSize = " + fontSizeLexeme +
				", .letterSpacing = " + letterSpacingLexeme +
				", .lineHeight = " + lineHeightLexeme +
				", .wrapMode = " + wrapModeName +
				", .textAlignment = " + textAlignmentName + "}";
			outChange.valueLexeme =
				"Clay_TextElementConfig{fontId=" + fontIdLexeme +
				", fontSize=" + fontSizeLexeme +
				", wrapMode=" + wrapModeName + " (" + std::to_string(wrapModeNumeric) +
				"), textAlignment=" + textAlignmentName + " (" + std::to_string(textAlignmentNumeric) + ")}";
			return true;
		}
		if (compositeType == "Clay_FloatingElementConfig") {
			std::string floatingCpp{};
			std::string floatingLexeme{};
			if (!parseFloatingObject(findObjectField(*rawValue, "floating"), "floating", floatingCpp, floatingLexeme)) {
				return false;
			}
			outChange.cppValue = floatingCpp;
			outChange.valueLexeme = floatingLexeme;
			return true;
		}
		if (compositeType == "Clay_ClipElementConfig") {
			std::string clipCpp{};
			std::string clipLexeme{};
			if (!parseClipObject(findObjectField(*rawValue, "clip"), "clip", clipCpp, clipLexeme)) {
				return false;
			}
			outChange.cppValue = clipCpp;
			outChange.valueLexeme = clipLexeme;
			return true;
		}
		if (compositeType == "Clay_BorderElementConfig") {
			std::string borderCpp{};
			std::string borderLexeme{};
			if (!parseBorderObject(findObjectField(*rawValue, "border"), "border", borderCpp, borderLexeme)) {
				return false;
			}
			outChange.cppValue = borderCpp;
			outChange.valueLexeme = borderLexeme;
			return true;
		}
		if (compositeType == "Clay_ElementDeclaration") {
			const JsonValue* declarationValue = findObjectField(*rawValue, "declaration");
			if (!declarationValue || declarationValue->kind != JsonValue::Kind::Object) {
				outError = "composite_struct element declaration requires declaration object.";
				return false;
			}

			std::string idCpp{}, idLexeme{};
			if (!parseElementIdObject(findObjectField(*declarationValue, "id"), "id", idCpp, idLexeme)) {
				return false;
			}

			std::string layoutCpp{}, layoutLexeme{};
			if (!parseLayoutConfigObject(findObjectField(*declarationValue, "layout"), "layout", layoutCpp, layoutLexeme)) {
				return false;
			}

			std::string backgroundColorCpp{}, backgroundColorLexeme{};
			if (!parseColorObject(findObjectField(*declarationValue, "backgroundColor"), "backgroundColor", backgroundColorCpp, backgroundColorLexeme)) {
				return false;
			}

			std::string cornerRadiusCpp{}, cornerRadiusLexeme{};
			if (!parseCornerRadiusObject(findObjectField(*declarationValue, "cornerRadius"), "cornerRadius", cornerRadiusCpp, cornerRadiusLexeme)) {
				return false;
			}

			const JsonValue* aspectRatioValue = findObjectField(*declarationValue, "aspectRatio");
			if (!aspectRatioValue || aspectRatioValue->kind != JsonValue::Kind::Object) {
				outError = "composite_struct element declaration requires aspectRatio object.";
				return false;
			}
			std::string aspectRatioLiteral{}, aspectRatioLexeme{};
			if (!parseFloatComponent(findObjectField(*aspectRatioValue, "aspectRatio"), "aspectRatio.aspectRatio", aspectRatioLiteral, aspectRatioLexeme)) {
				return false;
			}

			const JsonValue* imageValue = findObjectField(*declarationValue, "image");
			if (!imageValue || imageValue->kind != JsonValue::Kind::Object) {
				outError = "composite_struct element declaration requires image object.";
				return false;
			}
			std::string imageDataLexeme{};
			if (!parseUnsignedInteger(
				findObjectField(*imageValue, "imageData"),
				"image.imageData",
				std::numeric_limits<uint64_t>::max(),
				imageDataLexeme))
			{
				return false;
			}

			std::string floatingCpp{}, floatingLexeme{};
			if (!parseFloatingObject(findObjectField(*declarationValue, "floating"), "floating", floatingCpp, floatingLexeme)) {
				return false;
			}

			const JsonValue* customValue = findObjectField(*declarationValue, "custom");
			if (!customValue || customValue->kind != JsonValue::Kind::Object) {
				outError = "composite_struct element declaration requires custom object.";
				return false;
			}
			std::string customDataLexeme{};
			if (!parseUnsignedInteger(
				findObjectField(*customValue, "customData"),
				"custom.customData",
				std::numeric_limits<uint64_t>::max(),
				customDataLexeme))
			{
				return false;
			}

			std::string clipCpp{}, clipLexeme{};
			if (!parseClipObject(findObjectField(*declarationValue, "clip"), "clip", clipCpp, clipLexeme)) {
				return false;
			}

			std::string borderCpp{}, borderLexeme{};
			if (!parseBorderObject(findObjectField(*declarationValue, "border"), "border", borderCpp, borderLexeme)) {
				return false;
			}

			std::string userDataLexeme{};
			if (!parseUnsignedInteger(
				findObjectField(*declarationValue, "userData"),
				"userData",
				std::numeric_limits<uint64_t>::max(),
				userDataLexeme))
			{
				return false;
			}

			outChange.cppValue =
				"Clay_ElementDeclaration{.id = " + idCpp +
				", .layout = " + layoutCpp +
				", .backgroundColor = " + backgroundColorCpp +
				", .cornerRadius = " + cornerRadiusCpp +
				", .aspectRatio = Clay_AspectRatioElementConfig{.aspectRatio = " + aspectRatioLiteral + "}" +
				", .image = Clay_ImageElementConfig{.imageData = " + pointerExprFromLexeme(imageDataLexeme) + "}" +
				", .floating = " + floatingCpp +
				", .custom = Clay_CustomElementConfig{.customData = " + pointerExprFromLexeme(customDataLexeme) + "}" +
				", .clip = " + clipCpp +
				", .border = " + borderCpp +
				", .userData = " + pointerExprFromLexeme(userDataLexeme) + "}";
			outChange.valueLexeme = "Clay_ElementDeclaration{id=" + idLexeme + ", layout=" + layoutLexeme + "}";
			return true;
		}

		outError = "composite_struct change has unsupported type: " + compositeType;
		return false;
	}
	if (outChange.jsonKind == "float1") {
		if (rawValue->kind != JsonValue::Kind::Object) {
			outError = "float1 change has non-object value.";
			return false;
		}

		const JsonValue* typeValue = findObjectField(*rawValue, "type");
		if (!typeValue || typeValue->kind != JsonValue::Kind::String) {
			outError = "float1 change requires string type.";
			return false;
		}

		auto parseFloat1Component = [&](
			const JsonValue* numericValue,
			const char* componentName,
			std::string& outLiteral,
			std::string& outLexeme) -> bool {
			if (!numericValue || numericValue->kind != JsonValue::Kind::Number) {
				outError = std::string("float1 change has missing/invalid component: ") + componentName;
				return false;
			}

			double numeric = 0.0;
			try {
				std::size_t consumed = 0u;
				numeric = std::stod(numericValue->text, &consumed);
				if (consumed != numericValue->text.size()) {
					outError = std::string("float1 component has invalid numeric lexeme: ") + componentName;
					return false;
				}
			} catch (...) {
				outError = std::string("float1 component is not parseable as number: ") + componentName;
				return false;
			}

			if (numeric < -static_cast<double>(std::numeric_limits<float>::max()) ||
				numeric > static_cast<double>(std::numeric_limits<float>::max())) {
				outError = std::string("float1 component is out of float range: ") + componentName;
				return false;
			}

			outLexeme = numericValue->text;
			outLiteral = numericValue->text;
			if (outLiteral.find_first_of(".eE") == std::string::npos) {
				outLiteral += ".0";
			}
			outLiteral += "f";
			return true;
		};

		std::string float1Type{};
		(void)jsonString(*typeValue, float1Type);
		if (float1Type == "Clay_AspectRatioElementConfig") {
			std::string aspectRatioLiteral{};
			std::string aspectRatioLexeme{};
			if (!parseFloat1Component(
				findObjectField(*rawValue, "aspectRatio"),
				"aspectRatio",
				aspectRatioLiteral,
				aspectRatioLexeme))
			{
				return false;
			}

			outChange.cppValue =
				"Clay_AspectRatioElementConfig{.aspectRatio = " + aspectRatioLiteral + "}";
			outChange.valueLexeme =
				"Clay_AspectRatioElementConfig{aspectRatio=" + aspectRatioLexeme + "}";
			return true;
		}

		outError = "float1 change has unsupported type: " + float1Type;
		return false;
	}
	if (outChange.jsonKind == "edgeu16") {
		static const std::vector<AggregateTypeInfo> kEdgeU16TypeInfos{
			{"Clay_Padding", {{"left", "left"}, {"right", "right"}, {"top", "top"}, {"bottom", "bottom"}}},
			{"Clay_BorderWidth", {{"left", "left"}, {"right", "right"}, {"top", "top"}, {"bottom", "bottom"}, {"betweenChildren", "betweenChildren"}}},
		};
		if (!parseTypedAggregateChange(
			*rawValue,
			"edgeu16",
			kEdgeU16TypeInfos,
			false,
			outChange.cppValue,
			outChange.valueLexeme,
			outError))
		{
			return false;
		}
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
		const JsonValue* paramsStructTypeHashValue = findObjectField(item, "paramsStructTypeHash");
		const JsonValue* paramsStructNameValue = findObjectField(item, "paramsStructName");
		const JsonValue* hasSourceMetadataValue = findObjectField(item, "hasSourceMetadata");
		const JsonValue* sourceFileValue = findObjectField(item, "sourceFile");
		const JsonValue* sourceLineValue = findObjectField(item, "sourceLine");
		const JsonValue* sourceColumnValue = findObjectField(item, "sourceColumn");
		const JsonValue* sourceFunctionValue = findObjectField(item, "sourceFunction");
		const JsonValue* changesValue = findObjectField(item, "changes");
		if (!definitionIdValue || !definitionNameValue || !changesValue) {
			outError = "Definition entry is missing required fields.";
			return false;
		}
		if (!jsonUInt64(*definitionIdValue, target.definitionId) || !jsonString(*definitionNameValue, target.definitionName)) {
			outError = "Invalid definition entry fields.";
			return false;
		}
		if (paramsStructTypeHashValue != nullptr) {
			if (!jsonUInt64(*paramsStructTypeHashValue, target.paramsStructTypeHash)) {
				outError = "Invalid definition.paramsStructTypeHash.";
				return false;
			}
		}
		if (paramsStructNameValue != nullptr) {
			if (!jsonString(*paramsStructNameValue, target.paramsStructName)) {
				outError = "Invalid definition.paramsStructName.";
				return false;
			}
		}
		if (hasSourceMetadataValue != nullptr) {
			if (!jsonBool(*hasSourceMetadataValue, target.hasSourceMetadata)) {
				outError = "Invalid definition.hasSourceMetadata.";
				return false;
			}
		}
		if (sourceFileValue != nullptr) {
			if (!jsonString(*sourceFileValue, target.sourceFile)) {
				outError = "Invalid definition.sourceFile.";
				return false;
			}
		}
		if (sourceLineValue != nullptr) {
			if (!jsonUInt32(*sourceLineValue, target.sourceLine)) {
				outError = "Invalid definition.sourceLine.";
				return false;
			}
		}
		if (sourceColumnValue != nullptr) {
			if (!jsonUInt32(*sourceColumnValue, target.sourceColumn)) {
				outError = "Invalid definition.sourceColumn.";
				return false;
			}
		}
		if (sourceFunctionValue != nullptr) {
			if (!jsonString(*sourceFunctionValue, target.sourceFunction)) {
				outError = "Invalid definition.sourceFunction.";
				return false;
			}
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
		const JsonValue* sourceColumnValue = findObjectField(item, "sourceColumn");
		const JsonValue* sourceFunctionValue = findObjectField(item, "sourceFunction");
		const JsonValue* sourceLocationHashValue = findObjectField(item, "sourceLocationHash");
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
		if (sourceColumnValue != nullptr) {
			if (!jsonUInt32(*sourceColumnValue, target.sourceColumn)) {
				outError = "Invalid instance.sourceColumn.";
				return false;
			}
		}
		if (sourceFunctionValue != nullptr) {
			if (!jsonString(*sourceFunctionValue, target.sourceFunction)) {
				outError = "Invalid instance.sourceFunction.";
				return false;
			}
		}
		if (sourceLocationHashValue != nullptr) {
			if (!jsonUInt64(*sourceLocationHashValue, target.sourceLocationHash)) {
				outError = "Invalid instance.sourceLocationHash.";
				return false;
			}
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

bool isIdentifierChar(char c);

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

std::size_t findMethodCallOutside(
	const std::string& text,
	std::string_view methodName,
	std::size_t begin,
	std::size_t endExclusive) {
	std::size_t search = begin;
	while (search < endExclusive) {
		const std::size_t pos = findOutside(text, methodName, search, endExclusive);
		if (pos == std::string::npos) {
			return std::string::npos;
		}
		const std::size_t nameEnd = pos + methodName.size();
		const bool validPrefix = methodName.front() == '.' || pos == 0u || !isIdentifierChar(text[pos - 1u]);
		const bool validSuffix = nameEnd >= text.size() || !isIdentifierChar(text[nameEnd]);
		if (validPrefix && validSuffix) {
			std::size_t openParen = nameEnd;
			while (openParen < endExclusive && std::isspace(static_cast<unsigned char>(text[openParen])) != 0) {
				++openParen;
			}
			if (openParen < endExclusive && text[openParen] == '(') {
				return pos;
			}
		}
		search = pos + methodName.size();
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

bool isIdentifierChar(char c) {
	return (std::isalnum(static_cast<unsigned char>(c)) != 0) || c == '_';
}

bool isIdentifierStartChar(char c) {
	return (std::isalpha(static_cast<unsigned char>(c)) != 0) || c == '_';
}

std::size_t findTopLevelCharInRange(
	const std::string& text,
	char needle,
	std::size_t begin,
	std::size_t endExclusive) {
	if (begin >= text.size()) {
		return std::string::npos;
	}
	endExclusive = std::min(endExclusive, text.size());
	ScanState state{};
	int parenDepth = 0;
	int bracketDepth = 0;
	int braceDepth = 0;
	for (std::size_t i = begin; i < endExclusive; ++i) {
		if (isCodePosition(state)) {
			const char c = text[i];
			if (c == '(') {
				++parenDepth;
			} else if (c == ')' && parenDepth > 0) {
				--parenDepth;
			} else if (c == '[') {
				++bracketDepth;
			} else if (c == ']' && bracketDepth > 0) {
				--bracketDepth;
			} else if (c == '{') {
				++braceDepth;
			} else if (c == '}' && braceDepth > 0) {
				--braceDepth;
			} else if (c == needle && parenDepth == 0 && bracketDepth == 0 && braceDepth == 0) {
				return i;
			}
		}
		scanAdvance(state, text, i);
	}
	return std::string::npos;
}

std::size_t findFieldTerminatorSemicolon(const std::string& text, std::size_t begin, std::size_t endExclusive) {
	if (begin >= text.size()) {
		return std::string::npos;
	}
	endExclusive = std::min(endExclusive, text.size());
	ScanState state{};
	int parenDepth = 0;
	int bracketDepth = 0;
	int braceDepth = 0;
	bool sawTopLevelParen = false;
	for (std::size_t i = begin; i < endExclusive; ++i) {
		if (isCodePosition(state)) {
			const char c = text[i];
			if (c == '(') {
				if (parenDepth == 0 && bracketDepth == 0 && braceDepth == 0) {
					sawTopLevelParen = true;
				}
				++parenDepth;
			} else if (c == ')' && parenDepth > 0) {
				--parenDepth;
			} else if (c == '[') {
				++bracketDepth;
			} else if (c == ']' && bracketDepth > 0) {
				--bracketDepth;
			} else if (c == '{') {
				if (parenDepth == 0 && bracketDepth == 0 && braceDepth == 0 && sawTopLevelParen) {
					// Likely a method/function body, not a field declaration.
					return std::string::npos;
				}
				++braceDepth;
			} else if (c == '}') {
				if (braceDepth > 0) {
					--braceDepth;
				} else {
					return std::string::npos;
				}
			} else if (c == ';' && parenDepth == 0 && bracketDepth == 0 && braceDepth == 0) {
				return i;
			}
		}
		scanAdvance(state, text, i);
	}
	return std::string::npos;
}

bool findFieldDeclarationInStructBody(
	const std::string& text,
	std::size_t bodyBegin,
	std::size_t bodyEnd,
	std::string_view fieldName,
	std::size_t& outFieldNameBegin,
	std::size_t& outFieldNameEnd,
	std::size_t& outSemicolonPos) {
	outFieldNameBegin = std::string::npos;
	outFieldNameEnd = std::string::npos;
	outSemicolonPos = std::string::npos;

	if (fieldName.empty() || bodyBegin >= bodyEnd || bodyBegin >= text.size()) {
		return false;
	}
	bodyEnd = std::min(bodyEnd, text.size());
	ScanState state{};
	int nestedBraceDepth = 0;
	for (std::size_t i = bodyBegin; i < bodyEnd; ++i) {
		if (isCodePosition(state)) {
			const char c = text[i];
			if (c == '{') {
				++nestedBraceDepth;
			} else if (c == '}') {
				if (nestedBraceDepth > 0) {
					--nestedBraceDepth;
				}
			} else if (nestedBraceDepth == 0 && isIdentifierStartChar(c)) {
				std::size_t tokenEnd = i + 1u;
				while (tokenEnd < bodyEnd && isIdentifierChar(text[tokenEnd])) {
					++tokenEnd;
				}
				if (
					tokenEnd - i == fieldName.size() &&
					text.compare(i, fieldName.size(), fieldName) == 0)
				{
					const std::size_t semicolonPos = findFieldTerminatorSemicolon(text, tokenEnd, bodyEnd);
					if (semicolonPos == std::string::npos) {
						return false;
					}
					outFieldNameBegin = i;
					outFieldNameEnd = tokenEnd;
					outSemicolonPos = semicolonPos;
					return true;
				}
				i = tokenEnd - 1u;
			}
		}
		scanAdvance(state, text, i);
	}
	return false;
}

struct StructDefinitionCandidate {
	std::size_t structKeywordPos = std::string::npos;
	std::size_t namePos = std::string::npos;
	std::size_t openBracePos = std::string::npos;
};

std::optional<StructDefinitionCandidate> findStructDefinitionCandidateAboveOffset(
	const std::string& text,
	std::size_t searchEnd,
	std::string_view structName) {
	if (structName.empty()) {
		return std::nullopt;
	}

	searchEnd = std::min(searchEnd, text.size());
	ScanState state{};
	std::optional<StructDefinitionCandidate> lastCandidate{};
	std::optional<StructDefinitionCandidate> lastDefinitionCandidate{};

	for (std::size_t i = 0; i < searchEnd; ++i) {
		if (isCodePosition(state)) {
			constexpr std::string_view kStructKeyword = "struct";
			const bool keywordMatches =
				(i + kStructKeyword.size() <= searchEnd) &&
				(text.compare(i, kStructKeyword.size(), kStructKeyword) == 0);
			if (keywordMatches) {
				const std::size_t keywordEnd = i + kStructKeyword.size();
				const bool hasValidPrefix = (i == 0u) || !isIdentifierChar(text[i - 1u]);
				const bool hasValidSuffix = (keywordEnd >= searchEnd) || !isIdentifierChar(text[keywordEnd]);
				if (hasValidPrefix && hasValidSuffix) {
					bool foundName = false;
					std::size_t foundNamePos = std::string::npos;
					std::size_t foundOpenBrace = std::string::npos;

					ScanState declarationState{};
					for (std::size_t j = keywordEnd; j < searchEnd; ++j) {
						if (isCodePosition(declarationState)) {
							const char c = text[j];
							if (c == ';') {
								break;
							}
							if (c == '{') {
								foundOpenBrace = j;
								break;
							}
							if (isIdentifierStartChar(c)) {
								std::size_t tokenEnd = j + 1u;
								while (tokenEnd < searchEnd && isIdentifierChar(text[tokenEnd])) {
									++tokenEnd;
								}
								if (
									tokenEnd - j == structName.size() &&
									text.compare(j, structName.size(), structName) == 0)
								{
									foundName = true;
									foundNamePos = j;
								}
								j = tokenEnd - 1u;
							}
						}
						scanAdvance(declarationState, text, j);
					}

					if (foundName) {
						const StructDefinitionCandidate candidate{
							.structKeywordPos = i,
							.namePos = foundNamePos,
							.openBracePos = foundOpenBrace,
						};
						lastCandidate = candidate;
						if (candidate.openBracePos != std::string::npos) {
							lastDefinitionCandidate = candidate;
						}
					}
				}
			}
		}
		scanAdvance(state, text, i);
	}

	if (lastDefinitionCandidate.has_value()) {
		return lastDefinitionCandidate;
	}
	return lastCandidate;
}

void appendDefinitionUnresolvedForAllChanges(
	std::vector<UnresolvedEntry>& unresolved,
	const ParsedDefinitionTarget& target,
	std::string_view reason) {
	for (const ParsedChange& change : target.changes) {
		unresolved.push_back(UnresolvedEntry{
			.scope = "definition",
			.definitionId = target.definitionId,
			.flowId = 0u,
			.elementId = {},
			.sourceFile = target.sourceFile,
			.sourceLine = target.sourceLine,
			.fieldHash = change.fieldHash,
			.fieldName = change.fieldName,
			.reason = std::string(reason),
		});
	}
}

void appendInstanceUnresolvedForAllChanges(
	std::vector<UnresolvedEntry>& unresolved,
	const ParsedInstanceTarget& target,
	std::string_view reason) {
	for (const ParsedChange& change : target.changes) {
		unresolved.push_back(UnresolvedEntry{
			.scope = "instance",
			.definitionId = target.definitionId,
			.flowId = target.flowId,
			.elementId = target.elementId,
			.sourceFile = target.sourceFile,
			.sourceLine = target.sourceLine,
			.fieldHash = change.fieldHash,
			.fieldName = change.fieldName,
			.reason = std::string(reason),
		});
	}
}

struct PatchResult {
	bool patched = false;
	std::vector<UnresolvedEntry> unresolved{};
};

std::size_t distanceBetweenOffsets(std::size_t a, std::size_t b) {
	return (a > b) ? (a - b) : (b - a);
}

std::size_t findCreateElementForTarget(
	const std::string& content,
	std::size_t lineStart,
	std::size_t lineEnd,
	uint32_t sourceColumn) {
	constexpr std::string_view kCreateElement = ".createElement";

	std::size_t bestPos = std::string::npos;
	std::size_t bestScore = std::numeric_limits<std::size_t>::max();
	const std::size_t columnOffset =
		(sourceColumn == 0u) ? std::string::npos : lineStart + static_cast<std::size_t>(sourceColumn - 1u);

	std::size_t search = lineStart;
	while (search < lineEnd) {
		const std::size_t pos = findMethodCallOutside(content, kCreateElement, search, lineEnd);
		if (pos == std::string::npos) {
			break;
		}
		if (sourceColumn == 0u) {
			return pos;
		}

		const std::size_t dotScore = distanceBetweenOffsets(pos, columnOffset);
		const std::size_t nameScore = distanceBetweenOffsets(pos + 1u, columnOffset);
		const std::size_t score = std::min(dotScore, nameScore);
		if (score < bestScore) {
			bestScore = score;
			bestPos = pos;
		}
		search = pos + kCreateElement.size();
	}

	return bestPos;
}

struct ChainCall {
	enum class Kind : uint8_t {
		SetParameters,
		MergeParams,
	};

	Kind kind = Kind::SetParameters;
	std::string_view name{};
	std::size_t pos = std::string::npos;
	std::size_t openParen = std::string::npos;
	std::size_t closeParen = std::string::npos;
};

std::vector<ChainCall> findParameterMutationCalls(
	const std::string& content,
	std::size_t begin,
	std::size_t endExclusive) {
	static constexpr std::pair<std::string_view, ChainCall::Kind> kMethodNames[] = {
		{ ".setParameters", ChainCall::Kind::SetParameters },
		{ ".setParams", ChainCall::Kind::SetParameters },
		{ ".mergeParameters", ChainCall::Kind::MergeParams },
		{ ".mergeParams", ChainCall::Kind::MergeParams },
	};

	std::vector<ChainCall> calls{};
	for (const auto& [name, kind] : kMethodNames) {
		std::size_t search = begin;
		while (search < endExclusive) {
			const std::size_t pos = findMethodCallOutside(content, name, search, endExclusive);
			if (pos == std::string::npos) {
				break;
			}
			const std::size_t openParen = content.find('(', pos + name.size());
			if (openParen == std::string::npos || openParen >= endExclusive) {
				search = pos + name.size();
				continue;
			}
			const std::size_t closeParen = findMatchingBracket(content, openParen, '(', ')');
			if (closeParen == std::string::npos || closeParen > endExclusive) {
				calls.push_back(ChainCall{
					.kind = kind,
					.name = name,
					.pos = pos,
					.openParen = openParen,
					.closeParen = std::string::npos,
				});
				search = pos + name.size();
				continue;
			}
			calls.push_back(ChainCall{
				.kind = kind,
				.name = name,
				.pos = pos,
				.openParen = openParen,
				.closeParen = closeParen,
			});
			search = closeParen + 1u;
		}
	}

	std::sort(
		calls.begin(),
		calls.end(),
		[](const ChainCall& lhs, const ChainCall& rhs) {
			return lhs.pos < rhs.pos;
		});
	return calls;
}

PatchResult patchDefinitionTarget(std::string& content, const ParsedDefinitionTarget& target) {
	PatchResult result{};

	if (target.changes.empty()) {
		return result;
	}

	const std::vector<std::size_t> lines = lineOffsets(content);
	const std::size_t lineStart = lineToOffset(lines, target.sourceLine);
	if (lineStart == std::string::npos) {
		appendDefinitionUnresolvedForAllChanges(
			result.unresolved,
			target,
			"Definition source line is outside file bounds.");
		return result;
	}
	if (target.paramsStructName.empty()) {
		appendDefinitionUnresolvedForAllChanges(
			result.unresolved,
			target,
			"Definition is missing paramsStructName metadata.");
		return result;
	}

	const std::optional<StructDefinitionCandidate> candidate =
		findStructDefinitionCandidateAboveOffset(content, lineStart, target.paramsStructName);
	if (!candidate.has_value()) {
		appendDefinitionUnresolvedForAllChanges(
			result.unresolved,
			target,
			"Could not find params struct definition above source metadata line.");
		return result;
	}
	if (candidate->openBracePos == std::string::npos) {
		appendDefinitionUnresolvedForAllChanges(
			result.unresolved,
			target,
			"Found params struct name but could not find opening '{'.");
		return result;
	}
	if (findMatchingBracket(content, candidate->openBracePos, '{', '}') == std::string::npos) {
		appendDefinitionUnresolvedForAllChanges(
			result.unresolved,
			target,
			"Found params struct opening '{' but could not find matching '}'.");
		return result;
	}

	bool patchedAnyField = false;
	for (const ParsedChange& change : target.changes) {
		const std::size_t structClosePos = findMatchingBracket(content, candidate->openBracePos, '{', '}');
		if (structClosePos == std::string::npos) {
			result.unresolved.push_back(UnresolvedEntry{
				.scope = "definition",
				.definitionId = target.definitionId,
				.flowId = 0u,
				.elementId = {},
				.sourceFile = target.sourceFile,
				.sourceLine = target.sourceLine,
				.fieldHash = change.fieldHash,
				.fieldName = change.fieldName,
				.reason = "Struct braces became unbalanced while patching.",
			});
			continue;
		}

		std::size_t fieldNameBegin = std::string::npos;
		std::size_t fieldNameEnd = std::string::npos;
		std::size_t fieldTerminator = std::string::npos;
		const bool foundField = findFieldDeclarationInStructBody(
			content,
			candidate->openBracePos + 1u,
			structClosePos,
			change.fieldName,
			fieldNameBegin,
			fieldNameEnd,
			fieldTerminator);
		if (!foundField) {
			result.unresolved.push_back(UnresolvedEntry{
				.scope = "definition",
				.definitionId = target.definitionId,
				.flowId = 0u,
				.elementId = {},
				.sourceFile = target.sourceFile,
				.sourceLine = target.sourceLine,
				.fieldHash = change.fieldHash,
				.fieldName = change.fieldName,
				.reason = "Field was not found inside params struct scope.",
			});
			continue;
		}

		const std::size_t equalsPos = findTopLevelCharInRange(content, '=', fieldNameEnd, fieldTerminator);
		if (equalsPos != std::string::npos) {
			std::size_t valueBegin = equalsPos + 1u;
			while (valueBegin < fieldTerminator && std::isspace(static_cast<unsigned char>(content[valueBegin])) != 0) {
				++valueBegin;
			}
			std::size_t valueEnd = fieldTerminator;
			while (valueEnd > valueBegin && std::isspace(static_cast<unsigned char>(content[valueEnd - 1u])) != 0) {
				--valueEnd;
			}
			replaceRange(content, valueBegin, valueEnd, change.cppValue);
		} else {
			replaceRange(content, fieldTerminator, fieldTerminator, " = " + change.cppValue);
		}
		patchedAnyField = true;
	}

	result.patched = patchedAnyField;
	return result;
}

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

	const std::size_t createPos = findCreateElementForTarget(content, lineStart, lineEnd, target.sourceColumn);
	if (createPos == std::string::npos) {
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
	const std::size_t chainSemicolon = findTopLevelCharInRange(content, ';', afterCreate, content.size());
	const std::size_t searchEnd = (chainSemicolon == std::string::npos) ? content.size() : chainSemicolon;
	const std::size_t drawPos = findOutside(content, ".draw", afterCreate, searchEnd);
	const std::vector<ChainCall> parameterCalls = findParameterMutationCalls(content, afterCreate, searchEnd);

	std::unordered_map<std::string, std::string> fieldValues{};
	for (const ParsedChange& change : target.changes) {
		fieldValues[change.fieldName] = change.cppValue;
	}
	if (fieldValues.empty()) {
		return result;
	}

	// No parameter mutation call: insert one after createElement(...) call.
	if (parameterCalls.empty()) {
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

	const auto firstMerge = std::find_if(
		parameterCalls.begin(),
		parameterCalls.end(),
		[](const ChainCall& call) {
			return call.kind == ChainCall::Kind::MergeParams;
		});
	if (firstMerge != parameterCalls.end()) {
		const ChainCall& lastCall = parameterCalls.back();
		if (lastCall.closeParen == std::string::npos) {
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
					.reason = "Unbalanced parentheses in parameter mutation call.",
				});
			}
			return result;
		}
		std::string callIndent = leadingWhitespaceAt(content, lastCall.pos);
		if (callIndent.empty()) {
			callIndent = leadingWhitespaceAt(content, createPos);
			callIndent += "\t";
		}
		const std::string snippet = buildMergeParamsBlock(callIndent, fieldValues, false);
		replaceRange(content, lastCall.closeParen + 1u, lastCall.closeParen + 1u, snippet);
		result.patched = true;
		return result;
	}

	const ChainCall* setParametersCall = nullptr;
	for (const ChainCall& call : parameterCalls) {
		if (call.kind == ChainCall::Kind::SetParameters) {
			setParametersCall = &call;
			break;
		}
	}
	if (setParametersCall == nullptr) {
		return result;
	}
	const std::size_t setParametersPos = setParametersCall->pos;
	const std::size_t setNameEnd = setParametersCall->openParen;
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
	const std::size_t setClose = setParametersCall->closeParen;
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

std::string buildOutputJson(
	const OutputModel& model,
	std::size_t patchedDefinitionCount,
	std::size_t patchedInstanceCount,
	std::size_t patchedFileCount) {
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
	appendIndent(2); out += "\"patchedDefinitionTargets\":" + std::to_string(patchedDefinitionCount) + ",\n";
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
		appendIndent(3); out += "\"paramsStructTypeHash\":" + std::to_string(def.paramsStructTypeHash) + ",\n";
		appendIndent(3); out += "\"paramsStructName\":"; appendString(def.paramsStructName); out += ",\n";
		appendIndent(3); out += "\"hasSourceMetadata\":"; out += (def.hasSourceMetadata ? "true" : "false"); out += ",\n";
		appendIndent(3); out += "\"sourceFile\":"; appendString(def.sourceFile); out += ",\n";
		appendIndent(3); out += "\"sourceLine\":" + std::to_string(def.sourceLine) + ",\n";
		appendIndent(3); out += "\"sourceColumn\":" + std::to_string(def.sourceColumn) + ",\n";
		appendIndent(3); out += "\"sourceFunction\":"; appendString(def.sourceFunction); out += ",\n";
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
		appendIndent(3); out += "\"sourceColumn\":" + std::to_string(in.sourceColumn) + ",\n";
		appendIndent(3); out += "\"sourceFunction\":"; appendString(in.sourceFunction); out += ",\n";
		appendIndent(3); out += "\"sourceLocationHash\":" + std::to_string(in.sourceLocationHash) + ",\n";
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

	enum class FileTargetScope : uint8_t {
		Definition,
		Instance,
	};

	struct FileTargetRef {
		FileTargetScope scope = FileTargetScope::Instance;
		std::size_t index = 0u;
		uint32_t sourceLine = 0u;
		uint32_t sourceColumn = 0u;
	};

	std::unordered_map<std::string, std::vector<FileTargetRef>> targetIndexesByFile{};

	for (std::size_t i = 0; i < model.definitions.size(); ++i) {
		const ParsedDefinitionTarget& target = model.definitions[i];
		if (!target.hasSourceMetadata) {
			appendDefinitionUnresolvedForAllChanges(
				model.unresolved,
				target,
				"Definition has no source metadata.");
			continue;
		}
		if (target.sourceFile.empty()) {
			appendDefinitionUnresolvedForAllChanges(
				model.unresolved,
				target,
				"Definition source file metadata is empty.");
			continue;
		}
		if (target.sourceLine == 0u) {
			appendDefinitionUnresolvedForAllChanges(
				model.unresolved,
				target,
				"Definition source line metadata is invalid.");
			continue;
		}
		if (!std::filesystem::path(target.sourceFile).is_absolute()) {
			appendDefinitionUnresolvedForAllChanges(
				model.unresolved,
				target,
				"Definition source file path is not absolute.");
			continue;
		}
		targetIndexesByFile[target.sourceFile].push_back(FileTargetRef{
			.scope = FileTargetScope::Definition,
			.index = i,
			.sourceLine = target.sourceLine,
			.sourceColumn = target.sourceColumn,
		});
	}

	for (std::size_t i = 0; i < model.instances.size(); ++i) {
		const ParsedInstanceTarget& target = model.instances[i];
		if (target.sourceFile.empty()) {
			appendInstanceUnresolvedForAllChanges(
				model.unresolved,
				target,
				"Instance source file metadata is empty.");
			continue;
		}
		if (target.sourceLine == 0u) {
			appendInstanceUnresolvedForAllChanges(
				model.unresolved,
				target,
				"Instance source line metadata is invalid.");
			continue;
		}
		if (!std::filesystem::path(target.sourceFile).is_absolute()) {
			appendInstanceUnresolvedForAllChanges(
				model.unresolved,
				target,
				"V1 cant resolve relative filepaths.");
			continue;
		}
		targetIndexesByFile[target.sourceFile].push_back(FileTargetRef{
			.scope = FileTargetScope::Instance,
			.index = i,
			.sourceLine = target.sourceLine,
			.sourceColumn = target.sourceColumn,
		});
	}

	std::size_t patchedDefinitionTargets = 0u;
	std::size_t patchedInstanceTargets = 0u;
	std::unordered_set<std::string> patchedFileSet{};

	for (auto& [sourceFile, targets] : targetIndexesByFile) {
		const std::filesystem::path sourcePath = sourceFile;
		std::string content{};
		std::string readError{};
		if (!readFileText(sourcePath, content, readError)) {
			for (const FileTargetRef& targetRef : targets) {
				if (targetRef.scope == FileTargetScope::Definition) {
					const ParsedDefinitionTarget& target = model.definitions[targetRef.index];
					appendDefinitionUnresolvedForAllChanges(
						model.unresolved,
						target,
						"Failed to open definition source file.");
				} else {
					const ParsedInstanceTarget& target = model.instances[targetRef.index];
					appendInstanceUnresolvedForAllChanges(
						model.unresolved,
						target,
						"Failed to open source file.");
				}
			}
			continue;
		}

		std::sort(
			targets.begin(),
			targets.end(),
			[](const FileTargetRef& lhs, const FileTargetRef& rhs) {
				if (lhs.sourceLine != rhs.sourceLine) {
					return lhs.sourceLine > rhs.sourceLine;
				}
				if (lhs.sourceColumn != rhs.sourceColumn) {
					return lhs.sourceColumn > rhs.sourceColumn;
				}
				return static_cast<uint8_t>(lhs.scope) < static_cast<uint8_t>(rhs.scope);
			});

		bool fileChanged = false;
		for (const FileTargetRef& targetRef : targets) {
			if (targetRef.scope == FileTargetScope::Definition) {
				const ParsedDefinitionTarget& target = model.definitions[targetRef.index];
				PatchResult patch = patchDefinitionTarget(content, target);
				if (patch.patched) {
					++patchedDefinitionTargets;
					fileChanged = true;
				}
				for (const UnresolvedEntry& unresolved : patch.unresolved) {
					model.unresolved.push_back(unresolved);
				}
			} else {
				const ParsedInstanceTarget& target = model.instances[targetRef.index];
				PatchResult patch = patchInstanceTarget(content, target);
				if (patch.patched) {
					++patchedInstanceTargets;
					fileChanged = true;
				}
				for (const UnresolvedEntry& unresolved : patch.unresolved) {
					model.unresolved.push_back(unresolved);
				}
			}
		}

		if (fileChanged) {
			std::string writeError{};
			if (!writeFileText(sourcePath, content, writeError)) {
				for (const FileTargetRef& targetRef : targets) {
					if (targetRef.scope == FileTargetScope::Definition) {
						const ParsedDefinitionTarget& target = model.definitions[targetRef.index];
						appendDefinitionUnresolvedForAllChanges(
							model.unresolved,
							target,
							"Patched in memory but failed to write definition source file.");
					} else {
						const ParsedInstanceTarget& target = model.instances[targetRef.index];
						appendInstanceUnresolvedForAllChanges(
							model.unresolved,
							target,
							"Patched in memory but failed to write source file.");
					}
				}
			} else {
				patchedFileSet.insert(sourceFile);
			}
		}
	}

	const std::size_t patchedFiles = patchedFileSet.size();
	const std::string outputJson = buildOutputJson(
		model,
		patchedDefinitionTargets,
		patchedInstanceTargets,
		patchedFiles);
	std::string outputError{};
	if (!writeFileText(outputPath, outputJson, outputError)) {
		std::cerr << outputError << "\n";
		return 1;
	}

	std::cout
		<< "flowui_devChange_updater: patched definition targets=" << patchedDefinitionTargets
		<< ", patched instance targets=" << patchedInstanceTargets
		<< ", patched files=" << patchedFiles
		<< ", unresolved=" << model.unresolved.size()
		<< ", output='" << outputPath.string() << "'\n";
	return 0;
}
