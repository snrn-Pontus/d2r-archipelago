"""Generate the shared game tables for the apworld and the plugin.

Reads skills, levels, uniqueitems, setitems, weapons, armor, itemtypes, misc,
monstats, superuniques and experience from an
extracted D2R 3.x excel folder (`d2r-ap-setup --extract <folder>` writes one)
and writes apworld/d2r_archipelago/data.py and plugin/src/game_data.h. Only
ids, names, codes and level requirements are copied; no game files are
redistributed.

    python -I tools/gen_data.py <path-to-excel-folder>
"""
import csv
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent

CLASSES = {
    "ama": "Amazon", "sor": "Sorceress", "nec": "Necromancer", "pal": "Paladin",
    "bar": "Barbarian", "dru": "Druid", "ass": "Assassin", "war": "Warlock",
}

# Areas that aren't waypoints but make good checks.
EXTRA_AREAS = [8, 17, 38, 73, 102, 108, 131, 132]
# Towns you arrive in by killing the previous act boss; the Rogue Encampment is free.
SKIP_AREAS = {1}

# Story quests: (quest flag id, act index, name). Flag ids follow D2's quest
# state rows (A1Q1 = 1, A2Q1 = 9, ...); Act IV is 25 Fallen Angel, 27 Hell's
# Forge, 26 Terror's End.
QUESTS = [
    (1, 0, "Den of Evil"), (2, 0, "Sisters' Burial Grounds"), (3, 0, "Tools of the Trade"),
    (4, 0, "The Search for Cain"), (5, 0, "The Forgotten Tower"), (6, 0, "Sisters to the Slaughter"),
    (9, 1, "Radament's Lair"), (10, 1, "The Horadric Staff"), (11, 1, "Tainted Sun"),
    (12, 1, "Arcane Sanctuary"), (13, 1, "The Summoner"), (14, 1, "The Seven Tombs"),
    (17, 2, "Lam Esen's Tome"), (18, 2, "Khalim's Will"), (19, 2, "Blade of the Old Religion"),
    (20, 2, "The Golden Bird"), (21, 2, "The Blackened Temple"), (22, 2, "The Guardian"),
    (25, 3, "The Fallen Angel"), (27, 3, "Hell's Forge"), (26, 3, "Terror's End"),
    (35, 4, "Siege on Harrogath"), (36, 4, "Rescue on Mount Arreat"), (37, 4, "Prison of Ice"),
    (38, 4, "Betrayal of Harrogath"), (39, 4, "Rite of Passage"), (40, 4, "Eve of Destruction"),
]
GOAL_QUEST = 40

# Superuniques that roam the field: each kill is a check per difficulty, and
# d2r-ap-setup may shuffle their monsters. Quest bosses (Radament, the
# Summoner, the Council, the Countess, Shenk, Nihlathak, the Ancients), the
# Chaos Sanctuary seal bosses and Baal's minion waves are scripted, so they're
# left alone.
FIELD_SUPERUNIQUES = [
    "Bishibosh", "Bonebreak", "Coldcrow", "Rakanishu", "Treehead WoodFist", "Griswold", "Pitspawn Fouldog",
    "Flamespike the Crawler", "Boneash", "Corpsefire", "Bloodwitch the Wild", "Fangskin", "Beetleburst", "Leatherarm",
    "Coldworm the Burrower", "Fire Eye", "Dark Elder", "Ancient Kaa the Soulless", "Web Mage the Burning",
    "Witch Doctor Endugu", "Stormtree", "Sarina the Battlemaid", "Icehawk Riftwing", "Winged Death", "The Tormentor",
    "Taintbreeder", "Riftwraith the Cannibal", "Axe Dweller", "Bonesaw Breaker", "Dac Farren", "Megaflow Rectifier",
    "Eyeback Unleashed", "Threash Socket", "Pindleskin", "Snapchip Shatter", "Anodized Elite", "Vinvear Molech",
    "Sharp Tooth Sayer", "Magma Torquer", "Blaze Ripper", "Frozenstein",
]

