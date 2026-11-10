# Input Fields and Shortcuts

Use application-owned values with FSEL text controls, and use `InputFieldManager` directly when authoring a custom editor. `ShortcutManager` dispatches commands by focus scope and priority. Both systems belong to the window's `UiManager`.

## Application values and editable elements

A node's title belongs to the document model. `FSEL::TextInput` borrows it and synchronizes the manager's editable state without a separate static element-state pool.

```cpp
#include <FlowUi/Public/Flow.hpp>
#include <FSEL/TextInput.hpp>

#include <cstdint>
#include <string>

struct BoardNode {
    std::uint64_t key = 0;
    std::string title = "Node";
    Clay_Color color = FlowUi::Flow_Color("#202633ff");
};

struct EditableNodeParameters {
    BoardNode* node = nullptr;
};

struct EditableNode {
    using Parameters = EditableNodeParameters;
    using BuildContext = FlowUi::ElementBuildContext<EditableNode>;
    using InteractionContext = FlowUi::ElementInteractionContext<EditableNode>;
    static constexpr auto definitionId = FlowUi::DefinitionID("tutorial.editable-node");
    static constexpr std::string_view debugName = "Editable Node";

    static void onPressed(InteractionContext& context) {
        context.uiManager.shortcuts().setFocusedElement(context.clayID());
    }

    static void buildElement(BuildContext& context) {
        Clay_ElementDeclaration root{};
        root.layout.sizing = {
            .width = CLAY_SIZING_FIXED(280.0f),
            .height = CLAY_SIZING_FIXED(160.0f),
        };
        root.layout.padding = CLAY_PADDING_ALL(12);
        root.layout.layoutDirection = CLAY_TOP_TO_BOTTOM;
        root.backgroundColor = context.params.node != nullptr
            ? context.params.node->color : FlowUi::Flow_Color("#303030ff");
        root.cornerRadius = CLAY_CORNER_RADIUS(8);

        CLAY(context.clayID(), root) {
            if (context.params.node != nullptr) {
                context.uiManager.createElement(FlowUi::FSEL::kTextInput, "title")
                    .setParameters({
                        .placeholder = "Node title",
                        .value = &context.params.node->title,
                        .maxBytes = 128,
                    })
                    .draw();
            }
        }
    }
};

inline constexpr EditableNode kEditableNode{};
static_assert(FlowUi::FlowElement<EditableNode>);
```

Register a font family through `FontManager` and configure the FSEL theme's font family before drawing text. Keep each borrowed node/string alive during invocation and any action using it. Avoid invalidating container references while constructing the frame. For a list, use a stable data key:

```cpp
for (auto& node : board_nodes) {
    application.ui().createElement(kEditableNode, FlowUi::Keyed("node", node.key))
        .setParameters({.node = &node})
        .draw();
}
```

Primary text focus and focused-element shortcut state are separate. A title field may own the primary caret while the node card remains the shortcut-focused element. Scope ordering lets text copy take precedence over a whole-node copy command.

## Custom input fields

For a custom editor, request the same typed field identity every frame it is visible:

```cpp
const auto field_id = context.childID("title-field");
const auto field = context.uiManager.inputFields().requestField(field_id, {
    .initialText = "Untitled",
    .config = {
        .maxBytes = 128,
        .mode = FlowUi::TextFieldMode::SingleLine,
    },
    .textElementId = context.clayID("title-text"),
    .contentElementId = context.clayID("title-content"),
});
const std::string owned_text = field.text.copy();
```

`initialText` seeds a new field; later requests preserve its edited text. Use `TextFieldMode::MultiLine` when line breaks are supported. Mode governs normalization, editing, and keyboard input.

`FieldTextView` borrows manager text and can span several chunks. Use `.contiguous()` only when the view is contiguous, `.copy()` for an owned string, or `.forEachChunk()` for traversal. Mutating the field invalidates its borrowed text view. There is no implicit string-view conversion.

Custom input elements must supply layout and editable text rendering as required by their field mode. FSEL `TextInput`, `TextArea`, and numeric editors already implement these contracts, including synchronization, focus, and caret/selection presentation.

Use typed identities with `requestCaret()` to focus or add a caret. `ClearAll` ignores the supplied identity:

