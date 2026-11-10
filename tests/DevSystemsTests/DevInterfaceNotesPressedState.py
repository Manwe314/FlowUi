"""Retain a real pressed developer-header frame through the MCP interaction API."""
from __future__ import annotations

import asyncio
import json
from pathlib import Path
import time
import uuid


def _write_json(path: Path, value: object) -> None:
    path.write_text(json.dumps(value, ensure_ascii=False, indent=2) + "\n")


def _frame_key(frame: dict) -> tuple[str, ...]:
    return tuple(str(frame[field]) for field in ("run_id", "window_id", "window_frame", "app_tick"))


async def verify_header_pressed(verifier) -> None:
    """Join a held Undo input, completed observation, tree, and rendered image exactly.

    Moving away before releasing cancels the gesture and preserves the real undo history
    for the subsequent activation checks in the main suite.
    """
    case_id = "header-undo-pressed"
    assert int(verifier.fixture_state["undo_count"]) > 0, verifier.fixture_state
    original_undo_count = int(verifier.fixture_state["undo_count"])
    original_redo_count = int(verifier.fixture_state["redo_count"])
    target = verifier.flow("permanent-header/undo")
    flow_node = next(node for node in verifier.tree["flow_nodes"]
        if str(node["instance_id"]) == str(target["flow_instance_id"]))
    bounds = verifier.tree["clay_nodes"][flow_node["clay_root"]]["bounds"]
    away = {"point": {"x": bounds["x"] + bounds["width"] * .5,
        "y": bounds["y"] + bounds["height"] + 50}}
    await verifier.execute("header-undo-press-ready", [{"operation": "hover", "target": target,
        "frames": "3", "expect": ["target_hovered"]}], visual=False)
    plan = {"schema": "FlowUi.Agentic.InteractionPlan", "schema_version": "1.0",
        "mode": "common_controls", "window_id": verifier.window_id,
        "baseline": "require_neutral",
        "content": {"persistence": "retain_text", "record_observations": True},
        "limits": {"maximum_observations": "128"}, "steps": [
            {"step_id": "down", "operation": "pointer.down", "target": target,
                "require_hover_before_down": False, "frames": "1", "expect": ["target_pressed"]},
            {"step_id": "held", "operation": "pointer.hold", "target": target,
                "frames": "2", "expect": ["target_held", "frame_presented"]},
            {"step_id": "cancel-outside", "operation": "hover", "target": away,
                "frames": "1"},
            {"step_id": "release-outside", "operation": "pointer.up", "target": away,
                "frames": "1"}]}
    capture = {"profile": "visual_inspect", "mode": "windowed",
        "window_ids": [verifier.window_id], "duration_ns": "30000000000",
        "maximum_expected_tick_cadence": 60,
        "sections": {"performance": "omit", "runtime_diagnostics": "omit",
            "memory": "omit", "visual": "required"},
        "visual_sources": {"window": True, "viewports": False},
        "content": {"include_tree_text": True, "include_interaction_labels": True,
            "include_interaction_outcomes": True},
        "limits": {"maximum_images": "3", "maximum_image_bytes": "12582912",
            "maximum_snapshot_bytes": "33554432"}}
    admitted = await verifier.call("interaction.start", {"plan": plan, "capture": capture,
        "idempotency_key": "notes-header-pressed-" + uuid.uuid4().hex})
    deadline = time.monotonic() + 60
    while time.monotonic() < deadline:
        status = await verifier.call("job.status", {"job_id": admitted["job_id"]})
        if status["phase"] in {"sealed", "failed", "canceled"}:
            assert status["phase"] == "sealed", status
            break
        await asyncio.sleep(.05)
    else:
        raise TimeoutError("Pressed header interaction did not seal")
    outcome = await verifier.call("interaction.query", {"job_id": admitted["job_id"]})
    assert all(outcome[field] for field in ("execution_succeeded", "cleanup_complete",
        "handoff_complete", "evidence_complete")), outcome
    assert outcome["assertion_verdict"] == "passed", outcome
    observations = await verifier.call("interaction.query", {"job_id": admitted["job_id"],
        "kind": "observations", "limit": 128, "max_bytes": 524288})
    inputs = await verifier.call("interaction.query", {"job_id": admitted["job_id"],
        "kind": "inputs", "limit": 128, "max_bytes": 524288})
    capture_id = admitted["capture_id"]
    frames = await verifier.call("query.visual", {"capture": capture_id, "kind": "frames",
        "limit": 100, "max_bytes": 262144}, live=False)
    manifest_path = Path(status["manifest_path"]).resolve(strict=True)
    assert manifest_path.is_relative_to(verifier.store.resolve()), manifest_path
    manifest = json.loads(manifest_path.read_text())
    assert manifest["run_id"] == verifier.run_id and manifest["capture_id"] == capture_id, manifest
    artifacts = {entry["artifact_id"]: entry for entry in manifest["artifacts"]}
    for artifact in artifacts.values():
        payload = (manifest_path.parent / artifact["path"]).resolve(strict=True)
        assert payload.is_relative_to(manifest_path.parent), artifact
        assert payload.stat().st_size == int(artifact["bytes"]), artifact
        assert verifier.fingerprint(payload) == artifact["sha256"], artifact
    input_frames = {_frame_key(entry["frame"]): entry for entry in inputs["records"]
        if entry.get("frame") and not entry["baseline"] and not entry["cleanup"]}
    image_frames = {_frame_key(entry): entry for entry in frames["records"]}
    matches = []
    for observation in observations["records"]:
        if not observation.get("frame") or not observation.get("target"):
            continue
        frame_key = _frame_key(observation["frame"])
        clay_id = str(observation["target"]["clay_id"])
        if frame_key in image_frames and frame_key in input_frames and \
                clay_id in {str(value) for value in observation["held"]} and \
                observation["layout_completed"] and observation["presented"] and \
                not observation["input_suppressed"] and \
                0 in input_frames[frame_key]["effective_input"]["buttons_down"]:
            matches.append((observation, image_frames[frame_key], input_frames[frame_key]))
    assert matches, {"observations": observations, "images": frames}
    observation, image, selected_input = matches[0]
    assert image.get("tree"), image
    tree_artifact = artifacts[image["tree"]["artifact_id"]]
    tree = json.loads((manifest_path.parent / tree_artifact["path"]).read_text())
    assert tree["complete"] and not tree["truncated"], tree
    assert _frame_key(tree) == _frame_key(image) == _frame_key(observation["frame"])
    pressed_flow = next(node for node in tree["flow_nodes"]
        if str(node["instance_id"]) == str(target["flow_instance_id"]))
    pressed_bounds = tree["clay_nodes"][pressed_flow["clay_root"]]["bounds"]
    assert pressed_bounds == bounds, (pressed_bounds, bounds)
    image_path = verifier.output / (case_id + ".png")
    _write_json(verifier.output / (case_id + "-tree.json"), tree)
    await verifier.call("artifact.image", {"capture": capture_id, "image": image["image_id"],
        "include_image": True}, live=False, image_path=image_path)
    from PIL import Image
    with Image.open(image_path) as original:
        rendered = original.convert("RGB")
        root_bounds = tree["clay_nodes"][0]["bounds"]
        scale_x = rendered.width / root_bounds["width"]
        scale_y = rendered.height / root_bounds["height"]
        samples = [rendered.getpixel((int((bounds["x"] + inset_x) * scale_x),
            int((bounds["y"] + inset_y) * scale_y))) for inset_x, inset_y in ((4, 7), (7, 4), (4, 20))]
    expected_pressed_background = (14, 54, 84)
    matching_samples = sum(max(abs(actual[channel] - expected_pressed_background[channel])
        for channel in range(3)) <= 2 for actual in samples)
    check = {"capture_id": capture_id, "image_id": image["image_id"],
        "frame": observation["frame"], "observation_sequence": observation["sequence"],
        "input_sequence": selected_input["sequence"], "held_clay_id": observation["target"]["clay_id"],
        "tree_artifact": tree_artifact["artifact_id"], "samples": samples,
        "expected_background": expected_pressed_background, "outcome": outcome,
        "observation": observation, "input": selected_input, "image": image}
    _write_json(verifier.output / (case_id + "-evidence.json"), check)
    verifier.record_layout_check(case_id, "pressed-image-observation-input-tree-frame-join", True,
        frame=observation["frame"], image_id=image["image_id"], tree_artifact=tree_artifact["artifact_id"])
    verifier.record_layout_check(case_id, "pressed-header-background", matching_samples >= 2,
        expected=expected_pressed_background, samples=samples, bounds=bounds)
    state = await verifier.configure(active_tab="inspect")
    unchanged_history = int(state["undo_count"]) == original_undo_count and \
        int(state["redo_count"]) == original_redo_count
    verifier.record_layout_check(case_id, "canceled-press-preserves-history", unchanged_history,
        before={"undo_count": original_undo_count, "redo_count": original_redo_count},
        after={key: state[key] for key in ("undo_count", "redo_count")})
    verifier.cases.append({"case_id": case_id, "verdict": "pass", "job_id": admitted["job_id"],
        "coverage": {"required": "3"}, "capture_id": capture_id, "image_id": image["image_id"],
        "window_frame": image["window_frame"], "image": image_path.name})
    verifier.save()
    verifier.assert_case_layout(case_id)
