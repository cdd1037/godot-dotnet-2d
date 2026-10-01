"""Synthetic negative controls for the APK inventory checker, not real APK tests."""

import json
import struct
import subprocess
import sys
import tempfile
import unittest
import zipfile
from pathlib import Path

CHECKER = Path(__file__).with_name("check_apk.py")


def elf(alignment=16384):
    data = bytearray(120)
    data[:6] = b"\x7fELF\x02\x01"
    struct.pack_into("<H", data, 18, 183)
    struct.pack_into("<Q", data, 32, 64)
    struct.pack_into("<HH", data, 54, 56, 1)
    struct.pack_into("<I", data, 64, 1)
    struct.pack_into("<Q", data, 112, alignment)
    return bytes(data)


class ApkCheckerTests(unittest.TestCase):
    def check(self, marker="jit", expected="jit", mutate=None):
        entries = {
            "lib/arm64-v8a/libgodot_android.so": elf(),
            "lib/arm64-v8a/libmonosgen-2.0.so": elf(),
            "lib/arm64-v8a/libSystem.Security.Cryptography.Native.Android.so": elf(),
            "classes.dex": b"Lnet/dot/android/crypto/DotnetX509KeyManager;"
            b"Lnet/dot/android/crypto/PalPbkdf2;Lnet/dot/android/crypto/DotnetProxyTrustManager;",
            "assets/managed.sparsepck": b"test inventory only",
            "assets/AndroidMonoSmoke.runtimeconfig.json": json.dumps({
                "runtimeOptions": {"tfm": "net10.0", "includedFrameworks": [{"version": "10.0.12"}]}
            }).encode(),
            "assets/.godot-dotnet-publish-mode": (marker + "\n").encode(),
        }
        if mutate:
            mutate(entries)
        with tempfile.TemporaryDirectory() as tmp:
            apk = Path(tmp) / "synthetic.apk"
            with zipfile.ZipFile(apk, "w") as archive:
                for name, data in entries.items():
                    archive.writestr(name, data)
            return subprocess.run(
                [sys.executable, CHECKER, apk, "--publish-mode", expected],
                text=True,
                stdout=subprocess.PIPE,
                stderr=subprocess.STDOUT,
            )

    def test_both_explicit_mono_modes(self):
        for mode in ("jit", "trimmed-jit"):
            with self.subTest(mode=mode):
                result = self.check(mode, mode)
                self.assertEqual(result.returncode, 0, result.stdout)
                report = json.loads(result.stdout)
                self.assertEqual(report["publish_mode"], mode)
                self.assertFalse(report["device_runtime_test"])

    def test_wrong_marker_rejected(self):
        self.assertNotEqual(self.check("jit", "trimmed-jit").returncode, 0)
        self.assertNotEqual(self.check("aot", "jit").returncode, 0)

    def test_missing_crypto_java_class_rejected(self):
        self.assertNotEqual(self.check(mutate=lambda e: e.update({"classes.dex": b"incomplete"})).returncode, 0)

    def test_desktop_runtime_rejected(self):
        self.assertNotEqual(self.check(mutate=lambda e: e.update({"lib/arm64-v8a/libcoreclr.so": elf()})).returncode, 0)

    def test_low_alignment_rejected(self):
        self.assertNotEqual(
            self.check(mutate=lambda e: e.update({"lib/arm64-v8a/libmonosgen-2.0.so": elf(4096)})).returncode, 0
        )

    def test_static_archive_rejected(self):
        self.assertNotEqual(self.check(mutate=lambda e: e.update({"assets/runtime.a": b"archive"})).returncode, 0)

    def test_missing_native_crypto_rejected(self):
        self.assertNotEqual(
            self.check(
                mutate=lambda e: e.pop("lib/arm64-v8a/libSystem.Security.Cryptography.Native.Android.so")
            ).returncode,
            0,
        )


if __name__ == "__main__":
    unittest.main()
