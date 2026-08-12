#pragma once

#include "FlowUi/BuildConfig.hpp"

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

#include "FlowUi/ElementID.hpp"
#include "FlowUi/PublicStructs.hpp"
#include "internal/FlowScopeStack.hpp"
#include "internal/ElementInstanceKey.hpp"
#include "internal/StorageSystem/IStorageSystem.hpp"
#include "internal/ManagerStorage/FontCatalogController.hpp"
#if FLOW_UI_DEV_MODE
#include "devMode/devRuntime.hpp"
#include "devMode/performanceDiagnostics.hpp"
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

/** Frame-local detector for distinct 64-bit Flow IDs folded to one Clay ID. */
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
};

struct UiManagerState {
	explicit UiManagerState(storage::IStorageSystem& storageSystem, WindowId window, const AppConfig& config);
	~UiManagerState() noexcept;

	storage::IStorageSystem* storage = nullptr;
	storage::MemoryBlock clayMemory{};
	Clay_Arena clayArena{};
	Clay_Context* clayContext = nullptr;
	storage::FrameToken activeFrame{};
	storage::ArenaView frameArena{};
	FrameInput frameInputForCurrentLayout{};
	FrameInput previousFrameInputForCurrentLayout{};
	bool wasPrimaryPointerDownLastFrame = false;
	InteractionSnapshot previousInteractionSnapshot{};
	InteractionSnapshot currentInteractionSnapshot{};
	FlowScopeStack flowScopes{};
	std::vector<ConstructedElementFrame> constructedElementStack{};
#if FLOW_UI_DEV_MODE
	FlowRootIdTrackerForDev flowRootIdTracker{};
	ClayBridgeIdTrackerForDev clayBridgeIdTracker{};
	devMode::DevRuntime devRuntime{};
	devMode::PerformanceDiagnostics performanceDiagnostics{};
	DevToolsConfig devToolsConfig{};
	bool devPanelVisible = false;
	bool devRootElementOpenThisFrame = false;
	ShortcutId devPanelToggleShortcutId = 0;
#endif
	CursorType cursor = CursorType::Arrow;
	CursorType previousCursor = CursorType::Arrow;
	uint8_t cursorPriority = 0;
	FontFrameView fontView{};
	float pointsToPixelsScale = 96.0f / 72.0f;
	InputManagerConfig inputManagerConfig{};
};

} // namespace detail::manager_storage
} // namespace FlowUi
