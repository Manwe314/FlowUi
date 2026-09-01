# TypeEditor Shapes

## Purpose

This document defines the visual shape, editing behavior, composition rules, and transaction contract for the `DevTypeEditor` layer of the FlowUi Inspector.

It combines:

- the semantic types currently recognized by the new development schema;
- useful interaction patterns from the previous Dev Mode editors;
- the recursive `DevEditorCard -> DevEditingField -> DevEditor -> DevTypeEditor` architecture;
- types which need manager-owned catalogues, notably `ActionCall` and resources;
- generic fallbacks for primitives, enums, optionals, sequences, and application-defined structs.

This is a design specification. `DevTypeEditor` implementations remain free to use different internal elements as long as they preserve the behavior and hierarchy described here.

## 1. Place in the Inspector hierarchy

```text
DevEditorCard
├── DevEditorCardHeader
│   ├── Field name
│   ├── Collapsed quick value (semantic types only)
│   ├── Grow spacer
│   └── Collapse / Copy / Paste / Reset
└── DevEditingField
    ├── DevEditor
    │   ├── Optional nested field name
    │   ├── Type name
    │   └── DevTypeEditor
    └── DevEditorCard (when a nested field is itself a struct)
```

A `DevTypeEditor` edits exactly one schema value at one resolved field path. It does not own card-level collapse, copy, paste, or reset controls. A semantic editor may edit several primitive members together, but it still emits replacement values for the one semantic value it represents.

## 2. Shared visual language

All controls follow `devInterfaceArchitecture.md`.

- Working controls use the elevated surface color, `kDepth3Elevated`.
- Hover uses `kHoverSurface`.
- Normal control borders use `kBorderVisible`; dense internal separators use `kBorderSubtle`.
- Primary values use `kTextCanvas`; labels and type hints use `kTextSecondary`; unavailable metadata uses `kTextMuted`.
- Focus and selected choices use `kAccentSignalBlue`.
- A live, unbaked edit is marked with `kAccentSignalCoral`.
- A baked/current override may be marked with `kAccentSeaGlass`.
- Invalid input uses `kStatusRed`; warnings use `kStatusAmber`.
- Controls should normally be 24-28 px high. Dense multi-value editors may wrap to another row rather than forcing horizontal Inspector growth.
- Labels should not be repeated inside a semantic editor when the containing `DevEditor` or `DevEditorCardHeader` already establishes the field name.

### 2.1 Common control states

Every TypeEditor supports these presentation states:

| State | Presentation | Behavior |
|---|---|---|
| Editable | Normal value and border colors | Accepts input and can emit transactions |
| Hovered | Hover surface | No value mutation |
| Focused | Signal-blue focus border | Receives text/keyboard input |
| Dirty | Small coral indicator or coral leading border | Value differs through a live development override |
| Read-only | Muted value, normal background | Selectable/copyable when safe, never emits edits |
| Unsupported | Muted explanation | No fake or partial editing |
| Invalid draft | Red border and concise inline reason | Draft retained locally; no override emitted |
| Unavailable | `Unavailable` or a capability-specific message | No editor interaction |

## 3. Transaction and draft contract

All TypeEditors use the same mutation path.

1. Read the effective value from the current snapshot plus applied override.
2. Keep incomplete text or drag state as local UI draft state.
3. Validate and normalize the candidate using schema type operations and constraints.
4. Build a new owned value for the complete field, including untouched semantic members.
5. Submit one forward override command and its exact inverse as a development edit transaction.
6. Update the permanent footer with a concise action message.

### 3.1 Commit boundaries

- Toggle, enum, flag, resource, action, and preset selection commit immediately.
- Text and numeric typing commit on Enter or focus loss when valid.
- Escape cancels the local draft and restores the effective value.
- Dragging or sliding previews continuously, but the entire pointer-down to pointer-up gesture becomes one undo step.
- A semantic multi-value control emits the whole semantic object, not an isolated byte/member write.
- Switching an optional on/off or changing a tagged-union discriminator is one transaction.
- Adding, removing, or reordering a sequence item is one transaction.

Copy, paste, and reset remain card-level operations. Paste and reset replace the whole represented field and use the same transaction history as TypeEditor edits.

### 3.2 Coalescing

