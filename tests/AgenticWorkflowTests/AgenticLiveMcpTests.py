#!/usr/bin/env python3
"""Exercise every advertised MCP operation through stdio against one real FSEL demo."""
from __future__ import annotations

import argparse
import asyncio
import base64
from datetime import datetime, timezone
import hashlib
import json
import math
import os
from pathlib import Path
import shutil
import stat
import struct
import subprocess
import sys
import tempfile
import time
import traceback
import uuid


EXPECTED_OPERATIONS = {
    "capabilities", "instances", "status", "context.find", "context.read",
    "capture.plan", "capture.start", "capture.stop", "job.status", "job.cancel",
    "scenario.list", "scenario.run", "query.performance", "query.diagnostics",
    "query.memory", "query.visual", "artifact.list", "artifact.read", "artifact.image",
    "compare", "objective.check", "visual.compare", "changes.preview", "changes.apply",
    "interaction.capabilities", "interaction.targets", "interaction.plan", "interaction.start",
    "interaction.record.start", "interaction.record.stop", "interaction.query", "action.list",
    "action.invoke",
    "verification.plan", "verification.run", "verification.status", "verification.report", "verification.cancel",
}
TERMINAL_PHASES = {"sealed", "failed", "canceled"}


def sha256_file(path: Path) -> str:
    digest = hashlib.sha256()
    with path.open("rb") as source:
        while block := source.read(1024 * 1024):
            digest.update(block)
    return digest.hexdigest()


def write_json(path: Path, value: object) -> None:
    path.write_text(json.dumps(value, indent=2, ensure_ascii=False) + "\n")


