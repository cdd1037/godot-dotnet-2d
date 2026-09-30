#!/usr/bin/env python3
"""Static checks for an arm64 .NET 10 Mono game APK (not a device runtime test)."""

import argparse
import hashlib
import json
import struct
import zipfile
from pathlib import Path

p = argparse.ArgumentParser()
p.add_argument("apk", type=Path)
p.add_argument("--template", action="store_true", help="No game Mono runtime payload is expected")
p.add_argument("--fixture", action="store_true", help="Also check the ETC2 validation texture")
a = p.parse_args()
with zipfile.ZipFile(a.apk) as z:
    names = z.namelist()
    assert not any(n.lower().endswith(".a") for n in names), "Static archive included in APK"
    libs = [n for n in names if n.startswith("lib/") and n.endswith(".so")]
    assert libs, "No native libraries"
    assert all(n.startswith("lib/arm64-v8a/") for n in libs), libs
    assert all("coreclr" not in n and "hostfxr" not in n for n in libs), "Desktop/CoreCLR runtime in APK"
    assert "lib/arm64-v8a/libgodot_android.so" in libs
    dex = b"".join(z.read(n) for n in names if n.endswith(".dex"))
    assert dex, "Java DEX payload missing"
    for cls in ("DotnetX509KeyManager", "PalPbkdf2", "DotnetProxyTrustManager"):
        assert ("Lnet/dot/android/crypto/" + cls + ";").encode() in dex, f"Crypto JNI class missing: {cls}"
    assert b"GodotGLRenderView;" not in dex, "Removed GLES renderer included"
    if not a.template:
        assert "lib/arm64-v8a/libmonosgen-2.0.so" in libs
        assert "lib/arm64-v8a/libSystem.Security.Cryptography.Native.Android.so" in libs
        assert any(n.endswith((".pck", ".sparsepck")) for n in names), "Managed resource pack missing"
        runtimeconfigs = [n for n in names if n.endswith(".runtimeconfig.json")]
        assert runtimeconfigs, "Managed runtime configuration missing"
        for n in runtimeconfigs:
            options = json.loads(z.read(n))["runtimeOptions"]
            assert options["tfm"] == "net10.0", n
            assert options["includedFrameworks"][0]["version"] == "10.0.12", n
        markers = [n for n in names if n.endswith(".godot-dotnet-publish-mode")]
        assert len(markers) == 1 and z.read(markers[0]).strip() == b"jit", "Invalid publish marker"
        if a.fixture:
            assert any(n.endswith(".etc2.ctex") for n in names), "ETC2 validation texture missing"
            assert not any(n.endswith(".s3tc.ctex") for n in names), "Desktop texture included in Android APK"
    records = []
    for n in libs:
        b = z.read(n)
        assert b[:6] == b"\x7fELF\x02\x01", n
        assert struct.unpack_from("<H", b, 18)[0] == 183, n
        phoff = struct.unpack_from("<Q", b, 32)[0]
        phentsize, phnum = struct.unpack_from("<HH", b, 54)
        aligns = [
            struct.unpack_from("<Q", b, phoff + i * phentsize + 48)[0]
            for i in range(phnum)
            if struct.unpack_from("<I", b, phoff + i * phentsize)[0] == 1
        ]
        assert aligns and min(aligns) >= 16384, (n, aligns)
        records.append({"name": n, "bytes": len(b), "sha256": hashlib.sha256(b).hexdigest(), "load_alignments": aligns})
print(
    json.dumps(
        {
            "apk": str(a.apk),
            "bytes": a.apk.stat().st_size,
            "sha256": hashlib.sha256(a.apk.read_bytes()).hexdigest(),
            "libraries": records,
            "device_runtime_test": False,
        },
        indent=2,
    )
)
