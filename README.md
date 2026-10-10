# Diablo II: Resurrected Archipelago

An [Archipelago](https://archipelago.gg/) multiworld integration for Diablo II: Resurrected 3.x, built as a
[D2RLoader](https://www.nexusmods.com/diablo2resurrected/mods/1119) plugin.

Every class skill (all eight classes, 240 skills) is shuffled into the multiworld. You can't spend skill
points on the normal trees; instead each skill you receive is added to your **Archipelago Codex**, a small
charm that grants +1 to every skill you've found (D2's `oskill` property, which works for any class).

Locations are story quests on each difficulty up to the goal, first visits to waypoint and landmark areas,
and every fifth character level. The goal is killing Baal on the chosen difficulty.

Other rewards are rolled when they arrive and scaled to the character's level: random uniques and set
items, runes up to Zod, socketed runeword bases, Codex Empowerments (+1 to every Codex skill), and optional
Junk Backpack traps. Act boss quests always give a unique.

It's inspired by [Diablo II Archipelago](https://github.com/solida1987/Diablo-II-Archipelago) for LoD 1.10f,
but shares no code with it: that mod hooks fixed 1.10f addresses, while this one uses only D2RLoader's
public plugin API.

## Pieces

| Folder | What |
|---|---|
| `apworld/d2r_archipelago` | The Archipelago world (items, locations, options, logic). |
| `plugin/` | `d2rl-archipelago.dll`, the in-game client; `d2r-ap-setup.exe`, which writes the skill-lock mod; `ap_probe.exe`, a test client. |
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

## Installing and playing

See [the setup guide](apworld/d2r_archipelago/docs/setup_en.md).

## Status

The network client is tested against a real Archipelago server, and the setup tool against a live D2R
3.3 install. **The in-game parts haven't been run yet**: the Codex charm, the quest, area and level events,
and the overlay. The first in-game session needs to confirm:

- D2R creates a Magic small charm with only the forced `oskill` properties (`codex_quality` and `codex_item`
  in the config can change this).
- Oskills from the Codex work even though the trees' level requirement is 100.
- `QuestCompleted`'s quest row ids match D2's quest flags (the plugin logs every quest it sees).
- Uniques and set items come out as the rolled item: the plugin forces the table's `*ID` as the record and
  logs "created X as record N (asked for M)". If D2R rejects a record, it falls back to any unique for that base.

## Known limits

- One character per seed: fillers are handed out once per seed, to whichever character is playing.
- No DeathLink, no superunique or waypoint-object checks (D2RLoader has no events for them yet).
- The WebSocket client doesn't support compression. Current servers accept that with a warning.

## Third-party code

- [D2RLoader Plugin SDK](https://github.com/D2RLoader/PluginSDK) (MIT), vendored in `plugin/sdk`.
- [nlohmann/json](https://github.com/nlohmann/json) 3.12.0 (MIT), vendored in `plugin/third_party`.
- [CascLib](https://github.com/ladislav-zezula/CascLib) (MIT), fetched at build time for the setup tool.
