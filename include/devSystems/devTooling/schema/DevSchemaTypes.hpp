#pragma once

#include "FlowUi/Public/BuildConfig.hpp"

#if FLOW_UI_DEV_MODE

#include <cstddef>
#include <cstdint>
#include <memory>
#include <span>
#include <string_view>
#include <vector>

#include "FlowUi/Public/identificationTypes/ElementID.hpp"

namespace FlowUi::devSystems::tooling {

/** @brief Current-build C++ type identity used by the reflected schema registry; zero is invalid. */
using DevTypeId = std::uint64_t;
/** @brief Reflected member identity derived from its owner type and declared member name. */
using DevFieldId = std::uint64_t;
/** @brief Monotonic immutable schema publication identity. */
using DevSchemaGenerationId = std::uint64_t;

/** @brief Generation-local type array index; zero is the invalid/sentinel entry. */
struct DevTypeIndex {
	/** @brief Index into DevSchemaGeneration::types; meaningful only with its generation. */
	std::uint32_t value = 0;
	/** @brief Return whether this generation-local index is nonzero; bounds still depend on its generation. */
	[[nodiscard]] constexpr explicit operator bool() const noexcept { return value != 0; }
	/** @brief Compare identity/range fields; diagnostic names and owned values are not inspected. */
	friend constexpr bool operator==(DevTypeIndex, DevTypeIndex) = default;
};

/** @brief Generation-local field array index; zero is the invalid/sentinel entry. */
struct DevFieldIndex {
	/** @brief Index into DevSchemaGeneration::fields; meaningful only with its generation. */
	std::uint32_t value = 0;
	/** @brief Return whether this generation-local index is nonzero; bounds still depend on its generation. */
	[[nodiscard]] constexpr explicit operator bool() const noexcept { return value != 0; }
	/** @brief Compare identity/range fields; diagnostic names and owned values are not inspected. */
	friend constexpr bool operator==(DevFieldIndex, DevFieldIndex) = default;
};

/** @brief Byte range in one immutable schema generation string pool. */
struct DevStringRef {
	/** @brief Starting byte offset into DevSchemaGeneration::strings. */
	std::uint32_t offset = 0;
	/** @brief Byte length of the referenced string. */
	std::uint32_t size = 0;
	/** @brief Compare identity/range fields; diagnostic names and owned values are not inspected. */
	friend constexpr bool operator==(DevStringRef, DevStringRef) = default;
};

/** @brief Contiguous array range encoded as first index plus count. */
struct DevRange32 {
	/** @brief First element index, inclusive. */
	std::uint32_t first = 0;
	/** @brief Number of elements in the range. */
	std::uint32_t count = 0;
	/** @brief Compare identity/range fields; diagnostic names and owned values are not inspected. */
	friend constexpr bool operator==(DevRange32, DevRange32) = default;
};

/** @brief Structural classification of a reflected C++ value. */
enum class DevTypeKind : std::uint8_t {
	/** @brief Invalid/sentinel type metadata. */
	Invalid,
	/** @brief Boolean scalar. */
	Boolean,
	/** @brief Signed integral scalar. */
	SignedInteger,
	/** @brief Unsigned integral scalar. */
	UnsignedInteger,
	/** @brief Floating-point scalar. */
	FloatingPoint,
	/** @brief String or string-view data. */
	Text,
	/** @brief Enum with an underlying integer representation. */
	Enumeration,
	/** @brief Declared compound object. */
	Object,
	/** @brief Optional nested value. */
	Optional,
	/** @brief Dynamic or fixed contiguous sequence. */
	Sequence,
	/** @brief Raw non-owning pointer, observed as metadata. */
	Pointer,
	/** @brief Type without supported reflected structure. */
	Opaque,
};

/** @brief Maximum observation supported by a reflected type or root role. */
enum class DevCaptureCapability : std::uint8_t {
	/** @brief No observation is available. */
	None,
	/** @brief Publish type/field metadata without copying runtime values. */
	MetadataOnly,
	/** @brief Copy schema-typed runtime values. */
	Value,
};

/** @brief Effective editing support after adapter and role restrictions. */
enum class DevEditCapability : std::uint8_t {
	/** @brief Omit the value from visible editors. */
	Hidden,
	/** @brief No supported editing operation exists. */
	Unsupported,
	/** @brief Display observations while preventing direct edits. */
	ViewOnly,
	/** @brief Allow supported reflected edits. */
	Editable,
	/** @brief Only a subset of nested members supports edits. */
	PartiallyEditable,
	/** @brief Edit through a semantic operation rather than direct assignment. */
	SemanticCommand,
};

/** @brief Semantic editor presentation chosen by a reflected adapter. */
enum class DevEditorKind : std::uint8_t {
	/** @brief No semantic editor. */
	None,
	/** @brief Boolean toggle. */
	Toggle,
	/** @brief Signed integer editor. */
	SignedNumber,
	/** @brief Unsigned integer editor. */
	UnsignedNumber,
	/** @brief Floating-point number editor. */
	FloatingNumber,
	/** @brief Text editor. */
	Text,
	/** @brief Named enum choice. */
	EnumChoice,
	/** @brief Bit-flag choice editor. */
	Flags,
	/** @brief Color-channel editor. */
	Color,
	/** @brief Vector component editor. */
	Vector,
	/** @brief Edge spacing editor. */
	Spacing,
	/** @brief Per-corner radius editor. */
	CornerRadius,
	/** @brief Compound layout sizing editor. */
	Sizing,
	/** @brief Single layout-axis sizing editor. */
	SizingAxis,
	/** @brief Floating attachment point editor. */
	AttachmentPoints,
	/** @brief Nested declared-member group. */
	ObjectGroup,
	/** @brief Optional presence/value group. */
	OptionalGroup,
	/** @brief Ordered sequence editor. */
	Sequence,
	/** @brief Font face/family catalogue choice. */
	FontChoice,
	/** @brief Semantic action catalogue choice. */
	ActionChoice,
	/** @brief Image/icon/texture resource choice. */
	ResourceChoice,
	/** @brief Application-defined semantic editor. */
	Custom,
};

/** @brief Catalogue supplying values for a semantic choice editor. */
enum class DevChoiceDomain : std::uint8_t {
	/** @brief No catalogue domain. */
	None,
	/** @brief Concrete font faces. */
	FontFace,
	/** @brief Logical font families. */
	FontFamily,
	/** @brief Semantic app/UI actions. */
	Action,
	/** @brief Registered image resources. */
	Image,
	/** @brief Registered SVG icon resources. */
	Icon,
	/** @brief Available logical texture resources. */
	TextureResource,
};

/** @brief Explanation for restricted reflection/capture/edit support. */
enum class DevCapabilityReason : std::uint16_t {
	/** @brief No restriction requires explanation. */
	None,
	/** @brief Author explicitly marked the field read-only. */
	FieldMarkedReadOnly,
	/** @brief The root role defaults to observation-only. */
	RoleDefaultReadOnly,
	/** @brief Compound type lacks a schema declaration. */
	NoSchemaDeclaration,
	/** @brief No typed capture operation exists. */
	NoCaptureAdapter,
	/** @brief No typed edit operation exists. */
	NoEditAdapter,
	/** @brief Raw pointer referents are not owned or traversed. */
	RawPointer,
	/** @brief Callable internals are not reflected. */
	CallableType,
	/** @brief Recursive type traversal stopped at a cycle. */
	RecursiveCycle,
	/** @brief Declared constraints do not match the member type. */
	ConstraintMismatch,
	/** @brief Configured publication capacity was exhausted. */
	CapacityExceeded,
	/** @brief Schema declarations conflict. */
	ConflictingDeclaration,
};

/** @brief Author-declared visibility/mutability policy for a field. */
enum class DevFieldAccess : std::uint8_t {
	/** @brief Use resolved adapter and root-role defaults. */
	Inherit,
	/** @brief Expose observations without ordinary assignment. */
	ReadOnly,
	/** @brief Request editing subject to available typed operations. */
	Editable,
	/** @brief Omit the field from visible editors. */
	Hidden,
};

/** @brief Purpose of a reflected root and its default capability policy. */
enum class DevRootRole : std::uint8_t {
	/** @brief Additional reflected struct. */
	Struct,
	/** @brief Element invocation parameters. */
	ElementParameters,
	/** @brief Window-owned persistent element state. */
	ElementState,
	/** @brief App-shared element resources. */
	ElementResources,
	/** @brief Registered theme payload. */
	Theme,
};

/** @brief Severity of a schema publication diagnostic. */
enum class DevDiagnosticSeverity : std::uint8_t {
	/** @brief Informational observation. */
	Info,
	/** @brief Reduced capability or recoverable publication issue. */
	Warning,
	/** @brief Invalid or conflicting schema metadata. */
	Error,
};

/** @brief Explicit result of a typed value construction/assignment operation. */
enum class DevValueOperationStatus : std::uint8_t {
	/** @brief Operation completed. */
	Success,
	/** @brief Required source pointer was null. */
	NullSource,
	/** @brief Required destination pointer was null. */
	NullDestination,
	/** @brief The type cannot perform this operation. */
	Unsupported,
	/** @brief The supported typed operation failed. */
	Failed,
};

/** @brief Machine-readable reflected schema validation failure. */
enum class DevDiagnosticCode : std::uint16_t {
	/** @brief No diagnostic. */
	None,
	/** @brief Reachable compound type has no declared schema. */
	MissingStructSchema,
	/** @brief An owner declares the same field name more than once. */
	DuplicateFieldName,
	/** @brief Different complete type tokens share an identity. */
	TypeIdentityCollision,
	/** @brief Distinct fields share a reflected identity. */
	FieldIdentityCollision,
	/** @brief Different element types claim the same Flow definition identity. */
	ElementDefinitionConflict,
	/** @brief A type has incompatible schema declarations. */
	ConflictingTypeDeclaration,
	/** @brief Enum choices repeat a display name. */
	DuplicateEnumName,
	/** @brief Enum choices repeat an underlying value. */
	DuplicateEnumValue,
	/** @brief Constraint metadata is incompatible with its member type. */
	ConstraintMismatch,
	/** @brief Configured schema capacity was exceeded. */
	CapacityExceeded,
	/** @brief Schema traversal encountered an already active type. */
	RecursiveCycle,
};

/** @brief Source metadata whose strings belong to its schema generation. */
struct DevSourceLocation {
	/** @brief Source file path in the schema string pool. */
	DevStringRef file{};
	/** @brief Function name in the schema string pool. */
	DevStringRef function{};
	/** @brief Source line, or zero when unavailable. */
	std::uint32_t line = 0;
	/** @brief Source column, or zero when unavailable. */
	std::uint32_t column = 0;
};

/** @brief Optional numeric edit bounds; each has flag controls whether its value applies. */
struct DevNumericConstraint {
	/** @brief Inclusive minimum when hasMinimum is set. */
	double minimum = 0.0;
	/** @brief Inclusive maximum when hasMaximum is set. */
	double maximum = 0.0;
	/** @brief Editor increment when hasStep is set. */
	double step = 0.0;
	/** @brief Whether minimum is an active constraint. */
	bool hasMinimum = false;
	/** @brief Whether maximum is an active constraint. */
	bool hasMaximum = false;
	/** @brief Whether step is explicitly declared. */
	bool hasStep = false;
};

/** @brief Published numeric and text constraints for a reflected field. */
struct DevConstraintRecord {
	/** @brief Optional numeric bounds and editor increment. */
	DevNumericConstraint numeric{};
	/** @brief Maximum text length when hasTextMaximum is set. */
	std::uint32_t textMaximum = 0;
	/** @brief Whether textMaximum was explicitly declared. */
	bool hasTextMaximum = false;
};

/** @brief Published enum label with its underlying integer bit pattern. */
struct DevEnumValueSchema {
	/** @brief Enumerator display name in the schema string pool. */
	DevStringRef name{};
	/** @brief Underlying value bit pattern encoded without sign extension assumptions. */
	std::uint64_t bits = 0;
};

/** @brief Underlying enum representation and range of published named values. */
struct DevEnumShape {
	/** @brief Size of the enum underlying integer in bytes. */
	std::uint8_t widthBytes = 0;
	/** @brief Whether the underlying integer type is signed. */
	bool isSigned = false;
	/** @brief Range in DevSchemaGeneration::enumValues. */
	DevRange32 values{};
};

/**
 * Type-safe access to a declared member. The C++ member pointer remains encoded
 * in the template that owns these functions; it is never serialized into bytes.
 */
struct DevFieldOps {
	/** @brief Borrow a declared member from a correctly typed const owner; null owner yields null. */
	const void* (*constAddress)(const void* owner) noexcept = nullptr;
	/** @brief Borrow a mutable declared member; absent for const members. */
	void* (*mutableAddress)(void* owner) noexcept = nullptr;
	/** @brief Copy-construct the declared member in suitably sized/aligned uninitialized destination storage. */
	DevValueOperationStatus (*copyConstructMember)(
		const void* owner, void* destination) noexcept = nullptr;
	/** @brief Copy-assign the declared member from a correctly typed source; report unsupported or failed assignment. */
	DevValueOperationStatus (*assignMemberFromCopy)(
		void* draftOwner, const void* source) noexcept = nullptr;
};

/**
 * Type-erased entry points whose bodies remain fully typed template
 * instantiations. `destination` must satisfy the published size/alignment.
 */
struct DevTypeOps {
	/** @brief Reflected identity of the exact C++ type operated on. */
	DevTypeId type = 0;
	/** @brief Copy-construct in suitably sized/aligned uninitialized destination storage. */
	DevValueOperationStatus (*copyConstruct)(
		const void* source, void* destination) noexcept = nullptr;
	/** @brief Move-construct in suitably sized/aligned uninitialized destination storage. */
	DevValueOperationStatus (*moveConstruct)(
		void* source, void* destination) noexcept = nullptr;
	/** @brief Copy-assign an existing correctly typed destination object. */
	DevValueOperationStatus (*copyAssign)(
		void* destination, const void* source) noexcept = nullptr;
	/** @brief Destroy a correctly typed constructed value without releasing its storage. */
	void (*destroy)(void* value) noexcept = nullptr;
	/** @brief Borrow textual contents, when the type exposes text. */
	std::string_view (*textView)(const void* value) noexcept = nullptr;
	/** @brief Observe optional presence; null when the type is not optional. */
	bool (*optionalHasValue)(const void* value) noexcept = nullptr;
	/** @brief Borrow an engaged optional value as const storage. */
	const void* (*optionalValueAddress)(const void* value) noexcept = nullptr;
	/** @brief Borrow an engaged optional value as mutable storage. */
	void* (*optionalMutableValueAddress)(void* value) noexcept = nullptr;
	/** @brief Return current sequence length. */
	std::size_t (*sequenceSize)(const void* value) noexcept = nullptr;
	/** @brief Borrow a const addressable sequence element by index. */
	const void* (*sequenceElementAddress)(
		const void* value, std::size_t index) noexcept = nullptr;
	/** @brief Borrow a mutable addressable sequence element by index. */
	void* (*sequenceMutableElementAddress)(
		void* value, std::size_t index) noexcept = nullptr;
	/** @brief Append a default-constructed element to a supported dynamic sequence. */
	DevValueOperationStatus (*sequenceAppendDefault)(void* value) noexcept = nullptr;
	/** @brief Erase one indexed element from a supported dynamic sequence. */
	DevValueOperationStatus (*sequenceErase)(
		void* value, std::size_t index) noexcept = nullptr;
	/** @brief Move one indexed element within a supported dynamic sequence. */
	DevValueOperationStatus (*sequenceMove)(
		void* value, std::size_t from, std::size_t to) noexcept = nullptr;
	/** @brief Read an arithmetic/enum value into result; return false when unavailable. */
	bool (*numericValue)(const void* value, long double& result) noexcept = nullptr;
	/** @brief Observe a raw pointer value without dereferencing or taking ownership. */
	bool (*pointerValue)(const void* value, const void*& result) noexcept = nullptr;
	/** @brief Assign a validated arithmetic/enum candidate through the typed adapter. */
	DevValueOperationStatus (*assignNumericValue)(
		void* value, long double candidate) noexcept = nullptr;
	/** @brief Copy text into a supported owning text value. */
	DevValueOperationStatus (*assignTextValue)(
		void* value, std::string_view candidate) noexcept = nullptr;
	/** @brief Construct/reset optional presence through the typed adapter. */
	DevValueOperationStatus (*setOptionalPresence)(
		void* value, bool present) noexcept = nullptr;
};

/** @brief Immutable reflected type metadata; all indices refer to its schema generation. */
struct DevTypeSchema {
	/** @brief Stable process type identity. */
	DevTypeId id = 0;
	/** @brief Display label in the schema string pool. */
	DevStringRef displayName{};
	/** @brief Complete C++ type token in the schema string pool. */
	DevStringRef cppTypeName{};
	/** @brief Scalar, compound, optional, sequence, pointer, or opaque classification. */
	DevTypeKind kind = DevTypeKind::Invalid;
	/** @brief Whether metadata or owned values can be captured. */
	DevCaptureCapability capture = DevCaptureCapability::None;
	/** @brief Effective type-level edit capability. */
	DevEditCapability edit = DevEditCapability::Unsupported;
	/** @brief Preferred semantic editor presentation. */
	DevEditorKind editor = DevEditorKind::None;
	/** @brief Reason for reduced/unsupported capability, if any. */
	DevCapabilityReason reason = DevCapabilityReason::None;
	/** @brief Contiguous member range in the schema field array. */
	DevRange32 fields{};
	/** @brief Underlying representation and enumerator range for enum types. */
	DevEnumShape enumeration{};
	/** @brief Nested value type for optional/sequence types. */
	DevTypeIndex elementType{};
	/** @brief Compile-time fixed sequence length when sequenceFixed is set. */
	std::uint32_t sequenceExtent = 0;
	/** @brief Whether the sequence has a fixed extent. */
	bool sequenceFixed = false;
	/** @brief Required value storage size in bytes. */
	std::uint32_t size = 0;
	/** @brief Required value storage alignment in bytes. */
	std::uint32_t alignment = 0;
};

/** @brief Immutable declared-member identity, access policy, and typed-operation indices. */
struct DevFieldSchema {
	/** @brief Member identity derived from the root owner type and declared name. */
	DevFieldId id = 0;
	/** @brief Declared member name in the schema string pool. */
	DevStringRef name{};
	/** @brief Editor display label in the schema string pool. */
	DevStringRef displayName{};
	/** @brief Help text in the schema string pool. */
	DevStringRef hint{};
	/** @brief Generation-local declaring owner type index. */
	DevTypeIndex ownerType{};
	/** @brief Generation-local member value type index. */
	DevTypeIndex valueType{};
	/** @brief Author-declared visibility/mutability policy. */
	DevFieldAccess declaredAccess = DevFieldAccess::Inherit;
	/** @brief Resolved semantic editor kind. */
	DevEditorKind editor = DevEditorKind::None;
	/** @brief Catalogue domain used by a choice editor. */
	DevChoiceDomain choiceDomain = DevChoiceDomain::None;
	/** @brief Resolved edit capability after type and constraint validation. */
	DevEditCapability effectiveEdit = DevEditCapability::Unsupported;
	/** @brief Reason for reduced/unsupported editing capability. */
	DevCapabilityReason reason = DevCapabilityReason::None;
	/** @brief Constraint-array index, or zero when none is declared. */
	std::uint32_t constraint = 0;
	/** @brief Typed member-operation table index. */
	std::uint32_t operations = 0;
	/** @brief Zero-based source declaration order. */
	std::uint32_t declarationOrder = 0;
	/** @brief Declaration source metadata. */
	DevSourceLocation source{};
};

/** @brief Capture and editing defaults for one reflected root role. */
struct DevRolePolicy {
	/** @brief Value/metadata capture permitted for this root role. */
	DevCaptureCapability capture = DevCaptureCapability::None;
	/** @brief Default edit capability for this root role. */
	DevEditCapability edit = DevEditCapability::Unsupported;
};

/** @brief Immutable links from one Flow definition to its parameter/state/resource schemas. */
struct DevElementSchema {
	/** @brief Stable Flow definition identity. */
	FlowDefinitionID definitionId{};
	/** @brief Generation-local element tag type index. */
	DevTypeIndex definitionType{};
	/** @brief Generation-local parameter type index. */
	DevTypeIndex parametersType{};
	/** @brief Generation-local state type index, or zero when absent. */
	DevTypeIndex stateType{};
	/** @brief Generation-local shared-resource type index, or zero when absent. */
	DevTypeIndex resourcesType{};
	/** @brief Definition label in the schema string pool. */
	DevStringRef displayName{};
	/** @brief Definition help text in the schema string pool. */
	DevStringRef hint{};
	/** @brief Default capture/edit policy for parameters. */
	DevRolePolicy parametersPolicy{
		DevCaptureCapability::Value, DevEditCapability::Editable};
	/** @brief Default capture/edit policy for persistent state. */
	DevRolePolicy statePolicy{
		DevCaptureCapability::Value, DevEditCapability::ViewOnly};
	/** @brief Default capture/edit policy for shared resources. */
	DevRolePolicy resourcesPolicy{
		DevCaptureCapability::MetadataOnly, DevEditCapability::ViewOnly};
};

/** @brief Immutable reflected theme root and its default tooling policy. */
struct DevThemeSchema {
	/** @brief Generation-local theme type index. */
	DevTypeIndex themeType{};
	/** @brief Theme display label in the schema string pool. */
	DevStringRef displayName{};
	/** @brief Theme help text in the schema string pool. */
	DevStringRef hint{};
	/** @brief Default theme capture/edit capabilities. */
	DevRolePolicy policy{
		DevCaptureCapability::Value, DevEditCapability::Editable};
};

/** @brief Schema publication diagnostic referring to optional type/member identities. */
struct DevDiagnostic {
	/** @brief Informational, warning, or error classification. */
	DevDiagnosticSeverity severity = DevDiagnosticSeverity::Info;
	/** @brief Machine-readable schema failure classification. */
	DevDiagnosticCode code = DevDiagnosticCode::None;
	/** @brief Associated reflected type identity, or zero when not applicable. */
	DevTypeId type = 0;
	/** @brief Associated reflected field identity, or zero when not applicable. */
	DevFieldId field = 0;
	/** @brief Diagnostic explanation in the schema string pool. */
	DevStringRef message{};
};

/** @brief Immutable contiguous schema publication; retain a DevSchemaView while borrowing its arrays. */
struct DevSchemaGeneration {
	/** @brief Monotonic publication identity. */
	DevSchemaGenerationId generation = 0;
	/** @brief Content fingerprint used for compatibility checks. */
	std::uint64_t fingerprint = 0;
	/** @brief Owned UTF-8 metadata pool. */
	std::vector<char> strings{};
	/** @brief Contiguous type records, including the invalid sentinel. */
	std::vector<DevTypeSchema> types{};
	/** @brief Contiguous member records, including the invalid sentinel. */
	std::vector<DevFieldSchema> fields{};
	/** @brief Contiguous named enum values. */
	std::vector<DevEnumValueSchema> enumValues{};
	/** @brief Contiguous field constraints, including the absent sentinel. */
	std::vector<DevConstraintRecord> constraints{};
	/** @brief Contiguous registered element root records. */
	std::vector<DevElementSchema> elements{};
	/** @brief Contiguous registered theme root records. */
	std::vector<DevThemeSchema> themes{};
	/** @brief Contiguous publication diagnostics. */
	std::vector<DevDiagnostic> diagnostics{};
	/** @brief Borrowed static typed operation tables indexed by type metadata. */
	std::vector<const DevTypeOps*> typeOperations{};
	/** @brief Borrowed static member-operation tables indexed by field metadata. */
	std::vector<const DevFieldOps*> fieldOperations{};

