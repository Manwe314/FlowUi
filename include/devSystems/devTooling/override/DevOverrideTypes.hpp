#pragma once

#include "FlowUi/BuildConfig.hpp"

#if FLOW_UI_DEV_MODE

#include <array>
#include <cstddef>
#include <cstdint>
#include <new>
#include <string>
#include <string_view>
#include <vector>

#include "FlowUi/ElementID.hpp"
#include "FlowUi/WindowId.hpp"
#include "devSystems/devTooling/schema/DevSchemaTypes.hpp"
#include "internal/ElementInstanceKey.hpp"

namespace FlowUi::devSystems::tooling {

enum class DevOverrideScope : std::uint8_t {
	Definition,
	ExactInstance,
};

enum class DevOverrideLayer : std::uint8_t {
	BakedDefinition,
	BakedInstance,
	LiveDefinition,
	LiveInstance,
	EphemeralPreview,
	Count,
};

inline constexpr std::size_t DevOverrideLayerCount =
	static_cast<std::size_t>(DevOverrideLayer::Count);

struct DevElementOverrideTarget {
	FlowDefinitionID definition{};
	WindowId window = InvalidWindowId;
	::FlowUi::detail::element::ElementInstanceKey instance{};
	std::string instanceDebugLabel{};
	DevOverrideScope scope = DevOverrideScope::Definition;
	bool bakeable = true;

	friend bool operator==(
		const DevElementOverrideTarget& left,
		const DevElementOverrideTarget& right) noexcept {
		return left.definition == right.definition && left.window == right.window &&
			left.instance == right.instance && left.scope == right.scope;
	}
};

struct DevThemeOverrideTarget {
	devMode::DevTypeId themeType = 0;
	std::string variant{};

	friend bool operator==(
		const DevThemeOverrideTarget&,
		const DevThemeOverrideTarget&) = default;
};

struct DevOverrideFieldKey {
	devMode::DevTypeId ownerType = 0;
	devMode::DevFieldId field = 0;
	/** Field IDs from the root owner to the leaf owner, excluding `field`. */
	std::vector<devMode::DevFieldId> nestedPath{};

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
	static constexpr std::size_t InlineBytes = 48;

	DevOwnedValue() noexcept = default;
	~DevOwnedValue() { reset(); }
	DevOwnedValue(const DevOwnedValue& other) noexcept;
	DevOwnedValue& operator=(const DevOwnedValue& other) noexcept;
	DevOwnedValue(DevOwnedValue&& other) noexcept;
	DevOwnedValue& operator=(DevOwnedValue&& other) noexcept;

	[[nodiscard]] explicit operator bool() const noexcept { return operations_ != nullptr; }
	[[nodiscard]] devMode::DevTypeId type() const noexcept {
		return operations_ ? operations_->type : 0;
	}
	[[nodiscard]] const void* data() const noexcept;
	[[nodiscard]] void* data() noexcept;
	[[nodiscard]] std::size_t heapBytes() const noexcept {
		return heap_ ? size_ : 0;
	}

	void reset() noexcept;

	static devMode::DevValueOperationStatus copyFrom(
		const devMode::DevSchemaGeneration& schema,
		devMode::DevTypeIndex type,
		const void* source,
		DevOwnedValue& destination) noexcept;

private:
	devMode::DevValueOperationStatus initializeCopy(
		const devMode::DevTypeSchema& schema,
		const devMode::DevTypeOps& operations,
		const void* source) noexcept;
	devMode::DevValueOperationStatus initializeMove(
		const devMode::DevTypeSchema& schema,
		const devMode::DevTypeOps& operations,
		void* source) noexcept;
	void moveFrom(DevOwnedValue&& other) noexcept;

