# DevInterface Initial Implementation Choices

## Scope of this increment

This increment establishes the developer interface as one buildable root element and its first navigation layer. The result remains deliberately limited to the ownership boundary, persistent state, dependency input, visual tokens, permanent chrome, and content-shell structure described in `devInterfaceArchitecture.md`.

Splitters, inspectors, most commands, undo/redo behavior, overlays, settings, and close controls remain future work. The main workspace still contains only stub content. Navigation, CPU-reporting selection, and baking are the deliberately narrow exceptions that now have real behavior.

## Window owner versus drawable interface root

There are now two types named `DevInterface` in distinct namespaces, with intentionally different responsibilities:

- `FlowUi::devSystems::DevInterface` remains the app-owned service that registers the global toggle shortcut and creates or destroys the dedicated window.
- `FlowUi::devSystems::interface_elements::DevInterface` is the singleton Flow element drawn at the root of that window. Interface state and future interface behavior belong here.

Keeping window lifecycle in the existing app-owned service avoids making an element responsible for creating the window in which it is currently being built. All UI orchestration below that lifecycle boundary is intended to remain on the element.

The developer window starts directly from `interface_elements::DevInterface`; there is no second root name or compatibility layer to maintain.

## Parameter contract

`DevInterfaceParameters` currently carries:

- `App* app`: the stable app-owned gateway to monitoring, tooling, managers, and future application-control operations.
- `WindowId interfaceWindowId`: the identity of the window hosting the interface.
- `WindowId mainWindowId`: the initial semantic application target.

The window service uses the typed managed-window configurator to populate these values. This makes the app dependency explicit at element invocation rather than obtaining it through global state. The struct remains default-constructible because that is required by the Flow element contract.

The root itself does not query monitoring or tooling. Implemented child surfaces now use the supplied app for window telemetry, CPU-reporting configuration, bake status, and bake execution instead of introducing separate global or singleton access paths. If the interface later proves to need a smaller, test-oriented dependency surface, this parameter struct is the seam where an interface facade can replace the raw app pointer.

## Persistent interface state

`DevInterfaceState` is the single state object for interface-wide decisions. The element declares `ElementStatePolicy::windowLifetime()`, so state is not subject to transient absent-frame collection while the developer window exists.

The initial state reserves the architecture's major categories:

- navigation and selection;
- application operational state and overlay/pin switches;
- unbaked-change and last-action reporting;
- interface self-monitoring values;
- selector and inspector widths.

This gives future child surfaces one shared orchestration source while keeping live app data out of persistent state. App-owned reports and catalogues should be queried through the parameter dependency; only user choices, interface layout, and UI-derived snapshots should be stored here.

The command object hierarchy and undo/redo stacks from the architecture report are intentionally not implemented in this increment. Their ownership and interaction with the existing override/bake command results should be designed when the first editable operation is connected, rather than committing the root shell to a parallel command model prematurely.

Closing and reopening the developer window creates a new root instance and therefore a fresh state object. Persistence beyond one developer-window lifetime is a separate policy decision and is not implied by singleton root ownership.

## Root build callback and layout

`DevInterface::buildElement` creates a top-to-bottom Clay container with `CLAY_SIZING_GROW(0)` in both dimensions and the Keel depth-zero background. It therefore consumes the complete content area offered by the existing managed-window root container.

The callback now emits four direct children:

1. A fixed 44 px permanent header with a depth-one Panel fill and a 1 px bottom hairline.
2. A fixed 40 px content header containing tabs and contextual controls.
3. A growable constructible content surface with the depth-zero Keel fill.
4. A fixed 32 px permanent footer with a depth-one Panel fill and a 1 px top hairline.

Each region has a stable local element ID. The architectural report described the content header as nested conceptually inside main content; making both the header and body direct root children preserves the same top-to-bottom geometry while giving each an explicit Flow element ownership boundary.

## Content header and workspace boundary

`DevContentHeader` is a closed drawable element. It owns the complete 40 px navigation and contextual-control row, so callers cannot accidentally add children inside the tab chrome. `DevContent` is deliberately construct-only: the root opens it, emits the current workspace stub, and closes it with `drawConstructed()`. That is the intended extension point for the selector, workbench, and inspector surfaces in the next increment.

The content header binds six adjacent FSEL `RadioChoice` elements to the root-owned `activeTab` selection in the required order: Inspect, Performance, Memory, Diagnostics, Changes, and Catalogue. The shared value defaults to Inspect and is normalized before every build, so an invalid or no-selection value cannot survive into rendering. Each choice occupies the full header height, highlights its background on hover, and uses a two-pixel `Current` accent bottom border only while selected. FSEL controls are explicitly marked as internal development capture, matching the permanent header policy.

