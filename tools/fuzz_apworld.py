"""Generates many multiworlds with random Diablo II Resurrected options and checks each one fills and is
beatable. Run from an Archipelago source checkout that has worlds/d2r_archipelago:

    python tools/fuzz_apworld.py <archipelago-folder> [rounds]
"""
import os
import random
import sys
import traceback


def random_options(world_type, rng: random.Random) -> dict:
    from Options import Choice, OptionSet, Range, Toggle
    options = {}
    for key, option in world_type.options_dataclass.type_hints.items():
        if key in ("progression_balancing", "accessibility", "local_items", "non_local_items", "start_inventory",
                   "start_hints", "start_location_hints", "exclude_locations", "priority_locations", "item_links",
                   "plando_items", "start_inventory_from_pool"):
            continue
        if issubclass(option, Range):
            options[key] = rng.randint(option.range_start, option.range_end)
        elif issubclass(option, Toggle):
            options[key] = rng.random() < 0.5
        elif issubclass(option, Choice):
            options[key] = rng.choice([name for name in option.options if name != "random"])
        elif issubclass(option, OptionSet):
            keys = sorted(option.valid_keys)
            options[key] = rng.sample(keys, rng.randint(0, len(keys)))
    if rng.random() < 0.15:
        # A tiny game now and then: one act with few or no optional checks, where keys, locks and
        # skills compete for a handful of locations.
        options.update(goal="normal", goal_act=1, area_checks=rng.random() < 0.3, side_area_checks=rng.choice([0, 0, 1]),
                       level_checks=False, superunique_checks=rng.random() < 0.3, quest_item_checks=rng.random() < 0.5,
                       shop_checks=rng.choice([0, 0, 1, 2]))
    return options


def main() -> int:
    root = os.path.abspath(sys.argv[1])
    rounds = int(sys.argv[2]) if len(sys.argv) > 2 else 200
    os.chdir(root)
    sys.path.insert(0, root)
    # Load the worlds first: importing Fill before them makes some worlds' imports circular.
    from worlds.AutoWorld import AutoWorldRegister, call_all
    import worlds  # noqa: F401
    from Fill import distribute_items_restrictive
    from Options import OptionError
    from test.general import setup_multiworld

    world_type = AutoWorldRegister.world_types["Diablo II Resurrected"]
    rng = random.Random(1234)
    failures = refused = 0
    for round_number in range(rounds):
        players = rng.randint(1, 3)
        options = [random_options(world_type, rng) for _ in range(players)]
        seed = rng.getrandbits(32)
        try:
            try:
                multiworld = setup_multiworld([world_type] * players, seed=seed, options=options)
            except OptionError as error:
                # The world refused the options with a message, which is the right answer for an
                # impossible seed (more keys and locks than locations).
                refused += 1
                print(f"round {round_number} seed {seed} refused: {error}")
                continue
            distribute_items_restrictive(multiworld)
            call_all(multiworld, "post_fill")
            if not multiworld.can_beat_game(multiworld.state):
                raise AssertionError("not beatable")
            for player in multiworld.player_ids:
                multiworld.worlds[player].fill_slot_data()
        except Exception as error:
            failures += 1
            print(f"round {round_number} seed {seed} FAILED: {error!r}")
            for player, opts in enumerate(options, 1):
                print(f"  player {player}: {opts}")
            if failures <= 3:
                traceback.print_exc()
    print(f"{rounds - failures - refused}/{rounds} rounds generated and were beatable, {refused} refused with a message")
    return 1 if failures else 0


if __name__ == "__main__":
    sys.exit(main())
