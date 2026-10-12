"""The game profile, tools/paths.env, for Python: where the game, the BIOS, the test harness and the
lifter's ABI export are, and the game's frame-counter address. The one reader of paths.env in Python.

    import gamepaths                          # scripts in tools/ (others put tools/ on sys.path first)
    gamepaths.ORIG_ROM, gamepaths.GBA_BIOS    # absolute paths, as str

${ROOT} in paths.env is the checkout this file is in (a worktree's own root in a worktree).
"""
import os

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
ENV_FILE = os.path.join(ROOT, "tools", "paths.env")


def read(env_file: str = ENV_FILE, root: str = ROOT) -> dict[str, str]:
    """KEY=VALUE lines (blank lines and # comment lines skipped), with ${ROOT} replaced by root."""
    profile: dict[str, str] = {}
    with open(env_file) as fh:
        for line in fh:
            line = line.strip()
            if line and not line.startswith("#"):
                key, _, value = line.partition("=")
                profile[key] = value.replace("${ROOT}", root)
    return profile


_profile = read()
BN6F_DIR = _profile["BN6F_DIR"]
ORIG_ROM = _profile["ORIG_ROM"]
ORIG_ELF = _profile["ORIG_ELF"]
ORIG_SHA1 = _profile["ORIG_SHA1"]
GBA_BIOS = _profile["GBA_BIOS"]
MGBA_RUN = _profile["MGBA_RUN"]
RECORD = _profile["RECORD"]
GAME_FRAME_ADDR = _profile["GAME_FRAME_ADDR"]
TRACES = _profile["TRACES"]
CHECKPOINTS = _profile["CHECKPOINTS"]
ABI_DIR = _profile["ABI_DIR"]
ABI_GRAPH = _profile["ABI_GRAPH"]
