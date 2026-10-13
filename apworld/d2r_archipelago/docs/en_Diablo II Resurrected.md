# Diablo II: Resurrected

## What does randomization do to this game?

Every class skill, across all eight classes, is shuffled into the multiworld. Your character can't spend
skill points on its own tree. Instead, each skill you receive is added to your **Archipelago Codex**, a
small charm in your inventory that grants every skill you've found. Codex skills work for any class, so a
Barbarian can end up casting Frozen Orb. Skills from other classes go on a second Codex: Diablo II needs
character level 6 for those, and on a shared charm they would switch off your own class's skills too.

A Codex skill's level is the number of copies of it you've found (`skill_copies`), plus your Codex
Empowerments, plus one for every few character levels (`skill_level_scaling`, default every 5 levels, up to
`skill_scaling_cap`, default +10), so skills found early keep up. `skill_focus` picks the pool: any skills, half from your class, or fewer skills
with more copies each.

### Tree mode

With `skill_mode: tree`, there's no Codex. Your skills are the 30 in your class's skill tree, each one
locked until its item arrives. When it does, the skill gets its first point in the tree and from then on
you spend skill points on it as usual, prerequisites aside (there are none). Extra copies of a skill and
**Skill Point** items (`skill_points`) give unspent skill points. With `character_class: any`, the seed
picks your class; the client tells you which.

`skill_tree_shuffle` deals every class's tree out of skills from all classes, each in a row of its own
level, with icons and synergies to match: a synergy names a skill in the same tree. The Druid keeps its
shapeshifts. It works in both modes; in tree mode, the shuffled tree is the one you unlock.

## What counts as a location?

- Completing each story quest, on every difficulty up to your goal.
- Entering each waypoint area, plus a few landmarks (Den of Evil, Tristram, the Chaos Sanctuary and more),
  for the first time, on every difficulty up to your goal.
- Entering a few side areas, the bottom of optional dungeons you'd usually skip (the Mausoleum, the Ancient
  Tunnels, the Kurast temples, Abaddon and more): `side_area_checks` of them on each difficulty (5 by
  default), picked at random, so each difficulty sends you somewhere different.
- Reaching every fifth character level. Past level 70 these only hold filler.
- Killing each of the 31 superuniques that roam the field (Bishibosh, Rakanishu, Pindleskin and the rest), on
  every difficulty up to your goal (`superunique_checks`). Quest bosses and the Chaos Sanctuary seal bosses
  don't count.
- The Secret Cow Level, on each difficulty you finish before the goal (`cow_level_checks`): entering it,
  killing The Cow King, and killing 100 cows. The King closes the portal, so kill him last; the 100 cows
  only hold filler, in case you don't.
- Optional collections, which only ever hold filler: each rune up to `rune_checks`, every gem (`gem_checks`),
  every complete Lord of Destruction set (`set_checks`), and with a Hell goal the Pandemonium keys, organs,
  essences, Token of Absolution, Hellfire Torch and Annihilus (`special_item_checks`). Items count anywhere on
  your character, stash included; items Archipelago gives you don't.
- Picking up quest items (`quest_item_checks`, on): the Scroll of Inifuss, Wirt's Leg, the Horadric Scroll,
  the Staff of Kings, the Book of Skill, Khalim's Eye, Heart, Brain and Flail, the Golden Bird, the Potion of
  Life, Mephisto's Soulstone, the Hellforge Hammer and the Scroll of Resistance on every difficulty, and the
  Horadric Cube once.
- Buying Archipelago Parcels (`shop_checks`, 3 per act by default) from each act's potion seller: Akara,
  Lysander, Ormus, Jamella and Malah. They cost 2,500 to 40,000 gold; one bought past the last check is
  refunded.
- Filler-only extras: Pandemonium's six uber bosses with a Hell goal through Baal (`uber_checks`), killing 100,
  250 and 500 monsters in each act (`kill_count_checks`), and finding 1 to 50 uniques and 1 to 25 set items
  (`find_count_checks`).

## Keeping track

You start with an **Archipelago Ledger**, a small item whose tooltip shows the checks done in each act out of
its total, with the act you're in marked, and the ones not tied to an act (collections,
find counts). In the console, `ap-missing` lists the checks left in your current act and `ap-missing all`
counts them per act.

## What is the goal?

Kill the boss of the act set by `goal_act` on the difficulty set by `goal`. That sets the game's length:
`goal: normal` with `goal_act: 2` ends at Duriel and is a short game, `goal: hell` with `goal_act: 5` is the
full run. Quests, superuniques, areas and level milestones past the goal are left out.

## DeathLink

With `death_link`, when you die everyone else with DeathLink dies too, and the other way round. Careful with
Hardcore characters: a DeathLink death is a real one.

## What makes me wait for other players?

With `act_keys` (on by default), every act after the first needs its key, such as "Normal Act II Key", and
the keys are shuffled into the multiworld. You can still walk into a locked act, but the quests and areas
you complete there (and the superuniques you kill) are only recorded; they're sent, all at once, when the key arrives. Level milestones
count toward the act they're spread over (level 30 toward Act IV on a Normal goal), and are held the same way. The defaults (Normal to Diablo, with a little more experience and loot) aim at a 4 to 6 hour
game.