Repeated edits may coalesce only when all of these match:

- selected element instance;
- Inspector role (`Parameters`, `State`, or `Resources`);
- root owner type and complete nested field path;
- edit gesture identity;
- value type.

Typing in two different fields, changing an enum after typing, or beginning a second drag must create a new undo step.

### 3.3 Type safety

The editor must never reinterpret memory from display names. Dispatch uses schema `DevTypeId`, `DevTypeKind`, `DevEditorKind`, field constraints, and registered type operations. Display names are presentation only.

## 4. Dispatch order

Use the first matching rule:

1. A field-level explicit editor override.
2. A registered semantic type adapter.
3. `Optional`, wrapping the editor selected for its contained type.
4. Enumeration or flags metadata.
5. Primitive `DevTypeKind`.
6. Sequence metadata.
7. Registered object recursion through nested `DevEditorCard` elements.
8. Pointer/opaque/callable capability fallback.
9. Unsupported-type explanation.

This order permits an application to give a primitive field a semantic editor without changing its C++ storage type.

## 5. Primitive TypeEditors

### 5.1 Boolean / `Toggle`

**Shape:** a compact switch or two-state checkbox followed by `On`/`Off` when space permits.

**Interaction:** click or Space toggles immediately. The complete boolean value is committed as one transaction.

**Quick value:** `On` or `Off` when the enclosing card has explicitly opted into a semantic quick view. Generic primitive cards do not need a header quick view.

### 5.2 Signed integer / `SignedNumber`

**Shape:** a single-line numeric field. A subtle left-right drag affordance may occupy the type-label or field edge.

**Interaction:** accepts sign and decimal digits. Up/Down increments by the schema step; Shift and Alt provide coarse/fine stepping. Horizontal drag uses the configured integer rate.

**Validation:** enforce the exact C++ width and signed range, then schema minimum, maximum, and step. Overflow is invalid rather than clamped while typing. Dragging and stepping may clamp.

### 5.3 Unsigned integer / `UnsignedNumber`

Same shape as signed integer, but negative input is invalid. IDs should use decimal by default with an optional hexadecimal presentation if the field metadata identifies an ID or bit pattern.

### 5.4 Floating point / `FloatingNumber`

**Shape:** numeric text field with drag affordance. Display precision is separate from stored precision.

**Interaction:** accepts decimal and exponent forms. Dragging uses the schema rate or a magnitude-sensitive default. Do not round the stored value merely because the display is shortened.

**Validation:** reject non-finite values unless the field explicitly permits them. Respect schema min/max/step constraints.

### 5.5 Text / `Text`

**Shape:** one-line field by default; a vertically resizable multiline field when schema metadata allows newlines.

**Interaction:** normal caret, selection, clipboard, Home/End, and arrow behavior. Enter commits a one-line field; Ctrl/Cmd+Enter commits multiline content. Escape cancels the draft.

**Validation:** respect maximum bytes and any declared validation rule. Preserve UTF-8 boundaries.

### 5.6 Enumeration / `EnumChoice`

**Shape:** compact combo trigger showing the selected label and a disclosure arrow. The menu lists schema-provided values, not hard-coded numeric casts.

**Interaction:** click opens the menu; arrows navigate; Enter selects; Escape closes. Selection commits immediately.

**Unknown value:** display `Unknown (0x...)` in amber and retain it until the user chooses a registered value.

**Quick value:** selected label.

### 5.7 Flags / `Flags`

**Shape:** a summary trigger such as `3 flags`, opening a checklist popover. Very small flag sets may use inline chips.

**Interaction:** each check toggles one mask bit. `None` and `All` conveniences may be shown when valid. Unknown bits are preserved and reported as `Unknown 0x...`.

**Commit:** a single popover interaction may coalesce until the popover closes.

### 5.8 Pointer and opaque values

**Shape:** read-only hexadecimal address or opaque summary with a copy button supplied by the card.

Raw pointers are not generically editable. A registered semantic adapter may replace this with a safe manager-owned choice, as `TextureRef` and `ActionCall` do.

## 6. Reusable compound control shapes

These are implementation building blocks, not necessarily schema-level types.

### 6.1 Pair editor

Two equal numeric fields in one row, each with a short semantic label. Used by vectors, dimensions, ranges, offsets, and expansion.

