#!/usr/bin/env python3
"""Exercise the shipped CLI envelopes, configuration generation and bounded launcher."""

import argparse
import json
import os
import re
from pathlib import Path
import subprocess
import tempfile
import time


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("cli", type=Path)
    arguments = parser.parse_args()
    executable = arguments.cli.resolve(strict=True)
    with tempfile.TemporaryDirectory(prefix="flowui-cli-test-") as temporary:
        workspace = Path(temporary)

        def command(*argv, expected=0):
            completed = subprocess.run([str(executable), *argv, "--workspace", str(workspace), "--json"],
                stdin=subprocess.DEVNULL, capture_output=True, text=True, timeout=10)
            if completed.returncode != expected:
                raise AssertionError((argv, completed.returncode, completed.stdout, completed.stderr))
            result = json.loads(completed.stdout)
            assert result["schema"] == "FlowUi.Agentic.CommandResult"
            assert result["schema_version"] == "1.0"
            assert result["exit_code"] == expected
            assert re.fullmatch(r"[A-Za-z0-9_.-]{1,128}", result["request_id"])
            assert result["request_id"] not in [".", ".."]
            assert isinstance(result["operation"], str) and result["operation"]
            return result

        result = command("capabilities")
        assert result["result"]["mode"] == "offline"
        assert not (workspace / ".flowui").exists()
        assert command("--help")["operation"] == "help"
        assert command()["operation"] == "help"
        command("arbitrary", "shell", expected=2)
        command("capture", "start", expected=3)
        command("capture", "plan", "--objective", "window_present_call_cadence_fps:gt:40", expected=3)
        command("capture", "plan", "--objective", "bad-objective", expected=2)
        source_plan = {"schema": "FlowUi.Agentic.InteractionPlan", "schema_version": "1.0",
            "mode": "raw_frames", "window_id": "1", "clock": "window_input_frames",
            "coordinates": "window_logical", "baseline": "neutral_before_start",
            "frames": [{"offset": "0", "pointer": {"x": 20, "y": 30}},
                {"offset": "1", "pointer": {"x": 20, "y": 30}, "buttons_down": [0]},
                {"offset": "2", "pointer": {"x": 20, "y": 30}}]}
        plan_path = workspace / "plan.json"
        plan_path.write_text(json.dumps(source_plan))
        planned = command("interaction", "plan", "--plan-file", str(plan_path))["result"]
        assert planned["compiled"]["frame_count"] == "3"
        assert len(planned["compiled"]["digest"]) == 64
        assert not (workspace / ".flowui").exists()
        same = command("interaction", "plan", "--plan", json.dumps(source_plan))["result"]
        assert same["compiled"]["digest"] == planned["compiled"]["digest"]
        custom_plan = {"schema": "FlowUi.Agentic.InteractionPlan", "schema_version": "1.0",
            "mode": "common_controls", "window_id": "1", "steps": [{"operation": "click", "target": {"custom_id": "fixture.custom"}}]}
        custom = command("interaction", "plan", "--plan", json.dumps(custom_plan))["result"]
        assert custom["compiled"]["requires_geometry"] is True
        assert "offline_planning_requires_current_runtime_capability_and_geometry_validation" in custom["compiled"]["limitations"]
        focus_plan = {**custom_plan, "steps": [{"operation": "assert", "expect": ["text_focused"]}]}
        focus = command("interaction", "plan", "--plan", json.dumps(focus_plan))["result"]
        assert focus["compiled"]["requires_observations"] is True
        command("interaction", "record", "start", "--window-id", "1", "--max-frames", "10", "--deadline-ns", "1000000000", expected=3)
        command("interaction", "record", "stop", "--job", "job_missing", expected=3)
        command("interaction", "targets", "--window-id", "1", "--target", '{"clay_name":"entry"}', expected=3)
        command("action", "invoke", "--action-id", "action_save", "--revision", "1", "--arguments", '{"path":"$(literal)"}', expected=3)
        command("interaction", "plan", "--plan", "{broken", expected=2)
        command("interaction", "plan", "--plan-file", str(workspace / "missing.json"), expected=2)
        command("interaction", "targets", "--window-id", "bad", expected=2)
        command("interaction", "targets", "--window-id", "01", expected=2)
        command("interaction", "targets", "--window-id", "1", "--limit", "1.5", expected=2)
        command("interaction", "targets", "--window-id", "1", "--cursor", "2", expected=2)
        command("interaction", "record", "start", "--arguments-json", '{"include_text":"false"}', expected=2)
        command("action", "invoke", "--arguments-json", '{"action_id":"app.save","revision":1}', expected=2)
        command("query", "performance", "--capture", "../outside", expected=3)
        for selector in ["users_root", "node:4", "instance:18446744073709551615", '{"node_index":4}', '{"instance_id":"18446744073709551615"}']:
            command("query", "visual", "--capture", "cap_missing", "--kind", "subtree", "--tree", "tree_missing", "--flow-node", selector, expected=3)
            command("artifact", "image", "--capture", "cap_missing", "--image", "image_missing", "--crop-flow-node", selector, expected=3)
        command("artifact", "image", "--crop-flow-node", "{bad-json", expected=2)
        command("artifact", "image", "--crop", '{"x":0,"y":0,"width":10,"height":10}', "--crop-flow-node", "users_root", expected=2)
        for client in ["codex", "claude-code", "antigravity", "gemini", "cursor", "vscode", "generic"]:
            result = command("setup", "--client", client, "--print")
            assert result["result"]["written"] is False
            assert not (workspace / ".flowui").exists()
            if client == "codex":
                assert "[mcp_servers.flowui]" in result["result"]["configuration_text"]
            if client == "vscode":
                assert result["result"]["configuration"]["servers"]["flowui"]["type"] == "stdio"

        build_log = workspace / "build.log"
        build_log.write_text("src/app.cpp:42:7: warning: unused input [-Wunused-variable]\n")
        result = command("ingest", "build", "--log", str(build_log), "--producer", "gcc")
        assert "diagnostics" in result["result"]
        assert not (workspace / ".flowui").exists()

        if os.name == "posix":
            child = workspace / "fixture app"
            child.write_text("#!/usr/bin/env python3\nimport json,os,sys\n"
                "print(json.dumps({'argv':sys.argv[1:],'run_id':os.environ['FLOWUI_AGENTIC_RUN_ID']}),flush=True)\n"
                "print('x'*5000,flush=True)\nprint('y'*5000,file=sys.stderr,flush=True)\n")
            child.chmod(0o700)
            literal = "$(touch escaped); `echo unexpected`"
            launched = command("launch", "--executable", str(child), "--argv", json.dumps([literal]), "--log-bytes", "1024")["result"]
            session_path = Path(launched["launch_session"])
            deadline = time.monotonic() + 5
            while time.monotonic() < deadline:
                if session_path.exists():
                    session = json.loads(session_path.read_text())
                    if session["status"] == "exited":
                        break
                time.sleep(0.02)
            else:
                raise AssertionError("Launch supervisor did not finish")
            assert session["exit_code"] == 0
            stdout = Path(launched["stdout"]).read_bytes()
            stderr = Path(launched["stderr"]).read_bytes()
            assert len(stdout) <= 1024 and len(stderr) <= 1024
            first_line = json.loads(stdout.splitlines()[0])
            assert first_line["argv"] == [literal]
            assert first_line["run_id"] == launched["run_id"]
            assert session["stdout"]["truncated"] and session["stderr"]["truncated"]
            assert not (workspace / "escaped").exists()
    print("Agentic CLI contracts passed")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
