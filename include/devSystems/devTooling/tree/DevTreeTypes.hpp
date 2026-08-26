#pragma once

#include "FlowUi/BuildConfig.hpp"

#if FLOW_UI_DEV_MODE

#include <cstdint>
#include <span>
#include <string_view>
#include <vector>

#include <clay.h>

#include "FlowUi/ElementID.hpp"
#include "FlowUi/WindowId.hpp"
#include "internal/ElementInstanceKey.hpp"

namespace FlowUi::devSystems::tooling {

using DevFlowNodeIndex = uint32_t;
inline constexpr DevFlowNodeIndex InvalidFlowNode = UINT32_MAX;

#if FLOW_UI_DEV_CAPTURE_CLAY
using DevClayNodeIndex = uint32_t;
inline constexpr DevClayNodeIndex InvalidClayNode = UINT32_MAX;
#endif

struct DevTreeStringRef {
	uint32_t offset = 0;
	uint32_t length = 0;
	[[nodiscard]] constexpr explicit operator bool() const noexcept { return length != 0; }
};

enum class DevFlowNodeFlag : uint32_t {
	None = 0,
	Constructed = 1u << 0u,
	Drawn = 1u << 1u,
	InternalDev = 1u << 2u,
	AutoClosed = 1u << 3u,
	CaptureCanceled = 1u << 4u,
#if FLOW_UI_DEV_CAPTURE_CLAY
	MissingClayRoot = 1u << 5u,
	DuplicateClayRoot = 1u << 6u,
	ClayNameMismatch = 1u << 7u,
	EscapedClayEmission = 1u << 8u,
	ClayParentMismatch = 1u << 9u,
	FloatingClayRoot = 1u << 10u,
#endif
	Truncated = 1u << 11u,
};

constexpr DevFlowNodeFlag operator|(DevFlowNodeFlag left, DevFlowNodeFlag right) noexcept {
	return static_cast<DevFlowNodeFlag>(static_cast<uint32_t>(left) | static_cast<uint32_t>(right));
}
constexpr DevFlowNodeFlag& operator|=(DevFlowNodeFlag& left, DevFlowNodeFlag right) noexcept {
	left = left | right;
	return left;
}
[[nodiscard]] constexpr bool hasFlag(DevFlowNodeFlag value, DevFlowNodeFlag flag) noexcept {
	return (static_cast<uint32_t>(value) & static_cast<uint32_t>(flag)) != 0;
}

struct DevFlowNode {
	DevFlowNodeIndex parent = InvalidFlowNode;
	DevFlowNodeIndex firstChild = InvalidFlowNode;
	DevFlowNodeIndex nextSibling = InvalidFlowNode;
	uint32_t subtreeEnd = 0;
	uint32_t depth = 0;
	FlowDefinitionID definition{};
	::FlowUi::detail::element::ElementInstanceKey instance{};
	uint32_t expectedClayId = 0;
	DevTreeStringRef debugName{};
	DevTreeStringRef definitionName{};
	DevTreeStringRef definitionTypeToken{};
	DevTreeStringRef sourceFile{};
	DevTreeStringRef sourceFunction{};
	uint32_t sourceLine = 0;
	uint32_t sourceColumn = 0;
#if FLOW_UI_DEV_CAPTURE_CLAY
	uint32_t emissionBegin = 0;
	uint32_t emissionEnd = 0;
	DevClayNodeIndex clayRoot = InvalidClayNode;
	uint32_t firstDirectClay = 0;
	uint32_t directClayCount = 0;
#endif
	DevFlowNodeFlag flags = DevFlowNodeFlag::None;
};

struct DevFlowForest {
	std::vector<DevFlowNode> nodes{};
	std::vector<DevFlowNodeIndex> roots{};
#if FLOW_UI_DEV_CAPTURE_CLAY
	std::vector<DevClayNodeIndex> directClayNodes{};
#endif
};

#if FLOW_UI_DEV_CAPTURE_CLAY
struct DevClayPointerPresence {
	bool imageData = false;
	bool customData = false;
	bool userData = false;
	bool textUserData = false;
	bool transitionHandler = false;
	bool transitionInitialState = false;
	bool transitionFinalState = false;
};

enum class DevClayNodeFlag : uint32_t {
	None = 0,
	Text = 1u << 0u,
	Floating = 1u << 1u,
	Exiting = 1u << 2u,
	SyntheticAfterBuild = 1u << 3u,
	DuplicateId = 1u << 4u,
	BoundsUnavailable = 1u << 5u,
	UnownedRawClay = 1u << 6u,
};

constexpr DevClayNodeFlag operator|(DevClayNodeFlag left, DevClayNodeFlag right) noexcept {
	return static_cast<DevClayNodeFlag>(static_cast<uint32_t>(left) | static_cast<uint32_t>(right));
}
constexpr DevClayNodeFlag& operator|=(DevClayNodeFlag& left, DevClayNodeFlag right) noexcept {
	left = left | right;
	return left;
}
[[nodiscard]] constexpr bool hasFlag(DevClayNodeFlag value, DevClayNodeFlag flag) noexcept {
	return (static_cast<uint32_t>(value) & static_cast<uint32_t>(flag)) != 0;
}

struct DevClayNode {
	DevClayNodeIndex parent = InvalidClayNode;
	DevClayNodeIndex firstChild = InvalidClayNode;
	DevClayNodeIndex nextSibling = InvalidClayNode;
	uint32_t subtreeEnd = 0;
	uint32_t depthWithinRoot = 0;
	uint32_t rootIndex = 0;
	uint32_t layoutElementIndex = 0;
	uint32_t clayId = 0;
	DevTreeStringRef idString{};
	Clay_BoundingBox bounds{};
	Clay_Dimensions dimensions{};
	Clay_Dimensions minDimensions{};
	uint32_t clipClayId = 0;
	Clay_ElementDeclaration declaration{}; // all pointer/function fields are nulled
	Clay_TextElementConfig textConfig{}; // userData is nulled
	DevClayPointerPresence pointerPresence{};
	DevTreeStringRef text{};
	Clay_Dimensions unwrappedTextDimensions{};
	uint32_t wrappedLineCount = 0;
	DevFlowNodeIndex directFlowOwner = InvalidFlowNode;
	DevClayNodeFlag flags = DevClayNodeFlag::None;
};

struct DevClayRoot {
	DevClayNodeIndex node = InvalidClayNode;
	uint32_t attachmentParentClayId = 0;
	DevClayNodeIndex attachmentParent = InvalidClayNode;
	uint32_t clipClayId = 0;
	DevClayNodeIndex clipNode = InvalidClayNode;
	int16_t zIndex = 0;
	uint32_t paintOrder = 0;
};

struct DevClayForest {
	std::vector<DevClayNode> nodes{};
	std::vector<DevClayRoot> roots{};
};
#endif

enum class DevTreeDiagnosticCode : uint16_t {
	FlowCaptureUnbalanced,
	FlowNodeCapacityExceeded,
	StringCapacityExceeded,
#if FLOW_UI_DEV_CAPTURE_CLAY
	FlowElementMissingClayRoot,
	FlowElementDuplicateClayRoot,
	FlowClayDebugNameMismatch,
	FlowElementEmittedClayOutsideRoot,
	FlowChildClayParentMismatch,
	ClayBridgeIdCollision,
	ClayAttachmentParentMissing,
	ClayClipNodeMissing,
	ClayBridgeTraversalFailed,
	ClayNodeCapacityExceeded,
#endif
};

struct DevTreeDiagnostic {
	DevTreeDiagnosticCode code{};
	DevFlowNodeIndex flow = InvalidFlowNode;
#if FLOW_UI_DEV_CAPTURE_CLAY
	DevClayNodeIndex clay = InvalidClayNode;
#endif
	uint64_t expected = 0;
	uint64_t observed = 0;
};

struct DevTreeCaptureStats {
	uint64_t frameNumber = 0;
	uint32_t flowNodeCount = 0;
#if FLOW_UI_DEV_CAPTURE_CLAY
	uint32_t clayNodeCount = 0;
	uint32_t clayRootCount = 0;
	uint32_t directLinkCount = 0;
	uint32_t rawClayNodeCount = 0;
	uint32_t syntheticClayNodeCount = 0;
#endif
	uint32_t invalidFlowNodeCount = 0;
	uint32_t copiedStringBytes = 0;
	uint32_t vectorGrowthOperations = 0;
	uint64_t logicalLiveBytes = 0;
	uint64_t backingCapacityBytes = 0;
	uint64_t peakLogicalLiveBytes = 0;
	uint64_t peakBackingCapacityBytes = 0;
	bool complete = false;
	bool truncated = false;
};

struct DevTreeSnapshot {
	WindowId window = InvalidWindowId;
	uint64_t frameNumber = 0;
	uint64_t generation = 0;
#if FLOW_UI_DEV_CAPTURE_CLAY
	uint32_t authoredClayElementCount = 0;
#endif
	DevFlowForest flow{};
#if FLOW_UI_DEV_CAPTURE_CLAY
	DevClayForest clay{};
#endif
	std::vector<char> strings{};
	std::vector<DevTreeDiagnostic> diagnostics{};
	DevTreeCaptureStats stats{};

