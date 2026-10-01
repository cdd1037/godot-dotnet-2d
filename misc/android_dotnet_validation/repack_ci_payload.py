#!/usr/bin/env python3
"""TEST ONLY: repackage a verified CI payload into an unsigned fixture APK.

This is not an automatic Godot trimmed-export or device-runtime test. Run it for
BOTH CI modes against the same genuine APK so ZIP policies are matched. It never
replaces the production exporter or pretends a local dotnet publish succeeded.
"""

import argparse
import base64
import hashlib
import json
import re
import struct
import subprocess
import zipfile
from pathlib import Path, PurePosixPath

PREFIX = ".godot/mono/publish/arm64/"
INDEX = "assets/assets.sparsepck"


def parse_index(data, assets):
    if len(data) < 108 or data[:4] != b"GDPC":
        raise ValueError("Not a supported sparse PCK")
    version = struct.unpack_from("<I", data, 4)[0]
    flags, base, directory = struct.unpack_from("<IQQ", data, 20)
    if (version, flags, base, directory) != (4, 6, 0, 104):
        raise ValueError("Only plain, unencrypted V4 sparse fixture packs are supported")
    count = struct.unpack_from("<I", data, directory)[0]
    if count > 100000:
        raise ValueError("Unreasonable file count")
    cursor = directory + 4
    entries = {}
    for _ in range(count):
        length = struct.unpack_from("<I", data, cursor)[0]
        cursor += 4
        if not 0 < length <= 4096 or length % 4:
            raise ValueError("Invalid sparse path length")
        path = data[cursor : cursor + length].rstrip(b"\0").decode("utf-8")
        cursor += length
        offset, size, digest, entry_flags = struct.unpack_from("<QQ16sI", data, cursor)
        cursor += 36
        if path in entries or path.startswith("/") or ".." in PurePosixPath(path).parts:
            raise ValueError("Duplicate or unsafe sparse path")
        content = assets["assets/" + path]
        if offset or entry_flags or size != len(content) or digest != hashlib.md5(content).digest():
            raise ValueError(f"Invalid sparse entry: {path}")
        entries[path] = content
    if cursor != len(data):
        raise ValueError("Unexpected sparse pack trailer")
    return data[:directory], entries


def make_index(header, entries):
    data = bytearray(header + struct.pack("<I", len(entries)))
    for path, content in sorted(entries.items()):
        name = path.encode("utf-8")
        name += b"\0" * (-len(name) % 4)
        data += struct.pack("<I", len(name)) + name
        data += struct.pack("<QQ16sI", 0, len(content), hashlib.md5(content).digest(), 0)
    return bytes(data)


