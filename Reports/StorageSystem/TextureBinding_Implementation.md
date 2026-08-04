# Phase 3 texture binding implementation proposal

## Purpose and recommended result

Phase 3 should replace renderer-local texture slot numbers with app-level,
generation-checked texture identity and resolve that identity into a descriptor
index separately for each `AppWindow` and Vulkan frame slot. It should not expose
secondary-window creation, migrate every manager allocation to storage, or change
the public single-window execution model yet.

The completed data path should be:

```text
manager key
    -> logical TextureHandle in TextureRef
    -> icon/viewport pre-seal resolution, when applicable
    -> one batched storage binding preparation for this AppWindow/frame
    -> dirty Vk descriptor writes for this safe frame slot only
    -> logical handle -> window-local descriptor index during instance emission
    -> UiInstance.texIndex -> existing indexed-texture shader
```

The important distinction is that `TextureHandle` is a resource identity, not a
descriptor index. The same handle may resolve to descriptor 7 in one window and
descriptor 19 in another. No Clay command, manager record, or public object should
retain either of those local indices.

This follows `Ui_renderer_storage_integration.md`, but incorporates the actual
Phase 1 and Phase 2 implementation:

- `AppWindow` already owns window/UI/frame/descriptor state, while `App::Impl`
  owns the device, storage, and shared managers.
- instance emission now occurs before `sealFrame()`, directly into a storage
  buffer. The original proposal's idea of using a post-seal binding view during
  conversion therefore cannot be copied literally.
- renderer byte resources are storage-owned, but image, icon, font, and viewport
  managers still own raw Vulkan/VMA resources.
- the current storage binding limits (512 initial, 4096 maximum) disagree with
  the renderer and shader limit of 256. Phase 3 must make one limit authoritative.

## What exists now and what must disappear

Today, `TextureRef::id` is both public identity and a descriptor-array index.
`UiTextureRegistry` allocates those indices and writes native manager bindings
into the main renderer. Its growth path calls `vkDeviceWaitIdle()`, recreates the
descriptor layout and pipelines, and rewrites the full array. Images store one
slot, icon requests and atlas pages store slots, and viewports store one slot per
Vulkan frame.

This has three correctness problems for the target architecture:

1. a slot belongs to one renderer/window, so an app-shared manager cannot return
   it as an app-wide identity;
2. an index without a generation can silently refer to a different resource after
   reuse;
3. frame-bucket retirement is only an approximation of GPU completion, whereas
   storage already tracks the exact submission serial.

Phase 3 should remove `UiTextureRegistry`, `IUiTextureRegistry`, slot allocation,
`uiTextureSlotInfos_`, descriptor-wide rebuilds, and runtime descriptor-layout
growth. Manager records should retain logical handles instead of `slotId`.

## TextureHandle and TextureRef

### Recommended public representation

Introduce a small public `FlowUi::TextureHandle` in its own lightweight header,
with zero invalid and the complete index/generation pair. The internal storage
texture handle should alias or losslessly convert to this canonical type; do not
publish `detail::storage` names in the public API.

```cpp
namespace FlowUi {

struct TextureHandle {
    uint32_t index = 0;
    uint32_t generation = 0;

    [[nodiscard]] constexpr explicit operator bool() const noexcept {
        return index != 0 && generation != 0;
    }

    [[nodiscard]] constexpr uint64_t packed() const noexcept {
        return (uint64_t{generation} << 32u) | index;
    }

    friend constexpr auto operator<=>(const TextureHandle&, const TextureHandle&) = default;
};

struct TextureRef {
    TextureHandle handle{};
    float uv0x = 0.0f;
    float uv0y = 0.0f;
    float uv1x = 1.0f;
    float uv1y = 1.0f;
    TextureFitMode fitMode = TextureFitMode::Contain;
    TextureSamplingMode samplingMode = TextureSamplingMode::Linear;
    bool tintEnabled = false;
    int32_t sourceWidth = 0;
    int32_t sourceHeight = 0;
};

} // namespace FlowUi
```

The exact invalid index convention must match storage. If storage retains its
current generic `Handle<ResourceKind::TextureView>`, use explicit conversion
helpers with compile-time layout checks instead of `reinterpret_cast`.

