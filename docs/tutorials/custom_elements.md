# Custom Elements

This tutorial builds a small RGB color picker from several custom FlowUi elements. The focus is the element system itself: callbacks, state, resources, nested elements, constructed containers, and practical builder usage.

## Contents

- [Chapter 1: Overview](#chapter-1-overview)
  - [What We Are Building](#what-we-are-building)
  - [Implementation Plan](#implementation-plan)
- [Chapter 2: Build the Input and Slider](#chapter-2-build-the-input-and-slider)
  - [Shared Helpers](#shared-helpers)
  - [Channel Input Element](#channel-input-element)
  - [Channel Slider Element](#channel-slider-element)
- [Chapter 3: Build Rows and the Color Picker](#chapter-3-build-rows-and-the-color-picker)
  - [Channel Row Element](#channel-row-element)
  - [Color Picker Element](#color-picker-element)
  - [Nested IDs](#nested-ids)
- [Chapter 4: Use It in an App](#chapter-4-use-it-in-an-app)
  - [Construct a Demo Container](#construct-a-demo-container)
  - [Draw the Picker and Preview Box](#draw-the-picker-and-preview-box)
  - [setParameters and mergeParams](#setparameters-and-mergeparams)
- [Chapter 6: Register for Developer Mode](#chapter-6-register-for-developer-mode)
- [Final Shape](#final-shape)

## Chapter 1: Overview

### What We Are Building

The target is a contained RGB color picker. It draws three channel rows, one for `R`, `G`, and `B`. Each row owns a `0..255` numeric value and contains:

```text
label    [ fill ][ handle ][ unfilled ]    input field
```

The slider stores a normalized `0.0..1.0` value internally. When dragged, it emits a discrete `0..255` value. The input field reads and writes the same row value through `InputFieldManager`. The parent color picker collects the three row values into its own state, and the app uses that color to draw a preview box next to the picker.

### Implementation Plan

We will build the UI as a small tree of Flow elements:

- `kChannelInput`: wraps `InputFieldManager` and emits `onValueChanged(uint8_t)` when the field text changes.
- `kChannelSlider`: uses hover, press, held, release, logic, and build callbacks to implement a draggable slider.
- `kColorChannelRow`: draws a label, slider, and input field, and stores one channel value.
- `kColorPicker`: draws three row elements and collects their values into one RGB state.
- `kColorWorkbenchPanel`: a constructed left-to-right container used by the app to place the picker and preview box side by side.

One important practical detail: `InputFieldManager` does not expose a direct "set text by field id" API. To push a slider value into the sibling input, the row removes that field with `removeField(fieldId)`, then the input recreates it on the next `requestField()` call using the new `initialText`.

## Chapter 2: Build the Input and Slider

### Shared Helpers

The row, input, slider, and preview all need the same conversions.

```cpp
inline uint8_t clampByte(int value) {
    return static_cast<uint8_t>(std::clamp(value, 0, 255));
}

inline uint8_t normalizedToByte(float value) {
    return clampByte(static_cast<int>(std::round(std::clamp(value, 0.0f, 1.0f) * 255.0f)));
}

inline float byteToNormalized(uint8_t value) {
    return static_cast<float>(value) / 255.0f;
}

inline std::string byteToText(uint8_t value) {
    return std::to_string(static_cast<int>(value));
}

inline uint8_t parseByteText(std::string_view text) {
    const int parsed = std::atoi(std::string(text).c_str());
    return clampByte(parsed);
}

inline Clay_Color rgbColor(uint8_t r, uint8_t g, uint8_t b) {
    return Clay_Color{static_cast<float>(r), static_cast<float>(g), static_cast<float>(b), 255.0f};
}
```

These helpers keep the element code focused on FlowUi behavior rather than conversion noise.

### Channel Input Element

The input element receives the field id and the external value that should appear when the field is recreated.

```cpp
struct ChannelInputParams {
    std::string fieldId;
    uint8_t externalValue = 0;
    uint16_t fontId = 0;
    std::function<void(uint8_t)> onValueChanged;
    Clay_Sizing sizing{.width = CLAY_SIZING_FIXED(58.0f), .height = CLAY_SIZING_FIXED(28.0f)};
};

struct ChannelInputState {
    bool initialized = false;
    std::string lastText;
};
```

`ChannelInputState` is not the canonical value. It only remembers the last observed text so the element can detect when `InputFieldManager` changed.

```cpp
using ChannelInputDefinition = FlowUi::ElementDefinition<
    ChannelInputParams,
    ChannelInputState,
    void,
    FLOW_DEF_ID("tutorial_channel_input")>;
```

Use `onHovered` and `onPressed` for normal input behavior: show the I-beam cursor and focus the field.

```cpp
inline const ChannelInputDefinition kChannelInput = {
    +[](ChannelInputDefinition::InteractionContext& context) {
        context.uiManager.requestCursor(FlowUi::CursorType::IBeam);
    },
    +[](ChannelInputDefinition::InteractionContext& context) {
        if (!context.params.fieldId.empty()) {
            context.uiManager.inputFields().requestCaret(context.params.fieldId, FlowUi::CaretRequestKind::SetPrimary);
        }
    },
    nullptr,
    nullptr,
    nullptr,
    nullptr,
    +[](ChannelInputDefinition::BuildContext& context) {
        ChannelInputState& state =
            ChannelInputDefinition::getOrCreateState(FlowUi::toFlowId(context.elementID));
        const Clay_ElementId contentId = context.uiManager.toClayEID(context.createChildElementId("content"));
        const Clay_ElementId textId = context.uiManager.toClayEID(context.createChildElementId("text"));
```

The field request is made every frame. If the row removed this field because the slider changed, the input manager recreates it with `initialText`.

```cpp
        const FlowUi::FieldQueryResult field = context.uiManager.inputFields().requestField({
            .fieldId = context.params.fieldId,
            .initialText = byteToText(context.params.externalValue),
            .config = FlowUi::FieldConfig{.allowNewline = false, .maxBytes = 3},
            .textElementId = textId,
            .contentElementId = contentId,
        });

        const std::string currentText(field.text);
        if (!state.initialized) {
            state.initialized = true;
            state.lastText = currentText;
        } else if (state.lastText != currentText) {
            state.lastText = currentText;
            if (context.params.onValueChanged) {
                context.params.onValueChanged(parseByteText(currentText));
            }
        }
```

The visual part is just a small Clay input box.

```cpp
        Clay_TextElementConfig text{};
        text.fontId = context.params.fontId;
        text.fontSize = 14;
        text.textColor = FlowUi::Flow_Color("#f5f7fbff");

        Clay_ElementDeclaration root{};
        root.layout.sizing = context.params.sizing;
        root.layout.padding = CLAY_PADDING_ALL(6);
        root.backgroundColor = FlowUi::Flow_Color("#151922ff");
        root.cornerRadius = CLAY_CORNER_RADIUS(5);
        root.border = {.color = FlowUi::Flow_Color("#3a4252ff"), .width = {1, 1, 1, 1, 0}};

        CLAY(context.uiManager.toClayEID(context.elementID), root) {
            CLAY(contentId, {.layout = {.sizing = {.width = CLAY_SIZING_GROW(0), .height = CLAY_SIZING_GROW(0)}}}) {
                CLAY_TEXT(context.uiManager.toClayString(field.text), CLAY_TEXT_CONFIG(text));
            }
        }
    },
};
```

### Channel Slider Element

The slider consumes a normalized value and emits a byte value when dragging changes it.

```cpp
struct ChannelSliderParams {
    float value = 0.0f;
    std::function<void(uint8_t)> onValueChanged;
    Clay_Color fillColor = FlowUi::Flow_Color("#ff5a5aff");
    Clay_Color trackColor = FlowUi::Flow_Color("#2b3039ff");
    Clay_Color handleColor = FlowUi::Flow_Color("#c8cfdaff");
    Clay_Sizing sizing{.width = CLAY_SIZING_FIXED(180.0f), .height = CLAY_SIZING_FIXED(18.0f)};
    float functionalWidthPx = 180.0f;
    float handleWidthPx = 10.0f;
};

struct ChannelSliderState {
    bool dragging = false;
    float pressMouseX = 0.0f;
    float pressValue = 0.0f;
    float currentValue = 0.0f;
    float lastEmittedValue = -1.0f;
};
```

The state tracks drag lifetime and change detection. The row remains the canonical `0..255` owner.

```cpp
using ChannelSliderDefinition = FlowUi::ElementDefinition<
    ChannelSliderParams,
    ChannelSliderState,
    void,
    FLOW_DEF_ID("tutorial_channel_slider")>;
```

The slider uses almost the whole callback surface. Hover requests a cursor, press starts dragging, held updates the normalized value, release stops dragging, and logic emits changes.

```cpp
inline const ChannelSliderDefinition kChannelSlider = {
    +[](ChannelSliderDefinition::InteractionContext& context) {
        context.uiManager.requestCursor(FlowUi::CursorType::PointingHand);
    },
    +[](ChannelSliderDefinition::InteractionContext& context) {
        ChannelSliderState& state =
            ChannelSliderDefinition::getOrCreateState(FlowUi::toFlowId(context.elementID));
        state.dragging = true;
        state.pressMouseX = context.uiManager.getCurrentFrameInput().mouseX;
        state.pressValue = std::clamp(context.params.value, 0.0f, 1.0f);
        state.currentValue = state.pressValue;
    },
    +[](ChannelSliderDefinition::InteractionContext& context) {
        ChannelSliderState& state =
            ChannelSliderDefinition::getOrCreateState(FlowUi::toFlowId(context.elementID));
        if (!state.dragging) {
            return;
        }
        const float deltaPx = context.uiManager.getCurrentFrameInput().mouseX - state.pressMouseX;
        const float deltaValue = deltaPx / std::max(1.0f, context.params.functionalWidthPx);
        state.currentValue = std::clamp(state.pressValue + deltaValue, 0.0f, 1.0f);
    },
    +[](ChannelSliderDefinition::InteractionContext& context) {
        ChannelSliderDefinition::getOrCreateState(FlowUi::toFlowId(context.elementID)).dragging = false;
    },
```

`runLogic` is the right place for the built-in value-changed behavior because it runs once every builder invocation after event callbacks.

```cpp
    +[](ChannelSliderDefinition::InteractionContext& context) {
        ChannelSliderState& state =
            ChannelSliderDefinition::getOrCreateState(FlowUi::toFlowId(context.elementID));
        if (!state.dragging) {
            state.currentValue = std::clamp(context.params.value, 0.0f, 1.0f);
        }
        if (std::fabs(state.currentValue - state.lastEmittedValue) > 0.001f) {
            state.lastEmittedValue = state.currentValue;
            if (context.params.onValueChanged) {
                context.params.onValueChanged(normalizedToByte(state.currentValue));
            }
        }
    },
    nullptr,
```

The build callback draws fill, handle, and unfilled sections.

```cpp
    +[](ChannelSliderDefinition::BuildContext& context) {
        ChannelSliderState& state =
            ChannelSliderDefinition::getOrCreateState(FlowUi::toFlowId(context.elementID));
        const float value = state.dragging ? state.currentValue : std::clamp(context.params.value, 0.0f, 1.0f);
        const float fillWidth = std::max(0.0f, context.params.functionalWidthPx * value - context.params.handleWidthPx * 0.5f);
        const float unfillWidth = std::max(0.0f, context.params.functionalWidthPx - fillWidth - context.params.handleWidthPx);

        Clay_ElementDeclaration root{};
        root.layout.sizing = context.params.sizing;
        root.layout.layoutDirection = CLAY_LEFT_TO_RIGHT;
        root.layout.childAlignment.y = CLAY_ALIGN_Y_CENTER;

        Clay_ElementDeclaration fill{};
        fill.layout.sizing = {.width = CLAY_SIZING_FIXED(fillWidth), .height = CLAY_SIZING_FIXED(8.0f)};
        fill.backgroundColor = context.params.fillColor;
        fill.cornerRadius = CLAY_CORNER_RADIUS(4);

        Clay_ElementDeclaration handle{};
        handle.layout.sizing = {.width = CLAY_SIZING_FIXED(context.params.handleWidthPx), .height = CLAY_SIZING_FIXED(18.0f)};
        handle.backgroundColor = context.params.handleColor;
        handle.cornerRadius = CLAY_CORNER_RADIUS(5);

        Clay_ElementDeclaration unfill{};
        unfill.layout.sizing = {.width = CLAY_SIZING_FIXED(unfillWidth), .height = CLAY_SIZING_FIXED(8.0f)};
        unfill.backgroundColor = context.params.trackColor;
        unfill.cornerRadius = CLAY_CORNER_RADIUS(4);

        CLAY(context.uiManager.toClayEID(context.elementID), root) {
            CLAY(context.uiManager.toClayEID(context.createChildElementId("fill")), fill) {}
            CLAY(context.uiManager.toClayEID(context.createChildElementId("handle")), handle) {}
            CLAY(context.uiManager.toClayEID(context.createChildElementId("unfilled")), unfill) {}
        }
    },
};
```

This intentionally keeps the press target as the slider root. If you wanted only the track to be interactive, you could query child ids through `context.previousInteraction` inside the callbacks.

## Chapter 3: Build Rows and the Color Picker

### Channel Row Element

Each row owns one channel value as `0..255`.

```cpp
struct ColorChannelRowParams {
    std::string label = "R";
    uint16_t fontId = 0;
    Clay_Color channelColor = FlowUi::Flow_Color("#ff5a5aff");
};

struct ColorChannelRowState {
    uint8_t value = 128;
    bool pendingInputReset = false;
};

using ColorChannelRowDefinition = FlowUi::ElementDefinition<
    ColorChannelRowParams,
    ColorChannelRowState,
    void,
    FLOW_DEF_ID("tutorial_color_channel_row")>;
```

The row builds the label, slider, and input field directly in its `buildElement` callback. No node construction is deferred into helper functions.

```cpp
inline const ColorChannelRowDefinition kColorChannelRow = {
    nullptr,
    nullptr,
    nullptr,
    nullptr,
    nullptr,
    nullptr,
    +[](ColorChannelRowDefinition::BuildContext& context) {
        ColorChannelRowState& state =
            ColorChannelRowDefinition::getOrCreateState(FlowUi::toFlowId(context.elementID));
        const std::string inputId = context.createChildElementId("input");
        const std::string fieldId = context.createChildElementId("field");

        if (state.pendingInputReset) {
            (void)context.uiManager.inputFields().removeField(fieldId);
            state.pendingInputReset = false;
        }
```

The row declaration is a standard left-to-right Clay container.

```cpp
        Clay_ElementDeclaration root{};
        root.layout.sizing = {.width = CLAY_SIZING_GROW(0), .height = CLAY_SIZING_FIT(0)};
        root.layout.layoutDirection = CLAY_LEFT_TO_RIGHT;
        root.layout.childGap = 8;
        root.layout.childAlignment.y = CLAY_ALIGN_Y_CENTER;

        Clay_TextElementConfig labelText{};
        labelText.fontId = context.params.fontId;
        labelText.fontSize = 14;
        labelText.textColor = FlowUi::Flow_Color("#f5f7fbff");

        CLAY(context.uiManager.toClayEID(context.elementID), root) {
            CLAY_TEXT(context.uiManager.toClayString(context.params.label), CLAY_TEXT_CONFIG(labelText));
```

The slider is linked to the row value. Its value-changed callback updates row state and requests an input reset.

```cpp
            context.uiManager
                .createElement(kChannelSlider, context.createChildElementId("slider"))
                .setParameters(ChannelSliderParams{
                    .value = byteToNormalized(state.value),
                    .onValueChanged = [&](uint8_t next) {
                        if (state.value != next) {
                            state.value = next;
                            state.pendingInputReset = true;
                        }
                    },
                    .fillColor = context.params.channelColor,
                })
                .draw();
```

The input is linked back to the same row state. When input text changes, it updates the row value.

```cpp
            context.uiManager
                .createElement(kChannelInput, inputId)
                .setParameters(ChannelInputParams{
                    .fieldId = fieldId,
                    .externalValue = state.value,
                    .fontId = context.params.fontId,
                    .onValueChanged = [&](uint8_t next) {
                        state.value = next;
                    },
                })
                .draw();
        }
    },
};
```

That gives the row two-way synchronization:

- Slider drag updates row state, removes the sibling field, and recreates input text from the new value.
- Input edit updates row state, and the slider consumes that row value on the next build.

### Color Picker Element

The parent color picker owns only the combined color state and layout parameters.

```cpp
struct ColorPickerParams {
    std::string title = "Color";
    uint16_t fontId = 0;
    Clay_Sizing sizing{.width = CLAY_SIZING_FIXED(390.0f), .height = CLAY_SIZING_FIT(0)};
    Clay_Padding padding = CLAY_PADDING_ALL(12);
    Clay_Color backgroundColor = FlowUi::Flow_Color("#202633ff");
    Clay_LayoutDirection channelLayout = CLAY_TOP_TO_BOTTOM;
};

struct ColorPickerState {
    uint8_t r = 128;
    uint8_t g = 128;
    uint8_t b = 128;
};
```

Resources store the channel colors once for the definition.

```cpp
struct ColorPickerResources {
    std::array<Clay_Color, 3> channelColors{
        FlowUi::Flow_Color("#ff5a5aff"),
        FlowUi::Flow_Color("#45d483ff"),
        FlowUi::Flow_Color("#5aa7ffff"),
    };

    ColorPickerResources() = default;
    explicit ColorPickerResources(FlowUi::App& app) {
        (void)app;
    }
};
```

The definition draws three row elements and then collects their states.

```cpp
using ColorPickerDefinition = FlowUi::ElementDefinition<
    ColorPickerParams,
    ColorPickerState,
    ColorPickerResources,
    FLOW_DEF_ID("tutorial_color_picker")>;

inline const ColorPickerDefinition kColorPicker = {
    nullptr,
    nullptr,
    nullptr,
    nullptr,
    nullptr,
    nullptr,
    +[](ColorPickerDefinition::BuildContext& context) {
        ColorPickerState& state =
            ColorPickerDefinition::getOrCreateState(FlowUi::toFlowId(context.elementID));
        ColorPickerResources& resources = ColorPickerDefinition::resources.value();
```

The root and title are ordinary Clay nodes.

```cpp
        Clay_ElementDeclaration root{};
        root.layout.sizing = context.params.sizing;
        root.layout.padding = context.params.padding;
        root.layout.childGap = 10;
        root.layout.layoutDirection = CLAY_TOP_TO_BOTTOM;
        root.backgroundColor = context.params.backgroundColor;
        root.cornerRadius = CLAY_CORNER_RADIUS(8);

        Clay_TextElementConfig titleText{};
        titleText.fontId = context.params.fontId;
        titleText.fontSize = 16;
        titleText.textColor = FlowUi::Flow_Color("#f5f7fbff");

        Clay_ElementDeclaration rows{};
        rows.layout.sizing = {.width = CLAY_SIZING_GROW(0), .height = CLAY_SIZING_FIT(0)};
        rows.layout.layoutDirection = context.params.channelLayout;
        rows.layout.childGap = 8;
```

Inside the rows container, the parent draws three row elements.

```cpp
        CLAY(context.uiManager.toClayEID(context.elementID), root) {
            CLAY_TEXT(context.uiManager.toClayString(context.params.title), CLAY_TEXT_CONFIG(titleText));
            CLAY(context.uiManager.toClayEID(context.createChildElementId("rows")), rows) {
                context.uiManager.createElement(kColorChannelRow, context.createChildElementId("R")).setParameters(ColorChannelRowParams{.label = "R", .fontId = context.params.fontId, .channelColor = resources.channelColors[0]}).draw();
                context.uiManager.createElement(kColorChannelRow, context.createChildElementId("G")).setParameters(ColorChannelRowParams{.label = "G", .fontId = context.params.fontId, .channelColor = resources.channelColors[1]}).draw();
                context.uiManager.createElement(kColorChannelRow, context.createChildElementId("B")).setParameters(ColorChannelRowParams{.label = "B", .fontId = context.params.fontId, .channelColor = resources.channelColors[2]}).draw();
            }
        }
```

After drawing the rows, the parent reads their states and stores the combined color.

```cpp
        if (ColorChannelRowState* row = ColorChannelRowDefinition::tryGetState(FlowUi::toFlowId(context.createChildElementId("R")))) state.r = row->value;
        if (ColorChannelRowState* row = ColorChannelRowDefinition::tryGetState(FlowUi::toFlowId(context.createChildElementId("G")))) state.g = row->value;
        if (ColorChannelRowState* row = ColorChannelRowDefinition::tryGetState(FlowUi::toFlowId(context.createChildElementId("B")))) state.b = row->value;
    },
};
```

The parent is not directly editing row internals during construction. It draws rows, then collects the public result stored in their element state.

### Nested IDs

The color picker never hard-codes absolute ids for child elements. It derives them from `context.elementID`:

```cpp
context.createChildElementId("R")
context.createChildElementId("R/slider")
context.createChildElementId("R/input")
context.createChildElementId("R/field")
```

If the app draws the picker as `"workbench/color-picker"`, all child state and input field ids are automatically scoped below that instance.

## Chapter 4: Use It in an App

### Construct a Demo Container

The app example uses a constructed Flow element as a layout container. It opens a left-to-right root, then the app draws the picker and preview box as children.

```cpp
struct ColorWorkbenchPanelParams {
    Clay_Sizing sizing{.width = CLAY_SIZING_GROW(0), .height = CLAY_SIZING_FIT(0)};
    Clay_Padding padding = CLAY_PADDING_ALL(16);
    Clay_Color backgroundColor = FlowUi::Flow_Color("#11151cff");
};

using ColorWorkbenchPanelDefinition = FlowUi::ElementDefinition<
    ColorWorkbenchPanelParams,
    void,
    void,
    FLOW_DEF_ID("tutorial_color_workbench_panel")>;
```

Only `constructElement` is set, because the caller owns the children.

```cpp
inline const ColorWorkbenchPanelDefinition kColorWorkbenchPanel = {
    nullptr,
    nullptr,
    nullptr,
    nullptr,
    nullptr,
    +[](ColorWorkbenchPanelDefinition::BuildContext& context) -> Clay_ElementDeclaration {
        Clay_ElementDeclaration root{};
        root.layout.sizing = context.params.sizing;
        root.layout.padding = context.params.padding;
        root.layout.childGap = 18;
        root.layout.layoutDirection = CLAY_LEFT_TO_RIGHT;
        root.backgroundColor = context.params.backgroundColor;
        return root;
    },
    nullptr,
};
```

### Draw the Picker and Preview Box

Initialize the color picker resources after creating the app.

```cpp
inline void initializeColorTutorialResources(FlowUi::App& app) {
    (void)ColorPickerDefinition::getResources(app);
}
```

In the frame loop, construct the panel, draw the picker, then draw a plain Clay preview box using the picker state.

```cpp
app.ui()
    .createElement(kColorWorkbenchPanel, "workbench")
    .construct();

app.ui()
    .createElement(kColorPicker, "workbench/color-picker")
    .setParameters(ColorPickerParams{.title = "Base Color", .fontId = bodyFont})
    .draw();

const ColorPickerState* color =
    ColorPickerDefinition::tryGetState(FlowUi::toFlowId("workbench/color-picker"));

Clay_ElementDeclaration preview{};
preview.layout.sizing = {.width = CLAY_SIZING_FIXED(160.0f), .height = CLAY_SIZING_FIXED(160.0f)};
preview.backgroundColor = color ? rgbColor(color->r, color->g, color->b) : FlowUi::Flow_Color("#000000ff");
preview.cornerRadius = CLAY_CORNER_RADIUS(8);

CLAY(app.ui().toClayEID("workbench/preview"), preview) {}

app.ui().drawConstructed();
```

`drawConstructed()` closes the panel opened by `construct()`. Everything emitted before that call becomes a child of the constructed panel.

### setParameters and mergeParams

`setParameters()` replaces the builder's parameter object with the one you pass. Default field values still exist because `ColorPickerParams{...}` is default-initialized before the designated fields are assigned.

```cpp
app.ui()
    .createElement(kColorPicker, "workbench/color-picker")
    .setParameters(ColorPickerParams{.title = "Base Color", .fontId = bodyFont})
    .draw();
```

`mergeParams()` is for preserving params that are already stored on a builder and changing only part of that existing object. This is useful when a builder is configured once, then reused across call sites.

```cpp
auto picker = app.ui()
    .createElement(kColorPicker, "unused")
    .setParameters(ColorPickerParams{.fontId = bodyFont, .backgroundColor = FlowUi::Flow_Color("#202633ff")});

picker.withElementID("workbench/base").mergeParams([](ColorPickerParams& params) { params.title = "Base"; }).draw();
picker.withElementID("workbench/tint").mergeParams([](ColorPickerParams& params) { params.title = "Tint"; }).draw();
```

In that example, `mergeParams()` preserves `fontId` and `backgroundColor` from the builder's existing params while changing the title for each instance.

## Chapter 6: Register for Developer Mode

Developer-mode registration is optional for runtime behavior, but it makes the types visible to FlowUi's dev tooling.

```cpp
FLOWUI_DEV_REGISTER_STRUCT(
    ChannelInputParams,
    FLOWUI_DEV_REFLECT_FIELD(ChannelInputParams, fieldId),
    FLOWUI_DEV_REFLECT_FIELD(ChannelInputParams, externalValue),
    FLOWUI_DEV_REFLECT_FIELD(ChannelInputParams, sizing));

FLOWUI_DEV_REGISTER_STRUCT(
    ChannelInputState,
    FLOWUI_DEV_REFLECT_FIELD(ChannelInputState, initialized),
    FLOWUI_DEV_REFLECT_FIELD(ChannelInputState, lastText));

FLOWUI_DEV_REGISTER_STRUCT(
    ChannelSliderParams,
    FLOWUI_DEV_REFLECT_FIELD(ChannelSliderParams, value),
    FLOWUI_DEV_REFLECT_FIELD(ChannelSliderParams, fillColor),
    FLOWUI_DEV_REFLECT_FIELD(ChannelSliderParams, trackColor),
    FLOWUI_DEV_REFLECT_FIELD(ChannelSliderParams, handleColor),
    FLOWUI_DEV_REFLECT_FIELD(ChannelSliderParams, sizing));

FLOWUI_DEV_REGISTER_STRUCT(
    ChannelSliderState,
    FLOWUI_DEV_REFLECT_FIELD(ChannelSliderState, dragging),
    FLOWUI_DEV_REFLECT_FIELD(ChannelSliderState, currentValue));

FLOWUI_DEV_REGISTER_STRUCT(
    ColorChannelRowParams,
    FLOWUI_DEV_REFLECT_FIELD(ColorChannelRowParams, label),
    FLOWUI_DEV_REFLECT_FIELD(ColorChannelRowParams, channelColor));

FLOWUI_DEV_REGISTER_STRUCT(
    ColorChannelRowState,
    FLOWUI_DEV_REFLECT_FIELD(ColorChannelRowState, value));

FLOWUI_DEV_REGISTER_STRUCT(
    ColorPickerParams,
    FLOWUI_DEV_REFLECT_FIELD(ColorPickerParams, title),
    FLOWUI_DEV_REFLECT_FIELD(ColorPickerParams, sizing),
    FLOWUI_DEV_REFLECT_FIELD(ColorPickerParams, padding),
    FLOWUI_DEV_REFLECT_FIELD(ColorPickerParams, backgroundColor),
    FLOWUI_DEV_REFLECT_FIELD(ColorPickerParams, channelLayout));

FLOWUI_DEV_REGISTER_STRUCT(
    ColorPickerState,
    FLOWUI_DEV_REFLECT_FIELD(ColorPickerState, r),
    FLOWUI_DEV_REFLECT_FIELD(ColorPickerState, g),
    FLOWUI_DEV_REFLECT_FIELD(ColorPickerState, b));

FLOWUI_DEV_REGISTER_STRUCT(ColorPickerResources);

FLOWUI_DEV_REGISTER_STRUCT(
    ColorWorkbenchPanelParams,
    FLOWUI_DEV_REFLECT_FIELD(ColorWorkbenchPanelParams, sizing),
    FLOWUI_DEV_REFLECT_FIELD(ColorWorkbenchPanelParams, padding),
    FLOWUI_DEV_REFLECT_FIELD(ColorWorkbenchPanelParams, backgroundColor));

FLOWUI_DEV_REGISTER_ELEMENT(ChannelInputDefinition, "Tutorial Channel Input");
FLOWUI_DEV_REGISTER_ELEMENT(ChannelSliderDefinition, "Tutorial Channel Slider");
FLOWUI_DEV_REGISTER_ELEMENT(ColorChannelRowDefinition, "Tutorial Color Channel Row");
FLOWUI_DEV_REGISTER_ELEMENT(ColorPickerDefinition, "Tutorial Color Picker");
FLOWUI_DEV_REGISTER_ELEMENT(ColorWorkbenchPanelDefinition, "Tutorial Color Workbench Panel");
```

Registering the definitions lets the dev runtime identify element instances. Registering structs lets it inspect reflected parameter and state fields.

## Final Shape

The final element tree is:

```text
kColorWorkbenchPanel (construct)
  kColorPicker
    kColorChannelRow R
      CLAY_TEXT label
      kChannelSlider
      kChannelInput
    kColorChannelRow G
      CLAY_TEXT label
      kChannelSlider
      kChannelInput
    kColorChannelRow B
      CLAY_TEXT label
      kChannelSlider
      kChannelInput
  CLAY preview box
```

The core app loop looks like this:

```cpp
FlowUi::App app = FlowUi::makeApplication(config);
initializeColorTutorialResources(app);

while (!app.shouldClose()) {
    app.beginFrame();

    FlowUi::FontId bodyFont = app.ui().resolveFont("Body");

    app.ui().createElement(kColorWorkbenchPanel, "workbench").construct();
    app.ui().createElement(kColorPicker, "workbench/color-picker").setParameters(ColorPickerParams{.title = "Base Color", .fontId = bodyFont}).draw();

    const ColorPickerState* color = ColorPickerDefinition::tryGetState(FlowUi::toFlowId("workbench/color-picker"));
    Clay_ElementDeclaration preview{};
    preview.layout.sizing = {.width = CLAY_SIZING_FIXED(160.0f), .height = CLAY_SIZING_FIXED(160.0f)};
    preview.backgroundColor = color ? rgbColor(color->r, color->g, color->b) : FlowUi::Flow_Color("#000000ff");
    preview.cornerRadius = CLAY_CORNER_RADIUS(8);
    CLAY(app.ui().toClayEID("workbench/preview"), preview) {}

    app.ui().drawConstructed();

    app.endFrame();
    app.drawFrame();
}
```

The important pattern is ownership. The row owns the channel value. The slider owns drag state and emits changed values through `runLogic`. The input owns text-editing interaction through `InputFieldManager` and emits changed values when field text changes. The color picker collects row state and exposes the combined RGB value for the rest of the app.
