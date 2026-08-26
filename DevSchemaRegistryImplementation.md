# DevSchemaRegistry implementation design

## 1. Purpose and boundary

This report specifies the second functional part of `DevRegistryTooling.md`: the
development schema system that describes Flow element `Parameters`, `State`, and
`Resources`, plus user and library theme structs.

This phase is responsible for:

- declaring the fields and semantics of C++ types;
- discovering Flow element and theme schema roots;
- recursively resolving nested registered structs;
- classifying every field by its capture and editor capability;
- validating and publishing an immutable schema generation;
- retaining typed capture and future application operations without erasing C++
  safety;
- reporting unsupported, read-only, truncated, or conflicting schema portions.

It does **not** define the override store, the live-edit transaction queue, UI
widgets, persistence, undo, or replay. Those systems will consume the contracts
defined here later.

The implementation should be guarded by `FLOW_UI_DEV_MODE`. A production build
must not retain descriptor strings, registrars, schema vectors, adapters, or
capture/application trampolines.

---

## 2. Decisions

The implementation should proceed with these choices:

1. **Schema declarations are compile-time data.** A field macro or `consteval`
   customization produces a typed descriptor. It does not mutate a singleton
   during static initialization.
2. **Registration is a deliberate runtime ingestion step.** A registrar starts
   at an element, theme, or explicitly catalogued struct and recursively converts
   compile-time descriptors into normalized runtime records.
3. **Published schema shape is immutable.** Discovery occurs in a mutable
   builder. Consumers only see a complete `DevSchemaGeneration`.
4. **Elements and themes are separate root roles.** Both use the same recursive
   struct registrar, but `DevElementSchema` and `DevThemeSchema` remain separate
   records. A C++ type is not globally marked “theme” merely because one theme
   registration uses it.
5. **Elements are automatically discovered when used.** The dev-only element
   invocation path calls `ensureElement<Element>()`. An explicit catalogue is
   still recommended for deterministic startup validation and for elements not
   built in the current run.
6. **Themes are automatically discovered when registered.** The dev-only branch
   of `ThemeManager::registerTheme<T>` calls `ensureTheme<T>()`. Theme variants
   remain owned by `ThemeManager`; the schema registry owns only their type
   description and role metadata.
7. **Struct fields must be described explicitly.** Standard C++ currently cannot
   enumerate arbitrary members. A lean macro is the default; an ADL `consteval`
   function is the macro-free contract. Generated reflection may target the same
   API later.
8. **Nested objects are recursive and bounded.** The default maximum schema
   depth is 16. Type count, field count, enum value count, and string bytes also
   have development capacity limits.
9. **Editor capability is computed, not guessed by the UI.** Every retained type
   and field says whether it is editable, partially editable, view-only,
   metadata-only, hidden, or unsupported, and why.
10. **Only explicit semantic adapters make opaque library values editable.**
    `ActionCall` may use an action chooser and `TextureRef` may use a resource
    chooser. This does not imply support for arbitrary function pointers, raw
    pointers, handles, or unions.
11. **Typed member pointers stay in template-generated functions.** Do not copy a
    member pointer into bytes and reconstruct it later as the current registry
    does.
12. **Schema IDs in this phase are build-local identities.** They are derived
    from the complete C++ type token and field path and collision-checked against
    the original strings. They are sufficient for live tooling. A later
    persistence design must add explicit stable keys and migration aliases rather
    than pretending compiler type names are durable storage IDs.

---

## 3. Separate declaration, ingestion, and retained shape

“Registration” currently describes several different operations. They should be
made distinct.

### 3.1 Declaration layer

A declaration is static typed metadata associated with a C++ type:

```cpp
template <typename T>
struct DevSchemaTag {};

consteval auto flowUiDevSchema(DevSchemaTag<CardStyle>) {
    return devStruct(
        "CardStyle",
        devField<&CardStyle::padding>("padding"),
        devField<&CardStyle::accent>(
            "accent", DevFieldOptions{}.hint("Card accent color")));
}
```

The result contains string views into literals, member pointers as non-type
template parameters, option records, and references to type adapters. It owns no
heap memory and performs no global registration.

### 3.2 Registrar/ingestion layer

`DevSchemaRegistrar` accepts roots and recursively records them into a mutable
`DevSchemaBuilder`:

```cpp
registrar.ensureStruct<CardStyle>();
registrar.ensureElement<Card>();
registrar.ensureTheme<AppTheme>();
```

This is where duplicate detection, recursive resolution, capability analysis,
budget enforcement, and diagnostics happen. The same registrar implementation
serves all three calls; only their root records differ.

### 3.3 Retained schema layer

After successful ingestion, the builder normalizes its data and publishes one
`DevSchemaGeneration`. This is the compact, immutable data used by inspectors,
value capture, and eventually interface generation.

This separation matters for themes in particular:

- `flowUiDevSchema(DevSchemaTag<T>)` describes the struct shape;
- `ensureTheme<T>()` records that the type is used as a theme;
- `ThemeManager` continues to own named variants and active selection;
- a later theme capture source reads a variant through the manager and the
  retained typed operations.

The registry must not copy theme objects merely to “register” them.

---

## 4. Compile-time schema declaration API

## 4.1 Primary customization: ADL `consteval`

The canonical customization is a free `consteval` function found by argument
dependent lookup:

