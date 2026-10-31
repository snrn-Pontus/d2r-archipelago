import re
from pathlib import Path

from . import D2RTestBase
from ..data import SIDE_AREAS
from ..ids import ITEM_BASE, LOCATION_BASE, area_location_name
from ..options import STARTING_ITEMS, StartingGold, StartingItems


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
        self.assertEqual(world.location_name_to_id["Normal: Enter Mausoleum"], LOCATION_BASE + 1019)
        self.assertEqual(world.location_name_to_id["Nightmare: Enter Abaddon"], LOCATION_BASE + 1325)
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

    def test_default_trap_tuning(self) -> None:
        self.assertEqual(self.world.fill_slot_data()["traps"], {"wait_for_town": True, "ambush_size": 6, "ambush_monsters": "act",
                                                                "tax_per_level": 1000, "junk_size": 40})

    def test_default_reward_tuning(self) -> None:
        # The rewards from before these were options.
        self.assertEqual(self.world.fill_slot_data()["rewards"], {
            "gold_per_level": 1000, "experience_percent": 15, "level_below": 12, "level_above": 2, "rare_jewel_chance": 30,
            "high_rune_curve": 2, "shrines": [1, 6, 7, 8, 9, 10, 11, 12, 13, 15, 18]})
        weights = self.world.loot_weights()
        self.assertEqual(weights["Random Charm"], 8 * 0.6)
        self.assertEqual(weights["Random Jewel"], 8 * 0.4)
        self.assertEqual(weights["Random Shrine"], 6)

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


class TestDefaultSideAreas(D2RTestBase):
    def test_five_side_areas(self) -> None:
        side = {name for _level_id, _act, name in SIDE_AREAS}
        picked = [location for location in self.multiworld.get_locations(self.player)
                  if location.name.removeprefix("Normal: Enter ") in side]
        self.assertEqual(len(picked), 5)
        self.assertEqual(sorted(location.address for location in picked), self.world.fill_slot_data()["side_areas"])
        # Default goal is Normal Act IV: no Act V side areas.
        self.assertTrue(all(location.act_index < 4 for location in picked))


class TestSideAreasPerDifficulty(D2RTestBase):
    options = {"goal": "hell", "goal_act": 5, "side_area_checks": 10}

    def test_each_difficulty_picks_its_own(self) -> None:
        picks = self.world.side_areas
        self.assertEqual(sorted(picks), [0, 1, 2])
        self.assertTrue(all(len(areas) == 10 for areas in picks.values()))
        self.assertTrue(picks[0] != picks[1] or picks[1] != picks[2])
        names = {location.name for location in self.multiworld.get_locations(self.player)}
        for difficulty, areas in picks.items():
            for _level_id, _act, name in areas:
                self.assertIn(area_location_name(difficulty, name), names)


class TestAllSideAreas(D2RTestBase):
    options = {"goal": "nightmare", "goal_act": 2, "side_area_checks": 28}

    def test_every_open_side_area(self) -> None:
        self.assertEqual(len(self.world.side_areas[0]), len(SIDE_AREAS))
        # Nightmare stops after Act II, so only Acts I and II side areas.
        self.assertEqual(self.world.side_areas[1], [area for area in SIDE_AREAS if area[1] < 2])


class TestNoSideAreas(D2RTestBase):
    options = {"side_area_checks": 0}

    def test_none(self) -> None:
        self.assertEqual(self.world.fill_slot_data()["side_areas"], [])
        # D2RLoader settings left at keep (the default) aren't overridden.
        self.assertEqual(self.world.fill_slot_data()["loader_settings"], {})


class TestLoaderSettings(D2RTestBase):
    options = {"show_ground_sockets": "on", "item_stat_ranges": "off", "maximum_sockets": "keep", "display_item_levels": True,
               "shared_stash_tabs": 50, "materials_limit": 99, "floor_item_limit": 64}

    def test_only_the_chosen_ones(self) -> None:
        self.assertEqual(self.world.fill_slot_data()["loader_settings"], {
            "d2rcore.items.show_ground_sockets": True,
            "d2rcore.items.item_stat_ranges": False,
            "d2rcore.items.display_item_levels": True,
            "d2rcore.items.floor_item_display_limit": 64,
            "d2rcore.stash.add_shared_tabs": 50,
        })


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


class TestNoTeleportCharmByDefault(D2RTestBase):
    def test_no_charm(self) -> None:
        names = [item.name for item in [*self.multiworld.itempool, *self.multiworld.precollected_items[self.player]]]
        self.assertNotIn("Teleport Charm", names)


class TestTeleportCharmAtStart(D2RTestBase):
    options = {"teleport_charm": "start"}

    def test_charm_is_a_starting_item(self) -> None:
        self.assertIn("Teleport Charm", [item.name for item in self.multiworld.precollected_items[self.player]])
        self.assertNotIn("Teleport Charm", [item.name for item in self.multiworld.itempool])


