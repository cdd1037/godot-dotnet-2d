# 2D .NET fork validation

This C# project checks removed/retained API contracts, ordinary Vector2 animation,
2D physics, navigation synchronization/path queries, and noise. In a windowed
Vulkan session it also checks real compute dispatch/readback, GPU particle pixels,
2D light/occluder pixel differences, HDR values above 1, and 2D MSAA readback.

Build the fork's editor, generate its Mono glue, and build its managed assemblies
before building this project. Use an isolated NuGet cache and the fork's local
`bin/GodotSharp/Tools/nupkgs` feed: upstream and this fork currently have the same
4.7.2 package version and must not share cached Godot API packages.

Run `./build.sh /absolute/path/to/fork/bin`. It writes a local ignored NuGet config.
Then run the matching editor:

```
/path/to/bin/godot.linuxbsd.editor.x86_64.mono --path . --rendering-driver vulkan --rendering-method forward_plus -- --require-pruned
```

The `forward_plus` option is retained as a configuration alias; the fork uses its
canvas-only RenderingDevice compositor. This test requires Vulkan with a display
for graphics assertions. Headless execution skips graphics checks and is a
separate API/physics/navigation test, not a rendering pass. A successful run prints
`FORK_VALIDATION_OK` and exits 0. Screenshots are saved to the project's user data
directory. A timeout or forced `--quit-after` without the success marker is not a
pass. The test also works with the GDScript-removal-only checkpoint when run
without `--require-pruned`, allowing its assertions to be calibrated first.
