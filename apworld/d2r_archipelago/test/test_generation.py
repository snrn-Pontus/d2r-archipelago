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
        # Two starting skills cover Act II (2 needed) but not Act III (4 needed).
        self.assertTrue(self.can_reach_region("Normal Act II"))
        self.assertFalse(self.can_reach_region("Normal Act III"))
        self.collect(self.world.create_item(self.world.skill_pool[0]))
        self.collect(self.world.create_item(self.world.skill_pool[1]))
        self.assertTrue(self.can_reach_region("Normal Act III"))


class TestHell(D2RTestBase):
    options = {"goal": "hell", "skills_per_act": 2, "skill_pool_size": 10}

    def test_pool_grows_to_satisfy_logic(self) -> None:
        skills = [item for item in self.multiworld.get_items() if item.name in self.world.item_name_groups["Skills"]]
        self.assertGreaterEqual(len(skills) + 2, 28)


class TestSingleClass(D2RTestBase):
    options = {"skill_classes": ["Warlock"], "skills_per_act": 0, "skill_pool_size": 30}

    def test_only_warlock_skills(self) -> None:
        warlock = self.world.item_name_groups["Warlock"]
        for item in self.multiworld.get_items():
            if item.name in self.world.item_name_groups["Skills"]:
                self.assertIn(item.name, warlock)


class TestLoot(D2RTestBase):
    options = {"goal": "nightmare", "codex_empowerments": 4, "junk_trap_percentage": 20}

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
