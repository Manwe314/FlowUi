"""Adapter boundary tests; no SDK or GPU is needed for subprocess/schema verification."""

import asyncio
import json
import os
from pathlib import Path
import sys
import tempfile
import unittest
from unittest.mock import AsyncMock, patch

sys.path.insert(0, str(Path(__file__).resolve().parents[1] / "src"))

from flowui_agent_mcp.server import Adapter, TOOLS, create_server


class SchemaTests(unittest.TestCase):
    def test_packaged_tool_schemas_match_registry_mapping(self):
        schema_path = Path(__file__).resolve().parents[1] / "src/flowui_agent_mcp/schemas/tools.json"
        declared = json.loads(schema_path.read_text())
        self.assertEqual(set(declared), set(TOOLS))
        for name, (description, schema) in TOOLS.items():
            self.assertEqual(declared[name]["description"], description)
            self.assertEqual(declared[name]["inputSchema"], schema)

    def test_packaged_plan_schema_matches_authoritative_artifact(self):
        repository = Path(__file__).resolve().parents[3]
        packaged = Path(__file__).resolve().parents[1] / "src/flowui_agent_mcp/schemas/interaction-plan.schema.json"
        authoritative = repository / "include/devSystems/devAgenticWorkflow/schemas/v1/InteractionPlan.schema.json"
        self.assertEqual(json.loads(packaged.read_text()), json.loads(authoritative.read_text()))


