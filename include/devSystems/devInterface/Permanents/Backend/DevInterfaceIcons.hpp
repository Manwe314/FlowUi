#pragma once

#include <string_view>

#include "FlowUi/Public/BuildConfig.hpp"

#if FLOW_UI_DEV_MODE

#if FLOWUI_INCLUDE_ICON_MANAGER
#include "FlowUi/Public/managers/IconManager.hpp"
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
inline constexpr std::string_view kPreviewPanKey =
	"flowui/dev-interface/preview/pan";
inline constexpr std::string_view kPreviewResetKey =
	"flowui/dev-interface/preview/reset-one-to-one";
inline constexpr std::string_view kPreviewRulerKey =
	"flowui/dev-interface/preview/ruler";

inline constexpr std::string_view kAddKey = "flowui/dev-interface/action/add";
inline constexpr std::string_view kRemoveKey = "flowui/dev-interface/action/remove";
inline constexpr std::string_view kUpKey = "flowui/dev-interface/action/up";
inline constexpr std::string_view kDownKey = "flowui/dev-interface/action/down";
inline constexpr std::string_view kBackKey = "flowui/dev-interface/action/back";
inline constexpr std::string_view kForwardKey = "flowui/dev-interface/action/forward";
inline constexpr std::string_view kZoomInKey = "flowui/dev-interface/action/zoom-in";
inline constexpr std::string_view kZoomOutKey = "flowui/dev-interface/action/zoom-out";
inline constexpr std::string_view kFitKey = "flowui/dev-interface/action/fit";
inline constexpr std::string_view kClearKey = "flowui/dev-interface/action/clear";
inline constexpr std::string_view kHistoryKey = "flowui/dev-interface/action/history";
inline constexpr std::string_view kSettingsKey = "flowui/dev-interface/action/settings";
inline constexpr std::string_view kRefreshKey = "flowui/dev-interface/action/refresh";
inline constexpr std::string_view kExportKey = "flowui/dev-interface/action/export";
inline constexpr std::string_view kSaveKey = "flowui/dev-interface/action/save";
inline constexpr std::string_view kOpenKey = "flowui/dev-interface/action/open";
inline constexpr std::string_view kPlayKey = "flowui/dev-interface/action/play";
inline constexpr std::string_view kPauseKey = "flowui/dev-interface/action/pause";
inline constexpr std::string_view kStopKey = "flowui/dev-interface/action/stop";
inline constexpr std::string_view kRecordKey = "flowui/dev-interface/action/record";
inline constexpr std::string_view kEditKey = "flowui/dev-interface/action/edit";
inline constexpr std::string_view kCheckKey = "flowui/dev-interface/action/check";
inline constexpr std::string_view kShowKey = "flowui/dev-interface/action/show";
inline constexpr std::string_view kHideKey = "flowui/dev-interface/action/hide";
inline constexpr std::string_view kPinKey = "flowui/dev-interface/action/pin";
inline constexpr std::string_view kUnpinKey = "flowui/dev-interface/action/unpin";
inline constexpr std::string_view kInspectKey = "flowui/dev-interface/action/inspect";
inline constexpr std::string_view kLockKey = "flowui/dev-interface/action/lock";
inline constexpr std::string_view kUnlockKey = "flowui/dev-interface/action/unlock";
inline constexpr std::string_view kPreviousKey = kBackKey;
inline constexpr std::string_view kNextKey = kForwardKey;
inline constexpr std::string_view kMoveUpKey = kUpKey;
inline constexpr std::string_view kMoveDownKey = kDownKey;
inline constexpr std::string_view kCloseKey = kClearKey;
inline constexpr std::string_view kFollowKey = "flowui/dev-interface/action/follow";
inline constexpr std::string_view kCaptureKey = "flowui/dev-interface/action/capture";

// Semantic artwork shared by conventional developer-interface actions.
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

// Placeholder artwork for the preview toolbar. Keep these semantic keys when
// final artwork replaces the raw SVG literals.
inline constexpr std::string_view kPreviewPanSvg = R"svg(
<svg viewBox="0 0 16 16" xmlns="http://www.w3.org/2000/svg">
  <path d="M5.1 7.4V4.1a1 1 0 0 1 2 0v2.4-3.2a1 1 0 0 1 2 0v3.2-2.6a1 1 0 0 1 2 0v3.3-1.7a1 1 0 0 1 2 0v4.1c0 2.5-1.8 4.4-4.4 4.4H7.4c-1.4 0-2.5-.6-3.3-1.7L2.5 10a1.1 1.1 0 0 1 1.7-1.4l.9.9V7.4Z" fill="none" stroke="#fff" stroke-width="1.2" stroke-linecap="round" stroke-linejoin="round"/>