```cpp
namespace app {

struct Typography {
    float bodySize = 14.0f;
    float headingSize = 24.0f;
};

consteval auto flowUiDevSchema(FlowUi::devMode::DevSchemaTag<Typography>) {
    using namespace FlowUi::devMode;
    return devStruct(
        "Typography",
        devField<&Typography::bodySize>(
            "bodySize",
            DevFieldOptions{}
                .hint("Body text size in pixels")
                .numericRange(8.0, 36.0)
                .step(1.0)),
        devField<&Typography::headingSize>(
            "headingSize",
            DevFieldOptions{}
                .numericRange(12.0, 72.0)
                .step(1.0)));
}

} // namespace app
```

This contract is preferable to requiring metadata inside the struct. It works
for plain application types, keeps dev-only declarations outside production data
layout, and permits descriptors in a separate dev header.

For third-party types whose namespace cannot be extended, FlowUi provides a
library-owned specialization point:

```cpp
template <typename T>
struct DevTypeAdapter; // built-ins and deliberate external integrations only
```

User-defined struct schemas should use the ADL contract. `DevTypeAdapter` should
not become an escape hatch for blanket pointer support.

## 4.2 Lean macros

Macros remove descriptor boilerplate but expand to the same `consteval`
customization. They must not create static registrar objects.

```cpp
#define FLOWUI_DEV_FIELD(TYPE, MEMBER, ...) \
    ::FlowUi::devMode::devField<&TYPE::MEMBER>(#MEMBER __VA_OPT__(,) __VA_ARGS__)

#define FLOWUI_DEV_SCHEMA(TYPE, ...) \
    inline consteval auto flowUiDevSchema( \
        ::FlowUi::devMode::DevSchemaTag<TYPE>) { \
        return ::FlowUi::devMode::devStruct(#TYPE __VA_OPT__(,) __VA_ARGS__); \
    }
```

Typical user code stays small:

```cpp
struct CardStyle {
    Clay_Padding padding{};
    Clay_Color accent{};
    std::string label{};
};

FLOWUI_DEV_SCHEMA(
    CardStyle,
    FLOWUI_DEV_FIELD(CardStyle, padding),
    FLOWUI_DEV_FIELD(
        CardStyle,
        accent,
        FlowUi::devMode::hint("Card accent color")),
    FLOWUI_DEV_FIELD(
        CardStyle,
        label,
        FlowUi::devMode::textLimit(80)))
```

The member name and C++ member type supply the normal case. The only optional
user data is a hint and typed constraints. Display-name overrides, grouping, and
advanced editor selection may exist, but should not be required.

Macro declarations disappear entirely when `FLOW_UI_DEV_MODE == 0`:

```cpp
#if FLOW_UI_DEV_MODE
    #define FLOWUI_DEV_SCHEMA(...) /* descriptor definition */
    #define FLOWUI_DEV_FIELD(...)  /* typed field descriptor */
#else
    #define FLOWUI_DEV_SCHEMA(...)
    #define FLOWUI_DEV_FIELD(...)
#endif
```

## 4.3 Intrusive alternative

An element or struct may instead expose an intrusive static function:

```cpp
struct InspectorTheme {
    Clay_Color background{};

#if FLOW_UI_DEV_MODE
    static consteval auto devSchema() {
        using namespace FlowUi::devMode;
        return devStruct(
            "InspectorTheme",
            devField<&InspectorTheme::background>("background"));
    }
#endif
};
```

Descriptor lookup order should be:

1. ADL `flowUiDevSchema(DevSchemaTag<T>)`;
2. `T::devSchema()`;
3. a built-in `DevTypeAdapter<T>`;
4. automatic scalar/enum/container classification;
5. retained opaque unsupported type.

Defining more than one user schema source for the same type is an error, not a
priority-based silent override. The lookup order is for category selection; ADL
and intrusive declarations must be diagnosed if both exist.

## 4.4 What cannot be automatic

Without standardized static reflection, C++ cannot discover that `CardStyle`
contains `padding`, `accent`, and `label`. The viable options are:

| Option | Benefit | Cost |
|---|---|---|
| Lean field macro | Works now; good source diagnostics; no build tool | Member list is explicit |
| Handwritten `consteval` function | Macro-free and fully typed | More syntax |
| Generated descriptor | Almost no user boilerplate | Parser/build integration and generated-file lifecycle |
| Static-initializer registrar | Looks automatic | Order/linker hazards and mutable startup side effects |

The first two should ship now. A generator can emit the exact same descriptor
functions later. Static-initializer registration should be removed rather than
expanded.

---

## 5. Element and theme root registration

## 5.1 Elements

The element schema does not repeat field declarations. It derives the component
types from the existing Flow element concepts:

```cpp
template <FlowElement Element>
consteval auto makeElementRootDescriptor() {
    return DevStaticElementDescriptor{
        .definitionId = Element::definitionId,
        .definitionType = devTypeRef<Element>(),
        .parametersType = devTypeRef<ParametersOf<Element>>(),
        .stateType = optionalStateType<Element>(),
        .resourcesType = optionalResourcesType<Element>(),
        .presentation = elementPresentation<Element>(),
    };
}
```

`Parameters`, `State`, and `Resources` therefore cannot drift between the
element contract and dev metadata. `NoElementParameters` is retained as an empty
object schema. Absent `State` and `Resources` are represented by invalid type
indices, not by fake empty structs.