# Collection checks: quest items you find on Hell (by misc.txt code), then
# uniques (by name, with their base code).
SPECIAL_CODES = ["pk1", "pk2", "pk3", "mbr", "dhn", "bey", "toa", "tes", "ceh", "bet", "fed"]
SPECIAL_UNIQUES = [("cm2", "Hellfire Torch"), ("cm1", "Annihilus")]
GEM_TYPES = ["gema", "gemt", "gems", "geme", "gemr", "gemd", "gemz"]

# Quest items worth a check when they turn up (code, act index, every
# difficulty). Ones that already complete a quest (Horadric Malus, Amulet of
# the Viper, Horadric Staff...) are left out. You keep the Horadric Cube, so
# it only counts once.
QUEST_ITEMS = [
    ("bks", 0, True), ("leg", 0, True), ("box", 1, False), ("tr1", 1, True), ("msf", 1, True), ("ass", 1, True),
    ("qey", 2, True), ("qhr", 2, True), ("qbr", 2, True), ("qf1", 2, True), ("g34", 2, True), ("xyz", 2, True),
    ("mss", 3, True), ("hfh", 3, True), ("tr2", 4, True),
]
# Pandemonium's uber bosses, by MonStats Id.
UBERS = [("uberandariel", "Lilith"), ("uberduriel", "Uber Duriel"), ("uberizual", "Uber Izual"),
         ("ubermephisto", "Uber Mephisto"), ("uberdiablo", "Uber Diablo"), ("uberbaal", "Uber Baal")]

# Item kinds for level-scaled rewards.
WEAPON, ARMOR, JEWELRY = 0, 1, 2
JEWELRY_CODES = {"rin", "amu", "jew"}
# Annihilus, Hellfire Torch and Gheed's Fortune are earned, not handed out.
EXCLUDED_CODES = {"cm1", "cm2", "cm3"}


# The Druid's shapeshifts stay with the Druid in a skill tree shuffle.
PINNED_SKILLS = {"Wearwolf", "Wearbear", "Shape Shifting"}
# Only the Barbarian and the Assassin have the S2-S4 animation modes (animdata.d2); any other class
# casting an S2 skill (the Assassin's traps) freezes in place.
SPECIAL_MODE_CLASSES = ("bar", "ass")
# The classes that can hold a second weapon: the Barbarian anything, the Assassin claws (h2h is mele).
DUAL_WIELD = {"mele": ("bar", "ass"), "h2h": ("ass",), "h2h2": ("ass",)}
# Druid forms by skills.txt state, for the attacks that need one (restrict 2).
FORM_SKILLS = {"wolf": "Wearwolf", "bear": "Wearbear"}


def skill_users(row: dict[str, str]) -> tuple[str, ...] | None:
    """The classes that can use a skill, or None for every class: claw skills need claws (Assassin
    only), two-weapon skills a class that dual wields, S2-S4 skills the animation mode, and form attacks
    a Druid form."""
    users = set(CLASSES)
    if any(row.get(column) in ("h2h", "h2h2") for column in ("itypea1", "itypea2", "itypea3", "itypeb1", "itypeb2", "itypeb3")):
        users &= {"ass"}
    second = row.get("itypeb1", "")
    if second:
        users &= set(DUAL_WIELD.get(second, ("bar",)))
    if row.get("restrict") == "2":
        # Form attacks play the form's animations, not the class's.
        users &= {"dru"}
    elif row.get("anim") in ("S2", "S3", "S4"):
        users &= set(SPECIAL_MODE_CLASSES)
    return None if users == set(CLASSES) else tuple(code for code in CLASSES if code in users)


def read_table(path: Path):
    with path.open(newline="", encoding="utf-8", errors="replace") as handle:
        return list(csv.DictReader(handle, delimiter="\t"))


