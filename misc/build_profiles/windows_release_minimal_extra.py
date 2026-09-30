"""Second opt-in Windows profile: approved additional feature removal.

Use profile=misc/build_profiles/windows_release_minimal_extra.py.
Never use this profile for an editor build. See windows_release_minimal.md.
"""

platform = "windows"
target = "template_release"
arch = "x86_64"
optimize = "size"
debug_symbols = False
dev_build = False
production = False  # Keep LTO explicit; production=yes would select platform defaults.
lto = "none"  # Override with lto=full for the controlled comparison.
extra_suffix = "minimal_extra"

modules_enabled_by_default = False
module_astcenc_enabled = True
module_bcdec_enabled = True
module_freetype_enabled = True  # Required for normal TTF/OTF fonts; explicitly approved.
module_glslang_enabled = True
module_godot_physics_2d_enabled = True
module_jpg_enabled = True
module_mono_enabled = True
module_msdfgen_enabled = True  # Explicitly requested for the second profile.
module_mp3_enabled = True
module_ogg_enabled = True
module_svg_enabled = True
module_text_server_adv_enabled = True
module_vorbis_enabled = True
module_webp_enabled = True

# Driver/core options are distinct from modules. Keep existing user-facing defaults.
vulkan = True
metal = False
use_volk = True
disable_physics_2d = False
disable_advanced_gui = False
disable_navigation_2d = False  # Later instruction: retain API; backend module remains off.
accesskit = True
sdl = True
winrt = True

# Do not silently add navigation, noise, TLS or texture modules.
# SCons command-line options may opt in after the project has been checked.

# Additional, explicitly approved removals; keep the first comparison unchanged.
minizip = False
brotli = True  # Required by the embedded default WOFF2 font.
graphite = False
builtin_certs = False
builtin_harfbuzz = True  # HarfBuzz stays enabled; this is not a module toggle.
deprecated = True  # Later instruction: defer this additional API-compatibility cut.
# disable_physics_3d / disable_navigation_3d are already hardcoded True by this fork.
