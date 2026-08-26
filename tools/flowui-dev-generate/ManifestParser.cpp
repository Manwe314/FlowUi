#include "ManifestParser.hpp"

#include <charconv>
#include <cctype>
#include <fstream>
#include <optional>
#include <string_view>
#include <system_error>
#include <utility>

namespace flowui::bake_tool {
namespace {

struct JsonValue {
	enum class Kind { Null, Boolean, Number, String, Array, Object } kind = Kind::Null;
	std::string text{};
	std::vector<JsonValue> array{};
	std::vector<std::pair<std::string, JsonValue>> object{};
	const JsonValue* find(std::string_view key) const {
		for (const auto& item : object) if (item.first == key) return &item.second;
		return nullptr;
	}
};

class Parser {
public:
	explicit Parser(std::string_view input) : input_(input) {}
	bool parse(JsonValue& value) { space(); return parseValue(value) && (space(), at_ == input_.size()); }
private:
	void space() { while (at_ < input_.size() && (input_[at_] == ' ' || input_[at_] == '\n' || input_[at_] == '\r' || input_[at_] == '\t')) ++at_; }
	bool take(char value) { space(); if (at_ >= input_.size() || input_[at_] != value) return false; ++at_; return true; }
	bool parseValue(JsonValue& out) {
		space(); if (at_ >= input_.size()) return false;
		if (input_[at_] == '"') { out.kind = JsonValue::Kind::String; return string(out.text); }
		if (input_[at_] == '{') return object(out);
		if (input_[at_] == '[') return array(out);
		if (input_.substr(at_, 4) == "true" || input_.substr(at_, 4) == "null") { at_ += 4; return true; }
		if (input_.substr(at_, 5) == "false") { at_ += 5; return true; }
		const std::size_t first = at_;
		while (at_ < input_.size() && (input_[at_] == '-' || input_[at_] == '+' || input_[at_] == '.' ||
			(input_[at_] >= '0' && input_[at_] <= '9') || input_[at_] == 'e' || input_[at_] == 'E')) ++at_;
		if (first == at_) return false;
		out.kind = JsonValue::Kind::Number; out.text.assign(input_.substr(first, at_ - first)); return true;
	}
	bool string(std::string& out) {
		if (at_ >= input_.size() || input_[at_++] != '"') return false;
		while (at_ < input_.size()) {
			char value = input_[at_++];
			if (value == '"') return true;
			if (value != '\\') { out += value; continue; }
			if (at_ >= input_.size()) return false;
			switch (input_[at_++]) {
			case '"': out += '"'; break; case '\\': out += '\\'; break; case '/': out += '/'; break;
			case 'b': out += '\b'; break; case 'f': out += '\f'; break; case 'n': out += '\n'; break;
			case 'r': out += '\r'; break; case 't': out += '\t'; break; default: return false;
			}
		}
		return false;
	}
	bool array(JsonValue& out) {
		if (!take('[')) return false; out.kind = JsonValue::Kind::Array; space(); if (take(']')) return true;
		for (;;) { out.array.emplace_back(); if (!parseValue(out.array.back())) return false; if (take(']')) return true; if (!take(',')) return false; }
	}
	bool object(JsonValue& out) {
		if (!take('{')) return false; out.kind = JsonValue::Kind::Object; space(); if (take('}')) return true;
		for (;;) { std::string key; space(); if (!string(key) || !take(':')) return false; JsonValue value; if (!parseValue(value)) return false; out.object.emplace_back(std::move(key), std::move(value)); if (take('}')) return true; if (!take(',')) return false; }
	}
	std::string_view input_; std::size_t at_ = 0;
};

std::optional<std::string> member(const JsonValue& value, std::string_view key) {
	const auto* result = value.find(key);
	if (!result || result->kind != JsonValue::Kind::String) return std::nullopt;
	return result->text;
}

bool hex(std::string_view value, std::uint64_t& result) {
	if (value.starts_with("0x") || value.starts_with("0X")) value.remove_prefix(2);
	const auto parsed = std::from_chars(value.data(), value.data() + value.size(), result, 16);
	return parsed.ec == std::errc{} && parsed.ptr == value.data() + value.size();
}

bool safeType(std::string_view value) {
	if (value.empty()) return false;
	for (char c : value) if (!(std::isalnum(static_cast<unsigned char>(c)) || c == '_' || c == ':' ||
		c == '<' || c == '>' || c == ',' || c == ' ' || c == '*')) return false;
	return true;
}
bool safePath(std::string_view value) {
	if (value.empty() || value.front() == '.' || value.back() == '.') return false;
	for (char c : value) if (!(std::isalnum(static_cast<unsigned char>(c)) || c == '_' || c == '.')) return false;
	return true;
}
bool safeExpression(std::string_view value) {
	if (value.empty()) return false;
	for (char c : value) {
		if (!(std::isalnum(static_cast<unsigned char>(c)) || c == '_' || c == ':' || c == '<' || c == '>' ||
			c == ',' || c == ' ' || c == '\t' || c == '\n' || c == '.' || c == '+' || c == '-' ||
			c == '=' || c == '{' || c == '}' || c == '(' || c == ')')) return false;
	}
	return value.find(';') == std::string_view::npos && value.find('#') == std::string_view::npos;
}

} // namespace

bool parseManifest(const std::filesystem::path& path, Manifest& output, std::string& error) {
	std::ifstream input(path, std::ios::binary);
	if (!input) { error = "cannot open manifest: " + path.string(); return false; }
	const std::string contents((std::istreambuf_iterator<char>(input)), {});
	JsonValue root;
	if (!Parser(contents).parse(root) || root.kind != JsonValue::Kind::Object) { error = "manifest is not valid JSON"; return false; }
	const auto* version = root.find("manifestVersion");
	if (!version || version->kind != JsonValue::Kind::Number || version->text != "1") { error = "unsupported manifestVersion (expected 1)"; return false; }
	output = {}; output.version = 1;
	if (const auto fingerprint = member(root, "schemaFingerprint"); fingerprint && !hex(*fingerprint, output.schemaFingerprint)) { error = "invalid schemaFingerprint"; return false; }
	const auto* entries = root.find("bakedChanges");
	if (!entries || entries->kind != JsonValue::Kind::Array) { error = "bakedChanges must be an array"; return false; }
	for (const auto& item : entries->array) {
		if (item.kind != JsonValue::Kind::Object) { error = "bakedChanges entry must be an object"; return false; }
		const auto kind = member(item, "targetKind"); const auto scope = member(item, "targetScope");
		const auto fieldId = member(item, "fieldId"); const auto fieldPath = member(item, "fieldPath");
		const auto owner = member(item, "ownerCppType"); const auto header = member(item, "sourceHeader");
		const auto expression = member(item, "cppValue");
		if (!kind || !scope || !fieldId || !fieldPath || !owner || !header || !expression) { error = "bakedChanges entry is missing generator metadata"; return false; }
		Entry entry;
		if (*kind == "Element") entry.targetKind = TargetKind::Element;
		else if (*kind == "Theme") entry.targetKind = TargetKind::Theme;
		else { error = "unknown targetKind"; return false; }
		if (*scope == "Definition") entry.targetScope = TargetScope::Definition;
		else if (*scope == "ExactInstance") entry.targetScope = TargetScope::ExactInstance;
		else { error = "unknown targetScope"; return false; }
		entry.fieldPath = *fieldPath; entry.ownerCppType = *owner; entry.sourceHeader = *header; entry.cppValue = *expression;
		if (!hex(*fieldId, entry.fieldId) || !safePath(entry.fieldPath) || !safeType(entry.ownerCppType) ||
			entry.sourceHeader.find_first_of("\"\r\n") != std::string::npos || !safeExpression(entry.cppValue)) {
			error = "unsafe or malformed generator metadata"; return false;
		}
		if (entry.targetKind == TargetKind::Element) {
			const auto id = member(item, "definitionId"); if (!id || !hex(*id, entry.definitionId) || entry.definitionId == 0) { error = "invalid definitionId"; return false; }
			if (entry.targetScope == TargetScope::ExactInstance) { const auto key = member(item, "instanceKey"); if (!key || !hex(*key, entry.instanceKey) || entry.instanceKey == 0) { error = "invalid instanceKey"; return false; } }
			if (const auto label = member(item, "instanceDebugLabel")) entry.instanceDebugLabel = *label;
		} else {
			const auto type = member(item, "themeType"); const auto variant = member(item, "themeVariant");
			if (!type || !variant || !hex(*type, entry.themeType) || entry.themeType == 0 || variant->empty()) { error = "invalid theme target"; return false; }
			entry.themeVariant = *variant;
		}
		output.entries.push_back(std::move(entry));
	}
	return true;
}

} // namespace flowui::bake_tool
