#!/usr/bin/env python3
"""Verify raw RGB font atlas inspection through the real FlowUi MCP adapter."""
from __future__ import annotations

import argparse
import asyncio
import hashlib
import json
import math
from pathlib import Path
import sys

sys.path.insert(0, str(Path(__file__).resolve().parents[1] / "catalogue-verification"))
from verify_catalogue import CatalogueVerification, click, write_json


class MsdfAtlasVerification(CatalogueVerification):
    def __init__(self, arguments: argparse.Namespace):
        super().__init__(arguments)
        self.pixel_checks: list[dict] = []

    def atlas_pixels(self, case_id: str) -> tuple[bytes, dict]:
        """Crop inside the actual atlas image, excluding its outline and UI chrome."""
        from PIL import Image

        image_bounds = self.bounds("canvas-image")
        canvas_bounds = self.bounds("asset-canvas")
        root = self.tree["clay_nodes"][0]["bounds"]
        with Image.open(self.output / (case_id + ".png")) as image:
            rgb = image.convert("RGB")
            scale_x = rgb.width / root["width"]
            scale_y = rgb.height / root["height"]
            left = max(image_bounds["x"], canvas_bounds["x"]) + 3
            top = max(image_bounds["y"], canvas_bounds["y"]) + 3
            right = min(image_bounds["x"] + image_bounds["width"],
                canvas_bounds["x"] + canvas_bounds["width"]) - 3
            bottom = min(image_bounds["y"] + image_bounds["height"],
                canvas_bounds["y"] + canvas_bounds["height"]) - 3
            crop_box = (math.ceil(left * scale_x), math.ceil(top * scale_y),
                math.floor(right * scale_x), math.floor(bottom * scale_y))
            assert crop_box[2] > crop_box[0] and crop_box[3] > crop_box[1], crop_box
            crop = rgb.crop(crop_box)
            pixels = list(crop.getdata())
            colored = sum(max(pixel) - min(pixel) >= 24 and max(pixel) >= 40 for pixel in pixels)
            black = sum(max(pixel) <= 5 for pixel in pixels)
            stats = {"crop_box": crop_box, "pixel_count": len(pixels),
                "chromatic_pixels": colored, "black_pixels": black,
                "distinct_rgb_values": len(set(pixels)),
                "decoded_rgb_sha256": hashlib.sha256(crop.tobytes()).hexdigest(),
                "image_bounds": image_bounds, "canvas_bounds": canvas_bounds}
            return crop.tobytes(), stats

    def background_pixel(self, case_id: str) -> tuple[int, int, int]:
        from PIL import Image

        canvas = self.bounds("asset-canvas")
        atlas = self.bounds("canvas-image")
        candidates = [(canvas["x"] + 8, canvas["y"] + 8),
            (canvas["x"] + canvas["width"] - 8, canvas["y"] + 8),
            (canvas["x"] + 8, canvas["y"] + canvas["height"] - 8),
            (canvas["x"] + canvas["width"] - 8, canvas["y"] + canvas["height"] - 8)]
        point = next((point for point in candidates if not
            (atlas["x"] <= point[0] <= atlas["x"] + atlas["width"] and
             atlas["y"] <= point[1] <= atlas["y"] + atlas["height"])), None)
        assert point is not None, "Fit view has no letterbox pixel for the background check"
        root = self.tree["clay_nodes"][0]["bounds"]
        with Image.open(self.output / (case_id + ".png")) as image:
            point = (int(point[0] * image.width / root["width"]),
                int(point[1] * image.height / root["height"]))
            return image.convert("RGB").getpixel(point)

    def check(self, case_id: str, name: str, passed: bool, **evidence: object) -> None:
        self.pixel_checks.append({"case_id": case_id, "check": name,
            "passed": bool(passed), **evidence})
        self.save()
        assert passed, self.pixel_checks[-1]

    def fit_control(self) -> dict:
        """Resolve the icon-only first atlas toolbar control by its retained Clay identity."""
        nodes = self.tree["clay_nodes"]
        toolbar = next(node for node in nodes if
            node.get("name", "").endswith("/workbench-area/selected-content/toolbar"))
        controls = [node for node in nodes if node.get("parent") == toolbar["index"] and
            not node.get("text") and node.get("flow_owner") != toolbar.get("flow_owner")]
        assert len(controls) >= 2, "Fit and native atlas toolbar controls are missing"
        return {"clay_id": str(controls[0]["clay_id"])}

    async def catalogue(self) -> None:
        status = await self.call("status")
        self.window_id = next(window["window_id"] for window in status["windows"]
            if not window["internal_development_window"])
        self.open_native_developer_interface()
        await self.execute("open-developer-interface", visual=False)
        for attempt in range(40):
            status = await self.call("status")
            development = [window for window in status["windows"]
                if window["internal_development_window"]]
            if development:
                self.window_id = development[0]["window_id"]
                break
            await asyncio.sleep(.05)
        else:
            raise AssertionError("Developer interface did not open")
        await self.execute("inspect-reference")
        await self.execute("catalogue-open", [click(self.flow("tab-catalogue"))])
        await self.choose_domain(8, "atlases")
        await self.execute("msdf-raw-fit-checker", [click(self.card("MSDF"))])
        text = self.texts()
        self.check("msdf-raw-fit-checker", "raw-msdf-mode-described",
            "MSDF RGB" in text and "Raw RGB" in text,
            observed_text=[line for line in text.splitlines() if "MSDF" in line or "RGB" in line])
        self.check("msdf-raw-fit-checker", "reconstruction-fallback-removed",
            "MSDF reconstruction" not in text and "raw color previews are unavailable" not in text)
        checker_pixels, checker_stats = self.atlas_pixels("msdf-raw-fit-checker")
        self.check("msdf-raw-fit-checker", "raw-rgb-channels-visible",
            checker_stats["chromatic_pixels"] >= 64 and checker_stats["distinct_rgb_values"] >= 32,
            **checker_stats)
        self.check("msdf-raw-fit-checker", "unused-atlas-texels-remain-black",
            checker_stats["black_pixels"] >= 64, black_pixels=checker_stats["black_pixels"])
        initial_viewport = self.bounds("asset-canvas")
        initial_image = self.bounds("canvas-image")
        await self.execute("msdf-raw-light", [click(self.flow("background"))])
        light_pixels, light_stats = self.atlas_pixels("msdf-raw-light")
        light_background = self.background_pixel("msdf-raw-light")
        await self.execute("msdf-raw-dark", [click(self.flow("background"))])
        dark_pixels, dark_stats = self.atlas_pixels("msdf-raw-dark")
        dark_background = self.background_pixel("msdf-raw-dark")
        self.check("msdf-raw-dark", "atlas-rgb-is-opaque-across-backgrounds",
            checker_pixels == light_pixels == dark_pixels,
            checker_rgb_sha256=checker_stats["decoded_rgb_sha256"],
            light_rgb_sha256=light_stats["decoded_rgb_sha256"],
            dark_rgb_sha256=dark_stats["decoded_rgb_sha256"])
        self.check("msdf-raw-dark", "letterbox-background-changes",
            sum(light_background) > sum(dark_background) + 300,
            light_pixel=light_background, dark_pixel=dark_background)
        canvas = {**self.clay("asset-canvas"), "anchor": {"x": .09, "y": .045}}
        await self.execute("msdf-wheel-zoom", [{"operation": "scroll", "target": canvas,
            "scroll": {"x": 0, "y": 1}, "frames": "1", "expect": ["layout_completed"]}])
        zoomed_image = self.bounds("canvas-image")
        self.check("msdf-wheel-zoom", "wheel-zooms-raw-atlas-in-fixed-viewport",
            zoomed_image["width"] > initial_image["width"] + 1 and
            self.bounds("asset-canvas") == initial_viewport,
            initial_image=initial_image, zoomed_image=zoomed_image, viewport=initial_viewport)
        geometry = await self.call("interaction.targets",
            {"window_id": self.window_id, "target": canvas})
        point = geometry["window_point"]
        await self.execute("msdf-drag-pan", [{"operation": "drag", "target": canvas,
            "destination": {"point": {"x": point["x"] - 32, "y": point["y"] - 20}},
            "frames": "8", "expect": ["layout_completed"]}])
        panned_image = self.bounds("canvas-image")
        self.check("msdf-drag-pan", "drag-pans-raw-atlas-in-fixed-viewport",
            (abs(panned_image["x"] - zoomed_image["x"]) > 1 or
             abs(panned_image["y"] - zoomed_image["y"]) > 1) and
            self.bounds("asset-canvas") == initial_viewport,
            before=zoomed_image, after=panned_image, viewport=initial_viewport)
        await self.execute("msdf-fit-restored", [click(self.fit_control())])
        restored_pixels, restored_stats = self.atlas_pixels("msdf-fit-restored")
        self.check("msdf-fit-restored", "fit-restores-complete-raw-atlas",
            self.bounds("canvas-image") == initial_image and restored_pixels == checker_pixels,
            initial_image=initial_image, restored_image=self.bounds("canvas-image"),
            restored_rgb_sha256=restored_stats["decoded_rgb_sha256"])

    def save(self) -> None:
        unchanged = self.fingerprint(self.original_changes) == self.original_digest
        write_json(self.output / "evidence.json", {"schema": "FlowUi.MsdfAtlas.Verification",
            "run_id": self.run_id, "mcp_calls": len(self.records), "cases": self.cases,
            "required_checks": sum(int(case["coverage"]["required"]) for case in self.cases),
            "failures": self.failures, "layout_checks": self.layout_checks,
            "pixel_checks": self.pixel_checks, "user_changes_unchanged": unchanged,
            "bootstrap": "Native Ctrl+Shift+D on the owned demo PID; subsequent interactions use MCP",
            "tree_delivery": "Integrity-checked sealed local artifact; PNG retrieval uses MCP",
            "passed": not self.failures and unchanged and bool(self.cases) and
                all(check["passed"] for check in [*self.layout_checks, *self.pixel_checks])})

    async def run(self) -> int:
        result = await super().run()
        evidence = json.loads((self.output / "evidence.json").read_text())
        return result if result else (0 if evidence["passed"] else 1)


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.set_defaults(only_atlas=False)
    parser.add_argument("--cli", type=Path, required=True)
    parser.add_argument("--demo", type=Path, required=True)
    parser.add_argument("--workspace", type=Path, default=Path.cwd())
    parser.add_argument("--output-directory", type=Path, required=True)
    return asyncio.run(MsdfAtlasVerification(parser.parse_args()).run())


if __name__ == "__main__":
    raise SystemExit(main())
