#!/usr/bin/env python3
"""Verify wrapped real geometry and preserve the normal metric presentation through MCP."""
from __future__ import annotations

import argparse
import asyncio
import hashlib
import json
from pathlib import Path

from DevInterfaceNotesMcpTests import DevInterfaceNotesVerification, REPOSITORY


STRUCTURE_LABELS = ["Flow children", "Clay children", "Depth", "Z / paint", "Flags"]
GEOMETRY_LABELS = ["Position", "Dimensions", "Layout", "Padding", "Sizing", "Clip / overflow"]
OVERVIEW = "overview-and-performance/overview-and-performance/"


def metric_rows(tree: dict, labels: list[str]) -> list[tuple[dict, dict, dict]]:
    nodes = tree["clay_nodes"]
    structure = next(node for node in nodes if node.get("name", "").endswith("/" + OVERVIEW + "structure"))
    descendants = nodes[structure["index"]:structure["subtree_end_exclusive"]]
    rows = []
    for label in labels:
        matches = [node for node in descendants if node.get("text") == label]
        assert len(matches) == 1, (label, matches)
        label_node = matches[0]
        row = nodes[label_node["parent"]]
        values = [node for node in nodes[row["index"]:row["subtree_end_exclusive"]]
            if node.get("text_present") and str(node.get("text") or "").strip()
            and node["index"] != label_node["index"]]
        assert len(values) == 1, (label, values)
        rows.append((row, label_node, values[0]))
    return rows


def metric_signature(tree: dict, labels: list[str]) -> list[tuple[str, str, int]]:
    return [(label["text"], value["text"], int(value.get("wrapped_line_count", 1)))
        for _, label, value in metric_rows(tree, labels)]


