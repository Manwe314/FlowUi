#!/usr/bin/env python3
"""Verify native-window and focused-text shortcuts through the official MCP adapter."""
from __future__ import annotations

import argparse
import asyncio
import importlib.util
import json
import math
from pathlib import Path
import subprocess
import sys
import time
import uuid


REPOSITORY = Path(__file__).resolve().parents[2]
HELPER_PATH = REPOSITORY / "tests/DevSystemsTests/verify_catalogue.py"
SPECIFICATION = importlib.util.spec_from_file_location("window_shortcuts_helpers", HELPER_PATH)
HELPER = importlib.util.module_from_spec(SPECIFICATION)
SPECIFICATION.loader.exec_module(HELPER)

CONTROL = 341
SHIFT = 340
F8 = 297
F9 = 298
F10 = 299
F11 = 300
HOME = 268
END = 269
RIGHT = 262
DELETE = 261


def chord(*keys: int, frames: int = 1) -> dict:
    return {"operation": "key.chord", "keys": sorted(keys), "frames": str(frames)}


class WindowShortcutsVerification(HELPER.CatalogueVerification):
    def __init__(self, arguments: argparse.Namespace):
        arguments.only_atlas = False
        super().__init__(arguments)
        self.checks: list[dict] = []
        self.limitations: list[str] = []
        self.states: list[dict] = []

    def prepare(self) -> None:
        with (self.output / "demo.log").open("wb") as log:
            self.process = subprocess.Popen(
                [str(self.demo), "--agentic-store", str(self.store)], cwd=self.workspace,
                stdin=subprocess.DEVNULL, stdout=log, stderr=subprocess.STDOUT)

    def check(self, name: str, passed: bool, **evidence: object) -> None:
        record = {"check": name, "passed": bool(passed), **evidence}
        self.checks.append(record)
        self.save()
        assert passed, record

    async def call(self, operation: str, arguments: dict | None = None,
                   *, live: bool = True, image_path: Path | None = None) -> dict:
        outgoing = dict(arguments or {})
        if operation in {"verification.plan", "verification.run"}:
            request = dict(outgoing["request"])
            request["suite_id"] = "window.default-shortcuts"
            limits = dict(request["limits"])
            # Verification reserves one quarter of its snapshot budget for raw+RGB readback.
            # 1920x1080 RGBA8 fullscreen requires 14.5 MB, above the helper's 8 MiB budget.
            limits["maximum_snapshot_bytes"] = "67108864"
            limits["maximum_retained_bytes"] = "134217728"
            request["limits"] = limits
            outgoing["request"] = request
        if operation == "artifact.image" and image_path is not None and image_path.stem == "fullscreen-active":
            outgoing["crop"] = {"x": 0, "y": 0, "width": 960, "height": 540}
        return await super().call(operation, outgoing, live=live, image_path=image_path)

    async def action(self, action_id: str) -> dict:
        admitted = await self.call("action.invoke", {
            "action_id": action_id, "revision": "1", "arguments": {},
            "idempotency_key": action_id + "-" + uuid.uuid4().hex,
            "capture": {"duration_ns": "10000000000", "maximum_expected_tick_cadence": 60,
                "sections": {"performance": "omit", "runtime_diagnostics": "omit",
                    "memory": "omit", "visual": "omit"},
                "content": {"include_interaction_outcomes": True},
                "limits": {"maximum_snapshot_bytes": "33554432"}}})
        deadline = time.monotonic() + 20
        while time.monotonic() < deadline:
            status = await self.call("job.status", {"job_id": admitted["job_id"]})
            if status["phase"] in {"sealed", "failed", "canceled"}:
                assert status["phase"] == "sealed", status
                outcome = await self.call("interaction.query", {"job_id": admitted["job_id"]})
                assert outcome["action"]["completed"] and outcome["action"]["completion_succeeded"], outcome
                return outcome["action"]["result"]
            await asyncio.sleep(.025)
        raise TimeoutError(f"Action did not complete: {action_id}")

    async def state(self, label: str) -> dict:
        state = await self.action("window_shortcuts.state")
        self.states.append({"label": label, "state": state})
        self.save()
        return state

    def field_target(self, name: str) -> dict:
        candidates = [node for node in self.tree.get("flow_nodes", [])
            if node["debug_name"] == name or node["debug_name"].endswith("/" + name)]
        assert len(candidates) == 1, (name, [node["debug_name"] for node in candidates])
        return {"flow_instance_id": candidates[0]["instance_id"]}

    def measure_layout(self, case_id: str) -> None:
        nodes = self.tree.get("clay_nodes", [])
        root = nodes[0]["bounds"] if nodes else {}
        passed = bool(root) and all(math.isfinite(root[key]) for key in ("x", "y", "width", "height")) \
            and root["width"] > 0 and root["height"] > 0
        self.layout_checks.append({"case_id": case_id, "check": "complete-positive-window-layout",
            "passed": passed, "bounds": root})
        assert passed, self.layout_checks[-1]

    async def execute(self, name: str, steps: list[dict] | None = None,
                      *, visual: bool = True) -> dict:
        report = await super().execute(name, steps, visual=visual)
        for case in report["cases"]:
            manifest_path = Path(case["manifest"]).resolve(strict=True)
            assert manifest_path.is_relative_to(self.store.resolve()), manifest_path
            manifest = json.loads(manifest_path.read_text())
            assert manifest["run_id"] == self.run_id and manifest["capture_id"] == case["capture_id"]
            for artifact in manifest["artifacts"]:
                artifact_path = (manifest_path.parent / artifact["path"]).resolve(strict=True)
                assert artifact_path.is_relative_to(manifest_path.parent), artifact
                assert artifact_path.stat().st_size == int(artifact["bytes"]), artifact
                assert self.fingerprint(artifact_path) == artifact["sha256"], artifact
        if visual:
            from PIL import Image
            with Image.open(self.output / (name + ".png")) as image:
                colors = image.convert("RGB").getcolors(image.width * image.height)
                self.check(name + ":rendered-image", len(colors or []) >= 8,
                    dimensions=list(image.size), distinct_colors=len(colors or []))
        return report

    async def catalogue(self) -> None:
        initial = await self.state("initial")
        self.window_id = str(initial["main"]["window_id"])
        await self.execute("main-initial")
        baseline = initial["main"]
        geometry_keys = ("x", "y", "width", "height", "maximized")

        await self.execute("maximize-wrong-modifiers", [chord(F10, CONTROL)], visual=False)
        wrong_modifiers = await self.state("maximize-wrong-modifiers")
        self.check("window-chord-exact-modifiers",
            all(wrong_modifiers["main"][key] == baseline[key] for key in geometry_keys))

        await self.execute("fullscreen-held-press", [chord(F11, frames=6)], visual=False)
        fullscreen = await self.state("fullscreen-held-press")
        self.check("held-key-enters-fullscreen-once", fullscreen["main"]["fullscreen"] and
            not fullscreen["secondary"]["fullscreen"])
        await self.execute("fullscreen-active")
        await self.execute("fullscreen-restore-request", [chord(F11)], visual=False)
        restored = await self.state("fullscreen-restored")
        self.check("fullscreen-restores-windowed-geometry", not restored["main"]["fullscreen"] and
            all(restored["main"][key] == baseline[key] for key in geometry_keys),
            before={key: baseline[key] for key in geometry_keys},
            after={key: restored["main"][key] for key in geometry_keys})
        await self.execute("fullscreen-restored")

        await self.execute("maximize-request", [chord(F10, CONTROL, SHIFT)], visual=False)
        maximized = await self.state("maximize-request")
        if maximized["main"]["maximized"]:
            await self.execute("maximize-restore-request", [chord(F10, CONTROL, SHIFT)], visual=False)
            maximize_restored = await self.state("maximize-restored")
            self.check("maximize-restores-native-state", not maximize_restored["main"]["maximized"])
            await self.execute("maximize-restored")
        else:
            self.limitations.append("Native maximize request had no observed effect; window-manager behavior is unverified.")

        self.window_id = str(initial["secondary"]["window_id"])
        await self.execute("secondary-initial")
        editable = self.field_target("editable")
        await self.execute("text-cut", [HELPER.click(editable), chord(65, CONTROL), chord(88, CONTROL)])
        cut = await self.state("text-cut")
        self.check("select-all-and-cut-use-originating-field", cut["secondary"]["editable"] == "" and
            cut["main"]["editable"] == baseline["editable"])
        await self.execute("text-paste", [chord(86, CONTROL)])
        pasted = await self.state("text-paste")
        self.check("paste-restores-system-clipboard", pasted["secondary"]["editable"] == "alpha beta gamma")
        await self.execute("word-selection-copy", [chord(HOME, CONTROL), chord(RIGHT, CONTROL, SHIFT),
            chord(67, CONTROL), chord(END, CONTROL), chord(86, CONTROL)])
        copied = await self.state("word-selection-copy")
        self.check("word-selection-and-document-navigation",
            copied["secondary"]["editable"] == "alpha beta gammaalpha ")
        await self.execute("word-delete", [chord(HOME, CONTROL), chord(DELETE, CONTROL)])
        deleted = await self.state("word-delete")
        self.check("delete-following-word", deleted["secondary"]["editable"] == "beta gammaalpha ")
        await self.execute("read-only-editing", [HELPER.click(self.field_target("read_only")),
            chord(65, CONTROL), chord(67, CONTROL), chord(88, CONTROL), chord(86, CONTROL),
            chord(DELETE, CONTROL), HELPER.click(editable), chord(65, CONTROL), chord(86, CONTROL)])
        read_only = await self.state("read-only-editing")
        self.check("read-only-retains-copy-and-blocks-mutation", read_only["secondary"]["read_only"] == "locked text" and
            read_only["secondary"]["editable"] == "locked text")
        await self.execute("secondary-history-unbound", [HELPER.click(editable),
            chord(90, CONTROL), chord(90, CONTROL, SHIFT), chord(89, CONTROL)], visual=False)
        no_history = await self.state("secondary-history-unbound")
        self.check("no-default-undo-redo-requests", int(no_history["manual_history_count"]) == 0 and
            int(no_history["history_request_count"]) == 0 and
            no_history["secondary"]["editable"] == read_only["secondary"]["editable"])

        self.window_id = str(initial["main"]["window_id"])
        await self.execute("main-manual-history-ready")
        await self.execute("main-manual-history", [HELPER.click(self.field_target("editable")),
            chord(90, CONTROL, frames=6)], visual=False)
        manual = await self.state("main-manual-history")
        self.check("manual-history-registration-remains-available", int(manual["manual_history_count"]) == 1 and
            int(manual["history_request_count"]) == 0)

        self.window_id = str(initial["secondary"]["window_id"])
        await self.execute("secondary-minimize-request", [chord(F9)], visual=False)
        minimized = await self.state("secondary-minimize-request")
        if minimized["secondary"]["minimized"]:
            self.check("minimize-affects-originating-window", not minimized["main"]["minimized"])
        else:
            self.limitations.append("Native minimize request had no observed effect; window-manager behavior is unverified.")
        await self.action("window_shortcuts.reset")
        await self.execute("secondary-close-request", [chord(F8)], visual=False)
        closed = await self.state("secondary-close-request")
        self.check("secondary-close-leaves-main-open", closed["secondary"]["close_requested"] and
            not closed["main"]["close_requested"] and self.process.poll() is None)
        self.window_id = str(initial["main"]["window_id"])
        await self.execute("main-after-secondary-close")

    def save(self) -> None:
        unchanged = self.fingerprint(self.original_changes) == self.original_digest
        HELPER.write_json(self.output / "evidence.json", {
            "schema": "FlowUi.WindowShortcuts.Verification", "run_id": self.run_id,
            "mcp_calls": len(self.records), "cases": self.cases, "checks": self.checks,
            "states": self.states, "layout_checks": self.layout_checks,
            "failures": self.failures, "limitations": self.limitations,
            "user_changes_unchanged": unchanged,
            "input_delivery": "Official MCP SDK, stdio adapter, native frame input replacement",
            "tree_delivery": "Integrity-checked sealed local artifact; PNG retrieval uses MCP",
            "fullscreen_image_delivery": "960x540 upper-left crop; full native extent is retained in MCP metadata and state",
            "passed": bool(self.cases) and not self.failures and unchanged and
                all(check["passed"] for check in self.checks + self.layout_checks)})


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--cli", type=Path, required=True)
    parser.add_argument("--demo", "--fixture", dest="demo", type=Path, required=True)
    parser.add_argument("--workspace", type=Path, default=REPOSITORY)
    parser.add_argument("--output-directory", type=Path,
        default=REPOSITORY / ".flowui/agentic/verification/window-shortcuts")
    arguments = parser.parse_args()

    async def bounded_run() -> int:
        try:
            return await asyncio.wait_for(WindowShortcutsVerification(arguments).run(), timeout=120)
        except asyncio.TimeoutError:
            print("Window-shortcut MCP verification exceeded its 120-second deadline", file=sys.stderr)
            return 1

    return asyncio.run(bounded_run())


if __name__ == "__main__":
    raise SystemExit(main())
