/** @file
 * @brief Convenience include for the public application, managers, typed elements, and enabled optional APIs.
 */
#pragma once

#include "FlowUi/Public/BuildConfig.hpp"
#include "FlowUi/Public/identificationTypes/ElementID.hpp"
#include "FlowUi/Public/identificationTypes/AppActionID.hpp"
#include "FlowUi/Public/App.hpp"
#if FLOW_UI_DEV_MODE
#include "devSystems/devSystems.hpp"
#endif
#include "FlowUi/Public/identificationTypes/ResourceKey.hpp"
#include "FlowUi/Public/managers/FontManager.hpp"
#include "FlowUi/Public/managers/ImageManager.hpp"
#include "FlowUi/Public/managers/ThemeManager.hpp"
#include "FlowUi/Public/managers/ElementManager.hpp"
#include "FlowUi/Public/managers/ActionManager.hpp"
#include "FlowUi/Public/managerStructs/ActionTypes.hpp"
#include "FlowUi/Public/elementAuthoring/ElementSet.hpp"
#include "FlowUi/Public/managers/InputFieldManager.hpp"
#include "FlowUi/Public/managers/PopupManager.hpp"
#include "FlowUi/Public/managers/ShortcutManager.hpp"
#include "FlowUi/Public/elementAuthoring/ElementContexts.hpp"
#include "FlowUi/Public/elementAuthoring/ElementBuilder.hpp"
#include "FlowUi/Public/elements/Button.hpp"
#if FLOWUI_INCLUDE_ICON_MANAGER
#include "FlowUi/Public/managers/IconManager.hpp"
#endif
#include "FlowUi/Public/managers/UiManager.hpp"
#include "FlowUi/Public/AppElementWindows.hpp"
#include "clay.h"
#if FLOWUI_PUBLIC_VULKAN_INTEROP
#include "FlowUi/Public/managers/ViewPortManager.hpp"
#endif
#if COMPILE_FSELI
#include "FSEL.hpp"
#endif
