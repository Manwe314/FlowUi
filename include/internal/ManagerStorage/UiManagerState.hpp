#pragma once

#include "FlowUi/BuildConfig.hpp"

#include <cstdint>
#include <vector>
#if FLOW_UI_DEV_MODE
#include <string>
#include <string_view>
#include <unordered_map>
#endif

#include <clay.h>

#include "FlowUi/PublicStructs.hpp"
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
	uint64_t definitionId = 0;
	std::string logicalId{};
	std::string fileName{};
	std::string functionName{};
	uint32_t line = 0;
	uint32_t column = 0;
};

struct FlowRootCollisionForDev {
	uint64_t flowId = 0;
	FlowRootClaimSourceForDev first{};
	FlowRootClaimSourceForDev duplicate{};
};

class FlowRootIdTrackerForDev {
public:
	void beginFrame();
	void discardFrame() noexcept;
	[[nodiscard]] const FlowRootCollisionForDev* claim(
		uint64_t flowId,
		FlowRootClaimSourceForDev source);
	[[nodiscard]] size_t collisionCount() const noexcept { return collisions_.size(); }
	[[nodiscard]] const FlowRootCollisionForDev& collision(size_t index) const;

private:
	std::unordered_map<uint64_t, FlowRootClaimSourceForDev> claims_{};
	std::vector<FlowRootCollisionForDev> collisions_{};
};
#endif

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
	std::vector<Clay_ElementId> constructedElementStack{};
#if FLOW_UI_DEV_MODE
	FlowRootIdTrackerForDev flowRootIdTracker{};
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