When several players play Diablo II, `act_key_window` (1 by default) keeps your keys from sitting deep in
their games: the key for your Act N only lands in their Act N - 1 + the window or earlier, counting
difficulties too (a Nightmare Act I key can be in their Normal Act V). With 1, your Normal Act II Key is in
their Act I or II, so nobody waits on someone else's Act V. 0 keeps every key in an earlier act, and
`anywhere` turns the limit off. Keys in other games can still be anywhere.

## Hints you find in the game

Besides the hints you buy with hint points, the game hands some out:

- **Deckard Cain** has one after each act boss quest (`cain_hints`).
- Superuniques may drop a **torn page** the first time you kill them (`page_hint_chance`, 25%).
- Every potion seller sells **Scrolls of Prophecy** (`prophecy_shop`). The vendor lists eight prices from
  1,000 to 100,000 gold; only the next one counts (a cheaper scroll is refunded), and the dearest repeats.
- Other players can send you a **Prophecy** (`prophecy_weight`).

Most hints tell you where your own important items are, act keys first. `hint_balance` (30% by default) of
them instead name one of your locations that holds another player's important item, and that player is told.
Found hints go to the server like bought ones, without costing hint points; `ap-hints` in the console lists
them.

## Locks

Optional locks block part of the game until their item turns up from the multiworld:

- `enforce_act_keys` (on): walking into an act you don't have the key for warps you back to the town of an
  act you can be in.
- `cube_lock`: the Horadric Cube won't open without **Horadric Cube Access**. Logic expects it before the
  Horadric Staff, Duriel and Khalim's Will.
- `town_portal_lock`, `potion_lock`: Town Portal scrolls and tomes, or potions, can't be used without **Town
  Portal Access** or **Potion Access**. Potions in your inventory are set aside until Potion Access, which
  hands them back; belt potions stay but won't drink.
- `stash_lock`, `merchant_lock`: the stash, or trading with merchants, needs **Stash Access** or **Merchant
  Access**. Shop checks wait for Merchant Access, and so do the cow level checks: the portal takes a Tome of
  Town Portal, and new characters only have a scroll.
- `waypoint_lock`: the waypoints of Acts I to III need that act's **Waypoints** item (Act I Waypoints...).
  Acts IV and V keep theirs: a waypoint is the only way back from them, so a check left behind stays reachable.

## What else is randomized?

The setup tool rewrites the game's tables for your seed:

- `monster_shuffle`: areas trade monster line-ups, within their act (`nearby`) or from any act (`anywhere`).
  With `anywhere`, Normal-difficulty monsters are levelled to fit: each area only gets line-ups whose
  monsters have a version near its level, and each monster becomes that version.
- `superunique_shuffle`: field superuniques like Bishibosh and Pindleskin become other monsters. Quest and
  act bosses stay as they are.
- `runeword_shuffle`: every runeword gets a new recipe of similar-tier runes, listed in `runewords.txt` in the
  mod folder.
- `monster_density`, `champion_packs` and `better_drops` (uniques, sets and rares) in percent.
- `experience_multiplier`, `item_level_requirements` and `item_stat_requirements`.

## What do other items look like?

Everything else arrives in your inventory, rolled when you receive it and scaled to your character level:

- **Random Unique** (weapon, armor or jewelry) and **Random Set Item**: one your level can use, so you won't
  find a Windforce at level 3. The act boss quests always give a Random Unique (`boss_quests_give_uniques`).
- **Runes** in three tiers. High runes run from Pul to Zod, and the higher the rune, the rarer it is.
- **Socketed Base**: a white weapon or armor with three or more sockets, ready for a runeword.
- **Codex Empowerment**: +1 to every skill in your Codex.
- Potions and perfect gems.
- **Gold** (1,000 per character level), **Experience** (15% of your current level) and **Random Charms**
  and **Random Jewels**, rolled at your level.
- **Random Shrine**: a helpful shrine (refill, defense, combat, a resistance, skill, mana recharge, experience
  or gem) appears at your feet.
- **Traps** (`trap_percentage`, 10% of filler by default), which wait until you leave town: **Junk Backpack**
  (worthless gear fills your free inventory space), **Ambush** (a pack of the act's monsters), **Poison
  Cloud**, **Firestorm**, **Mana Drain**, **Tax Collector** (lose up to 1,000 gold per level) and, only if you
  give it a weight, **Mercenary Down**. Each has its own weight option. The traps section can also make traps
  go off in town (`traps_wait_for_town`), change the ambush size (`ambush_size`) and bring the area's own
  monsters instead of the act's (`ambush_monsters: area`), and set the tax per level (`tax_collector_gold`)
  and the most junk a backpack holds (`junk_backpack_size`).

The skill pool and Codex Empowerments are set for a four-act game; with `scale_with_length` (on by default)
longer games get proportionally more, so a full Hell run has about 60 skills and 8 Empowerments.

The weight options (`unique_weight`, `set_item_weight`, `high_rune_weight`, `socketed_base_weight` and
`basic_loot_weight`) set the mix.

## What does another world's item look like in Diablo II?

Nothing drops. When you complete a check, a line at the top of the screen tells you what you sent and to whom.