```text
[ X  12.0 ] [ Y  -4.0 ]
```

At narrow widths it wraps into two rows while remaining one semantic editor.

### 6.2 Quad edge editor

A compact box diagram with editable left, right, top, and bottom slots. This retains the strongest part of the old nine-split editor: spatial values are edited spatially.

```text
          [ Top ]
 [ Left ] [ box ] [ Right ]
         [ Bottom ]
```

A link button optionally locks all four values or opposite pairs. Linking changes editing behavior, not the stored type.

### 6.3 Quad corner editor

A rectangle preview with numeric fields at its four corners. Optional linking supports all corners or matching pairs.

### 6.4 Enum pair editor

Two independently labelled enum choices on one row. Used for child alignment and floating attach points when a graphical selector is unavailable.

### 6.5 Tagged-union editor

A discriminator control followed by only the active payload editor. Inactive union storage is never traversed, displayed, copied independently, or mutated.

### 6.6 Color editor

A swatch and hexadecimal field form the compact first row. Expanded content contains R, G, B, and A numeric controls/sliders.

```text
[ swatch ] [ #18B8A6FF ]
[ R 24 ] [ G 184 ] [ B 166 ] [ A 100% ]
```

The canonical stored channel range comes from the registered type. The display may show alpha as a percentage, but conversion must round-trip without changing untouched values. Invalid hex remains a local draft.

## 7. Current Clay semantic TypeEditors

### 7.1 `Clay_Color` / `Color`

Use the color editor shape. Numeric channels respect the current adapter range of 0-255.

**Collapsed quick value:** color swatch plus `#RRGGBBAA`.

**Atomicity:** changing any channel replaces one `Clay_Color` value.

### 7.2 `Clay_Padding` / `Spacing`

Use the quad edge editor with `left`, `right`, `top`, and `bottom` unsigned pixel values. Offer link-all and link-opposites modes.

**Collapsed quick value:** `L8 R8 T4 B4`, shortened to `8 all` when equal.

### 7.3 `Clay_BorderWidth` / spacing with center divider

Use the quad edge editor plus a separate `betweenChildren` numeric field beneath or to its right.

**Collapsed quick value:** `L1 R1 T1 B1 · Between 0`; use `1 all` when possible.

### 7.4 `Clay_CornerRadius` / `CornerRadius`

Use the quad corner editor for `topLeft`, `topRight`, `bottomLeft`, and `bottomRight`.

**Collapsed quick value:** `8 all`, or `TL8 TR4 BR8 BL4`.

### 7.5 `Clay_Vector2` / `Vector`

Use the pair editor labelled `X` and `Y`.

**Collapsed quick value:** `12.0, -4.0`.

### 7.6 `Clay_Dimensions` / `Vector`

Use the pair editor labelled `W` and `H`. Width and height are independent unless a consuming semantic editor explicitly adds an aspect lock.

**Collapsed quick value:** `320 x 180`.

### 7.7 `Clay_SizingMinMax` / `Vector`

Use the pair editor labelled `Min` and `Max` with `min <= max` validation. Crossing values should either move the paired bound during a drag or report an invalid typed draft; it must not silently swap on commit.

**Collapsed quick value:** `0-640 px`.

### 7.8 `Clay_ChildAlignment`

Preferred shape is a 3x3 alignment grid representing the combination of horizontal and vertical enum values. The selected cell is signal blue. Below it, or as an accessibility fallback, expose `X` and `Y` enum choices.

**Collapsed quick value:** `Center / Top`.

### 7.9 `Clay_FloatingAttachPoints`

Show two labelled 3x3 point grids: `Element point` and `Parent point`. A small preview draws a line between the chosen anchors. On narrow cards, stack the grids.

**Collapsed quick value:** `Element LT -> Parent CC`, using stable abbreviations with tooltips.

### 7.10 `Clay_AspectRatioElementConfig`

Use one positive floating-point field labelled `Ratio`, plus optional presets `1:1`, `4:3`, `16:9`, and `Free/0` if zero has the Clay meaning expected by the active version.

The displayed `W:H` formatter is convenience only; the canonical stored value remains `width / height`.

**Collapsed quick value:** `16:9` when close to a known preset, otherwise a concise decimal.

