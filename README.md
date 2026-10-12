# Diablo II: Resurrected Archipelago

An [Archipelago](https://archipelago.gg/) multiworld integration for Diablo II: Resurrected 3.x, built as a
[D2RLoader](https://www.nexusmods.com/diablo2resurrected/mods/1119) plugin.

Every class skill (all eight classes, 240 skills) is shuffled into the multiworld, or in tree mode the 30
skills of your class's tree (optionally a tree dealt from every class's skills, with icons and synergies
to match), each unlocked when its item arrives. In Codex mode (the default) you can't spend
skill points on the trees; instead each skill you receive is added to your **Archipelago Codex**, a small
charm that grants every skill you've found (D2's `oskill` property, which works for any class), at a
level set by copies found, Codex Empowerments and your character level.

Locations are story quests, kills of the 41 field superuniques and first visits to waypoint and landmark
areas, each on every difficulty up to the goal, every fifth character level, and the Secret Cow Level on
difficulties finished before the goal. Optional collection checks (runes, gems, complete sets, Hell's keys,
organs, essences, the Torch and Annihilus) only hold filler, and so do uber, kill-count and find-count checks.
Quest item pickups and Archipelago Parcels bought from each act's potion seller are checks too. Optional
locks keep the Horadric Cube, Town Portal, potions, the stash, merchants and the waypoints of Acts I to III closed until
their item arrives, and locked acts warp you back to town. Hints turn up in game too: from Deckard Cain after
act bosses, torn pages from superuniques, and Scrolls of Prophecy sold at rising prices. DeathLink is optional. The goal is killing Baal on the chosen difficulty.

Each act after the first needs an Act Key from the multiworld; checks in a locked act are held until it
arrives. The defaults aim at a 4-6 hour game (Normal to Diablo). Games can be as short as Normal Act 1 or as
long as a full Hell run (`goal` and `goal_act`). The setup tool
also randomizes the game itself per seed: monster line-ups (levelled to fit when shuffled across acts),
superuniques, runeword recipes, monster density, champion packs, drop quality, experience and item
requirements.

Other rewards are rolled when they arrive and scaled to the character's level: random uniques and set
items (only ones your class can use), runes up to Zod, socketed runeword bases, magic charms and jewels, gold, experience, Codex
Empowerments (+1 to every Codex skill), shrines at your feet, and traps (Junk Backpack, Ambush, Poison Cloud,
Firestorm, Mana Drain, Tax Collector, Mercenary Down). Gold, experience, shrines and traps go through
D2RCore's console cheats (`cheat gold`, `cheat experience`, `cheat mspawn`, `cheat pcloud`...). Skills and Empowerments
scale with the game's length. Act boss quests always give a unique.

It's inspired by [Diablo II Archipelago](https://github.com/solida1987/Diablo-II-Archipelago) for LoD 1.10f,
but shares no code with it: that mod hooks fixed 1.10f addresses, while this one uses only D2RLoader's
public plugin API.

## Pieces

| Folder | What |
|---|---|
| `apworld/d2r_archipelago` | The Archipelago world (items, locations, options, logic). |
| `plugin/` | `d2rl-archipelago.dll`, the in-game client; `d2r-ap-setup.exe`, which writes the seed's mod (skill lock and shuffles); `ap_probe.exe`, a test client. |
| `tools/gen_data.py` | Regenerates the skill, area, quest, unique, set and base tables from excel files (`d2r-ap-setup --extract <folder>` dumps them from your install). |

## Building

The plugin needs Visual Studio 2022 Build Tools with the C++ workload and a Windows SDK.

```
cd plugin
build.cmd
build\ap_tests.exe
```

The apworld's tests run inside an Archipelago source checkout with `worlds/d2r_archipelago` linked in:

```
python -m pytest worlds/d2r_archipelago/test
```

## Playing in a multiworld

`python -I tools/package_release.py <archipelago-checkout>` builds and tests the plugin, builds the
apworld and options template (`tools/build_apworld.py`), and packs `dist/D2R-Archipelago-<version>.zip`
with the setup tool and the plugin side by side, the apworld, the template and the setup guide. The
plugin's version must match the world's.

The `.apworld` and D2RLoader are all a player needs. It carries `d2r-ap-setup.exe` and the plugin in `bin/` and adds a
**Diablo II Resurrected** button to the Archipelago Launcher (`client.py`), which also opens from the
`archipelago://` link on a room page: it unpacks the tool to `%LOCALAPPDATA%\d2r-archipelago\tools` and runs
`d2r-ap-setup --play`, which checks for D2RLoader (opening its download page if it's missing; Defender flags a
tool that downloads it), installs the plugin, writes the seed's mod, saves the connection
for the plugin and starts the game. Custom worlds can't be generated on archipelago.gg, so the host
generates locally with the `.apworld` and can upload the seed there to host it.

`tools/fuzz_apworld.py <archipelago-checkout> [rounds]` generates random option sets, solo and with up to
three players, and checks every seed fills and can be beaten.

## Installing and playing

See [the setup guide](apworld/d2r_archipelago/docs/setup_en.md).

## Status

Played in game (D2R 3.3, D2RLoader 1.3.1-beta, Lord of Destruction characters): connecting, the Codex
charm and its oskills, its tooltip, loot rewards, quest checks, quest-item checks and messages in the game
chat. Everything added since (DeathLink, traps, locks, shop parcels, in-game hints, superunique and cow
checks, the shuffles, the Archipelago Ledger) still needs a session; [PLAYTEST.md](PLAYTEST.md) lists what
to check.

What playing has shown about D2RLoader:

- The `QuestCompleted` event doesn't fire, so quests complete from their boss's death, their quest item
  reaching your inventory, or reaching the area they lead to (Den of Evil: 20 kills inside it). The event
  still counts if it ever fires.
- The overlay never renders (draw calls succeed, nothing shows), so messages go to the game chat through
  D2RCore's undocumented `DisplayServerChatMessage` export. Recheck it after D2RLoader updates.
- Item copies on the UI thread carry no seeds, so tooltips recognise the Codex by its unique row.

## Known limits

- No Universal Tracker or PopTracker support. In game, the Archipelago Ledger's tooltip and `ap-missing` show
  what's left.

- One character per seed: fillers are handed out once per seed, to whichever character is playing.
- Superunique kills are told apart by giving each field superunique a monster row of its own (D2RLoader's
  monster-death event only names the monster). They need the seed's mod from `d2r-ap-setup`.
- DeathLink kills you with D2RCore's `cheat killme` and spots your own deaths from D2R's death screen
  (the `YouDiedModal` UI message), falling back to the resurrection event. Not yet tested in game.
- No waypoint-object checks.
- The WebSocket client doesn't support compression. Current servers accept that with a warning.

## Third-party code

- [D2RLoader Plugin SDK](https://github.com/D2RLoader/PluginSDK) (MIT), vendored in `plugin/sdk`.
- [nlohmann/json](https://github.com/nlohmann/json) 3.12.0 (MIT), vendored in `plugin/third_party`.
- [CascLib](https://github.com/ladislav-zezula/CascLib) (MIT), fetched at build time for the setup tool.

Ideas, not code: levelling shuffled monsters to their area comes from
[mapron/d2modgen](https://github.com/mapron/d2modgen), and finding unique rows by name in the live tables
from [CelestialRayOne/d2rloader_plugins](https://github.com/CelestialRayOne/d2rloader_plugins).
