#pragma once

#include <cstdint>
#include <optional>

#include "FlowUi/Public/managerStructs/ShortcutTypes.hpp"

namespace FlowUi {

/** Modifier convention for built-in text editing shortcuts. */
enum class PlatformShortcutStyle : uint8_t {
	/** Use Command on Apple platforms and Control elsewhere. */
	Auto = 0,
	/** Use Control as the primary command modifier. */
	Control,
	/** Use Super/Command as the primary command modifier. */
	Command,
};

/**
 * Built-in text commands registered at UI initialization for focused input fields.
 * Individual families require enabled and perform operations owned by FlowUi.
 */
struct DefaultTextShortcutConfig {
	/** Register the enabled command families during initialization. */
	bool enabled = true;
	/** Platform convention for primary command modifiers. */
	PlatformShortcutStyle platform = PlatformShortcutStyle::Auto;
	/** Dispatch priority assigned to built-in text shortcuts; higher values are considered first. */
	int32_t priority = 0;
	/** Register select-all. */
	bool selectAll = true;
	/** Register copy, cut, and paste. */
	bool clipboard = true;
	/** Register word/document navigation and word deletion. */
	bool wordNavigation = true;
};

/** Enable one native-window operation and optionally replace its platform chord. */
struct DefaultWindowShortcutBinding {
	/** Concrete key/modifiers; absent means inherit the platform binding. */
	std::optional<ShortcutChord> chord{};
	/** False removes this operation's shortcut, including a replacement chord. */
	bool enabled = true;
};

/** Shortcuts controlling the native window that receives keyboard input. */
struct DefaultWindowShortcutConfig {
	/** Request closure through the native window's existing close flag. */
	DefaultWindowShortcutBinding close_window{};
	/** Minimize the native window where supported. */
	DefaultWindowShortcutBinding minimize{};
	/** Explicit mapping only; there is no shipped maximize/restore chord. */
	DefaultWindowShortcutBinding maximize_restore{.enabled = false};
	/** Individually opt-in operation with a platform fullscreen chord. */
	DefaultWindowShortcutBinding toggle_fullscreen{.enabled = false};
	/** Higher values dispatch first within the window's Global shortcut scope. */
	int32_t priority = 0;
	/** Window shortcuts require explicit application opt-in. */
	bool enabled = false;
};

/** Configuration for complete default input-field and native-window shortcuts. */
struct ShortcutManagerConfig {
	/** Focused-input command registrations and dispatch defaults. */
	DefaultTextShortcutConfig textEditing{};
	/** Native-window command registrations; initially disabled. */
	DefaultWindowShortcutConfig window{};
};

} // namespace FlowUi
