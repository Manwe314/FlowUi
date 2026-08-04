# Manager storage migration implementation plan

## Purpose

This report defines the post-Phase 4 migration of `UiManager`, `FontManager`,
`IconManager`, `ImageManager`, `InputFieldManager`, `ShortcutManager`, and
`ViewPortManager` from manager-owned long-lived storage to the existing
`IStorageSystem`/`FlowStorageSystem` architecture.

The desired end state is not merely that manager containers use memory obtained
from `allocatePersistent()`. The StorageSystem must become the lifetime and
publication authority for the records, bytes, GPU objects, logical keys,
generations, uploads, and deferred retirement represented by those containers.
Managers remain the small, recognizable public interfaces through which users
express intent. They should know what an image, icon, font, input field,
shortcut, or viewport means, but they should no longer decide where its
long-lived bytes or Vulkan objects live or when that storage can be reused.

The existing public manager method families remain recognizable. Broad new
public manager concepts are out of scope. Small internal signature changes and
small correctness-driven public type adjustments are permitted, and source
compatibility is not a requirement. No StorageSystem configuration or internal
handle tables become public API.

This work deliberately precedes configurable internal multithreading. It must,
however, establish the immutable snapshots, mutation boundaries, ownership
rules, and per-window scopes that a later inline or worker executor can use
without moving manager data a second time.

## Current baseline after Phase 4

Phase 4 already establishes the correct high-level scopes:

- `FontManager`, `ImageManager`, and `IconManager` are app-shared.
- Every `AppWindow` owns its own `UiManager` and `ViewPortManager`.
- Every `UiManager` owns a window-local `InputFieldManager` and
  `ShortcutManager`.
- Logical textures and physical image bindings are already separated.
- Texture bindings, frame arenas, frame tokens, renderer descriptors, instance
  buffers, and viewport state are already scoped by `WindowId` where required.
- Storage owns generic buffers, images, image views, samplers, blobs, logical
  textures, uploads, frame leases, renderer bundles, and serial-based
  retirement.

The remaining manager-storage problems are concrete:

- `ImageManager`, `IconManager`, `FontManager`, and `ViewPortManager` still own
  Vulkan/VMA objects and private upload or command pools.
- They publish borrowed native texture objects through
  `IUiTexturePublisher`/`publishExternalTexture()`.
- Manager maps duplicate user keys as `std::string`, sometimes with constructed
  namespace prefixes.
- Managers retain their own retirement lists and cache metadata.
- `UiManager` owns its Clay allocation and per-frame string arenas directly.
- Input fields, shortcuts, interaction state, callbacks, and development data
  live in manager-owned standard-library containers.
- UI layout and renderer text conversion borrow `FontManager` directly.
- Several public query methods return pointers, references, or string views into
  manager-owned containers, so the migration must define stable view lifetimes
  rather than silently relocating those records.

## Meaning of the three implementation choices

The three choices need precise meanings so that “option 3” does not recreate
the borrowed-native bridge being removed.

### Option 1: manager behavior over generic StorageSystem records

The manager keeps its domain algorithm and calls generic StorageSystem
primitives for keyed records, interned strings, persistent object storage,
blobs, images, uploads, snapshots, and retirement. It holds only the storage
root, its scope, small configuration values, and opaque record handles.

This is appropriate when the domain behavior is simple, does not need to be
shared by several internal consumers, and can be expressed cleanly using
generic storage operations. It does **not** mean placing an `unordered_map` in
manager code with a `pmr` allocator and calling the migration complete. In the
end state, the keyed registry and record lifetime are storage-owned.

### Option 2: a focused semantic API owned by StorageSystem

Storage exposes a narrow, manager-specific operation group because correct
storage and lifetime management requires semantic operations such as replacing
a complete viewport target generation. The operation group may be a focused
capability view reached from the single `IStorageSystem` root; it must not turn
the root interface into an unrelated list of hundreds of methods.

This is appropriate when creation, replacement, frame acquisition, and exact
retirement cannot be safely split into arbitrary generic calls by a manager.
Storage owns both the records and the atomic resource transition. The manager
still owns user-facing policy and callback invocation.

### Option 3: a private domain controller over StorageSystem

A private controller contains a substantial domain algorithm that neither
belongs in a thin public manager nor in the generic storage core. The controller
uses StorageSystem handles and typed record tables; it owns no Vulkan/VMA
object, raw allocation arena, descriptor binding, upload pool, or retirement
queue. Its own durable tables are allocated and owned by StorageSystem.

This is not an ownership or native-handle bridge. In particular, it must never
accept a manager-owned `VkImage` and “publish” it to storage. It is a policy
engine operating on storage-owned resources. This option is reserved for the
font and icon systems, whose parsing, packing, matching, growth, and cache
algorithms would otherwise make either the manager or `IStorageSystem` a god
object.

## Decision summary

| Manager | Scope | Primary choice | Reason |
|---|---|---:|---|
| `UiManager` | Per window | 1 | Clay/UI behavior should remain in the manager; its persistent and transient memory can use generic window/frame storage directly. |
| `ImageManager` | Per app | 1 | Decode, validate, and user-facing registration are simple orchestration over generic blob/image/view/sampler/upload/texture and keyed-record operations. |
| `FontManager` | Per app | 3 | Font parsing/baking, face matching, immutable metric publication, and atlas growth form a substantial reusable domain engine but should not enter generic storage. |
| `IconManager` | Per app | 3 | SVG ownership, size reuse, atlas packing, LRU eviction, stable aliases, and raster demand merging require a dedicated cache controller. |
| `InputFieldManager` | Per window | 1 | Editing and hit-testing remain manager behavior; mutable text/caret records and frame scratch map naturally to window-persistent and frame storage. |
| `ShortcutManager` | Per window | 1 | Registration and dispatch are compact manager algorithms over storage-owned callback records and immutable dispatch snapshots. |
| `ViewPortManager` | Per window | 2 | A viewport target is a transactional per-frame GPU generation whose creation, resize, acquisition, and exact retirement must be one semantic storage operation group. |

These are primary choices rather than isolated implementations. Every option
still uses the same common key, handle, snapshot, upload, submission, and
retirement machinery.

## Common storage foundation required first

The existing generic GPU and frame APIs are a strong base, but moving all
manager state requires several additional internal facilities.

### Typed, non-relocating CPU record tables

