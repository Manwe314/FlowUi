#!/usr/bin/env python3
"""Exercise CPU selector ceilings and historical policy through the real MCP adapter."""
from __future__ import annotations

import argparse
import asyncio
from pathlib import Path

from DevInterfaceNotesMcpTests import DevInterfaceNotesVerification, HELPER, REPOSITORY


class ProbeVerification(DevInterfaceNotesVerification):
    async def catalogue(self) -> None:
        levels = ["Frame Only", "Summary", "Balanced", "Deep"]
        ceiling = self.arguments_ceiling
        await self.execute("probe-warmup", visual=False)
        await self.configure(active_tab="performance", inspector_width=320, window_width=1420)
        await self.execute("probe-ceiling-" + str(ceiling))
        assert "Build: " + levels[ceiling] in self.texts(), self.texts()
        await self.execute("probe-options-" + str(ceiling), [HELPER.click(self.flow("cpu-reporting-level"))])
        for level, label in enumerate(levels):
            assert ("Record: " + label in self.texts()) == (level <= ceiling), self.texts()
        await self.execute("probe-select-" + str(ceiling), [HELPER.click(self.clay("option[" + str(ceiling) + "]"))])
        state = await self.configure(active_tab="performance")
        assert int(state["recording_cpu_level"]) == ceiling, state
        await self.configure(capture_timing=True, active_tab="performance")
        await self.execute("probe-recorded-policy-" + str(ceiling))
        expected = "Recorded CPU: " + levels[ceiling]
        assert expected in self.texts() and "Depth controls change display only" in self.texts(), self.texts()
        if ceiling:
            await self.execute("probe-recording-open-zero", [HELPER.click(self.flow("cpu-reporting-level"))])
            await self.execute("probe-recording-zero", [HELPER.click(self.clay("option[0]"))])
            state = await self.configure(active_tab="performance")
            assert int(state["recording_cpu_level"]) == 0, state
            await self.execute("probe-historical-policy-" + str(ceiling))
            assert expected in self.texts(), self.texts()
        self.record_layout_check("probe-policy-contract", "recorded-policy-and-selector-independent", True,
            compiled_ceiling=ceiling, recorded_policy=levels[ceiling], current_recording_level=0)
        self.save()


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--cli", type=Path, required=True)
    parser.add_argument("--demo", type=Path, required=True)
    parser.add_argument("--workspace", type=Path, default=REPOSITORY)
    parser.add_argument("--output-directory", type=Path, required=True)
    parser.add_argument("--compiled-ceiling", type=int, choices=range(4), required=True)
    arguments = parser.parse_args()
    arguments.fixture = True
    arguments.stage = "complete"
    verifier = ProbeVerification(arguments)
    verifier.arguments_ceiling = arguments.compiled_ceiling
    return asyncio.run(verifier.run())


if __name__ == "__main__":
    raise SystemExit(main())
