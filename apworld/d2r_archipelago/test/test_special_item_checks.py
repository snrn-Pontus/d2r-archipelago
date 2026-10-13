import unittest

from BaseClasses import LocationProgressType
from Fill import distribute_items_restrictive
from test.general import setup_multiworld
from worlds.AutoWorld import AutoWorldRegister

from .. import GAME
from ..data import SPECIAL_ITEMS
from ..ids import find_location_name


class TestSpecialItemChecks(unittest.TestCase):
    def test_checks_require_full_hell_goal(self) -> None:
        world_type = AutoWorldRegister.world_types[GAME]
        special_names = {find_location_name(name) for name in SPECIAL_ITEMS}
        for goal in ("normal", "nightmare", "hell"):
            for goal_act in range(1, 6):
                with self.subTest(goal=goal, goal_act=goal_act):
                    multiworld = setup_multiworld([world_type], seed=68, options=[{
                        "goal": goal, "goal_act": goal_act, "special_item_checks": True,
                    }])
                    world = multiworld.worlds[1]
                    locations = {location.name: location for location in multiworld.get_locations(1)}
                    enabled = goal == "hell" and goal_act == 5
                    self.assertEqual(special_names & locations.keys(), special_names if enabled else set())
                    self.assertEqual(world.fill_slot_data()["special_item_checks"], enabled)
                    if enabled:
                        for name in special_names:
                            self.assertEqual(locations[name].parent_region.name, "Hell Act V")
                            self.assertEqual(locations[name].progress_type, LocationProgressType.EXCLUDED)
                    distribute_items_restrictive(multiworld)
                    self.assertTrue(multiworld.can_beat_game(multiworld.state))

    def test_checks_can_be_disabled_for_full_hell(self) -> None:
        world_type = AutoWorldRegister.world_types[GAME]
        multiworld = setup_multiworld([world_type], seed=68, options=[{
            "goal": "hell", "goal_act": 5, "special_item_checks": False,
        }])
        names = {location.name for location in multiworld.get_locations(1)}
        self.assertTrue(names.isdisjoint(find_location_name(name) for name in SPECIAL_ITEMS))
        self.assertFalse(multiworld.worlds[1].fill_slot_data()["special_item_checks"])
