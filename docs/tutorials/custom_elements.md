# Custom Elements

Build a small RGB picker from typed Flow elements and the modern Flow Standard Element Library (FSEL). This example demonstrates borrowed application data, nested identity, immutable resources, constructed containers, and schema descriptors. Enable `COMPILE_FSELI` to use the FSEL controls.

## Model and channel row

Keep the authoritative RGB value in application-owned contiguous data. Controls borrow a pointer only while their invocation and actions can use it; the model must outlive those uses.

```cpp
#include <FlowUi/Public/Flow.hpp>
#include <devSystems/devTooling/schema/DevSchemaDescriptor.hpp>
#include <FSEL/Slider.hpp>

#include <algorithm>
#include <array>
#include <string_view>

namespace color_tutorial {

struct ColorModel {
    std::array<double, 3> channels{128.0, 128.0, 128.0};
};

struct ChannelRowParameters {
    double* value = nullptr;
    std::string_view label{};
    Clay_Color color = FlowUi::Flow_Color("#ff5a5aff");
    std::uint16_t font_id = 0;
};

struct ChannelRow {
    using Parameters = ChannelRowParameters;
    using BuildContext = FlowUi::ElementBuildContext<ChannelRow>;
    static constexpr auto definitionId = FlowUi::DefinitionID("tutorial.channel-row");
    static constexpr std::string_view debugName = "Color Channel Row";

    static void buildElement(BuildContext& context) {
        Clay_ElementDeclaration root{};
        root.layout.sizing = {
            .width = CLAY_SIZING_GROW(0),
            .height = CLAY_SIZING_FIT(0),
        };
        root.layout.layoutDirection = CLAY_LEFT_TO_RIGHT;
        root.layout.childGap = 12;
        root.layout.childAlignment.y = CLAY_ALIGN_Y_CENTER;

        Clay_TextElementConfig text{};
        text.fontId = context.params.font_id;
        text.fontSize = 16;
        text.textColor = context.params.color;

        CLAY(context.clayID(), root) {
            CLAY_TEXT(context.uiManager.toClayString(context.params.label), CLAY_TEXT_CONFIG(text));
            context.uiManager.createElement(FlowUi::FSEL::kSlider, "value")
                .setParameters({
                    .roundingStep = 1.0,
                    .value = context.params.value,
                    .minimum = 0.0,
                    .maximum = 255.0,
                })
                .draw();
        }
    }
};

inline constexpr ChannelRow kChannelRow{};
static_assert(FlowUi::FlowElement<ChannelRow>);

} // namespace color_tutorial
```

`ChannelRow` is stateless because the channel value belongs to the model. FSEL's slider keeps only interaction state internally and updates the borrowed `double` value. Its `onBegin`, `onChanged`, and `onCommit` parameters can carry typed `ActionCall` values when the application needs notifications or transaction boundaries. A null value pointer disables interaction.

For editable numeric text, use the matching `FSEL::NumberInput<T>` with a supported native model type, or a `TextInput`/`TextArea` backed by `InputFieldManager`. These controls synchronize against authoritative application data; sibling controls do not need to erase and reconstruct each other's fields.

## Compose the picker

The picker borrows the complete model and shares an immutable channel-color palette through its `Resources` capability.

```cpp
namespace color_tutorial {

struct PickerParameters {
    ColorModel* model = nullptr;
    std::uint16_t font_id = 0;
};

struct PickerResources {
    std::array<Clay_Color, 3> colors{
        FlowUi::Flow_Color("#ff5a5aff"),
        FlowUi::Flow_Color("#45d483ff"),
        FlowUi::Flow_Color("#5aa7ffff"),
    };
};

struct ColorPicker {
    using Parameters = PickerParameters;
    using Resources = PickerResources;
    using BuildContext = FlowUi::ElementBuildContext<ColorPicker>;
    static constexpr auto definitionId = FlowUi::DefinitionID("tutorial.color-picker");
    static constexpr std::string_view debugName = "Color Picker";

    static void buildElement(BuildContext& context) {
        Clay_ElementDeclaration root{};
        root.layout.sizing = {
            .width = CLAY_SIZING_FIXED(360.0f),
            .height = CLAY_SIZING_FIT(0),
        };
        root.layout.padding = CLAY_PADDING_ALL(12);
        root.layout.childGap = 12;
        root.layout.layoutDirection = CLAY_TOP_TO_BOTTOM;
        root.backgroundColor = FlowUi::Flow_Color("#202633ff");
        root.cornerRadius = CLAY_CORNER_RADIUS(8);

        CLAY(context.clayID(), root) {
            if (context.params.model != nullptr) {
                constexpr std::array<std::string_view, 3> labels{"R", "G", "B"};
                for (std::size_t channel_index = 0; channel_index < labels.size(); ++channel_index) {
                    context.uiManager.createElement(kChannelRow, FlowUi::Indexed("channel", channel_index))
                        .setParameters({
                            .value = &context.params.model->channels[channel_index],
                            .label = labels[channel_index],
                            .color = context.resources().colors[channel_index],
                            .font_id = context.params.font_id,
                        })
                        .draw();
                }
            }
        }
    }
};

inline constexpr ColorPicker kColorPicker{};
static_assert(FlowUi::FlowElement<ColorPicker>);

} // namespace color_tutorial
```