	/** @brief Borrow a string from this generation; return empty for an out-of-bounds byte range. */
	[[nodiscard]] std::string_view string(DevStringRef ref) const noexcept {
		if (ref.offset > strings.size() || ref.size > strings.size() - ref.offset) return {};
		return {strings.data() + ref.offset, ref.size};
	}

	/** @brief Borrow a generation-local type record, or nullptr for an out-of-bounds index; zero selects the sentinel. */
	[[nodiscard]] const DevTypeSchema* type(DevTypeIndex index) const noexcept {
		return index.value < types.size() ? &types[index.value] : nullptr;
	}

	/** @brief Borrow the matching type record, or nullptr when the identity is absent. */
	[[nodiscard]] const DevTypeSchema* findType(DevTypeId id) const noexcept;
	/** @brief Borrow the matching Flow definition root, or nullptr when absent. */
	[[nodiscard]] const DevElementSchema* findElement(FlowDefinitionID id) const noexcept;
	/** @brief Borrow the matching theme root, or nullptr when absent. */
	[[nodiscard]] const DevThemeSchema* findTheme(DevTypeId id) const noexcept;
	/** @brief Borrow the contiguous declared field range for typeIndex, or an empty span when invalid. */
	[[nodiscard]] std::span<const DevFieldSchema> fieldsOf(DevTypeIndex typeIndex) const noexcept;
};

/** @brief Copyable shared ownership of an immutable schema generation. Retain this handle while borrowing any generation-local indices, arrays, strings, or metadata pointers. */
class DevSchemaView {
public:
	/** @brief Construct an empty generation handle. */
	DevSchemaView() = default;
	/** @brief Retain shared ownership of generation, which may be null for an empty handle. */
	explicit DevSchemaView(std::shared_ptr<const DevSchemaGeneration> generation)
		: generation_(std::move(generation)) {}

	/** @brief Return whether this handle owns an immutable schema generation. */
	[[nodiscard]] explicit operator bool() const noexcept { return static_cast<bool>(generation_); }
	/** @brief Borrow the immutable generation pointer; check this handle before dereferencing. */
	[[nodiscard]] const DevSchemaGeneration* operator->() const noexcept { return generation_.get(); }
	/** @brief Borrow the immutable generation; requires a nonempty handle. */
	[[nodiscard]] const DevSchemaGeneration& operator*() const noexcept { return *generation_; }
	/** @brief Borrow this handle's shared owner; copy it to retain the generation independently. */
	[[nodiscard]] const std::shared_ptr<const DevSchemaGeneration>& shared() const noexcept {
		return generation_;
	}

private:
	std::shared_ptr<const DevSchemaGeneration> generation_{};
};

} // namespace FlowUi::devSystems::tooling

#endif
