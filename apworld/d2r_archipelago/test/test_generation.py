from . import D2RTestBase
from ..ids import ITEM_BASE, LOCATION_BASE


class TestDefaults(D2RTestBase):
    def test_pool_matches_locations(self) -> None:
        locations = [loc for loc in self.multiworld.get_locations(self.player) if loc.address is not None]
        items = [item for item in self.multiworld.get_items() if item.code is not None]
        self.assertEqual(len(locations), len(items))

    def test_ids_follow_plugin_scheme(self) -> None:
        world = self.world
        self.assertEqual(world.location_name_to_id["Normal: Den of Evil"], LOCATION_BASE + 1)
        self.assertEqual(world.location_name_to_id["Hell: Eve of Destruction"], LOCATION_BASE + 240)
        self.assertEqual(world.location_name_to_id["Enter Tristram"], LOCATION_BASE + 1038)
        self.assertEqual(world.location_name_to_id["Reach Character Level 30"], LOCATION_BASE + 2030)
        self.assertEqual(world.item_name_to_id["Teleport (Sorceress)"], ITEM_BASE + 54)

    def test_starting_skills_precollected(self) -> None:
        self.assertEqual(len(self.multiworld.precollected_items[self.player]), 2)

    def test_act_gate(self) -> None:
        # Act II needs its key (and 1 skill, which the 2 starting skills cover).
        self.assertFalse(self.can_reach_region("Normal Act II"))
        self.collect_by_name("Normal Act II Key")
        self.assertTrue(self.can_reach_region("Normal Act II"))
        self.assertFalse(self.can_reach_region("Normal Act III"))
        self.collect_by_name("Normal Act III Key")
        self.assertTrue(self.can_reach_region("Normal Act III"))

    def test_keys_in_pool(self) -> None:
        names = [item.name for item in self.multiworld.itempool]
        for key in ("Normal Act II Key", "Normal Act III Key", "Normal Act IV Key"):
            self.assertEqual(names.count(key), 1)
        self.assertNotIn("Normal Act V Key", names)


class TestHell(D2RTestBase):
    options = {"goal": "hell", "goal_act": 5, "skills_per_act": 2, "skill_pool_size": 10}

    def test_pool_grows_to_satisfy_logic(self) -> None:
        skills = [item for item in self.multiworld.get_items() if item.name in self.world.item_name_groups["Skills"]]
        self.assertGreaterEqual(len(skills) + 2, 28)


class TestNoWarlockByDefault(D2RTestBase):
    def test_no_warlock_skills(self) -> None:
        warlock = self.world.item_name_groups["Warlock"]
        for item in [*self.multiworld.itempool, *self.multiworld.precollected_items[self.player]]:
            self.assertNotIn(item.name, warlock)


class TestSingleClass(D2RTestBase):
    options = {"skill_classes": ["Warlock"], "include_warlock": True, "skills_per_act": 0, "skill_pool_size": 30}

    def test_only_warlock_skills(self) -> None:
        warlock = self.world.item_name_groups["Warlock"]
        for item in self.multiworld.get_items():
            if item.name in self.world.item_name_groups["Skills"]:
                self.assertIn(item.name, warlock)


class TestLoot(D2RTestBase):
    options = {"goal": "nightmare", "goal_act": 5, "codex_empowerments": 4, "junk_trap_percentage": 20}

    def test_boss_quests_hold_uniques(self) -> None:
        for difficulty in ("Normal", "Nightmare"):
            for quest in ("Sisters to the Slaughter", "The Seven Tombs", "The Guardian", "Terror's End",
                          "Eve of Destruction"):
                item = self.multiworld.get_location(f"{difficulty}: {quest}", self.player).item
                self.assertEqual(item.name, "Random Unique")

    def test_empowerments_and_traps(self) -> None:
        names = [item.name for item in self.multiworld.itempool if item.player == self.player]
        self.assertEqual(names.count("Codex Empowerment"), 4)
        self.assertGreater(names.count("Junk Backpack"), 0)

    def test_pool_matches_locations(self) -> None:
        locations = [loc for loc in self.multiworld.get_locations(self.player) if loc.address is not None]
        items = [item for item in self.multiworld.get_items() if item.code is not None]
        self.assertEqual(len(locations), len(items))


