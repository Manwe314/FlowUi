#include "devMode/devJson.hpp"

#if FLOW_UI_DEV_MODE

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <optional>
#include <sstream>
#include <string>
#include <string_view>
#include <unordered_map>
#include <utility>
#include <vector>

#include "devMode/devRuntime.hpp"
#include "devMode/registry.hpp"
#include "managers/UiManager.hpp"

namespace FlowUi::devMode {
namespace {

struct ExportFieldChange {
	uint64_t fieldHash = 0u;
	uint64_t fieldTypeHash = 0u;
	std::string fieldName{};
	DevValue value{};
};

struct DefinitionExportGroup {
	uint64_t definitionId = 0u;
	std::string definitionName{};
	std::string definitionTypeToken{};
	std::vector<ExportFieldChange> changes{};
};

struct InstanceExportKey {
	uint64_t definitionId = 0u;
	uint64_t flowId = 0u;
	std::string elementId{};

	bool operator==(const InstanceExportKey& other) const {
		return
			definitionId == other.definitionId &&
			flowId == other.flowId &&
			elementId == other.elementId;
	}
};

struct InstanceExportKeyHash {
	std::size_t operator()(const InstanceExportKey& key) const noexcept {
		std::size_t hash = std::hash<uint64_t>{}(key.definitionId);
		hash ^= std::hash<uint64_t>{}(key.flowId) + 0x9e3779b97f4a7c15ull + (hash << 6u) + (hash >> 2u);
		hash ^= std::hash<std::string>{}(key.elementId) + 0x9e3779b97f4a7c15ull + (hash << 6u) + (hash >> 2u);
		return hash;
	}
};

struct InstanceExportGroup {
	uint64_t definitionId = 0u;
	uint64_t flowId = 0u;
	std::string elementId{};
	std::string definitionName{};
	std::string definitionTypeToken{};
	std::string authoredInstanceKey{};
	std::string authoredDefinitionKey{};
	std::string sourceFile{};
	uint32_t sourceLine = 0u;
	uint32_t sourceColumn = 0u;
	std::string sourceFunction{};
	uint64_t sourceLocationHash = 0u;
	std::vector<ExportFieldChange> changes{};
};

void appendJsonEscaped(std::string& out, std::string_view text) {
	for (char c : text) {
		switch (c) {
		case '\"':
			out += "\\\"";
			break;
		case '\\':
			out += "\\\\";
			break;
		case '\b':
			out += "\\b";
			break;
		case '\f':
			out += "\\f";
			break;
		case '\n':
			out += "\\n";
			break;
		case '\r':
			out += "\\r";
			break;
		case '\t':
			out += "\\t";
			break;
		default:
			if (static_cast<unsigned char>(c) < 0x20u) {
				static constexpr char kHex[] = "0123456789abcdef";
				out += "\\u00";
				out.push_back(kHex[(static_cast<unsigned char>(c) >> 4u) & 0x0Fu]);
				out.push_back(kHex[static_cast<unsigned char>(c) & 0x0Fu]);
			} else {
				out.push_back(c);
			}
			break;
		}
	}
}

void appendJsonString(std::string& out, std::string_view text) {
	out.push_back('"');
	appendJsonEscaped(out, text);
	out.push_back('"');
}

void appendJsonIndent(std::string& out, int indentLevel) {
	out.append(static_cast<std::size_t>(std::max(0, indentLevel) * 2), ' ');
}

std::string jsonNumberFromDouble(double value) {
	if (!std::isfinite(value)) {
		return "null";
	}
	std::ostringstream stream{};
	stream.precision(17);
	stream << value;
	return stream.str();
}

void appendJsonDevValue(std::string& out, const DevValue& value) {
	out += "{";
	if (const bool* boolValue = std::get_if<bool>(&value)) {
		out += "\"kind\":\"bool\",\"value\":";
		out += (*boolValue ? "true" : "false");
	} else if (const int64_t* intValue = std::get_if<int64_t>(&value)) {
		out += "\"kind\":\"int64\",\"value\":";
		out += std::to_string(*intValue);
	} else if (const double* doubleValue = std::get_if<double>(&value)) {
		out += "\"kind\":\"double\",\"value\":";
		out += jsonNumberFromDouble(*doubleValue);
	} else if (const std::string* textValue = std::get_if<std::string>(&value)) {
		out += "\"kind\":\"string\",\"value\":";
		appendJsonString(out, *textValue);
	} else {
		out += "\"kind\":\"null\",\"value\":null";
	}
	out += "}";
}

const ElementDescriptor* findDefinitionDescriptor(const DevRegistry& registry, uint64_t definitionId) {
	return registry.findElementByDefinitionId(definitionId);
}

std::optional<ExportFieldChange> buildExportFieldChange(
	const DevRegistry& registry,
	uint64_t definitionId,
	uint64_t fieldHash,
	const DevValue& value) {
	ExportFieldChange change{};
	change.fieldHash = fieldHash;
	change.fieldName = "field_" + std::to_string(fieldHash);
	change.value = value;

	const ElementDescriptor* element = findDefinitionDescriptor(registry, definitionId);
	if (element == nullptr) {
		return change;
	}

	const StructDescriptor* paramsStruct = registry.findStructByTypeHash(element->paramsStructTypeHash);
	if (paramsStruct == nullptr) {
		return change;
	}

	for (const FieldDescriptor& field : paramsStruct->fields) {
		const uint64_t registeredHash = (field.fieldHash == 0u) ? hashString64(field.name) : field.fieldHash;
		if (registeredHash != fieldHash) {
			continue;
		}
		change.fieldName = field.name;
		change.fieldTypeHash = field.fieldTypeHash;
		return change;
	}

	return change;
}

const ElementTreePlaceholder::FlatNode* findCapturedInstanceNode(
	const DevRuntime& runtime,
	uint64_t definitionId,
	uint64_t flowId,
	std::string_view elementId) {
	const auto& flatNodes = runtime.elementTreePlaceholder().flatNodes;
	for (const ElementTreePlaceholder::FlatNode& node : flatNodes) {
		if (node.kind != ElementTreePlaceholder::ElementKind::FlowElement) {
			continue;
		}
		if (node.definitionId != definitionId || node.flowId != flowId || node.elementId != elementId) {
			continue;
		}
		return &node;
	}
	return nullptr;
}

void sortChanges(std::vector<ExportFieldChange>& changes) {
	std::sort(
		changes.begin(),
		changes.end(),
		[](const ExportFieldChange& lhs, const ExportFieldChange& rhs) {
			if (lhs.fieldName != rhs.fieldName) {
				return lhs.fieldName < rhs.fieldName;
			}
			return lhs.fieldHash < rhs.fieldHash;
		});
}

std::string buildExportJson(
	const std::vector<DefinitionExportGroup>& definitionGroups,
	const std::vector<InstanceExportGroup>& instanceGroups) {
	std::string out{};
	out.reserve(4096u);

	out += "{\n";
	appendJsonIndent(out, 1);
	out += "\"schema\":";
	appendJsonString(out, "flowui.dev.export.v1");
	out += ",\n";
	appendJsonIndent(out, 1);
	out += "\"kind\":";
	appendJsonString(out, "params");
	out += ",\n";
	appendJsonIndent(out, 1);
	out += "\"definitions\": [\n";

	for (std::size_t i = 0; i < definitionGroups.size(); ++i) {
		const DefinitionExportGroup& group = definitionGroups[i];
		appendJsonIndent(out, 2);
		out += "{\n";
		appendJsonIndent(out, 3);
		out += "\"definitionId\":";
		out += std::to_string(group.definitionId);
		out += ",\n";
		appendJsonIndent(out, 3);
		out += "\"definitionName\":";
		appendJsonString(out, group.definitionName);
		out += ",\n";
		appendJsonIndent(out, 3);
		out += "\"definitionTypeToken\":";
		appendJsonString(out, group.definitionTypeToken);
		out += ",\n";
		appendJsonIndent(out, 3);
		out += "\"hasSourceMetadata\":false,\n";
		appendJsonIndent(out, 3);
		out += "\"changes\": [\n";

		for (std::size_t changeIndex = 0; changeIndex < group.changes.size(); ++changeIndex) {
			const ExportFieldChange& change = group.changes[changeIndex];
			appendJsonIndent(out, 4);
			out += "{";
			out += "\"fieldHash\":";
			out += std::to_string(change.fieldHash);
			out += ",\"fieldName\":";
			appendJsonString(out, change.fieldName);
			out += ",\"fieldTypeHash\":";
			out += std::to_string(change.fieldTypeHash);
			out += ",\"value\":";
			appendJsonDevValue(out, change.value);
			out += "}";
			out += (changeIndex + 1u < group.changes.size()) ? ",\n" : "\n";
		}

		appendJsonIndent(out, 3);
		out += "]\n";
		appendJsonIndent(out, 2);
		out += "}";
		out += (i + 1u < definitionGroups.size()) ? ",\n" : "\n";
	}

	appendJsonIndent(out, 1);
	out += "],\n";
	appendJsonIndent(out, 1);
	out += "\"instances\": [\n";

	for (std::size_t i = 0; i < instanceGroups.size(); ++i) {
		const InstanceExportGroup& group = instanceGroups[i];
		appendJsonIndent(out, 2);
		out += "{\n";
		appendJsonIndent(out, 3);
		out += "\"definitionId\":";
		out += std::to_string(group.definitionId);
		out += ",\n";
		appendJsonIndent(out, 3);
		out += "\"definitionName\":";
		appendJsonString(out, group.definitionName);
		out += ",\n";
		appendJsonIndent(out, 3);
		out += "\"definitionTypeToken\":";
		appendJsonString(out, group.definitionTypeToken);
		out += ",\n";
		appendJsonIndent(out, 3);
		out += "\"flowId\":";
		out += std::to_string(group.flowId);
		out += ",\n";
		appendJsonIndent(out, 3);
		out += "\"elementId\":";
		appendJsonString(out, group.elementId);
		out += ",\n";
		appendJsonIndent(out, 3);
		out += "\"authoredInstanceKey\":";
		appendJsonString(out, group.authoredInstanceKey);
		out += ",\n";
		appendJsonIndent(out, 3);
		out += "\"authoredDefinitionKey\":";
		appendJsonString(out, group.authoredDefinitionKey);
		out += ",\n";
		appendJsonIndent(out, 3);
		out += "\"sourceFile\":";
		appendJsonString(out, group.sourceFile);
		out += ",\n";
		appendJsonIndent(out, 3);
		out += "\"sourceLine\":";
		out += std::to_string(group.sourceLine);
		out += ",\n";
		appendJsonIndent(out, 3);
		out += "\"sourceColumn\":";
		out += std::to_string(group.sourceColumn);
		out += ",\n";
		appendJsonIndent(out, 3);
		out += "\"sourceFunction\":";
		appendJsonString(out, group.sourceFunction);
		out += ",\n";
		appendJsonIndent(out, 3);
		out += "\"sourceLocationHash\":";
		out += std::to_string(group.sourceLocationHash);
		out += ",\n";
		appendJsonIndent(out, 3);
		out += "\"changes\": [\n";

		for (std::size_t changeIndex = 0; changeIndex < group.changes.size(); ++changeIndex) {
			const ExportFieldChange& change = group.changes[changeIndex];
			appendJsonIndent(out, 4);
			out += "{";
			out += "\"fieldHash\":";
			out += std::to_string(change.fieldHash);
			out += ",\"fieldName\":";
			appendJsonString(out, change.fieldName);
			out += ",\"fieldTypeHash\":";
			out += std::to_string(change.fieldTypeHash);
			out += ",\"value\":";
			appendJsonDevValue(out, change.value);
			out += "}";
			out += (changeIndex + 1u < group.changes.size()) ? ",\n" : "\n";
		}

		appendJsonIndent(out, 3);
		out += "]\n";
		appendJsonIndent(out, 2);
		out += "}";
		out += (i + 1u < instanceGroups.size()) ? ",\n" : "\n";
	}

	appendJsonIndent(out, 1);
	out += "]\n";
	out += "}\n";

	return out;
}

bool writeJsonFile(std::filesystem::path path, std::string_view content) {
	if (path.empty()) {
		return false;
	}

	std::error_code createDirError{};
	const std::filesystem::path parent = path.parent_path();
	if (!parent.empty()) {
		(void)std::filesystem::create_directories(parent, createDirError);
		if (createDirError) {
			std::fprintf(
				stderr,
				"[FlowUi][Dev] Failed to create export directory '%s': %s\n",
				parent.string().c_str(),
				createDirError.message().c_str());
			return false;
		}
	}

	std::ofstream stream(path, std::ios::out | std::ios::binary | std::ios::trunc);
	if (!stream.is_open()) {
		std::fprintf(
			stderr,
			"[FlowUi][Dev] Failed to open export file '%s' for writing.\n",
			path.string().c_str());
		return false;
	}

	stream.write(content.data(), static_cast<std::streamsize>(content.size()));
	stream.flush();
	if (!stream.good()) {
		std::fprintf(
			stderr,
			"[FlowUi][Dev] Failed while writing export file '%s'.\n",
			path.string().c_str());
		return false;
	}

	return true;
}

} // namespace

bool exportOverridesAsJson(UiManager& uiManager) {
	DevRuntime& runtime = uiManager.devRuntime();
	const DevRegistry& registry = DevRegistry::instance();

	std::vector<DefinitionExportGroup> definitionGroups{};
	std::unordered_map<uint64_t, std::size_t> definitionIndexById{};
	definitionIndexById.reserve(runtime.definitionParamOverrides().size());

	for (const auto& [key, value] : runtime.definitionParamOverrides()) {
		std::size_t index = definitionGroups.size();
		const auto it = definitionIndexById.find(key.definitionId);
		if (it != definitionIndexById.end()) {
			index = it->second;
		} else {
			DefinitionExportGroup group{};
			group.definitionId = key.definitionId;
			if (const ElementDescriptor* descriptor = findDefinitionDescriptor(registry, key.definitionId)) {
				group.definitionName = descriptor->definitionName;
				group.definitionTypeToken = descriptor->definitionTypeToken;
			}
			if (group.definitionName.empty()) {
				group.definitionName = "UnknownDefinition";
			}
			definitionIndexById[key.definitionId] = index;
			definitionGroups.push_back(std::move(group));
		}

		const std::optional<ExportFieldChange> change =
			buildExportFieldChange(registry, key.definitionId, key.fieldHash, value);
		if (change.has_value()) {
			definitionGroups[index].changes.push_back(*change);
		}
	}

	std::vector<InstanceExportGroup> instanceGroups{};
	std::unordered_map<InstanceExportKey, std::size_t, InstanceExportKeyHash> instanceIndexByKey{};
	instanceIndexByKey.reserve(runtime.instanceParamOverrides().size());

	for (const auto& [key, value] : runtime.instanceParamOverrides()) {
		const InstanceExportKey groupKey{
			.definitionId = key.definitionId,
			.flowId = key.flowId,
			.elementId = key.elementId,
		};

		std::size_t index = instanceGroups.size();
		const auto it = instanceIndexByKey.find(groupKey);
		if (it != instanceIndexByKey.end()) {
			index = it->second;
		} else {
			InstanceExportGroup group{};
			group.definitionId = key.definitionId;
			group.flowId = key.flowId;
			group.elementId = key.elementId;

			if (const ElementDescriptor* descriptor = findDefinitionDescriptor(registry, key.definitionId)) {
				group.definitionName = descriptor->definitionName;
				group.definitionTypeToken = descriptor->definitionTypeToken;
			}
			if (group.definitionName.empty()) {
				group.definitionName = "UnknownDefinition";
			}

			const ElementTreePlaceholder::FlatNode* node =
				findCapturedInstanceNode(runtime, key.definitionId, key.flowId, key.elementId);
			if (node != nullptr) {
				group.authoredInstanceKey = node->authoredInstanceKey;
				group.authoredDefinitionKey = node->authoredDefinitionKey;
				group.sourceFile = node->sourceFile;
				group.sourceLine = node->sourceLine;
				group.sourceColumn = node->sourceColumn;
				group.sourceFunction = node->sourceFunction;
				group.sourceLocationHash = node->sourceLocationHash;
			}
			if (group.authoredInstanceKey.empty()) {
				group.authoredInstanceKey = group.elementId;
			}

			instanceIndexByKey[groupKey] = index;
			instanceGroups.push_back(std::move(group));
		}

		const std::optional<ExportFieldChange> change =
			buildExportFieldChange(registry, key.definitionId, key.fieldHash, value);
		if (change.has_value()) {
			instanceGroups[index].changes.push_back(*change);
		}
	}

	for (DefinitionExportGroup& group : definitionGroups) {
		sortChanges(group.changes);
	}
	for (InstanceExportGroup& group : instanceGroups) {
		sortChanges(group.changes);
	}

	std::sort(
		definitionGroups.begin(),
		definitionGroups.end(),
		[](const DefinitionExportGroup& lhs, const DefinitionExportGroup& rhs) {
			return lhs.definitionId < rhs.definitionId;
		});
	std::sort(
		instanceGroups.begin(),
		instanceGroups.end(),
		[](const InstanceExportGroup& lhs, const InstanceExportGroup& rhs) {
			if (lhs.definitionId != rhs.definitionId) {
				return lhs.definitionId < rhs.definitionId;
			}
			if (lhs.flowId != rhs.flowId) {
				return lhs.flowId < rhs.flowId;
			}
			return lhs.elementId < rhs.elementId;
		});

	const std::string json = buildExportJson(definitionGroups, instanceGroups);
	const std::filesystem::path outputPath = uiManager.devToolsConfig().overridesPath;
	const bool wrote = writeJsonFile(outputPath, json);
	if (!wrote) {
		return false;
	}

	std::fprintf(
		stderr,
		"[FlowUi][Dev] Exported overrides JSON to '%s' (%zu definition target(s), %zu instance target(s)).\n",
		outputPath.string().c_str(),
		definitionGroups.size(),
		instanceGroups.size());
	return true;
}

} // namespace FlowUi::devMode

#endif
