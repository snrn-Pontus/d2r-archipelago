# Diablo II: Resurrected Multiworld Setup

## Requirements

- Diablo II: Resurrected 3.x (Reign of the Warlock), installed through Battle.net.
- `d2r-ap-setup.exe` and the `d2rl-archipelago.dll` plugin from this project's release, in the same folder.
- [D2RLoader](https://d2rloader.net). The setup tool installs it if it's missing.

The game runs offline through D2RLoader. Don't use Battle.net characters.

## Installing

1. For each new seed, run `d2r-ap-setup.exe --connect <server> <slot> [--password <pw>]` with the game
   closed. It reads the seed's settings from the server and the tables from your own game files, and
   writes the `Archipelago` mod: skill trees locked, plus the seed's monster, superunique and runeword
   shuffles. After you've used `ap-connect` in game once, plain `d2r-ap-setup.exe` reuses that server and
   slot.
   - It installs the plugin in the mod, at `<D2R folder>/mods/Archipelago/d2rloader/plugins/`, so it only
     loads with the `Archipelago` mod. A copy in `<D2R folder>/d2rloader/plugins/` from an older setup is
     moved there, with its server and slot.
   - If D2RLoader isn't in your game folder, it downloads it from d2rloader.net. Windows Defender often
     flags D2RLoader, which patches the game while it runs, as a trojan; if the download is quarantined, the
     tool tells you which folder to exclude. When a newer D2RLoader is out, the tool says so, and
     `d2r-ap-setup.exe --update-loader` installs it. Your D2RLoader settings and other plugins are kept.
2. Start the game through D2RLoader with the `Archipelago` mod active. If the mod doesn't match the seed
   you connect to, the plugin tells you to rerun the setup tool.

## Connecting

Open the console and type:

```
ap-connect archipelago.gg:38281 YourSlotName [password]
```

The plugin remembers the server and slot in `mods/Archipelago/d2rloader/config/archipelago.toml` and reconnects on its
own next time. `ap-status` shows the connection, and `ap-disconnect` drops it.

Items you receive, checks you send and other server messages show in the game chat as
`Archipelago: ...`, and in the console. The Codex charm's tooltip keeps the latest few.

Use a fresh character for each seed. The Codex charms (one for your class's skills, one for other
classes') must stay in your inventory; if you lose one, `ap-codex` rebuilds it.

Your character also gets an **Archipelago Ledger**. Hover over it to see how many checks you've done in
each act, and outside any act, out of the total. `ap-missing` lists the checks left in the act you're in,
and `ap-missing all` counts them for every act. If you lose the Ledger, `ap-ledger` makes another.

**F9** (or `ap-panel`) opens the Archipelago command panel, so you don't need the console for everyday
commands: status, checks left here or by act, hints found in game, the server's hints (`!hint`), items
left (`!remaining`), the multiworld's status (`!status`), `!collect`, a new Ledger or Codex, and reconnect
or disconnect. Answers show in the chat. The key can be changed in Options, Controls.