class TestTeleportCharmEarly(D2RTestBase):
    options = {"teleport_charm": "early"}

    def test_charm_is_in_an_early_check_of_your_own(self) -> None:
        from BaseClasses import ItemClassification
        charm = [item for item in self.multiworld.itempool if item.name == "Teleport Charm"]
        self.assertEqual(len(charm), 1)
        self.assertEqual(charm[0].classification, ItemClassification.useful)
        self.assertEqual(self.multiworld.local_early_items[self.player].get("Teleport Charm"), 1)


class TestTeleportCharmAnywhere(D2RTestBase):
    options = {"teleport_charm": "anywhere"}

    def test_charm_is_in_the_pool(self) -> None:
        self.assertEqual([item.name for item in self.multiworld.itempool].count("Teleport Charm"), 1)
        self.assertNotIn("Teleport Charm", self.multiworld.local_early_items[self.player])
        self.assertEqual(len(self.multiworld.itempool), len(self.multiworld.get_unfilled_locations(self.player)))


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
        self.assertEqual(self.world.fill_slot_data()["levels_per_copy"], 1)
        self.assertEqual(self.world.fill_slot_data()["levels_per_empowerment"], 1)
        self.assertEqual(self.world.fill_slot_data()["codex_level_cap"], 60)


class TestClassWeightPercent(D2RTestBase):
    options = {"skill_focus": "class_weighted", "character_class": "sorceress", "skill_pool_size": 20,
               "class_weight_percent": 75}

    def test_share_from_class(self) -> None:
        sorceress = self.world.item_name_groups["Sorceress"]
        self.assertEqual(sum(1 for name in self.world.skill_pool if name in sorceress), 15)


class TestClassWeightPercentNone(D2RTestBase):
    options = {"skill_focus": "class_weighted", "character_class": "sorceress", "skill_pool_size": 20,
               "class_weight_percent": 0}

    def test_none_from_class(self) -> None:
        sorceress = self.world.item_name_groups["Sorceress"]
        self.assertEqual(sum(1 for name in self.world.skill_pool if name in sorceress), 0)


class TestCodexLevels(D2RTestBase):
    options = {"levels_per_copy": 3, "levels_per_empowerment": 2, "codex_level_cap": 99}

    def test_slot_data(self) -> None:
        slot_data = self.world.fill_slot_data()
        self.assertEqual((slot_data["levels_per_copy"], slot_data["levels_per_empowerment"], slot_data["codex_level_cap"]),
                         (3, 2, 99))


class TestStartingOptions(D2RTestBase):
    options = {"starting_gold": 5000, "starting_items": {"Full Rejuvenation Potion": 3, "Ber Rune": 1, "Scroll of Identify": 0},
               "free_merc_revive": True}

    def test_slot_data(self) -> None:
        data = self.world.fill_slot_data()
        self.assertEqual(data["starting_gold"], 5000)
        # Item codes in name order; zero counts left out.
        self.assertEqual(data["starting_items"], [["r30", 1], ["rvl", 3]])
        self.assertTrue(data["randomizer"]["free_merc_revive"])

    def test_items_match_plugin(self) -> None:
        # The plugin only hands out the codes in its own list (starting_items.h).
        header = Path(__file__).resolve().parents[3] / "plugin" / "src" / "starting_items.h"
        if not header.exists():
            self.skipTest("no plugin source next to this apworld")
        text = header.read_text(encoding="utf-8")
        pairs = re.findall(r'\{ "(\w{3})", "([^"]+)" \}', text)
        self.assertEqual({name: code for code, name in pairs}, STARTING_ITEMS)
        self.assertIn(f"MaxCopies = {StartingItems.max};", text)
        self.assertIn(f"MaxGold = {StartingGold.range_end};", text)


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
        self.assertEqual(data["randomizer"]["main_path_superunique_cap"], 2)
        self.assertEqual(data["randomizer"]["main_path_superunique_minions"], 5)
        self.assertEqual(data["randomizer"]["main_path_superunique_strength"], 100)
        self.assertEqual(data["randomizer"]["main_path_superunique_level"], 0)
        self.assertEqual(data["randomizer"]["superunique_shuffle_spread"], 6)
        self.assertEqual(data["randomizer"]["runeword_rune_spread"], 2)
        # Nightmare and Hell follow Normal by default.
        self.assertEqual(data["randomizer"]["xp_multiplier_hell"], data["randomizer"]["xp_multiplier"])
        self.assertEqual(data["randomizer"]["monster_density_nightmare"], data["randomizer"]["monster_density"])
        self.assertEqual(data["randomizer"]["death_xp_penalty"], 100)
        # Shop prices and vendor default to the game as it was: full price, potion sellers.
        self.assertEqual((data["randomizer"]["parcel_price"], data["randomizer"]["prophecy_price"],
                          data["randomizer"]["shop_vendor"]), (100, 100, 0))
        # No starting gold or items, and mercenaries cost the usual to revive.
        self.assertEqual((data["starting_gold"], data["starting_items"]), (0, []))
        self.assertFalse(data["randomizer"]["free_merc_revive"])
        self.assertTrue(data["superunique_checks"])
        self.assertFalse(data["death_link"])
        # DeathLink defaults to what it always did: die, and send every death.
        self.assertEqual((data["death_link_mode"], data["death_link_amnesty"]), ("die", 1))
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