Add storage-owned typed tables for non-trivial CPU records. They should use
generational handles, paged/non-relocating slots, explicit construction and
destruction, and dense hot arrays where iteration performance matters. A record
may contain strings, vectors, and `std::function`, so raw `MemoryBlock` alone is
not sufficient: StorageSystem must invoke the correct destructor at record
retirement and must account for nested allocations.

Suggested additional resource kinds include:

```cpp
UiContext,
UiInteractionState,
ImageAsset,
FontFamily,
FontFace,
FontAtlas,
SvgDocument,
IconAtlasPage,
IconVariant,
InputField,
ShortcutRegistration,
Viewport,
ViewportTarget,
```

Some already exist in `ResourceKind`; existing kinds should be reused rather
than duplicated. Public code must not see these internal handles.

Slots must not move while a compatible public pointer/reference view is valid.
Recycling a slot increments its generation. Hot render paths consume immutable
spans from a sealed frame snapshot rather than validating a virtual handle for
every glyph, icon, or shortcut.

### Keyed namespaces

All string-keyed managers use the existing structured `ResourceKey`:

```cpp
ResourceKey {
    ResourceDomain domain;
    StringId name;
    WindowId window;
};
```

Rules:

- App-shared image, icon, and font keys use `InvalidWindowId`.
- UI, input-field, and viewport keys include the owning `WindowId`.
- The same byte string may exist independently in different domains.
- Keys are exact byte strings unless a manager explicitly documents another
  policy. Do not silently lowercase or path-normalize user keys.
- Paths are data associated with a record, not its key. Store an interned
  normalized path ID separately where useful.
- Remove all constructed strings such as `"img:" + key`,
  `"svg/request/" + key`, and per-frame viewport key suffixes.
- `contains`, lookup, replacement, and removal operate on the committed keyed
  registry, never on descriptor slots or raw native handles.

`ShortcutManager` is the one current exception to the claim that every manager
has a human string key: its public identity is `ShortcutId`, while the chord is
an indexed dispatch key. The migration should preserve that model rather than
inventing a string API merely for uniformity.

### Storage transactions

Introduce an internal `StorageMutation`/`StorageTransaction` facility. It may
initially execute inline, but it must provide commit/rollback semantics across
typed CPU records and existing GPU resources.

A transaction must be able to:

- reserve record and handle slots without publishing them;
- intern or reference strings;
- construct blobs, buffers, images, views, and samplers;
- enqueue uploads without making a half-created asset discoverable;
- publish or revise logical textures;
- replace a keyed record atomically;
- attach dependency ownership between records;
- on failure, release every candidate in reverse dependency order;
- on commit, publish one registry revision and enqueue the replaced generation
  for exact serial-based retirement.

No existing window or previously committed record may change when a candidate
operation fails. Failure injection at every allocation, upload scheduling,
publication, and table-growth point is required to verify this.

### Ownership and dependency edges

Storage records retain their dependencies. For example:

```text
ImageAsset -> TextureView -> ImageView -> Image
                         -> Sampler

IconVariant -> IconAtlasPage -> ImageView -> Image
            -> logical TextureView
IconAtlasPage -> shared Sampler
SvgDocument -> source Blob + parser-owned document

FontFace -> immutable metric/glyph/kerning blocks
FontAtlas generation -> ImageView -> Image + shared Sampler

Viewport -> active ViewportTarget generation
ViewportTarget generation -> frame target images/views + frame command resources
                           -> per-frame logical TextureViews
```

Releasing a manager record releases strong dependency references. Actual object
destruction occurs only after the record has no owners and all submissions that
used the generation have completed.

### Publication barrier and immutable snapshots

Add an app-shared manager publication revision and a per-window state revision.
In the first single-threaded implementation, mutation can still commit inline,
but `beginFrame(WindowId)` captures the revisions visible to that frame and
`sealFrame()` freezes them into its `FrameReadLease`.

The later worker executor can then use this exact contract:

1. The app/platform resource lane commits shared manager mutations at a defined
   publication point.
2. Each window job captures an immutable shared revision and owns exclusive
   mutation rights to its own window-local UI/input/shortcut state.
3. Workers use record handles and frame views, never pointers into a resizable
   manager container.
4. New shared mutations are queued for the next publication point if jobs are
   already reading the current revision.
5. Submission stamps all storage resources referenced by the sealed snapshot.

This should be implemented even while the executor is inline. Adding mutexes to
the existing manager maps later is not an acceptable substitute.

### Focused capability views without a god interface

Keep one installable `IStorageSystem` root. Internally compose common record
tables and focused capability views. A possible shape is:

```cpp
IStorageSystem
  |- generic resources, frames, uploads, retirement
  |- manager record/key services
  |- viewport target operations
  |- optional Vulkan native frame views
```

The Font and Icon controllers receive the root and the typed tables they need.
`ViewPortManager` receives a focused viewport capability from the root. No
manager includes `FlowStorageSystem.hpp`, VMA, or a concrete Vulkan storage
implementation.

Virtual calls occur on creation, replacement, lookup misses, and lifecycle
boundaries. Hot loops receive cached spans/records in a frame view. A glyph,
quad, caret, or shortcut dispatch must not cross a virtual storage API for each
item.

## `ImageManager`: option 1

### Current ownership to remove

`ImageManager` currently owns `VkImage`, VMA allocation, `VkImageView`,
`VkSampler`, an upload command pool, the `imagesByKey_` map, duplicated paths,
missing-key warning strings, and a retired-resource vector. It publishes these
native objects through the transitional texture publisher.

All of that long-lived state moves to StorageSystem. The manager retains only a
storage reference and decode/validation policy.

### Storage record

An app-shared `ImageAssetRecord` should contain compact cold metadata:

```cpp
struct ImageAssetRecord {
    StringId key;
    StringId sourcePath;
    TextureHandle texture;
    ImageHandle image;
    ImageViewHandle imageView;
    SamplerHandle sampler;
    UploadTicket upload;
    uint32_t width;
    uint32_t height;
    ResourceState state;
};
```

The texture/view dependency graph may make some handles redundant in the record;
keep only handles needed for status, diagnostics, replacement, or explicit
release. The keyed registry maps `(Image, key, InvalidWindowId)` to an
`ImageAssetHandle`. Missing-key warning suppression belongs in storage
diagnostics as a `(domain, StringId, diagnostic-kind)` once-set, not a manager
`unordered_set<std::string>`.

### Registration transaction

`registerImage(key, filePath)` keeps its public behavior but becomes:

