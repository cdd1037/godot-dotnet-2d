"""Android arm64 .NET 10 Mono counterpart of the desktop extra release profile.

The API surface remains 2D/UI/C# with Vulkan. Texture imports use ETC2 by default;
ASTC decoding is retained for projects that explicitly opt into ASTC textures.
Bounded ThinLTO is the release configuration; use lto=none for iteration.
"""

platform = "android"
target = "template_release"
arch = "arm64"
optimize = "size"
debug_symbols = False
dev_build = False
production = False
lto = "thin"
linkflags = "-Wl,--threads=4,--thinlto-jobs=4"
modules_enabled_by_default = False
module_astcenc_enabled = True
module_bcdec_enabled = True
module_freetype_enabled = True
module_glslang_enabled = True
module_godot_physics_2d_enabled = True
module_jpg_enabled = True
module_mono_enabled = True
module_msdfgen_enabled = True
module_mp3_enabled = True
module_ogg_enabled = True
module_svg_enabled = True
module_text_server_adv_enabled = True
module_vorbis_enabled = True
module_webp_enabled = True
vulkan = True
metal = False
use_volk = True
disable_physics_2d = False
disable_advanced_gui = False
disable_navigation_2d = False
accesskit = False
sdl = False
minizip = False
brotli = True
graphite = False
builtin_certs = False
builtin_harfbuzz = True
deprecated = True
