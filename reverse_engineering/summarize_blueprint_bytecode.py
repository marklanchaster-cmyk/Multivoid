#!/usr/bin/env python3
"""Summarize selected UAssetAPI Blueprint function exports.

This intentionally reports symbolic structure rather than attempting to turn
Kismet bytecode back into source.  It is useful for correlating native Ghidra
findings with cooked Blueprint assets.
"""

from __future__ import annotations

import argparse
import collections
import json
from pathlib import Path
from typing import Any, Iterable


def walk(value: Any) -> Iterable[dict[str, Any]]:
    if isinstance(value, dict):
        yield value
        for child in value.values():
            yield from walk(child)
    elif isinstance(value, list):
        for child in value:
            yield from walk(child)


def short_type(value: Any) -> str | None:
    if not isinstance(value, str):
        return None
    return value.split(",", 1)[0].rsplit(".", 1)[-1]


def object_name(value: Any) -> str:
    if isinstance(value, str):
        return value
    if isinstance(value, dict):
        for key in ("Value", "Name", "ObjectName"):
            if key in value:
                return object_name(value[key])
    return str(value)


def resolve_package_index(doc: dict[str, Any], index: Any) -> str:
    if not isinstance(index, int) or index == 0:
        return str(index)
    collection = doc.get("Exports", []) if index > 0 else doc.get("Imports", [])
    offset = index - 1 if index > 0 else -index - 1
    if not 0 <= offset < len(collection):
        return f"{index} (out of range)"
    entry = collection[offset]
    return f"{index} ({object_name(entry.get('ObjectName', '?'))})"


def unique_in_order(values: Iterable[str]) -> list[str]:
    return list(dict.fromkeys(values))


def scalar_literals(nodes: Iterable[dict[str, Any]]) -> list[str]:
    result: list[str] = []
    literal_types = {
        "EX_IntConst", "EX_IntConstByte", "EX_IntZero", "EX_IntOne",
        "EX_FloatConst", "EX_DoubleConst", "EX_ByteConst", "EX_True",
        "EX_False", "EX_StringConst", "EX_UnicodeStringConst", "EX_NameConst",
    }
    for node in nodes:
        kind = short_type(node.get("$type"))
        if kind not in literal_types:
            continue
        if kind in {"EX_True", "EX_False", "EX_IntZero", "EX_IntOne"}:
            result.append(kind.removeprefix("EX_"))
        elif "Value" in node and not isinstance(node["Value"], (dict, list)):
            result.append(f"{kind}={node['Value']!r}")
    return unique_in_order(result)


def summarize_function(doc: dict[str, Any], export: dict[str, Any]) -> list[str]:
    nodes = list(walk(export.get("ScriptBytecode", [])))
    types = collections.Counter(
        kind for node in nodes if (kind := short_type(node.get("$type")))
    )
    properties: list[str] = []
    virtual_calls: list[str] = []
    stack_calls: list[str] = []
    for node in nodes:
        path = node.get("Path")
        if short_type(node.get("$type")) == "FFieldPath" and isinstance(path, list):
            rendered_path = ".".join(object_name(part) for part in path)
            if rendered_path:
                properties.append(rendered_path)
        if "VirtualFunctionName" in node:
            virtual_calls.append(object_name(node["VirtualFunctionName"]))
        if "StackNode" in node:
            stack_calls.append(resolve_package_index(doc, node["StackNode"]))

    name = object_name(export.get("ObjectName", "?"))
    lines = [
        f"## `{name}`",
        "",
        f"- Bytecode size: {export.get('ScriptBytecodeSize', '?')} bytes",
        f"- Serialized export: offset {export.get('SerialOffset', '?')}, size {export.get('SerialSize', '?')}",
        f"- Field/property paths: {', '.join(f'`{x}`' for x in unique_in_order(properties)) or '(none)'}",
        f"- Virtual calls: {', '.join(f'`{x}`' for x in unique_in_order(virtual_calls)) or '(none)'}",
        f"- Resolved stack nodes: {', '.join(f'`{x}`' for x in unique_in_order(stack_calls)) or '(none)'}",
        f"- Literals: {', '.join(f'`{x}`' for x in scalar_literals(nodes)) or '(none)'}",
        "- Expression mix: " + ", ".join(
            f"`{kind}` x{count}" for kind, count in types.most_common()
            if kind.startswith("EX_")
        ),
        "",
    ]
    return lines


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("input", type=Path, help="UAssetAPI JSON export")
    parser.add_argument("output", type=Path, help="Markdown output")
    parser.add_argument("functions", nargs="+", help="Function export names")
    args = parser.parse_args()

    doc = json.loads(args.input.read_text(encoding="utf-8"))
    exports = {
        object_name(item.get("ObjectName", "")): item
        for item in doc.get("Exports", [])
        if short_type(item.get("$type")) == "FunctionExport"
    }
    missing = [name for name in args.functions if name not in exports]

    lines = [
        "# Blueprint bytecode symbol summary",
        "",
        f"Source: `{args.input}`",
        "",
        "This is a structural inventory of UAssetAPI's decoded Kismet bytecode; it is not native Ghidra output and does not claim source-level control flow.",
        "",
    ]
    for name in args.functions:
        if name in exports:
            lines.extend(summarize_function(doc, exports[name]))
    if missing:
        lines.extend(["## Missing requested functions", "", ", ".join(f"`{x}`" for x in missing), ""])

    args.output.parent.mkdir(parents=True, exist_ok=True)
    args.output.write_text("\n".join(lines), encoding="utf-8")
    if missing:
        print("Missing: " + ", ".join(missing))
        return 2
    print(f"Wrote {args.output}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
