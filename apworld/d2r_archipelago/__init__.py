from typing import Any

from BaseClasses import Item, ItemClassification, Location, Region, Tutorial
from worlds.AutoWorld import WebWorld, World

from .data import AREAS, CLASSES, GOAL_QUEST, QUESTS, SKILLS
from .ids import (ACTS, DIFFICULTIES, FILLERS, LEVEL_CAPS, LEVEL_STEP, area_location_name, item_name_to_id,
                  level_location_name, location_name_to_id, quest_location_name, skill_item_name,
                  skill_names_by_class)
from .options import D2ROptions

GAME = "Diablo II Resurrected"
SKILLS_GROUP = "Skills"


class D2RItem(Item):
    game = GAME


class D2RLocation(Location):
    game = GAME


class D2RWeb(WebWorld):
    theme = "stone"
    tutorials = [Tutorial(
        "Multiworld Setup Guide",
        "Install the D2RLoader plugin and connect Diablo II: Resurrected to a multiworld.",
        "English",
        "setup_en.md",
        "setup/en",
        ["snrn"],
    )]


class D2RWorld(World):
    """Diablo II: Resurrected with every class skill shuffled into the multiworld.
    Skills you receive arrive as charms that grant them, whatever your class."""

    game = GAME
    web = D2RWeb()
    options_dataclass = D2ROptions
    options: D2ROptions
    topology_present = True

    item_name_to_id = item_name_to_id
    location_name_to_id = location_name_to_id
    item_name_groups = {SKILLS_GROUP: set().union(*skill_names_by_class.values()), **skill_names_by_class}

    def generate_early(self) -> None:
        self.act_count = (self.options.goal.value + 1) * len(ACTS)
        self.skill_pool: list[str] = []
        self.starting: list[str] = []

    def act_region_name(self, index: int) -> str:
        return f"{DIFFICULTIES[index // len(ACTS)]} {ACTS[index % len(ACTS)]}"

    def required_skills(self, index: int) -> int:
        return index * self.options.skills_per_act.value

    def create_regions(self) -> None:
        menu = Region("Menu", self.player, self.multiworld)
        self.multiworld.regions.append(menu)
        acts = []
        for index in range(self.act_count):
            region = Region(self.act_region_name(index), self.player, self.multiworld)
            self.multiworld.regions.append(region)
            acts.append(region)
        menu.connect(acts[0])
        for index in range(1, self.act_count):
            need = self.required_skills(index)
            acts[index - 1].connect(acts[index], rule=lambda state, need=need:
                                    state.has_group(SKILLS_GROUP, self.player, need))

        def add(region: Region, name: str) -> None:
            region.locations.append(D2RLocation(self.player, name, location_name_to_id[name], region))

        for difficulty in range(self.options.goal.value + 1):
            for _flag, act, name in QUESTS:
                add(acts[difficulty * len(ACTS) + act], quest_location_name(difficulty, name))
        if self.options.area_checks:
            for _level_id, act, name in AREAS:
                add(acts[act], area_location_name(name))
        if self.options.level_checks:
            cap = LEVEL_CAPS[self.options.goal.value]
            for level in range(LEVEL_STEP, cap + 1, LEVEL_STEP):
                # Spread the milestones over the acts in proportion to the level cap.
                add(acts[min(self.act_count - 1, (level - 1) * self.act_count // cap)], level_location_name(level))

        final = acts[-1]
        victory = D2RLocation(self.player, "Defeat Baal", None, final)
        victory.place_locked_item(D2RItem("Victory", ItemClassification.progression, None, self.player))
        final.locations.append(victory)
        self.multiworld.completion_condition[self.player] = lambda state: state.has("Victory", self.player)

    def create_item(self, name: str) -> D2RItem:
        if name in FILLERS:
            classification = ItemClassification.filler
        elif self.options.skills_per_act.value > 0:
            classification = ItemClassification.progression
        else:
            classification = ItemClassification.useful
        return D2RItem(name, classification, item_name_to_id[name], self.player)

    def create_items(self) -> None:
        allowed = {CLASSES[code] for code in CLASSES} if not self.options.skill_classes.value \
            else set(self.options.skill_classes.value)
        candidates = [skill_item_name(sid, name, cls) for sid, name, cls in SKILLS if CLASSES[cls] in allowed]
        location_count = len(self.multiworld.get_unfilled_locations(self.player))
        starting = min(self.options.starting_skills.value, len(candidates))
        size = min(self.options.skill_pool_size.value, len(candidates) - starting, location_count)
        # Logic needs every act's skill count to be reachable from the pool plus starting skills.
        size = max(size, min(self.required_skills(self.act_count - 1) - starting, len(candidates) - starting))
        if size > location_count:
            raise Exception(f"{self.player_name}: {self.required_skills(self.act_count - 1)} skills needed by "
                            f"skills_per_act don't fit in {location_count} locations; lower skills_per_act.")
        chosen = self.random.sample(candidates, starting + size)
        self.starting = chosen[:starting]
        self.skill_pool = chosen[starting:]
        for name in self.starting:
            self.multiworld.push_precollected(self.create_item(name))
        pool = [self.create_item(name) for name in self.skill_pool]
        pool += [self.create_filler() for _ in range(location_count - len(pool))]
        self.multiworld.itempool += pool

    def get_filler_item_name(self) -> str:
        return self.random.choice(FILLERS)

    def fill_slot_data(self) -> dict[str, Any]:
        return {
            "goal": self.options.goal.value,
            "goal_quest": GOAL_QUEST,
            "area_checks": bool(self.options.area_checks),
            "level_checks": bool(self.options.level_checks),
            "level_step": LEVEL_STEP,
            "level_cap": LEVEL_CAPS[self.options.goal.value],
        }
