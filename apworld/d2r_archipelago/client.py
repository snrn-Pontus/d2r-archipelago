"""The Archipelago Launcher's "Diablo II Resurrected" button. It also opens when a player clicks their name on a
room page (an archipelago://slot:password@host:port link).

It runs d2r-ap-setup.exe, which ships inside the .apworld with the plugin: the tool checks for D2RLoader, installs the
plugin, writes the seed's mod, saves the server and slot so the plugin connects on its own, and starts the game.
Without a link it reuses the last server and slot.
"""
import os
import pkgutil
import subprocess
import urllib.parse
from pathlib import Path

from worlds.LauncherComponents import Component, Type, components

GAME = "Diablo II Resurrected"
TOOLS = ("d2r-ap-setup.exe", "d2rl-archipelago.dll")


def unpack_tools() -> Path | None:
    """Copies the setup tool and plugin out of the .apworld (or the loose world folder), next to each other."""
    folder = Path(os.environ.get("LOCALAPPDATA", Path.home())) / "d2r-archipelago" / "tools"
    folder.mkdir(parents=True, exist_ok=True)
    for name in TOOLS:
        try:
            data = pkgutil.get_data(__package__, f"bin/{name}")
        except OSError:
            data = None
        if data is None:
            return None
        target = folder / name
        if not target.exists() or target.read_bytes() != data:
            target.write_bytes(data)
    return folder / TOOLS[0]


def connect_args(url: str) -> list[str]:
    """--connect arguments from an archipelago://slot:password@host:port link."""
    link = urllib.parse.urlparse(url)
    if link.scheme != "archipelago" or not link.hostname or not link.username:
        return []
    args = ["--connect", f"{link.hostname}:{link.port or 38281}", urllib.parse.unquote(link.username)]
    # The room page writes "None" when the room has no password.
    if link.password and link.password != "None":
        args += ["--password", urllib.parse.unquote(link.password)]
    return args


def launch_client(*args: str) -> None:
    from Utils import messagebox
    from . import D2RWorld

    setup = unpack_tools()
    if setup is None:
        messagebox("Diablo II Resurrected", "This apworld has no d2r-ap-setup.exe. Build it with "
                   "tools/package_release.py, or use the release zip.", True)
        return
    command = [str(setup), "--play"]
    # An empty setting comes back as Archipelago's own folder, so only a folder with the game counts;
    # otherwise the tool finds the Battle.net install.
    game_folder = Path(D2RWorld.settings.game_folder or "")
    if (game_folder / "D2R.exe").is_file():
        command += ["--game", str(game_folder)]
    for arg in args:
        command += connect_args(arg)
    # Its own window, so the player sees the setup, and any error.
    subprocess.Popen(command, cwd=setup.parent, creationflags=subprocess.CREATE_NEW_CONSOLE)


components.append(Component(
    GAME,
    func=launch_client,
    component_type=Type.CLIENT,
    game_name=GAME,
    supports_uri=True,
    description="Set up Diablo II: Resurrected for your seed and start it.\n"
                "Installs D2RLoader and the Archipelago plugin when needed.",
))


def launch_options_editor(*args: str) -> None:
    from .options_editor import launch_editor
    launch_editor(*args)


components.append(Component(
    f"{GAME} Options",
    func=launch_options_editor,
    component_type=Type.TOOL,
    description="Make or edit your Diablo II: Resurrected options YAML,\n"
                "with checkboxes, dropdowns and sliders.",
))