`TextureRef::id` should not remain as a deprecated 32-bit alias: it cannot carry
the generation and would allow old code to compile with unsafe semantics. This is
an intentional source change for code that manually reads or writes `.id`.
Manager lookup calls may keep returning `TextureRef` in this phase, so ordinary
code that treats it as an opaque value changes little.

`TextureRef` remains per-draw data. UVs differ between icon atlas regions, and
fit/tint settings may differ between two draws of the same resource. The
`TextureHandle` inside it is the stable resource identity.

### What the handle does and does not own

A copied public handle is a weak, generation-checked identity, not an RAII owner.
Publishing the texture gives storage its strong publication reference. Preparing
a frame adds a frame-use reference, and submission stamps the exact serial. Thus a
manager may replace/remove a texture without invalidating an already submitted
frame. A stale handle in a later frame resolves to slot-zero fallback rather than
to a reused resource.

The handle should not keep a removed manager resource alive forever merely because
application code copied it. Its documented validity ends when the manager removes
or replaces the corresponding logical resource.

### Important Clay lifetime distinction

A logical handle removes the need for callers to retain a GPU resource or a
descriptor slot through the frame. It does **not**, by itself, make this safe:

```cpp
config.imageData = &app.images().getTexture("logo"); // address of a temporary
```

Clay currently stores `imageData` as `void*`, and the renderer reads a complete
`TextureRef` through that pointer after layout. UV, fit, tint, and source dimensions
do not fit in a pointer-sized handle. Encoding an arbitrary 64-bit handle as a
fake pointer would be implementation-defined and would also lose those per-draw
fields. The report therefore rejects that shortcut.

For Phase 3, retain the frame-arena copy but rename/reframe it as an image-payload
adapter, not resource retention:

```cpp
void* UiManager::imageData(TextureRef texture) {
    return copyToCurrentFrameArena(texture);
}

// Transitional: raw Clay requires pointer-shaped payload storage. A later
// FlowUi image element accepts TextureRef by value and performs this internally.
TextureRef* UiManager::storeTexture(const TextureRef& texture); // compatibility
```

This permits the common use to be one expression instead of a manual two-step
"lookup then store" sequence:

```cpp
.imageData = app.ui().imageData(app.images().getTexture("logo"))
```

The desired final API is a FlowUi image element/helper that accepts `TextureRef`
or `TextureHandle` by value and stages the Clay payload internally:

```cpp
ui.image(app.images().getTexture("logo"), imageLayout);
```

At that point users receive only the manager handle/reference they need and never
call `storeTexture()`. Phase 3 establishes the functional prerequisite: manager
results contain no renderer-local state, and storage automatically pins every
resolved handle used by the frame. Exact manager and higher-level element API
cleanup can remain a later public-API pass. Raw Clay users will still need the
one-call `imageData()` bridge unless Clay's payload contract itself changes.

Strings are different: their bytes really must remain available to Clay, so
`toClayString()`/string interning remains. Texture resource lifetime no longer
depends on a copied `TextureRef`, although its per-command payload bytes still do.

## Multi-window renderer ownership and shared consumption

### One renderer facade per AppWindow

Yes: keep one `VulkanUiRenderer` object per `AppWindow`. It contains mutable,
window-specific state:

- per-frame instance-buffer handles;
- per-frame descriptor sets and applied binding state;
- target/swapchain format;
- prepared frame/run spans;
- font-binding revision per frame slot;
- the owning `WindowId` and window-local binding view.

Do not share this mutable facade between windows. Two windows can prepare different
frames concurrently and the same logical texture may occupy a different descriptor
index in each.

Each window renderer consumes shared data through immutable handles/references:

```cpp
struct App::Impl {
    VulkanContext vk;
    std::unique_ptr<IStorageSystem> storage;
    SharedUiByteResources sharedUiBytes;
    FontManager fonts;
    ImageManager images;
    IconManager icons;
    std::unordered_map<WindowId, std::unique_ptr<AppWindow>> windows;
};

struct AppWindow {
    WindowId id;
    UiManager ui;
    ViewPortManager viewPorts;
    VulkanUiRenderer renderer; // mutable window facade
};
```

