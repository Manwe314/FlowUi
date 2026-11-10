#pragma once
#include "FlowUi/Public/BuildConfig.hpp"
#if FLOW_UI_DEV_MODE
#include "FlowUi/Public/App.hpp"
#include "devSystems/devInterface/Permanents/Backend/DevInterfaceIcons.hpp"
#include <array>
#include <string_view>

namespace FlowUi::devSystems::diagnostics_icons {
/* Semantic raw-SVG artwork shared by the Diagnostics investigation surfaces. */
enum class Icon : uint8_t {
	None,
	Issue,
	Expanded,
	Collapsed,
	Copy,
	Refresh,
	Filter,
	Close,
	Check,
	Previous,
	Next,
	First,
	Latest,
	Pin,
	Follow,
	History,
	Advice,
	Timeline,
	Performance,
	Memory,
	Capture,
	Export,
	Freeze,
	Resume,
	Source,
	Info,
	Warning,
	Link,
	Snapshot,
	Settings,
	ChevronDown,
	ChevronUp,
	Back,
	Undo,
	Redo,
	Remove,
	ZoomIn,
	ZoomOut,
	Fit,
	Native,
	Count
};
/* Immutable registration record; descriptor text precedes its compact semantic identity. */
struct Descriptor {
	std::string_view key{}, svg{};
	Icon icon = Icon::None;
	constexpr Descriptor(Icon semantic, std::string_view registered_key,
						 std::string_view raw_svg) noexcept
		: key(registered_key), svg(raw_svg), icon(semantic) {}
};
inline constexpr std::array<Descriptor, 24> catalogue = {
	{{Icon::Filter, "flowui/dev-interface/diagnostics/filter",
	  R"svg(<svg viewBox="0 0 16 16" xmlns="http://www.w3.org/2000/svg"><path d="M2 3h12L9.5 8v4.5l-3 1V8L2 3Z" fill="none" stroke="#fff" stroke-width="1.5" stroke-linejoin="round"/></svg>)svg"},
	 {Icon::Close, "flowui/dev-interface/diagnostics/close",
	  R"svg(<svg viewBox="0 0 16 16" xmlns="http://www.w3.org/2000/svg"><path d="m4 4 8 8m0-8-8 8" fill="none" stroke="#fff" stroke-width="1.6" stroke-linecap="round"/></svg>)svg"},
	 {Icon::Check, "flowui/dev-interface/diagnostics/check",
	  R"svg(<svg viewBox="0 0 16 16" xmlns="http://www.w3.org/2000/svg"><path d="m3 8 3 3 7-7" fill="none" stroke="#fff" stroke-width="1.7" stroke-linecap="round" stroke-linejoin="round"/></svg>)svg"},
	 {Icon::Previous, "flowui/dev-interface/diagnostics/previous",
	  R"svg(<svg viewBox="0 0 16 16" xmlns="http://www.w3.org/2000/svg"><path d="m9.75 3.5-4.5 4.5 4.5 4.5" fill="none" stroke="#fff" stroke-width="1.7" stroke-linecap="round" stroke-linejoin="round"/></svg>)svg"},
	 {Icon::Next, "flowui/dev-interface/diagnostics/next",
	  R"svg(<svg viewBox="0 0 16 16" xmlns="http://www.w3.org/2000/svg"><path d="m6.25 3.5 4.5 4.5-4.5 4.5" fill="none" stroke="#fff" stroke-width="1.7" stroke-linecap="round" stroke-linejoin="round"/></svg>)svg"},
	 {Icon::First, "flowui/dev-interface/diagnostics/first",
	  R"svg(<svg viewBox="0 0 16 16" xmlns="http://www.w3.org/2000/svg"><path d="M3.5 3v10m8-9-4 4 4 4" fill="none" stroke="#fff" stroke-width="1.6" stroke-linecap="round" stroke-linejoin="round"/></svg>)svg"},
	 {Icon::Latest, "flowui/dev-interface/diagnostics/latest",
	  R"svg(<svg viewBox="0 0 16 16" xmlns="http://www.w3.org/2000/svg"><path d="M12.5 3v10m-8-9 4 4-4 4" fill="none" stroke="#fff" stroke-width="1.6" stroke-linecap="round" stroke-linejoin="round"/></svg>)svg"},
	 {Icon::Pin, "flowui/dev-interface/diagnostics/pin",
	  R"svg(<svg viewBox="0 0 16 16" xmlns="http://www.w3.org/2000/svg"><path d="m6 2 6 6m-5-5L4 6l-1 3 4 4 3-1 3-3M5.5 10.5 2 14" fill="none" stroke="#fff" stroke-width="1.4" stroke-linecap="round" stroke-linejoin="round"/></svg>)svg"},
	 {Icon::Follow, "flowui/dev-interface/diagnostics/follow",
	  R"svg(<svg viewBox="0 0 16 16" xmlns="http://www.w3.org/2000/svg"><circle cx="8" cy="8" r="4" fill="none" stroke="#fff" stroke-width="1.5"/><path d="M8 1v3m0 8v3M1 8h3m8 0h3" fill="none" stroke="#fff" stroke-width="1.5" stroke-linecap="round"/></svg>)svg"},
	 {Icon::History, "flowui/dev-interface/diagnostics/history",
	  R"svg(<svg viewBox="0 0 16 16" xmlns="http://www.w3.org/2000/svg"><path d="M3 5H1V2m.5 3a6 6 0 1 1 0 6M8 4v4l3 2" fill="none" stroke="#fff" stroke-width="1.5" stroke-linecap="round" stroke-linejoin="round"/></svg>)svg"},
	 {Icon::Advice, "flowui/dev-interface/diagnostics/advice",
	  R"svg(<svg viewBox="0 0 16 16" xmlns="http://www.w3.org/2000/svg"><path d="M5.5 11c0-2-2-2.3-2-5a4.5 4.5 0 0 1 9 0c0 2.7-2 3-2 5m-5 0h5M6 14h4M8 5v6" fill="none" stroke="#fff" stroke-width="1.4" stroke-linecap="round"/></svg>)svg"},
	 {Icon::Timeline,
	  "flowui/dev-interface/diagnostics/timeline", R"svg(<svg viewBox="0 0 16 16" xmlns="http://www.w3.org/2000/svg"><path d="M4 2v12m3-10h6M7 8h4m-4 4h6" fill="none" stroke="#fff" stroke-width="1.5" stroke-linecap="round"/><circle cx="4" cy="4" r="1.3" fill="#fff"/><circle cx="4" cy="8" r="1.3" fill="#fff"/><circle cx="4" cy="12" r="1.3" fill="#fff"/></svg>)svg"},
	 {Icon::Performance, "flowui/dev-interface/diagnostics/performance",
	  R"svg(<svg viewBox="0 0 16 16" xmlns="http://www.w3.org/2000/svg"><path d="M2 2v12h12M4.5 10l3-4 2.5 2 3-5" fill="none" stroke="#fff" stroke-width="1.5" stroke-linecap="round" stroke-linejoin="round"/></svg>)svg"},
	 {Icon::Memory, "flowui/dev-interface/diagnostics/memory",
	  R"svg(<svg viewBox="0 0 16 16" xmlns="http://www.w3.org/2000/svg"><rect x="4" y="4" width="8" height="8" rx="1" fill="none" stroke="#fff" stroke-width="1.4"/><path d="M6 1v3m4-3v3M6 12v3m4-3v3M1 6h3m-3 4h3m8-4h3m-3 4h3" fill="none" stroke="#fff" stroke-width="1.4" stroke-linecap="round"/></svg>)svg"},
	 {Icon::Capture, "flowui/dev-interface/diagnostics/capture",
	  R"svg(<svg viewBox="0 0 16 16" xmlns="http://www.w3.org/2000/svg"><path d="M2 6V2h4m4 0h4v4m0 4v4h-4m-4 0H2v-4" fill="none" stroke="#fff" stroke-width="1.5" stroke-linecap="round"/><circle cx="8" cy="8" r="2.5" fill="none" stroke="#fff" stroke-width="1.4"/></svg>)svg"},
	 {Icon::Export, "flowui/dev-interface/diagnostics/export",
	  R"svg(<svg viewBox="0 0 16 16" xmlns="http://www.w3.org/2000/svg"><path d="M8 10V1m-3 3 3-3 3 3M3 7v7h10V7" fill="none" stroke="#fff" stroke-width="1.5" stroke-linecap="round" stroke-linejoin="round"/></svg>)svg"},
	 {Icon::Freeze, "flowui/dev-interface/diagnostics/freeze",
	  R"svg(<svg viewBox="0 0 16 16" xmlns="http://www.w3.org/2000/svg"><path d="M5 3v10m6-10v10" fill="none" stroke="#fff" stroke-width="3" stroke-linecap="round"/></svg>)svg"},
	 {Icon::Resume, "flowui/dev-interface/diagnostics/resume",
	  R"svg(<svg viewBox="0 0 16 16" xmlns="http://www.w3.org/2000/svg"><path d="m5 2 8 6-8 6V2Z" fill="none" stroke="#fff" stroke-width="1.5" stroke-linejoin="round"/></svg>)svg"},
	 {Icon::Source, "flowui/dev-interface/diagnostics/source",
	  R"svg(<svg viewBox="0 0 16 16" xmlns="http://www.w3.org/2000/svg"><path d="m5 4-4 4 4 4m6-8 4 4-4 4M9 2 7 14" fill="none" stroke="#fff" stroke-width="1.4" stroke-linecap="round" stroke-linejoin="round"/></svg>)svg"},
	 {Icon::Info, "flowui/dev-interface/diagnostics/info",
	  R"svg(<svg viewBox="0 0 16 16" xmlns="http://www.w3.org/2000/svg"><circle cx="8" cy="8" r="6" fill="none" stroke="#fff" stroke-width="1.4"/><path d="M8 7v5m0-8h.01" fill="none" stroke="#fff" stroke-width="1.8" stroke-linecap="round"/></svg>)svg"},
	 {Icon::Link, "flowui/dev-interface/diagnostics/link",
	  R"svg(<svg viewBox="0 0 16 16" xmlns="http://www.w3.org/2000/svg"><path d="m6 4 2-2a3 3 0 0 1 4 4l-2 2m0 4-2 2a3 3 0 0 1-4-4l2-2m-1 3 6-6" fill="none" stroke="#fff" stroke-width="1.5" stroke-linecap="round"/></svg>)svg"},
	 {Icon::ChevronDown, "flowui/dev-interface/diagnostics/chevron-down",
      R"svg(<svg viewBox="0 0 16 16" xmlns="http://www.w3.org/2000/svg"><path d="m3 5.5 5 5 5-5" fill="none" stroke="#fff" stroke-width="1.6" stroke-linecap="round" stroke-linejoin="round"/></svg>)svg"},
     {Icon::ChevronUp, "flowui/dev-interface/diagnostics/chevron-up",
      R"svg(<svg viewBox="0 0 16 16" xmlns="http://www.w3.org/2000/svg"><path d="m3 10.5 5-5 5 5" fill="none" stroke="#fff" stroke-width="1.6" stroke-linecap="round" stroke-linejoin="round"/></svg>)svg"},
	 {Icon::Settings,
	  "flowui/dev-interface/diagnostics/settings", R"svg(<svg viewBox="0 0 16 16" xmlns="http://www.w3.org/2000/svg"><path d="M2 4h12M2 8h12M2 12h12" fill="none" stroke="#fff" stroke-width="1.4" stroke-linecap="round"/><path d="M5 2v4m6 0v4m-5 0v4" fill="none" stroke="#fff" stroke-width="2.5" stroke-linecap="round"/></svg>)svg"}}};

/* Registered texture handles; repeated surface use reuses the shared semantic keys. */
struct Resources {
	std::array<TextureRef, static_cast<std::size_t>(Icon::Count)> textures{};
	explicit Resources([[maybe_unused]] App& app) {
#if FLOWUI_INCLUDE_ICON_MANAGER
		auto& icons = app.icons();
		interface_icons::registerDevInterfaceIcons(icons);
		const auto assign = [&](Icon icon, std::string_view key) {
			textures[static_cast<std::size_t>(icon)] = icons.textureRef(key);
		};
		assign(Icon::Issue, interface_icons::kErrorReporterKey);
		assign(Icon::Warning, interface_icons::kErrorReporterKey);
		assign(Icon::Expanded, interface_icons::kTreeExpandedKey);
		assign(Icon::Collapsed, interface_icons::kTreeCollapsedKey);
		assign(Icon::Copy, interface_icons::kCopyKey);
		assign(Icon::Refresh, interface_icons::kRefreshKey);
		assign(Icon::Back, interface_icons::kBackKey);
		assign(Icon::Undo, interface_icons::kUndoKey);
		assign(Icon::Redo, interface_icons::kRedoKey);
		assign(Icon::Remove, interface_icons::kRemoveKey);
		assign(Icon::ZoomIn, interface_icons::kZoomInKey);
		assign(Icon::ZoomOut, interface_icons::kZoomOutKey);
		assign(Icon::Fit, interface_icons::kFitKey);
		assign(Icon::Native, interface_icons::kPreviewResetKey);
		for (const auto& descriptor : catalogue) {
			if (!icons.contains(descriptor.key)) {
				const auto registered = icons.registerSvg(descriptor.key, descriptor.svg);
				if (!registered)
					continue;
			}
			assign(descriptor.icon, descriptor.key);
		}
		textures[static_cast<std::size_t>(Icon::Snapshot)] =
			textures[static_cast<std::size_t>(Icon::Capture)];
#else
		textures.fill(TextureRef{});
#endif
	}
	/* Resolve a semantic icon without retaining pointers into the icon catalogue. */
	[[nodiscard]] TextureRef texture(Icon icon) const noexcept {
		const auto index = static_cast<std::size_t>(icon);
		return index < textures.size() ? textures[index] : TextureRef{};
	}
};

/* Conventional actions whose familiar artwork can replace a visible caption. */
[[nodiscard]] constexpr bool is_classic_action(Icon icon) noexcept {
	switch (icon) {
	case Icon::Copy:
	case Icon::Refresh:
	case Icon::Filter:
	case Icon::Close:
	case Icon::Previous:
	case Icon::Next:
	case Icon::First:
	case Icon::Latest:
	case Icon::Pin:
	case Icon::Follow:
	case Icon::Settings:
	case Icon::Export:
	case Icon::Freeze:
	case Icon::Resume:
	case Icon::Info:
	case Icon::ChevronDown:
	case Icon::ChevronUp:
	case Icon::Back:
	case Icon::Undo:
	case Icon::Redo:
	case Icon::Remove:
	case Icon::ZoomIn:
	case Icon::ZoomOut:
	case Icon::Fit:
	case Icon::Native:
		return true;
	default:
		return false;
	}
}

/* Legible fallback only used by builds without the optional icon manager. */
[[nodiscard]] constexpr std::string_view fallback(Icon icon) noexcept {
	switch (icon) {
	case Icon::Issue:
	case Icon::Warning:
		return "!";
	case Icon::ChevronDown:
		return "v";
	case Icon::ChevronUp:
		return "^";
	case Icon::Expanded:
		return "v";
	case Icon::Collapsed:
		return ">";
	case Icon::Check:
		return "yes";
	case Icon::Previous:
		return "<";
	case Icon::Next:
		return ">";
	default:
		return "";
	}
}
} // namespace FlowUi::devSystems::diagnostics_icons
#endif