### 7.11 `Clay_SizingAxis` / `SizingAxis`

Use a tagged-union editor.

- `Fit`: type selector plus `Min`/`Max` pixel range.
- `Grow`: type selector plus `Min`/`Max` pixel range.
- `Percent`: type selector plus a percentage editor. Display 0-100%, store Clay's 0-1 value.
- `Fixed`: type selector plus a pixel value. Clay stores this through the active sizing-axis payload; the adapter must expose it semantically rather than treating the union as generic fields.

Changing the type initializes the newly active payload with safe defaults and creates one transaction. The previous inactive payload may be cached as UI state for convenient switching, but only the active legal representation is submitted.

**Collapsed quick value:** `Grow 0-640`, `50%`, or `Fixed 240 px`.

### 7.12 `Clay_Sizing` / `Sizing`

Two semantic rows, `Width` and `Height`, each containing a `Clay_SizingAxis` editor. They are part of one semantic `Clay_Sizing` editor, not four generic union-member cards.

**Collapsed quick value:** `W Grow · H Fit`.

## 8. Clay composite semantic TypeEditors

Composite editors are vertically stacked groups of the semantic and primitive controls above. They should use lightweight internal section labels instead of nested card chrome for every known member. Unknown newly registered fields must fall back to schema recursion so the semantic editor never hides data added by a newer Clay version.

### 8.1 `Clay_LayoutConfig`

Rows:

1. `sizing` -> `Clay_Sizing` editor.
2. `padding` -> `Clay_Padding` editor.
3. `childGap` -> unsigned pixel editor.
4. `childAlignment` -> alignment grid.
5. `layoutDirection` -> enum choice or a compact horizontal/vertical segmented control.

**Collapsed quick value:** direction, width/height sizing kinds, and gap; for example `Row · W Grow · H Fit · Gap 8`.

### 8.2 `Clay_TextElementConfig`

Rows:

1. `textColor` -> color editor.
2. `fontId` -> preferably a font resource choice; unsigned ID fallback.
3. `fontSize` -> unsigned pixel editor.
4. `letterSpacing` -> unsigned pixel editor.
5. `lineHeight` -> unsigned pixel editor, with `Auto/0` if supported.
6. `wrapMode` -> enum choice.
7. `textAlignment` -> three-button left/center/right control with enum fallback.
8. `userData` -> read-only opaque pointer unless a custom adapter exists.

**Collapsed quick value:** `Font <id> · 14 px · Left · Words`.

### 8.3 `Clay_FloatingElementConfig`

Rows:

1. `attachTo` -> enum choice; this controls contextual availability of some following rows.
2. `attachPoints` -> dual anchor grids.
3. `offset` -> X/Y pair.
4. `expand` -> W/H pair.
5. `parentId` -> unsigned ID editor, enabled only when attaching to a specific element.
6. `zIndex` -> signed integer editor.
7. `pointerCaptureMode` -> enum choice.
8. `clipTo` -> enum choice.

Disabled contextual fields remain visible and muted so changing `attachTo` does not radically reshape the card.

**Collapsed quick value:** `Root · CC->CC · z200`, or `Disabled` for `AttachToNone`.

### 8.4 `Clay_ClipElementConfig`

Rows:

1. `horizontal` and `vertical` -> two labelled toggles or a four-state axis control (`None`, `X`, `Y`, `XY`).
2. `scrollInputDisabled` -> toggle with an explanatory tooltip.
3. `childOffset` -> X/Y pair.

**Collapsed quick value:** `XY scroll`, `X clip`, or `Off`, with `Input disabled` appended when applicable.

### 8.5 `Clay_BorderElementConfig`

Rows:

1. `color` -> color editor.
2. `width` -> `Clay_BorderWidth` editor.

**Collapsed quick value:** swatch plus width summary, such as `#26485C · 1 all`.

### 8.6 `Clay_ElementDeclaration`

This is a large semantic editor and should be organized into lightweight internal sections:

