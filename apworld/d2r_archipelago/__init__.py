from typing import Any, ClassVar

from BaseClasses import Item, ItemClassification, Location, LocationProgressType, Region, Tutorial
from Options import OptionError
from settings import Group, OptionalUserFolderPath
from worlds.AutoWorld import WebWorld, World
from worlds.generic.Rules import add_item_rule, add_rule

from . import client  # noqa: F401  (adds the Launcher button)
from .data import (AREAS, CLASSES, GEMS, QUEST_ITEMS, QUESTS, RUNES, SETS, SIDE_AREAS, SKILL_FORMS, SKILL_SLOTS, SKILL_USERS, SKILLS,
                   SPECIAL_ITEMS, STARTER_SKILLS, SUPERUNIQUES, UBERS)
from .ids import (ACTS, BASIC_LOOT, COW_CHECKS, CUBE_ACCESS, DIFFICULTIES, EMPOWERMENT, KEY_NAMES, KILL_COUNTS, LEVEL_CAPS,
                  LEVEL_STEP, LOCKS, MERCHANT_ACCESS, SET_FINDS, SKILL_POINT, TRAPS, UNIQUE_FINDS, UNIQUE_LOOT, act_key_name, area_location_name,
                  cow_location_name, find_count_location_name, find_location_name, item_name_to_id, kill_location_name,
                  level_location_name, location_name_to_id, quest_item_location_name, quest_location_name,
                  set_location_name, shop_location_name, skill_item_name, skill_names_by_class,
                  superunique_location_name)
from .options import OPTION_GROUPS, OPTION_PRESETS, D2ROptions, loader_settings

GAME = "Diablo II Resurrected"
SKILLS_GROUP = "Skills"
BOSS_UNIQUE = "Random Unique"
# Quest flags of the act boss kills: Andariel, Duriel, Mephisto, Diablo, Baal.
BOSS_QUESTS = (6, 14, 22, 26, 40)
BOSS_NAMES = ("Andariel", "Duriel", "Mephisto", "Diablo", "Baal")
# Act key name -> index of the act it unlocks (difficulty * 5 + act).
KEY_ACTS = {name: index for index, name in enumerate(KEY_NAMES, 1)}
USEFUL_LOOT = {BOSS_UNIQUE, *UNIQUE_LOOT, "Random Set Item", "High Rune", EMPOWERMENT, SKILL_POINT}
# With the Cube Lock, these need the Horadric Cube (the Horadric Staff and Khalim's Will), on every difficulty.
CUBE_QUESTS = ("The Horadric Staff", "The Seven Tombs", "Khalim's Will", "The Blackened Temple", "The Guardian")
CUBE_AREAS = ("Duriel's Lair", "Durance of Hate Level 2", "Durance of Hate Level 3")
# Level milestones above this take hours of grinding, so they only hold filler.
GRIND_LEVEL = 70
# Waypoint Lock covers Acts I to III, the acts you can leave backwards without a waypoint.
WAYPOINT_LOCK_ACTS = 3


def can_use(skill_id: int, cls: str) -> bool:
    """Whether a class (code) can use a skill: claw skills need the Assassin, the traps an S2 animation
    only the Barbarian and Assassin have, and so on (SKILL_USERS)."""
    return cls in SKILL_USERS.get(skill_id, (cls,))


