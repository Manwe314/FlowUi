#include "devMode/devJson.hpp"

#if FLOW_UI_DEV_MODE && !defined(FLOWUI_SKIP_LEGACY_DEV_ELEMENTS)

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
#include "devMode/devTypes/devEnum1.hpp"
#include "devMode/devTypes/devEnum2.hpp"
#include "devMode/devTypes/devFloat1.hpp"
#include "devMode/devTypes/devFloat2.hpp"
#include "devMode/devTypes/devFloat4.hpp"
#include "devMode/devTypes/devEdgeU16.hpp"
#include "devMode/devTypes/devTaggedUnion.hpp"
#include "devMode/devTypes/devCompositeStruct.hpp"
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
	uint64_t paramsStructTypeHash = 0u;
	std::string paramsStructName{};
	bool hasSourceMetadata = false;
	std::string sourceFile{};
	uint32_t sourceLine = 0u;
	uint32_t sourceColumn = 0u;
	std::string sourceFunction{};
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

void appendJsonEnumValuePayload(std::string& out, uint64_t enumTypeHash, uint8_t numericValue) {
	out += "{";
	out += "\"numeric\":";
	out += std::to_string(numericValue);
	out += ",\"name\":";
	std::string_view enumName{};
	if (tryDevEnum1ValueToName(enumTypeHash, numericValue, enumName)) {
		appendJsonString(out, enumName);
	} else {
		out += "null";
	}
	out += "}";
}

void appendJsonVector2Payload(std::string& out, const DevFloat2Value& value) {
	out += "{";
	out += "\"x\":";
	out += jsonNumberFromDouble(value.first);
	out += ",\"y\":";
	out += jsonNumberFromDouble(value.second);
	out += "}";
}

void appendJsonDimensionsPayload(std::string& out, const DevFloat2Value& value) {
	out += "{";
	out += "\"width\":";
	out += jsonNumberFromDouble(value.first);
	out += ",\"height\":";
	out += jsonNumberFromDouble(value.second);
	out += "}";
}

void appendJsonColorPayload(std::string& out, const DevFloat4Value& value) {
	out += "{";
	out += "\"r\":";
	out += jsonNumberFromDouble(value.first);
	out += ",\"g\":";
	out += jsonNumberFromDouble(value.second);
	out += ",\"b\":";
	out += jsonNumberFromDouble(value.third);
	out += ",\"a\":";
	out += jsonNumberFromDouble(value.fourth);
	out += "}";
}

void appendJsonCornerRadiusPayload(std::string& out, const DevFloat4Value& value) {
	out += "{";
	out += "\"topLeft\":";
	out += jsonNumberFromDouble(value.first);
	out += ",\"topRight\":";
	out += jsonNumberFromDouble(value.second);
	out += ",\"bottomLeft\":";
	out += jsonNumberFromDouble(value.third);
	out += ",\"bottomRight\":";
	out += jsonNumberFromDouble(value.fourth);
	out += "}";
}

void appendJsonPaddingPayload(std::string& out, const DevEdgeU16Value& value) {
	out += "{";
	out += "\"left\":";
	out += std::to_string(value.first);
	out += ",\"right\":";
	out += std::to_string(value.second);
	out += ",\"top\":";
	out += std::to_string(value.third);
	out += ",\"bottom\":";
	out += std::to_string(value.fourth);
	out += "}";
}

void appendJsonBorderWidthPayload(std::string& out, const DevEdgeU16Value& value) {
	out += "{";
	out += "\"left\":";
	out += std::to_string(value.first);
	out += ",\"right\":";
	out += std::to_string(value.second);
	out += ",\"top\":";
	out += std::to_string(value.third);
	out += ",\"bottom\":";
	out += std::to_string(value.fourth);
	out += ",\"betweenChildren\":";
	out += std::to_string(value.fifth);
	out += "}";
}

void appendJsonChildAlignmentPayload(std::string& out, const DevEnum2Value& value) {
	out += "{";
	out += "\"x\":";
	appendJsonEnumValuePayload(out, typeHash<Clay_LayoutAlignmentX>(), value.first.numeric);
	out += ",\"y\":";
	appendJsonEnumValuePayload(out, typeHash<Clay_LayoutAlignmentY>(), value.second.numeric);
	out += "}";
}

void appendJsonFloatingAttachPointsPayload(std::string& out, const DevEnum2Value& value) {
	out += "{";
	out += "\"element\":";
	appendJsonEnumValuePayload(out, typeHash<Clay_FloatingAttachPointType>(), value.first.numeric);
	out += ",\"parent\":";
	appendJsonEnumValuePayload(out, typeHash<Clay_FloatingAttachPointType>(), value.second.numeric);
	out += "}";
}

