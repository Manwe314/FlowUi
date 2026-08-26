#pragma once

#include "FlowUi/BuildConfig.hpp"

#if FLOW_UI_DEV_MODE

#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

#include "FlowUi/ElementID.hpp"
#include "devSystems/devTooling/override/DevOverrideTypes.hpp"
#include "internal/ElementInstanceKey.hpp"

namespace FlowUi::devSystems::tooling {

enum class DevBakeTargetKind : std::uint8_t {
	Element,
	Theme,
};

enum class DevBakeDiagnosticCode : std::uint16_t {
	None,
	SchemaUnavailable,
	UnstableInstanceKey,
	UnsupportedValueType,
	MissingSourceHeader,
	ManifestReadFailed,
	ManifestInvalid,
	ManifestWriteFailed,
};

struct DevBakeProvenance {
	std::string author{"developer"};
	std::string note{};
	std::string timestamp{};
};

struct DevBakeEntry {
	DevBakeTargetKind targetKind = DevBakeTargetKind::Element;
	DevOverrideScope targetScope = DevOverrideScope::Definition;
	FlowDefinitionID definition{};
	devMode::DevTypeId themeType = 0;
	std::string themeVariant{};
	::FlowUi::detail::element::ElementInstanceKey instanceKey{};
	std::string instanceDebugLabel{};
	devMode::DevFieldId fieldId = 0;
	std::string fieldPath{};
	devMode::DevTypeId valueType = 0;
	std::string valueTypeName{};
	std::string ownerCppType{};
	std::string sourceHeader{};
	std::string valueJson{};
	std::string cppValue{};
	DevBakeProvenance provenance{};
};

struct DevBakeManifest {
	std::uint32_t manifestVersion = 1;
	std::uint64_t schemaFingerprint = 0;
	std::string buildFingerprint{};
	std::string createdTimestamp{};
	std::vector<DevBakeEntry> entries{};
};

struct DevBakeDiagnostic {
	DevBakeDiagnosticCode code = DevBakeDiagnosticCode::None;
	std::string message{};
	FlowDefinitionID definition{};
	::FlowUi::detail::element::ElementInstanceKey instance{};
	devMode::DevFieldId fieldId = 0;
};

struct DevBakeStatusSnapshot {
	std::size_t activeLiveOverrideCount = 0;
	std::size_t bakeableOverrideCount = 0;
	std::size_t unbakeableOverrideCount = 0;
	std::vector<DevBakeDiagnostic> bakeDiagnostics{};
};

struct DevBakeDiffEntry {
	DevBakeTargetKind targetKind = DevBakeTargetKind::Element;
	FlowDefinitionID definition{};
	devMode::DevTypeId themeType = 0;
	std::string themeVariant{};
	::FlowUi::detail::element::ElementInstanceKey instance{};
	devMode::DevFieldId fieldId = 0;
	std::string fieldPath{};
	std::string authoredValueString{};
	std::string activeValueString{};
	std::string bakedValueString{};
	bool isOverridden = false;
	bool isBaked = false;
};

} // namespace FlowUi::devSystems::tooling

#endif
