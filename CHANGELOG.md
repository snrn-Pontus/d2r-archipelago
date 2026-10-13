# Changelog

What changed for players in each release. The numbers are GitHub issues.

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
