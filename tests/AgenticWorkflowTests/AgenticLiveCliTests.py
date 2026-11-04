#!/usr/bin/env python3
"""Exercise the shipped CLI and authenticated IPC against one real demo App process."""
from __future__ import annotations

import argparse
import json
import os
from pathlib import Path
import subprocess
import tempfile
import time


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("cli", type=Path)
    parser.add_argument("demo", type=Path)
    parser.add_argument("plan", type=Path)
    arguments = parser.parse_args()
    if os.name != "nt" and not os.environ.get("DISPLAY") and not os.environ.get("WAYLAND_DISPLAY"):
        return 77
    cli, demo, plan = arguments.cli.resolve(strict=True), arguments.demo.resolve(strict=True), arguments.plan.resolve(strict=True)
    with tempfile.TemporaryDirectory(prefix="flowui-live-interaction-") as directory:
        store = Path(directory) / "store"
        log_path = Path(directory) / "demo.log"
        run_id = None
        with log_path.open("wb") as log:
            process = subprocess.Popen([str(demo), "--agentic-store", str(store), "--agentic-input", "--agentic-actions"], stdout=log, stderr=subprocess.STDOUT)
            try:
                deadline = time.monotonic() + 20
                while time.monotonic() < deadline:
                    descriptors = list((store / "runtime" / "instances").glob("*.json"))
                    if descriptors:
                        run_id = json.loads(descriptors[0].read_text())["run_id"]
                        break
                    if process.poll() is not None:
                        raise AssertionError(log_path.read_text(errors="replace")[-4000:])
                    time.sleep(0.02)
                assert run_id is not None, "demo did not publish its endpoint"

                def command(operation: str, values: dict | None = None) -> dict:
                    completed = subprocess.run([str(cli), *operation.split("."), "--store", str(store), "--instance", run_id,
                        "--arguments-json", json.dumps(values or {}), "--json"], stdout=subprocess.PIPE, stderr=subprocess.PIPE, text=True, timeout=10)
                    envelope = json.loads(completed.stdout)
                    if completed.returncode != 0:
                        try:
                            process.wait(timeout=2)
                        except subprocess.TimeoutExpired:
                            pass
                    assert completed.returncode == 0, (envelope, log_path.read_text(errors="replace")[-4000:])
                    return envelope["result"]

                def finish(job_id: str) -> dict:
                    deadline = time.monotonic() + 40
                    while time.monotonic() < deadline:
                        status = command("job.status", {"job_id": job_id})
                        assert status["phase"] != "failed", status
                        if status["phase"] == "sealed":
                            result = command("interaction.query", {"job_id": job_id})
                            assert result["execution_succeeded"], result
                            return result
                        time.sleep(0.01)
                    raise AssertionError(f"job did not seal: {job_id}")

                capture = {"duration_ns": "30000000000", "maximum_expected_tick_cadence": 60, "sections": {"performance": "omit", "runtime_diagnostics": "omit"},
                    "content": {"include_interaction_outcomes": True}}
                capabilities = command("interaction.capabilities")
                assert capabilities["input_control_enabled"] and capabilities["action_control_enabled"]
                exports = command("action.list")
                assert "demo.configure" in json.dumps(exports)
                configured = command("action.invoke", {"action_id": "demo.configure", "revision": "1",
                    "arguments": {"page": "writing", "theme": "agentic-ocean", "editor_soft_wrap": False},
                    "capture": capture, "idempotency_key": "live-configure-writing"})
                result = finish(configured["job_id"])
                assert result["action"]["completed"] and result["action"]["result"]["page"] == "writing", result
                assert result["action"]["result"]["editor_soft_wrap"] is False, result
                reset = command("action.invoke", {"action_id": "demo.reset", "revision": "1", "arguments": {},
                    "capture": capture, "idempotency_key": "live-reset-gallery"})
                finish(reset["job_id"])
                clicked = command("interaction.start", {"plan": json.loads(plan.read_text()), "capture": capture,
                    "idempotency_key": "live-click-custom-region"})
                click_result = finish(clicked["job_id"])
                assert click_result["assertion_verdict"] == "passed", click_result
                observations = command("interaction.query", {"job_id": clicked["job_id"], "kind": "observations", "limit": 100})
                outcomes = [record.get("target_observation", {}).get("application_outcome", {}) for record in observations["records"]]
                assert any(int(outcome.get("button_activations", "0")) > 0 for outcome in outcomes), outcomes
                published = command("job.status", {"job_id": clicked["job_id"]})
                manifest = json.loads(Path(published["manifest_path"]).read_text())
                retained_plan = next(artifact for artifact in manifest["artifacts"] if artifact["artifact_id"] == "interaction_plan")
                replayed = command("interaction.start", {"plan_artifact": {"capture": manifest["capture_id"],
                    "artifact": retained_plan["artifact_id"], "sha256": retained_plan["sha256"]}, "rebind_run": True,
                    "capture": capture, "idempotency_key": "live-checksum-selected-replay"})
                replay_result = finish(replayed["job_id"])
                assert replay_result["plan_digest"] == click_result["plan_digest"], replay_result
                navigation = {"mode": "common_controls", "window_id": "1", "deadline_ns": "30000000000",
                    "content": {"persistence": "retain_text", "record_observations": True}, "steps": [
                        {"operation": "click", "target": {"custom_id": "demo.navigate"}, "deadline_ns": "10000000000",
                         "expect": ["target_hovered", "target_pressed", "target_released"]},
                        {"operation": "click", "target": {"custom_id": "demo.editor_body"}, "deadline_ns": "10000000000",
                         "expect": ["target_hovered", "target_pressed", "target_released", "text_focused"]},
                        {"operation": "text.enter", "text": "agentic replay", "target": {"custom_id": "demo.editor_body"},
                         "deadline_ns": "10000000000", "expect": ["text_focused"]}]}
                navigated = command("interaction.start", {"plan": navigation, "capture": capture,
                    "idempotency_key": "live-navigate-focus-enter-text"})
                finish(navigated["job_id"])
                edited = command("interaction.query", {"job_id": navigated["job_id"], "kind": "observations", "limit": 100})
                edit_outcomes = [record.get("target_observation", {}).get("application_outcome", {}) for record in edited["records"]]
                assert any(int(outcome.get("document_changes", "0")) > 0 for outcome in edit_outcomes), edit_outcomes
                recording = command("interaction.record.start", {"window_id": "1", "max_frames": "16", "deadline_ns": "5000000000",
                    "include_text": False, "idempotency_key": "live-record-input"})
                command("interaction.record.stop", {"job_id": recording["job_id"]})
                record_result = finish(recording["job_id"])
                assert int(record_result["coverage"]["consumed"]) > 0, record_result
                print("Live CLI, IPC, typed actions, custom clicks, stored replay, navigation, text entry, application results and recording passed")
            finally:
                process.terminate()
                try:
                    process.wait(timeout=10)
                except subprocess.TimeoutExpired:
                    process.kill()
                    process.wait(timeout=5)
                if os.name != "nt" and run_id:
                    (Path(tempfile.gettempdir()) / f"flowui_{run_id}.sock").unlink(missing_ok=True)
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
