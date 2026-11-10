# Developer Interface backend

`DevInterface` owns the dedicated developer window and its toggle shortcut. `App` initializes it from `DevToolsConfig`, attaches registered application windows, and synchronizes it at the platform safe point.

The retained `DevInterfaceState` reads the modern schema, tree, catalogue, override, timing, memory, and error services. Backend selection and resource leases support the Inspect, Catalogue, Changes, Performance, Memory, and Diagnostics surfaces. Closing or suspending the interface releases borrowed inspection resources before the storage system is destroyed.

See the [public tutorial](../../../../../docs/tutorials/developer_mode.md) and [systems internals](../../../../../docs/internals/dev_runtime.md).
