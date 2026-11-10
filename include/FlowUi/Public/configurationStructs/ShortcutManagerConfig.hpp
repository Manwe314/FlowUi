#pragma once

#include <cstdint>

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

/** Select built-in text editing commands registered for focused input fields. */
struct DefaultTextShortcutConfig {
	/** Register the enabled command families during initialization. */
	bool enabled = true;
	/** Platform convention for primary command modifiers. */
	PlatformShortcutStyle platform = PlatformShortcutStyle::Auto;
	/** Dispatch priority assigned to the default text shortcuts. */
	int32_t priority = 0;
	/** Register select-all. */
	bool selectAll = true;
	/** Register copy, cut, and paste. */
	bool clipboard = true;
	/** Register undo/redo requests for the application to handle. */
	bool undoRedoRequests = true;
	/** Register word/document navigation and word deletion. */
	bool wordNavigation = true;
};

/** Configuration for built-in ShortcutManager registrations. */
struct ShortcutManagerConfig {
	/** Focused-input command registrations and dispatch defaults. */
	DefaultTextShortcutConfig textEditing{};
};

} // namespace FlowUi
