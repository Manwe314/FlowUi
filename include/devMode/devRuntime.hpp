#pragma once

#include <cstddef>
#include <cstdint>
#include <limits>
#include <string>
#include <string_view>
#include <unordered_map>
#include <variant>
#include <vector>

#include "FlowUi/BuildConfig.hpp"
#include "FlowUi/ElementID.hpp"
#include "internal/ElementInstanceKey.hpp"

namespace FlowUi::devMode {

struct DevEnum1Value {
	uint8_t numeric = 0u;

	bool operator==(const DevEnum1Value& other) const {
		return numeric == other.numeric;
	}
};

struct DevEnum2Value {
	DevEnum1Value first{};
	DevEnum1Value second{};

	bool operator==(const DevEnum2Value& other) const {
		return first == other.first && second == other.second;
	}
};

struct DevFloat2Value {
	double first = 0.0;
	double second = 0.0;

	bool operator==(const DevFloat2Value& other) const {
		return first == other.first && second == other.second;
	}
};

struct DevFloat4Value {
	double first = 0.0;
	double second = 0.0;
	double third = 0.0;
	double fourth = 0.0;

	bool operator==(const DevFloat4Value& other) const {
		return
			first == other.first &&
			second == other.second &&
			third == other.third &&
			fourth == other.fourth;
	}
};

struct DevEdgeU16Value {
	uint16_t first = 0u;
	uint16_t second = 0u;
	uint16_t third = 0u;
	uint16_t fourth = 0u;
	uint16_t fifth = 0u;

	bool operator==(const DevEdgeU16Value& other) const {
		return
			first == other.first &&
			second == other.second &&
			third == other.third &&
			fourth == other.fourth &&
			fifth == other.fifth;
	}
};

struct DevTaggedUnionValue {
	DevEnum1Value tag{};
	DevFloat2Value minMax{};
	double percent = 0.0;

	bool operator==(const DevTaggedUnionValue& other) const {
		return tag == other.tag && minMax == other.minMax && percent == other.percent;
	}
};

struct DevPointerValue {
	uint64_t bits = 0u;

	bool operator==(const DevPointerValue& other) const {
		return bits == other.bits;
	}
};

struct DevElementIdValue {
	uint32_t id = 0u;
	uint32_t offset = 0u;
	uint32_t baseId = 0u;
	bool isStaticallyAllocated = false;
	std::string stringId{};

	bool operator==(const DevElementIdValue& other) const {
		return
			id == other.id &&
			offset == other.offset &&
			baseId == other.baseId &&
			isStaticallyAllocated == other.isStaticallyAllocated &&
			stringId == other.stringId;
	}
};

struct DevSizingValue {
	DevTaggedUnionValue width{};
	DevTaggedUnionValue height{};

	bool operator==(const DevSizingValue& other) const {
		return width == other.width && height == other.height;
	}
};

struct DevLayoutConfigValue {
	DevSizingValue sizing{};
	DevEdgeU16Value padding{};
	uint16_t childGap = 0u;
	DevEnum2Value childAlignment{};
	DevEnum1Value layoutDirection{};

	bool operator==(const DevLayoutConfigValue& other) const {
		return
			sizing == other.sizing &&
			padding == other.padding &&
			childGap == other.childGap &&
			childAlignment == other.childAlignment &&
			layoutDirection == other.layoutDirection;
	}
};

struct DevTextElementConfigValue {
	DevPointerValue userData{};
	DevFloat4Value textColor{};
	uint16_t fontId = 0u;
	uint16_t fontSize = 0u;
	uint16_t letterSpacing = 0u;
	uint16_t lineHeight = 0u;
	DevEnum1Value wrapMode{};
	DevEnum1Value textAlignment{};

	bool operator==(const DevTextElementConfigValue& other) const {
		return
			userData == other.userData &&
			textColor == other.textColor &&
			fontId == other.fontId &&
			fontSize == other.fontSize &&
			letterSpacing == other.letterSpacing &&
			lineHeight == other.lineHeight &&
			wrapMode == other.wrapMode &&
			textAlignment == other.textAlignment;
	}
};

struct DevFloatingElementConfigValue {
	DevFloat2Value offset{};
	DevFloat2Value expand{};
	uint32_t parentId = 0u;
	int16_t zIndex = 0;
	DevEnum2Value attachPoints{};
	DevEnum1Value pointerCaptureMode{};
	DevEnum1Value attachTo{};
	DevEnum1Value clipTo{};

