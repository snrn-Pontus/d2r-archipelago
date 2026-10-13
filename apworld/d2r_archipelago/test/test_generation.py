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
        self.assertEqual(world.location_name_to_id["Normal: Enter Tristram"], LOCATION_BASE + 1038)
        self.assertEqual(world.location_name_to_id["Hell: Enter The Worldstone Chamber"], LOCATION_BASE + 1532)
        self.assertEqual(world.location_name_to_id["Reach Character Level 30"], LOCATION_BASE + 2030)
        self.assertEqual(world.location_name_to_id["Normal: Kill Bishibosh (Cold Plains)"], LOCATION_BASE + 3000)
        self.assertEqual(world.location_name_to_id["Hell: Kill Frozenstein (Frozen River)"], LOCATION_BASE + 3259)
        self.assertEqual(world.location_name_to_id["Find Zod Rune"], LOCATION_BASE + 5033)
        self.assertEqual(world.location_name_to_id["Find Perfect Skull"], LOCATION_BASE + 5134)
        self.assertEqual(world.location_name_to_id["Complete Civerb's Vestments"], LOCATION_BASE + 5200)
        self.assertEqual(world.location_name_to_id["Find Annihilus"], LOCATION_BASE + 5312)
        self.assertEqual(world.location_name_to_id["Nightmare: Kill 100 Cows"], LOCATION_BASE + 5412)
        self.assertEqual(world.item_name_to_id["Random Jewel"], ITEM_BASE + 1015)
        self.assertEqual(world.item_name_to_id["Act V Waypoints"], ITEM_BASE + 989)
        self.assertEqual(world.location_name_to_id["Act V: Buy Archipelago Parcel 5"], LOCATION_BASE + 5544)
        self.assertEqual(world.location_name_to_id["Kill Uber Baal"], LOCATION_BASE + 5605)
        self.assertEqual(world.location_name_to_id["Find 50 Unique Items"], LOCATION_BASE + 5804)
        self.assertEqual(world.location_name_to_id["Find 1 Set Item"], LOCATION_BASE + 5810)
        self.assertEqual(world.location_name_to_id["Hell: Find Scroll of Resistance"], LOCATION_BASE + 5954)
        self.assertEqual(world.location_name_to_id["Hell: Kill 500 Monsters in Act V"], LOCATION_BASE + 6242)
        self.assertNotIn("Nightmare: Find Horadric Cube", world.location_name_to_id)
        self.assertEqual(world.item_name_to_id["Teleport (Sorceress)"], ITEM_BASE + 54)

    def test_default_quest_items_and_shops(self) -> None:
        names = {location.name for location in self.multiworld.get_locations(self.player)}
        self.assertIn("Normal: Find Horadric Cube", names)
        self.assertIn("Act IV: Buy Archipelago Parcel 3", names)
        self.assertNotIn("Act IV: Buy Archipelago Parcel 4", names)
        self.assertNotIn("Act V: Buy Archipelago Parcel 1", names)
        self.assertFalse(any("Waypoints" in item.name or "Access" in item.name for item in self.multiworld.itempool))

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
    options = {"goal": "nightmare", "goal_act": 5, "codex_empowerments": 4, "trap_percentage": 20,
               "scale_with_length": False}

    def test_boss_quests_hold_uniques(self) -> None:
        for difficulty in ("Normal", "Nightmare"):
            for quest in ("Sisters to the Slaughter", "The Seven Tombs", "The Guardian", "Terror's End",
                          "Eve of Destruction"):
                item = self.multiworld.get_location(f"{difficulty}: {quest}", self.player).item
                self.assertEqual(item.name, "Random Unique")

    def test_empowerments_and_traps(self) -> None:
        names = [item.name for item in self.multiworld.itempool if item.player == self.player]
        self.assertEqual(names.count("Codex Empowerment"), 4)
        traps = [name for name in names if name == "Junk Backpack" or name.endswith(" Trap")]
        self.assertGreater(len(traps), 0)
        self.assertNotIn("Mercenary Down Trap", traps)

    def test_pool_matches_locations(self) -> None:
        locations = [loc for loc in self.multiworld.get_locations(self.player) if loc.address is not None]
        items = [item for item in self.multiworld.get_items() if item.code is not None]
        self.assertEqual(len(locations), len(items))