1. Validate non-empty key and path and intern both.
2. Check the committed image registry to determine the returned inserted/replaced
   result according to current API semantics.
3. Decode into `DecodeTransient` storage. If stb cannot decode directly into a
   supplied allocator, copy once into a storage blob and immediately free the
   opaque stb allocation. Account that short opaque allocation in telemetry.
4. Validate dimensions and overflow before creating GPU objects.
5. Acquire the normalized shared sampler.
6. Create a storage-owned image and image view.
7. Create a pixel blob and enqueue the central upload. The current synchronous
   public behavior may call `flushUploads()` before return; the representation
   must still retain `Queued/Uploading/Ready/Failed` states for later async use.
8. Create or replace the logical texture using `publishTexture()` or
   `replaceTexture()` with storage-owned handles, never
   `publishExternalTexture()`.
9. Publish the `ImageAssetRecord` and key mapping as the transaction's final
   commit.
10. Retire the previous asset generation after its exact last use.

If any step fails, no key mapping or texture revision changes. Candidate blobs,
image/view references, sampler references, and upload requests are cancelled or
released.

### Query and removal

- `contains(key)` performs a keyed record lookup.
- `getTexture(key)` returns the record's stable logical `TextureHandle`; queued
  and failed records resolve through the existing fallback rules.
- `removeImage(key)` atomically removes the key and logical texture revision and
  releases the record. It does not wait for a device or maintain a private
  retired vector.
- `onSafePoint()` disappears. Upload completion and retirement collection are
  app-level StorageSystem safe-point work.
- `destroy(VulkanContext&)`, `init(VulkanContext&, ...)`, `vk_`,
  `uploadCommandPool_`, `framesInFlight_`, `IUiTexturePublisher`, and all native
  resource helpers disappear.

### Performance result

The common sampler is deduplicated, names are interned once, image records are
compact and cold, lookup is by `StringId`, and uploads can be batched across
images/fonts/icons. `getTexture()` remains an O(1) read with no Vulkan call and
no descriptor lookup.

## `FontManager`: option 3

### Why a private controller is appropriate

Font management combines several distinct algorithms: baked format parsing,
optional runtime MSDF generation, family/weight/style selection, glyph and
kerning lookup, atlas layer allocation, atlas growth/copy, upload, and native
renderer publication. Putting all of that in `IStorageSystem` would couple the
generic allocator/backend to artery-font, FreeType, msdfgen, and the current
font policy. Leaving it in the public facade would preserve the current large
manager and make worker-safe font snapshots difficult.

Introduce a private `FontCatalogController`. `FontManager` forwards its current
public calls to this controller. The controller owns algorithms and small
configuration only; StorageSystem owns all catalog records, immutable arrays,
source blobs, atlas generations, GPU resources, and table indexes.

### Storage representation

Use app-shared records:

- `FontFamilyRecord`: stable `FontFamilyId`, interned name, and a compact array
  of `(FontFaceHandle, weight, style)` entries.
- `FontFaceRecord`: stable public `FontId`, interned name/path/metadata, atlas
  placement, and handles/spans for immutable variants.
- `FontVariantRecord`: scalar metrics plus contiguous glyph and kerning data.
- `FontAtlasRecord`: active atlas generation, dimensions, layer usage/capacity,
  binding revision, image/view/sampler handles, and retired generations.

`FontId` remains the small ID Clay consumes. Maintain a storage-owned dense
`FontId -> FontFaceHandle` array. IDs are monotonic for the app lifetime and are
not reused. `FontFamilyId` follows the same rule. A failed transaction consumes
no published ID; reservation may be rolled back before commit.

Published faces are immutable. This allows `getFontById()` and text layout to
read without locks from a frame snapshot. Store glyphs contiguously. Replace
`unicodeToGlyphIndex` with a sorted compact codepoint/index array or a measured
dense/sparse hybrid, and replace node-based kerning maps with sorted packed
pairs or a flat hash table. The implementation choice should be benchmarked,
but neither lookup should allocate.

### Public view compatibility

Preserve `createFamily`, `getFamilyId`, `addFamilyFace`, both `resolveFont`
overloads, `getFontById`, and `getAtlasResource` as recognizable methods.

- `getFontById()` may continue returning `const FontFaceData*` because published
  face records live in non-relocating storage and are immutable until app
  shutdown. The method remains app/platform-thread API unless a frame read lease
  is held.
- `getAtlasResource()` may continue returning a compatibility reference backed
  by a stable storage record updated only at the quiescent publication point.
  Internal renderer code must not use that reference. It consumes a
  `FontFrameView` containing the exact atlas generation captured by its frame.
- If preserving the raw reference proves unsafe during implementation, the only
  recommended public adjustment is returning a small `FontAtlasView` by value.
  Do not expose `ImageHandle`, storage configuration, or mutable storage records.

### Family and face transaction

`createFamily()` must become strongly transactional. The current implementation
publishes the family before every face has loaded, so a later face failure can
leave a partial family. The new sequence is:

1. Reserve the family name/ID without publishing it.
2. Parse/bake every requested face into decode/worker transient storage.
3. Build immutable metric, glyph, and kerning blocks.
4. Reserve all required atlas placements in a candidate atlas generation.
5. Create/grow storage-owned atlas resources and enqueue all uploads.
6. Build every face and family record.
7. Publish the new atlas revision, face IDs, family ID, and name mapping in one
   commit.
8. Retire any replaced atlas generation by tracked submission use.

`addFamilyFace()` is a smaller transaction that publishes a new immutable
family-face array and atlas revision together. Existing frames retain their old
catalog/atlas snapshot.

### Atlas growth

The first implementation may preserve the current array-image strategy:

1. Allocate a larger storage-owned image generation.
2. Copy existing used layers and upload new layers through the central upload
   scheduler.
3. Create the view and acquire the shared sampler.
4. Atomically publish the atlas generation and increment its binding revision.
5. Retire the old image/view after exact frame completion.

No font operation owns an upload command pool or calls device idle. The
controller should later be free to select paged atlases without changing
`FontManager` methods or the `FontId` mapping.

### Renderer and UI consumers

Remove `VulkanUiRenderer::setFontManager()` and direct renderer borrowing of
`FontManager`. At frame preparation, StorageSystem provides a `FontFrameView`
with:

- immutable `FontId -> face` lookup spans;
- face/variant/glyph/kerning spans;
- the exact atlas image-view/sampler generation;
- its binding revision and resource-use handles.

