#include "devMode/devRuntime.hpp"

#include <functional>
#include <utility>

#if FLOW_UI_DEV_MODE
namespace {

constexpr std::size_t kHashSeed = 0x9e3779b97f4a7c15ull;
constexpr uint64_t kHashSeed64 = 14695981039346656037ull;
constexpr uint64_t kHashPrime64 = 1099511628211ull;

std::size_t hashCombine(std::size_t seed, std::size_t value) {
	seed ^= value + kHashSeed + (seed << 6u) + (seed >> 2u);
	return seed;
}

uint64_t hashString64(std::string_view value) {
	uint64_t hash = kHashSeed64;
	for (char c : value) {
		hash ^= static_cast<uint64_t>(static_cast<unsigned char>(c));
		hash *= kHashPrime64;
	}
	return (hash == 0u) ? 1u : hash;
}

template <typename MapT, typename KeyT>
const typename MapT::mapped_type* findMapValue(const MapT& map, const KeyT& key) {
	const auto it = map.find(key);
	if (it == map.end()) {
		return nullptr;
	}
	return &it->second;
}

template <typename MapT, typename KeyT>
bool setMapValue(MapT& map, const KeyT& key, const FlowUi::devMode::DevValue& value) {
	auto [it, inserted] = map.try_emplace(key, value);
	if (inserted) {
		return true;
	}
	if (it->second == value) {
		return false;
	}
	it->second = value;
	return true;
}

template <typename MapT, typename PredicateT>
std::size_t eraseIf(MapT& map, PredicateT&& predicate) {
	std::size_t removed = 0u;
	for (auto it = map.begin(); it != map.end();) {
		if (predicate(it->first)) {
			it = map.erase(it);
			++removed;
			continue;
		}
		++it;
	}
	return removed;
}

} // namespace

