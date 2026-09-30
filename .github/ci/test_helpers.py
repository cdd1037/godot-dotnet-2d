from __future__ import annotations

import hashlib
import json
import os
import tempfile
import unittest
from pathlib import Path
from unittest.mock import patch

from checks import classify, docs_only
from draft_release import validate_existing_release, validate_tag, verify_assets
from pipeline import verify_ltcg


class HelperTests(unittest.TestCase):
    def test_documentation_filter_is_conservative(self):
        self.assertTrue(docs_only(["README.md", "doc/intro.rst"]))
        for paths in [
            [],
            ["SConstruct"],
            [".github/workflows/windows.yml"],
            ["modules/mono/foo.cs"],
            ["thirdparty/foo.c"],
            ["doc/classes/Node.xml"],
            ["tests/fixture.txt"],
        ]:
            self.assertFalse(docs_only(paths))

    def test_event_classification(self):
        with tempfile.TemporaryDirectory() as temp:
            event = Path(temp) / "event.json"
            output = Path(temp) / "output.txt"
            event.write_text(json.dumps({"before": "a" * 40}))
            for event_name, ref, files, expected in [
                ("push", "refs/heads/main", "README.md\0", "build=false\nfull=false"),
                ("pull_request", "refs/pull/1/merge", "modules/mono/code.cs\0", "build=true\nfull=false"),
                ("workflow_dispatch", "refs/heads/main", "README.md\0", "build=true\nfull=true"),
                ("push", "refs/tags/v4.7.2-custom.1", "README.md\0", "build=true\nfull=true"),
            ]:
                output.write_text("")
                with (
                    patch.dict(
                        os.environ,
                        {
                            "GITHUB_EVENT_NAME": event_name,
                            "GITHUB_EVENT_PATH": str(event),
                            "GITHUB_REF": ref,
                            "GITHUB_REF_NAME": ref.rsplit("/", 1)[-1],
                            "GITHUB_OUTPUT": str(output),
                        },
                        clear=True,
                    ),
                    patch("checks.git", return_value=files),
                ):
                    classify()
                self.assertEqual(output.read_text().strip(), expected)

    def test_ltcg_requires_real_code_generation(self):
        verify_ltcg("link /LTCG foo.obj\nGenerating code\nFinished generating code\n")
        for text in [
            "/LTCG",
            "Generating code\nFinished generating code",
            "/LTCG\nGenerating code\nFinished generating code\n-fno-use-linker-plugin",
        ]:
            with self.assertRaises(RuntimeError):
                verify_ltcg(text)

    def test_published_and_foreign_releases_are_rejected(self):
        draft = {"draft": True, "target_commitish": "abc", "body": "<!-- ci-source-sha: abc -->"}
        validate_existing_release(draft, "abc")
        with self.assertRaises(RuntimeError):
            validate_existing_release({**draft, "draft": False}, "abc")
        with self.assertRaises(RuntimeError):
            validate_existing_release(draft, "def")
        with self.assertRaises(RuntimeError):
            validate_existing_release({**draft, "body": "unverified"}, "abc")

    def test_release_tag_is_narrow(self):
        validate_tag("v4.7.2-custom.1")
        for tag in ["main", "v4.7.2", "v4.7.2-custom.x", "v4.7.2-custom.1/evil"]:
            with self.assertRaises(ValueError):
                validate_tag(tag)

    def test_release_checksums_and_provenance(self):
        with tempfile.TemporaryDirectory() as temp:
            root = Path(temp)
            files = {}
            for name in ["editor.zip", "nuget.zip", "template.zip"]:
                (root / name).write_bytes(b"test")
                files[name] = {"sha256": hashlib.sha256(b"test").hexdigest(), "bytes": 4}
            (root / "manifest.json").write_text(
                json.dumps({"repository": "owner/repo", "commit": "abc", "files": files})
            )
            entries = list(files) + ["manifest.json"]
            (root / "SHA256SUMS.txt").write_text(
                "".join(hashlib.sha256((root / name).read_bytes()).hexdigest() + "  " + name + "\n" for name in entries)
            )
            self.assertEqual(len(verify_assets(root, "owner/repo", "abc")), 5)
            with self.assertRaises(ValueError):
                verify_assets(root, "owner/repo", "wrong-commit")
            (root / "editor.zip").write_bytes(b"changed")
            with self.assertRaises(ValueError):
                verify_assets(root, "owner/repo", "abc")


if __name__ == "__main__":
    unittest.main()
