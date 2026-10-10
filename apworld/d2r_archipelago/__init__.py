from typing import Any

from BaseClasses import Item, ItemClassification, Location, Region, Tutorial
from worlds.AutoWorld import WebWorld, World

from .data import AREAS, CLASSES, QUESTS, SKILLS, STARTER_SKILLS
from .ids import (ACTS, BASIC_LOOT, DIFFICULTIES, EMPOWERMENT, KEY_NAMES, LEVEL_CAPS, LEVEL_STEP, TRAP, UNIQUE_LOOT, act_key_name,
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
    item_name_groups = {SKILLS_GROUP: set().union(*skill_names_by_class.values()), **skill_names_by_class, "Act Keys": set(KEY_NAMES)}

    def generate_early(self) -> None:
        goal, goal_act = self.options.goal.value, self.options.goal_act.value
        # Acts in play, counting every act of the easier difficulties.
        self.act_count = goal * len(ACTS) + goal_act
        # The level cap grows with the acts in play, from the previous difficulty's cap.
        previous = LEVEL_CAPS[goal - 1] if goal > 0 else 0
        cap = previous + (LEVEL_CAPS[goal] - previous) * goal_act // len(ACTS)
        self.level_cap = max(LEVEL_STEP, cap - cap % LEVEL_STEP)
        self.randomizer_seed = self.random.getrandbits(31)
        # Logic caps, filled in by create_regions and create_items.
        self.locations_before: list[int] = [0] * (self.act_count + 1)
        self.skill_cap = 1 << 30
        self.skill_pool: list[str] = []
        self.starting: list[str] = []

    def act_region_name(self, index: int) -> str:
        return f"{DIFFICULTIES[index // len(ACTS)]} {ACTS[index % len(ACTS)]}"

    def required_skills(self, index: int) -> int:
        """Skills logic expects before act `index`: skills_per_act each, but never more skills than the
        seed has, nor more than half the locations in the earlier acts (the rest hold keys and loot)."""
        return min(index * self.options.skills_per_act.value, self.skill_cap, self.locations_before[index] // 2)

    def create_regions(self) -> None:
        menu = Region("Menu", self.player, self.multiworld)
        self.multiworld.regions.append(menu)
        acts = []
        for index in range(self.act_count):
            region = Region(self.act_region_name(index), self.player, self.multiworld)
            self.multiworld.regions.append(region)
            acts.append(region)
        menu.connect(acts[0])
        keys = bool(self.options.act_keys)
        for index in range(1, self.act_count):
            # The skill count is looked up when the rule runs: the caps are only known after create_items.
            key = act_key_name(index)
            acts[index - 1].connect(acts[index], rule=lambda state, index=index, key=key:
                                    state.has_group(SKILLS_GROUP, self.player, self.required_skills(index))
                                    and (not keys or state.has(key, self.player)))

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

        # Locations open before each act, not counting the boss quests' locked uniques.
        boss_locked = {quest_location_name(d, name) for d in range(3) for flag, _act, name in QUESTS if flag in BOSS_QUESTS} \
            if self.options.boss_quests_give_uniques else set()
        for index in range(1, self.act_count + 1):
            self.locations_before[index] = self.locations_before[index - 1] + sum(
                1 for location in acts[index - 1].locations if location.name not in boss_locked)

        final = acts[-1]
        victory = D2RLocation(self.player, f"Defeat {BOSS_NAMES[self.options.goal_act.value - 1]}", None, final)
        victory.place_locked_item(D2RItem("Victory", ItemClassification.progression, None, self.player))
        final.locations.append(victory)
        self.multiworld.completion_condition[self.player] = lambda state: state.has("Victory", self.player)

    def create_item(self, name: str) -> D2RItem:
        if name in KEY_NAMES:
            classification = ItemClassification.progression
        elif name == TRAP:
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

        allowed = set(self.options.skill_classes.value) or set(CLASSES.values())
        candidates = [skill_item_name(sid, name, cls) for sid, name, cls in SKILLS if CLASSES[cls] in allowed]
        keys = [self.create_item(act_key_name(index)) for index in range(1, self.act_count)] if self.options.act_keys else []
        location_count = len(self.multiworld.get_unfilled_locations(self.player)) - len(keys)
        starting = min(self.options.starting_skills.value, len(candidates))
        copies, distinct = self.options.skill_copies.value, self.options.skill_pool_size.value
        if self.options.skill_focus == self.options.skill_focus.option_few_but_deep:
            copies, distinct = max(copies, 3), max(5, distinct // 3)
        distinct = min(distinct, len(candidates) - starting, location_count // copies)
        # Logic needs every act's skill count to be reachable from the pool plus starting skills.
        needed = self.required_skills(self.act_count - 1) - starting
        if distinct * copies < needed:
            distinct = min(len(candidates) - starting, location_count // copies, -(-needed // copies))
        self.starting = self.pick_starting_skills(starting, candidates)
        rest = [name for name in candidates if name not in self.starting]
        self.skill_pool = self.pick_skill_pool(rest, distinct)
        for name in self.starting:
            self.multiworld.push_precollected(self.create_item(name))
        # Logic never asks for more skills than the seed holds.
        self.skill_cap = len(self.starting) + len(self.skill_pool) * copies
        pool = [self.create_item(name) for name in self.skill_pool for _ in range(copies)]
        empowerments = min(self.options.codex_empowerments.value, location_count - len(pool))
        location_count += len(keys)
        pool = keys + pool
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

    def pick_skill_pool(self, rest: list[str], count: int) -> list[str]:
        """The pool's distinct skills. class_weighted takes half from the chosen class."""
        own = self.class_name()
        if self.options.skill_focus != self.options.skill_focus.option_class_weighted or not own:
            return self.random.sample(rest, count)
        mine = [name for name in rest if name in self.item_name_groups[own]]
        others = [name for name in rest if name not in self.item_name_groups[own]]
        from_class = min(len(mine), count // 2)
        return self.random.sample(mine, from_class) + self.random.sample(others, min(len(others), count - from_class))

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
            # The plugin warns when its version doesn't match the world that made the seed.
            "world_version": ".".join(str(part) for part in self.world_version),
            "goal": self.options.goal.value,
            "goal_act": self.options.goal_act.value,
            "character_class": self.class_name(),
            "act_keys": bool(self.options.act_keys),
            "skill_scaling": self.options.skill_level_scaling.value,
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
                "monster_density": self.options.monster_density.value,
                "champion_packs": self.options.champion_packs.value,
                "better_drops": self.options.better_drops.value,
            },
        }
