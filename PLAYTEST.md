# Playtest checklist

The parts that still need checking in game, grouped so each session tests one area. Tick a box once it
works; if it doesn't, note what happened and the mark number (below), and attach `d2rloader\archipelago\logs\`
(the plugin's own log, one file per launch, the last 10 kept, so a crash's log survives the relaunch). It has
every check found and sent, every item received, what the chat showed, area and level changes, connection
changes, console commands and refused items. `mods\Archipelago\d2rloader\logs\` and `d2rloader\logs\` (wiped on
every launch) have D2RLoader's side.

**Logs for a bug report:** press **Mark the Log** on the debug panel (or `ap-debug mark <what you're testing>`)
before each test, and say "it broke after mark 3". **Log State** (`ap-debug state`) writes what the plugin
knows (connection, seed options, save, locks, queued rewards) to the log; press it when something looks wrong.

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
- **Testing:** **Starter Kit** for a fresh character: a Horadric Cube, a Token of Absolution, the four essences
  (cube them into a second Token), a Tome of Identify and 3 Full Rejuvenation Potions. They're made as rewards,
  so none sends a check (a Token cubed from the essences does, like a real one). Then **Mark the Log** and
  **Log State** (above), and set-ups for the harder scenarios:
  - **Kills to Next Check** sets the save's kill count for the act you're in one short of its next check, so
    the next kill sends it (`ap-debug kills`); **Cows to Next Check** does the same for the cow check
    (`ap-debug cows`).
  - **Save 260 Kills** saves 260 kills for the act you're in without their checks, as a crash right after the
    kill leaves it (`ap-debug kills <n>`). **Reconnect** then recovers them; **Disconnect** is there too.
  - **Saves Fail** makes every save fail as if the file were read-only, until **Saves Work**
    (`ap-debug savefail on|off`; it resets when the game restarts).
  - **4 Healing Potions** puts potions in the inventory as rewards, for the Potion lock to set aside.
  - **Crash D2R (Twice)** ends D2R at once, like Task Manager, for the crash tests: press it, open the panel
    again and press it within 10 seconds (`ap-debug crash`, twice).

`ap-debug give <code> [count] [sockets]` makes any item as a reward (no check), e.g. `ap-debug give r08 2` for
two Ral runes or `ap-debug give lsd 1 3` for a 3-socket Long Sword to try a shuffled runeword.
`ap-debug spawn <code>` makes it as if found, so it sends its check.

## Next up (from the 2026-10-12 session)

Built, pushed and installed, still to check in game:

