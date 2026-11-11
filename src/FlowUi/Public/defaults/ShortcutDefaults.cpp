#include "FlowUi/Public/defaults/ShortcutDefaults.hpp"

#include "FlowUi/Public/errorManagement/Error.hpp"

#define GLFW_INCLUDE_NONE
#include <GLFW/glfw3.h>

namespace FlowUi::defaults {

Result<TextShortcutDefaults> text_shortcuts(const DefaultTextShortcutConfig& configuration) noexcept {
	TextShortcutDefaults result{};
	if (!configuration.enabled)
		return result;
	if (configuration.platform > PlatformShortcutStyle::Command) {
		return unexpectedError(
			makeError(ErrorCode::ShortcutInvalid, ErrorSite::ShortcutManagerInitialize));
	}
	PlatformShortcutStyle platform = configuration.platform;
	if (platform == PlatformShortcutStyle::Auto) {
#if defined(__APPLE__)
		platform = PlatformShortcutStyle::Command;
#else
		platform = PlatformShortcutStyle::Control;
#endif
	}
	const auto chord = [platform](int key, bool shift = false, bool word_modifier = false) {
		ShortcutChord binding{.key = key, .shift = shift};
		if (word_modifier && platform == PlatformShortcutStyle::Command)
			binding.alt = true;
		else if (platform == PlatformShortcutStyle::Command)
			binding.super = true;
		else
			binding.ctrl = true;
		return binding;
	};
	const auto add = [&result](ShortcutChord binding, TextCommand command) {
		result.bindings[result.binding_count++] = TextShortcutBinding{binding, command};
	};
	if (configuration.selectAll)
		add(chord(GLFW_KEY_A), TextCommand::SelectAll);
	if (configuration.clipboard) {
		add(chord(GLFW_KEY_C), TextCommand::Copy);
		add(chord(GLFW_KEY_X), TextCommand::Cut);
		add(chord(GLFW_KEY_V), TextCommand::Paste);
	}
	if (configuration.wordNavigation) {
		add(chord(GLFW_KEY_LEFT, false, true), TextCommand::MoveWordLeft);
		add(chord(GLFW_KEY_RIGHT, false, true), TextCommand::MoveWordRight);
		add(chord(GLFW_KEY_LEFT, true, true), TextCommand::MoveWordLeft);
		add(chord(GLFW_KEY_RIGHT, true, true), TextCommand::MoveWordRight);
		add(chord(GLFW_KEY_BACKSPACE, false, true), TextCommand::DeleteWordBackward);
		add(chord(GLFW_KEY_DELETE, false, true), TextCommand::DeleteWordForward);
		if (platform == PlatformShortcutStyle::Control) {
			add(chord(GLFW_KEY_HOME), TextCommand::MoveDocumentStart);
			add(chord(GLFW_KEY_END), TextCommand::MoveDocumentEnd);
			add(chord(GLFW_KEY_HOME, true), TextCommand::MoveDocumentStart);
			add(chord(GLFW_KEY_END, true), TextCommand::MoveDocumentEnd);
		} else {
			add(chord(GLFW_KEY_LEFT), TextCommand::MoveDocumentStart);
			add(chord(GLFW_KEY_RIGHT), TextCommand::MoveDocumentEnd);
			add(chord(GLFW_KEY_LEFT, true), TextCommand::MoveDocumentStart);
			add(chord(GLFW_KEY_RIGHT, true), TextCommand::MoveDocumentEnd);
		}
	}
	return result;
}

} // namespace FlowUi::defaults