The quad buffer, placeholder/fallback texture, app images, icon atlas pages, font
atlas, and one `FlowStorageSystem` are shared at app/device scope. Descriptor sets,
descriptor indices, frame buffers, and binding revisions remain per window.

Raw pipeline/layout objects are still duplicated in the current per-window
renderer. Phase 3 need not expand into the Phase 4 pipeline-sharing migration.
Mark their ownership:

```cpp
// Transitional: Phase 4 publishes/acquires renderer layout and pipeline bundles
// by compatible target format so AppWindows share immutable Vulkan objects.
VulkanUiRenderer::Pipelines pipelines;
```

This preserves the implemented scope while keeping the intended direction clear.

### Shared managers and window visibility

Image and icon logical textures use `ResourceSharing::AppShared`, so storage can
resolve them for any registered app window. Viewport targets remain owned by an
`AppWindow` and use `WindowLocal`/`FrameLocal`; storage must reject resolving them
from a different window. The renderer does not ask a manager for native Vulkan
objects in a draw loop. It receives native image-view/sampler pairs only in the
coarse dirty binding batch.

Phase 3 should still create only the main AppWindow. Multi-window correctness is
proved with focused storage/renderer tests using two internal window scopes; the
public create/destroy APIs remain Phase 4.

## Shader and descriptor model

### No logical-handle lookup in shaders

The shader should not receive a `TextureHandle`. CPU preparation resolves the
handle to the current window's descriptor index and writes that index into the
existing `UiInstance::texIndex`. Consequently, the vertex/fragment interface and
the `nonuniformEXT` descriptor lookup remain conceptually unchanged.

This is deliberate: generations, fallback selection, revisions, scope checks, and
resource state belong in storage/CPU preparation, not in every fragment.

### Fix the current capacity disagreement

The shader currently declares 256 image descriptors and clamps to 255, while the
storage window defaults to 512 initial and 4096 maximum. Use one internal effective
capacity, initially 256 for source compatibility, and pass it to both renderer and
`WindowStorageDesc`:

```cpp
struct AppWindowConfig {
    WindowConfig native{};
    VulkanConfig vulkan{};
    UiConfig ui{};
    uint32_t uiTextureDescriptorCapacity = 256; // internal in Phase 3
};

windowStorage.initialTextureBindings = effective.uiTextureDescriptorCapacity;
windowStorage.maxTextureBindings = effective.uiTextureDescriptorCapacity;
renderer.init(..., effective.uiTextureDescriptorCapacity);
```

Validate the value against Vulkan sampled-image/per-stage/update-after-bind limits
at device setup. Slot zero is always the real storage fallback, so the usable
non-fallback count is capacity minus one.

The recommended Phase 3 policy is fixed capacity per window: allocate each
per-frame descriptor set once and remove runtime layout/pipeline growth and its
device-idle wait. Exhaustion should be transactional and deterministic—keep all
existing assignments and resolve excess/new textures to fallback with a diagnostic,
or fail the frame before descriptor writes. Storage currently can mutate some
assignments before throwing in a large batch, so the batch allocator should first
preflight required new indices or roll back on failure.

An alternative is a runtime-sized shader array plus a new descriptor-bundle
generation. That can support configurable capacities, but it requires synchronized
shader/layout limits and serial-retired pools. It is more work and gives no benefit
for the present 256 limit; defer it unless real workloads demonstrate a need.

At minimum, replace the literal shader clamp with a generated/shared shader
constant if shader build tooling supports it. No visual or texture-identity shader
rewrite is required.

### Fonts stay separate

Keep the font atlas at set 1, binding 0 as one `sampler2DArray`. Glyph instances use
an atlas layer and the MSDF shader has different sampling semantics. Each window's
descriptor set binds the same app-shared font atlas view/sampler and tracks its
revision independently.

Keep icons, ordinary images, and viewport/custom targets in the indexed array at
set 1, binding 1. An icon atlas page consumes one descriptor; individual icons on
that page differ only by UVs and do not consume one descriptor each.

`TextureSamplingMode` is currently not implemented. Since a combined image sampler
is selected by logical texture binding, genuinely different linear/nearest modes
would need separate logical texture variants or a future separate sampler-index
scheme. Keep the field and its no-op behavior clearly documented in Phase 3 rather
than implying the shader now honors it.

