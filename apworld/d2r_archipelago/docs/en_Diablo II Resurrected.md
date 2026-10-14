# Diablo II: Resurrected

## What does randomization do to this game?

Every class skill, across all eight classes, is shuffled into the multiworld. Your character can't spend
skill points on its own tree. Instead, each skill you receive is added to your **Archipelago Codex**, a
small charm in your inventory that grants every skill you've found. Codex skills work for any class, so a
Barbarian can end up casting Frozen Orb. Skills from other classes go on a second Codex: Diablo II needs
character level 6 for those, and on a shared charm they would switch off your own class's skills too.

A Codex skill's level is the number of copies of it you've found (`skill_copies`), plus your Codex
Empowerments, plus one for every few character levels (`skill_level_scaling`, default every 5 levels, up to
`skill_scaling_cap`, default +10), so skills found early keep up. `levels_per_copy` and `levels_per_empowerment`
make each copy or Empowerment worth more than +1, and `codex_level_cap` (default 60) is the most a skill gets.
`skill_focus` picks the pool: any skills, a share from your class (`class_weight_percent`, default half), or fewer
skills with more copies each.

### Tree mode

With `skill_mode: tree`, there's no Codex. Your skills are the 30 in your class's skill tree, each one
locked until its item arrives. When it does, the skill gets its first point in the tree and from then on
you spend skill points on it as usual, prerequisites aside (there are none). Extra copies of a skill and
**Skill Point** items (`skill_points`) give unspent skill points. A locked skill's icon is grey with a
padlock, so you can see what you've unlocked even with no points to spend. With `character_class: any`, the seed
picks your class; the client tells you which. After your first kill (or `ap-claim`) you get an
**Archipelago Emblem**, a one-cell item that tells the plugin your class, so later games need no kill
before your rewards arrive. Keep it on your character (not the shared stash); a lost one is made again.

`skill_tree_shuffle` deals every class's tree out of skills from all classes, each in a row of its own
level, with icons and synergies to match: a synergy names a skill in the same tree. The Druid keeps its
shapeshifts. It turns on tree mode, so the shuffled tree is the one you unlock.

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
  killing The Cow King, and killing 100 cows. The cows add up per difficulty over the whole seed, so they
  can take several games (cows killed before you connect count too, once you do). Only cows in the Cow Level
  count, unless `shuffle_cow_level` moved them to another area, where they count instead. The King closes the
  portal, so kill him last; the 100 cows only hold filler, in case you don't.
- Optional collections, which only ever hold filler: each rune up to `rune_checks`, every gem (`gem_checks`),
  every complete Lord of Destruction set (`set_checks`), and with a Hell goal through Baal the Pandemonium keys, organs,
  essences, Token of Absolution, Hellfire Torch and Annihilus (`special_item_checks`). Items count anywhere on
  your character, stash included; items Archipelago gives you don't.
- Picking up quest items (`quest_item_checks`, on): the Scroll of Inifuss, Wirt's Leg, the Horadric Scroll,
  the Staff of Kings, the Book of Skill, Khalim's Eye, Heart, Brain and Flail, the Golden Bird, the Potion of
  Life, Mephisto's Soulstone, the Hellforge Hammer and the Scroll of Resistance on every difficulty, and the
  Horadric Cube once.
- Buying Archipelago Parcels (`shop_checks`, 3 per act by default) from each act's potion seller: Akara,
  Lysander, Ormus, Jamella and Malah. They cost 2,500 to 40,000 gold; one bought past the last check is
  refunded. `shop_vendor: blacksmith` moves them to Charsi, Fara, Hratli, Halbu and Larzuk, and
  `parcel_price_multiplier` (10 to 500%, default 100) scales their price and the refund.
- Filler-only extras: Pandemonium's six uber bosses with a Hell goal through Baal (`uber_checks`), killing 100,
  250 and 500 monsters in each act (`kill_count_checks`), and finding 1 to 50 uniques and 1 to 25 set items
  (`find_count_checks`).

## Keeping track

A status line at the top of the screen shows the checks done out of the total, and in the act you're in.
In the console, `ap-missing` lists the checks left in your current act and `ap-missing all` counts them per
act and the ones not tied to an act (collections, find counts). F9's **Checks Left by Act** does the same.

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
  `prophecy_price_multiplier` (10 to 500%, default 100) scales every price.
- Other players can send you a **Prophecy** (`prophecy_weight`).

Most hints tell you where your own important items are, act keys first. `hint_balance` (30% by default) of
them instead name one of your locations that holds another player's important item, and that player is told.
Found hints go to the server like bought ones, without costing hint points; `ap-hints` in the console lists
them.

## Locks

Optional locks block part of the game until their item turns up from the multiworld:

- `enforce_act_keys` (on): walking into an act you don't have the key for warps you back to the town of an
  act you can be in. With `enforce_act_keys_mode: message_only` you stay, and the chat says the act is locked
  and its checks wait for its key.
- `cube_lock`: the Horadric Cube won't open without **Horadric Cube Access**. Logic expects it before the
  Horadric Staff, Duriel and Khalim's Will.