void appendJsonSizingAxisPayload(std::string& out, const DevTaggedUnionValue& value) {
	out += "{";
	out += "\"tag\":";
	appendJsonEnumValuePayload(out, typeHash<Clay__SizingType>(), value.tag.numeric);
	if (devSizingAxisTagUsesPercent(value.tag.numeric)) {
		out += ",\"active\":\"percent\"";
		out += ",\"percent\":";
		out += jsonNumberFromDouble(value.percent);
	} else {
		out += ",\"active\":\"minMax\"";
		out += ",\"minMax\":{";
		out += "\"min\":";
		out += jsonNumberFromDouble(value.minMax.first);
		out += ",\"max\":";
		out += jsonNumberFromDouble(value.minMax.second);
		out += "}";
	}
	out += "}";
}

void appendJsonSizingPayload(std::string& out, const DevSizingValue& value) {
	out += "{";
	out += "\"width\":";
	appendJsonSizingAxisPayload(out, value.width);
	out += ",\"height\":";
	appendJsonSizingAxisPayload(out, value.height);
	out += "}";
}

void appendJsonLayoutConfigPayload(std::string& out, const DevLayoutConfigValue& value) {
	out += "{";
	out += "\"sizing\":";
	appendJsonSizingPayload(out, value.sizing);
	out += ",\"padding\":";
	appendJsonPaddingPayload(out, value.padding);
	out += ",\"childGap\":";
	out += std::to_string(value.childGap);
	out += ",\"childAlignment\":";
	appendJsonChildAlignmentPayload(out, value.childAlignment);
	out += ",\"layoutDirection\":";
	appendJsonEnumValuePayload(out, typeHash<Clay_LayoutDirection>(), value.layoutDirection.numeric);
	out += "}";
}

void appendJsonTextElementConfigPayload(std::string& out, const DevTextElementConfigValue& value) {
	out += "{";
	out += "\"userData\":";
	out += std::to_string(value.userData.bits);
	out += ",\"textColor\":";
	appendJsonColorPayload(out, value.textColor);
	out += ",\"fontId\":";
	out += std::to_string(value.fontId);
	out += ",\"fontSize\":";
	out += std::to_string(value.fontSize);
	out += ",\"letterSpacing\":";
	out += std::to_string(value.letterSpacing);
	out += ",\"lineHeight\":";
	out += std::to_string(value.lineHeight);
	out += ",\"wrapMode\":";
	appendJsonEnumValuePayload(out, typeHash<Clay_TextElementConfigWrapMode>(), value.wrapMode.numeric);
	out += ",\"textAlignment\":";
	appendJsonEnumValuePayload(out, typeHash<Clay_TextAlignment>(), value.textAlignment.numeric);
	out += "}";
}

void appendJsonFloatingElementConfigPayload(std::string& out, const DevFloatingElementConfigValue& value) {
	out += "{";
	out += "\"offset\":";
	appendJsonVector2Payload(out, value.offset);
	out += ",\"expand\":";
	appendJsonDimensionsPayload(out, value.expand);
	out += ",\"parentId\":";
	out += std::to_string(value.parentId);
	out += ",\"zIndex\":";
	out += std::to_string(value.zIndex);
	out += ",\"attachPoints\":";
	appendJsonFloatingAttachPointsPayload(out, value.attachPoints);
	out += ",\"pointerCaptureMode\":";
	appendJsonEnumValuePayload(out, typeHash<Clay_PointerCaptureMode>(), value.pointerCaptureMode.numeric);
	out += ",\"attachTo\":";
	appendJsonEnumValuePayload(out, typeHash<Clay_FloatingAttachToElement>(), value.attachTo.numeric);
	out += ",\"clipTo\":";
	appendJsonEnumValuePayload(out, typeHash<Clay_FloatingClipToElement>(), value.clipTo.numeric);
	out += "}";
}

void appendJsonClipElementConfigPayload(std::string& out, const DevClipElementConfigValue& value) {
	out += "{";
	out += "\"horizontal\":";
	out += (value.horizontal ? "true" : "false");
	out += ",\"vertical\":";
	out += (value.vertical ? "true" : "false");
	out += ",\"childOffset\":";
	appendJsonVector2Payload(out, value.childOffset);
	out += "}";
}

