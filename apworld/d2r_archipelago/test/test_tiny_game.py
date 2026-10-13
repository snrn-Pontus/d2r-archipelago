import unittest

from Fill import distribute_items_restrictive
from Options import OptionError
from test.general import setup_multiworld
from worlds.AutoWorld import AutoWorldRegister

from .. import GAME

# A one-act game with every optional check off: only the six Act I quests, five of them fillable
# (Andariel's holds a Random Unique).
TINY = {"goal": "normal", "goal_act": 1, "area_checks": False, "side_area_checks": 0, "level_checks": False,
        "superunique_checks": False, "quest_item_checks": False, "shop_checks": 0, "starting_skills": 0}
ALL_LOCKS = {"cube_lock": True, "town_portal_lock": True, "potion_lock": True, "stash_lock": True, "merchant_lock": True,
             "waypoint_lock": True}


class TestTinyGame(unittest.TestCase):
    def generate(self, options: dict):
        world_type = AutoWorldRegister.world_types[GAME]
        multiworld = setup_multiworld([world_type], seed=1, options=[options])
        distribute_items_restrictive(multiworld)
        self.assertTrue(multiworld.can_beat_game(multiworld.state))
        return multiworld

    def test_more_locks_than_locations_is_refused(self) -> None:
        # Six lock items (five locks and Act I Waypoints) for five locations.
        with self.assertRaises(OptionError) as refused:
            self.generate({**TINY, **ALL_LOCKS})
        self.assertIn("5 locations", str(refused.exception))
        self.assertIn("6 such items", str(refused.exception))

    def test_as_many_locks_as_locations_generates(self) -> None:
        multiworld = self.generate({**TINY, **ALL_LOCKS, "waypoint_lock": False})
        world = multiworld.worlds[1]
        self.assertEqual(world.skill_pool, [])
        locations = [location for location in multiworld.get_locations(1) if location.address is not None]
        self.assertEqual(len(locations), 6)

    def test_tiny_game_with_spare_checks_fits_a_skill(self) -> None:
        # Three parcels: eight fillable locations, six keys and locks, two left for one skill with two copies.
        multiworld = self.generate({**TINY, **ALL_LOCKS, "shop_checks": 3})
        self.assertEqual(len(multiworld.worlds[1].skill_pool), 1)
