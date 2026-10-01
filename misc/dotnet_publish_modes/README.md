# .NET publishing mode contract

The export preset option `dotnet/publish_mode` selects:

- `0`: self-contained, untrimmed JIT (default), including ordinary runtime managed assembly loading
- `1`: self-contained trimmed JIT, with scripts/plugins and generic closures known when publishing
- `2`: NativeAOT shared library, with scripts/plugins and generic closures known when publishing

Each export writes `.godot-dotnet-publish-mode` beside its payload. The template honors it even if a directory contains stale files from another mode; it does not silently fall back to another runtime. Switching modes in an existing directory warns about old files without deleting user data. Use a clean directory for package-size comparisons.

The standard editor remains JIT. AOT does not contain a fallback JIT and cannot execute unknown managed IL. NuGet dependencies must themselves support the selected publishing mode. Compiler/linker warnings are retained; successful `dotnet publish` alone is not a runtime verification.

## Hosted entrypoint

Godot loads `GodotPlugins.Game.Main.InitializeFromGameProject`. The SDK roots this precise method for ILLink rather than adding a console `Main` or retaining every game method. NativeAOT exports `godotsharp_game_main_init` in the game shared library.

Desktop trimmed JIT additionally needs .NET native-host component activation. The SDK sets `_EnableConsumingManagedCodeFromNativeHosting` for this mode. This is an **SDK-internal, version-sensitive property**, introduced here during .NET 8 work and rechecked for .NET 10; retest it when upgrading the SDK. Android direct Mono hosting does not use this property. .NET emits an IL2026 native-hosting warning, which is intentionally not suppressed. Only the known Godot entrypoint and explicit script registrations are tested; arbitrary native-host activation targets are not promised.

## Known script metadata and libraries

For a closed generic or explicitly selected plugin type, declare an assembly-level manifest, for example:

```csharp
[assembly: Godot.RegisterScriptType(typeof(MyGenericScript<int>))]
```

The generated registration calls the script's static metadata interface. It does not use `MakeGenericType` in the trimmed/AOT core and does not create a resource-path association. `[NoScriptFileAssociation]` continues to prevent automatic file-path registration. Missing closed registrations fail with a descriptive error.

Godot script libraries use `IsGodotLibraryProject=true` so they do not export a second engine entrypoint. The game declares the library types it needs. A package being installed is not sufficient evidence of AOT compatibility.

Generic parameterless constructors are preserved explicitly and invoked against the native-owned object. Ordinary JIT retains the prior argument-count-based reflective constructor selection. A parameter-only C# class is not thereby supported for scene/inspector creation or editor reload: those workflows require a parameterless constructor.

## Comparisons

Compare the same scene, native template, RID, configuration, globalization setting, and symbol policy in all three modes. Separate the native engine, managed/runtime or AOT payload, resources, optional symbols, and full runnable directory. Do not compare a framework-dependent JIT application against self-contained AOT.

The SDK does not force invariant globalization. Tests of Godot's Chinese text rendering do not validate .NET culture APIs. A normal-globalization comparison must exercise those APIs independently.

Historical initial validation used Linux x86_64 / .NET SDK 8.0.425. Current .NET 10 Windows full CI has exported and run all three modes; this does not validate hardware Vulkan or macOS. Exact source revisions and CI evidence are maintained in [project results](../../PROJECT_RESULTS.md).

## .NET 10 migration

The current source targets .NET SDK 10.0.401 / runtime 10.0.12 and custom SDK
`4.7.2-2dtrim.3`. The .NET 8 results above are historical. The native-host trim
property is retained and rechecked as part of the migration. Android supports untrimmed Mono JIT and experimental trimmed Mono JIT with the
matching 10.0.12 crypto JAR. Android CoreCLR and NativeAOT remain rejected. The
Android direct Mono host does not require desktop ComponentActivator preservation.
See [Android validation](../android_dotnet_validation/README.md) for current
checks, the local ILLink IPC blocker, and device acceptance requirements.