Element presentation metadata is optional:

```cpp
FLOWUI_DEV_ELEMENT_INFO(Button, "Button", "Activates one ActionCall")
```

This macro describes the element name/hint only. It is not necessary for the
element to be captured or registered.

The dev-only element invocation path should contain:

```cpp
template <FlowElement Element>
ElementInvocation<Element>::ElementInvocation(/* ... */) {
#if FLOW_UI_DEV_MODE
    uiManager.devSchemas().ensureElement<Element>();
#endif
    // Existing invocation setup...
}
```

`ensureElement` should be idempotent and inexpensive after the first lookup. It
queues an unseen root for ingestion at the next schema publication safe point;
it does not mutate the published generation while a frame inspector is reading
it.

## 5.2 Themes

Themes need their own registrar entry and retained role record even though their
shape is a normal struct schema:

```cpp
template <typename Theme>
void DevSchemaRegistrar::ensureTheme(DevThemePresentation presentation = {});
```

The integration point is already naturally typed:

```cpp
template <typename T>
Status ThemeManager::registerTheme(
    std::string_view variantName,
    T themeData,
    bool makeActive) {
#if FLOW_UI_DEV_MODE
    devSchemas_->ensureTheme<T>();
#endif
    // Existing ThemeStorageController registration...
}
```

The exact call may live in `ThemeStorageController::registerThemeVariant<T>` if
that is the only location with access to the dev service, but it should occur
before moving `themeData` and should not take the controller mutex while schema
publication occurs.

Optional theme presentation is separate from struct shape:

```cpp
FLOWUI_DEV_THEME_INFO(
    AppTheme,
    "Application theme",
    "Design tokens shared by application elements")
```

`DevThemeSchema` retains the theme type and presentation. Dynamic variant name,
active state, storage handle, and revision belong to a later
`DevThemeBindingSnapshot`, sourced from `ThemeManager`. They must not be mixed
into the immutable type graph.

## 5.3 Explicit catalogue

Automatic discovery covers executed elements and registered themes. It does not
prove at startup that every schema in the application is valid. Tests and dev
applications should be able to provide a catalogue:

```cpp
inline constexpr auto ApplicationDevCatalogue = FlowUi::devMode::devCatalogue(
    FlowUi::devMode::elements<Button, Slider, SettingsPanel>(),
    FlowUi::devMode::themes<AppTheme, InspectorTheme>(),
    FlowUi::devMode::structs<CardStyle>());

app.dev().schemas().ingest(ApplicationDevCatalogue);
```

The catalogue is constexpr data, not a static side effect. It gives deterministic
coverage, reliable static-library retention, and one place for schema validation
tests. Runtime `ensure...` remains as a safety net and as the ergonomic default
during development.

## 5.4 Registration strategy tradeoff and final choice

| Strategy | When registration occurs | Result |
|---|---|---|
| Namespace static registrar | Before `main`, in linker-dependent order | Reject; repeats the current order and dead-stripping problems |
| Compile-time descriptor only | During template/constant evaluation | Necessary for shape, but cannot by itself populate a runtime inspector |
| Explicit constexpr catalogue | Deliberate dev initialization | Recommended completeness and validation path |
| Runtime `ensureElement/ensureTheme` | First element use/theme registration | Recommended automatic fallback for excellent DX |
| Source-generated catalogue | Build generation step | Compatible future convenience |

The final choice is therefore a hybrid with sharply separated responsibilities:
compile-time descriptors define facts, the explicit catalogue supplies known
roots, and first-use `ensure...` catches uncatalogued roots. Both ingestion paths
produce the same normalized records and diagnostics. There is no hidden global
constructor path.

---

## 6. Recursive type resolution

## 6.1 Resolution state

The mutable builder tracks each `DevTypeId` in one of four states:

```cpp
enum class DevResolutionState : uint8_t {
    Unseen,
    Visiting,
    Complete,
    Failed,
};
```

For each root:

1. classify the C++ type;
2. allocate a provisional type entry and mark it `Visiting`;
3. resolve each member type or adapter dependency;
4. compute capture/editor capability bottom-up;
5. validate constraints and duplicate field names;
6. mark the type `Complete`;
7. retain a diagnostic-bearing opaque record instead of aborting the whole
   registry for unsupported leaves;
8. reject publication only for ambiguous/conflicting identity or structurally
   invalid descriptors.

A `Visiting` edge indicates recursion. Direct by-value recursive structs are not
valid C++ objects, so practical cycles arrive through pointers, reference-like
wrappers, variants, or semantic adapters. Raw pointers stop recursion and become
metadata-only or unsupported. A deliberate reference adapter must state how it
handles cycles and capture depth.

## 6.2 Limits

Recommended initial capacities per schema generation:

```cpp
struct DevSchemaLimits {
    uint16_t maxDepth = 16;
    uint32_t maxTypes = 2048;
    uint32_t maxFields = 16384;
    uint32_t maxEnumValues = 8192;
    uint32_t maxConstraints = 16384;
    uint32_t maxStringBytes = 2 * 1024 * 1024;
};
```

These are validation/capacity limits, not silent clipping rules. When a limit is
reached, preserve already valid types, record which path exceeded the limit, and
mark that branch `UnsupportedBudget`. A conflicting type definition remains a
publication failure because consumers cannot safely choose between two shapes.