Each row inherits the picker invocation's parent scope. `Indexed("channel", channel_index)` intentionally makes state follow channel position; the channels have a fixed order. Use `Keyed()` for reorderable application objects whose identity should follow a stable key. Child Flow calls take local name tokens, and plain Clay children use `context.clayID("child")`.

Resources are initialized lazily by `App::elements()` and shared across picker instances. No manual resource initialization or static state pool is required. For per-instance behavior, add a `State` alias and access it with `context.state()`; [Quick Start](quick_start.md) shows a stateful custom element.

## Construct a caller-owned container

A constructed element returns its root declaration and lets the caller emit children before `drawConstructed()` closes it.

```cpp
namespace color_tutorial {

struct WorkbenchPanel {
    using BuildContext = FlowUi::ElementBuildContext<WorkbenchPanel>;
    static constexpr auto definitionId = FlowUi::DefinitionID("tutorial.color-workbench");

    [[nodiscard]] static Clay_ElementDeclaration constructElement(BuildContext&) {
        Clay_ElementDeclaration root{};
        root.layout.sizing = {
            .width = CLAY_SIZING_GROW(0),
            .height = CLAY_SIZING_FIT(0),
        };
        root.layout.padding = CLAY_PADDING_ALL(16);
        root.layout.childGap = 18;
        root.layout.layoutDirection = CLAY_LEFT_TO_RIGHT;
        return root;
    }
};

inline constexpr WorkbenchPanel kWorkbenchPanel{};
static_assert(FlowUi::ConstructibleFlowElement<WorkbenchPanel>);

} // namespace color_tutorial
```

After creating the application, retain a `ColorModel` outside the frame loop. Resolve `body_font` from a registered font family before drawing text. Inside each successful frame:

```cpp
using namespace color_tutorial;
auto& ui = application.ui();
ui.createElement(kWorkbenchPanel, "workbench").construct();
ui.createElement(kColorPicker, "picker")
    .setParameters({.model = &color_model, .font_id = body_font})
    .draw();

Clay_ElementDeclaration preview{};
preview.layout.sizing = {
    .width = CLAY_SIZING_FIXED(160.0f),
    .height = CLAY_SIZING_FIXED(160.0f),
};
preview.backgroundColor = {
    static_cast<float>(std::clamp(color_model.channels[0], 0.0, 255.0)),
    static_cast<float>(std::clamp(color_model.channels[1], 0.0, 255.0)),
    static_cast<float>(std::clamp(color_model.channels[2], 0.0, 255.0)),
    255.0f,
};
preview.cornerRadius = CLAY_CORNER_RADIUS(8);
CLAY(ui.toClaySID("workbench/preview"), preview) {}
ui.drawConstructed();
```

`setParameters()` replaces the complete parameter draft. `mergeParams()` mutates an existing draft without dropping earlier values. Builders are invocation objects associated with their captured scope; create them while building the active window frame.

## Describe developer-visible values

Modern descriptors can expose the model and palette without making borrowed pointers editable:

```cpp
namespace color_tutorial {

FLOWUI_DEV_SCHEMA(ColorModel, FLOWUI_DEV_FIELD(ColorModel, channels))
FLOWUI_DEV_SCHEMA(
    ChannelRowParameters,
    FLOWUI_DEV_FIELD(ChannelRowParameters, label),
    FLOWUI_DEV_FIELD(ChannelRowParameters, color),
    FLOWUI_DEV_FIELD(ChannelRowParameters, font_id))
FLOWUI_DEV_SCHEMA(PickerParameters, FLOWUI_DEV_FIELD(PickerParameters, font_id))
FLOWUI_DEV_SCHEMA(PickerResources, FLOWUI_DEV_FIELD(PickerResources, colors))

} // namespace color_tutorial
```

Place descriptors beside their types before schema instantiation. The macros expand away when developer support is disabled. Normal builder invocation discovers element schemas; explicit `devCatalogue(...)` ingestion can populate offline definitions before first use. See [Developer Systems](developer_mode.md) for enum schemas, field constraints, catalogues, safe-point overrides, and durable baking.