`UiManager` and `InputFieldManager` text measurement use the same immutable font
catalog view, not raw pointers to another manager. The renderer tracks the atlas
generation in the storage frame before sealing.

### Runtime font baking and future threads

Runtime baking is already computationally parallel inside msdf-atlas. Treat it
as a resource-production task, not a UI-window job. It produces an unpublished
candidate in worker/decode arenas and commits through the app resource lane.
Third-party FreeType/msdf allocations are either redirected through supported
allocators or reported as opaque external bytes. They never become untracked
manager ownership.

## `IconManager`: option 3

### Why a private controller is required

The icon system is a cache and atlas allocator, not just a table of images. It
owns parsed SVG documents, size-tolerant variant selection, padded rectangle
allocation, free-rectangle merging, atlas-page growth, stable requested-texture
aliases, raster upload, per-frame touches, and LRU eviction. These algorithms
deserve a private `IconCacheController`; they do not belong in generic storage
and should not leave `IconManager` as a large ownership class.

As with fonts, the controller owns policy, not resources. StorageSystem owns all
documents, strings, source blobs, variant/page records, arrays, GPU objects,
logical textures, uploads, and retirement.

### Storage representation

Use app-shared records:

- `SvgDocumentRecord`: user `StringId`, retained source `BlobHandle`, parsed
  document owner/deleter, intrinsic dimensions, parser state, and opaque-memory
  accounting.
- `IconRequestRecord`: stable user-key alias texture and the currently selected
  variant/revision.
- `IconVariantRecord`: document handle, normalized requested dimensions, page
  handle, padded/content rectangles, UVs, source dimensions, last-use epoch,
  pin/reference state, and logical texture revision.
- `IconAtlasPageRecord`: image/view/sampler handles, extent, used area,
  free-rectangle storage, page generation, and last submitted use.

Variant keys use `(StringId, requestedWidth, requestedHeight)`, not copied
strings. The reverse lookup from logical texture to request uses handles/indexes,
not `unordered_map<uint64_t, std::string>`. Atlas pages acquire one normalized
shared sampler from StorageSystem.

### SVG registration and removal

`registerSvg()` and `registerFromFile()` preserve their public behavior while
using a transaction:

1. Intern and validate the key.
2. Retain source bytes in a storage blob.
3. Parse a candidate document and validate intrinsic dimensions.
4. Adopt the parsed document into a typed StorageSystem record with an explicit
   destructor. If plutosvg cannot use a supplied allocator, record its opaque
   allocation category.
5. Publish the document and request-alias record only after all steps succeed.

`remove()` atomically removes the request alias and document key, invalidates
or retires every variant belonging to that document, returns atlas regions to
their page only when no submitted frame can reference the old view revision,
and lets empty pages retire by exact serial. No raw document or atlas page is
destroyed by `IconManager`.

### Demand resolution and same-frame behavior

`textureRef(key)` returns the stable request alias logical texture. The existing
layout-size behavior remains in `prepareFrameTextures`, but the implementation
is split into demand collection and cache resolution:

