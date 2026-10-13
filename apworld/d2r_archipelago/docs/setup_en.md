# Diablo II: Resurrected Multiworld Setup

## Requirements

- Diablo II: Resurrected 3.x (Reign of the Warlock), installed through Battle.net.
- [Archipelago](https://github.com/ArchipelagoMW/Archipelago/releases) 0.6.4 or newer, with
  `d2r_archipelago.apworld` installed (double-click it, or put it in Archipelago's `custom_worlds` folder).
- [D2RLoader](https://d2rloader.net/download.html), extracted into your Diablo II: Resurrected folder. If you
  don't have it yet, the setup opens its download page for you. Windows Defender often flags D2RLoader, which
  patches the game while it runs, as a trojan; if it blocks or removes it, allow it under Windows Security,
  Virus & threat protection, Protection history.

The plugin and the mod are installed for you. The game runs offline through D2RLoader; don't use
Battle.net characters.

## Your options

Your options YAML decides how your game plays. In the Archipelago Launcher, **Diablo II Resurrected Options**
opens an editor with a checkbox, dropdown or slider for each option, with its description:

- **Open...** loads an existing YAML, such as `Diablo II Resurrected.yaml` from the release zip or one you made
  before.
- **Preset** starts from Quick Normal, Full Hell, Locked Down or Treasure Hunter.
- **Save** writes the file back. It only changes the lines of options you changed, so your comments stay.
  Options the file sets to weights or `random` are kept unless you change them here.

Put the saved YAML in Archipelago's `Players` folder, or send it to whoever hosts the multiworld.
Archipelago's own Options Creator works too, but it can only make new files.

The **D2RLoader Settings** group sets some of D2RLoader's own game settings for this seed: ground socket
counts, item levels, stat ranges, maximum sockets, floor item labels, extra shared stash tabs and the
materials stack limit. The setup writes them to `mods/Archipelago/d2rloader/config/d2rloader.toml`, where they
override your D2RLoader settings while the Archipelago mod runs. Left at `keep` (or the game's own number),
your own D2RLoader setting applies. Change them in your YAML, not in that file, since each setup rewrites it.

## Playing

With the game closed, do one of these:

- **Click your name on the room page** (on archipelago.gg, or any Archipelago website). The Archipelago
  Launcher asks which client to open; pick **Diablo II Resurrected**.
- **Or press Diablo II Resurrected in the Archipelago Launcher.** It reuses the last server and slot, so use
  this to continue a game you've already connected to.

A window shows the setup, then the game starts with the `Archipelago` mod and connects on its own. If
something goes wrong, the window stays open and says what. Start a new Lord of Destruction character for
each new seed.

The setup:

- checks for D2RLoader in your game folder. If it's missing, it opens D2RLoader's download page and stops;
  extract D2RLoader into the game folder and start again. When a newer D2RLoader is out, it says so; update
  when it suits you, since a D2RLoader update can need a new plugin.
- installs the plugin in the mod, at `<D2R folder>/mods/Archipelago/d2rloader/plugins/`, so it only loads
  with the `Archipelago` mod. A copy in `<D2R folder>/d2rloader/plugins/` from an older setup is moved
  there, with its server and slot.
- reads the seed's settings from the server and the tables from your own game files, and writes the
  `Archipelago` mod: skill trees locked, plus the seed's monster, superunique and runeword shuffles.
- saves the server and slot in `mods/Archipelago/d2rloader/config/archipelago.toml`, so the plugin connects
  as soon as the game starts.

If the Launcher can't find your game, set `game_folder` under `d2r_archipelago_options` in Archipelago's
`host.yaml` (Launcher, **Open host.yaml**).

### Without the Archipelago Launcher

The release zip, `D2R-Archipelago-<version>.zip`, has `d2r-ap-setup.exe` and the `d2rl-archipelago.dll`
plugin; keep them in the same folder. With the game closed, run:

```
d2r-ap-setup.exe --connect <server> <slot> [--password <pw>] --play
```

Without `--play` it only sets up, and you start D2RLoader with the `Archipelago` mod yourself. Without
`--connect` it reuses the last server and slot. (The Launcher keeps its copy of the tool in
`%LOCALAPPDATA%\d2r-archipelago\tools`.)

## Connecting

The plugin connects on its own. To connect somewhere else from the game, open the console and type:

```
ap-connect archipelago.gg:38281 YourSlotName [password]
```

`ap-status` shows the connection, and `ap-disconnect` drops it. If the mod doesn't match the seed you
connect to, the plugin tells you to set up again.

Items you receive, checks you send and other server messages show in the game chat as
`Archipelago: ...`, and in the console. The Codex charm's tooltip keeps the latest few.

Use a fresh character for each seed. The Codex charms (one for your class's skills, one for other
classes') must stay in your inventory; if you lose one, `ap-codex` rebuilds it.

Rewards only go to a character of the seed's class (with `character_class: any`, the first one that plays
the seed claims it), so a mule of another class gets nothing. The game doesn't tell the plugin your class
until you kill a monster or carry your Codex, so rewards may wait for your first kill. To move the seed to
another class, kill something with that character and type `ap-claim`. If the game crashes before it saved
your character, the next connect offers `ap-redeliver` to hand out that game's rewards again.

Your character also gets an **Archipelago Ledger**. Hover over it to see how many checks you've done in
each act, and outside any act, out of the total. `ap-missing` lists the checks left in the act you're in,
and `ap-missing all` counts them for every act. If you lose the Ledger, `ap-ledger` makes another.

**F9** (or `ap-panel`) opens the Archipelago command panel, so you don't need the console for everyday
commands: status, checks left here or by act, hints found in game, the server's hints (`!hint`), items
left (`!remaining`), the multiworld's status (`!status`), `!collect`, a new Ledger or Codex, and reconnect
or disconnect. Answers show in the chat. The key can be changed in Options, Controls.
