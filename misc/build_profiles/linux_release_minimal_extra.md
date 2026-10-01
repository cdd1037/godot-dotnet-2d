# Linux minimal-extra .NET release template

This opt-in Linux x86_64 profile matches the 14-module selection and portable
feature switches in `windows_release_minimal_extra.py`. It retains Vulkan, SDL,
advanced GUI, HarfBuzz, FreeType, MSDFgen, 2D physics, deprecated compatibility,
and the navigation API (without the optional navigation backend).

The maintained profile uses `optimize=size`, no debug symbols, and full LTO:

The maintained command is in [BUILDING.md](../../BUILDING.md#2-桌面-release-模板).

Use the resulting
`bin/godot.linuxbsd.template_release.x86_64.minimal_extra.mono` as a custom release
export template. Do not use this profile to build the editor. Linux's normal
display, audio and input defaults apply; Wayland and AccessKit availability
depends on the local build dependencies. Windows-only WinRT settings and the
MinGW COFF/plugin/partition workarounds are intentionally absent.

## Brotli retained for the default font

This maintained profile explicitly enables `brotli=True` to retain the embedded
default font and WOFF2 support. The historical `brotli=no` control prevents loading Godot's embedded default font,
`OpenSans_SemiBold.woff2`: `scene/theme/default_theme.cpp` guards its data setup
with `BROTLI_ENABLED`. Earlier size-cut guidance omitted this dependency.
Keeping FreeType and MSDFgen enabled alone does not preserve the default font.

Linux validation found garbled default Label text in both the non-LTO and full-LTO
extra builds. The old template and the module-only control rendered correctly.
The unchanged full-LTO template rendered correctly when the project explicitly
set `gui/theme/custom_font` to an imported TTF font. Therefore a Brotli-free
template requires an explicit supported project font; retain Brotli if stock
default-font behavior is required. A headless C# smoke success does not cover
this visual requirement. These observations do not certify Windows rendering.

The restored-Brotli Linux full-LTO build was re-exported with the same .NET 8
C# test scene, without `gui/theme/custom_font` or a custom TTF. Headless and
Vulkan window checks passed; the saved frame was byte-identical to the known-good
old baseline. The stripped native template measured 31,341,176 bytes
(29.88927 MiB), versus the historical Brotli-free control
31,103,640 bytes (29.66274 MiB): +237,536 bytes (+0.22653 MiB).
These are native-template sizes, not complete .NET export sizes. The comparison
uses the same GCC 14.2.0, size optimization and full LTO; source revision strings
differ, while Linux native implementation code is unchanged between the runs.
Windows was not rebuilt or runtime-tested.

## Scope and compatibility

- This adds no Linux CI or automatic release job. Both minimal-extra profiles
  retain Brotli for the default font.
- The measurements above belong to the .NET 8 checkpoint. Current source targets .NET 10; see [current results](../../PROJECT_RESULTS.md). This historical profile report does not itself validate the
  .NET 10 or Android migration.
- The output is a native template, not a complete export with GodotSharp,
  application assemblies or the .NET runtime. Use the matching fork's SDK and
  editor, and export a C# project to test the complete package.
- A broader editor/SDK can expose classes absent from this minimal native
  template. Projects must not depend on removed modules such as visual shaders,
  TLS, regex, noise, optional image codecs or the navigation backend. Preserving
  shared ClassDB APIs does not make those missing features available.
- The one-time non-LTO size comparison is a temporary command-line experiment;
  it is not a second maintained profile or release product. Do not infer exact
  per-module savings from object sizes or from a cross-toolchain comparison.

For natural-build timing, add `--debug=time,action-timestamps,json` and preserve
the SCons log and statistics file. Compare matched compiler, architecture,
optimization, feature flags and stripping; distinguish LTO savings from the
combined effect of module selection and optimization changes.