	alignas(std::max_align_t) std::array<std::byte, InlineBytes> inline_{};
	void* allocation_ = nullptr;
	const devMode::DevTypeOps* operations_ = nullptr;
	std::uint32_t size_ = 0;
	std::uint32_t alignment_ = 0;
	bool heap_ = false;
};

enum class DevOverrideCommandKind : std::uint8_t {
	SetElementField,
	ClearElementField,
	ResetDefinition,
	ResetInstance,
	ClearAllElements,
	BeginBatchDrag,
	UpdateBatchDrag,
	EndBatchDrag,
	SetThemeField,
	ClearThemeField,
};

struct DevOverrideCommand {
	DevOverrideCommandKind kind = DevOverrideCommandKind::SetElementField;
	DevElementOverrideTarget element{};
	DevThemeOverrideTarget theme{};
	DevOverrideFieldKey field{};
	DevOverrideLayer layer = DevOverrideLayer::LiveDefinition;
	DevOwnedValue value{};
};

struct DevChangeSet {
	std::uint64_t transaction = 0;
	std::vector<DevOverrideCommand> commands{};
};

enum class DevCommandStatus : std::uint8_t {
	Applied,
	QueueFull,
	EmptyTransaction,
	InvalidTarget,
	SchemaUnavailable,
	SchemaMismatch,
	FieldUnavailable,
	FieldReadOnly,
	TypeMismatch,
	ConstraintRejected,
	ThemeVariantUnavailable,
	ValueCopyFailed,
	NothingToBake,
	BakeManifestInvalid,
	BakeWriteFailed,
	InternalFailure,
};

struct DevCommandResult {
	std::uint64_t transaction = 0;
	DevCommandStatus status = DevCommandStatus::Applied;
	std::uint32_t command = 0;
	bool applied = false;
};

struct DevCapturedField {
	/** One-based index into DevSchemaGeneration::fields. */
	devMode::DevFieldIndex field{};
	DevOwnedValue value{};
	DevOverrideLayer winningLayer = DevOverrideLayer::LiveDefinition;
	bool overridden = false;
};

struct DevCapturedElement {
	FlowDefinitionID definition{};
	WindowId window = InvalidWindowId;
	::FlowUi::detail::element::ElementInstanceKey instance{};
	std::uint32_t flowNode = 0;
	std::uint32_t firstField = 0;
	std::uint32_t fieldCount = 0;
};

struct DevElementCaptureSnapshot {
	WindowId window = InvalidWindowId;
	std::uint64_t frameNumber = 0;
	std::uint64_t generation = 0;
	devMode::DevSchemaView schema{};
	std::vector<DevCapturedElement> elements{};
	std::vector<DevCapturedField> fields{};
};

struct DevCapturedTheme {
	devMode::DevTypeId themeType = 0;
	std::uint64_t revision = 0;
	std::uint32_t variantOffset = 0;
	std::uint32_t variantSize = 0;
	std::uint32_t firstField = 0;
	std::uint32_t fieldCount = 0;
	bool active = false;
};

struct DevThemeCaptureSnapshot {
	std::uint64_t generation = 0;
	devMode::DevSchemaView schema{};
	std::vector<char> strings{};
	std::vector<DevCapturedTheme> themes{};
	std::vector<DevCapturedField> fields{};

	[[nodiscard]] std::string_view variant(const DevCapturedTheme& theme) const noexcept {
		if (theme.variantOffset > strings.size() ||
			theme.variantSize > strings.size() - theme.variantOffset) return {};
		return {strings.data() + theme.variantOffset, theme.variantSize};
	}
};

struct DevOverrideStats {
	std::uint64_t committedTransactions = 0;
	std::uint64_t rejectedTransactions = 0;
	std::uint64_t appliedElementFields = 0;
	std::uint64_t capturedElementFields = 0;
	std::uint64_t capturedThemeFields = 0;
	std::uint32_t activeElementOverrides = 0;
	std::uint32_t activeThemeOverrides = 0;
	std::size_t memoryFootprintBytes = 0;
};

} // namespace FlowUi::devSystems::tooling

#endif
