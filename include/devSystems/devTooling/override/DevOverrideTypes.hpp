#pragma once

#include "FlowUi/Public/BuildConfig.hpp"

#if FLOW_UI_DEV_MODE

#include <array>
#include <cstddef>
#include <cstdint>
#include <new>
#include <memory>
#include <string>
#include <string_view>
#include <vector>

#include "FlowUi/Public/identificationTypes/ElementID.hpp"
#include "FlowUi/Public/identificationTypes/WindowId.hpp"
#include "devSystems/devTooling/schema/DevSchemaTypes.hpp"
#include "FlowUi/Private/elementSystem/ElementInstanceKey.hpp"

namespace FlowUi::devSystems::tooling {

/** @brief Extent of an element-field assignment. */
enum class DevOverrideScope : std::uint8_t {
	/** @brief Apply to every invocation of the definition. */
	Definition,
	/** @brief Apply only to one window-qualified instance. */
	ExactInstance,
};

/** @brief Assignment precedence from compiled defaults to ephemeral preview. */
enum class DevOverrideLayer : std::uint8_t {
	/** @brief Compiled definition-wide assignment. */
	BakedDefinition,
	/** @brief Compiled exact-instance assignment. */
	BakedInstance,
	/** @brief Live definition-wide assignment. */
	LiveDefinition,
	/** @brief Live exact-instance assignment. */
	LiveInstance,
	/** @brief Temporary preview with highest precedence. */
	EphemeralPreview,
	/** @brief Sentinel number of assignment layers. */
	Count,
};

/** @brief Number of real precedence layers; excludes the Count sentinel. */
inline constexpr std::size_t DevOverrideLayerCount =
	static_cast<std::size_t>(DevOverrideLayer::Count);

/** @brief Owned scope-qualified destination for reflected element overrides. */
struct DevElementOverrideTarget {
	/** @brief Element definition whose parameter schema owns the field. */
	FlowDefinitionID definition{};
	/** @brief Application window for exact-instance scope. */
	WindowId window = InvalidWindowId;
	/** @brief Exact instance key when scope is ExactInstance. */
	::FlowUi::detail::element::ElementInstanceKey instance{};
	/** @brief Owned human-readable instance path used in review metadata. */
	std::string instanceDebugLabel{};
	/** @brief Definition-wide or window-qualified exact-instance selection. */
	DevOverrideScope scope = DevOverrideScope::Definition;
	/** @brief Whether the instance identity is stable enough for persistence. */
	bool bakeable = true;

	/** @brief Compare identity/range fields; diagnostic names and owned values are not inspected. */
	friend bool operator==(
		const DevElementOverrideTarget& left,
		const DevElementOverrideTarget& right) noexcept {
		return left.definition == right.definition && left.window == right.window &&
			left.instance == right.instance && left.scope == right.scope;
	}
};

/** @brief Owned destination for one named reflected theme variant. */
struct DevThemeOverrideTarget {
	/** @brief Reflected theme type identity. */
	tooling::DevTypeId themeType = 0;
	/** @brief Owned target variant name. */
	std::string variant{};

	/** @brief Compare identity/range fields; diagnostic names and owned values are not inspected. */
	friend bool operator==(
		const DevThemeOverrideTarget&,
		const DevThemeOverrideTarget&) = default;
};

/** @brief Schema-qualified nested path ending at one reflected leaf field. */
struct DevOverrideFieldKey {
	/** @brief Root owner type identity. */
	tooling::DevTypeId ownerType = 0;
	/** @brief Leaf member identity. */
	tooling::DevFieldId field = 0;
	/** Field IDs from the root owner to the leaf owner, excluding `field`. */
	std::vector<tooling::DevFieldId> nestedPath{};

	/** @brief Compare identity/range fields; diagnostic names and owned values are not inspected. */
	friend bool operator==(
		const DevOverrideFieldKey&,
		const DevOverrideFieldKey&) = default;
};

/**
 * Owned, schema-typed value used by commands, retained overrides, and captures.
 * Common scalar and Clay values stay inline; larger values use one aligned
 * allocation. The object never owns or copies schema descriptors.
 */
class DevOwnedValue {
public:
	/** @brief Inline payload capacity in bytes; larger/aligned values use owned aligned storage. */
	static constexpr std::size_t InlineBytes = 48;