class TestDeathLinkModes(D2RTestBase):
    options = {"death_link": True, "death_link_mode": "random_trap", "death_link_amnesty": 3,
               "ambush_weight": 5, "poison_cloud_weight": 0, "firestorm_weight": 0, "mana_drain_weight": 0,
               "tax_collector_weight": 0, "mercenary_down_weight": 2, "junk_backpack_weight": 50}

    def test_slot_data(self) -> None:
        data = self.world.fill_slot_data()
        self.assertEqual((data["death_link_mode"], data["death_link_amnesty"]), ("random_trap", 3))
        # Only the traps with a weight, and never the Junk Backpack, by their LOOT index (the plugin's Loot enum).
        self.assertEqual(data["death_link_traps"], [[17, 5], [22, 2]])


class TestHellSuperUniques(D2RTestBase):
    options = {"goal": "hell", "goal_act": 5, "skill_pool_size": 40}

    def test_every_difficulty(self) -> None:
        names = {location.name for location in self.multiworld.get_locations(self.player)}
        # 31 superuniques per difficulty, plus The Cow King and the cows on Normal and Nightmare.
        self.assertEqual(sum(1 for name in names if ": Kill " in name), 3 * 31 + 2 * 2)
        # 46 areas and 5 side areas per difficulty, plus the cow level on Normal and Nightmare.
        self.assertEqual(sum(1 for name in names if ": Enter " in name), 3 * (46 + 5) + 2)

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


class TestTrapTuning(D2RTestBase):
    options = {"traps_wait_for_town": False, "ambush_size": 12, "ambush_monsters": "area", "tax_collector_gold": 250,
               "junk_backpack_size": 10}

    def test_slot_data(self) -> None:
        self.assertEqual(self.world.fill_slot_data()["traps"], {"wait_for_town": False, "ambush_size": 12, "ambush_monsters": "area",
                                                                "tax_per_level": 250, "junk_size": 10})


class TestRewardTuning(D2RTestBase):
    options = {"gold_reward_per_level": 250, "experience_reward_percent": 50, "reward_level_below": 5, "reward_level_above": 0,
               "rare_jewel_chance": 100, "high_rune_curve": "flat", "shrine_pool": ["Experience", "Skill"], "jewel_share": 100,
               "boss_reward_type": "high_rune"}

    def test_slot_data(self) -> None:
        self.assertEqual(self.world.fill_slot_data()["rewards"], {
            "gold_per_level": 250, "experience_percent": 50, "level_below": 5, "level_above": 0, "rare_jewel_chance": 100,
            "high_rune_curve": 0, "shrines": [12, 15]})

    def test_jewels_only(self) -> None:
        weights = self.world.loot_weights()
        self.assertEqual(weights["Random Charm"], 0)
        self.assertEqual(weights["Random Jewel"], 8)
        names = [item.name for item in self.multiworld.itempool if item.player == self.player]
        self.assertNotIn("Random Charm", names)

    def test_boss_quests_hold_the_chosen_reward(self) -> None:
        for quest in ("Sisters to the Slaughter", "The Seven Tombs", "The Guardian"):
            self.assertEqual(self.multiworld.get_location(f"Normal: {quest}", self.player).item.name, "High Rune")


class TestNoShrines(D2RTestBase):
    options = {"shrine_pool": [], "shrine_weight": 100}

    def test_no_random_shrines(self) -> None:
        self.assertEqual(self.world.loot_weights()["Random Shrine"], 0)
        names = [item.name for item in self.multiworld.itempool if item.player == self.player]
        self.assertNotIn("Random Shrine", names)
        self.assertEqual(self.world.fill_slot_data()["rewards"]["shrines"], [])


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
        self.assertTrue(self.world.create_item("Stash Access").useful)
        self.assertEqual(self.world.fill_slot_data()["locks"]["cube"], True)
        self.assertEqual(self.world.fill_slot_data()["locks"]["potion_mode"], "set_aside")
        self.assertEqual(self.world.fill_slot_data()["locks"]["town_portal_tome_charges"], 20)
        self.assertEqual(self.world.fill_slot_data()["enforce_act_keys_mode"], "warp_to_town")
        # No cow level here, so no check waits for Town Portal Access.
        self.assertFalse(self.world.create_item("Town Portal Access").advancement)

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


class TestDrinkPotions(D2RTestBase):
    options = {"potion_lock": True, "potion_lock_mode": "drink"}

    def test_slot_data(self) -> None:
        locks = self.world.fill_slot_data()["locks"]
        self.assertTrue(locks["potions"])
        self.assertEqual(locks["potion_mode"], "drink")


