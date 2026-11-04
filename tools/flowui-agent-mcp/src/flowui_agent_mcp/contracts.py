"""Finite typed command schemas shared by MCP discovery and packaging checks."""

from __future__ import annotations

import json
import copy
from pathlib import Path
from typing import Any

RESULT_SCHEMA = {
    "type": "object",
    "properties": {
        "schema": {"const": "FlowUi.Agentic.CommandResult"},
        "schema_version": {"const": "1.0"},
        "request_id": {"type": "string"},
        "operation": {"type": "string"},
        "operation_status": {"enum": ["completed", "accepted", "failed", "canceled", "timeout"]},
        "exit_code": {"type": "integer", "minimum": 0, "maximum": 7},
        "result": {},
        "error": {"type": "object"},
        "limitations": {"type": "array"},
    },
    "required": ["schema", "schema_version", "request_id", "operation", "operation_status", "result"],
}

_STRING = {"type": "string", "maxLength": 4096}
_DECIMAL = {"type": "string", "pattern": "^(0|[1-9][0-9]{0,19})$"}
_FLOW_SELECTOR = {
    "oneOf": [
        {"type": "string", "minLength": 1, "maxLength": 4096},
        {"type": "object", "properties": {"node_index": {"type": "integer", "minimum": 0, "maximum": 4294967295}},
         "required": ["node_index"], "additionalProperties": False},
        {"type": "object", "properties": {"instance_id": _DECIMAL}, "required": ["instance_id"], "additionalProperties": False},
        {"type": "object", "properties": {"debug_name": {"type": "string", "minLength": 1, "maxLength": 4096}},
         "required": ["debug_name"], "additionalProperties": False},
    ]
}
_COMMON_ARGUMENTS = {
    "instance": _STRING,
    "capture": _STRING,
    "manifest": _STRING,
    "max_bytes": {"type": "integer", "minimum": 1024, "maximum": 1048576},
    "limit": {"type": "integer", "minimum": 1, "maximum": 1000},
    "cursor": _STRING,
}
_CAPTURE_ARGUMENTS = {
    "profile": {"enum": ["performance_diagnose", "performance_verify", "diagnostics_reproduce", "visual_inspect", "memory_diagnose", "startup_diagnose"]},
    "mode": {"enum": ["windowed", "cpu_scenario", "layout_scenario", "offscreen_vulkan"]},
    "window_ids": {"type": "array", "items": _DECIMAL, "maxItems": 32},
    "scenario_id": _STRING,
    "scenario": {"type": "object"},
    "duration_ns": _DECIMAL,
    "measurement_ticks": _DECIMAL,
    "warmup_ticks": _DECIMAL,
    "measurement": {"type": "object"},
    "warmup": {"type": "object"},
    "sections": {"type": "object"},
    "limits": {"type": "object"},
    "objective": {"type": ["object", "string"]},
    "idempotency_key": _STRING,
    "allow_degraded": {"type": "boolean"},
    "seed": _DECIMAL,
    "fixture_id": _STRING,
    "maximum_expected_tick_cadence": {"type": "integer", "minimum": 1, "maximum": 1000000},
    "content": {"type": "object", "properties": {field: {"type": "boolean"} for field in ["include_tree_text", "include_native_messages", "include_stacks", "include_source_paths", "include_interaction_labels", "include_interaction_outcomes"]}, "additionalProperties": False},
    "visual_sources": {"type": "object", "properties": {"window": {"type": "boolean"}, "viewports": {"type": "boolean"}}, "additionalProperties": False},
}


def _inline_local_schema(document: dict[str, Any]) -> dict[str, Any]:
    """Copy the finite authoritative schema into a nested MCP input without broken local refs."""
    def expand(value: Any, depth: int = 0) -> Any:
        if depth > 32:
            raise ValueError("Interaction schema reference nesting exceeds its bound")
        if isinstance(value, list):
            return [expand(member, depth + 1) for member in value]
        if not isinstance(value, dict):
            return value
        if "$ref" in value:
            reference = value["$ref"]
            if not reference.startswith("#/"):
                raise ValueError("Only finite local interaction schema references are supported")
            selected: Any = document
            for key in reference[2:].split("/"):
                selected = selected[key.replace("~1", "/").replace("~0", "~")]
            resolved = expand(selected, depth + 1)
            remaining = {key: member for key, member in value.items() if key != "$ref"}
            return {"allOf": [resolved, expand(remaining, depth + 1)]} if remaining else resolved
        return {key: expand(member, depth + 1) for key, member in value.items()
                if key not in {"$schema", "$id", "$defs", "title"}}

    return expand(document)


_PLAN_DOCUMENT = json.loads((Path(__file__).with_name("schemas") / "interaction-plan.schema.json").read_text())
_SOURCE_PLAN_DOCUMENT = copy.deepcopy(_PLAN_DOCUMENT)
_SOURCE_PLAN_DOCUMENT["properties"].pop("compiled", None)
for _definition in ["input", "step", "frame"]:
    for _redacted_field in ["text_redacted", "text_codepoints"]:
        _SOURCE_PLAN_DOCUMENT["$defs"][_definition]["properties"].pop(_redacted_field, None)
