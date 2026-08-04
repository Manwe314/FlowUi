#pragma once

#include <cstdint>
#include <vector>

#include <clay.h>

#include "FlowUi/BuildConfig.hpp"
#include "FlowUi/PublicStructs.hpp"
#include "internal/StorageSystem/IStorageSystem.hpp"
#include "internal/ManagerStorage/FontCatalogController.hpp"
#if FLOW_UI_DEV_MODE
#include "devMode/devRuntime.hpp"
#include "devMode/performanceDiagnostics.hpp"
#endif

namespace FlowUi {
namespace detail::manager_storage {

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
