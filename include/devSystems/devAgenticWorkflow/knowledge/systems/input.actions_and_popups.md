---
topic_id: input.actions_and_popups
pack_version: "1.0"
supported_flowui_versions: ["0.1.0"]
required_capabilities: []
owning_subsystem: input
source_selectors: ["input.actions_and_popups.primary"]
---

# Semantic actions, input and focus

Use finite interaction plans to exercise actual input routing, or explicitly exported application actions when testing application semantics directly. `action list` discovers only remote-callable exports; a development catalogue entry alone grants no invocation capability. Record arguments, observed tick/frame, state revision and assertion result. Readiness can remain pending across normal frames and must have a deadline.

Input and popup behavior is frame-sensitive and platform-thread owned. A transport callback queues work; it cannot mutate GLFW/Clay state while another window is being built. Replay based on coordinates requires explicit window geometry, DPI, visibility and focus preconditions.

A screenshot documents a rendered state. It does not establish tab order, keyboard navigation, shortcut handling, popup lifetime or accessibility semantics. Scenario assertions provide the interaction evidence. Restore developer window visibility separately from keyboard focus after a capture.

Use `interaction targets` for window coordinates, Clay identities and optional Flow/custom regions. A click is a sequence of normal input states, with down/up on distinct frames; it never writes requested IDs into an interaction snapshot. `action invoke` dispatches an exported action and preserves its separate completion contract. `scenario list`/`scenario run` remain available for application-declared workloads and fixtures.

Read `interaction.execution` for offsets, consumer delay and recording. Offset `0` is valid immediately after baseline synchronization. Optionally begin at offset `2` or `3` when startup layout frames are not representative; there is no mandatory delay.

Source: [FlowUi::ActionManager](include/managers/ActionManager.hpp).
