"""Sparse-index and payload-integrity negative controls; no device claims."""

import hashlib
import struct
import tempfile
import unittest
from pathlib import Path

from repack_ci_payload import make_index, parse_index, verified_payload


def header():
    return b"GDPC" + struct.pack("<IIIIIQQ", 4, 4, 7, 2, 6, 0, 104) + bytes(64)


class SparseRepackTests(unittest.TestCase):
    def test_round_trip_and_asset_integrity(self):
        entries = {"Main.cs": b"", ".godot/mono/publish/arm64/Game.dll": b"managed"}
        data = make_index(header(), entries)
        assets = {"assets/" + name: content for name, content in entries.items()}
        parsed_header, parsed = parse_index(data, assets)
        self.assertEqual(parsed_header, header())
        self.assertEqual(parsed, entries)
        assets["assets/Main.cs"] = b"changed"
        with self.assertRaises(ValueError):
            parse_index(data, assets)

    def test_encrypted_and_nonsparse_rejected(self):
        for flags in (2, 7):
            data = bytearray(make_index(header(), {}))
            struct.pack_into("<I", data, 20, flags)
            with self.assertRaises(ValueError):
                parse_index(data, {})

    def test_traversal_rejected(self):
        with self.assertRaises(ValueError):
            parse_index(make_index(header(), {"../outside": b"x"}), {"assets/../outside": b"x"})

    def test_duplicate_path_and_trailer_rejected(self):
        data = make_index(header(), {"one": b"x"})
        duplicate = bytearray(data + data[108:])
        struct.pack_into("<I", duplicate, 104, 2)
        with self.assertRaises(ValueError):
            parse_index(duplicate, {"assets/one": b"x"})
        with self.assertRaises(ValueError):
            parse_index(data + b"trailer", {"assets/one": b"x"})

    def test_payload_path_forms_and_duplicates_rejected(self):
        with tempfile.TemporaryDirectory() as tmp:
            directory = Path(tmp)
            for name in ("../outside", "/outside", "C:\\outside", "nested\\outside"):
                report = {
                    "mode": "jit",
                    "rooted_entrypoint": True,
                    "files": [{"name": name, "bytes": 0, "sha256": "unused"}],
                }
                with self.subTest(name=name), self.assertRaises(ValueError):
                    verified_payload(directory, report, "jit")
            name = ".godot-dotnet-publish-mode"
            (directory / name).write_bytes(b"jit\n")
            record = {"name": name, "bytes": 4, "sha256": hashlib.sha256(b"jit\n").hexdigest()}
            report = {"mode": "jit", "rooted_entrypoint": True, "files": [record, record]}
            with self.assertRaises(ValueError):
                verified_payload(directory, report, "jit")

    def test_payload_hashes_and_complete_inventory(self):
        with tempfile.TemporaryDirectory() as tmp:
            directory = Path(tmp)
            marker = directory / ".godot-dotnet-publish-mode"
            marker.write_bytes(b"jit\n")
            report = {
                "mode": "jit",
                "rooted_entrypoint": True,
                "files": [{"name": marker.name, "bytes": 4, "sha256": hashlib.sha256(b"jit\n").hexdigest()}],
            }
            self.assertEqual(verified_payload(directory, report, "jit"), {marker.name: b"jit\n"})
            with self.assertRaises(ValueError):
                verified_payload(directory, report, "trimmed-jit")
            marker.write_bytes(b"aot\n")
            with self.assertRaises(ValueError):
                verified_payload(directory, report, "jit")
            marker.write_bytes(b"jit\n")
            (directory / "unlisted.dll").write_bytes(b"extra")
            with self.assertRaises(ValueError):
                verified_payload(directory, report, "jit")


if __name__ == "__main__":
    unittest.main()
