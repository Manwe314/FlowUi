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

namespace FlowUi::devMode {

using DevValue = std::variant<std::monostate, bool, int64_t, double, std::string>;

struct DefinitionFieldKey {
	uint64_t definitionId = 0u;
	uint64_t fieldHash = 0u;

	bool operator==(const DefinitionFieldKey& other) const {
		return definitionId == other.definitionId && fieldHash == other.fieldHash;
	}
};

struct InstanceScopeKey {
	uint64_t definitionId = 0u;
	uint64_t flowId = 0u;
	std::string elementId{};

	bool operator==(const InstanceScopeKey& other) const {
		return
			definitionId == other.definitionId &&
			flowId == other.flowId &&
			elementId == other.elementId;
	}
};

struct InstanceFieldKey {
	uint64_t definitionId = 0u;
	uint64_t flowId = 0u;
	std::string elementId{};
	uint64_t fieldHash = 0u;

	bool operator==(const InstanceFieldKey& other) const {
		return
			definitionId == other.definitionId &&
			flowId == other.flowId &&
			fieldHash == other.fieldHash &&
			elementId == other.elementId;
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

		uint64_t definitionId = 0u;
		uint64_t definitionTypeHash = 0u;
		uint64_t flowId = 0u;
		std::string elementId{};

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
	using SnapshotByDefinitionMap = std::unordered_map<uint64_t, StructSnapshot>;

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
		uint64_t definitionId,
		uint64_t definitionTypeHash,
		uint64_t flowId,
		std::string_view elementId,
		std::string_view definitionDisplayName = {},
		std::string_view definitionTypeToken = {},
		bool isInternalToDevMode = false,
		bool isFloating = false);
	std::size_t appendCapturedClayElement(
		std::string_view elementId,
		uint64_t flowId = 0u,
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

	void setDefinitionParamOverride(uint64_t definitionId, uint64_t fieldHash, const DevValue& value);
	const DevValue* findDefinitionParamOverride(uint64_t definitionId, uint64_t fieldHash) const;
	bool hasDefinitionParamOverride(uint64_t definitionId, uint64_t fieldHash) const;
	bool clearDefinitionParamOverride(uint64_t definitionId, uint64_t fieldHash);
	std::size_t clearDefinitionParamOverridesForDefinition(uint64_t definitionId);

	void setInstanceParamOverride(
		uint64_t definitionId,
		uint64_t flowId,
		std::string_view elementId,
		uint64_t fieldHash,
		const DevValue& value);
	const DevValue* findInstanceParamOverride(
		uint64_t definitionId,
		uint64_t flowId,
		std::string_view elementId,
		uint64_t fieldHash) const;
	bool hasInstanceParamOverride(
		uint64_t definitionId,
		uint64_t flowId,
		std::string_view elementId,
		uint64_t fieldHash) const;
	bool clearInstanceParamOverride(
		uint64_t definitionId,
		uint64_t flowId,
		std::string_view elementId,
		uint64_t fieldHash);
	std::size_t clearInstanceParamOverridesForElement(
		uint64_t definitionId,
		uint64_t flowId,
		std::string_view elementId);

	void setStateOverride(
		uint64_t definitionId,
		uint64_t flowId,
		std::string_view elementId,
		uint64_t fieldHash,
		const DevValue& value);
	const DevValue* findStateOverride(
		uint64_t definitionId,
		uint64_t flowId,
		std::string_view elementId,
		uint64_t fieldHash) const;
	bool clearStateOverride(
		uint64_t definitionId,
		uint64_t flowId,
		std::string_view elementId,
		uint64_t fieldHash);
	std::size_t clearStateOverridesForElement(
		uint64_t definitionId,
		uint64_t flowId,
		std::string_view elementId);

	void setResourceOverride(uint64_t definitionId, uint64_t fieldHash, const DevValue& value);
	const DevValue* findResourceOverride(uint64_t definitionId, uint64_t fieldHash) const;
	bool hasResourceOverride(uint64_t definitionId, uint64_t fieldHash) const;
	bool clearResourceOverride(uint64_t definitionId, uint64_t fieldHash);
	std::size_t clearResourceOverridesForDefinition(uint64_t definitionId);

	void captureLastSeenParamField(
		uint64_t definitionId,
		uint64_t flowId,
		std::string_view elementId,
		uint64_t fieldHash,
		const DevValue& value);
	void captureLastSeenStateField(
		uint64_t definitionId,
		uint64_t flowId,
		std::string_view elementId,
		uint64_t fieldHash,
		const DevValue& value);
	void captureLastSeenResourceField(
		uint64_t definitionId,
		uint64_t fieldHash,
		const DevValue& value);

	const StructSnapshot* findLastSeenParams(
		uint64_t definitionId,
		uint64_t flowId,
		std::string_view elementId) const;
	const StructSnapshot* findLastSeenState(
		uint64_t definitionId,
		uint64_t flowId,
		std::string_view elementId) const;
	const StructSnapshot* findLastSeenResources(uint64_t definitionId) const;

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
	static InstanceScopeKey makeInstanceScopeKey(uint64_t definitionId, uint64_t flowId, std::string_view elementId);
	static InstanceFieldKey makeInstanceFieldKey(
		uint64_t definitionId,
		uint64_t flowId,
		std::string_view elementId,
		uint64_t fieldHash);
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