class TestOnlyBasicLoot(D2RTestBase):
    options = {"unique_weight": 0, "set_item_weight": 0, "high_rune_weight": 0, "socketed_base_weight": 0,
               "basic_loot_weight": 0, "boss_quests_give_uniques": False}

    def test_falls_back_to_basic_loot(self) -> None:
        from ..ids import BASIC_LOOT
        for item in self.multiworld.itempool:
            if item.classification == item.classification.filler:
                self.assertIn(item.name, BASIC_LOOT)


class TestBarbarianStart(D2RTestBase):
    options = {"character_class": "barbarian", "starting_skills": 3}

    def test_starting_skills_are_level_one_barbarian_skills(self) -> None:
        from ..data import SKILLS, STARTER_SKILLS
        barbarian = {f"{name} (Barbarian)" for sid, name, cls in SKILLS if cls == "bar" and sid in STARTER_SKILLS}
        starting = [item.name for item in self.multiworld.precollected_items[self.player]]
        self.assertEqual(len(starting), 3)
        for name in starting:
            self.assertIn(name, barbarian)
        self.assertEqual(self.world.fill_slot_data()["character_class"], "Barbarian")

    def test_starting_skills_not_in_pool(self) -> None:
        starting = {item.name for item in self.multiworld.precollected_items[self.player]}
        self.assertFalse(starting & {item.name for item in self.multiworld.itempool})


class TestSkillCopies(D2RTestBase):
    options = {"skill_copies": 3, "skill_pool_size": 10}

    def test_each_skill_three_times(self) -> None:
        skills = [item.name for item in self.multiworld.itempool if item.name in self.world.item_name_groups["Skills"]]
        self.assertEqual(len(skills), 30)
        self.assertTrue(all(skills.count(name) == 3 for name in skills))


class TestFewButDeep(D2RTestBase):
    options = {"skill_focus": "few_but_deep", "skill_pool_size": 30}

    def test_ten_skills_three_copies(self) -> None:
        skills = [item.name for item in self.multiworld.itempool if item.name in self.world.item_name_groups["Skills"]]
        self.assertEqual(len(set(skills)), 10)
        self.assertEqual(len(skills), 30)


class TestClassWeighted(D2RTestBase):
    options = {"skill_focus": "class_weighted", "character_class": "sorceress", "skill_pool_size": 20}

    def test_half_from_class(self) -> None:
        sorceress = self.world.item_name_groups["Sorceress"]
        self.assertEqual(sum(1 for name in self.world.skill_pool if name in sorceress), 10)
        self.assertEqual(self.world.fill_slot_data()["skill_scaling"], 3)


class TestShortGame(D2RTestBase):
    options = {"goal": "normal", "goal_act": 2}

    def test_only_first_two_acts(self) -> None:
        names = {location.name for location in self.multiworld.get_locations(self.player)}
        self.assertIn("Normal: The Seven Tombs", names)
        self.assertNotIn("Normal: Lam Esen's Tome", names)
        self.assertIn("Enter Lut Gholein", names)
        self.assertNotIn("Enter Kurast Docktown", names)
        self.assertIn("Reach Character Level 10", names)
        self.assertNotIn("Reach Character Level 15", names)
        self.assertIn("Defeat Duriel", names)

    def test_slot_data(self) -> None:
        data = self.world.fill_slot_data()
        self.assertEqual(data["goal_quest"], 14)
        self.assertEqual(data["level_cap"], 10)
        self.assertEqual(data["randomizer"]["monster_shuffle"], 1)


class TestAct1Only(D2RTestBase):
    options = {"goal": "normal", "goal_act": 1, "level_checks": False}

    def test_generates(self) -> None:
        self.assertIn("Defeat Andariel", {location.name for location in self.multiworld.get_locations(self.player)})


class TestHellAct3(D2RTestBase):
    options = {"goal": "hell", "goal_act": 3}

    def test_level_cap_between_nightmare_and_hell(self) -> None:
        self.assertEqual(self.world.level_cap, 75)
        self.assertEqual(self.world.fill_slot_data()["goal_quest"], 22)
