import unittest

from Utils import parse_yaml

from .. import GAME, options
from ..options import OPTION_PRESETS, D2ROptions
from ..options_editor import (APPLIES, COMMAND_TRAPS, OPTIONS, grouped_options, inactive, linked_changes, load_options,
                              name_problem, needed_toggle, option_kind, preset_values, read_value, render_yaml,
                              save_options)

FILE = """# My settings.
name: Old
game: Diablo II Resurrected

Diablo II Resurrected:
  # Length
  goal: normal              # normal, nightmare, hell
  goal_act:
    3: 50
    random: 0
  skill_classes:
    - Amazon

  # Locks
  cube_lock: false
  monster_shuffle: {nearby: 1, anywhere: 1}

A Link to the Past:
  goal: ganon
"""


class TestOptionsEditor(unittest.TestCase):
    def test_every_option_has_a_control_and_a_group(self) -> None:
        for name, option in D2ROptions.type_hints.items():
            if option.__module__ == options.__name__:
                with self.subTest(option=name):
                    self.assertIsNotNone(option_kind(option))
                    self.assertIn(name, OPTIONS)
                    self.assertTrue(option.__doc__)
        listed = [name for _, names in grouped_options() for name in names]
        self.assertCountEqual(listed, OPTIONS)

    def test_tree_shuffle_and_skill_mode_go_together(self) -> None:
        tree, codex = options.SkillMode.option_tree, options.SkillMode.option_codex
        self.assertEqual(linked_changes("skill_tree_shuffle", True), {"skill_mode": tree})
        self.assertEqual(linked_changes("skill_tree_shuffle", False), {})
        self.assertEqual(linked_changes("skill_mode", codex), {"skill_tree_shuffle": False})
        self.assertEqual(linked_changes("skill_mode", tree), {})
        self.assertEqual(linked_changes("goal", 2), {})
        # An opened file with the old combination shows tree mode, and saving writes it.
        data = {"name": "Old", "game": GAME, GAME: {"skill_mode": "codex", "skill_tree_shuffle": True}}
        values, kept = load_options(data)
        self.assertEqual(values["skill_mode"], tree)
        self.assertEqual(save_options(data, "Old", values, kept)[GAME]["skill_mode"], "tree")

    def test_dependent_options_name_real_options(self) -> None:
        for name, (parents, needs, _applies) in APPLIES.items():
            self.assertIn(name, OPTIONS)
            self.assertTrue(needs.startswith("Needs "), name)
            for parent in parents:
                self.assertIn(parent, OPTIONS, name)

    def test_dependent_options_only_read_their_parents(self) -> None:
        # A parent the file sets to weights or random keeps its dependents active, so every option a rule reads
        # must be listed as a parent.
        class Reads(dict):
            def __init__(self, values: dict) -> None:
                super().__init__(values)
                self.read: set[str] = set()

            def __getitem__(self, key: str):
                self.read.add(key)
                return super().__getitem__(key)

        presets = [load_options({})[0], *(preset_values(preset) for preset in OPTION_PRESETS)]
        tree = {**presets[0], "skill_mode": options.SkillMode.option_tree, "death_link": True, "goal": 2}
        for values in [*presets, tree]:
            for name, (parents, _needs, applies) in APPLIES.items():
                reads = Reads(values)
                applies(reads)
                self.assertLessEqual(reads.read, set(parents), name)

    def test_greyed_out_options(self) -> None:
        values, kept = load_options({})
        # Defaults: Normal goal, Codex mode, no DeathLink.
        for name in ("cow_level_checks", "uber_checks", "monster_density_hell", "death_experience_penalty",
                     "skill_points", "death_link_mode", "death_link_amnesty", "monster_shuffle_tolerance"):
            self.assertIsNotNone(inactive(name, values, kept), name)
        for name in ("act_keys", "skill_pool_size", "level_check_step", "goal", "shuffle_cow_level"):
            self.assertIsNone(inactive(name, values, kept), name)
        self.assertEqual(inactive("death_link_mode", values, kept), "Needs DeathLink")

        tree = {**values, "skill_tree_shuffle": True}
        self.assertIsNotNone(inactive("skill_pool_size", tree, kept))
        self.assertIsNone(inactive("skill_points", tree, kept))

        # DeathLink's random_trap sets off traps with Trap Percentage at 0 (but never a Junk Backpack).
        no_traps = {**values, "trap_percentage": 0}
        self.assertIsNotNone(inactive("ambush_size", no_traps, kept))
        random_trap = {**no_traps, "death_link": True, "death_link_mode": options.DeathLinkMode.option_random_trap}
        for name in ("ambush_size", "ambush_weight", "tax_collector_gold", "traps_wait_for_town"):
            self.assertIsNone(inactive(name, random_trap, kept), name)
        self.assertIsNotNone(inactive("junk_backpack_weight", random_trap, kept))
        # A trap at weight 0 never goes off, unless every weight is 0 and random_trap falls back to the five.
        no_ambush = {**random_trap, "ambush_weight": 0, "poison_cloud_weight": 5}
        self.assertIsNotNone(inactive("ambush_size", no_ambush, kept))
        weights = {weight: 0 for weight in COMMAND_TRAPS}
        fallback = {**random_trap, **weights}
        for name in ("ambush_size", "tax_collector_gold", "traps_wait_for_town"):
            self.assertIsNone(inactive(name, fallback, kept), name)
        self.assertIsNone(inactive("tax_collector_gold", {**fallback, "death_link_mode": 2}, kept))
        # Junk Backpacks alone never wait for town.
        junk_only = {**values, **weights, "trap_percentage": 20, "junk_backpack_weight": 10}
        self.assertIsNotNone(inactive("traps_wait_for_town", junk_only, kept))
        self.assertIsNone(inactive("junk_backpack_size", junk_only, kept))

        # A one-act game has no act keys; a parent set to random keeps its dependents.
        one_act = {**values, "goal_act": 1}
        self.assertIsNotNone(inactive("act_keys", one_act, kept))
        self.assertIsNone(inactive("act_keys", one_act, {"goal_act": "random"}))
        self.assertIsNone(inactive("death_link_mode", values, {"death_link": "random"}))

    def test_early_locks_need_their_lock(self) -> None:
        for name in ("early_locks", "local_early_locks"):
            for key in OPTIONS[name].valid_keys:
                self.assertIs(option_kind(OPTIONS[needed_toggle(name, key)]), "toggle", key)
        self.assertEqual(needed_toggle("early_locks", "Merchant Access"), "merchant_lock")
        self.assertEqual(needed_toggle("local_early_locks", "Act II Waypoints"), "waypoint_lock")
        self.assertIsNone(needed_toggle("skill_classes", "Barbarian"))

    def test_reads_plain_and_template_values(self) -> None:
        values, kept = load_options({GAME: {
            "goal": "hell",
            "goal_act": {3: 50, "random": 0, "random-low": 0},
            "act_key_window": "anywhere",
            "cube_lock": "true",
            "area_checks": False,
            "skill_classes": ["Sorceress", "Amazon"],
        }})
        self.assertEqual(kept, {})
        self.assertEqual(values["goal"], 2)
        self.assertEqual(values["goal_act"], 3)
        self.assertEqual(values["act_key_window"], 4)
        self.assertIs(values["cube_lock"], True)
        self.assertIs(values["area_checks"], False)
        self.assertEqual(values["skill_classes"], ["Amazon", "Sorceress"])
        self.assertEqual(values["shop_checks"], 3)
        # Named values outside the range ("same" is 0, below 50) are kept, and are the defaults.
        self.assertEqual(values["monster_density_hell"], 0)
        self.assertEqual(values["main_path_superunique_minions"], 5)

    def test_named_values_outside_a_range(self) -> None:
        from ..options_editor import snap_range, write_value
        option = options.MainPathSuperUniqueMinions
        self.assertEqual(read_value(option, "off"), -1)
        self.assertEqual(write_value(option, -1), "off")
        density = options.MonsterDensityNightmare
        self.assertEqual(read_value(density, "same"), 0)
        self.assertEqual(write_value(density, 0), "same")
        # A slider between "same" (0) and the range's start (50) snaps to the nearer one.
        self.assertEqual((snap_range(density, 10), snap_range(density, 40), snap_range(density, 120)), (0, 50, 120))

    def test_item_counts(self) -> None:
        from ..options_editor import write_value
        option = options.StartingItems
        self.assertEqual(option_kind(option), "counter")
        self.assertEqual(read_value(option, {"Ber Rune": 1, "Full Rejuvenation Potion": 3, "El Rune": 0}),
                         {"Ber Rune": 1, "Full Rejuvenation Potion": 3})
        self.assertEqual(read_value(option, {}), {})
        # Unknown items, counts past the limit and non-numbers are kept as the file has them.
        self.assertIsNone(read_value(option, {"Horadric Cube": 1}))
        self.assertIsNone(read_value(option, {"Ber Rune": 21}))
        self.assertIsNone(read_value(option, {"Ber Rune": "one"}))
        self.assertEqual(write_value(option, {"Zod Rune": 2, "El Rune": 0, "Antidote Potion": 1}), {"Antidote Potion": 1, "Zod Rune": 2})
        values, kept = load_options({})
        self.assertEqual(values["starting_items"], {})
        values["starting_items"] = {"Ber Rune": 1}
        saved, text = render_yaml(FILE, parse_yaml(FILE), "Snrn", values, load_options(parse_yaml(FILE))[1])
        self.assertEqual(parse_yaml(text), saved)
        self.assertEqual(saved[GAME]["starting_items"], {"Ber Rune": 1})
        self.assertEqual(load_options(saved)[0]["starting_items"], {"Ber Rune": 1})

    def test_keeps_what_it_cannot_show(self) -> None:
        section = {"goal": {"normal": 50, "hell": 50}, "goal_act": "random-high", "monster_shuffle": "sideways",
                   "skill_classes": ["Paladin", "Clown"], "shop_checks": 99}
        values, kept = load_options({GAME: section})
        self.assertEqual(kept, section)
        self.assertEqual(values["goal"], 0)
        saved = save_options({"name": "Old", GAME: section}, "Snrn", values, kept)
        self.assertEqual({key: saved[GAME][key] for key in section}, section)

    def test_saves_names_and_keeps_the_rest_of_the_file(self) -> None:
        values, kept = load_options({})
        values["goal"] = 1
        values["act_key_window"] = 4
        values["goal_act"] = 5
        data = {"name": "Old", "description": "mine", "requires": {"version": "0.6.4"}, "A Link to the Past": {"x": 1},
                GAME: {"not_an_option": 7}}
        saved = save_options(data, "Snrn", values, kept)
        self.assertEqual(saved["name"], "Snrn")
        self.assertEqual(saved["game"], GAME)
        self.assertEqual(saved["description"], "mine")
        self.assertEqual(saved["requires"], {"version": "0.6.4"})
        self.assertEqual(saved["A Link to the Past"], {"x": 1})
        self.assertEqual(saved[GAME]["not_an_option"], 7)
        self.assertEqual(saved[GAME]["goal"], "nightmare")
        self.assertEqual(saved[GAME]["act_key_window"], "anywhere")
        self.assertEqual(saved[GAME]["goal_act"], 5)
        # Options the file leaves out stay out while they're at their default.
        self.assertNotIn("shop_checks", saved[GAME])
        self.assertEqual(data["name"], "Old")
        # What it writes reads back the same.
        self.assertEqual(load_options(saved), (values, {}))

    def test_saving_keeps_comments_and_untouched_lines(self) -> None:
        data = parse_yaml(FILE)
        values, kept = load_options(data)
        self.assertEqual(kept, {"monster_shuffle": {"nearby": 1, "anywhere": 1}})
        values.update(goal=2, goal_act=5, cube_lock=True, skill_classes=["Amazon", "Paladin"], shop_checks=4)
        saved, text = render_yaml(FILE, data, "Snrn", values, kept)
        self.assertEqual(parse_yaml(text), saved)
        self.assertEqual(text, FILE
                         .replace("name: Old", "name: Snrn")
                         .replace("goal: normal  ", "goal: hell    ")
                         .replace("goal_act:\n    3: 50\n    random: 0\n", "goal_act: 5\n")
                         .replace("skill_classes:\n    - Amazon\n", "skill_classes: [Amazon, Paladin]\n")
                         .replace("cube_lock: false", "cube_lock: true")
                         .replace("anywhere: 1}\n", "anywhere: 1}\n  shop_checks: 4\n"))
        self.assertEqual(load_options(saved), (values, kept))

    def test_saving_unchanged_keeps_the_file(self) -> None:
        data = parse_yaml(FILE)
        values, kept = load_options(data)
        self.assertEqual(render_yaml(FILE, data, "Old", values, kept)[1], FILE)
        crlf = FILE.replace("\n", "\r\n")
        self.assertEqual(render_yaml(crlf, data, "Old", values, kept)[1], crlf)

    def test_new_option_goes_before_the_next_sections_comment(self) -> None:
        text = "name: Old\ngame: Diablo II Resurrected\nDiablo II Resurrected:\n  goal: normal\n\n# Other games\nFoo: {}\n"
        data = parse_yaml(text)
        values, kept = load_options(data)
        values["shop_checks"] = 4
        saved, patched = render_yaml(text, data, "Old", values, kept)
        self.assertEqual(patched, text.replace("goal: normal\n", "goal: normal\n  shop_checks: 4\n"))

    def test_new_file_and_fallback(self) -> None:
        values, kept = load_options({})
        saved, text = render_yaml(None, {}, "Snrn", values, kept)
        self.assertEqual(parse_yaml(text), saved)
        self.assertEqual(list(saved)[:3], ["name", "game", "description"])
        self.assertEqual(load_options(saved), (values, {}))
        # A file the patch can't handle (the game's options in flow style) is written out whole.
        flow = "name: Old\ngame: Diablo II Resurrected\nDiablo II Resurrected: {goal: normal}\n"
        values["goal"] = 1
        saved, text = render_yaml(flow, parse_yaml(flow), "Old", values, kept)
        self.assertEqual(parse_yaml(text), saved)
        self.assertEqual(saved[GAME], {"goal": "nightmare"})

    def test_presets(self) -> None:
        self.assertEqual(preset_values(None), load_options({})[0])
        for preset, settings in OPTION_PRESETS.items():
            values = preset_values(preset)
            for name, raw in settings.items():
                with self.subTest(preset=preset, option=name):
                    self.assertEqual(values[name], read_value(OPTIONS[name], raw))
                    self.assertIsNotNone(values[name])

    def test_player_name(self) -> None:
        self.assertIsNotNone(name_problem(" "))
        self.assertIsNotNone(name_problem("x" * 17))
        self.assertIsNone(name_problem("Snrn"))