void appendJsonBorderElementConfigPayload(std::string& out, const DevBorderElementConfigValue& value) {
	out += "{";
	out += "\"color\":";
	appendJsonColorPayload(out, value.color);
	out += ",\"width\":";
	appendJsonBorderWidthPayload(out, value.width);
	out += "}";
}

void appendJsonElementIdPayload(std::string& out, const DevElementIdValue& value) {
	out += "{";
	out += "\"id\":";
	out += std::to_string(value.id);
	out += ",\"offset\":";
	out += std::to_string(value.offset);
	out += ",\"baseId\":";
	out += std::to_string(value.baseId);
	out += ",\"stringId\":";
	appendJsonString(out, value.stringId);
	out += ",\"isStaticallyAllocated\":";
	out += (value.isStaticallyAllocated ? "true" : "false");
	out += "}";
}

void appendJsonElementDeclarationPayload(std::string& out, const DevElementDeclarationValue& value) {
	out += "{";
	out += "\"id\":";
	appendJsonElementIdPayload(out, value.id);
	out += ",\"layout\":";
	appendJsonLayoutConfigPayload(out, value.layout);
	out += ",\"backgroundColor\":";
	appendJsonColorPayload(out, value.backgroundColor);
	out += ",\"cornerRadius\":";
	appendJsonCornerRadiusPayload(out, value.cornerRadius);
	out += ",\"aspectRatio\":{";
	out += "\"aspectRatio\":";
	out += jsonNumberFromDouble(value.aspectRatio);
	out += "}";
	out += ",\"image\":{";
	out += "\"imageData\":";
	out += std::to_string(value.imageData.bits);
	out += "}";
	out += ",\"floating\":";
	appendJsonFloatingElementConfigPayload(out, value.floating);
	out += ",\"custom\":{";
	out += "\"customData\":";
	out += std::to_string(value.customData.bits);
	out += "}";
	out += ",\"clip\":";
	appendJsonClipElementConfigPayload(out, value.clip);
	out += ",\"border\":";
	appendJsonBorderElementConfigPayload(out, value.border);
	out += ",\"userData\":";
	out += std::to_string(value.userData.bits);
	out += "}";
}