class AdapterTests(unittest.IsolatedAsyncioTestCase):
    async def asyncSetUp(self):
        self.temporary = tempfile.TemporaryDirectory(prefix="flowui-adapter-test-")
        self.workspace = Path(self.temporary.name)

    async def asyncTearDown(self):
        self.temporary.cleanup()

    def fake_cli(self, body):
        path = self.workspace / "fake cli"
        path.write_text("#!/usr/bin/env python3\n" + body)
        path.chmod(0o700)
        return str(path)

    async def test_structured_arguments_preserve_shell_metacharacters(self):
        cli = self.fake_cli('''import json,sys
operation='.'.join(sys.argv[1:sys.argv.index('--workspace')])
arguments=json.loads(sys.argv[sys.argv.index('--arguments-json')+1])
print(json.dumps({'schema':'FlowUi.Agentic.CommandResult','schema_version':'1.0','request_id':'request_test','operation':operation,'operation_status':'completed','exit_code':0,'result':arguments}))
''')
        adapter = Adapter(self.workspace, cli)
        envelope, image = await adapter.call("flowui_context", {
            "operation": "context.find", "arguments": {"intent": "$(touch escaped); `echo bad`"}})
        self.assertEqual(envelope["operation_status"], "completed")
        self.assertEqual(envelope["result"]["intent"], "$(touch escaped); `echo bad`")
        self.assertFalse((self.workspace / "escaped").exists())
        self.assertIsNone(image)

    async def test_objective_miss_remains_successful_analysis(self):
        cli = self.fake_cli('''import json,sys
operation='.'.join(sys.argv[1:sys.argv.index('--workspace')])
print(json.dumps({'schema':'FlowUi.Agentic.CommandResult','schema_version':'1.0','request_id':'request_test','operation':operation,'operation_status':'completed','exit_code':7,'result':{'verdict':'fail'}}))
sys.exit(7)
''')
        envelope, _ = await Adapter(self.workspace, cli).call("flowui_compare", {
            "operation": "objective.check", "arguments": {"capture": "cap_test", "require_pass": True}})
        self.assertEqual(envelope["operation_status"], "completed")
        self.assertEqual(envelope["result"]["verdict"], "fail")

    async def test_rejects_administration_and_unknown_arguments(self):
        adapter = Adapter(self.workspace, "/does/not/exist")
        envelope, _ = await adapter.call("flowui_query", {"operation": "store.prune"})
        self.assertEqual(envelope["error"]["code"], "invalid_arguments")
        envelope, _ = await adapter.call("flowui_artifact", {
            "operation": "artifact.read", "arguments": {"path": "/etc/passwd"}})
        self.assertEqual(envelope["error"]["code"], "invalid_arguments")

    async def test_apply_requires_exact_review_revisions_and_operation(self):
        cli = self.fake_cli('''import json,sys
operation='.'.join(sys.argv[1:sys.argv.index('--workspace')])
arguments=json.loads(sys.argv[sys.argv.index('--arguments-json')+1])
print(json.dumps({'schema':'FlowUi.Agentic.CommandResult','schema_version':'1.0','request_id':'request_test','operation':operation,'operation_status':'completed','exit_code':0,'result':arguments}))
''')
        adapter = Adapter(self.workspace, cli)
        incomplete, _ = await adapter.call("flowui_changes_apply", {"operation": "changes.apply", "arguments": {"instance": "run_test"}})
        self.assertEqual(incomplete["exit_code"], 2)
        arguments = {"instance": "run_test", "review_id": "review_test", "schema_generation": "12", "proposal_revision": "24",
            "theme_revision": "7", "disk_revision": "sha256:disk", "operation_id": "operation_test"}
        complete, _ = await adapter.call("flowui_changes_apply", {"operation": "changes.apply", "arguments": arguments})
        self.assertEqual(complete["operation_status"], "completed")
        self.assertEqual(complete["result"], arguments)

    async def test_visual_flow_selectors_remain_typed_and_reject_ambiguous_crops(self):
        adapter = Adapter(self.workspace, "unused")
        response = {"operation_status": "completed"}
        for selector in ["users_root", "node:3", "instance:18446744073709551615",
                {"node_index": 3}, {"instance_id": "18446744073709551615"}, {"debug_name": "users_root"}]:
            arguments = {"capture": "cap_test", "kind": "subtree", "tree": "tree_test", "flow_node": selector}
            with patch.object(adapter, "invoke", new=AsyncMock(return_value=(response, None))) as invoke:
                result, _ = await adapter.call("flowui_query", {"operation": "query.visual", "arguments": arguments})
                self.assertEqual(result, response)
                invoke.assert_awaited_once_with("query.visual", arguments)
            arguments = {"capture": "cap_test", "image": "image_test", "crop_flow_node": selector}
            with patch.object(adapter, "invoke", new=AsyncMock(return_value=(response, None))) as invoke:
                result, _ = await adapter.call("flowui_artifact", {"operation": "artifact.image", "arguments": arguments})
                self.assertEqual(result, response)
                invoke.assert_awaited_once_with("artifact.image", arguments)
        for invalid in [{}, {"node_index": -1}, {"node_index": "3"}, {"instance_id": 3},
                {"debug_name": ""}, {"node_index": 3, "debug_name": "users_root"}, {"path": "/etc/passwd"}]:
            with patch.object(adapter, "invoke", new=AsyncMock()) as invoke:
                result, _ = await adapter.call("flowui_artifact", {"operation": "artifact.image", "arguments": {"crop_flow_node": invalid}})
                self.assertEqual(result["exit_code"], 2)
                invoke.assert_not_awaited()
        with patch.object(adapter, "invoke", new=AsyncMock()) as invoke:
            result, _ = await adapter.call("flowui_artifact", {"operation": "artifact.image", "arguments": {
                "crop_flow_node": "users_root", "crop": {"x": 0, "y": 0, "width": 10, "height": 10}}})
            self.assertEqual(result["exit_code"], 2)
            invoke.assert_not_awaited()

    async def test_interaction_and_action_operations_preserve_typed_finite_data(self):
        adapter = Adapter(self.workspace, "unused")
        plan = {"schema": "FlowUi.Agentic.InteractionPlan", "schema_version": "1.0",
            "mode": "raw_frames", "window_id": "1", "frames": [{"offset": "0"}, {"offset": "1", "buttons_down": [0]}]}
        response = {"operation_status": "accepted", "result": {"job_id": "interaction_test"}}
        cases = [
            ("flowui_interaction", "interaction.plan", {"plan": plan}),
            ("flowui_interaction", "interaction.start", {"instance": "run_test", "idempotency_key": "case_test", "plan": plan,
                "capture": {"content": {"include_interaction_labels": True, "include_interaction_outcomes": True}}}),
            ("flowui_interaction", "interaction.start", {"instance": "run_test", "idempotency_key": "case_test", "plan_artifact": {"capture": "cap_test", "artifact": "interaction_plan", "sha256": "a" * 64}, "rebind_run": True}),
            ("flowui_interaction", "interaction.record.start", {"instance": "run_test", "window_id": "1", "max_frames": "100", "deadline_ns": "1000000000", "idempotency_key": "record_test", "include_text": False}),
            ("flowui_interaction", "interaction.record.stop", {"instance": "run_test", "job_id": "interaction_test"}),
            ("flowui_interaction", "interaction.query", {"capture": "cap_test", "kind": "inputs", "offset": "0"}),
            ("flowui_interaction", "interaction.targets", {"instance": "run_test", "window_id": "1", "target": {"clay_name": "entry", "anchor": {"x": .5, "y": .5}}}),
            ("flowui_action", "action.invoke", {"instance": "run_test", "action_id": "app.save", "revision": "18446744073709551615", "idempotency_key": "save_test", "arguments": {"document": "$(literal)"}}),
        ]
        for tool, operation, arguments in cases:
            with self.subTest(operation=operation), patch.object(adapter, "invoke", new=AsyncMock(return_value=(response, None))) as invoke:
                result, _ = await adapter.call(tool, {"operation": operation, "arguments": arguments})
                self.assertEqual(result, response)
                invoke.assert_awaited_once_with(operation, arguments)
        invalid_cases = [
            ("interaction.start", {"instance": "run_test", "plan": plan}),
            ("interaction.start", {"instance": "run_test", "idempotency_key": "case", "plan": plan, "plan_artifact": {"capture": "cap", "artifact": "plan", "sha256": "a" * 64}}),
            ("interaction.plan", {"plan": {**plan, "script": "run arbitrary callback"}}),
            ("interaction.plan", {"plan": {**plan, "frames": [{"offset": "0", "keys_down": [33]}]}}),
            ("interaction.plan", {"plan": {**plan, "frames": [{"offset": "0", "keys_down": [32, 32]}]}}),
            ("interaction.plan", {"plan": {**plan, "frames": [{"offset": "0", "text_redacted": True}]}}),
            ("interaction.start", {"instance": "run_test", "idempotency_key": "case", "plan": {**plan, "clock": "exact_window_frames"}, "rebind_run": True}),
            ("interaction.record.stop", {"instance": "run_test"}),
            ("interaction.query", {"instance": "run_test"}),
            ("interaction.targets", {"instance": "run_test", "window_id": "1", "target": {"clay_name": "entry", "point": {"x": 1, "y": 2}}}),
            ("interaction.targets", {"instance": "run_test", "window_id": "1", "cursor": "2"}),
        ]
        for operation, arguments in invalid_cases:
            with self.subTest(operation=operation), patch.object(adapter, "invoke", new=AsyncMock()) as invoke:
                result, _ = await adapter.call("flowui_interaction", {"operation": operation, "arguments": arguments})
                self.assertEqual(result["error"]["code"], "invalid_arguments")
                invoke.assert_not_awaited()
        result, _ = await adapter.call("flowui_action", {"operation": "action.invoke", "arguments": {"instance": "run_test", "action_id": "app.save", "idempotency_key": "save_test"}})
        self.assertEqual(result["error"]["code"], "invalid_arguments")

    async def test_rejects_malformed_and_oversized_subprocess_output(self):
        for body in ["print('not-json')\n", "print('x' * 1100000)\n"]:
            envelope, _ = await Adapter(self.workspace, self.fake_cli(body)).call(
                "flowui_capabilities", {"operation": "capabilities"})
            self.assertEqual(envelope["operation_status"], "failed")

    async def test_timeout_does_not_hang_protocol_call(self):
        cli = self.fake_cli("import time\ntime.sleep(10)\n")
        started = asyncio.get_running_loop().time()
        envelope, _ = await Adapter(self.workspace, cli, timeout=0.05).call(
            "flowui_capabilities", {"operation": "capabilities"})
        self.assertEqual(envelope["exit_code"], 5)
        self.assertLess(asyncio.get_running_loop().time() - started, 2)

    async def test_png_delivery_and_metadata_fallback(self):
        cli = self.fake_cli('''import json,struct,sys,zlib
operation='.'.join(sys.argv[1:sys.argv.index('--workspace')])
arguments=json.loads(sys.argv[sys.argv.index('--arguments-json')+1])
def chunk(kind,payload):
    return struct.pack('>I',len(payload))+kind+payload+struct.pack('>I',zlib.crc32(kind+payload))
png=b'\\x89PNG\\r\\n\\x1a\\n'+chunk(b'IHDR',struct.pack('>IIBBBBB',1,1,8,2,0,0,0))+chunk(b'IDAT',zlib.compress(b'\\x00\\xff\\x00\\x00'))+chunk(b'IEND',b'')
open(arguments['output'],'wb').write(png)
print(json.dumps({'schema':'FlowUi.Agentic.CommandResult','schema_version':'1.0','request_id':'request_test','operation':operation,'operation_status':'completed','exit_code':0,'result':{'output':arguments['output'],'image_id':'image_test'}}))
''')
        adapter = Adapter(self.workspace, cli)
        for include_image in [True, False]:
            envelope, image = await adapter.call("flowui_artifact", {"operation": "artifact.image", "arguments": {
                "capture": "cap_test", "image": "image_test", "include_image": include_image}})
            self.assertEqual(envelope["operation_status"], "completed")
            self.assertNotIn("output", envelope["result"])
            self.assertEqual(image is not None, include_image)

    async def test_actual_cli_parity_when_provided(self):
        cli = os.environ.get("FLOWUI_AGENT_CLI")
        if not cli:
            self.skipTest("Set FLOWUI_AGENT_CLI to execute actual CLI parity")
        envelope, _ = await Adapter(Path.cwd(), cli).call("flowui_capabilities", {"operation": "capabilities"})
        self.assertEqual(envelope["operation_status"], "completed")
        self.assertIn("commands", envelope["result"])

    async def test_cancellation_hands_known_job_to_same_bounded_command(self):
        started = asyncio.Event()
        calls = []
        processes = []

        class FakeStream:
            def __init__(self, payload=b"", blocked=None):
                self.payload = payload
                self.blocked = blocked

            async def read(self, amount):
                if self.blocked is not None:
                    await self.blocked.wait()
                payload, self.payload = self.payload[:amount], self.payload[amount:]
                return payload

        class FakeProcess:
            def __init__(self, blocked, payload=b""):
                self.finished = asyncio.Event()
                self.returncode = None if blocked else 0
                if not blocked:
                    self.finished.set()
                self.stdout = FakeStream(payload, self.finished if blocked else None)
                self.stderr = FakeStream()
                self.killed = False

            async def wait(self):
                await self.finished.wait()
                return self.returncode

            def kill(self):
                self.killed = True
                self.returncode = -9
                self.finished.set()

        async def execute(*argv, **kwargs):
            operation = ".".join(argv[1:argv.index("--workspace")])
            arguments = json.loads(argv[argv.index("--arguments-json") + 1])
            calls.append((operation, arguments))
            if operation == "job.cancel":
                payload = json.dumps({"schema": "FlowUi.Agentic.CommandResult", "schema_version": "1.0",
                    "request_id": "request_cancel", "operation": operation, "operation_status": "completed",
                    "exit_code": 0, "result": {"cancellation_requested": True}}).encode()
                process = FakeProcess(False, payload)
            else:
                process = FakeProcess(True)
                started.set()
            processes.append(process)
            return process

        adapter = Adapter(self.workspace, "fake-executor")
        for operation in ["job.status", "interaction.query", "interaction.record.stop"]:
            started.clear()
            calls.clear()
            processes.clear()
            with patch("flowui_agent_mcp.server.asyncio.create_subprocess_exec", new=execute):
                pending = asyncio.create_task(adapter.invoke(operation, {"instance": "run_test", "job_id": "job_test"}))
                await started.wait()
                pending.cancel()
                with self.assertRaises(asyncio.CancelledError):
                    await pending
            self.assertEqual(calls, [(operation, {"instance": "run_test", "job_id": "job_test"}),
                ("job.cancel", {"instance": "run_test", "job_id": "job_test"})])
            self.assertTrue(processes[0].killed)

        started.clear()
        calls.clear()
        with patch("flowui_agent_mcp.server.asyncio.create_subprocess_exec", new=execute):
            pending = asyncio.create_task(adapter.invoke("capture.start", {
                "instance": "run_test", "idempotency_key": "capture_retry"}))
            await started.wait()
            pending.cancel()
            with self.assertRaises(asyncio.CancelledError):
                await pending
        self.assertEqual([operation for operation, arguments in calls], ["capture.start"])


