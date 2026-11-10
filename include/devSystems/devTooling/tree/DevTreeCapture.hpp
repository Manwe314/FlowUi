#pragma once

#include "FlowUi/Public/BuildConfig.hpp"

#if FLOW_UI_DEV_MODE

#include "FlowUi/Private/storageSystem/capacity/MemoryDemandPeaks.hpp"
#include <cstdint>
#include <string_view>
#include <vector>

#include "FlowUi/Public/identificationTypes/ElementID.hpp"
#include "FlowUi/Public/identificationTypes/WindowId.hpp"
#include "clay.h"
#include "devSystems/DevConfig.hpp"
#include "devSystems/devTooling/tree/DevTreeTypes.hpp"
#if FLOW_UI_DEV_CAPTURE_CLAY
#include "FlowUi/Private/layoutService/ClayDevTreeBridge.hpp"
#endif

namespace FlowUi::devSystems {
class DevTimingRecorder;
class MemorySampleSink;
}

namespace FlowUi::devSystems::tooling {

/** @brief Window-frame tree copier with separate building and published buffers. Borrow current() only until the next successful publication or owner destruction. */
class DevTreeCapture {
public:

	/** @brief Borrowed invocation metadata copied by beginFlow() during an active capture. */
	struct FlowBegin {
		/** @brief Stable Flow definition identity. */
		FlowDefinitionID definition{};
		/** @brief Resolved Flow instance identity. */
		FlowElementID instance{};
		/** @brief Definition display name copied into the capture string pool. */
		std::string_view definitionName{};
		/** @brief Complete definition C++ type token copied into the string pool. */
		std::string_view definitionTypeToken{};
		/** @brief Invocation source path copied during beginFlow(). */
		std::string_view sourceFile{};
		/** @brief Invocation function name copied during beginFlow(). */
		std::string_view sourceFunction{};
		/** @brief Invocation source line. */
		uint32_t sourceLine = 0;
		/** @brief Invocation source column. */
		uint32_t sourceColumn = 0;
		/** @brief Whether the invocation opens a constructed root rather than drawing immediately. */
		bool constructed = false;
		/** @brief Whether this invocation belongs to protected developer UI. */
		bool internalDev = false;
		/** @brief Whether this invocation should be omitted from captured nodes. */
		bool suppress = false;
	};

	/** @brief Frame-qualified scope token returned by beginFlow(); valid only for its matching endFlow(). */
	struct Token {
		/** @brief Capture generation that issued this token. */
		uint64_t frameGeneration = 0;
		/** @brief Copied Flow node index, or InvalidFlowNode for a suppressed scope. */
		DevFlowNodeIndex node = InvalidFlowNode;
		/** @brief Nonzero balanced-scope identity; zero denotes an inactive token. */
		uint32_t scopeId = 0;
		/** @brief Return whether this token identifies a nonzero balanced capture scope. */
		[[nodiscard]] explicit operator bool() const noexcept { return scopeId != 0; }
	};

	/** @brief Copy capture limits and initial reserve capacities; supplied configuration values are not retained by reference. */
	explicit DevTreeCapture(DevTreeCaptureConfig config = {},
							const FlowUi::detail::InitialMemoryCapacities& capacities = {});

	/** Bind the owning application monitoring gate before capture. */
	void bind_memory_demand(const ::FlowUi::detail::MemoryDemandControl* control) noexcept {
		capacity_demand_.bind(control);
	}
	/** @brief Begin a capture for window/frame and borrow Clay context and optional timing until finish/cancel; app_tick and ui_runtime_frame provide source evidence identity. */
	void beginFrame(WindowId window, uint64_t frameNumber, Clay_Context& clay,
					DevTimingRecorder* timing, uint64_t app_tick = 0,
					uint64_t ui_runtime_frame = 0) noexcept;
	/** @brief Copy invocation metadata and open a balanced capture scope; pass the returned frame-qualified token to endFlow(). */
	[[nodiscard]] Token beginFlow(const FlowBegin& begin) noexcept;
	/** @brief Close the matching token's scope; autoClosed records that lifecycle recovery closed an unbalanced constructed element. */
	void endFlow(Token token, bool autoClosed = false) noexcept;
#if FLOW_UI_DEV_CAPTURE_CLAY
	/** @brief Mark the boundary between application-authored Clay nodes and synthetic development emission. */
	void noteAuthoredClayEnd() noexcept;
#endif
	/** @brief Copy completed layout, validate ownership, and publish the frame; inspect lastFinishFailed() for a failed observation. */
	void finishAfterLayout() noexcept;
	/** @brief Discard in-progress capture without replacing the latest successful publication. */
	void cancelFrame() noexcept;

