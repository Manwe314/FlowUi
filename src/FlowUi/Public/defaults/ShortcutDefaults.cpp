#include "FlowUi/Public/defaults/ShortcutDefaults.hpp"
#include "FlowUi/Public/input/Key.hpp"

#include "FlowUi/Public/errorManagement/Error.hpp"

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
		add(chord(FlowUi::keyCode(FlowUi::Key::A)), TextCommand::SelectAll);
	if (configuration.clipboard) {
		add(chord(FlowUi::keyCode(FlowUi::Key::C)), TextCommand::Copy);
		add(chord(FlowUi::keyCode(FlowUi::Key::X)), TextCommand::Cut);
		add(chord(FlowUi::keyCode(FlowUi::Key::V)), TextCommand::Paste);
	}
	if (configuration.wordNavigation) {
		add(chord(FlowUi::keyCode(FlowUi::Key::Left), false, true), TextCommand::MoveWordLeft);
		add(chord(FlowUi::keyCode(FlowUi::Key::Right), false, true), TextCommand::MoveWordRight);
		add(chord(FlowUi::keyCode(FlowUi::Key::Left), true, true), TextCommand::MoveWordLeft);
		add(chord(FlowUi::keyCode(FlowUi::Key::Right), true, true), TextCommand::MoveWordRight);
		add(chord(FlowUi::keyCode(FlowUi::Key::Backspace), false, true),
			TextCommand::DeleteWordBackward);
		add(chord(FlowUi::keyCode(FlowUi::Key::Delete), false, true),
			TextCommand::DeleteWordForward);
		if (platform == PlatformShortcutStyle::Control) {
			add(chord(FlowUi::keyCode(FlowUi::Key::Home)), TextCommand::MoveDocumentStart);
			add(chord(FlowUi::keyCode(FlowUi::Key::End)), TextCommand::MoveDocumentEnd);
			add(chord(FlowUi::keyCode(FlowUi::Key::Home), true), TextCommand::MoveDocumentStart);
			add(chord(FlowUi::keyCode(FlowUi::Key::End), true), TextCommand::MoveDocumentEnd);
		} else {
			add(chord(FlowUi::keyCode(FlowUi::Key::Left)), TextCommand::MoveDocumentStart);
			add(chord(FlowUi::keyCode(FlowUi::Key::Right)), TextCommand::MoveDocumentEnd);
			add(chord(FlowUi::keyCode(FlowUi::Key::Left), true), TextCommand::MoveDocumentStart);
			add(chord(FlowUi::keyCode(FlowUi::Key::Right), true), TextCommand::MoveDocumentEnd);
		}
	}
	return result;
}

} // namespace FlowUi::defaults
