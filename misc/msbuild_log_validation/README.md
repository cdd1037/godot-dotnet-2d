# MSBuild log buffering regression test

Build the fork's .NET editor and API assemblies, put the matching .NET SDK on
PATH, and run:

```sh
misc/msbuild_log_validation/run.sh /absolute/path/to/bin/godot.linuxbsd.editor.x86_64.mono /absolute/path/to/isolated-output
```

The runner rebuilds GodotTools and a small editor plugin, then runs the real
editor headlessly. Use the fork's isolated NuGet cache, as with the other managed
validation projects. All generated projects, build outputs and logs are placed
in the given output directory; the tools build uses its normal ignored outputs.

The plugin reflects the actual loaded `GodotTools.Build.MSBuildPanel` methods and
connects a real `BuildOutputView` to a real `RichTextLabel`. It checks:

- Null, empty, whitespace, embedded LF and CRLF inputs
- Buffering until the editor's real deferred-call queue runs
- Whole-batch delivery, builder clearing/reuse, and scheduling of later batches
- Repeated empty and queued flushes after an explicit flush
- 20,000 concurrent stdout/stderr lines with no loss or duplication and preserved
  ordering within each stream

It deliberately avoids adding the test panel to the scene tree, so its normal
build-event handlers do not interfere with the editor's actual build panel.
Success requires exit code 0 and `MSBUILD_LOG_TESTS_PASSED`. A timeout is a failure.
Raw buffered text is asserted before rendering. `RichTextLabel` normalizes CR
characters away, so rendered-text assertions account for that separately.
This validates Linux's platform newline behavior; it does not claim a Windows
runtime pass (`AppendLine` uses the platform newline, as in upstream #123739).
