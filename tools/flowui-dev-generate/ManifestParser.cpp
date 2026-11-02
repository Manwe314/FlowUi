#include "ManifestParser.hpp"
#include "../Utf8Paths.hpp"
#include "devSystems/devTooling/bake/DevBakeValidation.hpp"

#include <algorithm>
#include <cctype>
#include <charconv>
#include <chrono>
#include <fstream>
#include <optional>
#include <sstream>
#include <string_view>
#include <system_error>
#include <utility>

namespace flowui::bake_tool {
namespace {

struct JsonValue {
	enum class Kind { Null, Boolean, Number, String, Array, Object } kind = Kind::Null;
	bool boolean = false;
	std::string text{};
	std::vector<JsonValue> array{};
	std::vector<std::pair<std::string, JsonValue>> object{};
	std::size_t begin = 0;
	std::size_t end = 0;
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
		space();
		out.begin = at_;
		const bool accepted = parseValueBody(out);
		out.end = at_;
		return accepted;
	}
	bool parseValueBody(JsonValue& out) {
		space(); if (at_ >= input_.size()) return false;
		if (input_[at_] == '"') { out.kind = JsonValue::Kind::String; return string(out.text); }
		if (input_[at_] == '{') return object(out);
		if (input_[at_] == '[') return array(out);
		if (input_.substr(at_, 4) == "true") {
			at_ += 4;
			out.kind = JsonValue::Kind::Boolean;
			out.boolean = true;
			return true;
		}
		if (input_.substr(at_, 4) == "null") {
			at_ += 4;
			return true;
		}
		if (input_.substr(at_, 5) == "false") {
			at_ += 5;
			out.kind = JsonValue::Kind::Boolean;
			out.boolean = false;
			return true;
		}
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
		for (;;) {
			std::string key;
			space();
			if (!string(key) || !take(':'))
				return false;
			JsonValue value;
			if (!parseValue(value))
				return false;
			if (out.find(key))
				return false;
			out.object.emplace_back(std::move(key), std::move(value));
			if (take('}'))
				return true;
			if (!take(','))
				return false;
		}
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

bool read_document(const std::filesystem::path& path, std::string& contents, JsonValue& root,
				   std::string& error) {
	std::ifstream input(path, std::ios::binary);
	if (!input) {
		error = "cannot open manifest: " + flowui::tools::utf8_text(path);
		return false;
	}
	contents.assign(std::istreambuf_iterator<char>(input), {});
	if (!Parser(contents).parse(root) || root.kind != JsonValue::Kind::Object) {
		error = "manifest is not valid JSON: " + flowui::tools::utf8_text(path);
		return false;
	}
	const auto* entries = root.find("bakedChanges");
	if (!entries || entries->kind != JsonValue::Kind::Array) {
		error = "bakedChanges must be an array: " + flowui::tools::utf8_text(path);
		return false;
	}
	return true;
}

class ManifestRepairLock {
public:
	explicit ManifestRepairLock(const std::filesystem::path& manifest_path) : path_(manifest_path) {
		path_ += ".lock";
		std::error_code filesystem_error;
		acquired_ = std::filesystem::create_directory(path_, filesystem_error);
	}
	~ManifestRepairLock() {
		if (acquired_) {
			std::error_code filesystem_error;
			std::filesystem::remove(path_, filesystem_error);
		}
	}
	ManifestRepairLock(const ManifestRepairLock&) = delete;
	ManifestRepairLock& operator=(const ManifestRepairLock&) = delete;
	[[nodiscard]] bool acquired() const noexcept { return acquired_; }

private:
	std::filesystem::path path_;
	bool acquired_ = false;
};

} // namespace

bool parseManifest(const std::filesystem::path& path, Manifest& output, std::string& error) {
	std::string contents;
	JsonValue root;
	if (!read_document(path, contents, root, error))
		return false;
	const auto* version = root.find("manifestVersion");
	if (!version || version->kind != JsonValue::Kind::Number ||
		(version->text != "1" && version->text != "2")) {
		error = "unsupported manifestVersion (expected 1 or 2)";
		return false;
	}
	output = {};
	output.content_digest = FlowUi::devSystems::tooling::manifest_content_digest(contents);
	output.source_path = flowui::tools::utf8_text(path);
	output.version = version->text == "1" ? 1 : 2;
	if (const auto* memory = root.find("memoryCapacity")) {
		if (output.version != 2) {
			error = "memoryCapacity requires manifestVersion 2";
			return false;
		}
		FlowUi::detail::MemoryCapacityManifest decoded;
		if (!FlowUi::detail::decode_memory_capacity_manifest(*memory, decoded, error))
			return false;
		output.memoryCapacity = std::move(decoded);
	}
	if (const auto fingerprint = member(root, "schemaFingerprint"); fingerprint && !hex(*fingerprint, output.schemaFingerprint)) { error = "invalid schemaFingerprint"; return false; }
	const auto* entries = root.find("bakedChanges");
	if (!entries || entries->kind != JsonValue::Kind::Array) { error = "bakedChanges must be an array"; return false; }
	output.entries.reserve(entries->array.size());
	for (const auto& item : entries->array) {
		const std::size_t entry_index = output.entries.size();
		const auto fail_entry = [&](std::string_view reason) {
			error = flowui::tools::utf8_text(path) + ": bakedChanges[" +
					std::to_string(entry_index) + "] field '" +
					member(item, "fieldPath").value_or("unknown") + "': " + std::string(reason) +
					". Inspect with --manifest <file> --inspect; remove explicitly with "
					"--remove-entry " +
					std::to_string(entry_index);
			return false;
		};
		if (item.kind != JsonValue::Kind::Object)
			return fail_entry("entry must be an object");
		const auto kind = member(item, "targetKind"); const auto scope = member(item, "targetScope");
		const auto fieldId = member(item, "fieldId"); const auto fieldPath = member(item, "fieldPath");
		const auto owner = member(item, "ownerCppType"); const auto header = member(item, "sourceHeader");
		const auto expression = member(item, "cppValue");
		if (!kind || !scope || !fieldId || !fieldPath || !owner || !header || !expression)
			return fail_entry("missing generator metadata");
		Entry entry;
		entry.source_index = entry_index;
		entry.source_line =
			1 + static_cast<std::size_t>(
					std::count(contents.begin(),
							   contents.begin() + static_cast<std::ptrdiff_t>(item.begin), '\n'));
		if (*kind == "Element") entry.targetKind = TargetKind::Element;
		else if (*kind == "Theme") entry.targetKind = TargetKind::Theme;
		else
			return fail_entry("unknown targetKind");
		if (*scope == "Definition") entry.targetScope = TargetScope::Definition;
		else if (*scope == "ExactInstance") entry.targetScope = TargetScope::ExactInstance;
		else
			return fail_entry("unknown targetScope");
		entry.fieldPath = *fieldPath; entry.ownerCppType = *owner; entry.sourceHeader = *header; entry.cppValue = *expression;
		if (!hex(*fieldId, entry.fieldId) ||
			!FlowUi::devSystems::tooling::generator_entry_metadata_valid(
				entry.ownerCppType, entry.fieldPath, entry.sourceHeader, entry.cppValue)) {
			return fail_entry("unsafe or malformed generator metadata");
		}
		if (const auto* value_json = item.find("valueJson")) {
			entry.value_json =
				value_json->kind == JsonValue::Kind::String
					? value_json->text
					: contents.substr(value_json->begin, value_json->end - value_json->begin);
		}
		if (const auto value_type = member(item, "valueTypeId")) {
			if (!hex(*value_type, entry.value_type))
				return fail_entry("invalid valueTypeId");
		} else if (const auto legacy_type = member(item, "valueType");
				   legacy_type && legacy_type->starts_with("0x")) {
			if (!hex(*legacy_type, entry.value_type))
				return fail_entry("invalid legacy valueType identity");
		}
		if (entry.targetKind == TargetKind::Element) {
			const auto id = member(item, "definitionId");
			if (!id || !hex(*id, entry.definitionId) || entry.definitionId == 0)
				return fail_entry("invalid definitionId");
			if (entry.targetScope == TargetScope::ExactInstance) {
				const auto key = member(item, "instanceKey");
				if (!key || !hex(*key, entry.instanceKey) || entry.instanceKey == 0)
					return fail_entry("invalid instanceKey");
			}
			if (const auto label = member(item, "instanceDebugLabel")) entry.instanceDebugLabel = *label;
		} else {
			const auto type = member(item, "themeType"); const auto variant = member(item, "themeVariant");
			if (!type || !variant || !hex(*type, entry.themeType) || entry.themeType == 0 ||
				variant->empty())
				return fail_entry("invalid theme target");
			entry.themeVariant = *variant;
		}
		for (const auto& previous : output.entries)
			if (previous.targetKind == entry.targetKind && previous.targetScope == entry.targetScope &&
				previous.definitionId == entry.definitionId && previous.themeType == entry.themeType &&
				previous.instanceKey == entry.instanceKey && previous.themeVariant == entry.themeVariant &&
				previous.fieldPath == entry.fieldPath)
				return fail_entry("duplicate assignment destination");
		output.entries.push_back(std::move(entry));
	}
	return true;
}

bool inspect_manifest(const std::filesystem::path& path, std::string& output, std::string& error) {
	std::string contents;
	JsonValue root;
	if (!read_document(path, contents, root, error))
		return false;
	std::ostringstream description;
	description << flowui::tools::utf8_text(path) << '\n';
	const auto& entries = root.find("bakedChanges")->array;
	for (std::size_t entry_index = 0; entry_index < entries.size(); ++entry_index) {
		const auto& entry = entries[entry_index];
		description
			<< '[' << entry_index << "] " << member(entry, "targetKind").value_or("unknown") << ' '
			<< member(entry, "definitionId")
				   .value_or(member(entry, "themeType").value_or("unknown"))
			<< ' ' << member(entry, "targetScope").value_or("unknown") << ' '
			<< member(entry, "instanceKey").value_or(member(entry, "themeVariant").value_or(""))
			<< ' ' << member(entry, "fieldPath").value_or("unknown field") << " = "
			<< member(entry, "cppValue").value_or("missing initializer") << '\n';
	}
	description << entries.size() << " element/theme entries; memory payload "
				<< (root.find("memoryCapacity") ? "present" : "absent") << '\n';
	Manifest decoded;
	std::string validation_error;
	if (!parseManifest(path, decoded, validation_error))
		description << "Validation: " << validation_error << '\n';
	else
		description << "Validation: generator metadata accepted\n";
	output = description.str();
	return true;
}

bool remove_manifest_entry(const std::filesystem::path& path, std::size_t entry_index,
						   std::string& error) {
	ManifestRepairLock writer_lock(path);
	if (!writer_lock.acquired()) {
		error = "cannot acquire manifest writer lock " + flowui::tools::utf8_text(path) +
				".lock; another writer may be active; retry after it finishes";
		return false;
	}
	std::string contents;
	JsonValue root;
	if (!read_document(path, contents, root, error))
		return false;
	const auto& entries = root.find("bakedChanges")->array;
	if (entry_index >= entries.size()) {
		error = "--remove-entry index is outside bakedChanges";
		return false;
	}
	std::size_t first = entries[entry_index].begin;
	std::size_t last = entries[entry_index].end;
	if (entry_index + 1 < entries.size())
		last = entries[entry_index + 1].begin;
	else if (entry_index > 0)
		first = entries[entry_index - 1].end;
	std::string repaired = contents;
	repaired.erase(first, last - first);
	const auto temporary = flowui::tools::utf8_path(
		flowui::tools::utf8_text(path) + ".repair-" +
		std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()) + ".tmp");
	{
		std::ofstream output(temporary, std::ios::binary | std::ios::trunc);
		output.write(repaired.data(), static_cast<std::streamsize>(repaired.size()));
		if (!output) {
			error = "cannot write repair temporary file";
			output.close();
			std::error_code filesystem_error;
			std::filesystem::remove(temporary, filesystem_error);
			return false;
		}
	}
	std::string current;
	JsonValue current_root;
	std::error_code filesystem_error;
	if (!read_document(path, current, current_root, error) || current != contents) {
		std::filesystem::remove(temporary, filesystem_error);
		error = "manifest changed during repair; inspect it again before removing an entry";
		return false;
	}
	std::filesystem::rename(temporary, path, filesystem_error);
	if (filesystem_error) {
		error = "cannot replace manifest: " + filesystem_error.message();
		std::filesystem::remove(temporary, filesystem_error);
		return false;
	}
	return true;
}

} // namespace flowui::bake_tool