```cpp
context.uiManager.inputFields().requestCaret(field_id, FlowUi::CaretRequestKind::SetPrimary);
application.ui().inputFields().requestCaret(
    FlowUi::FlowElementID{}, FlowUi::CaretRequestKind::ClearAll);
```

`replaceText(field_id, text, preserve_caret)` updates managed text explicitly. `removeField(field_id)` removes one field; `clear()` resets all managed input state. All accept supported strong identities or `ResourceKey`, not path strings. Configure caret/selection appearance through `AppConfig::ui.inputManager` before application creation, or field-local overlay styles.

## Register text shortcuts

Shortcut registration returns `Result<ShortcutId>`. Check it before retaining the ID. The following uses GLFW key codes; include GLFW's header when using its symbolic constants.

```cpp
const auto copy_shortcut = application.ui().shortcuts().registerShortcut(
    {.key = GLFW_KEY_C, .ctrl = true},
    FlowUi::ShortcutScope::FocusedInput,
    100,
    [](FlowUi::ShortcutContext& context) {
        const std::string selected(context.ui.inputFields().getSelectedText());
        if (selected.empty()) return false;
        context.ui.setClipboardText(selected);
        return true;
    });
if (!copy_shortcut) {
    throw FlowUi::FlowUiException(copy_shortcut.error());
}

const auto paste_shortcut = application.ui().shortcuts().registerShortcut(
    {.key = GLFW_KEY_V, .ctrl = true},
    FlowUi::ShortcutScope::FocusedInput,
    100,
    [](FlowUi::ShortcutContext& context) {
        return context.ui.inputFields().insertTextAtPrimaryCaret(context.ui.clipboardText());
    });
if (!paste_shortcut) {
    throw FlowUi::FlowUiException(paste_shortcut.error());
}
```

A callback returns `true` only when it handled the chord. Returning `false` allows the next eligible handler to run. Text insertion respects read-only fields, byte limits, and selections.

## Register app commands

Semantic application commands can be shared between a Button and a shortcut. Bind an app action once, check the result, and retain only its `AppActionCall`:

```cpp
constexpr auto SaveDocument = FlowUi::AppAction("editor.save");
const auto save_binding = application.actions().appActions().bind(
    SaveDocument, [](std::string* document) { document->append(" saved"); }, &document_text);
if (!save_binding) {
    throw FlowUi::FlowUiException(save_binding.error());
}
const auto save_shortcut = application.ui().shortcuts().registerShortcut(
    {.key = GLFW_KEY_S, .ctrl = true},
    FlowUi::ShortcutScope::Global,
    0,
    *save_binding,
    FlowUi::ShortcutHandling::Consume);
if (!save_shortcut) {
    throw FlowUi::FlowUiException(save_shortcut.error());
}
```

The example operation changes a borrowed string to make its effect explicit; substitute the application's actual document-save operation. Borrowed resources must outlive the binding. Shortcuts cannot retain local `UiActionCall` recipes; use those for immediate local UI behavior. See [Actions](../api/action_manager.md).

Whole-node copy, paste, and delete remain application model operations. Register them in `FocusedElement` scope, use `ShortcutContext::focusedElementId` to find the model target, and return `false` when no suitable target exists. Stage mutations that would invalidate node references until the current UI invocation has completed.

## Dispatch and lifetime

Dispatch evaluates matching chords in this order:

1. `FocusedInput`, when primary text focus exists.
2. `FocusedElement`, when an application-defined Clay focus marker exists.
3. `Global`, independent of focus.

Within a scope, higher priorities run first, then registration order. Callbacks stop dispatch by returning `true`. App-action registrations use the explicit `ShortcutHandling` policy: `Consume` stops after a successful invocation, and `PassThrough` continues. Disabled and unbound actions fall through.

`ShortcutTrigger::Press` fires on a press edge, `Release` on release, and `Down` every frame while held. Modifiers must match the registered chord. Frame dispatch uses current/previous input; focus assigned during UI construction affects subsequent dispatch.

Registrations belong to the window's `ShortcutManager`. Keep captures/resources alive for their full registration lifetime. Remove a checked registration with `unregisterShortcut(*result)`, or call `clear()` to remove all registrations. `clearFocusedElement()` clears only element shortcut focus; input caret focus is managed separately.

See [InputFieldManager API](../api/input_field_manager.md), [ShortcutManager API](../api/shortcut_manager.md), and [custom elements](custom_elements.md).