class TestOnlyBasicLoot(D2RTestBase):
    options = {"unique_weight": 0, "set_item_weight": 0, "high_rune_weight": 0, "socketed_base_weight": 0,
               "gold_weight": 0, "experience_weight": 0, "charm_weight": 0, "shrine_weight": 0, "trap_percentage": 0,
               "prophecy_weight": 0,
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
        self.assertEqual(self.world.fill_slot_data()["skill_scaling"], 5)
        self.assertEqual(self.world.fill_slot_data()["skill_scaling_cap"], 10)


class TestShortGame(D2RTestBase):
    options = {"goal": "normal", "goal_act": 2}

    def test_only_first_two_acts(self) -> None:
        names = {location.name for location in self.multiworld.get_locations(self.player)}
        self.assertIn("Normal: The Seven Tombs", names)
        self.assertNotIn("Normal: Lam Esen's Tome", names)
        self.assertIn("Normal: Enter Lut Gholein", names)
        self.assertNotIn("Normal: Enter Kurast Docktown", names)
        self.assertNotIn("Nightmare: Enter Cold Plains", names)
        self.assertIn("Reach Character Level 10", names)
        self.assertNotIn("Reach Character Level 15", names)
        self.assertIn("Defeat Duriel", names)
        self.assertIn("Normal: Kill Ancient Kaa the Soulless (Tal Rasha's Tomb)", names)
        self.assertNotIn("Normal: Kill Stormtree (Flayer Jungle)", names)
        self.assertNotIn("Nightmare: Kill Bishibosh (Cold Plains)", names)

    def test_slot_data(self) -> None:
        data = self.world.fill_slot_data()
        self.assertEqual(data["goal_quest"], 14)
        self.assertEqual(data["level_cap"], 10)
        self.assertEqual(data["randomizer"]["monster_shuffle"], 1)
        self.assertTrue(data["superunique_checks"])
        self.assertFalse(data["death_link"])
        self.assertEqual(data["hint_sources"], {"cain": True, "page_chance": 25, "prophecy_shop": True})

    def test_hint_list(self) -> None:
        from Fill import distribute_items_restrictive
        distribute_items_restrictive(self.multiworld)
        # Hints: this player's items, act keys first, in act order.
        hints = self.world.fill_slot_data()["hints"]
        self.assertGreater(len(hints), 0)
        self.assertEqual(hints[0][2], self.world.item_name_to_id["Normal Act II Key"])
        for location_id, player, item_id in hints:
            location = self.multiworld.get_location(
                self.multiworld.worlds[player].location_id_to_name[location_id], player)
            self.assertEqual((location.item.player, location.item.code), (self.player, item_id))


class TestNoSuperUniqueChecks(D2RTestBase):
    options = {"superunique_checks": False, "death_link": True}

    def test_no_superunique_locations(self) -> None:
        names = {location.name for location in self.multiworld.get_locations(self.player)}
        self.assertFalse(any(": Kill " in name for name in names))
        data = self.world.fill_slot_data()
        self.assertFalse(data["superunique_checks"])
        self.assertTrue(data["death_link"])


class TestHellSuperUniques(D2RTestBase):
    options = {"goal": "hell", "goal_act": 5, "skill_pool_size": 40}

    def test_every_difficulty(self) -> None:
        names = {location.name for location in self.multiworld.get_locations(self.player)}
        # 41 superuniques per difficulty, plus The Cow King and the cows on Normal and Nightmare.
        self.assertEqual(sum(1 for name in names if ": Kill " in name), 3 * 41 + 2 * 2)
        self.assertEqual(sum(1 for name in names if ": Enter " in name), 3 * 46 + 2)

    def test_missable_and_grind_checks_hold_filler(self) -> None:
        from BaseClasses import LocationProgressType
        excluded = {location.name for location in self.multiworld.get_locations(self.player)
                    if location.progress_type == LocationProgressType.EXCLUDED}
        # The Cow King can close the level before the 100th cow.
        self.assertIn("Normal: Kill 100 Cows", excluded)
        self.assertNotIn("Normal: Kill The Cow King", excluded)
        # Past level 70 the milestones are a long grind.
        self.assertIn("Reach Character Level 75", excluded)
        self.assertNotIn("Reach Character Level 70", excluded)


class TestCollections(D2RTestBase):
    options = {"goal": "hell", "goal_act": 5, "skill_pool_size": 40, "rune_checks": 33, "gem_checks": True,
               "set_checks": True, "special_item_checks": True, "unique_weight": 100, "high_rune_weight": 100}

    def test_collection_locations(self) -> None:
        from BaseClasses import LocationProgressType
        locations = {location.name: location for location in self.multiworld.get_locations(self.player)}
        collected = [location for name, location in locations.items() if name.startswith(("Find ", "Complete "))]
        self.assertEqual(len(collected), 33 + 35 + 32 + 13)
        for location in collected:
            self.assertEqual(location.progress_type, LocationProgressType.EXCLUDED)
        # Enough plain filler for every filler-only location, even with useful loot weighted up.
        plain = [item for item in self.multiworld.itempool
                 if item.player == self.player and not item.advancement and not item.useful]
        self.assertGreaterEqual(len(plain), len(collected))
        # Cow level on the two difficulties finished before the goal.
        self.assertIn("Nightmare: Kill The Cow King", locations)
        self.assertNotIn("Hell: Enter the Secret Cow Level", locations)
        data = self.world.fill_slot_data()
        self.assertEqual(data["rune_checks"], 33)
        self.assertTrue(data["special_item_checks"] and data["cow_level_checks"])


class TestSpecialsNeedHell(D2RTestBase):
    options = {"goal": "nightmare", "special_item_checks": True, "rune_checks": 5}

    def test_no_specials_without_hell(self) -> None:
        names = {location.name for location in self.multiworld.get_locations(self.player)}
        self.assertNotIn("Find Key of Terror", names)
        self.assertIn("Find Eth Rune", names)
        self.assertNotIn("Find Ith Rune", names)
        self.assertIn("Normal: Enter the Secret Cow Level", names)
        self.assertFalse(self.world.fill_slot_data()["special_item_checks"])


class TestScaleWithLength(D2RTestBase):
    options = {"goal": "hell", "goal_act": 5}

    def test_more_skills_and_empowerments(self) -> None:
        names = [item.name for item in self.multiworld.itempool if item.player == self.player]
        # 16 skills and 2 empowerments for four acts; a full Hell run has 15.
        self.assertEqual(len(self.world.skill_pool), 60)
        self.assertEqual(names.count("Codex Empowerment"), 8)


class TestNoTraps(D2RTestBase):
    options = {"trap_percentage": 0}

    def test_no_traps(self) -> None:
        names = [item.name for item in self.multiworld.itempool if item.player == self.player]
        self.assertFalse(any(name == "Junk Backpack" or name.endswith(" Trap") for name in names))


class TestLocks(D2RTestBase):
    options = {"cube_lock": True, "town_portal_lock": True, "potion_lock": True, "stash_lock": True, "merchant_lock": True,
               "waypoint_lock": True, "goal_act": 3, "act_keys": False}

    def test_lock_items(self) -> None:
        names = [item.name for item in self.multiworld.itempool if item.player == self.player]
        for name in ("Horadric Cube Access", "Town Portal Access", "Potion Access", "Stash Access", "Merchant Access",
                     "Act I Waypoints", "Act III Waypoints"):
            self.assertEqual(names.count(name), 1, name)
        self.assertNotIn("Act IV Waypoints", names)
        self.assertTrue(self.world.create_item("Horadric Cube Access").advancement)
        self.assertTrue(self.world.create_item("Merchant Access").advancement)
        self.assertFalse(self.world.create_item("Stash Access").advancement)
        self.assertEqual(self.world.fill_slot_data()["locks"]["cube"], True)

    def test_late_waypoints_never_lock(self) -> None:
        # From Acts IV and V only a waypoint goes back an act, so those must work from the start.
        self.world.act_count = 5
        self.assertEqual([name for name in self.world.lock_items() if name.endswith("Waypoints")],
                         ["Act I Waypoints", "Act II Waypoints", "Act III Waypoints"])

    def test_cube_gates_duriel(self) -> None:
        self.collect_all_but(["Horadric Cube Access"])
        self.assertFalse(self.can_reach_location("Normal: The Seven Tombs"))
        self.assertTrue(self.can_reach_location("Normal: Radament's Lair"))
        self.assertFalse(self.can_reach_region("Normal Act III"))
        self.collect_by_name("Horadric Cube Access")
        self.assertTrue(self.can_reach_location("Normal: The Seven Tombs"))
        self.assertTrue(self.can_reach_region("Normal Act III"))

    def test_shops_need_merchants(self) -> None:
        self.collect_all_but(["Merchant Access"])
        self.assertFalse(self.can_reach_location("Act I: Buy Archipelago Parcel 1"))


class TestCowLevelMerchants(D2RTestBase):
    # No shops, so only the cow level makes Merchant Access progression.
    options = {"merchant_lock": True, "shop_checks": 0, "goal": "nightmare", "goal_act": 1, "act_keys": False}

    def test_cow_portal_needs_a_tome(self) -> None:
        # The cow portal takes a Tome of Town Portal, which only a merchant sells.
        self.assertTrue(self.world.create_item("Merchant Access").advancement)
        self.collect_all_but(["Merchant Access"])
        self.assertTrue(self.can_reach_region("Normal Act V"))
        self.assertFalse(self.can_reach_location("Normal: Enter the Secret Cow Level"))
        self.assertFalse(self.can_reach_location("Normal: Kill The Cow King"))
        self.collect_by_name("Merchant Access")
        self.assertTrue(self.can_reach_location("Normal: Kill The Cow King"))


class TestEndgameChecks(D2RTestBase):
    options = {"goal": "hell", "goal_act": 5, "uber_checks": True, "kill_count_checks": True, "find_count_checks": True}

    def test_endgame_locations(self) -> None:
        from BaseClasses import LocationProgressType
        locations = {location.name: location for location in self.multiworld.get_locations(self.player)}
        for name in ("Kill Lilith", "Kill Uber Baal", "Hell: Kill 500 Monsters in Act V", "Find 50 Unique Items", "Find 25 Set Items"):
            self.assertEqual(locations[name].progress_type, LocationProgressType.EXCLUDED, name)
        self.assertEqual(sum(1 for name in locations if " Monsters in " in name), 15 * 3)
        self.assertIn("Hell: Find Hellforge Hammer", locations)


class TestAct1Only(D2RTestBase):
    options = {"goal": "normal", "goal_act": 1, "level_checks": False}

    def test_generates(self) -> None:
        self.assertIn("Defeat Andariel", {location.name for location in self.multiworld.get_locations(self.player)})


class TestHellAct3(D2RTestBase):
    options = {"goal": "hell", "goal_act": 3}

    def test_level_cap_between_nightmare_and_hell(self) -> None:
        self.assertEqual(self.world.level_cap, 75)
        self.assertEqual(self.world.fill_slot_data()["goal_quest"], 22)


class TestTreeMode(D2RTestBase):
    options = {"skill_mode": "tree", "character_class": "barbarian", "skill_copies": 2, "skill_points": 5}

    def test_pool_is_the_barbarian_tree(self) -> None:
        barbarian = self.world.item_name_groups["Barbarian"]
        self.assertTrue(all(name in barbarian for name in self.world.skill_pool + self.world.starting))
        self.assertEqual(len(self.world.skill_pool) + len(self.world.starting), 30)
        names = [item.name for item in self.multiworld.itempool]
        self.assertEqual(names.count("Skill Point"), 5)
        self.assertEqual(names.count("Codex Empowerment"), 0)

    def test_slot_data(self) -> None:
        data = self.world.fill_slot_data()
        self.assertEqual(data["skill_mode"], "tree")
        self.assertTrue(data["randomizer"]["tree_mode"])
        self.assertEqual(data["randomizer"]["skill_tree"], [])


class TestTreeShuffle(D2RTestBase):
    options = {"skill_mode": "tree", "character_class": "any", "skill_tree_shuffle": True}

    def test_tree_has_thirty_skills_from_many_classes(self) -> None:
        from ..data import SKILL_SLOTS, SKILLS
        tree = self.world.tree_skills()
        self.assertEqual(len(tree), 30)
        self.assertEqual(sorted(self.world.skill_pool + self.world.starting), sorted(tree))
        self.assertNotEqual(self.world.class_name(), "")
        self.assertGreater(len({name.split(" (")[1] for name in tree}), 2)
        # The deal is a permutation within each row, and pinned skills stay put.
        row = {skill: slot_row for skill, _page, slot_row, _column, _pinned in SKILL_SLOTS}
        pinned = {skill for skill, *_rest, is_pinned in SKILL_SLOTS if is_pinned}
        dealt = self.world.skill_tree
        self.assertEqual(sorted(dealt.values()), sorted(dealt))
        self.assertTrue(all(row[skill] == row[slot] for skill, slot in dealt.items()))
        self.assertFalse(pinned & set(dealt))
        self.assertTrue(all(cls != "war" for skill, _name, cls in SKILLS if skill in dealt))