</svg>
)svg";

inline constexpr std::string_view kPreviewResetSvg = R"svg(
<svg viewBox="0 0 16 16" xmlns="http://www.w3.org/2000/svg">
  <path d="M4.5 2.5H2.25v2.25M2.6 4.4A6 6 0 1 1 2.5 11" fill="none" stroke="#fff" stroke-width="1.5" stroke-linecap="round" stroke-linejoin="round"/>
  <path d="M8 5.25v5.5M6.5 6.25 8 5.25v5.5M10.1 5.25h2.15v5.5H10.1Z" fill="none" stroke="#fff" stroke-width="1.1" stroke-linecap="round" stroke-linejoin="round"/>
</svg>
)svg";

inline constexpr std::string_view kPreviewRulerSvg = R"svg(
<svg viewBox="0 0 16 16" xmlns="http://www.w3.org/2000/svg">
  <path d="m3 11.75 8.75-8.75 2.25 2.25L5.25 14H3v-2.25Z" fill="none" stroke="#fff" stroke-width="1.4" stroke-linejoin="round"/>
  <path d="m9.8 4.95 1.25 1.25M7.9 6.85l1.25 1.25M6 8.75 7.25 10" fill="none" stroke="#fff" stroke-width="1.1" stroke-linecap="round"/>
</svg>
)svg";

inline constexpr std::string_view kAddSvg = R"svg(
<svg viewBox="0 0 16 16" xmlns="http://www.w3.org/2000/svg" fill="none" stroke="#fff" stroke-width="1.5" stroke-linecap="round" stroke-linejoin="round">
  <path d="M8 3v10M3 8h10"/>
</svg>
)svg";

inline constexpr std::string_view kRemoveSvg = R"svg(
<svg viewBox="0 0 16 16" xmlns="http://www.w3.org/2000/svg" fill="none" stroke="#fff" stroke-width="1.5" stroke-linecap="round" stroke-linejoin="round">
  <path d="M3 8h10"/>
</svg>
)svg";

inline constexpr std::string_view kUpSvg = R"svg(
<svg viewBox="0 0 16 16" xmlns="http://www.w3.org/2000/svg" fill="none" stroke="#fff" stroke-width="1.5" stroke-linecap="round" stroke-linejoin="round">
  <path d="m3 8 5-5 5 5M8 3v10"/>
</svg>
)svg";

inline constexpr std::string_view kDownSvg = R"svg(
<svg viewBox="0 0 16 16" xmlns="http://www.w3.org/2000/svg" fill="none" stroke="#fff" stroke-width="1.5" stroke-linecap="round" stroke-linejoin="round">
  <path d="m3 8 5 5 5-5M8 3v10"/>
</svg>
)svg";

inline constexpr std::string_view kBackSvg = R"svg(
<svg viewBox="0 0 16 16" xmlns="http://www.w3.org/2000/svg" fill="none" stroke="#fff" stroke-width="1.5" stroke-linecap="round" stroke-linejoin="round">
  <path d="m7 3-5 5 5 5M2 8h12"/>
</svg>
)svg";

inline constexpr std::string_view kForwardSvg = R"svg(
<svg viewBox="0 0 16 16" xmlns="http://www.w3.org/2000/svg" fill="none" stroke="#fff" stroke-width="1.5" stroke-linecap="round" stroke-linejoin="round">
  <path d="m9 3 5 5-5 5M2 8h12"/>
</svg>
)svg";

inline constexpr std::string_view kZoomInSvg = R"svg(
<svg viewBox="0 0 16 16" xmlns="http://www.w3.org/2000/svg" fill="none" stroke="#fff" stroke-width="1.5" stroke-linecap="round" stroke-linejoin="round">
  <circle cx="6.5" cy="6.5" r="4.5"/><path d="m10 10 4 4M4 6.5h5M6.5 4v5"/>
</svg>
)svg";

inline constexpr std::string_view kZoomOutSvg = R"svg(
<svg viewBox="0 0 16 16" xmlns="http://www.w3.org/2000/svg" fill="none" stroke="#fff" stroke-width="1.5" stroke-linecap="round" stroke-linejoin="round">
  <circle cx="6.5" cy="6.5" r="4.5"/><path d="m10 10 4 4M4 6.5h5"/>
</svg>
)svg";

inline constexpr std::string_view kFitSvg = R"svg(
<svg viewBox="0 0 16 16" xmlns="http://www.w3.org/2000/svg" fill="none" stroke="#fff" stroke-width="1.5" stroke-linecap="round" stroke-linejoin="round">
  <path d="M6 2H2v4m8-4h4v4M2 10v4h4m8-4v4h-4"/><rect x="5" y="5" width="6" height="6" rx=".5"/>
