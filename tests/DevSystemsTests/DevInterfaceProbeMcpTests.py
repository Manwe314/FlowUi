#!/usr/bin/env python3
"""Verify CPU selector ceilings and reproject one sealed timeline through the real MCP adapter."""
from __future__ import annotations

import argparse
import asyncio
from pathlib import Path

from DevInterfaceNotesMcpTests import DevInterfaceNotesVerification, HELPER, REPOSITORY


class ProbeVerification(DevInterfaceNotesVerification):
    async def call(self, operation: str, arguments: dict | None = None,
                   *, live: bool = True, image_path: Path | None = None) -> dict:
        if operation == "action.invoke" and arguments and arguments.get("action_id") == "dev_notes.configure":
            arguments = dict(arguments)
            arguments["capture"] = {**arguments["capture"], "duration_ns": "1000000000"}
        return await super().call(operation, arguments, live=live, image_path=image_path)

    async def choose_level(self, level: int, case_prefix: str) -> dict:
        await self.execute(case_prefix + "-options", [HELPER.click(self.flow("cpu-reporting-level"))])
        await self.execute(case_prefix, [HELPER.click(self.clay("option[" + str(level) + "]"))])
        state = await self.configure(active_tab="performance")
        assert int(state["recording_cpu_level"]) == level, state
        assert int(state["timeline_view_cpu_level"]) == level, state
        return state

    async def catalogue(self) -> None:
        levels = ["Frame Only", "Summary", "Balanced", "Deep"]
        ceiling = self.arguments_ceiling
        await self.execute("probe-warmup", visual=False)
        state = await self.configure(active_tab="performance", selection="sample",
            inspector_width=320, window_width=1420)
        assert int(state["compiled_cpu_level"]) == ceiling, state
        await self.execute("probe-ceiling-" + str(ceiling))
        assert "Build: " + levels[ceiling] in self.texts(), self.texts()
        await self.execute("probe-options-" + str(ceiling), [HELPER.click(self.flow("cpu-reporting-level"))])
        for level, label in enumerate(levels):
            assert ("CPU: " + label in self.texts()) == (level <= ceiling), self.texts()
        await self.execute("probe-select-" + str(ceiling), [HELPER.click(self.clay("option[" + str(ceiling) + "]"))])
        state = await self.configure(active_tab="performance")
        assert int(state["recording_cpu_level"]) == ceiling, state
        await self.configure(capture_timing=True, active_tab="performance")
        await self.execute("probe-recorded-policy-" + str(ceiling))
        expected = "Recorded CPU: " + levels[ceiling]
        assert expected in self.texts() and "View CPU: " + levels[ceiling] in self.texts(), self.texts()
        state = await self.configure(active_tab="performance")
        generation = int(state["timeline_capture_generation"])
        assert generation > 0 and int(state["timeline_cpu_block_count"]) > 0, state
        baseline = state
        previous_revision = int(state["timeline_snapshot_revision"])
        observations = {ceiling: state}
        sequence = [level for level in [1, 2, 0] if level < ceiling] + ([ceiling] if ceiling else [])
        for level in sequence:
            case_id = "probe-filter-" + str(level)
            state = await self.choose_level(level, case_id)
            counts = [int(count) for count in state["timeline_cpu_level_counts"]]
            self.record_layout_check(case_id, "same-capture-reprojected-at-selected-cpu-level",
                int(state["timeline_capture_generation"]) == generation
                and int(state["timeline_snapshot_revision"]) > previous_revision
                and int(state["timeline_recorded_min_cpu_level"]) == ceiling
                and int(state["timeline_recorded_max_cpu_level"]) == ceiling
                and sum(counts) == int(state["timeline_cpu_block_count"])
                and not any(counts[level + 1:])
                and int(state["timeline_gpu_block_count"]) == int(baseline["timeline_gpu_block_count"]),
                selected_level=level, observed=state, original=baseline)
            assert expected in self.texts() and "View CPU: " + levels[level] in self.texts(), self.texts()
            previous_revision = int(state["timeline_snapshot_revision"])
            observations[level] = state
        ordered_counts = [int(observations[level]["timeline_cpu_block_count"])
            for level in range(ceiling + 1)]
        self.record_layout_check("probe-policy-contract", "cpu-levels-change-real-timeline-content",
            all(lower < higher for lower, higher in zip(ordered_counts, ordered_counts[1:])),
            compiled_ceiling=ceiling, cpu_blocks_by_level=ordered_counts)
        self.record_layout_check("probe-policy-contract", "restoring-detail-restores-the-same-capture",
            int(state["timeline_capture_generation"]) == generation
            and state["timeline_cpu_level_counts"] == baseline["timeline_cpu_level_counts"]
            and state["timeline_cpu_block_count"] == baseline["timeline_cpu_block_count"],
            observed=state, original=baseline)
        self.save()


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--cli", type=Path, required=True)
    parser.add_argument("--demo", type=Path, required=True)
    parser.add_argument("--workspace", type=Path, default=REPOSITORY)
    parser.add_argument("--output-directory", type=Path,
        default=REPOSITORY / ".flowui/agentic/verification/dev-interface-probe")
    parser.add_argument("--compiled-ceiling", type=int, choices=range(4), required=True)
    arguments = parser.parse_args()
    arguments.fixture = True
    arguments.stage = "complete"
    verifier = ProbeVerification(arguments)
    verifier.arguments_ceiling = arguments.compiled_ceiling
    return asyncio.run(verifier.run())


if __name__ == "__main__":
    raise SystemExit(main())