class TestLockModes(D2RTestBase):
    options = {"potion_lock": True, "potion_lock_mode": "rejuvenation_allowed", "town_portal_lock": True,
               "tp_access_tome_charges": 0, "enforce_act_keys_mode": "message_only"}

    def test_slot_data(self) -> None:
        data = self.world.fill_slot_data()
        self.assertEqual(data["locks"]["potion_mode"], "rejuvenation_allowed")
        self.assertEqual(data["locks"]["town_portal_tome_charges"], 0)
        self.assertEqual(data["enforce_act_keys_mode"], "message_only")


class TestCowLevelPortals(D2RTestBase):
    # The Town Portal lock sets tomes aside, so the cow portal waits for Town Portal Access.
    options = {"town_portal_lock": True, "cow_level_checks": True, "goal": "nightmare", "goal_act": 1, "act_keys": False}

    def test_cow_portal_needs_town_portal_access(self) -> None:
        self.assertTrue(self.world.create_item("Town Portal Access").advancement)
        self.collect_all_but(["Town Portal Access"])
        self.assertTrue(self.can_reach_region("Normal Act V"))
        self.assertFalse(self.can_reach_location("Normal: Kill The Cow King"))
        self.collect_by_name("Town Portal Access")
        self.assertTrue(self.can_reach_location("Normal: Kill The Cow King"))


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


class TestDefaultThresholds(D2RTestBase):
    """The check thresholds from before they were options (#40)."""

    def test_slot_data(self) -> None:
        data = self.world.fill_slot_data()
        self.assertEqual((data["level_step"], data["cow_kills"]), (5, 100))
        self.assertEqual((data["kill_counts"], data["unique_finds"], data["set_finds"]),
                         ([100, 250, 500], [1, 5, 10, 25, 50], [1, 5, 10, 25]))

    def test_level_checks(self) -> None:
        # Normal through Diablo: every fifth level up to 20 (the cap, 24, rounded down to the step).
        levels = sorted(int(location.name.rsplit(" ", 1)[1]) for location in self.multiworld.get_locations(self.player)
                        if location.name.startswith("Reach Character Level "))
        self.assertEqual(levels, [5, 10, 15, 20])

    def test_ids_stay_put(self) -> None:
        # Every count any option makes has its own id; the counts from before keep theirs.
        ids = self.world.location_name_to_id
        self.assertEqual(len(set(ids.values())), len(ids))
        self.assertEqual(ids["Reach Character Level 5"], LOCATION_BASE + 2005)
        self.assertEqual(ids["Reach Character Level 2"], LOCATION_BASE + 2002)
        self.assertEqual(ids["Reach Character Level 90"], LOCATION_BASE + 2090)
        self.assertNotIn("Reach Character Level 1", ids)
        self.assertNotIn("Reach Character Level 91", ids)
        self.assertEqual(ids["Normal: Kill 100 Cows"], LOCATION_BASE + 5402)
        self.assertEqual(ids["Normal: Kill 50 Cows"], LOCATION_BASE + 5403)
        self.assertEqual(ids["Hell: Kill 500 Cows"], LOCATION_BASE + 5425)
        self.assertEqual(ids["Normal: Kill 100 Monsters in Act I"], LOCATION_BASE + 6000)
        self.assertEqual(ids["Normal: Kill 50 Monsters in Act I"], LOCATION_BASE + 6003)
        self.assertEqual(ids["Hell: Kill 1000 Monsters in Act V"], LOCATION_BASE + 6246)
        self.assertEqual(ids["Find 1 Unique Item"], LOCATION_BASE + 5800)
        self.assertEqual(ids["Find 100 Unique Items"], LOCATION_BASE + 5808)
        self.assertEqual(ids["Find 12 Set Items"], LOCATION_BASE + 5815)
        self.assertEqual(ids["Find 50 Set Items"], LOCATION_BASE + 5817)

    def test_every_choice_has_ids(self) -> None:
        from ..ids import (BASE_KILL_COUNTS, BASE_SET_FINDS, BASE_UNIQUE_FINDS, COW_KILLS, KILL_COUNTS, SET_FINDS, UNIQUE_FINDS,
                           scaled_counts)
        from ..options import CowKillCount, FindCountMultiplier, KillCountMultiplier
        # The plugin finds each count's id by its index (ids.h), so each catalog fits its ten slots.
        for catalog in (KILL_COUNTS, UNIQUE_FINDS, SET_FINDS, COW_KILLS):
            self.assertEqual(len(set(catalog)), len(catalog))
            self.assertLessEqual(len(catalog), 10 if catalog is not COW_KILLS else 8)
        for percent in KillCountMultiplier.options.values():
            self.assertLessEqual(set(scaled_counts(BASE_KILL_COUNTS, percent)), set(KILL_COUNTS))
            self.assertEqual(len(set(scaled_counts(BASE_KILL_COUNTS, percent))), len(BASE_KILL_COUNTS))
        for percent in FindCountMultiplier.options.values():
            for base, catalog in ((BASE_UNIQUE_FINDS, UNIQUE_FINDS), (BASE_SET_FINDS, SET_FINDS)):
                self.assertLessEqual(set(scaled_counts(base, percent)), set(catalog))
                self.assertEqual(len(set(scaled_counts(base, percent))), len(base))
        self.assertLessEqual(set(CowKillCount.options.values()), set(COW_KILLS))