	/** @brief Borrow the latest successful snapshot until the next successful publication or destruction. */
	[[nodiscard]] const DevTreeSnapshot& current() const noexcept { return published_; }
	/** @brief Return whether a capture frame is currently open. */
	[[nodiscard]] bool frameActive() const noexcept { return frameActive_; }
	/** @brief Return whether the latest completion attempt failed to publish. */
	[[nodiscard]] bool lastFinishFailed() const noexcept { return lastFinishFailed_; }
	/** @brief Append retained capture memory observations to a borrowed sink without consuming capture data. */
	void appendDevMemorySamples(MemorySampleSink& sink) const noexcept;

private:
	struct ActiveFlowCapture { Token token{}; bool suppressed = false; };

	[[nodiscard]] DevTreeStringRef copyString(std::string_view value);
	void addDiagnostic(DevTreeDiagnostic diagnostic) noexcept;
	void finalizeFlowStats() noexcept;
	void publish() noexcept;
	void clearSnapshot(DevTreeSnapshot& snapshot) noexcept;
	void updateMemoryStats(DevTreeSnapshot& snapshot) noexcept;

#if FLOW_UI_DEV_CAPTURE_CLAY
	struct ClayIdIndexEntry { uint32_t clayId = 0; DevClayNodeIndex node = InvalidClayNode; };
	enum class OwnershipEventKind : uint8_t { End, Begin };
	struct OwnershipEvent {
		uint32_t ordinal = 0;
		DevFlowNodeIndex flow = InvalidFlowNode;
		uint32_t depth = 0;
		OwnershipEventKind kind{};
	};
	struct CopyContext { DevTreeCapture* self = nullptr; bool failed = false; };

	static bool copyRootCallback(void* userData, const ::FlowUi::detail::ClayDevRootView& root) noexcept;
	static bool copyElementCallback(void* userData, const ::FlowUi::detail::ClayDevElementView& element) noexcept;
	bool copyRoot(const ::FlowUi::detail::ClayDevRootView& root);
	bool copyElement(const ::FlowUi::detail::ClayDevElementView& element);
	bool copyClayForest() noexcept;
	void finishClaySubtrees();
	void resolveClayLinksAndDuplicates();
	void assignFlowOwnership();
	void packDirectClayLinks();
	void resolveAndValidateFlowRoots();
	[[nodiscard]] DevClayNodeIndex uniqueClayNode(uint32_t id) const noexcept;
	[[nodiscard]] bool isClayRoot(DevClayNodeIndex node) const noexcept;

	std::vector<::FlowUi::detail::ClayDevTraversalEntry> clayTraversalScratch_{};
	std::vector<DevClayNodeIndex> layoutIndexToClayNode_{};
	std::vector<ClayIdIndexEntry> clayIdIndex_{};
	std::vector<DevClayNodeIndex> lastClayChild_{};
	std::vector<DevClayNodeIndex> clayOpenByDepth_{};
	std::vector<OwnershipEvent> ownershipEvents_{};
	std::vector<DevFlowNodeIndex> ownershipActive_{};
	std::vector<uint32_t> directClayCounts_{};
	std::vector<uint32_t> directClayCursors_{};
	std::vector<DevFlowNodeIndex> clayRootOwners_{};
	uint32_t currentClayRoot_ = 0;
#endif

	::FlowUi::detail::MemoryDemandPeaks capacity_demand_{};
	DevTreeCaptureConfig config_{};
	DevTreeSnapshot building_{};
	DevTreeSnapshot published_{};
	std::vector<ActiveFlowCapture> activeFlowScopes_{};
	std::vector<DevFlowNodeIndex> lastFlowChild_{};
	Clay_Context* clay_ = nullptr;
	DevTimingRecorder* timing_ = nullptr;
	uint64_t frameGeneration_ = 0;
	uint64_t publishedGeneration_ = 0;
	uint64_t peakLogicalLiveBytes_ = 0;
	uint64_t peakBackingCapacityBytes_ = 0;
	uint32_t nextScopeId_ = 1;
	bool frameActive_ = false;
	bool lastFinishFailed_ = false;
};

} // namespace FlowUi::devSystems::tooling

#endif
