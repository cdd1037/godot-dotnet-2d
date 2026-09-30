from __future__ import annotations

import hashlib
import json
import os
import tempfile
import unittest
from pathlib import Path
from unittest.mock import patch
from xml.etree import ElementTree as ET

import pipeline
from checks import classify, docs_only
from draft_release import validate_existing_release, validate_tag, verify_assets
from pipeline import verify_ltcg


class HelperTests(unittest.TestCase):
    def test_scons_creates_cache_parent_on_clean_and_repeated_runs(self):
        for target in ["editor", "template_release"]:
            with self.subTest(target=target), tempfile.TemporaryDirectory() as temp:
                root = Path(temp)
                cache = root / ".scons-cache"
                self.assertFalse(cache.exists())
                with patch.object(pipeline, "ROOT", root):
                    first = pipeline.scons(target)
                    self.assertTrue(cache.is_dir())
                    sentinel = cache / "existing-cache-entry"
                    sentinel.write_text("preserved", encoding="utf-8")
                    self.assertEqual(pipeline.scons(target), first)
                    self.assertEqual(sentinel.read_text(encoding="utf-8"), "preserved")
                self.assertIn(f"target={target}", first)

    def test_scons_accesskit_path_is_absolute_and_independent_of_working_directory(self):
        with tempfile.TemporaryDirectory() as temp:
            root = Path(temp) / "checkout"
            unrelated = Path(temp) / "elsewhere"
            unrelated.mkdir()
            previous = Path.cwd()
            try:
                os.chdir(unrelated)
                with patch.object(pipeline, "ROOT", root):
                    for target in ["editor", "template_release"]:
                        with self.subTest(target=target):
                            command = pipeline.scons(target)
                            argument = next(arg for arg in command if arg.startswith("accesskit_sdk_path="))
                            sdk = Path(argument.split("=", 1)[1])
                            self.assertTrue(sdk.is_absolute())
                            self.assertEqual(sdk, root / "bin/build_deps/accesskit")
            finally:
                os.chdir(previous)

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

    def test_full_ci_marker_is_branch_push_title_only(self):
        with tempfile.TemporaryDirectory() as temp:
            event = Path(temp) / "event.json"
            output = Path(temp) / "output.txt"
            for event_name, ref, head_commit, expected in [
                ("push", "refs/heads/main", {"message": "[full-ci] Validate .NET 10"}, True),
                ("push", "refs/heads/check", {"message": "Validate .NET 10 [full-ci]"}, True),
                ("push", "refs/heads/main", {"message": "Docs\n\n[full-ci]"}, False),
                ("push", "refs/heads/main", {"message": "full-ci without brackets"}, False),
                ("push", "refs/heads/main", None, False),
                ("pull_request", "refs/pull/1/merge", {"message": "[full-ci] Untrusted title"}, False),
                ("pull_request", "refs/heads/main", {"message": "[full-ci] Untrusted title"}, False),
                ("push", "refs/pull/1/merge", {"message": "[full-ci] Not a branch"}, False),
            ]:
                event.write_text(json.dumps({"before": "a" * 40, "head_commit": head_commit}))
                output.write_text("")
                with (
                    patch.dict(
                        os.environ,
                        {
                            "GITHUB_EVENT_NAME": event_name,
                            "GITHUB_EVENT_PATH": str(event),
                            "GITHUB_REF": ref,
                            "GITHUB_OUTPUT": str(output),
                        },
                        clear=True,
                    ),
                    patch("checks.git", return_value="README.md\0"),
                ):
                    classify()
                value = str(expected).lower()
                self.assertEqual(output.read_text().strip(), f"build={value}\nfull={value}")

    def test_ltcg_requires_real_code_generation(self):
        verify_ltcg("link /LTCG foo.obj\nGenerating code\nFinished generating code\n")
        for text in [
            "/LTCG",
            "Generating code\nFinished generating code",
            "/LTCG\nGenerating code\nFinished generating code\n-fno-use-linker-plugin",
        ]:
            with self.assertRaises(RuntimeError):
                verify_ltcg(text)

    def test_smoke_preparation_creates_exportable_solution(self):
        with tempfile.TemporaryDirectory() as temp:
            root = Path(temp)
            smoke = root / "isolated-output" / "smoke"
            fixtures = root / ".github/ci/smoke"
            fixtures.mkdir(parents=True)
            (fixtures / "project.godot").write_text("config_version=5\n")
            versions = root / "modules/mono/SdkPackageVersions.props"
            versions.parent.mkdir(parents=True)
            versions.write_text(
                "<Project><PropertyGroup><PackageVersion_Godot_NET_Sdk>4.7.2-2dtrim.2"
                "</PackageVersion_Godot_NET_Sdk></PropertyGroup></Project>"
            )
            with patch.multiple(pipeline, ROOT=root, SMOKE=smoke), patch("pipeline.run") as run:
                pipeline.prepare_smoke()
                pipeline.prepare_smoke()
            self.assertEqual(run.call_count, 4)
            expected = [
                (
                    ["dotnet", "new", "sln", "--format", "sln", "--name", "CiSmoke", "--output", smoke, "--force"],
                    "smoke-solution",
                    60,
                ),
                (["dotnet", "sln", smoke / "CiSmoke.sln", "add", smoke / "CiSmoke.csproj"], "smoke-solution-add", 60),
            ]
            self.assertEqual([call.args for call in run.call_args_list], expected * 2)
            self.assertEqual(ET.parse(smoke / "CiSmoke.csproj").getroot().get("Sdk"), "Godot.NET.Sdk/4.7.2-2dtrim.2")
            feed = ET.parse(smoke / "NuGet.Config").find(".//packageSources/add[@key='fork']")
            self.assertEqual(feed.get("value"), str(root / "bin/GodotSharp/Tools/nupkgs"))

    def test_full_export_runs_three_modes_on_one_template(self):
        with tempfile.TemporaryDirectory() as temp:
            root = Path(temp)
            smoke = root / "smoke"
            smoke.mkdir()
            with patch.multiple(pipeline, ROOT=root, SMOKE=smoke), patch("pipeline.run", return_value="") as run:
                pipeline.export()
            self.assertEqual(run.call_count, 6)
            calls = [call.args for call in run.call_args_list]
            self.assertEqual(
                [calls[i][1] for i in [0, 2, 4]], ["smoke-export", "smoke-export-trimmed-jit", "smoke-export-aot"]
            )
            self.assertNotIn("--expect-aot", calls[1][0])
            self.assertNotIn("--expect-aot", calls[3][0])
            self.assertIn("--expect-aot", calls[5][0])
            self.assertTrue(all(calls[i][3] == pipeline.MARKER for i in [1, 3, 5]))
            self.assertIn("dotnet/publish_mode=2", (smoke / "export_presets.cfg").read_text())

    def test_export_rejects_logged_errors_even_with_zero_exit(self):
        with tempfile.TemporaryDirectory() as temp:
            root = Path(temp)
            smoke = root / "smoke"
            smoke.mkdir()
            with (
                patch.multiple(pipeline, ROOT=root, SMOKE=smoke),
                patch("pipeline.run", return_value="ERROR: failed") as run,
            ):
                with self.assertRaises(RuntimeError):
                    pipeline.export()
            self.assertEqual(run.call_count, 1)

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