</svg>
)svg";

inline constexpr std::string_view kClearSvg = R"svg(
<svg viewBox="0 0 16 16" xmlns="http://www.w3.org/2000/svg" fill="none" stroke="#fff" stroke-width="1.5" stroke-linecap="round" stroke-linejoin="round">
  <path d="m4 4 8 8m0-8-8 8"/>
</svg>
)svg";

inline constexpr std::string_view kHistorySvg = R"svg(
<svg viewBox="0 0 16 16" xmlns="http://www.w3.org/2000/svg" fill="none" stroke="#fff" stroke-width="1.5" stroke-linecap="round" stroke-linejoin="round">
  <path d="M3 5H1V2m.5 3a6 6 0 1 1 0 6M8 4v4l3 2"/>
</svg>
)svg";

inline constexpr std::string_view kSettingsSvg = R"svg(
<svg viewBox="0 0 16 16" xmlns="http://www.w3.org/2000/svg" fill="none" stroke="#fff" stroke-width="1.5" stroke-linecap="round" stroke-linejoin="round">
  <path d="M2 4h12M2 8h12M2 12h12M5 2v4m6 0v4m-5 0v4"/>
</svg>
)svg";

inline constexpr std::string_view kRefreshSvg = R"svg(
<svg viewBox="0 0 16 16" xmlns="http://www.w3.org/2000/svg" fill="none" stroke="#fff" stroke-width="1.5" stroke-linecap="round" stroke-linejoin="round">
  <path d="M13 2v4H9M3 14v-4h4M13 6A5.2 5.2 0 0 0 3.5 3.5M3 10a5.2 5.2 0 0 0 9.5 2.5"/>
</svg>
)svg";

inline constexpr std::string_view kExportSvg = R"svg(
<svg viewBox="0 0 16 16" xmlns="http://www.w3.org/2000/svg" fill="none" stroke="#fff" stroke-width="1.5" stroke-linecap="round" stroke-linejoin="round">
  <path d="M8 10V1m-3 3 3-3 3 3M3 8v6h10V8"/>
</svg>
)svg";

inline constexpr std::string_view kSaveSvg = R"svg(
<svg viewBox="0 0 16 16" xmlns="http://www.w3.org/2000/svg" fill="none" stroke="#fff" stroke-width="1.5" stroke-linecap="round" stroke-linejoin="round">
  <path d="M3 2h8l3 3v9H2V2h1Zm2 0v4h6V2M5 14V9h6v5"/>
</svg>
)svg";

inline constexpr std::string_view kOpenSvg = R"svg(
<svg viewBox="0 0 16 16" xmlns="http://www.w3.org/2000/svg" fill="none" stroke="#fff" stroke-width="1.5" stroke-linecap="round" stroke-linejoin="round">
  <path d="M2 13V3h5l2 2h5v3M2 13l2-6h11l-2 6H2Z"/>
</svg>
)svg";

inline constexpr std::string_view kPlaySvg = R"svg(
<svg viewBox="0 0 16 16" xmlns="http://www.w3.org/2000/svg" fill="none" stroke="#fff" stroke-width="1.5" stroke-linecap="round" stroke-linejoin="round">
  <path d="m5 2 8 6-8 6V2Z"/>
</svg>
)svg";

inline constexpr std::string_view kPauseSvg = R"svg(
<svg viewBox="0 0 16 16" xmlns="http://www.w3.org/2000/svg" fill="none" stroke="#fff" stroke-width="1.5" stroke-linecap="round" stroke-linejoin="round">
  <path d="M5 3v10m6-10v10" stroke-width="3"/>
</svg>
)svg";

inline constexpr std::string_view kStopSvg = R"svg(
<svg viewBox="0 0 16 16" xmlns="http://www.w3.org/2000/svg" fill="none" stroke="#fff" stroke-width="1.5" stroke-linecap="round" stroke-linejoin="round">
  <rect x="3" y="3" width="10" height="10" rx="1"/>
</svg>
)svg";

inline constexpr std::string_view kRecordSvg = R"svg(
<svg viewBox="0 0 16 16" xmlns="http://www.w3.org/2000/svg" fill="none" stroke="#fff" stroke-width="1.5" stroke-linecap="round" stroke-linejoin="round">
  <circle cx="8" cy="8" r="5" fill="#fff"/>
</svg>
)svg";

