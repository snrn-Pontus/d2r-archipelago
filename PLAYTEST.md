# Playtest checklist

The parts that still need checking in game, grouped so each session tests one area. Tick a box once it
works; if it doesn't, note what happened and attach `d2rloader\logs\` (the plugin logs every check, reward,
console command and refused item).

**Before every session:** after changing the plugin or the seed, rerun `d2r-ap-setup.exe` so the mod
matches, then start a new Lord of Destruction character. Most sessions test faster with items handed out
from the server console: `/send <slot> <item name>` (for example `/send Snrn Act I Waypoints`).

## 0. Codex charms

- [ ] Your class's skills are on one Codex and other classes' skills on a second one. Below level 6 the
      second one's skills don't work, but your class's still do.
- [ ] Codex skill levels: copies + Empowerments + 1 per 5 character levels, the last part stopping at +10.

## 1. Tracking: Ledger and ap-missing

Any seed with the defaults.

- [ ] A new character gets an **Archipelago Ledger** (Token of Absolution picture) next to the Codex, and the
      chat says to hover over it.
- [ ] Its tooltip shows the total (`Archipelago: n/m checks`), one line per act in the seed with
      `done/total`, `> ` before the act you're in, and a "Not tied to an act" line.
- [ ] Entering Cold Plains raises Normal Act I's count by one; the tooltip updates without reopening anything.
- [ ] Stashing the Ledger doesn't make a second one. Dropping it and selling it doesn't make a new one
      either; `ap-ledger` does.
- [ ] `ap-missing` lists the checks left in your act by name; `ap-missing all` gives a count per act.
- [ ] With `ap-disconnect`, the tooltip says "not connected" and `ap-missing` says it needs the server.

## 2. New checks

Defaults, plus `shop_checks: 3`, `kill_count_checks: true`, `find_count_checks: true`.

- [ ] Killing **Bishibosh** (Cold Plains) and **Rakanishu** (Stony Field) each sends a check. A plain
      monster of the same type doesn't.
- [ ] Picking up the **Scroll of Inifuss** and **Wirt's Leg** each sends a check, once.
- [ ] Buying an **Archipelago Parcel** from Akara (2,500 gold) sends a check and the parcel disappears. The
      fourth one is refunded.
- [ ] 100 kills in Act I sends "Kill 100 monsters"; finding your first unique sends "Find 1 Unique".
- [ ] Rewards don't count: a Random Unique from the server doesn't count toward "Find 1 Unique".

## 3. Act keys

Defaults (`act_keys` and `enforce_act_keys` on).

- [ ] Walking into Act II without its key warps you back to the Rogue Encampment.
- [ ] With `enforce_act_keys: false`, Act II's checks are recorded but held: the chat says so, the Ledger
      line shows "held until their act's key arrives", and they're sent all at once after
      `/send Snrn Normal Act II Key`.

## 4. Locks

`cube_lock`, `town_portal_lock`, `potion_lock`, `stash_lock`, `merchant_lock` and `waypoint_lock` all on.

- [ ] Each is blocked with a chat message: opening the Cube, reading a Town Portal scroll or tome, drinking a
      potion, opening the stash, trading with a merchant, using an Act I waypoint.
- [ ] Each opens after its item: `Horadric Cube Access`, `Town Portal Access`, `Potion Access`,
      `Stash Access`, `Merchant Access`, `Act I Waypoints`.
- [ ] Shop parcels can't be bought before Merchant Access.
- [ ] None of the blocks freezes the game or leaves a panel half-open.

## 5. Rewards and traps

Any seed. Send each with `/send`, standing outside town for the traps.

- [ ] `Gold`, `Experience`, `Random Charm`, `Random Jewel`, `Socketed Base`, `Random Set Item`: arrive,
      scaled to your level.
- [ ] `Random Shrine` appears at your feet and works.
- [ ] Traps wait while you're in town and fire when you leave: `Junk Backpack`, `Ambush Trap`,
      `Poison Cloud Trap`, `Firestorm Trap`, `Mana Drain Trap`, `Tax Collector Trap`,
      `Mercenary Down Trap` (with a hired mercenary).
- [ ] `Codex Empowerment` raises every Codex skill by one.

## 6. Hints

`page_hint_chance: 100`, `prophecy_shop: true`, `cain_hints: true`.

- [ ] The first Bishibosh kill drops a torn page hint; the second kill doesn't.
- [ ] A Scroll of Prophecy from Akara (1,000 gold) gives a hint; a cheaper tier than the next one is refunded.
- [ ] After Andariel, Deckard Cain says "Stay awhile and listen..." and gives a hint.
- [ ] `ap-hints` lists them; the hints show on the server (`!hint` in the text client) without costing
      hint points.
- [ ] `/send Snrn Prophecy` gives a hint too.

## 7. DeathLink

Add a second slot to the multiworld, `Players\Probe.yaml`: a copy of `Snrn.yaml` with `name: Probe`. Turn
on `death_link: true` in both.

- [ ] Dying sends a death: the server log shows a DeathLink from Snrn.
- [ ] `ap_probe localhost:38281 Probe --death` kills your character, with the cause in chat.
- [ ] The death that DeathLink caused isn't sent back out.
- [ ] Dying once sends one death, not one per frame or per resurrection.

## 8. Shuffles

`monster_shuffle: anywhere`, `superunique_shuffle: anywhere`, `runeword_shuffle: true`,
`monster_density: 150`, `experience_multiplier: 200`.

- [ ] Blood Moor and Cold Plains have monsters from other acts, at a level that fits (not Hell-strength in
      Normal).
- [ ] Bishibosh is a different monster but still sends his check.
- [ ] `mods\Archipelago\runewords.txt` lists new recipes, and one made from low runes works in game.
- [ ] More monsters, and faster levelling than usual.

## 9. Later: long runs

These need a character that has beaten Baal, so test them during a real run.

- [ ] Secret Cow Level (on difficulties before the goal): entering it, The Cow King, and 100 cows each send
      a check.
- [ ] Quest checks that only use fallbacks: Radament (Book of Skill), the Golden Bird, Lam Esen's Tome,
      rescuing Anya, Prison of Ice, Siege on Harrogath.
- [ ] Hell goal: Pandemonium keys, organs, essences, the Torch and Annihilus; the uber kills.
- [ ] The goal: killing the goal boss sends it, and only once its act's key is in.