_INTERACTION_PLAN = _inline_local_schema(_SOURCE_PLAN_DOCUMENT)
_INTERACTION_TARGET = _inline_local_schema({"$defs": _PLAN_DOCUMENT["$defs"], "$ref": "#/$defs/target"})
_PLAN_ARTIFACT = {"type": "object", "properties": {
    "capture": _STRING, "artifact": _STRING,
    "sha256": {"type": "string", "pattern": "^[a-f0-9]{64}$"}},
    "required": ["capture", "artifact", "sha256"], "additionalProperties": False}


def _operation_tool_schema(contracts: dict[str, tuple[dict[str, Any], list[str]]]) -> dict[str, Any]:
    """Declare each operation's own bounded arguments and required identities."""
    branches = []
    for operation, (properties, required) in contracts.items():
        argument_schema = {"type": "object", "properties": {
            "max_bytes": _COMMON_ARGUMENTS["max_bytes"], **properties}, "additionalProperties": False}
        if required:
            argument_schema["required"] = required
        branches.append({"type": "object", "properties": {
            "operation": {"const": operation}, "arguments": argument_schema},
            "required": ["operation", "arguments"] if required else ["operation"], "additionalProperties": False})
    return {"type": "object", "properties": {"operation": {"enum": list(contracts)},
        "arguments": {"type": "object"}}, "required": ["operation"], "oneOf": branches,
        "additionalProperties": False}


_LIVE = {"instance": _STRING}
_PAGING = {key: _COMMON_ARGUMENTS[key] for key in ["limit", "cursor"]}
_OPTIONAL_CAPTURE = {"type": "object", "properties": _CAPTURE_ARGUMENTS, "additionalProperties": False}
_INTERACTION_CONTRACTS = {
    "interaction.capabilities": ({**_LIVE}, ["instance"]),
    "interaction.targets": ({**_LIVE, "limit": _COMMON_ARGUMENTS["limit"], "window_id": _DECIMAL, "target": _INTERACTION_TARGET}, ["instance", "window_id"]),
    "interaction.plan": ({"plan": _INTERACTION_PLAN}, ["plan"]),
    "interaction.start": ({**_LIVE, "plan": _INTERACTION_PLAN, "plan_artifact": _PLAN_ARTIFACT,
        "capture": _OPTIONAL_CAPTURE, "idempotency_key": _STRING, "rebind_run": {"type": "boolean"}}, ["instance", "idempotency_key"]),
    "interaction.record.start": ({**_LIVE, "window_id": _DECIMAL, "max_frames": _DECIMAL,
        "deadline_ns": _DECIMAL, "include_text": {"type": "boolean"}, "capture": _OPTIONAL_CAPTURE,
        "idempotency_key": _STRING}, ["instance", "window_id", "max_frames", "deadline_ns", "idempotency_key"]),
    "interaction.record.stop": ({**_LIVE, "job_id": _STRING}, ["instance", "job_id"]),
    "interaction.query": ({**_LIVE, **_PAGING, "job_id": _STRING, "capture": _STRING, "manifest": _STRING,
        "kind": {"enum": ["plan", "inputs", "observations", "results"]},
        "window_id": _DECIMAL, "app_tick": _DECIMAL, "window_frame": _DECIMAL,
        "sequence": _DECIMAL, "offset": _DECIMAL, "plan_digest": {"type": "string", "pattern": "^[a-f0-9]{64}$"}}, []),
}
_ACTION_CONTRACTS = {
    "action.list": ({**_LIVE, **_PAGING}, ["instance"]),
    "action.invoke": ({**_LIVE, "action_id": _STRING, "revision": _DECIMAL,
        "arguments": {"type": "object"}, "capture": _OPTIONAL_CAPTURE, "window_id": _DECIMAL,
        "idempotency_key": _STRING, "deadline_ns": _DECIMAL},
        ["instance", "action_id", "revision", "idempotency_key"]),
}


def _tool_schema(operations: list[str], properties: dict[str, Any]) -> dict[str, Any]:
    return {
        "type": "object",
        "properties": {
            "operation": {"enum": operations},
            "arguments": {
                "type": "object",
                "properties": {**_COMMON_ARGUMENTS, **properties},
                "additionalProperties": False,
            },
        },
        "required": ["operation"],
        "additionalProperties": False,
    }