Depth is counted through compound schema edges. Scalar and semantic leaf adapters
do not add depth. Runtime value capture will have separate depth and sequence-item
budgets because a bounded schema can still describe an unbounded vector.

## 6.3 Supported generic categories

Initial automatic classification should cover:

- `bool`;
- signed and unsigned integral types;
- floating-point types;
- `std::string` and safe string value types;
- any registered enum, regardless of underlying integer width;
- registered structs;
- `std::optional<T>`;
- fixed-size `std::array<T, N>`;
- bounded capture views of `std::vector<T>` and equivalent approved sequences;
- approved FlowUi/Clay value adapters.

Pointers, member-function pointers, arbitrary callables, unions, raw byte blobs,
and unknown containers are not recursively traversed by default.

Enums should retain their full underlying representation:

```cpp
struct DevEnumValueSchema {
    DevStringRef name;
    uint64_t bits;
};

struct DevEnumShape {
    uint8_t widthBytes;
    bool isSigned;
    DevRange32 values;
};
```

This replaces the current `uint8_t` restriction and preserves negative values
without narrowing.

---

## 7. Editor semantics, hints, and constraints

## 7.1 Capability is explicit

Schema registration should answer two related questions:

1. Can this value be captured meaningfully?
2. Can an interface editor safely produce a replacement?

```cpp
enum class DevCaptureCapability : uint8_t {
    None,
    MetadataOnly,
    Value,
};

enum class DevEditCapability : uint8_t {
    Hidden,
    Unsupported,
    ViewOnly,
    Editable,
    PartiallyEditable,
    SemanticCommand,
};

enum class DevEditorKind : uint8_t {
    None,
    Toggle,
    SignedNumber,
    UnsignedNumber,
    FloatingNumber,
    Text,
    EnumChoice,
    Flags,
    Color,
    Vector,
    Spacing,
    Sizing,
    ObjectGroup,
    OptionalGroup,
    Sequence,
    ActionChoice,
    ResourceChoice,
    Custom,
};
```

An object editor is constructed by stacking its field editors. A nested object is
fully editable only when all visible fields are editable. It is partially
editable when some are view-only or unsupported; the UI still shows the useful
subtree and explains unavailable leaves. One unsupported member must not erase
all value from an otherwise serviceable compound type.

The retained reason should be machine-readable and displayable:

```cpp
enum class DevCapabilityReason : uint16_t {
    None,
    FieldMarkedReadOnly,
    RoleDefaultReadOnly,
    NoCaptureAdapter,
    NoEditAdapter,
    RawPointer,
    CallableType,
    RecursiveCycle,
    ConstraintMismatch,
    CapacityExceeded,
};
```

## 7.2 Role policy and field policy

Type shape must not hard-code one mutability policy. The same `Spacing` struct
could occur in element parameters, state, resources, and a theme.

The effective policy is computed from:

```text
type adapter capability
    intersect field declaration policy
    intersect root-role policy
    intersect future live-instance policy
```

Initial role defaults should be:

| Role | Capture | Edit declaration |
|---|---|---|
| Element parameters | Value | Editable when adapter permits |
| Element state | Value | View-only by default; opt-in editable |
| Element resources | Value/metadata | View-only unless a semantic adapter explicitly permits selection |
| Theme | Value | Editable when adapter permits |

This is schema policy only. A later mutation system may further deny an edit for
a particular instance or frame.

## 7.3 Hints and constraints

Keep the common declaration lean. Options are typed clauses rather than an
unstructured property bag:

```cpp
struct DevFieldOptions {
    DevStringLiteral hint{};
    DevFieldAccess access = DevFieldAccess::Inherit;
    DevEditorOverride editor{};
    DevConstraintPack constraints{};
};
```

Initial constraints:

- numeric minimum and maximum, inclusive/exclusive endpoints, step, unit, and
  finite-only;
- text maximum bytes and/or Unicode scalar count, single/multiline, and optional
  validator identifier;
- enum allowed subset and flags semantics;
- sequence maximum editable items;
- optional display precision for floating point;
- group label/collapsed-by-default as presentation hints.

Examples:

```cpp
devField<&SearchParams::query>(
    "query",
    DevFieldOptions{}
        .hint("Visible search query")
        .maxCharacters(120)
        .singleLine());

devField<&SliderParams::value>(
    "value",
    DevFieldOptions{}
        .numericRange(0.0f, 1.0f)
        .step(0.01f)
        .displayPrecision(2));
```

The descriptor builders are `consteval` and type-aware. `maxCharacters` on a
number or `numericRange` on a struct should fail compilation at the declaration
site. Runtime ingestion repeats validation defensively for descriptors produced
by generated or compatibility paths.

Constraints guide and validate editors; they do not imply a mutation policy.
Whether out-of-domain input is rejected or clamped should be an explicit future
application choice. The schema should default to rejection so dev edits do not
silently change meaning.

---

## 8. Library-specific semantic adapters

Compound traversal is appropriate for plain data. Some FlowUi values have useful
semantics that are not their physical representation.

## 8.1 `ActionCall`

`ActionCall` is a discriminated app/UI action value, not a generic function
pointer. Its adapter should:

- capture kind, stable action identity where available, debug name, availability,
  and source/retention properties;
