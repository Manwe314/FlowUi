#!/usr/bin/env python3
"""Check major/minor timeline pixels during consecutive MCP hover input frames."""
from __future__ import annotations

import argparse
import asyncio
import hashlib
import json
import math
from pathlib import Path
import time
import uuid

from DevInterfaceProbeMcpTests import ProbeVerification
from DevInterfaceNotesMcpTests import HELPER, REPOSITORY


class TimelineHoverVerification(ProbeVerification):
    def __init__(self, arguments: argparse.Namespace):
        super().__init__(arguments)
        self.require_minor = False
        self.require_timeline = False
        self.hover_frames: list[dict] = []
        self.baseline_pixels: dict[str, int] = {}

    def measure_layout(self, case_id: str) -> None:
        if not self.require_timeline:
            return
        for name in ("macro", "minor"):
            nodes = [node for node in self.tree.get("clay_nodes", [])
                if node.get("name", "").endswith("/" + name + "/body")]
            if name == "minor" and not self.require_minor:
                continue
            self.record_layout_check(case_id, name + "-body-present", len(nodes) == 1
                and nodes[0]["bounds"]["width"] > 150 and nodes[0]["bounds"]["height"] > 40,
                bounds=[node["bounds"] for node in nodes])

    def timeline_pixels(self, image_path: Path, name: str) -> dict:
        from PIL import Image
        body = self.clay_node(name + "/body")["bounds"]
        clip = self.clay_node(name + "/plot-scroll")["bounds"]
        root = self.tree["clay_nodes"][0]["bounds"]
        with Image.open(image_path) as original:
            image = original.convert("RGB")
            scale_x, scale_y = image.width / root["width"], image.height / root["height"]
            left = max(body["x"], clip["x"]) + min(150, body["width"] * .25) + 3
            top = max(body["y"], clip["y"]) + (40 if name == "minor" else 20) + 3
            right = min(body["x"] + body["width"], clip["x"] + clip["width"]) - 3
            bottom = min(body["y"] + body["height"], clip["y"] + clip["height"]) - 3
            box = (math.ceil(left * scale_x), math.ceil(top * scale_y),
                math.floor(right * scale_x), math.floor(bottom * scale_y))
            assert box[2] > box[0] and box[3] > box[1], (name, box)
            crop = image.crop(box)
            pixels = list(crop.getdata())
            colored = sum(max(pixel) - min(pixel) >= 28 and max(pixel) >= 50
                for pixel in pixels)
            return {"colored_pixels": colored, "pixel_count": len(pixels), "crop": box,
                "sha256": hashlib.sha256(crop.tobytes()).hexdigest()}

    def measure_pixels(self, case_id: str, image_path: Path) -> None:
        for name in ("macro", "minor"):
            if name == "minor" and not self.require_minor:
                continue
            if not any(node.get("name", "").endswith("/" + name + "/body")
                    for node in self.tree.get("clay_nodes", [])):
                continue
            pixels = self.timeline_pixels(image_path, name)
            minimum = max(64, self.baseline_pixels.get(name, 0) // 2)
            self.record_layout_check(case_id, name + "-colored-samples-retained",
                pixels["colored_pixels"] >= minimum, minimum_colored_pixels=minimum, **pixels)

    def sample_point(self, image_path: Path) -> dict:
        """Pick the widest colored sample in the first CPU lane from real rendered pixels."""
        from PIL import Image
        body = self.clay_node("macro/body")["bounds"]
        root = self.tree["clay_nodes"][0]["bounds"]
        with Image.open(image_path) as original:
            image = original.convert("RGB")
            scale_x, scale_y = image.width / root["width"], image.height / root["height"]
            left = math.ceil((body["x"] + min(150, body["width"] * .25) + 4) * scale_x)
            right = math.floor((body["x"] + body["width"] - 4) * scale_x)
            vertical = int((body["y"] + 36) * scale_y)
            runs = []
            start = None
            for horizontal in range(left, right + 1):
                pixel = image.getpixel((min(horizontal, image.width - 1), vertical))
                colored = horizontal < right and max(pixel) - min(pixel) >= 28 and max(pixel) >= 50
                if colored and start is None:
                    start = horizontal
                if not colored and start is not None:
                    runs.append((start, horizontal))
                    start = None
            assert runs, "No real CPU sample pixels available for minor navigation"
            start, end = max(runs, key=lambda interval: interval[1] - interval[0])
            return {"point": {"x": (start + end) * .5 / scale_x, "y": vertical / scale_y}}

    @staticmethod
    def frame_key(frame: dict) -> tuple[str, ...]:
        return tuple(str(frame[key]) for key in ("run_id", "window_id", "window_frame", "app_tick"))

    async def hover_capture(self, name: str, target: dict) -> None:
        away = {"point": {"x": 20, "y": 20}}
        plan = {"schema": "FlowUi.Agentic.InteractionPlan", "schema_version": "1.0",
            "mode": "common_controls", "window_id": self.window_id,
            "content": {"persistence": "retain_text", "record_observations": True},
            "limits": {"maximum_observations": "128"}, "steps": [
                {"step_id": "before", "operation": "pointer.move", "target": away, "frames": "1"},
                {"step_id": "enter", "operation": "pointer.move", "target": target, "frames": "1"},
                {"step_id": "hover", "operation": "hover", "target": target, "frames": "2",
                    "expect": ["target_hovered", "frame_presented"]},
                {"step_id": "leave", "operation": "pointer.move", "target": away, "frames": "1"},
                {"step_id": "after", "operation": "wait", "frames": "2"}]}
        capture = {"profile": "visual_inspect", "window_ids": [self.window_id],
            "duration_ns": "30000000000", "maximum_expected_tick_cadence": 60,
            "sections": {"performance": "omit", "runtime_diagnostics": "omit", "memory": "omit",
                "visual": "required"}, "visual_sources": {"window": True, "viewports": False},
            "content": {"include_tree_text": True, "include_interaction_labels": True,
                "include_interaction_outcomes": True}, "limits": {"maximum_images": "8",
                "maximum_image_bytes": "33554432", "maximum_snapshot_bytes": "134217728"}}
        admitted = await self.call("interaction.start", {"plan": plan, "capture": capture,
            "idempotency_key": name + "-" + uuid.uuid4().hex})
        deadline = time.monotonic() + 45
        while time.monotonic() < deadline:
            status = await self.call("job.status", {"job_id": admitted["job_id"]})
            if status["phase"] in {"sealed", "failed", "canceled"}:
                assert status["phase"] == "sealed", status
                break
            await asyncio.sleep(.05)
        else:
            raise TimeoutError("Timeline hover capture did not seal")
        outcome = await self.call("interaction.query", {"job_id": admitted["job_id"]})
        assert outcome["execution_succeeded"] and outcome["evidence_complete"], outcome
        assert outcome["assertion_verdict"] == "passed", outcome
        inputs = await self.call("interaction.query", {"job_id": admitted["job_id"],
            "kind": "inputs", "limit": 128, "max_bytes": 524288})
        observations = await self.call("interaction.query", {"job_id": admitted["job_id"],
            "kind": "observations", "limit": 128, "max_bytes": 524288})
        frames = await self.call("query.visual", {"capture": admitted["capture_id"],
            "kind": "frames", "limit": 100, "max_bytes": 262144}, live=False)
        manifest_path = Path(status["manifest_path"]).resolve(strict=True)
        assert manifest_path.is_relative_to(self.store.resolve()), manifest_path
        manifest = json.loads(manifest_path.read_text())
        artifacts = {entry["artifact_id"]: entry for entry in manifest["artifacts"]}
        input_frames = {self.frame_key(entry["frame"]): entry for entry in inputs["records"]
            if entry.get("frame") and not entry["baseline"] and not entry["cleanup"]}
        observed = {self.frame_key(entry["frame"]): entry for entry in observations["records"]
            if entry.get("frame") and entry.get("layout_completed") and entry.get("presented")}
        joined = []
        joined_offsets = []
        for image in frames["records"]:
            key = self.frame_key(image)
            if key not in input_frames or key not in observed:
                continue
            tree_artifact = artifacts[image["tree"]["artifact_id"]]
            tree_path = (manifest_path.parent / tree_artifact["path"]).resolve(strict=True)
            assert tree_path.is_relative_to(manifest_path.parent)
            assert self.fingerprint(tree_path) == tree_artifact["sha256"], tree_artifact
            self.tree = json.loads(tree_path.read_text())
            assert self.tree["complete"] and not self.tree["truncated"]
            assert self.frame_key(self.tree) == key
            case_id = name + "-frame-" + image["window_frame"]
            image_path = self.output / (case_id + ".png")
            await self.call("artifact.image", {"capture": admitted["capture_id"],
                "image": image["image_id"], "include_image": True}, live=False, image_path=image_path)
            HELPER.write_json(self.output / (case_id + "-tree.json"), self.tree)
            self.measure_layout(case_id)
            self.measure_pixels(case_id, image_path)
            self.assert_case_layout(case_id)
            joined.append(int(image["window_frame"]))
            joined_offsets.append(int(input_frames[key]["offset"]))
            self.hover_frames.append({"control": name, "window_frame": image["window_frame"],
                "image": image_path.name, "input": input_frames[key], "observation": observed[key]})
        self.record_layout_check(name, "consecutive-input-image-observation-frames", len(joined) >= 6
            and all(second == first + 1 for first, second in zip(sorted(joined), sorted(joined)[1:])),
            window_frames=joined)
        self.record_layout_check(name, "hover-entry-and-leave-images-retained",
            1 in joined_offsets and 4 in joined_offsets,
            captured_input_offsets=joined_offsets, enter_offset=1, leave_offset=4)
        self.cases.append({"case_id": name, "verdict": "pass", "job_id": admitted["job_id"],
            "coverage": {"required": str(len(joined) * 2 + 2)}, "capture_id": admitted["capture_id"]})
        self.save()
        self.assert_case_layout(name)
        print(json.dumps({"case_id": name, "joined_frames": len(joined),
            "window_frames": joined}), flush=True)

    async def catalogue(self) -> None:
        await self.execute("timeline-warmup", visual=False)
        state = await self.configure(active_tab="performance", selection="sample",
            inspector_width=320, window_width=1420)
        await self.execute("timeline-controls")
        await self.choose_level(int(state["compiled_cpu_level"]), "timeline-cpu-policy")
        await self.configure(capture_timing=True, active_tab="performance")
        self.require_timeline = True
        await self.execute("timeline-recorded")
        await self.execute("timeline-fit", [HELPER.click(self.flow("macro/control[1]"))])
        point = self.sample_point(self.output / "timeline-fit.png")
        await self.execute("timeline-open-minor", [{"operation": "click", "target": point,
            "expect": ["layout_completed", "frame_presented"]}])
        self.require_minor = True
        await self.execute("timeline-ready")
        for name in ("macro", "minor"):
            self.baseline_pixels[name] = self.timeline_pixels(
                self.output / "timeline-ready.png", name)["colored_pixels"]
        for name, suffix in [("major-fit-history", "macro/control[1]"),
                ("major-fit-selected", "macro/control[2]"), ("minimap-fit", "header/control[32]"),
                ("minor-toolbar", "minor/control[3]")]:
            await self.hover_capture(name, self.flow(suffix))
        self.contact_sheet()
        self.save()

    def contact_sheet(self) -> None:
        from PIL import Image, ImageDraw
        selected = [entry for index, entry in enumerate(self.hover_frames) if index % 2 == 0]
        columns, width, height = 4, 355, 225
        sheet = Image.new("RGB", (columns * width, math.ceil(len(selected) / columns) * height), "#071A22")
        drawing = ImageDraw.Draw(sheet)
        for index, entry in enumerate(selected):
            horizontal, vertical = (index % columns) * width, (index // columns) * height
            with Image.open(self.output / entry["image"]) as source:
                source.thumbnail((width, height - 20))
                sheet.paste(source, (horizontal, vertical + 20))
            drawing.text((horizontal + 4, vertical + 4),
                entry["control"] + " #" + entry["window_frame"], fill="white")
        sheet.save(self.output / "timeline-hover-contact-sheet.png")

    def save(self) -> None:
        super().save()
        path = self.output / "evidence.json"
        evidence = json.loads(path.read_text())
        evidence.update({"schema": "FlowUi.TimelineHover.Verification",
            "hover_frames": self.hover_frames, "baseline_colored_pixels": self.baseline_pixels})
        HELPER.write_json(path, evidence)


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--cli", type=Path, required=True)
    parser.add_argument("--demo", type=Path, required=True)
    parser.add_argument("--workspace", type=Path, default=REPOSITORY)
    parser.add_argument("--output-directory", type=Path,
        default=REPOSITORY / ".flowui/agentic/verification/timeline-hover")
    arguments = parser.parse_args()
    arguments.fixture, arguments.stage = True, "complete"
    return asyncio.run(TimelineHoverVerification(arguments).run())


if __name__ == "__main__":
    raise SystemExit(main())
