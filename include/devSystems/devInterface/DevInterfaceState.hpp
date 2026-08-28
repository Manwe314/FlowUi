#pragma once

#include "FlowUi/BuildConfig.hpp"

#if FLOW_UI_DEV_MODE

#include <cstddef>
#include <cstdint>
#include <string>

#include "FlowUi/ElementID.hpp"
#include "FlowUi/WindowId.hpp"

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

/** Persistent orchestration state owned by the singleton interface element. */
struct DevInterfaceState {
	// RadioChoice binds to an unsigned selection value. This is normalized to a
	// valid DevInterfaceTab by DevContentHeader on every build.
	uint64_t activeTab = static_cast<uint64_t>(DevInterfaceTab::Inspect);
	WindowId selectedWindowId = MainWindowId;
	FlowElementID selectedElementId{};
	std::string searchQuery{};

	DevApplicationState applicationState = DevApplicationState::Running;
	bool overlayEnabled = true;
	bool panelPinned = false;

	uint32_t unbakedChangeCount = 0u;
	std::string lastActionMessage = "Developer interface initialized";

	std::size_t interfaceMemoryBytes = 0;
	uint32_t activeErrorCount = 0;
	uint64_t cpuReportingLevel = FLOWUI_DEV_TIMING_LEVEL >= 1 ? 1u : 0u;

	float selectorWidth = 280.0f;
	float inspectorWidth = 320.0f;
};

} // namespace FlowUi::devSystems

#endif
