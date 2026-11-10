#include "FlowUi/Private/layoutService/UiManagerState.hpp"
#include "FlowUi/Private/storageSystem/capacity/InitialMemoryCapacity.hpp"
#include "FlowUi/Public/errorManagement/Exception.hpp"
#include <algorithm>

namespace {
constexpr float kPointsPerInch = 72.0f;
}

namespace FlowUi {
namespace manager_storage = detail::manager_storage;
namespace storage = detail::storage;

	manager_storage::UiManagerState::UiManagerState(storage::IStorageSystem& storageSystem,
													WindowId window, const AppConfig& appConfig)
		: storage(&storageSystem)
#if FLOW_UI_DEV_MODE
		  ,
		  devTreeCapture(appConfig.dev.tooling.treeCapture,
						 [&] {
							 FlowUi::detail::InitialMemoryCapacities capacities;
							 for (const auto& setting : appConfig.memoryCapacityProfile.settings)
								 if (const auto* definition =
										 FlowUi::detail::find_initial_capacity(setting.target))
									 capacities.values[static_cast<size_t>(definition->target)] =
										 setting.value;
							 return capacities;
						 }())
#endif
	{
		const size_t minimumClayArenaCapacityBytes = static_cast<size_t>(Clay_MinMemorySize());
		const size_t configuredClayArenaCapacityBytes = appConfig.ui.clayArenaCapacityBytes;
		const size_t clayArenaCapacityBytes = (configuredClayArenaCapacityBytes == 0)
			? minimumClayArenaCapacityBytes
			: std::max(configuredClayArenaCapacityBytes, minimumClayArenaCapacityBytes);

		const storage::StringId name = storageSystem.intern("flowui.ui.clay");
		clayMemory = storageSystem.allocatePersistent(
			clayArenaCapacityBytes,
			alignof(std::max_align_t),
			storage::AllocationTag{
				.memoryClass = storage::MemoryClass::WindowPersistent,
				.resourceKind = storage::ResourceKind::UiContext,
				.window = window,
				.frameSlot = storage::InvalidFrameSlot,
				.debugName = name,
			});
		clayArena = Clay_CreateArenaWithCapacityAndMemory(clayArenaCapacityBytes, clayMemory.data);

		const float initialScreenWidth = static_cast<float>(std::max(1, appConfig.window.width));
		const float initialScreenHeight = static_cast<float>(std::max(1, appConfig.window.height));
		const Clay_Dimensions initialLayoutDimensions{initialScreenWidth, initialScreenHeight};

		clayContext = Clay_Initialize(clayArena, initialLayoutDimensions, Clay_ErrorHandler{});
		if (!clayContext) {
			storageSystem.releasePersistent(clayMemory);
			clayMemory = {};
			throw FlowUiException(makeError(ErrorCode::RendererConfigurationInvalid, ErrorSite::UiManagerInitialize));
		}

		const float configuredDpi = std::max(1.0f, appConfig.ui.dpi);
		pointsToPixelsScale = std::max(0.0f, appConfig.ui.fontScale) * (configuredDpi / kPointsPerInch);
		if (pointsToPixelsScale <= 0.0f) {
			pointsToPixelsScale = configuredDpi / kPointsPerInch;
		}
		inputManagerConfig = appConfig.ui.inputManager;
#if FLOW_UI_DEV_MODE && FLOWUI_DEV_MEMORY_LEVEL >= 2
		capacity_demand.bind(storageSystem.memory_demand_control());
		flowScopes.bind_memory_demand(storageSystem.memory_demand_control());
		textLayoutService.bind_memory_demand(storageSystem.memory_demand_control());
		devTreeCapture.bind_memory_demand(storageSystem.memory_demand_control());
#endif
		flowScopes.reserve(storageSystem.initial_memory_capacity(
			FlowUi::detail::InitialCapacityTarget::ui_flow_scope_depth));
		detail::reserve_initial_capacity(
			currentInteractionSnapshot.hoveredElementIds,
			storageSystem.initial_memory_capacity(
				FlowUi::detail::InitialCapacityTarget::ui_hovered_element_ids));
		detail::reserve_initial_capacity(
			currentInteractionSnapshot.pressedElementIds,
			storageSystem.initial_memory_capacity(
				FlowUi::detail::InitialCapacityTarget::ui_pressed_element_ids));
		detail::reserve_initial_capacity(
			currentInteractionSnapshot.heldElementIds,
			storageSystem.initial_memory_capacity(
				FlowUi::detail::InitialCapacityTarget::ui_held_element_ids));
		detail::reserve_initial_capacity(
			currentInteractionSnapshot.releasedElementIds,
			storageSystem.initial_memory_capacity(
				FlowUi::detail::InitialCapacityTarget::ui_released_element_ids));
		detail::reserve_initial_capacity(
			previousInteractionSnapshot.hoveredElementIds,
			storageSystem.initial_memory_capacity(
				FlowUi::detail::InitialCapacityTarget::ui_hovered_element_ids));
		detail::reserve_initial_capacity(
			previousInteractionSnapshot.pressedElementIds,
			storageSystem.initial_memory_capacity(
				FlowUi::detail::InitialCapacityTarget::ui_pressed_element_ids));
		detail::reserve_initial_capacity(
			previousInteractionSnapshot.heldElementIds,
			storageSystem.initial_memory_capacity(
				FlowUi::detail::InitialCapacityTarget::ui_held_element_ids));
		detail::reserve_initial_capacity(
			previousInteractionSnapshot.releasedElementIds,
			storageSystem.initial_memory_capacity(
				FlowUi::detail::InitialCapacityTarget::ui_released_element_ids));
		constructedElementStack.reserve(storageSystem.initial_memory_capacity(
			FlowUi::detail::InitialCapacityTarget::ui_constructed_depth));
		textLayoutService.reserve_initial_entries(storageSystem.initial_memory_capacity(
			FlowUi::detail::InitialCapacityTarget::ui_text_cache));
#if FLOW_UI_DEV_MODE
		devToolsConfig = appConfig.dev;
#endif
	}

	manager_storage::UiManagerState::~UiManagerState() noexcept {
		if (Clay_GetCurrentContext() == clayContext)
			Clay_SetCurrentContext(nullptr);
		if (storage && clayMemory) storage->releasePersistent(clayMemory);
	}


} // namespace FlowUi
