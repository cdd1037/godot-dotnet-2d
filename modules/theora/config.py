def can_build(env, platform):
    if env["arch"].startswith("rv"):
        return False
    env.module_add_dependencies("theora", ["ogg", "vorbis"])
    # Only the editor MovieMaker encoder remains; runtime video playback is removed.
    return env.editor_build


def configure(env):
    pass
