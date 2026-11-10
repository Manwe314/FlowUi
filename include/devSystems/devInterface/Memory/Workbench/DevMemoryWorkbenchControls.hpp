#pragma once
#include "FSEL/internal/SelectableSurfaceBehavior.hpp"
#include "devSystems/devInterface/Memory/Workbench/DevMemoryWorkbenchData.hpp"
#include "devSystems/devInterface/Permanents/Backend/DevInterfaceIcons.hpp"
#if FLOW_UI_DEV_MODE
namespace FlowUi::devSystems::interface_elements {
/* Cached conventional action artwork for memory transport and authoring controls. */
struct DevMemoryControlResources {
	static constexpr auto keys = std::to_array<std::string_view>(
		{interface_icons::kFollowKey,	interface_icons::kFitKey,
		 interface_icons::kRecordKey,	interface_icons::kStopKey,
		 interface_icons::kCaptureKey,	interface_icons::kSaveKey,
		 interface_icons::kRemoveKey,	interface_icons::kCloseKey,
		 interface_icons::kSettingsKey, interface_icons::kEditKey,
		 interface_icons::kAddKey,		interface_icons::kCheckKey,
		 interface_icons::kCopyKey,		interface_icons::kMoveUpKey,
		 interface_icons::kUndoKey,		interface_icons::kPreviousKey,
		 interface_icons::kLockKey,		interface_icons::kUnlockKey,
		 interface_icons::kRevertKey,	interface_icons::kExpandKey,
		 interface_icons::kCollapseKey});
	std::array<TextureRef, keys.size()> textures{};
	explicit DevMemoryControlResources(App& app);
	/* Resolve a registered action without exposing the backing icon manager. */
	[[nodiscard]] TextureRef texture(std::string_view key) const noexcept {
		for (size_t key_index = 0; key_index < keys.size(); ++key_index)
			if (keys[key_index] == key)
				return textures[key_index];
		return {};
	}
};
/* Retained release-inside control for Memory transport and track focus. */
struct DevMemoryControl {
	struct Parameters {
		Clay_ElementDeclaration surface{};
		DevMemoryWorkbenchState* state = nullptr;
		std::string label{};
		uint32_t action = 0;
		uint64_t identity = 0;
		DevMemorySelection* selection = nullptr;
		bool whole_track = false, enabled = true;
	};
	using State = FSEL::detail::selectable_surface::State;
	using Resources = DevMemoryControlResources;
	using BuildContext = ElementBuildContext<DevMemoryControl>;
	using InteractionContext = ElementInteractionContext<DevMemoryControl>;
	static constexpr FlowDefinitionID definitionId =
		DefinitionID("flowui.dev_interface.memory.workbench.control");
	static constexpr std::string_view debugName = "Memory Workbench Control";
	static constexpr bool isDevInternal = true;
	static void onPressed(InteractionContext& context);
	static void onReleased(InteractionContext& context);
	static void onHovered(InteractionContext& context);
	static void runLogic(InteractionContext& context);
	/* Open a selectable track surface around caller-provided content. */
	[[nodiscard]] static Clay_ElementDeclaration constructElement(BuildContext& context);
	static void buildElement(BuildContext& context);
};
inline constexpr DevMemoryControl kDevMemoryControl{};
} // namespace FlowUi::devSystems::interface_elements
#endif
