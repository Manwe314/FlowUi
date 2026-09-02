#include "devSystems/devTooling/bake/DevBakePipeline.hpp"

#if FLOW_UI_DEV_MODE

#include <algorithm>
#include <charconv>
#include <chrono>
#include <cmath>
#include <ctime>
#include <fstream>
#include <iomanip>
#include <locale>
#include <optional>
#include <sstream>
#include <string_view>
#include <system_error>
#include <tuple>
#include <utility>

#include "devSystems/devTooling/override/DevOverrideEngine.hpp"
#include "devSystems/devTooling/schema/DevSchemaRegistry.hpp"
#include "managers/structs/ActionManagerStructs.hpp"

namespace FlowUi::devSystems::tooling {
namespace {

constexpr std::uint32_t kManifestVersion = 1;

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
		while (position_ < input_.size() && input_[position_] >= '0' && input_[position_] <= '9') ++position_;
		if (position_ < input_.size() && input_[position_] == '.') {
			++position_;
			while (position_ < input_.size() && input_[position_] >= '0' && input_[position_] <= '9') ++position_;
		}
		if (position_ < input_.size() && (input_[position_] == 'e' || input_[position_] == 'E')) {
			++position_;
			if (position_ < input_.size() && (input_[position_] == '+' || input_[position_] == '-')) ++position_;
			while (position_ < input_.size() && input_[position_] >= '0' && input_[position_] <= '9') ++position_;
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
		default:
			if (static_cast<unsigned char>(character) < 0x20u) output += '?';
			else output.push_back(character);
		}
	}
	output.push_back('"');
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

const devMode::DevFieldSchema* findField(
	const devMode::DevSchemaGeneration& schema,
	devMode::DevTypeIndex owner,
	devMode::DevFieldId field) {
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
	const devMode::DevSchemaGeneration& schema,
	devMode::DevTypeIndex typeIndex,
	const void* value,
	std::uint32_t depth = 0) {
	if (!value || depth > 12) return std::nullopt;
	const devMode::DevTypeSchema* type = schema.type(typeIndex);
	if (!type || typeIndex.value >= schema.typeOperations.size()) return std::nullopt;
	const devMode::DevTypeOps* operations = schema.typeOperations[typeIndex.value];
	if (!operations) return std::nullopt;
	const std::string_view cppType = schema.string(type->cppTypeName);
	if (cppType.ends_with("ActionCall")) {
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
	if (cppType.ends_with("TextureRef")) {
		const auto& texture = *static_cast<const TextureRef*>(value);
		if (texture.sourceKey.empty() ||
			(texture.sourceDomain != ResourceDomain::Image &&
			 texture.sourceDomain != ResourceDomain::Icon)) return std::nullopt;
		std::string keyLiteral;
		appendEscaped(keyLiteral, texture.sourceKey);
		const std::string domain = texture.sourceDomain == ResourceDomain::Image
			? "Image" : "Icon";
		return SerializedValue{
			"{\"domain\":" + std::to_string(static_cast<unsigned>(texture.sourceDomain)) +
				",\"key\":" + keyLiteral + "}",
			"FlowUi::TextureRef::fromStable(FlowUi::ResourceDomain::" + domain +
				"," + keyLiteral + ",static_cast<FlowUi::TextureFitMode>(" +
				std::to_string(static_cast<unsigned>(texture.fitMode)) +
				"),static_cast<FlowUi::TextureSamplingMode>(" +
				std::to_string(static_cast<unsigned>(texture.samplingMode)) +
				")," + (texture.tintEnabled ? "true" : "false") + ")"};
	}
	long double numeric = 0.0L;
	if (type->kind == devMode::DevTypeKind::Boolean) {
		if (!operations->numericValue || !operations->numericValue(value, numeric)) return std::nullopt;
		return SerializedValue{numeric == 0.0L ? "false" : "true", numeric == 0.0L ? "false" : "true"};
	}
	if (type->kind == devMode::DevTypeKind::SignedInteger ||
		type->kind == devMode::DevTypeKind::UnsignedInteger ||
		type->kind == devMode::DevTypeKind::FloatingPoint ||
		type->kind == devMode::DevTypeKind::Enumeration) {
		if (!operations->numericValue || !operations->numericValue(value, numeric) ||
			!std::isfinite(numeric)) return std::nullopt;
		std::ostringstream stream;
		stream.imbue(std::locale::classic());
		stream << std::setprecision(20) << numeric;
		std::string literal = stream.str();
		if (type->kind == devMode::DevTypeKind::Enumeration) {
			literal = "static_cast<" + std::string(schema.string(type->cppTypeName)) + ">(" + literal + ")";
		}
		return SerializedValue{stream.str(), std::move(literal)};
	}
	if (type->kind != devMode::DevTypeKind::Object || type->fields.count == 0) return std::nullopt;
	SerializedValue result{"{", std::string(schema.string(type->cppTypeName)) + "{"};
	bool first = true;
	for (const auto& field : schema.fieldsOf(typeIndex)) {
		if (field.operations >= schema.fieldOperations.size()) return std::nullopt;
		const devMode::DevFieldOps* fieldOps = schema.fieldOperations[field.operations];
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
	const devMode::DevFieldSchema* leaf = nullptr;
};

ResolvedPath resolvePath(
	const devMode::DevSchemaGeneration& schema,
	devMode::DevTypeIndex root,
	const DevOverrideFieldKey& key) {
	ResolvedPath result;
	devMode::DevTypeIndex owner = root;
	const auto append = [&](const devMode::DevFieldSchema& field, ResolvedPath& target) {
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
	const devMode::DevSchemaGeneration& schema,
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
	const devMode::DevSchemaGeneration& schema,
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
	return true;
}

bool loadManifest(const std::filesystem::path& path, DevBakeManifest& manifest, bool& existed) {
	existed = std::filesystem::exists(path);
	if (!existed) { manifest = {}; return true; }
	std::ifstream input(path, std::ios::binary);
	if (!input) return false;
	const std::string contents((std::istreambuf_iterator<char>(input)), {});
	JsonValue root;
	if (!JsonParser(contents).parse(root) || root.kind != JsonValue::Kind::Object) return false;
	const JsonValue* version = root.find("manifestVersion");
	if (!version || version->kind != JsonValue::Kind::Number || version->text != "1") return false;
	manifest = {};
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
	return true;
}

std::string encodeManifest(const DevBakeManifest& manifest) {
	std::string output = "{\n  \"manifestVersion\": 1,\n  \"schemaFingerprint\": ";
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
		output += "}\n    }";
	}
	if (!manifest.entries.empty()) output += '\n';
	output += "  ]\n}\n";
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
		if (!output) return false;
	}
	std::filesystem::rename(temporary, destination, error);
	if (!error) return true;
	std::filesystem::remove(destination, error);
	error.clear();
	std::filesystem::rename(temporary, destination, error);
	return !error;
}

} // namespace

DevBakePipeline::DevBakePipeline(
	devMode::DevSchemaRegistry& schemas,
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

DevCommandResult DevBakePipeline::bake() noexcept {
	status_ = {};
	try {
		const devMode::DevSchemaView schema = schemas_.view();
		if (!schema) {
			status_.bakeDiagnostics.push_back({DevBakeDiagnosticCode::SchemaUnavailable,
				"Publish the developer schema before baking."});
			return {.status = DevCommandStatus::SchemaUnavailable};
		}
		bool existed = false;
		DevBakeManifest updated;
		if (!loadManifest(manifestPath_, updated, existed)) {
			status_.bakeDiagnostics.push_back({DevBakeDiagnosticCode::ManifestInvalid,
				"The active .flowchanges manifest is invalid or unreadable."});
			return {.status = DevCommandStatus::BakeManifestInvalid};
		}
		if (updated.schemaFingerprint != 0 &&
			updated.schemaFingerprint != schema->fingerprint) {
			updated.entries.clear();
			status_.bakeDiagnostics.push_back({
				DevBakeDiagnosticCode::ManifestInvalid,
				"Stale baked entries were ignored because the developer schema fingerprint changed."});
		}
		updated.manifestVersion = kManifestVersion;
		updated.schemaFingerprint = schema->fingerprint;
		updated.buildFingerprint = FLOWUI_BAKE_BUILD_FINGERPRINT;
		updated.createdTimestamp = nowUtc();

		const auto upsert = [&](DevBakeEntry entry) {
			const auto found = std::find_if(updated.entries.begin(), updated.entries.end(),
				[&](const DevBakeEntry& current) { return sameIdentity(current, entry); });
			if (found == updated.entries.end()) updated.entries.push_back(std::move(entry));
			else *found = std::move(entry);
		};
		for (const auto& record : overrides_.appliedOverrides().records()) {
			if (!isLive(record.layer)) continue;
			++status_.activeLiveOverrideCount;
			DevBakeDiagnostic diagnostic;
			if (auto entry = makeElementEntry(*schema, record, diagnostic)) {
				++status_.bakeableOverrideCount;
				upsert(std::move(*entry));
			} else if (diagnostic.code != DevBakeDiagnosticCode::None) {
				++status_.unbakeableOverrideCount;
				status_.bakeDiagnostics.push_back(std::move(diagnostic));
			}
		}
		for (const auto& record : overrides_.themeBakeRecords()) {
			++status_.activeLiveOverrideCount;
			DevBakeDiagnostic diagnostic;
			if (auto entry = makeThemeEntry(*schema, record, diagnostic)) {
				++status_.bakeableOverrideCount;
				upsert(std::move(*entry));
			} else if (diagnostic.code != DevBakeDiagnosticCode::None) {
				++status_.unbakeableOverrideCount;
				status_.bakeDiagnostics.push_back(std::move(diagnostic));
			}
		}
		for (const auto& removed : overrides_.appliedOverrides().bakeTombstones()) {
			DevBakeDiagnostic ignored;
			if (auto entry = makeElementEntry(*schema, removed, ignored)) {
				std::erase_if(updated.entries, [&](const DevBakeEntry& current) {
					return sameIdentity(current, *entry);
				});
			}
		}
		for (const auto& removed : overrides_.themeBakeTombstones()) {
			DevBakeDiagnostic ignored;
			if (auto entry = makeThemeEntry(*schema, removed, ignored)) {
				std::erase_if(updated.entries, [&](const DevBakeEntry& current) {
					return sameIdentity(current, *entry);
				});
			}
		}

		std::sort(updated.entries.begin(), updated.entries.end(), [](const auto& left, const auto& right) {
			return std::tie(left.targetKind, left.definition.value, left.themeType,
				left.themeVariant, left.targetScope, left.instanceKey.value, left.fieldPath) <
				std::tie(right.targetKind, right.definition.value, right.themeType,
				right.themeVariant, right.targetScope, right.instanceKey.value, right.fieldPath);
		});
		if (!writeAtomically(manifestPath_, encodeManifest(updated))) {
			status_.bakeDiagnostics.push_back({DevBakeDiagnosticCode::ManifestWriteFailed,
				"Failed to atomically write the active .flowchanges manifest."});
			return {.status = DevCommandStatus::BakeWriteFailed};
		}
		manifest_ = std::move(updated);
		overrides_.appliedOverrides().clearBakeTombstones();
		overrides_.clearThemeBakeTombstones();
		return {.status = status_.bakeableOverrideCount == 0 && !existed
			? DevCommandStatus::NothingToBake : DevCommandStatus::Applied,
			.applied = true};
	} catch (...) {
		return {.status = DevCommandStatus::InternalFailure};
	}
}

DevBakeStatusSnapshot DevBakePipeline::queryStatus() const noexcept {
	try {
		DevBakeStatusSnapshot result{};
		const devMode::DevSchemaView schema = schemas_.view();
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
				.targetKind = entry.targetKind,
				.definition = entry.definition,
				.themeType = entry.themeType,
				.themeVariant = entry.themeVariant,
				.instance = entry.instanceKey,
				.fieldId = entry.fieldId,
				.fieldPath = entry.fieldPath,
				.bakedValueString = entry.valueJson,
				.isBaked = true,
			});
		}
		const devMode::DevSchemaView schema = schemas_.view();
		if (!schema) return result;
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
				.targetKind = live.targetKind,
				.definition = live.definition,
				.themeType = live.themeType,
				.themeVariant = live.themeVariant,
				.instance = live.instanceKey,
				.fieldId = live.fieldId,
				.fieldPath = live.fieldPath,
				.activeValueString = live.valueJson,
				.bakedValueString = baked == manifest_.entries.end() ? std::string{} : baked->valueJson,
				.isOverridden = true,
				.isBaked = baked != manifest_.entries.end(),
			});
		};
		for (const auto& record : overrides_.appliedOverrides().records()) {
			if (!isLive(record.layer)) continue;
			DevBakeDiagnostic ignored;
			if (auto entry = makeElementEntry(*schema, record, ignored)) mergeLive(*entry);
		}
		for (const auto& record : overrides_.themeBakeRecords()) {
			DevBakeDiagnostic ignored;
			if (auto entry = makeThemeEntry(*schema, record, ignored)) mergeLive(*entry);
		}
		return result;
	} catch (...) {
		return {};
	}
}

} // namespace FlowUi::devSystems::tooling

#endif
