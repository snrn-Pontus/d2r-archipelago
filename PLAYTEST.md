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
- [ ] Class-usable skills (PR #18, new seed needed): a Sorceress pool and tree hold no claw skills, no
      Assassin traps (Fire Blast froze a Sorceress), no Frenzy or Double Swing, and no wolf or bear attacks.
      A Druid only gets Feral Rage, Maul and the like alongside Werewolf or Werebear.
- [ ] A Barbarian with an Assassin trap in the pool (allowed: they have the S2 animation) can cast it
      without freezing.
- [ ] Werewolf or Werebear on another class: does the form work? If not, keep forms Druid-only too.
- [ ] `ap-relearn` (tree mode): with all skills unlocked it says so; after a respec at Akara it lists the
      lost skills and asks for `ap-relearn confirm`, which gives each one level back; running `confirm`
      again changes nothing.
- [ ] Respec with a Token of Absolution (tree mode, #8; cube the four essences for one): within about 5 seconds the chat says the Archipelago skills are
      unlocked again and names them, and each has 1 point in the tree. Count the unspent points before and
      after: does the respec refund the skadd levels too (then each skill comes back as an extra point)?
- [ ] Right-clicking a Token of Absolution in the stash or Cube changes nothing, and leaving the game right
      after using one doesn't relearn in the next game.
- [ ] Writ of Absolution (#6, rerun setup): every potion seller's Misc tab has one for 1,000 gold, with the
      Token of Absolution's picture. Right-clicking it respecs like the token, and in tree mode the skills
      come back on their own. Buying it doesn't count as the Token of Absolution check. Can't buy it
      before Merchant Access.
- [ ] Finishing Den of Evil in tree mode shows the hint that Akara's respec needs Relearn Skills; only once
      per difficulty.
- [ ] Shuffled superuniques are visible now (they had no HD model): after rerunning setup, Rakanishu in
      Stony Field and his minions show their new monster's model (section 8).
- [ ] Main-path superunique cap (#3, new seed needed, `superunique_shuffle: nearby`, default
      `main_path_superunique_cap: 2`): setup prints the superuniques that changed shape. On Normal, Griswold
      in Tristram and Rakanishu in Stony Field are no tougher than their usual selves (no level-15 mummy for
      Griswold); the stronger swaps show up in the Crypt, the Cave or the Stony Tomb. With the cap at 50,
      Griswold can be strong again.
- [ ] Shuffle tuning (#38, new seed needed): with defaults, a reshaped Rakanishu brings 5 minions, not 8;
      Bishibosh, if he kept his Fallen Shaman, still brings his 2. `superunique_minions: 50` halves every field
      superunique's pack and setup says how many got a new count. `monster_density_hell: 300` makes Hell
      crowded while Normal stays as set, and `experience_multiplier_nightmare` changes only Nightmare's
      experience (compare a kill's XP in the Blood Moor on Normal and Nightmare). `better_magic: 300` gives
      noticeably more blue items. `shuffle_cow_level: true` with `monster_shuffle: anywhere`: the Cow Level has
      other monsters, the Cow King is still there, and the cows turn up in some other area, where killing 100
      still sends the cow check. `runeword_rune_spread: 0` with `runeword_shuffle` leaves `runewords.txt`
      showing the usual recipes.
- [ ] Main-path superunique level and strength (#93, new seed needed): with defaults and
      `superunique_shuffle: nearby`, a reshaped Griswold or Rakanishu shows his usual level on Normal (hover
      him: the tooltip has no level, so compare how fast he dies at a known character level, or check
      `monstats.txt` in the mod for `ap_superunique_5`'s Level). `main_path_superunique_strength: 50` makes
      Bishibosh and Rakanishu die in about half the hits and hit softer, on Normal and Nightmare, while their
      minions and Bonebreak in the Crypt stay as usual. With `superunique_checks: false` and strength at 50,
      setup still says the superuniques have their own monster, they show their HD models, and no checks are sent.
- [ ] Unspawnable superuniques dropped (issue #28, new seed needed): `ap-missing` and the Ledger list 31
      superunique checks per difficulty, none without an area. Frigid Highlands has Eldritch, Sharptooth
      Slayer and Eyeback but no Axe Dweller, Anodized Elite, Vinvear Molech, Magma Torquer or Blaze Ripper.
- [ ] With `superunique_checks` and a mod from the old setup tool, the plugin says the mod doesn't match;
      after rerunning setup, killing Bishibosh and Corpsefire sends their own checks (not a neighbour's).
- [ ] Options editor (#13, needs the apworld in the release Archipelago): the Launcher shows **Diablo II
      Resurrected Options**; it opens a window (no error in the Launcher log). **Open...** a copy of
      `Players\Snrn.yaml`, change Goal Difficulty and Shop Checks, **Save**: only those two lines change and
      the comments stay. A preset, then **Save as...** a new name, generates a seed.
- [ ] D2RLoader settings from the YAML (#9, new seed needed): with `show_ground_sockets: on`,
      `display_item_levels: on`, `shared_stash_tabs: 10` and `materials_limit: 150`, setup lists them and writes
      `mods\Archipelago\d2rloader\config\d2rloader.toml`. In game, white items on the ground show sockets,
      tooltips show item levels, the shared stash has 10 extra tabs (after Stash Access with Stash Lock) and
      runes stack past 99. D2RLoader's Options show the mod's values winning. Then a seed with everything at
      `keep`: your own D2RLoader settings are back. Watch the loader log for a complaint about the mod file.
- [ ] Character switch (#72, `level_checks` on): play a high-level character (any seed), Save and Exit,
      then connect a fresh seed and start a level 1 character. No level checks are sent (`ap-missing` still
      lists them all), the Codex is built for the new class, and loot isn't high-level. With
      `traps_wait_for_town`, a trap sent right as the game loads in town waits until you leave town.
      Level 6 from the debug panel then sends only the level-5 check.

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
- [ ] `banner_volume` (percent, default 10, up to 200 for the game's own loudness, 0 silent) changes how
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
- [ ] Side areas (#31): `ap-missing` (or the Ledger) lists five "Normal: Enter <side area>" checks. Walking
      into one of them (e.g. the Mausoleum or Cave Level 2) sends its check; a side area the seed didn't
      pick sends nothing and shows no banner.

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

## 2c. D2RLoader's quest event (issue #20)

Quest checks use fallbacks (boss kill, quest item, reaching an area, 20 Den kills) because D2RLoader's
`QuestCompleted` event didn't fire on 1.3.1-beta. The log now names what completed each quest
("Normal Den of Evil completed by the quest event / a boss kill / ..."), and every event logs its raw row
("quest event for row 1 on difficulty 0"). Search the plugin log for "quest event" after each step.

- [ ] Fresh character: clear the Den of Evil. Does "quest event for row 1" show up, and before or after
      "completed by Den of Evil kills"? Does it come when the last monster dies or when you talk to Akara?
- [ ] Sisters' Burial Grounds (row 2) and The Search for Cain (row 4): the event's row matches the quest
      (if not, note the rows; the plugin needs a row-to-quest map instead).
- [ ] Debug panel "Complete Quests" on a fresh character: note which rows the events log, if any.
- [ ] Rejoin a character with finished quests: no events for them (they're baselined), and nothing is lost.
- [ ] Nightmare: the event's difficulty is 1.

## 3. Act keys

Defaults (`act_keys` and `enforce_act_keys` on).

- [ ] Walking into Act II without its key warps you back to the Rogue Encampment.
- [ ] With `enforce_act_keys: false`, Act II's checks are recorded but held: the chat says so, the Ledger
      line shows "held until their act's key arrives", and they're sent all at once after
      `/send Snrn Normal Act II Key`.
- [ ] In a multiworld with another D2 slot (default `act_key_window: 1`), the spoiler log has no Normal
      Act II Key in the other D2 world past its Act II, and the Options Creator lists Act Key Window
      under Goal and Length.

## 4. Locks

`cube_lock`, `town_portal_lock`, `potion_lock`, `stash_lock`, `merchant_lock` and `waypoint_lock` all on.

- [x] Blocked with a chat message: drinking a potion, opening the stash (`BankExpansionLayout`), trading
      with a merchant (`VendorPanelLayout`), using an Act I waypoint (`WaypointsPanelExpansion`). You can
      still move afterwards.
- [ ] Opening the Cube is blocked (its panel name hasn't been seen yet; the log lists each panel opened).
- [x] Potions stay locked from the belt hotkeys too.
- [x] D2RLoader doesn't report drinking from the inventory (only the belt), so within 5 seconds potions
      in the inventory are set aside, with a chat message (confirmed 2026-10-12).
- [ ] Moving locked items with modifier clicks (#17). If any of these fails, copy the "activated" lines
      from the plugin log: each lists the input (1 mouse, 2 controller), modifiers (1 Shift, 2 Ctrl, 4 Alt)
      and the mouse buttons held (L, R). A move only passes with L held, so an empty "buttons" means the
      event fires on release. Also report any plain left click or controller pickup that gets logged.
  - [ ] Ctrl+click a locked potion, or a Town Portal scroll before it crumbles, into the stash: it moves,
        with no lock message.
  - [ ] Shift+click a potion to the belt: it moves.
  - [ ] Right-click a locked potion in the inventory, with or without Shift: it's blocked. So is
        Shift+right-clicking a belt potion (feeding the mercenary).
  - [ ] Ctrl+click the Horadric Scroll into the stash: its check doesn't send.
- [ ] D2RLoader 1.3.1-beta logs inventory activations (container 4) for potions now: if a plain right click
      on an inventory potion is logged, the set-aside on pickup could go.
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

- [ ] Switching slots (#66): connect to another player's slot of the same seed by mistake, get an item or
      two, then `ap-connect` to your own slot that has received nothing yet. Nothing from the other slot
      arrives, the Codex and locks reset to your own slot, and the first item `/send`-ed to your slot after
      that arrives (it isn't skipped). Reconnecting to the same slot delivers nothing twice.

- [ ] `Gold` (works), `Experience`, `Random Charm`, `Random Jewel`, `Socketed Base`, `Random Set Item`: arrive,
      scaled to your level.
- [x] `Random Shrine` appears at your feet and works.
- [ ] Traps wait while you're in town and fire when you leave: `Junk Backpack`, `Ambush Trap`,
      `Poison Cloud Trap`, `Firestorm Trap`, `Mana Drain Trap`, `Tax Collector Trap`,
      `Mercenary Down Trap` (with a hired mercenary). Ambush and Poison Cloud work.
- [ ] `Codex Empowerment` raises every Codex skill by one.
- [ ] Trap options (#34), with `ambush_monsters: area`, `ambush_size: 12`, `tax_collector_gold: 250`,
      `junk_backpack_size: 10` and `traps_wait_for_town: false`:
  - [ ] The plugin log has "Levels line-ups at +N and +M" (not "couldn't find the monster line-ups").
  - [ ] An `Ambush Trap` in Cold Plains brings 12 of Cold Plains' own monsters; in Nightmare too.
  - [ ] An `Ambush Trap` in town goes off right away (monsters in town), since waiting is off.
  - [ ] `Tax Collector Trap` takes 250 per level; `Junk Backpack` places at most 10 pieces.
- [ ] A release no longer crashes (#51). Three-slot D2R seed, two games in, one slot offline, then
      `/release <slot>` from the server console. Neither game crashes; rewards trickle in (three loot items
      a second, traps 3 seconds apart), chat shows a few lines at a time and says when the rest went to the
      log, and every reward arrives in the end.

## 5b. Personalized rewards

Uniques and sets found by another player carry their name, like Anya's reward. The item-data offsets are
a guess checked against the item before writing, so test the command first.

- [ ] Hold a junk unique on the cursor and run `ap-personalize` (no name). The console says the layout
      matches; the log has the item data. If it says it doesn't match, send me the log lines.
- [ ] `ap-personalize Bob` on the same item, then Save and Exit and rejoin: it reads "Bob's <item>", and
      the game doesn't crash.
- [ ] Multiworld seed with a second slot: a `Random Unique` that slot finds arrives named after it, and
      chat says "you got <item> from <name>!". Your own finds and `/send` (from the server) stay unnamed.
- [ ] Two-socket base rewards (#61): send `Socketed Base` repeatedly to an early-level character.
      Confirm two-socket normal weapons and armor arrive, can be equipped by the receiving class,
      and accept a matching vanilla or seed-shuffled two-rune runeword in the right order.
      Counts are equally likely from two through the selected base's legal maximum (two-socket
      bases always give two). At higher levels, confirm larger socket counts still arrive and
      no base exceeds its item/type limit (Sorceress orbs cap at three).
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
- [ ] Hints and the Ledger name superunique checks with the game's name and area, e.g. "Normal: Kill
      Bonebreaker (Crypt)"; Bonebreaker's torn page says "A torn page falls from Bonebreaker:" (#26).

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
- [ ] With `superunique_checks` on too, the shuffled superuniques (Rakanishu in Stony Field) are visible,
      with their new monster's model, and their minions are too.
- [ ] `mods\Archipelago\runewords.txt` lists new recipes, and one made from low runes works in game.
- [ ] More monsters, and faster levelling than usual.
- [ ] After rerunning setup with `monster_shuffle: nearby` and again with `anywhere`, Talic, Madawc
      and Korlic stay on Arreat Summit and spawn only when the altar is activated; their defeat completes
      Rite of Passage. No Ancients appear as ordinary monsters in other areas (#74).
- [ ] With either monster shuffle mode, areas with zero monster density on every difficulty keep their
      original line-ups. Swampy Pit 3, Flayer Dungeon 3 and Bloody Foothills still have monsters on
      Nightmare/Hell when their density is positive there (#74).

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

## 10. Experimental repairs with Merchant Lock (#5)

This prototype keeps the stock vendor panel blocked and routes only D2R's native
`VendorPanelMessage:RepairAll` from a separate modal. Whether the native handler
works with its vendor panel closed is **not yet verified**. A successful dispatch
log only means the message was routed, not that repairs happened. It does not
change durability or charge gold itself. There are no native hooks or new NPCs.

Use a disposable character with damaged non-ethereal equipment and enough gold,
in a seed with `merchant_lock: true`, before receiving Merchant Access.

- [ ] Run `ap-repairs on` in the D2RLoader console (or set `repair_only = true`
      in the plugin config and reload). Talk to Charsi and choose Trade/Repair:
      the shop stays closed and an Archipelago Repairs modal appears.
- [ ] Click **Repair All (NPC Prices)**. Inspect durability, broken-item recovery,
      charges, throwing quantities and gold before/after; compare with ordinary
      native repairs. Insufficient gold and ethereal gear retain native rules.
- [ ] Try Akara/Gheed: they must not gain repair abilities or open a shop. The
      prototype cannot identify the NPC through the SDK and relies on native
      validation, so this check is required before enabling it by default.
- [ ] Buying, selling, gambling, Shift/Ctrl-clicks, dragging, controller shortcuts
      and repeated panel toggles never bypass Merchant Lock. Test HD/classic
      graphics and keyboard/controller separately; unsupported routing should
      leave shopping blocked.
- [ ] Double-click Repair All: one request at most. Close/Escape, wait 30 seconds,
      leave town, die, switch game/slot, or run `ap-repairs off`: an old repair
      offer no longer works. Talk to the blacksmith again for another offer.
- [ ] Receive Merchant Access: the ordinary vendor opens normally. Run
      `ap-repairs off` to restore the original behavior; the setting persists.

If no modal appears, record the `blocked vendor panel` / `repair offer failed`
log lines. If gear stays damaged, record `native RepairAll dispatch result` and
the UI messages around the NPC interaction. Keep the feature off until this
closed-panel route is proven; opening the shop as a fallback would defeat the
lock. A loader API for native NPC repair/context would be the next approach.