class GeometryStressVerification(DevInterfaceNotesVerification):
    def __init__(self, arguments: argparse.Namespace):
        super().__init__(arguments)
        self.reference_directory = arguments.reference_directory.resolve(strict=True)

    def compare_normal_metrics(self, width: int, kind: str) -> None:
        from PIL import Image, ImageChops, ImageStat

        case_id = "geometry-" + kind + "-visible-" + str(width)
        current_tree = json.loads((self.output / (case_id + "-tree.json")).read_text())
        reference_tree = json.loads((self.reference_directory / (case_id + "-tree.json")).read_text())
        labels = STRUCTURE_LABELS if kind == "structure" else GEOMETRY_LABELS
        self.record_layout_check(case_id, "normal-metric-values-and-lines-unchanged",
            metric_signature(current_tree, labels) == metric_signature(reference_tree, labels),
            expected=metric_signature(reference_tree, labels), observed=metric_signature(current_tree, labels))
        suffix = "/" + OVERVIEW + ("column[0]" if kind == "structure" else "column[1]")
        current = next(node["bounds"] for node in current_tree["clay_nodes"]
            if node.get("name", "").endswith(suffix))
        reference = next(node["bounds"] for node in reference_tree["clay_nodes"]
            if node.get("name", "").endswith(suffix))
        self.record_layout_check(case_id, "normal-metric-column-size-unchanged",
            abs(current["width"] - reference["width"]) < 1 and
            abs(current["height"] - reference["height"]) < 1,
            expected=reference, observed=current)
        crop_width = max(1, round(min(current["width"], reference["width"])))
        crop_height = max(1, round(min(current["height"], reference["height"])))

        def crop(path: Path, bounds: dict):
            left, top = round(bounds["x"]), round(bounds["y"])
            with Image.open(path) as image:
                return image.convert("RGB").crop((left, top, left + crop_width, top + crop_height))

        observed_image = crop(self.output / (case_id + ".png"), current)
        reference_image = crop(self.reference_directory / (case_id + ".png"), reference)
        difference = ImageChops.difference(observed_image, reference_image)
        mean_error = sum(ImageStat.Stat(difference).mean) / 3
        channels = difference.tobytes()
        changed_fraction = sum(value > 8 for value in channels) / max(1, len(channels))
        # The real splitter reference differs by0.176px; allow subpixel glyph antialiasing,
        # while requiring identical labels, values, line counts and column dimensions above.
        self.record_layout_check(case_id, "normal-metric-screenshot-unchanged",
            mean_error <= 2 and changed_fraction <= .08, mean_channel_error=mean_error,
            changed_channel_fraction=changed_fraction, crop_size=[crop_width, crop_height],
            reference_sha256=hashlib.sha256(reference_image.tobytes()).hexdigest(),
            observed_sha256=hashlib.sha256(observed_image.tobytes()).hexdigest())
        self.save()
        self.assert_case_layout(case_id)

    async def verify_stress(self, width: int) -> None:
        configuration = await self.configure(geometry_stress=True, selection="sample")
        assert configuration["geometry_stress"] is True, configuration
        await self.execute("geometry-stress-reference-" + str(width))
        card_suffix = "selected-content/content/overview-and-performance"
        body_suffix = OVERVIEW + "overview-body"
        if not self.fully_visible(self.clay_node(card_suffix)):
            await self.align_scroll_element("geometry-stress-overview-" + str(width),
                "selected-content/content", card_suffix, 12)
        await self.align_scroll_element("geometry-stress-structure-" + str(width),
            body_suffix, OVERVIEW + "structure", 8)
        structure_case = "geometry-stress-structure-visible-" + str(width)
        await self.execute(structure_case)
        self.measure_visible_metrics(structure_case, STRUCTURE_LABELS)
        self.assert_case_layout(structure_case)

        position_value = metric_rows(self.tree, ["Position"])[0][2]
        self.record_layout_check(structure_case, "stress-position-really-wraps",
            int(position_value.get("wrapped_line_count", 1)) >= 2,
            text=position_value["text"], wrapped_line_count=position_value.get("wrapped_line_count"),
            observed=position_value["bounds"])
        dimensions = metric_rows(self.tree, ["Dimensions"])[0][2]["text"]
        self.record_layout_check(structure_case, "stress-uses-real-large-dimensions",
            "345678" in dimensions and "456789" in dimensions, observed=dimensions)
        clip = metric_rows(self.tree, ["Clip / overflow"])[0][2]["text"]
        self.record_layout_check(structure_case, "stress-sample-has-viewport-clip",
            not clip.startswith("Clay 0 "), observed=clip)
        self.assert_case_layout(structure_case)
        remaining = list(GEOMETRY_LABELS)
        page = 0
        while remaining:
            page += 1
            rows = metric_rows(self.tree, remaining)
            body = self.clay_node(body_suffix)["bounds"]
            top = rows[0][0]["bounds"]["y"]
            group = []
            group_height = 0.0
            for row, label, _ in rows:
                height = row["bounds"]["y"] + row["bounds"]["height"] - top
                if height > body["height"] - 2:
                    break
                group.append(label["text"])
                group_height = height
            assert group, ("A wrapped metric exceeds the entire viewport", width, remaining, rows[0][0]["bounds"], body)
            row_suffix = rows[0][0]["name"].split(OVERVIEW, 1)[1]
            padding = max(1, min(8, (body["height"] - group_height) / 2))
            case_id = "geometry-stress-values-" + str(width) + "-page-" + str(page)
            await self.align_scroll_element(case_id, body_suffix, OVERVIEW + row_suffix, padding)
            await self.execute(case_id)
            self.measure_visible_metrics(case_id, group)
            self.record_layout_check(case_id, "stress-page-covers-metric-values", bool(group),
                metrics=group, remaining_count=len(remaining) - len(group))
            self.save()
            self.assert_case_layout(case_id)
            remaining = remaining[len(group):]

    async def catalogue(self) -> None:
        await self.execute("geometry-stress-warmup", visual=False)
        await self.configure(selection="sample", active_tab="inspect", inspector_width=320,
            window_width=980, clear_override=True, theme="default", geometry_stress=False)
        await self.execute("geometry-normal-reference")
        normal_signature = []
        for width in [980, 1100, 1420]:
            await self.configure(geometry_stress=False)
            await self.verify_geometry(width)
            self.compare_normal_metrics(width, "structure")
            self.compare_normal_metrics(width, "values")
            normal_signature = metric_signature(self.tree, STRUCTURE_LABELS + GEOMETRY_LABELS)
            await self.verify_stress(width)
        restored = await self.configure(geometry_stress=False)
        assert restored["geometry_stress"] is False, restored
        await self.execute("geometry-stress-disabled-restored")
        self.record_layout_check("geometry-stress-disabled-restored", "stress-disabling-restores-normal-metrics",
            metric_signature(self.tree, STRUCTURE_LABELS + GEOMETRY_LABELS) == normal_signature,
            expected=normal_signature, observed=metric_signature(self.tree, STRUCTURE_LABELS + GEOMETRY_LABELS))
        self.save()
        self.assert_case_layout("geometry-stress-disabled-restored")


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--cli", type=Path, required=True)
    parser.add_argument("--demo", type=Path, required=True)
    parser.add_argument("--workspace", type=Path, default=REPOSITORY)
    parser.add_argument("--output-directory", type=Path, required=True)
    parser.add_argument("--reference-directory", type=Path, required=True)
    parser.set_defaults(stage="complete", fixture=True)
    return asyncio.run(GeometryStressVerification(parser.parse_args()).run())


if __name__ == "__main__":
    raise SystemExit(main())