	bool operator==(const DevFloatingElementConfigValue& other) const {
		return
			offset == other.offset &&
			expand == other.expand &&
			parentId == other.parentId &&
			zIndex == other.zIndex &&
			attachPoints == other.attachPoints &&
			pointerCaptureMode == other.pointerCaptureMode &&
			attachTo == other.attachTo &&
			clipTo == other.clipTo;
	}
};

struct DevClipElementConfigValue {
	bool horizontal = false;
	bool vertical = false;
	DevFloat2Value childOffset{};

	bool operator==(const DevClipElementConfigValue& other) const {
		return
			horizontal == other.horizontal &&
			vertical == other.vertical &&
			childOffset == other.childOffset;
	}
};

struct DevBorderElementConfigValue {
	DevFloat4Value color{};
	DevEdgeU16Value width{};

	bool operator==(const DevBorderElementConfigValue& other) const {
		return color == other.color && width == other.width;
	}
};

struct DevElementDeclarationValue {
	DevElementIdValue id{};
	DevLayoutConfigValue layout{};
	DevFloat4Value backgroundColor{};
	DevFloat4Value cornerRadius{};
	double aspectRatio = 0.0;
	DevPointerValue imageData{};
	DevFloatingElementConfigValue floating{};
	DevPointerValue customData{};
	DevClipElementConfigValue clip{};
	DevBorderElementConfigValue border{};
	DevPointerValue userData{};

	bool operator==(const DevElementDeclarationValue& other) const {
		return
			id == other.id &&
			layout == other.layout &&
			backgroundColor == other.backgroundColor &&
			cornerRadius == other.cornerRadius &&
			aspectRatio == other.aspectRatio &&
			imageData == other.imageData &&
			floating == other.floating &&
			customData == other.customData &&
			clip == other.clip &&
			border == other.border &&
			userData == other.userData;
	}
};

struct DevCompositeStructValue {
	uint64_t typeHash = 0u;
	DevSizingValue sizing{};
	DevLayoutConfigValue layoutConfig{};
	DevTextElementConfigValue textElementConfig{};
	DevFloatingElementConfigValue floatingElementConfig{};
	DevClipElementConfigValue clipElementConfig{};
	DevBorderElementConfigValue borderElementConfig{};
	DevElementDeclarationValue elementDeclaration{};

	bool operator==(const DevCompositeStructValue& other) const {
		return
			typeHash == other.typeHash &&
			sizing == other.sizing &&
			layoutConfig == other.layoutConfig &&
			textElementConfig == other.textElementConfig &&
			floatingElementConfig == other.floatingElementConfig &&
			clipElementConfig == other.clipElementConfig &&
			borderElementConfig == other.borderElementConfig &&
			elementDeclaration == other.elementDeclaration;
	}
};

using DevValue = std::variant<
	std::monostate,
	bool,
	int64_t,
	double,
	std::string,
	DevEnum1Value,
	DevEnum2Value,
	DevFloat2Value,
	DevFloat4Value,
	DevEdgeU16Value,
	DevTaggedUnionValue,
	DevCompositeStructValue>;

struct DefinitionFieldKey {
	FlowDefinitionID definitionId{};
	uint64_t fieldHash = 0u;

	bool operator==(const DefinitionFieldKey& other) const {
		return definitionId == other.definitionId && fieldHash == other.fieldHash;
	}
};

struct InstanceScopeKey {
	FlowDefinitionID definitionId{};
	detail::element::ElementInstanceKey instanceId{};

	bool operator==(const InstanceScopeKey& other) const {
		return
			definitionId == other.definitionId &&
			instanceId == other.instanceId;
	}
};

struct InstanceFieldKey {
	FlowDefinitionID definitionId{};
	detail::element::ElementInstanceKey instanceId{};
	uint64_t fieldHash = 0u;

	bool operator==(const InstanceFieldKey& other) const {
		return
			definitionId == other.definitionId &&
			instanceId == other.instanceId &&
			fieldHash == other.fieldHash;
	}
};

struct DefinitionFieldKeyHash {
	std::size_t operator()(const DefinitionFieldKey& key) const noexcept;
};

struct InstanceScopeKeyHash {
	std::size_t operator()(const InstanceScopeKey& key) const noexcept;
};

struct InstanceFieldKeyHash {
	std::size_t operator()(const InstanceFieldKey& key) const noexcept;
};

struct StructSnapshot {
	std::unordered_map<uint64_t, DevValue> valuesByFieldHash{};
};

struct ElementTreePlaceholder {
	enum class ElementKind : uint8_t {
		Unknown = 0,
		FlowElement = 1,
		ClayElement = 2,
		InternalDevElement = 3,
	};

	struct FlatNode {
		// Pre-order index assigned at capture time.
		uint64_t captureOrder = 0u;
		// Tree depth (0 is root-level).
		uint32_t depth = 0u;
		ElementKind kind = ElementKind::Unknown;

