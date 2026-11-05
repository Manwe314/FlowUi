#!/usr/bin/env python3
"""Refresh or check finite MCP input schemas without requiring the optional SDK."""

import argparse
import importlib.util
import json
from pathlib import Path


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--check", action="store_true")
    arguments = parser.parse_args()
    package = Path(__file__).resolve().parent
    repository = package.parents[1]
    schema_directory = package / "src/flowui_agent_mcp/schemas"
    for source_name, packaged_name in [
        ("InteractionPlan.schema.json", "interaction-plan.schema.json"),
        ("VerificationRequest.schema.json", "verification-request.schema.json"),
    ]:
        source = repository / "include/devSystems/devAgenticWorkflow/schemas/v1" / source_name
        destination = schema_directory / packaged_name
        expected = json.dumps(json.loads(source.read_text()), indent=2) + "\n"
        if arguments.check:
            if not destination.is_file() or destination.read_text() != expected:
                raise SystemExit(f"Packaged {packaged_name} is stale; run tools/flowui-agent-mcp/update_schemas.py")
        else:
            destination.write_text(expected)

    specification = importlib.util.spec_from_file_location("flowui_mcp_contracts", package / "src/flowui_agent_mcp/contracts.py")
    module = importlib.util.module_from_spec(specification)
    specification.loader.exec_module(module)
    documents = {
        "tools.json": {name: {"description": description, "inputSchema": schema}
                       for name, (description, schema) in module.TOOLS.items()},
        "command-result.schema.json": module.RESULT_SCHEMA,
    }
    for name, document in documents.items():
        destination = schema_directory / name
        expected = json.dumps(document, indent=2) + "\n"
        if arguments.check:
            if not destination.is_file() or destination.read_text() != expected:
                raise SystemExit(f"Packaged MCP {name} is stale; run tools/flowui-agent-mcp/update_schemas.py")
        else:
            destination.write_text(expected)
    print(f"Validated {len(module.TOOLS)} MCP tool schemas and packaged workflow contracts")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