class TestCheckThresholds(D2RTestBase):
    options = {"goal": "hell", "goal_act": 5, "skill_pool_size": 40, "level_check_step": 3, "grind_level": 50,
               "kill_count_checks": True, "kill_count_multiplier": "double", "find_count_checks": True,
               "find_count_multiplier": "half", "cow_kill_count": 250}

    def test_locations(self) -> None:
        from BaseClasses import LocationProgressType
        locations = {location.name: location for location in self.multiworld.get_locations(self.player)}
        levels = sorted(int(name.rsplit(" ", 1)[1]) for name in locations if name.startswith("Reach Character Level "))
        self.assertEqual(levels, list(range(3, 91, 3)))
        self.assertEqual(locations["Reach Character Level 51"].progress_type, LocationProgressType.EXCLUDED)
        self.assertNotEqual(locations["Reach Character Level 48"].progress_type, LocationProgressType.EXCLUDED)
        # Names show the real counts.
        for count in (200, 500, 1000):
            self.assertIn(f"Nightmare: Kill {count} Monsters in Act III", locations)
        self.assertNotIn("Nightmare: Kill 100 Monsters in Act III", locations)
        self.assertEqual(sum(1 for name in locations if " Monsters in " in name), 15 * 3)
        uniques = sorted(int(name.split()[1]) for name in locations if name.endswith(("Unique Item", "Unique Items")))
        sets = sorted(int(name.split()[1]) for name in locations if name.endswith(("Set Item", "Set Items")))
        self.assertEqual((uniques, sets), ([1, 2, 5, 12, 25], [1, 2, 5, 12]))
        self.assertEqual(locations["Normal: Kill 250 Cows"].progress_type, LocationProgressType.EXCLUDED)
        self.assertIn("Nightmare: Kill 250 Cows", locations)
        self.assertNotIn("Normal: Kill 100 Cows", locations)
        self.assertNotIn("Hell: Kill 250 Cows", locations)

    def test_slot_data(self) -> None:
        data = self.world.fill_slot_data()
        self.assertEqual((data["level_step"], data["level_cap"], data["cow_kills"]), (3, 90, 250))
        self.assertEqual((data["kill_counts"], data["unique_finds"], data["set_finds"]),
                         ([200, 500, 1000], [1, 2, 5, 12, 25], [1, 2, 5, 12]))


class TestEveryLevel(D2RTestBase):
    options = {"goal": "normal", "goal_act": 1, "level_check_step": 1, "grind_level": 0, "kill_count_checks": True,
               "kill_count_multiplier": "half"}

    def test_locations(self) -> None:
        from BaseClasses import LocationProgressType
        locations = {location.name: location for location in self.multiworld.get_locations(self.player)}
        levels = [location for name, location in locations.items() if name.startswith("Reach Character Level ")]
        # Act I alone has a cap of 6; level 1 is where you start, so not a check.
        self.assertEqual(sorted(location.name for location in levels), [f"Reach Character Level {level}" for level in range(2, 7)])
        self.assertTrue(all(location.progress_type == LocationProgressType.EXCLUDED for location in levels))
        self.assertEqual(sorted(name for name in locations if " Monsters in " in name),
                         ["Normal: Kill 125 Monsters in Act I", "Normal: Kill 250 Monsters in Act I", "Normal: Kill 50 Monsters in Act I"])
        self.assertEqual(self.world.fill_slot_data()["level_cap"], 6)


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
        # The Barbarian's level-1 skills don't attack on their own; one of them still goes on the weapon.
        from ..data import SKILLS, STARTER_SKILLS
        start = data["randomizer"]["start_skill"]
        self.assertEqual(start["class"], "Barbarian")
        self.assertIn(start["skill"], STARTER_SKILLS)
        self.assertIn(next(f"{name} (Barbarian)" for sid, name, _cls in SKILLS if sid == start["skill"]), self.world.starting)


class TestTreeModeStartSkill(D2RTestBase):
    options = {"skill_mode": "tree", "character_class": "sorceress", "starting_skills": 2}

    def test_starting_weapon_gets_an_attack(self) -> None:
        from ..data import START_ATTACK_SKILLS
        start = self.world.fill_slot_data()["randomizer"]["start_skill"]
        self.assertEqual(start["class"], "Sorceress")
        # Fire Bolt, Charged Bolt or Ice Bolt, never Frozen Armor.
        self.assertIn(start["skill"], START_ATTACK_SKILLS)

    def test_no_starting_skills_clears_it(self) -> None:
        self.world.starting = []
        self.assertEqual(self.world.fill_slot_data()["randomizer"]["start_skill"], {"class": "Sorceress", "skill": -1})


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


