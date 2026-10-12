"""Builds the player download, dist/D2R-Archipelago-<version>.zip, with everything in one D2R-Archipelago/ folder:

    d2r-ap-setup.exe            writes the seed's mod, installs D2RLoader and the plugin next to it
    d2rl-archipelago.dll        the in-game client
    d2r_archipelago.apworld     for whoever generates the multiworld
    Diablo II Resurrected.yaml  the options template
    README.md                   the setup guide

It builds the plugin, runs its tests, and builds the apworld and template with tools/build_apworld.py, so the
Archipelago checkout needs worlds/d2r_archipelago linked in (see that script).

    python -I tools/package_release.py <archipelago-folder>
"""
import json
import re
import subprocess
import sys
import zipfile
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent
PLUGIN = ROOT / "plugin"
DIST = ROOT / "dist"
GAME = "Diablo II Resurrected"
FOLDER = "D2R-Archipelago"


def versions() -> str:
    """The world's version, which the plugin's must match."""
    world = json.loads((ROOT / "apworld" / "d2r_archipelago" / "archipelago.json").read_text(encoding="utf-8"))["world_version"]
    plugin = re.search(r'PluginVersion = "([^"]+)"', (PLUGIN / "src" / "plugin.cpp").read_text(encoding="utf-8"))
    if plugin is None or plugin.group(1) != world:
        sys.exit(f"The plugin's version ({plugin.group(1) if plugin else 'missing'}) doesn't match the world's ({world}).")
    return world


def main() -> None:
    if len(sys.argv) != 2:
        sys.exit(__doc__)
    version = versions()

    subprocess.run(["cmd", "/c", str(PLUGIN / "build.cmd")], cwd=PLUGIN, check=True)
    subprocess.run([str(PLUGIN / "build" / "ap_tests.exe")], cwd=PLUGIN, check=True)
    subprocess.run([sys.executable, "-I", str(ROOT / "tools" / "build_apworld.py"), sys.argv[1]], check=True)

    files = {
        "d2r-ap-setup.exe": PLUGIN / "build" / "d2r-ap-setup.exe",
        "d2rl-archipelago.dll": PLUGIN / "build" / "d2rl-archipelago.dll",
        "d2r_archipelago.apworld": DIST / "d2r_archipelago.apworld",
        f"{GAME}.yaml": DIST / f"{GAME}.yaml",
        "README.md": ROOT / "apworld" / "d2r_archipelago" / "docs" / "setup_en.md",
    }
    archive = DIST / f"{FOLDER}-{version}.zip"
    with zipfile.ZipFile(archive, "w", zipfile.ZIP_DEFLATED) as zip:
        for name, source in files.items():
            zip.write(source, f"{FOLDER}/{name}")
    print(f"wrote {archive}")


if __name__ == "__main__":
    main()
