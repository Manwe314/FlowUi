# Configuration glossary {#flowui_configuration_glossary}

This reference lists startup and operation configuration, their defaults, units, and build conditions. Core types live in `FlowUi`; development types retain their `FlowUi::devSystems` namespaces and headers under `include/devSystems`.

Include [AppConfig.hpp](../../include/FlowUi/Public/configurationStructs/AppConfig.hpp) to configure startup, and [App.hpp](../../include/FlowUi/Public/App.hpp) for `makeApplication()`. Startup copies the configuration; editing the caller's original struct does not reconfigure the running application. Use explicit runtime APIs, such as `App::setWindowInputConfig()`, for supported live changes.

- [Startup configuration](#flowui_config_startup)
- [Capacity settings and precedence](#flowui_config_capacity)
- [Build-time settings](#flowui_config_build)
- [Operation settings](#flowui_config_operations)
- [Registered capacity keys](#flowui_config_capacity_keys)

## Capacity settings and precedence {#flowui_config_capacity}

`MemoryCapacityProfile::storage` supplies seven typed startup byte capacities for storage pools and buffers. `settings` supplies individual registered capacities by stable ID, including those same byte capacities and manager container reserves in bytes or entries. Each setting is an initial allocation hint; it does not impose a universal maximum on manager containers.

Startup resolves values in this order, with each later layer overriding matching targets:

1. Owner defaults.
2. Nonzero typed `storage` hints.
3. Explicit `settings` entries.
4. Available baked capacities when `AppConfig::prefer_baked_capacities` is true.

A zero typed storage hint preserves defaults. A zero explicit setting selects the target's registered default, overriding a matching typed hint. Unknown targets, conflicting caller duplicates, values above the registered maximum, and misaligned nonzero values reject application creation. Identical duplicates are accepted. `growthFactor` must be finite and greater than one; storage applies a minimum factor of 1.1. Growth also depends on `allowRuntimeGrowth` and the configured capacity error policies and budgets.

```cpp
FlowUi::AppConfig config{};
config.prefer_baked_capacities = false; // Use caller hints for manual tuning.
config.memoryCapacityProfile.storage.initialInstanceBytesPerFrame =
    4ull * 1024 * 1024; // 4 MiB per frame slot.

config.memoryCapacityProfile.settings.emplace_back(
    FlowUi::MemoryCapacitySetting{
        FlowUi::makeMemoryCapacityTargetId(
            "flowui.tuning.input_fields.registry_entries"),
        256 // Entries, as registered for this target.
    });
```

Use the exact case-sensitive keys in the [capacity catalog](#flowui_config_capacity_keys). The public ID helper hashes the key at compile time; application creation checks registration and value constraints. Capacity resolution runs at startup; owners retrieve resolved indexed values when reserving their containers or initializing pools.

`MemoryCapacityProfile::metadata` describes the evidence behind a profile and does not change allocations. Aggregate manager demand is retained separately in `devSystems::MemoryCapacityPreview::managers` for profiling; it is not part of startup configuration.

Development capture controls require `FLOW_UI_DEV_MODE`. Agentic controls additionally require `AGENTIC_WORKFLOW`, which generates `FLOW_UI_AGENTIC_WORKFLOW`. Icon settings require icon support. Settings for an omitted subsystem do not enable it.

## Startup configuration {#flowui_config_startup}

Defaults below are declaration defaults unless an effective app default is explicitly stated. Colors use RGBA components in the 0–255 range. KiB/MiB/GiB are binary units. Child configuration rows lead to the detailed tables that follow.

### AppConfig

[Declaration](../../include/FlowUi/Public/configurationStructs/AppConfig.hpp).

| Member | Default | Meaning |
|---|---|---|
| `resources` | `{}` | Resource search roots and discovery switches. |
| `window` | `{}` | Main native window and input defaults. |
| `vk` | `{}` | App Vulkan/device/presentation defaults. |
| `ui` | `{}` | Layout, text, font and editing defaults shared by windows. |
| `iconManager` | `{}` | Shared icon atlas/cache configuration. |
| `memoryCapacityProfile` | `{}` | Initial storage/container capacity hints and growth policy. |
| `prefer_baked_capacities` | `true` | Let available compiled capacities override caller hints. |
| `errors` | `{}` | Recovery policy and reporting destination. |
| `dev` | `{}` | Developer tooling and monitoring configuration. |
| `agentic_workflow` | `{}`; agentic builds | Evidence service configuration and permissions. |

### ResourceConfig

[Declaration](../../include/FlowUi/Public/configurationStructs/ResourceConfig.hpp); [consumer](../../src/FlowUi/Private/resourceService/ResourceDiscovery.cpp).

| Member | Default | Meaning |
|---|---|---|
| `roots` | empty | Ordered explicit resource roots, searched before default roots. |
| `executable_directory` | empty | Override executable-directory discovery inside platform-default lookup. |
| `use_platform_defaults` | `true` | Add environment/executable/module/platform resource locations. |
| `use_build_directory` | `true` | Append the configured build-resource directory. |

`FLOWUI_RESOURCE_ROOT` participates when platform-default discovery is enabled. Explicit roots precede it; the build directory follows platform roots. Absolute resource filenames are checked directly. Relative parent traversal is rejected.

### WindowConfig and WindowConfigOverrides

[Declaration](../../include/FlowUi/Public/configurationStructs/WindowConfig.hpp); [backend](../../include/FlowUi/Private/windowService/Window.hpp).

| Member | Default | Meaning |
|---|---|---|
| `width` | `1280` | Initial screen-coordinate width; must be positive. |
| `height` | `720` | Initial screen-coordinate height; must be positive. |
| `title` | `"FlowUi App"` | Native title. |
| `resizable` | `true` | Request user/window-manager resizing. |
| `decorated` | `true` | Request native border/title bar. |
| `maximized` | `false` | Request maximized startup. |
| `fullscreen` | `false` | Request primary-monitor fullscreen. |
| `highDPI` | `true` | Request supported high-DPI backend behavior. |
| `input` | `{}` | Initial native input modes. |

[`WindowConfigOverrides`](../../include/FlowUi/Public/configurationStructs/WindowConfig.hpp) has exactly the same nine member names and meanings, each wrapped in `std::optional`, all initially absent. An absent title uses the main-window title followed by ` - Window N`. Its `input` replaces the whole input configuration; it is not a recursive partial override.

### WindowInputConfig

[Declaration](../../include/FlowUi/Public/configurationStructs/InputConfig.hpp); [consumer](../../include/FlowUi/Private/windowService/Window.hpp).

| Member | Default | Meaning |
|---|---|---|
| `cursorMode` | `Normal` | Normal, hidden, or disabled/captured native cursor. |
| `stickyKeys` | `false` | Enable GLFW sticky key polling behavior. |
| `stickyMouseButtons` | `false` | Enable GLFW sticky mouse-button polling behavior. |
| `lockKeyMods` | `false` | Request Caps/Num Lock modifier bits. |
| `rawMouseMotion` | `false` | Request unaccelerated motion where supported. |

### VulkanConfig

[Declaration](../../include/FlowUi/Public/configurationStructs/VulkanConfig.hpp).

| Member | Default | Meaning |
|---|---|---|
| `presentMode` | `Fifo` | Requested FIFO, Mailbox, or Immediate swapchain mode. |
| `framesInFlight` | `2` | Frame resource slots; effective minimum one. |
| `enableValidation` | `true` | Request available validation layers. |
| `enableDebugUtils` | `true` | Request available Vulkan debug-utils support. |
| `preferDiscreteGPU` | `true` | Favor discrete devices during GPU scoring. |
| `srgbBackbuffer` | `true` | Prefer supported sRGB surface format. |

Rendering uses sample count one; there is no MSAA setting to configure.

### UiConfig

[Declaration](../../include/FlowUi/Public/configurationStructs/UiConfig.hpp); [initialization](../../src/FlowUi/Private/layoutService/UiManagerState.cpp); [layout/input scaling](../../src/FlowUi/Private/AppRuntime.cpp).

| Member | Default | Meaning |
|---|---|---|
| `dpi` | `96` | Logical text DPI; point scale starts at `max(1,dpi)/72`. |
| `uiScale` | `1` | Global layout/input scale; larger values enlarge UI relative to the window. |
| `fontScale` | `1` | Multiplies DPI text scaling; nonpositive values use DPI scaling alone. |
| `clayArenaCapacityBytes` | `0` | Persistent Clay arena bytes; zero/minimum uses `Clay_MinMemorySize()`. |
| `defaultFontFamily` | default family below | First family loaded at startup, with error-policy fallback behavior. |
| `fontAtlasSize` | `2048` | Square font-atlas side length in pixels. |
| `inputManager` | `{}` | Default caret/selection appearance. |
| `shortcuts` | `{}` | Built-in text shortcut registration choices. |
| `primitive_antialiasing.enabled` | `true` | Startup linear analytic coverage for solids, borders, and image silhouettes; false selects hard edges. Does not affect glyph AA. |
| `primitive_antialiasing.coverage_width_scale` | `1` | Finite positive multiplier of derivative-based coverage width in framebuffer pixels. The raster fringe grows with coverage support. Invalid values fail startup, even when AA is disabled. |

### FontFamilyCreateInfo and FontFaceCreateInfo

[Declarations](../../include/FlowUi/Public/managerStructs/FontTypes.hpp). These public resource descriptions are also reusable with FontManager APIs.

| Type.member | Default | Meaning |
|---|---|---|
| `FontFamilyCreateInfo.name` | `"Default"` | Registered family name. |
| `FontFamilyCreateInfo.faces` | one Inter face | Faces belonging to the family; default face overrides its path to `assets/fonts/Inter.arfont`. |
| `FontFaceCreateInfo.path` | empty | Source font resource path; the family's default face supplies the Inter path. |
| `FontFaceCreateInfo.pixelSize` | `18` | Requested face size, relevant to runtime font baking. Baked font data carries its own metrics. |
| `FontFaceCreateInfo.weight` | `400` | Weight used for face selection. |
| `FontFaceCreateInfo.style` | `Normal` | Normal/italic face selection. |
| `FontFaceCreateInfo.name` | empty | Optional face name. |
| `FontFaceCreateInfo.baked_data` | empty | Borrowed in-memory `.arfont` bytes; nonempty data overrides `path` and is consumed synchronously during registration. Keep configured bytes alive through app initialization. |

### InputManagerConfig

[Declaration](../../include/FlowUi/Public/configurationStructs/InputConfig.hpp); [appearance mapping](../../src/FlowUi/Public/managers/InputFieldManager.cpp).

| Member | Default | Meaning |
|---|---|---|
| `caretWidthPx` | `2` | Caret rectangle width. |
| `caretHeightOverflowTopPx` | `1` | Extra caret extent above text. |
| `caretHeightOverflowBottomPx` | `1` | Extra caret extent below text. |
| `caretColor` | `(255,255,255,255)` | Caret color. |
| `highlightBoxColor` | `(66,133,244,150)` | Selected-range background. |
| `highlightedTextColor` | `(255,255,255,255)` | Selected-range foreground text. |

Field-local overlay styles can override these defaults.

### ShortcutManagerConfig and default shortcut families

[Declarations](../../include/FlowUi/Public/configurationStructs/ShortcutManagerConfig.hpp); [registration consumer](../../src/FlowUi/Public/managers/ShortcutManager.cpp).

| Type.member | Default | Meaning |
|---|---|---|
| `ShortcutManagerConfig.textEditing` | `{}` | Built-in editing registration block. |
| `ShortcutManagerConfig.window` | `{}` | Opt-in native-window registration block. |
| `DefaultTextShortcutConfig.enabled` | `true` | Install built-in text shortcuts. |
| `.platform` | `Auto` | Automatic, Control, or Command modifier convention. |
| `.priority` | `0` | Priority of installed shortcut registrations. |
| `.selectAll` | `true` | Enable select-all registration. |
| `.clipboard` | `true` | Enable clipboard registrations. |
| `.wordNavigation` | `true` | Enable word-navigation registrations. |
| `DefaultWindowShortcutConfig.enabled` | `false` | Install enabled window bindings. |
| `.priority` | `0` | Priority within the owning window's Global scope. |
| `.close_window` | Enabled, inherited chord | Request closure of the originating window. |
| `.minimize` | Enabled, inherited chord | Minimize the originating window. |
| `.maximize_restore` | Disabled | Explicit chord required; toggle native maximization. |
| `.toggle_fullscreen` | Disabled | Opt in to the platform fullscreen chord. |
| `DefaultWindowShortcutBinding.chord` | Absent | Inherit the platform chord; a value replaces it. |
| `.enabled` | `true` | Register this operation when its family is enabled and the backend supports it. |

Window defaults use Cmd+W/Cmd+M on macOS and native desktop handling without extra
close/minimize chords on Windows/Linux. Fullscreen uses Ctrl+Cmd+F on macOS or
F11 elsewhere. Enabled replacement chords require exact modifiers and Press;
duplicate window chords are rejected. Automatic undo/redo requests and their
configuration member have been removed; see the
[shortcut migration and usage guide](shortcut_manager.md).

### IconManagerConfig

[Declaration](../../include/FlowUi/Public/configurationStructs/IconManagerConfig.hpp); [cache consumer](../../src/FlowUi/Private/resourceService/IconCacheController.cpp).

| Member | Default | Meaning |
|---|---|---|
| `atlasSize` | `2048` | Square atlas-page side length in pixels. |
| `maxAtlasPages` | `10` | Maximum icon atlas pages. |
| `sizeBucketStep` | `8` | Pixel tolerance for reusing nearby cached raster sizes, not simply a rounding step. |
| `atlasPadding` | `1` | Empty pixels around allocations. |

### MemoryCapacityProfile and MemoryCapacitySetting

[Declarations](../../include/FlowUi/Public/configurationStructs/MemoryCapacityProfile.hpp).

| Type.member | Default | Meaning |
|---|---|---|
| `MemoryCapacityProfile.storage` | all zero | Typed startup storage byte hints. |
| `.settings` | empty | Granular registered initial-capacity overrides. |
| `.growthFactor` | `1.5` | Geometric storage growth factor; subject to validation/minimum described above. |
| `.allowRuntimeGrowth` | `true` | Permit storage growth, additionally restricted by error policies. |
| `.metadata` | `{}` | Descriptive capture/build information. |
| `MemoryCapacitySetting.target` | `0` | Stable registered capacity target ID; zero itself is not a valid target. |
| `.value` | `0` | Native target-unit capacity; zero chooses the registered default. |

### StorageMemoryCapacities

[Declaration](../../include/FlowUi/Public/configurationStructs/StorageConfig.hpp). Every member defaults to zero, meaning preserve the effective default. Values are bytes.

| Member | Effective app default | Meaning |
|---|---|---|
| `initialPersistentCpuBytes` | `4 MiB` | Initial persistent CPU arena backing. |
| `initialStringBytes` | `1 MiB` | Initial interned-string storage. |
| `transientBytesPerFramePerWindow` | `1 MiB` | Temporary storage per window/frame slot; the actual UI frame allocation budget. |
| `transientBytesPerWorker` | `1 MiB` | Temporary storage for each worker arena. |
| `initialDecodeScratchBytes` | `8 MiB` | Initial decode scratch backing. |
| `initialUploadStagingBytes` | `16 MiB` | Initial upload staging backing. |
| `initialInstanceBytesPerFrame` | `1 MiB` | Renderer instance-stream backing per frame. Private StorageConfig alone defaults to 256 KiB; app startup replaces it. |

### MemoryCapacityProfileMetadata

[Declaration](../../include/FlowUi/Public/configurationStructs/MemoryCapacityProfile.hpp). These members do not tune runtime allocations.

| Member | Default | Meaning |
|---|---|---|
| `platform` | empty | Capture platform description. |
| `build` | empty | Build identity. |
| `gpu` | empty | GPU description. |
| `captureBeginTick` | `0` | First captured tick. |
| `captureEndTickExclusive` | `0` | Exclusive end of capture interval. |
| `warmUpTicks` | `0` | Warm-up excluded from measured evidence. |
| `framesInFlight` | `0` | Frame-slot count associated with evidence. |
| `complete` | `false` | Whether evidence is marked complete. |

### ErrorContract, ErrorPolicy, ErrorObserverConfig, ErrorSink

[Declarations](../../include/FlowUi/Public/errorManagement/ErrorPolicy.hpp). Contract defaults construct both its `policy` and `observer` blocks. Recovery choices affect the app lifetime; the observer reports outcomes rather than choosing recovery.

| Type.member | Default | Meaning |
|---|---|---|
| `ErrorContract.policy` | `{}` | Recoverable failure resolutions. |
| `.observer` | `{}` | Central reporting configuration. |
| `ErrorPolicy.defaultFont` | `TryFallbackThenDisableText` | Recovery tries packaged Inter, then an embedded rectangle glyph; after recovery failure, disable text or fail app. Immediate failure skips recovery. |
| `.missingImage` | `UseFallbackTexture` | Missing image: fallback texture or skip visual. |
| `.missingViewport` | `UseFallbackTexture` | Missing viewport: fallback texture or skip visual. |
| `.iconGeneration` | `UseFallbackTexture` | Icon failure: fallback, skip, or fail request. |
| `.transientCapacity` | `GrowWithinBudget` | Temporary storage exhaustion: grow within budget or reject. |
| `.persistentCapacity` | `GrowWithinBudget` | Persistent storage exhaustion: grow within budget or reject. |
| `.descriptorCapacity` | `RejectOperation` | Cache capacity handling: reject or evict/retry at supporting owners. Does not configure a larger descriptor pool. |
| `.duplicatePopup` | `FirstSubmissionWins` | Keep first popup submission or reject duplicate. |
| `.missingPopupAnchor` | `SkipPopup` | Skip, defer, or anchor to viewport. |
| `.popupCapacity` | `ClampLayer` | Clamp layer or skip overflowing popup. |
| `.inputQueueOverflow` | `DropNewest` | Drop newest or oldest queued text input. |
| `.inputTextQueueCapacity` | `4096` | Per-window queued text input capacity. |
| `ErrorObserverConfig.sink` | empty | App callback destination. |
| `.mode` | `SinkOrDefault` | Sink-or-default, sink-and-default, sink-only, or default-only routing. |
| `.output` | `stderr` | Default report stream. |
| `ErrorSink.userData` | `nullptr` | Non-owning callback context. |
| `.callback` | `nullptr` | Callback receiving structured error views. |

### DevToolsConfig and DevShortcutChord

[Declarations](../../include/FlowUi/Public/configurationStructs/DevToolsConfig.hpp). Monitoring/tooling exist only in development builds.

| Type.member | Default | Meaning |
|---|---|---|
| `DevToolsConfig.monitoring` | `{}` | Monitoring capture and report retention controls. |
| `.tooling` | `{}` | Schema, override, and tree limits. |
| `.panelToggleChord` | Ctrl+Shift+D, press | Developer-window toggle registration. |
| `.enabled` | `false` | Enable the developer interface. Monitoring owners are constructed independently in development builds; their capture levels are separate controls. |
| `.capture_timing_on_startup` | `false` | When enabled with tooling, capture until timing-ring capacity/opening the interface; preserve first snapshot. |
| `.useShortcutManagerForPanelToggle` | `true` | Register the toggle chord through ShortcutManager. |
| `.excludeInternalDevElementsFromCapture` | `true` | Filter developer UI elements from captured authored UI. |
| `DevShortcutChord.key` | `68` | Backend key code (`D` by default). |
| `.ctrl` | `true` | Require Control. |
| `.shift` | `true` | Require Shift. |
| `.alt` | `false` | Require Alt. |
| `.super` | `false` | Require Super/Command. |
| `.trigger` | `Press` | Fire on Press, Release, or Down. |

### Developer monitoring blocks

Declarations in this subsection live in [DevConfig.hpp](../../include/devSystems/DevConfig.hpp), namespace `FlowUi::devSystems`.

`DevMonitoringConfig` contains exactly six default-constructed blocks: `timing` (CPU/GPU capture), `timingReporting` (timing retention/statistics), `memory` (memory producers/probes), `memoryReporting` (memory retention/aggregation), `errors` (error recording), and `errorReporting` (error correlation/retention).

| Type.member | Default | Meaning |
|---|---|---|
| `DevTimingConfig.selectedElementInstance` | empty ID | Instance selected for focused timing. |
| `.balancedElementRetentionThresholdNs` | `50,000` | Balanced-mode element retention threshold, ns. |
| `.selectedElementDefinition` | empty ID | Definition selected for focused timing. |
| `.cpuLevel` | `Summary` | OnlyFrameTime / Summary / Balanced / Deep, bounded by compiled timing level. |
| `.enabledCategoryMask` | `0xFFFFFFFF` | Enabled timing category bits. |
| `.gpuQueryCapacityPerFrame` | `512` | GPU timestamp slots; normalized to an even number of at least 16. |
| `.producerRecordCapacity` | `8192` | Producer timing record capacity. |
| `.gpuTimingEnabled` | `true` | Enable supported GPU timing. |
| `TimingReportingConfig.retainedAppTickCapacity` | `4096` | Retained app-tick ring size. |
| `.minimumFramesInFlightMultiplier` | `20` | Minimum retention proportional to observed frame-slot count. |
| `.rollingSampleCapacity` | `2048` | Rolling timing statistics sample capacity. |
| `.percentilePoints` | `.50,.90,.95,.99` | Requested quantiles in fractional form. |
| `DevMemoryConfig.processSampleIntervalNs` | `500,000,000` | Process-memory sample cadence. |
| `.gpuBudgetSampleIntervalNs` | `250,000,000` | GPU-memory budget sample cadence. |
| `.level` | `SubsystemCapacity` | Disabled / StorageSummary / SubsystemCapacity / DetailedLifetimes / DeepAllocations, bounded by build. |
| `.producerEventCapacity` | `8192` | Producer memory-event capacity. |
| `.detailedGpuStatsEverySamples` | `0` | Detailed GPU statistics cadence in samples; zero leaves periodic detailed sampling off. |
| `.gpuMemory` | `true` | Enable GPU-memory probing. |
| `.processMemory` | `true` | Enable process-memory probing. |
| `.detailedVmaStatistics` | `false` | Enable detailed allocator statistics where supported. |
| `MemoryReportingConfig.eventByteCapacity` | `16 MiB` | Retained memory-event storage budget. |
| `.consumeWarningThresholdNs` | `2,000,000` | Reporting-consumption warning threshold. |
| `.segmentCapacity` | `100,000` | Retained memory segments. |
| `.managerSampleEveryTicks` | `8` | Manager snapshot cadence. |
| `.quantileWindowSegments` | `20,000` | Segment window for quantile calculations. |
| `.retainLifetimeEvents` | `false` | Retain detailed lifetime evidence when available. |

Consumer families: [timing](../../src/devSystems/devMonitoringAndReporting/timing/DevTiming.cpp), [timing reporting](../../src/devSystems/devMonitoringAndReporting/reporting/DevTimingReporting.cpp), [memory](../../src/devSystems/devMonitoringAndReporting/memory/DevMemory.cpp), [environment probes](../../src/devSystems/devMonitoringAndReporting/memory/DevEnvironmentMemoryProbe.cpp), [memory reporting](../../src/devSystems/devMonitoringAndReporting/reporting/DevMemoryReporting.cpp).

### Developer error configuration

[Declarations](../../include/devSystems/DevConfig.hpp); [recording](../../src/devSystems/devMonitoringAndReporting/errors/DevError.cpp); [reporting](../../src/devSystems/devMonitoringAndReporting/reporting/DevErrorReporting.cpp). Namespace: `FlowUi::devSystems`.

| Type.member | Default | Meaning |
|---|---|---|
| `DevErrorConfig.level` | `Causal` | Disabled / Summary / Causal / StackAndState / Deep, bounded by build. |
| `.producerRecordCapacity` | `256` | Error producer records. |
| `.breadcrumbCapacity` | `512` | Breadcrumb recorder capacity. |
| `.recentBreadcrumbCount` | `32` | Recent breadcrumbs associated with an occurrence. |
| `.nativeTextLimit` | `160` | Native diagnostic text bound, limited by fixed native text storage. |
| `.threadRecorderCapacity` | `16` | Thread recorder slots. |
| `.sourceDescriptorCapacity` | `1024` | Error source descriptors. |
| `.breadcrumbDescriptorCapacity` | `256` | Breadcrumb descriptors. |
| `.stackTraceCapacity` | `256` | Retained stack trace slots. |
| `.maximumStackFrames` | `32` | Frames requested per stack. |
| `.snapshotProviderCapacity` | `64` | Registered snapshot providers. |
| `.pendingSnapshotCapacity` | `128` | Pending state snapshot requests. |
| `.retainedSnapshotCapacity` | `256` | Retained state snapshots. |
| `.stackProvider` | `nullptr` | Non-owning optional native stack provider. |
| `DevErrorTrigger.code` | `None` | Error-code trigger selector. |
| `.site` | `None` | Error-site trigger selector. |
| `.nativeCode` | `0` | Native code to match when requested. |
| `.matchNativeCode` | `false` | Enable native-code filtering. |
| `.preOccurrenceTicks` | `2` | Trigger capture look-back. |
| `.postOccurrenceTicks` | `2` | Trigger capture look-ahead. |
| `DevErrorReportingConfig.triggers` | empty | Automatic error capture rules. |
| `.retainedByteBudget` | `4 MiB` | Retained occurrence evidence byte budget. |
| `.retainedOccurrenceCapacity` | `512` | Retained occurrences. |
| `.retainedBreadcrumbCapacity` | `4096` | Retained reporting breadcrumbs. |
| `.postOccurrenceTicks` | `2` | Grace window for automatic correlation. |
| `.maxStepsPerOccurrence` | `64` | Causal steps per occurrence. |
| `.maximumAdvicePerOccurrence` | `4` | Advice items per occurrence. |
| `.retainedCaptureCapacity` | `32` | Retained triggered captures. |
| `.maximumPinnedTimingTicks` | `16` | Timing ticks pinned per capture. |
| `.maximumPinnedMemoryEvents` | `128` | Memory events pinned per capture. |

Stack/snapshot behavior requires the relevant detail level and available providers; setting a capacity does not create a stack implementation.

### Developer tooling configuration

[Declarations](../../include/devSystems/DevConfig.hpp). `devSystems::DevToolingConfig` groups `schema`, `overrides`, and `treeCapture`, all `{}`. Their types live in `FlowUi::devSystems::tooling`.

| Type.member | Default | Meaning |
|---|---|---|
| `DevSchemaLimits.maxDepth` | `16` | Maximum schema traversal depth. |
| `.maxTypes` | `2048` | Registered schema types. |
| `.maxFields` | `16384` | Retained fields. |
| `.maxEnumValues` | `8192` | Retained enum values. |
| `.maxConstraints` | `16384` | Retained constraints. |
| `.maxStringBytes` | `2 MiB` | Schema string table budget. |
| `DevOverrideEngineConfig.maximumPendingTransactions` | `256` | Pending override transactions. |
| `.maximumCommandsPerTransaction` | `4096` | Commands admitted in one transaction. |
| `.maximumPendingCommands` | `16384` | Total queued commands. |
| `DevTreeCaptureConfig.flowNodeReserve` | `512` | Initial Flow node reserve. |
| `.stringByteReserve` | `64 KiB` | Initial captured string reserve. |
| `.diagnosticReserve` | `64` | Initial capture diagnostic reserve. |
| `.maximumFlowNodes` | `1,048,576` | Flow capture node ceiling. |
| `.maximumStringBytes` | `64 MiB` | Captured string ceiling. |
| `.clayNodeReserve` | `2048` | Initial Clay node reserve; Clay capture builds only. |
| `.clayRootReserve` | `32` | Initial Clay root reserve; Clay capture builds only. |
| `.directLinkReserve` | `2048` | Initial Flow–Clay link reserve; Clay capture builds only. |
| `.maximumClayNodes` | `4,194,304` | Clay node ceiling; Clay capture builds only. |

Consumers: [schema registry](../../include/devSystems/devTooling/schema/DevSchemaRegistry.hpp), [override engine](../../src/devSystems/devTooling/override/DevOverrideEngine.cpp), [tree capture](../../src/devSystems/devTooling/tree/DevTreeCapture.cpp). Capacity-profile entries can override the corresponding initial tree reserves during app creation.

### AgenticWorkflowConfig and AgenticLimits

[Declarations](../../include/devSystems/devAgenticWorkflow/AgenticTypes.hpp). Namespace: `FlowUi::devSystems::agentic`; available only in agentic builds. [Configuration consumer](../../src/devSystems/devAgenticWorkflow/AgenticService.cpp).

| Type.member | Default | Meaning |
|---|---|---|
| `AgenticWorkflowConfig.store_path` | empty | Evidence store location; may be supplied through launch handoff. |
| `.application_id` | empty | App identity. |
| `.application_revision` | empty | App revision identity. |
| `.run_id` | empty | Run identity. |
| `.provenance` | empty JSON object | App-supplied provenance. |
| `.limits` | defaults below | Service bounds used in admission/capture/storage. |
| `.enabled` | unset | Accept launch handoff; explicit false keeps service dormant. |
| `.local_attachment_enabled` | unset | Accept launch handoff; explicit false disables endpoint. |
| `.visual_capture_enabled` | `false` | Permit visual capture. |
| `.input_control_enabled` | `false` | Permit frame-bound input replacement. |
| `.action_control_enabled` | `false` | Permit invocation of app-exported actions. |
| `AgenticLimits.maximum_snapshot_bytes` | `128 MiB` | Maximum capture snapshot bytes. |
| `.maximum_artifact_bytes` | `256 MiB` | Maximum artifact bytes. |
| `.maximum_disk_bytes` | `2 GiB` | Store disk budget. |
| `.maximum_ticks` | `4096` | Captured tick bound. |
| `.maximum_records` | `100000` | Evidence record bound. |
| `.maximum_images` | `8` | Image count bound. |
| `.maximum_image_bytes` | `64 MiB` | Image byte bound. |
| `.finalization_timeout_ns` | `2,000,000,000` | Finalization timeout. |
| `.maximum_queued_jobs` | `8` | Bounded job queue admission. |

Unset optional enablement is different from false. Compiling support does not itself authorize input/action control.

## Build-time settings outside AppConfig {#flowui_config_build}

These change the compiled library or package and cannot be replaced with runtime booleans. Configure them through [CMakeLists.txt](../../CMakeLists.txt); consume the generated build configuration consistently with the linked library.

| CMake setting | Default | Meaning |
|---|---|---|
| `FLOW_UI_DEV_MODE` | `OFF` | Compile developer systems and APIs. |
| `FLOW_UI_DEV_CAPTURE_CLAY` | follows dev mode | Compile post-layout Clay forest capture; requires development mode. |
| `FLOWUI_DEV_TIMING_LEVEL` | `2` | Maximum compiled CPU timing detail, 0–3. |
| `FLOWUI_DEV_MEMORY_LEVEL` | `2` | Maximum compiled memory detail, 0–4. |
| `FLOWUI_DEV_ERROR_LEVEL` | `2` | Maximum compiled error detail, 0–4. |
| `AGENTIC_WORKFLOW` | `OFF` | Compile agentic services/tools; requires development mode. Generates `FLOW_UI_AGENTIC_WORKFLOW`. |
| `FLOWUI_INCLUDE_ICON_MANAGER` | `ON` | Include SVG icon support. |
| `FLOWUI_PUBLIC_VULKAN_INTEROP` | `ON` | Expose public Vulkan viewport interop APIs. |
| `FLOWUI_ENABLE_RUNTIME_FONT_BAKING` | `OFF` | Include runtime TTF-to-MSDF generation. |
| `FLOWUI_BUILD_FONT_BAKER` | `ON` | Build offline font baker tool. |
| `COMPILE_FSELI` | `ON` | Include standard elements through the public Flow header; development builds force effective FSEL support on. |
| `FLOW_UI_ENABLE_BAKED_CHANGES` | `ON` | Enable automatic baked-change compilation. |
| `FLOWUI_GLFW_PROVIDER` | `auto` | Choose auto/system/vendored GLFW. |
| `FLOWUI_RESOURCE_SUBDIRECTORY` | `flowui` | Resource-pack directory name. |
| `FLOWUI_INSTALL_RESOURCE_DIRECTORY` | platform-derived | Installed resource-pack destination: bindir on Windows, datadir elsewhere, plus resource subdirectory. |
| `FLOWUI_INSTALL` | `ON` | Enable install rules. |
| `FLOWUI_BUILD_TESTS` | top-level `ON`, dependency `OFF` | Build test targets. |
| `FLOWUI_REQUIRE_VULKAN_TEST_DEVICE` | `OFF` | Fail rather than skip device-dependent storage tests when no suitable device exists. |

Per-target resource-copy, baking, and agentic integration helper arguments live in [FlowUiResources.cmake](../../cmake/FlowUiResources.cmake), [FlowUiBaking.cmake](../../cmake/FlowUiBaking.cmake), and [FlowUiAgentic.cmake](../../cmake/FlowUiAgentic.cmake). They are build integration inputs rather than additional runtime configuration structs.

## Additional operation-level settings {#flowui_config_operations}

These belong to their operation or element rather than `AppConfig`. Supply them to the relevant operation rather than adding them to startup configuration.

### FieldConfig and element state policy

[`FieldConfig`](../../include/FlowUi/Public/managerStructs/InputFieldTypes.hpp) is already public and supplied with each field request.

| Member | Default | Meaning |
|---|---|---|
| `maxBytes` | `size_t` maximum | UTF-8 stored-byte limit. |
| `mode` | `SingleLine` | Single-line or multiline storage/edit behavior. |
| `transactionDetail` | `Summary` | Detail of frame-scoped edit reports. |
| `readOnly` | `false` | Reject edits while retaining query/selection capability. |
| `softWrap` | `false` | Wrap multiline hard lines into visual slices. |
| `allowArrowNavigation` | `true` | Let the manager handle caret arrow navigation. |

[`TextLayoutDescriptor`](../../include/FlowUi/Public/managerStructs/InputFieldTypes.hpp) supplies `fontId` (0, font), `fontSize` (14), `letterSpacing` (0), `viewportWidth` and `viewportHeight` (0, available region), `tabWidth` (4), and `floatingZIndex` (0, generated line layering). Field requests may supply an optional `InputFieldOverlayStyle`; its visual defaults replace the global caret/selection style for that field.

[`ElementStatePolicy`](../../include/FlowUi/Public/elementAuthoring/ElementStatePolicy.hpp) has `retention` (`Transient`, or `WindowLifetime`) and `graceFrames` (5 absent committed frames). Transient state expires on the next absent committed frame after its grace period; window-lifetime policy ignores the grace period. This is definition-level configuration and is correctly public.

### Popup configuration

[Declarations](../../include/FlowUi/Public/managerStructs/PopupTypes.hpp). All are already public.

| Type.member | Default | Meaning |
|---|---|---|
| `PopupPlacement.anchorPoint` | `BottomLeft` | Attachment point on anchor. |
| `.popupPoint` | `TopLeft` | Attachment point on popup. |
| `.offset` | `(0,0)` | Placement offset. |
| `PopupAnchor.kind` | `Parent` | Parent, element, pointer snapshot/follow, position, or viewport anchor. |
| `.elementValue` | `0` | Explicit element identity for element anchoring. |
| `.coordinate` | `(0,0)` | Coordinate payload for positional anchoring. |
| `PopupOverflowPolicy.flipX` | `true` | Permit horizontal placement flip. |
| `.flipY` | `true` | Permit vertical placement flip. |
| `.shiftToFit` | `true` | Shift popup into available area. |
| `PopupRequest.anchor` | parent | Anchor selection. |
| `.placement` | `{}` | Attachment/offset settings. |
| `.overflow` | `{}` | Overflow adjustments. |
| `.firstFrame` | `DeferUntilMeasured` | Defer or place immediately before measured size exists. |
| `.expectedSize` | absent | Optional expected dimensions. |
| `.layer` | `CasualPopup` | Casual, warning, or critical layer. |
| `.pointerCaptureMode` | `CAPTURE` | Clay floating pointer ownership. |
| `.clipTo` | `NONE` | Clay floating clipping target. |
| `.outsidePress` | `DismissAndBlockAnchor` | Dismiss and block anchor, dismiss and consume gesture, or ignore. |
| `.dismissOnEscape` | `true` | Close on Escape. |

### PerformanceCaptureSettings

[Declaration](../../include/devSystems/devMonitoringAndReporting/reporting/DevPerformanceCapture.hpp); accepted by the [public App capture API](../../include/FlowUi/Public/App.hpp). Namespace: `FlowUi::devSystems`.

| Member | Default | Meaning |
|---|---|---|
| `start_chord` | Ctrl+Shift+key 297 | Shortcut-start trigger; remaining chord defaults apply. |
| `duration_ns` | `5,000,000,000` | Duration-mode recording length. |
| `budget_ns` | `16,600,000` | Frame budget event threshold. |
| `post_event_ns` | `500,000,000` | Recording tail after a budget event. |
| `finalization_timeout_ns` | `2,000,000,000` | Bound waiting for pending measurements. |
| `warmup_ticks` | `0` | Initial unmeasured ticks. |
| `budget_window` | invalid ID | Budget event window selection. |
| `start_mode` | `Immediate` | Immediate or Shortcut arming. |
| `end_mode` | `Duration` | Manual, BudgetEvent, or Duration stop policy. |
| `reopen_interface` | `true` | Reopen developer UI after recording. |

### Agentic capture requests and content policy

[Declarations](../../include/devSystems/devAgenticWorkflow/AgenticCapture.hpp), namespace `FlowUi::devSystems::agentic`. These user-supplied operation policies remain under `include/devSystems`, alongside development startup types.

| Type.member | Default | Meaning |
|---|---|---|
| `AgenticObjective.metric` | `window_present_call_cadence_fps` | Metric used for objective evaluation. |
| `.comparison` | `gt` | Comparison operator. |
| `.threshold` | `40` | Objective threshold. |
| `.minimum_valid_intervals` | `120` | Minimum valid measurement intervals. |
| `.minimum_duration_ns` | `10,000,000,000` | Minimum evidence duration. |
| `.minimum_passes` | `5` | Minimum successful passes. |
| `.maximum_p95_interval_ns` | absent | Optional p95 interval ceiling. |
| `AgenticContentPolicy.include_tree_text` | `false` | Include tree text. |
| `.include_native_messages` | `true` | Include native diagnostic messages. |
| `.include_stacks` | `true` | Include captured stacks. |
| `.include_source_paths` | `true` | Include source paths. |
| `.include_interaction_labels` | `false` | Persist copied target labels. |
| `.include_interaction_outcomes` | `false` | Persist app target outcomes/action payloads. |
| `AgenticCaptureRequest.window_ids` | empty | Explicit window selection. |
| `.scenario_id` | empty | Scenario identity. |
| `.scenario_revision` | empty | Scenario revision. |
| `.scenario_digest` | empty | Scenario content digest. |
| `.fixture_id` | empty | Fixture identity. |
| `.idempotency_key` | empty | Repeated request identity. |
| `.objective` | absent | Optional objective policy. |
| `.limits` | `AgenticLimits{}` | Per-request bounds, capability checked against service. |
| `.content` | `{}` | Evidence content selection. |
| `.duration_ns` | `1,000,000,000` | Requested duration. |
| `.warmup_ticks` | `0` | Initial warm-up ticks. |
| `.measurement_ticks` | `0` | Requested explicit measurement tick count. |
| `.seed` | `0` | Scenario seed. |
| `.maximum_expected_tick_cadence` | `240` | Cadence bound used in planning. |
| `.mode` | `windowed` | Execution mode. |
| `.profile` | `performance_diagnose` | Capture profile. |
| `.performance` | `required` | Performance evidence requirement. |
| `.runtime_diagnostics` | `required` | Runtime diagnostic evidence requirement. |
| `.memory` | `omit` | Memory evidence requirement. |
| `.visual` | `omit` | Visual evidence requirement. |
| `.allow_degraded` | `false` | Permit degraded evidence where supported. |
| `.capture_window` | `true` | Capture window images when visual capture is requested/permitted. |
| `.capture_viewports` | `false` | Include viewport images. |

[`AgenticShutdownPolicy`](../../include/devSystems/devAgenticWorkflow/AgenticService.hpp) additionally has `publish_partial=true` (publish incomplete CPU evidence on shutdown) and `device_lost=false` (device-loss shutdown context). It is an operation policy, not an app default.

### Other authoring values and internal UI settings

Public style/resource descriptions are intentionally separate: [FlowUiTheme](../../include/FlowUi/Public/FlowUiTheme.hpp), [TextureRef](../../include/FlowUi/Public/TextureRef.hpp), [viewport API and descriptions](../../include/FlowUi/Public/managers/ViewPortManager.hpp), and [FSEL](../../include/FSEL). They configure particular elements/resources rather than library startup. Theme fields, texture descriptions, FSEL parameters, and Clay layouts are documented by their authoring APIs.

Developer-panel-only settings can remain internal. Examples are [`MemoryPlannerSettings`](../../include/devSystems/devInterface/Memory/Analysis/DevMemoryPlanner.hpp), which holds reserve/percentile/margin/growth-factor choices for a what-if calculation, and [`MemoryHistoryLaneSettings`](../../include/devSystems/devInterface/Memory/Workbench/DevMemoryWorkbenchData.hpp), which holds panel source/scope/metric selections and resolved UI bookkeeping. They are not missing AppConfig members. Likewise [`DevFieldOptions`](../../include/devSystems/devTooling/schema/DevSchemaDescriptor.hpp) and [`DevRolePolicy`](../../include/devSystems/devTooling/schema/DevSchemaTypes.hpp) configure schema authoring, not startup.

### Agentic interaction limits

[AgenticInteractionLimits](../../include/devSystems/devAgenticWorkflow/AgenticInteraction.hpp) bounds one decoded/compiled input plan. These bounds are separate from service transport limits and require agentic support.

| Member | Default | Meaning |
|---|---|---|
| `maximum_frames` | `4096` | Selected input frames. |
| `maximum_spans` | `4096` | Compiled input spans. |
| `maximum_steps` | `256` | Mechanical steps. |
| `maximum_target_candidates` | `128` | Geometry candidates considered during target resolution. |
| `maximum_text_codepoints` | `16384` | Unicode scalar values accepted as interaction text. |
| `maximum_observations` | `8192` | Retained interaction observations. |
| `maximum_artifact_bytes` | `16 MiB` | Artifact payload bound. |
| `maximum_duration_ns` | `60,000,000,000` | Compiled workload duration bound. |

## Internal assembly types

[StorageConfig and WindowStorageDesc](../../include/FlowUi/Private/storageSystem/StorageTypes.hpp) assemble resolved capacities, budgets, frame arenas, and descriptor limits. [AppWindowConfig](../../include/FlowUi/Private/AppRuntime.hpp) combines native, Vulkan, and UI settings for a window. These remain implementation types; configure the supported startup and operation values described above.

## Documentation and output locations

`docs/` holds Markdown documentation and generated Doxygen output. Agentic screenshots, captures, logs, reports, and verification matrices, including Markdown reports, belong under `.flowui/agentic/`. Harness exports default to `.flowui/agentic/verification/<suite>/`; the standard evidence store is `.flowui/agentic/v1/`. See the [agentic workflow guide](../agentic-workflow.md).

## Registered capacity keys {#flowui_config_capacity_keys}

Pass these exact keys to `makeMemoryCapacityTargetId()`. Defaults and alignment follow the [capacity registry](../../include/FlowUi/Private/storageSystem/capacity/MemoryCapacityTargets.hpp). The accepted maximum constrains the startup hint; it is not a command to allocate that maximum or a runtime growth ceiling. Zero selects the listed default, which can itself mean no explicit reserve.

`SIZE_MAX` follows the target architecture; `UINT32_MAX` is the 32-bit unsigned maximum. Scope identifies the owner or context. Entries exclude invalid-handle table sentinels. Feature-specific owners only consume their capacities when the corresponding subsystem is built.

| Key | Scope | Default | Unit | Maximum | Alignment |
|---|---|---|---|---|---|
| `flowui.tuning.storage.initial_persistent_cpu_bytes` | storage owner | `4194304` | bytes | `SIZE_MAX / 2` | `64` |
| `flowui.tuning.storage.initial_string_bytes` | storage owner | `1048576` | bytes | `SIZE_MAX / 2` | `64` |
| `flowui.tuning.storage.transient_bytes_per_frame_per_window` | storage owner | `1048576` | bytes | `SIZE_MAX / 2` | `64` |
| `flowui.tuning.storage.transient_bytes_per_worker` | storage owner | `1048576` | bytes | `SIZE_MAX / 2` | `64` |
| `flowui.tuning.storage.initial_decode_scratch_bytes` | storage owner | `8388608` | bytes | `SIZE_MAX / 2` | `64` |
| `flowui.tuning.storage.initial_upload_staging_bytes` | storage owner | `16777216` | bytes | `SIZE_MAX / 2` | `64` |
| `flowui.tuning.storage.initial_instance_bytes_per_frame` | storage owner | `1048576` | bytes | `SIZE_MAX / 2` | `256` |
| `flowui.tuning.storage.blobs_entries` | storage | `128` | entries | `UINT32_MAX - 1` | `1` |
| `flowui.tuning.storage.buffers_entries` | storage | `64` | entries | `UINT32_MAX - 1` | `1` |
| `flowui.tuning.storage.images_entries` | storage | `256` | entries | `UINT32_MAX - 1` | `1` |
| `flowui.tuning.storage.image_views_entries` | storage | `384` | entries | `UINT32_MAX - 1` | `1` |
| `flowui.tuning.storage.samplers_entries` | storage | `16` | entries | `UINT32_MAX - 1` | `1` |
| `flowui.tuning.storage.textures_entries` | storage | `512` | entries | `UINT32_MAX - 1` | `1` |
| `flowui.tuning.storage.renderer_objects_entries` | storage | `32` | entries | `UINT32_MAX - 1` | `1` |
| `flowui.tuning.storage.manager_records_entries` | storage | `128` | entries | `UINT32_MAX - 1` | `1` |
| `flowui.tuning.storage.persistent_records_entries` | storage | `1024` | entries | `UINT32_MAX - 1` | `1` |
| `flowui.tuning.storage.windows_entries` | storage | `2` | entries | `UINT32_MAX - 1` | `1` |
| `flowui.tuning.storage.strings_entries` | storage | `0` | entries | `UINT32_MAX - 1` | `1` |
| `flowui.tuning.storage.string_ids_entries` | storage | `0` | entries | `UINT32_MAX - 1` | `1` |
| `flowui.tuning.storage.texture_lookup_entries` | storage | `0` | entries | `UINT32_MAX - 1` | `1` |
| `flowui.tuning.storage.sampler_lookup_entries` | storage | `0` | entries | `UINT32_MAX - 1` | `1` |
| `flowui.tuning.storage.renderer_layout_lookup_entries` | storage | `0` | entries | `UINT32_MAX - 1` | `1` |
| `flowui.tuning.storage.renderer_pipeline_lookup_entries` | storage | `0` | entries | `UINT32_MAX - 1` | `1` |
| `flowui.tuning.storage.upload_states_entries` | storage | `0` | entries | `UINT32_MAX - 1` | `1` |
| `flowui.tuning.storage.completed_submissions_entries` | storage | `0` | entries | `UINT32_MAX - 1` | `1` |
| `flowui.tuning.storage.diagnostics_entries` | storage | `0` | entries | `UINT32_MAX - 1` | `1` |
| `flowui.tuning.storage.retirements_entries` | storage | `0` | entries | `UINT32_MAX - 1` | `1` |
| `flowui.tuning.input_fields.registry_entries` | window | `0` | entries | `UINT32_MAX - 1` | `1` |
| `flowui.tuning.input_fields.pending_commands_entries` | window | `0` | entries | `UINT32_MAX - 1` | `1` |
| `flowui.tuning.input_fields.selected_text_bytes` | window | `0` | bytes | `UINT32_MAX - 1` | `1` |
| `flowui.tuning.input_fields.rect_override_entries` | window | `64` | entries | `UINT32_MAX - 1` | `1` |
| `flowui.tuning.input_fields.color_override_entries` | window | `32` | entries | `UINT32_MAX - 1` | `1` |
| `flowui.tuning.input_fields.layout_override_entries` | window | `0` | entries | `UINT32_MAX - 1` | `1` |
| `flowui.tuning.popups.committed_records_entries` | window | `0` | entries | `UINT32_MAX - 1` | `1` |
| `flowui.tuning.popups.working_records_entries` | window | `0` | entries | `UINT32_MAX - 1` | `1` |
| `flowui.tuning.popups.submission_lookup_entries` | window | `0` | entries | `UINT32_MAX - 1` | `1` |
| `flowui.tuning.popups.stack_entries` | window | `0` | entries | `UINT32_MAX - 1` | `1` |
| `flowui.tuning.popups.submissions_entries` | window | `0` | entries | `UINT32_MAX - 1` | `1` |
| `flowui.tuning.shortcuts.chords_entries` | window | `0` | entries | `UINT32_MAX - 1` | `1` |
| `flowui.tuning.shortcuts.registrations_entries` | window | `0` | entries | `UINT32_MAX - 1` | `1` |
| `flowui.tuning.shortcuts.keys_entries` | window | `0` | entries | `UINT32_MAX - 1` | `1` |
| `flowui.tuning.actions.bindings_entries` | app | `0` | entries | `UINT32_MAX - 1` | `1` |
| `flowui.tuning.actions.deferred_removals_entries` | app | `0` | entries | `UINT32_MAX - 1` | `1` |
| `flowui.tuning.fonts.family_lookup_entries` | app | `0` | entries | `UINT32_MAX - 1` | `1` |
| `flowui.tuning.fonts.id_lookup_entries` | app | `0` | entries | `UINT32_MAX - 1` | `1` |
| `flowui.tuning.fonts.name_lookup_entries` | app | `0` | entries | `UINT32_MAX - 1` | `1` |
| `flowui.tuning.fonts.atlas_layers_entries` | app | `0` | entries | `UINT32_MAX - 1` | `1` |
| `flowui.tuning.icons.documents_entries` | app | `0` | entries | `UINT32_MAX - 1` | `1` |
| `flowui.tuning.icons.requests_entries` | app | `0` | entries | `UINT32_MAX - 1` | `1` |
| `flowui.tuning.icons.reverse_lookup_entries` | app | `0` | entries | `UINT32_MAX - 1` | `1` |
| `flowui.tuning.icons.variants_entries` | app | `0` | entries | `UINT32_MAX - 1` | `1` |
| `flowui.tuning.icons.pages_entries` | app | `0` | entries | `UINT32_MAX - 1` | `1` |
| `flowui.tuning.icons.retired_regions_entries` | app | `0` | entries | `UINT32_MAX - 1` | `1` |
| `flowui.tuning.input_fields.field.carets_entries` | one field maximum | `0` | entries | `UINT32_MAX - 1` | `1` |
| `flowui.tuning.input_fields.field.focus_retention_element_ids_entries` | one field maximum | `0` | entries | `UINT32_MAX - 1` | `1` |
| `flowui.tuning.input_fields.field.visible_line_strings_entries` | one field maximum | `0` | entries | `UINT32_MAX - 1` | `1` |
| `flowui.tuning.input_fields.field.visible_lines_entries` | one field maximum | `0` | entries | `UINT32_MAX - 1` | `1` |
| `flowui.tuning.input_fields.field.submitted_text_spans_entries` | one field maximum | `0` | entries | `UINT32_MAX - 1` | `1` |
| `flowui.tuning.input_fields.field.frame_transactions_entries` | one field maximum | `0` | entries | `UINT32_MAX - 1` | `1` |
| `flowui.tuning.input_fields.field.frame_transaction_views_entries` | one field maximum | `0` | entries | `UINT32_MAX - 1` | `1` |
| `flowui.tuning.input_fields.field.frame_command_requests_entries` | one field maximum | `0` | entries | `UINT32_MAX - 1` | `1` |
| `flowui.tuning.input_fields.field.wrap_cache_by_hard_line_entries` | one field maximum | `0` | entries | `UINT32_MAX - 1` | `1` |
| `flowui.tuning.ui.flow_scope_depth_entries` | one service/window maximum | `32` | entries | `UINT32_MAX - 1` | `1` |
| `flowui.tuning.ui.constructed_depth_entries` | one service/window maximum | `16` | entries | `UINT32_MAX - 1` | `1` |
| `flowui.tuning.ui.text_cache_entries` | one service/window maximum | `0` | entries | `256` | `1` |
| `flowui.tuning.ui.hovered_element_ids_entries` | window snapshot | `0` | entries | `UINT32_MAX - 1` | `1` |
| `flowui.tuning.ui.pressed_element_ids_entries` | window snapshot | `0` | entries | `UINT32_MAX - 1` | `1` |
| `flowui.tuning.ui.held_element_ids_entries` | window snapshot | `0` | entries | `UINT32_MAX - 1` | `1` |
| `flowui.tuning.ui.released_element_ids_entries` | window snapshot | `0` | entries | `UINT32_MAX - 1` | `1` |
| `flowui.tuning.app.windows_entries` | app | `0` | entries | `UINT32_MAX - 1` | `1` |
| `flowui.tuning.app.managed_entries` | app | `0` | entries | `UINT32_MAX - 1` | `1` |
| `flowui.tuning.app.dispatch_entries` | app | `0` | entries | `UINT32_MAX - 1` | `1` |
| `flowui.tuning.themes.types_entries` | app | `0` | entries | `UINT32_MAX - 1` | `1` |
| `flowui.tuning.themes.variants_entries` | one type maximum | `0` | entries | `UINT32_MAX - 1` | `1` |
| `flowui.tuning.elements.windows_entries` | app | `0` | entries | `UINT32_MAX - 1` | `1` |
| `flowui.tuning.elements.states_entries` | window | `0` | entries | `UINT32_MAX - 1` | `1` |
| `flowui.tuning.elements.deferred_erases_entries` | window | `0` | entries | `UINT32_MAX - 1` | `1` |
| `flowui.tuning.elements.gc_candidates_entries` | window | `0` | entries | `UINT32_MAX - 1` | `1` |
| `flowui.tuning.elements.touched_entries` | window | `0` | entries | `UINT32_MAX - 1` | `1` |
| `flowui.tuning.elements.created_entries` | window | `0` | entries | `UINT32_MAX - 1` | `1` |
| `flowui.tuning.elements.policies_entries` | window | `0` | entries | `UINT32_MAX - 1` | `1` |
| `flowui.tuning.icons.free_rectangles_entries` | one page maximum | `0` | entries | `UINT32_MAX - 1` | `1` |
| `flowui.tuning.allocator.arena_pages_entries` | one owner maximum | `0` | entries | `UINT32_MAX - 1` | `1` |
| `flowui.tuning.allocator.pool_slabs_entries` | one owner maximum | `0` | entries | `UINT32_MAX - 1` | `1` |
| `flowui.tuning.allocator.pool_allocations_entries` | one owner maximum | `0` | entries | `UINT32_MAX - 1` | `1` |
| `flowui.tuning.allocator.pool_free_blocks_entries` | one owner maximum | `2` | entries | `UINT32_MAX - 1` | `1` |
| `flowui.tuning.storage.frame.used_resources_entries` | one owner maximum | `0` | entries | `UINT32_MAX - 1` | `1` |
| `flowui.tuning.storage.frame.pending_writes_entries` | one owner maximum | `4` | entries | `UINT32_MAX - 1` | `1` |
| `flowui.tuning.viewports.records_entries` | one owner maximum | `0` | entries | `UINT32_MAX - 1` | `1` |
| `flowui.tuning.viewports.facades_entries` | one owner maximum | `0` | entries | `UINT32_MAX - 1` | `1` |
| `flowui.tuning.viewports.texture_owners_entries` | one owner maximum | `0` | entries | `UINT32_MAX - 1` | `1` |
| `flowui.tuning.viewports.retired_entries` | one owner maximum | `0` | entries | `UINT32_MAX - 1` | `1` |
| `flowui.tuning.fonts.family_faces_entries` | one owner maximum | `0` | entries | `UINT32_MAX - 1` | `1` |
| `flowui.tuning.development.tree.flow_nodes_entries` | one owner maximum | `512` | entries | `UINT32_MAX - 1` | `1` |
| `flowui.tuning.development.tree.flow_roots_entries` | one owner maximum | `128` | entries | `UINT32_MAX - 1` | `1` |
| `flowui.tuning.development.tree.strings_bytes` | one owner maximum | `65536` | bytes | `UINT32_MAX - 1` | `1` |
| `flowui.tuning.development.tree.diagnostics_entries` | one owner maximum | `64` | entries | `UINT32_MAX - 1` | `1` |
| `flowui.tuning.development.tree.active_depth_entries` | one owner maximum | `64` | entries | `UINT32_MAX - 1` | `1` |
