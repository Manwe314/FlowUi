#pragma once

#include "FlowUi/BuildConfig.hpp"

#if FLOW_UI_DEV_MODE

#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

#include "FlowUi/ElementID.hpp"
#include "FlowUi/WindowId.hpp"
#include "devSystems/devTooling/override/DevOverrideTypes.hpp"

namespace FlowUi::devSystems {

enum class DevInterfaceTab : uint8_t {
	Inspect,
	Performance,
	Memory,
	Diagnostics,
	Changes,
	Catalogue,
};

enum class DevApplicationState : uint8_t {
	Running,
	Paused,
	FrameStepping,
};

struct DevInterfaceEditorClipboard {
	devMode::DevTypeId type = 0u;
	std::string typeName{};
	tooling::DevOwnedValue value{};

	[[nodiscard]] explicit operator bool() const noexcept {
		return type != 0u && static_cast<bool>(value);
	}
};

struct DevInterfaceEditTransaction {
	std::string description{};
	std::vector<tooling::DevOverrideCommand> forward{};
	std::vector<tooling::DevOverrideCommand> inverse{};
	DevInterfaceEditorClipboard clipboardBefore{};
	DevInterfaceEditorClipboard clipboardAfter{};
	bool changesClipboard = false;
};

/** Persistent orchestration state owned by the singleton interface element. */
struct DevInterfaceState {
	DevInterfaceEditorClipboard editorClipboard{};
	std::string searchQuery{};
	std::string lastActionMessage = "Developer interface initialized";
	FlowElementID selectedElementId{};
	std::vector<DevInterfaceEditTransaction> editUndoStack{};
	std::vector<DevInterfaceEditTransaction> editRedoStack{};
	// RadioChoice binds to an unsigned selection value. This is normalized to a
	// valid DevInterfaceTab by DevContentHeader on every build.
	uint64_t activeTab = static_cast<uint64_t>(DevInterfaceTab::Inspect);
	WindowId selectedWindowId = MainWindowId;
	// Inspect selector controls use unsigned values to bind directly to FSEL.
	// Forest: 0 = Flow, 1 = Clay. Definition 0 means no filter.
	uint64_t inspectForest = 0u;
	uint64_t inspectDefinitionFilter = 0u;
	// Selection is intentionally independent from the active tab. A key of zero
	// means that no tree node is selected; kind is 1 for Flow and 2 for Clay.
	uint64_t inspectSelectedNodeKind = 0u;
	uint64_t inspectSelectedNodeKey = 0u;
	// Inspector detail tabs: 0 = Parameters, 1 = State, 2 = Resources,
	// 3 = Changes.
	uint64_t inspectInspectorTab = 0u;
	uint64_t nextEditTransaction = 1u;

	std::size_t interfaceMemoryBytes = 0;
	uint64_t cpuReportingLevel = FLOWUI_DEV_TIMING_LEVEL >= 1 ? 1u : 0u;

	DevApplicationState applicationState = DevApplicationState::Running;

	uint32_t unbakedChangeCount = 0u;
	uint32_t activeErrorCount = 0;

	float selectorWidth = 280.0f;
	float inspectorWidth = 320.0f;
	uint64_t inspect_selection_revision = 0u;
	uint8_t inspect_reveal_frames = 0u;
	bool panelPinned = false;
};

} // namespace FlowUi::devSystems

#endif
