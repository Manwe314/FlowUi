#pragma once

#include "FlowUi/Public/configurationStructs/InputConfig.hpp"
#include "FlowUi/Public/managerStructs/InputFieldTypes.hpp"

namespace FlowUi::defaults {

/** Build the default field caret and selection style from the window input settings. */
[[nodiscard]] InputFieldOverlayStyle input_field_overlay_style(
	const InputManagerConfig& config) noexcept;

} // namespace FlowUi::defaults
