# Developer Systems

FlowUi's developer support lives in `devSystems`. It combines a dedicated developer-interface window with schema-driven inspection, typed overrides, resource catalogues, monitoring, and manifest-driven baking. The obsolete developer subsystem has been removed; current APIs do not provide compatibility aliases.

## Enable the systems

Compile the feature before adding FlowUi to your CMake project:

```cmake
set(FLOW_UI_DEV_MODE ON CACHE BOOL "" FORCE)
add_subdirectory(external/FlowUi)
```

Activate it before creating the application:

```cpp
FlowUi::AppConfig configuration{};
configuration.dev.enabled = true;
configuration.dev.excludeInternalDevElementsFromCapture = true;
configuration.dev.useShortcutManagerForPanelToggle = true;
configuration.dev.panelToggleChord.ctrl = true;
configuration.dev.panelToggleChord.shift = true;
configuration.dev.panelToggleChord.key = 68; // D in the GLFW backend

auto application = FlowUi::makeApplication(configuration);
```

The default toggle is Ctrl+Shift+D. `App` owns the modern `DevInterface`, its dedicated window, and synchronization at the platform safe point. Application code uses the regular frame loop; the interface is not an element drawn into the application's window. Creating the interface window requires the same presentation capabilities as other secondary windows.

`configuration.dev.monitoring` controls monitoring collection and retention. `configuration.dev.tooling` controls schema, override, and tree-capture capacities. `capture_timing_on_startup` optionally retains the first timing capture for later investigation. Compile-time feature support and runtime activation are separate choices.

## Describe editable values

Schema descriptors live in [DevSchemaDescriptor.hpp](../../include/devSystems/devTooling/schema/DevSchemaDescriptor.hpp), in `FlowUi::devSystems::tooling`. Put schema declarations in the same namespace as their C++ types so argument-dependent lookup can find them.

```cpp
#include <FlowUi/Public/Flow.hpp>
#include <devSystems/devTooling/schema/DevSchemaDescriptor.hpp>

#include <cstdint>

namespace dashboard {

enum class CardTone : std::uint8_t { Neutral, Warning, Success };

struct CardParameters {
    Clay_Sizing sizing{
        .width = CLAY_SIZING_FIXED(240.0f),
        .height = CLAY_SIZING_FIXED(100.0f),
    };
    CardTone tone = CardTone::Neutral;
    float corner_radius = 8.0f;
};

FLOWUI_DEV_ENUM_SCHEMA(
    CardTone,
    FLOWUI_DEV_ENUM_VALUE(CardTone, CardTone::Neutral),
    FLOWUI_DEV_ENUM_VALUE(CardTone, CardTone::Warning),
    FLOWUI_DEV_ENUM_VALUE(CardTone, CardTone::Success))

FLOWUI_DEV_SCHEMA(
    CardParameters,
    FLOWUI_DEV_FIELD(CardParameters, sizing),
    FLOWUI_DEV_FIELD(CardParameters, tone),
    FLOWUI_DEV_FIELD(
        CardParameters, corner_radius,
        FlowUi::devSystems::tooling::DevFieldOptions{}.numericRange(0.0, 32.0)))

struct Card {
    using Parameters = CardParameters;
    using BuildContext = FlowUi::ElementBuildContext<Card>;

    static constexpr auto definitionId = FlowUi::DefinitionID("dashboard.card");
    static constexpr std::string_view debugName = "Dashboard Card";

    static void buildElement(BuildContext& context) {
        Clay_ElementDeclaration root{};
        root.layout.sizing = context.params.sizing;
        root.cornerRadius = CLAY_CORNER_RADIUS(context.params.corner_radius);
        switch (context.params.tone) {
        case CardTone::Neutral: root.backgroundColor = FlowUi::Flow_Color("#354052ff"); break;
        case CardTone::Warning: root.backgroundColor = FlowUi::Flow_Color("#b87b20ff"); break;
        case CardTone::Success: root.backgroundColor = FlowUi::Flow_Color("#287c51ff"); break;
        }
        CLAY(context.clayID(), root) {}
    }
};

inline constexpr Card kCard{};
static_assert(FlowUi::FlowElement<Card>);

} // namespace dashboard
```

These are descriptor macros, not global mutable registration. With the feature disabled they expand away. `FLOWUI_DEV_FIELD` records source metadata; the underlying `devField`, `devFieldAt`, `devStruct`, and `devEnum` factories support explicit descriptors. A type can instead expose `static consteval auto devSchema()`, or a third-party type can use a `DevTypeAdapter`/`DevEnumAdapter`. Use one descriptor route per type.