namespace FlowUi::devMode {

std::size_t DefinitionFieldKeyHash::operator()(const DefinitionFieldKey& key) const noexcept {
	std::size_t hash = std::hash<uint64_t>{}(key.definitionId.value);
	hash = hashCombine(hash, std::hash<uint64_t>{}(key.fieldHash));
	return hash;
}

std::size_t InstanceScopeKeyHash::operator()(const InstanceScopeKey& key) const noexcept {
	std::size_t hash = std::hash<uint64_t>{}(key.definitionId.value);
	hash = hashCombine(hash, std::hash<uint64_t>{}(key.instanceId.value));
	return hash;
}

std::size_t InstanceFieldKeyHash::operator()(const InstanceFieldKey& key) const noexcept {
	std::size_t hash = std::hash<uint64_t>{}(key.definitionId.value);
	hash = hashCombine(hash, std::hash<uint64_t>{}(key.instanceId.value));
	hash = hashCombine(hash, std::hash<uint64_t>{}(key.fieldHash));
	return hash;
}

void DevRuntime::beginFrame() {
	++frameCounter_;
	lastSeenParamsByInstance_.clear();
	lastSeenStateByInstance_.clear();
	lastSeenResourcesByDefinition_.clear();
	beginElementTreeCapture();
}

void DevRuntime::endFrame() {
	endElementTreeCapture();
}

void DevRuntime::beginElementTreeCapture() {
	elementTreeCaptureActive_ = true;
	elementTreeCaptureDepth_ = 0u;
	nextElementCaptureOrder_ = 0u;
	elementCaptureSuppressedStack_.clear();
	elementTreePlaceholder_.clear();
}

void DevRuntime::endElementTreeCapture() {
	elementTreeCaptureActive_ = false;
	elementTreeCaptureDepth_ = 0u;
}

std::size_t DevRuntime::appendCapturedElement(const ElementTreePlaceholder::FlatNode& nodeTemplate) {
	if (!elementTreeCaptureActive_ || isSuppressedCaptureActive()) {
		return kInvalidCaptureIndex;
	}

	ElementTreePlaceholder::FlatNode node = nodeTemplate;
	node.captureOrder = nextElementCaptureOrder_++;
	node.depth = elementTreeCaptureDepth_;
	elementTreePlaceholder_.flatNodes.push_back(std::move(node));
	return elementTreePlaceholder_.flatNodes.size() - 1u;
}

void DevRuntime::pushElementTreeDepth() {
	if (!elementTreeCaptureActive_) {
		return;
	}
	++elementTreeCaptureDepth_;
}

bool DevRuntime::popElementTreeDepth() {
	if (!elementTreeCaptureActive_ || elementTreeCaptureDepth_ == 0u) {
		return false;
	}
	--elementTreeCaptureDepth_;
	return true;
}

std::size_t DevRuntime::beginCapturedElement(
	const ElementTreePlaceholder::FlatNode& nodeTemplate,
	bool suppressCapture) {
	if (!elementTreeCaptureActive_) {
		return kInvalidCaptureIndex;
	}

	const bool suppressFromParent = isSuppressedCaptureActive();
	const bool suppressThisNode = suppressCapture || suppressFromParent;
	elementCaptureSuppressedStack_.push_back(suppressThisNode);

	const std::size_t index = suppressThisNode ? kInvalidCaptureIndex : appendCapturedElement(nodeTemplate);
	pushElementTreeDepth();
	return index;
}

bool DevRuntime::endCapturedElement() {
	if (!elementTreeCaptureActive_) {
		return false;
	}
	const bool popped = popElementTreeDepth();
	if (!elementCaptureSuppressedStack_.empty()) {
		elementCaptureSuppressedStack_.pop_back();
	}
	return popped;
}

std::size_t DevRuntime::beginCapturedFlowElement(
	FlowDefinitionID definitionId,
	uint64_t definitionTypeHash,
	FlowElementID elementId,
	std::string_view definitionDisplayName,
	std::string_view definitionTypeToken,
	bool isInternalToDevMode,
	bool isFloating) {
	ElementTreePlaceholder::FlatNode node{};
	node.kind = ElementTreePlaceholder::ElementKind::FlowElement;
	node.definitionId = definitionId;
	node.definitionTypeHash = definitionTypeHash;
	node.instanceId = detail::element::toInstanceKey(elementId);
#if FLOW_UI_DEV_MODE
	node.debugPath.assign(elementId.debugName.data(), elementId.debugName.size());
#endif
	node.definitionDisplayName.assign(definitionDisplayName.data(), definitionDisplayName.size());
	node.definitionTypeToken.assign(definitionTypeToken.data(), definitionTypeToken.size());
	node.isInternalToDevMode = isInternalToDevMode;
	node.isFloating = isFloating;
	return beginCapturedElement(node, isInternalToDevMode);
}

std::size_t DevRuntime::appendCapturedClayElement(
	FlowElementID elementId,
	bool isInternalToDevMode,
	bool isFloating) {
	if (isInternalToDevMode || isSuppressedCaptureActive()) {
		return kInvalidCaptureIndex;
	}

	ElementTreePlaceholder::FlatNode node{};
	node.kind = ElementTreePlaceholder::ElementKind::ClayElement;
	node.instanceId = detail::element::toInstanceKey(elementId);
#if FLOW_UI_DEV_MODE
	node.debugPath.assign(elementId.debugName.data(), elementId.debugName.size());
#endif
	node.isInternalToDevMode = isInternalToDevMode;
	node.isFloating = isFloating;
	return appendCapturedElement(node);
}

bool DevRuntime::isSuppressedCaptureActive() const {
	return !elementCaptureSuppressedStack_.empty() && elementCaptureSuppressedStack_.back();
}

bool DevRuntime::setCapturedElementSource(
	std::size_t captureIndex,
	std::string_view sourceFile,
	uint32_t sourceLine,
	uint32_t sourceColumn,
	std::string_view sourceFunction) {
	if (captureIndex >= elementTreePlaceholder_.flatNodes.size()) {
		return false;
	}

	auto& node = elementTreePlaceholder_.flatNodes[captureIndex];
	node.sourceFile.assign(sourceFile.data(), sourceFile.size());
	node.sourceLine = sourceLine;
	node.sourceColumn = sourceColumn;
	node.sourceFunction.assign(sourceFunction.data(), sourceFunction.size());

	std::string packed{};
	packed.reserve(node.sourceFile.size() + node.sourceFunction.size() + 64u);
	packed.append(node.sourceFile);
	packed.push_back(':');
	packed.append(std::to_string(node.sourceLine));
	packed.push_back(':');
	packed.append(std::to_string(node.sourceColumn));
	packed.push_back(':');
	packed.append(node.sourceFunction);
	node.sourceLocationHash = hashString64(packed);
	return true;
}

bool DevRuntime::setCapturedElementAuthoringKeys(
	std::size_t captureIndex,
	std::string_view authoredInstanceKey,
	std::string_view authoredDefinitionKey) {
	if (captureIndex >= elementTreePlaceholder_.flatNodes.size()) {
		return false;
	}

	auto& node = elementTreePlaceholder_.flatNodes[captureIndex];
	node.authoredInstanceKey.assign(authoredInstanceKey.data(), authoredInstanceKey.size());
	node.authoredDefinitionKey.assign(authoredDefinitionKey.data(), authoredDefinitionKey.size());
	return true;
}

bool DevRuntime::setCapturedElementRegistrationMetadata(
	std::size_t captureIndex,
	bool hasRegisteredDefinition,
	bool hasRegisteredParamsStruct,
	bool hasRegisteredStateStruct,
	bool hasRegisteredResourcesStruct,
	std::string_view definitionDisplayName,
	std::string_view definitionTypeToken) {
	if (captureIndex >= elementTreePlaceholder_.flatNodes.size()) {
		return false;
	}

	auto& node = elementTreePlaceholder_.flatNodes[captureIndex];
	node.hasRegisteredDefinition = hasRegisteredDefinition;
	node.hasRegisteredParamsStruct = hasRegisteredParamsStruct;
	node.hasRegisteredStateStruct = hasRegisteredStateStruct;
	node.hasRegisteredResourcesStruct = hasRegisteredResourcesStruct;
	if (!definitionDisplayName.empty()) {
		node.definitionDisplayName.assign(definitionDisplayName.data(), definitionDisplayName.size());
	}
	if (!definitionTypeToken.empty()) {
		node.definitionTypeToken.assign(definitionTypeToken.data(), definitionTypeToken.size());
	}
	return true;
}

bool DevRuntime::consumeDirty() {
	const bool wasDirty = dirty_;
	dirty_ = false;
	return wasDirty;
}

void DevRuntime::clearAllOverrides() {
	if (
		definitionParamOverrides_.empty() &&
		instanceParamOverrides_.empty() &&
		stateOverrides_.empty() &&
		resourceOverrides_.empty()) {
		return;
	}

	definitionParamOverrides_.clear();
	instanceParamOverrides_.clear();
	stateOverrides_.clear();
	resourceOverrides_.clear();
	markDirty();
}

void DevRuntime::clearAllSnapshots() {
	lastSeenParamsByInstance_.clear();
	lastSeenStateByInstance_.clear();
	lastSeenResourcesByDefinition_.clear();
	elementTreePlaceholder_.clear();
	elementTreeCaptureActive_ = false;
	elementTreeCaptureDepth_ = 0u;
	nextElementCaptureOrder_ = 0u;
	elementCaptureSuppressedStack_.clear();
}

void DevRuntime::setDefinitionParamOverride(FlowDefinitionID definitionId, uint64_t fieldHash, const DevValue& value) {
	const DefinitionFieldKey key{ definitionId, fieldHash };
	if (setMapValue(definitionParamOverrides_, key, value)) {
		markDirty();
	}
}

const DevValue* DevRuntime::findDefinitionParamOverride(FlowDefinitionID definitionId, uint64_t fieldHash) const {
	return findMapValue(definitionParamOverrides_, DefinitionFieldKey{ definitionId, fieldHash });
}

bool DevRuntime::hasDefinitionParamOverride(FlowDefinitionID definitionId, uint64_t fieldHash) const {
	return findDefinitionParamOverride(definitionId, fieldHash) != nullptr;
}

bool DevRuntime::clearDefinitionParamOverride(FlowDefinitionID definitionId, uint64_t fieldHash) {
	const std::size_t removed = definitionParamOverrides_.erase(DefinitionFieldKey{ definitionId, fieldHash });
	if (removed == 0u) {
		return false;
	}
	markDirty();
	return true;
}

std::size_t DevRuntime::clearDefinitionParamOverridesForDefinition(FlowDefinitionID definitionId) {
	const std::size_t removed = eraseIf(
		definitionParamOverrides_,
		[definitionId](const DefinitionFieldKey& key) {
			return key.definitionId == definitionId;
		});
	if (removed > 0u) {
		markDirty();
	}
	return removed;
}

void DevRuntime::setInstanceParamOverride(
	FlowDefinitionID definitionId,
	FlowElementID elementId,
	uint64_t fieldHash,
	const DevValue& value) {
	const InstanceFieldKey key = makeInstanceFieldKey(definitionId, elementId, fieldHash);
	if (setMapValue(instanceParamOverrides_, key, value)) {
		markDirty();
	}
}

const DevValue* DevRuntime::findInstanceParamOverride(
	FlowDefinitionID definitionId,
	FlowElementID elementId,
	uint64_t fieldHash) const {
	return findMapValue(instanceParamOverrides_, makeInstanceFieldKey(definitionId, elementId, fieldHash));
}

bool DevRuntime::hasInstanceParamOverride(
	FlowDefinitionID definitionId,
	FlowElementID elementId,
	uint64_t fieldHash) const {
	return findInstanceParamOverride(definitionId, elementId, fieldHash) != nullptr;
}

bool DevRuntime::clearInstanceParamOverride(
	FlowDefinitionID definitionId,
	FlowElementID elementId,
	uint64_t fieldHash) {
	const std::size_t removed =
		instanceParamOverrides_.erase(makeInstanceFieldKey(definitionId, elementId, fieldHash));
	if (removed == 0u) {
		return false;
	}
	markDirty();
	return true;
}

std::size_t DevRuntime::clearInstanceParamOverridesForElement(
	FlowDefinitionID definitionId,
	FlowElementID elementId) {
	const InstanceScopeKey scope = makeInstanceScopeKey(definitionId, elementId);
	const std::size_t removed = clearInstanceScopedEntries(instanceParamOverrides_, scope);
	if (removed > 0u) {
		markDirty();
	}
	return removed;
}

void DevRuntime::setStateOverride(
	FlowDefinitionID definitionId,
	FlowElementID elementId,
	uint64_t fieldHash,
	const DevValue& value) {
	const InstanceFieldKey key = makeInstanceFieldKey(definitionId, elementId, fieldHash);
	if (setMapValue(stateOverrides_, key, value)) {
		markDirty();
	}
}

const DevValue* DevRuntime::findStateOverride(
	FlowDefinitionID definitionId,
	FlowElementID elementId,
	uint64_t fieldHash) const {
	return findMapValue(stateOverrides_, makeInstanceFieldKey(definitionId, elementId, fieldHash));
}

bool DevRuntime::clearStateOverride(
	FlowDefinitionID definitionId,
	FlowElementID elementId,
	uint64_t fieldHash) {
	const std::size_t removed = stateOverrides_.erase(makeInstanceFieldKey(definitionId, elementId, fieldHash));
	if (removed == 0u) {
		return false;
	}
	markDirty();
	return true;
}

std::size_t DevRuntime::clearStateOverridesForElement(
	FlowDefinitionID definitionId,
	FlowElementID elementId) {
	const InstanceScopeKey scope = makeInstanceScopeKey(definitionId, elementId);
	const std::size_t removed = clearInstanceScopedEntries(stateOverrides_, scope);
	if (removed > 0u) {
		markDirty();
	}
	return removed;
}

void DevRuntime::setResourceOverride(FlowDefinitionID definitionId, uint64_t fieldHash, const DevValue& value) {
	const DefinitionFieldKey key{ definitionId, fieldHash };
	if (setMapValue(resourceOverrides_, key, value)) {
		markDirty();
	}
}

const DevValue* DevRuntime::findResourceOverride(FlowDefinitionID definitionId, uint64_t fieldHash) const {
	return findMapValue(resourceOverrides_, DefinitionFieldKey{ definitionId, fieldHash });
}

bool DevRuntime::hasResourceOverride(FlowDefinitionID definitionId, uint64_t fieldHash) const {
	return findResourceOverride(definitionId, fieldHash) != nullptr;
}

bool DevRuntime::clearResourceOverride(FlowDefinitionID definitionId, uint64_t fieldHash) {
	const std::size_t removed = resourceOverrides_.erase(DefinitionFieldKey{ definitionId, fieldHash });
	if (removed == 0u) {
		return false;
	}
	markDirty();
	return true;
}

std::size_t DevRuntime::clearResourceOverridesForDefinition(FlowDefinitionID definitionId) {
	const std::size_t removed = eraseIf(
		resourceOverrides_,
		[definitionId](const DefinitionFieldKey& key) {
			return key.definitionId == definitionId;
		});
	if (removed > 0u) {
		markDirty();
	}
	return removed;
}

void DevRuntime::captureLastSeenParamField(
	FlowDefinitionID definitionId,
	FlowElementID elementId,
	uint64_t fieldHash,
	const DevValue& value) {
	StructSnapshot& snapshot =
		lastSeenParamsByInstance_[makeInstanceScopeKey(definitionId, elementId)];
	snapshot.valuesByFieldHash[fieldHash] = value;
}

void DevRuntime::captureLastSeenStateField(
	FlowDefinitionID definitionId,
	FlowElementID elementId,
	uint64_t fieldHash,
	const DevValue& value) {
	StructSnapshot& snapshot =
		lastSeenStateByInstance_[makeInstanceScopeKey(definitionId, elementId)];
	snapshot.valuesByFieldHash[fieldHash] = value;
}

void DevRuntime::captureLastSeenResourceField(
	FlowDefinitionID definitionId,
	uint64_t fieldHash,
	const DevValue& value) {
	StructSnapshot& snapshot = lastSeenResourcesByDefinition_[definitionId];
	snapshot.valuesByFieldHash[fieldHash] = value;
}

const StructSnapshot* DevRuntime::findLastSeenParams(
	FlowDefinitionID definitionId,
	FlowElementID elementId) const {
	return findMapValue(lastSeenParamsByInstance_, makeInstanceScopeKey(definitionId, elementId));
}

const StructSnapshot* DevRuntime::findLastSeenState(
	FlowDefinitionID definitionId,
	FlowElementID elementId) const {
	return findMapValue(lastSeenStateByInstance_, makeInstanceScopeKey(definitionId, elementId));
}

const StructSnapshot* DevRuntime::findLastSeenResources(FlowDefinitionID definitionId) const {
	return findMapValue(lastSeenResourcesByDefinition_, definitionId);
}

InstanceScopeKey DevRuntime::makeInstanceScopeKey(
	FlowDefinitionID definitionId,
	FlowElementID elementId) noexcept {
	return InstanceScopeKey{
		.definitionId = definitionId,
		.instanceId = detail::element::toInstanceKey(elementId),
	};
}

InstanceFieldKey DevRuntime::makeInstanceFieldKey(
	FlowDefinitionID definitionId,
	FlowElementID elementId,
	uint64_t fieldHash) noexcept {
	return InstanceFieldKey{
		.definitionId = definitionId,
		.instanceId = detail::element::toInstanceKey(elementId),
		.fieldHash = fieldHash,
	};
}

std::size_t DevRuntime::clearInstanceScopedEntries(InstanceOverrideMap& map, const InstanceScopeKey& scope) {
	return eraseIf(
		map,
		[&scope](const InstanceFieldKey& key) {
			return
				key.definitionId == scope.definitionId &&
				key.instanceId == scope.instanceId;
		});
}

} // namespace FlowUi::devMode
#endif