class TestTreeShuffleTurnsOnTreeMode(D2RTestBase):
    options = {"skill_mode": "codex", "character_class": "necromancer", "skill_tree_shuffle": True}

    def test_codex_with_shuffle_plays_tree_mode(self) -> None:
        self.assertTrue(self.world.tree_mode)
        self.assertNotEqual(self.world.skill_tree, {})
        self.assertEqual(sorted(self.world.skill_pool + self.world.starting), sorted(self.world.tree_skills()))
        data = self.world.fill_slot_data()
        self.assertEqual(data["skill_mode"], "tree")
        self.assertTrue(data["randomizer"]["tree_mode"])
        self.assertNotIn("Codex Empowerment", [item.name for item in self.multiworld.itempool])
        self.assertEqual(data["randomizer"]["start_skill"]["class"], "Necromancer")


class TestWarlockTreeShuffle(D2RTestBase):
    options = {"character_class": "warlock", "skill_tree_shuffle": True, "starting_skills": 2}

    def test_warlock_tree_is_dealt(self) -> None:
        from ..data import SKILLS
        self.assertTrue(self.world.options.include_warlock)
        slot_class = {skill_id: cls for skill_id, _name, cls in SKILLS}
        dealt = self.world.skill_tree
        # Warlock skills go to other trees, and other classes' skills into the Warlock's.
        self.assertTrue(any(slot_class[skill] == "war" and slot_class[slot] != "war" for skill, slot in dealt.items()))
        self.assertTrue(any(slot_class[skill] != "war" and slot_class[slot] == "war" for skill, slot in dealt.items()))
        tree = self.world.tree_skills()
        self.assertEqual(len(tree), 30)
        self.assertEqual(sorted(self.world.skill_pool + self.world.starting), sorted(tree))
        self.assertEqual(self.world.fill_slot_data()["randomizer"]["start_skill"]["class"], "Warlock")


class TestWarlockStaysOutOfTheShuffle(D2RTestBase):
    options = {"character_class": "sorceress", "skill_tree_shuffle": True}

    def test_no_warlock_in_the_deal(self) -> None:
        from ..data import SKILLS
        slot_class = {skill_id: cls for skill_id, _name, cls in SKILLS}
        self.assertFalse(self.world.options.include_warlock)
        self.assertTrue(all("war" not in (slot_class[skill], slot_class[slot]) for skill, slot in self.world.skill_tree.items()))


class TestWarlockTurnsOnIncludeWarlock(D2RTestBase):
    options = {"character_class": "warlock", "starting_skills": 2}

    def test_warlock_starts_with_warlock_skills(self) -> None:
        warlock = self.world.item_name_groups["Warlock"]
        self.assertTrue(self.world.options.include_warlock)
        self.assertTrue(all(name in warlock for name in self.world.starting))
        self.assertTrue(any(name in warlock for name in self.world.skill_pool))


class TestUnusableSkills(D2RTestBase):
    options = {"character_class": "sorceress", "skill_focus": "any_skills", "skill_pool_size": 240, "skill_copies": 1}

    def test_sorceress_gets_no_claw_trap_or_form_skills(self) -> None:
        from ..data import SKILL_USERS, SKILLS
        from ..ids import skill_item_name
        bound = {skill_item_name(sid, name, cls) for sid, name, cls in SKILLS if "sor" not in SKILL_USERS.get(sid, ("sor",))}
        self.assertIn("Fire Trauma (Assassin)", bound)
        self.assertIn("Claw Mastery (Assassin)", bound)
        names = {item.name for item in self.multiworld.itempool} | {item.name for item in self.multiworld.precollected_items[self.player]}
        self.assertFalse(names & bound)
        self.assertGreater(len(self.world.skill_pool), 50)


class TestBarbarianSkills(D2RTestBase):
    options = {"character_class": "barbarian", "skill_focus": "any_skills", "skill_pool_size": 240, "skill_copies": 1}

    def test_barbarian_gets_no_smite_or_traps(self) -> None:
        # The Barbarian's S1 animation has no hit frame (#117) and its S2 only one weapon class.
        names = {item.name for item in self.multiworld.itempool} | {item.name for item in self.multiworld.precollected_items[self.player]}
        for skill in ("Smite (Paladin)", "Fire Trauma (Assassin)", "Lightning Sentry (Assassin)"):
            self.assertNotIn(skill, names)
        self.assertGreater(len(self.world.skill_pool), 50)


class TestS1Skills(D2RTestBase):
    def test_s1_skills_follow_animdata(self) -> None:
        from .. import can_use
        smite, dodges = 97, (13, 18, 29)
        for cls in ("ama", "bar"):
            self.assertFalse(can_use(smite, cls), cls)
        for cls in ("sor", "nec", "pal", "dru", "ass", "war"):
            self.assertTrue(can_use(smite, cls), cls)
        for skill in dodges:
            self.assertFalse(can_use(skill, "pal"))
            self.assertTrue(can_use(skill, "ama"))
            self.assertTrue(can_use(skill, "bar"))


