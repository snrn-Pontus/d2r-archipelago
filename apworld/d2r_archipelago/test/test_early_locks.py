import unittest

from BaseClasses import CollectionState
from Fill import distribute_items_restrictive
from test.general import gen_steps, setup_multiworld
from worlds.AutoWorld import AutoWorldRegister, call_all

from .. import GAME
from . import D2RTestBase

ALL_LOCKS = {"cube_lock": True, "town_portal_lock": True, "potion_lock": True, "stash_lock": True,
             "merchant_lock": True, "waypoint_lock": True}


class TestEarlyLocks(D2RTestBase):
    options = {**ALL_LOCKS, "local_early_locks": ["Town Portal Access", "Act I Waypoints", "Potion Access"],
               "early_locks": ["Merchant Access", "Potion Access", "Stash Access"]}

    def test_lists_reach_the_fill(self) -> None:
        self.assertEqual(self.multiworld.local_early_items[self.player],
                         {"Act I Waypoints": 1, "Potion Access": 1, "Town Portal Access": 1})
        self.assertEqual(self.multiworld.early_items[self.player], {"Merchant Access": 1, "Stash Access": 1})

    def test_early_locks_are_in_first_checks(self) -> None:
        for seed in range(5):
            self.world_setup(seed)
            distribute_items_restrictive(self.multiworld)
            start = CollectionState(self.multiworld)
            for name in ("Town Portal Access", "Act I Waypoints", "Potion Access", "Merchant Access", "Stash Access"):
                location = self.multiworld.find_item(name, self.player)
                self.assertTrue(location.can_reach(start), f"seed {seed}: {name} at {location.name}")


class TestLinkedEarlyLocks(unittest.TestCase):
    def test_linked_lock_is_still_early(self) -> None:
        link = [{"name": "Locks", "item_pool": ["Merchant Access", "Potion Access"], "replacement_item": None,
                 "link_replacement": None}]
        options = [{**ALL_LOCKS, "item_links": link, "early_locks": ["Merchant Access"]},
                   {**ALL_LOCKS, "item_links": link, "local_early_locks": ["Potion Access"]}]
        for seed in range(5):
            multiworld = setup_multiworld([AutoWorldRegister.world_types[GAME]] * 2, steps=(), seed=seed, options=options)
            multiworld.set_item_links()
            multiworld.state = CollectionState(multiworld)
            for step in gen_steps:
                if step == "pre_fill":
                    multiworld.link_items()
                call_all(multiworld, step)
            distribute_items_restrictive(multiworld)
            start = CollectionState(multiworld)
            for name in ("Merchant Access", "Potion Access"):
                location = next(location for location in multiworld.get_filled_locations()
                                if location.item.name == name and location.item.player in multiworld.groups)
                self.assertTrue(location.can_reach(start), f"seed {seed}: {name} at {location.name}")


class TestEarlyLocksOff(D2RTestBase):
    options = {"early_locks": ["Merchant Access"], "local_early_locks": ["Act III Waypoints"], "goal_act": 2,
               "waypoint_lock": True}

    def test_only_locks_that_are_on_count(self) -> None:
        # The Merchant Lock is off, and a two-act game has no Act III Waypoints.
        self.assertEqual(self.multiworld.early_items[self.player], {})
        self.assertEqual(self.multiworld.local_early_items[self.player], {})

    def test_later_waypoints_are_not_offered(self) -> None:
        # Acts IV and V never lock their waypoints.
        self.assertNotIn("Act IV Waypoints", self.world.options.early_locks.valid_keys)
        self.assertIn("Act III Waypoints", self.world.options.local_early_locks.valid_keys)
