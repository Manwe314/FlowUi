# Input Fields and Shortcuts

This tutorial follows one editor-style UI from text fields to scoped keyboard commands. The target is a small node board where each node has an editable title, can become the focused element, and can be copied or pasted as a whole FlowUi element instance through app-specific shortcut code.

## Contents

- [Chapter 1: The Example App](#chapter-1-the-example-app)
  - [What We Are Building](#what-we-are-building)
  - [State Owned by the App](#state-owned-by-the-app)
  - [Why Focus Matters](#why-focus-matters)
- [Chapter 2: Configure Input Editing](#chapter-2-configure-input-editing)
  - [InputManagerConfig](#inputmanagerconfig)
  - [Caret Requests](#caret-requests)
  - [Cursor Requests](#cursor-requests)
- [Chapter 3: Build the Editable Node Element](#chapter-3-build-the-editable-node-element)
  - [Parameters and State](#parameters-and-state)
  - [Interaction Callbacks](#interaction-callbacks)
  - [Request the Input Field](#request-the-input-field)
  - [Remove or Reset a Field](#remove-or-reset-a-field)
- [Chapter 4: Register Shortcuts](#chapter-4-register-shortcuts)
  - [Shortcut Chords and Triggers](#shortcut-chords-and-triggers)
  - [Focused Input Shortcuts](#focused-input-shortcuts)
  - [Focused Element Shortcuts](#focused-element-shortcuts)
  - [Global Shortcuts](#global-shortcuts)
- [Chapter 5: How Shortcut Dispatch Resolves](#chapter-5-how-shortcut-dispatch-resolves)
  - [Scope Order](#scope-order)
  - [Priority and Registration Order](#priority-and-registration-order)
  - [Handled Return Values](#handled-return-values)
  - [Focused Element Copy and Paste](#focused-element-copy-and-paste)
- [Chapter 6: Shortcut Lifetime](#chapter-6-shortcut-lifetime)
- [Final Shape](#final-shape)

## Chapter 1: The Example App

### What We Are Building

Assume the app is a simple node board. Each node is a reusable Flow element with a title input and a body area.

```text
+-----------------------+
| Editable node title   |
|                       |
| node content          |
+-----------------------+
```

The user can:

- Click the title and edit it with a caret.
- Select text inside the title and use text copy/paste shortcuts.
- Click the node body to make the whole node shortcut-focused.
- Press `Ctrl+C` to copy the whole focused node.
- Press `Ctrl+V` to paste a duplicated node.
- Press `Delete` to remove the focused node.

This target uses both systems together: input fields own text editing, while shortcuts own app commands.

### State Owned by the App

The actual node data should live in the app model, not inside the input field manager.

```cpp
struct BoardNode {
    uint64_t id = 0;
    std::string title;
    Clay_Color color = FlowUi::Flow_Color("#202633ff");
};

struct BoardState {
    std::vector<BoardNode> nodes;
    std::optional<BoardNode> copiedNode;
    std::unordered_map<uint32_t, uint64_t> nodeIdByFocusedClayId;
    uint64_t nextNodeId = 1;
};
```

`InputFieldManager` stores editable field text. The app model stores the real document data. The element bridges them by reading the field result and writing changed text back to the matching `BoardNode`.

### Why Focus Matters

FlowUi has two useful focus concepts:

- **Primary input focus** belongs to `InputFieldManager`. It means text input and caret operations target a field.
- **Focused element** belongs to `ShortcutManager`. It means app-defined element shortcuts can target a Clay element id.

They are separate on purpose. A title field may have text focus, while a node card may have shortcut focus. Shortcut scopes let those contexts coexist without every `Ctrl+C` meaning the same thing.

## Chapter 2: Configure Input Editing

### InputManagerConfig

Input visuals are configured before app creation.

```cpp
FlowUi::AppConfig config{};
config.ui.inputManager.caretWidthPx = 2.0f;
config.ui.inputManager.caretColor = FlowUi::Flow_Color("#ffffffff");
config.ui.inputManager.highlightBoxColor = FlowUi::Flow_Color("#4285f496");
config.ui.inputManager.highlightedTextColor = FlowUi::Flow_Color("#ffffffff");

FlowUi::App app = FlowUi::makeApplication(config);
```

These settings affect caret and selection rendering. Field behavior is set per field with `FlowUi::FieldConfig`.

### Caret Requests

Input elements use `requestCaret(fieldId, kind)` from interaction callbacks.

```cpp
context.uiManager.inputFields().requestCaret(
    context.params.fieldId,
    FlowUi::CaretRequestKind::SetPrimary);
```

The request kinds are:

- `SetPrimary`: make this field the focused text input. This is the usual click-to-edit operation.
- `Add`: add another caret to the field. This is for advanced multi-caret editing.
- `ClearAll`: clear all carets and remove primary input focus. The `fieldId` is ignored by the operation.

Example: pressing `Escape` in the app can clear text focus.

```cpp
app.ui().inputFields().requestCaret("", FlowUi::CaretRequestKind::ClearAll);
```

### Cursor Requests

Elements can request cursor shapes during callbacks.

```cpp
context.uiManager.requestCursor(FlowUi::CursorType::IBeam, 20);
```

Common cursor types:

- `Arrow` or `Default`: normal pointer.
- `IBeam`: editable text.
- `PointingHand`: clickable controls.
- `Grab` and `Grabbing`: draggable regions.
- `ResizeHorizontal`, `ResizeVertical`, `ResizeDiagonalTL`, `ResizeDiagonalTR`, `ResizeAll`: resize handles.
- `NotAllowed`, `Wait`, `Progress`, `Crosshair`, `Custom`: specialized states.

The optional priority decides which request wins when multiple elements request cursors in one frame. Higher priority wins.

## Chapter 3: Build the Editable Node Element

### Parameters and State

The node element receives a pointer to app state and the node id it represents.

```cpp
struct EditableNodeParams {
    BoardState* board = nullptr;
    uint64_t nodeId = 0;
    uint16_t fontId = 0;
    Clay_Sizing sizing{.width = CLAY_SIZING_FIXED(280.0f), .height = CLAY_SIZING_FIT(0)};
};

struct EditableNodeState {
    bool initialized = false;
    std::string lastTitleText;
};

using EditableNodeDefinition = FlowUi::ElementDefinition<
    EditableNodeParams,
    EditableNodeState,
    void,
    FLOW_DEF_ID("tutorial_editable_node")>;
```

The Flow element state only tracks field-change detection. The node title itself stays in `BoardState`.

### Interaction Callbacks

Hovering a node asks for a pointing-hand cursor. Pressing the node sets the shortcut-focused element.

```cpp
inline const EditableNodeDefinition kEditableNode = {
    +[](EditableNodeDefinition::InteractionContext& context) {
        context.uiManager.requestCursor(FlowUi::CursorType::PointingHand, 5);
    },
    +[](EditableNodeDefinition::InteractionContext& context) {
        const Clay_ElementId rootId = context.uiManager.toClayEID(context.elementID);
        context.uiManager.shortcuts().setFocusedElement(rootId);
        if (context.params.board) {
            context.params.board->nodeIdByFocusedClayId[rootId.id] = context.params.nodeId;
        }
    },
    nullptr,
    nullptr,
    nullptr,
    nullptr,
```

The focused element marker is app-defined. FlowUi does not decide what a focused node means; the element does by calling `setFocusedElement()`.

Because shortcut dispatch happens at the beginning of a frame, a focus set during UI construction is used by later frames.

### Request the Input Field

The build callback finds the app node, creates child ids, and requests an input field for the title.

```cpp
    +[](EditableNodeDefinition::BuildContext& context) {
        if (!context.params.board) {
            return;
        }

        auto nodeIt = std::find_if(
            context.params.board->nodes.begin(),
            context.params.board->nodes.end(),
            [&](const BoardNode& node) { return node.id == context.params.nodeId; });
        if (nodeIt == context.params.board->nodes.end()) {
            return;
        }

        EditableNodeState& state =
            EditableNodeDefinition::getOrCreateState(FlowUi::toFlowId(context.elementID));

        const std::string titleFieldId = context.createChildElementId("title-field");
        const Clay_ElementId titleContentId = context.uiManager.toClayEID(context.createChildElementId("title-content"));
        const Clay_ElementId titleTextId = context.uiManager.toClayEID(context.createChildElementId("title-text"));
```

`requestField()` must be called every frame the field exists.

```cpp
        FlowUi::FieldQueryResult titleField = context.uiManager.inputFields().requestField({
            .fieldId = titleFieldId,
            .initialText = nodeIt->title,
            .config = FlowUi::FieldConfig{
                .readOnly = false,
                .allowNewline = false,
                .allowArrowNavigation = true,
                .maxBytes = 128,
            },
            .textElementId = titleTextId,
            .contentElementId = titleContentId,
        });
```

The returned `FieldQueryResult` contains manager-owned text, primary-caret state, and selection state.

```cpp
        const std::string titleText(titleField.text);
        if (!state.initialized) {
            state.initialized = true;
            state.lastTitleText = titleText;
        } else if (state.lastTitleText != titleText) {
            state.lastTitleText = titleText;
            nodeIt->title = titleText;
        }
```

Then draw the root and text field.

```cpp
        Clay_TextElementConfig text{};
        text.fontId = context.params.fontId;
        text.fontSize = 16;
        text.textColor = FlowUi::Flow_Color("#f5f7fbff");

        Clay_ElementDeclaration root{};
        root.layout.sizing = context.params.sizing;
        root.layout.padding = CLAY_PADDING_ALL(12);
        root.layout.childGap = 10;
        root.layout.layoutDirection = CLAY_TOP_TO_BOTTOM;
        root.backgroundColor = nodeIt->color;
        root.cornerRadius = CLAY_CORNER_RADIUS(8);

        Clay_ElementDeclaration titleBox{};
        titleBox.layout.sizing = {.width = CLAY_SIZING_GROW(0), .height = CLAY_SIZING_FIXED(30.0f)};
        titleBox.layout.padding = CLAY_PADDING_ALL(6);
        titleBox.backgroundColor = FlowUi::Flow_Color("#10141cff");
        titleBox.cornerRadius = CLAY_CORNER_RADIUS(5);

        CLAY(context.uiManager.toClayEID(context.elementID), root) {
            CLAY(titleContentId, titleBox) {
                CLAY_TEXT(context.uiManager.toClayString(titleField.text), CLAY_TEXT_CONFIG(text));
            }
        }
    },
};
```

### Remove or Reset a Field

Use `removeField()` when an app-side value should replace the current edited text or when a dynamic node is deleted.

```cpp
const std::string fieldId = "board/node-42/title-field";
const bool removed = app.ui().inputFields().removeField(fieldId);
```

Use `clear()` for a full input reset, for example when loading a different document.

```cpp
app.ui().inputFields().clear();
```

That removes all field text, focus, selections, key-repeat state, and pending input field render overrides.

## Chapter 4: Register Shortcuts

### Shortcut Chords and Triggers

A shortcut chord is a key, modifiers, and trigger.

```cpp
FlowUi::ShortcutChord copyChord{
    .key = GLFW_KEY_C,
    .ctrl = true,
    .trigger = FlowUi::ShortcutTrigger::Press,
};
```

Trigger modes:

- `Press`: fires on the frame the key changes from up to down. Use this for most commands.
- `Release`: fires on the frame the key changes from down to up.
- `Down`: fires every frame while the chord is held. Use carefully for repeated movement or scrub behavior.

Registration returns a `FlowUi::ShortcutId`.

```cpp
FlowUi::ShortcutId id = app.ui().shortcuts().registerShortcut(
    copyChord,
    FlowUi::ShortcutScope::Global,
    0,
    [](FlowUi::ShortcutContext&) { return false; });
```

`0` means registration failed, usually because the callback was empty or the key was outside the supported keyboard range.

### Focused Input Shortcuts

Focused input shortcuts run only when an input field owns primary focus.

```cpp
FlowUi::ShortcutId textCopy = app.ui().shortcuts().registerShortcut(
    FlowUi::ShortcutChord{.key = GLFW_KEY_C, .ctrl = true},
    FlowUi::ShortcutScope::FocusedInput,
    100,
    [](FlowUi::ShortcutContext& context) {
        const std::string selected(context.ui.inputFields().getSelectedText());
        if (selected.empty()) {
            return false;
        }
        context.ui.setClipboardText(selected);
        return true;
    });
```

Paste is symmetrical:

```cpp
FlowUi::ShortcutId textPaste = app.ui().shortcuts().registerShortcut(
    FlowUi::ShortcutChord{.key = GLFW_KEY_V, .ctrl = true},
    FlowUi::ShortcutScope::FocusedInput,
    100,
    [](FlowUi::ShortcutContext& context) {
        return context.ui.inputFields().insertTextAtPrimaryCaret(context.ui.clipboardText());
    });
```

The text copy shortcut uses `getSelectedText()`. The text paste shortcut uses `insertTextAtPrimaryCaret()`, which replaces active selections and respects `FieldConfig::readOnly` and `maxBytes`.

### Focused Element Shortcuts

Focused element shortcuts run only when `ShortcutManager` has a non-zero focused Clay element id.

```cpp
FlowUi::ShortcutId copyNode = app.ui().shortcuts().registerShortcut(
    FlowUi::ShortcutChord{.key = GLFW_KEY_C, .ctrl = true},
    FlowUi::ShortcutScope::FocusedElement,
    50,
    [&board](FlowUi::ShortcutContext& context) {
        const auto focusedIt = board.nodeIdByFocusedClayId.find(context.focusedElementId.id);
        if (focusedIt == board.nodeIdByFocusedClayId.end()) {
            return false;
        }

        auto nodeIt = std::find_if(
            board.nodes.begin(),
            board.nodes.end(),
            [&](const BoardNode& node) { return node.id == focusedIt->second; });
        if (nodeIt == board.nodes.end()) {
            return false;
        }

        board.copiedNode = *nodeIt;
        return true;
    });
```

This copies the whole node from app state. FlowUi only provides the focused Clay id; the app decides how that id maps to a domain object.

Pasting the copied node creates a new app model entry.

```cpp
FlowUi::ShortcutId pasteNode = app.ui().shortcuts().registerShortcut(
    FlowUi::ShortcutChord{.key = GLFW_KEY_V, .ctrl = true},
    FlowUi::ShortcutScope::FocusedElement,
    50,
    [&board](FlowUi::ShortcutContext&) {
        if (!board.copiedNode) {
            return false;
        }

        BoardNode pasted = *board.copiedNode;
        pasted.id = board.nextNodeId++;
        pasted.title += " copy";
        board.nodes.push_back(std::move(pasted));
        return true;
    });
```

This is the pattern for copying and pasting a whole FlowUi element: copy the app model that drives that element, then draw a new element instance for the new model id.

### Global Shortcuts

Global shortcuts are always eligible. They should usually avoid stealing commands from text editing.

```cpp
FlowUi::ShortcutId deleteNode = app.ui().shortcuts().registerShortcut(
    FlowUi::ShortcutChord{.key = GLFW_KEY_DELETE},
    FlowUi::ShortcutScope::Global,
    0,
    [&board](FlowUi::ShortcutContext& context) {
        if (context.ui.inputFields().hasPrimaryFieldFocus()) {
            return false;
        }

        const auto focusedIt = board.nodeIdByFocusedClayId.find(context.focusedElementId.id);
        if (focusedIt == board.nodeIdByFocusedClayId.end()) {
            return false;
        }

        std::erase_if(board.nodes, [&](const BoardNode& node) {
            return node.id == focusedIt->second;
        });
        context.ui.shortcuts().clearFocusedElement();
        return true;
    });
```

`hasPrimaryFieldFocus()` is the guard that keeps `Delete` from removing a node while the user is editing title text.

## Chapter 5: How Shortcut Dispatch Resolves

### Scope Order

Each frame, `ShortcutManager` compares current and previous input and dispatches matching chords.

For a matching chord, callbacks are ordered by scope:

1. `FocusedInput`
2. `FocusedElement`
3. `Global`

That means `Ctrl+C` first gives focused text fields a chance to copy selected text. If no input callback handles it, focused element callbacks can copy the selected node. If that also does not handle it, global callbacks can run.

### Priority and Registration Order

Inside the same scope, higher priority runs first.

```cpp
app.ui().shortcuts().registerShortcut(chord, FlowUi::ShortcutScope::FocusedElement, 100, highPriority);
app.ui().shortcuts().registerShortcut(chord, FlowUi::ShortcutScope::FocusedElement, 10, lowPriority);
```

If scope and priority are the same, earlier registrations run first.

### Handled Return Values

A shortcut callback returns `true` when it handled the command.

```cpp
return true;
```

Once a callback returns `true`, later callbacks for that same chord do not run. Returning `false` allows the next eligible callback to try.

This is why the text copy shortcut returns `false` when no text is selected. It lets the focused node copy shortcut handle `Ctrl+C`.

### Focused Element Copy and Paste

The focused-element copy/paste example works because the element sets focus:

```cpp
context.uiManager.shortcuts().setFocusedElement(rootId);
context.params.board->nodeIdByFocusedClayId[rootId.id] = context.params.nodeId;
```

The shortcut callback receives that same id:

```cpp
context.focusedElementId
```

The app then maps it back to a model id, copies the model object, and later pastes a new model object. FlowUi does not copy element state or clone UI nodes by itself. The right unit to copy is the app data that causes the element to be drawn.

## Chapter 6: Shortcut Lifetime

Use `unregisterShortcut()` when a feature or panel unloads.

```cpp
if (copyNode != 0) {
    (void)app.ui().shortcuts().unregisterShortcut(copyNode);
}
```

It is valid to unregister from inside a shortcut callback.

Use `clearFocusedElement()` when clicking empty board space:

```cpp
app.ui().shortcuts().clearFocusedElement();
```

Use `focusedElement()` when app code wants to inspect whether anything currently owns focused-element shortcut context.

```cpp
Clay_ElementId focused = app.ui().shortcuts().focusedElement();
if (focused.id != 0u) {
    // A FocusedElement shortcut can be eligible.
}
```

Use `clear()` when reloading all shortcut bindings, switching documents, or tearing down the editor.

```cpp
app.ui().shortcuts().clear();
```

That removes every registration and clears focused-element state.

## Final Shape

The final setup function registers text shortcuts, element shortcuts, and global fallbacks:

```cpp
struct BoardShortcuts {
    FlowUi::ShortcutId textCopy = 0;
    FlowUi::ShortcutId textPaste = 0;
    FlowUi::ShortcutId copyNode = 0;
    FlowUi::ShortcutId pasteNode = 0;
    FlowUi::ShortcutId deleteNode = 0;
};

BoardShortcuts registerBoardShortcuts(FlowUi::App& app, BoardState& board) {
    BoardShortcuts out{};

    out.textCopy = app.ui().shortcuts().registerShortcut(
        FlowUi::ShortcutChord{.key = GLFW_KEY_C, .ctrl = true},
        FlowUi::ShortcutScope::FocusedInput,
        100,
        [](FlowUi::ShortcutContext& context) {
            const std::string selected(context.ui.inputFields().getSelectedText());
            if (selected.empty()) {
                return false;
            }
            context.ui.setClipboardText(selected);
            return true;
        });

    out.textPaste = app.ui().shortcuts().registerShortcut(
        FlowUi::ShortcutChord{.key = GLFW_KEY_V, .ctrl = true},
        FlowUi::ShortcutScope::FocusedInput,
        100,
        [](FlowUi::ShortcutContext& context) {
            return context.ui.inputFields().insertTextAtPrimaryCaret(context.ui.clipboardText());
        });

    out.copyNode = app.ui().shortcuts().registerShortcut(
        FlowUi::ShortcutChord{.key = GLFW_KEY_C, .ctrl = true},
        FlowUi::ShortcutScope::FocusedElement,
        50,
        [&board](FlowUi::ShortcutContext& context) {
            const auto focusedIt = board.nodeIdByFocusedClayId.find(context.focusedElementId.id);
            if (focusedIt == board.nodeIdByFocusedClayId.end()) {
                return false;
            }
            auto nodeIt = std::find_if(board.nodes.begin(), board.nodes.end(), [&](const BoardNode& node) { return node.id == focusedIt->second; });
            if (nodeIt == board.nodes.end()) {
                return false;
            }
            board.copiedNode = *nodeIt;
            return true;
        });

    out.pasteNode = app.ui().shortcuts().registerShortcut(
        FlowUi::ShortcutChord{.key = GLFW_KEY_V, .ctrl = true},
        FlowUi::ShortcutScope::FocusedElement,
        50,
        [&board](FlowUi::ShortcutContext&) {
            if (!board.copiedNode) {
                return false;
            }
            BoardNode pasted = *board.copiedNode;
            pasted.id = board.nextNodeId++;
            pasted.title += " copy";
            board.nodes.push_back(std::move(pasted));
            return true;
        });

    return out;
}
```

The frame loop draws one editable element per node:

```cpp
while (!app.shouldClose()) {
    app.beginFrame();

    BoardState& board = getBoard();
    board.nodeIdByFocusedClayId.clear();

    for (const BoardNode& node : board.nodes) {
        app.ui()
            .createElement(kEditableNode, "board/node/" + std::to_string(node.id))
            .setParameters(EditableNodeParams{
                .board = &board,
                .nodeId = node.id,
                .fontId = app.ui().resolveFont("Body"),
            })
            .draw();
    }

    app.endFrame();
    app.drawFrame();
}
```

The core model is that input fields manage text focus and caret editing, while shortcuts manage app commands. `FocusedInput` lets text editing win first, `FocusedElement` lets the selected FlowUi element run app-specific behavior, and `Global` provides fallback commands when no local context handles the chord.
