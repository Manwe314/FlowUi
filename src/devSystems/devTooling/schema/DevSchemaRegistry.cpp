#include "devSystems/devTooling/schema/DevSchemaRegistry.hpp"

#if FLOW_UI_DEV_MODE

#include <algorithm>
#include <memory>
#include <unordered_map>

namespace FlowUi::devMode {
namespace {

class StringTableBuilder {
public:
	StringTableBuilder(std::vector<char>& destination, std::uint32_t maximumBytes)
		: destination_(destination), maximumBytes_(maximumBytes) {}

	DevStringRef intern(std::string_view value) {
		if (value.empty()) return {};
		if (const auto found = refs_.find(std::string(value)); found != refs_.end()) {
			return found->second;
		}
		if (value.size() > maximumBytes_ || destination_.size() > maximumBytes_ - value.size()) {
			exceeded_ = true;
			return {};
		}
		const DevStringRef ref{
			.offset = static_cast<std::uint32_t>(destination_.size()),
			.size = static_cast<std::uint32_t>(value.size()),
		};
		destination_.insert(destination_.end(), value.begin(), value.end());
		refs_.emplace(std::string(value), ref);
		return ref;
	}

	[[nodiscard]] bool exceeded() const noexcept { return exceeded_; }

private:
	std::vector<char>& destination_;
	std::uint32_t maximumBytes_ = 0;
	bool exceeded_ = false;
	std::unordered_map<std::string, DevStringRef> refs_{};
};

void hashBytes(std::uint64_t& hash, const void* data, std::size_t size) noexcept {
	const auto* bytes = static_cast<const unsigned char*>(data);
	for (std::size_t index = 0; index < size; ++index) {
		hash ^= bytes[index];
		hash *= 1099511628211ull;
	}
}

template <typename T>
void hashValue(std::uint64_t& hash, const T& value) noexcept {
	hashBytes(hash, std::addressof(value), sizeof(T));
}

void hashString(std::uint64_t& hash, std::string_view value) noexcept {
	hashBytes(hash, value.data(), value.size());
	const std::uint8_t terminator = 0xff;
	hashValue(hash, terminator);
}

} // namespace

const DevTypeSchema* DevSchemaGeneration::findType(DevTypeId id) const noexcept {
	const auto first = types.begin() + static_cast<std::ptrdiff_t>(std::min<std::size_t>(1, types.size()));
	const auto found = std::lower_bound(first, types.end(), id,
		[](const DevTypeSchema& type, DevTypeId value) { return type.id < value; });
	return found != types.end() && found->id == id ? std::addressof(*found) : nullptr;
}

const DevElementSchema* DevSchemaGeneration::findElement(FlowDefinitionID id) const noexcept {
	const auto found = std::lower_bound(elements.begin(), elements.end(), id.value,
		[](const DevElementSchema& element, std::uint64_t value) {
			return element.definitionId.value < value;
		});
	return found != elements.end() && found->definitionId == id ? std::addressof(*found) : nullptr;
}

const DevThemeSchema* DevSchemaGeneration::findTheme(DevTypeId id) const noexcept {
	const DevTypeSchema* schema = findType(id);
	if (schema == nullptr) return nullptr;
	const auto typeIndex = static_cast<std::uint32_t>(schema - types.data());
	const auto found = std::lower_bound(themes.begin(), themes.end(), typeIndex,
		[](const DevThemeSchema& theme, std::uint32_t value) {
			return theme.themeType.value < value;
		});
	return found != themes.end() && found->themeType.value == typeIndex
		? std::addressof(*found) : nullptr;
}

std::span<const DevFieldSchema> DevSchemaGeneration::fieldsOf(DevTypeIndex typeIndex) const noexcept {
	const DevTypeSchema* schema = type(typeIndex);
	if (!schema || schema->fields.first > fields.size() ||
		schema->fields.count > fields.size() - schema->fields.first) return {};
	return {fields.data() + schema->fields.first, schema->fields.count};
}

DevSchemaRegistry::DevSchemaRegistry(DevSchemaLimits limits) : limits_(limits) {
	auto empty = std::make_shared<DevSchemaGeneration>();
	empty->types.emplace_back();
	empty->constraints.emplace_back();
	empty->typeOperations.push_back(nullptr);
	empty->fieldOperations.push_back(nullptr);
	published_ = std::move(empty);
}

DevSchemaRegistry::~DevSchemaRegistry() = default;

std::uint64_t DevSchemaRegistry::rootKey(DevRootKind kind, std::uint64_t identity) noexcept {
	std::uint64_t hash = 14695981039346656037ull;
	const auto kindValue = static_cast<std::uint8_t>(kind);
	hashValue(hash, kindValue);
	hashValue(hash, identity);
	return hash == 0 ? 1 : hash;
}

DevFieldId DevSchemaRegistry::fieldIdentity(DevTypeId owner, std::string_view name) noexcept {
	std::uint64_t hash = 14695981039346656037ull;
	hashValue(hash, owner);
	hashString(hash, name);
	return hash == 0 ? 1 : hash;
}

void DevSchemaRegistry::queueRoot(std::uint64_t key, PendingRoot root) {
	std::lock_guard lock(mutex_);
	if (!queuedOrIngestedRoots_.insert(key).second) return;
	pendingRoots_.push_back(root);
}

bool DevSchemaRegistry::publishPendingAtSafePoint() {
	std::lock_guard lock(mutex_);
	if (pendingRoots_.empty()) return false;
	std::vector<PendingRoot> roots;
	roots.swap(pendingRoots_);
	for (const PendingRoot& root : roots) {
		if (root.ingest) root.ingest(*this);
	}
	published_ = buildGeneration();
	return true;
}

DevSchemaView DevSchemaRegistry::view() const noexcept {
	std::lock_guard lock(mutex_);
	return DevSchemaView(published_);
}

std::size_t DevSchemaRegistry::pendingRootCount() const noexcept {
	std::lock_guard lock(mutex_);
	return pendingRoots_.size();
}

void DevSchemaRegistry::configureLeaf(
	std::size_t index,
	DevTypeKind kind,
	DevEditorKind editor,
	DevCaptureCapability capture,
	DevEditCapability edit,
	DevCapabilityReason reason) {
	MutableType& type = mutableTypes_[index];
	type.kind = kind;
	type.editor = editor;
	type.capture = capture;
	type.edit = edit;
	type.reason = reason;
	type.state = ResolutionState::Complete;
}

void DevSchemaRegistry::configureCompound(
	std::size_t index,
	DevTypeKind kind,
	DevEditorKind editor,
	DevTypeId elementType,
	std::uint32_t sequenceExtent,
	bool sequenceFixed) {
	MutableType& type = mutableTypes_[index];
	type.kind = kind;
	type.editor = editor;
	type.elementType = elementType;
	type.sequenceExtent = sequenceExtent;
	type.sequenceFixed = sequenceFixed;
	if (const MutableType* child = mutableType(elementType);
		child && child->state == ResolutionState::Visiting) {
		type.capture = DevCaptureCapability::MetadataOnly;
		type.edit = DevEditCapability::Unsupported;
		type.reason = DevCapabilityReason::RecursiveCycle;
	} else if (child) {
		type.capture = child->capture;
		type.edit = child->edit;
		type.reason = child->reason;
	} else {
		type.capture = DevCaptureCapability::None;
		type.edit = DevEditCapability::Unsupported;
		type.reason = DevCapabilityReason::NoCaptureAdapter;
	}
	type.state = ResolutionState::Complete;
}

void DevSchemaRegistry::addDiagnostic(
	DevDiagnosticSeverity severity,
	DevDiagnosticCode code,
	DevTypeId type,
	DevFieldId field,
	std::string_view message) {
	mutableDiagnostics_.push_back(MutableDiagnostic{
		.severity = severity,
		.code = code,
		.type = type,
		.field = field,
		.message = std::string(message),
	});
}

DevSchemaRegistry::MutableType* DevSchemaRegistry::mutableType(DevTypeId id) noexcept {
	const auto found = mutableTypeIndex_.find(id);
	return found == mutableTypeIndex_.end() ? nullptr : &mutableTypes_[found->second];
}

const DevSchemaRegistry::MutableType* DevSchemaRegistry::mutableType(DevTypeId id) const noexcept {
	const auto found = mutableTypeIndex_.find(id);
	return found == mutableTypeIndex_.end() ? nullptr : &mutableTypes_[found->second];
}

std::string DevSchemaRegistry::displayNameForType(DevTypeId id) const {
	const MutableType* type = mutableType(id);
	return type ? type->displayName : std::string{};
}

std::shared_ptr<const DevSchemaGeneration> DevSchemaRegistry::buildGeneration() {
	auto generation = std::make_shared<DevSchemaGeneration>();
	generation->generation = nextGeneration_++;
	generation->types.emplace_back();
	generation->constraints.emplace_back();
	generation->typeOperations.push_back(nullptr);
	generation->fieldOperations.push_back(nullptr);
	StringTableBuilder strings(generation->strings, limits_.maxStringBytes);

	std::vector<const MutableType*> orderedTypes;
	orderedTypes.reserve(mutableTypes_.size());
	for (const MutableType& type : mutableTypes_) orderedTypes.push_back(&type);
	std::sort(orderedTypes.begin(), orderedTypes.end(),
		[](const MutableType* left, const MutableType* right) { return left->id < right->id; });

	std::unordered_map<DevTypeId, DevTypeIndex> indices;
	indices.reserve(orderedTypes.size());
	for (const MutableType* type : orderedTypes) {
		indices.emplace(type->id, DevTypeIndex{static_cast<std::uint32_t>(generation->types.size())});
		generation->types.emplace_back();
	}

	for (std::size_t orderedIndex = 0; orderedIndex < orderedTypes.size(); ++orderedIndex) {
		const MutableType& source = *orderedTypes[orderedIndex];
		DevTypeSchema& target = generation->types[orderedIndex + 1];
		target.id = source.id;
		target.displayName = strings.intern(source.displayName);
		target.cppTypeName = strings.intern(source.cppTypeName);
		target.kind = source.kind;
		target.capture = source.capture;
		target.edit = source.edit;
		target.editor = source.editor;
		target.reason = source.reason;
		target.enumeration.widthBytes = source.enumWidthBytes;
		target.enumeration.isSigned = source.enumIsSigned;
		target.enumeration.values.first = static_cast<std::uint32_t>(generation->enumValues.size());
		target.enumeration.values.count = static_cast<std::uint32_t>(source.enumValues.size());
		for (const MutableType::EnumValue& enumValue : source.enumValues) {
			generation->enumValues.push_back(DevEnumValueSchema{
				.name = strings.intern(enumValue.name),
				.bits = enumValue.bits,
			});
		}
		target.elementType = source.elementType == 0 ? DevTypeIndex{} : indices.at(source.elementType);
		target.sequenceExtent = source.sequenceExtent;
		target.sequenceFixed = source.sequenceFixed;
		target.size = source.size;
		target.alignment = source.alignment;
		target.fields.first = static_cast<std::uint32_t>(generation->fields.size());
		target.fields.count = static_cast<std::uint32_t>(source.fields.size());
		generation->typeOperations.push_back(source.operations);

		for (const MutableField& field : source.fields) {
			std::uint32_t constraintIndex = 0;
			if (field.hasConstraint) {
				constraintIndex = static_cast<std::uint32_t>(generation->constraints.size());
				generation->constraints.push_back(field.constraint);
			}
			const std::uint32_t operationIndex = static_cast<std::uint32_t>(generation->fieldOperations.size());
			generation->fieldOperations.push_back(field.operations);
			generation->fields.push_back(DevFieldSchema{
				.id = field.id,
				.name = strings.intern(field.name),
				.displayName = strings.intern(field.displayName),
				.hint = strings.intern(field.hint),
				.ownerType = indices.at(field.ownerType),
				.valueType = field.valueType == 0 ? DevTypeIndex{} : indices.at(field.valueType),
				.declaredAccess = field.declaredAccess,
				.editor = field.editor,
				.choiceDomain = field.choiceDomain,
				.effectiveEdit = field.effectiveEdit,
				.reason = field.reason,
				.constraint = constraintIndex,
				.operations = operationIndex,
				.declarationOrder = field.declarationOrder,
				.source = DevSourceLocation{
					.file = strings.intern(field.sourceFile),
					.function = strings.intern(field.sourceFunction),
					.line = field.sourceLine,
					.column = field.sourceColumn,
				},
			});
		}
	}

	std::vector<const MutableElement*> orderedElements;
	for (const MutableElement& element : mutableElements_) orderedElements.push_back(&element);
	std::sort(orderedElements.begin(), orderedElements.end(),
		[](const MutableElement* left, const MutableElement* right) {
			return left->definitionId.value < right->definitionId.value;
		});
	for (const MutableElement* element : orderedElements) {
		generation->elements.push_back(DevElementSchema{
			.definitionId = element->definitionId,
			.definitionType = indices.at(element->definitionType),
			.parametersType = indices.at(element->parametersType),
			.stateType = element->stateType == 0 ? DevTypeIndex{} : indices.at(element->stateType),
			.resourcesType = element->resourcesType == 0 ? DevTypeIndex{} : indices.at(element->resourcesType),
			.displayName = strings.intern(element->displayName),
			.hint = strings.intern(element->hint),
		});
	}

	std::vector<const MutableTheme*> orderedThemes;
	for (const MutableTheme& theme : mutableThemes_) orderedThemes.push_back(&theme);
	std::sort(orderedThemes.begin(), orderedThemes.end(),
		[](const MutableTheme* left, const MutableTheme* right) { return left->themeType < right->themeType; });
	for (const MutableTheme* theme : orderedThemes) {
		generation->themes.push_back(DevThemeSchema{
			.themeType = indices.at(theme->themeType),
			.displayName = strings.intern(theme->displayName),
			.hint = strings.intern(theme->hint),
		});
	}

	for (const MutableDiagnostic& diagnostic : mutableDiagnostics_) {
		generation->diagnostics.push_back(DevDiagnostic{
			.severity = diagnostic.severity,
			.code = diagnostic.code,
			.type = diagnostic.type,
			.field = diagnostic.field,
			.message = strings.intern(diagnostic.message),
		});
	}
	if (strings.exceeded()) {
		generation->diagnostics.push_back(DevDiagnostic{
			.severity = DevDiagnosticSeverity::Error,
			.code = DevDiagnosticCode::CapacityExceeded,
		});
	}

	std::uint64_t fingerprint = 14695981039346656037ull;
	for (std::size_t index = 1; index < generation->types.size(); ++index) {
		const DevTypeSchema& type = generation->types[index];
		hashValue(fingerprint, type.id);
		hashString(fingerprint, generation->string(type.cppTypeName));
		hashValue(fingerprint, type.kind);
		hashValue(fingerprint, type.capture);
		hashValue(fingerprint, type.edit);
		hashValue(fingerprint, type.editor);
		hashValue(fingerprint, type.reason);
		const DevTypeSchema* elementType = generation->type(type.elementType);
		const DevTypeId elementTypeId = elementType ? elementType->id : 0;
		hashValue(fingerprint, elementTypeId);
		hashValue(fingerprint, type.sequenceExtent);
		hashValue(fingerprint, type.sequenceFixed);
		hashValue(fingerprint, type.enumeration.widthBytes);
		hashValue(fingerprint, type.enumeration.isSigned);
		for (std::uint32_t enumOffset = 0; enumOffset < type.enumeration.values.count; ++enumOffset) {
			const DevEnumValueSchema& enumValue = generation->enumValues[
				type.enumeration.values.first + enumOffset];
			hashString(fingerprint, generation->string(enumValue.name));
			hashValue(fingerprint, enumValue.bits);
		}
		for (const DevFieldSchema& field : generation->fieldsOf(DevTypeIndex{static_cast<std::uint32_t>(index)})) {
			hashValue(fingerprint, field.id);
			hashString(fingerprint, generation->string(field.name));
			const DevTypeSchema* valueType = generation->type(field.valueType);
			const DevTypeId valueId = valueType ? valueType->id : 0;
			hashValue(fingerprint, valueId);
			hashValue(fingerprint, field.declaredAccess);
			hashValue(fingerprint, field.editor);
			hashValue(fingerprint, field.effectiveEdit);
			hashValue(fingerprint, field.reason);
			hashValue(fingerprint, field.declarationOrder);
			if (field.constraint != 0 && field.constraint < generation->constraints.size()) {
				const DevConstraintRecord& constraint = generation->constraints[field.constraint];
				hashValue(fingerprint, constraint.numeric.minimum);
				hashValue(fingerprint, constraint.numeric.maximum);
				hashValue(fingerprint, constraint.numeric.step);
				hashValue(fingerprint, constraint.numeric.hasMinimum);
				hashValue(fingerprint, constraint.numeric.hasMaximum);
				hashValue(fingerprint, constraint.numeric.hasStep);
				hashValue(fingerprint, constraint.textMaximum);
				hashValue(fingerprint, constraint.hasTextMaximum);
			}
		}
	}
	for (const DevElementSchema& element : generation->elements) {
		hashValue(fingerprint, element.definitionId.value);
		for (DevTypeIndex index : {
			element.definitionType,
			element.parametersType,
			element.stateType,
			element.resourcesType}) {
			const DevTypeSchema* linkedType = generation->type(index);
			const DevTypeId linkedId = linkedType ? linkedType->id : 0;
			hashValue(fingerprint, linkedId);
		}
		hashString(fingerprint, generation->string(element.displayName));
		hashString(fingerprint, generation->string(element.hint));
		hashValue(fingerprint, element.parametersPolicy.capture);
		hashValue(fingerprint, element.parametersPolicy.edit);
		hashValue(fingerprint, element.statePolicy.capture);
		hashValue(fingerprint, element.statePolicy.edit);
		hashValue(fingerprint, element.resourcesPolicy.capture);
		hashValue(fingerprint, element.resourcesPolicy.edit);
	}
	for (const DevThemeSchema& theme : generation->themes) {
		const DevTypeSchema* type = generation->type(theme.themeType);
		const DevTypeId id = type ? type->id : 0;
		hashValue(fingerprint, id);
		hashString(fingerprint, generation->string(theme.displayName));
		hashString(fingerprint, generation->string(theme.hint));
		hashValue(fingerprint, theme.policy.capture);
		hashValue(fingerprint, theme.policy.edit);
	}
	generation->fingerprint = fingerprint == 0 ? 1 : fingerprint;
	return generation;
}

} // namespace FlowUi::devMode

#endif