- [ ] S1 skills (#117, new seed needed): a Barbarian or Amazon seed never gets Smite, a Paladin never gets
      Dodge, Avoid or Evade, and a Barbarian no longer gets Assassin traps (its S2 animation only exists with a
      one-handed thrusting weapon). A Sorceress, Necromancer, Druid or Assassin with Smite and a shield can
      hit with it; a non-Amazon with Dodge, Avoid or Evade doesn't freeze when it triggers. Engine-bound
      skills stay with their class in pools and shuffled trees (Whirlwind, Leap, Frenzy, Zeal, Strafe, Jab,
      Fend, Inferno, Arctic Blast, the kicks and martial arts...), and a Necromancer gets no Charge, Impale or
      Blade Fury. Still to see: Impale and Blade Fury on another non-Necromancer class (only Charge's
      Necromancer failure was tested elsewhere); Lightning and Chain Lightning off the Sorceress fire without
      a cast animation (known, #119).
- [ ] No more Archipelago Ledger (#54, rerun setup): a new character gets no Ledger. F9's Maintenance column
      has no New Ledger button and `ap-ledger` is an unknown command. The status line still shows the total,
      the act you're in and "n held", and **Checks Left by Act** (`ap-missing all`) still counts every act,
      with an "Any act" line.
- [ ] Archipelago Emblem (#54, rerun setup), tree mode: a new character gets one after its first kill or
      **Claim Character** / `ap-claim`, with a chat line naming its class. It shows the Token of Absolution
      picture in one cell (LoD too), and its tooltip says "marks this character as the seed's <class>" (or
      "tells the plugin this character's class" if the tooltip's copy has no seed; note which). Save and Exit, join again: rewards and locks work at once, no kill or
      claim needed. Put it in the shared stash: it doesn't count, and a new one arrives; drop or sell it: a
      new one arrives too. Codex mode: no Emblem, and a rejoin still works at once from the Codex.
- [x] Class without a kill: a new character in town gets a banner and chat line saying rewards and locks wait
      (kill a monster or ap-claim). **Claim Character** (F9) or `ap-claim`: the skills unlock and the locks
      apply (Town Portal scroll crumbled) (2026-10-13). (Belt potions aren't set aside by design: only
      inventory potions are, and the belt hotkeys are blocked instead.)
- [x] With Merchant Access locked, an Identify scroll works outside a shop; buying stays blocked (2026-10-13).
- [ ] Reward runes after a rejoin (#108), seed with `rune_checks` on: `ap-debug give r08` (or `/send` a Low
      Rune reward), "Find Ral Rune" isn't sent. Save and Exit, join again: still not sent, and the seed's save
      has a `rewardcopies` line. Then a Ral you find (or `ap-debug spawn r08`) sends it. Same with a
      reward gem.
- [ ] Other players' sends (#49): first `ap-chat grey test` shows "test" in grey (not "ÿc5test"). In a
      multiworld with other games, another player's progression item for a third player shows in the chat as
      "Alice sent Hookshot to Bob (Ice Palace)" in grey, no banner, and the session log has an "other players:"
      line for it; their filler doesn't show. `other_sends = "all"` in archipelago.toml (relaunch) shows filler
      too; `"mine"` hides them all. A `/release` of another slot (or `!collect`) shows a few grey lines, then
      "N more items sent between other players are in the D2RLoader log", and your own lines still come first.

- [x] Debug tools: on a new character, **Starter Kit** puts its 10 items in the inventory (the chat lists them)
      and `ap-missing` shows no new checks; with the Cube lock on, the Cube still arrives (2026-10-13).
- [x] With Horadric Cube Access, cubing the four essences makes a Token (2026-10-13; `special_item_checks` was
      off, so no check). `ap-debug give lsd 1 2` made a 2-socket Long Sword.
- [x] Session log: `d2rloader\archipelago\logs\` has a `session-<date>_<time>.log` for this launch that can be
      opened while the game runs. It starts with the plugin version and config, has "seed options:" after
      connecting, "found ... checks", "sending ... checks", "received #..." and "shown: ..." lines as you play,
      and "entered <area>" on each area change. **Mark the Log** twice adds "mark 1" and "mark 2"; **Log
      State** adds the "state:" lines (2026-10-13; town logs as "entered area 1", without its name).
- [ ] Still open: quit with the menu's Exit and relaunch: the new file doesn't say the
      last session "has no normal exit". End D2R in Task Manager (or **Crash D2R** twice) and relaunch: it does. (Does Alt+F4 count as
      a normal exit? Note which.)
- [ ] With the server down, the session log says "Could not connect" once, not every 5 seconds. While it
      retries, `ap-connect` to another server: the log shows "Connecting to" the new one right away.

- [x] The **Archipelago Parcel** is in Akara's Misc tab now (shop item type fix) and buying it sends a check
      (section 2) (2026-10-13: "parcel delivered (Act I shop, 1 of 5)", then 2 of 5).
- [x] Buying a Scroll of Prophecy tier you've already used refunds the gold (section 6) (2026-10-13: "you
      already know that prophecy; the next is the 2500-gold scroll. Your gold is refunded.").
- [ ] **Potion Access** gives back the potions set aside from the inventory (section 4).
- [x] All eight 2x2 Scrolls of Prophecy fit in Akara's Misc tab next to her usual stock (2026-10-13).
- [ ] Parcels and scrolls are no longer quest items: check whether a vendor buys them back, and that a
      bought one doesn't stay in the inventory.
- [ ] Class-usable skills (PR #18, new seed needed): a Sorceress pool and tree hold no claw skills, no
      Assassin traps (Fire Blast froze a Sorceress), no Frenzy or Double Swing, and no wolf or bear attacks.
      A Druid only gets Feral Rage, Maul and the like alongside Werewolf or Werebear. (2026-10-13, two
      Sorceress seeds' spoilers: none of those in the pool; the other classes' skills were Cobra Strike, Blade
      Shield, Shadow Warrior, Fade, Cyclone Armor, Molten Boulder, Spirit of Barbs and Barbarian masteries and
      warcries. Still to check in game: the tree and casting them.)
- [ ] Werewolf or Werebear on another class: does the form work? If not, keep forms Druid-only too.
- [x] Tree mode unlocks received skills in the Skills table (no free level): a new character spends its own
      points on them, and a Token respec refunds only those points, with the skills still open (2026-10-13).
- [ ] The same after Akara's respec (Den of Evil) and a Writ of Absolution; `ap-relearn` says there's
      nothing to relearn, and finishing Den of Evil shows no Relearn hint.
- [x] Writ of Absolution (#6, rerun setup): Akara's Misc tab has one with the Token of Absolution's picture,
      and it works (2026-10-13). Its name shows as "An Evil Force" (#113). Still to see: the other acts'
      potion sellers.
- [x] Shuffled superuniques are visible now (they had no HD model): Rakanishu in Stony Field showed as a Yeti
      and his kill sent the check (2026-10-13).
- [x] Main-path superunique cap (#3, new seed needed, `superunique_shuffle: nearby`, default
      `main_path_superunique_cap: 2`): setup prints the superuniques that changed shape. On Normal, Griswold
      in Tristram and Rakanishu in Stony Field are no tougher than their usual selves (no level-15 mummy for
      Griswold); the stronger swaps show up in the Crypt, the Cave or the Stony Tomb. With the cap at 50,
      Griswold can be strong again. 2026-10-13 seed (cap 2), from the mod's files: Bishibosh is a Dark
      Ranger (level 2), Rakanishu a Brute (level 5), Griswold kept his shape (level 5) and seemed normal in game;
      the strong swaps are off the main path (Pitspawn Fouldog a level-11 Sand Maggot Queen in the Jail, Bone
      Ash a level-14 Burning Dead Mage in the Cathedral). Rakanishu showed as a Yeti in game, and seemed fine
      on the cap-50, strength-50 Sorceress seed too (2026-10-13); the rest is fine without more testing.
- [x] Shuffle tuning (#38, new seed needed): with defaults, a reshaped Rakanishu brings 5 minions, not 8;
      Bishibosh, if he kept his Fallen Shaman, still brings his 2. `superunique_minions: 50` halves every field
      superunique's pack and setup says how many got a new count. `monster_density_hell: 300` makes Hell
      crowded while Normal stays as set, and `experience_multiplier_nightmare` changes only Nightmare's
      experience (compare a kill's XP in the Blood Moor on Normal and Nightmare). `better_magic: 300` gives
      noticeably more blue items. `shuffle_cow_level: true` with `monster_shuffle: anywhere`: the Cow Level has
      other monsters, the Cow King is still there, and the cows turn up in some other area, where killing 100
      still sends the cow check. `runeword_rune_spread: 0` with `runeword_shuffle` leaves `runewords.txt`
      showing the usual recipes. (2026-10-13, defaults: `superuniques.txt` gives Rakanishu 5 minions and
      Bishibosh 2. Sorceress seed with `superunique_minions: 50` and `runeword_rune_spread: 0`: Rakanishu 4,
      Bishibosh 1, Corpsefire 3, and `runewords.txt` lists the usual recipes, e.g. Black: Thul Io Nef. The
      density, experience, magic and cow parts are fine without more testing.)
- [x] Main-path superunique level and strength (#93, new seed needed): with defaults and
      `superunique_shuffle: nearby`, a reshaped Griswold or Rakanishu shows his usual level on Normal (hover
      him: the tooltip has no level, so compare how fast he dies at a known character level, or check
      `monstats.txt` in the mod for `ap_superunique_5`'s Level). `main_path_superunique_strength: 50` makes
      Bishibosh and Rakanishu die in about half the hits and hit softer, on Normal and Nightmare, while their
      minions and Bonebreak in the Crypt stay as usual. With `superunique_checks: false` and strength at 50,
      setup still says the superuniques have their own monster, they show their HD models, and no checks are sent.
      (2026-10-13: Rakanishu at strength 50 seemed fine; the rest is fine without more testing.)
- [x] Unspawnable superuniques dropped (issue #28): the seed has 31 superunique checks per difficulty, none
      without an area. Frigid Highlands has Eldritch, Sharptooth Slayer and Eyeback but no Axe Dweller, Anodized
      Elite, Vinvear Molech, Magma Torquer or Blaze Ripper (2026-10-13, from the spoiler; `ap-missing` listed
      Act I's with areas).
- [ ] With `superunique_checks` and a mod from the old setup tool, the plugin says the mod doesn't match;
      after rerunning setup, killing Bishibosh and Corpsefire sends their own checks (not a neighbour's).
- [ ] Options editor (#13, needs the apworld in the release Archipelago): the Launcher shows **Diablo II
      Resurrected Options**; it opens a window (no error in the Launcher log). **Open...** a copy of
      `Players\Snrn.yaml`, change Goal Difficulty and Shop Checks, **Save**: only those two lines change and
      the comments stay. A preset, then **Save as...** a new name, generates a seed.
- [x] D2RLoader settings from the YAML (#9): with `show_ground_sockets: on`, `display_item_levels: on`,
      `shared_stash_tabs: 10` and `materials_limit: 150`, setup lists them and writes
      `mods\Archipelago\d2rloader\config\d2rloader.toml`. In game, items on the ground show sockets and item
      levels, and the shared stash has 10 extra tabs (Page 1 / 13) (2026-10-13).
- [ ] Still open from #9: runes stack past 99. Can't be tested on a Lord of Destruction character: its runes
      don't stack anywhere (inventory, personal or shared stash) and Options has no advanced stash setting
      (2026-10-13). Needs a Reign of the Warlock character, or skip it. D2RLoader's Options show the mod's values winning. Then a seed with everything at
      `keep`: your own D2RLoader settings are back. Watch the loader log for a complaint about the mod file.
- [ ] Character switch (#72, `level_checks` on): play a high-level character (any seed), Save and Exit,
      then connect a fresh seed and start a level 1 character. No level checks are sent (`ap-missing` still
      lists them all), the Codex is built for the new class, and loot isn't high-level. With
      `traps_wait_for_town`, a trap sent right as the game loads in town waits until you leave town.
      Level 6 from the debug panel then sends only the level-5 check.
- [ ] `ap-disconnect` (#69): `ap-status` and the status line say "Disconnected" right away, not "Connected as".
      Find a check while disconnected, then `ap-connect`: the check is sent on reconnect. Also `ap-connect`
      to a server that hangs (e.g. a wrong host) and `ap-disconnect` straight after: no session starts later.
- [ ] Hints found while disconnected (#71, section 6 options): `ap-disconnect`, then get a hint (Scroll of
      Prophecy, or a torn page). `ap-hints` lists it, but `!hint` in the text client doesn't. `ap-connect`:
      the hint now shows in `!hint` (and the other player sees theirs), the plugin log says how many hints were
      sent again, and reconnecting a second time doesn't repeat the hint messages in the text client.
- [ ] Goal before connecting (#71): start the game with the server down (so the plugin never connects this
      launch), finish the goal quest, then start the server and connect. The goal is sent ("goal complete!") and the room shows
      the slot as finished; with `act_keys` and the goal act still locked, the chat says it counts once you
      hold the key, and it's sent when the key arrives. A goal-quest kill of the wrong difficulty sends nothing.
- [ ] Client hardening (#75): `ap-connect` to another seed while in game: the log shows no "bound to seed"
      line for the old seed after the command, only the new one once it connects; kill counts and rewards
      land in the new seed's save file (`d2rloader\archipelago\<seed>_<slot>.txt`), the old file is untouched.
      `ap-disconnect` and `ap-connect` back and forth a few times: no hang or crash, and the plugin log has
      no "bad packet" lines. Exiting the game while connected still closes promptly. Freeze the server
      (suspend its process in Resource Monitor) and `ap-connect` to another one: within about 3 seconds the
      new connection starts.
- [ ] Save failures (#75): press **Saves Fail** on the debug panel and `/send` a few rewards: the chat says
      once that progress couldn't be saved and rewards are paused, and none arrive. **Saves Work**: within a
      few seconds it says saving works again and the rewards arrive, once each. Then once the real way: make
      the seed's save file read-only (Properties, Read-only), and clear it again.
- [ ] Data package (#75): `ap-reconnect` a few times: item and location names in chat still resolve, and
      the server log shows a GetDataPackage only on the first connect (or none once names are held).
- [x] Runeword shuffle (#75, rerun setup): `runewords.txt` in the mod has no two complete runewords with the
      same rune sequence (2026-10-13: none in either bank's `runes.txt`); a shuffled runeword with a common
      base still makes the right word (El Eld in a normal 2-socket Long Sword made Steel on a LoD character,
      from the base bank's recipe; the list itself is #110).
- [x] Writ name (#113, rerun setup): the Writ of Absolution in a potion seller's Misc tab is named "Writ of
      Absolution", not "An Evil Force" (2026-10-13).
- [x] Nothing drops on the ground (#109; one paced ground drop still crashed): with full bags, Random Uniques
      no longer crash (2026-10-13). Rewards that only lack room now wait ("rewards are waiting; make room ...
      and they arrive within 30 seconds", retried every 30 seconds) instead of rerolling into smaller uniques or
      a mid rune; that last change wasn't replayed in game.
- [ ] With potions and Town Portal locked, set potions aside (`ap-debug give hp1 4`), fill the inventory, then
      `!getitem` Town Portal Access and Potion Access together: the chat asks for room for the Tome and says
      the potions come back as you make room; freeing cells brings them, ending with "the last of your
      set-aside potions is back" (#109).
- [x] Runeword banks agree (#110, rerun setup; 2026-10-13: both banks say Steel is Tir Tir, the five words are
      marked, and Tir Tir made Steel on a LoD character): `excel\runes.txt` and `excel\base\runes.txt` in the mod give
      every runeword both have the same runes, and `runewords.txt` marks the five Reign of the Warlock words
      (Authority, Coven, Ritual, Vigilance, Void) "(Reign of the Warlock only)". On a Lord of Destruction
      character, `runewords.txt`'s recipe for Steel (or another cheap word) in a normal sword of the right
      socket count makes the runeword.

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

`skill_tree_shuffle: true` (which turns on tree mode).

- [ ] With `skill_mode: codex` and `skill_tree_shuffle: true`, the seed plays in tree mode: no Codex, and the
      shuffled tree's skills unlock as they arrive.
- [ ] In the options editor, ticking Skill Tree Shuffle switches Skill Mode to Tree; picking Codex unticks it.

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

## 1. Tracking: status line and ap-missing

Any seed with the defaults.

- [ ] Entering Cold Plains raises the status line's Normal Act I count by one.
- [ ] `ap-missing` lists the checks left in your act by name; `ap-missing all` gives a count per act, and an
      "Any act" line for the ones not tied to an act.
- [ ] With `ap-disconnect`, the status line says "not connected" and `ap-missing` says it needs the server.

## 1b. Command panel

Any seed, connected.

Restart D2R after updating the plugin: the game caches panel layouts.

- [ ] F9 opens the Archipelago panel with three headed columns: This Game (5 buttons), Server
      (4 buttons), Maintenance (4 buttons), plus Close. Title, help and every button fit on screen
      at the usual resolution and UI scale. F9 again, Escape and Close each close it.
- [ ] `ap-panel` opens and closes it too; Options, Controls lists "Archipelago Commands".
- [ ] Status, Checks Left Here, Checks Left by Act and Hints Found close the panel and answer in chat.
      Claim Character retains its existing character-binding flow. Server Hints, Items Left and Multiworld Status show the server's reply. Rebuild
      Codex works as before; Reconnect restores the saved connection.
- [ ] Collect, Disconnect and Relearn Skills first show a description and action button with **Cancel
      focused by default**. Cancel or Escape changes nothing. Reopen and explicitly confirm: Collect
      asks the server to collect the slot (use a disposable seed), Disconnect drops the connection,
      Relearn restores missing AP tree skills after a respec without needing to type a command.
- [ ] Double-clicking Confirm runs the action once. Confirm Relearn then immediately switch AP slots:
      deferred work reports cancellation and adds no old-slot skills. Also switch after the first
      Relearn batch: no remaining old-slot skill commands run in subsequent batches, and one cancellation
      message says the remaining commands were discarded. Opening F9 during a dialog closes that dialog and
      returns to the main panel. Cancel then opening another action never runs the cancelled action.
- [ ] Start reconnecting to another AP slot while a confirmation is open: the old confirmation refuses
      to run even before the handshake finishes (also test a refused/unreachable server).
      Relearn cannot open while connecting/refused, before the slot is bound, or on another character.
      Leaving the D2R game also cancels a pending dialog, including when joining another character.
- [ ] Clicks on the panel don't move the character or hit anything behind it.
- [ ] Controller: all three columns, Close, Confirm and Cancel can be reached and pressed; entering
      a confirmation selects Cancel. A controller-only shortcut remains a follow-up (F9/`ap-panel`
      still opens it). Live panel text and inline answer history remain for the related UI stories;
      the existing status overlay and Codex history still provide live data.

## 1c. Debug panel

Any seed, connected. Several of these D2RCore cheats haven't been tried yet; note any that do nothing
(the log has each command).

- [ ] F10 does nothing until `ap-debug on`; then it opens "Archipelago Debug" with five headed columns of
      twelve buttons and Close, all on screen. `ap-debug off` closes it and F10 stops working.
- [ ] Go To: Cold Plains and Tristram from Rogue Encampment. Lut Gholein without the Act II key warps you
      back (that's section 3's test); with the keys from From the Server, Kurast Docks and Harrogath work.
- [ ] God Mode, One-Hit Kills (`easykill`), Level 6 and Level 30 (`level <n>`) do what they say.
- [ ] Spawn Andariel / Spawn Radament: killing the spawn sends their quest check.
- [ ] Items: each lands in the inventory and its check is sent; the unique ring counts for "Find 1 Unique".
- [ ] From the Server: the chat shows "Cheat console: sending ..." and the item arrives.
- [ ] Testing, with `kill_count_checks`: **Kills to Next Check** in Cold Plains after one kill says "Normal Act I
      kills set to 99", and one more kill sends "Kill 100 monsters"; pressed again it says 249. Without
      `kill_count_checks`, before connecting, after Disconnect or before the game's first kill, it says why
      and changes nothing. **Cows to Next Check** works the same with
      `cow_level_checks` in the Cow Level.
- [ ] **Saves Fail** and **Saves Work** each answer in the chat; **Log State** shows "(ap-debug savefail on)"
      on the save line while it's on.
- [ ] **Crash D2R (Twice)**: one press only asks for a second; a second press more than 10 seconds later asks
      again; within 10 seconds D2R closes, and the log ends with "ap-debug crash: ending D2R now."

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
- [ ] Scroll of Inifuss after a rejoin (#123), on Normal: pick up the scroll and Save & Exit within 5 seconds,
      rejoin and hand it to Akara before killing anything. Nothing is sent in town; the first kill after
      that sends Find Scroll of Inifuss (the debug panel's queues line counts it as 1 quest item meanwhile).
- [ ] Holding the deciphered scroll (Key to the Cairn Stones, `ap-debug spawn bkd`) on a difficulty
      whose scroll check isn't sent yet sends it. Entering Tristram sends it too, alongside The Search
      for Cain.
- [ ] Buying an **Archipelago Parcel** from Akara (2,500 gold) sends a check and the parcel disappears. The
      fourth one is refunded.
- [ ] 100 kills in Act I sends "Kill 100 monsters" (**Kills to Next Check** on the debug panel, then one
      kill); finding your first unique sends "Find 1 Unique".
- [ ] Kill-count recovery (#78), the state a crash right after the 100th kill leaves: before any Normal
      Act I kill check, press **Save 260 Kills** in Act I, then **Reconnect** (or quit D2R and edit the
      seed's save in `<D2R>\d2rloader\archipelago\` so it reads `kills_0 260`, `kills_<difficulty * 10 + act>`,
      and start again). The log says "2 kill or cow count checks recovered from the save" and "Kill 100
      monsters" and "Kill 250 monsters" go out with no extra kill. Another reconnect or another kill doesn't
      send or banner them again. Try the same with `kills_1` (edit the save, or with `enforce_act_keys: false`
      press the button in Act II) on a seed where Act II is still locked: the checks show "(held)" and go out
      with the Act II Key.
- [ ] Rewards don't count: a Random Unique from the server doesn't count toward "Find 1 Unique".
- [ ] Side areas (#31): `ap-missing` lists five "Normal: Enter <side area>" checks. Walking
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

## 2d. Difficulty and class before a kill (issue #126)

The plugin only learns the difficulty, and usually the class, from a kill; until then area checks, quest
items and rewards wait. These checks find out whether something earlier could tell it. With debug tools on
(`ap-debug on`, then restart D2R), the plugin also listens to D2RLoader's encounter requests and logs them
("encounter boss request ... difficulty N", "encounter herald request"); the start of the log says whether
that registered ("encounter logging provider: result 0").

- [ ] **Area events on Nightmare/Hell:** enter a few areas on Nightmare (and Hell, if you have a character
      there) before killing anything. What does "entered ... (event difficulty N)" say: 1 / 2, or still 0?
- [ ] **Encounters:** with debug tools on, walk out of town on a new game without killing. Is there an
      "encounter ... request" line, and does it end "before the difficulty was known"? When does it come:
      entering an area, when monsters spawn, near a boss or superunique, only in a Terror Zone? On
      Nightmare, does the boss request say difficulty 1?
- [ ] **Class:** `ap-debug probe` logs whether each class's first skill could take a point
      ("can allocate: Amazon Magic Arrow (allowed), Sorceress Fire Bolt (refused), ..."). Try it right after
      joining, with unspent skill points (debug panel **+5 Skill Points**) and with none, on a seed with Skill
      Tree Shuffle (tree mode) and without. Is only your own class's skill allowed? (A Warlock isn't asked,
      so it shows every other class refused.)

## 3. Act keys

Defaults (`act_keys` and `enforce_act_keys` on).

- [ ] Walking into Act II without its key warps you back to the Rogue Encampment.
- [ ] With `enforce_act_keys: false`, Act II's checks are recorded but held: the chat says so, the status
      line shows "n held", and they're sent all at once after
      `/send Snrn Normal Act II Key`.
- [ ] In a multiworld with another D2 slot (default `act_key_window: 1`), the spoiler log has no Normal
      Act II Key in the other D2 world past its Act II, and the Options Creator lists Act Key Window
      under Goal and Length.

## 4. Locks

`cube_lock`, `town_portal_lock`, `potion_lock`, `stash_lock`, `merchant_lock` and `waypoint_lock` all on.

- [x] Blocked with a chat message: drinking a potion, opening the stash (`BankExpansionLayout`), trading
      with a merchant (`VendorPanelLayout`), using an Act I waypoint (`WaypointsPanelExpansion`). You can
      still move afterwards.
- [x] Opening the Cube is blocked (right-click in the inventory; 2026-10-13).
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
- [ ] Potion Access gives the set-aside potions back (as room frees up if the inventory is full). **4 Healing
      Potions** on the debug panel gives some to set aside.
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
- [ ] Rewards only go to the seed's character (#70). With a fixed-class seed (say Sorceress), load a
      Barbarian mule while connected and `/send` a `Gold`: nothing arrives, no Codex appears, and
      after the Barbarian's first kill the chat says it isn't this seed's Sorceress and mentions `ap-claim`.
      From then on the seed's locks (stash, cube, merchants, waypoints, potions, act keys) don't hold it.
      Leave, load the Sorceress: if she carries a Codex, the gold arrives right away; otherwise after her
      first kill (the chat asks for one). `ap-claim` on the Barbarian (after a kill) moves the seed to it.
- [ ] With `character_class: any`, the first character that kills something (or carries a Codex) claims the
      seed; the log says "this seed's character is a ...". A character of another class then gets nothing.
- [ ] Reward commands survive a restart (#70): `/send` a few `Gold` and `Experience` items and close D2R from
      Task Manager within a second. After relaunching and loading the character, the ones that hadn't run yet
      arrive. The seed's save file (`d2rloader\archipelago\<seed>_<slot>.txt` in the game folder) lists waiting ones as `command` lines,
      and they're gone once they ran.
- [ ] Crash recovery (#70): get a few rewards, then kill D2R from Task Manager (or **Crash D2R** twice)
      without leaving the game. On
      the next connect the chat says D2R closed during the last game and offers `ap-redeliver`. If the
      character lost the rewards, `ap-redeliver` hands the loot, gold and experience out again (no traps or
      prophecies), and the chat says when they're all back. After a normal Save and Exit, no such message.
      (2026-10-13: after the #109 crash the chat offered `ap-redeliver` as described; `ap-redeliver` itself not
      tried yet. The 8 potions Potion Access gave back right before that crash were lost: held potions aren't
      part of the redelivery.)

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

- **Bug (2026-10-13, #108):** a rune sent as a reward counts as found after rejoining. A Mid Rune reward (Amn)
  arrived at 13:50; on the next join the plugin sent "Find Amn Rune". The reward marker lives in the item
  seed, which runes and other simple items don't keep in the save, so `IsReward` misses them after a reload.
  Gems and other simple items likely do the same.
- **Crash (2026-10-13, 14:09:32, #109):** getting all the lock items at once with a full inventory (the debug Long
  Swords had already gone to the stash) crashed D2R right after "Potion Access! Your 8 set-aside potions are
  back.". The loader log has `BC_ASSERT: hRoom` and `DungeonTestRoomGame(hRoom, nX, nY)` (UnitRoom.cpp), then
  an access violation (crash report and `CrashDumps\D2RLoader.exe.39564.dmp`). Likely the ground fallback:
  the Tome of Town Portal and 8 potions created on the ground in one go.
- **Bug (2026-10-13, #110):** `runewords.txt` lists the Reign of the Warlock bank's recipes, but the two banks are
  shuffled separately (same random stream, different rows), so 91 of the 94 complete runewords differ for a
  Lord of Destruction character. Steel is Tir Tir in the list and in `excel/runes.txt`, but El Eld in
  `excel/base/runes.txt`; Tir Tir in a 2-socket Long Sword stayed a plain 'TirTir' sword. Neither bank has
  duplicate recipes.

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
- [ ] Hints and `ap-missing` name superunique checks with the game's name and area, e.g. "Normal: Kill
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
- [ ] `mods\Archipelago\runewords.txt` lists new recipes, and one made from low runes works in game. (The
      shuffled recipe works: Steel from El Eld, 2026-10-13. The list shows the other bank's recipes until #110.)
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
- [ ] Cow recovery (#78): with D2R closed, set `cows_<difficulty> 100` in the seed's save (the 100-cow check
      not yet sent). Start again and connect: the 100-cow check goes out once, without another cow. Disconnect
      from the server before the 100th cow and reconnect after it: the check also goes out, once.
- [ ] Lifetime cows (#79): the 100 cows add up per difficulty over the seed. Kill about 60 cows, leave the
      game (the save's `cows_<difficulty>` shows them), make a new game and kill 40 more: the check goes out at
      the 100th. Likewise 60, `ap-connect` again to the same slot in the same game, then 40. Cows on Normal
      don't count toward Nightmare's. With `ap-connect` to the slot only after killing some cows, they join
      the save's count once it binds. With `ambush_monsters: area`, an Ambush trap in the Cow Level brings cows
      that count (they're cows in the Cow Level); elsewhere no cows show up. With
      the 100-cow check held behind the Act V key, it waits and goes out once the key arrives.
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