inline constexpr std::string_view kEditSvg = R"svg(
<svg viewBox="0 0 16 16" xmlns="http://www.w3.org/2000/svg" fill="none" stroke="#fff" stroke-width="1.5" stroke-linecap="round" stroke-linejoin="round">
  <path d="m3 10 8-8 3 3-8 8-4 1 1-4Zm6-6 3 3"/>
</svg>
)svg";

inline constexpr std::string_view kCheckSvg = R"svg(
<svg viewBox="0 0 16 16" xmlns="http://www.w3.org/2000/svg" fill="none" stroke="#fff" stroke-width="1.5" stroke-linecap="round" stroke-linejoin="round">
  <path d="m3 8 3 3 7-7"/>
</svg>
)svg";

inline constexpr std::string_view kShowSvg = R"svg(
<svg viewBox="0 0 16 16" xmlns="http://www.w3.org/2000/svg" fill="none" stroke="#fff" stroke-width="1.5" stroke-linecap="round" stroke-linejoin="round">
  <path d="M1 8s2.5-5 7-5 7 5 7 5-2.5 5-7 5-7-5-7-5Z"/><circle cx="8" cy="8" r="2"/>
</svg>
)svg";

inline constexpr std::string_view kHideSvg = R"svg(
<svg viewBox="0 0 16 16" xmlns="http://www.w3.org/2000/svg" fill="none" stroke="#fff" stroke-width="1.5" stroke-linecap="round" stroke-linejoin="round">
  <path d="M2.5 5.5C1.5 6.8 1 8 1 8s2.5 5 7 5c1.2 0 2.3-.3 3.3-.8M5 3.7c.9-.4 1.9-.7 3-.7 4.5 0 7 5 7 5s-.5 1.1-1.5 2.4M2 2l12 12"/>
</svg>
)svg";

inline constexpr std::string_view kPinSvg = R"svg(
<svg viewBox="0 0 16 16" xmlns="http://www.w3.org/2000/svg" fill="none" stroke="#fff" stroke-width="1.5" stroke-linecap="round" stroke-linejoin="round">
  <path d="m6 2 6 6m-5-5L4 6l-1 3 4 4 3-1 3-3M5.5 10.5 2 14"/>
</svg>
)svg";

inline constexpr std::string_view kUnpinSvg = R"svg(
<svg viewBox="0 0 16 16" xmlns="http://www.w3.org/2000/svg" fill="none" stroke="#fff" stroke-width="1.5" stroke-linecap="round" stroke-linejoin="round">
  <path d="m6 2 6 6m-5-5L5.5 4.5M3.5 6.5 3 9l4 4 3-1 1.5-1.5M5.5 10.5 2 14M2 2l12 12"/>
</svg>
)svg";

inline constexpr std::string_view kInspectSvg = R"svg(
<svg viewBox="0 0 16 16" xmlns="http://www.w3.org/2000/svg" fill="none" stroke="#fff" stroke-width="1.5" stroke-linecap="round" stroke-linejoin="round">
  <path d="m3 2 3 10 2-4 4-2L3 2Zm5 6 5 5"/>
</svg>
)svg";

inline constexpr std::string_view kLockSvg = R"svg(
<svg viewBox="0 0 16 16" xmlns="http://www.w3.org/2000/svg" fill="none" stroke="#fff" stroke-width="1.5" stroke-linecap="round" stroke-linejoin="round">
  <rect x="3" y="7" width="10" height="7" rx="1"/><path d="M5 7V5a3 3 0 0 1 6 0v2M8 10v1"/>
</svg>
)svg";

inline constexpr std::string_view kUnlockSvg = R"svg(
<svg viewBox="0 0 16 16" xmlns="http://www.w3.org/2000/svg" fill="none" stroke="#fff" stroke-width="1.5" stroke-linecap="round" stroke-linejoin="round">
  <rect x="3" y="7" width="10" height="7" rx="1"/><path d="M5 7V5a3 3 0 0 1 6 0M8 10v1"/>
</svg>
)svg";

inline constexpr std::string_view kFollowSvg = R"svg(
<svg viewBox="0 0 16 16" xmlns="http://www.w3.org/2000/svg" fill="none" stroke="#fff" stroke-width="1.5" stroke-linecap="round">
  <circle cx="8" cy="8" r="4"/><path d="M8 1v3m0 8v3M1 8h3m8 0h3"/>
</svg>
)svg";

