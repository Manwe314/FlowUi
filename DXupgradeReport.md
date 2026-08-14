# FlowUi UI-Data DX Upgrade Options

## Problem boundary

FSEL has made the correct ownership split: application-meaningful values remain
outside element `State`, while element `State` contains only interaction
mechanics. A pointer or future `ValueBinding<T>` is an efficient final connection
between a control and one value, but it does not answer the larger DX question:

> How does UI code in another translation unit find the correct application data
> without every intermediate function forwarding it manually?

The desired system should preserve one source of truth, support UI-to-data and
data-to-UI flow, diagnose missing data clearly, avoid per-frame allocation, and
not turn domain code into a collection of element IDs.

## Option 1: Explicit view-model parameters

Build a small application-specific view model and pass it once into each major
UI branch:

```cpp
struct AudioPanelModel {
	AudioSettings& settings;
	HistoryController& history;
};

void drawAudioPanel(UiManager& ui, AudioPanelModel model);
```

This is the simplest and most C++-native option. It has no lookup cost, hidden
dependencies, manager, or new allocation. It also makes dependencies obvious in
tests. Its weakness is exactly the current complaint: large or deeply composed
UI trees accumulate forwarding parameters, even when intermediate functions do
not use them.

**Pitch:** Keep this as the baseline and recommended choice for small UI trees,
but it is not a universal FlowUi DX upgrade.

## Option 2: Hierarchical typed UI context

Allow a caller to provide a borrowed typed value once around a UI subtree. Any
descendant builder function can require it from `UiManager` or an element
context:

```cpp
auto scope = ui.provide(kAudioPanelData, audioPanelModel);
drawWorkspace(ui); // descendants may call ui.require(kAudioPanelData)
```

Providers form a frame-local stack. A nearer provider shadows an outer provider,
which makes the same component reusable for different documents, inspectors, or
windows. The scope stores only a typed key and pointer; it does not own or copy
the model. Lookup can be a short reverse scan at provider boundaries, with a
small cache if profiling shows a need.

This directly solves branch plumbing with almost no memory cost, but lifetime
remains the caller's responsibility. It also introduces ambient dependencies,
so `require()` must produce a focused missing-provider diagnostic and components
should document the keys they consume.

**Pitch:** The smallest feature that materially fixes deep UI composition.

## Option 3: Typed app-level UI data store

Add an app-owned `UiDataManager` that stores UI-relevant application models in
stable typed slots:

```cpp
inline constexpr UiDataKey<AudioPanelModel> kAudioPanelData{"audio-panel"};

auto audioData = app.uiData().emplace(kAudioPanelData, AudioPanelModel{...});
AudioPanelModel& model = app.uiData().get(audioData);
```

`UiDataHandle<T>` can be a compact index-plus-generation value. Registration may
use a hash once, while hot access resolves the handle directly. Stable records
can reuse FlowUi's existing storage/controller patterns, so values do not move
when the registry grows. The manager can support both:

- `emplace`: FlowUi owns the presentation model;
- `expose`: FlowUi registers a borrow of an externally owned domain model.

This centralizes lifetime and makes data reachable from any translation unit
that has `App`, but a global registry alone is a service locator. It needs typed
keys, explicit removal, generation checking, and optional window/document scope
to avoid accidental coupling.

**Pitch:** Best when FlowUi should also help own and locate presentation data,
not merely transport references during one frame.

## Option 4: Reactive signals or reducer store

Values become signals, or UI emits commands into a reducer-owned store. Reads
track versions and writes can automatically feed validation, undo, networking,
or derived-value recomputation.

This is powerful for asynchronous and highly transactional applications, but it
adds the most policy: mutation queues, subscription lifetimes, scheduling,
thread rules, and possibly one-frame latency. Immediate-mode drawing already
re-evaluates the UI each frame, so a retained dependency graph is not required
for ordinary FSEL controls.

**Pitch:** Valuable as a future application layer, but too opinionated and heavy
for FlowUi's first general data-distribution facility.

## Recommendation: store plus context, bindings only at the leaf

Combine Options 2 and 3 as two independent layers:

```text
UiDataManager owns/exposes stable typed models
                  ↓
UiDataScope provides a chosen model to one UI subtree
                  ↓
branch code calls require(key) and obtains T&
                  ↓
FSEL receives &model.member in its Parameters
```

Illustrative use:

```cpp
struct AudioPanelData {
	bool muted = false;
	double volume = 0.8;
};

inline constexpr UiDataKey<AudioPanelData> kAudioPanel{"audio-panel"};

// Application setup, possibly in the main translation unit.
auto audio = app.uiData().emplace(kAudioPanel, AudioPanelData{});

// One provision at a composition boundary.
auto audioScope = app.ui().provide(kAudioPanel, audio);
drawSettingsWorkspace(app.ui());

// A distant translation unit.
void drawAudioSettings(UiManager& ui) {
	auto& model = ui.require(kAudioPanel);
	ui.createElement(FSEL::kSlider, "volume")
		.setParameters({.value = &model.volume})
		.draw();
}
```

The final pointer is intentional: once the correct model reaches the leaf, a
direct borrow is the fastest and clearest control contract. A `ValueBinding<T>`
may later standardize read-only values, accessor-backed properties, or write
notifications, but it should not be mistaken for the distribution system.

## Cost and guardrails

- **Hot path:** one scope lookup per consuming branch, then direct references and
  pointer writes inside controls. No per-element registry lookup is required.
- **Memory:** one stable record per stored model plus small frame-local provider
  entries. No per-frame model copies or callback allocations.
- **Identity:** keys must combine value type with a stable authored tag; a plain
  string-only key is insufficient.
- **Lifetime:** owned records use generation-checked handles. Borrowed records
  must state their lifetime contract and should fail clearly after removal.
- **Scope:** provider stacks belong to a `UiManager`/window and are restored by
  RAII, just like FlowUi's existing construction scopes.
- **No hidden element storage:** this system stores application presentation
  data, never relocates it into an FSEL element's `State`.
- **No automatic reactivity initially:** frame reconstruction already carries
  data-to-UI changes. Optional revision counters and transactional edit guards
  can be added later without changing lookup or ownership.

## Suggested first prototype

Prototype the borrowed hierarchical context first: typed keys, `provide()`,
`find()`, and diagnostic `require()`. It is small enough to validate the DX
before committing to ownership APIs. If it proves useful, back the same key and
handle vocabulary with `UiDataManager` stable storage. FSEL controls can continue
using their current controlled parameters throughout both phases.