`RadioChoice` intentionally accepts a `uint64_t` selection pointer. `DevInterfaceState::activeTab` therefore stores the enum's numeric value rather than using the enum type directly; this avoids aliasing or duplicate mirrored selection fields. The surrounding header is the validation boundary and converts the normalized value back to `DevInterfaceTab` for control dispatch.

An inset separator divides navigation from a growable contextual-control area. Each tab currently produces this deliberately shallow surface:

- Inspect: a button-shaped placeholder for the future two-section inspection dropdown and a Pick Element stub.
- Performance: a frame-selector placeholder and an FSEL combo box for CPU reporting detail. The combo exposes only levels compiled into the build and writes selection changes into the existing `DevTimingConfig` immediately.
- Memory: a `Captured State: None` reporter and Capture State stub.
- Diagnostics: an intentionally empty control area.
- Changes: a Bake Review stub and a functional `Bake X Changes` action.
- Catalogue: a Refresh stub.

The inspection and frame placeholders are intentionally rendered as button surfaces, not FSEL combo boxes. Their future behavior requires multi-section and frame-history models respectively, and representing either as a one-value combo box now would establish the wrong contract.

The bake action is a frame-local UI action bound to the supplied `App` and root state. It invokes the existing `DevTooling::bakeActiveEdits()` pipeline, refreshes live and bakeable counts from `queryBakeStatus()`, and reports success, no-op, or failure through the footer's last-action message. The button is disabled when no bakeable overrides exist. The header refreshes `unbakedChangeCount` from active live overrides each frame, which gives the existing footer reporter real data without making the footer depend directly on tooling.

## Three-area content backbone

The constructible `DevContent` container now lays out children left to right and contains five direct items: selector area, selector splitter, workbench area, inspector splitter, and inspector area. The three semantic regions are closed drawable elements—`DevSelectorArea`, `DevWorkbenchArea`, and `DevInspectorArea`—so their future tab-specific trees can grow without exposing their internal child composition to the root.

The selector and inspector widths remain owned by `DevInterfaceState`, starting at 280 px and 320 px. The root clamps them before layout and binds them directly to FSEL `SplitterHandle` controls:

- selector: trailing handle, 180–520 px;
- inspector: leading handle, 240–560 px;
- workbench: `CLAY_SIZING_GROW(0)`, consuming the space between them.

Each splitter has a one-pixel structural line inside a ten-pixel interaction target. Hover uses the Current accent and active dragging uses the selected-row surface. Both handles are explicitly excluded from application tree capture.

The area implementation contains the central `DevInterfaceTab` switch intended as the replacement boundary for real content. Every tab currently dispatches a distinct selector, workbench, and inspector stub label. Later increments can replace one case at a time with concrete elements while leaving the root layout, width ownership, and splitter behavior unchanged.

The root build callback opens the `flowui.dev_interface.build` CPU timing zone in the `DevTool` category with the `DevToolWork` role. Timing therefore flows through the existing per-thread recorder, frame context, reporting, and quality controls instead of writing a manually sampled duration into interface state. A development-only build-context accessor provides the callback with its active recorder while keeping the recorder private on `UiManager`. The zone is nested beneath the element system's existing invocation and generic build-callback zones, retaining both definition-level aggregation and a directly searchable developer-interface measurement.

## Theme token boundary

`DevTheme.hpp` translates the complete Teal-Ocean palette from the architecture report into named `Clay_Color` constants. Colors in the root shell use these tokens rather than local literals.

The token file contains the planned depth surfaces, border strengths, brand accents, text colors, and semantic status colors even though only a subset is used by this first shell. This creates one reviewable source of truth for later controls and prevents small color variations from accumulating while individual regions are implemented.

Typography resources are not selected yet. Current direct labels use font ID `0` so the shell remains buildable with the current application setup. Inter and JetBrains Mono should be resolved through the eventual interface font/resource contract before production text components are built.

## Permanent header element

`DevInterfaceHeader` is a separate closed build-callback element owned by the root. It retains the root shell's 44 px fixed height, depth-one background, 16 px horizontal padding, and 1 px bottom hairline, while keeping all header composition behind one element boundary.

Its content is laid out left to right:

