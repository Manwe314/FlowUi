#pragma once

#include "FlowUi/Private/textLayoutService/TextLayoutServiceConcept.hpp"
#include "FlowUi/Public/BuildConfig.hpp"

#if FLOWUI_CONFIG_CUSTOM_TEXT_LAYOUT_PROVIDER
#include FLOWUI_CONFIG_TEXT_LAYOUT_PROVIDER_HEADER
#else
#include "FlowUi/Private/textLayoutService/providers/basic/BasicTextLayoutProvider.hpp"
#endif

namespace FlowUi::detail::text {
#if FLOWUI_CONFIG_CUSTOM_TEXT_LAYOUT_PROVIDER
using ActiveTextLayoutProvider = FLOWUI_CONFIG_TEXT_LAYOUT_PROVIDER_TYPE;
#else
using ActiveTextLayoutProvider = BasicTextLayoutProvider;
#endif

static_assert(TextLayoutServiceConcept<ActiveTextLayoutProvider>,
			  "Selected text provider must satisfy TextLayoutServiceConcept revision 1.");
} // namespace FlowUi::detail::text
