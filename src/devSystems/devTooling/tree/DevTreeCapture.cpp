#include "devSystems/devTooling/tree/DevTreeCapture.hpp"

#if FLOW_UI_DEV_MODE

#include <algorithm>
#include <limits>
#include <utility>

#include "devSystems/devMonitoringAndReporting/memory/DevContainerMemory.hpp"
#include "devSystems/devMonitoringAndReporting/memory/DevMemorySources.hpp"
#include "devSystems/devMonitoringAndReporting/timing/DevTimingZone.hpp"

namespace FlowUi::devSystems::tooling {
namespace {

template<class Vector>
void clearRetained(Vector& value) noexcept { value.clear(); }

template<class Vector>
uint64_t liveBytes(const Vector& value) noexcept {
	return static_cast<uint64_t>(value.size()) * sizeof(typename Vector::value_type);
}

template<class Vector>
uint64_t capacityBytes(const Vector& value) noexcept {
	return static_cast<uint64_t>(value.capacity()) * sizeof(typename Vector::value_type);
}

} // namespace

DevTreeCapture::DevTreeCapture(DevTreeCaptureConfig config) : config_(config) {
	building_.flow.nodes.reserve(config_.flowNodeReserve);
	building_.flow.roots.reserve(config_.flowNodeReserve / 4u);
	building_.strings.reserve(config_.stringByteReserve);
	building_.diagnostics.reserve(config_.diagnosticReserve);
	published_.flow.nodes.reserve(config_.flowNodeReserve);
	published_.flow.roots.reserve(config_.flowNodeReserve / 4u);
	published_.strings.reserve(config_.stringByteReserve);
	published_.diagnostics.reserve(config_.diagnosticReserve);
	activeFlowScopes_.reserve(64);
	lastFlowChild_.reserve(config_.flowNodeReserve);
#if FLOW_UI_DEV_CAPTURE_CLAY
	building_.flow.directClayNodes.reserve(config_.directLinkReserve);
	building_.clay.nodes.reserve(config_.clayNodeReserve);
	building_.clay.roots.reserve(config_.clayRootReserve);
	published_.flow.directClayNodes.reserve(config_.directLinkReserve);
	published_.clay.nodes.reserve(config_.clayNodeReserve);
	published_.clay.roots.reserve(config_.clayRootReserve);
	clayTraversalScratch_.reserve(config_.clayNodeReserve);
	layoutIndexToClayNode_.reserve(config_.clayNodeReserve);
	clayIdIndex_.reserve(config_.clayNodeReserve);
	lastClayChild_.reserve(config_.clayNodeReserve);
	clayOpenByDepth_.reserve(64);
	ownershipEvents_.reserve(config_.flowNodeReserve * 2u);
	ownershipActive_.reserve(64);
	directClayCounts_.reserve(config_.flowNodeReserve);
	directClayCursors_.reserve(config_.flowNodeReserve);
	clayRootOwners_.reserve(config_.clayNodeReserve);
#endif
}

void DevTreeCapture::clearSnapshot(DevTreeSnapshot& snapshot) noexcept {
	clearRetained(snapshot.flow.nodes);
	clearRetained(snapshot.flow.roots);
#if FLOW_UI_DEV_CAPTURE_CLAY
	clearRetained(snapshot.flow.directClayNodes);
	clearRetained(snapshot.clay.nodes);
	clearRetained(snapshot.clay.roots);
	snapshot.authoredClayElementCount = 0;
#endif
	clearRetained(snapshot.strings);
	clearRetained(snapshot.diagnostics);
	snapshot.stats = {};
}

void DevTreeCapture::beginFrame(
	WindowId window,
	uint64_t frameNumber,
	Clay_Context& clay,
	DevTimingRecorder* timing) noexcept {
	if (frameActive_) cancelFrame();
	clearSnapshot(building_);
	building_.window = window;
	building_.frameNumber = frameNumber;
	building_.stats.frameNumber = frameNumber;
	clearRetained(activeFlowScopes_);
	clearRetained(lastFlowChild_);
	clay_ = &clay;
	timing_ = timing;
	lastFinishFailed_ = false;
	++frameGeneration_;
	if (frameGeneration_ == 0) ++frameGeneration_;
	frameActive_ = true;
}

DevTreeStringRef DevTreeCapture::copyString(std::string_view value) {
	if (value.empty()) return {};
	if (value.size() > config_.maximumStringBytes ||
		building_.strings.size() > config_.maximumStringBytes - value.size()) {
		const bool firstFailure = !building_.stats.truncated;
		building_.stats.truncated = true;
		if (firstFailure) addDiagnostic({.code = DevTreeDiagnosticCode::StringCapacityExceeded});
		return {};
	}
	const auto oldCapacity = building_.strings.capacity();
	const auto offset = static_cast<uint32_t>(building_.strings.size());
	building_.strings.insert(building_.strings.end(), value.begin(), value.end());
	if (building_.strings.capacity() != oldCapacity) ++building_.stats.vectorGrowthOperations;
	return {offset, static_cast<uint32_t>(value.size())};
}

void DevTreeCapture::addDiagnostic(DevTreeDiagnostic diagnostic) noexcept {
	try { building_.diagnostics.push_back(diagnostic); }
	catch (...) { building_.stats.truncated = true; }
}

DevTreeCapture::Token DevTreeCapture::beginFlow(const FlowBegin& begin) noexcept {
	if (!frameActive_) return {};
	Token token{.frameGeneration = frameGeneration_, .scopeId = nextScopeId_++};
	if (nextScopeId_ == 0) nextScopeId_ = 1;
	const bool parentSuppressed =
		!activeFlowScopes_.empty() && activeFlowScopes_.back().suppressed;
	const bool atCapacity = building_.flow.nodes.size() >= config_.maximumFlowNodes;
	const bool suppressed = begin.suppress || parentSuppressed || atCapacity;
	try {
		if (atCapacity && !parentSuppressed) {
			building_.stats.truncated = true;
			addDiagnostic({.code = DevTreeDiagnosticCode::FlowNodeCapacityExceeded});
		}
		if (suppressed) {
			activeFlowScopes_.push_back({.token = token, .suppressed = true});
			return token;
		}

		const DevFlowNodeIndex index = static_cast<DevFlowNodeIndex>(building_.flow.nodes.size());
		const DevFlowNodeIndex parent = activeFlowScopes_.empty()
			? InvalidFlowNode : activeFlowScopes_.back().token.node;
		DevFlowNode node{};
		node.parent = parent;
		node.depth = static_cast<uint32_t>(activeFlowScopes_.size());
		node.definition = begin.definition;
		node.instance = ::FlowUi::detail::element::toInstanceKey(begin.instance);
		node.expectedClayId = FlowIDToClayID(begin.instance);
		node.debugName = copyString(begin.instance.debugName);
		node.definitionName = copyString(begin.definitionName);
		node.definitionTypeToken = copyString(begin.definitionTypeToken);
		node.sourceFile = copyString(begin.sourceFile);
		node.sourceFunction = copyString(begin.sourceFunction);
		node.sourceLine = begin.sourceLine;
		node.sourceColumn = begin.sourceColumn;
		node.flags = begin.constructed ? DevFlowNodeFlag::Constructed : DevFlowNodeFlag::Drawn;
		if (begin.internalDev) node.flags |= DevFlowNodeFlag::InternalDev;
#if FLOW_UI_DEV_CAPTURE_CLAY
		node.emissionBegin = ::FlowUi::detail::clayDevLayoutElementCount(*clay_);
#endif
		const auto oldCapacity = building_.flow.nodes.capacity();
		building_.flow.nodes.push_back(node);
		if (building_.flow.nodes.capacity() != oldCapacity) ++building_.stats.vectorGrowthOperations;
		lastFlowChild_.push_back(InvalidFlowNode);
		if (parent == InvalidFlowNode) {
			building_.flow.roots.push_back(index);
		} else {
			DevFlowNode& parentNode = building_.flow.nodes[parent];
			DevFlowNodeIndex& last = lastFlowChild_[parent];
			if (last == InvalidFlowNode) parentNode.firstChild = index;
			else building_.flow.nodes[last].nextSibling = index;
			last = index;
		}
		token.node = index;
		activeFlowScopes_.push_back({.token = token, .suppressed = false});
		return token;
	} catch (...) {
		building_.stats.truncated = true;
		try { activeFlowScopes_.push_back({.token = token, .suppressed = true}); } catch (...) {}
		return token;
	}
}

void DevTreeCapture::endFlow(Token token, bool autoClosed) noexcept {
	if (!frameActive_ || !token || token.frameGeneration != frameGeneration_) return;
	if (activeFlowScopes_.empty() || activeFlowScopes_.back().token.scopeId != token.scopeId) {
		addDiagnostic({.code = DevTreeDiagnosticCode::FlowCaptureUnbalanced, .flow = token.node});
		auto found = std::find_if(activeFlowScopes_.rbegin(), activeFlowScopes_.rend(),
			[token](const ActiveFlowCapture& value) { return value.token.scopeId == token.scopeId; });
		if (found == activeFlowScopes_.rend()) return;
		while (!activeFlowScopes_.empty() && activeFlowScopes_.back().token.scopeId != token.scopeId) {
			const ActiveFlowCapture dangling = activeFlowScopes_.back();
			activeFlowScopes_.pop_back();
			if (!dangling.suppressed && dangling.token.node < building_.flow.nodes.size()) {
				DevFlowNode& node = building_.flow.nodes[dangling.token.node];
#if FLOW_UI_DEV_CAPTURE_CLAY
				node.emissionEnd = ::FlowUi::detail::clayDevLayoutElementCount(*clay_);
#endif
				node.subtreeEnd = static_cast<uint32_t>(building_.flow.nodes.size());
				node.flags |= DevFlowNodeFlag::AutoClosed;
			}
		}
	}
	if (activeFlowScopes_.empty()) return;
	const ActiveFlowCapture active = activeFlowScopes_.back();
	activeFlowScopes_.pop_back();
	if (active.suppressed || active.token.node >= building_.flow.nodes.size()) return;
	DevFlowNode& node = building_.flow.nodes[active.token.node];
#if FLOW_UI_DEV_CAPTURE_CLAY
	node.emissionEnd = ::FlowUi::detail::clayDevLayoutElementCount(*clay_);
#endif
	node.subtreeEnd = static_cast<uint32_t>(building_.flow.nodes.size());
	if (autoClosed) node.flags |= DevFlowNodeFlag::AutoClosed;
}

#if FLOW_UI_DEV_CAPTURE_CLAY
void DevTreeCapture::noteAuthoredClayEnd() noexcept {
	if (frameActive_ && clay_) {
		building_.authoredClayElementCount = ::FlowUi::detail::clayDevLayoutElementCount(*clay_);
	}
}
#endif

void DevTreeCapture::finalizeFlowStats() noexcept {
	building_.stats.flowNodeCount = static_cast<uint32_t>(building_.flow.nodes.size());
	building_.stats.copiedStringBytes = static_cast<uint32_t>(building_.strings.size());
	uint32_t invalid = 0;
	for (const DevFlowNode& node : building_.flow.nodes) {
		uint32_t invalidMask = static_cast<uint32_t>(DevFlowNodeFlag::Truncated);
#if FLOW_UI_DEV_CAPTURE_CLAY
		invalidMask |= static_cast<uint32_t>(DevFlowNodeFlag::MissingClayRoot) |
			static_cast<uint32_t>(DevFlowNodeFlag::DuplicateClayRoot) |
			static_cast<uint32_t>(DevFlowNodeFlag::ClayNameMismatch) |
			static_cast<uint32_t>(DevFlowNodeFlag::EscapedClayEmission) |
			static_cast<uint32_t>(DevFlowNodeFlag::ClayParentMismatch);
#endif
		if ((static_cast<uint32_t>(node.flags) & invalidMask) != 0) ++invalid;
	}
	building_.stats.invalidFlowNodeCount = invalid;
	building_.stats.complete = !building_.stats.truncated;
}

void DevTreeCapture::updateMemoryStats(DevTreeSnapshot& snapshot) noexcept {
	uint64_t live = liveBytes(snapshot.flow.nodes) + liveBytes(snapshot.flow.roots) +
		liveBytes(snapshot.strings) + liveBytes(snapshot.diagnostics);
	uint64_t capacity = capacityBytes(snapshot.flow.nodes) + capacityBytes(snapshot.flow.roots) +
		capacityBytes(snapshot.strings) + capacityBytes(snapshot.diagnostics);
#if FLOW_UI_DEV_CAPTURE_CLAY
	live += liveBytes(snapshot.flow.directClayNodes) + liveBytes(snapshot.clay.nodes) +
		liveBytes(snapshot.clay.roots);
	capacity += capacityBytes(snapshot.flow.directClayNodes) + capacityBytes(snapshot.clay.nodes) +
		capacityBytes(snapshot.clay.roots);
#endif
	snapshot.stats.logicalLiveBytes = live;
	snapshot.stats.backingCapacityBytes = capacity;
	peakLogicalLiveBytes_ = std::max(peakLogicalLiveBytes_, live);
	peakBackingCapacityBytes_ = std::max(peakBackingCapacityBytes_, capacity);
	snapshot.stats.peakLogicalLiveBytes = peakLogicalLiveBytes_;
	snapshot.stats.peakBackingCapacityBytes = peakBackingCapacityBytes_;
}

void DevTreeCapture::publish() noexcept {
	building_.generation = ++publishedGeneration_;
	updateMemoryStats(building_);
	std::swap(building_, published_);
	clearSnapshot(building_);
	frameActive_ = false;
	clay_ = nullptr;
	timing_ = nullptr;
}

void DevTreeCapture::finishAfterLayout() noexcept {
	if (!frameActive_) return;
	while (!activeFlowScopes_.empty()) {
		const ActiveFlowCapture active = activeFlowScopes_.back();
		activeFlowScopes_.pop_back();
		if (active.suppressed || active.token.node >= building_.flow.nodes.size()) continue;
		DevFlowNode& node = building_.flow.nodes[active.token.node];
#if FLOW_UI_DEV_CAPTURE_CLAY
		node.emissionEnd = building_.authoredClayElementCount;
#endif
		node.subtreeEnd = static_cast<uint32_t>(building_.flow.nodes.size());
		node.flags |= DevFlowNodeFlag::AutoClosed;
		addDiagnostic({.code = DevTreeDiagnosticCode::FlowCaptureUnbalanced, .flow = active.token.node});
	}
#if FLOW_UI_DEV_CAPTURE_CLAY
	{
		FLOWUI_DEV_TIMING_ZONE_BALANCED_IF(
			timing_, TimingCategory::DevTool, TimingZoneRole::DevToolWork,
			"flowui.dev_tree.clay_bridge");
		if (!copyClayForest()) {
			lastFinishFailed_ = true;
			frameActive_ = false;
			clay_ = nullptr;
			timing_ = nullptr;
			return;
		}
	}
	try {
		{
			FLOWUI_DEV_TIMING_ZONE_BALANCED_IF(
				timing_, TimingCategory::DevTool, TimingZoneRole::DevToolWork,
				"flowui.dev_tree.correlate");
			resolveClayLinksAndDuplicates();
			assignFlowOwnership();
			packDirectClayLinks();
		}
		{
			FLOWUI_DEV_TIMING_ZONE_BALANCED_IF(
				timing_, TimingCategory::DevTool, TimingZoneRole::DevToolWork,
				"flowui.dev_tree.validate");
			resolveAndValidateFlowRoots();
		}
	} catch (...) {
		lastFinishFailed_ = true;
		frameActive_ = false;
		clay_ = nullptr;
		timing_ = nullptr;
		return;
	}
#endif
	finalizeFlowStats();
	FLOWUI_DEV_TIMING_ZONE_BALANCED_IF(
		timing_, TimingCategory::DevTool, TimingZoneRole::DevToolWork,
		"flowui.dev_tree.publish");
	publish();
}

void DevTreeCapture::cancelFrame() noexcept {
	if (!frameActive_) return;
	for (const ActiveFlowCapture& active : activeFlowScopes_) {
		if (!active.suppressed && active.token.node < building_.flow.nodes.size()) {
			building_.flow.nodes[active.token.node].flags |= DevFlowNodeFlag::CaptureCanceled;
		}
	}
	clearRetained(activeFlowScopes_);
	clearRetained(lastFlowChild_);
	clearSnapshot(building_);
	frameActive_ = false;
	clay_ = nullptr;
	timing_ = nullptr;
}

#if FLOW_UI_DEV_CAPTURE_CLAY
bool DevTreeCapture::copyRootCallback(void* userData, const ::FlowUi::detail::ClayDevRootView& root) noexcept {
	auto& context = *static_cast<CopyContext*>(userData);
	try { return context.self->copyRoot(root); }
	catch (...) { context.failed = true; return false; }
}

bool DevTreeCapture::copyElementCallback(void* userData, const ::FlowUi::detail::ClayDevElementView& element) noexcept {
	auto& context = *static_cast<CopyContext*>(userData);
	try { return context.self->copyElement(element); }
	catch (...) { context.failed = true; return false; }
}

bool DevTreeCapture::copyRoot(const ::FlowUi::detail::ClayDevRootView& root) {
	if (building_.clay.nodes.size() >= config_.maximumClayNodes) return false;
	currentClayRoot_ = static_cast<uint32_t>(building_.clay.roots.size());
	building_.clay.roots.push_back({
		.attachmentParentClayId = root.parentId,
		.clipClayId = root.clipElementId,
		.zIndex = root.zIndex,
		.paintOrder = root.paintOrder,
	});
	clayOpenByDepth_.clear();
	return true;
}

bool DevTreeCapture::copyElement(const ::FlowUi::detail::ClayDevElementView& view) {
	if (building_.clay.nodes.size() >= config_.maximumClayNodes ||
		view.layoutElementIndex < 0 ||
		static_cast<size_t>(view.layoutElementIndex) >= layoutIndexToClayNode_.size()) return false;
	DevClayNode node{};
	node.rootIndex = currentClayRoot_;
	node.depthWithinRoot = view.depthWithinRoot;
	node.layoutElementIndex = static_cast<uint32_t>(view.layoutElementIndex);
	node.clayId = view.id;
	if (view.idString.chars && view.idString.length > 0) {
		node.idString = copyString({view.idString.chars, static_cast<size_t>(view.idString.length)});
	}
	node.bounds = view.bounds;
	if (!view.boundsAvailable) node.flags |= DevClayNodeFlag::BoundsUnavailable;
	node.dimensions = view.dimensions;
	node.minDimensions = view.minDimensions;
	node.clipClayId = view.clipElementId;
	node.declaration = view.declaration;
	node.pointerPresence.imageData = node.declaration.image.imageData != nullptr;
	node.pointerPresence.customData = node.declaration.custom.customData != nullptr;
	node.pointerPresence.userData = node.declaration.userData != nullptr;
	node.pointerPresence.transitionHandler = node.declaration.transition.handler != nullptr;
	node.pointerPresence.transitionInitialState = node.declaration.transition.enter.setInitialState != nullptr;
	node.pointerPresence.transitionFinalState = node.declaration.transition.exit.setFinalState != nullptr;
	node.declaration.image.imageData = nullptr;
	node.declaration.custom.customData = nullptr;
	node.declaration.userData = nullptr;
	node.declaration.transition.handler = nullptr;
	node.declaration.transition.enter.setInitialState = nullptr;
	node.declaration.transition.exit.setFinalState = nullptr;
	if (view.isText) {
		node.flags |= DevClayNodeFlag::Text;
		node.textConfig = view.textConfig;
		node.pointerPresence.textUserData = node.textConfig.userData != nullptr;
		node.textConfig.userData = nullptr;
		if (view.text.chars && view.text.length > 0) {
			node.text = copyString({view.text.chars, static_cast<size_t>(view.text.length)});
		}
		node.unwrappedTextDimensions = view.unwrappedTextDimensions;
		node.wrappedLineCount = view.wrappedLineCount;
	}
	if (view.exiting) node.flags |= DevClayNodeFlag::Exiting;
	if (node.declaration.floating.attachTo != CLAY_ATTACH_TO_NONE) node.flags |= DevClayNodeFlag::Floating;
	if (node.layoutElementIndex >= building_.authoredClayElementCount) {
		node.flags |= DevClayNodeFlag::SyntheticAfterBuild;
	}
	const DevClayNodeIndex index = static_cast<DevClayNodeIndex>(building_.clay.nodes.size());
	if (view.parentLayoutElementIndex >= 0) {
		const auto parentOrdinal = static_cast<size_t>(view.parentLayoutElementIndex);
		if (parentOrdinal >= layoutIndexToClayNode_.size()) return false;
		node.parent = layoutIndexToClayNode_[parentOrdinal];
		if (node.parent == InvalidClayNode) return false;
	}
	building_.clay.nodes.push_back(node);
	layoutIndexToClayNode_[node.layoutElementIndex] = index;
	lastClayChild_.push_back(InvalidClayNode);
	if (node.parent != InvalidClayNode) {
		DevClayNodeIndex& last = lastClayChild_[node.parent];
		if (last == InvalidClayNode) building_.clay.nodes[node.parent].firstChild = index;
		else building_.clay.nodes[last].nextSibling = index;
		last = index;
	} else {
		if (currentClayRoot_ >= building_.clay.roots.size()) return false;
		building_.clay.roots[currentClayRoot_].node = index;
	}
	clayIdIndex_.push_back({node.clayId, index});
	return true;
}

bool DevTreeCapture::copyClayForest() noexcept {
	try {
		const uint32_t count = ::FlowUi::detail::clayDevLayoutElementCount(*clay_);
		if (count > config_.maximumClayNodes) {
			building_.stats.truncated = true;
			addDiagnostic({.code = DevTreeDiagnosticCode::ClayNodeCapacityExceeded});
			return false;
		}
		building_.clay.nodes.clear();
		building_.clay.roots.clear();
		building_.flow.directClayNodes.clear();
		clayTraversalScratch_.resize(std::max<uint32_t>(count, 1u));
		layoutIndexToClayNode_.assign(count, InvalidClayNode);
		clayIdIndex_.clear();
		lastClayChild_.clear();
		CopyContext context{.self = this};
		const auto result = ::FlowUi::detail::clayDevVisitTrees(
			*clay_, clayTraversalScratch_, &context,
			{.onRoot = &copyRootCallback, .onElement = &copyElementCallback});
		if (result != ::FlowUi::detail::ClayDevVisitResult::Complete || context.failed) {
			addDiagnostic({.code = DevTreeDiagnosticCode::ClayBridgeTraversalFailed,
				.expected = static_cast<uint64_t>(::FlowUi::detail::ClayDevVisitResult::Complete),
				.observed = static_cast<uint64_t>(result)});
			return false;
		}
		finishClaySubtrees();
		building_.stats.clayNodeCount = static_cast<uint32_t>(building_.clay.nodes.size());
		building_.stats.clayRootCount = static_cast<uint32_t>(building_.clay.roots.size());
		return true;
	} catch (...) {
		addDiagnostic({.code = DevTreeDiagnosticCode::ClayBridgeTraversalFailed});
		return false;
	}
}

void DevTreeCapture::finishClaySubtrees() {
	clayOpenByDepth_.clear();
	uint32_t priorRoot = UINT32_MAX;
	for (DevClayNodeIndex index = 0; index < building_.clay.nodes.size(); ++index) {
		DevClayNode& node = building_.clay.nodes[index];
		node.subtreeEnd = static_cast<uint32_t>(building_.clay.nodes.size());
		if (node.rootIndex != priorRoot) {
			while (!clayOpenByDepth_.empty()) {
				building_.clay.nodes[clayOpenByDepth_.back()].subtreeEnd = index;
				clayOpenByDepth_.pop_back();
			}
			priorRoot = node.rootIndex;
		}
		while (clayOpenByDepth_.size() > node.depthWithinRoot) {
			building_.clay.nodes[clayOpenByDepth_.back()].subtreeEnd = index;
			clayOpenByDepth_.pop_back();
		}
		clayOpenByDepth_.push_back(index);
	}
	while (!clayOpenByDepth_.empty()) {
		building_.clay.nodes[clayOpenByDepth_.back()].subtreeEnd =
			static_cast<uint32_t>(building_.clay.nodes.size());
		clayOpenByDepth_.pop_back();
	}
}

DevClayNodeIndex DevTreeCapture::uniqueClayNode(uint32_t id) const noexcept {
	auto first = std::lower_bound(clayIdIndex_.begin(), clayIdIndex_.end(), id,
		[](const ClayIdIndexEntry& entry, uint32_t value) { return entry.clayId < value; });
	if (first == clayIdIndex_.end() || first->clayId != id) return InvalidClayNode;
	auto next = first + 1;
	if (next != clayIdIndex_.end() && next->clayId == id) return InvalidClayNode;
	return first->node;
}

void DevTreeCapture::resolveClayLinksAndDuplicates() {
	std::sort(clayIdIndex_.begin(), clayIdIndex_.end(),
		[](const ClayIdIndexEntry& left, const ClayIdIndexEntry& right) {
			return left.clayId < right.clayId ||
				(left.clayId == right.clayId && left.node < right.node);
		});
	for (size_t begin = 0; begin < clayIdIndex_.size();) {
		size_t end = begin + 1;
		while (end < clayIdIndex_.size() && clayIdIndex_[end].clayId == clayIdIndex_[begin].clayId) ++end;
		if (end - begin > 1) {
			for (size_t i = begin; i < end; ++i) {
				building_.clay.nodes[clayIdIndex_[i].node].flags |= DevClayNodeFlag::DuplicateId;
			}
			addDiagnostic({.code = DevTreeDiagnosticCode::ClayBridgeIdCollision,
				.clay = clayIdIndex_[begin].node, .observed = clayIdIndex_[begin].clayId});
		}
		begin = end;
	}
	for (DevClayRoot& root : building_.clay.roots) {
		if (root.attachmentParentClayId != 0) {
			root.attachmentParent = uniqueClayNode(root.attachmentParentClayId);
			if (root.attachmentParent == InvalidClayNode) addDiagnostic({
				.code = DevTreeDiagnosticCode::ClayAttachmentParentMissing,
				.clay = root.node, .expected = root.attachmentParentClayId});
		}
		if (root.clipClayId != 0) {
			root.clipNode = uniqueClayNode(root.clipClayId);
			if (root.clipNode == InvalidClayNode) addDiagnostic({
				.code = DevTreeDiagnosticCode::ClayClipNodeMissing,
				.clay = root.node, .expected = root.clipClayId});
		}
	}
}

void DevTreeCapture::assignFlowOwnership() {
	ownershipEvents_.clear();
	for (DevFlowNodeIndex flow = 0; flow < building_.flow.nodes.size(); ++flow) {
		const DevFlowNode& node = building_.flow.nodes[flow];
		if (node.emissionBegin >= node.emissionEnd) continue;
		ownershipEvents_.push_back({node.emissionBegin, flow, node.depth, OwnershipEventKind::Begin});
		ownershipEvents_.push_back({node.emissionEnd, flow, node.depth, OwnershipEventKind::End});
	}
	std::sort(ownershipEvents_.begin(), ownershipEvents_.end(),
		[](const OwnershipEvent& left, const OwnershipEvent& right) {
			if (left.ordinal != right.ordinal) return left.ordinal < right.ordinal;
			if (left.kind != right.kind) return left.kind == OwnershipEventKind::End;
			return left.kind == OwnershipEventKind::End
				? left.depth > right.depth : left.depth < right.depth;
		});
	ownershipActive_.clear();
	size_t event = 0;
	for (uint32_t ordinal = 0; ordinal < building_.authoredClayElementCount; ++ordinal) {
		while (event < ownershipEvents_.size() && ownershipEvents_[event].ordinal == ordinal) {
			const OwnershipEvent current = ownershipEvents_[event++];
			if (current.kind == OwnershipEventKind::Begin) ownershipActive_.push_back(current.flow);
			else {
				auto found = std::find(ownershipActive_.rbegin(), ownershipActive_.rend(), current.flow);
				if (found != ownershipActive_.rend()) ownershipActive_.erase(std::next(found).base());
				else addDiagnostic({.code = DevTreeDiagnosticCode::FlowCaptureUnbalanced, .flow = current.flow});
			}
		}
		if (ordinal >= layoutIndexToClayNode_.size()) continue;
		const DevClayNodeIndex clay = layoutIndexToClayNode_[ordinal];
		if (clay == InvalidClayNode) continue;
		DevClayNode& node = building_.clay.nodes[clay];
		if (ownershipActive_.empty()) node.flags |= DevClayNodeFlag::UnownedRawClay;
		else node.directFlowOwner = ownershipActive_.back();
	}
	for (DevClayNode& node : building_.clay.nodes) {
		if (node.directFlowOwner == InvalidFlowNode) {
			node.flags |= DevClayNodeFlag::UnownedRawClay;
			++building_.stats.rawClayNodeCount;
		}
		if (hasFlag(node.flags, DevClayNodeFlag::SyntheticAfterBuild)) {
			node.directFlowOwner = InvalidFlowNode;
			++building_.stats.syntheticClayNodeCount;
		}
	}
}

void DevTreeCapture::packDirectClayLinks() {
	directClayCounts_.assign(building_.flow.nodes.size(), 0);
	for (const DevClayNode& node : building_.clay.nodes) {
		if (node.directFlowOwner != InvalidFlowNode && node.directFlowOwner < directClayCounts_.size()) {
			++directClayCounts_[node.directFlowOwner];
		}
	}
	uint32_t offset = 0;
	for (DevFlowNodeIndex flow = 0; flow < building_.flow.nodes.size(); ++flow) {
		DevFlowNode& node = building_.flow.nodes[flow];
		node.firstDirectClay = offset;
		node.directClayCount = directClayCounts_[flow];
		offset += node.directClayCount;
	}
	building_.flow.directClayNodes.assign(offset, InvalidClayNode);
	directClayCursors_.resize(building_.flow.nodes.size());
	for (DevFlowNodeIndex flow = 0; flow < building_.flow.nodes.size(); ++flow) {
		directClayCursors_[flow] = building_.flow.nodes[flow].firstDirectClay;
	}
	for (DevClayNodeIndex clay = 0; clay < building_.clay.nodes.size(); ++clay) {
		const DevFlowNodeIndex owner = building_.clay.nodes[clay].directFlowOwner;
		if (owner != InvalidFlowNode && owner < directClayCursors_.size()) {
			building_.flow.directClayNodes[directClayCursors_[owner]++] = clay;
		}
	}
	building_.stats.directLinkCount = offset;
}

bool DevTreeCapture::isClayRoot(DevClayNodeIndex node) const noexcept {
	return std::any_of(building_.clay.roots.begin(), building_.clay.roots.end(),
		[node](const DevClayRoot& root) { return root.node == node; });
}

void DevTreeCapture::resolveAndValidateFlowRoots() {
	for (DevFlowNodeIndex flow = 0; flow < building_.flow.nodes.size(); ++flow) {
		DevFlowNode& node = building_.flow.nodes[flow];
		uint32_t matches = 0;
		for (DevClayNodeIndex clay : directClayContribution(building_, flow)) {
			if (clay < building_.clay.nodes.size() &&
				building_.clay.nodes[clay].clayId == node.expectedClayId) {
				node.clayRoot = clay;
				++matches;
			}
		}
		if (matches == 0) {
			node.flags |= DevFlowNodeFlag::MissingClayRoot;
			addDiagnostic({.code = DevTreeDiagnosticCode::FlowElementMissingClayRoot,
				.flow = flow, .expected = node.expectedClayId});
			continue;
		}
		if (matches > 1) {
			node.flags |= DevFlowNodeFlag::DuplicateClayRoot;
			addDiagnostic({.code = DevTreeDiagnosticCode::FlowElementDuplicateClayRoot,
				.flow = flow, .expected = node.expectedClayId, .observed = matches});
			node.clayRoot = InvalidClayNode;
			continue;
		}
		const std::string_view flowName = building_.string(node.debugName);
		const std::string_view clayName = building_.string(building_.clay.nodes[node.clayRoot].idString);
		if (!flowName.empty() && !clayName.empty() && flowName != clayName) {
			node.flags |= DevFlowNodeFlag::ClayNameMismatch;
			addDiagnostic({.code = DevTreeDiagnosticCode::FlowClayDebugNameMismatch,
				.flow = flow, .clay = node.clayRoot});
		}
		const DevClayNode& root = building_.clay.nodes[node.clayRoot];
		for (DevClayNodeIndex clay : directClayContribution(building_, flow)) {
			if (clay < node.clayRoot || clay >= root.subtreeEnd) {
				node.flags |= DevFlowNodeFlag::EscapedClayEmission;
				addDiagnostic({.code = DevTreeDiagnosticCode::FlowElementEmittedClayOutsideRoot,
					.flow = flow, .clay = clay});
				break;
			}
		}
		if (root.declaration.floating.attachTo != CLAY_ATTACH_TO_NONE && isClayRoot(node.clayRoot)) {
			node.flags |= DevFlowNodeFlag::FloatingClayRoot;
		}
	}

	clayRootOwners_.assign(building_.clay.nodes.size(), InvalidFlowNode);
	for (DevFlowNodeIndex flow = 0; flow < building_.flow.nodes.size(); ++flow) {
		const DevClayNodeIndex root = building_.flow.nodes[flow].clayRoot;
		if (root < clayRootOwners_.size()) clayRootOwners_[root] = flow;
	}
	for (DevFlowNodeIndex flow = 0; flow < building_.flow.nodes.size(); ++flow) {
		DevFlowNode& node = building_.flow.nodes[flow];
		if (node.parent == InvalidFlowNode || node.clayRoot == InvalidClayNode) continue;
		if (hasFlag(node.flags, DevFlowNodeFlag::FloatingClayRoot)) continue;
		DevClayNodeIndex ancestor = building_.clay.nodes[node.clayRoot].parent;
		DevFlowNodeIndex nearest = InvalidFlowNode;
		while (ancestor != InvalidClayNode) {
			if (ancestor < clayRootOwners_.size() && clayRootOwners_[ancestor] != InvalidFlowNode) {
				nearest = clayRootOwners_[ancestor];
				break;
			}
			ancestor = building_.clay.nodes[ancestor].parent;
		}
		if (nearest != node.parent) {
			node.flags |= DevFlowNodeFlag::ClayParentMismatch;
			addDiagnostic({.code = DevTreeDiagnosticCode::FlowChildClayParentMismatch,
				.flow = flow, .clay = node.clayRoot, .expected = node.parent, .observed = nearest});
		}
	}
}
#endif

void DevTreeCapture::appendDevMemorySamples(MemorySampleSink& sink) const noexcept {
#if FLOWUI_DEV_MEMORY_LEVEL >= 2
	try {
		DevContainerMemoryAccumulator memory{};
		const auto addSnapshot = [&](const DevTreeSnapshot& value) {
			memory.add(value.flow.nodes);
			memory.add(value.flow.roots);
#if FLOW_UI_DEV_CAPTURE_CLAY
			memory.add(value.flow.directClayNodes);
			memory.add(value.clay.nodes);
			memory.add(value.clay.roots);
#endif
			memory.add(value.strings);
			memory.add(value.diagnostics);
		};
		addSnapshot(building_);
		addSnapshot(published_);
		memory.add(activeFlowScopes_);
		memory.add(lastFlowChild_);
#if FLOW_UI_DEV_CAPTURE_CLAY
		memory.add(clayTraversalScratch_);
		memory.add(layoutIndexToClayNode_);
		memory.add(clayIdIndex_);
		memory.add(lastClayChild_);
		memory.add(clayOpenByDepth_);
		memory.add(ownershipEvents_);
		memory.add(ownershipActive_);
		memory.add(directClayCounts_);
		memory.add(directClayCursors_);
		memory.add(clayRootOwners_);
#endif
		appendManagerSample(sink, memory_sources::kDevTreeCapture.id, memory, published_.window);
	} catch (...) {}
#else
	(void)sink;
#endif
}

} // namespace FlowUi::devSystems::tooling

#endif
