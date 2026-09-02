#pragma once

#include "FlowUi/BuildConfig.hpp"

#if FLOW_UI_DEV_MODE

#include <cstddef>
#include <cstdint>
#include <memory>
#include <span>
#include <string_view>
#include <vector>

#include "FlowUi/ElementID.hpp"

namespace FlowUi::devMode {

using DevTypeId = std::uint64_t;
using DevFieldId = std::uint64_t;
using DevSchemaGenerationId = std::uint64_t;

struct DevTypeIndex {
	std::uint32_t value = 0;
	[[nodiscard]] constexpr explicit operator bool() const noexcept { return value != 0; }
	friend constexpr bool operator==(DevTypeIndex, DevTypeIndex) = default;
};

struct DevFieldIndex {
	std::uint32_t value = 0;
	[[nodiscard]] constexpr explicit operator bool() const noexcept { return value != 0; }
	friend constexpr bool operator==(DevFieldIndex, DevFieldIndex) = default;
};

struct DevStringRef {
	std::uint32_t offset = 0;
	std::uint32_t size = 0;
	friend constexpr bool operator==(DevStringRef, DevStringRef) = default;
};

struct DevRange32 {
	std::uint32_t first = 0;
	std::uint32_t count = 0;
	friend constexpr bool operator==(DevRange32, DevRange32) = default;
};

enum class DevTypeKind : std::uint8_t {
	Invalid,
	Boolean,
	SignedInteger,
	UnsignedInteger,
	FloatingPoint,
	Text,
	Enumeration,
	Object,
	Optional,
	Sequence,
	Pointer,
	Opaque,
};

enum class DevCaptureCapability : std::uint8_t {
	None,
	MetadataOnly,
	Value,
};

enum class DevEditCapability : std::uint8_t {
	Hidden,
	Unsupported,
	ViewOnly,
	Editable,
	PartiallyEditable,
	SemanticCommand,
};

enum class DevEditorKind : std::uint8_t {
	None,
	Toggle,
	SignedNumber,
	UnsignedNumber,
	FloatingNumber,
	Text,
	EnumChoice,
	Flags,
	Color,
	Vector,
	Spacing,
	CornerRadius,
	Sizing,
	SizingAxis,
	AttachmentPoints,
	ObjectGroup,
	OptionalGroup,
	Sequence,
	FontChoice,
	ActionChoice,
	ResourceChoice,
	Custom,
};

enum class DevChoiceDomain : std::uint8_t {
	None,
	FontFace,
	FontFamily,
	Action,
	Image,
	Icon,
	TextureResource,
};

enum class DevCapabilityReason : std::uint16_t {
	None,
	FieldMarkedReadOnly,
	RoleDefaultReadOnly,
	NoSchemaDeclaration,
	NoCaptureAdapter,
	NoEditAdapter,
	RawPointer,
	CallableType,
	RecursiveCycle,
	ConstraintMismatch,
	CapacityExceeded,
	ConflictingDeclaration,
};

enum class DevFieldAccess : std::uint8_t {
	Inherit,
	ReadOnly,
	Editable,
	Hidden,
};

enum class DevRootRole : std::uint8_t {
	Struct,
	ElementParameters,
	ElementState,
	ElementResources,
	Theme,
};

enum class DevDiagnosticSeverity : std::uint8_t {
	Info,
	Warning,
	Error,
};

enum class DevValueOperationStatus : std::uint8_t {
	Success,
	NullSource,
	NullDestination,
	Unsupported,
	Failed,
};

enum class DevDiagnosticCode : std::uint16_t {
	None,
	MissingStructSchema,
	DuplicateFieldName,
	TypeIdentityCollision,
	FieldIdentityCollision,
	ElementDefinitionConflict,
	ConflictingTypeDeclaration,
	DuplicateEnumName,
	DuplicateEnumValue,
	ConstraintMismatch,
	CapacityExceeded,
	RecursiveCycle,
};

struct DevSourceLocation {
	DevStringRef file{};
	DevStringRef function{};
	std::uint32_t line = 0;
	std::uint32_t column = 0;
};

struct DevNumericConstraint {
	double minimum = 0.0;
	double maximum = 0.0;
	double step = 0.0;
	bool hasMinimum = false;
	bool hasMaximum = false;
	bool hasStep = false;
};

struct DevConstraintRecord {
	DevNumericConstraint numeric{};
	std::uint32_t textMaximum = 0;
	bool hasTextMaximum = false;
};

struct DevEnumValueSchema {
	DevStringRef name{};
	std::uint64_t bits = 0;
};

struct DevEnumShape {
	std::uint8_t widthBytes = 0;
	bool isSigned = false;
	DevRange32 values{};
};

/**
 * Type-safe access to a declared member. The C++ member pointer remains encoded
 * in the template that owns these functions; it is never serialized into bytes.
 */
struct DevFieldOps {
	const void* (*constAddress)(const void* owner) noexcept = nullptr;
	void* (*mutableAddress)(void* owner) noexcept = nullptr;
	DevValueOperationStatus (*copyConstructMember)(
		const void* owner, void* destination) noexcept = nullptr;
	DevValueOperationStatus (*assignMemberFromCopy)(
		void* draftOwner, const void* source) noexcept = nullptr;
};

/**
 * Type-erased entry points whose bodies remain fully typed template
 * instantiations. `destination` must satisfy the published size/alignment.
 */
struct DevTypeOps {
	DevTypeId type = 0;
	DevValueOperationStatus (*copyConstruct)(
		const void* source, void* destination) noexcept = nullptr;
	DevValueOperationStatus (*moveConstruct)(
		void* source, void* destination) noexcept = nullptr;
	DevValueOperationStatus (*copyAssign)(
		void* destination, const void* source) noexcept = nullptr;
	void (*destroy)(void* value) noexcept = nullptr;
	std::string_view (*textView)(const void* value) noexcept = nullptr;
	bool (*optionalHasValue)(const void* value) noexcept = nullptr;
	const void* (*optionalValueAddress)(const void* value) noexcept = nullptr;
	void* (*optionalMutableValueAddress)(void* value) noexcept = nullptr;
	std::size_t (*sequenceSize)(const void* value) noexcept = nullptr;
	const void* (*sequenceElementAddress)(
		const void* value, std::size_t index) noexcept = nullptr;
	void* (*sequenceMutableElementAddress)(
		void* value, std::size_t index) noexcept = nullptr;
	DevValueOperationStatus (*sequenceAppendDefault)(void* value) noexcept = nullptr;
	DevValueOperationStatus (*sequenceErase)(
		void* value, std::size_t index) noexcept = nullptr;
	DevValueOperationStatus (*sequenceMove)(
		void* value, std::size_t from, std::size_t to) noexcept = nullptr;
	bool (*numericValue)(const void* value, long double& result) noexcept = nullptr;
	bool (*pointerValue)(const void* value, const void*& result) noexcept = nullptr;
	DevValueOperationStatus (*assignNumericValue)(
		void* value, long double candidate) noexcept = nullptr;
	DevValueOperationStatus (*assignTextValue)(
		void* value, std::string_view candidate) noexcept = nullptr;
	DevValueOperationStatus (*setOptionalPresence)(
		void* value, bool present) noexcept = nullptr;
};

struct DevTypeSchema {
	DevTypeId id = 0;
	DevStringRef displayName{};
	DevStringRef cppTypeName{};
	DevTypeKind kind = DevTypeKind::Invalid;
	DevCaptureCapability capture = DevCaptureCapability::None;
	DevEditCapability edit = DevEditCapability::Unsupported;
	DevEditorKind editor = DevEditorKind::None;
	DevCapabilityReason reason = DevCapabilityReason::None;
	DevRange32 fields{};
	DevEnumShape enumeration{};
	DevTypeIndex elementType{};
	std::uint32_t sequenceExtent = 0;
	bool sequenceFixed = false;
	std::uint32_t size = 0;
	std::uint32_t alignment = 0;
};

struct DevFieldSchema {
	DevFieldId id = 0;
	DevStringRef name{};
	DevStringRef displayName{};
	DevStringRef hint{};
	DevTypeIndex ownerType{};
	DevTypeIndex valueType{};
	DevFieldAccess declaredAccess = DevFieldAccess::Inherit;
	DevEditorKind editor = DevEditorKind::None;
	DevChoiceDomain choiceDomain = DevChoiceDomain::None;
	DevEditCapability effectiveEdit = DevEditCapability::Unsupported;
	DevCapabilityReason reason = DevCapabilityReason::None;
	std::uint32_t constraint = 0;
	std::uint32_t operations = 0;
	std::uint32_t declarationOrder = 0;
	DevSourceLocation source{};
};

struct DevRolePolicy {
	DevCaptureCapability capture = DevCaptureCapability::None;
	DevEditCapability edit = DevEditCapability::Unsupported;
};

struct DevElementSchema {
	FlowDefinitionID definitionId{};
	DevTypeIndex definitionType{};
	DevTypeIndex parametersType{};
	DevTypeIndex stateType{};
	DevTypeIndex resourcesType{};
	DevStringRef displayName{};
	DevStringRef hint{};
	DevRolePolicy parametersPolicy{
		DevCaptureCapability::Value, DevEditCapability::Editable};
	DevRolePolicy statePolicy{
		DevCaptureCapability::Value, DevEditCapability::ViewOnly};
	DevRolePolicy resourcesPolicy{
		DevCaptureCapability::MetadataOnly, DevEditCapability::ViewOnly};
};

struct DevThemeSchema {
	DevTypeIndex themeType{};
	DevStringRef displayName{};
	DevStringRef hint{};
	DevRolePolicy policy{
		DevCaptureCapability::Value, DevEditCapability::Editable};
};

struct DevDiagnostic {
	DevDiagnosticSeverity severity = DevDiagnosticSeverity::Info;
	DevDiagnosticCode code = DevDiagnosticCode::None;
	DevTypeId type = 0;
	DevFieldId field = 0;
	DevStringRef message{};
};

struct DevSchemaGeneration {
	DevSchemaGenerationId generation = 0;
	std::uint64_t fingerprint = 0;
	std::vector<char> strings{};
	std::vector<DevTypeSchema> types{};
	std::vector<DevFieldSchema> fields{};
	std::vector<DevEnumValueSchema> enumValues{};
	std::vector<DevConstraintRecord> constraints{};
	std::vector<DevElementSchema> elements{};
	std::vector<DevThemeSchema> themes{};
	std::vector<DevDiagnostic> diagnostics{};
	std::vector<const DevTypeOps*> typeOperations{};
	std::vector<const DevFieldOps*> fieldOperations{};