	[[nodiscard]] std::string_view string(DevTreeStringRef ref) const noexcept {
		if (ref.offset > strings.size() || ref.length > strings.size() - ref.offset) return {};
		return {strings.data() + ref.offset, ref.length};
	}
};

#if FLOW_UI_DEV_CAPTURE_CLAY
[[nodiscard]] inline std::span<const DevClayNode> fullClaySubtree(
	const DevTreeSnapshot& snapshot, DevFlowNodeIndex flow) noexcept {
	if (flow >= snapshot.flow.nodes.size()) return {};
	const DevClayNodeIndex root = snapshot.flow.nodes[flow].clayRoot;
	if (root >= snapshot.clay.nodes.size()) return {};
	const uint32_t end = snapshot.clay.nodes[root].subtreeEnd;
	if (end < root || end > snapshot.clay.nodes.size()) return {};
	return std::span<const DevClayNode>(snapshot.clay.nodes).subspan(root, end - root);
}

[[nodiscard]] inline std::span<const DevClayNodeIndex> directClayContribution(
	const DevTreeSnapshot& snapshot, DevFlowNodeIndex flow) noexcept {
	if (flow >= snapshot.flow.nodes.size()) return {};
	const DevFlowNode& node = snapshot.flow.nodes[flow];
	if (node.firstDirectClay > snapshot.flow.directClayNodes.size() ||
		node.directClayCount > snapshot.flow.directClayNodes.size() - node.firstDirectClay) return {};
	return std::span<const DevClayNodeIndex>(snapshot.flow.directClayNodes)
		.subspan(node.firstDirectClay, node.directClayCount);
}
#endif

} // namespace FlowUi::devSystems::tooling

#endif
