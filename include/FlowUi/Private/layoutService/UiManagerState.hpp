#pragma once

#include "FlowUi/Public/BuildConfig.hpp"

#include <cstdint>
#include <vector>
#if FLOW_UI_DEV_MODE
#include <stdexcept>
#include <string>
#include <string_view>
#include <unordered_map>
#include <unordered_set>
#include <utility>
#endif

#include <clay.h>

#include "FlowUi/Public/identificationTypes/ElementID.hpp"
#include "FlowUi/Public/configurationStructs/AppConfig.hpp"
#include "FlowUi/Public/configurationStructs/DevToolsConfig.hpp"
#include "FlowUi/Public/configurationStructs/InputConfig.hpp"
#include "FlowUi/Public/identificationTypes/WindowId.hpp"
#include "FlowUi/Private/elementSystem/ElementInstanceKey.hpp"
#include "FlowUi/Private/layoutService/FlowScopeStack.hpp"
#include "FlowUi/Private/resourceService/FontCatalogController.hpp"
#include "FlowUi/Private/storageSystem/IStorageSystem.hpp"
#include "FlowUi/Private/textLayoutService/TextLayoutService.hpp"
#include "FlowUi/Public/elementAuthoring/ElementContexts.hpp"
#include "FlowUi/Public/managerStructs/InputTypes.hpp"
#if FLOW_UI_DEV_MODE
#include "devSystems/devTooling/tree/DevTreeCapture.hpp"
#if FLOWUI_DEV_TIMING_LEVEL >= 2
#include "devSystems/devMonitoringAndReporting/timing/DevTimingZone.hpp"
#endif
#endif

namespace FlowUi {
namespace detail::manager_storage {

#if FLOW_UI_DEV_MODE
struct FlowRootClaimSourceForDev {
	FlowDefinitionID definitionId{};
	std::string debugPath{};
	std::string fileName{};
	std::string functionName{};
	uint32_t line = 0;
	uint32_t column = 0;
	bool automaticIdentity = false;
};

struct FlowRootCollisionForDev {
	element::ElementInstanceKey instanceId{};
	FlowRootClaimSourceForDev first{};
	FlowRootClaimSourceForDev duplicate{};
};

class FlowRootIdTrackerForDev {
public:
	void beginFrame() {
		claims_.clear();
		collisions_.clear();
	}
	void discardFrame() noexcept {
		claims_.clear();
		collisions_.clear();
	}
	[[nodiscard]] const FlowRootCollisionForDev* claim(
		element::ElementInstanceKey instanceId,
		FlowRootClaimSourceForDev source) {
		auto [entry, inserted] = claims_.try_emplace(instanceId, std::move(source));
		if (inserted) return nullptr;
		collisions_.push_back(FlowRootCollisionForDev{
			.instanceId = instanceId,
			.first = entry->second,
			.duplicate = std::move(source),
		});
		return &collisions_.back();
	}
	[[nodiscard]] size_t collisionCount() const noexcept { return collisions_.size(); }
	[[nodiscard]] size_t retainedBytesForDev() const noexcept {
		return claims_.size() * sizeof(decltype(claims_)::value_type) +
			claims_.bucket_count() * sizeof(void*) +
			collisions_.capacity() * sizeof(FlowRootCollisionForDev);
	}
	[[nodiscard]] const FlowRootCollisionForDev& collision(size_t index) const {
		if (index >= collisions_.size()) {
			throw std::out_of_range("FlowUi dev Flow-root collision index is out of range.");
		}
		return collisions_[index];
	}

private:
	std::unordered_map<
		element::ElementInstanceKey,
		FlowRootClaimSourceForDev,
		element::ElementInstanceKeyHash> claims_{};
	std::vector<FlowRootCollisionForDev> collisions_{};
};

struct ClayBridgeClaimForDev {
	element::ElementInstanceKey instanceId{};
	FlowRootClaimSourceForDev source{};
};

struct ClayBridgeCollisionForDev {
	uint32_t clayId = 0;
	ClayBridgeClaimForDev first{};
	ClayBridgeClaimForDev duplicate{};
};

/* Frame-local detector for distinct 64-bit Flow IDs folded to one Clay ID. */
class ClayBridgeIdTrackerForDev {
public:
	void beginFrame() {
		claims_.clear();
		claimedInstanceIds_.clear();
		collisions_.clear();
	}

	void discardFrame() noexcept {
		claims_.clear();
		claimedInstanceIds_.clear();
		collisions_.clear();
	}

