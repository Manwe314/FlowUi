#pragma once

#include "FlowUi/BuildConfig.hpp"

#if FLOW_UI_DEV_MODE

#include "FlowUi/App.hpp"

namespace FlowUi::devSystems::interface_theme {

// Depth surfaces
inline constexpr Clay_Color kDepth0Keel = Flow_Color("#071A22");
inline constexpr Clay_Color kDepth1Panel = Flow_Color("#0D2230");
inline constexpr Clay_Color kDepth2Ink = Flow_Color("#182227");
inline constexpr Clay_Color kDepth3Elevated = Flow_Color("#1E2E38");
inline constexpr Clay_Color kHoverSurface = Flow_Color("#243848");
inline constexpr Clay_Color kSelectedRow = Flow_Color("#0E3654");

// Structural hairlines
inline constexpr Clay_Color kBorderPrimary = Flow_Color("#182F3C");
inline constexpr Clay_Color kBorderSubtle = Flow_Color("#182227");
inline constexpr Clay_Color kBorderVisible = Flow_Color("#26485C");

// Brand accents
inline constexpr Clay_Color kAccentCurrent = Flow_Color("#18B8A6");
inline constexpr Clay_Color kAccentSeaGlass = Flow_Color("#73D5C5");
inline constexpr Clay_Color kAccentSignalBlue = Flow_Color("#3288D8");
inline constexpr Clay_Color kAccentSignalCoral = Flow_Color("#F2684A");

// Typography
inline constexpr Clay_Color kTextCanvas = Flow_Color("#F5F8F7");
inline constexpr Clay_Color kTextSecondary = Flow_Color("#78B8C8");
inline constexpr Clay_Color kTextMuted = Flow_Color("#3D6878");

// Semantic status
inline constexpr Clay_Color kStatusGreen = Flow_Color("#2AB870");
inline constexpr Clay_Color kStatusAmber = Flow_Color("#D4922A");
inline constexpr Clay_Color kStatusRed = Flow_Color("#E05252");

} // namespace FlowUi::devSystems::interface_theme

#endif