- expose `DevEditorKind::ActionChoice` only when the owning action system can list
  compatible choices;
- treat frame-scoped `UiActionCall` retention rules explicitly;
- use semantic replacement operations rather than exposing inline payload bytes.

No adapter should be registered for arbitrary callbacks, lambdas, `std::function`,
or function pointers merely because `ActionCall` has an editor.

## 8.2 `TextureRef` and resource identities

`TextureRef` should be captured as a logical resource reference plus editable
presentation fields that are safe to expose. Manager-owned source dimensions and
availability signals remain read-only. If a resource catalogue can enumerate
compatible textures, the logical identity can use `ResourceChoice`.

Likewise, `ResourceKey`, font identities, icon identities, and handles require
separate deliberate adapters. A raw storage pointer or packed handle is never
made editable through a generic pointer/integer editor.

## 8.3 Clay value types

Built-in adapters should describe common Clay values semantically:

- `Clay_Color` as one color editor rather than four unrelated floats;
- `Clay_Padding` as a spacing group;
- `Clay_CornerRadius` as a corner group;
- sizing and axis enums with enum choices;
- vector/dimension values as labeled numeric compounds.

Their component constraints still apply. For example, color channels can retain
the numeric domain used by FlowUi while the editor renders one color control.

Adapters should declare their child structure through the same normalized schema
model so generic inspection, paths, and capability aggregation continue to work.

---

## 9. Typed capture and future application trampolines

The current registry stores member pointers as `std::vector<std::byte>` and later
reconstructs them. Schema v2 should instead encode the member pointer in a
template instantiation and retain ordinary function pointers.

## 9.1 Operation records

```cpp
struct DevTypeOps {
    DevCaptureStatus (*capture)(
        const void* value,
        DevValueWriter& writer,
        DevCaptureContext& context) noexcept;

    DevDecodeStatus (*decodeReplacement)(
        const DevValueView& input,
        DevOwnedValue& output,
        DevDecodeContext& context) noexcept;

    void (*destroyOwned)(DevOwnedValue& value) noexcept;
};

struct DevFieldOps {
    DevCaptureStatus (*captureMember)(
        const void* owner,
        DevValueWriter& writer,
        DevCaptureContext& context) noexcept;

    DevApplyStatus (*applyReplacementToDraft)(
        void* draftOwner,
        const DevValueView& input,
        DevApplyContext& context) noexcept;
};
```

`DevOwnedValue` is a later mutation-layer staging object with small-buffer or
arena storage and typed destruction. Its ABI is declared now so schemas do not
need redesign, but the schema registrar does not create overrides.

## 9.2 Field generation

```cpp
template <auto Member>
struct DevMemberOps {
    using Traits = MemberPointerTraits<decltype(Member)>;
    using Owner = typename Traits::Owner;
    using Value = typename Traits::Value;

    static DevCaptureStatus capture(
        const void* owner,
        DevValueWriter& writer,
        DevCaptureContext& context) noexcept {
        if (owner == nullptr) return DevCaptureStatus::NullOwner;
        const auto& typedOwner = *static_cast<const Owner*>(owner);
        return captureDevValue<Value>(typedOwner.*Member, writer, context);
    }

    static DevApplyStatus applyToDraft(
        void* owner,
        const DevValueView& input,
        DevApplyContext& context) noexcept {
        if (owner == nullptr) return DevApplyStatus::NullOwner;
        auto& typedOwner = *static_cast<Owner*>(owner);
        return decodeAndAssign<Value>(typedOwner.*Member, input, context);
    }

    static constexpr DevFieldOps operations{
        .captureMember = &capture,
        .applyReplacementToDraft = editableValue<Value>
            ? &applyToDraft
            : nullptr,
    };
};
```

There is no offset calculation, no `reinterpret_cast` of serialized member
pointer bytes, and no assumption that a member pointer has pointer size.

## 9.3 Capture behavior

Capture writes a recursive value arena whose nodes reference the immutable schema
generation. A value node needs only its schema type index, kind-specific payload,
and child range. Field names, hints, constraints, and editor kinds remain in the
schema and are not copied per instance.

Capture must:

- accept `const void*` only;
- be bounded by value depth, sequence count, and copied-string budgets;
- return status rather than throw through a frame build;
- redact fields later marked sensitive;
- preserve unsupported children as diagnostic nodes where useful;
- never retain pointers into ephemeral parameter objects after the capture call.

## 9.4 Application contract retained now, executed later

The application trampoline must never mean “write arbitrary bytes to the live
object.” Its intended later use is:

1. validate the incoming value tree against schema type and constraints;
2. decode into a typed owned replacement;
3. copy or construct an owning draft of the correct root object;
4. call typed field operations on that draft;
5. submit the complete draft to the role-specific safe point;
6. atomically accept it or discard it with no partial live mutation.

For element parameters, the destination will be an override object. For state,
the element state manager must control when an opted-in mutation is committed.
For resources, most operations are view-only or semantic selections. For themes,
the final commit should use the existing queued `ThemeManager` mutation path and
its rollback behavior.

Keeping a nullable typed application operation in the schema now accurately
distinguishes “capturable” from “editable” without prematurely implementing the
override system.

---

## 10. Retained runtime records

