# Developer Systems

All developer support is implemented under [include/devSystems](../../include/devSystems/) and [src/devSystems](../../src/devSystems/). `FLOW_UI_DEV_MODE` is the build switch for these modern systems. The obsolete developer runtime, registry, JSON exporter, debug view, and legacy widget tree have been removed completely.

## Ownership and data flow

`App` owns [DevTooling](../../include/devSystems/devTooling/DevTooling.hpp), [DevMonitoringAndReporting](../../include/devSystems/devMonitoringAndReporting/DevMonitoringAndReporting.hpp), and the dedicated-window [DevInterface](../../include/devSystems/devInterface/Permanents/Backend/DevInterface.hpp). Window UI managers borrow the schema/override services and retain their own tree-capture state. Shared changes publish at the application safe point, outside active layout construction.

The `FlowUi::devSystems::tooling` namespace contains the schema descriptors and registry, catalogues, tree snapshots, typed override engine, and baking pipeline. Descriptors discover types through ADL, intrusive `devSchema()`, or explicit adapters. The registry queues roots and publishes immutable generation-owning schema views. Effective parameter and theme captures retain their schema generation; they are not untyped field maps.

During a builder invocation, baked and live overrides modify the parameter draft before callbacks run. The effective value capture and Flow/Clay tree correlation then feed Inspect and Catalogue. `DevOverrideEngine` accepts typed transactions, validates targets/fields/constraints against the schema, and commits at a safe point. The interface reads these services and submits changes through the same command path.

Monitoring has separate timing, memory, and error services under [devMonitoringAndReporting](../../include/devSystems/devMonitoringAndReporting/). The Performance interface reads bounded timing captures and immutable analysis snapshots. There is no parallel rolling-telemetry runtime.

## Durable changes and optional evidence

[DevBakePipeline](../../include/devSystems/devTooling/bake/DevBakePipeline.hpp) reviews and writes `.flowui/changes/active.flowchanges`. [CMake baking](../../cmake/FlowUiBaking.cmake) generates typed overlays compiled into the next build, including builds without developer support. Schema, target, source, and revision diagnostics remain explicit.

[Agentic workflow](../agentic-workflow.md) is an additional compile-time and runtime opt-in. Its immutable observations and frame images live in a separate evidence store. Captures are not change manifests and do not authorize source mutation.

See the [Developer Systems tutorial](../tutorials/developer_mode.md) for configuration, schemas, catalogue ingestion, and current authoring examples.
