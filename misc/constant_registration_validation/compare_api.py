#!/usr/bin/env python3
"""Compare full extension API dumps and verbose ClassDB hashes, without rebuilding."""

import argparse
import hashlib
import json
import re
from pathlib import Path


def read_snapshot(directory):
    api_path = directory / "extension_api.json"
    api = json.loads(api_path.read_text())
    hashes = dict(re.findall(r"(CORE|EDITOR) API HASH: (\d+)", (directory / "api-dump.log").read_text()))
    if set(hashes) != {"CORE", "EDITOR"}:
        raise SystemExit(f"Missing core/editor API hashes in {directory}")
    return api, hashes, hashlib.sha256(api_path.read_bytes()).hexdigest()


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("before", type=Path)
    parser.add_argument("after", type=Path)
    args = parser.parse_args()
    before, before_hashes, before_sha = read_snapshot(args.before)
    after, after_hashes, after_sha = read_snapshot(args.after)
    changed = [key for key in sorted(before.keys() | after.keys()) if before.get(key) != after.get(key)]
    if changed or before_hashes != after_hashes:
        raise SystemExit(f"API mismatch: changed sections={changed}; hashes={before_hashes} -> {after_hashes}")
    print(
        json.dumps(
            {
                "full_api_equal": True,
                "api_hashes": after_hashes,
                "before_json_sha256": before_sha,
                "after_json_sha256": after_sha,
                "global_constants": len(after["global_constants"]),
                "global_enums": len(after["global_enums"]),
                "global_enum_values": sum(len(enum["values"]) for enum in after["global_enums"]),
                "classes": len(after["classes"]),
                "class_constants": sum(len(cls.get("constants", [])) for cls in after["classes"]),
                "class_enums": sum(len(cls.get("enums", [])) for cls in after["classes"]),
                "class_enum_values": sum(
                    len(enum["values"]) for cls in after["classes"] for enum in cls.get("enums", [])
                ),
            },
            indent=2,
        )
    )


if __name__ == "__main__":
    main()
