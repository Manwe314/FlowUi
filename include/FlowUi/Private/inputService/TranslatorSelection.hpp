#pragma once

#include "FlowUi/Public/BuildConfig.hpp"
#include "FlowUi/Public/input/InputTranslatorPack.hpp"
#if FLOWUI_CONFIG_CUSTOM_ADDITIONAL_TRANSLATORS
#include FLOWUI_CONFIG_ADDITIONAL_TRANSLATORS_HEADER
#endif

namespace FlowUi::detail::input {
#if FLOWUI_CONFIG_CUSTOM_ADDITIONAL_TRANSLATORS
using AdditionalInputTranslators = FLOWUI_CONFIG_ADDITIONAL_TRANSLATORS_TYPE;
#else
using AdditionalInputTranslators = InputTranslatorPack<>;
#endif
static_assert(IsInputTranslatorPack<AdditionalInputTranslators>::value,
			  "FLOWUI_ADDITIONAL_TRANSLATORS_TYPE must be a FlowUi::InputTranslatorPack of valid "
			  "translators");
} // namespace FlowUi::detail::input
