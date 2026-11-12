#pragma once
#include "FlowUi/Private/platformProviderService/PlatformProviderServiceConcept.hpp"
#include "FlowUi/Public/BuildConfig.hpp"
#if FLOWUI_CONFIG_CUSTOM_PLATFORM_PROVIDER
#include FLOWUI_CONFIG_PLATFORM_PROVIDER_HEADER
#else
#include "FlowUi/Private/platformProviderService/providers/glfw/GlfwPlatformProviderService.hpp"
#endif
namespace FlowUi::detail::platform {
#if FLOWUI_CONFIG_CUSTOM_PLATFORM_PROVIDER
using ActivePlatformProviderService = FLOWUI_CONFIG_PLATFORM_PROVIDER_TYPE;
#else
using ActivePlatformProviderService = GlfwPlatformProviderService;
#endif
static_assert(PlatformProviderServiceConcept<ActivePlatformProviderService>,
			  "Selected platform provider must satisfy the complete phase-1 Vulkan "
			  "PlatformProviderServiceConcept");
} // namespace FlowUi::detail::platform