def main() -> None:
    excel = Path(sys.argv[1])
    skills = []
    # Active skills a level 1 character could normally take, for class starting skills.
    starters = []
    # Skills only some classes can use, and the Druid form each form attack needs.
    users, forms = {}, {}
    skill_ids = {row["skill"]: int(row["*Id"]) for row in read_table(excel / "skills.txt") if row.get("*Id", "").isdigit()}
    for row in read_table(excel / "skills.txt"):
        cls = row.get("charclass", "")
        if cls in CLASSES and row.get("*Id", "").isdigit():
            skills.append((int(row["*Id"]), row["skill"], cls))
            if (who := skill_users(row)) is not None:
                users[int(row["*Id"])] = who
            if row.get("restrict") == "2":
                states = [row.get(f"State{n}", "") for n in (1, 2, 3)]
                forms[int(row["*Id"])] = tuple(skill_ids[FORM_SKILLS[state]] for state in states if state in FORM_SKILLS)
            if row.get("reqlevel") == "1" and row.get("passive") != "1":
                starters.append(int(row["*Id"]))
    skills.sort()

    # Tree slots for the skill tree shuffle: (skill id, tree page, row, column, pinned). The Warlock
    # (Reign of the Warlock only) and skills without a tree place stay out; the Druid's forms and the
    # attacks that need one (restrict 2) are pinned to the Druid.
    descs = {row["skilldesc"]: row for row in read_table(excel / "skilldesc.txt")}
    skill_slots = []
    for row in read_table(excel / "skills.txt"):
        cls = row.get("charclass", "")
        desc = descs.get(row.get("skilldesc", ""), {})
        if cls in CLASSES and cls != "war" and row.get("*Id", "").isdigit() and desc.get("SkillPage", "0") not in ("", "0"):
            pinned = row["skill"] in PINNED_SKILLS or row.get("restrict") == "2"
            skill_slots.append((int(row["*Id"]), int(desc["SkillPage"]), int(desc["SkillRow"]), int(desc["SkillColumn"]), pinned))
    skill_slots.sort()

    areas = []
    # The act of every level id, for counting kills per act.
    level_acts = {}
    for row in read_table(excel / "levels.txt"):
        if row.get("Id", "").isdigit() and row.get("Act", "").isdigit():
            level_acts[int(row["Id"])] = int(row["Act"])
    for row in read_table(excel / "levels.txt"):
        if not row.get("Id", "").isdigit():
            continue
        level_id = int(row["Id"])
        waypoint = row.get("Waypoint", "")
        is_waypoint = waypoint not in ("", "255")
        if level_id in SKIP_AREAS or not (is_waypoint or level_id in EXTRA_AREAS):
            continue
        areas.append((level_id, int(row["Act"]), row["*StringName"]))
    areas.sort()

    # (superunique row, act index, name). The act comes from the treasure class
    # ("Act 2 Super A"); Griswold has his own, in Act I.
    superuniques = []
    for position, row in enumerate(read_table(excel / "superuniques.txt")):
        name = row.get("Superunique", "")
        if name in FIELD_SUPERUNIQUES:
            tc = row.get("TC", "")
            act = int(tc.split()[1]) - 1 if tc.startswith("Act ") else 0
            superuniques.append((int(row["hcIdx"]), act, name))
    assert len(superuniques) == len(FIELD_SUPERUNIQUES), "a field superunique is missing from superuniques.txt"
    superuniques.sort()

    misc = read_table(excel / "misc.txt")
    misc_names = {row["code"]: row["name"] for row in misc if row.get("code")}
    # (rune number, name) and (index, code, name) of each gem, in misc.txt order.
    runes = [(n, misc_names[f"r{n:02d}"]) for n in range(1, 34)]
    gems = [(row["code"], row["name"]) for row in misc if row.get("type") in GEM_TYPES]
    assert len(gems) == 35, len(gems)
    specials = [(code, "", misc_names[code].replace("Essense", "Essence")) for code in SPECIAL_CODES] + [(code, name, name) for code, name in SPECIAL_UNIQUES]

    # Lord of Destruction sets and their pieces, in setitems.txt order.
    item_sets: dict[str, list[str]] = {}
    for row in read_table(excel / "setitems.txt"):
        if row.get("*ID", "").isdigit() and int(row["*ID"]) <= 126 and row.get("set"):
            item_sets.setdefault(row["set"], []).append(row["index"])
    assert len(item_sets) == 32, len(item_sets)

    # Monster rows (*hcIdx) of the Secret Cow Level's cows and their king.
    monster_rows = {row["Id"]: int(row["*hcIdx"]) for row in read_table(excel / "monstats.txt") if row.get("*hcIdx", "").isdigit()}
    cow, cow_king = monster_rows["cow"], monster_rows["cowking"]
    ubers = [(monster_rows[monster], name) for monster, name in UBERS]

    names_by_code = dict(misc_names)
    for table in ("weapons.txt", "armor.txt"):
        names_by_code.update({row["code"]: row["name"] for row in read_table(excel / table) if row.get("code")})
    quest_items = [(code, act, every, names_by_code[code]) for code, act, every in QUEST_ITEMS]

    # Experience needed for each character level (1..99): row N is level N + 1.
    experience = [int(row["Amazon"]) for row in read_table(excel / "experience.txt") if row.get("Level", "").isdigit()][:99]
    assert len(experience) == 99 and experience[0] == 0, len(experience)

    weapon_codes = {row["code"] for row in read_table(excel / "weapons.txt") if row.get("code")}
    armor_codes = {row["code"] for row in read_table(excel / "armor.txt") if row.get("code")}

    # The class that alone can use each base (index into CLASSES), or -1. An item
    # type's Class can come from the types it's equivalent to (h2h2 -> h2h).
    item_types = {row["Code"]: row for row in read_table(excel / "itemtypes.txt") if row.get("Code")}

    def type_class(code: str) -> str:
        row = item_types.get(code)
        if row is None:
            return ""
        return row.get("Class") or type_class(row.get("Equiv1", "")) or type_class(row.get("Equiv2", ""))

    class_codes = list(CLASSES)
    base_class = {}
    for table in ("weapons.txt", "armor.txt"):
        for row in read_table(excel / table):
            if row.get("code"):
                cls = type_class(row.get("type", ""))
                base_class[row["code"]] = class_codes.index(cls) if cls in CLASSES else -1

    def kind_of(code: str):
        if code in EXCLUDED_CODES:
            return None
        if code in JEWELRY_CODES:
            return JEWELRY
        if code in weapon_codes:
            return WEAPON
        if code in armor_codes:
            return ARMOR
        return None

    # D2R's record numbers count every row of UniqueItems/SetItems, divider rows
    # like "Expansion" included, so they're the row position, not the *ID column.
    # (Confirmed in game: asking for *ID-based record 438 was refused.)
    LAST_LOD_SET_ID = 126  # McAuley's Superstition
    # Quest items have unique rows too (Khalim's Will, Horadric Staff...). As loot
    # they'd trip the quest-item checks, so they're never rewards.
    QUEST_ITEM_CODES = {"leg", "hdm", "hfh", "hst", "msf", "qf1", "qf2", "g33", "d33", "vip"}

    def quality_rows(table: str, code_column: str):
        rows = []
        for position, row in enumerate(read_table(excel / table)):
            # Reign of the Warlock items: uniques after the "Warlock Class Pack"
            # divider and sets past McAuley's Folly. Lord of Destruction
            # characters can't receive them.
            if row.get("index") == "Warlock Class Pack":
                break
            if table == "setitems.txt" and row.get("*ID", "").isdigit() and int(row["*ID"]) > LAST_LOD_SET_ID:
                continue
            code = row.get(code_column, "")
            if code in QUEST_ITEM_CODES:
                continue
            if not code or row.get("disabled") == "1" or row.get("firstLadderSeason"):
                continue
            kind = kind_of(code)
            if kind is None or not row.get("lvl req", "").isdigit():
                continue
            rows.append((position, code, int(row["lvl"] or 1), int(row["lvl req"]), kind, base_class.get(code, -1), row["index"]))
        return rows

    # The Codex charm's forced affix: "Stout" (+1 Defense), the small-charm prefix
    # with no level requirement. Affix ids count MagicSuffix rows, then
    # MagicPrefix rows, one-based, without MagicSuffix's blank first row.
    # (Confirmed in game: id 922 gave prefix row 137 and suffix id 257 was row 257.)
    suffix_rows = read_table(excel / "magicsuffix.txt")
    codex_prefix = 0
    for index, row in enumerate(read_table(excel / "magicprefix.txt")):
        itypes = {row.get(f"itype{n}", "") for n in range(1, 8)}
        if row.get("Name") == "Stout" and "scha" in itypes and row.get("levelreq") == "1":
            codex_prefix = len(suffix_rows) - 1 + index
            break
    assert codex_prefix, "no level-1 Stout prefix for small charms"

    # The Codex is a unique small charm appended to UniqueItems after the last row.
    codex_unique = len(read_table(excel / "uniqueitems.txt"))

    uniques = quality_rows("uniqueitems.txt", "code")
    sets = quality_rows("setitems.txt", "item")

    # Bases that can take 4+ sockets, for runewords.
    bases = []
    for table, kind in (("weapons.txt", WEAPON), ("armor.txt", ARMOR)):
        for row in read_table(excel / table):
            sockets = row.get("gemsockets", "")
            if (row.get("code") and row.get("spawnable") == "1" and not row.get("quest") and sockets.isdigit()
                    and int(sockets) >= 4 and row.get("levelreq", "").isdigit()):
                bases.append((row["code"], int(row["levelreq"]), min(int(sockets), 6), kind, base_class[row["code"]], row["name"]))

    py = ["# Generated by tools/gen_data.py. Do not edit by hand.", "",
          f"CLASSES = {CLASSES!r}", "", "# (skill id, name, class code)", "SKILLS = ["]
    py += [f"    ({sid}, {name!r}, {cls!r})," for sid, name, cls in skills]
    py += ["]", "", "# Active skills with a vanilla level requirement of 1.", f"STARTER_SKILLS = frozenset({sorted(starters)!r})"]
    py += ["", "# Skills only these classes can use (claws, two weapons, the S2-S4 animations, Druid forms).",
           "SKILL_USERS = {"]
    py += [f"    {sid}: {who!r}," for sid, who in sorted(users.items())]
    py += ["}", "", "# Druid form attacks: skill id -> the form skills (any one) it needs.", "SKILL_FORMS = {"]
    py += [f"    {sid}: {need!r}," for sid, need in sorted(forms.items())]
    py += ["}"]
    py += ["", "# (skill id, tree page, row, column, pinned): class skills' tree places, for the skill tree shuffle.", "SKILL_SLOTS = ["]
    py += [f"    ({sid}, {page}, {row}, {column}, {pinned})," for sid, page, row, column, pinned in skill_slots]
    py += ["]"]
    py += ["", "# (level id, act index, name)", "AREAS = ["]
    py += [f"    ({lid}, {act}, {name!r})," for lid, act, name in areas]
    py += ["]", "", "# (quest flag id, act index, name)", "QUESTS = ["]
    py += [f"    ({qid}, {act}, {name!r})," for qid, act, name in QUESTS]
    py += ["]", "", "# (superunique id, act index, name): the field superuniques, one check each per difficulty.", "SUPERUNIQUES = ["]
    py += [f"    ({sid}, {act}, {name!r})," for sid, act, name in superuniques]
    py += ["]", "", "# (rune number, name)", "RUNES = ["]
    py += [f"    ({n}, {name!r})," for n, name in runes]
    py += ["]", "", "# Gem names, in misc.txt order (index = position).", "GEMS = ["]
    py += [f"    {name!r}," for _code, name in gems]
    py += ["]", "", "# Lord of Destruction sets: (name, piece names).", "SETS = ["]
    py += [f"    ({name!r}, {pieces!r})," for name, pieces in item_sets.items()]
    py += ["]", "", "# Rare Hell items worth a check (index = position).", "SPECIAL_ITEMS = ["]
    py += [f"    {name!r}," for _code, _unique, name in specials]
    py += ["]", "", "# Quest items worth a check: (act index, every difficulty, name); index = position.", "QUEST_ITEMS = ["]
    py += [f"    ({act}, {every}, {name!r})," for _code, act, every, name in quest_items]
    py += ["]", "", "# Pandemonium's uber bosses; index = position.", "UBERS = ["]
    py += [f"    {name!r}," for _row, name in ubers]
    py += ["]", "", f"COW_LEVEL = 39", "", f"GOAL_QUEST = {GOAL_QUEST}", ""]
    (ROOT / "apworld" / "d2r_archipelago" / "data.py").write_text("\n".join(py), encoding="utf-8")

    def cstr(text: str) -> str:
        return '"' + text.replace("\\", "\\\\").replace('"', '\\"') + '"'

    h = ["// Generated by tools/gen_data.py. Do not edit by hand.", "#pragma once", "",
         "#include <cstdint>", "", "namespace ap::data {", "",
         "struct Skill { uint16_t id; const char* name; const char* cls; };",
         "struct Area { uint16_t id; uint8_t act; const char* name; };",
         "struct Quest { uint8_t flag; uint8_t act; const char* name; };",
         "struct SuperUnique { uint8_t id; uint8_t act; const char* name; };",
         "struct Collectible { const char* code; const char* unique; const char* name; };",
         "struct ItemSet { const char* name; const char* pieces[8]; };",
         "struct QuestItem { const char* code; uint8_t act; bool everyDifficulty; const char* name; };",
         "struct Uber { uint16_t monster; const char* name; };",
         "enum Kind : uint8_t { Weapon, Armor, Jewelry };",
         "// record is the UniqueItems/SetItems *ID; code is the base item. cls is the",
         "// only class that can use it (ClassNames order: 0 Amazon .. 7 Warlock), or -1.",
         "struct QualityItem { uint16_t record; const char* code; uint8_t itemLevel; uint8_t reqLevel; Kind kind; int8_t cls; const char* name; };",
         "struct Base { const char* code; uint8_t reqLevel; uint8_t sockets; Kind kind; int8_t cls; const char* name; };", "",
         "inline constexpr Skill Skills[] = {"]
    h += [f"\t{{ {sid}, {cstr(name)}, {cstr(CLASSES[cls])} }}," for sid, name, cls in skills]
    h += ["};", "", "inline constexpr Area Areas[] = {"]
    h += [f"\t{{ {lid}, {act}, {cstr(name)} }}," for lid, act, name in areas]
    h += ["};", "", "inline constexpr Quest Quests[] = {"]
    h += [f"\t{{ {qid}, {act}, {cstr(name)} }}," for qid, act, name in QUESTS]
    h += ["};", "", "// The field superuniques (SuperUniques row, act, name), one check each per difficulty.",
          "inline constexpr SuperUnique SuperUniques[] = {"]
    h += [f"\t{{ {sid}, {act}, {cstr(name)} }}," for sid, act, name in superuniques]
    h += ["};", "", "// Collection checks. Runes are r01..r33; Gems and Specials go by base code,",
          "// and a Special with a unique name must be that unique.", "inline constexpr Collectible Gems[] = {"]
    h += [f"\t{{ {cstr(code)}, nullptr, {cstr(name)} }}," for code, name in gems]
    h += ["};", "", "inline constexpr Collectible Specials[] = {"]
    h += [f"\t{{ {cstr(code)}, {cstr(unique) if unique else 'nullptr'}, {cstr(name)} }}," for code, unique, name in specials]
    h += ["};", "", "// Lord of Destruction sets: a check once every piece is on the character.", "inline constexpr ItemSet Sets[] = {"]
    h += ["\t{ " + cstr(name) + ", { " + ", ".join(cstr(piece) for piece in pieces) + " } }," for name, pieces in item_sets.items()]
    h += ["};", "", "// Quest items worth a check, per difficulty unless everyDifficulty is false.",
          "inline constexpr QuestItem QuestItems[] = {"]
    h += [f"\t{{ {cstr(code)}, {act}, {'true' if every else 'false'}, {cstr(name)} }}," for code, act, every, name in quest_items]
    h += ["};", "", "// Pandemonium's uber bosses (MonStats rows).", "inline constexpr Uber Ubers[] = {"]
    h += [f"\t{{ {row}, {cstr(name)} }}," for row, name in ubers]
    top = max(level_acts)
    h += ["};", "", "// The act of each level id (0-4), 255 for none.", "inline constexpr uint8_t LevelActs[] = {"]
    acts_list = [level_acts.get(level, 255) for level in range(top + 1)]
    h += ["\t" + ", ".join(str(act) for act in acts_list[i:i + 20]) + "," for i in range(0, len(acts_list), 20)]
    h += ["};", "", "// The Secret Cow Level and its monster rows.", "inline constexpr uint16_t CowLevel = 39;",
          f"inline constexpr uint16_t CowMonster = {cow};", f"inline constexpr uint16_t CowKingMonster = {cow_king};"]
    h += ["", "// Experience needed for character levels 1..99.", "inline constexpr uint32_t Experience[] = {"]
    h += ["\t" + ", ".join(str(value) for value in experience[i:i + 10]) + "," for i in range(0, len(experience), 10)]
    kinds = ("Weapon", "Armor", "Jewelry")
    for label, rows in (("Uniques", uniques), ("SetItems", sets)):
        h += ["};", "", f"inline constexpr QualityItem {label}[] = {{"]
        h += [f"	{{ {rid}, {cstr(code)}, {min(ilvl, 99)}, {req}, {kinds[kind]}, {cls}, {cstr(name)} }}," for rid, code, ilvl, req, kind, cls, name in rows]
    h += ["};", "", "inline constexpr Base SocketBases[] = {"]
    h += [f"	{{ {cstr(code)}, {req}, {sockets}, {kinds[kind]}, {cls}, {cstr(name)} }}," for code, req, sockets, kind, cls, name in bases]
    h += ["};", "", "// Combined affix id of the Codex charm's fixed prefix (Stout, +1 Defense).",
          f"inline constexpr uint32_t CodexPrefix = {codex_prefix};",
          "// UniqueItems record d2r-ap-setup adds for the Codex charm (the RotW bank).",
          f"inline constexpr uint32_t CodexUnique = {codex_unique};", ""]
    h += [f"inline constexpr uint8_t GoalQuest = {GOAL_QUEST};", "", "} // namespace ap::data", ""]
    (ROOT / "plugin" / "src" / "game_data.h").write_text("\n".join(h), encoding="utf-8")
    print(f"{len(skills)} skills, {len(areas)} areas, {len(QUESTS)} quests, {len(superuniques)} superuniques, {len(item_sets)} sets, cows {cow}/{cow_king}, {len(uniques)} uniques, "
          f"{len(sets)} set items, {len(bases)} socket bases, codex prefix {codex_prefix}")


if __name__ == "__main__":
    main()
