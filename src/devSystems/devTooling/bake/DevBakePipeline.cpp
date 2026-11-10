#include "devSystems/devTooling/bake/DevBakePipeline.hpp"

#if FLOW_UI_DEV_MODE

#include <algorithm>
#include <cctype>
#include <charconv>
#include <chrono>
#include <cmath>
#include <ctime>
#include <fstream>
#include <iomanip>
#include <limits>
#include <locale>
#include <optional>
#include <sstream>
#include <string_view>
#include <system_error>
#include <tuple>
#include <utility>

#include "devSystems/devTooling/bake/DevBakeInternals.hpp"
#include "devSystems/devTooling/override/DevOverrideEngine.hpp"
#include "devSystems/devTooling/schema/DevSchemaRegistry.hpp"
#include "FlowUi/Public/managerStructs/ActionTypes.hpp"

namespace FlowUi::devSystems::tooling {
namespace {

struct JsonValue {
	enum class Kind { Null, Boolean, Number, String, Array, Object } kind = Kind::Null;
	bool boolean = false;
	std::string text{};
	std::vector<JsonValue> array{};
	std::vector<std::pair<std::string, JsonValue>> object{};

	[[nodiscard]] const JsonValue* find(std::string_view name) const noexcept {
		for (const auto& member : object) if (member.first == name) return &member.second;
		return nullptr;
	}
};

class JsonParser {
public:
	explicit JsonParser(std::string_view input) : input_(input) {}
	[[nodiscard]] bool parse(JsonValue& output) {
		skipSpace();
		if (!parseValue(output)) return false;
		skipSpace();
		return position_ == input_.size();
	}

private:
	void skipSpace() noexcept {
		while (position_ < input_.size() &&
			(input_[position_] == ' ' || input_[position_] == '\n' ||
			 input_[position_] == '\r' || input_[position_] == '\t')) ++position_;
	}
	bool consume(char value) noexcept {
		skipSpace();
		if (position_ >= input_.size() || input_[position_] != value) return false;
		++position_;
		return true;
	}
	bool parseValue(JsonValue& output) {
		skipSpace();
		if (position_ >= input_.size()) return false;
		const char value = input_[position_];
		if (value == '"') {
			output.kind = JsonValue::Kind::String;
			return parseString(output.text);
		}
		if (value == '{') return parseObject(output);
		if (value == '[') return parseArray(output);
		if (input_.substr(position_, 4) == "true") {
			position_ += 4; output.kind = JsonValue::Kind::Boolean; output.boolean = true; return true;
		}
		if (input_.substr(position_, 5) == "false") {
			position_ += 5; output.kind = JsonValue::Kind::Boolean; output.boolean = false; return true;
		}
		if (input_.substr(position_, 4) == "null") {
			position_ += 4; output.kind = JsonValue::Kind::Null; return true;
		}
		return parseNumber(output);
	}
	bool parseString(std::string& output) {
		if (position_ >= input_.size() || input_[position_++] != '"') return false;
		while (position_ < input_.size()) {
			const char value = input_[position_++];
			if (value == '"') return true;
			if (static_cast<unsigned char>(value) < 32)
				return false;
			if (value != '\\') { output.push_back(value); continue; }
			if (position_ >= input_.size()) return false;
			switch (input_[position_++]) {
			case '"': output.push_back('"'); break;
			case '\\': output.push_back('\\'); break;
			case '/': output.push_back('/'); break;
			case 'b': output.push_back('\b'); break;
			case 'f': output.push_back('\f'); break;
			case 'n': output.push_back('\n'); break;
			case 'r': output.push_back('\r'); break;
			case 't': output.push_back('\t'); break;
			default: return false;
			}
		}
		return false;
	}
	bool parseNumber(JsonValue& output) {
		const std::size_t first = position_;
		if (position_ < input_.size() && input_[position_] == '-') ++position_;
		if (position_ >= input_.size() || input_[position_] < '0' || input_[position_] > '9')
			return false;
		if (input_[position_] == '0')
			++position_;
		else
			while (position_ < input_.size() && input_[position_] >= '0' &&
				   input_[position_] <= '9')
				++position_;
		if (position_ < input_.size() && input_[position_] == '.') {
			++position_;
			const auto fraction_start = position_;
			while (position_ < input_.size() && input_[position_] >= '0' && input_[position_] <= '9') ++position_;
			if (position_ == fraction_start)
				return false;
		}
		if (position_ < input_.size() && (input_[position_] == 'e' || input_[position_] == 'E')) {
			++position_;
			if (position_ < input_.size() && (input_[position_] == '+' || input_[position_] == '-')) ++position_;
			const auto exponent_start = position_;
			while (position_ < input_.size() && input_[position_] >= '0' && input_[position_] <= '9') ++position_;
			if (position_ == exponent_start)
				return false;
		}
		if (position_ == first) return false;
		output.kind = JsonValue::Kind::Number;
		output.text.assign(input_.substr(first, position_ - first));
		return true;
	}
	bool parseArray(JsonValue& output) {
		if (!consume('[')) return false;
		output.kind = JsonValue::Kind::Array;
		skipSpace();
		if (consume(']')) return true;
		for (;;) {
			output.array.emplace_back();
			if (!parseValue(output.array.back())) return false;
			if (consume(']')) return true;
			if (!consume(',')) return false;
		}
	}
	bool parseObject(JsonValue& output) {
		if (!consume('{')) return false;
		output.kind = JsonValue::Kind::Object;
		skipSpace();
		if (consume('}')) return true;
		for (;;) {
			std::string name;
			skipSpace();
			if (!parseString(name) || !consume(':')) return false;
			JsonValue value;
			if (!parseValue(value)) return false;
			if (output.find(name))
				return false;
			output.object.emplace_back(std::move(name), std::move(value));
			if (consume('}')) return true;
			if (!consume(',')) return false;
		}
	}

