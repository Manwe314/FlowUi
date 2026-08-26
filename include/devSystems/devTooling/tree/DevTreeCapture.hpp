#pragma once

#include "FlowUi/BuildConfig.hpp"

#if FLOW_UI_DEV_MODE

#include <cstdint>
#include <string_view>
#include <vector>

#include "devSystems/devTooling/tree/DevTreeTypes.hpp"
#if FLOW_UI_DEV_CAPTURE_CLAY
#include "internal/ClayDevTreeBridge.hpp"
#endif

namespace FlowUi::devSystems {
class DevTimingRecorder;
class MemorySampleSink;
}

namespace FlowUi::devSystems::tooling {

struct DevTreeCaptureConfig {
	uint32_t flowNodeReserve = 512;
	uint32_t stringByteReserve = 64u * 1024u;
	uint32_t diagnosticReserve = 64;
	uint32_t maximumFlowNodes = 1u << 20u;
	uint32_t maximumStringBytes = 64u * 1024u * 1024u;
#if FLOW_UI_DEV_CAPTURE_CLAY
	uint32_t clayNodeReserve = 2048;
	uint32_t clayRootReserve = 32;
	uint32_t directLinkReserve = 2048;
	uint32_t maximumClayNodes = 1u << 22u;
#endif
};

class DevTreeCapture {
public:
	struct FlowBegin {
		FlowDefinitionID definition{};
		FlowElementID instance{};
		std::string_view definitionName{};
		std::string_view definitionTypeToken{};
		std::string_view sourceFile{};
		std::string_view sourceFunction{};
		uint32_t sourceLine = 0;
		uint32_t sourceColumn = 0;
		bool constructed = false;
		bool internalDev = false;
		bool suppress = false;
	};

	struct Token {
		DevFlowNodeIndex node = InvalidFlowNode;
		uint64_t frameGeneration = 0;
		uint32_t scopeId = 0;
		[[nodiscard]] explicit operator bool() const noexcept { return scopeId != 0; }
	};

	explicit DevTreeCapture(DevTreeCaptureConfig config = {});

	void beginFrame(
		WindowId window,
		uint64_t frameNumber,
		Clay_Context& clay,
		DevTimingRecorder* timing) noexcept;
	[[nodiscard]] Token beginFlow(const FlowBegin& begin) noexcept;
	void endFlow(Token token, bool autoClosed = false) noexcept;
#if FLOW_UI_DEV_CAPTURE_CLAY
	void noteAuthoredClayEnd() noexcept;
#endif
	void finishAfterLayout() noexcept;
	void cancelFrame() noexcept;

	[[nodiscard]] const DevTreeSnapshot& current() const noexcept { return published_; }
	[[nodiscard]] bool frameActive() const noexcept { return frameActive_; }
	[[nodiscard]] bool lastFinishFailed() const noexcept { return lastFinishFailed_; }
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

	DevTreeCaptureConfig config_{};
	DevTreeSnapshot building_{};
	DevTreeSnapshot published_{};
	std::vector<ActiveFlowCapture> activeFlowScopes_{};
	std::vector<DevFlowNodeIndex> lastFlowChild_{};
	Clay_Context* clay_ = nullptr;
	DevTimingRecorder* timing_ = nullptr;
	uint64_t frameGeneration_ = 0;
	uint64_t publishedGeneration_ = 0;
	uint32_t nextScopeId_ = 1;
	uint64_t peakLogicalLiveBytes_ = 0;
	uint64_t peakBackingCapacityBytes_ = 0;
	bool frameActive_ = false;
	bool lastFinishFailed_ = false;
};

} // namespace FlowUi::devSystems::tooling

#endif
