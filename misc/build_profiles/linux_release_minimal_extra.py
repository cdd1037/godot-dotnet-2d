"""Linux x86_64 release template matching the Windows extra feature selection.

Full LTO is the maintained configuration. See linux_release_minimal_extra.md.
This profile is not for editor builds.
"""

platform = "linuxbsd"
target = "template_release"
arch = "x86_64"
optimize = "size"
debug_symbols = False
dev_build = False
production = False
lto = "full"
extra_suffix = "minimal_extra"

modules_enabled_by_default = False
module_astcenc_enabled = True
module_bcdec_enabled = True
module_freetype_enabled = True
module_glslang_enabled = True
module_godot_physics_2d_enabled = True
module_jpg_enabled = True
module_mono_enabled = True
module_mp3_enabled = True
module_msdfgen_enabled = True
module_ogg_enabled = True
module_svg_enabled = True
module_text_server_adv_enabled = True
module_vorbis_enabled = True
module_webp_enabled = True

vulkan = True
use_volk = True
sdl = True
disable_physics_2d = False
disable_advanced_gui = False
disable_navigation_2d = False  # Keep the API; the optional backend module is off.

minizip = False
brotli = True  # Required by the embedded default WOFF2 font.
graphite = False
builtin_certs = False
builtin_harfbuzz = True
deprecated = True

# Linux display/audio/input drivers retain the platform defaults. AccessKit and
# Wayland follow the platform's dependency detection; Windows-only flags and
# MinGW COFF/plugin workarounds do not apply to the native Linux toolchain.