inline constexpr std::string_view kCaptureSvg = R"svg(
<svg viewBox="0 0 16 16" xmlns="http://www.w3.org/2000/svg" fill="none" stroke="#fff" stroke-width="1.5" stroke-linejoin="round">
  <path d="M2 5h3l1-2h4l1 2h3v9H2V5Z"/><circle cx="8" cy="9" r="2.5"/>
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
	if (!icons.contains(kPreviewPanKey)) {
		(void)icons.registerSvg(kPreviewPanKey, kPreviewPanSvg);
	}
	if (!icons.contains(kPreviewResetKey)) {
		(void)icons.registerSvg(kPreviewResetKey, kPreviewResetSvg);
	}
	if (!icons.contains(kPreviewRulerKey)) {
		(void)icons.registerSvg(kPreviewRulerKey, kPreviewRulerSvg);
	}
	if (!icons.contains(kAddKey)) {
		(void)icons.registerSvg(kAddKey, kAddSvg);
	}
	if (!icons.contains(kRemoveKey)) {
		(void)icons.registerSvg(kRemoveKey, kRemoveSvg);
	}
	if (!icons.contains(kUpKey)) {
		(void)icons.registerSvg(kUpKey, kUpSvg);
	}
	if (!icons.contains(kDownKey)) {
		(void)icons.registerSvg(kDownKey, kDownSvg);
	}
	if (!icons.contains(kBackKey)) {
		(void)icons.registerSvg(kBackKey, kBackSvg);
	}
	if (!icons.contains(kForwardKey)) {
		(void)icons.registerSvg(kForwardKey, kForwardSvg);
	}
	if (!icons.contains(kZoomInKey)) {
		(void)icons.registerSvg(kZoomInKey, kZoomInSvg);
	}
	if (!icons.contains(kZoomOutKey)) {
		(void)icons.registerSvg(kZoomOutKey, kZoomOutSvg);
	}
	if (!icons.contains(kFitKey)) {
		(void)icons.registerSvg(kFitKey, kFitSvg);
	}
	if (!icons.contains(kClearKey)) {
		(void)icons.registerSvg(kClearKey, kClearSvg);
	}
	if (!icons.contains(kHistoryKey)) {
		(void)icons.registerSvg(kHistoryKey, kHistorySvg);
	}
	if (!icons.contains(kSettingsKey)) {
		(void)icons.registerSvg(kSettingsKey, kSettingsSvg);
	}
	if (!icons.contains(kRefreshKey)) {
		(void)icons.registerSvg(kRefreshKey, kRefreshSvg);
	}
	if (!icons.contains(kExportKey)) {
		(void)icons.registerSvg(kExportKey, kExportSvg);
	}
	if (!icons.contains(kSaveKey)) {
		(void)icons.registerSvg(kSaveKey, kSaveSvg);
	}
	if (!icons.contains(kOpenKey)) {
		(void)icons.registerSvg(kOpenKey, kOpenSvg);
	}
	if (!icons.contains(kPlayKey)) {
		(void)icons.registerSvg(kPlayKey, kPlaySvg);
	}
	if (!icons.contains(kPauseKey)) {
		(void)icons.registerSvg(kPauseKey, kPauseSvg);
	}
	if (!icons.contains(kStopKey)) {
		(void)icons.registerSvg(kStopKey, kStopSvg);
	}
	if (!icons.contains(kRecordKey)) {
		(void)icons.registerSvg(kRecordKey, kRecordSvg);
	}
	if (!icons.contains(kEditKey)) {
		(void)icons.registerSvg(kEditKey, kEditSvg);
	}
	if (!icons.contains(kCheckKey)) {
		(void)icons.registerSvg(kCheckKey, kCheckSvg);
	}
	if (!icons.contains(kShowKey)) {
		(void)icons.registerSvg(kShowKey, kShowSvg);
	}
	if (!icons.contains(kHideKey)) {
		(void)icons.registerSvg(kHideKey, kHideSvg);
	}
	if (!icons.contains(kPinKey)) {
		(void)icons.registerSvg(kPinKey, kPinSvg);
	}
	if (!icons.contains(kUnpinKey)) {
		(void)icons.registerSvg(kUnpinKey, kUnpinSvg);
	}
	if (!icons.contains(kInspectKey)) {
		(void)icons.registerSvg(kInspectKey, kInspectSvg);
	}
	if (!icons.contains(kLockKey)) {
		(void)icons.registerSvg(kLockKey, kLockSvg);
	}
	if (!icons.contains(kUnlockKey)) {
		(void)icons.registerSvg(kUnlockKey, kUnlockSvg);
	}
	if (!icons.contains(kFollowKey)) {
		(void)icons.registerSvg(kFollowKey, kFollowSvg);
	}
	if (!icons.contains(kCaptureKey)) {
		(void)icons.registerSvg(kCaptureKey, kCaptureSvg);
	}
}
#endif

} // namespace FlowUi::devSystems::interface_icons

#endif
