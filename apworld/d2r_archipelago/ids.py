"""Item and location ids. The plugin computes the same ids (plugin/src/ids.h)."""
from .data import AREAS, CLASSES, GEMS, QUEST_ITEMS, QUESTS, RUNES, SETS, SKILLS, SPECIAL_ITEMS, SUPERUNIQUES, UBERS

ITEM_BASE = 2_430_000
LOCATION_BASE = 2_420_000

DIFFICULTIES = ("Normal", "Nightmare", "Hell")
ACTS = ("Act I", "Act II", "Act III", "Act IV", "Act V")
LEVEL_STEP = 5
LEVEL_CAPS = (30, 60, 90)

# Loot ids are ITEM_BASE + 1000 + index; the plugin rolls the actual item, scaled to
# the character's level, when it arrives. Append only: the index is the id.
LOOT = (
    "Full Rejuvenation Potion",
    "Perfect Gem",
    "Low Rune",
    "Mid Rune",
    "High Rune",
    "Random Unique",
    "Random Unique Weapon",
    "Random Unique Armor",
    "Random Unique Jewelry",
    "Random Set Item",
    "Socketed Base",
    "Junk Backpack",
    "Gold",
    "Experience",
    "Random Charm",
    "Random Jewel",
    "Random Shrine",
    "Ambush Trap",
    "Poison Cloud Trap",
    "Firestorm Trap",
    "Mana Drain Trap",
    "Tax Collector Trap",
    "Mercenary Down Trap",
    "Prophecy",
)
BASIC_LOOT = ("Full Rejuvenation Potion", "Perfect Gem", "Low Rune", "Mid Rune")
UNIQUE_LOOT = ("Random Unique Weapon", "Random Unique Armor", "Random Unique Jewelry")
# Traps, with the option holding each one's weight.
TRAPS = {
    "Junk Backpack": "junk_backpack_weight",
    "Ambush Trap": "ambush_weight",
    "Poison Cloud Trap": "poison_cloud_weight",
    "Firestorm Trap": "firestorm_weight",
    "Mana Drain Trap": "mana_drain_weight",
    "Tax Collector Trap": "tax_collector_weight",
    "Mercenary Down Trap": "mercenary_down_weight",
}
# Each copy adds +1 to every skill in the Codex.
EMPOWERMENT = "Codex Empowerment"
EMPOWERMENT_ID = ITEM_BASE + 900
# Act keys unlock acts 2..15 (Normal Act II through Hell Act V): ITEM_BASE + 950 + act index.
KEY_BASE = ITEM_BASE + 950


def act_key_name(act_index: int) -> str:
    return f"{DIFFICULTIES[act_index // len(ACTS)]} {ACTS[act_index % len(ACTS)]} Key"


KEY_NAMES = [act_key_name(index) for index in range(1, len(DIFFICULTIES) * len(ACTS))]

# Locks: ITEM_BASE + 980 + index. Must match plugin/src/ids.h.
LOCK_BASE = ITEM_BASE + 980
CUBE_ACCESS = "Horadric Cube Access"
MERCHANT_ACCESS = "Merchant Access"
LOCKS = (CUBE_ACCESS, "Town Portal Access", "Potion Access", "Stash Access", MERCHANT_ACCESS,
         *(f"{act} Waypoints" for act in ACTS))


def skill_item_name(skill_id: int, name: str, cls: str) -> str:
    return f"{name} ({CLASSES[cls]})"


def quest_location_name(difficulty: int, quest_name: str) -> str:
    return f"{DIFFICULTIES[difficulty]}: {quest_name}"


def area_location_name(difficulty: int, area_name: str) -> str:
    return f"{DIFFICULTIES[difficulty]}: Enter {area_name}"


def level_location_name(level: int) -> str:
    return f"Reach Character Level {level}"


def superunique_location_name(difficulty: int, name: str) -> str:
    return f"{DIFFICULTIES[difficulty]}: Kill {name}"


def find_location_name(item_name: str) -> str:
    return f"Find {item_name}"


def set_location_name(set_name: str) -> str:
    return f"Complete {set_name}"


COW_CHECKS = ("Enter the Secret Cow Level", "Kill The Cow King", "Kill 100 Cows")
COW_KILLS = 100


def cow_location_name(difficulty: int, check: int) -> str:
    return f"{DIFFICULTIES[difficulty]}: {COW_CHECKS[check]}"


def quest_item_location_name(difficulty: int, name: str) -> str:
    return f"{DIFFICULTIES[difficulty]}: Find {name}"


def shop_location_name(act: int, number: int) -> str:
    return f"{ACTS[act]}: Buy Archipelago Parcel {number + 1}"