## Binding preparation with the implemented Phase 2 ordering

Phase 2 commits direct-emitted instances before `sealFrame()`. A
`WindowBindingView` currently requires a post-seal `FrameReadLease`, so the old
roadmap's “seal, then resolve while emitting” order is no longer legal.

The recommended fix is a coarse pre-seal snapshot returned by the existing batch
operation:

```cpp
struct PreparedTextureBindings {
    std::span<const DescriptorWriteRecord> dirtyBindings;
    std::span<const BindingHotRecord> bindingsByTextureIndex; // pre-seal snapshot
    uint32_t requiredDescriptorCapacity = 1;
    FrameEpoch epoch = 0;

    const BindingHotRecord* binding(TextureHandle handle) const noexcept;
};
```

The span must be stable until the frame is sealed/cancelled and validated by the
frame epoch in development mode. Prefer an arena-owned snapshot if future parallel
publication could resize the window table; otherwise explicitly guarantee that no
binding mutation occurs between preparation and immediate conversion.

The per-window sequence becomes:

```cpp
// Producer-specific rewrites happen first.
viewPorts.resolveFrameTextures(commands, frame.frameSlot);
icons.resolveFrameTextures(commands, scaleX, scaleY);

auto unique = gatherUniqueTextureHandles(commands, frameArena);
auto prepared = storage.prepareTextureBindings(frame, unique);

renderer.applyDirtyBindings(device, frame.frameSlot, prepared.dirtyBindings);
storage.acknowledgeTextureBindings(frame, prepared.dirtyBindings);

PreparedUiFrame uiFrame = renderer.prepareFrame(
    storage, frame, commands, prepared.bindingsByTextureIndex, ...);

lease = storage.sealFrame(frame);
```

Gather with an arena array and sort/unique packed `(generation,index)` values, or
use an arena epoch-marker table plus generation comparison. Do not introduce a
heap `unordered_set`. Invalid handles may be omitted or included; either way they
must resolve to fallback index zero.

Instance conversion becomes an array lookup, not a storage virtual call:

```cpp
const BindingHotRecord* binding = prepared.binding(textureRef.handle);
instance.texIndex = binding ? binding->descriptorIndex : 0u;
```

`prepareTextureBindings()` already tracks the logical texture and fallback as
frame uses. The existing `noteSubmission()`, exact-slot fence completion,
`noteCompleted()`, cancellation guard, and `collect()` lifecycle remains unchanged.
If descriptor update or direct emission throws, the frame guard cancels those uses.

### Dirty descriptor writes

Allocate `VkDescriptorImageInfo` and `VkWriteDescriptorSet` arrays from the frame
arena, fill only `prepared.dirtyBindings`, and issue one `vkUpdateDescriptorSets`
for the current safe frame-slot set. Acknowledge only after the Vulkan call returns.
Storage's per-frame-slot applied revision array then decides what is dirty next
time that slot is reused.

Do not update descriptor sets belonging to an in-flight slot and do not rebuild the
whole descriptor array. With fixed capacity, renderer descriptor sets remain raw
Vulkan control objects for now:

```cpp
// Transitional: Phase 4 adopts/shares renderer object bundles through storage.
Descriptors descriptors_;
```

## Transitional bridge for manager-owned Vulkan resources

`publishTexture(TextureViewDesc)` currently accepts only storage-owned
`ImageViewHandle` and `SamplerHandle`. Image, icon, and viewport managers still
own raw VMA images/views/samplers, so merely replacing `slotId` with
`publishTexture()` will not compile or be lifetime-safe.

Migrating all three stores now would also exceed Phase 3: icon atlas uploads occur
during frame preparation, while the current storage upload path is synchronous and
requires the shared mutation phase. Use one deliberately narrow internal bridge:

```cpp
struct ExternalTextureDesc {
    uint64_t nativeImageView = 0;
    uint64_t nativeSampler = 0;
    ResourceSharing sharing = ResourceSharing::AppShared;
    WindowId window = InvalidWindowId;
    uint32_t frameSlot = InvalidFrameSlot;
    int32_t sourceWidth = 0;
    int32_t sourceHeight = 0;
};

// Transitional: removed when managers allocate images/views/samplers in storage.
TextureHandle publishExternalTexture(ResourceKey, const ExternalTextureDesc&);
```

