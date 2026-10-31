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
each new seed, or a Reign of the Warlock one for a seed with Warlock skills (Character Class warlock or
Include Warlock).

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

### Side area destinations for MapSense (optional)

If you already play with [RuffnecKk MapSense](https://github.com/RuffDood/RuffnecKk-D2RLoader-Suite), a
separate D2RLoader plugin that draws route lines on the automap, the setup tool can list your seed's side area
checks as MapSense destinations. Nothing here installs, turns on or edits MapSense. Run:

```
"%LOCALAPPDATA%\d2r-archipelago\tools\d2r-ap-setup.exe" --mapsense
```

(add `--connect <server> <slot>` if you haven't connected yet). It reads the seed from the server, writes
`<D2R folder>/mods/Archipelago/mapsense-destinations.toml` and opens its folder. The file has a
`targets = [ ... ]` list for MapSense 2.2.0 (config `schema_version = 21`): one `{ level_id = N }` per side area,
each once with a comment naming the area, its act and the difficulties on which it's a check.

1. Open MapSense's own config, `ruffneckk-mapsense.toml` in the `d2rloader/config/` folder you installed
   MapSense into (`<D2R folder>/d2rloader/config/`, or `<D2R folder>/mods/Archipelago/d2rloader/config/`).
2. Under `[navigation.custom_levels]`, replace `targets = [ ... ]` with the list from the export, or copy its
   lines into your own list.
3. Turn on Custom Levels in the MapSense menu, and restart D2R: MapSense only reads the list when the game starts.

The destinations are areas, not checks. The list is written once and doesn't change as you play, so an area
stays on it after you've sent its check (`ap-missing` shows what's left). MapSense draws a line when a destination
connects to the area you're in, so for a deeper level such as Tower Cellar Level 5 the line shows from the level
above. MapSense can also reveal maps and mark things far beyond these destinations, depending on its own settings.
Run `--mapsense` again for each new seed.

## Connecting

The plugin connects on its own. To connect somewhere else from the game, open the console and type:

```
ap-connect archipelago.gg:38281 YourSlotName [password]
```

`ap-status` shows the connection, and `ap-disconnect` drops it. If the mod doesn't match the seed you
connect to, the plugin tells you to set up again.

Items you receive, checks you send and other server messages show in the game chat as
`Archipelago: ...`, and in the console. The Codex charm's tooltip keeps the latest few.
Progression items other players send each other show too, in grey and after your own messages; a big
release or collect shows a few and counts the rest, which are in the log. Set `other_sends` in
`mods/Archipelago/d2rloader/config/archipelago.toml` to `"mine"` to hide them or `"all"` to see every item.

Items you receive and checks you send also show as banners near the top of the screen. The same file tunes
them (restart the game after editing it): `banner_lines` (5) is how many show at once, `banner_ms` (4500)
and `banner_important_ms` (6000, for the gold and red ones) how many milliseconds each stays, and
`summarize_checks_over` (4), `summarize_items_over` (10) and `summarize_loot_over` (3) how many checks,
items, or ordinary items among them arrive together before they become one "Checks sent: n" or
"Received n items" line. Keys, locks, skills and traps always get their own line unless more than
`summarize_items_over` items arrive at once. `notifications = false` turns the banners off, and
`banner_volume` (percent, 0 to 200) sets their sounds' volume.

Use a fresh character for each seed. The Codex charms (one for your class's skills, one for other
classes') must stay in your inventory; if you lose one, `ap-codex` rebuilds it.

Rewards only go to the seed's character: the first character of the seed's class that plays it (with
`character_class: any`, the first one of any class) claims it by name, so a mule, even one of the same class,
gets nothing. The plugin reads your character's name and class from the game as a game begins. If a game
update stops that from working, it falls back to the class alone, learned when you kill a monster or carry
your Codex, so rewards may wait for your first kill. In tree mode, which has no Codex, your character then
gets an **Archipelago Emblem** after that first kill; keep it on your character (not the shared stash), and
later games know your class at once. To move the seed to another character, type `ap-claim` with that
character (kill something first if the plugin doesn't know its class yet). If the game crashes before it saved
your character, the next connect offers `ap-redeliver` to hand out that game's rewards again.

At the main menu, the status line at the top left says which character to play: the seed's own once it has
one ("play SorkenTwo (Sorceress, level 12)"), or else your characters of the seed's class, or "not connected"
if the plugin can't reach the server. In a game it shows how many checks you've done out of the total, and in
the act you're in. `ap-missing` lists the checks left in the act you're in, and `ap-missing all` counts them for every act
and outside any act.

**F9** (or `ap-panel`) opens the Archipelago command panel, so you don't need the console for everyday
commands: status, checks left here or by act, hints found in game, the server's hints (`!hint`), items
left (`!remaining`), the multiworld's status (`!status`), `!collect`, a new Codex, and reconnect
or disconnect. Answers show in the chat. The key can be changed in Options, Controls.
