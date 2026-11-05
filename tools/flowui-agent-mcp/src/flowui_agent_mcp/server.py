"""Bounded official-SDK stdio mapping to the shared flowui-agent JSON command API."""

from __future__ import annotations

import argparse
import asyncio
import base64
import json
import logging
import struct
import sys
import tempfile
import uuid
from pathlib import Path
from typing import Any

from jsonschema import Draft202012Validator
from .contracts import RESULT_SCHEMA, TOOLS

MAXIMUM_RESPONSE_BYTES = 1024 * 1024
MAXIMUM_IMAGE_BYTES = 4 * 1024 * 1024
MAXIMUM_ARGUMENT_BYTES = 64 * 1024
MAXIMUM_VERIFICATION_WAIT_MS = 20_000
VERIFICATION_TRANSPORT_MARGIN_SECONDS = 5.0
MUTATING_TOOLS = {
    "flowui_capture", "flowui_job", "flowui_scenario", "flowui_changes_apply",
    "flowui_interaction", "flowui_action", "flowui_verification_run", "flowui_verification_cancel",
}
DESTRUCTIVE_TOOLS = {
    "flowui_changes_apply", "flowui_interaction", "flowui_action", "flowui_verification_run",
}


def error_envelope(operation: str, code: str, message: str, exit_code: int = 4) -> dict[str, Any]:
    return {"schema": "FlowUi.Agentic.CommandResult", "schema_version": "1.0", "request_id": "request_" + uuid.uuid4().hex,
            "operation": operation, "operation_status": "failed", "exit_code": exit_code, "result": None,
            "limitations": [], "error": {"code": code, "message": message}}


class Adapter:
    """Own only protocol mapping and bounded child processes, never application state."""

    def __init__(self, workspace: Path, cli: str = "flowui-agent", timeout: float = 10.0):
        self.workspace = workspace.resolve(strict=True)
        self.cli = cli
        self.timeout = timeout

    def command_timeout(self, operation: str, arguments: dict[str, Any]) -> float:
        """Allow bounded server waits to finish before the subprocess transport expires."""
        if operation not in {"verification.run", "verification.status"}:
            return self.timeout
        request = arguments.get("request", {}) if operation == "verification.run" else arguments
        wait_ms = arguments.get("wait_timeout_ms", request.get("wait_timeout_ms", MAXIMUM_VERIFICATION_WAIT_MS))
        return max(self.timeout, min(wait_ms, MAXIMUM_VERIFICATION_WAIT_MS) / 1000 + VERIFICATION_TRANSPORT_MARGIN_SECONDS)

    async def invoke(self, operation: str, arguments: dict[str, Any], *, cancel_on_interrupt: bool = True) -> tuple[dict[str, Any], bytes | None]:
        """Use structured argv; subprocess stdout/stderr and PNG output are bounded separately."""
        encoded = json.dumps(arguments, ensure_ascii=False, separators=(",", ":"))
        if len(encoded.encode()) > MAXIMUM_ARGUMENT_BYTES:
            return error_envelope(operation, "argument_budget_exhausted", "Arguments exceed the byte limit.", 2), None
        temporary_directory = tempfile.TemporaryDirectory(prefix="flowui-agent-mcp-")
        request_arguments = dict(arguments)
        include_image = request_arguments.pop("include_image", True)
        image_path = Path(temporary_directory.name) / "image.png"
        if operation == "artifact.image":
            request_arguments.update(output=str(image_path), format="png")
        argv = [self.cli, *operation.split("."), "--workspace", str(self.workspace), "--json", "--arguments-json",
                json.dumps(request_arguments, ensure_ascii=False, separators=(",", ":"))]
        process = None
        response_task = None
        try:
            process = await asyncio.create_subprocess_exec(*argv, cwd=self.workspace,
                stdin=asyncio.subprocess.DEVNULL, stdout=asyncio.subprocess.PIPE, stderr=asyncio.subprocess.PIPE)

            async def read_bounded(stream: asyncio.StreamReader, limit: int) -> bytes:
                collected = bytearray()
                while True:
                    block = await stream.read(min(8192, limit + 1 - len(collected)))
                    if not block:
                        return bytes(collected)
                    collected.extend(block)
                    if len(collected) > limit:
                        raise ValueError("CLI response exceeded its byte budget")

            response_task = asyncio.gather(
                read_bounded(process.stdout, MAXIMUM_RESPONSE_BYTES),
                read_bounded(process.stderr, 16384), process.wait())
            stdout, stderr, return_code = await asyncio.wait_for(response_task, self.command_timeout(operation, arguments))
            if stderr:
                logging.getLogger(__name__).debug("flowui-agent stderr: %s", stderr.decode("utf-8", errors="replace"))
            envelope = json.loads(stdout.decode("utf-8"))
            Draft202012Validator(RESULT_SCHEMA).validate(envelope)
            if (return_code == 2 and envelope["operation"] == "invalid"
                    and envelope.get("operation_status") == "failed"
                    and envelope.get("error", {}).get("code") == "invalid_arguments"):
                envelope["operation"] = operation
            if envelope["operation"] != operation:
                raise ValueError("CLI response operation did not match the request")
            if return_code not in range(8) or envelope.get("exit_code", return_code) != return_code:
                raise ValueError("CLI exit status did not match its result envelope")
            image_bytes = None
            if operation == "artifact.image" and envelope["operation_status"] != "failed":
                if image_path.is_symlink() or not image_path.is_file():
                    raise ValueError("Image is missing")
                with image_path.open("rb") as image_file:
                    header = image_file.read(24)
                if not header.startswith(b"\x89PNG\r\n\x1a\n") or len(header) < 24:
                    raise ValueError("Image is not a PNG")
                width, height = struct.unpack(">II", header[16:24])
                if width == 0 or height == 0:
                    raise ValueError("PNG dimensions are invalid")
                within_inline_budget = image_path.stat().st_size <= MAXIMUM_IMAGE_BYTES and width * height <= 16_777_216
                if include_image and within_inline_budget:
                    image_bytes = image_path.read_bytes()
                elif include_image:
                    envelope.setdefault("limitations", []).append("inline_image_budget_exceeded_request_a_bounded_crop")
                if isinstance(envelope.get("result"), dict):
                    envelope["result"].pop("output", None)
                    envelope["result"]["image_delivery"] = "inline_png" if image_bytes is not None else "metadata_only_use_artifact_image_to_retrieve"
                    envelope["result"]["artifact_selector"] = {key: request_arguments[key] for key in ["capture", "manifest", "image", "artifact"] if key in request_arguments}
            return envelope, image_bytes
        except asyncio.CancelledError:
            # A CLI call ending does not free submitted GPU buffers. A known job can
            # be canceled through the same safe-point command and recovered by ID.
            if cancel_on_interrupt and not operation.startswith("verification.") and arguments.get("instance") and arguments.get("job_id"):
                try:
                    await asyncio.shield(asyncio.wait_for(self.invoke("job.cancel", {
                        "instance": arguments["instance"], "job_id": arguments["job_id"]}, cancel_on_interrupt=False),
                        timeout=min(self.timeout, 2.0)))
                except Exception:
                    logging.getLogger(__name__).debug("Cancellation handoff failed", exc_info=True)
            raise
        except (asyncio.TimeoutError, OSError, ValueError, json.JSONDecodeError) as failure:
            return error_envelope(operation, "adapter_execution_failed", str(failure), 5 if isinstance(failure, asyncio.TimeoutError) else 4), None
        except Exception as failure:
            return error_envelope(operation, "invalid_cli_result", str(failure)), None
        finally:
            if process is not None and process.returncode is None:
                try:
                    process.kill()
                except ProcessLookupError:
                    logging.getLogger(__name__).debug("CLI already exited during cancellation")
                await process.wait()
            if response_task is not None:
                if not response_task.done():
                    response_task.cancel()
                await asyncio.gather(response_task, return_exceptions=True)
            temporary_directory.cleanup()

    async def call(self, name: str, arguments: dict[str, Any]) -> tuple[dict[str, Any], bytes | None]:
        if name not in TOOLS:
            return error_envelope("unknown", "unknown_tool", "The tool is not registered.", 2), None
        try:
            Draft202012Validator(TOOLS[name][1]).validate(arguments)
        except Exception as failure:
            return error_envelope(str(arguments.get("operation", "unknown")), "invalid_arguments", str(failure), 2), None
        return await self.invoke(arguments["operation"], arguments.get("arguments", {}))


