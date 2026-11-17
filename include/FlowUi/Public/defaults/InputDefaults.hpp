#pragma once

#include "FlowUi/Public/configurationStructs/InputConfig.hpp"
#include "FlowUi/Public/managerStructs/InputFieldTypes.hpp"

namespace FlowUi::defaults {

/** Build the default field caret and selection style from the window input settings.
 * @par Example
 * @code{.cpp}
 * FlowUi::InputManagerConfig configuration{};
 * configuration.caretWidthPx = 3.0f;
 * FlowUi::InputFieldOverlayStyle style =
 *     FlowUi::defaults::input_field_overlay_style(configuration);
 * @endcode
 */
[[nodiscard]] InputFieldOverlayStyle input_field_overlay_style(
	const InputManagerConfig& config) noexcept;

} // namespace FlowUi::defaults
