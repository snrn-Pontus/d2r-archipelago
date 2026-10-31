# Changelog

What changed for players in each release. The numbers are GitHub issues.

## 0.15.4 - 2026-10-31

Works with seeds from 0.15.0; no need to rerun `d2r-ap-setup`.

- Runes, gems, set pieces and uniques other characters left in the shared stash no longer send "Find" checks
  when a new character joins; only your own containers count (#186)

## 0.15.3 - 2026-10-31

Works with seeds from 0.15.0; rerun `d2r-ap-setup` (New Seed does it) for the names.

- The Teleport Charm and the Codex show their names instead of "An Evil Force", and a few of the game's own
  texts that clashed with them (like "Learn More") read right again (#184)

## 0.15.2 - 2026-10-31

Works with seeds from 0.15.0. For the fix, make a new seed and rerun `d2r-ap-setup`.

- The Teleport Charm needs level 6 (a Sorceress none) in tree mode too, instead of 24. Teleport itself
  can then take points from level 1 once it's unlocked in your tree (#182)

## 0.15.1 - 2026-10-31

Works with seeds from 0.15.0; no need to rerun `d2r-ap-setup`.

- Warlocks can get the Reign of the Warlock uniques (the grimoires, Dreadfang, Sling, Entropy Locket and the
  rest) and the Bane's Garments and Horazon's Splendor sets as rewards (#180)

## 0.15.0 - 2026-10-31

Make a new seed and rerun `d2r-ap-setup`.

- New option Teleport Charm: a unique small charm with +1 to Teleport that any class can use from its
  inventory (from level 6 for classes other than the Sorceress). Start with it, find it in one of your first
  checks, or anywhere in the pool. If you lose or sell it, you get a new one (#178)

## 0.14.5 - 2026-10-31

Works with seeds from 0.14.0; the fix applies to new seeds.

- Skill Tree Shuffle keeps Mirrored Blades, Cleave and Bind Demon in the Warlock's tree: Mirrored Blades
  doesn't work for other classes, and the other two are built the same way (#176)

## 0.14.4 - 2026-10-31

Works with seeds from 0.14.0. For a Warlock in the shuffle, make a new seed and rerun `d2r-ap-setup`.

- Skill Tree Shuffle deals the Warlock in when Include Warlock is on (or your Character Class is warlock):
  Warlock skills can land in any class's tree, and the Warlock's tree gets other classes' skills. Play
  these seeds on a Reign of the Warlock character. Without Include Warlock, the shuffle is as before (#174)

## 0.14.3 - 2026-10-31

Works with seeds from 0.14.0. The Warlock fixes need a new seed, and rerun `d2r-ap-setup` for the padlocks.

- Playing a Warlock: picking Character Class warlock now puts Warlock skills in the pool on its own, without
  Include Warlock. In tree mode a Warlock starts with its level-1 skills (Summon Goatman or Miasma Bolt on the
  starting weapon) instead of any of its skills, and its locked skills show padlocks like the other
  classes' (#172)

## 0.14.2 - 2026-10-17

Works with seeds from 0.14.0; no need to rerun `d2r-ap-setup`.

- Level checks are sent as soon as you reach the level, wherever you are, instead of being held until a
  later act's key arrives. Ones already held go out the next time a check is sent (#170)

## 0.14.1 - 2026-10-17

Works with seeds from 0.14.0; no need to rerun `d2r-ap-setup`.

- The Options editor greys out settings that do nothing with your other choices (DeathLink Mode without
  DeathLink, trap settings without traps, Codex settings in tree mode...) and says what each one needs.
  Their values are kept, so turning the other setting back on restores them. (#168)

## 0.14.0 - 2026-10-17

New options, so make a new seed to use them and rerun `d2r-ap-setup` so the plugin matches. They default
to how the game played before.

- Choose locks to get early: `local_early_locks` puts them in your own first checks (Act I with act keys
  on), `early_locks` in the first checks anyone in the multiworld can reach, in any game. In the Options
  editor, a lock's box only ticks while that lock is on. (#166)

## 0.13.1 - 2026-10-14

Works with seeds from 0.13.0; no need to rerun `d2r-ap-setup`.

- The plugin knows your character's name and class as soon as a game begins, so rewards and locks no
  longer wait for your first kill. A seed now belongs to the character that first plays it, by name: a
  mule of the same class no longer gets its rewards, and `ap-claim` moves the seed to another character (#155)
- The main menu's status line says which character to play: the seed's own ("play SorkenTwo (Sorceress,
  level 12)"), your characters of the seed's class, or "not connected" when the server can't be reached.
  Joining with the wrong character shows a banner right away (#156)
- The plugin logs each waypoint you activate, on every difficulty (`ap-debug quests` lists them). Groundwork
  for waypoint checks (#157)

## 0.13.0 - 2026-10-14

Rerun `d2r-ap-setup` for this version and make a new seed: most of these are new options, and the
death experience loss, shop prices, parcel vendor and free mercenary revive are written into the mod.
Every new option defaults to how the game played before.

- Death Experience Penalty: lose less experience when you die, or none at all (#58)
- Potion Lock Mode: potions picked up while potions are locked can be drunk on the spot, or
  rejuvenation potions can stay usable, instead of being set aside (#4, #37)
- Town Portal Lock sets scrolls and tomes aside with their charges instead of destroying them, and Town
  Portal Access gives them back. Town Portal Access Tome Charges sets how full the new tome is (#37)
- Enforce Act Keys Mode: walking into a locked act can just tell you it's locked instead of warping you
  to town (#37)
- Useful Starting Skills (on by default): you always start with at least one skill that attacks or
  summons, so no more two curses at level 1. Skill Blocklist keeps skills you never want out of the
  seed (#16)
- Codex options: levels per skill copy, levels per Codex Empowerment, the Codex level cap, and your
  class's share of the pool with class-weighted skills (#39)
- Reward options: gold and experience reward sizes, the level window for Random Uniques and Set Items,
  rare jewel chance, jewels' share of charms, the high rune curve, which shrines can appear, and what
  boss quests give (#35)
- Shop options: parcel and Scroll of Prophecy price multipliers, and parcels sold by the blacksmiths
  instead of the potion sellers (#36)
- DeathLink Mode: another player's death can drop you to 1 life, take gold or spring a trap instead of
  killing you. DeathLink Amnesty only sends every Nth death of yours (#41)
- Check threshold options: a level check every 1 to 10 levels, the level above which level checks hold
  only filler, half or double kill and find counts, and 50 to 500 cows (#40)
- Starting Gold and Starting Items (potions, scrolls, perfect gems, runes) given once at the start, and
  Free Mercenary Revive (#42)
- `archipelago.toml` can set how many banner lines show, how long they stay, and when checks, items and
  loot get summed into one line (#43)
- `d2r-ap-setup --mapsense` writes the seed's side areas as MapSense destinations, so MapSense can draw
  a route to them (#59)
- Areas and quest items count from the moment you join a game: the plugin reads the difficulty from the
  game instead of waiting for your first kill (#154)

## 0.12.0 - 2026-10-14

Rerun `d2r-ap-setup` for this version: the mod gets tree mode's starting skill and locked-skill icons.
The Skill Tree Shuffle and starting-skill changes apply to new seeds.

- Quests complete when the game itself marks them done, read from its own quest log, instead of from
  guesses (a boss kill, a quest item, reaching an area, 20 Den of Evil kills). Den of Evil no longer
  completes early, walking into Tristram no longer completes The Search for Cain, and a quest finished
  right before a Save & Exit still counts. An older character on a new seed doesn't hand it the quests it
  had already done. If a game update ever breaks this, the log says so and the old guesses take over (#138)
- Tree mode: skills you haven't unlocked show a greyed, padlocked icon in the skill tree, and unlocked ones
  show in colour even with no points to spend (#143)
- Tree mode: the starting weapon gives +1 to one of your starting skills, like the Sorceress's Fire Bolt
  staff, so it's usable at level 1. A shuffled Sorceress no longer gets a stray +1 Fire Bolt (#127)
- Skill Tree Shuffle always plays in tree mode, whatever Skill Mode says; the options editor switches the
  two together (#125)
- Areas you enter before your first kill still count if you Save & Exit before killing anything: the
  plugin reads the difficulty from your character's save (#130)
- The same for quest items handed in or used before your first kill, even after two Save & Exits in a
  row (#135)
- Handing the Golden Bird to Alkor, or cubing Khalim's Will or the Horadric Staff, right after picking up
  the last part no longer loses those parts' "Find" checks (#136)
- When the connection drops, the status line and the log say why: the server closed it, or a network
  error (#134)

## 0.11.1 - 2026-10-14

A plugin fix that works with existing seeds: run this download's `d2r-ap-setup` to install it.

- A quest item handed in or used in town right after a Save & Exit, before your first kill, now sends its
  check after that kill instead of being lost. The Scroll of Inifuss also counts once Akara has deciphered
  it, and entering Tristram sends its check if it was still missing (#123)

## 0.11.0 - 2026-10-13

Rerun `d2r-ap-setup` for this version: the Ledger is gone from the mod and the Emblem is new.

- The Archipelago Ledger is gone: the status line shows your check counts, and F9's "Checks Left by Act"
  (`ap-missing all`) the rest. In tree mode a small Archipelago Emblem takes over its other job: it tells the
  plugin your character's class, so rewards arrive in later games without a kill first (#54)
- The chat now shows items other players send each other, greyed and behind your own lines. Set
  `other_sends` in `archipelago.toml` to `mine` (none), `progression` (the default) or `all`; a big release
  shows a few and counts the rest (#49)
- A rune or gem handed out as a reward no longer sends its "Find" check after you rejoin the game; one you
  find yourself still does (#108)

## 0.10.1 - 2026-10-13

New seeds only; existing seeds and the mod are unchanged, so there's no need to rerun `d2r-ap-setup`.

- Skills from other classes only go to a class that can really use them. Smite no longer lands on a
  Barbarian or Amazon (it swung without hitting), the Assassin's traps stay with the Assassin, and Dodge,
  Avoid and Evade stay off the Paladin. Skills the game ties to their own class (Fend, Strafe, Jab, Leap,
  Leap Attack, Whirlwind, Double Swing, Double Throw, Frenzy, Inferno, Zeal, Arctic Blast, Fury, Shock Wave,
  Rabies, Hunger, the kicks and claw skills, Weapon Block) stay with it, and Charge, Impale and Blade Fury
  never go to the Necromancer (#117)

## 0.10.0 - 2026-10-13

Rerun `d2r-ap-setup` for this version: the runeword recipes and the Writ's name are in the mod.

- Fixed a crash when a reward, the Tome of Town Portal or returned potions didn't fit in your bags: nothing
  is dropped on the ground any more. Rewards wait until you make room (they arrive within 30 seconds), and
  a reward that only lacked room no longer turns into a smaller item or a rune (#109)
- Shuffled runewords now have the same recipes for Lord of Destruction and Reign of the Warlock characters,
  so `runewords.txt` is right for everyone; the few words only Reign of the Warlock has are marked (#110)
- The Writ of Absolution shows its name instead of "An Evil Force" (#113)
- F9 now groups commands into This Game, Server and Maintenance. Collect, Disconnect and Relearn Skills
  ask for confirmation with Cancel selected first; Relearn cancels safely if the connection or character
  changes before its queued skill commands finish (#47)
- F10 testing tools now include kill and cow counters near a check, reconnect controls, simulated save
  failures, healing potions and a guarded crash test for checking recovery (#107)

## 0.9.3 - 2026-10-13

- Tree mode: a received skill opens up in your skill tree for your own points instead of coming with a free
  level, so a respec (Token, Writ or Akara) refunds only what you spent and the skills stay unlocked, with no
  Relearn needed and no extra points (#106)

## 0.9.2 - 2026-10-13

- A character no longer needs a kill each game before its rewards, skills and locks work: the Archipelago
  Ledger remembers its class. A new character can use **Claim Character** (F9) or `ap-claim` right away, and
  a banner says when rewards are waiting (#105)
- An Identify scroll or tome works outside shops while Merchant Access is locked (#105)

## 0.9.1 - 2026-10-13

- The plugin keeps its own log, one file per launch in `d2rloader\archipelago\logs` (the last 10), with every
  check found and sent, every item received and what the chat showed, so a crash's log survives relaunching;
  attach that folder to bug reports (#104)
- Debug panel: a Starter Kit for a fresh test character (Cube, Token of Absolution, the four essences, Tome of
  Identify, rejuvenation potions; no checks), Mark the Log and Log State, and `ap-debug give` for any item
  without a check (#104)

## 0.9.0 - 2026-10-13

- Rewards only go to a character of the seed's class, so loading a mule no longer hands it the seed's loot,
  Codex and unlocks; `ap-claim` moves the seed to the character you're playing (#70)
- Gold, experience and skill rewards wait in the seed's save until the game accepts them, and after a crash
  `ap-redeliver` hands back what that game lost (#70)
- Hints found while disconnected reach the server when you reconnect, and a goal finished before connecting
  still counts (#71)
- A kill or cow count check missed by a crash or disconnect is sent on the next connect, with no extra kill
  (#78)
- The 100 cows add up per difficulty over the whole seed, across games and reconnects, and only cows in the
  Cow Level count (#79)
- Reconnecting or switching slots no longer hangs or saves into the wrong seed, a save that can't be written
  pauses rewards and says so, and no two runewords share a rune recipe; rerun `d2r-ap-setup` (#75)

## 0.8.1 - 2026-10-13

- Starting a new game no longer carries over the last character's level, class and area, so it no longer
  sends level checks you didn't earn, builds the Codex for the wrong class or fires town traps outside town
  (#72)
- `ap-disconnect` really disconnects, and one bad packet from the server no longer leaves the client stuck
  on Connecting or loses the items after it (#69)
- New seeds: locks that gate nothing (Horadric Cube Access in a one-act game, Merchant Access with no shop
  checks or cow level) count as filler, so they no longer take a spot a skill could have (#77)

## 0.8.0 - 2026-10-13

- Superuniques you can't avoid keep their usual level on Normal, whatever Superunique Shuffle made them, and
  the new Main-Path Superunique Strength option scales their HP and damage on every difficulty; Battlemaid
  Sarina now counts as optional (#93)
- A reshaped main-path superunique brings at most 5 minions by default (Main-Path Superunique Minions), and
  Superunique Minions scales every field superunique's pack (#38)
- New tuning options: Nightmare and Hell values for Monster Density and Experience Multiplier, Better Magic
  Drops, Better Drops below 100, Monster Shuffle Tolerance, Superunique Shuffle Spread, Runeword Rune Spread
  and Shuffle Cow Level (#38)
- The Options editor can pick named settings that sit outside a range, like "same" and "off" (#38)

## 0.7.4 - 2026-10-13

- Switching to another slot with `ap-connect` no longer delivers the previous slot's items, and the new
  slot's first items are no longer skipped (#66)
- Superunique Shuffle no longer turns the superuniques you can't avoid (Rakanishu, Griswold...) into much
  stronger monsters; the new Main-Path Superunique Cap option (default 2 levels) sends the tough swaps to
  optional dungeons (#3)

## 0.7.3 - 2026-10-13

- Added an opt-in repair experiment: run `ap-repairs on`, then choose Trade/Repair at a blacksmith
  while Merchant Access is locked to try Repair All without opening the shop. It uses normal NPC
  repair handling, stays off by default, and still needs in-game verification (#5)

## 0.7.2 - 2026-10-13

- Socketed Base rewards now include two-socket weapons and armor for early runewords, while keeping
  larger socket counts available and respecting each base's socket limit (#61)

## 0.7.1 - 2026-10-13

- New Hell runs ending before Baal no longer include special-item checks that require inaccessible later
  acts; generate a new seed to apply this correction (#68)
- Invalid numeric Codex settings no longer crash the game at startup; the plugin keeps safe values and
  logs which setting needs fixing (#73)

## 0.7.0 - 2026-10-13

- Monster shuffling keeps the Ancients on Arreat Summit and leaves areas without random spawns alone;
  rerun setup to update an existing mod (#74)
- Impossible short-game settings now stop generation with a clear message when there are more required
  keys and locks than locations (#67)

## 0.6.0 - 2026-10-13

The first published release. Everything below is new since the 0.5.0 builds that were only tested locally.

- Side Area Checks: an option that adds random optional dungeons as checks, per difficulty (#31)
- An options editor that opens and saves player YAMLs, with dropdowns and sliders (#13)
- D2RLoader's game settings, such as stash size, can be set from the options YAML (#9)
- Trap tuning options: ambush size and monsters, gold tax, junk and town wait (#34)
- An option to keep act keys near their act in other Diablo II players' games (#12)
- A banner sound volume option (#7)
- The Writ of Absolution, a cheap respec, is sold at every potion seller (#6)
- Received skills unlock again after a Token of Absolution (#8)
- Skills your class can't use stay out of its item pool and skill tree (#1)
- Superunique check names say where the monster is (#26), each superunique has its own HD model (#2), and
  monsters that never spawn no longer have checks (#28)
- Ctrl- and Shift-clicks move locked items (#17)
- Rewards no longer land in a locked stash or cube (#11)
- Rewards after a `!release` arrive paced, so the game no longer crashes (#51)
- The plugin log names which source completed each quest (#22)
