#include "TestHarness.hpp"

#include "FlowUi/Public/defaults/ShortcutDefaults.hpp"
#include "FlowUi/Public/errorManagement/Error.hpp"

#include "FlowUi/Public/input/Key.hpp"

#include <span>

namespace {

using namespace FlowUi;

void check_platform_shortcuts(PlatformShortcutStyle platform) {
	DefaultTextShortcutConfig configuration{};
	configuration.platform = platform;
	const auto plan = defaults::text_shortcuts(configuration);
	FLOWUI_CHECK(plan);
	FLOWUI_CHECK(plan->binding_count == 14u);
	const auto bindings = std::span{plan->bindings}.first(plan->binding_count);
	FLOWUI_CHECK(bindings.front().command == TextCommand::SelectAll);
	FLOWUI_CHECK(bindings.front().chord.key == keyCode(Key::A));
	for (const auto& binding : bindings) {
		const auto& chord = binding.chord;
		FLOWUI_CHECK(chord.trigger == ShortcutTrigger::Press);
		FLOWUI_CHECK(chord.key != keyCode(Key::Z) && chord.key != keyCode(Key::Y));
		const bool word_command = binding.command == TextCommand::MoveWordLeft ||
			binding.command == TextCommand::MoveWordRight ||
			binding.command == TextCommand::DeleteWordBackward ||
			binding.command == TextCommand::DeleteWordForward;
		if (platform == PlatformShortcutStyle::Control) {
			FLOWUI_CHECK(chord.ctrl && !chord.super && !chord.alt);
		} else {
			FLOWUI_CHECK(!chord.ctrl);
			FLOWUI_CHECK(chord.alt == word_command);
			FLOWUI_CHECK(chord.super != word_command);
		}
		if (binding.command == TextCommand::MoveDocumentStart)
			FLOWUI_CHECK(chord.key == (platform == PlatformShortcutStyle::Control
										   ? keyCode(Key::Home)
										   : keyCode(Key::Left)));
		if (binding.command == TextCommand::MoveDocumentEnd)
			FLOWUI_CHECK(chord.key == (platform == PlatformShortcutStyle::Control
										   ? keyCode(Key::End)
										   : keyCode(Key::Right)));
	}
	for (size_t first = 0u; first < bindings.size(); ++first) {
		for (size_t second = first + 1u; second < bindings.size(); ++second) {
			const auto& first_chord = bindings[first].chord;
			const auto& second_chord = bindings[second].chord;
			FLOWUI_CHECK(first_chord.key != second_chord.key ||
				first_chord.ctrl != second_chord.ctrl ||
				first_chord.shift != second_chord.shift ||
				first_chord.alt != second_chord.alt ||
				first_chord.super != second_chord.super);
		}
	}
}

void check_command_families() {
	DefaultTextShortcutConfig configuration{};
	configuration.clipboard = false;
	configuration.wordNavigation = false;
	const auto selection = defaults::text_shortcuts(configuration);
	FLOWUI_CHECK(selection && selection->binding_count == 1u);
	FLOWUI_CHECK(selection->bindings[0].command == TextCommand::SelectAll);
	configuration.selectAll = false;
	configuration.clipboard = true;
	const auto clipboard = defaults::text_shortcuts(configuration);
	FLOWUI_CHECK(clipboard && clipboard->binding_count == 3u);
	FLOWUI_CHECK(clipboard->bindings[0].command == TextCommand::Copy);
	FLOWUI_CHECK(clipboard->bindings[1].command == TextCommand::Cut);
	FLOWUI_CHECK(clipboard->bindings[2].command == TextCommand::Paste);
	configuration.clipboard = false;
	configuration.wordNavigation = true;
	const auto navigation = defaults::text_shortcuts(configuration);
	FLOWUI_CHECK(navigation && navigation->binding_count == 10u);
	configuration.wordNavigation = false;
	const auto empty = defaults::text_shortcuts(configuration);
	FLOWUI_CHECK(empty && empty->binding_count == 0u);
}

void check_disabled_and_invalid_configuration() {
	DefaultTextShortcutConfig configuration{};
	configuration.platform = static_cast<PlatformShortcutStyle>(255u);
	const auto invalid = defaults::text_shortcuts(configuration);
	FLOWUI_CHECK(!invalid);
	FLOWUI_CHECK(invalid.error().code == ErrorCode::ShortcutInvalid);
	FLOWUI_CHECK(invalid.error().site == ErrorSite::ShortcutManagerInitialize);
	configuration.enabled = false;
	const auto disabled = defaults::text_shortcuts(configuration);
	FLOWUI_CHECK(disabled && disabled->binding_count == 0u);
}

void check_automatic_platform() {
	const auto automatic = defaults::text_shortcuts(DefaultTextShortcutConfig{});
	FLOWUI_CHECK(automatic && automatic->binding_count == 14u);
#if defined(__APPLE__)
	FLOWUI_CHECK(automatic->bindings[0].chord.super);
	FLOWUI_CHECK(!automatic->bindings[0].chord.ctrl);
#else
	FLOWUI_CHECK(automatic->bindings[0].chord.ctrl);
	FLOWUI_CHECK(!automatic->bindings[0].chord.super);
#endif
}

} // namespace

int main() {
	FlowUi::test::Runner runner{};
	runner.run("Control text shortcut convention", [] {
		check_platform_shortcuts(PlatformShortcutStyle::Control);
	});
	runner.run("Command text shortcut convention", [] {
		check_platform_shortcuts(PlatformShortcutStyle::Command);
	});
	runner.run("text shortcut family switches", check_command_families);
	runner.run("disabled and invalid text shortcuts", check_disabled_and_invalid_configuration);
	runner.run("automatic text shortcut platform", check_automatic_platform);
	return runner.finish();
}
