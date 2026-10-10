"""Generate the shared game tables for the apworld and the plugin.

Reads skills, levels, uniqueitems, setitems, weapons, armor and misc from an
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

# Item kinds for level-scaled rewards.
WEAPON, ARMOR, JEWELRY = 0, 1, 2
JEWELRY_CODES = {"rin", "amu", "jew"}
# Annihilus, Hellfire Torch and Gheed's Fortune are earned, not handed out.
EXCLUDED_CODES = {"cm1", "cm2", "cm3"}


def read_table(path: Path):
    with path.open(newline="", encoding="utf-8", errors="replace") as handle:
        return list(csv.DictReader(handle, delimiter="\t"))


def main() -> None:
    excel = Path(sys.argv[1])
    skills = []
    for row in read_table(excel / "skills.txt"):
        cls = row.get("charclass", "")
        if cls in CLASSES and row.get("*Id", "").isdigit():
            skills.append((int(row["*Id"]), row["skill"], cls))
    skills.sort()

    areas = []
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

    weapon_codes = {row["code"] for row in read_table(excel / "weapons.txt") if row.get("code")}
    armor_codes = {row["code"] for row in read_table(excel / "armor.txt") if row.get("code")}

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
    def quality_rows(table: str, code_column: str):
        rows = []
        for position, row in enumerate(read_table(excel / table)):
            code = row.get(code_column, "")
            if not code or row.get("disabled") == "1" or row.get("firstLadderSeason"):
                continue
            kind = kind_of(code)
            if kind is None or not row.get("lvl req", "").isdigit():
                continue
            rows.append((position, code, int(row["lvl"] or 1), int(row["lvl req"]), kind, row["index"]))
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
                bases.append((row["code"], int(row["levelreq"]), min(int(sockets), 6), kind, row["name"]))

    py = ["# Generated by tools/gen_data.py. Do not edit by hand.", "",
          f"CLASSES = {CLASSES!r}", "", "# (skill id, name, class code)", "SKILLS = ["]
    py += [f"    ({sid}, {name!r}, {cls!r})," for sid, name, cls in skills]
    py += ["]", "", "# (level id, act index, name)", "AREAS = ["]
    py += [f"    ({lid}, {act}, {name!r})," for lid, act, name in areas]
    py += ["]", "", "# (quest flag id, act index, name)", "QUESTS = ["]
    py += [f"    ({qid}, {act}, {name!r})," for qid, act, name in QUESTS]
    py += ["]", "", f"GOAL_QUEST = {GOAL_QUEST}", ""]
    (ROOT / "apworld" / "d2r_archipelago" / "data.py").write_text("\n".join(py), encoding="utf-8")

    def cstr(text: str) -> str:
        return '"' + text.replace("\\", "\\\\").replace('"', '\\"') + '"'

    h = ["// Generated by tools/gen_data.py. Do not edit by hand.", "#pragma once", "",
         "#include <cstdint>", "", "namespace ap::data {", "",
         "struct Skill { uint16_t id; const char* name; const char* cls; };",
         "struct Area { uint16_t id; uint8_t act; const char* name; };",
         "struct Quest { uint8_t flag; uint8_t act; const char* name; };",
         "enum Kind : uint8_t { Weapon, Armor, Jewelry };",
         "// record is the UniqueItems/SetItems *ID; code is the base item.",
         "struct QualityItem { uint16_t record; const char* code; uint8_t itemLevel; uint8_t reqLevel; Kind kind; const char* name; };",
         "struct Base { const char* code; uint8_t reqLevel; uint8_t sockets; Kind kind; const char* name; };", "",
         "inline constexpr Skill Skills[] = {"]
    h += [f"\t{{ {sid}, {cstr(name)}, {cstr(CLASSES[cls])} }}," for sid, name, cls in skills]
    h += ["};", "", "inline constexpr Area Areas[] = {"]
    h += [f"\t{{ {lid}, {act}, {cstr(name)} }}," for lid, act, name in areas]
    h += ["};", "", "inline constexpr Quest Quests[] = {"]
    h += [f"\t{{ {qid}, {act}, {cstr(name)} }}," for qid, act, name in QUESTS]
    kinds = ("Weapon", "Armor", "Jewelry")
    for label, rows in (("Uniques", uniques), ("SetItems", sets)):
        h += ["};", "", f"inline constexpr QualityItem {label}[] = {{"]
        h += [f"	{{ {rid}, {cstr(code)}, {min(ilvl, 99)}, {req}, {kinds[kind]}, {cstr(name)} }}," for rid, code, ilvl, req, kind, name in rows]
    h += ["};", "", "inline constexpr Base SocketBases[] = {"]
    h += [f"	{{ {cstr(code)}, {req}, {sockets}, {kinds[kind]}, {cstr(name)} }}," for code, req, sockets, kind, name in bases]
    h += ["};", "", "// Combined affix id of the Codex charm's fixed prefix (Stout, +1 Defense).",
          f"inline constexpr uint32_t CodexPrefix = {codex_prefix};",
          "// UniqueItems record d2r-ap-setup adds for the Codex charm (the RotW bank).",
          f"inline constexpr uint32_t CodexUnique = {codex_unique};", ""]
    h += [f"inline constexpr uint8_t GoalQuest = {GOAL_QUEST};", "", "} // namespace ap::data", ""]
    (ROOT / "plugin" / "src" / "game_data.h").write_text("\n".join(h), encoding="utf-8")
    print(f"{len(skills)} skills, {len(areas)} areas, {len(QUESTS)} quests, {len(uniques)} uniques, "
          f"{len(sets)} set items, {len(bases)} socket bases, codex prefix {codex_prefix}")


if __name__ == "__main__":
    main()