def verified_payload(directory, report, mode):
    if report["mode"] != mode or report["rooted_entrypoint"] is not True:
        raise ValueError("CI payload mode/entrypoint report mismatch")
    result = {}
    for record in report["files"]:
        path = PurePosixPath(record["name"])
        if path.is_absolute() or ".." in path.parts or "\\" in str(path) or ":" in str(path) or str(path) in result:
            raise ValueError("Unsafe or duplicate payload path")
        local = directory / path
        if not local.resolve().is_relative_to(directory.resolve()):
            raise ValueError("Payload symlink escapes its directory")
        content = local.read_bytes()
        if len(content) != record["bytes"] or hashlib.sha256(content).hexdigest() != record["sha256"]:
            raise ValueError(f"CI payload content mismatch: {path}")
        result[str(path)] = content
    actual = {p.relative_to(directory).as_posix() for p in directory.rglob("*") if p.is_file()}
    if actual != set(result):
        raise ValueError("Unlisted or missing CI payload files")
    if result[".godot-dotnet-publish-mode"].strip() != mode.encode():
        raise ValueError("CI publish marker mismatch")
    return result


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("baseline_apk", type=Path)
    parser.add_argument("publish", type=Path)
    parser.add_argument("output", type=Path, help="New unsigned, aligned APK")
    parser.add_argument("--mode", required=True, choices=["jit", "trimmed-jit"])
    parser.add_argument("--report", type=Path, required=True)
    parser.add_argument("--comparison", type=Path, required=True)
    parser.add_argument("--expected-source", required=True, help="Verified CI source commit")
    parser.add_argument("--zipalign", type=Path, required=True)
    args = parser.parse_args()
    if not re.fullmatch(r"[0-9a-f]{40}", args.expected_source):
        parser.error("Expected source must be the full verified CI commit SHA")
    raw = args.baseline_apk.read_bytes()
    if b"APK Sig Block 42" in raw:
        parser.error("Signed APKs are unsupported")
    comparison = json.loads(args.comparison.read_text(encoding="utf-8"))
    if (
        comparison["source_commit"] != args.expected_source
        or comparison["runtime"] != "10.0.12"
        or comparison["sdk"] != "10.0.401"
        or comparison["rid"] != "android-arm64"
    ):
        parser.error("CI source/runtime provenance mismatch")
    if comparison["native_runtime_identical"] is not True or comparison["crypto_jar_matches_template"] is not True:
        parser.error("CI pairing checks did not pass")
    payload = verified_payload(args.publish, json.loads(args.report.read_text(encoding="utf-8")), args.mode)
    with zipfile.ZipFile(args.baseline_apk) as source:
        if len(source.namelist()) != len(set(source.namelist())):
            parser.error("Duplicate ZIP names")
        assets = {name: source.read(name) for name in source.namelist()}
        methods = {info.filename: info.compress_type for info in source.infolist()}
    if any(name.startswith("META-INF/") and name.upper().endswith((".RSA", ".DSA", ".EC", ".SF")) for name in assets):
        parser.error("Signed APKs are unsupported")
    header, entries = parse_index(assets[INDEX], assets)
    native = {name: data for name, data in assets.items() if name.startswith("lib/")}
    for name, content in payload.items():
        if name.endswith((".so", ".dex", ".jar")):
            destination = "lib/arm64-v8a/" + name
            if assets.get(destination) != content:
                parser.error(f"Baseline native/DEX payload differs: {destination}")
    for name in list(entries):
        if name.startswith(PREFIX):
            del entries[name]
            del assets["assets/" + name]
    manifest = []
    for name, content in sorted(payload.items()):
        if name.endswith((".a", ".jar", ".so", ".dex", ".pdb", ".dbg")):
            continue
        entries[PREFIX + name] = content
        assets["assets/" + PREFIX + name] = content
        manifest.append(name + "\t" + base64.b64encode(hashlib.sha512(content).digest()).decode() + "\n")
    manifest_path = PREFIX + ".dotnet-publish-manifest"
    entries[manifest_path] = "".join(manifest).encode()
    assets["assets/" + manifest_path] = entries[manifest_path]
    assets[INDEX] = make_index(header, entries)
    parse_index(assets[INDEX], assets)
    if native != {name: data for name, data in assets.items() if name.startswith("lib/")}:
        raise ValueError("Native payload changed")
    intermediate = args.output.with_suffix(".unaligned.apk")
    evidence = args.output.with_suffix(".repack.json")
    if any(p.exists() for p in (args.output, intermediate, evidence)):
        parser.error("Output/evidence paths already exist")
    with zipfile.ZipFile(intermediate, "x") as destination:
        for name, content in sorted(assets.items()):
            info = zipfile.ZipInfo(name, (1980, 1, 1, 0, 0, 0))
            info.compress_type = methods.get(name, zipfile.ZIP_DEFLATED)
            info.external_attr = 0o100644 << 16
            destination.writestr(info, content, compresslevel=6)
    subprocess.run([str(args.zipalign), "-P", "16", "4", str(intermediate), str(args.output)], check=True)
    subprocess.run([str(args.zipalign), "-c", "-P", "16", "4", str(args.output)], check=True)
    with zipfile.ZipFile(args.output) as output:
        if output.testzip() is not None:
            raise ValueError("ZIP CRC validation failed")
        parse_index(output.read(INDEX), {name: output.read(name) for name in output.namelist()})
    evidence.write_text(
        json.dumps(
            {
                "method": "test-only CI payload repack",
                "mode": args.mode,
                "ci_source_commit": args.expected_source,
                "baseline_apk_sha256": hashlib.sha256(raw).hexdigest(),
                "apk_sha256": hashlib.sha256(args.output.read_bytes()).hexdigest(),
                "apk_bytes": args.output.stat().st_size,
                "native_payload_unchanged": True,
                "sparse_index_verified": True,
                "automatic_trimmed_export_test": False,
                "android_device_runtime_test": False,
            },
            indent=2,
        )
        + "\n",
        encoding="utf-8",
    )
    print(evidence.read_text(encoding="utf-8"))


if __name__ == "__main__":
    main()