def create_server(adapter: Adapter):
    """Construct the official SDK low-level server with exact input/output schemas."""
    from mcp.server import Server
    from mcp.types import CallToolResult, ImageContent, ListToolsResult, TextContent, Tool, ToolAnnotations

    async def list_tools(context, parameters):
        return ListToolsResult(tools=[Tool(name=name, description=description, input_schema=schema,
            output_schema=RESULT_SCHEMA, annotations=ToolAnnotations(read_only_hint=name not in MUTATING_TOOLS,
                destructive_hint=name in DESTRUCTIVE_TOOLS, open_world_hint=False))
            for name, (description, schema) in TOOLS.items()])

    async def call_tool(context, parameters):
        envelope, image_bytes = await adapter.call(parameters.name, parameters.arguments or {})
        contents = [TextContent(type="text", text=json.dumps(envelope, ensure_ascii=False, separators=(",", ":")))]
        if image_bytes is not None:
            contents.append(ImageContent(type="image", data=base64.b64encode(image_bytes).decode("ascii"), mime_type="image/png"))
        return CallToolResult(content=contents, structured_content=envelope,
            is_error=envelope.get("operation_status") in {"failed", "canceled", "timeout"})

    return Server("flowui-agent", version="1.0.0", on_list_tools=list_tools, on_call_tool=call_tool)


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--workspace", type=Path, default=Path.cwd())
    parser.add_argument("--cli", default="flowui-agent")
    parser.add_argument("--timeout", type=float, default=10.0)
    arguments = parser.parse_args()
    if not 0 < arguments.timeout <= 60:
        parser.error("--timeout must be between zero and sixty seconds")
    logging.basicConfig(stream=sys.stderr, level=logging.WARNING)
    server = create_server(Adapter(arguments.workspace, arguments.cli, arguments.timeout))

    async def run() -> None:
        from mcp.server.stdio import stdio_server
        async with stdio_server() as (read_stream, write_stream):
            await server.run(read_stream, write_stream, server.create_initialization_options())

    asyncio.run(run())


if __name__ == "__main__":
    main()