		FlowDefinitionID definitionId{};
		uint64_t definitionTypeHash = 0u;
		detail::element::ElementInstanceKey instanceId{};
		// Display-only copy. Never participates in override or snapshot identity.
		std::string debugPath{};

		// Useful display metadata from registry/definition.
		std::string definitionDisplayName{};
		std::string definitionTypeToken{};
		bool hasRegisteredDefinition = false;
		bool hasRegisteredParamsStruct = false;
		bool hasRegisteredStateStruct = false;
		bool hasRegisteredResourcesStruct = false;

		// Future combiner-tool hints for patching source.
		std::string sourceFile{};
		uint32_t sourceLine = 0u;
		uint32_t sourceColumn = 0u;
		std::string sourceFunction{};
		uint64_t sourceLocationHash = 0u;
		std::string authoredInstanceKey{};
		std::string authoredDefinitionKey{};
		std::string debugLabel{};
		bool isFloating = false;
		bool isInternalToDevMode = false;
	};

	std::vector<FlatNode> flatNodes{};

	void clear() { flatNodes.clear(); }
	std::size_t size() const { return flatNodes.size(); }
	bool empty() const { return flatNodes.empty(); }
};

class DevRuntime {
public:
	static constexpr std::size_t kInvalidCaptureIndex = std::numeric_limits<std::size_t>::max();

	using DefinitionOverrideMap = std::unordered_map<DefinitionFieldKey, DevValue, DefinitionFieldKeyHash>;
	using InstanceOverrideMap = std::unordered_map<InstanceFieldKey, DevValue, InstanceFieldKeyHash>;
	using SnapshotByInstanceMap = std::unordered_map<InstanceScopeKey, StructSnapshot, InstanceScopeKeyHash>;
	using SnapshotByDefinitionMap =
		std::unordered_map<FlowDefinitionID, StructSnapshot, FlowDefinitionIDHash>;

	void beginFrame();
	void endFrame();

	void beginElementTreeCapture();
	void endElementTreeCapture();
	bool isElementTreeCaptureActive() const { return elementTreeCaptureActive_; }
	uint32_t currentElementTreeDepth() const { return elementTreeCaptureDepth_; }

	std::size_t appendCapturedElement(const ElementTreePlaceholder::FlatNode& nodeTemplate);
	void pushElementTreeDepth();
	bool popElementTreeDepth();
	std::size_t beginCapturedElement(
		const ElementTreePlaceholder::FlatNode& nodeTemplate,
		bool suppressCapture = false);
	bool endCapturedElement();
	std::size_t beginCapturedFlowElement(
		FlowDefinitionID definitionId,
		uint64_t definitionTypeHash,
		FlowElementID elementId,
		std::string_view definitionDisplayName = {},
		std::string_view definitionTypeToken = {},
		bool isInternalToDevMode = false,
		bool isFloating = false);
	std::size_t appendCapturedClayElement(
		FlowElementID elementId,
		bool isInternalToDevMode = false,
		bool isFloating = false);
	bool setCapturedElementSource(
		std::size_t captureIndex,
		std::string_view sourceFile,
		uint32_t sourceLine,
		uint32_t sourceColumn = 0u,
		std::string_view sourceFunction = {});
	bool setCapturedElementAuthoringKeys(
		std::size_t captureIndex,
		std::string_view authoredInstanceKey,
		std::string_view authoredDefinitionKey);
	bool setCapturedElementRegistrationMetadata(
		std::size_t captureIndex,
		bool hasRegisteredDefinition,
		bool hasRegisteredParamsStruct,
		bool hasRegisteredStateStruct,
		bool hasRegisteredResourcesStruct,
		std::string_view definitionDisplayName = {},
		std::string_view definitionTypeToken = {});

	uint64_t frameCounter() const { return frameCounter_; }

	bool isDirty() const { return dirty_; }
	bool consumeDirty();

	void clearAllOverrides();
	void clearAllSnapshots();

	void setDefinitionParamOverride(FlowDefinitionID definitionId, uint64_t fieldHash, const DevValue& value);
	const DevValue* findDefinitionParamOverride(FlowDefinitionID definitionId, uint64_t fieldHash) const;
	bool hasDefinitionParamOverride(FlowDefinitionID definitionId, uint64_t fieldHash) const;
	bool clearDefinitionParamOverride(FlowDefinitionID definitionId, uint64_t fieldHash);
	std::size_t clearDefinitionParamOverridesForDefinition(FlowDefinitionID definitionId);

