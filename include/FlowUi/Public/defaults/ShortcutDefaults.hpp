#pragma once

#include "FlowUi/Public/configurationStructs/ShortcutManagerConfig.hpp"
#include "FlowUi/Public/errorManagement/Result.hpp"
#include "FlowUi/Public/managerStructs/InputFieldTypes.hpp"

#include <array>
#include <cstddef>

namespace FlowUi::defaults {

/** One built-in focused-input chord and the text command it requests. */
struct TextShortcutBinding {
	/** Exact key, modifiers, and trigger required by the command. */
	ShortcutChord chord{};
	/** Input-field operation enqueued when the chord matches. */
	TextCommand command{};
};

/** Owned, allocation-free built-in text shortcut registration plan. */
struct TextShortcutDefaults {
	/** Maximum number of bindings produced by the enabled command families. */
	static constexpr size_t capacity = 14u;
	/** Contiguous bindings; only the first binding_count entries are populated. */
	std::array<TextShortcutBinding, capacity> bindings{};
	/** Number of populated bindings, in their registration order. */
	size_t binding_count = 0u;
};

/**
 * Construct the configured built-in text shortcuts without registering callbacks.
 * Auto selects Command on Apple platforms and Control elsewhere. Disabled
 * configurations produce an empty plan; an invalid enabled platform returns
 * ShortcutInvalid. Scope and priority remain the installer's responsibility.
 */
[[nodiscard]] Result<TextShortcutDefaults>
text_shortcuts(const DefaultTextShortcutConfig& configuration) noexcept;

} // namespace FlowUi::defaults