	[[nodiscard]] std::string_view string(DevStringRef ref) const noexcept {
		if (ref.offset > strings.size() || ref.size > strings.size() - ref.offset) return {};
		return {strings.data() + ref.offset, ref.size};
	}

	[[nodiscard]] const DevTypeSchema* type(DevTypeIndex index) const noexcept {
		return index.value < types.size() ? &types[index.value] : nullptr;
	}

	[[nodiscard]] const DevTypeSchema* findType(DevTypeId id) const noexcept;
	[[nodiscard]] const DevElementSchema* findElement(FlowDefinitionID id) const noexcept;
	[[nodiscard]] const DevThemeSchema* findTheme(DevTypeId id) const noexcept;
	[[nodiscard]] std::span<const DevFieldSchema> fieldsOf(DevTypeIndex typeIndex) const noexcept;
};

class DevSchemaView {
public:
	DevSchemaView() = default;
	explicit DevSchemaView(std::shared_ptr<const DevSchemaGeneration> generation)
		: generation_(std::move(generation)) {}

	[[nodiscard]] explicit operator bool() const noexcept { return static_cast<bool>(generation_); }
	[[nodiscard]] const DevSchemaGeneration* operator->() const noexcept { return generation_.get(); }
	[[nodiscard]] const DevSchemaGeneration& operator*() const noexcept { return *generation_; }
	[[nodiscard]] const std::shared_ptr<const DevSchemaGeneration>& shared() const noexcept {
		return generation_;
	}

private:
	std::shared_ptr<const DevSchemaGeneration> generation_{};
};

} // namespace FlowUi::devMode

#endif