	/** @brief Construct an empty owned value. */
	DevOwnedValue() noexcept = default;
	/** @brief Destroy the value and release its aligned payload storage. */
	~DevOwnedValue() { reset(); }
	/** @brief Copy the owned value and retained text references; failure leaves this value empty. */
	DevOwnedValue(const DevOwnedValue& other) noexcept;
	/** @brief Replace this value with an owned copy; copy failure produces an empty destination. */
	DevOwnedValue& operator=(const DevOwnedValue& other) noexcept;
	/** @brief Move the constructed value and its retained text ownership from other. */
	DevOwnedValue(DevOwnedValue&& other) noexcept;
	/** @brief Destroy the previous payload, then move the constructed value and retained references from other. */
	DevOwnedValue& operator=(DevOwnedValue&& other) noexcept;

	/** @brief Return whether a schema-typed value is currently constructed. */
	[[nodiscard]] explicit operator bool() const noexcept { return operations_ != nullptr; }
	/** @brief Return the owned C++ type identity, or zero when empty. */
	[[nodiscard]] tooling::DevTypeId type() const noexcept {
		return operations_ ? operations_->type : 0;
	}
	/** @brief Borrow payload storage; check operator bool() before reading it and cast only through the matching reflected type. */
	[[nodiscard]] const void* data() const noexcept;
	/** @brief Borrow payload storage; check operator bool() before reading it and cast only through the matching reflected type. */
	[[nodiscard]] void* data() noexcept;
	/** @brief Return owned out-of-line payload, retained text, and their backing-container bytes. */
	[[nodiscard]] std::size_t heapBytes() const noexcept;

	/** @brief Destroy the owned value and release its payload/retained text storage. */
	void reset() noexcept;
	/** Own a string view inside this value; copies and moves preserve its referent lifetime. */
	[[nodiscard]] bool retain_text_reference(std::string_view& field, std::string text) noexcept;

	/** @brief Copy a correctly typed source using schema/type metadata into destination. Destination owns its payload; the operation reports null, unsupported, or copy failures explicitly. */
	static tooling::DevValueOperationStatus copyFrom(
		const tooling::DevSchemaGeneration& schema,
		tooling::DevTypeIndex type,
		const void* source,
		DevOwnedValue& destination) noexcept;

private:
	struct RetainedTextReference {
		std::unique_ptr<const std::string> text{};
		std::size_t offset = 0;
	};
	[[nodiscard]] bool retain_schema_text_references(const tooling::DevSchemaGeneration& schema,
		tooling::DevTypeIndex type) noexcept;
	tooling::DevValueOperationStatus initializeCopy(
		const tooling::DevTypeSchema& schema,
		const tooling::DevTypeOps& operations,
		const void* source) noexcept;
	tooling::DevValueOperationStatus initializeMove(
		const tooling::DevTypeSchema& schema,
		const tooling::DevTypeOps& operations,
		void* source) noexcept;
	void moveFrom(DevOwnedValue&& other) noexcept;

