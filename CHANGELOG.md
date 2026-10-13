# Changelog

What changed for players in each release. The numbers are GitHub issues.

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