The corresponding storage hot record may use a tagged backing: storage-owned
view/sampler handles or borrowed native values. Borrowed values are never destroyed
by storage. They are safe only with an explicit retirement handshake:

1. manager publishes/replaces/removes the logical texture;
2. storage keeps the retiring logical generation until all frame uses and its last
   submission serial complete;
3. the manager puts the raw resource in a retirement queue paired with that old
   logical handle;
4. after `collect()`, the adapter destroys the raw resource only when
   `validateHandle(TextureView, index, generation)` is false.

For replacement, a new generation/handle is preferred for external bindings. Do
not mutate the native backing in place while older frames may still use the same
logical generation. A dedicated `ExternalTextureRetirementTicket` would be a
cleaner alternative to polling and is worth choosing if storage interface clarity
outweighs one extra internal type.

Every bridge declaration and use should carry this marker:

```cpp
// Transitional: manager-native binding bridge; removed when this manager's
// images, views, samplers, uploads, and retirement are owned by storage.
```

Do not use a callback from storage into arbitrary manager code while holding the
storage mutex. Do not destroy a manager VMA image merely because a logical key was
removed; its last submitted generation may still be sampled.

## Complete treatment of current texture producers

### Ordinary ImageManager images

Each registered key publishes one app-shared external logical texture. The manager
record stores `TextureHandle`, source dimensions, and its existing raw resource.
`getTexture()` returns that handle and dimensions. Replacement publishes a new
generation and retires the old raw resource by the handshake above. Missing/stale
keys return an invalid handle, which binds the storage fallback.

### IconManager atlas

An icon document/request and the atlas page that is sampled are not the same thing.
The least disruptive Phase 3 design preserves the current two-stage resolution:

- each icon request key gets a stable logical request handle backed by fallback;
- the icon manager maps that packed request handle to its SVG key;
- each atlas page publishes one app-shared logical texture handle;
- pre-seal icon preparation rasterizes/selects a variant and rewrites the command's
  request handle to the page handle, then writes the atlas UVs and source size.

Request handles must never reach renderer conversion unresolved; add a development
assert and use fallback in production if resolution fails. All variants on one
page share a descriptor. Atlas-region eviction does not retire the page handle;
page destruction/replacement uses the external retirement handshake.

This request-handle-as-fallback convention is transitional. A later icon-manager
storage migration should use storage's SVG/icon variant resource types explicitly
and return a resolved texture reference through a manager/UI preparation service.

### Viewports and custom render targets

A single mutable logical handle cannot be rebound to a different per-frame image:
older Vulkan frames may still sample the previous backing. Keep one logical texture
handle for every viewport Vulkan frame-slot image. Also keep a stable request
handle, backed by fallback, so UI can name the viewport before the target size and
slot are finalized.

During pre-seal preparation, the window-owned `ViewPortManager`:

1. scans request handles and computes desired target sizes;
2. transactionally creates/replaces the raw per-slot target if required;
3. publishes a `WindowLocal`/`FrameLocal` logical handle for the exact frame slot;
4. rewrites the Clay payload to that handle and current source dimensions.

Viewport resize retires the old per-slot handle/resource by exact submission
completion, not the old frame-bucket assumption. Other custom render targets
exposed through the viewport callback use this same path. If a future API accepts
user-owned Vulkan targets directly, it should publish through an explicit borrowed
external registration with owner, scope, layout, and retirement rules; it should
not accept a naked descriptor index.

### Fallback and placeholder

Publish the Phase 2 storage-owned placeholder UI view/sampler as one app-shared
logical texture during app initialization and call `setFallbackTexture()`. It owns
slot zero in every window and is tracked automatically for invalid, not-ready, or
failed resources. This is distinct from the font placeholder.

### Font atlas

The font manager remains a semantic shared manager but is not converted into
`TextureRef` in this phase. Its MSDF array view/sampler and revision continue to be
bound separately in every window descriptor set. Its eventual storage resource
migration is independent of logical UI image identity.

