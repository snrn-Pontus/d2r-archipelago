# Diablo II: Resurrected

## What does randomization do to this game?

Every class skill, across all eight classes, is shuffled into the multiworld. Your character can't spend
skill points on its own tree. Instead, each skill you receive is added to your **Archipelago Codex**, a
small charm in your inventory that grants +1 to every skill you've found. Codex skills work for any class,
so a Barbarian can end up casting Frozen Orb.

## What counts as a location?

- Completing each story quest, on every difficulty up to your goal.
- Entering each waypoint area, plus a few landmarks (Den of Evil, Tristram, the Chaos Sanctuary and more),
  for the first time.
- Reaching every fifth character level.

## What is the goal?

Kill the boss of the act set by `goal_act` on the difficulty set by `goal`. That sets the game's length:
`goal: normal` with `goal_act: 2` ends at Duriel and is a short game, `goal: hell` with `goal_act: 5` is the
full run. Quests, areas and level milestones past the goal are left out.

## What else is randomized?

The setup tool rewrites the game's tables for your seed:

- `monster_shuffle`: areas trade monster line-ups, within their act (`nearby`) or anywhere.
- `superunique_shuffle`: field superuniques like Bishibosh and Pindleskin become other monsters. Quest and
  act bosses stay as they are.
- `runeword_shuffle`: every runeword gets a new recipe of similar-tier runes, listed in `runewords.txt` in the
  mod folder.
- `experience_multiplier`, `item_level_requirements` and `item_stat_requirements`.

## What do other items look like?

Everything else arrives in your inventory, rolled when you receive it and scaled to your character level:

- **Random Unique** (weapon, armor or jewelry) and **Random Set Item**: one your level can use, so you won't
  find a Windforce at level 3. The act boss quests always give a Random Unique (`boss_quests_give_uniques`).
- **Runes** in three tiers. High runes run from Pul to Zod, and the higher the rune, the rarer it is.
- **Socketed Base**: a white weapon or armor with three or more sockets, ready for a runeword.
- **Codex Empowerment**: +1 to every skill in your Codex.
- Potions and perfect gems.
- Optionally, **Junk Backpack** traps that stuff your free inventory space with worthless gear
  (`junk_trap_percentage`).

The weight options (`unique_weight`, `set_item_weight`, `high_rune_weight`, `socketed_base_weight` and
`basic_loot_weight`) set the mix.

## What does another world's item look like in Diablo II?

Nothing drops. When you complete a check, a line at the top of the screen tells you what you sent and to whom.
