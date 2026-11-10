#!/usr/bin/env python3
"""Exercise the Catalogue in an isolated FSEL demo through the official MCP SDK."""
from __future__ import annotations

import argparse
import asyncio
import base64
import ctypes
import ctypes.util
import hashlib
import json
import os
from pathlib import Path
import re
import stat
import struct
import subprocess
import sys
import time
import traceback
import uuid


def write_json(path: Path, value: object) -> None:
    path.write_text(json.dumps(value, ensure_ascii=False, indent=2) + "\n")


def click(target: dict) -> dict:
    return {"operation": "click", "target": target,
        "expect": ["target_hovered", "target_pressed", "target_released", "layout_completed"]}


class CatalogueVerification:
    def __init__(self, arguments: argparse.Namespace):
        self.only_atlas = arguments.only_atlas
        self.repository = arguments.workspace.resolve(strict=True)
        self.cli = arguments.cli.resolve(strict=True)
        self.demo = arguments.demo.resolve(strict=True)
        self.output = arguments.output_directory.resolve() / ("run_" + uuid.uuid4().hex[:12])
        self.output.mkdir(parents=True)
        self.workspace = self.output / "workspace"
        self.workspace.mkdir()
        self.store = self.workspace / ".flowui/agentic/v1"
        self.changes = self.workspace / ".flowui/changes/active.flowchanges"
        self.run_id = ""
        self.window_id = "1"
        self.tree: dict = {}
        self.process = None
        self.endpoint = None
        self.client = None
        self.owners: dict[str, str] = {}
        self.records: list[dict] = []
        self.cases: list[dict] = []
        self.failures: list[dict] = []
        self.layout_checks: list[dict] = []
        self.original_changes = self.repository / ".flowui/changes/active.flowchanges"
        self.original_digest = self.fingerprint(self.original_changes)

    @staticmethod
    def fingerprint(path: Path) -> str | None:
        return hashlib.sha256(path.read_bytes()).hexdigest() if path.is_file() else None

    def prepare(self) -> None:
        self.changes.parent.mkdir(parents=True)
        write_json(self.changes, {"manifestVersion": 2, "schemaFingerprint": "0x0000000000000000",
            "buildFingerprint": "", "createdTimestamp": "", "bakedChanges": [],
            "memoryCapacity": {"version": 1, "enabled": False, "targets": []}})
        with (self.output / "demo.log").open("wb") as log:
            self.process = subprocess.Popen([str(self.demo), "--agentic-store", str(self.store),
                "--agentic-input", "--agentic-actions", "--agentic-visual", "--agentic-dev-interface",
                "--agentic-changes", str(self.changes)], cwd=self.workspace,
                stdin=subprocess.DEVNULL, stdout=log, stderr=subprocess.STDOUT)

    async def ready(self) -> None:
        deadline = time.monotonic() + 30
        while time.monotonic() < deadline:
            if self.process.poll() is not None:
                raise RuntimeError((self.output / "demo.log").read_text(errors="replace")[-4000:])
            descriptors = list((self.store / "runtime/instances").glob("*.json"))
            if descriptors:
                descriptor = json.loads(descriptors[0].read_text())
                assert int(descriptor["pid"]) == self.process.pid, descriptor
                self.run_id = descriptor["run_id"]
                self.endpoint = Path(descriptor["endpoint"])
                return
            await asyncio.sleep(.05)
        raise TimeoutError("FSEL demo did not publish its local endpoint")

    async def discover(self) -> None:
        result = await self.client.list_tools()
        tool_list = getattr(result, "tools", result)
        for tool in tool_list:
            schema = getattr(tool, "input_schema", None) or getattr(tool, "inputSchema")
            for operation in schema["properties"]["operation"]["enum"]:
                self.owners[operation] = tool.name
        write_json(self.output / "mcp-discovery.json", self.owners)
        assert "verification.run" in self.owners and "artifact.image" in self.owners

    async def call(self, operation: str, arguments: dict | None = None,
                   *, live: bool = True, image_path: Path | None = None) -> dict:
        outgoing = dict(arguments or {})
        if live:
            outgoing["instance"] = self.run_id
        sequence = len(self.records) + 1
        record = {"sequence": sequence, "operation": operation, "arguments": outgoing}
        self.records.append(record)
        try:
            result = await self.client.call_tool(self.owners[operation],
                {"operation": operation, "arguments": outgoing})
            envelope = getattr(result, "structured_content", None) or getattr(result, "structuredContent", None)
            assert isinstance(envelope, dict), result
            record["envelope"] = envelope
            assert envelope["operation"] == operation, envelope
            assert envelope.get("exit_code", 0) == 0, envelope
            assert not (getattr(result, "is_error", False) or getattr(result, "isError", False)), envelope
            if image_path is not None:
                images = [item for item in result.content if item.type == "image"]
                assert len(images) == 1, result
                payload = base64.b64decode(images[0].data, validate=True)
                assert payload.startswith(b"\x89PNG\r\n\x1a\n"), "MCP image is not PNG"
                image_path.write_bytes(payload)
                record["image"] = {"path": image_path.name,
                    "sha256": hashlib.sha256(payload).hexdigest(),
                    "dimensions": struct.unpack(">II", payload[16:24])}
            return envelope["result"]
        finally:
            with (self.output / "mcp-calls.jsonl").open("a") as log:
                log.write(json.dumps(record, ensure_ascii=False) + "\n")

    async def terminal_report(self, admitted: dict) -> dict:
        if admitted.get("schema") == "FlowUi.Agentic.VerificationReport":
            return admitted
        if "report" in admitted:
            return admitted["report"]
        deadline = time.monotonic() + 45
        while time.monotonic() < deadline:
            status = await self.call("verification.status", {"job_id": admitted["job_id"],
                "wait_timeout_ms": 1000})
            if status.get("report_published") or status.get("terminal"):
                result = await self.call("verification.report", {"job_id": admitted["job_id"],
                    "max_bytes": 1048576})
                return result.get("report", result)
        raise TimeoutError(f"Verification did not complete: {admitted}")

    async def execute(self, name: str, steps: list[dict] | None = None,
                      *, visual: bool = True) -> dict:
        steps = list(steps or [])
        steps.append({"operation": "wait", "frames": "4", "expect": ["layout_completed"]})
        plan = {"schema": "FlowUi.Agentic.InteractionPlan", "schema_version": "1.0",
            "mode": "common_controls", "window_id": self.window_id,
            "deadline_ns": "20000000000", "content": {"persistence": "retain_text",
                "record_observations": True}, "steps": steps}
        case = {"case_id": name, "workload": {"kind": "interaction_plan", "plan": plan}}
        if visual:
            case["checkpoints"] = [{"checkpoint_id": name, "trigger": "case_verified",
                "evidence": "image_and_tree", "window_id": self.window_id}]
        request = {"schema": "FlowUi.Agentic.VerificationRequest", "schema_version": "1.0",
            "suite_id": "catalogue.layout.interactions", "suite_revision": "1",
            "idempotency_key": name + "-" + uuid.uuid4().hex[:8], "wait_timeout_ms": 20000,
            "execution_timeout_ns": "30000000000", "failure_policy": "stop_suite", "cases": [case],
            "evidence": {"policy": "checkpoints_and_failure" if visual else "assertions_only",
                "include_tree_text": True, "include_interaction_outcomes": True},
            "limits": {"maximum_cases": 1, "maximum_images": "1" if visual else "0",
                "maximum_snapshot_bytes": "33554432", "maximum_retained_bytes": "67108864",
                "maximum_report_bytes": "1048576"}, "report": {"detail": "all"}}
        report = await self.terminal_report(await self.call("verification.run", {"request": request}))
        write_json(self.output / (name + "-report.json"), report)
        assert report["verdict"] == "pass" and report["execution_status"] == "completed", report
        assert int(report["coverage"]["failed"]) == 0, report
        entry = {"case_id": name, "verdict": report["verdict"], "job_id": report["job_id"],
            "coverage": report["coverage"]}
        if visual:
            result_case = report["cases"][0]
            evidence = result_case["checkpoints"][0]
            image = evidence["image"]
            assert evidence["available"] and evidence["tree_artifact"] == image["tree"]["artifact_id"], evidence
            manifest_path = Path(result_case["manifest"]).resolve(strict=True)
            assert manifest_path.is_relative_to(self.store.resolve()), manifest_path
            manifest = json.loads(manifest_path.read_text())
            artifact = next(item for item in manifest["artifacts"]
                if item["artifact_id"] == evidence["tree_artifact"])
            tree_path = (manifest_path.parent / artifact["path"]).resolve(strict=True)
            assert tree_path.is_relative_to(manifest_path.parent), tree_path
            assert tree_path.stat().st_size == int(artifact["bytes"]), artifact
            assert self.fingerprint(tree_path) == artifact["sha256"], artifact
            self.tree = json.loads(tree_path.read_text())
            assert self.tree["complete"] and not self.tree["truncated"], self.tree.keys()
            assert self.tree["window_frame"] == image["window_frame"], evidence
            write_json(self.output / (name + "-tree.json"), self.tree)
            self.measure_layout(name)
            await self.call("artifact.image", {"capture": result_case["capture_id"],
                "image": image["image_id"], "include_image": True}, live=False,
                image_path=self.output / (name + ".png"))
            entry.update({"capture_id": result_case["capture_id"], "image_id": image["image_id"],
                "window_frame": image["window_frame"], "image": name + ".png"})
        self.cases.append(entry)
        self.save()
        print(json.dumps(entry), flush=True)
        return report

    def flow(self, suffix: str, *, contains: str = "") -> dict:
        candidates = [node for node in self.tree.get("flow_nodes", [])
            if node["debug_name"].endswith("/" + suffix) and contains in node["debug_name"]]
        if not candidates:
            labels = {"preview-help": ("Info",), "tokens": ("Tokens",), "components": ("Components",),
                "icon-tint": ("Source", "Tinted"), "paragraph": ("Paragraph",), "glyphs": ("Glyph metrics",),
                "create-icon-raster": ("Rasterize 24px",), "background": ("Checker", "Light", "Dark")}
            if suffix in labels:
                return self.control(labels[suffix])
        assert len(candidates) == 1, (suffix, contains, [node["debug_name"] for node in candidates])
        return {"flow_instance_id": candidates[0]["instance_id"]}

    def control(self, labels: tuple[str, ...]) -> dict:
        candidates = []
        clay_nodes = self.tree.get("clay_nodes", [])
        for node in clay_nodes:
            if node.get("text") not in labels:
                continue
            parent = clay_nodes[node["parent"]]
            ancestor = parent
            while ancestor.get("parent") is not None and "workbench-area" not in ancestor.get("name", ""):
                ancestor = clay_nodes[ancestor["parent"]]
            if "workbench-area" in ancestor.get("name", ""):
                candidates.append(parent)
        assert len(candidates) == 1, (labels, candidates)
        return {"clay_id": str(candidates[0]["clay_id"])}

    def clay(self, suffix: str) -> dict:
        candidates = [node for node in self.tree.get("clay_nodes", [])
            if node.get("name", "").endswith(suffix) and node.get("bounds", {}).get("width", 0) > 0]
        assert len(candidates) == 1, (suffix, [node.get("name") for node in candidates])
        return {"clay_id": str(candidates[0]["clay_id"])}

    def card(self, text: str = "", *, reject: str = "") -> dict:
        candidates = []
        clay_nodes = self.tree.get("clay_nodes", [])
        for node in self.tree.get("flow_nodes", []):
            if not re.search(r"/asset-card\[\d+\]$", node["debug_name"]):
                continue
            root_index = node.get("clay_root")
            if root_index is None:
                continue
            root = clay_nodes[root_index]
            labels = " ".join(str(child.get("text") or "")
                for child in clay_nodes[root_index:root["subtree_end_exclusive"]])
            if text.lower() in labels.lower() and (not reject or reject.lower() not in labels.lower()):
                candidates.append((node, labels))
        assert candidates, ("No gallery card", text, reject)
        return {"flow_instance_id": candidates[0][0]["instance_id"]}

    def bounds(self, suffix: str) -> dict:
        matches = [node["bounds"] for node in self.tree.get("clay_nodes", [])
            if node.get("name", "").endswith("/" + suffix)]
        assert len(matches) == 1, (suffix, matches)
        return dict(matches[0])

    def cached_raster(self, size: int) -> dict:
        nodes = self.tree.get("clay_nodes", [])
        for node in self.tree.get("flow_nodes", []):
            if "/cached-raster-choice[" not in node["debug_name"] or node.get("clay_root") is None:
                continue
            root = nodes[node["clay_root"]]
            if any(child.get("text") == f"{size}px"
                   for child in nodes[root["index"]:root["subtree_end_exclusive"]]):
                return {"flow_instance_id": node["instance_id"]}
        raise AssertionError(f"No cached raster choice for {size}px")

    def texts(self) -> str:
        return "\n".join(str(node.get("text") or "") for node in self.tree.get("clay_nodes", []))

    def measure_layout(self, case_id: str) -> None:
        nodes = self.tree.get("clay_nodes", [])
        for area in ("workbench-area", "selector-area"):
            roots = [node for node in nodes
                if node.get("name", "").endswith("/" + area + "/selected-content")]
            if len(roots) != 1:
                continue
            root = roots[0]
            bounds = root["bounds"]
            for node in nodes[root["index"]:root["subtree_end_exclusive"]]:
                name = node.get("name", "")
                is_viewport = name.endswith(("/gallery-scroll", "/focused-scroll", "/asset-canvas", "/browser-section"))
                is_footer = bool(re.match(r"\d+ / \d+ registered assets", str(node.get("text") or "")))
                if not is_viewport and not is_footer:
                    continue
                observed = node["bounds"]
                passed = (observed["x"] >= bounds["x"] - 2 and observed["y"] >= bounds["y"] - 2 and
                    observed["x"] + observed["width"] <= bounds["x"] + bounds["width"] + 2 and
                    observed["y"] + observed["height"] <= bounds["y"] + bounds["height"] + 2)
                self.layout_checks.append({"case_id": case_id,
                    "check": "footer-visible" if is_footer else name.rsplit("/", 1)[-1] + "-contained",
                    "passed": passed, "root": bounds, "observed": observed})

    async def choose_domain(self, domain: int, name: str) -> None:
        await self.execute(name + "-overview", [click(self.flow(f"domain[{domain}]"))])
        assert "Catalogue" in self.texts(), self.texts()[:500]

    def open_native_developer_interface(self) -> None:
        """Bootstrap the global shortcut, which intentionally ignores agentic-owned input."""
        x11 = ctypes.CDLL(ctypes.util.find_library("X11"))
        xtest = ctypes.CDLL(ctypes.util.find_library("Xtst"))
        pointer = ctypes.c_void_p
        window_type = ctypes.c_ulong
        x11.XOpenDisplay.restype = pointer
        x11.XOpenDisplay.argtypes = [ctypes.c_char_p]
        x11.XDefaultRootWindow.restype = window_type
        x11.XDefaultRootWindow.argtypes = [pointer]
        x11.XInternAtom.restype = window_type
        x11.XInternAtom.argtypes = [pointer, ctypes.c_char_p, ctypes.c_int]
        x11.XQueryTree.argtypes = [pointer, window_type, ctypes.POINTER(window_type),
            ctypes.POINTER(window_type), ctypes.POINTER(ctypes.POINTER(window_type)), ctypes.POINTER(ctypes.c_uint)]
        x11.XGetWindowProperty.argtypes = [pointer, window_type, window_type, ctypes.c_long,
            ctypes.c_long, ctypes.c_int, window_type, ctypes.POINTER(window_type),
            ctypes.POINTER(ctypes.c_int), ctypes.POINTER(ctypes.c_ulong),
            ctypes.POINTER(ctypes.c_ulong), ctypes.POINTER(pointer)]
        x11.XGetInputFocus.argtypes = [pointer, ctypes.POINTER(window_type), ctypes.POINTER(ctypes.c_int)]
        x11.XSetInputFocus.argtypes = [pointer, window_type, ctypes.c_int, ctypes.c_ulong]
        x11.XKeysymToKeycode.argtypes = [pointer, ctypes.c_ulong]
        x11.XKeysymToKeycode.restype = ctypes.c_uint
        x11.XFlush.argtypes = [pointer]
        x11.XFree.argtypes = [pointer]
        x11.XCloseDisplay.argtypes = [pointer]
        xtest.XTestFakeKeyEvent.argtypes = [pointer, ctypes.c_uint, ctypes.c_int, ctypes.c_ulong]
        display = x11.XOpenDisplay(None)
        assert display, "No X11 display for developer shortcut bootstrap"
        root = x11.XDefaultRootWindow(display)
        pid_atom = x11.XInternAtom(display, b"_NET_WM_PID", 0)
        pending = [root]
        owned_window = None
        try:
            while pending and owned_window is None:
                window = pending.pop()
                property_type = window_type()
                property_format = ctypes.c_int()
                count, remaining = ctypes.c_ulong(), ctypes.c_ulong()
                data = pointer()
                x11.XGetWindowProperty(display, window, pid_atom, 0, 1, 0, 0,
                    ctypes.byref(property_type), ctypes.byref(property_format), ctypes.byref(count),
                    ctypes.byref(remaining), ctypes.byref(data))
                if data:
                    if count.value and ctypes.cast(data, ctypes.POINTER(ctypes.c_ulong))[0] == self.process.pid:
                        owned_window = window
                    x11.XFree(data)
                root_return, parent_return = window_type(), window_type()
                children = ctypes.POINTER(window_type)()
                child_count = ctypes.c_uint()
                if x11.XQueryTree(display, window, ctypes.byref(root_return), ctypes.byref(parent_return),
                        ctypes.byref(children), ctypes.byref(child_count)):
                    pending.extend(children[index] for index in range(child_count.value))
                    if children:
                        x11.XFree(children)
            assert owned_window, "No native window belongs to the launched demo PID"
            previous_focus, revert = window_type(), ctypes.c_int()
            x11.XGetInputFocus(display, ctypes.byref(previous_focus), ctypes.byref(revert))
            x11.XSetInputFocus(display, owned_window, 1, 0)
            keys = [x11.XKeysymToKeycode(display, keysym) for keysym in (0xffe3, 0xffe1, 0x64)]
            for key in keys:
                xtest.XTestFakeKeyEvent(display, key, 1, 0)
            x11.XFlush(display)
            time.sleep(.15)
            for key in reversed(keys):
                xtest.XTestFakeKeyEvent(display, key, 0, 0)
            if previous_focus.value > 1:
                x11.XSetInputFocus(display, previous_focus, revert.value, 0)
            x11.XFlush(display)
        finally:
            x11.XCloseDisplay(display)

    async def catalogue(self) -> None:
        status = await self.call("status")
        self.window_id = next(window["window_id"] for window in status["windows"]
            if not window["internal_development_window"])
        self.open_native_developer_interface()
        await self.execute("open-developer-interface", visual=False)
        for attempt in range(40):
            status = await self.call("status")
            development = [window for window in status["windows"] if window["internal_development_window"]]
            if development:
                self.window_id = development[0]["window_id"]
                break
            await asyncio.sleep(.05)
        else:
            raise AssertionError("Developer interface did not open")
        await self.execute("inspect-reference")
        await self.execute("elements-overview", [click(self.flow("tab-catalogue"))])
        if self.only_atlas:
            await self.atlas()
            return
        await self.execute("element-focused", [click(self.card("Button"))])
        assert "element-stage" in json.dumps(self.tree), "Element preview stage missing"
        await self.execute("element-help", [{"operation": "hover", "target": self.flow("preview-help"),
            "frames": "4", "expect": ["target_hovered"]}])
        splitter = self.flow("domain-splitter")
        geometry = await self.call("interaction.targets", {"window_id": self.window_id, "target": splitter})
        point = geometry["window_point"]
        await self.execute("selector-resized", [{"operation": "drag", "target": splitter,
            "destination": {"point": {"x": point["x"], "y": point["y"] - 65}},
            "frames": "8", "expect": ["layout_completed"]}])
        geometry = await self.call("interaction.targets", {"window_id": self.window_id, "target": splitter})
        point = geometry["window_point"]
        await self.execute("selector-restored", [{"operation": "drag", "target": splitter,
            "destination": {"point": {"x": point["x"], "y": point["y"] + 65}},
            "frames": "8", "expect": ["layout_completed"]}])
        await self.choose_domain(1, "themes")
        await self.execute("theme-tokens", [click(self.card("FSEL"))])
        assert "tokens" in self.texts().lower(), self.texts()[:400]
        await self.execute("theme-components", [click(self.flow("components"))])
        assert "Controls" in self.texts(), self.texts()[:400]
        await self.choose_domain(2, "icons")
        await self.execute("icon-focused", [click(self.card(reject="No cached raster"))])
        await self.execute("icon-rasterized", [click(self.flow("create-icon-raster"))])
        await self.execute("icon-cached-24", [click(self.cached_raster(24))])
        await self.execute("icon-tint", [click(self.flow("icon-tint"))])
        tint_hex = self.flow("hex", contains="/icon-tint-color/")
        await self.execute("icon-tint-edited", [click(tint_hex),
            {"operation": "key.chord", "keys": [65, 341]},
            {"operation": "text.enter", "target": tint_hex, "text": "#FF8844FF", "expect": ["text_focused"]},
            {"operation": "key.chord", "keys": [257]}])
        assert "#FF8844FF" in self.texts(), "Tint editor did not retain the entered hex color"
        await self.execute("icon-light", [click(self.flow("background"))])
        from PIL import Image
        background_bounds = self.bounds("asset-canvas")
        sample_point = (int(background_bounds["x"] + 8), int(background_bounds["y"] + 8))
        with Image.open(self.output / "icon-light.png") as image:
            light_pixel = image.convert("RGB").getpixel(sample_point)
        await self.execute("icon-dark", [click(self.flow("background"))])
        with Image.open(self.output / "icon-dark.png") as image:
            dark_pixel = image.convert("RGB").getpixel(sample_point)
        self.layout_checks.append({"case_id": "icon-dark", "check": "background-pixels-change",
            "passed": sum(light_pixel) > sum(dark_pixel) + 300,
            "light_pixel": light_pixel, "dark_pixel": dark_pixel})
        await self.choose_domain(3, "fonts")
        await self.execute("font-focused", [click(self.card())])
        sample = self.flow("font-sample")
        await self.execute("font-sample-edited", [click(sample), {"operation": "key.chord", "keys": [65, 341]},
            {"operation": "text.enter", "target": sample, "text": "Catalogue sample Aa 012345",
                "expect": ["text_focused"]}])
        assert "Catalogue sample Aa 012345" in self.texts(), "Font specimen did not update from text input"
        await self.execute("font-paragraph", [click(self.flow("paragraph"))])
        await self.execute("font-glyphs", [click(self.flow("glyphs"))])
        await self.choose_domain(5, "actions")
        await self.execute("action-name-tooltip", [{"operation": "hover", "target": self.card("flowui.dev_interface"),
            "frames": "4", "expect": ["target_hovered"]}])
        await self.execute("action-focused", [click(self.card())])
        await self.choose_domain(6, "shortcuts")
        await self.execute("shortcut-focused", [click(self.card())])
        await self.atlas()

    async def atlas(self) -> None:
        await self.choose_domain(8, "atlases")
        await self.execute("icon-atlas-focused", [click(self.card("Icon"))])
        assert "Verified icon owners" not in self.texts(), "Atlas owner list still occupies canvas space"
        canvas = {**self.clay("asset-canvas"), "anchor": {"x": .05, "y": .015}}
        initial_viewport = self.bounds("asset-canvas")
        initial_image = self.bounds("canvas-image")
        await self.execute("icon-atlas-wheel", [{"operation": "scroll", "target": canvas,
            "scroll": {"x": 0, "y": 6}, "frames": "1", "expect": ["layout_completed"]}])
        zoomed_image = self.bounds("canvas-image")
        self.layout_checks.append({"case_id": "icon-atlas-wheel", "check": "wheel-zooms-fixed-viewport",
            "passed": zoomed_image["width"] > initial_image["width"] + 1 and
                self.bounds("asset-canvas") == initial_viewport,
            "initial_image": initial_image, "zoomed_image": zoomed_image,
            "initial_viewport": initial_viewport, "current_viewport": self.bounds("asset-canvas")})
        geometry = await self.call("interaction.targets", {"window_id": self.window_id, "target": canvas})
        point = geometry["window_point"]
        await self.execute("icon-atlas-pan", [{"operation": "drag", "target": canvas,
            "destination": {"point": {"x": point["x"] - 20, "y": point["y"] - 5}},
            "frames": "8", "expect": ["layout_completed"]}])
        panned_image = self.bounds("canvas-image")
        self.layout_checks.append({"case_id": "icon-atlas-pan", "check": "drag-pans-fixed-viewport",
            "passed": (abs(panned_image["x"] - zoomed_image["x"]) > 1 or
                abs(panned_image["y"] - zoomed_image["y"]) > 1) and
                self.bounds("asset-canvas") == initial_viewport,
            "before": zoomed_image, "after": panned_image,
            "initial_viewport": initial_viewport, "current_viewport": self.bounds("asset-canvas")})
        await self.execute("changes-selector", [click(self.flow("tab-changes"))])

    def save(self) -> None:
        unchanged = self.fingerprint(self.original_changes) == self.original_digest
        write_json(self.output / "evidence.json", {"schema": "FlowUi.Catalogue.Verification",
            "run_id": self.run_id, "mcp_calls": len(self.records), "cases": self.cases,
            "required_checks": sum(int(case["coverage"]["required"]) for case in self.cases),
            "failures": self.failures, "layout_checks": self.layout_checks,
            "user_changes_unchanged": unchanged,
            "bootstrap": "Native Ctrl+Shift+D on the owned demo PID; subsequent interactions use MCP",
            "tree_delivery": "Integrity-checked sealed local artifact; PNG retrieval uses MCP",
            "passed": not self.failures and unchanged and bool(self.cases) and
                all(check["passed"] for check in self.layout_checks)})

    async def run(self) -> int:
        try:
            from mcp import Client
            from mcp.client.stdio import StdioServerParameters, stdio_client
            self.prepare()
            await self.ready()
            environment = dict(os.environ)
            environment["PYTHONPATH"] = os.pathsep.join([str(self.repository / "tools/flowui-agent-mcp/src"),
                environment.get("PYTHONPATH", "")])
            parameters = StdioServerParameters(command=sys.executable,
                args=["-m", "flowui_agent_mcp.server", "--workspace", str(self.workspace),
                    "--cli", str(self.cli), "--timeout", "30"], cwd=str(self.workspace), env=environment)
            with (self.output / "adapter.log").open("w") as log:
                async with Client(stdio_client(parameters, errlog=log), read_timeout_seconds=45) as client:
                    self.client = client
                    await self.discover()
                    await self.catalogue()
        except Exception as exception:
            self.failures.append({"error": str(exception), "traceback": traceback.format_exc()})
            print(traceback.format_exc(), file=sys.stderr, flush=True)
        finally:
            if self.process is not None and self.process.poll() is None:
                self.process.terminate()
                try:
                    self.process.wait(timeout=10)
                except subprocess.TimeoutExpired:
                    self.process.kill()
                    self.process.wait(timeout=10)
            if self.endpoint is not None:
                try:
                    metadata = self.endpoint.lstat()
                    if stat.S_ISSOCK(metadata.st_mode) and metadata.st_uid == os.getuid():
                        self.endpoint.unlink()
                except FileNotFoundError:
                    pass
            self.save()
        print(json.dumps({"passed": not self.failures and all(check["passed"] for check in self.layout_checks),
            "cases": len(self.cases),
            "evidence": str(self.output / "evidence.json")}), flush=True)
        return 1 if self.failures or not all(check["passed"] for check in self.layout_checks) else 0


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--only-atlas", action="store_true", help="Run only navigation and atlas zoom/pan checks")
    parser.add_argument("--cli", type=Path, required=True)
    parser.add_argument("--demo", type=Path, required=True)
    parser.add_argument("--workspace", type=Path, default=Path.cwd())
    parser.add_argument("--output-directory", type=Path, required=True)
    return asyncio.run(CatalogueVerification(parser.parse_args()).run())


if __name__ == "__main__":
    raise SystemExit(main())
