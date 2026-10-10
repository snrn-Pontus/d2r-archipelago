from typing import Any

from BaseClasses import Item, ItemClassification, Location, Region, Tutorial
from worlds.AutoWorld import WebWorld, World

from .data import AREAS, CLASSES, QUESTS, SKILLS, STARTER_SKILLS
from .ids import (ACTS, BASIC_LOOT, DIFFICULTIES, EMPOWERMENT, LEVEL_CAPS, LEVEL_STEP, TRAP, UNIQUE_LOOT,
                  area_location_name, item_name_to_id, level_location_name, location_name_to_id,
                  quest_location_name, skill_item_name, skill_names_by_class)
from .options import D2ROptions

GAME = "Diablo II Resurrected"
SKILLS_GROUP = "Skills"
BOSS_UNIQUE = "Random Unique"
# Quest flags of the act boss kills: Andariel, Duriel, Mephisto, Diablo, Baal.
BOSS_QUESTS = (6, 14, 22, 26, 40)
BOSS_NAMES = ("Andariel", "Duriel", "Mephisto", "Diablo", "Baal")
USEFUL_LOOT = {BOSS_UNIQUE, *UNIQUE_LOOT, "Random Set Item", "High Rune", EMPOWERMENT}


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
        goal, goal_act = self.options.goal.value, self.options.goal_act.value
        # Acts in play, counting every act of the easier difficulties.
        self.act_count = goal * len(ACTS) + goal_act
        # The level cap grows with the acts in play, from the previous difficulty's cap.
        previous = LEVEL_CAPS[goal - 1] if goal > 0 else 0
        cap = previous + (LEVEL_CAPS[goal] - previous) * goal_act // len(ACTS)
        self.level_cap = max(LEVEL_STEP, cap - cap % LEVEL_STEP)
        self.randomizer_seed = self.random.getrandbits(31)
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
                if difficulty * len(ACTS) + act < self.act_count:
                    add(acts[difficulty * len(ACTS) + act], quest_location_name(difficulty, name))
        if self.options.area_checks:
            for _level_id, act, name in AREAS:
                if act < self.act_count:
                    add(acts[act], area_location_name(name))
        if self.options.level_checks:
            cap = self.level_cap
            for level in range(LEVEL_STEP, cap + 1, LEVEL_STEP):
                # Spread the milestones over the acts in proportion to the level cap.
                add(acts[min(self.act_count - 1, (level - 1) * self.act_count // cap)], level_location_name(level))

        final = acts[-1]
        victory = D2RLocation(self.player, f"Defeat {BOSS_NAMES[self.options.goal_act.value - 1]}", None, final)
        victory.place_locked_item(D2RItem("Victory", ItemClassification.progression, None, self.player))
        final.locations.append(victory)
        self.multiworld.completion_condition[self.player] = lambda state: state.has("Victory", self.player)

    def create_item(self, name: str) -> D2RItem:
        if name == TRAP:
            classification = ItemClassification.trap
        elif name in USEFUL_LOOT:
            classification = ItemClassification.useful
        elif name in item_name_to_id and name not in self.item_name_groups[SKILLS_GROUP]:
            classification = ItemClassification.filler
        elif self.options.skills_per_act.value > 0:
            classification = ItemClassification.progression
        else:
            classification = ItemClassification.useful
        return D2RItem(name, classification, item_name_to_id[name], self.player)

    def create_items(self) -> None:
        if self.options.boss_quests_give_uniques:
            boss_names = [name for flag, _act, name in QUESTS if flag in BOSS_QUESTS]
            ours = {location.name for location in self.get_locations()}
            for difficulty in range(self.options.goal.value + 1):
                for name in boss_names:
                    if quest_location_name(difficulty, name) in ours:
                        location = self.get_location(quest_location_name(difficulty, name))
                        location.place_locked_item(self.create_item(BOSS_UNIQUE))

        allowed = {CLASSES[code] for code in CLASSES} if not self.options.skill_classes.value             else set(self.options.skill_classes.value)
        candidates = [skill_item_name(sid, name, cls) for sid, name, cls in SKILLS if CLASSES[cls] in allowed]
        location_count = len(self.multiworld.get_unfilled_locations(self.player))
        starting = min(self.options.starting_skills.value, len(candidates))
        size = min(self.options.skill_pool_size.value, len(candidates) - starting, location_count)
        # Logic needs every act's skill count to be reachable from the pool plus starting skills.
        size = max(size, min(self.required_skills(self.act_count - 1) - starting, len(candidates) - starting))
        if size > location_count:
            raise Exception(f"{self.player_name}: {self.required_skills(self.act_count - 1)} skills needed by "
                            f"skills_per_act don't fit in {location_count} locations; lower skills_per_act.")
        self.starting = self.pick_starting_skills(starting, candidates)
        rest = [name for name in candidates if name not in self.starting]
        self.skill_pool = self.random.sample(rest, min(size, len(rest)))
        for name in self.starting:
            self.multiworld.push_precollected(self.create_item(name))
        pool = [self.create_item(name) for name in self.skill_pool]
        empowerments = min(self.options.codex_empowerments.value, location_count - len(pool))
        pool += [self.create_item(EMPOWERMENT) for _ in range(empowerments)]

        remaining = location_count - len(pool)
        traps = remaining * self.options.junk_trap_percentage.value // 100
        pool += [self.create_item(TRAP) for _ in range(traps)]
        pool += [self.create_filler() for _ in range(remaining - traps)]
        self.multiworld.itempool += pool

    def class_name(self) -> str:
        """The chosen class's name ("Barbarian"), or "" for any."""
        if self.options.character_class.value == 0:
            return ""
        return list(CLASSES.values())[self.options.character_class.value - 1]

    def pick_starting_skills(self, count: int, candidates: list[str]) -> list[str]:
        """Starting skills: the chosen class's level-1 skills first, then the rest of its skills.
        With no class chosen, any skills from the pool."""
        own = self.class_name()
        if not own:
            return self.random.sample(candidates, count)
        starters = [skill_item_name(sid, name, cls) for sid, name, cls in SKILLS if CLASSES[cls] == own and sid in STARTER_SKILLS]
        others = [skill_item_name(sid, name, cls) for sid, name, cls in SKILLS if CLASSES[cls] == own and sid not in STARTER_SKILLS]
        self.random.shuffle(starters)
        self.random.shuffle(others)
        return (starters + others)[:count]

    def loot_weights(self) -> dict[str, float]:
        weights = {name: self.options.basic_loot_weight.value / len(BASIC_LOOT) for name in BASIC_LOOT}
        weights.update({name: self.options.unique_weight.value / len(UNIQUE_LOOT) for name in UNIQUE_LOOT})
        weights["Random Set Item"] = self.options.set_item_weight.value
        weights["High Rune"] = self.options.high_rune_weight.value
        weights["Socketed Base"] = self.options.socketed_base_weight.value
        if sum(weights.values()) == 0:
            return {name: 1 for name in BASIC_LOOT}
        return weights

    def get_filler_item_name(self) -> str:
        weights = self.loot_weights()
        return self.random.choices(list(weights), weights=list(weights.values()))[0]

    def fill_slot_data(self) -> dict[str, Any]:
        return {
            "goal": self.options.goal.value,
            "goal_act": self.options.goal_act.value,
            "character_class": self.class_name(),
            "goal_quest": BOSS_QUESTS[self.options.goal_act.value - 1],
            "area_checks": bool(self.options.area_checks),
            "level_checks": bool(self.options.level_checks),
            "level_step": LEVEL_STEP,
            "level_cap": self.level_cap,
            # d2r-ap-setup reads this and writes the matching game tables.
            "randomizer": {
                "seed": self.randomizer_seed,
                "monster_shuffle": self.options.monster_shuffle.value,
                "superunique_shuffle": self.options.superunique_shuffle.value,
                "runeword_shuffle": bool(self.options.runeword_shuffle),
                "xp_multiplier": self.options.experience_multiplier.value,
                "item_level_requirements": bool(self.options.item_level_requirements),
                "item_stat_requirements": bool(self.options.item_stat_requirements),
            },
        }
