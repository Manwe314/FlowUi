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
inline constexpr std::string_view kTreeExpandedKey =
	"flowui/dev-interface/tree/expanded";
inline constexpr std::string_view kTreeCollapsedKey =
	"flowui/dev-interface/tree/collapsed";
inline constexpr std::string_view kCopyKey =
	"flowui/dev-interface/action/copy";
inline constexpr std::string_view kPasteKey =
	"flowui/dev-interface/action/paste";
inline constexpr std::string_view kUndoKey =
	"flowui/dev-interface/action/undo";
inline constexpr std::string_view kRedoKey =
	"flowui/dev-interface/action/redo";
inline constexpr std::string_view kRevertKey =
	"flowui/dev-interface/action/revert";
inline constexpr std::string_view kExpandKey =
	"flowui/dev-interface/action/expand";
inline constexpr std::string_view kCollapseKey =
	"flowui/dev-interface/action/collapse";

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

inline constexpr std::string_view kTreeExpandedSvg = R"svg(
<svg viewBox="0 0 16 16" xmlns="http://www.w3.org/2000/svg">
  <path d="M3.5 5.75 8 10.25l4.5-4.5" fill="none" stroke="#fff" stroke-width="1.75" stroke-linecap="round" stroke-linejoin="round"/>
</svg>
)svg";

inline constexpr std::string_view kTreeCollapsedSvg = R"svg(
<svg viewBox="0 0 16 16" xmlns="http://www.w3.org/2000/svg">
  <path d="m5.75 3.5 4.5 4.5-4.5 4.5" fill="none" stroke="#fff" stroke-width="1.75" stroke-linecap="round" stroke-linejoin="round"/>
</svg>
)svg";

inline constexpr std::string_view kCopySvg = R"svg(
<svg viewBox="0 0 16 16" xmlns="http://www.w3.org/2000/svg">
  <rect x="5.25" y="4.25" width="7" height="8" rx="1" fill="none" stroke="#fff" stroke-width="1.5"/>
  <path d="M3.75 10.25h-.5a1 1 0 0 1-1-1v-6a1 1 0 0 1 1-1h6a1 1 0 0 1 1 1v.5" fill="none" stroke="#fff" stroke-width="1.5" stroke-linecap="round"/>
</svg>
)svg";

inline constexpr std::string_view kPasteSvg = R"svg(
<svg viewBox="0 0 16 16" xmlns="http://www.w3.org/2000/svg">
  <path d="M5 4H3.75a1 1 0 0 0-1 1v7.25a1 1 0 0 0 1 1h8.5a1 1 0 0 0 1-1V5a1 1 0 0 0-1-1H11" fill="none" stroke="#fff" stroke-width="1.5"/>
  <rect x="5" y="2" width="6" height="3.5" rx="1" fill="none" stroke="#fff" stroke-width="1.5"/>
</svg>
)svg";

inline constexpr std::string_view kUndoSvg = R"svg(
<svg viewBox="0 0 16 16" xmlns="http://www.w3.org/2000/svg">
  <path d="M6.25 4 2.75 7.5 6.25 11M3.25 7.5h5.5a4 4 0 0 1 4 4" fill="none" stroke="#fff" stroke-width="1.6" stroke-linecap="round" stroke-linejoin="round"/>
</svg>
)svg";

inline constexpr std::string_view kRedoSvg = R"svg(
<svg viewBox="0 0 16 16" xmlns="http://www.w3.org/2000/svg">
  <path d="m9.75 4 3.5 3.5-3.5 3.5M12.75 7.5h-5.5a4 4 0 0 0-4 4" fill="none" stroke="#fff" stroke-width="1.6" stroke-linecap="round" stroke-linejoin="round"/>
</svg>
)svg";

inline constexpr std::string_view kRevertSvg = R"svg(
<svg viewBox="0 0 16 16" xmlns="http://www.w3.org/2000/svg">
  <path d="M4.25 5.5H1.75V3M2.25 5.25A6 6 0 1 1 2.5 11" fill="none" stroke="#fff" stroke-width="1.6" stroke-linecap="round" stroke-linejoin="round"/>
</svg>
)svg";

inline constexpr std::string_view kExpandSvg = R"svg(
<svg viewBox="0 0 16 16" xmlns="http://www.w3.org/2000/svg">
  <path d="M3.5 5.75 8 10.25l4.5-4.5" fill="none" stroke="#fff" stroke-width="1.75" stroke-linecap="round" stroke-linejoin="round"/>
</svg>
)svg";

inline constexpr std::string_view kCollapseSvg = R"svg(
<svg viewBox="0 0 16 16" xmlns="http://www.w3.org/2000/svg">
  <path d="M3.5 10.25 8 5.75l4.5 4.5" fill="none" stroke="#fff" stroke-width="1.75" stroke-linecap="round" stroke-linejoin="round"/>
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
	if (!icons.contains(kTreeExpandedKey)) {
		(void)icons.registerSvg(kTreeExpandedKey, kTreeExpandedSvg);
	}
	if (!icons.contains(kTreeCollapsedKey)) {
		(void)icons.registerSvg(kTreeCollapsedKey, kTreeCollapsedSvg);
	}
	if (!icons.contains(kCopyKey)) {
		(void)icons.registerSvg(kCopyKey, kCopySvg);
	}
	if (!icons.contains(kPasteKey)) {
		(void)icons.registerSvg(kPasteKey, kPasteSvg);
	}
	if (!icons.contains(kUndoKey)) {
		(void)icons.registerSvg(kUndoKey, kUndoSvg);
	}
	if (!icons.contains(kRedoKey)) {
		(void)icons.registerSvg(kRedoKey, kRedoSvg);
	}
	if (!icons.contains(kRevertKey)) {
		(void)icons.registerSvg(kRevertKey, kRevertSvg);
	}
	if (!icons.contains(kExpandKey)) {
		(void)icons.registerSvg(kExpandKey, kExpandSvg);
	}
	if (!icons.contains(kCollapseKey)) {
		(void)icons.registerSvg(kCollapseKey, kCollapseSvg);
	}
}
#endif

} // namespace FlowUi::devSystems::interface_icons

#endif