Field options can mark a value read-only or hidden and supply numeric, text, choice, and editor constraints. Unsupported values retain explicit capability reasons instead of being presented as editable scalar data. Clay and FlowUi adapters are provided by [DevClayTypeAdapters.hpp](../../include/devSystems/devTooling/schema/DevClayTypeAdapters.hpp) and [DevFlowUiTypeAdapters.hpp](../../include/devSystems/devTooling/schema/DevFlowUiTypeAdapters.hpp).

## Publish schema and catalogue entries

Normal element invocation discovers its element schema; theme registration discovers theme schemas. Explicitly ingest a catalogue when types should be available before their first invocation, including offline definitions:

```cpp
#if FLOW_UI_DEV_MODE
namespace tooling = FlowUi::devSystems::tooling;
constexpr auto catalogue = tooling::devCatalogue(
    tooling::elements<dashboard::Card>(),
    tooling::themes<>(),
    tooling::structs<dashboard::CardParameters>());
application.devTooling().schemas().ingest(catalogue);
#endif
```

[DevSchemaRegistry](../../include/devSystems/devTooling/schema/DevSchemaRegistry.hpp) queues roots and publishes immutable schema generations at the application safe point. `schemas().view()` returns a generation-owning view; snapshots retain their corresponding schema rather than borrowing a mutable global registry.

`application.devTooling().catalogues()` exposes current element, theme, action, image, icon, font, and atlas metadata. Reflection discovery does not authorize arbitrary application element execution in the catalogue. A passive preview must be explicitly admitted with `registerElementPreview<Element>(set_name, true)` and must be safe to draw with default parameters.

## Inspect and edit

Build the application UI normally:

```cpp
application.ui().createElement(dashboard::kCard, "dashboard/summary")
    .setParameters({.tone = dashboard::CardTone::Warning})
    .draw();
```

The modern pipeline is:

1. The builder resolves a strong element identity, applies baked and live parameter overrides, invokes callbacks, and records the effective reflected values.
2. Each window publishes a `DevTreeSnapshot` with Flow/Clay structure and identity correlation. Internal developer elements are excluded by default.
3. The developer interface reads those snapshots, schema generations, catalogues, and monitoring data through modern services.
4. Edits enter `DevOverrideEngine` as typed `DevChangeSet` transactions. They are validated and committed at a safe point, with result statuses for unavailable schemas, invalid targets, read-only fields, type mismatches, and rejected constraints.

Definition overrides affect every matching element definition. Exact-instance overrides include window and stable instance identity. Explicit local, keyed, global, or semantic-part identities support durable targeting. Automatic callsite identities are useful for immediate UI but must not be assumed bakeable in every context.

Custom tooling can access `application.devTooling().overrides()`, `schemas()`, `catalogues()`, and `bakePipeline()`. A window's `UiManager` exposes `devTreeSnapshot()`, `devSchemaRegistry()`, and `devOverrideEngine()`. Use these modern services rather than maintaining a second capture or override runtime.

## Review, bake, and rebuild

The Changes interface reviews live edits and durable settings before saving. The canonical durable artifact is `.flowui/changes/active.flowchanges`, separate from agentic capture evidence. It contains typed element/theme entries, source/header provenance, generated C++ values, and supported capacity selections.

For a programmatic bake of current bakeable live edits:

```cpp
#if FLOW_UI_DEV_MODE
const auto result = application.devTooling().bakeActiveEdits();
if (!result.applied) {
    // Display or record result.status in the application's own tool UI.
}
#endif
```

For revision-checked review, use `bakePipeline().query_review()` and `commit_review()` with the reviewed plan. The interface uses this path to keep staged changes and revision checks consistent. Query `queryBakeStatus()` and `queryBakeDiff()` to inspect diagnostics and authored/live/baked values.

`FLOW_UI_ENABLE_BAKED_CHANGES` enables the [CMake baking integration](../../cmake/FlowUiBaking.cmake). Its generator reads the active manifest, validates entries, and produces `FlowUiBakedChanges.hpp`/`.cpp` in the build tree. Rebuild to compile durable changes into the application, including production builds. This workflow generates a typed overlay; it does not apply the removed JSON-to-source updater workflow or silently rewrite application source.

Use `imageData(TextureRef)` for frame-owned Clay image payloads, configure input line behavior with `TextFieldMode`, and traverse `FieldTextView` explicitly with `contiguous()`, `copy()`, or `forEachChunk()`. Current schemas and generated values use these same supported runtime contracts.

See [Developer Systems internals](../internals/dev_runtime.md), [agentic workflow](../agentic-workflow.md), and [revision-checked review and baking](../../include/devSystems/devAgenticWorkflow/knowledge/systems/changes.review_and_baking.md) for the corresponding service boundaries.