- `town_portal_lock`, `potion_lock`: Town Portal scrolls and tomes, or potions, can't be used without **Town
  Portal Access** or **Potion Access**. Town Portal scrolls and tomes you carry are set aside until Town
  Portal Access, which hands them back with a new tome of `tp_access_tome_charges` charges (20; 0 for none).
  Potions in your inventory are set aside until Potion Access, which hands them back; belt potions stay but
  won't drink. With `potion_lock_mode: drink`, healing, mana and rejuvenation potions are drunk as you pick
  them up instead (filling life or mana, whatever the potion's size); with `rejuvenation_allowed`,
  rejuvenation potions aren't locked at all. The cow level checks wait for Town Portal Access, since its
  portal takes a tome.
- `stash_lock`, `merchant_lock`: the stash, or trading with merchants, needs **Stash Access** or **Merchant
  Access**. Shop checks wait for Merchant Access, and so do the cow level checks: the portal takes a Tome of
  Town Portal, and new characters only have a scroll.
- `waypoint_lock`: the waypoints of Acts I to III need that act's **Waypoints** item (Act I Waypoints...).
  Acts IV and V keep theirs: a waypoint is the only way back from them, so a check left behind stays reachable.

Each lock item needs a location that isn't filler-only, so in a short game they crowd out skills: a one-act
game should use at most a couple of locks. A lock that gates no check in your seed (the Cube Lock in a one-act
game, or the Merchant Lock with no shop checks and no cow level) counts as filler and can turn up anywhere.

## What else is randomized?

The setup tool rewrites the game's tables for your seed:

- `monster_shuffle`: areas trade monster line-ups, within their act (`nearby`) or from any act (`anywhere`).
  With `anywhere`, Normal-difficulty monsters are levelled to fit: each area only gets line-ups whose
  monsters have a version near its level, and each monster becomes that version
  (`monster_shuffle_tolerance` levels above the area at most, default 3). `shuffle_cow_level` lets the Secret
  Cow Level join in.
- `superunique_shuffle`: field superuniques like Bishibosh and Pindleskin become other monsters. Quest and
  act bosses stay as they are. `main_path_superunique_cap` (default 2) keeps the ones you can't avoid, like
  Rakanishu and Griswold, within that many levels of their own monster; the tougher swaps land in optional
  dungeons (the Crypt, the Cave, the Stony Tomb, Tal Rasha's tombs, the Ruined Temple and the Icy Cellar). Once
  one of those main-path superuniques is a new monster, it brings at most `main_path_superunique_minions`
  minions (default 5). `superunique_shuffle_spread` sets how many swap together with `nearby` (default 6), and
  `superunique_minions` scales every field superunique's minions in percent.
- `main_path_superunique_strength` scales the main-path superuniques' HP and damage in percent on every
  difficulty, and `main_path_superunique_level` sets their Normal level relative to their usual one (default 0:
  their usual level, whatever monster they became). These work on each superunique's own monster, so ordinary
  monsters of the same kind and the superunique's minions stay as they are.
- `runeword_shuffle`: every runeword gets a new recipe of similar-tier runes (`runeword_rune_spread` tiers
  away at most, default 2), listed in `runewords.txt` in the mod folder.
- `monster_density`, `champion_packs`, `better_drops` (uniques, sets and rares) and `better_magic` in percent.
- `experience_multiplier`, `item_level_requirements` and `item_stat_requirements`.
- `monster_density_nightmare`, `monster_density_hell`, `experience_multiplier_nightmare` and
  `experience_multiplier_hell` override Nightmare and Hell; `same` (the default) follows the Normal value.
- `death_experience_penalty`: the experience a death costs on Nightmare and Hell, in percent of the game's own
  5% and 10% of a level (default 100; 0 never loses any). It counts for DeathLink deaths too, and leaves gold
  loss, corpse recovery and Hardcore alone.

## What do other items look like?

Everything else arrives in your inventory, rolled when you receive it and scaled to your character level:

- **Random Unique** (weapon, armor or jewelry) and **Random Set Item**: one your level can use, so you won't
  find a Windforce at level 3: its level requirement is at most 12 below your level and 2 above it
  (`reward_level_below`, `reward_level_above`). The act boss quests always give a Random Unique
  (`boss_quests_give_uniques`; `boss_reward_type` can make it a unique weapon, armor or jewelry, a set item or
  a high rune).
- **Runes** in three tiers. High runes run from Pul to Zod, and the higher the rune, the rarer it is
  (`high_rune_curve`: flat, gentle, steep or very steep).
- **Socketed Base**: a white weapon or armor with two or more sockets, ready for a runeword.
  Each socket count from two through that base's legal maximum is equally likely.
- **Codex Empowerment**: +1 to every skill in your Codex.
- Potions and perfect gems.
- **Gold** (1,000 per character level, `gold_reward_per_level`), **Experience** (15% of your current level,
  `experience_reward_percent`) and **Random Charms** and **Random Jewels**, rolled at your level. Jewels take
  40% of `charm_weight` (`jewel_share`) and are rare 30% of the time (`rare_jewel_chance`), magic otherwise.
- **Random Shrine**: a helpful shrine (refill, defense, combat, a resistance, skill, mana recharge, experience
  or gem) appears at your feet. `shrine_pool` picks which ones; with none, there are no Random Shrines.
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
