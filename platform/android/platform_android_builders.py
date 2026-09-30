"""Functions used to generate source files during build time"""

import subprocess
import sys


def generate_android_binaries(target, source, env):
    gradle_process = []

    if sys.platform.startswith("win"):
        gradle_process = [
            "cmd",
            "/c",
            "gradlew.bat",
        ]
    else:
        gradle_process = ["./gradlew"]

    if env["target"] == "editor":
        raise RuntimeError("This fork provides Android export templates, not an Android editor.")
    if env["module_mono_enabled"]:
        gradle_process += ["generateGodotMonoTemplates"]
    else:
        gradle_process += ["generateGodotTemplates"]
    gradle_process += ["--quiet", "--no-daemon", "--max-workers=4"]

    if env["debug_symbols"] and not env["separate_debug_symbols"]:
        gradle_process += ["-PdoNotStrip=true"]

    subprocess.run(
        gradle_process,
        cwd="platform/android/java",
        check=True,
    )