Published records should be plain, relocatable metadata containing indices and
string references. Builder-only maps and descriptor templates must not leak into
the consumer API.

These vectors are owned directly by `DevSchemaRegistry` in the dev tooling
subsystem. They do not use `StorageSystem`: schemas are process-local developer
metadata, compile out of production, and have generation lifetimes unlike UI
resources. Their allocated size and builder peak still feed
`DevMonitoringAndReporting`.

```cpp
using DevTypeId = uint64_t;
using DevFieldId = uint64_t;
using DevSchemaGenerationId = uint64_t;

struct DevTypeSchema {
    DevTypeId id{};
    DevStringRef displayName{};
    DevStringRef cppTypeName{};
    DevTypeKind kind{};
    DevCaptureCapability capture{};
    DevEditCapability edit{};
    DevEditorKind editor{};
    DevCapabilityReason reason{};
    DevRange32 fields{};
    DevRange32 enumValues{};
    DevTypeIndex elementType{};       // optional/sequence element
    DevConstraintIndex constraints{};
    DevTypeOpsIndex operations{};
    uint32_t size{};
    uint32_t alignment{};
};

struct DevFieldSchema {
    DevFieldId id{};
    DevStringRef name{};
    DevStringRef displayName{};
    DevStringRef hint{};
    DevTypeIndex ownerType{};
    DevTypeIndex valueType{};
    DevFieldAccess declaredAccess{};
    DevEditCapability effectiveEdit{};
    DevCapabilityReason reason{};
    DevConstraintIndex constraints{};
    DevFieldOpsIndex operations{};
    uint32_t declarationOrder{};
    DevSourceLocation source{};
};

struct DevElementSchema {
    FlowDefinitionID definitionId{};
    DevTypeIndex definitionType{};
    DevTypeIndex parametersType{};
    DevTypeIndex stateType{};
    DevTypeIndex resourcesType{};
    DevStringRef displayName{};
    DevStringRef hint{};
};

struct DevThemeSchema {
    DevTypeIndex themeType{};
    DevStringRef displayName{};
    DevStringRef hint{};
};
```

`DevTypeIndex{}` needs a defined invalid sentinel; index zero can be a reserved
invalid record. `DevElementSchema` and `DevThemeSchema` are role edges into the
type graph, not subclasses of `DevTypeSchema`.

The generation owns dense tables:

```cpp
struct DevSchemaGeneration {
    DevSchemaGenerationId generation{};
    uint64_t fingerprint{};

    std::vector<char> strings{};
    std::vector<DevTypeSchema> types{};
    std::vector<DevFieldSchema> fields{};
    std::vector<DevEnumValueSchema> enumValues{};
    std::vector<DevConstraintRecord> constraints{};
    std::vector<DevElementSchema> elements{};
    std::vector<DevThemeSchema> themes{};
    std::vector<DevDiagnostic> diagnostics{};

    std::vector<DevIdIndex<DevTypeId, DevTypeIndex>> typeIndex{};
    std::vector<DevIdIndex<FlowDefinitionID, DevElementIndex>> elementIndex{};
    std::vector<DevIdIndex<DevTypeId, DevThemeIndex>> themeIndex{};

    std::vector<const DevTypeOps*> typeOperations{};
    std::vector<const DevFieldOps*> fieldOperations{};
};
```

The ID indices are sorted vectors and use binary search. Schema lookup is not a
per-node layout hot path, so this offers compact, deterministic storage without
publishing `unordered_map` allocation overhead. The mutable builder may use maps
for ingestion.

Strings are interned while building and copied once into the generation. Records
store offset/length references. Source paths may be normalized or stripped by a
dev privacy/build setting.

Function pointers refer to template-generated static operation tables in the
executable. They remain valid for the process lifetime and are excluded from the
canonical fingerprint.

## 10.1 Identity and fingerprinting

For this live-editing phase:

```text
DevTypeId  = hash(complete C++ type token)
DevFieldId = hash(DevTypeId, declared member name)
Theme key  = theme DevTypeId
Element key = existing FlowDefinitionID
```

The complete token/name is retained and compared whenever hashes match. A hash
collision is a registry error, never treated as equality.

The generation fingerprint hashes normalized semantic data: type tokens, field
names and order, kinds, child type IDs, enum values, constraints, capabilities,
element links, and theme links. It excludes addresses, vector capacities,
generation number, and source paths. Identical declarations discovered in a
different order must produce the same fingerprint.

Future persisted overrides will require explicit stable type/field keys. That is
deliberately separate from this implementation so the initial macros remain lean
and no accidental persistence promise is made.

---

## 11. Publication and lifetime

`DevSchemaRegistry` should own one published generation and one pending builder:

```cpp
class DevSchemaRegistry {
public:
    template <FlowElement E> void ensureElement();
    template <typename T> void ensureTheme();
    template <typename T> void ensureStruct();

    void publishPendingAtSafePoint();
    [[nodiscard]] DevSchemaView view() const noexcept;
};
```

Recommended lifecycle:

1. ingest FlowUi built-in adapters and built-in element/theme catalogue;
2. ingest the optional application catalogue during dev initialization;
3. publish generation 1;
4. queue late roots discovered by element invocation or theme registration;
5. resolve and publish them after the current UI/dev-reader phase;
6. retain the previous generation until no snapshot/view references it.

