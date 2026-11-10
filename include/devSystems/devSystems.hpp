#pragma once

/** @namespace FlowUi::devSystems
 * @brief Optional development monitoring, reporting and application extension APIs. Available when FLOW_UI_DEV_MODE is enabled.
 */

/** @file
 * @brief Application integration declarations for the optional development services.
 */

#include "FlowUi/Public/BuildConfig.hpp"

#if FLOW_UI_DEV_MODE
#include "devSystems/devInterface/Permanents/Backend/DevInterface.hpp"
#include "devSystems/devMonitoringAndReporting/DevMonitoringAndReporting.hpp"
#include "devSystems/devMonitoringAndReporting/errors/DevError.hpp"
#include "devSystems/devMonitoringAndReporting/errors/DevErrorRecorder.hpp"
#include "devSystems/devMonitoringAndReporting/errors/DevErrorStackProvider.hpp"
#include "devSystems/devMonitoringAndReporting/errors/DevErrorTypes.hpp"
#include "devSystems/devMonitoringAndReporting/memory/DevMemory.hpp"
#include "devSystems/devMonitoringAndReporting/memory/DevMemoryProbe.hpp"
#include "devSystems/devMonitoringAndReporting/memory/DevMemoryRecorder.hpp"
#include "devSystems/devMonitoringAndReporting/memory/DevMemoryTypes.hpp"
#include "devSystems/devMonitoringAndReporting/reporting/DevMemoryReporting.hpp"
#include "devSystems/devMonitoringAndReporting/reporting/DevErrorAdvice.hpp"
#include "devSystems/devMonitoringAndReporting/reporting/DevErrorReporting.hpp"
#include "devSystems/devMonitoringAndReporting/reporting/DevTimingReporting.hpp"
#include "devSystems/devMonitoringAndReporting/timing/DevGpuTiming.hpp"
#include "devSystems/devMonitoringAndReporting/timing/DevTiming.hpp"
#include "devSystems/devMonitoringAndReporting/timing/DevTimingTypes.hpp"
#include "devSystems/devMonitoringAndReporting/timing/DevTimingZone.hpp"
#include "devSystems/devTooling/DevTooling.hpp"
#include "devSystems/devTooling/catalogue/DevCatalogLease.hpp"
#include "devSystems/devTooling/catalogue/DevCatalogTypes.hpp"
#include "devSystems/devTooling/catalogue/DevCatalogues.hpp"
#include "devSystems/devTooling/override/DevOverrideApply.hpp"
#include "devSystems/devTooling/override/DevOverrideCapture.hpp"
#include "devSystems/devTooling/override/DevOverrideEngine.hpp"
#include "devSystems/devTooling/override/DevOverrideTypes.hpp"
#include "devSystems/devTooling/schema/DevClayTypeAdapters.hpp"
#include "devSystems/devTooling/schema/DevFlowUiTypeAdapters.hpp"
#include "devSystems/devTooling/schema/DevSchemaDescriptor.hpp"
#include "devSystems/devTooling/schema/DevSchemaRegistry.hpp"
#include "devSystems/devTooling/schema/DevSchemaTypes.hpp"
#endif

#if FLOW_UI_AGENTIC_WORKFLOW
#include "devSystems/devAgenticWorkflow/AgenticWorkflow.hpp"
#endif
