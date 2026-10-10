# Diablo II: Resurrected Multiworld Setup

## Requirements

- Diablo II: Resurrected 3.x (Reign of the Warlock), installed through Battle.net.
- [D2RLoader](https://www.nexusmods.com/diablo2resurrected/mods/1119).
- The `d2rl-archipelago.dll` plugin and the `Archipelago` mod from this project's release.

The game runs offline through D2RLoader. Don't use Battle.net characters.

## Installing

1. Install D2RLoader into your Diablo II: Resurrected folder.
2. Copy `d2rl-archipelago.dll` into `<D2R folder>/d2rloader/plugins/`.
3. Run `d2r-ap-setup.exe` once. It reads `skills.txt` from your own game files and writes a small
   `Archipelago` mod that stops skill points being spent on the normal skill trees.
4. Start the game through D2RLoader with the `Archipelago` mod active.

## Connecting

Open the console and type:

```
ap-connect archipelago.gg:38281 YourSlotName [password]
```

The plugin remembers the server and slot in `d2rloader/config/archipelago.toml` and reconnects on its
own next time. `ap-status` shows the connection, and `ap-disconnect` drops it.

Use a fresh character for each seed. The Codex charm must stay in your inventory; if you lose it,
`ap-codex` rebuilds it.