## User-visible changes

Most users who only retain a manager-returned `TextureRef` see a field migration,
not a rendering-model change. Code that treats `.id` as a descriptor slot must
change; that value was never portable across windows. Missing and stale resources
continue to draw the fallback. `AppConfig::window` and all no-argument main-window
APIs remain unchanged.

### Example 1: an ordinary image

How it used to be:

```cpp
FlowUi::TextureRef logo = app.images().getTexture("logo");
Clay_ImageElementConfig image{};
image.imageData = app.ui().storeTexture(logo);
```

How it will be now in the Phase 3 raw-Clay bridge:

```cpp
Clay_ImageElementConfig image{};
image.imageData = app.ui().imageData(app.images().getTexture("logo"));
```

The returned reference contains a logical generational handle. The `imageData()`
call only stages per-command bytes for Clay; storage automatically keeps the GPU
resource valid through submission. A later FlowUi image helper absorbs even that
bridge.

### Example 2: an icon with per-draw options

How it used to be:

```cpp
FlowUi::TextureRef save = app.icons().textureRef("toolbar/save");
save.fitMode = FlowUi::TextureFitMode::Contain;
save.tintEnabled = true;
image.imageData = app.ui().storeTexture(save);
```

How it will be now:

```cpp
FlowUi::TextureRef save = app.icons().textureRef("toolbar/save");
save.fitMode = FlowUi::TextureFitMode::Contain;
save.tintEnabled = true;
image.imageData = app.ui().imageData(save);
```

The icon request handle is resolved pre-seal to its atlas-page logical handle and
UVs. Neither user code nor the command observes the page's window-local descriptor
index. With the later by-value FlowUi image element, the final line becomes
`ui.image(save, imageLayout)`.

## Scoped implementation sequence

1. Add canonical public `TextureHandle`, update `TextureRef`, conversions, docs,
   and compile-time packing/generation tests.
2. Publish the Phase 2 UI placeholder as storage fallback and reconcile the
   renderer/storage/shader descriptor capacity at 256.
3. Extend storage with the minimal borrowed external-texture variant and exact
   retirement contract; add replacement, stale-generation, scope, and in-flight
   tests.
4. Replace `IUiTextureRegistry` with the transitional logical texture publisher;
   migrate ImageManager, icon request/page records, and viewport request/per-slot
   records from `slotId` to `TextureHandle`.
5. Change icon/viewport command preparation to rewrite logical request handles to
   final sampleable handles before binding gather.
6. Extend `prepareTextureBindings()` with a stable pre-seal binding snapshot,
   gather unique handles in the frame arena, apply only dirty descriptor writes to
   the current frame set, then acknowledge them.
7. Pass the prepared binding snapshot into direct instance emission and write its
   checked descriptor index into `UiInstance::texIndex`.
8. Remove the legacy registry, slot tables, full-array descriptor rewrites,
   capacity growth/device-idle path, and all renderer/manager `slotId` fields.
9. Add `UiManager::imageData(TextureRef)` and retain `storeTexture()` only as a
   deprecated/transitional compatibility alias. Update examples and internal
   elements without changing string lifetime handling.
10. Mark every remaining manager-native bridge and raw renderer-object boundary
    with `//Transitional:` and the phase that removes it.

## Tests and validation required

- public handle packing, invalid value, generation mismatch, and `TextureRef`
  trivial-copy/layout checks;
- one logical texture resolving to independent indices in two registered internal
  windows;
- fallback for invalid, stale, failed, and unresolved request handles;
- one dirty descriptor write on first use, no write after acknowledgement for the
  same frame slot, and a new write after replacement/revision;
- no update to an in-flight frame-slot descriptor set;
- descriptor-capacity preflight/rollback and slot-zero reservation;
- ImageManager replacement/removal with the old native resource retained through
  its last submission;
- icon request-to-page rewrite, multiple icons sharing one page descriptor, and
  page retirement;
- viewport request-to-exact-frame-slot rewrite, resize while another slot is in
  flight, and cross-window scope rejection;
- direct conversion using the prepared span with no per-image storage call and no
  renderer-local slot in `TextureRef` or Clay commands;
