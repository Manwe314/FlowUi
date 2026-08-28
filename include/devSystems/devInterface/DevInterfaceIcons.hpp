#pragma once

#include <string_view>

#include "FlowUi/BuildConfig.hpp"

#if FLOW_UI_DEV_MODE

#if FLOWUI_INCLUDE_ICON_MANAGER
#include "managers/IconManager.hpp"
#endif

namespace FlowUi::devSystems::interface_icons {

inline constexpr std::string_view kErrorReporterKey =
	"flowui/dev-interface/reporter/error";
inline constexpr std::string_view kUnbakedChangesReporterKey =
	"flowui/dev-interface/reporter/unbaked-changes";

// Placeholder artwork. Keep the semantic keys when replacing these SVGs.
inline constexpr std::string_view kErrorReporterSvg = R"svg(
<svg viewBox="0 0 16 16" xmlns="http://www.w3.org/2000/svg">
  <circle cx="8" cy="8" r="6" fill="none" stroke="#fff" stroke-width="2"/>
  <path d="M8 4.5v4.25M8 11.5h.01" fill="none" stroke="#fff" stroke-width="2" stroke-linecap="round"/>
</svg>
)svg";

inline constexpr std::string_view kUnbakedChangesReporterSvg = R"svg(
<svg viewBox="0 0 16 16" xmlns="http://www.w3.org/2000/svg">
  <path d="M8 2.25 13.75 8 8 13.75 2.25 8 8 2.25Z" fill="none" stroke="#fff" stroke-width="2" stroke-linejoin="round"/>
</svg>
)svg";

#if FLOWUI_INCLUDE_ICON_MANAGER
inline void registerDevInterfaceIcons(IconManager& icons) {
	if (!icons.contains(kErrorReporterKey)) {
		(void)icons.registerSvg(kErrorReporterKey, kErrorReporterSvg);
	}
	if (!icons.contains(kUnbakedChangesReporterKey)) {
		(void)icons.registerSvg(
			kUnbakedChangesReporterKey, kUnbakedChangesReporterSvg);
	}
}
#endif

} // namespace FlowUi::devSystems::interface_icons

#endif