- **Identity:** element ID information when the active Clay/schema version exposes it.
- **Layout:** `Clay_LayoutConfig`.
- **Surface:** `backgroundColor`, `overlayColor`, and `cornerRadius`.
- **Aspect/Image:** aspect ratio plus image/resource data through safe adapters.
- **Floating:** `Clay_FloatingElementConfig`.
- **Clip:** `Clay_ClipElementConfig`.
- **Border:** `Clay_BorderElementConfig`.
- **Transition:** registered transition fields from the active Clay schema.
- **Custom/User data:** read-only opaque values unless custom adapters exist.

The old Dev Mode editor is useful interaction precedent, but it must not be the field list. In particular, current Clay includes fields such as `overlayColor` and `transition` that older code did not cover. The registered schema is authoritative; every unrecognized field receives a recursive fallback row/card.

**Collapsed quick value:** a restrained surface/layout summary such as `Row · Grow/Fit · #071A22`; never attempt to serialize the whole declaration into the header.

## 9. FlowUi semantic TypeEditors

### 9.1 `ActionCall` / `ActionChoice`

`ActionCall` is manager-owned and cannot be safely reconstructed from its private payload using generic schema traversal.

**Shape:** a combo/search trigger containing:

- `None`;
- `App action` entries;
- `UI action` entries valid for the current context.

Each entry shows its debug name and a small `App` or `UI` tag. Search matches debug name. Duplicate names show a secondary stable ID or owner context.

**Behavior:**

- Selecting an action creates a manager-mediated semantic command, not a raw memory override.
- Unbound or no-longer-registered calls display `Missing action` with the retained stable identity when available.
- Disabled actions remain selectable as values if binding is legal; disabled invocation state is presentation metadata, not necessarily invalid configuration.
- A small inspect/open-catalogue affordance may navigate to action metadata but must not invoke the action.

**Collapsed quick value:** `None`, `App · Save`, or `UI · SelectTab`.

**Required backend upgrade:** the current adapter is view-only because it has no edit adapter. Editable support requires the Action Manager to expose a stable catalogue snapshot plus construction/serialization operations for legal `ActionCall` values.

### 9.2 `TextureRef` / `ResourceChoice`

**Shape:** a resource search/choice trigger with thumbnail, resource name/key, and status. Below it:

- `fitMode` -> enum choice;
- `samplingMode` -> enum choice;
- `tintEnabled` -> toggle;
- handle, UVs, source dimensions, and availability flags -> compact read-only metadata.

The manager-owned handle and UV coordinates are never edited numerically. Choosing a resource requests a manager-produced `TextureRef`, after which user-editable presentation members are preserved or reset according to an explicit product rule.

**Collapsed quick value:** thumbnail plus resource debug name; fall back to handle and status.

### 9.3 `FlowUiTheme`

`FlowUiTheme` is currently a registered ordinary struct rather than a single semantic leaf. It should therefore appear as a recursive card whose fields automatically receive color, corner-radius, numeric, and font-resource editors.

For usability, group its fields without changing their paths:

- Primary colors;
- Surfaces;
- Text colors;
- Borders/status colors;
- Radii;
- Typography;
- Spacing.

This is an optional presentation adapter. Generic schema recursion remains the correctness fallback.

## 10. Optional TypeEditor

An optional is a wrapper around the editor selected for its contained type.

**Shape:** an `Enabled`/`Has value` toggle followed by the contained editor. When empty, show a muted `None` surface instead of the contained editor.

```text
[x] Enabled   [ contained TypeEditor ... ]
[ ] None
```

For a large contained struct, enabling the optional reveals a nested `DevEditorCard` rather than forcing it into one row.

**Behavior:**

- Turning off commits `nullopt` in one transaction.
- Turning on constructs a value through registered type operations. Prefer the last value cached in UI state during the same session; otherwise use the schema default/value initializer.
- If the contained type cannot be constructed safely, the optional is view-only and explains why.
- Copy/paste uses the complete optional type. `T` and `optional<T>` do not match.

**Collapsed quick value:** `None` or the contained semantic quick value prefixed only when ambiguity requires it.

Nested optionals are legal but should be labelled by level (`Outer`, `Inner`) to avoid indistinguishable toggles.

## 11. Sequence TypeEditor

**Shape:** a vertical list with item index/key, item editor or nested card, and per-item remove/reorder controls. The footer row contains `Add` and, when useful, `Clear`.

```text
0  [ item editor                         ] [drag] [remove]
1  [ nested DevEditorCard                ] [drag] [remove]
                                           [ Add item ]
```