- raw Clay `imageData()` frame lifetime and the deprecated compatibility alias;
- source regression checks proving `UiTextureRegistry`, `slotId`, descriptor-wide
  rebuild, and texture-capacity `vkDeviceWaitIdle()` paths are gone;
- full release and development builds, storage/type/renderer/manager/Vulkan tests,
  validation layers when available, and `git diff --check`.

Phase 3 is complete when the same logical image can be prepared for two internal
window scopes with different descriptor indices, all final image commands contain
full generation-checked logical identity, direct conversion performs only checked
span lookup, dirty updates are frame-slot-safe, and manager-owned native resources
cannot be destroyed before storage completes their last use.

## Remaining work after Phase 3

- Phase 4 exposes multi-window creation/destruction, checks presentation support
  per surface, shares renderer layout/pipeline bundles by compatible format, and
  removes device-wide idle from independent window recreation/closure.
- Later manager-storage phases replace the borrowed-native adapter with
  storage-owned image/view/sampler/upload records, including asynchronous icon
  atlas region uploads and storage-owned viewport targets.
- The public UI layer should accept texture references by value so raw Clay payload
  staging is completely hidden; `storeTexture()` and the transitional `imageData()`
  bridge can then be removed.
- A later sampler policy may implement `TextureSamplingMode` using logical sampler
  variants or separate image/sampler indexing.
- Phase 5 can parallelize per-window preparation/recording once mutation barriers
  and immutable snapshots are proven.

## Proposed implementation prompt

> Implement Phase 3 exactly as scoped in `TextureBinding_Implementation.md`, first
> inspecting the working tree and preserving all unrelated changes. Keep current
> single-window public behavior and do not expose secondary-window creation. Add a
> canonical public generation-checked `FlowUi::TextureHandle` and change
> `TextureRef` from renderer-local `id` to the full handle. Publish the Phase 2 UI
> placeholder as storage's app-shared fallback. Make one internal effective
> descriptor capacity authoritative across storage, renderer, and shader (preserve
> the current 256 capacity), preflight capacity transactionally, and remove
> device-idle descriptor growth.
>
> Add the narrowly scoped `//Transitional:` borrowed-native texture publication and
> exact serial/handle retirement bridge needed while ImageManager, IconManager, and
> ViewPortManager still own VMA resources. Replace all manager `slotId` state with
> logical handles: images publish one app-shared handle per resource generation;
> icons use request handles resolved pre-seal to app-shared atlas-page handles and
> UVs; viewports use request handles resolved pre-seal to the exact window/frame-
> local target handle. Do not migrate manager allocations/uploads or the font atlas
> store in this phase.
>
> Extend the batched storage binding result with an epoch-checked pre-seal binding
> snapshot compatible with Phase 2 direct emission. Gather unique logical handles
> from final image commands in frame-arena memory, call
> `prepareTextureBindings()` once, apply only dirty Vulkan writes to the current
> safe frame-slot descriptor set, acknowledge only successful writes, and resolve
> `UiInstance::texIndex` from the prepared span with no storage calls, locks,
> strings, or maps in the conversion loop. Preserve the exact existing
> seal/submission/completion/cancellation lifecycle.
>
> Remove `UiTextureRegistry`, `IUiTextureRegistry`, renderer slot tables,
> descriptor-wide rewrites, slot retirement, and all `slotId` fields. Keep the font
> atlas as the separate shared MSDF `sampler2DArray`; keep CPU-to-descriptor-index
> shader semantics and document that sampling mode remains transitional. Add
> `UiManager::imageData(TextureRef)` as the clear raw-Clay payload bridge, retain
> `storeTexture()` only as a deprecated `//Transitional:` alias, and do not claim
> that a logical handle removes Clay's pointer-payload lifetime requirement.
>
> Add focused handle, two-window resolution, dirty-write, capacity rollback,
> fallback/stale, external-resource retirement, image/icon/viewport rewrite,
> frame-slot safety, Clay payload, and direct-conversion tests. Run full release and
> development builds, storage/types/renderer/manager/Vulkan tests, validation when
> available, and `git diff --check`. Finish with `phase3.md` describing changes,
> deviations, validation, transitional limitations, and the Phase 4 outline.