An immutable generation can be held by `std::shared_ptr<const
DevSchemaGeneration>` initially. If profiling shows meaningful atomic reference
cost, replace it with the dev system's frame-generation lifetime mechanism. Do
not expose references into a vector that may reallocate during late discovery.

Registration calls may originate from manager setup outside the UI thread. The
lightweight `ensure...` operation should queue a type-erased static root operation
under a small mutex. Recursive ingestion and publication happen on the dev
coordinator thread, outside `ThemeStorageController` and element-state locks.

## 11.1 Duplicate rules

- Repeating the same root and identical descriptor is idempotent.
- Two `FlowDefinitionID` values pointing to the same definition schema are
  idempotent only when all links and presentation metadata agree.
- Two different element definition types claiming one `FlowDefinitionID` are an
  error.
- Two struct descriptors for the same C++ type must normalize identically.
- Duplicate field names in one struct are errors.
- Duplicate enum names or duplicate numeric values are errors unless explicit
  enum aliases are supported later.
- The same struct used under multiple roles is valid and stored once.

Unsupported fields are not conflicts. They publish with a capability reason so
the inspector can explain them.

---

## 12. End-to-end examples

## 12.1 Nested element data

```cpp
namespace app {

struct LabelStyle {
    Clay_Color color{};
    float size = 14.0f;
};

struct CardParameters {
    std::string title{};
    LabelStyle titleStyle{};
    std::optional<uint32_t> maxLines{};
    FlowUi::ActionCall onActivate{};
};

struct CardState {
    bool expanded = false;
    uint32_t activationCount = 0;
};

struct CardResources {
    FlowUi::TextureRef disclosureIcon{};
};

struct Card {
    using Parameters = CardParameters;
    using State = CardState;
    using Resources = CardResources;

    static constexpr FlowUi::FlowDefinitionID definitionId{/* ... */};
    static void buildElement(FlowUi::ElementBuildContext<Card>&);
};

FLOWUI_DEV_SCHEMA(
    LabelStyle,
    FLOWUI_DEV_FIELD(LabelStyle, color),
    FLOWUI_DEV_FIELD(
        LabelStyle,
        size,
        FlowUi::devMode::numericRange(8.0f, 72.0f)))

FLOWUI_DEV_SCHEMA(
    CardParameters,
    FLOWUI_DEV_FIELD(
        CardParameters,
        title,
        FlowUi::devMode::textLimit(100)),
    FLOWUI_DEV_FIELD(CardParameters, titleStyle),
    FLOWUI_DEV_FIELD(CardParameters, maxLines),
    FLOWUI_DEV_FIELD(CardParameters, onActivate))

FLOWUI_DEV_SCHEMA(
    CardState,
    FLOWUI_DEV_FIELD(CardState, expanded),
    FLOWUI_DEV_FIELD(CardState, activationCount))

FLOWUI_DEV_SCHEMA(
    CardResources,
    FLOWUI_DEV_FIELD(CardResources, disclosureIcon))

FLOWUI_DEV_ELEMENT_INFO(Card, "Card", "Expandable application card")

} // namespace app
```

When the first `Card` is invoked, `ensureElement<Card>()` creates one element
record, resolves the three role types, then recursively resolves `LabelStyle`,
`optional<uint32_t>`, `ActionCall`, `TextureRef`, and the scalar/Clay adapters.

The expected editor result is:

```text
Card
  Parameters                          editable
    title                             text, max 100 characters
    titleStyle                        object group
      color                           color
      size                            number, 8..72
    maxLines                          optional + unsigned number
    onActivate                        action chooser
  State                               view-only by role default
    expanded                          boolean
    activationCount                   unsigned number
  Resources                           view-only/semantic
    disclosureIcon                    texture resource view/chooser
```

This hierarchy is derived from the schema graph. It is not stored as duplicated
per-element flattened UI rows.

## 12.2 Theme

```cpp
struct AppTheme {
    Clay_Color canvas{};
    LabelStyle labels{};
    float density = 1.0f;
};

FLOWUI_DEV_SCHEMA(
    AppTheme,
    FLOWUI_DEV_FIELD(AppTheme, canvas),
    FLOWUI_DEV_FIELD(AppTheme, labels),
    FLOWUI_DEV_FIELD(
        AppTheme,
        density,
        FlowUi::devMode::numericRange(0.75f, 2.0f)))

FLOWUI_DEV_THEME_INFO(AppTheme, "Application theme", "Application design tokens")

app.themes().registerTheme<AppTheme>("dark", darkTheme, true);
app.themes().registerTheme<AppTheme>("light", lightTheme, false);
```

The first registration ensures one `DevThemeSchema` and the recursively shared
`AppTheme`/`LabelStyle` type graph. The second adds no schema. The theme manager
still owns two live variant records. A later binding snapshot can expose `dark`,
`light`, active selection, and revisions without changing the immutable schema
generation.

## 12.3 Unsupported data remains informative

```cpp
struct ExperimentalParameters {
    float amount = 0.0f;
    void (*callback)(int) = nullptr;
    VendorOpaqueHandle handle{};
};
```

If all fields are declared, `amount` remains editable, `callback` is retained as
unsupported callable metadata, and `handle` is unsupported until a deliberate
adapter exists. The compound becomes `PartiallyEditable`; it does not disappear
from the interface.

---

## 13. Diagnostics

