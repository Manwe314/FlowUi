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
 * Startup configuration copied by makeApplication(). Change these values before app creation;
 * editing the caller's copy later does not reconfigure a running app. Secondary windows inherit
 * the app's Vulkan and UI defaults and supply their own native-window settings.
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

	/** Startup storage/container reserves and storage growth policy; may be handwritten or profiled. */
	MemoryCapacityProfile memoryCapacityProfile{};
	/** Let available baked capacities override caller hints before storage and managers are created. */
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
