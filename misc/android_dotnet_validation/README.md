# Experimental Android Mono trimmed JIT

The Android preset keeps the ordinary **APK** export, including
`gradle_build/use_gradle_build=false` with a precompiled Mono APK template.
`dotnet/publish_mode=0` is untrimmed Mono JIT; `1` is experimental trimmed Mono
JIT. NativeAOT and Android CoreCLR remain rejected. Trimming does not turn the
runtime into AOT, remove the JIT, or require MAUI / a .NET Android workload.

Use the matching fork editor, SDK packages, and rebuilt native template. An old
untrimmed-only template rejects the `trimmed-jit` marker intentionally. This
iteration starts at published source `0630831d5de0f98662cfc6bc23815330de12ba04`;
historical APK byte counts are not a matched baseline for this change.

## Root and runtime policy

- `GodotPlugins.Game.Main.InitializeFromGameProject` is the precise ILLink root.
  Its direct call graph reaches native/managed initialization, callback pointers,
  static script metadata, and known closed-generic registrations. The whole game
  assembly is not rooted. Unknown late-loaded IL/plugins are not supported in
  trimmed mode; dependencies must themselves be trim-safe.
- Publish constants are evaluated after the project body, so a project-local
  `PublishTrimmed=true` selects the same trim-safe callbacks as editor CLI flags.
- Desktop hostfxr's ComponentActivator preservation feature is not enabled for
  Android: Mono's `coreclr_create_delegate` directly resolves the rooted method.
  Mono's own runtime descriptor preserves its native-invoked managed methods.
- The official `android-arm64` Mono runtime remains pinned to `10.0.12`, paired
  with the template's crypto JAR. Java `System.loadLibrary` and JNI initialization
  remain intact; no blanket crypto managed roots or warning suppressions are added.
- The existing Android host does not load runtimeconfig JSON properties into
  Mono. Packaging that JSON is not proof that runtime-only settings are consumed.
  ILLink's compile-time feature substitutions are separate from this limitation.

Source audit: [Mono host](https://github.com/dotnet/runtime/blob/v10.0.12/src/mono/mono/mini/monovm.c),
[Mono roots](https://github.com/dotnet/runtime/blob/v10.0.12/src/mono/System.Private.CoreLib/src/ILLink/ILLink.Descriptors.xml),
[desktop activation feature](https://github.com/dotnet/runtime/blob/v10.0.12/src/libraries/System.Private.CoreLib/src/ILLink/ILLink.Descriptors.Shared.xml),
[crypto JNI startup](https://github.com/dotnet/runtime/blob/v10.0.12/src/native/libs/System.Security.Cryptography.Native.Android/pal_jni_onload.c).

## Reproducible validation

Use SDK `10.0.401`, runtime `10.0.12`, and freshly built local fork packages.
Each command requires a new/empty output directory and never erases old evidence.

```sh
python misc/android_dotnet_validation/check_sdk_guards.py /absolute/sdk-guard-results
python misc/android_dotnet_validation/run_publish_validation.py /absolute/mono-publish-results
python misc/android_dotnet_validation/check_apk.py /absolute/untrimmed.apk --publish-mode jit
python misc/android_dotnet_validation/check_apk.py /absolute/trimmed.apk --publish-mode trimmed-jit
```

The guard test executes production SDK evaluation, including project-local and
CLI trim settings, normal JIT, desktop activation preservation, and rejected
AOT/CoreCLR/bionic/wrong-runtime/editor/legacy combinations.

The publish runner produces identical fixture source under both modes with the
same configuration, RID, globalization, and symbol policy. The metadata inspector
checks the native-host method/body survives and an unused game type disappears
only in trimmed mode. It also checks Mono/corelib/API/native-crypto/JAR presence,
paired JAR hashes, absence of desktop runtimes, and identical native-runtime
bytes. `comparison.json` separates managed, native-runtime, and other bytes.
These are publish/IL checks, not Android execution or APK measurements.

The full Windows CI path runs this bounded publish comparison after the existing
desktop matrix and uploads both managed payloads plus logs. It does not add an
Android device claim, change ordinary push into a full build, or require Gradle.
The local restricted Linux executor cannot create the Unix socket required by
.NET 10 ILLink's TaskHostFactory (`MSB4216` / `MSB4027`). No task-host workaround or
trim-warning suppression is used; that publish stage must run in a permitted host.

## Device acceptance remains required

The fixture checks scene-created scripts, generated properties and method/signal
callbacks, typed containers, explicitly registered closed generics, dynamic-code
availability, SHA256/PBKDF2 known vectors, RNG, AES-GCM/RSA round trips, crypto on
a thread-pool thread, signal awaiting, and normal .NET globalization. Success
requires `ANDROID_MONO_TRIM_SMOKE_OK` in device output with no JNI, missing-method,
or assembly-load errors. Static APK checks cannot prove these behaviors.

Before shipping, validate cold start, Vulkan/ETC2 rendering, pause/resume,
activity recreation, Java class loading/JNI, crypto, and SAF permission behavior
on target devices. Only arm64 is in this comparison; do not generalize it to
other ABIs or to AAB/custom-Gradle export.

## Test-only CI payload repack

Where local ILLink IPC is blocked, `repack_ci_payload.py` can consume the **verified
full-CI artifacts** and a freshly exported unsigned fixture APK. Run it for both
CI modes against the same APK to keep native code, resources, ZIP compression,
and alignment policies matched. It verifies every payload file against the CI
inventory, the expected CI source revision, identical native/JAR/DEX assets,
and the complete sparse-PCK directory (size/MD5). It replaces managed assets and
the SHA512 publish manifest together, regenerates the directory, and runs
16 KiB ZIP alignment plus CRC/index checks. It rejects signed/encrypted packs,
unsafe paths, unlisted files, wrong modes, and mismatched native payloads.

```sh
python misc/android_dotnet_validation/repack_ci_payload.py genuine-jit.apk \
  ci-artifact/trimmed-jit/publish trimmed-repacked.apk \
  --mode trimmed-jit --report ci-artifact/payload-trimmed-jit.json \
  --comparison ci-artifact/comparison.json --expected-source VERIFIED_CI_COMMIT \
  --zipalign /absolute/android-sdk/build-tools/36.1.0/zipalign
```

Repeat with `jit` and a separate output before comparing bytes. Keep the genuine
export, both repack provenance files and `check_apk.py` outputs. These APKs provide
**static packaging and size evidence only**. They do not establish automatic
one-click trimmed APK export, local trimmed publishing, or Android runtime/device
success. No production export bypass or fake `dotnet publish` is introduced.


## Local checkpoint before permitted-host CI

See [trim_local_results.json](trim_local_results.json) for exact build revisions,
hashes, passed checks and blocked stages. The post-merge genuine no-Gradle JIT
export is 33,263,019 bytes unsigned; its matching template is 17,594,966 bytes.
These are an untrimmed baseline, not a trim saving. Both native/Java payload and
all 186 sparse-PCK records were checked. Gradle's extra strip pass removes eight
bytes from the SCons engine input; both hashes are recorded rather than falsely
claiming those two intermediate files are byte-identical. Device execution and
automatic trimmed export remain unverified.
