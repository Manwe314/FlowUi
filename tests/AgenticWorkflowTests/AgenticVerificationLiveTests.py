#!/usr/bin/env python3
"""Verify run-and-report against the real FSEL demo and its developer window."""
from __future__ import annotations

import argparse
import copy
import json
import os
from pathlib import Path
import struct
import subprocess
import tempfile
import time
import uuid


def interaction_case(case_id: str, steps: list[dict], checkpoints: list[dict] | None = None) -> dict:
    return {"case_id": case_id, "workload": {"kind": "interaction_plan", "plan": {
        "schema": "FlowUi.Agentic.InteractionPlan", "schema_version": "1.0",
        "mode": "common_controls", "window_id": "1", "deadline_ns": "10000000000",
        "limits": {"maximum_frames": "128", "maximum_spans": "128", "maximum_steps": "16",
            "maximum_observations": "256", "maximum_artifact_bytes": "1048576"},
        "content": {"persistence": "redact_text", "record_observations": True},
        "steps": steps}}, "checkpoints": checkpoints or []}


def click(target: str, step_id: str) -> dict:
    return {"step_id": step_id, "operation": "click", "target": {"custom_id": target},
        "expect": ["target_hovered", "target_pressed", "target_released", "layout_completed"]}


def scenario_case(case_id: str, scenario_id: str, fixture_id: str,
                  checkpoints: list[dict] | None = None) -> dict:
    return {"case_id": case_id, "workload": {"kind": "registered_scenario",
        "scenario_id": scenario_id, "revision": "1", "fixture_id": fixture_id, "seed": "17"},
        "checkpoints": checkpoints or []}


def request(cases: list[dict], key: str, visual: bool = False) -> dict:
    return {"schema": "FlowUi.Agentic.VerificationRequest", "schema_version": "1.0",
        "suite_id": "fsel.developer.smoke", "suite_revision": "1", "idempotency_key": key,
        "execution_timeout_ns": "30000000000", "wait_timeout_ms": 20000,
        "failure_policy": "stop_suite", "cases": cases,
        "evidence": {"policy": "checkpoints_and_failure" if visual else "assertions_only"},
        "limits": {"maximum_cases": 16, "maximum_images": "2" if visual else "0",
            "maximum_snapshot_bytes": "33554432", "maximum_retained_bytes": "67108864",
            "maximum_report_bytes": "65536"},
        "report": {"detail": "all", "include_contact_sheet": False}}


