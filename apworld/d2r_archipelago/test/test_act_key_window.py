import unittest

from Fill import distribute_items_restrictive
from test.general import setup_multiworld
from worlds.AutoWorld import AutoWorldRegister

from .. import GAME, KEY_ACTS
from . import D2RTestBase


def key_spots(multiworld) -> list[tuple[int, int]]:
    """(act index of the location, act index the key unlocks) for every act key placed in a D2 world."""
    return [(location.act_index, KEY_ACTS[location.item.name]) for location in multiworld.get_filled_locations()
            if location.game == GAME and location.item.game == GAME and location.item.name in KEY_ACTS]


class TestActKeyWindow(unittest.TestCase):
    def fill(self, players: int, options: list[dict], seed: int):
        world_type = AutoWorldRegister.world_types[GAME]
        multiworld = setup_multiworld([world_type] * players, seed=seed, options=options)
        distribute_items_restrictive(multiworld)
        self.assertTrue(multiworld.can_beat_game(multiworld.state))
        return multiworld

    def test_keys_stay_within_the_window(self) -> None:
        for window in range(4):
            for seed in range(3):
                with self.subTest(window=window, seed=seed):
                    options = {"goal": "nightmare", "goal_act": 5, "act_key_window": window}
                    multiworld = self.fill(3, [options] * 3, seed)
                    spots = key_spots(multiworld)
                    self.assertEqual(len(spots), 3 * 9)
                    for location_act, key_act in spots:
                        self.assertLessEqual(location_act, key_act - 1 + window)

    def test_each_owner_uses_their_own_window(self) -> None:
        options = [{"goal": "nightmare", "goal_act": 5, "act_key_window": window} for window in (0, 4)]
        multiworld = self.fill(2, options, seed=7)
        for location in multiworld.get_filled_locations():
            item = location.item
            if location.game == GAME and item.game == GAME and item.name in KEY_ACTS and item.player == 1:
                self.assertLess(location.act_index, KEY_ACTS[item.name])

    def test_rule_reads_the_key_owners_window(self) -> None:
        world_type = AutoWorldRegister.world_types[GAME]
        options = [{"goal": "hell", "goal_act": 5, "act_key_window": window} for window in (0, 4)]
        multiworld = setup_multiworld([world_type] * 2, seed=1, options=options)
        tight, anywhere = (multiworld.worlds[player].create_item("Normal Act II Key") for player in (1, 2))
        late = multiworld.get_location("Hell: Eve of Destruction", 1)
        early = multiworld.get_location("Normal: Den of Evil", 2)
        self.assertFalse(late.item_rule(tight))
        self.assertTrue(early.item_rule(tight))
        self.assertTrue(late.item_rule(anywhere))


class TestTightWindowSolo(D2RTestBase):
    options = {"goal": "hell", "goal_act": 5, "act_key_window": 0}