TOOLS = {
    "flowui_capabilities": ("Discover offline or selected-run capabilities and status.", _tool_schema(["capabilities", "instances", "status"], {})),
    "flowui_context": ("Retrieve bounded library contracts and source selectors.", _tool_schema(["context.find", "context.read"], {"intent": _STRING, "topic": _STRING, "selector": _STRING})),
    "flowui_capture": ("Plan, start or stop a bounded evidence job.", _tool_schema(["capture.plan", "capture.start", "capture.stop"], {**_CAPTURE_ARGUMENTS, "job_id": _STRING})),
    "flowui_job": ("Recover job progress or request bounded cancellation.", _tool_schema(["job.status", "job.cancel"], {"job_id": _STRING})),
    "flowui_scenario": ("List or execute application-registered workloads in their declared mode.", _tool_schema(["scenario.list", "scenario.run"], _CAPTURE_ARGUMENTS)),
    "flowui_query": ("Query retained performance, diagnostics, memory or visual evidence.", _tool_schema(["query.performance", "query.diagnostics", "query.memory", "query.visual"], {"window": _DECIMAL, "window_id": _DECIMAL, "app_tick": _DECIMAL, "window_frame": _DECIMAL, "artifact": _STRING, "domain": {"enum": ["runtime", "build"]}, "kind": {"enum": ["frames", "trees", "themes", "catalogue", "samples", "events"]}, "severity": _STRING, "source_id": _STRING, "entity_id": _STRING, "filter": {"type": "object"}, "top": {"type": "integer", "minimum": 1, "maximum": 1000}})),
    "flowui_artifact": ("Read inventoried metadata/text or a bounded PNG image.", _tool_schema(["artifact.list", "artifact.read", "artifact.image"], {"artifact": _STRING, "image": _STRING, "format": {"const": "png"}, "include_image": {"type": "boolean"}, "crop": {"type": "object", "properties": {field: {"type": "integer", "minimum": 0, "maximum": 16384} for field in ["x", "y", "width", "height"]}, "required": ["x", "y", "width", "height"], "additionalProperties": False}})),
    "flowui_compare": ("Evaluate comparability and explicit pass/fail/inconclusive objectives.", _tool_schema(["compare", "objective.check", "visual.compare"], {"baseline": _STRING, "candidate": _STRING, "objective": {"type": ["object", "string"]}, "passes": {"type": "array", "items": _STRING, "maxItems": 100}, "minimum_passes": {"type": "integer", "minimum": 1, "maximum": 100}, "minimum_valid_intervals": _DECIMAL, "minimum_duration_ns": _DECIMAL, "require_pass": {"type": "boolean"}, "image": _STRING, "candidate_image": _STRING, "tolerance": {"type": "integer", "minimum": 0, "maximum": 255}})),
    "flowui_changes_preview": ("Review typed tooling changes with exact expected revisions.", _tool_schema(["changes.preview"], {"decisions": {"type": "array", "maxItems": 1024, "items": {"type": "object", "properties": {"key": _STRING, "decision": {"enum": ["include", "omit", "retain_disk", "remove"]}, "disk_revision": _STRING}, "required": ["key", "decision"], "additionalProperties": False}}})),
    "flowui_changes_apply": ("Commit an explicitly selected revision-checked tooling transaction.", _tool_schema(["changes.apply"], {"review_id": _STRING, "schema_generation": _DECIMAL, "proposal_revision": _DECIMAL, "theme_revision": _DECIMAL, "disk_revision": _STRING, "operation_id": _STRING})),
    "flowui_interaction": ("Discover, compile, execute or record finite frame-bound input plans. Start and recording operations acquire runtime control; planning and queries only read data.", _operation_tool_schema(_INTERACTION_CONTRACTS)),
    "flowui_action": ("Discover explicitly exported application actions or invoke a revision-checked action as a mutating evidence job.", _operation_tool_schema(_ACTION_CONTRACTS)),
}

for _branch in TOOLS["flowui_interaction"][1]["oneOf"]:
    if _branch["properties"]["operation"]["const"] == "interaction.start":
        _branch["properties"]["arguments"]["oneOf"] = [{"required": ["plan"]}, {"required": ["plan_artifact"]}]
        _branch["properties"]["arguments"]["not"] = {"properties": {
            "rebind_run": {"const": True}, "plan": {"properties": {"clock": {"const": "exact_window_frames"}}, "required": ["clock"]}},
            "required": ["rebind_run", "plan"]}
    elif _branch["properties"]["operation"]["const"] == "interaction.query":
        _branch["required"].append("arguments")
        _branch["properties"]["arguments"]["anyOf"] = [{"required": ["instance", "job_id"]}, {"required": ["capture"]}, {"required": ["manifest"]}]
        _branch["properties"]["arguments"]["oneOf"] = [{"required": ["job_id"]}, {"required": ["capture"]}, {"required": ["manifest"]}]

TOOLS["flowui_changes_apply"][1]["properties"]["arguments"]["required"] = [
    "instance", "review_id", "schema_generation", "proposal_revision", "theme_revision", "disk_revision", "operation_id"]
TOOLS["flowui_changes_apply"][1]["required"].append("arguments")
TOOLS["flowui_query"][1]["properties"]["arguments"]["properties"].update({"tree": _STRING, "flow_node": _FLOW_SELECTOR})
TOOLS["flowui_query"][1]["properties"]["arguments"]["properties"]["kind"]["enum"].append("subtree")
TOOLS["flowui_artifact"][1]["properties"]["arguments"]["properties"]["crop_flow_node"] = _FLOW_SELECTOR
TOOLS["flowui_artifact"][1]["properties"]["arguments"]["not"] = {"required": ["crop", "crop_flow_node"]}

