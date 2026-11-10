#pragma once

#include "FlowUi/Public/BuildConfig.hpp"

#if FLOW_UI_DEV_MODE

#include <cstdint>
#include <span>
#include <string_view>
#include <vector>

#include <clay.h>

#include "FlowUi/Public/identificationTypes/ElementID.hpp"
#include "FlowUi/Public/identificationTypes/WindowId.hpp"
#include "FlowUi/Private/elementSystem/ElementInstanceKey.hpp"

namespace FlowUi::devSystems::tooling {

/** @brief Zero-based index in one DevTreeSnapshot Flow node array. */
using DevFlowNodeIndex = uint32_t;
/** @brief Sentinel indicating that a Flow node link is absent. */
inline constexpr DevFlowNodeIndex InvalidFlowNode = UINT32_MAX;

#if FLOW_UI_DEV_CAPTURE_CLAY
/** @brief Zero-based index in one DevTreeSnapshot Clay node array. */
using DevClayNodeIndex = uint32_t;
/** @brief Sentinel indicating that a Clay node link is absent. */
inline constexpr DevClayNodeIndex InvalidClayNode = UINT32_MAX;
#endif

/** @brief Byte range in the string pool of one completed tree snapshot. */
struct DevTreeStringRef {
	/** @brief Starting byte offset into DevTreeSnapshot::strings. */
	uint32_t offset = 0;
	/** @brief Byte length of the copied string. */
	uint32_t length = 0;
	/** @brief Return whether the copied string range has a nonzero byte length. */
	[[nodiscard]] constexpr explicit operator bool() const noexcept { return length != 0; }
};

/** @brief Bit flags describing Flow construction and captured Clay ownership. */
enum class DevFlowNodeFlag : uint32_t {
	/** @brief No special observations. */
	None = 0,
	/** @brief Invocation left its root open for manual child emission. */
	Constructed = 1u << 0u,
	/** @brief Invocation used the immediate draw path. */
	Drawn = 1u << 1u,
	/** @brief Node belongs to protected developer UI. */
	InternalDev = 1u << 2u,
	/** @brief Lifecycle recovery closed an unbalanced constructed scope. */
	AutoClosed = 1u << 3u,
	/** @brief Capture of the invocation was canceled. */
	CaptureCanceled = 1u << 4u,
#if FLOW_UI_DEV_CAPTURE_CLAY
	/** @brief Expected numeric Clay root could not be resolved. */
	MissingClayRoot = 1u << 5u,
	/** @brief Multiple Clay nodes claim the expected root ID. */
	DuplicateClayRoot = 1u << 6u,
	/** @brief Flow and Clay diagnostic root names disagree. */
	ClayNameMismatch = 1u << 7u,
	/** @brief Invocation emitted Clay nodes outside its root subtree. */
	EscapedClayEmission = 1u << 8u,
	/** @brief Child Flow root has an unexpected Clay parent. */
	ClayParentMismatch = 1u << 9u,
	/** @brief Flow root participates in a floating attachment forest. */
	FloatingClayRoot = 1u << 10u,
#endif
	/** @brief Configured capture limits truncated this node or its subtree. */
	Truncated = 1u << 11u,
};

/** @brief Combine enabled flags without mutating either input. */
constexpr DevFlowNodeFlag operator|(DevFlowNodeFlag left, DevFlowNodeFlag right) noexcept {
	return static_cast<DevFlowNodeFlag>(static_cast<uint32_t>(left) | static_cast<uint32_t>(right));
}
/** @brief Add right-hand flags to left and return left. */
constexpr DevFlowNodeFlag& operator|=(DevFlowNodeFlag& left, DevFlowNodeFlag right) noexcept {
	left = left | right;
	return left;
}
/** @brief Return whether value contains any bit in flag; combined flags use an any-bit test. */
[[nodiscard]] constexpr bool hasFlag(DevFlowNodeFlag value, DevFlowNodeFlag flag) noexcept {
	return (static_cast<uint32_t>(value) & static_cast<uint32_t>(flag)) != 0;
}

/** @brief Preorder Flow invocation node with stable identity and frame-local links. */
struct DevFlowNode {
	/** @brief Parent node index, or InvalidFlowNode for a root. */
	DevFlowNodeIndex parent = InvalidFlowNode;
	/** @brief First direct child index, or InvalidFlowNode. */
	DevFlowNodeIndex firstChild = InvalidFlowNode;
	/** @brief Next sibling index, or InvalidFlowNode. */
	DevFlowNodeIndex nextSibling = InvalidFlowNode;
	/** @brief Exclusive preorder index ending this node's subtree. */
	uint32_t subtreeEnd = 0;
	/** @brief Depth within the Flow invocation forest. */
	uint32_t depth = 0;
	/** @brief Stable element definition identity. */
	FlowDefinitionID definition{};
	/** @brief Stable resolved element instance address. */
	::FlowUi::detail::element::ElementInstanceKey instance{};
	/** @brief Numeric Clay ID expected for the element's root. */
	uint32_t expectedClayId = 0;
	/** @brief Copied instance diagnostic path in the snapshot string pool. */
	DevTreeStringRef debugName{};
	/** @brief Copied element definition display name. */
	DevTreeStringRef definitionName{};
	/** @brief Copied complete C++ definition type token. */
	DevTreeStringRef definitionTypeToken{};
	/** @brief Copied invocation source path. */
	DevTreeStringRef sourceFile{};
	/** @brief Copied invocation function name. */
	DevTreeStringRef sourceFunction{};
	/** @brief Invocation source line, or zero when unavailable. */
	uint32_t sourceLine = 0;
	/** @brief Invocation source column, or zero when unavailable. */
	uint32_t sourceColumn = 0;
#if FLOW_UI_DEV_CAPTURE_CLAY
	/** @brief First captured Clay emission ordinal belonging to this invocation. */
	uint32_t emissionBegin = 0;
	/** @brief Exclusive end of the invocation's Clay emission range. */
	uint32_t emissionEnd = 0;
	/** @brief Resolved root index in the copied Clay forest. */
	DevClayNodeIndex clayRoot = InvalidClayNode;
	/** @brief First index in DevFlowForest::directClayNodes for this owner. */
	uint32_t firstDirectClay = 0;
	/** @brief Number of directly owned Clay nodes, excluding child Flow invocations. */
	uint32_t directClayCount = 0;
#endif
	/** @brief Construction, ownership, truncation, and validation observations. */
	DevFlowNodeFlag flags = DevFlowNodeFlag::None;
};

/** @brief Contiguous preorder Flow nodes and direct ownership links. */
struct DevFlowForest {
	/** @brief Copied invocation nodes in preorder. */
	std::vector<DevFlowNode> nodes{};
	/** @brief Indices of top-level Flow invocation nodes. */
	std::vector<DevFlowNodeIndex> roots{};
#if FLOW_UI_DEV_CAPTURE_CLAY
	/** @brief Packed directly owned Clay node indices addressed by each Flow node's range. */
	std::vector<DevClayNodeIndex> directClayNodes{};
#endif
};

#if FLOW_UI_DEV_CAPTURE_CLAY
/** @brief Presence flags for pointer/callback fields deliberately removed from copied Clay data. */
struct DevClayPointerPresence {
	/** @brief Whether source image data was nonnull. */
	bool imageData = false;
	/** @brief Whether source custom data was nonnull. */
	bool customData = false;
	/** @brief Whether source element user data was nonnull. */
	bool userData = false;
	/** @brief Whether source text user data was nonnull. */
	bool textUserData = false;
	/** @brief Whether a source transition handler was installed. */
	bool transitionHandler = false;
	/** @brief Whether source transition initial-state storage was nonnull. */
	bool transitionInitialState = false;
	/** @brief Whether source transition final-state storage was nonnull. */
	bool transitionFinalState = false;
};

/** @brief Bit flags describing copied Clay nodes and their ownership. */
enum class DevClayNodeFlag : uint32_t {
	/** @brief No special observations. */
	None = 0,
	/** @brief Node contains rendered text. */
	Text = 1u << 0u,
	/** @brief Node begins a floating attachment subtree. */
	Floating = 1u << 1u,
	/** @brief Node is retained by Clay during an exit transition. */
	Exiting = 1u << 2u,
	/** @brief Node was emitted after authored UI construction. */
	SyntheticAfterBuild = 1u << 3u,
	/** @brief Numeric Clay ID is duplicated in the captured forest. */
	DuplicateId = 1u << 4u,
	/** @brief Completed node bounds were unavailable. */
	BoundsUnavailable = 1u << 5u,
	/** @brief No typed Flow invocation owns this raw Clay node. */
	UnownedRawClay = 1u << 6u,
};

/** @brief Combine enabled flags without mutating either input. */
constexpr DevClayNodeFlag operator|(DevClayNodeFlag left, DevClayNodeFlag right) noexcept {
	return static_cast<DevClayNodeFlag>(static_cast<uint32_t>(left) | static_cast<uint32_t>(right));
}
/** @brief Add right-hand flags to left and return left. */
constexpr DevClayNodeFlag& operator|=(DevClayNodeFlag& left, DevClayNodeFlag right) noexcept {
	left = left | right;
	return left;
}
/** @brief Return whether value contains any bit in flag; combined flags use an any-bit test. */
[[nodiscard]] constexpr bool hasFlag(DevClayNodeFlag value, DevClayNodeFlag flag) noexcept {
	return (static_cast<uint32_t>(value) & static_cast<uint32_t>(flag)) != 0;
}

/** @brief Copied preorder Clay layout node; pointer-bearing declaration fields are cleared. */
struct DevClayNode {
	/** @brief Parent Clay node, or InvalidClayNode for an attachment root. */
	DevClayNodeIndex parent = InvalidClayNode;
	/** @brief First direct Clay child, or InvalidClayNode. */
	DevClayNodeIndex firstChild = InvalidClayNode;
	/** @brief Next sibling, or InvalidClayNode. */
	DevClayNodeIndex nextSibling = InvalidClayNode;
	/** @brief Exclusive preorder end of this Clay subtree. */
	uint32_t subtreeEnd = 0;
	/** @brief Depth relative to this attachment root. */
	uint32_t depthWithinRoot = 0;
	/** @brief Index of this node's root in DevClayForest::roots. */
	uint32_t rootIndex = 0;
	/** @brief Original Clay layout array position for correlation. */
	uint32_t layoutElementIndex = 0;
	/** @brief Numeric Clay element identity. */
	uint32_t clayId = 0;
	/** @brief Copied diagnostic Clay ID text. */
	DevTreeStringRef idString{};
	/** @brief Completed layout bounds in UI coordinates. */
	Clay_BoundingBox bounds{};
	/** @brief Completed layout size in UI coordinates. */
	Clay_Dimensions dimensions{};
	/** @brief Copied Clay minimum dimensions. */
	Clay_Dimensions minDimensions{};
	/** @brief Numeric Clay ID of the clipping ancestor. */
	uint32_t clipClayId = 0;
	/** @brief Copied element declaration with pointer and callback fields cleared. */
	Clay_ElementDeclaration declaration{}; // all pointer/function fields are nulled
	/** @brief Copied text configuration with user data cleared. */
	Clay_TextElementConfig textConfig{}; // userData is nulled
	/** @brief Presence of source pointer/callback fields before clearing. */
	DevClayPointerPresence pointerPresence{};
	/** @brief Copied UTF-8 text in the snapshot string pool. */
	DevTreeStringRef text{};
	/** @brief Measured unwrapped text size. */
	Clay_Dimensions unwrappedTextDimensions{};
	/** @brief Observed number of wrapped text lines. */
	uint32_t wrappedLineCount = 0;
	/** @brief Nearest owning Flow invocation, or InvalidFlowNode for raw Clay. */
	DevFlowNodeIndex directFlowOwner = InvalidFlowNode;
	/** @brief Text, attachment, duplicate-ID, and ownership observations. */
	DevClayNodeFlag flags = DevClayNodeFlag::None;
};

/** @brief Attachment, clipping, and paint-order metadata for a copied Clay forest root. */
struct DevClayRoot {
	/** @brief Root node index in DevClayForest::nodes. */
	DevClayNodeIndex node = InvalidClayNode;
	/** @brief Numeric Clay ID of the attachment parent. */
	uint32_t attachmentParentClayId = 0;
	/** @brief Resolved copied attachment parent, or InvalidClayNode. */
	DevClayNodeIndex attachmentParent = InvalidClayNode;
	/** @brief Numeric Clay ID of the clipping ancestor. */
	uint32_t clipClayId = 0;
	/** @brief Resolved copied clipping node, or InvalidClayNode. */
	DevClayNodeIndex clipNode = InvalidClayNode;
	/** @brief Clay stacking layer for this attachment root. */
	int16_t zIndex = 0;
	/** @brief Observed root paint order used during hit testing. */
	uint32_t paintOrder = 0;
};

/** @brief Owned copied Clay attachment forest in preorder. */
struct DevClayForest {
	/** @brief Contiguous copied layout nodes. */
	std::vector<DevClayNode> nodes{};
	/** @brief Contiguous attachment-root metadata in paint order. */
	std::vector<DevClayRoot> roots{};
};
#endif

/** @brief Capture capacity, balance, and Flow-to-Clay ownership failures. */
enum class DevTreeDiagnosticCode : uint16_t {
	/** @brief Flow begin/end capture scopes are unbalanced. */
	FlowCaptureUnbalanced,
	/** @brief Flow node reserve/capacity policy prevented capture. */
	FlowNodeCapacityExceeded,
	/** @brief Copied string capacity was exhausted. */
	StringCapacityExceeded,
#if FLOW_UI_DEV_CAPTURE_CLAY
	/** @brief Flow invocation has no matching Clay root. */
	FlowElementMissingClayRoot,
	/** @brief Flow invocation root ID matches multiple Clay nodes. */
	FlowElementDuplicateClayRoot,
	/** @brief Flow and Clay root diagnostic names disagree. */
	FlowClayDebugNameMismatch,
	/** @brief Invocation emitted Clay nodes outside its expected subtree. */
	FlowElementEmittedClayOutsideRoot,
	/** @brief Child Flow root has an incompatible Clay parent. */
	FlowChildClayParentMismatch,
	/** @brief Distinct Flow identities collide in the Clay ID bridge. */
	ClayBridgeIdCollision,
	/** @brief Clay attachment parent was not captured/resolved. */
	ClayAttachmentParentMissing,
	/** @brief Clay clipping ancestor was not captured/resolved. */
	ClayClipNodeMissing,
	/** @brief Clay bridge traversal failed before a full copy. */
	ClayBridgeTraversalFailed,
	/** @brief Clay node capacity prevented a complete capture. */
	ClayNodeCapacityExceeded,
#endif
};

/** @brief Copied tree-capture validation failure with optional owner and numeric evidence. */
struct DevTreeDiagnostic {
	/** @brief Machine-readable capture/ownership failure. */
	DevTreeDiagnosticCode code{};
	/** @brief Associated Flow node, or InvalidFlowNode. */
	DevFlowNodeIndex flow = InvalidFlowNode;
#if FLOW_UI_DEV_CAPTURE_CLAY
	/** @brief Associated Clay node, or InvalidClayNode. */
	DevClayNodeIndex clay = InvalidClayNode;
#endif
	/** @brief Expected identity/count/relationship encoded for the diagnostic. */
	uint64_t expected = 0;
	/** @brief Observed identity/count/relationship encoded for the diagnostic. */
	uint64_t observed = 0;
};

/** @brief Copied node totals and memory accounting for one attempted capture. */
struct DevTreeCaptureStats {
	/** @brief Window frame represented by this capture attempt. */
	uint64_t frameNumber = 0;
	/** @brief Number of copied Flow invocation nodes. */
	uint32_t flowNodeCount = 0;
#if FLOW_UI_DEV_CAPTURE_CLAY
	/** @brief Number of copied Clay layout nodes. */
	uint32_t clayNodeCount = 0;
	/** @brief Number of copied Clay attachment roots. */
	uint32_t clayRootCount = 0;
	/** @brief Number of packed direct Flow-to-Clay ownership links. */
	uint32_t directLinkCount = 0;
	/** @brief Number of Clay nodes without a Flow owner. */
	uint32_t rawClayNodeCount = 0;
	/** @brief Number of Clay nodes emitted after authored UI construction. */
	uint32_t syntheticClayNodeCount = 0;
#endif
	/** @brief Number of Flow nodes with failed ownership/root validation. */
	uint32_t invalidFlowNodeCount = 0;
	/** @brief Bytes used by copied names, paths, and text. */
	uint32_t copiedStringBytes = 0;
	/** @brief Observed backing-vector growth events during capture. */
	uint32_t vectorGrowthOperations = 0;
	/** @brief Bytes logically occupied by retained capture data. */
	uint64_t logicalLiveBytes = 0;
	/** @brief Bytes reserved by capture backing containers. */
	uint64_t backingCapacityBytes = 0;
	/** @brief Maximum logical capture bytes observed by this owner. */
	uint64_t peakLogicalLiveBytes = 0;
	/** @brief Maximum reserved capture bytes observed by this owner. */
	uint64_t peakBackingCapacityBytes = 0;
	/** @brief Whether capture completed without missing/truncated data. */
	bool complete = false;
	/** @brief Whether a configured capacity prevented a full copy. */
	bool truncated = false;
};

/** @brief Owned completed window tree; all node indices and string references are snapshot-local. */
struct DevTreeSnapshot {
	/** @brief Application window represented by the copied tree. */
	WindowId window = InvalidWindowId;
	/** @brief Capture frame supplied by the owning window. */
	uint64_t frameNumber = 0;
	/** @var uint64_t app_tick
	 * @brief App-global tick owning this completed capture.
	 */
	/** @var uint64_t ui_runtime_frame
	 * @brief Authoritative runtime window-frame identity associated with this capture.
	 */
	uint64_t app_tick = 0, ui_runtime_frame = 0;
	/** @brief Monotonic publication identity. */
	uint64_t generation = 0;
#if FLOW_UI_DEV_CAPTURE_CLAY
	/** @brief Clay layout element count before synthetic development overlays. */
	uint32_t authoredClayElementCount = 0;
#endif
	/** @brief Copied Flow invocation forest. */
	DevFlowForest flow{};
#if FLOW_UI_DEV_CAPTURE_CLAY
	/** @brief Copied Clay attachment forest when capture support is enabled. */
	DevClayForest clay{};
#endif
	/** @brief Owned names, source metadata, and text used by string references. */
	std::vector<char> strings{};
	/** @brief Owned validation/capacity diagnostics. */
	std::vector<DevTreeDiagnostic> diagnostics{};
	/** @brief Node counts, completeness, and retained-memory accounting. */
	DevTreeCaptureStats stats{};