def deal_skill_trees(random) -> dict[int, int]:
    """The skill tree shuffle: every unpinned class skill takes the tree slot of a random skill from the
    same row (so the same level) of any class whose tree it can be used in. Returns skill id -> id of the
    skill whose slot it takes."""
    slot_class = {skill_id: cls for skill_id, _name, cls in SKILLS}
    rows: dict[int, list[int]] = {}
    for skill_id, _page, row, _column, pinned in SKILL_SLOTS:
        if not pinned:
            rows.setdefault(row, []).append(skill_id)
    dealt = {}
    for row in sorted(rows):
        skills, slots = list(rows[row]), list(rows[row])
        random.shuffle(skills)
        random.shuffle(slots)
        # A matching of skills to slots their class can use. Every skill fits its own slot, so each one
        # finds a place, moving earlier skills along when it has to.
        holder: dict[int, int] = {}

        def place(skill: int, seen: set[int]) -> bool:
            for slot in slots:
                if slot not in seen and can_use(skill, slot_class[slot]):
                    seen.add(slot)
                    if slot not in holder or place(holder[slot], seen):
                        holder[slot] = skill
                        return True
            return False

        for skill in skills:
            place(skill, set())
        dealt.update({skill: slot for slot, skill in holder.items()})
    return dealt


class D2RItem(Item):
    game = GAME


class D2RLocation(Location):
    game = GAME
    act_index = 0


class D2RWeb(WebWorld):
    theme = "stone"
    option_groups = OPTION_GROUPS
    options_presets = OPTION_PRESETS
    tutorials = [Tutorial(
        "Multiworld Setup Guide",
        "Play Diablo II: Resurrected in a multiworld from the Archipelago Launcher.",
        "English",
        "setup_en.md",
        "setup/en",
        ["snrn"],
    )]


class D2RSettings(Group):
    class GameFolder(OptionalUserFolderPath):
        """The Diablo II: Resurrected folder, with D2R.exe. Leave it empty to find the Battle.net install."""

    game_folder: GameFolder = GameFolder("")