# Skills the engine ties to their own class (gen_data.py ENGINE_CLASS_SKILLS), by item name.
ENGINE_CLASS_SKILLS = {
    "Amazon": ("Fend", "Strafe", "Jab"),
    "Barbarian": ("Leap", "Leap Attack", "Whirlwind", "Double Swing", "Double Throw", "Frenzy"),
    "Sorceress": ("Inferno",),
    "Paladin": ("Zeal",),
    "Druid": ("Arctic Blast", "Fury", "Shock Wave", "Rabies", "Hunger"),
    "Assassin": ("Dragon Flight", "Dragon Talon", "Dragon Tail", "Fists of Fire", "Claws of Thunder", "Blades of Ice",
                 "Dragon Claw", "Claw Mastery", "Weapon Block"),
}


class TestEngineClassSkills(D2RTestBase):
    options = {"skill_focus": "any_skills", "skill_pool_size": 240, "skill_copies": 1, "include_warlock": True}

    def test_engine_class_skills_stay_with_their_class(self) -> None:
        from .. import can_use
        from ..data import CLASSES, SKILLS
        code = {name: cls for cls, name in CLASSES.items()}
        id_of = {f"{name} ({CLASSES[cls]})": sid for sid, name, cls in SKILLS}
        for owner, names in ENGINE_CLASS_SKILLS.items():
            for name in names:
                skill = id_of[f"{name} ({owner})"]
                self.assertEqual([cls for cls in CLASSES if can_use(skill, cls)], [code[owner]], name)
        self.assertFalse(can_use(id_of["Charge (Paladin)"], "nec"))
        self.assertTrue(can_use(id_of["Lightning (Sorceress)"], "nec"))
        for class_name in CLASSES.values():
            others = {f"{name} ({owner})" for owner, names in ENGINE_CLASS_SKILLS.items() if owner != class_name
                      for name in names}
            if class_name == "Necromancer":
                others.add("Charge (Paladin)")
            for seed in range(3):
                self.options = {**type(self).options, "character_class": class_name.lower()}
                self.world_setup(seed)
                self.assertEqual(self.world.class_name(), class_name)
                names = ({item.name for item in self.multiworld.itempool}
                         | {item.name for item in self.multiworld.precollected_items[self.player]})
                self.assertFalse(names & others, f"{class_name} seed {seed}")
                self.assertGreater(len(self.world.skill_pool), 50, f"{class_name} seed {seed}")


class TestAnyClassPoolSize(D2RTestBase):
    options = {"character_class": "any", "skill_pool_size": 240, "skill_copies": 1}

    def test_any_class_still_fills_a_big_pool(self) -> None:
        from ..data import SKILL_USERS, SKILLS
        free = [sid for sid, _name, cls in SKILLS if sid not in SKILL_USERS and cls != "war"]
        self.assertGreater(len(free), 150)
        self.assertGreater(len(self.world.skill_pool), 50)
        self.assertGreaterEqual(len(self.world.skill_pool) + len(self.world.starting), 16)


class TestAnyClassSkills(D2RTestBase):
    options = {"character_class": "any", "skill_focus": "any_skills", "skill_pool_size": 240, "skill_copies": 1}

    def test_any_class_leaves_out_class_bound_skills(self) -> None:
        from ..data import SKILL_USERS, SKILLS
        bound = {f"{name} ({cls})" for sid, name, cls in SKILLS if sid in SKILL_USERS}
        names = {item.name for item in self.multiworld.itempool} | {item.name for item in self.multiworld.precollected_items[self.player]}
        self.assertFalse({name.split(" (")[0] for name in names} & {name.split(" (")[0] for name in bound})


class TestDruidForms(D2RTestBase):
    options = {"character_class": "druid", "skill_classes": ["Druid"], "skill_pool_size": 12, "skill_copies": 1,
               "starting_skills": 4}

    def test_form_attacks_come_with_a_form(self) -> None:
        from ..data import SKILL_FORMS, SKILLS
        from ..ids import skill_item_name
        id_of = {skill_item_name(sid, name, cls): sid for sid, name, cls in SKILLS}
        for seed in range(20):
            self.world_setup(seed)
            starting = {id_of[name] for name in self.world.starting}
            held = starting | {id_of[name] for name in self.world.skill_pool}
            for skill in held:
                if skill in SKILL_FORMS:
                    self.assertTrue(held & set(SKILL_FORMS[skill]), f"seed {seed}: {skill} without its form")
            for skill in starting:
                if skill in SKILL_FORMS:
                    self.assertTrue(starting & set(SKILL_FORMS[skill]), f"seed {seed}: starting {skill} without a starting form")


