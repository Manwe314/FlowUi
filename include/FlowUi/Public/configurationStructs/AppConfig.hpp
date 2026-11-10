#pragma once

#include "FlowUi/Public/BuildConfig.hpp"
#include "FlowUi/Public/configurationStructs/DevToolsConfig.hpp"
#include "FlowUi/Public/configurationStructs/IconManagerConfig.hpp"
#include "FlowUi/Public/configurationStructs/MemoryCapacityProfile.hpp"
#include "FlowUi/Public/configurationStructs/ResourceConfig.hpp"
#include "FlowUi/Public/configurationStructs/UiConfig.hpp"
#include "FlowUi/Public/configurationStructs/VulkanConfig.hpp"
#include "FlowUi/Public/configurationStructs/WindowConfig.hpp"
#include "FlowUi/Public/errorManagement/ErrorPolicy.hpp"
#if FLOW_UI_AGENTIC_WORKFLOW
#include "devSystems/devAgenticWorkflow/AgenticTypes.hpp"
#endif

namespace FlowUi {

/**
 * @brief Top-level configuration used to initialize a FlowUi app.
 *
 * This struct groups the subsystem-specific configuration blocks passed through
 * app startup.
 */
struct AppConfig {
	/** Packaged shader/font and application resource lookup policy. */
	ResourceConfig resources{};
	/**
	 * @brief Initial native-window configuration for the semantic main window.
	 *
	 * @see FlowUi::WindowConfig
	 */
	WindowConfig window{};

	/**
	 * @brief Vulkan device, swapchain, and frame scheduling defaults.
	 *
	 * @see FlowUi::VulkanConfig
	 */
	VulkanConfig vk{};

	/**
	 * @brief Layout, text, and UI resource defaults.
	 *
	 * @see FlowUi::UiConfig
	 */
	UiConfig ui{};

	/**
	 * @brief SVG icon rasterization and atlas cache defaults.
	 *
	 * @see FlowUi::IconManagerConfig
	 */
	IconManagerConfig iconManager{};

	/** Optional profiled startup capacities; zero fields preserve library defaults. */
	MemoryCapacityProfile memoryCapacityProfile{};
	/** Present compiled capacities override caller hints before any owner is created. */
	bool prefer_baked_capacities = true;

	/** Resolution and centralized reporting choices used for the App lifetime. */
	ErrorContract errors{};

	/**
	 * @brief Developer tooling and capture defaults.
	 *
	 * @see FlowUi::DevToolsConfig
	 */
	DevToolsConfig dev{};
#if FLOW_UI_AGENTIC_WORKFLOW
	/** Explicit, dormant-by-default agentic evidence and local attachment policy. */
	devSystems::agentic::AgenticWorkflowConfig agentic_workflow{};
#endif
};

} // namespace FlowUi