class D2RWorld(World):
    """Diablo II: Resurrected with every class skill shuffled into the multiworld.
    Skills you receive arrive as charms that grant them, whatever your class."""

    game = GAME
    web = D2RWeb()
    settings: ClassVar[D2RSettings]
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
        self.tree_mode = self.options.skill_mode == self.options.skill_mode.option_tree
        # Tree mode needs a class: with "any", the seed picks one (the Warlock only with Include Warlock).
        self.class_choice = self.options.character_class.value
        if self.tree_mode and self.class_choice == self.options.character_class.option_any:
            self.class_choice = self.random.choice([value for value in range(1, len(CLASSES) + 1)
                                                    if value != self.options.character_class.option_warlock
                                                    or self.options.include_warlock])
        self.skill_tree = deal_skill_trees(self.random) if self.options.skill_tree_shuffle else {}
        # Side Area Checks: each difficulty picks its own side areas, from the acts in play.
        self.side_areas: dict[int, list[tuple[int, int, str]]] = {}
        for difficulty in range(goal + 1):
            open_areas = [area for area in SIDE_AREAS if difficulty * len(ACTS) + area[1] < self.act_count]
            picked = self.random.sample(open_areas, min(self.options.side_area_checks.value, len(open_areas)))
            self.side_areas[difficulty] = sorted(picked)

    def key_fits(self, item: Item, act_index: int) -> bool:
        """Act Key Window: whether a D2 act key may sit at a location in act `act_index` of this world, by the
        window of the player whose key it is. An item-linked key goes to every linked player, so the
        tightest of their windows counts."""
        if item.game != GAME or item.name not in KEY_ACTS:
            return True
        group = self.multiworld.groups.get(item.player)
        owners = group["players"] if group else (item.player,)
        windows = [self.multiworld.worlds[owner].options.act_key_window for owner in owners]
        window = min(option.value for option in windows)
        return window == windows[0].range_end or act_index <= KEY_ACTS[item.name] - 1 + window

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
        cube = bool(self.options.cube_lock)
        for index in range(1, self.act_count):
            # The skill count is looked up when the rule runs: the caps are only known after create_items.
            key = act_key_name(index)
            # Act III comes after Duriel, who needs the Horadric Staff and so the Cube.
            needs_cube = cube and index % len(ACTS) == 2
            acts[index - 1].connect(acts[index], rule=lambda state, index=index, key=key, needs_cube=needs_cube:
                                    state.has_group(SKILLS_GROUP, self.player, self.required_skills(index))
                                    and (not keys or state.has(key, self.player))
                                    and (not needs_cube or state.has(CUBE_ACCESS, self.player)))

        def add(region: Region, name: str, filler_only: bool = False) -> None:
            location = D2RLocation(self.player, name, location_name_to_id[name], region)
            location.act_index = acts.index(region)
            add_item_rule(location, lambda item, act_index=location.act_index: self.key_fits(item, act_index))
            if filler_only:
                location.progress_type = LocationProgressType.EXCLUDED
            region.locations.append(location)

        for difficulty in range(self.options.goal.value + 1):
            for _flag, act, name in QUESTS:
                if difficulty * len(ACTS) + act < self.act_count:
                    add(acts[difficulty * len(ACTS) + act], quest_location_name(difficulty, name))
        if self.options.superunique_checks:
            for difficulty in range(self.options.goal.value + 1):
                for _superunique_id, act, _name, label in SUPERUNIQUES:
                    if difficulty * len(ACTS) + act < self.act_count:
                        add(acts[difficulty * len(ACTS) + act], superunique_location_name(difficulty, label))
        if self.options.cow_level_checks:
            # The cow portal opens once Baal is dead, so only difficulties finished before the goal.
            for difficulty in range(self.options.goal.value):
                for check in range(len(COW_CHECKS)):
                    # Killing The Cow King first closes the level for good, so the 100 cows can be missed.
                    add(acts[difficulty * len(ACTS) + 4], cow_location_name(difficulty, check),
                        filler_only=COW_CHECKS[check] == "Kill 100 Cows")
                    # The portal takes a Tome of Town Portal, and new characters only have a scroll.
                    if self.options.merchant_lock:
                        add_rule(acts[difficulty * len(ACTS) + 4].locations[-1],
                                 lambda state: state.has(MERCHANT_ACCESS, self.player))
        # Collections can take a long time (or luck), so they only hold filler.
        for _number, name in RUNES[:self.options.rune_checks.value]:
            add(acts[0], find_location_name(name), filler_only=True)
        if self.options.gem_checks:
            for name in GEMS:
                add(acts[0], find_location_name(name), filler_only=True)
        if self.options.set_checks:
            for name, _pieces in SETS:
                add(acts[0], set_location_name(name), filler_only=True)
        if self.options.special_item_checks and self.act_count == len(DIFFICULTIES) * len(ACTS):
            for name in SPECIAL_ITEMS:
                add(acts[-1], find_location_name(name), filler_only=True)
        in_play = range(self.options.goal.value + 1)
        if self.options.quest_item_checks:
            for difficulty in in_play:
                for act, every, name in QUEST_ITEMS:
                    if (every or difficulty == 0) and difficulty * len(ACTS) + act < self.act_count:
                        add(acts[difficulty * len(ACTS) + act], quest_item_location_name(difficulty, name))
        for act in range(min(len(ACTS), self.act_count)):
            for number in range(self.options.shop_checks.value):
                add(acts[act], shop_location_name(act, number))
                if self.shop_locked():
                    add_rule(acts[act].locations[-1], lambda state: state.has(MERCHANT_ACCESS, self.player))
        if self.options.uber_checks and self.act_count == len(DIFFICULTIES) * len(ACTS):
            for name in UBERS:
                add(acts[-1], f"Kill {name}", filler_only=True)
        if self.options.kill_count_checks:
            for index in range(self.act_count):
                for count in KILL_COUNTS:
                    add(acts[index], kill_location_name(index // len(ACTS), index % len(ACTS), count), filler_only=True)
        if self.options.find_count_checks:
            for count in UNIQUE_FINDS:
                add(acts[0], find_count_location_name(count, "Unique"), filler_only=True)
            for count in SET_FINDS:
                add(acts[0], find_count_location_name(count, "Set"), filler_only=True)
        if self.options.area_checks:
            for difficulty in range(self.options.goal.value + 1):
                for _level_id, act, name in AREAS:
                    if difficulty * len(ACTS) + act < self.act_count:
                        add(acts[difficulty * len(ACTS) + act], area_location_name(difficulty, name))
        for difficulty, areas in self.side_areas.items():
            for _level_id, act, name in areas:
                add(acts[difficulty * len(ACTS) + act], area_location_name(difficulty, name))
        if self.options.level_checks:
            cap = self.level_cap
            for level in range(LEVEL_STEP, cap + 1, LEVEL_STEP):
                # Spread the milestones over the acts in proportion to the level cap. The plugin holds
                # each one for its act's key the same way (ids::LevelAct).
                add(acts[min(self.act_count - 1, (level - 1) * self.act_count // cap)], level_location_name(level),
                    filler_only=level > GRIND_LEVEL)

        # Locations open before each act, not counting the boss quests' locked uniques.
        boss_locked = {quest_location_name(d, name) for d in range(3) for flag, _act, name in QUESTS if flag in BOSS_QUESTS} \
            if self.options.boss_quests_give_uniques else set()
        for index in range(1, self.act_count + 1):
            self.locations_before[index] = self.locations_before[index - 1] + sum(
                1 for location in acts[index - 1].locations
                if location.name not in boss_locked and location.progress_type != LocationProgressType.EXCLUDED)

        if cube:
            ours = {location.name: location for region in acts for location in region.locations}
            for difficulty in in_play:
                names = [quest_location_name(difficulty, quest) for quest in CUBE_QUESTS]
                names += [area_location_name(difficulty, area) for area in CUBE_AREAS]
                for name in names:
                    if name in ours:
                        add_rule(ours[name], lambda state: state.has(CUBE_ACCESS, self.player))

        final = acts[-1]
        victory = D2RLocation(self.player, f"Defeat {BOSS_NAMES[self.options.goal_act.value - 1]}", None, final)
        if cube and self.options.goal_act.value >= 2:
            add_rule(victory, lambda state: state.has(CUBE_ACCESS, self.player))
        victory.place_locked_item(D2RItem("Victory", ItemClassification.progression, None, self.player))
        final.locations.append(victory)
        self.multiworld.completion_condition[self.player] = lambda state: state.has("Victory", self.player)

    def create_item(self, name: str) -> D2RItem:
        if name in KEY_NAMES or (name == CUBE_ACCESS and self.options.cube_lock) or (name == MERCHANT_ACCESS and self.merchants_gate_checks()):
            classification = ItemClassification.progression
        elif name in LOCKS:
            classification = ItemClassification.useful
        elif name in TRAPS:
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
        if not self.options.include_warlock:
            # Warlock skills need the Reign of the Warlock expansion.
            allowed = (allowed - {"Warlock"}) or set(CLASSES.values()) - {"Warlock"}
        # Skills the class can't use stay out; with no class chosen, every skill that needs one does.
        own = self.class_code()
        candidates = [skill_item_name(sid, name, cls) for sid, name, cls in SKILLS
                      if CLASSES[cls] in allowed and (sid not in SKILL_USERS or (own and can_use(sid, own)))]
        if self.tree_mode:
            candidates = self.tree_skills()
        keys = [self.create_item(act_key_name(index)) for index in range(1, self.act_count)] if self.options.act_keys else []
        keys += [self.create_item(name) for name in self.lock_items()]
        unfilled = self.multiworld.get_unfilled_locations(self.player)
        # Keys, skills and empowerments can't go on filler-only locations.
        fillable = sum(1 for location in unfilled if location.progress_type != LocationProgressType.EXCLUDED)
        if fillable < len(keys):
            # A one-act game with every check off and all the locks on has more keys and locks than
            # places to put them; say so instead of failing deep in the pool maths.
            raise OptionError(f"[{GAME} - '{self.player_name}'] The seed has {fillable} locations that can hold a key or "
                              f"lock item but {len(keys)} such items. Turn on more checks (areas, side areas, levels, "
                              f"superuniques, quest items, shops) or fewer locks.")
        location_count = fillable - len(keys)
        filler_only = len(unfilled) - len(keys) - location_count
        starting = min(self.options.starting_skills.value, len(candidates))
        copies, distinct = self.options.skill_copies.value, self.options.skill_pool_size.value
        empowerments = self.options.codex_empowerments.value
        if self.tree_mode:
            # Every skill in the tree, whatever the pool options say.
            distinct, empowerments = len(candidates), 0
        elif self.options.scale_with_length:
            # The options are for a four-act game (Normal to Diablo).
            distinct = round(distinct * self.act_count / 4)
            empowerments = round(empowerments * self.act_count / 4)
        if self.options.skill_focus == self.options.skill_focus.option_few_but_deep and not self.tree_mode:
            copies, distinct = max(copies, 3), max(5, distinct // 3)
        distinct = min(distinct, len(candidates) - starting, location_count // copies)
        # Logic needs every act's skill count to be reachable from the pool plus starting skills.
        needed = self.required_skills(self.act_count - 1) - starting
        if distinct * copies < needed:
            distinct = min(len(candidates) - starting, location_count // copies, -(-needed // copies))
        self.starting = (self.pick_tree_starting_skills(starting, candidates) if self.tree_mode
                         else self.pick_starting_skills(starting, candidates))
        rest = [name for name in candidates if name not in self.starting]
        self.skill_pool = self.pick_skill_pool(rest, distinct)
        if not self.tree_mode:
            self.drop_formless_attacks(candidates)
        for name in self.starting:
            self.multiworld.push_precollected(self.create_item(name))
        # Logic never asks for more skills than the seed holds.
        self.skill_cap = len(self.starting) + len(self.skill_pool) * copies
        pool = [self.create_item(name) for name in self.skill_pool for _ in range(copies)]
        empowerments = min(empowerments, location_count - len(pool))
        # Skill Points are useful, so like skills they need locations that aren't filler-only.
        points = min(self.options.skill_points.value, location_count - len(pool) - empowerments) if self.tree_mode else 0
        location_count += len(keys) + filler_only
        pool = keys + pool
        pool += [self.create_item(EMPOWERMENT) for _ in range(empowerments)]
        pool += [self.create_item(SKILL_POINT) for _ in range(points)]

        remaining = location_count - len(pool)
        trap_weights = {name: getattr(self.options, option).value for name, option in TRAPS.items()}
        traps = remaining * self.options.trap_percentage.value // 100 if sum(trap_weights.values()) > 0 else 0
        pool += [self.create_item(name) for name in
                 self.random.choices(list(trap_weights), weights=list(trap_weights.values()), k=traps)]
        # Filler-only locations can't take useful loot (uniques, sets, high runes), so enough of the
        # filler has to be plain.
        plain = min(remaining - traps, max(0, filler_only - traps))
        pool += [self.create_item(self.pick_loot(plain_only=True)) for _ in range(plain)]
        pool += [self.create_filler() for _ in range(remaining - traps - plain)]
        self.multiworld.itempool += pool

    def shop_locked(self) -> bool:
        """Whether the shop checks wait for Merchant Access."""
        return bool(self.options.merchant_lock) and self.options.shop_checks.value > 0

    def merchants_gate_checks(self) -> bool:
        """Whether any check waits for Merchant Access: the shops, or the cow level (its portal takes a
        Tome of Town Portal) on a difficulty before the goal."""
        return self.shop_locked() or (bool(self.options.merchant_lock) and bool(self.options.cow_level_checks)
                                      and self.options.goal.value > 0)

    def lock_items(self) -> list[str]:
        """The lock items in the pool: one per lock that's on, and a Waypoints item per act in play up to
        Act III. Acts IV and V can only go back an act by waypoint, so their waypoints never lock: a check
        left behind in Act III or IV must stay reachable."""
        toggles = (self.options.cube_lock, self.options.town_portal_lock, self.options.potion_lock,
                   self.options.stash_lock, self.options.merchant_lock)
        names = [name for name, on in zip(LOCKS, toggles) if on]
        if self.options.waypoint_lock:
            names += [f"{act} Waypoints" for act in ACTS[:min(WAYPOINT_LOCK_ACTS, self.act_count)]]
        return names

    def class_name(self) -> str:
        """The chosen class's name ("Barbarian"), or "" for any."""
        if self.class_choice == 0:
            return ""
        return list(CLASSES.values())[self.class_choice - 1]

    def class_code(self) -> str:
        """The chosen class's code ("bar"), or "" for any."""
        return next((code for code, name in CLASSES.items() if name == self.class_name()), "")

    def drop_formless_attacks(self, candidates: list[str]) -> None:
        """Wolf and bear attacks (Feral Rage, Maul...) only work in their form, so they only go in when the
        skills hold a form they can use, and a starting attack only with a starting form. A starting attack
        without one trades places with its form or another pool skill; otherwise, and in the pool, another
        skill takes the attack's place."""
        id_of = {skill_item_name(sid, name, cls): sid for sid, name, cls in SKILLS}
        held = self.starting + self.skill_pool
        spare = [name for name in candidates if name not in held and id_of[name] not in SKILL_FORMS]
        self.random.shuffle(spare)

        def formless(name: str, forms_from: list[str]) -> bool:
            need = SKILL_FORMS.get(id_of[name])
            return bool(need) and not {id_of[form] for form in forms_from} & set(need)

        for name in list(self.starting):
            if not formless(name, self.starting):
                continue
            index = self.starting.index(name)
            forms = [held for held in self.skill_pool if id_of[held] in SKILL_FORMS[id_of[name]]]
            others = [held for held in self.skill_pool if id_of[held] not in SKILL_FORMS]
            swap = forms[0] if forms else None if spare else self.random.choice(others) if others else None
            if swap:
                self.starting[index], self.skill_pool[self.skill_pool.index(swap)] = swap, name
            elif spare:
                self.starting[index] = spare.pop()
            else:
                self.starting.remove(name)
        for name in list(self.skill_pool):
            if formless(name, self.starting + self.skill_pool):
                if spare:
                    self.skill_pool[self.skill_pool.index(name)] = spare.pop()
                else:
                    self.skill_pool.remove(name)

    def tree_skills(self) -> list[str]:
        """The skills in the class's tree (after the tree shuffle), as item names."""
        code = next(code for code, name in CLASSES.items() if name == self.class_name())
        slot_class = {skill_id: cls for skill_id, _name, cls in SKILLS}
        return [skill_item_name(skill_id, name, cls) for skill_id, name, cls in SKILLS
                if slot_class[self.skill_tree.get(skill_id, skill_id)] == code]

    def pick_tree_starting_skills(self, count: int, candidates: list[str]) -> list[str]:
        """Tree mode: starting skills from the tree's top row, active level-1 skills first."""
        row_of = {skill_id: row for skill_id, _page, row, _column, _pinned in SKILL_SLOTS}
        id_of = {skill_item_name(skill_id, name, cls): skill_id for skill_id, name, cls in SKILLS}
        top = [name for name in candidates if row_of.get(self.skill_tree.get(id_of[name], id_of[name])) == 1]
        active = [name for name in top if id_of[name] in STARTER_SKILLS]
        rest = [name for name in candidates if name not in active]
        self.random.shuffle(active)
        self.random.shuffle(rest)
        return (active + rest)[:count]

    def pick_starting_skills(self, count: int, candidates: list[str]) -> list[str]:
        """Starting skills: the chosen class's level-1 skills first, then the rest of its skills.
        With no class chosen, any skills from the pool."""
        own = self.class_name()
        if own == "Warlock" and not self.options.include_warlock:
            own = ""
        if not own:
            return self.random.sample(candidates, count)
        starters = [skill_item_name(sid, name, cls) for sid, name, cls in SKILLS if CLASSES[cls] == own and sid in STARTER_SKILLS]
        others = [skill_item_name(sid, name, cls) for sid, name, cls in SKILLS if CLASSES[cls] == own and sid not in STARTER_SKILLS]
        self.random.shuffle(starters)
        self.random.shuffle(others)
        return (starters + others)[:count]

    def pick_skill_pool(self, rest: list[str], count: int) -> list[str]:
        """The pool's distinct skills. class_weighted takes half from the chosen class, and when either half
        runs out, the other one makes up the difference."""
        own = self.class_name()
        if self.options.skill_focus != self.options.skill_focus.option_class_weighted or not own or self.tree_mode:
            return self.random.sample(rest, count)
        mine = [name for name in rest if name in self.item_name_groups[own]]
        others = [name for name in rest if name not in self.item_name_groups[own]]
        from_others = min(len(others), count - min(len(mine), count // 2))
        from_class = min(len(mine), count - from_others)
        return self.random.sample(mine, from_class) + self.random.sample(others, from_others)

    def loot_weights(self) -> dict[str, float]:
        weights = {name: self.options.basic_loot_weight.value / len(BASIC_LOOT) for name in BASIC_LOOT}
        weights.update({name: self.options.unique_weight.value / len(UNIQUE_LOOT) for name in UNIQUE_LOOT})
        weights["Random Set Item"] = self.options.set_item_weight.value
        weights["High Rune"] = self.options.high_rune_weight.value
        weights["Socketed Base"] = self.options.socketed_base_weight.value
        weights["Gold"] = self.options.gold_weight.value
        weights["Experience"] = self.options.experience_weight.value
        weights["Random Charm"] = self.options.charm_weight.value * 0.6
        weights["Random Jewel"] = self.options.charm_weight.value * 0.4
        weights["Random Shrine"] = self.options.shrine_weight.value
        weights["Prophecy"] = self.options.prophecy_weight.value
        if sum(weights.values()) == 0:
            return {name: 1 for name in BASIC_LOOT}
        return weights

    def pick_loot(self, plain_only: bool = False) -> str:
        weights = self.loot_weights()
        if plain_only:
            weights = {name: weight for name, weight in weights.items() if name not in USEFUL_LOOT}
            if sum(weights.values()) == 0:
                weights = {name: 1 for name in BASIC_LOOT}
        return self.random.choices(list(weights), weights=list(weights.values()))[0]

    def get_filler_item_name(self) -> str:
        return self.pick_loot()

    def hint_list(self) -> list[list[int]]:
        """Where this player's important items ended up, as [location id, finding player, item id], for the hints
        found in game: act keys in act order, then progression locks, other progression and useful items."""
        keys, locks, progression, useful = [], [], [], []
        for location in self.multiworld.get_locations():
            item = location.item
            if item is None or item.player != self.player or location.address is None or item.code is None:
                continue
            entry = [location.address, location.player, item.code]
            if item.name in KEY_NAMES:
                keys.append((KEY_NAMES.index(item.name), entry))
            elif item.name in LOCKS and item.advancement:
                locks.append(entry)
            elif item.advancement:
                progression.append(entry)
            elif item.useful and not location.locked:
                useful.append(entry)
        for group in (locks, progression, useful):
            self.random.shuffle(group)
        return ([entry for _index, entry in sorted(keys)] + locks + progression + useful)[:200]

    def fill_slot_data(self) -> dict[str, Any]:
        return {
            # Hints found in game reveal these in order (see hint_list), and hint_balance of them point at other
            # players' items in this world instead.
            "hints": self.hint_list(),
            "hint_sources": {
                "cain": bool(self.options.cain_hints),
                "page_chance": self.options.page_hint_chance.value,
                "prophecy_shop": bool(self.options.prophecy_shop),
            },
            "hint_balance": self.options.hint_balance.value,
            # The plugin warns when its version doesn't match the world that made the seed.
            "world_version": ".".join(str(part) for part in self.world_version),
            "goal": self.options.goal.value,
            "goal_act": self.options.goal_act.value,
            "character_class": self.class_name(),
            "skill_mode": "tree" if self.tree_mode else "codex",
            "act_keys": bool(self.options.act_keys),
            "skill_scaling": self.options.skill_level_scaling.value,
            "skill_scaling_cap": self.options.skill_scaling_cap.value,
            "goal_quest": BOSS_QUESTS[self.options.goal_act.value - 1],
            "area_checks": bool(self.options.area_checks),
            # The side areas this seed picked, as location ids; the plugin only sends these.
            "side_areas": sorted(location_name_to_id[area_location_name(difficulty, name)]
                                 for difficulty, areas in self.side_areas.items() for _level_id, _act, name in areas),
            "level_checks": bool(self.options.level_checks),
            "level_step": LEVEL_STEP,
            "level_cap": self.level_cap,
            "superunique_checks": bool(self.options.superunique_checks),
            "cow_level_checks": bool(self.options.cow_level_checks),
            "rune_checks": self.options.rune_checks.value,
            "gem_checks": bool(self.options.gem_checks),
            "set_checks": bool(self.options.set_checks),
            "special_item_checks": bool(self.options.special_item_checks) and self.act_count == len(DIFFICULTIES) * len(ACTS),
            "quest_item_checks": bool(self.options.quest_item_checks),
            "shop_checks": self.options.shop_checks.value,
            "uber_checks": bool(self.options.uber_checks) and self.act_count == len(DIFFICULTIES) * len(ACTS),
            "kill_count_checks": bool(self.options.kill_count_checks),
            "find_count_checks": bool(self.options.find_count_checks),
            "enforce_act_keys": bool(self.options.enforce_act_keys),
            # The plugin blocks each of these until its item arrives.
            "locks": {
                "cube": bool(self.options.cube_lock),
                "town_portal": bool(self.options.town_portal_lock),
                "potions": bool(self.options.potion_lock),
                "stash": bool(self.options.stash_lock),
                "merchants": bool(self.options.merchant_lock),
                "waypoints": bool(self.options.waypoint_lock),
            },
            "traps": {
                "wait_for_town": bool(self.options.traps_wait_for_town),
                "ambush_size": self.options.ambush_size.value,
                "ambush_monsters": self.options.ambush_monsters.current_key,
                "tax_per_level": self.options.tax_collector_gold.value,
                "junk_size": self.options.junk_backpack_size.value,
            },
            "death_link": bool(self.options.death_link),
            # d2r-ap-setup writes these into the mod's d2rloader.toml, overriding the player's D2RLoader settings.
            "loader_settings": loader_settings(self.options),
            # d2r-ap-setup reads this and writes the matching game tables.
            "randomizer": {
                "seed": self.randomizer_seed,
                "monster_shuffle": self.options.monster_shuffle.value,
                "superunique_shuffle": self.options.superunique_shuffle.value,
                "runeword_shuffle": bool(self.options.runeword_shuffle),
                # Skill tree shuffle: [skill id, id of the skill whose tree slot it takes], for moved skills.
                "skill_tree": sorted([skill, slot] for skill, slot in self.skill_tree.items() if skill != slot),
                # Tree mode keeps the trees' level requirements (the Codex needs them at 0).
                "tree_mode": self.tree_mode,
                "xp_multiplier": self.options.experience_multiplier.value,
                "item_level_requirements": bool(self.options.item_level_requirements),
                "item_stat_requirements": bool(self.options.item_stat_requirements),
                "monster_density": self.options.monster_density.value,
                "champion_packs": self.options.champion_packs.value,
                "better_drops": self.options.better_drops.value,
            },
        }