class TestTreeShuffleUsable(D2RTestBase):
    options = {"skill_mode": "tree", "character_class": "sorceress", "skill_tree_shuffle": True}

    def test_shuffled_trees_only_hold_usable_skills(self) -> None:
        from .. import can_use, deal_skill_trees
        from ..data import SKILLS
        import random
        slot_class = {sid: cls for sid, _name, cls in SKILLS}
        for seed in range(50):
            for warlock in (False, True):
                dealt = deal_skill_trees(random.Random(seed), warlock)
                for skill, slot in dealt.items():
                    self.assertTrue(can_use(skill, slot_class[slot]), f"seed {seed}: {skill} in {slot_class[slot]}'s tree")

    def test_s1_skills_stay_with_classes_that_have_s1(self) -> None:
        from .. import deal_skill_trees
        from ..data import SKILLS
        import random
        slot_class = {sid: cls for sid, _name, cls in SKILLS}
        smite_classes = set()
        for seed in range(200):
            dealt = deal_skill_trees(random.Random(seed))
            if 97 in dealt:
                smite_classes.add(slot_class[dealt[97]])
            for dodge in (13, 18, 29):
                if dodge in dealt:
                    self.assertNotEqual(slot_class[dealt[dodge]], "pal", f"seed {seed}: {dodge} in the Paladin's tree")
        self.assertFalse(smite_classes & {"ama", "bar"})
        self.assertGreater(len(smite_classes), 1)

    def test_engine_class_skills_stay_in_their_class_tree(self) -> None:
        from .. import deal_skill_trees
        from ..data import CLASSES, SKILLS
        import random
        slot_class = {sid: cls for sid, _name, cls in SKILLS}
        id_of = {f"{name} ({CLASSES[cls]})": sid for sid, name, cls in SKILLS}
        pinned = {id_of[f"{name} ({owner})"] for owner, names in ENGINE_CLASS_SKILLS.items() for name in names}
        # The SQ sequences (#176).
        pinned |= {id_of[name] for name in ("Mirrored Blades (Warlock)", "Cleave (Warlock)", "Bind Demon (Warlock)")}
        charge = id_of["Charge (Paladin)"]
        for seed in range(200):
            dealt = deal_skill_trees(random.Random(seed), seed % 2 == 1)
            for skill in pinned & set(dealt):
                self.assertEqual(slot_class[dealt[skill]], slot_class[skill], f"seed {seed}: {skill}")
            if charge in dealt:
                self.assertNotEqual(slot_class[dealt[charge]], "nec", f"seed {seed}: Charge in the Necromancer's tree")


class TestDruidFullStart(D2RTestBase):
    options = {"character_class": "druid", "skill_classes": ["Druid"], "skill_focus": "any_skills", "skill_pool_size": 30,
               "skill_copies": 1, "starting_skills": 10}

    def test_starting_attacks_swap_instead_of_vanishing(self) -> None:
        from ..data import SKILL_FORMS, SKILLS
        from ..ids import skill_item_name
        id_of = {skill_item_name(sid, name, cls): sid for sid, name, cls in SKILLS}
        for seed in range(20):
            self.world_setup(seed)
            self.assertEqual(len(self.world.starting), 10, f"seed {seed}")
            starting = {id_of[name] for name in self.world.starting}
            for skill in starting:
                if skill in SKILL_FORMS:
                    self.assertTrue(starting & set(SKILL_FORMS[skill]), f"seed {seed}: starting {skill} without a starting form")


class TestShuffleTuning(D2RTestBase):
    options = {"experience_multiplier": 200, "experience_multiplier_hell": 400, "monster_density_nightmare": 300,
               "main_path_superunique_minions": "off", "better_drops": 50, "shuffle_cow_level": True,
               "main_path_superunique_level": "off", "main_path_superunique_strength": 70, "death_experience_penalty": 0,
               "parcel_price_multiplier": 50, "prophecy_price_multiplier": 250, "shop_vendor": "blacksmith"}

    def test_slot_data(self) -> None:
        randomizer = self.world.fill_slot_data()["randomizer"]
        self.assertEqual((randomizer["xp_multiplier"], randomizer["xp_multiplier_nightmare"], randomizer["xp_multiplier_hell"]),
                         (200, 200, 400))
        self.assertEqual((randomizer["monster_density_nightmare"], randomizer["monster_density_hell"]), (300, 100))
        self.assertEqual(randomizer["main_path_superunique_minions"], -1)
        self.assertIsNone(randomizer["main_path_superunique_level"])
        self.assertEqual(randomizer["main_path_superunique_strength"], 70)
        self.assertEqual(randomizer["better_drops"], 50)
        self.assertEqual(randomizer["death_xp_penalty"], 0)
        self.assertEqual((randomizer["parcel_price"], randomizer["prophecy_price"], randomizer["shop_vendor"]), (50, 250, 1))
        self.assertTrue(randomizer["shuffle_cow_level"])


class TestClassWeightedBackfill(D2RTestBase):
    options = {"character_class": "sorceress", "skill_classes": ["Sorceress", "Assassin"], "skill_focus": "class_weighted"}

    def test_pool_fills_from_the_class_when_others_run_out(self) -> None:
        sorceress = [f"Skill {n} (Sorceress)" for n in range(28)]
        assassin = [f"Skill {n} (Assassin)" for n in range(16)]
        self.world.item_name_groups = {**self.world.item_name_groups, "Sorceress": set(sorceress)}
        rest = sorceress + assassin
        for count in (10, 30, 44):
            picked = self.world.pick_skill_pool(rest, count)
            self.assertEqual(len(picked), count)
            self.assertEqual(len(set(picked)), count)
        self.assertEqual(len([name for name in self.world.pick_skill_pool(rest, 10) if name in sorceress]), 5)