class LiveMcpSuite:
    def __init__(self, arguments: argparse.Namespace):
        self.cli = arguments.cli.resolve(strict=True)
        self.demo = arguments.demo.resolve(strict=True)
        self.repository = arguments.workspace.resolve(strict=True)
        output = arguments.output_directory.resolve()
        output.mkdir(parents=True, exist_ok=True)
        identifier = datetime.now(timezone.utc).strftime("%Y%m%dT%H%M%SZ_") + uuid.uuid4().hex[:8]
        self.output = output / ("run_" + identifier)
        self.output.mkdir(mode=0o700)
        self.workspace = self.output / "workspace"
        self.workspace.mkdir()
        self.store = self.workspace / ".flowui/agentic/v1"
        self.changes = self.workspace / ".flowui/changes/active.flowchanges"
        self.calls_path = self.output / "calls.jsonl"
        self.process: subprocess.Popen | None = None
        self.client = None
        self.run_id = ""
        self.window_id = ""
        self.visual_capture = ""
        self.endpoint: Path | None = None
        self.timeout = arguments.timeout
        self.owners: dict[str, str] = {}
        self.operations: dict[str, dict] = {}
        self.tool_schemas: dict[str, dict] = {}
        self.calls: list[dict] = []
        self.stages: list[dict] = []
        self.failures: list[dict] = []
        self.jobs: set[str] = set()
        self.captures: dict[str, dict] = {}
        self.capabilities: dict = {}
        self.user_changes = self.repository / ".flowui/changes/active.flowchanges"
        self.user_changes_before = self.fingerprint(self.user_changes)
        self.passes: list[dict] = []

    @staticmethod
    def fingerprint(path: Path) -> dict:
        return {"exists": path.exists(), "sha256": sha256_file(path) if path.is_file() else None}

    def prepare(self) -> None:
        if os.name != "nt" and not os.environ.get("DISPLAY") and not os.environ.get("WAYLAND_DISPLAY"):
            raise RuntimeError("A real display is required; this integration suite never skips GUI execution")
        pack = self.repository / "include/devSystems/devAgenticWorkflow/knowledge"
        source_map = json.loads((pack / "source-map.json").read_text())
        for selector in source_map["selectors"]:
            relative = Path(selector["file"])
            if relative.is_absolute() or ".." in relative.parts:
                raise AssertionError("Unsafe source-map fixture path")
            source = self.repository / relative
            if sha256_file(source) != selector["sha256"]:
                raise AssertionError(f"Knowledge source digest is stale: {relative}")
            destination = self.workspace / relative
            destination.parent.mkdir(parents=True, exist_ok=True)
            shutil.copy2(source, destination)
        shutil.copytree(pack, self.workspace / "include/devSystems/devAgenticWorkflow/knowledge")
        self.changes.parent.mkdir(parents=True, exist_ok=True)
        seed = {"manifestVersion": 2, "schemaFingerprint": "0x0000000000000000",
            "buildFingerprint": "", "createdTimestamp": "", "bakedChanges": [],
            "memoryCapacity": {"version": 1, "enabled": True, "targets": []}}
        write_json(self.changes, seed)
        write_json(self.output / "changes-before.json", seed)

    async def start_demo(self) -> None:
        log = (self.output / "demo.log").open("wb")
        try:
            self.process = subprocess.Popen([str(self.demo), "--agentic-store", str(self.store),
                "--agentic-visual", "--agentic-input", "--agentic-actions",
                "--agentic-changes", str(self.changes)], cwd=self.workspace,
                stdin=subprocess.DEVNULL, stdout=log, stderr=subprocess.STDOUT)
        finally:
            log.close()
        deadline = time.monotonic() + 30
        while time.monotonic() < deadline:
            self.ensure_demo()
            descriptors = list((self.store / "runtime/instances").glob("*.json"))
            if descriptors:
                if len(descriptors) != 1:
                    raise AssertionError("The isolated suite must own exactly one App process")
                descriptor = json.loads(descriptors[0].read_text())
                if int(descriptor["pid"]) != self.process.pid:
                    raise AssertionError("Instance descriptor belongs to a different process")
                self.run_id = descriptor["run_id"]
                if os.name != "nt":
                    endpoint = Path(descriptor["endpoint"])
                    expected = Path(tempfile.gettempdir()) / f"flowui_{self.run_id}.sock"
                    assert endpoint == expected, "Unexpected local endpoint ownership path"
                    self.endpoint = endpoint
                return
            await asyncio.sleep(0.05)
        raise AssertionError("The real demo did not publish its authenticated local endpoint")

    def ensure_demo(self) -> None:
        if self.process is not None and self.process.poll() is not None:
            tail = (self.output / "demo.log").read_text(errors="replace")[-4000:]
            raise AssertionError(f"Demo exited {self.process.returncode}: {tail}")

    async def discover(self) -> None:
        from importlib.metadata import version
        self.sdk_version = version("mcp")
        declared = await self.client.list_tools()
        tools = declared.tools if hasattr(declared, "tools") else declared
        for tool in tools:
            schema = tool.input_schema
            self.tool_schemas[tool.name] = schema
            for operation in schema["properties"]["operation"]["enum"]:
                if operation in self.owners:
                    raise AssertionError(f"Ambiguous operation ownership: {operation}")
                self.owners[operation] = tool.name
                self.operations[operation] = {"tool": tool.name, "calls": 0,
                    "successful_results": 0, "expected_failures": 0, "failed_checks": 0}
            if tool.name in {"flowui_interaction", "flowui_action", "flowui_changes_apply",
                    "flowui_verification_run", "flowui_verification_cancel"}:
                assert not tool.annotations.read_only_hint, tool.name
        assert len(tools) == 16, f"Unexpected tool count: {len(tools)}"
        assert set(self.owners) == EXPECTED_OPERATIONS, (set(self.owners) ^ EXPECTED_OPERATIONS)
        write_json(self.output / "discovery.json", {"sdk_version": self.sdk_version,
            "tools": {tool.name: {"description": tool.description, "input_schema": tool.input_schema}
                      for tool in tools}})

    async def call(self, operation: str, arguments: dict | None = None, *,
                   live: bool = False, failure: bool = False, exits: tuple[int, ...] = (0,)) -> dict:
        self.ensure_demo()
        values = dict(arguments or {})
        if live:
            values["instance"] = self.run_id
        tool = self.owners[operation]
        record = {"sequence": len(self.calls) + 1, "tool": tool, "operation": operation,
            "arguments": values, "expected_failure": failure, "started_ns": str(time.monotonic_ns())}
        self.calls.append(record)
        coverage = self.operations[operation]
        coverage["calls"] += 1
        try:
            result = await self.client.call_tool(tool, {"operation": operation, "arguments": values})
            envelope = result.structured_content
            assert isinstance(envelope, dict), "Missing structured MCP result"
            record["envelope"] = envelope
            assert envelope["schema"] == "FlowUi.Agentic.CommandResult"
            assert envelope["schema_version"] == "1.0" and envelope["request_id"]
            assert envelope["operation"] == operation
            text = [item.text for item in result.content if item.type == "text"]
            assert len(text) == 1 and json.loads(text[0]) == envelope, "Text/structured result mismatch"
            assert bool(result.is_error) == (envelope["operation_status"] in {"failed", "canceled", "timeout"})
            if failure:
                assert result.is_error and envelope.get("exit_code", 0) != 0, envelope
                assert envelope.get("error", {}).get("code"), envelope
                assert envelope["error"]["code"] not in {"adapter_execution_failed", "invalid_cli_result"}, envelope
            else:
                assert not result.is_error, envelope
                assert envelope.get("exit_code", 0) in exits, envelope
            image_paths = []
            for image_index, item in enumerate(item for item in result.content if item.type == "image"):
                payload = base64.b64decode(item.data, validate=True)
                assert item.mime_type == "image/png" and payload.startswith(b"\x89PNG\r\n\x1a\n")
                width, height = struct.unpack(">II", payload[16:24])
                assert 0 < width * height <= 16_777_216 and len(payload) <= 4 * 1024 * 1024
                path = self.output / f"call-{record['sequence']:04d}-image-{image_index}.png"
                path.write_bytes(payload)
                image_paths.append({"path": str(path.relative_to(self.output)), "width": width,
                    "height": height, "bytes": len(payload), "sha256": hashlib.sha256(payload).hexdigest()})
            record["images"] = image_paths
            record["passed"] = True
            coverage["expected_failures" if failure else "successful_results"] += 1
            if not failure and envelope["operation_status"] == "accepted" and isinstance(envelope["result"], dict):
                if job_id := envelope["result"].get("job_id"):
                    self.jobs.add(job_id)
            return envelope
        except Exception as error:
            coverage["failed_checks"] += 1
            record["passed"] = False
            record["failure"] = str(error)
            raise
        finally:
            record["completed_ns"] = str(time.monotonic_ns())
            with self.calls_path.open("a") as log:
                log.write(json.dumps(record, ensure_ascii=False) + "\n")

    async def result(self, operation: str, arguments: dict | None = None, **options) -> dict:
        return (await self.call(operation, arguments, **options))["result"]

    async def stage(self, name: str, function) -> None:
        started = len(self.calls)
        try:
            await function()
            self.stages.append({"name": name, "passed": True, "calls": len(self.calls) - started})
        except Exception as error:
            failure = {"stage": name, "error": str(error), "traceback": traceback.format_exc()}
            self.failures.append(failure)
            if len(self.calls) > started:
                self.operations[self.calls[-1]["operation"]]["failed_checks"] += 1
            self.stages.append({"name": name, "passed": False, "calls": len(self.calls) - started})
            print(f"FAIL {name}: {error}", file=sys.stderr, flush=True)
            await self.retire_jobs()
        self.save_report()

    async def wait_job(self, job_id: str, *, canceled: bool = False) -> dict:
        self.jobs.add(job_id)
        deadline = time.monotonic() + self.timeout
        while time.monotonic() < deadline:
            status = await self.result("job.status", {"job_id": job_id}, live=True)
            if status["phase"] in TERMINAL_PHASES:
                self.jobs.discard(job_id)
                if canceled:
                    assert status["phase"] in {"canceled", "sealed"} and status["cancellation_requested"], status
                else:
                    assert status["phase"] == "sealed", status
                return status
            await asyncio.sleep(0.1)
        raise AssertionError(f"Job did not reach a terminal phase: {job_id}")

    async def wait_recording(self, job_id: str) -> dict:
        self.jobs.add(job_id)
        deadline = time.monotonic() + self.timeout
        while time.monotonic() < deadline:
            status = await self.result("job.status", {"job_id": job_id}, live=True)
            assert status["phase"] not in TERMINAL_PHASES, status
            if status["phase"] == "recording" and int(status["measured_ticks"]) >= 2:
                return status
            await asyncio.sleep(0.1)
        raise AssertionError("Capture never entered a measured recording interval")

    def manifest(self, status: dict) -> dict:
        path = Path(status["manifest_path"]).resolve(strict=True)
        assert path.is_relative_to(self.store.resolve()), "Manifest escaped the owned store"
        document = json.loads(path.read_text())
        assert document["run_id"] == self.run_id and document["capture_id"] == status["capture_id"]
        for artifact in document["artifacts"]:
            payload = (path.parent / artifact["path"]).resolve(strict=True)
            assert payload.is_relative_to(path.parent) and not (path.parent / artifact["path"]).is_symlink()
            assert payload.stat().st_size == int(artifact["bytes"]), artifact
            assert sha256_file(payload) == artifact["sha256"], artifact
        self.captures[document["capture_id"]] = {"manifest_path": str(path),
            "manifest_sha256": sha256_file(path), "artifact_count": len(document["artifacts"]),
            "status": document["status"], "coverage": document["coverage"]}
        return document

    async def capture_quality(self, manifest: dict) -> dict:
        artifact = await self.result("artifact.read", {"capture": manifest["capture_id"],
            "artifact": "capture_quality", "max_bytes": 262144})
        quality = json.loads(artifact["content"])
        assert quality["schema"] == "FlowUi.Agentic.Quality" and artifact["sha256"]
        assert quality["complete"] == (manifest["status"] == "sealed"), quality
        for section, disposition in manifest["coverage"].items():
            entry = quality["sections"][section]
            assert entry["status"] == disposition
            if disposition == "partial":
                assert not quality["complete"]
                assert entry["reasons"] or entry["uncertain"] or any(int(entry[field]) > 0
                    for field in ["dropped_records", "evicted_records", "pending_records"]), entry
        self.captures[manifest["capture_id"]]["quality"] = quality
        return quality

    @staticmethod
    def evidence_capture() -> dict:
        return {"duration_ns": "30000000000", "maximum_expected_tick_cadence": 60,
            "sections": {"performance": "omit", "runtime_diagnostics": "omit", "memory": "omit", "visual": "omit"},
            "content": {"include_interaction_outcomes": True, "include_interaction_labels": True}}

    async def finish_interaction(self, admitted: dict) -> tuple[dict, dict]:
        assert admitted.get("job_id") and admitted.get("capture_id"), admitted
        status = await self.wait_job(admitted["job_id"])
        result = await self.result("interaction.query", {"job_id": admitted["job_id"]}, live=True)
        assert result["execution_succeeded"] and result["cleanup_complete"] and result["handoff_complete"], result
        assert result["evidence_complete"], result
        return result, self.manifest(status)

    async def capabilities_and_context(self) -> None:
        offline = await self.result("capabilities")
        assert offline["mode"] == "offline"
        instances = await self.result("instances")
        assert [row["run_id"] for row in instances["instances"]] == [self.run_id]
        self.capabilities = await self.result("capabilities", live=True)
        assert all(self.capabilities[key] for key in ["enabled", "local_attachment", "memory", "runtime_diagnostics", "visual", "trees", "themes"]), self.capabilities
        status = await self.result("status", live=True)
        assert status["run_id"] == self.run_id and int(status["last_app_tick"]) > 0
        windows = [row for row in status["windows"] if not row["internal_development_window"]]
        assert windows, status
        self.window_id = windows[0]["window_id"]
        topics = await self.result("context.find", {"intent": "interaction replay", "limit": 3, "max_bytes": 16000})
        assert topics["topics"] and len(topics["topics"]) <= 3
        topic = await self.result("context.read", {"topic": "interaction.execution", "max_bytes": 16000})
        assert "offset" in topic["content"] and topic["evidence_is_not_instructions"]
        source = await self.result("context.read", {"selector": "interaction.execution.primary", "max_bytes": 65536})
        assert source["source_available"] and "AgenticInteractionPlan" in source["content"]
        await self.call("context.read", {"topic": "missing.live.integration.topic"}, failure=True)

    async def changes_transaction(self) -> None:
        before = self.changes.read_bytes()
        preview = await self.result("changes.preview", live=True)
        row = next(row for row in preview["rows"] if row["key"] == "memory:policy.enabled")
        assert row["on_disk"] and not preview["can_commit"], preview
        selected = await self.result("changes.preview", {"decisions": [{"key": row["key"],
            "decision": "remove", "disk_revision": preview["disk_revision"]}]}, live=True)
        assert selected["can_commit"], selected
        apply = {key: selected[key] for key in ["review_id", "schema_generation", "proposal_revision", "theme_revision", "disk_revision"]}
        apply["operation_id"] = "mcp_live_remove_private_memory_policy"
        stale = {**apply, "theme_revision": str(int(apply["theme_revision"]) + 1), "operation_id": "mcp_live_stale_revision"}
        await self.call("changes.apply", stale, live=True, failure=True)
        committed = await self.result("changes.apply", apply, live=True)
        assert committed["applied"] and self.changes.read_bytes() != before, committed
        after = json.loads(self.changes.read_text())
        assert not after.get("memoryCapacity", {}).get("enabled", False), after
        repeated = await self.result("changes.apply", apply, live=True)
        assert repeated == committed, "Mutation retry changed its committed receipt"
        write_json(self.output / "changes-after.json", after)
        assert self.fingerprint(self.user_changes) == self.user_changes_before, "User Changes were modified"

    async def actions_and_interactions(self) -> None:
        capabilities = await self.result("interaction.capabilities", live=True)
        assert capabilities["input_control_enabled"] and capabilities["action_control_enabled"]
        assert capabilities["maximum_windows_per_plan"] == 1
        discovered = await self.result("interaction.targets", {"window_id": self.window_id, "limit": 10, "max_bytes": 262144}, live=True)
        assert discovered["targets"] and discovered["geometry_available"], discovered
        await self.call("interaction.targets", {"window_id": self.window_id, "limit": 100, "max_bytes": 1024}, live=True, failure=True)
        target = await self.result("interaction.targets", {"window_id": self.window_id,
            "target": {"custom_id": "demo.primary_button"}}, live=True)
        assert target["selector"]["custom_id"] == "demo.primary_button" and target["visible_region"]["width"] > 0
        exports = await self.result("action.list", {"limit": 100}, live=True)
        assert "demo.configure" in json.dumps(exports) and "demo.reset" in json.dumps(exports)
        configured = await self.result("action.invoke", {"action_id": "demo.configure", "revision": "1",
            "arguments": {"page": "writing", "theme": "agentic-ocean", "editor_soft_wrap": False},
            "idempotency_key": "mcp_live_configure_writing", "capture": self.evidence_capture()}, live=True)
        outcome, _ = await self.finish_interaction(configured)
        assert outcome["action"]["completed"] and outcome["action"]["completion_succeeded"]
        assert outcome["action"]["result"]["page"] == "writing" and not outcome["action"]["result"]["editor_soft_wrap"]
        reset = await self.result("action.invoke", {"action_id": "demo.reset", "revision": "1", "arguments": {},
            "idempotency_key": "mcp_live_reset_gallery", "capture": self.evidence_capture()}, live=True)
        await self.finish_interaction(reset)
        plan = json.loads((self.repository / "tests/demo/agentic-click.json").read_text())
        plan["window_id"] = self.window_id
        compiled = await self.result("interaction.plan", {"plan": plan, "max_bytes": 65536})
        assert compiled["compiled"]["requires_geometry"] and int(compiled["compiled"]["frame_count"]) > 2
        clicked = await self.result("interaction.start", {"plan": plan, "capture": self.evidence_capture(),
            "idempotency_key": "mcp_live_custom_click", "max_bytes": 1024}, live=True)
        outcome, manifest = await self.finish_interaction(clicked)
        assert outcome["assertion_verdict"] == "passed", outcome
        observations = await self.result("interaction.query", {"job_id": clicked["job_id"],
            "kind": "observations", "limit": 100, "max_bytes": 262144}, live=True)
        assert any(int(row.get("target_observation", {}).get("application_outcome", {}).get("button_activations", "0")) > 0
                   for row in observations["records"]), observations
        inputs = await self.result("interaction.query", {"capture": manifest["capture_id"], "kind": "inputs", "limit": 100})
        assert inputs["records"] and all(row["run_id"] == self.run_id for row in inputs["records"])
        retained = next(row for row in manifest["artifacts"] if row["artifact_id"] == "interaction_plan")
        replay = {"plan_artifact": {"capture": manifest["capture_id"], "artifact": retained["artifact_id"], "sha256": retained["sha256"]},
            "rebind_run": True, "capture": self.evidence_capture(), "idempotency_key": "mcp_live_stored_replay"}
        replayed = await self.result("interaction.start", replay, live=True)
        replay_result, _ = await self.finish_interaction(replayed)
        assert replay_result["plan_digest"] == outcome["plan_digest"]
        bad_replay = {**replay, "idempotency_key": "mcp_live_bad_source_hash", "plan_artifact": {**replay["plan_artifact"], "sha256": "0" * 64}}
        await self.call("interaction.start", bad_replay, live=True, failure=True)
        navigation = {"schema": "FlowUi.Agentic.InteractionPlan", "schema_version": "1.0",
            "mode": "common_controls", "window_id": self.window_id, "deadline_ns": "30000000000",
            "content": {"persistence": "retain_text", "record_observations": True}, "steps": [
                {"operation": "click", "target": {"custom_id": "demo.navigate"}, "deadline_ns": "10000000000",
                    "expect": ["target_hovered", "target_pressed", "target_released"]},
                {"operation": "click", "target": {"custom_id": "demo.editor_body"}, "deadline_ns": "10000000000",
                    "expect": ["target_hovered", "target_pressed", "target_released", "text_focused"]},
                {"operation": "text.enter", "target": {"custom_id": "demo.editor_body"}, "text": "MCP live input",
                    "deadline_ns": "10000000000", "expect": ["text_focused"]}]}
        navigated = await self.result("interaction.start", {"plan": navigation, "capture": self.evidence_capture(),
            "idempotency_key": "mcp_live_navigation_and_text"}, live=True)
        await self.finish_interaction(navigated)
        edited = await self.result("interaction.query", {"job_id": navigated["job_id"], "kind": "observations",
            "limit": 100, "max_bytes": 262144}, live=True)
        assert any(int(row.get("target_observation", {}).get("application_outcome", {}).get("document_changes", "0")) > 0
                   for row in edited["records"]), edited
        recording = await self.result("interaction.record.start", {"window_id": self.window_id,
            "max_frames": "240", "deadline_ns": "10000000000", "include_text": False,
            "idempotency_key": "mcp_live_record_effective_input"}, live=True)
        self.jobs.add(recording["job_id"])
        await asyncio.sleep(0.4)
        await self.call("interaction.record.stop", {"job_id": recording["job_id"]}, live=True)
        recorded, _ = await self.finish_interaction(recording)
        assert 0 < int(recorded["coverage"]["consumed"]) < 240, recorded
        await self.call("interaction.plan", {"plan": {**plan, "script": "unregistered callback"}}, failure=True)

    async def capture_and_queries(self) -> None:
        request = {"profile": "visual_inspect", "mode": "windowed", "window_ids": [self.window_id],
            "duration_ns": "10000000000", "measurement_ticks": "4", "maximum_expected_tick_cadence": 60,
            "sections": {"performance": "required", "runtime_diagnostics": "omit", "memory": "omit", "visual": "required"},
            "visual_sources": {"window": True, "viewports": False}, "limits": {"maximum_images": "4"},
            "idempotency_key": "mcp_live_combined_capture"}
        planned = await self.result("capture.plan", request, live=True)
        assert planned["feasible"] and not planned["rejected_reasons"], planned
        unsupported = await self.result("capture.plan", {**request, "mode": "offscreen_vulkan"}, live=True)
        assert not unsupported["feasible"] and unsupported["rejected_reasons"], unsupported
        admitted = await self.result("capture.start", request, live=True)
        status = await self.wait_job(admitted["job_id"])
        manifest = self.manifest(status)
        self.visual_capture = manifest["capture_id"]
        assert all(manifest["coverage"][key] == "complete" for key in ["performance", "visual"]), manifest["coverage"]
        inventory = await self.result("artifact.list", {"capture": self.visual_capture, "max_bytes": 262144})
        assert len(inventory["artifacts"]) == len(manifest["artifacts"])
        quality = await self.capture_quality(manifest)
        assert int(quality["sections"]["performance"]["generation"]) > 0
        timing = await self.result("query.performance", {"capture": self.visual_capture,
            "artifact": "performance_timing", "limit": 4, "max_bytes": 262144})
        assert timing["records"], "Explicit performance collection in the visual profile produced no native CSV records"
        frames = await self.result("query.visual", {"capture": self.visual_capture, "kind": "frames", "limit": 100, "max_bytes": 262144})
        assert frames["records"] and all(row["capture_id"] == self.visual_capture for row in frames["records"])
        artifact_ids = {artifact["artifact_id"] for artifact in manifest["artifacts"]}
        assert any(frame.get("tree") for frame in frames["records"]), "No captured image retained a tree join"
        assert any(frame.get("theme") for frame in frames["records"]), "No captured image retained a theme join"
        for frame in frames["records"]:
            assert frame["run_id"] == self.run_id and frame["window_id"] == self.window_id
            if frame.get("tree"):
                assert frame["tree"]["artifact_id"] in artifact_ids
            if frame.get("theme"):
                assert frame["theme"]["artifact_id"] in artifact_ids
        trees = await self.result("query.visual", {"capture": self.visual_capture, "kind": "trees", "limit": 2})
        assert trees["records"] and all(row["schema"] == "FlowUi.Agentic.Tree" and "image_id" not in row for row in trees["records"])
        subtree = await self.result("query.visual", {"capture": self.visual_capture, "kind": "subtree",
            "tree": trees["records"][0]["artifact_id"], "flow_node": {"node_index": 0}, "max_bytes": 1048576})
        assert subtree["flow_nodes"], subtree
        theme_query = {"capture": self.visual_capture, "kind": "themes", "limit": 5, "max_bytes": 65536}
        themes = await self.result("query.visual", theme_query)
        assert themes["records"] and len(themes["records"]) <= 5 and themes["cursor"]
        assert all(row["variant"] and row["path"] and row["run_id"] == self.run_id for row in themes["records"])
        next_themes = await self.result("query.visual", {**theme_query, "cursor": themes["cursor"]})
        row_identity = lambda row: (row["variant"], row["variant_type_id"], row["path"], row["field_id"])
        assert not ({row_identity(row) for row in themes["records"]} & {row_identity(row) for row in next_themes["records"]})
        await self.call("query.visual", {**theme_query, "cursor": themes["cursor"], "filter": {"variant": "agentic-default"}}, failure=True)
        overlay_fields = []
        for path in ["textInputTheme.overlayStyle", "numberInputTheme.field.overlayStyle", "dragValueTheme.field.overlayStyle", "textAreaTheme.overlayStyle"]:
            selected = await self.result("query.visual", {**theme_query, "filter": {"variant": "agentic-default", "path": path}})
            assert len(selected["records"]) == 1 and selected["records"][0]["path"] == path, selected
            assert selected["records"][0]["variant"] == "agentic-default"
            overlay_fields.extend(selected["records"])
        for field in overlay_fields:
            value = field["effective"]
            assert "caretShape" in value and value.get("available") is not False, field
            for name in ["caretThicknessPx", "caretBlockWidthPx", "caretHeightOverflowTopPx", "caretHeightOverflowBottomPx",
                         "caretBlinkPeriodSeconds", "caretBlinkVisibleSeconds"]:
                assert isinstance(value[name], (float, int)) and math.isfinite(value[name]), field
            for name in ["caretColor", "selectionBoxColor", "selectedTextColor"]:
                assert all(isinstance(value[name][channel], (float, int)) and math.isfinite(value[name][channel])
                           for channel in ["r", "g", "b", "a"]), field
        catalogue = await self.result("query.visual", {"capture": self.visual_capture, "kind": "catalogue", "max_bytes": 1048576})
        assert catalogue["records"]

    async def artifact_images(self) -> None:
        assert self.visual_capture, "The real visual capture prerequisite did not complete"
        frames = await self.result("query.visual", {"capture": self.visual_capture, "kind": "frames", "limit": 1})
        assert frames["records"], frames
        image = frames["records"][0]["image_id"]
        inline = await self.result("artifact.image", {"capture": self.visual_capture, "image": image,
            "crop": {"x": 0, "y": 0, "width": 64, "height": 64}, "include_image": True})
        image_record = self.calls[-1]["images"][0]
        assert image_record["width"] == 64 and image_record["height"] == 64
        assert "output" not in inline and inline["image_delivery"] == "inline_png"
        assert inline["artifact_selector"] == {"capture": self.visual_capture, "image": image}
        oracle_path = self.output / "cli-image-oracle.png"
        oracle_arguments = {"capture": self.visual_capture, "image": image, "format": "png",
            "crop": {"x": 0, "y": 0, "width": 64, "height": 64}, "output": str(oracle_path)}
        oracle = await asyncio.create_subprocess_exec(str(self.cli), "artifact", "image", "--workspace", str(self.workspace),
            "--json", "--arguments-json", json.dumps(oracle_arguments), cwd=self.workspace,
            stdin=asyncio.subprocess.DEVNULL, stdout=asyncio.subprocess.PIPE, stderr=asyncio.subprocess.PIPE)
        try:
            stdout, stderr = await asyncio.wait_for(oracle.communicate(), 30)
        finally:
            if oracle.returncode is None:
                oracle.kill()
                await oracle.wait()
        assert oracle.returncode == 0 and len(stdout) <= 1048576 and len(stderr) <= 16384, (stdout, stderr)
        oracle_envelope = json.loads(stdout)
        write_json(self.output / "cli-image-oracle.json", oracle_envelope)
        assert sha256_file(oracle_path) == image_record["sha256"]
        normalized = lambda value: {key: member for key, member in value.items() if key not in {"output", "image_delivery", "artifact_selector"}}
        assert normalized(oracle_envelope["result"]) == normalized(inline), "MCP/CLI image metadata differ"
        metadata_only = await self.result("artifact.image", {"capture": self.visual_capture, "image": image, "include_image": False})
        assert not self.calls[-1]["images"]
        assert "output" not in metadata_only and metadata_only["image_delivery"] == "metadata_only_use_artifact_image_to_retrieve"
        difference = await self.result("visual.compare", {"baseline": self.visual_capture, "candidate": self.visual_capture,
            "image": image, "candidate_image": image, "tolerance": 0})
        assert int(difference["changed_pixels"]) == 0 and difference["maximum_channel_difference"] == 0, difference
        await self.call("artifact.read", {"capture": self.visual_capture, "artifact": "missing_artifact"}, failure=True)

    async def diagnostics_and_memory(self) -> None:
        diagnostic_request = {"profile": "diagnostics_reproduce", "mode": "windowed", "window_ids": [self.window_id],
            "measurement_ticks": "8", "duration_ns": "10000000000", "maximum_expected_tick_cadence": 60,
            "sections": {"performance": "omit", "runtime_diagnostics": "required", "memory": "omit", "visual": "omit"},
            "idempotency_key": "mcp_live_runtime_diagnostics"}
        diagnostic_job = await self.result("capture.start", diagnostic_request, live=True)
        diagnostic = self.manifest(await self.wait_job(diagnostic_job["job_id"]))
        assert diagnostic["coverage"]["runtime_diagnostics"] in {"complete", "partial"}, diagnostic["coverage"]
        await self.capture_quality(diagnostic)
        records = await self.result("query.diagnostics", {"capture": diagnostic["capture_id"], "domain": "runtime", "limit": 100, "max_bytes": 262144})
        assert isinstance(records["records"], list)
        assert any(artifact["path"] == "diagnostics/runtime.json" for artifact in diagnostic["artifacts"])
        await self.call("query.diagnostics", {"capture": diagnostic["capture_id"], "domain": "build"}, failure=True)
        memory_request = {"profile": "memory_diagnose", "mode": "windowed", "window_ids": [self.window_id],
            "duration_ns": "2000000000", "maximum_expected_tick_cadence": 240,
            "sections": {"performance": "omit", "runtime_diagnostics": "omit", "memory": "required", "visual": "omit"},
            "idempotency_key": "mcp_live_memory_samples"}
        memory_job = await self.result("capture.start", memory_request, live=True)
        retained = self.manifest(await self.wait_job(memory_job["job_id"]))
        assert retained["coverage"]["memory"] in {"complete", "partial"}, retained["coverage"]
        await self.capture_quality(retained)
        memory_capture = retained["capture_id"]
        memory = await self.result("query.memory", {"capture": memory_capture, "kind": "samples", "limit": 100, "max_bytes": 262144})
        assert memory["records"] and any("logical_live_bytes" in row and "backing_bytes" in row for row in memory["records"]), memory
        interval = next(row for row in memory["records"] if int(row["end_app_tick_exclusive"]) > int(row["first_app_tick"]))
        identity_fields = ["source_id", "window_id", "sequence", "epoch"]
        same_interval = lambda row: all(row.get(key) == interval[key] for key in identity_fields)
        interval_query = {"capture": memory_capture, "kind": "samples", "source_id": interval["source_id"], "limit": 100, "max_bytes": 262144}
        at_start = await self.result("query.memory", {**interval_query, "app_tick": interval["first_app_tick"]})
        assert any(same_interval(row) for row in at_start["records"]), at_start
        overlap = await self.result("query.memory", {**interval_query, "filter": {
            "first_app_tick": interval["first_app_tick"], "end_app_tick_exclusive": interval["end_app_tick_exclusive"]}})
        assert any(same_interval(row) for row in overlap["records"]), overlap
        at_end = await self.result("query.memory", {**interval_query, "app_tick": interval["end_app_tick_exclusive"]})
        assert not any(same_interval(row) for row in at_end["records"]), at_end

    async def scenarios_and_analysis(self) -> None:
        scenarios = await self.result("scenario.list", live=True)
        assert any(row["scenario_id"] == "demo.gallery.steady" for row in scenarios["scenarios"]), scenarios
        for index in range(2):
            request = {"scenario_id": "demo.gallery.steady", "profile": "performance_verify", "mode": "windowed",
                "window_ids": [self.window_id], "measurement_ticks": "24", "warmup_ticks": "4", "duration_ns": "3000000000",
                "sections": {"performance": "required", "runtime_diagnostics": "omit", "memory": "omit", "visual": "omit"},
                "seed": "17", "idempotency_key": f"mcp_live_comparable_pass_{index}"}
            admitted = await self.result("scenario.run", request, live=True)
            status = await self.wait_job(admitted["job_id"])
            self.passes.append(self.manifest(status))
        assert self.passes[0]["capture_id"] != self.passes[1]["capture_id"]
        measured = await self.result("query.performance", {"capture": self.passes[0]["capture_id"], "window": self.window_id, "top": 5, "max_bytes": 262144})
        assert measured["windows"] and int(measured["windows"][0]["observed_frames"]) > 1, measured
        comparison = await self.result("compare", {"baseline": self.passes[0]["capture_id"], "candidate": self.passes[1]["capture_id"]})
        assert comparison["compatibility"] == "compatible", comparison
        objective = {"metric": "window_present_call_cadence_fps", "operator": "gt", "threshold": 0.001,
            "minimum_valid_intervals": "4", "minimum_duration_ns": "1", "minimum_passes": 2}
        accepted = await self.call("objective.check", {"passes": [item["capture_id"] for item in self.passes],
            "objective": objective, "require_pass": True, "max_bytes": 262144}, exits=(0, 6))
        verdict = accepted["result"]["verdict"]
        exclusions = any(int(item.get("metrics", {}).get("excluded_frames", "0")) for item in accepted["result"]["passes"])
        if exclusions:
            assert verdict == "inconclusive" and accepted["exit_code"] == 6, accepted
            assert "observer_or_interruption_exclusions" in json.dumps(accepted["result"]["passes"])
        else:
            assert verdict == "pass" and accepted["exit_code"] == 0, accepted
        duplicate = await self.result("objective.check", {"passes": [self.passes[0]["capture_id"]] * 2, "objective": objective})
        assert duplicate["verdict"] == "inconclusive" and "duplicate_pass_identity" in json.dumps(duplicate)

    async def stopping_and_cancellation(self) -> None:
        request = {"profile": "performance_diagnose", "mode": "windowed", "window_ids": [self.window_id],
            "duration_ns": "10000000000", "sections": {"performance": "required", "runtime_diagnostics": "omit", "memory": "omit", "visual": "omit"},
            "idempotency_key": "mcp_live_explicit_stop"}
        stop_job = await self.result("capture.start", request, live=True)
        await self.wait_recording(stop_job["job_id"])
        await self.call("capture.stop", {"job_id": stop_job["job_id"]}, live=True)
        stopped = self.manifest(await self.wait_job(stop_job["job_id"]))
        elapsed = int(stopped["measurement"]["end_monotonic_ns"]) - int(stopped["measurement"]["begin_monotonic_ns"])
        assert 0 < elapsed < 9_000_000_000, "capture.stop did not shorten the requested ten-second capture"
        cancel_job = await self.result("capture.start", {**request, "idempotency_key": "mcp_live_explicit_cancel"}, live=True)
        self.jobs.add(cancel_job["job_id"])
        await self.call("job.cancel", {"job_id": cancel_job["job_id"]}, live=True)
        await self.wait_job(cancel_job["job_id"], canceled=True)
        await self.call("job.status", {"job_id": "job_missing_mcp_live"}, live=True, failure=True)

    async def verification_run_and_report(self) -> None:
        suite = {"schema": "FlowUi.Agentic.VerificationRequest", "schema_version": "1.0",
            "suite_id": "mcp.fsel.smoke", "suite_revision": "1", "idempotency_key": "mcp_verification_smoke",
            "wait_timeout_ms": 20000, "cases": [{"case_id": "gallery", "workload": {
                "kind": "registered_scenario", "scenario_id": "demo.gallery.steady", "revision": "1",
                "fixture_id": "gallery-default-v1", "seed": "17"}}]}
        planned = await self.result("verification.plan", {"request": suite}, live=True)
        assert planned["feasible"], planned

        async def terminal_report(result: dict) -> dict:
            if result.get("schema") == "FlowUi.Agentic.VerificationReport":
                return result
            identity = result["job_id"]
            self.jobs.discard(identity)
            deadline = time.monotonic() + self.timeout
            while time.monotonic() < deadline:
                status = await self.result("verification.status", {"job_id": identity,
                    "wait_timeout_ms": 1000}, live=True)
                self.jobs.discard(identity)
                assert "images" not in status and "trees" not in status, status
                if status.get("report_published") or status.get("terminal"):
                    return await self.result("verification.report", {"job_id": identity}, live=True)
            raise AssertionError(f"Verification did not seal: {identity}")

        completed = await self.result("verification.run", {"request": suite, "require_pass": True}, live=True)
        report = await terminal_report(completed)
        identity = report["job_id"]
        self.jobs.discard(identity)
        assert report["verdict"] == "pass" and int(report["coverage"]["evaluated"]) > 0, report
        reread = await self.result("verification.report", {"job_id": identity, "require_pass": True}, live=True)
        assert reread["verdict"] == "pass" and reread["job_id"] == identity, reread
        await self.result("verification.status", {"job_id": identity, "after_sequence": "0",
            "wait_timeout_ms": 0}, live=True)
        long_plan = {"schema": "FlowUi.Agentic.InteractionPlan", "schema_version": "1.0",
            "mode": "common_controls", "window_id": self.window_id, "deadline_ns": "30000000000",
            "steps": [{"operation": "wait", "frames": "2048", "minimum_duration_ns": "20000000000",
                "deadline_ns": "30000000000"}]}
        cancellation = {**suite, "idempotency_key": "mcp_verification_cancel", "wait_timeout_ms": 0,
            "cases": [{"case_id": "cancel-long-wait", "workload": {"kind": "interaction_plan", "plan": long_plan}}]}
        admitted = await self.result("verification.run", {"request": cancellation}, live=True)
        canceled_identity = admitted["job_id"]
        self.jobs.discard(canceled_identity)
        try:
            await self.result("verification.cancel", {"job_id": canceled_identity}, live=True)
            canceled_report = await terminal_report(admitted)
            assert canceled_report["execution_status"] == "canceled", canceled_report
            assert canceled_report["verdict"] != "pass", canceled_report
        finally:
            await self.result("verification.cancel", {"job_id": canceled_identity}, live=True)

    async def retire_jobs(self) -> None:
        if self.client is None or self.process is None or self.process.poll() is not None:
            return
        for job_id in list(self.jobs):
            try:
                await self.call("job.cancel", {"job_id": job_id}, live=True)
                await self.wait_job(job_id, canceled=True)
            except Exception as error:
                self.failures.append({"stage": "job_cleanup", "job_id": job_id, "error": str(error)})
            self.jobs.discard(job_id)

    def save_report(self) -> dict:
        uncovered = [operation for operation in sorted(EXPECTED_OPERATIONS)
                     if self.operations.get(operation, {}).get("successful_results", 0) == 0]
        user_unchanged = self.fingerprint(self.user_changes) == self.user_changes_before
        report = {"schema": "FlowUi.Tests.LiveMcpCoverage", "schema_version": "1.0",
            "cli": str(self.cli), "demo": str(self.demo), "source_workspace": str(self.repository),
            "owned_workspace": str(self.workspace), "store": str(self.store), "run_id": self.run_id,
            "sdk_version": getattr(self, "sdk_version", None), "tool_count": len(self.tool_schemas),
            "operation_count": len(self.operations), "calls": len(self.calls), "stages": self.stages,
            "tools": {tool: {"operations": sorted(operation for operation, owner in self.owners.items() if owner == tool),
                "calls": sum(entry["calls"] for entry in self.operations.values() if entry["tool"] == tool)}
                for tool in sorted(self.tool_schemas)}, "operations": self.operations,
            "uncovered_operations": uncovered, "failures": self.failures, "captures": self.captures,
            "user_changes_unchanged": user_unchanged,
            "passed": not self.failures and not uncovered and user_unchanged and len(self.tool_schemas) == 16}
        write_json(self.output / "coverage.json", report)
        return report

    async def run(self) -> int:
        try:
            from mcp import Client
            from mcp.client.stdio import StdioServerParameters, stdio_client
            self.prepare()
            await self.start_demo()
            adapter_source = self.repository / "tools/flowui-agent-mcp/src"
            python_paths = [str(adapter_source), *(str(Path(value).resolve()) for value in os.environ.get("PYTHONPATH", "").split(os.pathsep) if value)]
            environment = dict(os.environ)
            environment["PYTHONPATH"] = os.pathsep.join(python_paths)
            parameters = StdioServerParameters(command=sys.executable,
                args=["-m", "flowui_agent_mcp.server", "--workspace", str(self.workspace),
                    "--cli", str(self.cli), "--timeout", "30"], cwd=str(self.workspace), env=environment)
            with (self.output / "adapter.log").open("w") as adapter_log:
                async with Client(stdio_client(parameters, errlog=adapter_log), read_timeout_seconds=45) as client:
                    self.client = client
                    await self.discover()
                    for name, stage in [("capabilities_context", self.capabilities_and_context),
                        ("changes_transaction", self.changes_transaction), ("actions_interactions", self.actions_and_interactions),
                        ("visual_capture_queries", self.capture_and_queries),
                        ("artifact_images_and_visual_comparison", self.artifact_images),
                        ("diagnostics_memory_and_quality", self.diagnostics_and_memory),
                        ("scenarios_comparison_objective", self.scenarios_and_analysis),
                        ("stop_cancel_recovery", self.stopping_and_cancellation),
                        ("verification_run_and_report", self.verification_run_and_report)]:
                        await self.stage(name, stage)
                    await self.retire_jobs()
        except Exception as error:
            self.failures.append({"stage": "bootstrap_or_transport", "error": str(error), "traceback": traceback.format_exc()})
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
                    endpoint_status = self.endpoint.lstat()
                    if not stat.S_ISSOCK(endpoint_status.st_mode) or endpoint_status.st_uid != os.getuid():
                        raise AssertionError("Refusing to remove a local endpoint not owned by this suite")
                    self.endpoint.unlink()
                except FileNotFoundError:
                    pass
        report = self.save_report()
        print(json.dumps({"passed": report["passed"], "tools": report["tool_count"],
            "operations": report["operation_count"], "calls": report["calls"], "coverage": str(self.output / "coverage.json")}), flush=True)
        return 0 if report["passed"] else 1


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--cli", required=True, type=Path)
    parser.add_argument("--demo", required=True, type=Path)
    parser.add_argument("--workspace", required=True, type=Path)
    parser.add_argument("--output-directory", required=True, type=Path)
    parser.add_argument("--timeout", type=float, default=90)
    arguments = parser.parse_args()
    if not 0 < arguments.timeout <= 240:
        parser.error("--timeout must be between zero and 240 seconds")
    suite = LiveMcpSuite(arguments)

    async def bounded_run() -> int:
        try:
            return await asyncio.wait_for(suite.run(), timeout=270)
        except asyncio.TimeoutError:
            suite.failures.append({"stage": "suite_deadline", "error": "The complete MCP suite exceeded 270 seconds"})
            suite.save_report()
            print(json.dumps({"passed": False, "coverage": str(suite.output / "coverage.json")}), flush=True)
            return 1

    return asyncio.run(bounded_run())


if __name__ == "__main__":
    raise SystemExit(main())