class SdkTests(unittest.IsolatedAsyncioTestCase):
    async def test_sdk_discovery_and_structured_tool_result(self):
        try:
            from mcp import Client
        except ImportError:
            self.skipTest("Official SDK is an optional separately installed package")
        cli = os.environ.get("FLOWUI_AGENT_CLI")
        if not cli:
            self.skipTest("Set FLOWUI_AGENT_CLI for official SDK parity")
        server = create_server(Adapter(Path.cwd(), cli))
        async with Client(server) as client:
            tools = await client.list_tools()
            names = {tool.name for tool in (tools.tools if hasattr(tools, "tools") else tools)}
            self.assertEqual(names, set(TOOLS))
            result = await client.call_tool("flowui_capabilities", {"operation": "capabilities"})
            self.assertEqual(result.structured_content["operation_status"], "completed")
            self.assertFalse(result.is_error)

    async def test_actual_stdio_sdk_initialization_and_cli_parity(self):
        try:
            from mcp import Client
            from mcp.client.stdio import StdioServerParameters
        except ImportError:
            self.skipTest("Official SDK is an optional separately installed package")
        cli = os.environ.get("FLOWUI_AGENT_CLI")
        if not cli:
            self.skipTest("Set FLOWUI_AGENT_CLI for official SDK stdio execution")
        parameters = StdioServerParameters(command=sys.executable,
            args=["-m", "flowui_agent_mcp.server", "--workspace", str(Path.cwd()), "--cli", cli],
            cwd=str(Path.cwd()), env={"PYTHONPATH": os.environ.get("PYTHONPATH", "")})
        async with Client(parameters, read_timeout_seconds=10) as client:
            tools = await client.list_tools()
            discovered = tools.tools if hasattr(tools, "tools") else tools
            names = {tool.name for tool in discovered}
            self.assertEqual(names, set(TOOLS))
            for tool in discovered:
                if tool.name in {"flowui_interaction", "flowui_action"}:
                    self.assertFalse(tool.annotations.read_only_hint)
                    self.assertTrue(tool.annotations.destructive_hint)
            result = await client.call_tool("flowui_capabilities", {"operation": "capabilities"})
            self.assertEqual(result.structured_content["result"]["mode"], "offline")
            self.assertFalse(result.is_error)
            source_plan = {"schema": "FlowUi.Agentic.InteractionPlan", "schema_version": "1.0",
                "mode": "raw_frames", "window_id": "1", "frames": [{"offset": "0"},
                    {"offset": "1", "buttons_down": [0]}, {"offset": "2"}]}
            planned = await client.call_tool("flowui_interaction", {"operation": "interaction.plan", "arguments": {"plan": source_plan}})
            self.assertFalse(planned.is_error)
            self.assertEqual(planned.structured_content["result"]["compiled"]["frame_count"], "3")
            self.assertEqual(len(planned.structured_content["result"]["compiled"]["digest"]), 64)
            invalid = await client.call_tool("flowui_query", {"operation": "store.prune"})
            self.assertTrue(invalid.is_error)
            self.assertEqual(invalid.structured_content["exit_code"], 2)


if __name__ == "__main__":
    unittest.main()
