#!/usr/bin/env python3
"""Verify developer-interface changes with the real MCP adapter and rendered evidence."""
from __future__ import annotations

import argparse
import asyncio
import importlib.util
import json
from pathlib import Path
import sys
import time
import uuid
import subprocess

from DevInterfaceNotesPressedState import verify_header_pressed

REPOSITORY = Path(__file__).resolve().parents[2]
HELPER_PATH = Path(__file__).with_name("verify_catalogue.py")
SPECIFICATION = importlib.util.spec_from_file_location("flowui_catalogue_verification", HELPER_PATH)
HELPER = importlib.util.module_from_spec(SPECIFICATION)
SPECIFICATION.loader.exec_module(HELPER)


class DevInterfaceNotesVerification(HELPER.CatalogueVerification):
    def __init__(self, arguments: argparse.Namespace):
        arguments.only_atlas = False
        super().__init__(arguments)
        self.stage = arguments.stage
        self.fixture = arguments.fixture
        self.fixture_state: dict = {}
        self.defer_layout_failures = True

    def prepare(self) -> None:
        if not self.fixture:
            super().prepare()
            return
        self.changes.parent.mkdir(parents=True)
        HELPER.write_json(self.changes, {"manifestVersion": 2, "schemaFingerprint": "0x0000000000000000",
            "buildFingerprint": "", "createdTimestamp": "", "bakedChanges": [],
            "memoryCapacity": {"version": 1, "enabled": False, "targets": []}})
        with (self.output / "demo.log").open("wb") as log:
            self.process = subprocess.Popen([str(self.demo), "--agentic-store", str(self.store)],
                cwd=self.workspace, stdin=subprocess.DEVNULL, stdout=log, stderr=subprocess.STDOUT)

    async def terminal_report(self, admitted: dict) -> dict:
        if "report" in admitted:
            return admitted["report"]
        if admitted.get("schema") == "FlowUi.Agentic.VerificationReport":
            return admitted
        deadline = time.monotonic() + 120
        sequence = "0"
        while time.monotonic() < deadline:
            status = await self.call("verification.status", {"job_id": admitted["job_id"],
                "after_sequence": sequence, "wait_timeout_ms": 1000})
            sequence = status["sequence"]
            if status.get("report_published") or status.get("terminal"):
                result = await self.call("verification.report", {"job_id": admitted["job_id"],
                    "max_bytes": 1048576})
                return result.get("report", result)
            await asyncio.sleep(.05)
        raise TimeoutError("Verification did not publish its terminal report")

    async def configure(self, *, capture_timing: bool = False, **values: object) -> dict:
        admitted = await self.call("action.invoke", {"action_id": "dev_notes.configure",
            "revision": "1", "arguments": values, "idempotency_key": "configure-" + uuid.uuid4().hex,
            "capture": {"duration_ns": "30000000000", "maximum_expected_tick_cadence": 60,
                **({"profile": "performance_diagnose"} if capture_timing else {}),
                "limits": {"maximum_snapshot_bytes": "33554432"}, "sections": {
                "performance": "required" if capture_timing else "omit", "runtime_diagnostics": "omit", "memory": "omit", "visual": "omit"},
                "content": {"include_interaction_outcomes": True}}})
        deadline = time.monotonic() + 30
        while time.monotonic() < deadline:
            status = await self.call("job.status", {"job_id": admitted["job_id"]})
            if status["phase"] in {"sealed", "failed", "canceled"}:
                assert status["phase"] == "sealed", status
                result = await self.call("interaction.query", {"job_id": admitted["job_id"]})
                assert result["action"]["completed"] and result["action"]["completion_succeeded"], result
                self.fixture_state = result["action"]["result"]
                self.window_id = str(self.fixture_state["interface_window_id"])
                return self.fixture_state
            await asyncio.sleep(.05)
        raise TimeoutError("Fixture configuration did not complete")

    async def execute(self, name: str, steps: list[dict] | None = None,
                      *, visual: bool = True) -> dict:
        report = await super().execute(name, steps, visual=visual)
        if visual:
            self.measure_pixels(name, self.output / (name + ".png"))
            self.save()
            self.assert_case_layout(name)
        return report

    async def execute_batch(self, cases: list[tuple[str, list[dict]]]) -> None:
        """Run a complete finite set in one MCP verification job, retaining each checkpoint."""
        requests = []
        for name, steps in cases:
            requests.append({"case_id": name, "workload": {"kind": "interaction_plan", "plan": {
                "schema": "FlowUi.Agentic.InteractionPlan", "schema_version": "1.0",
                "mode": "common_controls", "window_id": self.window_id,
                "content": {"persistence": "retain_text", "record_observations": True},
                "steps": steps + [{"operation": "wait", "frames": "4", "expect": ["layout_completed"]}]}},
                "checkpoints": [{"checkpoint_id": name, "trigger": "case_verified",
                    "evidence": "image_and_tree", "window_id": self.window_id}]})
        suite = {"schema": "FlowUi.Agentic.VerificationRequest", "schema_version": "1.0",
            "suite_id": "developer.notes.batch", "suite_revision": "1",
            "idempotency_key": "notes-batch-" + uuid.uuid4().hex, "wait_timeout_ms": 20000,
            "execution_timeout_ns": "90000000000", "failure_policy": "stop_suite", "cases": requests,
            "evidence": {"policy": "checkpoints_and_failure", "include_tree_text": True,
                "include_interaction_outcomes": True},
            "limits": {"maximum_cases": len(cases), "maximum_images": str(len(cases)),
                "maximum_snapshot_bytes": "33554432", "maximum_retained_bytes": "268435456",
                "maximum_report_bytes": "1048576"}, "report": {"detail": "all"}}
        plan = await self.call("verification.plan", {"request": suite})
        assert plan["feasible"], plan
        report = await self.terminal_report(await self.call("verification.run", {"request": suite}))
        HELPER.write_json(self.output / (suite["idempotency_key"] + "-report.json"), report)
        assert report["execution_status"] == "completed" and report["verdict"] == "pass", report
        for case in report["cases"]:
            name = case["case_id"]
            checkpoint = case["checkpoints"][0]
            assert checkpoint["available"], checkpoint
            manifest_path = Path(case["manifest"]).resolve(strict=True)
            assert manifest_path.is_relative_to(self.store.resolve()), manifest_path
            manifest = json.loads(manifest_path.read_text())
            artifact = next(item for item in manifest["artifacts"] if item["artifact_id"] == checkpoint["tree_artifact"])
            tree_path = (manifest_path.parent / artifact["path"]).resolve(strict=True)
            assert tree_path.is_relative_to(manifest_path.parent)
            assert self.fingerprint(tree_path) == artifact["sha256"]
            self.tree = json.loads(tree_path.read_text())
            assert self.tree["complete"] and not self.tree["truncated"]
            HELPER.write_json(self.output / (name + "-tree.json"), self.tree)
            self.measure_layout(name)
            image = checkpoint["image"]
            await self.call("artifact.image", {"capture": case["capture_id"], "image": image["image_id"],
                "include_image": True}, live=False, image_path=self.output / (name + ".png"))
            self.measure_pixels(name, self.output / (name + ".png"))
            self.assert_case_layout(name)
            self.cases.append({"case_id": name, "verdict": "pass", "job_id": report["job_id"],
                "coverage": case.get("coverage", {"required": "1"}), "image": name + ".png",
                "capture_id": case["capture_id"], "image_id": image["image_id"],
                "window_frame": image["window_frame"]})
        self.save()

    def record_layout_check(self, case_id: str, check: str, passed: bool, **evidence: object) -> None:
        """Publish the evidence before failing so a broken layout cannot become a passing suite."""
        entry = {"case_id": case_id, "check": check, "passed": bool(passed), **evidence}
        self.layout_checks.append(entry)
        if not self.defer_layout_failures:
            assert passed, entry

    def assert_case_layout(self, case_id: str) -> None:
        failures = [check for check in self.layout_checks if check["case_id"] == case_id and not check["passed"]]
        assert not failures, failures[:3]

    @staticmethod
    def horizontally_contained(inner: dict, outer: dict, tolerance: float = 1.0) -> bool:
        return inner["width"] >= 0 and inner["x"] >= outer["x"] - tolerance and \
            inner["x"] + inner["width"] <= outer["x"] + outer["width"] + tolerance

    def clay_node(self, suffix: str) -> dict:
        matches = [node for node in self.tree.get("clay_nodes", [])
            if node.get("name", "").endswith("/" + suffix)]
        assert len(matches) == 1, (suffix, [node.get("name") for node in matches])
        return matches[0]

    def visible_region(self, node: dict) -> dict:
        """Intersect every ancestor clip, rather than treating offscreen solved text as visible."""
        nodes = self.tree["clay_nodes"]
        region = dict(nodes[0]["bounds"])
        current = node
        while current.get("parent") is not None:
            current = nodes[current["parent"]]
            bounds = current["bounds"]
            axes = current.get("clip_axes", {})
            if axes.get("horizontal"):
                right = min(region["x"] + region["width"], bounds["x"] + bounds["width"])
                region["x"] = max(region["x"], bounds["x"])
                region["width"] = max(0, right - region["x"])
            if axes.get("vertical"):
                bottom = min(region["y"] + region["height"], bounds["y"] + bounds["height"])
                region["y"] = max(region["y"], bounds["y"])
                region["height"] = max(0, bottom - region["y"])
        return region

    def fully_visible(self, node: dict, tolerance: float = .75) -> bool:
        region = self.visible_region(node)
        bounds = node["bounds"]
        return region["width"] > 0 and region["height"] > 0 and \
            self.horizontally_contained(bounds, region, tolerance) and \
            bounds["y"] >= region["y"] - tolerance and \
            bounds["y"] + bounds["height"] <= region["y"] + region["height"] + tolerance

    async def align_scroll_element(self, case_prefix: str, container_suffix: str,
                                   moving_suffix: str, padding: float) -> None:
        """Use real wheel input, then calibrate its movement from the captured solved bounds."""
        gain = None
        for attempt in range(6):
            container = self.clay_node(container_suffix)
            region = self.visible_region(container)
            viewport_top = max(container["bounds"]["y"], region["y"])
            moving = self.clay_node(moving_suffix)
            error = viewport_top + padding - moving["bounds"]["y"]
            if abs(error) < .4:
                return
            amount = max(-1.0, min(1.0, error / 10)) if gain is None else \
                max(-100.0, min(100.0, error / gain))
            previous_top = moving["bounds"]["y"]
            # These anchors land in the container's padding, outside nested scroll surfaces.
            target = {**self.clay(container_suffix), "anchor": {"x": .005, "y": .5}}
            await self.execute(case_prefix + "-adjust-" + str(attempt + 1), [{
                "operation": "scroll", "target": target, "scroll": {"x": 0, "y": amount},
                "frames": "1", "expect": ["layout_completed"]}])
            movement = self.clay_node(moving_suffix)["bounds"]["y"] - previous_top
            assert abs(movement) > .01, ("Wheel did not move requested container", case_prefix,
                amount, error, self.clay_node(container_suffix)["bounds"])
            gain = abs(movement / amount)
        remaining = self.clay_node(moving_suffix)["bounds"]["y"] - \
            (self.clay_node(container_suffix)["bounds"]["y"] + padding)
        assert abs(remaining) < 1, ("Could not align scroll content", case_prefix, remaining)

    def measure_visible_metrics(self, case_id: str, labels: list[str]) -> None:
        nodes = self.tree["clay_nodes"]
        structure = self.clay_node("overview-and-performance/overview-and-performance/structure")
        descendants = nodes[structure["index"]:structure["subtree_end_exclusive"]]
        for label in labels:
            matches = [node for node in descendants if node.get("text") == label]
            self.record_layout_check(case_id, "semantic-metric-present", len(matches) == 1,
                metric=label, observed_count=len(matches))
            if len(matches) != 1:
                continue
            label_node = matches[0]
            row = nodes[label_node["parent"]]
            values = [node for node in nodes[row["index"]:row["subtree_end_exclusive"]]
                if node.get("text_present") and str(node.get("text") or "").strip()
                and node["index"] != label_node["index"]]
            self.record_layout_check(case_id, "semantic-metric-has-value", len(values) == 1,
                metric=label, values=[node.get("text") for node in values])
            for node in [label_node] + values:
                self.record_layout_check(case_id, "semantic-metric-visible", self.fully_visible(node),
                    metric=label, text=node.get("text"), observed=node["bounds"],
                    visible_region=self.visible_region(node))

    async def verify_geometry(self, width: int) -> None:
        await self.configure(window_width=width, inspector_width=320, selection="sample")
        await self.execute("geometry-window-" + str(width))
        self.record_layout_check("geometry-window-" + str(width), "representative-workbench-width",
            abs(self.bounds("workbench-area")["width"] - (width - 620)) < 1,
            expected=width - 620, observed=self.bounds("workbench-area")["width"])
        self.assert_case_layout("geometry-window-" + str(width))
        card_suffix = "selected-content/content/overview-and-performance"
        container_suffix = "overview-and-performance/overview-and-performance/overview-body"
        card = self.clay_node(card_suffix)
        if not self.fully_visible(card):
            await self.align_scroll_element("geometry-overview-" + str(width),
                "selected-content/content", card_suffix, 12)
        assert self.fully_visible(self.clay_node(card_suffix)), "Overview card is outside its viewport"
        await self.align_scroll_element("geometry-structure-" + str(width), container_suffix,
            "overview-and-performance/overview-and-performance/structure", 8)
        await self.execute("geometry-structure-visible-" + str(width))
        body = self.clay_node(container_suffix)
        geometry = self.clay_node("overview-and-performance/overview-and-performance/column[1]")
        padding = max(0, min(8, (body["bounds"]["height"] - geometry["bounds"]["height"]) / 2))
        await self.align_scroll_element("geometry-values-" + str(width), container_suffix,
            "overview-and-performance/overview-and-performance/column[1]", padding)
        await self.execute("geometry-values-visible-" + str(width))

    def measure_layout(self, case_id: str) -> None:
        """Check real inner actions and geometry text, beyond the column clipping boundary."""
        nodes = self.tree.get("clay_nodes", [])
        for trigger in [node for node in nodes if node.get("name", "").endswith("/cpu-reporting-level")]:
            slots = [node for node in nodes[trigger["index"]:trigger["subtree_end_exclusive"]]
                if node.get("name") == trigger["name"] + "/label"]
            self.record_layout_check(case_id, "cpu-recording-label-slot-present", len(slots) == 1,
                observed_count=len(slots), trigger=trigger.get("name"))
            for slot in slots:
                labels = [node for node in nodes[slot["index"]:slot["subtree_end_exclusive"]]
                    if node.get("text_present") and str(node.get("text") or "").strip()]
                self.record_layout_check(case_id, "cpu-recording-selected-label-present", len(labels) == 1,
                    observed_count=len(labels), slot=slot.get("name"))
                for label in labels:
                    observed = label["bounds"]
                    passed = all(self.horizontally_contained(observed, parent["bounds"], .5)
                        and observed["y"] >= parent["bounds"]["y"] - .5
                        and observed["y"] + observed["height"] <= parent["bounds"]["y"] + parent["bounds"]["height"] + .5
                        for parent in [slot, trigger])
                    self.record_layout_check(case_id, "cpu-recording-selected-label-fits", passed,
                        text=label.get("text"), observed=observed, slot=slot["bounds"], trigger=trigger["bounds"])
        for suffix in ["selector-area", "workbench-area", "inspector-area"]:
            matches = [node for node in nodes if node.get("name", "").endswith("/" + suffix)]
            if len(matches) != 1:
                continue
            root = matches[0]
            bounds = root["bounds"]
            for node in nodes[root["index"]:root["subtree_end_exclusive"]]:
                if not node.get("name", "").endswith("/selected-content"):
                    continue
                observed = node["bounds"]
                self.record_layout_check(case_id, suffix + "-content-bounded",
                    self.horizontally_contained(observed, bounds), root=bounds, observed=observed)
        inspectors = [node for node in nodes if node.get("name", "").endswith("/inspector-area")]
        for inspector in inspectors:
            for strip in nodes[inspector["index"]:inspector["subtree_end_exclusive"]]:
                if not strip.get("name", "").endswith("/selected-content/tabs"):
                    continue
                tabs = [node for node in nodes[strip["index"] + 1:strip["subtree_end_exclusive"]]
                    if node.get("parent") == strip["index"] and "/tab[" in node.get("name", "")]
                if len(tabs) != 4:  # Theme inspectors use their own two-tab surface.
                    continue
                self.record_layout_check(case_id, "inspector-tab-strip-bounded",
                    self.horizontally_contained(strip["bounds"], inspector["bounds"], .5),
                    inspector=inspector["bounds"], observed=strip["bounds"])
                for tab in tabs:
                    self.record_layout_check(case_id, "inspector-tab-reachable",
                        tab["bounds"]["width"] >= 40 and
                        self.horizontally_contained(tab["bounds"], inspector["bounds"], .5),
                        element=tab.get("name"), inspector=inspector["bounds"], observed=tab["bounds"])
                    for label in nodes[tab["index"] + 1:tab["subtree_end_exclusive"]]:
                        if not label.get("text_present"):
                            continue
                        self.record_layout_check(case_id, "inspector-tab-label-fits",
                            self.horizontally_contained(label["bounds"], tab["bounds"], .5) and
                            label["bounds"]["height"] <= tab["bounds"]["height"] + .5,
                            text=label.get("text"), tab=tab["bounds"], observed=label["bounds"])
            for action_row in nodes[inspector["index"]:inspector["subtree_end_exclusive"]]:
                if not action_row.get("name", "").endswith("/field-actions"):
                    continue
                controls = [node for node in nodes[action_row["index"] + 1:action_row["subtree_end_exclusive"]]
                    if node.get("parent") == action_row["index"]]
                self.record_layout_check(case_id, "inspector-four-actions-present", len(controls) == 4,
                    action_row=action_row["name"], observed_count=len(controls))
                for control in controls:
                    observed = control["bounds"]
                    passed = observed["width"] >= 20 and observed["height"] >= 20 and \
                        self.horizontally_contained(observed, action_row["bounds"], .5) and \
                        self.horizontally_contained(observed, inspector["bounds"], .5)
                    self.record_layout_check(case_id, "inspector-action-reachable", passed,
                        element=control.get("name"), action_row=action_row["bounds"],
                        inspector=inspector["bounds"], observed=observed)
        structures = [node for node in nodes if node.get("name", "").endswith("/structure")
            and "/overview-and-performance/" in node.get("name", "")]
        if not structures:
            structures = [node for node in nodes if node.get("name", "").endswith("/structure")
                and any("TREE STRUCTURE" in str(child.get("text") or "")
                    for child in nodes[node["index"]:node["subtree_end_exclusive"]])]
        for structure in structures:
            text_nodes = [node for node in nodes[structure["index"]:structure["subtree_end_exclusive"]]
                if node.get("text_present") and str(node.get("text") or "").strip()]
            self.record_layout_check(case_id, "structure-metrics-have-text", len(text_nodes) >= 10,
                observed_count=len(text_nodes), structure=structure["bounds"])
            for node in text_nodes:
                # Identity/source values may deliberately elide elsewhere; these actual solved
                # structure/geometry rows must fit their own content width and height.
                bounds = node["bounds"]
                parent_index = node.get("parent")
                parent = nodes[parent_index] if parent_index is not None else structure
                passed = self.horizontally_contained(bounds, structure["bounds"], .75) and \
                    self.horizontally_contained(bounds, parent["bounds"], .75) and \
                    bounds["y"] >= structure["bounds"]["y"] - .75 and \
                    bounds["y"] + bounds["height"] <= structure["bounds"]["y"] + structure["bounds"]["height"] + .75
                self.record_layout_check(case_id, "structure-metric-text-fits", passed,
                    text=node.get("text"), wrapped_line_count=node.get("wrapped_line_count"),
                    structure=structure["bounds"], parent=parent["bounds"], observed=bounds)
        if case_id.startswith("geometry-structure-visible-"):
            self.measure_visible_metrics(case_id, ["Flow children", "Clay children", "Depth", "Z / paint", "Flags"])
        if case_id.startswith("geometry-values-visible-"):
            self.measure_visible_metrics(case_id, ["Position", "Dimensions", "Layout", "Padding", "Sizing", "Clip / overflow"])
        if case_id.startswith(("inspector-nested-width-240", "theme-box-width-240")):
            inputs = [nodes[node["clay_root"]] for node in self.tree.get("flow_nodes", [])
                if "/inspector-area/" in node.get("debug_name", "") and node.get("clay_root") is not None
                and any(kind in node.get("definition_type", "")
                    for kind in ["FSEL::DragValue", "FSEL::NumberInput"])]
            self.record_layout_check(case_id, "expanded-inspector-has-numeric-controls", bool(inputs),
                observed_count=len(inputs))
            for control in inputs:
                self.record_layout_check(case_id, "expanded-inspector-control-bounded",
                    any(self.horizontally_contained(control["bounds"], inspector["bounds"], .5)
                        for inspector in inspectors), element=control.get("name"), observed=control["bounds"])
            self.record_layout_check(case_id, "expanded-inspector-control-visible",
                any(self.fully_visible(control) for control in inputs), observed_count=len(inputs))

    def measure_pixels(self, case_id: str, image_path: Path) -> None:
        """Read screenshot pixels for adjacent separator surfaces and stable chrome controls."""
        from PIL import Image
        import hashlib
        if not image_path.is_file():
            return
        nodes = self.tree.get("clay_nodes", [])
        if not nodes:
            return
        window_bounds = nodes[0]["bounds"]
        with Image.open(image_path) as original:
            rendered = original.convert("RGB")
            scale_x = rendered.width / max(window_bounds["width"], 1)
            scale_y = rendered.height / max(window_bounds["height"], 1)
            def pixel_at(x: float, y: float) -> tuple[int, int, int]:
                point = (min(rendered.width - 1, max(0, int((x - window_bounds["x"]) * scale_x))),
                    min(rendered.height - 1, max(0, int((y - window_bounds["y"]) * scale_y))))
                return rendered.getpixel(point)
            panel, keel = (13, 34, 48), (7, 26, 34)
            for boundary, leading, trailing in [("selector-splitter", panel, keel),
                                               ("inspector-splitter", keel, panel)]:
                for side, expected in [("leading-surface", leading), ("trailing-surface", trailing)]:
                    matches = [node for node in nodes if node.get("name", "").endswith("/" + boundary + "/" + side)]
                    for surface in matches:
                        bounds = surface["bounds"]
                        if bounds["width"] < 1 or bounds["height"] < 1:
                            continue
                        samples = [pixel_at(bounds["x"] + bounds["width"] * .5,
                            bounds["y"] + bounds["height"] * fraction) for fraction in (.15, .5, .85)]
                        matching = sum(max(abs(actual[channel] - expected[channel]) for channel in range(3)) <= 2
                            for actual in samples)
                        self.record_layout_check(case_id, boundary + "-" + side + "-pixels", matching >= 2,
                            expected=expected, samples=samples, bounds=bounds)
            headers = [node for node in nodes if node.get("name", "").endswith("/permanent-header")]
            if not headers:
                return
            header = headers[0]
            chrome = {}
            for node in nodes[header["index"]:header["subtree_end_exclusive"]]:
                suffix = node.get("name", "").rsplit("/", 1)[-1]
                if suffix not in {"undo", "redo", "history", "configurations"}:
                    continue
                bounds = node["bounds"]
                left = max(0, int(bounds["x"] * scale_x))
                top = max(0, int(bounds["y"] * scale_y))
                right = min(rendered.width, int((bounds["x"] + bounds["width"]) * scale_x))
                bottom = min(rendered.height, int((bounds["y"] + bounds["height"]) * scale_y))
                if right > left and bottom > top:
                    crop = rendered.crop((left, top, right, bottom))
                    chrome[suffix] = {"size": crop.size, "pixels": crop.tobytes()}
            hostile = "hostile-application-theme" in case_id or "application-definition-isolation" in case_id
            restored = "restored-application-theme" in case_id
            reference = getattr(self, "_chrome_neutral_pixels", {})
            if hostile or restored:
                self.record_layout_check(case_id, "chrome-action-pixels-observed", bool(chrome) and bool(reference),
                    controls=sorted(chrome), reference_controls=sorted(reference))
                for name in sorted(chrome.keys() & reference.keys()):
                    current, baseline = chrome[name], reference[name]
                    same_size = current["size"] == baseline["size"]
                    changed_channels = sum(actual != expected for actual, expected in
                        zip(current["pixels"], baseline["pixels"])) if same_size else len(current["pixels"])
                    changed_fraction = changed_channels / max(1, len(current["pixels"]))
                    self.record_layout_check(case_id, "chrome-theme-isolated-" + name,
                        same_size and changed_fraction <= .002,
                        size=current["size"], reference_size=baseline["size"], changed_channel_fraction=changed_fraction,
                        sha256=hashlib.sha256(current["pixels"]).hexdigest(),
                        reference_sha256=hashlib.sha256(baseline["pixels"]).hexdigest())
            else:
                self._chrome_neutral_pixels = chrome

    async def catalogue(self) -> None:
        if self.fixture:
            # Wait through real completed layouts before requesting schema-backed setup.
            await self.execute("fixture-warmup", visual=False)
            HELPER.write_json(self.output / "fixture-actions.json", await self.call("action.list", {"limit": 100}))
            HELPER.write_json(self.output / "fixture-status.json", await self.call("status"))
            await self.configure(selection="sample", active_tab="inspect", inspector_width=320,
                window_width=1100, clear_override=True, theme="default")
            await self.execute("fixture-reference")
            if self.stage != "baseline":
                await self.verify_fixture()
            return
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
        HELPER.write_json(self.output / "inspect-flow-identities.json", self.tree.get("flow_nodes", []))
        if self.stage == "baseline":
            return
        await self.verify_notes()

    async def verify_notes(self) -> None:
        """Navigate each primary panel through observed pointer routing."""
        await self.execute_batch([("panel-" + tab, [HELPER.click(self.flow("tab-" + tab))])
            for tab in ["performance", "memory", "diagnostics", "changes", "catalogue", "inspect"]])

    async def drag_column_to_width(self, splitter_name: str, area_name: str,
                                   desired_width: float, case_id: str) -> None:
        gain = 1.0
        for attempt in range(3):
            before = self.bounds(area_name)
            error = desired_width - before["width"]
            if abs(error) < .5:
                return
            target = self.flow(splitter_name)
            geometry = await self.call("interaction.targets", {"window_id": self.window_id, "target": target})
            point = geometry["window_point"]
            displacement = error / gain * (1 if area_name == "selector-area" else -1)
            name = case_id if attempt == 0 else case_id + "-adjust-" + str(attempt)
            report = await self.execute(name, [{"operation": "drag", "target": target,
                "destination": {"point": {"x": point["x"] + displacement, "y": point["y"]}},
                "frames": str(max(12, int(abs(displacement) / 4) + 3)), "deadline_ns": "10000000000",
                # Moving handles may release outside their previous hit rectangle. The mechanical
                # up transition and final neutral input are verified from the sealed input trace.
                "expect": ["target_pressed", "layout_completed"]}])
            after = self.bounds(area_name)
            manifest_path = Path(report["cases"][0]["manifest"]).resolve(strict=True)
            manifest = json.loads(manifest_path.read_text())
            artifact = next(item for item in manifest["artifacts"] if item["artifact_id"] == "interaction_inputs")
            input_path = (manifest_path.parent / artifact["path"]).resolve(strict=True)
            assert input_path.is_relative_to(manifest_path.parent)
            assert self.fingerprint(input_path) == artifact["sha256"]
            final_input = json.loads(input_path.read_text().splitlines()[-1])
            neutral = final_input["effective_input"]
            self.record_layout_check(name, "splitter-drag-input-neutral",
                final_input["cleanup"] and not neutral["buttons_down"] and not neutral["keys_down"]
                and neutral["scroll"] == {"x": 0, "y": 0}, observed=neutral)
            settled = await self.call("interaction.targets", {"window_id": self.window_id, "target": target})
            self.record_layout_check(name, "splitter-stops-after-release",
                abs(settled["bounds"]["x"] - self.clay_node(splitter_name)["bounds"]["x"]) < .5,
                captured=self.clay_node(splitter_name)["bounds"], settled=settled["bounds"])
            movement = after["width"] - before["width"]
            self.record_layout_check(name, "splitter-width-moves-toward-target",
                movement * error > 0, before=before, after=after, desired_width=desired_width)
            self.assert_case_layout(name)
            gain = abs(movement / displacement)
        assert abs(self.bounds(area_name)["width"] - desired_width) < 1, (case_id, desired_width, self.bounds(area_name))

    async def verify_splitters(self) -> None:
        await self.configure(window_width=1420, inspector_width=320, selection="sample")
        await self.execute("splitter-drag-reference")
        for splitter_name, area_name in [
                ("selector-splitter", "selector-area"),
                ("inspector-splitter", "inspector-area")]:
            before = self.bounds(area_name)
            await self.drag_column_to_width(splitter_name, area_name, before["width"] + 40,
                splitter_name + "-dragged")
            after = self.bounds(area_name)
            self.record_layout_check(splitter_name + "-dragged", "splitter-drag-changes-column-width",
                after["width"] >= before["width"] + 20, before=before, after=after)
            self.assert_case_layout(splitter_name + "-dragged")
            await self.drag_column_to_width(splitter_name, area_name, before["width"],
                splitter_name + "-restored")
            restored = self.bounds(area_name)
            self.record_layout_check(splitter_name + "-restored", "splitter-drag-restores-column-width",
                abs(restored["width"] - before["width"]) < 2, before=before, after=restored)
            self.assert_case_layout(splitter_name + "-restored")
        await self.drag_column_to_width("selector-splitter", "selector-area", 280,
            "geometry-selector-normalized")
        self.record_layout_check("geometry-selector-normalized", "selector-width-normalized-for-geometry",
            abs(self.bounds("selector-area")["width"] - 280) < 1,
            expected=280, observed=self.bounds("selector-area")["width"])
        self.save()

    async def verify_fixture(self) -> None:
        assert not any("permanent-header/window-selector" in node.get("debug_name", "")
            for node in self.tree.get("flow_nodes", [])), "Window targeting remains in permanent header"
        await self.execute("inspect-window-target-options", [HELPER.click(self.flow("window-target"))])
        secondary_window = str(self.fixture_state["secondary_window_id"])
        await self.execute("inspect-window-target-secondary", [HELPER.click(self.clay("option[" + secondary_window + "]"))])
        selected = await self.configure(inspector_width=320)
        assert str(selected["selected_window_id"]) == secondary_window and str(selected["selected_instance_id"]) == "0", selected
        await self.execute("inspect-window-target-cleared")
        for width in [240, 320, 560]:
            await self.configure(selection="sample", inspector_width=width, inspector_tab="parameters")
            await self.execute("inspector-width-" + str(width))
            assert "Inspector" in self.texts()
        await self.configure(selection="sample", inspector_width=240, inspector_tab="parameters")
        await self.execute("inspector-nested-reference")
        await self.execute("inspector-nested-width-240", [HELPER.click(self.card_header("nested"))])
        assert all(label in self.texts() for label in ["color", "radius", "padding"]), self.texts()
        await self.execute("inspector-nested-width-240-scrolled", [{"operation": "scroll",
            "target": {**self.clay("selected-content/fields"), "anchor": {"x": .005, "y": .5}},
            "scroll": {"x": 0, "y": -1}, "frames": "1", "expect": ["layout_completed"]}])
        await self.execute("inspector-nested-restored", [{"operation": "scroll",
            "target": {**self.clay("selected-content/fields"), "anchor": {"x": .005, "y": .5}},
            "scroll": {"x": 0, "y": 100}, "frames": "1", "expect": ["layout_completed"]},
            {"operation": "wait", "frames": "2", "expect": ["layout_completed"]},
            HELPER.click(self.card_header("nested"))])
        for tab in ["state", "resources", "changes", "parameters"]:
            await self.configure(inspector_tab=tab)
            await self.execute("inspector-role-" + tab)
            if tab in {"state", "resources"}:
                assert "Value not captured" not in self.texts(), self.texts()
                assert ("builds" in self.texts() and "On" in self.texts()) if tab == "state" \
                    else ("icon" in self.texts() and "Anonymous texture · " in self.texts()), self.texts()
        for width in [240, 320, 560]:
            await self.configure(selection="theme", inspector_width=width, window_width=1420)
            await self.execute("theme-inspector-width-" + str(width))
            assert "Tokens & Styles" in self.texts(), self.texts()
        await self.configure(selection="theme", inspector_width=240)
        await self.execute("theme-box-reference")
        await self.execute("theme-box-width-240", [HELPER.click(self.card_header("boxTheme"))])
        await self.execute("theme-box-width-240-scrolled", [{"operation": "scroll",
            "target": {**self.clay("inspector-area/selected-content/themes"), "anchor": {"x": .005, "y": .5}},
            "scroll": {"x": 0, "y": -1}, "frames": "1", "expect": ["layout_completed"]}])
        await self.execute("theme-box-restored", [{"operation": "scroll",
            "target": {**self.clay("inspector-area/selected-content/themes"), "anchor": {"x": .005, "y": .5}},
            "scroll": {"x": 0, "y": 100}, "frames": "1", "expect": ["layout_completed"]},
            {"operation": "wait", "frames": "2", "expect": ["layout_completed"]},
            HELPER.click(self.card_header("boxTheme"))])
        await self.verify_splitters()
        for width in [980, 1100, 1420]:
            await self.verify_geometry(width)
        await self.configure(window_width=1420, authored_value=10, live_value=30, inspector_tab="changes")
        await self.execute("authored-10-effective-30")
        assert "Authored" in self.texts() and "30" in self.texts(), self.texts()
        await self.configure(authored_value=11)
        await self.execute("authored-11-effective-30")
        assert "11" in self.texts() and "30" in self.texts(), self.texts()
        await verify_header_pressed(self)
        await self.execute("header-undo-hover", [{"operation": "hover", "target": self.flow("permanent-header/undo"),
            "frames": "3", "expect": ["target_hovered"]}])
        await self.execute("header-undo-activate", [HELPER.click(self.flow("permanent-header/undo"))])
        assert "This element has no changes" in self.texts(), self.texts()
        history = await self.configure(inspector_width=320)
        assert int(history["redo_count"]) > 0, history
        await self.execute("header-redo-hover", [{"operation": "hover", "target": self.flow("permanent-header/redo"),
            "frames": "3", "expect": ["target_hovered"]}])
        await self.execute("header-redo-activate", [HELPER.click(self.flow("permanent-header/redo"))])
        assert "winning_value" in self.texts() and "30" in self.texts(), self.texts()
        history = await self.configure(inspector_width=320)
        assert int(history["undo_count"]) > 0, history
        await self.configure(clear_override=True, optional_present=False, inspector_tab="parameters", selection="sample")
        await self.execute("optional-absent-quick-value")
        assert "Unset / inherits" in self.texts(), self.texts()
        await self.configure(optional_present=True, authored_value=11)
        await self.configure(clear_override=True, inspector_tab="parameters", selection="sample")
        await self.execute("font-parameter-choices")
        await self.execute("font-picker-expanded", [HELPER.click(self.card_header("font_family"))])
        await self.execute("font-picker-options", [HELPER.click(self.flow("font-choice/combo"))])
        assert "Inter" in self.texts() or "Default" in self.texts(), self.texts()
        await self.execute("font-picker-select", [HELPER.click(self.clay("option[0]"))])
        await self.configure(selection="clay")
        await self.execute("clay-inspector-scroll")
        await self.configure(selection="sample", theme="hostile")
        await self.execute("hostile-application-theme")
        await self.configure(theme="default")
        await self.execute("restored-application-theme")
        await self.configure(icons_enabled=False)
        await self.execute("definition-isolation-text-header-reference")
        await self.configure(button_definition_font_size=32)
        await self.execute("application-definition-isolation")
        self.window_id = str(self.fixture_state["main_window_id"])
        await self.execute("application-definition-font-32")
        await self.configure(button_definition_font_size=0, icons_enabled=True)
        await self.execute("application-definition-restored-interface")
        await self.verify_notes()
        await self.configure(active_tab="performance")
        await self.execute("cpu-recording-reference")
        await self.execute("cpu-recording-selector", [HELPER.click(self.flow("cpu-reporting-level"))])
        assert "Build: Balanced" in self.texts() and "CPU: Deep" not in self.texts(), self.texts()
        frame_only_option = self.clay("option[0]")
        await self.execute("cpu-recording-balanced", [HELPER.click(self.clay("option[2]"))])
        policy = await self.configure(inspector_width=320)
        assert int(policy["recording_cpu_level"]) == 2, policy
        await self.execute_batch([
            ("cpu-recording-open-frame-only", [HELPER.click(self.flow("cpu-reporting-level"))]),
            ("cpu-recording-frame-only", [HELPER.click(frame_only_option)])])
        policy = await self.configure(active_tab="inspect", selection="sample")
        assert int(policy["recording_cpu_level"]) == 0, policy
        for height in [20, 24, 28]:
            await self.configure(combo_height=height, requested_icon_size=80, icons_enabled=True)
            self.window_id = str(self.fixture_state["main_window_id"])
            await self.execute("combo-oversized-icons-" + str(height))
            self.check_combo_geometry("combo-oversized-icons-" + str(height))
            await self.execute("combo-popup-icons-" + str(height), [HELPER.click({
                "flow_instance_id": str(self.fixture_state["combo_instance_id"])})])
            self.check_combo_geometry("combo-popup-icons-" + str(height))
            await self.execute("combo-popup-select-" + str(height), [HELPER.click(self.clay("option[2]"))])
            observed = await self.configure(combo_height=height)
            assert int(observed["combo_selection"]) == 2, observed
        await self.configure(combo_height=24, icons_enabled=False)
        await self.execute("header-missing-icon-fallback")
        self.window_id = str(self.fixture_state["main_window_id"])
        await self.execute("combo-text-disclosure-fallback")
        self.check_combo_geometry("combo-text-disclosure-fallback")

    def check_combo_geometry(self, case_id: str) -> None:
        nodes = self.tree.get("clay_nodes", [])
        roots = [nodes[node["clay_root"]] for node in self.tree.get("flow_nodes", [])
            if node.get("instance_id") == str(self.fixture_state["combo_instance_id"])
            and node.get("clay_root") is not None]
        assert roots, "The real fixture combo is missing"
        for root in roots:
            bounds = root["bounds"]
            icons = [node for node in nodes[root["index"]:root["subtree_end_exclusive"]]
                if node.get("name", "").endswith(("/selected-icon", "/disclosure-icon", "/disclosure-slot"))]
            assert icons, "No disclosure geometry was captured"
            for icon in icons:
                observed = icon["bounds"]
                passed = observed["x"] >= bounds["x"] - .5 and observed["y"] >= bounds["y"] - .5 \
                    and observed["x"] + observed["width"] <= bounds["x"] + bounds["width"] + .5 \
                    and observed["y"] + observed["height"] <= bounds["y"] + bounds["height"] + .5
                self.layout_checks.append({"case_id": case_id, "check": "combo-icon-contained", "passed": passed,
                    "root": bounds, "observed": observed})
                assert passed, (case_id, bounds, observed)
            if case_id.startswith("combo-popup-icons-"):
                prefix = root.get("name", "") + "/"
                option_icons = [node for node in nodes if node.get("name", "").startswith(prefix)
                    and "/option-icon[" in node.get("name", "")]
                self.record_layout_check(case_id, "combo-popup-has-option-icons", bool(option_icons),
                    observed_count=len(option_icons), combo=root.get("name"))
                for icon in option_icons:
                    parent = nodes[icon["parent"]]
                    observed = icon["bounds"]
                    bounds = parent["bounds"]
                    passed = parent.get("name", "").startswith(prefix) and "/option[" in parent.get("name", "") \
                        and self.horizontally_contained(observed, bounds, .5) \
                        and observed["y"] >= bounds["y"] - .5 \
                        and observed["y"] + observed["height"] <= bounds["y"] + bounds["height"] + .5
                    self.record_layout_check(case_id, "combo-popup-option-icon-contained", passed,
                        element=icon.get("name"), option=parent.get("name"), root=bounds, observed=observed)
                self.assert_case_layout(case_id)
        self.save()

    def card_header(self, field_name: str) -> dict:
        nodes = self.tree.get("clay_nodes", [])
        candidates = []
        for node in self.tree.get("flow_nodes", []):
            name = node.get("debug_name", "")
            if not name.endswith("/header") or not any(part in name for part in ["/editor-card[", "/themes/field-"]):
                continue
            index = node.get("clay_root")
            if index is None:
                continue
            labels = [str(child.get("text") or "").lower().replace("_", " ")
                for child in nodes[index:nodes[index]["subtree_end_exclusive"]]]
            if field_name.lower().replace("_", " ") in labels:
                candidates.append(node)
        assert len(candidates) == 1, (field_name, candidates)
        return {"flow_instance_id": candidates[0]["instance_id"]}

    def save(self) -> None:
        unchanged = self.fingerprint(self.original_changes) == self.original_digest
        HELPER.write_json(self.output / "evidence.json", {
            "schema": "FlowUi.DevInterfaceNotes.Verification", "run_id": self.run_id,
            "mcp_calls": len(self.records), "cases": self.cases,
            "required_checks": sum(int(case["coverage"]["required"]) for case in self.cases),
            "failures": self.failures, "layout_checks": self.layout_checks,
            "user_changes_unchanged": unchanged,
            "bootstrap": "Configurable native three-window fixture; all setup and input use MCP" if self.fixture else
                "Native Ctrl+Shift+D on the owned demo PID; subsequent interactions use MCP",
            "passed": not self.failures and unchanged and bool(self.cases)
                and all(check["passed"] for check in self.layout_checks)})


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--cli", type=Path, required=True)
    parser.add_argument("--demo", type=Path, required=True)
    parser.add_argument("--workspace", type=Path, default=REPOSITORY)
    parser.add_argument("--output-directory", type=Path,
        default=REPOSITORY / ".flowui/agentic/verification/dev-interface-notes")
    parser.add_argument("--stage", choices=["baseline", "complete"], default="complete")
    parser.add_argument("--fixture", action="store_true", help="Use the configurable notes fixture")
    return asyncio.run(DevInterfaceNotesVerification(parser.parse_args()).run())


if __name__ == "__main__":
    raise SystemExit(main())
