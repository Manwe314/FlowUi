# Agentic verification validation — 5 November 2026

The Debug Ninja build on Linux with X11 and Vulkan completed with exit code 0. Verification exercised the FSEL demo and its DevInterface through the CLI and official MCP SDK.

| Validation | Result | Evidence |
| --- | --- | --- |
| Agentic, FSEL and Catalogue CTests | 35/35 CTests passed; 120.91 s; includes schema/CLI contracts, image codecs and fences, concurrent admission, required/optional evidence, nested input cancellation, cleanup failures, and retained retry identities and recovery selectors | [CTest registrations](../tests/CMakeLists.txt), [service regressions](../tests/AgenticWorkflowTests/AgenticVerificationServiceTests.cpp) |
| MCP adapter | 21 tests passed, zero skips, including real CLI and SDK stdio parity | [Adapter tests](../tools/flowui-agent-mcp/tests/test_adapter.py) |
| Official SDK against the live demo | 16 tools, 38 operations, 247 calls; nine stages passed, zero failures or uncovered operations | [Portable coverage](verification-evidence/coverage.json) |
| FSEL and DevInterface verification harness | 1,011 checks passed, including 1,000 assertions-only soak repetitions | [Measured summary](verification-evidence/soak-summary.json), [full run results](../build-interactions/verification-soak-final/run_cd551f855868/verification-live-results.json) |

The GUI harness tested Gallery/Writing pointer routing and text entry, real developer Catalogue tab clicks, sequential fixture restoration, exact image/tree frame identity, zero-valued seeds, and deliberately failing assertions with successful command transport. Held input was released before cleanup, a subsequent click passed, and required-pass failures returned exit code 7 with the original report intact. Retry tombstones prevented reruns beyond runtime-history eviction. Reports and original failure pixels remained readable after 1,000 repetitions and application exit.

Screenshots: [FSEL Gallery](../build-interactions/verification-soak-final/run_cd551f855868/gallery-visual.png), [DevInterface Catalogue](../build-interactions/verification-soak-final/run_cd551f855868/developer-visual.png), [failure checkpoint](../build-interactions/verification-soak-final/run_cd551f855868/failure-visual.png). Failure pixels are labeled `next_eligible_frame_after_failure`. Original inventories preserve frame, tree, theme, dimensions and checksums. Generated screenshots remain in build output; portable summary records are saved alongside this report.

| Soak measurement | Observed result |
| --- | --- |
| Repetitions | 1,000 passing suites |
| Completion latency | Median 0.931 s; p95 1.048 s |
| Peak owned snapshot evidence | 719,834 bytes on every repetition |
| Application RSS | 259,743,744–260,104,192 bytes; first-to-last growth 352 KiB |
| Assertions-only image/readback collection | Zero on every repetition |
| Retained evidence store | 54,627,533 bytes across 11,175 files |

The earlier diagnostic baseline was intentionally stopped after 774 successful repetitions. Its late publication median was 2.459 s; the optimized run's late median was 0.121 s after incremental advisory-index maintenance. These Debug runs have different case prefixes and host load, so the design's release-equivalent 50% workflow speed target remains unmeasured. Authoritative manifest validation remains available; the index provides an advisory publication catalogue.

Owned evidence, allocator-retained memory and process RSS are separate measurements. GPU residency, complete readback reservations, encoder workspace and unavailable pipeline spans remain explicitly unavailable where instrumentation cannot measure them. Disk evidence grows independently of bounded runtime history; automatic retention is deferred.

Reproduce the soak after building the demo and CLI:

```sh
python3 tests/AgenticWorkflowTests/AgenticVerificationLiveTests.py \
  build-interactions/flowui-agent build-interactions/tests/flowui_fsel_demo \
  --soak-count 1000 --output-directory build-interactions/verification-soak
```

This first delivery implements batched run-and-report, selected checkpoints, bounded runtime history, durable recovery and protected pruning. [Usage and contracts](agentic-verification.md) describe the supported surface. Contact sheets, automatic retention, generation-checked `store.prune.plan`/`store.prune.apply`, indexed capture reads, and a persistent command bridge remain deferred. Windows transport code has not been executed on Windows.