	alignas(std::max_align_t) std::array<std::byte, InlineBytes> inline_{};
	std::unique_ptr<std::vector<RetainedTextReference>> retained_text_{};
	void* allocation_ = nullptr;
	const tooling::DevTypeOps* operations_ = nullptr;
	std::uint32_t size_ = 0;
	std::uint32_t alignment_ = 0;
	bool heap_ = false;
};

/** @brief Mutation operation carried by an owned override command. */
enum class DevOverrideCommandKind : std::uint8_t {
	/** @brief Set a reflected element field. */
	SetElementField,
	/** @brief Clear a reflected element assignment. */
	ClearElementField,
	/** @brief Clear assignments targeting an element definition. */
	ResetDefinition,
	/** @brief Clear assignments targeting an exact instance. */
	ResetInstance,
	/** @brief Clear all element assignments. */
	ClearAllElements,
	/** @brief Begin a coalesced interactive edit. */
	BeginBatchDrag,
	/** @brief Update the current coalesced interactive edit. */
	UpdateBatchDrag,
	/** @brief Finalize the current coalesced interactive edit. */
	EndBatchDrag,
	/** @brief Set a reflected named theme field. */
	SetThemeField,
	/** @brief Clear a named theme assignment. */
	ClearThemeField,
	/** @brief Restore the observed baseline of a named theme field. */
	ResetThemeField,
};

/** @brief Owned command queued for validation and application at a tooling safe point. */
struct DevOverrideCommand {
	/** @brief Mutation, reset, or drag-batch operation to perform. */
	DevOverrideCommandKind kind = DevOverrideCommandKind::SetElementField;
	/** @brief Element destination for element commands. */
	DevElementOverrideTarget element{};
	/** @brief Theme destination for theme commands. */
	DevThemeOverrideTarget theme{};
	/** @brief Root type and nested member path to mutate. */
	DevOverrideFieldKey field{};
	/** @brief Override precedence layer receiving this command. */
	DevOverrideLayer layer = DevOverrideLayer::LiveDefinition;
	/** @brief Owned schema-typed replacement for set/update commands. */
	DevOwnedValue value{};
	/** A live-only discard must not request deletion from the next manifest. */
	bool preserve_baked_assignment = false;
};

/** @brief Owned transaction submitted as a batch to the override engine. */
struct DevChangeSet {
	/** @brief Caller-supplied transaction identity used by acknowledgements. */
	std::uint64_t transaction = 0;
	/** @brief Owned commands validated together at the next safe point. */
	std::vector<DevOverrideCommand> commands{};
};

/** @brief Acknowledged application or rejection result for queued tooling work. */
enum class DevCommandStatus : std::uint8_t {
	/** @brief Command was applied. */
	Applied,
	/** @brief Ingress capacity rejected the transaction. */
	QueueFull,
	/** @brief Change set contained no commands. */
	EmptyTransaction,
	/** @brief Target identity or scope is invalid. */
	InvalidTarget,
	/** @brief No compatible schema publication is available. */
	SchemaUnavailable,
	/** @brief Command metadata belongs to a different schema. */
	SchemaMismatch,
	/** @brief Requested member cannot be resolved. */
	FieldUnavailable,
	/** @brief Effective policy forbids editing this member. */
	FieldReadOnly,
	/** @brief Replacement type differs from the reflected member type. */
	TypeMismatch,
	/** @brief Replacement violates a declared constraint. */
	ConstraintRejected,
	/** @brief Named theme variant is absent. */
	ThemeVariantUnavailable,
	/** @brief Owned replacement or baseline could not be copied. */
	ValueCopyFailed,
	/** @brief No eligible persisted change exists. */
	NothingToBake,
	/** @brief Manifest input failed validation. */
	BakeManifestInvalid,
	/** @brief Candidate manifest could not be written. */
	BakeWriteFailed,
	/** @brief Tooling operation failed before it could complete. */
	InternalFailure,
};

/** @brief Acknowledgement of one command or transaction-level rejection. */
struct DevCommandResult {
	/** @brief Transaction identity supplied with the submitted change set. */
	std::uint64_t transaction = 0;
	/** @brief Zero-based command position within the submitted change set. */
	std::uint32_t command = 0;
	/** @brief Applied state or machine-readable rejection reason. */
	DevCommandStatus status = DevCommandStatus::Applied;
	/** @brief Whether the command was committed to live override state. */
	bool applied = false;
};

/** Availability of an owned value at a named capture stage. */
enum class DevValueCaptureStatus : std::uint8_t {
	/** @brief Owned value is available. */
	Available,
	/** @brief The current stage did not observe a value. */
	Unavailable,
	/** @brief Type has no supported capture/serialization adapter. */
	Unsupported,
	/** @brief Typed value copy failed. */
	CopyFailed,
	/** @brief Observed field metadata no longer matches the requested schema. */
	SchemaMismatch,
	/** @brief Definition-wide observations disagree across instances. */
	Ambiguous,
};

/** @brief Owned effective/authored values for one reflected field at a capture stage. */
struct DevCapturedField {
	/** @brief Owned effective value observed after overrides. */
	DevOwnedValue value{};
	/** Application-supplied value observed before baked and live assignments. */
	DevOwnedValue authoredValue{};
	/** One-based index into DevSchemaGeneration::fields. */
	tooling::DevFieldIndex field{};
	/** @brief Highest-precedence contributing layer when overridden is true. */
	DevOverrideLayer winningLayer = DevOverrideLayer::LiveDefinition;
	/** @brief Whether the effective value was captured or why it was unavailable. */
	DevValueCaptureStatus captureStatus = DevValueCaptureStatus::Unavailable;
	/** @brief Whether the authored baseline was captured or why it was unavailable. */
	DevValueCaptureStatus authoredStatus = DevValueCaptureStatus::Unavailable;
	/** @brief Whether a retained assignment contributed to this effective value. */
	bool overridden = false;
};

/** Invocation-local owned authored fields, pinned to their originating schema. */
struct DevAuthoredElementCapture {
	/** @brief Shared immutable generation pinning the field indices and operations. */
	tooling::DevSchemaView schema{};
	/** @brief Owned authored field values before baked/live assignments. */
	std::vector<DevCapturedField> fields{};
	/** @brief Availability of the authored capture as a whole. */
	DevValueCaptureStatus status = DevValueCaptureStatus::Unavailable;
};

/** @brief Ranges of owned parameter, state, and resource field values for one invocation. */
struct DevCapturedElement {
	/** @brief Captured element definition identity. */
	FlowDefinitionID definition{};
	/** @brief Application window owning the invocation. */
	WindowId window = InvalidWindowId;
	/** @brief Captured stable instance identity. */
	::FlowUi::detail::element::ElementInstanceKey instance{};
	/** @brief Node index in the associated completed Flow tree. */
	std::uint32_t flowNode = 0;
	/** @brief First parameter-field index in the snapshot field array. */
	std::uint32_t firstField = 0;
	/** @brief Number of captured parameter fields. */
	std::uint32_t fieldCount = 0;
	/** @brief First state-field index in the snapshot field array. */
	std::uint32_t firstStateField = 0;
	/** @brief Number of captured state fields. */
	std::uint32_t stateFieldCount = 0;
	/** @brief First resource-field index in the snapshot field array. */
	std::uint32_t firstResourceField = 0;
	/** @brief Number of captured resource fields. */
	std::uint32_t resourceFieldCount = 0;
	/** @brief Whether the invocation belongs to protected developer UI. */
	bool isDevInternal = false;
};

/** @brief Owned immutable window publication; consumers borrow its arrays until the next publication. */
struct DevElementCaptureSnapshot {
	/** @brief Application window represented by this publication. */
	WindowId window = InvalidWindowId;
	/** @brief Window frame associated with these values. */
	std::uint64_t frameNumber = 0;
	/** @brief Monotonic capture publication identity. */
	std::uint64_t generation = 0;
	/** @brief Shared immutable schema generation pinning field metadata. */
	tooling::DevSchemaView schema{};
	/** @brief Contiguous captured invocation records. */
	std::vector<DevCapturedElement> elements{};
	/** @brief Contiguous owned field values referenced by element ranges. */
	std::vector<DevCapturedField> fields{};
};

/** @brief One named theme variant and its range in a theme capture publication. */
struct DevCapturedTheme {
	/** @brief Reflected theme type identity. */
	tooling::DevTypeId themeType = 0;
	/** @brief Theme revision observed during capture. */
	std::uint64_t revision = 0;
	/** @brief Byte offset of the variant name in snapshot strings. */
	std::uint32_t variantOffset = 0;
	/** @brief Byte length of the variant name. */
	std::uint32_t variantSize = 0;
	/** @brief First captured field index in the snapshot field array. */
	std::uint32_t firstField = 0;
	/** @brief Number of captured fields for this variant. */
	std::uint32_t fieldCount = 0;
	/** @brief Whether this variant was active during capture. */
	bool active = false;
};

/** @brief Owned immutable publication of registered theme values and variant names. */
struct DevThemeCaptureSnapshot {
	/** @brief Monotonic theme capture publication identity. */
	std::uint64_t generation = 0;
	/** @brief Shared immutable schema generation pinning field metadata. */
	tooling::DevSchemaView schema{};
	/** @brief Owned UTF-8 variant-name pool. */
	std::vector<char> strings{};
	/** @brief Contiguous captured theme variant records. */
	std::vector<DevCapturedTheme> themes{};
	/** @brief Contiguous owned field values referenced by theme ranges. */
	std::vector<DevCapturedField> fields{};