MAX_SHOP_CHECKS = 5
KILL_COUNTS = (100, 250, 500)
UNIQUE_FINDS = (1, 5, 10, 25, 50)
SET_FINDS = (1, 5, 10, 25)


def kill_location_name(difficulty: int, act: int, count: int) -> str:
    return f"{DIFFICULTIES[difficulty]}: Kill {count} Monsters in {ACTS[act]}"


def find_count_location_name(count: int, kind: str) -> str:
    return f"Find {count} {kind} Item{'s' if count > 1 else ''}"


item_name_to_id: dict[str, int] = {}
skill_names_by_class: dict[str, set[str]] = {name: set() for name in CLASSES.values()}
for skill_id, name, cls in SKILLS:
    item_name = skill_item_name(skill_id, name, cls)
    assert item_name not in item_name_to_id, item_name
    item_name_to_id[item_name] = ITEM_BASE + skill_id
    skill_names_by_class[CLASSES[cls]].add(item_name)
for index, name in enumerate(LOOT):
    item_name_to_id[name] = ITEM_BASE + 1000 + index
item_name_to_id[EMPOWERMENT] = EMPOWERMENT_ID
for index in range(1, len(DIFFICULTIES) * len(ACTS)):
    item_name_to_id[act_key_name(index)] = KEY_BASE + index
for index, name in enumerate(LOCKS):
    item_name_to_id[name] = LOCK_BASE + index

location_name_to_id: dict[str, int] = {}
for difficulty in range(3):
    for flag, _act, name in QUESTS:
        location_name_to_id[quest_location_name(difficulty, name)] = LOCATION_BASE + difficulty * 100 + flag
# Area entries: LOCATION_BASE + 1000 + difficulty * 200 + level id.
for difficulty in range(3):
    for level_id, _act, name in AREAS:
        location_name_to_id[area_location_name(difficulty, name)] = LOCATION_BASE + 1000 + difficulty * 200 + level_id
for level in range(LEVEL_STEP, LEVEL_CAPS[-1] + 1, LEVEL_STEP):
    location_name_to_id[level_location_name(level)] = LOCATION_BASE + 2000 + level
# Superunique kills: LOCATION_BASE + 3000 + difficulty * 100 + superunique id.
for difficulty in range(3):
    for superunique_id, _act, name in SUPERUNIQUES:
        location_name_to_id[superunique_location_name(difficulty, name)] = LOCATION_BASE + 3000 + difficulty * 100 + superunique_id
# Collections, once per seed: runes 5000 + rune number, gems 5100 + index, sets 5200 + index,
# special items 5300 + index. Cow level: 5400 + difficulty * 10 + check.
for number, name in RUNES:
    location_name_to_id[find_location_name(name)] = LOCATION_BASE + 5000 + number
for index, name in enumerate(GEMS):
    location_name_to_id[find_location_name(name)] = LOCATION_BASE + 5100 + index
for index, (name, _pieces) in enumerate(SETS):
    location_name_to_id[set_location_name(name)] = LOCATION_BASE + 5200 + index
for index, name in enumerate(SPECIAL_ITEMS):
    location_name_to_id[find_location_name(name)] = LOCATION_BASE + 5300 + index
for difficulty in range(3):
    for check in range(len(COW_CHECKS)):
        location_name_to_id[cow_location_name(difficulty, check)] = LOCATION_BASE + 5400 + difficulty * 10 + check
# Shops 5500 + act * 10 + number, ubers 5600 + index, finds 5800 + index (uniques) and 5810 + index (sets),
# quest items 5900 + difficulty * 20 + index, kill counts 6000 + difficulty * 100 + act * 10 + index.
for act in range(len(ACTS)):
    for number in range(MAX_SHOP_CHECKS):
        location_name_to_id[shop_location_name(act, number)] = LOCATION_BASE + 5500 + act * 10 + number
for index, name in enumerate(UBERS):
    location_name_to_id[find_location_name(name).replace("Find ", "Kill ")] = LOCATION_BASE + 5600 + index
for index, count in enumerate(UNIQUE_FINDS):
    location_name_to_id[find_count_location_name(count, "Unique")] = LOCATION_BASE + 5800 + index
for index, count in enumerate(SET_FINDS):
    location_name_to_id[find_count_location_name(count, "Set")] = LOCATION_BASE + 5810 + index
for difficulty in range(3):
    for index, (_act, every, name) in enumerate(QUEST_ITEMS):
        if every or difficulty == 0:
            location_name_to_id[quest_item_location_name(difficulty, name)] = LOCATION_BASE + 5900 + difficulty * 20 + index
    for act in range(len(ACTS)):
        for index, count in enumerate(KILL_COUNTS):
            location_name_to_id[kill_location_name(difficulty, act, count)] = \
                LOCATION_BASE + 6000 + difficulty * 100 + act * 10 + index
