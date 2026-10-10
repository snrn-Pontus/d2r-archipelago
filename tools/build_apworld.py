"""Builds dist/d2r_archipelago.apworld and dist/Diablo II Resurrected.yaml with Archipelago's own tools,
so the .apworld manifest and the options template are exactly what a multiworld host expects.

The Archipelago source checkout must have worlds/d2r_archipelago (a link to apworld/d2r_archipelago).

    python -I tools/build_apworld.py <archipelago-folder>
"""
import shutil
import subprocess
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent
GAME = "Diablo II Resurrected"

TEMPLATE_SCRIPT = """
import sys
sys.path.insert(0, '.')
import ModuleUpdate
import worlds
from Options import generate_yaml_templates
generate_yaml_templates('build/templates', False)
"""


def main() -> None:
    archipelago = Path(sys.argv[1]).resolve()
    python = archipelago / ".venv" / "Scripts" / "python.exe"
    if not python.exists():
        python = Path(sys.executable)
    subprocess.run([str(python), "Launcher.py", "Build APWorlds", "--", GAME, "--skip_open_folder"], cwd=archipelago, check=True)
    subprocess.run([str(python), "-c", TEMPLATE_SCRIPT], cwd=archipelago, check=True)

    dist = ROOT / "dist"
    dist.mkdir(exist_ok=True)
    for source in (archipelago / "build" / "apworlds" / "d2r_archipelago.apworld", archipelago / "build" / "templates" / f"{GAME}.yaml"):
        shutil.copy2(source, dist / source.name)
        print(f"wrote {dist / source.name}")


if __name__ == "__main__":
    main()