1. Scan the window's image commands and gather `(request, pixelWidth,
   pixelHeight)` demands into its frame arena.
2. Deduplicate demands within the window job.
3. Query an immutable icon-cache snapshot for a tolerance-compatible variant.
4. Cache hits update only a compact per-job use list and remap the command to
   the selected logical variant.
5. Cache misses are sent to the serialized app resource lane. To preserve
   current same-frame behavior, frame sealing waits for that lane to rasterize,
   allocate/upload, publish, and return the new variant. A later explicitly
   asynchronous mode may instead display fallback for one or more frames.
6. Merge job use lists at the publication point for deterministic LRU state.

Do not mutate global `referencedThisFrame` booleans from several workers. Use a
monotonic app-tick/use epoch plus per-job demand/use lists. GPU submission use
continues to come from StorageSystem's normal resource tracking.

### Atlas allocation and eviction

The controller retains the current packing policy initially, but the data moves
to storage-owned flat/paged arrays. Atlas page creation is transactional:

1. Create storage image/view and acquire sampler.
2. Clear/initialize the image through the upload scheduler.
3. Publish the page record only when ready.

Variant insertion reserves a region, rasterizes into decode transient memory,
uploads the exact subregion, and then publishes the variant and alias revision.
If rasterization, upload scheduling, or publication fails, the region is
returned and no alias changes.

LRU selection remains controller policy. Storage supplies last-use serial,
resident/retired byte counts, pin state, and budget pressure. A variant can be
logically evicted immediately but its region must not be overwritten until the
old image/texture generation is no longer in use. If subregion-safe reuse cannot
be proven, use copy-on-write page generations or defer region reuse; never rely
only on an app frame counter.

### Removed ownership

`VkSampler`, command pool, VMA pages, native page helpers,
`IUiTexturePublisher`, copied namespaced strings, and manual page destruction
leave `IconManager`. `beginAppTick()` becomes a StorageSystem/controller
publication operation. The public manager becomes a small facade over the
controller.

## `UiManager`: option 1

### State classification

`UiManager` mixes three different lifetimes that must not be stored alike.

Window-persistent:

- Clay context backing memory and the `Clay_Context` lifetime;
- previous/current interaction snapshots needed across frames;
- input field and shortcut roots;
- cursor/clipboard accessors and window UI configuration;
- development runtime configuration/state that persists across frames.

Frame-local:

- copied Clay strings and stored `TextureRef` objects;
- constructed-element stack;
- Clay render commands until conversion finishes;
- input-field rendering overrides and selection/hit-test scratch;
- capture scratch and temporary diagnostics.

Small facade state:

- `IStorageSystem*`, `WindowId`, the active `FrameToken`, and cached handles/views
  valid for that frame.

### Persistent Clay allocation

Allocate the Clay arena with a tagged window-persistent storage allocation.
Storage owns the block; `UiManager` constructs/destroys the Clay context over
it. Clay-specific sizing and any future context rebuild remain manager
behavior. This is option 1: StorageSystem does not gain Clay layout methods.

Clay's persistent arena cannot be relocated after initialization. Growth must
therefore either be rejected with a clear capacity diagnostic or implemented as
a transactional manager-level Clay context rebuild at a quiescent point. Raw
pool growth must never move the active context memory.

### Frame allocation

Remove `arenas_`, `curArena_`, `allocBytes()` ownership, and
`clayArenaMemory_`. `beginFrame()` receives/caches the window's `FrameToken` and
obtains its `FrameTransient` arena. `toClayString()`, `imageData()`, and
`storeTexture()` allocate from that frame arena. Arena overflow follows
StorageSystem growth/budget policy instead of immediately requiring a fixed
`stringArenaSize` increase.

Use frame-arena spans or `pmr` containers for constructed elements,
interaction-capture scratch, and input overrides. Any span passed to renderer
conversion is part of the frame epoch and remains valid through `sealFrame()`
and preparation. It is invalid after cancellation or frame-slot reuse.

### Interaction and development state

Store the two persistent interaction generations in a window-scoped typed
record. Publish an immutable previous-frame interaction view at `beginFrame`;
write current-frame results into the exclusive window state and rotate at the
frame boundary. This avoids copying data between manager-owned vectors and
gives future jobs a precise owner.

Development runtime data is not exempt. Persistent developer settings/state use
`MemoryClass::Persistent` or a dedicated development tag; capture lists and
per-frame diagnostics use the frame arena. Process-global immutable definition
metadata can remain shared, but mutable per-app/per-window development state
must not hide in global storage.

### Manager relationships

`UiManager` continues exposing `inputFields()` and `shortcuts()`, but those
objects are facades bound to the same `(IStorageSystem, WindowId)` rather than
owners embedded for their containers. Remove direct `FontManager*` borrowing;
font measurement uses the frame's font catalog snapshot.

Clipboard and cursor callbacks remain window/platform infrastructure and may
stay as small manager facade members. Their invocation remains on the app thread
in the first worker phase. They are not GPU/resource storage and should not be
moved merely for uniformity.

### Clay threading constraint

Before parallel UI construction is enabled, verify Clay's current-context
implementation. `Clay_SetCurrentContext()` followed by process-global context
access is unsafe if two worker threads can overwrite one shared current-context
slot. The acceptable outcomes are:

- Clay provides or is patched to use thread-local current context, allowing one
  window context per worker; or
- Clay layout calls remain serialized while later renderer conversion and
  command recording run in parallel.

Storage migration must not claim that per-window memory alone makes Clay calls
thread-safe.

## `InputFieldManager`: option 1

### Storage representation

Create a window-scoped input-state root containing:

- a `StringId -> InputFieldHandle` keyed index;
- the primary field handle rather than a copied primary-field string;
- pointer-drag state using a field handle;
- key-repeat and caret-blink state;
- manager configuration and the per-window revision.

Each `InputFieldRecord` stores mutable UTF-8 text, config, carets, Clay element
IDs, fallback metrics, and last-touched epoch. Text and caret capacity come from
window-persistent storage. Records live in non-relocating slots; no manager
holds a pointer into a rehashing map.

The manager retains all editing policy: UTF-8 boundary handling, insertion,
deletion, multi-caret movement, repeat behavior, hit testing, selection merging,
and caret rendering decisions. Moving those functions into generic storage
would add no ownership or performance benefit.

### Frame behavior

- `beginFrame()` marks presence using an epoch counter. Do not iterate every
  cached field only to clear `touchedThisFrame`; compare
  `lastTouchedEpoch == currentEpoch`.
- `requestField()` interns/looks up the field ID, creates a typed record on first
  use, updates the exclusive window record, and returns a view of its text.
- Runtime hit-test arrays, merged selections, caret-drawn flags, render override
  ranges, and generated rectangles use the frame arena.
- `removeField()` releases the keyed record and repairs focus by handle.
- `clear()` releases all records but does not require destroying/recreating the
  manager facade.

`FieldQueryResult::text` and `getSelectedText()` retain their current
`string_view` shape. Their views are valid until that field's next text mutation,
removal, or window destruction. In future worker mode they are valid only on the
window job that owns the state unless copied by user code.

### Font access and threading

Text measurement receives the immutable `FontFrameView` captured for the
window frame. It does not hold `FontManager*`. The window job is the sole writer
of its input field records; app code must not mutate the same window's fields
concurrently. Cross-window input managers can operate independently.

## `ShortcutManager`: option 1

### Storage representation

Use a window-scoped shortcut root with:

- a packed-chord index to immutable sorted spans of registration handles;
- a `ShortcutId -> registration` index;
- registered-key reference counts or a compact key bitset/count array;
- monotonic next-ID and registration-order counters;
- the focused Clay element ID;
- typed callback records whose destructors run on the app thread.

`ShortcutRegistrationRecord` contains scope, priority, ID, registration order,
packed chord, and callback. Storage owns the `std::function` object and its
captured payload lifetime. The manager owns ordering, scope evaluation, and
dispatch behavior.

For future asynchronous jobs, IDs should not be reused after `clear()`. Resetting
`nextShortcutId_` to one creates an ABA hazard if an old snapshot or queued
unregister refers to an ID that has been assigned again. This is a small
intentional behavior correction; `0` remains invalid, overflow fails clearly,
and no new public API is needed.

### Mutation-safe dispatch

The current implementation copies an entire `std::vector<std::function>` bucket
so callbacks can register/unregister during dispatch. Replace that heap-heavy
copy with an immutable storage dispatch snapshot:

1. Capture a span of strong registration handles for the packed chord.
2. Mark unregisters as tombstoned immediately so later callbacks in the same
   dispatch are skipped.
3. Queue structural bucket changes in a small window mutation batch.
4. Retain callback records until the dispatch snapshot releases them.
5. Publish the new sorted bucket after dispatch.

Registration during a callback should preserve current semantics by becoming
visible to later frame dispatch, not retroactively to the active snapshot.
Unregistration of a not-yet-invoked callback takes effect immediately via the
tombstone check.

No shortcut operation needs Vulkan, GPU retirement, or an app-global lock.
Different window shortcut roots are independent. Callback invocation remains on
the app/window UI thread unless a future explicitly worker-safe callback API is
introduced.

## `ViewPortManager`: option 2

### Why StorageSystem needs semantic viewport operations

A viewport is a window-local mutable render-target set with one image per frame
slot, command resources, layouts, logical texture revisions, user-visible native
views, and exact submission retirement. If `ViewPortManager` separately calls
generic `createImage`, `publishTexture`, and native command-pool helpers, a
failure in the middle can expose mismatched per-frame targets. Correct resize
and removal require StorageSystem to understand and atomically replace the
whole generation.

Add a focused internal viewport operation group to the storage root. Suggested
operations are conceptually:

```cpp
ViewportHandle createViewport(ResourceKey, const ViewportDesc&);
ViewportHandle findViewport(ResourceKey) const;
ViewportSnapshot viewportSnapshot(ViewportHandle) const;
ViewportTargetCandidate buildViewportTargets(ViewportHandle, Extent2D);
void commitViewportTargets(ViewportHandle, ViewportTargetCandidate&&);
ViewportFrameView viewportFrameView(const FrameToken&, ViewportHandle);
void trackViewportUse(const FrameToken&, ViewportHandle);
bool removeViewport(ResourceKey);
```

The concrete names may differ, but create/resize/commit/acquire/remove semantics
must remain grouped and transactional. A Vulkan-specific native view may be
available through a narrow optional capability because public viewport callbacks
are explicitly Vulkan interop. That capability borrows a frame/generation view;
it does not transfer ownership and is not the old texture publication bridge.

### Storage records

A window-scoped `ViewportRecord` stores:

- key, format, clear policy, desired extent, current extent, and revision;
- callback ownership or a stable callback record handle;
- the active `ViewportTargetHandle` generation;
- per-frame logical texture handles;
- last referenced UI/frame epochs.

A `ViewportTargetRecord` owns:

- storage-owned image and image-view handles for every frame slot;
- per-image layout/access state;
- frame command-pool/command-buffer resources with one recording owner;
- the exact texture revision for every frame slot;
- last-use submissions and generation state.

`ViewPort` public facade objects may themselves live in non-relocating
window-persistent storage. `getViewPort()` can then preserve its pointer API:
the pointer is valid until `remove(key)` or window destruction, just as today.
The facade contains a storage/window/viewport handle, not native resource
ownership. Setters update the typed record through `ViewPortManager`/storage.

### Transactional creation and resize

Creation reserves the public facade and record, builds the complete 1x1 target
generation for every frame slot, publishes every logical texture, creates frame
command resources, and publishes the keyed viewport only after all candidates
succeed.

Resize is copy-on-replace:

1. Read desired size from UI command analysis and clamp/validate it.
2. Build all new per-frame images/views and command resources in an unpublished
   candidate generation.
3. Create all new logical texture revisions.
4. Atomically switch the viewport's active generation.
5. New frames acquire only the new generation.
6. Old frame snapshots retain the old generation.
7. Retire old images, views, texture revisions, and command resources after
   their exact target-window submissions complete.

No resize/removal operation calls `vkDeviceWaitIdle()`, drains another window,
or relies on `destroyDrained()` except final already-drained window teardown.
The manager's `retiredImages_` and `retiredViewPorts_` disappear.

### Frame preparation and callbacks

`ViewPortManager` retains the domain/UI behavior:

- scan Clay image commands and calculate desired target sizes;
- choose clear/load policy from the public `ViewPort` settings;
- invoke the user render callback;
- remap a viewport's stable public texture to its current frame-slot logical
  texture.

Storage supplies the exact `ViewportFrameView` for the `FrameToken`, including
the target image/view/layout, command buffer, extent, format, generation, and
resource-use handles. The view expires with the frame lease. The manager records
transitions and invokes the callback using this borrowed frame view, then tells
storage the resulting layout/state. A future render worker owns that view and
command pool exclusively.

`ViewPortVulkanInterop` remains the public compatibility surface. It must be a
borrowed, read-only device identity bundle; no user can destroy or republish its
objects. Per-target native objects are exposed only through the existing
callback context and only for that callback/frame generation.

### Teardown

Removing one viewport releases its active record and defers exact generation
retirement. Destroying a secondary window first cancels/joins its job and drains
its frame submissions, then releases the whole window viewport scope. App-wide
shared image/font/icon storage is unaffected. Final app shutdown may still use
the existing whole-device shutdown policy.

## Public manager shape after migration

The public calls stay recognizable:

```cpp
app.images().registerImage(key, path);
app.images().getTexture(key);
app.icons().registerSvg(key, source);
app.icons().textureRef(key);
app.fonts().createFamily(info);
app.fonts().resolveFont(family, weight, style);
app.ui(window).inputFields().requestField(request);
app.ui(window).shortcuts().registerShortcut(...);
app.viewPorts(window).create(key, info);
app.viewPorts(window).getViewPort(key);
```

Construction changes internally. Each facade is bound once to a storage root and
scope:

```cpp
ImageManager(storage, AppScope);
FontManager(storage, fontCatalogController);
IconManager(storage, iconCacheController);
UiManager(storage, windowId, uiConfig);
InputFieldManager(storage, windowId);
ShortcutManager(storage, windowId);
ViewPortManager(storage, windowId);
```

These constructors remain private to `App`. Managers expose no storage pointer,
record handle, mutation queue, allocator, or native ownership API.

## App and window lifecycle integration

### App initialization

1. Create and initialize StorageSystem.
2. Create shared fallback GPU resources and logical fallback texture in storage.
3. Construct app-shared font/image/icon facades and their private controllers.
4. Register the main window storage scope.
5. Construct window-local UI/input/shortcut/viewport facades against that scope.
6. Initialize the window renderer using storage views, not manager pointers.
7. Transactionally publish default fonts and other configured resources.

Secondary creation constructs only window-local facades/records. It reuses
app-shared font/image/icon resources and must not clone their stores.

### App tick and frame

The quiescent `pollEvents()` safe point performs:

- completion reporting and storage collection;
- publication of queued app-shared manager mutations;
- batched upload progress;
- icon use-epoch/LRU merge and budget trimming;
- diagnostics aggregation.

`beginFrame(window)` captures shared and window revisions, resets frame arenas,
and gives UI/input/shortcut/viewport code exclusive access to that window epoch.
`endFrame(window)` finishes UI policy, resolves icon/viewport demands, prepares
immutable renderer views, tracks every resource generation, and seals the
storage frame. `drawFrame(window)` consumes only the sealed snapshot.

The Phase 4 transitional app-thread gate remains until the later executor phase,
but none of the manager stores should depend on that global gate for correctness.

### Window destruction

1. Stop/cancel/join work for the target window.
2. Drain only its submissions and exact present completions.
3. Release its viewport, UI, input-field, shortcut, development, and frame
   storage scopes.
4. Destroy window renderer/WSI objects in their established order.
5. Unregister the window storage scope.

App-shared images/icons/fonts remain live. Public pointers/string views into the
destroyed window managers become invalid as documented.

### App shutdown

1. Stop manager mutation intake.
2. Cancel or join every outstanding window job.
3. Drain/destroy windows.
4. Release app-shared manager records/controllers.
5. Collect all completed generations.
6. Shut down StorageSystem and finally destroy the device/backend.

Manager destructors become facade cleanup. They do not accept `VulkanContext`,
destroy native objects, or wait for the device.

## Removal of transitional bridges and direct manager ownership

Completion of this migration requires all of the following removals:

- `IUiTexturePublisher` and `UiTexturePublisher`.
- `publishExternalTexture()` and `ExternalTextureDesc` once no other internal
  imported-resource user remains.
- The `BorrowedNativeTextures` capability.
- `ImageManager::setTexturePublisher`, `IconManager::setTexturePublisher`, and
  `ViewPortManager::setTexturePublisher`.
- Manager `VulkanContext*`, manager VMA allocations, manager samplers, manager
  upload command pools, and manager retirement vectors.
- Constructed manager namespace strings.
- `VulkanUiRenderer::setFontManager` and `UiManager`/`InputFieldManager` raw
  `FontManager*` links.

The optional Vulkan viewport frame view is not an ownership bridge. It is a
generation-bound read/record lease over storage-owned native objects and exists
only because the current public viewport feature explicitly permits Vulkan
recording.

## Multithreading preparation and policy

This migration must support both future `SingleThreaded` and `InternalWorkers`
execution modes with one implementation, not two manager backends.

### Thread-affinity contract for the first worker phase

- Window-system operations and public callbacks remain on the app/platform
  thread unless an API explicitly says otherwise.
- App-shared manager mutations execute on one serialized resource/publication
  lane. Reads use immutable published revisions.
- Each window UI/input/shortcut state has one exclusive job owner per epoch.
- Icon/font decoding and raster/bake work may run in producer jobs, but commit is
  serialized through the publication lane.
- Viewport command recording can run in its owning window job with exclusive
  command-pool/frame-target leases.
- Vulkan queue submission remains externally synchronized.
- A manager API never returns a mutable reference to a record that another
  worker may concurrently replace.

The single-thread executor performs the same stages inline. It remains the
deterministic reference implementation and race-debugging baseline.

### Mutation timing

Calls made before app-shared snapshot publication may be visible to all window
jobs in the current tick. Calls made after jobs have captured that revision are
committed for the next revision unless the frame explicitly waits on a
same-frame resource lane operation, as icon size resolution currently requires.

Window-local UI/input/shortcut mutations made by the owning window job are
visible immediately to that job. Mutating a manager for a window whose job is
running from another thread is rejected or queued by an explicit future API;
it must never race silently.

### Data structures for concurrency

- Immutable shared font faces and published image/icon records are lock-free
  frame reads.
- Table mutation takes short domain/table locks or uses a serialized mutation
  queue, never one global draw lock.
- Interning may initially lock because it is not a glyph/quad hot path.
- Icon demand and use tracking are per-job arrays merged at publication.
- Frame and worker scratch are isolated arenas.
- Callback records are retained by snapshot handles while invocation is active.
- No manager stores pointers into resizable vectors or hash tables.

## Performance and memory-layout requirements

The migration should improve the hot path rather than just centralize it.

- Use node-free/flat key indexes where stable pointers are not required and
  paged record slots where they are.
- Split hot immutable font metrics/glyph arrays from cold names, paths, metadata,
  and diagnostics.
- Store icon variant lookup keys as compact IDs and dimensions; keep atlas free
  rectangles contiguous per page.
- Use epoch stamps instead of clearing every input-field/icon record each frame.
- Store registered shortcut keys in a fixed-size count array or bitset matching
  `FrameInput::kKeyboardKeyCount` when measurement supports it.
- Deduplicate normalized samplers across image, font, icon, and viewport domains.
- Batch uploads from all managers through one staging/scheduling system.
- Ensure steady-state manager queries and frame preparation make no general
  heap allocations.
- Include live and retired CPU/GPU bytes by manager domain in StorageStats.
- Measure cache hits, icon raster misses/evictions, font lookup probes, input
  field counts/text capacity, shortcut dispatch snapshot sizes, and viewport
  generation churn.

## Error and lifetime rules

- Empty/invalid keys retain each manager's documented error behavior.
- A failed create or replacement never changes a committed key.
- A failed font family creation leaves no partial family or face mapping.
- A failed icon variant allocation leaves the request alias on its previous
  revision or fallback.
- A failed viewport resize leaves the old complete target generation active.
- A stale generational handle never resolves to a recycled record.
- User-facing logical texture handles remain stable across physical replacement
  where current semantics require an alias; descriptor indices remain
  window-local and invisible.
- Public pointer/reference/string-view queries keep explicit validity windows.
- Removal is logical immediately and physical only after all snapshot and GPU
  uses complete.
- Normal manager mutation, resize, removal, eviction, and window destruction do
  not call `vkDeviceWaitIdle()`.
- Third-party opaque allocations are either redirected or explicitly accounted,
  not falsely reported as StorageSystem-owned bytes.

## Migration sequence

### Stage 1: common manager-record infrastructure

- Add typed non-relocating CPU record tables and destructor-aware retirement.
- Add keyed manager record publication and transaction support.
- Add domain/scope telemetry and diagnostic once-sets.
- Add shared/window publication revisions and immutable manager frame views.
- Add failure injection before moving any manager.

### Stage 2: `ImageManager`

- Move decode blobs, images, views, samplers, uploads, logical keys, records, and
  retirement to storage.
- Preserve synchronous visible behavior initially.
- Validate replacement/removal under two in-flight windows.
- Use this simplest manager to prove the transaction API.

### Stage 3: `UiManager`, `InputFieldManager`, and `ShortcutManager`

- Move Clay and frame allocations first.
- Bind the nested manager facades to the same window storage scope.
- Move persistent field/callback records and frame scratch.
- Establish the exact window job/snapshot ownership contract.
- Keep execution inline and verify behavior equivalence.

### Stage 4: `FontManager` and font consumers

- Add `FontCatalogController` and storage-owned immutable catalog tables.
- Move atlas resources/uploads and implement transactional growth.
- Replace manager pointer borrowing in UI, input, and renderer with font frame
  views.
- Validate public pointer/reference compatibility and atlas generations.

### Stage 5: `IconManager`

- Add `IconCacheController` and storage-owned document/page/variant records.
- Move parsing ownership, atlas allocation, uploads, logical aliases, LRU state,
  and retirement.
- Split window demand collection from app-shared cache mutation.
- Validate deterministic multi-window cache behavior.

### Stage 6: `ViewPortManager`

- Add focused viewport target operations and generation-bound Vulkan frame
  views.
- Move target images, views, samplers, command resources, texture revisions,
  resize transactions, and retirement.
- Preserve callback and public `ViewPort` behavior.
- Delete drained-manager native destruction logic once window scope release is
  authoritative.

### Stage 7: remove compatibility paths

- Delete `IUiTexturePublisher`, borrowed native texture publication, manager
  Vulkan ownership, private upload pools, private retirement buckets, and
  manager-to-manager font pointers.
- Make storage stats account for every migrated category.
- Run source scans that fail if forbidden ownership types return to manager
  headers/sources.

### Stage 8: executor-ready seal

- Confirm inline execution uses publication batches and immutable snapshots.
- Document app-thread callbacks and manager mutation timing.
- Benchmark and tune initial reservations from telemetry.
- Only then add configurable worker execution; no manager data should move again.

## Required validation

### Compatibility and facade tests

- Compile every existing public manager method family and normal usage pattern.
- Verify main-window no-argument and explicit `WindowId` manager access.
- Verify optional IconManager-off and public-Vulkan-interop-off builds.
- Verify public pointers/views remain valid for their documented lifetime.

### Key, ID, and scope tests

- Same key in image/icon/font/viewport/input domains does not collide.
- App-shared image/font/icon lookup returns the same logical resource in two
  windows while bindings remain distinct.
- UI/input/shortcut/viewport records do not leak across windows.
- Removed/replaced generations reject stale handles.
- Font, family, and shortcut IDs do not create ABA aliases.

### Transaction and fault-injection tests

- Fail every image decode/blob/image/view/sampler/upload/publication step.
- Fail every face parse/atlas growth/upload/family publication step.
- Fail SVG parse/page creation/region upload/variant publication.
- Fail each viewport frame image, command resource, texture, and commit step.
- In every case, compare all pre-existing records, native object counts,
  registry revisions, and live/retired byte statistics before and after.

### Manager-specific behavior tests

- Image insert/replace/remove/missing/fallback and dimension metadata.
- Font family duplicate handling, weight/style selection, baked/runtime faces,
  glyph/kerning equivalence, atlas growth, and failed multi-face family rollback.
- Icon tolerance reuse, UVs, padding, free-rect merge, maximum pages,
  deterministic LRU, removal, wrap-safe use epochs, and in-flight region reuse.
- Input UTF-8 insertion/deletion, multi-caret, selection, focus loss, untouched
  fields, limits, pointer drag, repeat, and returned view lifetimes.
- Shortcut priority/scope/order, callback-side register/unregister, clear,
  callback capture destruction, and ID exhaustion.
- Viewport create/remove/resize/minimize, per-frame texture remap, clear/load,
  callback context, format changes, and in-flight close.
- UI arena growth, cancellation, interaction snapshot rotation, Clay capacity
  failure, and development data accounting.

### Multi-window and synchronization tests

- Two windows consume the same image/icon/font while running different frame
  slots and rates.
- Each window independently mutates input fields, shortcuts, UI interactions,
  and viewports.
- Shared resource replacement between two sealed/submitted windows preserves
  both physical generations until their exact completions.
- Closing one in-flight secondary window retires only its UI/viewport state.
- Icon demands from two windows merge deterministically without duplicate
  rasterization.
- Font atlas growth while both windows use the old generation is exact and
  device-idle-free.

### Storage and performance tests

- Every manager allocation appears under the correct app/window/frame/domain
  memory category.
- Steady-state UI, input, shortcut dispatch, cached icon, image lookup, font
  layout, and unchanged viewport frames allocate no general heap memory.
- Shared samplers and immutable assets are not duplicated per window.
- Budget trimming evicts eligible icon variants but never active viewport or
  pinned font/image records.
- No source-level manager path contains `vkDeviceWaitIdle`, manager-owned
  `VkImage`/VMA allocation/upload command pool, `publishExternalTexture`, or a
  private retirement bucket.

Run development and release builds, all existing storage/types/renderer/manager
and Vulkan tests, real two-window Vulkan tests, AddressSanitizer,
UndefinedBehaviorSanitizer, ThreadSanitizer when worker tests exist and the
platform supports it, Vulkan validation when installed, and `git diff --check`.

## Acceptance criteria

The manager-storage migration is complete only when:

- Every long-lived manager record and FlowUi-controlled allocation is owned and
  accounted by StorageSystem under the correct app/window/frame scope.
- Managers own no Vulkan/VMA resource, upload pool, descriptor identity,
  retirement queue, or resizable long-lived keyed container.
- `IUiTexturePublisher` and borrowed external manager texture publication are
  gone.
- Image, font, icon, and viewport creation/replacement are strongly
  transactional.
- Fonts/images/icons are physically app-shared; UI/input/shortcut/viewport state
  is physically window-local.
- Manager queries resolve logical keys/IDs to stable records without exposing
  descriptor slots or StorageSystem configuration.
- Renderer and UI text consumers use sealed storage views instead of borrowing
  `FontManager`.
- Normal removal, resize, eviction, and secondary-window destruction use exact
  retirement and never device idle.
- Inline single-thread execution and future worker execution consume the same
  mutation/snapshot interfaces.
- No manager data must be moved or re-owned when configurable multithreading is
  introduced.

## Final recommendation

Do this manager migration before enabling broad internal worker execution, but
design and implement the mutation batches, frame snapshots, and exclusive
window ownership epochs as part of the migration. The managers are currently
the largest remaining islands of mutable ownership. Threading them first would
require synchronizing manager maps, native resources, private upload pools, and
StorageSystem generations simultaneously, then deleting much of that
synchronization after migration.

The correct boundary is:

- public managers express user intent and retain domain behavior;
- `FontCatalogController` and `IconCacheController` retain only their complex
  private policies;
- the focused viewport storage API owns atomic target-generation behavior;
- StorageSystem owns every durable record, physical resource, publication
  revision, and retirement decision;
- window/frame jobs later consume immutable storage snapshots and isolated
  transient arenas.

That produces a clean end to the StorageSystem conceptual upgrade and a stable
base for offering both deterministic single-threaded internals and configurable
worker-backed execution without changing how users build applications.