	/** @brief Borrow copied UTF-8 data from this snapshot; return empty for an invalid byte range. */
	[[nodiscard]] std::string_view string(DevTreeStringRef ref) const noexcept {
		if (ref.offset > strings.size() || ref.length > strings.size() - ref.offset) return {};
		return {strings.data() + ref.offset, ref.length};
	}
};

#if FLOW_UI_DEV_CAPTURE_CLAY
/** @brief Borrow the entire contiguous Clay subtree owned by a Flow root; return empty for invalid/unresolved indices. The span expires with snapshot mutation/destruction. */
[[nodiscard]] inline std::span<const DevClayNode> fullClaySubtree(
	const DevTreeSnapshot& snapshot, DevFlowNodeIndex flow) noexcept {
	if (flow >= snapshot.flow.nodes.size()) return {};
	const DevClayNodeIndex root = snapshot.flow.nodes[flow].clayRoot;
	if (root >= snapshot.clay.nodes.size()) return {};
	const uint32_t end = snapshot.clay.nodes[root].subtreeEnd;
	if (end < root || end > snapshot.clay.nodes.size()) return {};
	return std::span<const DevClayNode>(snapshot.clay.nodes).subspan(root, end - root);
}

/** @brief Borrow packed Clay node indices directly owned by the selected Flow invocation, excluding nested Flow contributions; invalid ranges return an empty span. */
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
