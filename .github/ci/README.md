# Windows-only CI for this fork

This replaces the upstream multi-platform workflow matrix. It changes CI only;
Linux/macOS source support and the historical MinGW comparison packages remain.
The Android work is separate and is not built here.

## Triggers and outputs

- Ordinary branch pushes and pull requests: fast Python/SCons syntax checks,
  changed-file formatting and Python style checks, workflow/actionlint checks,
  helper tests, and four retained raw/RenderingDevice shader-generator fixtures.
- Any non-documentation change conservatively builds one Windows x64 editor,
  generates its C# glue, builds the matching GodotSharp/SDK/source-generator
  packages, runs the .NET source-generator tests, and runs a small headless C#
  project. No template or distributable archive is built on this path.
- Markdown/reStructuredText and issue-template-only diffs skip the build job.
  Changes to native/managed code, third-party code, build scripts, profiles,
  workflow files, or API XML all require it. An unknown diff base builds rather
  than guessing that a change is documentation-only. Initial/new-branch runs
  style-check the CI helpers rather than gating all inherited upstream style debt.
- A tag matching `v*-custom.*` or a manual `workflow_dispatch` runs the full path:
  editor, matching managed packages, **only** the final minimal-extra Release
  **full-LTO** template, actual C# project export, and execution of the exported
  executable on the Windows runner. The script accepts the narrower tag syntax
  `v4.7.2-custom.1` (numeric major/minor/patch/revision).
- Only a successful tag-push run creates a **draft, prerelease-marked Release**.
  Review the binaries and notes and publish it manually. Manual dispatch uploads
  Actions artifacts only. Nothing is pushed to NuGet.org.

The full build's Actions artifact contains three ZIPs, `manifest.json`, and
`SHA256SUMS.txt`:

1. `godot-windows-x64-editor-dotnet.zip`: native editor and matching GodotSharp/tools
2. `godot-windows-x64-fork-nuget.zip`: locally generated `.nupkg` files
3. `godot-windows-x64-release-template-extra-lto.zip`: native Release template,
   console wrapper, and exact feature profile

The .NET runtime/game payload and smoke-project export are test outputs, not part
of the native template ZIP. There is no template_debug or non-LTO template job.
Build/test logs are retained for 7 days; build archives for 14 days. Tag draft
attachments remain associated with the Release. No repository name is hardcoded.

## Compiler, versions, and LTO evidence

- Runner: `windows-2022`, native MSVC toolset family `14.3`, x64; SCons `4.10.1`
- Python `3.12.10`, .NET SDK `10.0.401`, Ruff `0.15.8`, actionlint `1.7.7`
- AccessKit `0.22.3` official Godot release, pinned SHA-256; MSVC x64 files only
- All GitHub actions are pinned to verified full commit SHAs

The hosted Windows image/MSVC servicing revision can change. It is not a frozen
compiler container; the SCons environment and build logs record the selected
compiler/SDK configuration. Pinning .NET is enforced in the disposable checkout
by replacing its `global.json` selectors, so preinstalled newer .NET SDKs cannot silently
replace .NET 10. These temporary changes are not committed or packaged as source.

The editor uses `optimize=speed lto=none`. The template uses
`misc/build_profiles/windows_release_minimal_extra.py` with `lto=full` and
`optimize=size`. `platform/windows/detect.py` selects MSVC `/GL` and `/LTCG`.
CI checks those effective flags plus real `Generating code` / `Finished generating
code` linker output before accepting the artifact. It fails instead of calling a
switch-only build true LTO. Warnings remain visible.

This intentionally uses native MSVC rather than the historical Linux/MinGW
cross-toolchain. The MinGW plugin/bigobj/partition workarounds are not passed to
MSVC, and historical MinGW sizes are not predictions for these artifacts.

## Managed API boundary

A fresh checkout does not contain generated GodotSharp bindings. The .NET
source-generator tests therefore run **after** an editor build and glue generation;
they are not falsely advertised as a standalone no-native-build check. SCons
caches keep unchanged objects reusable, with separate editor/template and
runner-image/compiler/profile keys, each capped at 2 GiB. Native compile jobs use at most
4 workers (or the runner CPU count if smaller). NuGet caches are per-run and never restored from other
projects. The smoke project maps the four Godot package IDs exclusively to the
new local fork feed, not official Godot packages with a colliding version.
Preparation also creates `CiSmoke.sln` and adds its project with the pinned .NET
SDK, explicitly using `--format sln` because .NET 10 defaults to `.slnx`: the export
plugin requires the classic solution, even when a project-only build passes.

The normal editor retains its broader module API. The minimal template disables
optional modules and retains the previously agreed navigation/deprecated APIs.
The smoke exercises Node2D, Control, World2D/physics bindings, TileMapLayer,
2D shapes, generated signal dispatch, typed native/script collections, a closed
generic script, and template module exclusions. It requires
an explicit success marker and exit code 0; a timeout/forced quit is not success.
A successful smoke does not make disabled module APIs available. Reimport old
Basis Universal assets or explicitly add the required module before using them.

Headless smoke does not test Vulkan rendering or a desktop UI. Full/manual builds
export and run the same project and native LTO template in untrimmed JIT, trimmed
JIT and NativeAOT modes. The AOT run additionally rejects a runtime with dynamic
code support. This uses the runner’s normal MSBuild task host, without local
sandbox workarounds. Normal push builds retain the editor/generator/headless checks;
the three exported modes run only in the existing full-build pipeline.

## Tag and package-version policy

A tag names a tested source commit; it does **not** rewrite `version.py` or change
the generated NuGet version. The current `4.7.2-2dtrim.2` package version is included
in the manifest and draft notes. Before distributing different source versions
through a shared feed, give them unique package versions in a reviewed version
change. Never overwrite a published NuGet version or move a published tag.

The release job verifies the tag's resolved commit, repository/commit provenance,
and checksums. It refuses an already published Release or a draft with a different
source SHA. Rerunning the same tag skips matching SHA-256 assets and refuses to
replace conflicting or unverifiable assets. To retry a nondeterministically rebuilt
asset set, review/remove the unpublished draft manually or create a new version;
the workflow will not silently overwrite it.

## Permissions and maintenance

Ordinary jobs have only `contents: read`, checkout credentials are not persisted,
and PR code never runs under `pull_request_target`. Only the tag-specific draft
job has `contents: write`; it uses the temporary built-in token and has no external
PAT, signing key, publishing secret, or NuGet.org credential. There are no automatic
merges, tag writes, or public Release publication steps.

The editor cache is saved on default-branch pushes. The template cache is saved
only after a successful full build. GitHub's branch/tag cache isolation still
applies: a manual full run on the default branch can prime the default template
cache for future tags. Cache keys also include the hosted image revision to avoid reusing MSVC LTO objects
across serviced toolchains, and omit generated `__pycache__` files.

Local validation during this change is not a green GitHub Actions run. The first
native MSVC build, full Windows export/runtime smoke, cache lifecycle, artifact
upload, and draft creation must be observed in the future repository. Local helper
checks must not be mistaken for those remote validations.

References: [Windows runner image](https://github.com/actions/runner-images/blob/main/images/windows/Windows2022-Readme.md),
[MSVC /GL](https://learn.microsoft.com/en-us/cpp/build/reference/gl-whole-program-optimization),
[MSVC /LTCG](https://learn.microsoft.com/en-us/cpp/build/reference/ltcg-link-time-code-generation).