	[[nodiscard]] const ClayBridgeCollisionForDev* claim(
		element::ElementInstanceKey instanceId,
		uint32_t clayId,
		FlowRootClaimSourceForDev source) {
		if (!claimedInstanceIds_.insert(instanceId).second) return nullptr;
		const auto existing = claims_.find(clayId);
		if (existing == claims_.end()) {
			claims_.emplace(
				clayId,
				ClayBridgeClaimForDev{.instanceId = instanceId, .source = std::move(source)});
			return nullptr;
		}
		if (existing->second.instanceId == instanceId) return nullptr;
		collisions_.push_back(ClayBridgeCollisionForDev{
			.clayId = clayId,
			.first = existing->second,
			.duplicate = ClayBridgeClaimForDev{
				.instanceId = instanceId,
				.source = std::move(source),
			},
		});
		return &collisions_.back();
	}

	[[nodiscard]] size_t collisionCount() const noexcept { return collisions_.size(); }
	[[nodiscard]] size_t retainedBytesForDev() const noexcept {
		return claims_.size() * sizeof(decltype(claims_)::value_type) +
			claims_.bucket_count() * sizeof(void*) +
			claimedInstanceIds_.size() * sizeof(decltype(claimedInstanceIds_)::value_type) +
			collisions_.capacity() * sizeof(ClayBridgeCollisionForDev);
	}
	[[nodiscard]] const ClayBridgeCollisionForDev& collision(size_t index) const {
		if (index >= collisions_.size()) {
			throw std::out_of_range("FlowUi dev Clay-bridge collision index is out of range.");
		}
		return collisions_[index];
	}

private:
	std::unordered_map<uint32_t, ClayBridgeClaimForDev> claims_{};
	std::unordered_set<
		element::ElementInstanceKey,
		element::ElementInstanceKeyHash> claimedInstanceIds_{};
	std::vector<ClayBridgeCollisionForDev> collisions_{};
};
#endif

struct ConstructedElementFrame {
	Clay_ElementId clayId{};
	FlowElementID flowId{};
	size_t priorFlowScopeDepth = 0;
#if FLOW_UI_DEV_MODE
	devSystems::tooling::DevTreeCapture::Token treeToken{};
	bool isDevInternal = false;
#endif
#if FLOW_UI_DEV_MODE && FLOWUI_DEV_TIMING_LEVEL >= 2
	devSystems::ManualTimingZone subtreeTiming{};
#endif
};

struct UiManagerState {
	~UiManagerState() noexcept;
	FrameInput frameInputForCurrentLayout{};
	FrameInput previousFrameInputForCurrentLayout{};
#if FLOW_UI_DEV_MODE
	devSystems::tooling::DevTreeCapture devTreeCapture{};
	uint64_t dev_ui_frame_number = 0;
#endif
	DevToolsConfig devToolsConfig{};
	text::TextLayoutService textLayoutService{};
#if FLOW_UI_DEV_MODE
	ClayBridgeIdTrackerForDev clayBridgeIdTracker{};
#endif
	FontFrameView fontView{};
	::FlowUi::detail::MemoryDemandPeaks capacity_demand{};
	InteractionSnapshot previousInteractionSnapshot{};
	InteractionSnapshot currentInteractionSnapshot{};
#if FLOW_UI_AGENTIC_WORKFLOW
	Clay_Dimensions agentic_layout_dimensions{};
	std::uint64_t agentic_completed_window_frame = 0, agentic_completed_app_tick = 0;
#endif
#if FLOW_UI_DEV_MODE
	FlowRootIdTrackerForDev flowRootIdTracker{};
#endif
	storage::MemoryBlock clayMemory{};
	storage::FrameToken activeFrame{};
	storage::ArenaView frameArena{};
	Clay_Arena clayArena{};
	FlowScopeStack flowScopes{};
	std::vector<ConstructedElementFrame> constructedElementStack{};
	explicit UiManagerState(storage::IStorageSystem& storageSystem, WindowId window, const AppConfig& config);

	storage::IStorageSystem* storage = nullptr;
	Clay_Context* clayContext = nullptr;
	InputManagerConfig inputManagerConfig{};
	CursorType cursor = CursorType::Arrow;
	CursorType previousCursor = CursorType::Arrow;
	float pointsToPixelsScale = 96.0f / 72.0f;
	bool wasPrimaryPointerDownLastFrame = false;
	uint8_t cursorPriority = 0;
};

} // namespace detail::manager_storage
} // namespace FlowUi