1. The `FlowUi DevInterface` title.
2. A 1 px structural separator inset by 10 px at the top and bottom.
3. An FSEL combo box bound directly to `DevInterfaceState::selectedWindowId`.
4. Informational text for selected-window maximum frames in flight and rolling FPS, plus current application resident memory.
5. A transparent grow-width spacer.
6. A right-aligned action row containing Undo, History, Redo, and Configurations buttons.

The buttons are presentational stubs and intentionally have no actions. The combo box is functional only as interface selection state: it does not yet trigger overlay or inspection work.

The app now supplies a development-only `devWindowSnapshot()` containing stable window IDs, current native titles, and configured frames-in-flight. The header sorts by identity, excludes its own interface-window ID, prefers titles for labels, and falls back to an ID-derived label only when a title is empty. Display labels are capped at 24 UTF-8-safe bytes with an ASCII ellipsis so unusually long native titles cannot claim the rest of the header. If a selected window disappears, selection moves to the first remaining application window.

Rolling FPS comes from the selected window's existing `PerformanceDiagnostics`. The app frame lifecycle now begins diagnostics after calculating frame delta and publishes them only after a successfully completed presentation; previously this diagnostics object was never advanced and therefore always reported zero. Memory prefers current process resident bytes from `DevMemoryReporting` and formats them in decimal MB to make the value comparable with common system monitors. When the process sample is unavailable, it falls back to current per-source backing allocation. These are read-only displays and do not introduce duplicate monitoring state.

FSEL's combo-box trigger and label viewport now clip their painted content. `CLAY_TEXT_WRAP_NONE` controls wrapping but does not itself establish a clipping boundary, so relying on sizing alone allowed selected text to paint over adjacent header content. The generic FSEL fix protects every combo-box use; header-side truncation additionally keeps the displayed label legible rather than merely cutting it at an arbitrary pixel.

Every FSEL combo-box and button invocation explicitly calls `setDevInternalCapture(true)`. With the default `excludeInternalDevElementsFromCapture` policy, the controls and their nested popup/options are suppressed from the inspected Flow tree rather than appearing as application-authored UI.

## Permanent footer element

`DevInterfaceFooter` is a separate closed build-callback element owned by the root. It retains the 32 px fixed height, depth-one background, 12 px horizontal padding, and 1 px top hairline established by the shell.

Its content is laid out left to right:

1. An operational-state tag containing a green status dot and `RUNNING` text.
2. The same inset 1 px structural separator pattern used by the header.
3. An error reporter containing an icon and numeric count.
4. An unbaked-change reporter containing an icon and numeric count.
5. A second inset structural separator.
6. The last-action message.
7. A transparent grow-width spacer.
8. A right-aligned tag that distinguishes no unbaked changes from a non-zero count.

Operational-state detection is intentionally not connected. The current green `RUNNING` tag is hard-wired presentation; freeze detection can later switch it to the planned red `STOPPED` state without changing the footer parameter contract.

`DevInterfaceFooterParameters` accepts `errorCount`, `unbakedChangeCount`, and `lastActionMessage`. The root forwards the matching persistent interface-state fields. Error count remains an unwired zero-value stub; the content header now refreshes unbaked count from the bake pipeline and writes bake results into the action message. The footer itself still does not query or infer those systems.

`DevInterfaceIcons.hpp` is the dedicated semantic icon catalogue for this UI. It currently contains raw-string placeholder SVGs for error reporting and unbaked changes. `DevInterfaceFooterResources` registers those SVGs once with the app-owned icon manager and retains their texture references, keeping icon parsing and registration out of the per-frame callback. Final artwork can replace the raw SVG strings while preserving the keys and footer layout.

## Build and contract integration

The new source and public development headers are registered only inside the existing `FLOW_UI_DEV_MODE` CMake branch. Production builds continue to omit the developer interface implementation.

The multi-window API contract checks that the new root is drawable, has state, and resolves to the expected parameter and state types.

Verification performed for this increment:

- the `flowui` target builds in the existing `build-schema` development configuration;
- the release-configured `build_demo` target builds with the permanent header and FSEL controls;
- `DevContentHeader` and constructible `DevContent` compile into `flowui_fsel_demo`;
- `flowui_multi_window_api_contract_tests` builds and passes;
- `flowui_element_concept_contract_tests` builds and passes;
- `git diff --check` reports no whitespace errors.

## Intended next review boundary

The next structural increment can replace any one tab's three area stubs with its selector, workbench, and inspector elements. The custom inspection/frame dropdowns, memory capture model, bake-review surface, catalogue refresh, permanent-header action behavior, dedicated font resources, and richer telemetry styling remain separate review points. Stub controls deliberately do not manufacture temporary data models or commands.