	/** @brief Borrow a captured theme's variant name from this snapshot; return empty for an invalid range. */
	[[nodiscard]] std::string_view variant(const DevCapturedTheme& theme) const noexcept {
		if (theme.variantOffset > strings.size() ||
			theme.variantSize > strings.size() - theme.variantOffset) return {};
		return {strings.data() + theme.variantOffset, theme.variantSize};
	}
};

/** @brief Copied transaction, capture, and retained-storage accounting. */
struct DevOverrideStats {
	/** @brief Cumulative successfully committed change sets. */
	std::uint64_t committedTransactions = 0;
	/** @brief Cumulative rejected change sets. */
	std::uint64_t rejectedTransactions = 0;
	/** @brief Cumulative reflected assignments applied to element drafts. */
	std::uint64_t appliedElementFields = 0;
	/** @brief Cumulative captured element field values. */
	std::uint64_t capturedElementFields = 0;
	/** @brief Cumulative captured theme field values. */
	std::uint64_t capturedThemeFields = 0;
	/** @brief Current retained element assignments. */
	std::uint32_t activeElementOverrides = 0;
	/** @brief Current retained theme assignments. */
	std::uint32_t activeThemeOverrides = 0;
	/** @brief Observed retained override/capture backing storage in bytes. */
	std::size_t memoryFootprintBytes = 0;
};

} // namespace FlowUi::devSystems::tooling

#endif
