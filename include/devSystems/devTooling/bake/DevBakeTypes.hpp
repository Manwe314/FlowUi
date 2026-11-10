#pragma once
#include "FlowUi/Private/storageSystem/capacity/MemoryCapacityManifest.hpp"
#include <optional>

#include "FlowUi/Public/BuildConfig.hpp"

#if FLOW_UI_DEV_MODE

#include <cstddef>
#include <cstdint>
#include <string>
#include <utility>
#include <vector>

#include "FlowUi/Public/identificationTypes/ElementID.hpp"
#include "devSystems/devTooling/override/DevOverrideTypes.hpp"
#include "FlowUi/Private/elementSystem/ElementInstanceKey.hpp"

namespace FlowUi::devSystems::tooling {

/** @brief Domain receiving a persisted reflected assignment. */
enum class DevBakeTargetKind : std::uint8_t {
	/** @brief Element parameter destination. */
	Element,
	/** @brief Named theme variant destination. */
	Theme,
};

/** @brief Reason a live change cannot be persisted or compiled. */
enum class DevBakeDiagnosticCode : std::uint16_t {
	/** @brief No diagnostic. */
	None,
	/** @brief Required reflected schema is unavailable. */
	SchemaUnavailable,
	/** @brief Instance identity cannot safely be persisted. */
	UnstableInstanceKey,
	/** @brief Value has no supported serialization/generation adapter. */
	UnsupportedValueType,
	/** @brief Generated assignment cannot name an includable source header. */
	MissingSourceHeader,
	/** @brief Existing manifest could not be read. */
	ManifestReadFailed,
	/** @brief Manifest content or version failed validation. */
	ManifestInvalid,
	/** @brief Candidate manifest could not be written. */
	ManifestWriteFailed,
};

/** @brief Owned authorship metadata preserved with a persisted reflected assignment. */
struct DevBakeProvenance {
	/** @brief Author label saved with this assignment. */
	std::string author{"developer"};
	/** @brief Optional human explanation for this saved change. */
	std::string note{};
	/** @brief Timestamp string retained as manifest metadata. */
	std::string timestamp{};
	/** Unknown historical provenance members survive compatible manifest rewrites. */
	std::vector<std::pair<std::string, std::string>> preserved_metadata{};
};

/** @brief Owned, serializable assignment to an element field or named theme field. */
struct DevBakeEntry {
	/** @brief Whether this assignment targets element parameters or a theme. */
	DevBakeTargetKind targetKind = DevBakeTargetKind::Element;
	/** @brief Definition-wide or exact-instance scope for an element assignment. */
	DevOverrideScope targetScope = DevOverrideScope::Definition;
	/** @brief Element definition identity; unused for theme assignments. */
	FlowDefinitionID definition{};
	/** @brief Reflected theme type identity; unused for element assignments. */
	tooling::DevTypeId themeType = 0;
	/** @brief Owned name of the theme variant receiving this assignment. */
	std::string themeVariant{};
	/** @brief Exact element instance key when targetScope selects an instance. */
	::FlowUi::detail::element::ElementInstanceKey instanceKey{};
	/** @brief Owned diagnostic instance label; does not replace numeric identity. */
	std::string instanceDebugLabel{};
	/** @brief Reflected leaf field identity. */
	tooling::DevFieldId fieldId = 0;
	/** @brief Complete nested member path used for generated assignments. */
	std::string fieldPath{};
	/** @brief Reflected type identity of the assigned value. */
	tooling::DevTypeId valueType = 0;
	/** @brief Owned C++ value-type name used for diagnostics and generation. */
	std::string valueTypeName{};
	/** @brief Complete C++ owner type used by the code generator. */
	std::string ownerCppType{};
	/** @brief Header to include when compiling the generated assignment. */
	std::string sourceHeader{};
	/** @brief Owned JSON encoding of the reflected value. */
	std::string valueJson{};
	/** @brief Validated C++ initializer encoding of the reflected value. */
	std::string cppValue{};
	/** @brief Owned authorship metadata for this assignment. */
	DevBakeProvenance provenance{};
	/** Compatible extension members retained verbatim as encoded JSON values. */
	std::vector<std::pair<std::string, std::string>> preserved_metadata{};
};

/** @brief Owned manifest envelope for reflected assignments and optional memory capacities. */
struct DevBakeManifest {
	/** @brief Optional persisted capacity selections and growth policy. */
	std::optional<FlowUi::detail::MemoryCapacityManifest> memoryCapacity{};
	/** @brief Supported envelope format version; defaults to version one. */
	std::uint32_t manifestVersion = 1;
	/** @brief Fingerprint of the schema used to materialize assignments. */
	std::uint64_t schemaFingerprint = 0;
	/** @brief Build provenance string retained with the manifest. */
	std::string buildFingerprint{};
	/** @brief Manifest creation timestamp retained as metadata. */
	std::string createdTimestamp{};
	/** @brief Owned element/theme assignments in persisted order. */
	std::vector<DevBakeEntry> entries{};
	/** Unknown envelope metadata is retained while supported manifest versions are rewritten. */
	std::vector<std::pair<std::string, std::string>> preserved_metadata{};
};

/** @brief Owned reason that an observed live edit cannot be persisted or generated. */
struct DevBakeDiagnostic {
	/** @brief Machine-readable bake failure classification. */
	DevBakeDiagnosticCode code = DevBakeDiagnosticCode::None;
	/** @brief Owned diagnostic explanation. */
	std::string message{};
	/** @brief Element definition associated with this diagnostic, when applicable. */
	FlowDefinitionID definition{};
	/** @brief Exact instance associated with the diagnostic, when applicable. */
	::FlowUi::detail::element::ElementInstanceKey instance{};
	/** @brief Reflected leaf field associated with the diagnostic. */
	tooling::DevFieldId fieldId = 0;
};

/** @brief Owned summary of current live-override persistence eligibility. */
struct DevBakeStatusSnapshot {
	/** @brief Number of current live overrides considered for baking. */
	std::size_t activeLiveOverrideCount = 0;
	/** @brief Number that can be represented by supported manifest assignments. */
	std::size_t bakeableOverrideCount = 0;
	/** @brief Number rejected for persistence or generation. */
	std::size_t unbakeableOverrideCount = 0;
	/** @brief Owned explanations for rejected or unavailable edits. */
	std::vector<DevBakeDiagnostic> bakeDiagnostics{};
};

/** @brief Owned comparison of authored, effective, and compiled field values. */
struct DevBakeDiffEntry {
	/** @brief Named theme variant for a theme-target row. */
	std::string themeVariant{};
	/** @brief Complete nested member path for this comparison. */
	std::string fieldPath{};
	/** @brief Formatted application-authored baseline. */
	std::string authoredValueString{};
	/** @brief Formatted currently effective value. */
	std::string activeValueString{};
	/** @brief Formatted value compiled into this executable. */
	std::string bakedValueString{};
	/** @brief Element definition for an element-target row. */
	FlowDefinitionID definition{};
	/** @brief Reflected theme type for a theme-target row. */
	tooling::DevTypeId themeType = 0;
	/** @brief Instance identity for exact-instance comparisons. */
	::FlowUi::detail::element::ElementInstanceKey instance{};
	/** @brief Reflected leaf field identity. */
	tooling::DevFieldId fieldId = 0;
	/** @brief Whether the row describes element or theme data. */
	DevBakeTargetKind targetKind = DevBakeTargetKind::Element;
	/** @brief Whether a live override contributes to the effective value. */
	bool isOverridden = false;
	/** @brief Whether a compiled assignment exists for this destination. */
	bool isBaked = false;
};

} // namespace FlowUi::devSystems::tooling

#endif