**Behavior:**

- Primitive and small semantic items use inline TypeEditors.
- Struct items use nested cards.
- Stable item identity must not depend solely on the current index when the sequence adapter can provide keys.
- Add/remove/reorder are transactional whole-sequence operations unless the backend supports stable item-path commands.
- Large sequences should virtualize rows and offer filtering; this does not change value semantics.

Fixed-size arrays omit add/remove and expose only their items.

## 12. Arbitrary registered structs

An arbitrary struct does not receive a monolithic TypeEditor. Its containing `DevEditingField` emits:

- a `DevEditor` for each primitive, enum, or small semantic leaf;
- a nested `DevEditorCard` for each object field;
- an optional or sequence wrapper as appropriate.

Field order follows schema declaration order unless explicit presentation metadata supplies groups/order. Hidden and read-only capabilities are honored at every depth.

There is no generic collapsed quick value for an arbitrary struct. A registered presentation adapter may add one without changing the underlying schema or edit path.

## 13. Custom TypeEditor

`DevEditorKind::Custom` is an application-provided semantic editor factory keyed by stable type identity, optionally refined by field metadata.

A custom editor must provide:

- supported type ID and schema generation compatibility;
- draw/build function;
- owned-value read and replacement operations;
- validation and normalization;
- read-only behavior;
- optional collapsed quick-value formatter;
- optional default construction for use by optionals/sequences;
- no direct mutation outside the shared transaction submission path.

If the factory is missing or rejects the current schema generation, the Inspector falls back to generic object recursion or an unsupported explanation. It must not retain a stale function pointer across schema generations.

## 14. Unsupported and partially editable types

The Inspector should state the capability reason in plain language:

- `View only: field is marked read-only`;
- `View only: runtime state editing is not enabled`;
- `Unsupported: no capture adapter`;
- `Unsupported: no edit adapter`;
- `Unsupported: raw pointer`;
- `Unsupported: callable type`;
- `Unsupported: recursive schema cycle`.

A partially editable semantic composite shows safe editors for supported members and read-only rows for the rest. Its card-level copy remains available when the whole value can be captured. Card-level paste/reset is enabled only if the backend can replace/reset the complete value safely.

## 15. Accessibility and keyboard behavior

- Every graphical editor also has a textual name and keyboard path.
- Tab moves between logical controls; it must not enter decorative diagram cells.
- Arrow keys change selected enum/grid cells only while that control is focused.
- Enter commits drafts or opens a choice; Escape cancels drafts/closes popovers.
- Color, alignment, spacing, and corner editors cannot rely on color or position alone; each editable slot has a label or accessible description.
- Tooltips explain abbreviated labels and disabled controls.
- Focus remains on the corresponding control after a transactional rebuild whenever the field path still exists.

## 16. Recommended implementation order

1. Shared transaction-aware value replacement helper and draft/gesture coalescing.
2. Boolean, signed, unsigned, floating, text, and enum editors.
3. Optional wrapper and arbitrary-struct recursion integration.
4. Pair, color, edge, and corner control shapes.
5. `Clay_Color`, padding, border width, corner radius, vectors, dimensions, and ranges.
6. `Clay_SizingAxis`, `Clay_Sizing`, alignment, and attach-point editors.
7. Clay layout/text/floating/clip/border composites.
8. Sequence and flags editors.
9. Texture/resource catalogue integration.
10. Action Manager catalogue and semantic-command integration.
11. `Clay_ElementDeclaration` presentation grouping and forward-compatible fallback.
12. Application custom-editor factory API.

## 17. Acceptance criteria for every TypeEditor

A TypeEditor is complete only when:

- it renders editable, read-only, invalid, and unavailable states;
- it uses schema identity and type operations rather than display-name casting;
- it preserves untouched members and inactive union safety;
- every committed mutation produces an undoable and redoable transaction;
- a drag gesture or text edit has a sensible single-step undo boundary;
- copy/paste/reset at the containing card still operate on the complete value;
- it remains usable in a narrow Inspector without horizontal overflow;
- keyboard and non-color cues expose the same information as its visual shape;
- missing manager/catalogue support degrades honestly to view-only;
- schema fields unknown to a semantic presentation are still visible through fallback recursion.
