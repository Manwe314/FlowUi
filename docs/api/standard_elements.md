# Standard element helpers

Include [FSEL.hpp](include/FSEL.hpp) for `FlowUi::FSEL`. The umbrella is also included by the public Flow header when effective `COMPILE_FSELI` is enabled. Definition tags are passed to `UiManager::createElement()`; use the [element builder and context reference](docs/api/elements.md) for drawing, identity, state and resources. Register `FSELTheme` through the checked [ThemeManager](docs/api/theme_manager.md) API before using themed controls. Model pointers, text views, spans and action resources in parameters remain borrowed through their documented frame/invocation lifetimes.

## Resource constructors

| Declaration | Behavior |
| --- | --- |
| `ComboBoxResources() = default` | Empty open/closed icon references. |
| `explicit ComboBoxResources(App& app)` | Resolve available semantic disclosure icon keys from the owning app. |
| `NumberInputResources() = default` | Empty increment/decrement icon references. |
| `explicit NumberInputResources(App& app)` | Resolve available semantic numeric icon keys from the owning app. |

These are direct constructors. With icon support enabled, a failed App/manager result is raised as `FlowUiException`; missing unregistered keys remain empty. With icon support disabled they leave the references empty. They neither own the App nor extend icon/GPU lifetime. Normal element resource preparation creates one immutable app-wide resource payload per definition; construct these values directly only when that ownership/lifetime is appropriate.

```cpp
FlowUi::FSEL::ComboBoxResources comboArtwork{application};
FlowUi::FSEL::NumberInputResources numericArtwork{application};
```

See [ComboBox.hpp](include/FSEL/ComboBox.hpp), [NumberInput.hpp](include/FSEL/NumberInput.hpp) and [standard icon registration](docs/api/resources.md#registerstandardicons). These constructors can propagate errors to the enclosing application boundary.

## Slider visibility functions

Include [Slider.hpp](include/FSEL/Slider.hpp). All three functions are `[[nodiscard]] constexpr` and `noexcept`:

```cpp
SliderVisualParts operator|(SliderVisualParts left, SliderVisualParts right) noexcept;
SliderVisualParts operator&(SliderVisualParts left, SliderVisualParts right) noexcept;
bool hasSliderVisualPart(SliderVisualParts parts, SliderVisualParts part) noexcept;
```

`|` combines masks, `&` intersects masks, and `hasSliderVisualPart()` tests whether **any** requested bit is present. `None` always tests false; a composite requested mask does not require every bit. These helpers do not validate unrelated bits introduced through casts.

```cpp
constexpr auto trackAndThumb = FlowUi::FSEL::SliderVisualParts::Track |
                               FlowUi::FSEL::SliderVisualParts::Thumb;
static_assert(FlowUi::FSEL::hasSliderVisualPart(
    trackAndThumb, FlowUi::FSEL::SliderVisualParts::Thumb));
static_assert((trackAndThumb & FlowUi::FSEL::SliderVisualParts::Fill) ==
              FlowUi::FSEL::SliderVisualParts::None);
```

## Definition hooks

Standard element tags expose the same `buildElement`/`constructElement`, interaction and logic hooks described by [element concepts](docs/api/elements.md). The framework supplies their callback-scoped contexts and frame state. Application code draws through a builder; each tag's public header documents its supported hooks, exact qualifiers, parameters, state and optional semantic parts. Internal numeric, text and pointer helpers under `FSEL::detail` implement those hooks.

The core frame bridge and input authoring operations return their values directly. For retained theme/resource or named-identity Results, standard elements use the public `FlowUi::ValueOrThrow` convenience when no local fallback is selected. Application elements can use the same API without private FlowUi headers. See [result handling](docs/api/result_handling.md) and [implemented boundaries](ResultBoundaryMigration.md).