	void setInstanceParamOverride(
		FlowDefinitionID definitionId,
		FlowElementID elementId,
		uint64_t fieldHash,
		const DevValue& value);
	const DevValue* findInstanceParamOverride(
		FlowDefinitionID definitionId,
		FlowElementID elementId,
		uint64_t fieldHash) const;
	bool hasInstanceParamOverride(
		FlowDefinitionID definitionId,
		FlowElementID elementId,
		uint64_t fieldHash) const;
	bool clearInstanceParamOverride(
		FlowDefinitionID definitionId,
		FlowElementID elementId,
		uint64_t fieldHash);
	std::size_t clearInstanceParamOverridesForElement(
		FlowDefinitionID definitionId,
		FlowElementID elementId);

	void setStateOverride(
		FlowDefinitionID definitionId,
		FlowElementID elementId,
		uint64_t fieldHash,
		const DevValue& value);
	const DevValue* findStateOverride(
		FlowDefinitionID definitionId,
		FlowElementID elementId,
		uint64_t fieldHash) const;
	bool clearStateOverride(
		FlowDefinitionID definitionId,
		FlowElementID elementId,
		uint64_t fieldHash);
	std::size_t clearStateOverridesForElement(
		FlowDefinitionID definitionId,
		FlowElementID elementId);

	void setResourceOverride(FlowDefinitionID definitionId, uint64_t fieldHash, const DevValue& value);
	const DevValue* findResourceOverride(FlowDefinitionID definitionId, uint64_t fieldHash) const;
	bool hasResourceOverride(FlowDefinitionID definitionId, uint64_t fieldHash) const;
	bool clearResourceOverride(FlowDefinitionID definitionId, uint64_t fieldHash);
	std::size_t clearResourceOverridesForDefinition(FlowDefinitionID definitionId);

	void captureLastSeenParamField(
		FlowDefinitionID definitionId,
		FlowElementID elementId,
		uint64_t fieldHash,
		const DevValue& value);
	void captureLastSeenStateField(
		FlowDefinitionID definitionId,
		FlowElementID elementId,
		uint64_t fieldHash,
		const DevValue& value);
	void captureLastSeenResourceField(
		FlowDefinitionID definitionId,
		uint64_t fieldHash,
		const DevValue& value);

	const StructSnapshot* findLastSeenParams(
		FlowDefinitionID definitionId,
		FlowElementID elementId) const;
	const StructSnapshot* findLastSeenState(
		FlowDefinitionID definitionId,
		FlowElementID elementId) const;
	const StructSnapshot* findLastSeenResources(FlowDefinitionID definitionId) const;

	const DefinitionOverrideMap& definitionParamOverrides() const { return definitionParamOverrides_; }
	const InstanceOverrideMap& instanceParamOverrides() const { return instanceParamOverrides_; }
	const InstanceOverrideMap& stateOverrides() const { return stateOverrides_; }
	const DefinitionOverrideMap& resourceOverrides() const { return resourceOverrides_; }

	const SnapshotByInstanceMap& lastSeenParamsByInstance() const { return lastSeenParamsByInstance_; }
	const SnapshotByInstanceMap& lastSeenStateByInstance() const { return lastSeenStateByInstance_; }
	const SnapshotByDefinitionMap& lastSeenResourcesByDefinition() const { return lastSeenResourcesByDefinition_; }

	ElementTreePlaceholder& elementTreePlaceholder() { return elementTreePlaceholder_; }
	const ElementTreePlaceholder& elementTreePlaceholder() const { return elementTreePlaceholder_; }

private:
	static InstanceScopeKey makeInstanceScopeKey(
		FlowDefinitionID definitionId,
		FlowElementID elementId) noexcept;
	static InstanceFieldKey makeInstanceFieldKey(
		FlowDefinitionID definitionId,
		FlowElementID elementId,
		uint64_t fieldHash) noexcept;
	static std::size_t clearInstanceScopedEntries(InstanceOverrideMap& map, const InstanceScopeKey& scope);
	bool isSuppressedCaptureActive() const;
	void markDirty() { dirty_ = true; }

private:
	uint64_t frameCounter_ = 0u;
	bool dirty_ = false;
	bool elementTreeCaptureActive_ = false;
	uint32_t elementTreeCaptureDepth_ = 0u;
	uint64_t nextElementCaptureOrder_ = 0u;
	std::vector<bool> elementCaptureSuppressedStack_{};
	DefinitionOverrideMap definitionParamOverrides_{};
	InstanceOverrideMap instanceParamOverrides_{};
	InstanceOverrideMap stateOverrides_{};
	DefinitionOverrideMap resourceOverrides_{};
	SnapshotByInstanceMap lastSeenParamsByInstance_{};
	SnapshotByInstanceMap lastSeenStateByInstance_{};
	SnapshotByDefinitionMap lastSeenResourcesByDefinition_{};
	ElementTreePlaceholder elementTreePlaceholder_{};
};

} // namespace FlowUi::devMode
