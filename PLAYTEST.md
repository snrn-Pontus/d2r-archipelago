# Playtest checklist

The parts that still need checking in game, grouped so each session tests one area. Tick a box once it
works; if it doesn't, note what happened and attach `mods\Archipelago\d2rloader\logs\` and `d2rloader\logs\` (the plugin logs every check, reward,
console command and refused item).

**Before every session:** after changing the plugin or the seed, rerun `d2r-ap-setup.exe` so the mod
matches, then start a new Lord of Destruction character. Most sessions test faster with items handed out
from the server console: `/send <slot> <item name>` (for example `/send Snrn Act I Waypoints`).

**Debug panel:** `ap-debug on` once (it's saved), then F10 or `ap-debug` opens it. Four columns:
- **Go To:** jumps to the areas the checks below need (Cold Plains, Stony Field, Tristram, Moo Moo Farm,
  Lut Gholein, Frigid Highlands...).
- **Character:** god mode, one-hit kills, heal, Level 6 / 30, skill points, gold, all waypoints, 25 Fallen,
  Andariel, Radament, Kill Me.
- **Items:** quest items, a unique ring, a Town Portal scroll and a potion, made in your inventory as if
  found, so they send their checks.
- **From the Server:** `!getitem` for all the lock items, the Normal act keys, a bundle of rewards, Codex
  Empowerment, Skill Point and each trap, the same as `/send`. They stay in the seed's save.

## Next up (from the 2026-10-12 session)

Built, pushed and installed, still to check in game:

- [ ] The **Archipelago Parcel** is in Akara's Misc tab now (shop item type fix) and buying it sends a check
      (section 2).
- [ ] Buying a Scroll of Prophecy tier you've already used refunds the gold (section 6).
- [ ] **Potion Access** gives back the potions set aside from the inventory (section 4).
- [ ] All eight 2x2 Scrolls of Prophecy fit in Akara's Misc tab next to her usual stock. If they crowd it,
      switch them to a 1x1 picture (size and picture must match).
- [ ] Parcels and scrolls are no longer quest items: check whether a vendor buys them back, and that a
      bought one doesn't stay in the inventory.

Ideas, not started:

- A Scroll of Prophecy tooltip line, e.g. "A hint about your world. Next prophecy: 1,000 gold", so it's clear
  which tier to buy (the tooltip only shows price and name).
- Optionally name shuffled superuniques after their new body ("Bishibosh (Soul Killer Shaman)"); with
  `superunique_shuffle` on, Bishibosh as a Flayer looks like an ordinary monster.

## 00. Mod-scoped plugin

- [x] After `d2r-ap-setup.exe`, the plugin is in `mods\Archipelago\d2rloader\plugins\` and the global copy
      is gone. D2RLoader's plugin list shows Archipelago as `[mod]`, not `[global]`.
- [x] Without `ap-connect`, it reconnects to the same server and slot (the config moved into the mod).
- [x] A character from before the move keeps its checks (save state stays in `d2rloader\archipelago\`).
- [ ] The banners still play sounds after the move.
- [ ] TCP/IP with a second player: joining works when both ran the new setup tool.
- [x] With the built `.apworld` installed in a release Archipelago: clicking your name on a room page opens
      the Launcher, **Diablo II Resurrected** sets up in its own window and starts the game, and the plugin
      connects without `ap-connect`. (The Launcher leaves its "Connect to Multiworld" popup open afterwards;
      that's Archipelago's.)
- [x] The Launcher's button alone, without a link, does the same with the last server and slot.
- [ ] With the game still running, the setup window says to close it and waits for Enter.

## 0. Codex charms

- [ ] Your class's skills are on one Codex and other classes' skills on a second one. Below level 6 the
      second one's skills don't work, but your class's still do.
- [ ] Codex skill levels: copies + Empowerments + 1 per 5 character levels, the last part stopping at +10.
- [ ] With `skill_tree_shuffle` in Codex mode, skills dealt into your class's tree go on your-class Codex
      and work from level 1; your class's original skills dealt elsewhere go on the other one.

## 0b. Tree mode

`skill_mode: tree`, `skill_tree_shuffle: true`, `character_class: barbarian`.

Seed AP_33491417470862561012 (2026-10-12): starting skills Plague Poppy and Tiger Strike;
Bishibosh holds Telekinesis, Dark Wood a Skill Point, the Scroll of Inifuss Fanaticism.

- [ ] No Codex. The starting skills have a point in the tree, and the chat says each one is unlocked.
- [ ] At level 2 a point goes into a starting skill (it rises to 2); a locked skill (Bash) refuses points.
- [ ] A skill found later gets its first point; after that, skill points go into it as usual, from its
      row's level (Telekinesis from Bishibosh: level 6; Fanaticism from the Scroll of Inifuss: level 30).
- [ ] A second copy, a found Skill Point (Dark Wood) and `/send Snrn Skill Point` each give an unspent
      skill point.
- [ ] After Save and Exit and rejoining, nothing is unlocked twice and no points are given again.
- [ ] Moved-in skills work for a Barbarian: Plague Poppy (Poison Creeper) summons, Tiger Strike charges.

## 0c. Skill tree shuffle

`skill_tree_shuffle: true` (either skill mode).

- [x] Your tree shows the dealt skills in their slots, with names and tooltips.
- [x] Icons match the skills, in the tree and on the skill bar and hotkeys.
- [x] No prerequisite arrows on the mouse skill panel.
- [x] Synergies name skills in the same tree, and a point in a partner raises the bonus.
- [x] A moved-in skill works for another class (Dragon Talon on a Barbarian).
- [ ] Controller (gamepad) skill panel: no arrows, right icons.
- [ ] Low graphics quality (the low-end sprites): icons and the arrow-free panel.
- [ ] Legacy graphics (Alt): the old icon files aren't rebuilt, so note what shows.
- [ ] Druid and Assassin skills dealt elsewhere: their tooltips list the new synergy partners.
- [ ] A Reign of the Warlock character: the other classes' trees are dealt the same way, and the Warlock's
      tree is unchanged.

## 1. Tracking: Ledger and ap-missing

Any seed with the defaults.

- [x] A new character gets an **Archipelago Ledger** next to the Codex, and the chat says to hover over it.
- [x] The Ledger shows the Token of Absolution picture in one cell, in Lord of Destruction too (fixed
      2026-10-12: LoD showed the previous item's 2x2 bark scroll).
- [ ] Its tooltip shows the total (`Archipelago: n/m checks`), one line per act in the seed with
      `done/total`, `> ` before the act you're in, and a "Not tied to an act" line.
- [ ] Entering Cold Plains raises Normal Act I's count by one; the tooltip updates without reopening anything.
- [ ] Stashing the Ledger doesn't make a second one. Dropping it and selling it doesn't make a new one
      either; `ap-ledger` does.
- [ ] `ap-missing` lists the checks left in your act by name; `ap-missing all` gives a count per act.
- [ ] With `ap-disconnect`, the tooltip says "not connected" and `ap-missing` says it needs the server.

## 1b. Command panel

Any seed, connected.

- [ ] F9 opens the Archipelago panel: a title, "Answers show in the chat.", twelve buttons in two
      columns and Close under them, over a dimmed screen. F9 again, Escape and Close each close it.
- [ ] `ap-panel` in the console opens and closes it too; Options, Controls lists "Archipelago Commands".
- [ ] Each button closes the panel and answers in the chat: Status, Checks Left Here, Checks Left by Act,
      Hints Found, New Ledger, Rebuild Codex; Server Hints, Items Left, Multiworld Status and Collect show the
      server's reply. Disconnect then Reconnect drops and restores the connection.
- [ ] Clicks on the panel don't move the character or hit anything behind it.
- [ ] Controller: the buttons can be reached and pressed with the gamepad.

## 1c. Debug panel

Any seed, connected. Several of these D2RCore cheats haven't been tried yet; note any that do nothing
(the log has each command).

- [ ] F10 does nothing until `ap-debug on`; then it opens "Archipelago Debug" with four headed columns of
      twelve buttons and Close, all on screen. `ap-debug off` closes it and F10 stops working.
- [ ] Go To: Cold Plains and Tristram from Rogue Encampment. Lut Gholein without the Act II key warps you
      back (that's section 3's test); with the keys from From the Server, Kurast Docks and Harrogath work.
- [ ] God Mode, One-Hit Kills (`easykill`), Level 6 and Level 30 (`level <n>`) do what they say.
- [ ] Spawn Andariel / Spawn Radament: killing the spawn sends their quest check.
- [ ] Items: each lands in the inventory and its check is sent; the unique ring counts for "Find 1 Unique".
- [ ] From the Server: the chat shows "Cheat console: sending ..." and the item arrives.

## 1d. Banners

Any seed, connected. D2RLoader's overlay draws nothing (every call succeeds, nothing shows), so banners
are a click-through window of the plugin's own over the game. `notifications = false` in the plugin
config turns them off.

- [x] A received item shows "Received: ..." near the top centre in Exocet, gold for keys, locks, skills,
      Empowerments and Skill Points; clicks go through it.
- [x] Traps show in red; ordinary rewards in white. (Still to see: "from <player>" for another slot's item.)
- [ ] A check shows a grey "Check: ..." line, naming another player's item it held, and "(held)" in a
      locked act. More than four at once become one "Checks sent: n" line.
- [x] Sounds (converted from the game's own by d2r-ap-setup into `d2rloader\archipelago\sounds`; `--sounds`
      writes only those): gold banners play the quest-done chime, red the hostile warning, white the gem
      sound; checks are silent. `banner_sounds = false` turns them off.
- [ ] `banner_volume` (percent, default 100, up to 200 for the game's own loudness, 0 silent) changes how
      loud the banner sounds play, after a restart.
- [x] Status line, top-left by default: "Archipelago done/total - <act> done/total", with "n held" when
      checks wait for a key; it updates within a second of a check. `status_line = false` hides it and
      `status_position` moves it (top-left, top, top-right, bottom-left, bottom-right).
- [ ] With a hired mercenary, the status line and the mercenary's portrait don't overlap.
- [ ] Several items at once: skills, keys, locks and traps each get a banner; more than three ordinary
      items become "Received n more items", and more than ten in all become one line.
- [x] Alt-Tab away and back, and minimizing: the banner stays with the game and doesn't float over
      other windows.

## 2. New checks

Defaults, plus `shop_checks: 3`, `kill_count_checks: true`, `find_count_checks: true`.

- [ ] Killing **Bishibosh** (Cold Plains) and **Rakanishu** (Stony Field) each sends a check. A plain
      monster of the same type doesn't.
- [ ] Picking up the **Scroll of Inifuss** and **Wirt's Leg** each sends a check, once.
- [ ] Buying an **Archipelago Parcel** from Akara (2,500 gold) sends a check and the parcel disappears. The
      fourth one is refunded.
- [ ] 100 kills in Act I sends "Kill 100 monsters"; finding your first unique sends "Find 1 Unique".
- [ ] Rewards don't count: a Random Unique from the server doesn't count toward "Find 1 Unique".

## 2b. Quest items used at once

Defaults (`quest_item_checks` on). The plugin scans for quest items every 5 seconds, so these are now also
recorded the moment you use them. Use each one right after picking it up, within a second or two.

- [ ] **Book of Skill** (Radament): read it at once; "Find Book of Skill" still sends.
- [ ] **Horadric Scroll**: read it at once; "Find Horadric Scroll" still sends.
- [ ] **Potion of Life** (Alkor, after the Golden Bird): drink it at once; "Find Potion of Life" still sends.
- [ ] **Scroll of Resistance** (Anya): read it at once; "Find Scroll of Resistance" still sends.
- [ ] Each sends once, not again on the next scan or after rejoining.
- [ ] Note whether handing the Golden Bird to Alkor right after Meshif gives it to you still sends
      "Find The Golden Bird" (this one is handed over, not used, so the change doesn't cover it).

## 3. Act keys

Defaults (`act_keys` and `enforce_act_keys` on).

- [ ] Walking into Act II without its key warps you back to the Rogue Encampment.
- [ ] With `enforce_act_keys: false`, Act II's checks are recorded but held: the chat says so, the Ledger
      line shows "held until their act's key arrives", and they're sent all at once after
      `/send Snrn Normal Act II Key`.

## 4. Locks

`cube_lock`, `town_portal_lock`, `potion_lock`, `stash_lock`, `merchant_lock` and `waypoint_lock` all on.

- [x] Blocked with a chat message: drinking a potion, opening the stash (`BankExpansionLayout`), trading
      with a merchant (`VendorPanelLayout`), using an Act I waypoint (`WaypointsPanelExpansion`). You can
      still move afterwards.
- [ ] Opening the Cube is blocked (its panel name hasn't been seen yet; the log lists each panel opened).
- [x] Potions stay locked from the belt hotkeys too.
- [x] D2RLoader doesn't report drinking from the inventory (only the belt), so within 5 seconds potions
      in the inventory are set aside, with a chat message (confirmed 2026-10-12).
- [ ] Potion Access gives the set-aside potions back (to the ground if the inventory is full).
- [x] Town Portal: D2RLoader doesn't report scroll use, so within 5 seconds every scroll and tome on the
      character crumbles, with a chat message (the starting scroll too).
- [x] Town Portal Access brings a Tome of Town Portal, once.
- [ ] Each opens after its item: `Horadric Cube Access`, `Town Portal Access`, `Potion Access`,
      `Stash Access`, `Merchant Access`, `Act I Waypoints`. (Stash confirmed after All Lock Items.)
- [ ] Shop parcels can't be bought before Merchant Access.
- [ ] None of the blocks freezes the game or leaves a panel half-open.

## 5. Rewards and traps

Any seed. Send each with `/send`, standing outside town for the traps.

- [ ] `Gold` (works), `Experience`, `Random Charm`, `Random Jewel`, `Socketed Base`, `Random Set Item`: arrive,
      scaled to your level.
- [x] `Random Shrine` appears at your feet and works.
- [ ] Traps wait while you're in town and fire when you leave: `Junk Backpack`, `Ambush Trap`,
      `Poison Cloud Trap`, `Firestorm Trap`, `Mana Drain Trap`, `Tax Collector Trap`,
      `Mercenary Down Trap` (with a hired mercenary). Ambush and Poison Cloud work.
- [ ] `Codex Empowerment` raises every Codex skill by one.

## 5b. Personalized rewards

Uniques and sets found by another player carry their name, like Anya's reward. The item-data offsets are
a guess checked against the item before writing, so test the command first.

- [ ] Hold a junk unique on the cursor and run `ap-personalize` (no name). The console says the layout
      matches; the log has the item data. If it says it doesn't match, send me the log lines.
- [ ] `ap-personalize Bob` on the same item, then Save and Exit and rejoin: it reads "Bob's <item>", and
      the game doesn't crash.
- [ ] Multiworld seed with a second slot: a `Random Unique` that slot finds arrives named after it, and
      chat says "you got <item> from <name>!". Your own finds and `/send` (from the server) stay unnamed.
- [ ] Several `/send`s of `Random Unique`, `Random Set Item` and `Socketed Base` never give another
      class's gear (no Arreat's Face or Paladin shield on a Sorceress); your own class's still turn up.

## Found in testing

- `ap-core <command>` runs any D2RCore command for testing, e.g. `ap-core skillpoints 3`.
- D2RCore runs gold, experience, mspawn, pcloud, killme, skadd and the rest only as cheats
  (`cheat gold 100`); the plugin adds the prefix. Typed by hand they need `debug_mode`, but the plugin's
  calls work without it.
- D2R matches `data\hd\items\items.json` to items by position, not code, and both banks share it. Any item
  the mod adds must sit on the same row in both banks' tables (setup pads LoD's misc for RotW's `cs2`), and
  its inventory size must match its picture.
- Vendors only stock `spawnable` items whose item type has a `StorePage`; the quest type has none, so the
  shop items use their own `apsh` type.
- `cheat skadd <skill>,1` gives a locked skill its first point, and after that normal skill points can go
  into it: the basis for a randomized skill tree mode.

## 6. Hints

`page_hint_chance: 100`, `prophecy_shop: true`, `cain_hints: true`.

- [ ] The first Bishibosh kill drops a torn page hint; the second kill doesn't.
- [x] A Scroll of Prophecy from Akara (1,000 gold) gives a hint (confirmed 2026-10-12, after the shop item type fix).
- [ ] A cheaper tier than the next one is refunded.
- [ ] After Andariel, Deckard Cain says "Stay awhile and listen..." and gives a hint.
- [ ] `ap-hints` lists them; the hints show on the server (`!hint` in the text client) without costing
      hint points.
- [ ] `/send Snrn Prophecy` gives a hint too.

## 7. DeathLink

Add a second slot to the multiworld, `Players\Probe.yaml`: a copy of `Snrn.yaml` with `name: Probe`. Turn
on `death_link: true` in both.

- [ ] Dying sends a death: the server log shows a DeathLink from Snrn.
- [ ] `ap_probe localhost:38281 Probe --death` kills your character (through `cheat killme`), with the cause
      in chat.
- [ ] The death that DeathLink caused isn't sent back out.
- [ ] Dying once sends one death, not one per frame or per resurrection.

## 8. Shuffles

`monster_shuffle: anywhere`, `superunique_shuffle: anywhere`, `runeword_shuffle: true`,
`monster_density: 150`, `experience_multiplier: 200`.

- [ ] Blood Moor and Cold Plains have monsters from other acts, at a level that fits (not Hell-strength in
      Normal).
- [ ] Bishibosh is a different monster but still sends his check. (2026-10-12: he was a Flayer shaman, as
      the seed said; the check is still to confirm.)
- [ ] `mods\Archipelago\runewords.txt` lists new recipes, and one made from low runes works in game.
- [ ] More monsters, and faster levelling than usual.

## 9. Later: long runs

These need a character that has beaten Baal, so test them during a real run.

- [ ] Secret Cow Level (on difficulties before the goal): entering it, The Cow King, and 100 cows each send
      a check.
- [ ] With `merchant_lock`: the cow portal needs a Tome of Town Portal, so logic now waits for Merchant
      Access. Check whether a tome ever drops or can be had without a merchant (new characters only have
      a scroll); if it can't, the rule is right.
- [ ] Quest checks that only use fallbacks: Radament (Book of Skill), the Golden Bird, Lam Esen's Tome,
      rescuing Anya, Prison of Ice, Siege on Harrogath.
- [ ] Hell goal: Pandemonium keys, organs, essences, the Torch and Annihilus; the uber kills.
- [ ] The goal: killing the goal boss sends it, and only once its act's key is in.
