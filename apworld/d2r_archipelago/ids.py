"""Item and location ids. The plugin computes the same ids (plugin/src/ids.h)."""
from .data import AREAS, CLASSES, QUESTS, SKILLS

ITEM_BASE = 2_430_000
LOCATION_BASE = 2_420_000

DIFFICULTIES = ("Normal", "Nightmare", "Hell")
ACTS = ("Act I", "Act II", "Act III", "Act IV", "Act V")
LEVEL_STEP = 5
LEVEL_CAPS = (30, 60, 90)

# Filler ids are ITEM_BASE + 1000 + index; the plugin maps each index to an item.
FILLERS = (
    "Full Rejuvenation Potion",
    "Perfect Gem",
    "Low Rune",
    "Mid Rune",
)


def skill_item_name(skill_id: int, name: str, cls: str) -> str:
    return f"{name} ({CLASSES[cls]})"


def quest_location_name(difficulty: int, quest_name: str) -> str:
    return f"{DIFFICULTIES[difficulty]}: {quest_name}"


def area_location_name(area_name: str) -> str:
    return f"Enter {area_name}"


def level_location_name(level: int) -> str:
    return f"Reach Character Level {level}"


item_name_to_id: dict[str, int] = {}
skill_names_by_class: dict[str, set[str]] = {name: set() for name in CLASSES.values()}
for skill_id, name, cls in SKILLS:
    item_name = skill_item_name(skill_id, name, cls)
    assert item_name not in item_name_to_id, item_name
    item_name_to_id[item_name] = ITEM_BASE + skill_id
    skill_names_by_class[CLASSES[cls]].add(item_name)
for index, name in enumerate(FILLERS):
    item_name_to_id[name] = ITEM_BASE + 1000 + index

location_name_to_id: dict[str, int] = {}
for difficulty in range(3):
    for flag, _act, name in QUESTS:
        location_name_to_id[quest_location_name(difficulty, name)] = LOCATION_BASE + difficulty * 100 + flag
for level_id, _act, name in AREAS:
    location_name_to_id[area_location_name(name)] = LOCATION_BASE + 1000 + level_id
for level in range(LEVEL_STEP, LEVEL_CAPS[-1] + 1, LEVEL_STEP):
    location_name_to_id[level_location_name(level)] = LOCATION_BASE + 2000 + level