class VerificationSuite:
    def __init__(self, arguments: argparse.Namespace, workspace: Path):
        self.cli = arguments.cli.resolve(strict=True)
        self.demo = arguments.demo.resolve(strict=True)
        self.workspace = workspace
        self.store = workspace / "store"
        self.log_path = workspace / "demo.log"
        self.process: subprocess.Popen | None = None
        self.run_id = ""
        self.results: dict = {"checks": [], "measurements": {}}

    def command(self, operation: str, values: dict | None = None,
                expected_code: int = 0) -> dict:
        outgoing = copy.deepcopy(values or {})
        if operation in {"verification.plan", "verification.run"} and "request" in outgoing:
            outgoing["request"].setdefault("instance", self.run_id)
        completed = subprocess.run([str(self.cli), *operation.split("."), "--store", str(self.store),
            "--instance", self.run_id, "--arguments-json", json.dumps(outgoing), "--json"],
            stdout=subprocess.PIPE, stderr=subprocess.PIPE, text=True, timeout=30)
        try:
            envelope = json.loads(completed.stdout)
        except json.JSONDecodeError as exception:
            raise AssertionError((operation, completed.returncode, completed.stdout,
                completed.stderr[-2000:])) from exception
        assert completed.returncode == expected_code, (operation, envelope,
            self.log_path.read_text(errors="replace")[-4000:])
        return envelope

    def start(self) -> None:
        with self.log_path.open("wb") as log:
            self.process = subprocess.Popen([str(self.demo), "--agentic-store", str(self.store),
                "--agentic-input", "--agentic-actions", "--agentic-visual", "--agentic-dev-interface"],
                cwd=self.workspace, stdout=log, stderr=subprocess.STDOUT)
        deadline = time.monotonic() + 30
        while time.monotonic() < deadline:
            assert self.process.poll() is None, self.log_path.read_text(errors="replace")[-4000:]
            descriptors = list((self.store / "runtime/instances").glob("*.json"))
            if descriptors:
                assert len(descriptors) == 1
                descriptor = json.loads(descriptors[0].read_text())
                assert int(descriptor["pid"]) == self.process.pid
                self.run_id = descriptor["run_id"]
                return
            time.sleep(0.02)
        raise AssertionError("Demo did not publish its authenticated endpoint")

    def report(self, result: dict) -> dict:
        if "report" in result:
            return result["report"]
        if result.get("schema") == "FlowUi.Agentic.VerificationReport":
            return result
        job_id = result["job_id"]
        deadline = time.monotonic() + 45
        sequence = "0"
        while time.monotonic() < deadline:
            status = self.command("verification.status", {"job_id": job_id,
                "after_sequence": sequence, "wait_timeout_ms": 1000})["result"]
            sequence = status["sequence"]
            assert "images" not in status and "tree" not in status, status
            if status.get("report_published") or status.get("terminal") or \
                    status.get("execution_status") in {"completed", "failed", "canceled", "timeout"} or \
                    status.get("phase") in {"completed", "failed", "canceled", "timeout", "terminal", "sealed"}:
                reported = self.command("verification.report", {"job_id": job_id})["result"]
                return reported.get("report", reported)
        raise AssertionError(f"Verification report did not seal: {job_id}")

    def run(self, workload: dict, *, require_pass: bool = False, expected_code: int = 0) -> dict:
        admitted = self.command("verification.run", {"request": workload,
            "require_pass": require_pass}, expected_code=expected_code)
        return self.report(admitted["result"])

    def passed(self, name: str, report: dict) -> None:
        assert report["execution_status"] == "completed", report
        assert report["verdict"] == "pass", report
        assert int(report["coverage"]["evaluated"]) > 0, report
        assert int(report["coverage"]["failed"]) == 0, report
        self.results["checks"].append({"name": name, "job_id": report["job_id"],
            "verdict": report["verdict"], "coverage": report["coverage"]})

    def execute(self, soak_count: int) -> None:
        gallery = scenario_case("gallery", "demo.gallery.steady", "gallery-default-v1")
        smoke = request([gallery], "gallery-assertions-001")
        planned = self.command("verification.plan", {"request": smoke})
        assert planned["operation_status"] == "completed", planned
        assert planned["result"]["feasible"], planned
        stale = copy.deepcopy(smoke)
        stale["cases"][0]["workload"]["revision"] = "stale"
        stale_plan = self.command("verification.plan", {"request": stale})["result"]
        assert stale_plan["feasible"] is False and stale_plan["rejected_reasons"], stale_plan
        unknown = copy.deepcopy(smoke)
        unknown["execute_script"] = "invalid"
        self.command("verification.plan", {"request": unknown}, expected_code=2)
        first = self.run(smoke)
        self.passed("registered semantic assertions", first)
        assert int(first["evidence"]["image_count"]) == 0, first
        assert int(first["resources"]["readback_reserved_bytes"]) == 0, first
        duplicate = copy.deepcopy(smoke)
        duplicate["wait_timeout_ms"] = 1
        duplicate["report"]["detail"] = "failures"
        retried = self.run(duplicate)
        assert retried["job_id"] == first["job_id"], (first, retried)
        conflicting = copy.deepcopy(smoke)
        conflicting["suite_revision"] = "2"
        conflict = self.command("verification.run", {"request": conflicting}, expected_code=2)
        assert conflict["error"]["code"] == "idempotency_conflict", conflict
        self.results["checks"].append({"name": "idempotent retry and response preferences", "passed": True})

        incorrect_seed = copy.deepcopy(gallery)
        incorrect_seed["workload"]["seed"] = "0"
        rejected_seed = self.run(request([incorrect_seed], "seed-zero-001"))
        assert rejected_seed["execution_status"] == "failed" and rejected_seed["verdict"] == "inconclusive", rejected_seed
        assert rejected_seed["cleanup"]["completed"], rejected_seed
        self.results["checks"].append({"name": "explicit zero seed reaches registered fixture", "passed": True})

        second_gallery = copy.deepcopy(gallery)
        second_gallery["case_id"] = "gallery-repeated"
        paginated = self.run(request([gallery, second_gallery], "pagination-001"))
        self.passed("sequential resettable cases", paginated)
        first_page = self.command("verification.report", {"job_id": paginated["job_id"], "limit": 1})["result"]
        assert first_page["truncated"] and first_page["cursor"] and len(first_page["cases"]) == 1, first_page
        second_page = self.command("verification.report", {"job_id": paginated["job_id"],
            "limit": 1, "cursor": first_page["cursor"]})["result"]
        assert second_page["coverage"] == first_page["coverage"] and not second_page["truncated"], second_page
        assert second_page["cases"][0]["case_id"] != first_page["cases"][0]["case_id"]

        navigation = interaction_case("gallery-writing-pointer-routing", [
            click("demo.primary_button", "activate-gallery-button"),
            click("demo.navigate", "open-writing"),
            {**click("demo.editor_body", "focus-document"),
                "expect": ["target_hovered", "target_pressed", "target_released", "text_focused"]},
            {"step_id": "enter-document", "operation": "text.enter", "text": "suite verification",
                "target": {"custom_id": "demo.editor_body"}, "expect": ["text_focused"]},
            click("demo.navigate", "return-gallery")])
        self.passed("batched pointer navigation and editing", self.run(request([navigation], "pointer-001")))

        # This fixture owns a closed developer window and resets it after a genuine tab click.
        developer = scenario_case("developer-catalogue", "demo.dev_interface.catalogue",
            "developer-interface-catalogue-v1")
        developer_report = self.run(request([developer], "developer-001"))
        self.passed("developer interface pointer routing and catalogue layout", developer_report)
        observed_frames = [assertion["observed_frame"] for case in developer_report["cases"]
            for assertion in case["assertions"] if "observed_frame" in assertion]
        assert observed_frames, developer_report
        developer_window = observed_frames[0]["window_id"]
        assert developer_window != "1", observed_frames

        checkpoint = {"checkpoint_id": "gallery-visible", "trigger": "case_verified",
            "evidence": "image_and_tree"}
        visual_gallery = scenario_case("gallery-visual", "demo.gallery.steady", "gallery-default-v1", [checkpoint])
        visual_developer = scenario_case("developer-visual", "demo.dev_interface.catalogue",
            "developer-interface-catalogue-v1", [{"checkpoint_id": "catalogue-visible",
                "trigger": "case_verified", "evidence": "image_and_tree", "window_id": developer_window}])
        visual = self.run(request([visual_gallery, visual_developer], "visual-001", visual=True))
        self.passed("selected image and tree checkpoint", visual)
        assert int(visual["evidence"]["image_count"]) == 2, visual
        for case in visual["cases"]:
            manifest_path = Path(case["manifest"])
            manifest = json.loads(manifest_path.read_text())
            for evidence in case["checkpoints"]:
                image = evidence["image"]
                assert evidence["tree_artifact"] == image["tree"]["artifact_id"], evidence
                artifact = next(artifact for artifact in manifest["artifacts"]
                    if artifact["artifact_id"] == evidence["tree_artifact"])
                tree = json.loads((manifest_path.parent / artifact["path"]).read_text())
                for field in ("run_id", "app_tick", "window_id", "window_frame"):
                    assert tree[field] == image[field], (field, tree[field], image[field])
                assert tree["generation"] == image["tree"]["generation"]
                image_path = self.workspace / f"{case['case_id']}.png"
                self.command("artifact.image", {"capture": case["capture_id"], "image": image["image_id"],
                    "format": "png", "output": str(image_path)})
                encoded = image_path.read_bytes()
                assert encoded.startswith(b"\x89PNG\r\n\x1a\n")
                assert struct.unpack(">II", encoded[16:24]) == (image["width"], image["height"])
        self.results["offline_job_id"] = visual["job_id"]
        (self.workspace / "visual-report.json").write_text(json.dumps(visual, indent=2) + "\n")

        impossible = interaction_case("deliberate-failure", [{"step_id": "unpressed-button",
            "operation": "assert", "target": {"custom_id": "demo.primary_button"},
            "expect": ["target_pressed"]}], [{"checkpoint_id": "after-failed-observation",
                "trigger": "case_failed", "evidence": "image_and_tree"}])
        failed = self.run(request([impossible], "failure-001", visual=True))
        assert failed["verdict"] == "fail", failed
        assert int(failed["coverage"]["failed"]) > 0, failed
        failure_evidence = failed["cases"][0]["checkpoints"][0]
        assert failure_evidence["available"] and \
            failure_evidence["temporal_relation"] == "next_eligible_frame_after_failure", failed
        require_pass_failed = self.run(request([impossible], "failure-001", visual=True), require_pass=True, expected_code=7)
        assert require_pass_failed["job_id"] == failed["job_id"], require_pass_failed
        self.results["checks"].append({"name": "assertion failure preserves successful transport", "passed": True})

        # A long held-input case gives cancellation time to race with real frame delivery.
        held = interaction_case("cancel-held-input", [{"operation": "pointer.move",
            "target": {"custom_id": "demo.primary_button"}, "expect": ["target_hovered"]},
            {"operation": "pointer.down",
            "target": {"custom_id": "demo.primary_button"}},
            {"operation": "wait", "frames": "64", "minimum_duration_ns": "20000000000",
                "deadline_ns": "30000000000"}])
        held["workload"]["plan"]["deadline_ns"] = "30000000000"
        held["workload"]["plan"]["limits"].update({"maximum_frames": "2048",
            "maximum_spans": "2048", "maximum_observations": "2048"})
        cancellation = request([held], "cancel-001")
        cancellation["wait_timeout_ms"] = 0
        accepted = self.command("verification.run", {"request": cancellation})["result"]
        cancellation_deadline = time.monotonic() + 5
        while time.monotonic() < cancellation_deadline:
            progress = self.command("verification.status", {"job_id": accepted["job_id"],
                "after_sequence": accepted["sequence"], "wait_timeout_ms": 100})["result"]
            if progress["phase"] == "interaction":
                self.command("verification.status", {"job_id": accepted["job_id"],
                    "after_sequence": progress["sequence"], "wait_timeout_ms": 1000})
                break
            accepted["sequence"] = progress["sequence"]
        self.command("verification.cancel", {"job_id": accepted["job_id"]})
        canceled = self.report(accepted)
        assert canceled["execution_status"] == "canceled", canceled
        assert canceled["verdict"] != "pass", canceled
        assert canceled["cleanup"]["completed"], canceled
        canceled_manifest_path = Path(canceled["cases"][0]["manifest"])
        canceled_manifest = json.loads(canceled_manifest_path.read_text())
        inputs = [json.loads(line) for artifact in canceled_manifest["artifacts"]
            if artifact["artifact_id"].startswith("interaction_inputs")
            for line in (canceled_manifest_path.parent / artifact["path"]).read_text().splitlines()]
        assert any(record["effective_input"]["buttons_down"] for record in inputs), inputs
        assert any(record["cleanup"] and not record["effective_input"]["buttons_down"]
            for record in inputs), inputs
        after_cancel = interaction_case("post-cancellation-pointer-routing", [
            click("demo.primary_button", "activate-after-release")])
        self.passed("input usable after cancellation", self.run(request([after_cancel], "post-cancel-001")))

        samples: list[dict] = []
        self.results["measurements"]["soak_samples"] = samples
        for iteration in range(soak_count):
            started = time.monotonic()
            result = self.run(request([gallery], f"soak-{iteration:06d}"))
            self.passed(f"soak {iteration}", result)
            assert int(result["evidence"]["image_count"]) == 0, result
            assert int(result["resources"]["readback_reserved_bytes"]) == 0, result
            sample = {"iteration": iteration, "elapsed_seconds": time.monotonic() - started,
                "peak_owned_evidence_bytes": int(result["resources"]["peak_owned_evidence_bytes"]),
                "readback_reserved_bytes": int(result["resources"]["readback_reserved_bytes"])}
            if self.process and Path(f"/proc/{self.process.pid}/status").is_file():
                status = Path(f"/proc/{self.process.pid}/status").read_text()
                for line in status.splitlines():
                    if line.startswith("VmRSS:"):
                        sample["application_rss_bytes"] = int(line.split()[1]) * 1024
            samples.append(sample)
            if (iteration + 1) % 100 == 0:
                print(f"Soak {iteration + 1}/{soak_count} passed; "
                    f"owned evidence peak {sample['peak_owned_evidence_bytes']} bytes; "
                    f"RSS {sample.get('application_rss_bytes', 'unavailable')}", flush=True)
        if soak_count >= 256:
            expired = self.command("verification.run", {"request": smoke}, expected_code=6)
            assert expired["error"]["code"] == "result_expired", expired
            self.results["checks"].append({"name": "bounded terminal history preserves retry tombstone", "passed": True})
        self.results["measurements"] = {"soak_samples": samples,
            "retained_store_bytes": sum(path.stat().st_size for path in self.store.rglob("*") if path.is_file()),
            "retained_store_files": sum(1 for path in self.store.rglob("*") if path.is_file()),
            "rss_kind": "process resident memory; allocator retention is included"}

    def close(self) -> None:
        if self.process:
            self.process.terminate()
            try:
                self.process.wait(timeout=10)
            except subprocess.TimeoutExpired:
                self.process.kill()
                self.process.wait(timeout=5)
            self.process = None
        if os.name != "nt" and self.run_id:
            (Path(tempfile.gettempdir()) / f"flowui_{self.run_id}.sock").unlink(missing_ok=True)


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("cli", type=Path)
    parser.add_argument("demo", type=Path)
    parser.add_argument("--output-directory", type=Path,
        default=Path(__file__).resolve().parents[2] / ".flowui/agentic/verification/suites")
    parser.add_argument("--soak-count", type=int, default=4)
    arguments = parser.parse_args()
    if arguments.output_directory.resolve().is_relative_to(
            Path(__file__).resolve().parents[2] / "docs"):
        parser.error("Verification outputs belong under .flowui/agentic, outside docs")
    if os.name != "nt" and not os.environ.get("DISPLAY") and not os.environ.get("WAYLAND_DISPLAY"):
        return 77
    if arguments.soak_count < 0:
        parser.error("--soak-count must be nonnegative")
    workspace = arguments.output_directory.resolve() / ("run_" + uuid.uuid4().hex[:12])
    workspace.mkdir(parents=True, exist_ok=True)
    suite = VerificationSuite(arguments, workspace)
    try:
        suite.start()
        suite.execute(arguments.soak_count)
        suite.close()
        offline = suite.command("verification.report", {"job_id": suite.results["offline_job_id"]})["result"]
        assert offline["verdict"] == "pass" and offline["job_id"] == suite.results["offline_job_id"], offline
        suite.results["checks"].append({"name": "sealed report recovery after instance exit", "passed": True})
    finally:
        suite.close()
        (workspace / "verification-live-results.json").write_text(json.dumps(suite.results, indent=2) + "\n")
    print(f"FSEL verification suite passed ({len(suite.results['checks'])} checks); evidence: {workspace}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
