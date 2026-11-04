#!/usr/bin/env python3
"""Refresh or check the finite agentic knowledge pack against its source checkout."""

import argparse
import hashlib
import json
import re
from pathlib import Path


def digest(contents: bytes) -> str:
    return hashlib.sha256(contents).hexdigest()


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--check", action="store_true")
    parser.add_argument("--root", type=Path, default=Path(__file__).resolve().parents[2])
    arguments = parser.parse_args()
    repository = arguments.root.resolve()
    pack = repository / "include/devSystems/devAgenticWorkflow/knowledge"
    index = json.loads((pack / "index.json").read_text())
    source_map = json.loads((pack / "source-map.json").read_text())
    failures = []
    changed = False
    for topic in index["topics"]:
        topic_path = (pack / topic["path"]).resolve()
        if not topic_path.is_relative_to(pack) or not topic_path.is_file():
            failures.append(f"invalid topic path: {topic['topic_id']}")
            continue
        contents = topic_path.read_bytes()
        expected = {"bytes": str(len(contents)), "sha256": digest(contents)}
        for field, value in expected.items():
            if topic.get(field) != value:
                if arguments.check:
                    failures.append(f"stale topic {field}: {topic['topic_id']}")
                topic[field] = value
                changed = True
        for target in re.findall(r"\]\(([^)]+)\)", contents.decode()):
            path = target.split("#", 1)[0]
            if "://" in path:
                continue
            resolved = (repository / path).resolve()
            if not resolved.is_relative_to(repository) or not resolved.is_file():
                failures.append(f"broken source link: {topic['topic_id']}: {target}")
    for selector in source_map["selectors"]:
        source_path = (repository / selector["file"]).resolve()
        if not source_path.is_relative_to(repository) or not source_path.is_file():
            failures.append(f"missing selector source: {selector['selector_id']}")
            continue
        contents = source_path.read_bytes()
        symbol_name = selector["symbol"].split("::")[-1]
        if not re.search(r"\b" + re.escape(symbol_name) + r"\b", contents.decode()):
            failures.append(f"missing selector symbol: {selector['selector_id']}")
        source_digest = digest(contents)
        if selector.get("sha256") != source_digest:
            if arguments.check:
                failures.append(f"stale source digest: {selector['selector_id']}")
            selector["sha256"] = source_digest
            changed = True
    if failures:
        for failure in failures:
            print(failure)
        return 1
    if changed and not arguments.check:
        (pack / "index.json").write_text(json.dumps(index, indent=2) + "\n")
        (pack / "source-map.json").write_text(json.dumps(source_map, indent=2) + "\n")
    print(f"Validated {len(index['topics'])} topics and {len(source_map['selectors'])} source selectors")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
