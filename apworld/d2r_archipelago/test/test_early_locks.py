from BaseClasses import CollectionState
from Fill import distribute_items_restrictive

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


class TestEarlyLocksOff(D2RTestBase):
    options = {"early_locks": ["Merchant Access"], "local_early_locks": ["Act III Waypoints"], "goal_act": 2,
               "waypoint_lock": True}

    def test_only_locks_that_are_on_count(self) -> None:
        # The Merchant Lock is off, and a two-act game has no Act III Waypoints.
        self.assertEqual(self.multiworld.early_items[self.player], {})
        self.assertEqual(self.multiworld.local_early_items[self.player], {})