	std::string_view input_{};
	std::size_t position_ = 0;
};

void appendEscaped(std::string& output, std::string_view value) {
	output.push_back('"');
	for (const char character : value) {
		switch (character) {
		case '"': output += "\\\""; break;
		case '\\': output += "\\\\"; break;
		case '\n': output += "\\n"; break;
		case '\r': output += "\\r"; break;
		case '\t': output += "\\t"; break;
		case '\b': output += "\\b"; break;
		case '\f': output += "\\f"; break;
		default:
			if (static_cast<unsigned char>(character) < 0x20u) output += '?';
			else output.push_back(character);
		}
	}
	output.push_back('"');
}

std::string encodeJsonValue(const JsonValue& value) {
	if (value.kind == JsonValue::Kind::Null)
		return "null";
	if (value.kind == JsonValue::Kind::Boolean)
		return value.boolean ? "true" : "false";
	if (value.kind == JsonValue::Kind::Number)
		return value.text;
	std::string output;
	if (value.kind == JsonValue::Kind::String) {
		appendEscaped(output, value.text);
		return output;
	}
	const bool object = value.kind == JsonValue::Kind::Object;
	output += object ? '{' : '[';
	std::size_t index = 0;
	if (object) {
		for (const auto& member : value.object) {
			if (index++)
				output += ',';
			appendEscaped(output, member.first);
			output += ':' + encodeJsonValue(member.second);
		}
	} else {
		for (const auto& item : value.array) {
			if (index++)
				output += ',';
			output += encodeJsonValue(item);
		}
	}
	output += object ? '}' : ']';
	return output;
}

std::string compactCpp(std::string_view expression) {
	std::string output;
	output.reserve(expression.size());
	bool quoted = false, escaped = false;
	for (const char character : expression) {
		if (!quoted &&
			(character == ' ' || character == '\t' || character == '\r' || character == '\n'))
			continue;
		output += character;
		if (escaped)
			escaped = false;
		else if (quoted && character == '\\')
			escaped = true;
		else if (character == '"')
			quoted = !quoted;
	}
	return output;
}

std::vector<std::string_view> splitCppArguments(std::string_view expression) {
	std::vector<std::string_view> arguments;
	arguments.reserve(8);
	std::size_t start = 0;
	int parentheses = 0, braces = 0, angles = 0;
	bool quoted = false, escaped = false;
	for (std::size_t index = 0; index < expression.size(); ++index) {
		const char character = expression[index];
		if (escaped) {
			escaped = false;
			continue;
		}
		if (quoted && character == '\\') {
			escaped = true;
			continue;
		}
		if (character == '"') {
			quoted = !quoted;
			continue;
		}
		if (quoted)
			continue;
		if (character == '(')
			++parentheses;
		else if (character == ')')
			--parentheses;
		else if (character == '{')
			++braces;
		else if (character == '}')
			--braces;
		else if (character == '<')
			++angles;
		else if (character == '>')
			--angles;
		else if (character == ',' && parentheses == 0 && braces == 0 && angles == 0) {
			arguments.emplace_back(expression.substr(start, index - start));
			start = index + 1;
		}
	}
	if (start < expression.size())
		arguments.emplace_back(expression.substr(start));
	return arguments;
}

bool parseCppNumber(std::string_view expression, long double& number) {
	if (expression.starts_with("static_cast<")) {
		const auto opening = expression.find(">(");
		if (opening == std::string_view::npos || !expression.ends_with(')'))
			return false;
		expression = expression.substr(opening + 2, expression.size() - opening - 3);
	}
	while (!expression.empty() &&
		   std::string_view("uUlLfF").find(expression.back()) != std::string_view::npos)
		expression.remove_suffix(1);
	const auto parsed =
		std::from_chars(expression.data(), expression.data() + expression.size(), number);
	return !expression.empty() && parsed.ec == std::errc{} &&
		   parsed.ptr == expression.data() + expression.size() && std::isfinite(number);
}

std::string normalizedCpp(std::string_view expression) {
	const auto compact = compactCpp(expression);
	std::string output;
	output.reserve(compact.size());
	bool quoted = false, escaped = false;
	for (std::size_t index = 0; index < compact.size(); ++index) {
		const char character = compact[index];
		if (quoted) {
			output += character;
			if (escaped)
				escaped = false;
			else if (character == '\\')
				escaped = true;
			else if (character == '"')
				quoted = false;
			continue;
		}
		if (character == '"') {
			quoted = true;
			output += character;
			continue;
		}
		const bool starts_number =
			character >= '0' && character <= '9' &&
			(index == 0 || (!std::isalnum(static_cast<unsigned char>(compact[index - 1])) &&
							compact[index - 1] != '_'));
		if (!starts_number) {
			output += character;
			continue;
		}
		const auto start = index;
		while (index + 1 < compact.size()) {
			const char next = compact[index + 1];
			if ((next >= '0' && next <= '9') || next == '.' || next == 'e' || next == 'E' ||
				((next == '+' || next == '-') && (compact[index] == 'e' || compact[index] == 'E')))
				++index;
			else
				break;
		}
		long double number = 0;
		const auto literal = std::string_view(compact).substr(start, index - start + 1);
		if (parseCppNumber(literal, number)) {
			std::ostringstream stream;
			stream.imbue(std::locale::classic());
			stream << std::setprecision(std::numeric_limits<long double>::max_digits10) << number;
			output += stream.str();
		} else
			output += literal;
		while (index + 1 < compact.size() &&
			   std::string_view("uUlLfF").find(compact[index + 1]) != std::string_view::npos)
			++index;
	}
	return output;
}

std::string hex64(std::uint64_t value) {
	std::ostringstream stream;
	stream << "0x" << std::uppercase << std::hex << std::setw(16) << std::setfill('0') << value;
	return stream.str();
}

bool parseHex(std::string_view value, std::uint64_t& output) {
	if (value.starts_with("0x") || value.starts_with("0X")) value.remove_prefix(2);
	const auto result = std::from_chars(value.data(), value.data() + value.size(), output, 16);
	return result.ec == std::errc{} && result.ptr == value.data() + value.size();
}

std::optional<std::string> stringMember(const JsonValue& value, std::string_view name) {
	const JsonValue* member = value.find(name);
	if (!member || member->kind != JsonValue::Kind::String) return std::nullopt;
	return member->text;
}

std::string nowUtc() {
	const std::time_t value = std::chrono::system_clock::to_time_t(
		std::chrono::system_clock::now());
	std::tm utc{};
#if defined(_WIN32)
	gmtime_s(&utc, &value);
#else
	gmtime_r(&value, &utc);
#endif
	char buffer[32]{};
	std::strftime(buffer, sizeof(buffer), "%Y-%m-%dT%H:%M:%SZ", &utc);
	return buffer;
}

const tooling::DevFieldSchema* findField(
	const tooling::DevSchemaGeneration& schema,
	tooling::DevTypeIndex owner,
	tooling::DevFieldId field) {
	for (const auto& candidate : schema.fieldsOf(owner)) if (candidate.id == field) return &candidate;
	return nullptr;
}

bool validIdentifier(std::string_view value) {
	if (value.empty() || !((value[0] >= 'A' && value[0] <= 'Z') ||
		(value[0] >= 'a' && value[0] <= 'z') || value[0] == '_')) return false;
	return std::all_of(value.begin() + 1, value.end(), [](char character) {
		return (character >= 'A' && character <= 'Z') ||
			(character >= 'a' && character <= 'z') ||
			(character >= '0' && character <= '9') || character == '_';
	});
}

bool looksLikeHeader(const std::filesystem::path& path) {
	const std::string extension = path.extension().string();
	return extension == ".h" || extension == ".hh" || extension == ".hpp" ||
		extension == ".hxx" || extension == ".inl";
}

struct SerializedValue { std::string json; std::string cpp; };

std::optional<SerializedValue> serializeValue(
	const tooling::DevSchemaGeneration& schema,
	tooling::DevTypeIndex typeIndex,
	const void* value,
	std::uint32_t depth = 0) {
	if (!value || depth > 12) return std::nullopt;
	const tooling::DevTypeSchema* type = schema.type(typeIndex);
	if (!type || typeIndex.value >= schema.typeOperations.size()) return std::nullopt;
	const tooling::DevTypeOps* operations = schema.typeOperations[typeIndex.value];
	if (!operations) return std::nullopt;
	if (type->id == ::FlowUi::detail::typeHash<ActionCall>()) {
		const auto& action = *static_cast<const ActionCall*>(value);
		const ActionStableReference reference =
			action.stableReference();
		if (action && reference.kind == ActionCallKind::None) return std::nullopt;
		if (reference.kind == ActionCallKind::None || reference.id == 0u) {
			return SerializedValue{"{\"kind\":0,\"id\":0}", "FlowUi::ActionCall{}"};
		}
		if (reference.kind != ActionCallKind::App &&
			reference.kind != ActionCallKind::Ui) return std::nullopt;
		const char* kindName = reference.kind == ActionCallKind::App ? "App" : "Ui";
		return SerializedValue{
			"{\"kind\":" + std::to_string(static_cast<unsigned>(reference.kind)) +
				",\"id\":" + std::to_string(reference.id) + "}",
			"FlowUi::ActionCall::fromStable(FlowUi::ActionCallKind::" +
				std::string(kindName) + "," + std::to_string(reference.id) + "ULL)"};
	}
	if (type->id == ::FlowUi::detail::typeHash<TextureRef>()) {
		const auto& texture = *static_cast<const TextureRef*>(value);
		if (texture.sourceKey.empty() ||
			(texture.sourceDomain != ResourceDomain::Image &&
			 texture.sourceDomain != ResourceDomain::Icon) ||
			static_cast<unsigned>(texture.fitMode) > static_cast<unsigned>(TextureFitMode::None))
			return std::nullopt;
		std::string keyLiteral;
		appendEscaped(keyLiteral, texture.sourceKey);
		const std::string domain = texture.sourceDomain == ResourceDomain::Image ? "Image" : "Icon";
		return SerializedValue{
			"{\"domain\":" + std::to_string(static_cast<unsigned>(texture.sourceDomain)) +
				",\"key\":" + keyLiteral +
				",\"fitMode\":" + std::to_string(static_cast<unsigned>(texture.fitMode)) +
				",\"tintEnabled\":" + (texture.tintEnabled ? "true" : "false") + "}",
			"FlowUi::TextureRef::fromStable(FlowUi::ResourceDomain::" + domain + "," + keyLiteral +
				",static_cast<FlowUi::TextureFitMode>(" +
				std::to_string(static_cast<unsigned>(texture.fitMode)) + ")," +
				(texture.tintEnabled ? "true" : "false") + ")"};
	}
	long double numeric = 0.0L;
	if (type->kind == tooling::DevTypeKind::Boolean) {
		if (!operations->numericValue || !operations->numericValue(value, numeric)) return std::nullopt;
		return SerializedValue{numeric == 0.0L ? "false" : "true", numeric == 0.0L ? "false" : "true"};
	}
	if (type->kind == tooling::DevTypeKind::SignedInteger ||
		type->kind == tooling::DevTypeKind::UnsignedInteger ||
		type->kind == tooling::DevTypeKind::FloatingPoint ||
		type->kind == tooling::DevTypeKind::Enumeration) {
		if (!operations->numericValue || !operations->numericValue(value, numeric) ||
			!std::isfinite(numeric)) return std::nullopt;
		std::ostringstream stream;
		stream.imbue(std::locale::classic());
		stream << std::setprecision(20) << numeric;
		std::string literal = stream.str();
		if (type->kind == tooling::DevTypeKind::FloatingPoint && numeric == 0 &&
			std::signbit(numeric))
			literal = "-0.0";
		if (type->kind == tooling::DevTypeKind::Enumeration) {
			literal = "static_cast<" + std::string(schema.string(type->cppTypeName)) + ">(" + literal + ")";
		}
		return SerializedValue{stream.str(), std::move(literal)};
	}
	if (type->kind != tooling::DevTypeKind::Object || type->fields.count == 0) return std::nullopt;
	SerializedValue result{"{", std::string(schema.string(type->cppTypeName)) + "{"};
	bool first = true;
	for (const auto& field : schema.fieldsOf(typeIndex)) {
		if (field.operations >= schema.fieldOperations.size()) return std::nullopt;
		const tooling::DevFieldOps* fieldOps = schema.fieldOperations[field.operations];
		const std::string name(schema.string(field.name));
		if (!fieldOps || !fieldOps->constAddress || !validIdentifier(name)) return std::nullopt;
		const auto child = serializeValue(schema, field.valueType, fieldOps->constAddress(value), depth + 1);
		if (!child) return std::nullopt;
		if (!first) { result.json += ','; result.cpp += ','; }
		first = false;
		appendEscaped(result.json, name);
		result.json += ':' + child->json;
		result.cpp += "." + name + "=" + child->cpp;
	}
	result.json += '}';
	result.cpp += '}';
	return result;
}

struct ResolvedPath {
	std::string path{};
	std::string header{};
	const tooling::DevFieldSchema* leaf = nullptr;
};

ResolvedPath resolvePath(
	const tooling::DevSchemaGeneration& schema,
	tooling::DevTypeIndex root,
	const DevOverrideFieldKey& key) {
	ResolvedPath result;
	tooling::DevTypeIndex owner = root;
	const auto append = [&](const tooling::DevFieldSchema& field, ResolvedPath& target) {
		const std::string name(schema.string(field.name));
		if (!validIdentifier(name)) return false;
		if (!target.path.empty()) target.path += '.';
		target.path += name;
		if (target.header.empty()) target.header = std::string(schema.string(field.source.file));
		return true;
	};
	for (const auto fieldId : key.nestedPath) {
		const auto* field = findField(schema, owner, fieldId);
		if (!field || !append(*field, result)) return {};
		owner = field->valueType;
	}
	result.leaf = findField(schema, owner, key.field);
	if (!result.leaf || !append(*result.leaf, result)) return {};
	return result;
}

bool sameIdentity(const DevBakeEntry& left, const DevBakeEntry& right) {
	return left.targetKind == right.targetKind && left.targetScope == right.targetScope &&
		left.definition == right.definition && left.themeType == right.themeType &&
		left.themeVariant == right.themeVariant && left.instanceKey == right.instanceKey &&
		left.fieldId == right.fieldId && left.fieldPath == right.fieldPath;
}

bool isLive(DevOverrideLayer layer) {
	return layer == DevOverrideLayer::LiveDefinition || layer == DevOverrideLayer::LiveInstance;
}

std::optional<DevBakeEntry> makeElementEntry(
	const tooling::DevSchemaGeneration& schema,
	const DevOverrideApply::Record& record,
	DevBakeDiagnostic& diagnostic) {
	if (!isLive(record.layer) || !record.schemaValid || !record.value) return std::nullopt;
	if (record.target.scope == DevOverrideScope::ExactInstance && !record.target.bakeable) {
		diagnostic = {DevBakeDiagnosticCode::UnstableInstanceKey,
			"This instance identity is order-derived and cannot be baked safely; add an authored key such as Indexed(\"item\", id).",
			record.target.definition, record.target.instance, record.field.field};
		return std::nullopt;
	}
	const auto* element = schema.findElement(record.target.definition);
	if (!element) return std::nullopt;
	const auto* owner = schema.type(element->parametersType);
	const ResolvedPath path = resolvePath(schema, element->parametersType, record.field);
	if (!owner || !path.leaf) return std::nullopt;
	const auto serialized = serializeValue(schema, path.leaf->valueType, record.value.data());
	if (!serialized) {
		diagnostic = {DevBakeDiagnosticCode::UnsupportedValueType,
			"The edited value cannot be represented as a typed allocation-free C++ initializer.",
			record.target.definition, record.target.instance, record.field.field};
		return std::nullopt;
	}
	if (path.header.empty() || !looksLikeHeader(path.header)) {
		diagnostic = {DevBakeDiagnosticCode::MissingSourceHeader,
			"The schema field is not declared in a header; move its FLOWUI_DEV_SCHEMA declaration to a header before baking.",
			record.target.definition, record.target.instance, record.field.field};
		return std::nullopt;
	}
	const auto* valueType = schema.type(path.leaf->valueType);
	return DevBakeEntry{
		.targetKind = DevBakeTargetKind::Element,
		.targetScope = record.target.scope,
		.definition = record.target.definition,
		.instanceKey = record.target.instance,
		.instanceDebugLabel = record.target.instanceDebugLabel,
		.fieldId = record.field.field,
		.fieldPath = path.path,
		.valueType = valueType ? valueType->id : 0,
		.valueTypeName = valueType ? std::string(schema.string(valueType->cppTypeName)) : std::string{},
		.ownerCppType = std::string(schema.string(owner->cppTypeName)),
		.sourceHeader = path.header,
		.valueJson = serialized->json,
		.cppValue = serialized->cpp,
		.provenance = {.timestamp = nowUtc()},
	};
}

std::optional<DevBakeEntry> makeThemeEntry(
	const tooling::DevSchemaGeneration& schema,
	const DevOverrideEngine::ThemeBakeRecord& record,
	DevBakeDiagnostic& diagnostic) {
	if (!record.schemaValid || !record.value) return std::nullopt;
	const auto* theme = schema.findTheme(record.target.themeType);
	if (!theme) return std::nullopt;
	const auto* owner = schema.type(theme->themeType);
	const ResolvedPath path = resolvePath(schema, theme->themeType, record.field);
	if (!owner || !path.leaf) return std::nullopt;
	const auto serialized = serializeValue(schema, path.leaf->valueType, record.value.data());
	if (!serialized) {
		diagnostic = {DevBakeDiagnosticCode::UnsupportedValueType,
			"The edited theme value cannot be represented as a typed allocation-free C++ initializer.",
			{}, {}, record.field.field};
		return std::nullopt;
	}
	if (path.header.empty() || !looksLikeHeader(path.header)) {
		diagnostic = {DevBakeDiagnosticCode::MissingSourceHeader,
			"The theme schema field is not declared in a header.", {}, {}, record.field.field};
		return std::nullopt;
	}
	const auto* valueType = schema.type(path.leaf->valueType);
	return DevBakeEntry{
		.targetKind = DevBakeTargetKind::Theme,
		.targetScope = DevOverrideScope::Definition,
		.themeType = record.target.themeType,
		.themeVariant = record.target.variant,
		.fieldId = record.field.field,
		.fieldPath = path.path,
		.valueType = valueType ? valueType->id : 0,
		.valueTypeName = valueType ? std::string(schema.string(valueType->cppTypeName)) : std::string{},
		.ownerCppType = std::string(schema.string(owner->cppTypeName)),
		.sourceHeader = path.header,
		.valueJson = serialized->json,
		.cppValue = serialized->cpp,
		.provenance = {.timestamp = nowUtc()},
	};
}

bool decodeEntry(const JsonValue& value, DevBakeEntry& entry) {
	if (value.kind != JsonValue::Kind::Object) return false;
	entry.provenance.author = "unknown";
	const auto targetKind = stringMember(value, "targetKind");
	const auto targetScope = stringMember(value, "targetScope");
	const auto fieldPath = stringMember(value, "fieldPath");
	const auto fieldId = stringMember(value, "fieldId");
	const auto valueType = stringMember(value, "valueTypeId");
	const auto ownerType = stringMember(value, "ownerCppType");
	const auto sourceHeader = stringMember(value, "sourceHeader");
	const auto cppValue = stringMember(value, "cppValue");
	const auto valueJson = stringMember(value, "valueJson");
	if (!targetKind || !targetScope || !fieldPath || !fieldId || !valueType ||
		!ownerType || !sourceHeader || !cppValue || !valueJson) return false;
	if ((*targetKind != "Element" && *targetKind != "Theme") ||
		(*targetScope != "Definition" && *targetScope != "ExactInstance"))
		return false;
	entry.targetKind = *targetKind == "Theme" ? DevBakeTargetKind::Theme : DevBakeTargetKind::Element;
	entry.targetScope = *targetScope == "ExactInstance" ? DevOverrideScope::ExactInstance : DevOverrideScope::Definition;
	entry.fieldPath = *fieldPath;
	entry.ownerCppType = *ownerType;
	entry.sourceHeader = *sourceHeader;
	entry.cppValue = *cppValue;
	entry.valueJson = *valueJson;
	if (!parseHex(*fieldId, entry.fieldId) || !parseHex(*valueType, entry.valueType)) return false;
	if (const auto name = stringMember(value, "valueType")) entry.valueTypeName = *name;
	if (const auto id = stringMember(value, "definitionId")) {
		if (!parseHex(*id, entry.definition.value)) return false;
	}
	if (const auto id = stringMember(value, "themeType")) {
		if (!parseHex(*id, entry.themeType)) return false;
	}
	if (const auto variant = stringMember(value, "themeVariant")) entry.themeVariant = *variant;
	if (const auto key = stringMember(value, "instanceKey")) {
		if (!parseHex(*key, entry.instanceKey.value)) return false;
	}
	if (const auto label = stringMember(value, "instanceDebugLabel")) entry.instanceDebugLabel = *label;
	if (const auto* provenance = value.find("provenance")) {
		if (provenance->kind != JsonValue::Kind::Object)
			return false;
		if (const auto author = stringMember(*provenance, "author"))
			entry.provenance.author = *author;
		if (const auto note = stringMember(*provenance, "note"))
			entry.provenance.note = *note;
		if (const auto timestamp = stringMember(*provenance, "timestamp"))
			entry.provenance.timestamp = *timestamp;
		for (const auto& member : provenance->object)
			if (member.first != "author" && member.first != "note" && member.first != "timestamp")
				entry.provenance.preserved_metadata.emplace_back(member.first,
																 encodeJsonValue(member.second));
	} else {
		entry.provenance.author = "unknown";
	}
	constexpr std::array<std::string_view, 17> known_members{
		"targetKind",	"targetScope",	"definitionId",		  "themeType",
		"themeVariant", "instanceKey",	"instanceDebugLabel", "fieldId",
		"fieldPath",	"valueTypeId",	"valueType",		  "value",
		"ownerCppType", "sourceHeader", "cppValue",			  "valueJson",
		"provenance"};
	for (const auto& member : value.object) {
		if (std::find(known_members.begin(), known_members.end(), member.first) ==
			known_members.end())
			entry.preserved_metadata.emplace_back(member.first, encodeJsonValue(member.second));
	}
	return true;
}

bool decodeManifest(std::string_view contents, DevBakeManifest& manifest,
					std::string* diagnostic = nullptr) {
	JsonValue root;
	if (!JsonParser(contents).parse(root) || root.kind != JsonValue::Kind::Object) return false;
	const JsonValue* version = root.find("manifestVersion");
	if (!version || version->kind != JsonValue::Kind::Number ||
		(version->text != "1" && version->text != "2"))
		return false;
	manifest = {};
	manifest.manifestVersion = version->text == "1" ? 1 : 2;
	if (const auto* memory = root.find("memoryCapacity")) {
		if (manifest.manifestVersion != 2)
			return false;
		FlowUi::detail::MemoryCapacityManifest decoded;
		std::string error;
		if (!FlowUi::detail::decode_memory_capacity_manifest(*memory, decoded, error)) {
			if (diagnostic)
				*diagnostic = error;
			return false;
		}
		manifest.memoryCapacity = std::move(decoded);
	}
	if (const auto fingerprint = stringMember(root, "schemaFingerprint")) {
		if (!parseHex(*fingerprint, manifest.schemaFingerprint)) return false;
	}
	if (const auto build = stringMember(root, "buildFingerprint")) manifest.buildFingerprint = *build;
	if (const auto created = stringMember(root, "createdTimestamp")) manifest.createdTimestamp = *created;
	const JsonValue* entries = root.find("bakedChanges");
	if (!entries || entries->kind != JsonValue::Kind::Array) return false;
	for (const JsonValue& item : entries->array) {
		DevBakeEntry entry;
		if (!decodeEntry(item, entry)) return false;
		manifest.entries.push_back(std::move(entry));
	}
	constexpr std::array<std::string_view, 6> known_members{"manifestVersion",	"schemaFingerprint",
															"buildFingerprint", "createdTimestamp",
															"bakedChanges",		"memoryCapacity"};
	for (const auto& member : root.object) {
		if (std::find(known_members.begin(), known_members.end(), member.first) ==
			known_members.end())
			manifest.preserved_metadata.emplace_back(member.first, encodeJsonValue(member.second));
	}
	return true;
}

bool loadManifest(const std::filesystem::path& path, DevBakeManifest& manifest, bool& existed) {
	existed = std::filesystem::exists(path);
	if (!existed) {
		manifest = {};
		return true;
	}
	std::ifstream input(path, std::ios::binary);
	if (!input)
		return false;
	const std::string contents((std::istreambuf_iterator<char>(input)), {});
	return !input.bad() && decodeManifest(contents, manifest);
}

std::string encodeManifest(const DevBakeManifest& manifest) {
	std::string output = "{\n  \"manifestVersion\": " +
						 std::to_string(manifest.memoryCapacity ? 2 : manifest.manifestVersion) +
						 ",\n  \"schemaFingerprint\": ";
	appendEscaped(output, hex64(manifest.schemaFingerprint));
	output += ",\n  \"buildFingerprint\": "; appendEscaped(output, manifest.buildFingerprint);
	output += ",\n  \"createdTimestamp\": "; appendEscaped(output, manifest.createdTimestamp);
	output += ",\n  \"bakedChanges\": [";
	for (std::size_t index = 0; index < manifest.entries.size(); ++index) {
		const auto& entry = manifest.entries[index];
		output += index == 0 ? "\n" : ",\n";
		output += "    {\n      \"targetKind\": ";
		appendEscaped(output, entry.targetKind == DevBakeTargetKind::Theme ? "Theme" : "Element");
		output += ",\n      \"targetScope\": ";
		appendEscaped(output, entry.targetScope == DevOverrideScope::ExactInstance ? "ExactInstance" : "Definition");
		if (entry.targetKind == DevBakeTargetKind::Element) {
			output += ",\n      \"definitionId\": "; appendEscaped(output, hex64(entry.definition.value));
			if (entry.targetScope == DevOverrideScope::ExactInstance) {
				output += ",\n      \"instanceKey\": "; appendEscaped(output, hex64(entry.instanceKey.value));
				output += ",\n      \"instanceDebugLabel\": "; appendEscaped(output, entry.instanceDebugLabel);
			}
		} else {
			output += ",\n      \"themeType\": "; appendEscaped(output, hex64(entry.themeType));
			output += ",\n      \"themeVariant\": "; appendEscaped(output, entry.themeVariant);
		}
		output += ",\n      \"fieldPath\": "; appendEscaped(output, entry.fieldPath);
		output += ",\n      \"fieldId\": "; appendEscaped(output, hex64(entry.fieldId));
		output += ",\n      \"valueType\": "; appendEscaped(output, entry.valueTypeName);
		output += ",\n      \"valueTypeId\": "; appendEscaped(output, hex64(entry.valueType));
		output += ",\n      \"value\": " + entry.valueJson;
		output += ",\n      \"ownerCppType\": "; appendEscaped(output, entry.ownerCppType);
		output += ",\n      \"sourceHeader\": "; appendEscaped(output, entry.sourceHeader);
		output += ",\n      \"cppValue\": "; appendEscaped(output, entry.cppValue);
		output += ",\n      \"valueJson\": "; appendEscaped(output, entry.valueJson);
		output += ",\n      \"provenance\": {\"author\": "; appendEscaped(output, entry.provenance.author);
		output += ", \"note\": "; appendEscaped(output, entry.provenance.note);
		output += ", \"timestamp\": "; appendEscaped(output, entry.provenance.timestamp);
		for (const auto& member : entry.provenance.preserved_metadata) {
			output += ", ";
			appendEscaped(output, member.first);
			output += ": " + member.second;
		}
		output += '}';
		for (const auto& member : entry.preserved_metadata) {
			output += ",\n      ";
			appendEscaped(output, member.first);
			output += ": " + member.second;
		}
		output += "\n    }";
	}
	if (!manifest.entries.empty()) output += '\n';
	output += "  ]";
	if (manifest.memoryCapacity)
		output += ",\n  \"memoryCapacity\": " +
				  FlowUi::detail::encode_memory_capacity_manifest(*manifest.memoryCapacity);
	for (const auto& member : manifest.preserved_metadata) {
		output += ",\n  ";
		appendEscaped(output, member.first);
		output += ": " + member.second;
	}
	output += "\n}\n";
	return output;
}

bool writeAtomically(const std::filesystem::path& destination, std::string_view contents) {
	std::error_code error;
	if (!destination.parent_path().empty()) std::filesystem::create_directories(destination.parent_path(), error);
	if (error) return false;
	std::filesystem::path temporary = destination;
	temporary += ".tmp";
	{
		std::ofstream output(temporary, std::ios::binary | std::ios::trunc);
		if (!output) return false;
		output.write(contents.data(), static_cast<std::streamsize>(contents.size()));
		output.flush();
		if (!output) return false;
		output.close();
		if (output.fail())
			return false;
	}
	std::filesystem::rename(temporary, destination, error);
	if (!error) return true;
	std::error_code cleanup_error;
	std::filesystem::remove(temporary, cleanup_error);
	return false;
}

} // namespace

namespace bake_detail {
std::string utc_timestamp() {
	return nowUtc();
}

bool entry_cpp_value_consistent(const tooling::DevSchemaGeneration& schema,
								const tooling::DevFieldSchema& field, const DevBakeEntry& entry) {
	JsonValue value;
	if (!JsonParser(entry.valueJson).parse(value))
		return false;
	const auto expression = compactCpp(entry.cppValue);
	const auto json_number = [](const JsonValue* candidate, long double& number) {
		return candidate && candidate->kind == JsonValue::Kind::Number &&
			   parseCppNumber(candidate->text, number);
	};
	const auto json_unsigned_integer = [](const JsonValue* candidate,
										  std::uint64_t& number) noexcept {
		if (!candidate || candidate->kind != JsonValue::Kind::Number)
			return false;
		const auto parsed = std::from_chars(
			candidate->text.data(), candidate->text.data() + candidate->text.size(), number);
		return parsed.ec == std::errc{} &&
			   parsed.ptr == candidate->text.data() + candidate->text.size();
	};
	const auto compare = [&](const auto& self, const JsonValue& candidate,
							 tooling::DevTypeIndex type_index, std::string_view cpp,
							 std::uint32_t depth) -> bool {
		const auto* type = schema.type(type_index);
		if (!type || depth > 16)
			return false;
		const auto type_name = compactCpp(schema.string(type->cppTypeName));
		if (type->id == ::FlowUi::detail::typeHash<ActionCall>()) {
			long double kind = 0, identifier = 0;
			if (!json_number(candidate.find("kind"), kind) ||
				!json_number(candidate.find("id"), identifier))
				return false;
			if (kind == 0 && identifier == 0)
				return cpp == "FlowUi::ActionCall{}";
			constexpr std::string_view prefix = "FlowUi::ActionCall::fromStable(";
			if ((kind != 1 && kind != 2) || identifier <= 0 || !cpp.starts_with(prefix) ||
				!cpp.ends_with(')'))
				return false;
			const auto arguments =
				splitCppArguments(cpp.substr(prefix.size(), cpp.size() - prefix.size() - 1));
			long double generated_id = 0;
			return arguments.size() == 2 &&
				   arguments[0] ==
					   (kind == 1 ? "FlowUi::ActionCallKind::App" : "FlowUi::ActionCallKind::Ui") &&
				   parseCppNumber(arguments[1], generated_id) && generated_id == identifier;
		}
		if (type->id == ::FlowUi::detail::typeHash<TextureRef>()) {
			constexpr std::string_view prefix = "FlowUi::TextureRef::fromStable(";
			if (candidate.kind != JsonValue::Kind::Object || candidate.object.size() != 4 ||
				!cpp.starts_with(prefix) || !cpp.ends_with(')'))
				return false;
			const auto arguments =
				splitCppArguments(cpp.substr(prefix.size(), cpp.size() - prefix.size() - 1));
			std::uint64_t domain = 0;
			const auto* key = candidate.find("key");
			JsonValue generated_key;
			if (arguments.size() != 4 || !json_unsigned_integer(candidate.find("domain"), domain) ||
				(domain != 1 && domain != 2) || !key || key->kind != JsonValue::Kind::String ||
				key->text.empty() ||
				arguments[0] != (domain == 1 ? "FlowUi::ResourceDomain::Image"
											 : "FlowUi::ResourceDomain::Icon") ||
				!JsonParser(arguments[1]).parse(generated_key) ||
				generated_key.kind != JsonValue::Kind::String || generated_key.text != key->text)
				return false;
			long double generated_fit = 0;
			if (!arguments[2].starts_with("static_cast<FlowUi::TextureFitMode>(") ||
				!parseCppNumber(arguments[2], generated_fit) || generated_fit < 0 ||
				generated_fit > 3 || std::floor(generated_fit) != generated_fit ||
				(arguments[3] != "true" && arguments[3] != "false"))
				return false;
			std::uint64_t saved_setting = 0;
			if (!json_unsigned_integer(candidate.find("fitMode"), saved_setting) ||
				saved_setting != generated_fit)
				return false;
			const auto* tint = candidate.find("tintEnabled");
			if (!tint || tint->kind != JsonValue::Kind::Boolean ||
				tint->boolean != (arguments[3] == "true"))
				return false;
			return true;
		}
		if (type->kind == tooling::DevTypeKind::Boolean)
			return candidate.kind == JsonValue::Kind::Boolean &&
				   cpp == (candidate.boolean ? "true" : "false");
		if (type->kind == tooling::DevTypeKind::SignedInteger ||
			type->kind == tooling::DevTypeKind::UnsignedInteger ||
			type->kind == tooling::DevTypeKind::FloatingPoint ||
			type->kind == tooling::DevTypeKind::Enumeration) {
			long double saved_number = 0, generated_number = 0;
			if (!json_number(std::addressof(candidate), saved_number) ||
				!parseCppNumber(cpp, generated_number))
				return false;
			if (type->kind == tooling::DevTypeKind::Enumeration &&
				!cpp.starts_with("static_cast<" + type_name + ">("))
				return false;
			if (type->kind != tooling::DevTypeKind::Enumeration && cpp.starts_with("static_cast<"))
				return false;
			if (type->kind == tooling::DevTypeKind::FloatingPoint) {
				if (saved_number == 0 && std::signbit(saved_number) &&
					cpp.find_first_of(".eE") == std::string_view::npos)
					return false;
				if (type->size <= sizeof(float)) {
					saved_number = static_cast<float>(saved_number);
					generated_number = static_cast<float>(generated_number);
				} else if (type->size <= sizeof(double)) {
					saved_number = static_cast<double>(saved_number);
					generated_number = static_cast<double>(generated_number);
				}
			}
			return saved_number == generated_number &&
				   std::signbit(saved_number) == std::signbit(generated_number);
		}
		if (type->kind != tooling::DevTypeKind::Object ||
			candidate.kind != JsonValue::Kind::Object || !cpp.starts_with(type_name + "{") ||
			!cpp.ends_with('}'))
			return false;
		const auto assignments =
			splitCppArguments(cpp.substr(type_name.size() + 1, cpp.size() - type_name.size() - 2));
		if (assignments.size() != candidate.object.size())
			return false;
		std::uint32_t previous_order = 0;
		bool first_assignment = true;
		for (const auto assignment : assignments) {
			const auto equal_sign = assignment.find('=');
			if (equal_sign == std::string_view::npos || !assignment.starts_with('.'))
				return false;
			const auto name = assignment.substr(1, equal_sign - 1);
			const auto* member = candidate.find(name);
			const auto fields = schema.fieldsOf(type_index);
			const auto found = std::find_if(fields.begin(), fields.end(), [&](const auto& child) {
				return schema.string(child.name) == name;
			});
			if (!member || found == fields.end() ||
				!self(self, *member, found->valueType, assignment.substr(equal_sign + 1),
					  depth + 1))
				return false;
			if (!first_assignment && found->declarationOrder <= previous_order)
				return false;
			previous_order = found->declarationOrder;
			first_assignment = false;
		}
		return true;
	};
	return compare(compare, value, field.valueType, expression, 0);
}

bool entry_value_valid(const tooling::DevSchemaGeneration& schema,
					   const tooling::DevFieldSchema& field, const DevBakeEntry& entry,
					   std::string& reason) {
	JsonValue value;
	if (!JsonParser(entry.valueJson).parse(value)) {
		reason = "Saved valueJson is not valid JSON.";
		return false;
	}
	const auto validate = [&](const auto& self, const JsonValue& candidate,
							  tooling::DevTypeIndex type_index,
							  const tooling::DevFieldSchema* member, std::uint32_t depth) -> bool {
		const auto* type = schema.type(type_index);
		if (!type || depth > 16)
			return false;
		long double numeric = 0;
		const bool numeric_type = type->kind == tooling::DevTypeKind::SignedInteger ||
								  type->kind == tooling::DevTypeKind::UnsignedInteger ||
								  type->kind == tooling::DevTypeKind::FloatingPoint ||
								  type->kind == tooling::DevTypeKind::Enumeration;
		if (numeric_type) {
			if (candidate.kind != JsonValue::Kind::Number)
				return false;
			const auto parsed = std::from_chars(
				candidate.text.data(), candidate.text.data() + candidate.text.size(), numeric);
			if (parsed.ec != std::errc{} ||
				parsed.ptr != candidate.text.data() + candidate.text.size() ||
				!std::isfinite(numeric))
				return false;
			if (type->kind != tooling::DevTypeKind::FloatingPoint && std::floor(numeric) != numeric)
				return false;
			if (type->kind == tooling::DevTypeKind::UnsignedInteger &&
				(numeric < 0 || numeric >= std::ldexp(1.0L, static_cast<int>(type->size * 8))))
				return false;
			if (type->kind == tooling::DevTypeKind::SignedInteger) {
				const auto bound = std::ldexp(1.0L, static_cast<int>(type->size * 8 - 1));
				if (numeric < -bound || numeric >= bound)
					return false;
			}
			if (type->kind == tooling::DevTypeKind::FloatingPoint &&
				std::abs(numeric) >
					(type->size <= sizeof(float)
						 ? static_cast<long double>(std::numeric_limits<float>::max())
						 : static_cast<long double>(std::numeric_limits<double>::max())))
				return false;
			if (type->kind == tooling::DevTypeKind::Enumeration && type->enumeration.values.count) {
				bool matched = false;
				for (std::uint32_t index = 0; index < type->enumeration.values.count; ++index) {
					const auto enum_index = type->enumeration.values.first + index;
					if (enum_index >= schema.enumValues.size())
						return false;
					const auto bits = schema.enumValues[enum_index].bits;
					auto enumerator =
						type->enumeration.isSigned
							? static_cast<long double>(static_cast<std::int64_t>(bits))
							: static_cast<long double>(bits);
					const auto enum_width = static_cast<unsigned>(type->enumeration.widthBytes * 8);
					if (type->enumeration.isSigned && enum_width > 0 && enum_width < 64 &&
						(bits & (std::uint64_t{1} << (enum_width - 1))))
						enumerator -= std::ldexp(1.0L, static_cast<int>(enum_width));
					matched |= enumerator == numeric;
				}
				if (!matched)
					return false;
			}
		} else if (type->kind == tooling::DevTypeKind::Boolean) {
			if (candidate.kind != JsonValue::Kind::Boolean)
				return false;
		} else if (type->kind == tooling::DevTypeKind::Text) {
			if (candidate.kind != JsonValue::Kind::String)
				return false;
		} else if (type->kind == tooling::DevTypeKind::Object ||
				   type->id == ::FlowUi::detail::typeHash<ActionCall>()) {
			if (candidate.kind != JsonValue::Kind::Object)
				return false;
			if (type->id != ::FlowUi::detail::typeHash<ActionCall>() &&
				type->id != ::FlowUi::detail::typeHash<TextureRef>()) {
				for (const auto& property : candidate.object) {
					const auto fields = schema.fieldsOf(type_index);
					const auto found =
						std::find_if(fields.begin(), fields.end(), [&](const auto& child) {
							return schema.string(child.name) == property.first;
						});
					if (found == fields.end() || !self(self, property.second, found->valueType,
													   std::addressof(*found), depth + 1))
						return false;
				}
			}
		} else
			return false;
		if (member && member->constraint) {
			if (member->constraint >= schema.constraints.size())
				return false;
			const auto& constraint = schema.constraints[member->constraint];
			if (constraint.hasTextMaximum && (candidate.kind != JsonValue::Kind::String ||
											  candidate.text.size() > constraint.textMaximum))
				return false;
			if (constraint.numeric.hasMinimum &&
				(!numeric_type || numeric < constraint.numeric.minimum))
				return false;
			if (constraint.numeric.hasMaximum &&
				(!numeric_type || numeric > constraint.numeric.maximum))
				return false;
			if (constraint.numeric.hasStep) {
				const auto step = static_cast<long double>(constraint.numeric.step);
				if (!numeric_type || !std::isfinite(step) || step <= 0)
					return false;
				const auto steps =
					(numeric - (constraint.numeric.hasMinimum ? constraint.numeric.minimum : 0)) /
					step;
				if (std::abs(steps - std::round(steps)) > 1.0e-9L * std::max(1.0L, std::abs(steps)))
					return false;
			}
		}
		return true;
	};
	if (!validate(validate, value, field.valueType, std::addressof(field), 0)) {
		reason = "Saved value does not match the current field shape, enum, range or constraints.";
		return false;
	}
	if (!entry_cpp_value_consistent(schema, field, entry)) {
		reason = "Saved JSON and generated C++ describe different values or an unsupported "
				 "initializer; explicitly replace or remove this assignment.";
		return false;
	}
	return true;
}

bool entry_values_equal(const DevBakeEntry& left, const DevBakeEntry& right) {
	if (left.valueType != right.valueType)
		return false;
	if (normalizedCpp(left.cppValue) != normalizedCpp(right.cppValue))
		return false;
	JsonValue left_value, right_value;
	if (!JsonParser(left.valueJson).parse(left_value) ||
		!JsonParser(right.valueJson).parse(right_value))
		return false;
	const auto compare = [](const auto& self, const JsonValue& first,
							const JsonValue& second) -> bool {
		if (first.kind != second.kind)
			return false;
		if (first.kind == JsonValue::Kind::Null)
			return true;
		if (first.kind == JsonValue::Kind::Boolean)
			return first.boolean == second.boolean;
		if (first.kind == JsonValue::Kind::String)
			return first.text == second.text;
		if (first.kind == JsonValue::Kind::Number) {
			long double first_number = 0, second_number = 0;
			const auto first_parsed = std::from_chars(
				first.text.data(), first.text.data() + first.text.size(), first_number);
			const auto second_parsed = std::from_chars(
				second.text.data(), second.text.data() + second.text.size(), second_number);
			return first_parsed.ec == std::errc{} && second_parsed.ec == std::errc{} &&
				   first_parsed.ptr == first.text.data() + first.text.size() &&
				   second_parsed.ptr == second.text.data() + second.text.size() &&
				   first_number == second_number;
		}
		if (first.kind == JsonValue::Kind::Array) {
			if (first.array.size() != second.array.size())
				return false;
			for (std::size_t index = 0; index < first.array.size(); ++index)
				if (!self(self, first.array[index], second.array[index]))
					return false;
			return true;
		}
		if (first.object.size() != second.object.size())
			return false;
		for (const auto& member : first.object) {
			const auto* other = second.find(member.first);
			if (!other || !self(self, member.second, *other))
				return false;
		}
		return true;
	};
	return compare(compare, left_value, right_value);
}
bool decode_manifest(std::string_view contents, DevBakeManifest& manifest) {
	return decodeManifest(contents, manifest);
}
bool read_manifest_snapshot(const std::filesystem::path& path, DevBakeManifest& manifest,
							std::string& contents, bool& existed, std::string* diagnostic) {
	std::error_code error;
	existed = std::filesystem::exists(path, error);
	if (error)
		return false;
	if (!existed) {
		manifest = {};
		contents.clear();
		return true;
	}
	std::ifstream input(path, std::ios::binary);
	if (!input)
		return false;
	contents.assign(std::istreambuf_iterator<char>(input), {});
	return !input.bad() && decodeManifest(contents, manifest, diagnostic);
}
std::string encode_manifest(const DevBakeManifest& manifest) {
	return encodeManifest(manifest);
}
bool write_manifest_atomically(const std::filesystem::path& destination,
							   std::string_view contents) {
	return writeAtomically(destination, contents);
}
std::optional<DevBakeEntry> element_proposal(const tooling::DevSchemaGeneration& schema,
											 const DevOverrideApply::Record& record,
											 DevBakeDiagnostic& diagnostic) {
	return makeElementEntry(schema, record, diagnostic);
}
std::optional<DevBakeEntry> theme_proposal(const tooling::DevSchemaGeneration& schema,
										   const DevOverrideEngine::ThemeBakeRecord& record,
										   DevBakeDiagnostic& diagnostic) {
	return makeThemeEntry(schema, record, diagnostic);
}
} // namespace bake_detail

std::optional<DevSerializedValue> serialize_dev_value(const tooling::DevSchemaGeneration& schema,
													  tooling::DevTypeIndex type,
													  const void* value) noexcept {
	try {
		if (const auto formatted = serializeValue(schema, type, value))
			return DevSerializedValue{formatted->json, formatted->cpp};
	} catch (...) {
		return std::nullopt;
	}
	return std::nullopt;
}

DevValueCaptureStatus query_authored_element_value(
	const DevOverrideEngine& overrides, const tooling::DevSchemaGeneration& schema,
	const DevElementOverrideTarget& target, const DevOverrideFieldKey& field,
	DevOwnedValue& destination) noexcept {
	destination.reset();
	const auto* element = schema.findElement(target.definition);
	if (!element || !overrides.value_collection())
		return DevValueCaptureStatus::Unavailable;
	const auto* parameters_type = schema.type(element->parametersType);
	if (!parameters_type || parameters_type->id != field.ownerType)
		return DevValueCaptureStatus::SchemaMismatch;
	try {
		struct QueryContext {
			std::vector<const tooling::DevFieldSchema*> path;
			std::string serialized;
			const tooling::DevSchemaGeneration* schema;
			const DevElementOverrideTarget* target;
			DevOwnedValue* destination;
			DevValueCaptureStatus failure = DevValueCaptureStatus::Available;
			bool found = false;
		} context{{}, {}, &schema, &target, &destination};
		context.path.reserve(field.nestedPath.size() + 1);
		auto owner = element->parametersType;
		const auto append_field = [&](tooling::DevFieldId identity) {
			const auto fields = schema.fieldsOf(owner);
			const auto selected = std::find_if(fields.begin(), fields.end(),
				[&](const auto& candidate) { return candidate.id == identity; });
			if (selected == fields.end())
				return false;
			context.path.emplace_back(&*selected);
			owner = selected->valueType;
			return true;
		};
		for (const auto identity : field.nestedPath)
			if (!append_field(identity))
				return DevValueCaptureStatus::SchemaMismatch;
		if (!append_field(field.field))
			return DevValueCaptureStatus::SchemaMismatch;
		overrides.visitElementSnapshots(&context,
			[](void* pointer, const DevElementCaptureSnapshot& snapshot) noexcept {
				auto& query = *static_cast<QueryContext*>(pointer);
				if (query.target->window != InvalidWindowId &&
					query.target->window != snapshot.window)
					return true;
				try {
					for (const auto& captured_element : snapshot.elements) {
						if (captured_element.isDevInternal ||
							captured_element.definition != query.target->definition ||
							(query.target->scope == DevOverrideScope::ExactInstance &&
							 captured_element.instance != query.target->instance))
							continue;
						if (!snapshot.schema || snapshot.schema->generation != query.schema->generation) {
							query.failure = DevValueCaptureStatus::SchemaMismatch;
							continue;
						}
						const DevCapturedField* root = nullptr;
						const auto end = std::min<std::size_t>(snapshot.fields.size(),
							captured_element.firstField + captured_element.fieldCount);
						for (std::size_t index = captured_element.firstField; index < end; ++index) {
							const auto& candidate = snapshot.fields[index];
							if (candidate.field && candidate.field.value <= query.schema->fields.size() &&
								query.schema->fields[candidate.field.value - 1].id == query.path.front()->id) {
								root = &candidate;
								break;
							}
						}
						if (!root || root->authoredStatus != DevValueCaptureStatus::Available ||
							!root->authoredValue) {
							query.failure = root ? root->authoredStatus : DevValueCaptureStatus::Unavailable;
							continue;
						}
						const void* value = root->authoredValue.data();
						for (std::size_t index = 1; index < query.path.size() && value; ++index) {
							const auto operation_index = query.path[index]->operations;
							const auto* operations = operation_index < query.schema->fieldOperations.size()
								? query.schema->fieldOperations[operation_index] : nullptr;
							value = operations && operations->constAddress ? operations->constAddress(value) : nullptr;
						}
						const auto type = query.path.back()->valueType;
						const auto serialized = serialize_dev_value(*query.schema, type, value);
						if (!serialized) {
							query.failure = DevValueCaptureStatus::Unsupported;
							continue;
						}
						if (query.found && query.serialized != serialized->json) {
							query.failure = DevValueCaptureStatus::Ambiguous;
							return false;
						}
						if (!query.found) {
							if (DevOwnedValue::copyFrom(*query.schema, type, value, *query.destination) !=
								tooling::DevValueOperationStatus::Success) {
								query.failure = DevValueCaptureStatus::CopyFailed;
								return false;
							}
							query.serialized = serialized->json;
							query.found = true;
						}
					}
					return true;
				} catch (...) {
					query.failure = DevValueCaptureStatus::CopyFailed;
					return false;
				}
			});
		if (context.failure != DevValueCaptureStatus::Available || !context.found) {
			destination.reset();
			return context.failure == DevValueCaptureStatus::Available
				? DevValueCaptureStatus::Unavailable : context.failure;
		}
		return DevValueCaptureStatus::Available;
	} catch (...) {
		destination.reset();
		return DevValueCaptureStatus::CopyFailed;
	}
}

DevValueCaptureStatus query_authored_element_value(
	const DevOverrideEngine& overrides, const tooling::DevSchemaGeneration& schema,
	const DevBakeEntry& entry, WindowId window, DevOwnedValue& destination) noexcept {
	destination.reset();
	const auto* element = schema.findElement(entry.definition);
	if (entry.targetKind != DevBakeTargetKind::Element || !element)
		return DevValueCaptureStatus::Unavailable;
	try {
		auto owner = element->parametersType;
		const auto* root = schema.type(owner);
		if (!root)
			return DevValueCaptureStatus::SchemaMismatch;
		DevOverrideFieldKey key{.ownerType = root->id};
		key.nestedPath.reserve(8);
		std::string_view remaining = entry.fieldPath;
		while (!remaining.empty()) {
			const auto separator = remaining.find('.');
			const auto name = remaining.substr(0, separator);
			const auto fields = schema.fieldsOf(owner);
			const auto field = std::find_if(fields.begin(), fields.end(),
				[&](const auto& candidate) { return schema.string(candidate.name) == name; });
			if (field == fields.end())
				return DevValueCaptureStatus::SchemaMismatch;
			if (separator == std::string_view::npos) {
				key.field = field->id;
				const auto* value_type = schema.type(field->valueType);
				if (key.field != entry.fieldId || !value_type || value_type->id != entry.valueType)
					return DevValueCaptureStatus::SchemaMismatch;
				const DevElementOverrideTarget target{.definition = entry.definition,
					.window = window, .instance = entry.instanceKey, .scope = entry.targetScope};
				return query_authored_element_value(overrides, schema, target, key, destination);
			}
			key.nestedPath.emplace_back(field->id);
			owner = field->valueType;
			remaining.remove_prefix(separator + 1);
		}
		return DevValueCaptureStatus::SchemaMismatch;
	} catch (...) {
		return DevValueCaptureStatus::CopyFailed;
	}
}

DevBakePipeline::DevBakePipeline(
	tooling::DevSchemaRegistry& schemas,
	DevOverrideEngine& overrides) noexcept
	: schemas_(schemas), overrides_(overrides) {
	try {
		bool existed = false;
		(void)loadManifest(manifestPath_, manifest_, existed);
	} catch (...) {}
}

void DevBakePipeline::setManifestPath(std::filesystem::path path) {
	if (path.empty()) return;
	manifestPath_ = std::move(path);
	bool existed = false;
	DevBakeManifest loaded;
	if (loadManifest(manifestPath_, loaded, existed)) {
		manifest_ = std::move(loaded);
	} else {
		status_.bakeDiagnostics.push_back({
			DevBakeDiagnosticCode::ManifestInvalid,
			"The configured .flowchanges manifest is invalid or unreadable."});
	}
}

DevCommandResult
DevBakePipeline::save_memory_capacities(const FlowUi::detail::MemoryCapacityManifest& selections,
										std::span<const std::string_view> removed,
										const std::filesystem::path& review_path) noexcept {
	status_ = {};
	try {
		std::string diagnostic;
		if (!FlowUi::detail::validate_memory_capacity_manifest(selections, diagnostic)) {
			status_.bakeDiagnostics.push_back({DevBakeDiagnosticCode::ManifestInvalid, diagnostic});
			return {.status = DevCommandStatus::BakeManifestInvalid};
		}
		const auto& destination = review_path.empty() ? manifestPath_ : review_path;
		bake_detail::ManifestWriterLock writer_lock(destination);
		if (!writer_lock.acquired()) {
			status_.bakeDiagnostics.emplace_back(DevBakeDiagnostic{
				DevBakeDiagnosticCode::ManifestWriteFailed,
				"Another manifest writer holds " + destination.string() + ".lock."});
			return {.status = DevCommandStatus::BakeWriteFailed};
		}
		bool existed = false;
		DevBakeManifest updated;
		if (!loadManifest(manifestPath_, updated, existed)) {
			status_.bakeDiagnostics.push_back({DevBakeDiagnosticCode::ManifestInvalid,
											   "Invalid manifest: " + manifestPath_.string()});
			return {.status = DevCommandStatus::BakeManifestInvalid};
		}
		if (!updated.memoryCapacity)
			updated.memoryCapacity.emplace();
		auto& section = *updated.memoryCapacity;
		section.enabled = selections.enabled;
		if (selections.growth_factor)
			section.growth_factor = selections.growth_factor;
		if (selections.allow_runtime_growth)
			section.allow_runtime_growth = selections.allow_runtime_growth;
		for (const auto key : removed) {
			const auto* definition =
				FlowUi::detail::find_initial_capacity(FlowUi::memory_capacity_target_id(key));
			if (!definition || definition->key != key) {
				status_.bakeDiagnostics.push_back({DevBakeDiagnosticCode::ManifestInvalid,
												   "Unknown remove target: " + std::string(key)});
				return {.status = DevCommandStatus::BakeManifestInvalid};
			}
			std::erase_if(section.targets, [&](const auto& target) { return target.key == key; });
		}
		for (const auto& target : selections.targets) {
			const auto found =
				std::find_if(section.targets.begin(), section.targets.end(),
							 [&](const auto& current) { return current.key == target.key; });
			if (found == section.targets.end())
				section.targets.emplace_back(target);
			else
				*found = target;
		}
		if (!FlowUi::detail::validate_memory_capacity_manifest(section, diagnostic)) {
			status_.bakeDiagnostics.push_back({DevBakeDiagnosticCode::ManifestInvalid, diagnostic});
			return {.status = DevCommandStatus::BakeManifestInvalid};
		}
		updated.manifestVersion = 2;
		updated.createdTimestamp = nowUtc();
		if (!writeAtomically(destination, encodeManifest(updated))) {
			status_.bakeDiagnostics.push_back({DevBakeDiagnosticCode::ManifestWriteFailed,
											   "Cannot replace manifest: " + destination.string()});
			return {.status = DevCommandStatus::BakeWriteFailed};
		}
		if (review_path.empty())
			manifest_ = std::move(updated);
		return {.status = DevCommandStatus::Applied, .applied = true};
	} catch (...) {
		return {.status = DevCommandStatus::BakeWriteFailed};
	}
}
DevCommandResult DevBakePipeline::reset_memory_capacities() noexcept {
	try {
		bake_detail::ManifestWriterLock writer_lock(manifestPath_);
		if (!writer_lock.acquired())
			return {.status = DevCommandStatus::BakeWriteFailed};
		bool existed = false;
		DevBakeManifest updated;
		if (!loadManifest(manifestPath_, updated, existed))
			return {.status = DevCommandStatus::BakeManifestInvalid};
		updated.memoryCapacity.reset();
		if (!writeAtomically(manifestPath_, encodeManifest(updated)))
			return {.status = DevCommandStatus::BakeWriteFailed};
		manifest_ = std::move(updated);
		return {.status = DevCommandStatus::Applied, .applied = true};
	} catch (...) {
		return {.status = DevCommandStatus::BakeWriteFailed};
	}
}

DevCommandResult DevBakePipeline::bake() noexcept {
	const auto plan = query_review();
	return commit_review(plan);
}

DevBakeStatusSnapshot DevBakePipeline::queryStatus() const noexcept {
	try {
		DevBakeStatusSnapshot result{};
		const tooling::DevSchemaView schema = schemas_.view();
		if (!schema) {
			result.bakeDiagnostics.push_back({DevBakeDiagnosticCode::SchemaUnavailable,
				"Publish the developer schema before baking."});
			return result;
		}
		for (const auto& record : overrides_.appliedOverrides().records()) {
			if (!isLive(record.layer)) continue;
			++result.activeLiveOverrideCount;
			DevBakeDiagnostic diagnostic;
			if (makeElementEntry(*schema, record, diagnostic)) ++result.bakeableOverrideCount;
			else {
				++result.unbakeableOverrideCount;
				if (diagnostic.code != DevBakeDiagnosticCode::None)
					result.bakeDiagnostics.push_back(std::move(diagnostic));
			}
		}
		for (const auto& record : overrides_.themeBakeRecords()) {
			if (record.preview_only || record.remove_baked)
				continue;
			++result.activeLiveOverrideCount;
			DevBakeDiagnostic diagnostic;
			if (makeThemeEntry(*schema, record, diagnostic)) ++result.bakeableOverrideCount;
			else {
				++result.unbakeableOverrideCount;
				if (diagnostic.code != DevBakeDiagnosticCode::None)
					result.bakeDiagnostics.push_back(std::move(diagnostic));
			}
		}
		for (const auto& diagnostic : status_.bakeDiagnostics) {
			if (diagnostic.code == DevBakeDiagnosticCode::ManifestReadFailed ||
				diagnostic.code == DevBakeDiagnosticCode::ManifestInvalid ||
				diagnostic.code == DevBakeDiagnosticCode::ManifestWriteFailed) {
				result.bakeDiagnostics.push_back(diagnostic);
			}
		}
		return result;
	} catch (...) {
		return {};
	}
}

std::vector<DevBakeDiffEntry> DevBakePipeline::queryDiff() const noexcept {
	try {
		std::vector<DevBakeDiffEntry> result;
		result.reserve(manifest_.entries.size() +
			overrides_.appliedOverrides().records().size() + overrides_.themeBakeRecords().size());
		for (const auto& entry : manifest_.entries) {
			result.push_back({
				.themeVariant = entry.themeVariant,
				.fieldPath = entry.fieldPath,
				.bakedValueString = entry.valueJson,
				.definition = entry.definition,
				.themeType = entry.themeType,
				.instance = entry.instanceKey,
				.fieldId = entry.fieldId,
				.targetKind = entry.targetKind,
				.isBaked = true,});
		}
		const tooling::DevSchemaView schema = schemas_.view();
		if (!schema) return result;
		for (std::size_t index = 0; index < manifest_.entries.size(); ++index) {
			const auto& entry = manifest_.entries[index];
			DevOwnedValue authored;
			if (query_authored_element_value(overrides_, *schema, entry, InvalidWindowId, authored) !=
				DevValueCaptureStatus::Available)
				continue;
			const auto* type = schema->findType(authored.type());
			const auto original = type ? serializeValue(*schema,
				tooling::DevTypeIndex{static_cast<std::uint32_t>(type - schema->types.data())},
				authored.data()) : std::nullopt;
			if (original)
				result[index].authoredValueString = original->json;
		}
		const auto mergeLive = [&](const DevBakeEntry& live) {
			const auto baked = std::find_if(manifest_.entries.begin(), manifest_.entries.end(),
				[&](const DevBakeEntry& entry) { return sameIdentity(entry, live); });
			const auto diff = std::find_if(result.begin(), result.end(), [&](const DevBakeDiffEntry& entry) {
				return entry.targetKind == live.targetKind && entry.definition == live.definition &&
					entry.themeType == live.themeType && entry.themeVariant == live.themeVariant &&
					entry.instance == live.instanceKey && entry.fieldId == live.fieldId &&
					entry.fieldPath == live.fieldPath;
			});
			if (diff != result.end()) {
				diff->activeValueString = live.valueJson;
				diff->isOverridden = true;
				return;
			}
			result.push_back({
				.themeVariant = live.themeVariant,
				.fieldPath = live.fieldPath,
				.activeValueString = live.valueJson,
				.bakedValueString = baked == manifest_.entries.end() ? std::string{} : baked->valueJson,
				.definition = live.definition,
				.themeType = live.themeType,
				.instance = live.instanceKey,
				.fieldId = live.fieldId,
				.targetKind = live.targetKind,
				.isOverridden = true,
				.isBaked = baked != manifest_.entries.end(),
			});
		};
		for (const auto& record : overrides_.appliedOverrides().records()) {
			if (!isLive(record.layer)) continue;
			DevBakeDiagnostic ignored;
			if (auto entry = makeElementEntry(*schema, record, ignored)) {
				mergeLive(*entry);
				DevOwnedValue authored;
				auto target = record.target;
				if (target.scope == DevOverrideScope::Definition)
					target.window = InvalidWindowId;
				if (query_authored_element_value(overrides_, *schema, target, record.field, authored) ==
					DevValueCaptureStatus::Available) {
					const auto* type = schema->findType(authored.type());
					const auto original = type ? serializeValue(*schema,
						tooling::DevTypeIndex{static_cast<std::uint32_t>(type - schema->types.data())},
						authored.data()) : std::nullopt;
					if (original)
						for (auto& diff : result)
							if (diff.targetKind == DevBakeTargetKind::Element &&
								diff.definition == entry->definition && diff.instance == entry->instanceKey &&
								diff.fieldPath == entry->fieldPath)
								diff.authoredValueString = original->json;
				}
			}
		}
		for (const auto& record : overrides_.themeBakeRecords()) {
			if (record.preview_only || record.remove_baked)
				continue;
			DevBakeDiagnostic ignored;
			if (auto entry = makeThemeEntry(*schema, record, ignored)) {
				mergeLive(*entry);
				const auto* field =
					(record.fieldIndex && record.fieldIndex.value <= schema->fields.size()
						 ? &schema->fields[record.fieldIndex.value - 1]
						 : nullptr);
				const auto original = field
										  ? serializeValue(*schema, field->valueType,
														   record.authored ? record.authored.data()
																		   : record.original.data())
										  : std::nullopt;
				for (auto& diff : result)
					if (diff.targetKind == DevBakeTargetKind::Theme &&
						diff.themeType == entry->themeType &&
						diff.themeVariant == entry->themeVariant &&
						diff.fieldPath == entry->fieldPath && original)
						diff.authoredValueString = original->json;
			}
		}
		return result;
	} catch (...) {
		return {};
	}
}

} // namespace FlowUi::devSystems::tooling

#endif