void appendJsonDevValue(std::string& out, const DevValue& value, uint64_t fieldTypeHash) {
	out += "{";
	if (const bool* boolValue = std::get_if<bool>(&value)) {
		out += "\"kind\":\"bool\",\"value\":";
		out += (*boolValue ? "true" : "false");
	} else if (const int64_t* intValue = std::get_if<int64_t>(&value)) {
		out += "\"kind\":\"int64\",\"value\":";
		out += std::to_string(*intValue);
	} else if (const double* doubleValue = std::get_if<double>(&value)) {
		const DevFloat1TypeInfo* float1Info = findDevFloat1TypeInfo(fieldTypeHash);
		if (float1Info != nullptr) {
			double normalized = 0.0;
			if (!tryMakeDevFloat1Value(fieldTypeHash, *doubleValue, normalized)) {
				out += "\"kind\":\"double\",\"value\":";
				out += jsonNumberFromDouble(*doubleValue);
			} else {
				out += "\"kind\":\"float1\",\"value\":{";
				out += "\"type\":";
				appendJsonString(out, float1Info->typeName);
				out += ",";
				appendJsonString(out, float1Info->valueFieldName);
				out += ":";
				out += jsonNumberFromDouble(normalized);
				out += "}";
			}
		} else {
			out += "\"kind\":\"double\",\"value\":";
			out += jsonNumberFromDouble(*doubleValue);
		}
	} else if (const std::string* textValue = std::get_if<std::string>(&value)) {
		out += "\"kind\":\"string\",\"value\":";
		appendJsonString(out, *textValue);
	} else if (const DevEnum1Value* enumValue = std::get_if<DevEnum1Value>(&value)) {
		out += "\"kind\":\"enum1\",\"value\":{";
		out += "\"numeric\":";
		out += std::to_string(enumValue->numeric);
		out += ",\"name\":";
		std::string_view enumName{};
		if (tryDevEnum1ValueToName(fieldTypeHash, enumValue->numeric, enumName)) {
			appendJsonString(out, enumName);
		} else {
			out += "null";
		}
		out += "}";
	} else if (const DevEnum2Value* enumValue = std::get_if<DevEnum2Value>(&value)) {
		out += "\"kind\":\"enum2\",\"value\":{";

		const DevEnum2TypeInfo* enum2Info = findDevEnum2TypeInfo(fieldTypeHash);
		if (enum2Info == nullptr) {
			out += "\"type\":null";
		} else {
			out += "\"type\":";
			appendJsonString(out, enum2Info->typeName);
			out += ",";
			appendJsonString(out, enum2Info->firstFieldName);
			out += ":";
			appendJsonEnumValuePayload(out, enum2Info->firstEnumTypeHash, enumValue->first.numeric);
			out += ",";
			appendJsonString(out, enum2Info->secondFieldName);
			out += ":";
			appendJsonEnumValuePayload(out, enum2Info->secondEnumTypeHash, enumValue->second.numeric);
		}
		out += "}";
	} else if (const DevFloat2Value* floatValue = std::get_if<DevFloat2Value>(&value)) {
		out += "\"kind\":\"float2\",\"value\":{";

		const DevFloat2TypeInfo* float2Info = findDevFloat2TypeInfo(fieldTypeHash);
		if (float2Info == nullptr) {
			out += "\"type\":null";
		} else {
			out += "\"type\":";
			appendJsonString(out, float2Info->typeName);
			out += ",";
			appendJsonString(out, float2Info->firstFieldName);
			out += ":";
			out += jsonNumberFromDouble(floatValue->first);
			out += ",";
			appendJsonString(out, float2Info->secondFieldName);
			out += ":";
			out += jsonNumberFromDouble(floatValue->second);
		}
		out += "}";
	} else if (const DevFloat4Value* floatValue = std::get_if<DevFloat4Value>(&value)) {
		out += "\"kind\":\"float4\",\"value\":{";

		const DevFloat4TypeInfo* float4Info = findDevFloat4TypeInfo(fieldTypeHash);
		if (float4Info == nullptr) {
			out += "\"type\":null";
		} else {
			out += "\"type\":";
			appendJsonString(out, float4Info->typeName);
			out += ",";
			appendJsonString(out, float4Info->firstFieldName);
			out += ":";
			out += jsonNumberFromDouble(floatValue->first);
			out += ",";
			appendJsonString(out, float4Info->secondFieldName);
			out += ":";
			out += jsonNumberFromDouble(floatValue->second);
			out += ",";
			appendJsonString(out, float4Info->thirdFieldName);
			out += ":";
			out += jsonNumberFromDouble(floatValue->third);
			out += ",";
			appendJsonString(out, float4Info->fourthFieldName);
			out += ":";
			out += jsonNumberFromDouble(floatValue->fourth);
		}
		out += "}";
	} else if (const DevEdgeU16Value* edgeValue = std::get_if<DevEdgeU16Value>(&value)) {
		out += "\"kind\":\"edgeu16\",\"value\":{";

		const DevEdgeU16TypeInfo* edgeInfo = findDevEdgeU16TypeInfo(fieldTypeHash);
		if (edgeInfo == nullptr) {
			out += "\"type\":null";
		} else {
			out += "\"type\":";
			appendJsonString(out, edgeInfo->typeName);
			out += ",";
			appendJsonString(out, edgeInfo->firstFieldName);
			out += ":";
			out += std::to_string(edgeValue->first);
			out += ",";
			appendJsonString(out, edgeInfo->secondFieldName);
			out += ":";
			out += std::to_string(edgeValue->second);
			out += ",";
			appendJsonString(out, edgeInfo->thirdFieldName);
			out += ":";
			out += std::to_string(edgeValue->third);
			out += ",";
			appendJsonString(out, edgeInfo->fourthFieldName);
			out += ":";
			out += std::to_string(edgeValue->fourth);
			if (edgeInfo->fifthFieldUsed) {
				out += ",";
				appendJsonString(out, edgeInfo->fifthFieldName);
				out += ":";
				out += std::to_string(edgeValue->fifth);
			}
		}
		out += "}";
	} else if (const DevTaggedUnionValue* taggedValue = std::get_if<DevTaggedUnionValue>(&value)) {
		out += "\"kind\":\"tagged_union\",\"value\":{";

		const DevTaggedUnionTypeInfo* taggedInfo = findDevTaggedUnionTypeInfo(fieldTypeHash);
		DevTaggedUnionValue normalized{};
		if (
			taggedInfo == nullptr ||
			!tryMakeDevTaggedUnionValue(
				fieldTypeHash,
				static_cast<int64_t>(taggedValue->tag.numeric),
				taggedValue->minMax.first,
				taggedValue->minMax.second,
				taggedValue->percent,
				normalized))
		{
			out += "\"type\":null";
		} else {
			out += "\"type\":";
			appendJsonString(out, taggedInfo->typeName);
			out += ",\"tagField\":";
			appendJsonString(out, taggedInfo->tagFieldName);
			out += ",\"tag\":";
			appendJsonEnumValuePayload(out, taggedInfo->tagEnumTypeHash, normalized.tag.numeric);

			if (devSizingAxisTagUsesPercent(normalized.tag.numeric)) {
				out += ",\"active\":";
				appendJsonString(out, taggedInfo->percentFieldName);
				out += ",";
				appendJsonString(out, taggedInfo->percentFieldName);
				out += ":";
				out += jsonNumberFromDouble(normalized.percent);
			} else {
				out += ",\"active\":";
				appendJsonString(out, taggedInfo->minMaxFieldName);
				out += ",";
				appendJsonString(out, taggedInfo->minMaxFieldName);
				out += ":{";
				appendJsonString(out, taggedInfo->minFieldName);
				out += ":";
				out += jsonNumberFromDouble(normalized.minMax.first);
				out += ",";
				appendJsonString(out, taggedInfo->maxFieldName);
				out += ":";
				out += jsonNumberFromDouble(normalized.minMax.second);
				out += "}";
			}
		}
		out += "}";
	} else if (const DevCompositeStructValue* compositeValue = std::get_if<DevCompositeStructValue>(&value)) {
		out += "\"kind\":\"composite_struct\",\"value\":{";

		const DevCompositeStructTypeInfo* compositeInfo = findDevCompositeStructTypeInfo(fieldTypeHash);
		DevCompositeStructValue normalized{};
		if (
			compositeInfo == nullptr ||
			!tryMakeDevCompositeStructValue(fieldTypeHash, *compositeValue, normalized))
		{
			out += "\"type\":null";
		} else {
			out += "\"type\":";
			appendJsonString(out, compositeInfo->typeName);

			if (fieldTypeHash == typeHash<Clay_Sizing>()) {
				out += ",\"sizing\":";
				appendJsonSizingPayload(out, normalized.sizing);
			} else if (fieldTypeHash == typeHash<Clay_LayoutConfig>()) {
				out += ",\"layout\":";
				appendJsonLayoutConfigPayload(out, normalized.layoutConfig);
			} else if (fieldTypeHash == typeHash<Clay_TextElementConfig>()) {
				out += ",\"text\":";
				appendJsonTextElementConfigPayload(out, normalized.textElementConfig);
			} else if (fieldTypeHash == typeHash<Clay_FloatingElementConfig>()) {
				out += ",\"floating\":";
				appendJsonFloatingElementConfigPayload(out, normalized.floatingElementConfig);
			} else if (fieldTypeHash == typeHash<Clay_ClipElementConfig>()) {
				out += ",\"clip\":";
				appendJsonClipElementConfigPayload(out, normalized.clipElementConfig);
			} else if (fieldTypeHash == typeHash<Clay_BorderElementConfig>()) {
				out += ",\"border\":";
				appendJsonBorderElementConfigPayload(out, normalized.borderElementConfig);
			} else if (fieldTypeHash == typeHash<Clay_ElementDeclaration>()) {
				out += ",\"declaration\":";
				appendJsonElementDeclarationPayload(out, normalized.elementDeclaration);
			}
		}

		out += "}";
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
		out += "\"paramsStructTypeHash\":";
		out += std::to_string(group.paramsStructTypeHash);
		out += ",\n";
		appendJsonIndent(out, 3);
		out += "\"paramsStructName\":";
		appendJsonString(out, group.paramsStructName);
		out += ",\n";
		appendJsonIndent(out, 3);
		out += "\"hasSourceMetadata\":";
		out += (group.hasSourceMetadata ? "true" : "false");
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
			appendJsonDevValue(out, change.value, change.fieldTypeHash);
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
			appendJsonDevValue(out, change.value, change.fieldTypeHash);
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
				group.paramsStructTypeHash = descriptor->paramsStructTypeHash;

				const StructDescriptor* paramsStruct =
					registry.findStructByTypeHash(descriptor->paramsStructTypeHash);
				if (paramsStruct != nullptr) {
					group.paramsStructName = paramsStruct->name;
					if (group.paramsStructName.empty()) {
						group.paramsStructName = paramsStruct->typeToken;
					}

					group.hasSourceMetadata = paramsStruct->hasSourceMetadata;
					group.sourceFile = paramsStruct->sourceFile;
					group.sourceLine = paramsStruct->sourceLine;
					group.sourceColumn = paramsStruct->sourceColumn;
					group.sourceFunction = paramsStruct->sourceFunction;
				}
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
