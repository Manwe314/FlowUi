# Element System Implementation Rules

This file records the settled rules for the compile-time Flow element redesign so later implementation stages do not lose design context.

## Final element contract

1. Element behavior uses static functions. Element callback dispatch must not use runtime function-pointer fields.
2. Element definitions are plain user-authored structs satisfying a C++20 concept. They do not derive from an abstract class or a required convenience base.
3. The element value passed to `createElement()` is an empty constexpr tag. Enforce the empty/tag-like contract rather than accepting ignored per-object data.
4. Optional `Parameters`, `State`, and `Resources` capabilities are expressed by the presence of their aliases. An absent alias means the capability is absent.
5. Do not require `void` aliases or use `void` as the final capability representation.
6. Do not add public or internal `NoElementState`/`NoElementResources` placeholder types for absent final capabilities; handle absence with constrained APIs and compile-time branches.
7. Use concepts, `requires`, and `if constexpr` aggressively so missing capabilities and hooks produce no runtime storage, branches, or dispatch.
8. An element must define at least one of `buildElement` or `constructElement`; defining both is valid.
9. Preserve the existing hook names:
   - `onHovered`
   - `onPressed`
   - `onHeld`
   - `onReleased`
   - `runLogic`
   - `constructElement`
   - `buildElement`
10. Preserve the current hook signatures and authoring meaning as closely as possible. This migration changes dispatch, not what element hooks do.
11. Keep `ElementBuildContext` and `ElementInteractionContext` separate.
12. Continue using explicit `FLOW_DEF_ID(...)` definition identities for now.
13. Runtime/dynamic element registration is not planned and must not influence the core design.

## Builder and UI composition

1. Do not change the UI-building call shape. Code should continue to use:

   ```cpp
   ui.createElement(kElement, "logical/id")
       .setParameters(...)
       .draw();
   ```

2. `draw()` and `construct()` must share one common preparation and callback pipeline.
3. Their terminal behavior remains distinct: draw emits/closes the complete element flow, while construct leaves the constructed root open for child emission and later `drawConstructed()` closure.
4. Preserve `setParameters`, `mergeParams`, `withElementID`, draw options, dev capture behavior, and both string/`ResourceKey` creation paths unless a later stage explicitly changes them.

## Existing state/resource architecture

1. Keep the completed `ElementManager`, `ElementStorageController`, StorageSystem record layout, state identity, resource ownership, frame transactions, GC, and lifetime contracts.
2. State remains unique per `(WindowId, FlowElementId)`.
3. Resources remain one app-wide object per element definition.
4. State and resource C++ types remain declared by the element type, while payload ownership remains in StorageSystem.
5. Internal erased placement-construction/destruction thunks are storage operations, not element callback dispatch. They may remain internal where heterogeneous payload lifetime requires them.
6. Remove deprecated definition-owned state/resource storage and helper APIs as part of the staged migration; do not retain them as a shipped parallel source of truth.

## Staging

1. The repository owner will define implementation stages as work progresses; do not assume the stage sequence from `ElementSystemConceptsImplementation.md` is binding.
2. Do not choose between an atomic and incremental full-repository cutover in advance.
3. The definition traits, concepts, descriptors, typed invocation, and compile-time builder cutover are complete.
4. Later stages must preserve this typed user/runtime surface while migrating first-party consumers and adding convenience APIs.
5. Temporary compatibility required solely by the old builder has been removed; any remaining transition must name its separate follow-up boundary.

### Current builder-migration stage

1. The second implementation stage upgrades `ElementBuilder` in two explicit steps.
2. The first step introduced the internal typed `ElementInvocation<Element>` data pipeline and updated contexts around it.
3. The second step completed the `ElementBuilder<Element>` and `UiManager::createElement()` compile-time dispatch cutover.
4. `ElementInvocation<Element>` owns automatic definition registration, the invocation-scoped state lease, the single cached state pointer, and lazy app-wide resource caching.
5. Callback contexts reference the typed invocation instead of independently carrying erased state and resource cache fields.
6. The old five-parameter builder, stored definition pointer, callback-carrier `ElementDefinition`, and its legacy descriptor traits are removed.
7. `draw()` and `construct()` use one shared typed preparation/callback pipeline with compile-time terminal behavior.
8. First-party dev elements, type-only `ElementSet`, `template.hpp`, and broad API documentation remain separately staged follow-up work.

## Namespace and file-layout direction

1. Keep only actively relevant and configurable user-facing APIs in namespace `FlowUi`.
2. Put implementation details aggressively under `FlowUi::detail` or a more specific nested detail namespace.
3. The long-term file-layout rule is that internal/detail types belong under `include/internal`, while headers under `include/managers` should expose public `FlowUi` manager APIs.
4. Do not perform a broad file move merely to satisfy that layout during the current stage. Choose namespaces and dependencies now so a later move is mechanical.