Registration diagnostics must carry the root role, recursive field path, source
location when available, relevant type token, and generation attempt. Required
initial diagnostics include:

- struct used as a root or nested object without a field schema;
- conflicting ADL and intrusive descriptors;
- duplicate/conflicting type, field, element, theme, or enum identity;
- two definition types claiming one `FlowDefinitionID`;
- invalid constraint for a field type;
- requested custom editor adapter not linked;
- capture adapter without a compatible schema kind;
- editable declaration without a decode/application operation;
- raw pointer, callable, unsafe union, or opaque type;
- recursive adapter cycle;
- schema depth or capacity exhaustion;
- late discovery and publication generation;
- type-ID hash collision with both complete type tokens;
- role policy downgrading a nominally editable field to view-only.

An unregistered compound type should publish as an opaque unsupported leaf in
interactive dev runs, allowing the rest of the inspector to work. The explicit
catalogue validation test should offer a strict mode that fails when any
reachable field lacks the intended capture/editor coverage.

---

## 14. Migration from the current registry

The current implementation can be replaced incrementally:

### Step 1 — introduce descriptor primitives

Add `DevSchemaTag`, `devStruct`, `devField`, typed option builders,
`DevTypeAdapter`, and descriptor-detection concepts. Add compile-time tests for
owner/member matching and incompatible constraints.

### Step 2 — add normalized generation storage

Implement `DevSchemaBuilder`, immutable `DevSchemaGeneration`, string interning,
sorted ID indices, fingerprints, diagnostics, and publication lifetime. Keep the
current `DevRegistry` available behind an adapter temporarily.

### Step 3 — add built-in type adapters

Implement scalars, strings, arbitrary-width enums, optionals, bounded sequences,
common Clay structs, Flow IDs, `ActionCall`, and resource reference adapters.
Each adapter needs capture tests and capability classification tests.

### Step 4 — replace field pointer byte storage

Generate `DevMemberOps<&Owner::member>` tables and route current capture through
them. Remove `memberPointerBytes`, `tryGetMemberPointer`, and the hardcoded
type-switch growth pattern once all required adapters exist.

### Step 5 — add element roots and automatic discovery

Implement `ensureElement<Element>()`, derive `Parameters`/`State`/`Resources`
links from the existing concepts, and call it from the dev-only element
invocation path. Add duplicate `FlowDefinitionID` tests.

### Step 6 — add theme roots and automatic discovery

Implement `ensureTheme<T>()` and `DevThemeSchema`; connect it to
`ThemeManager::registerTheme<T>` without publishing while holding theme storage
locks. Register `FlowUiTheme` through the built-in catalogue.

### Step 7 — provide compatibility macros

Temporarily redefine:

- `FLOWUI_DEV_REGISTER_STRUCT` to emit a compile-time schema descriptor;
- `FLOWUI_DEV_REGISTER_ELEMENT` to emit presentation metadata or a catalogue
  anchor;
- `FLOWUI_DEV_REGISTER_ENUM` to emit an enum adapter without the `uint8_t` limit.

Emit deprecation diagnostics for static-registration semantics. Compatibility
must not preserve `RegistryRegistrar` static constructors.

### Step 8 — add catalogue validation and measurements

Provide strict catalogue tests and report schema generation time, retained bytes,
builder peak bytes, counts by role/kind/capability, unsupported paths, and late
registrations to `DevMonitoringAndReporting`.

---

## 15. Verification plan

The implementation is complete when tests cover:

1. scalar, enum, string, optional, sequence, and nested object resolution;
2. recursion depth and every capacity limit;
3. identical duplicate coalescing and conflicting duplicate rejection;
4. full-width signed and unsigned enum values;
5. compile-time rejection of a field from the wrong owner;
6. compile-time rejection of incompatible constraints;
7. partial editability aggregation through multiple nested levels;
8. default role policy differences for parameters, state, resources, and theme;
9. semantic classification of `ActionCall`, `TextureRef`, and common Clay types;
10. explicit rejection of arbitrary function and raw pointers;
11. field capture through typed member operations, including multiple and virtual
    inheritance layouts where valid member pointers differ from raw offsets;
12. element alias inference and absent state/resources handling;
13. automatic first-use element discovery and idempotent repeat invocation;
14. automatic first-registration theme discovery and idempotent variant additions;
15. explicit catalogue startup validation;
16. deterministic fingerprints independent of registration order;
17. generation publication while an older reader remains alive;
18. no descriptor symbols or retained schema allocation in a production build;
19. timing and memory samples reported through development monitoring.

---

## 16. Resulting contract for later systems

After this phase, later live-edit tooling receives a stable and narrow contract:

- a Flow element definition links to exactly one parameters type and optional
  state/resources types;
- a theme registration links to exactly one normal struct type;
- every compound type links recursively to ordered field schemas;
- every field exposes capture capability, effective editor capability, semantic
  editor kind, hints, constraints, and a reason for any downgrade;
- every capturable field has a type-safe capture operation;
- every potentially editable field has a type-safe draft-application operation;
- immutable generations make captured values unambiguous while late types are
  discovered;
- unsupported functional or opaque data is visible and explained without
  weakening the safety of supported fields.

That is enough to build parameter, state, resource, and theme interfaces later
without embedding editor policy in tree capture or returning to a hardcoded list
of field types.
