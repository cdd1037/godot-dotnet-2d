# Android export templates for the 2D/.NET fork

This platform restores Android exports and the SAF directory/file-access patch on
the fork's 2D/UI/C# API. It does not restore the Android editor, GDScript, 3D, XR,
OpenGL/GLES, or Android-specific network overrides. Standard low-level networking
continues to use the fork's POSIX implementation.

## Paired toolchain and managed runtime

- Build SDK: .NET 10.0.401; Godot SDK/API package: `4.7.2-2dtrim.3`
- Android game target: `net10.0`, Mono runtime `10.0.12`, standard Android RID
- The native crypto JAR is taken from the same Microsoft Mono runtime package
- SDK/export guards reject Android CoreCLR and NativeAOT; trimmed Mono JIT is experimental
- Desktop JIT, trimmed JIT and NativeAOT remain separate supported build paths
- JDK 17; Gradle 8.11.1; Android API 36; build tools 36.1.0; NDK 29.0.14206865

The default delivery ABI is arm64-v8a. Other ABI export options need independently
built matching native templates and their own validation. Android API 29 is the
normal exported-app minimum for Vulkan 1.1; API 24 is the native library floor.
The renderer is Vulkan-only. The `mobile` and `forward_plus` configuration names
both enter this fork's 2D canvas renderer; neither restores a 3D pipeline.

## Build and export

Build commands and template outputs are maintained in the root
[build/export guide](../../BUILDING.md#4-android-模板与普通-apk).

Use an Android export preset with arm64 and Vulkan. `dotnet/publish_mode=0` keeps
untrimmed Mono JIT; `1` enables experimental trimmed Mono JIT for known scripts.
Both retain ordinary APK export with a precompiled template and Gradle disabled.
Use the matching rebuilt template and SDK; older templates reject the trimmed marker.
See [trimming validation](../../misc/android_dotnet_validation/README.md) for
rooting policy, reproducible checks, and device-verification limits.
Enable `rendering/textures/vram_compression/import_etc2_astc` and reimport assets.
ETC2 is the conservative baseline; ASTC is optional and should be selected only
for a known supporting device set. Windows/Linux desktop texture imports are
unchanged. Native `.so` files go into APK ABI directories, managed files go into
the PCK, duplicate JARs are deduplicated, and static `.a` linker inputs are excluded.

SAF access requires the app to obtain a valid Android content/tree URI and the
corresponding permission grant from the system picker. Ordinary filesystem
paths retain their prior behavior. See [current customization](../../CUSTOMIZATION.md#android-专属边界) for
source attribution and [results](../../PROJECT_RESULTS.md) for verification limits.

Full LTO exceeded the validation machine’s RAM budget even with bounded threads.
This is a build-environment limit, not a claim that Android cannot use full LTO.
The maintained release profile therefore uses four-thread ThinLTO.

## JVM provider regression tests

Run `./gradlew :lib:testTemplateDebugUnitTest` from `platform/android/java`.
The SAF suite covers Android API 24/28 and 36. Install a Java 21 JDK for the
Robolectric test launcher; the Android application still compiles to Java 17.
This is provider simulation, not device validation.
