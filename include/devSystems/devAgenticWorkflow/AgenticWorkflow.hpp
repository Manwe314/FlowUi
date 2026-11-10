#pragma once

/** @namespace FlowUi::devSystems::agentic
 * @brief Bounded capture, application-declared scenarios and owned evidence APIs. Available when FLOW_UI_AGENTIC_WORKFLOW is enabled.
 */

/** @file
 * @brief Public AgenticWorkflow declarations for optional development services.
 */
#include "FlowUi/Public/BuildConfig.hpp"
#if FLOW_UI_AGENTIC_WORKFLOW
#include "devSystems/devAgenticWorkflow/AgenticAnalysis.hpp"
#include "devSystems/devAgenticWorkflow/AgenticKnowledge.hpp"
#include "devSystems/devAgenticWorkflow/AgenticManifest.hpp"
#include "devSystems/devAgenticWorkflow/AgenticService.hpp"
#include "devSystems/devAgenticWorkflow/AgenticInteraction.hpp"
#include "devSystems/devAgenticWorkflow/AgenticInteractionRuntime.hpp"
#include "devSystems/devAgenticWorkflow/AgenticInteractionTargets.hpp"
#include "devSystems/devAgenticWorkflow/AgenticAction.hpp"
#include "devSystems/devAgenticWorkflow/AgenticVerification.hpp"
#endif
