from Options import OptionError

from . import D2RTestBase
from .. import SKILL_IDS, useful_skill
from ..data import CLASSES, SKILL_ROLES, SKILLS, STARTER_SKILLS
from ..options import SkillBlocklist

CLASS_OPTIONS = [name.lower() for name in CLASSES.values()] + ["any"]


class TestSkillRoles(D2RTestBase):
    def test_every_class_skill_has_a_role(self) -> None:
        roles = {"damage", "summon", "curse", "aura", "buff", "passive", "utility"}
        self.assertEqual(set(SKILL_ROLES), {sid for sid, _name, _cls in SKILLS})
        self.assertLessEqual(set(SKILL_ROLES.values()), roles)
        role = {name: SKILL_ROLES[sid] for sid, name, _cls in SKILLS}
        self.assertEqual((role["Amplify Damage"], role["Teeth"], role["Raise Skeleton"], role["Bone Armor"]),
                         ("curse", "damage", "summon", "buff"))
        self.assertEqual((role["Find Potion"], role["Bash"], role["Might"], role["Critical Strike"]),
                         ("utility", "damage", "aura", "passive"))
        self.assertEqual((role["Bone Wall"], role["Oak Sage"], role["Revive"]), ("utility", "buff", "summon"))

    def test_every_class_has_a_useful_level_one_skill(self) -> None:
        for cls, class_name in CLASSES.items():
            useful = [name for sid, name, owner in SKILLS
                      if owner == cls and sid in STARTER_SKILLS and useful_skill(f"{name} ({class_name})")]
            self.assertTrue(useful, class_name)


class TestUsefulStart(D2RTestBase):
    options = {"starting_skills": 1, "include_warlock": True}

    def check(self, extra: dict, seeds: int) -> None:
        for class_option in CLASS_OPTIONS:
            for seed in range(seeds):
                self.options = {**type(self).options, **extra, "character_class": class_option}
                self.world_setup(seed)
                starting = [item.name for item in self.multiworld.precollected_items[self.player]]
                self.assertTrue(any(useful_skill(name) for name in starting), f"{class_option} seed {seed}: {starting}")

    def test_codex_start_deals_damage_or_summons(self) -> None:
        self.check({}, 8)
        self.check({"starting_skills": 2}, 4)

    def test_codex_class_start_stays_level_one(self) -> None:
        for seed in range(10):
            self.options = {**type(self).options, "character_class": "necromancer"}
            self.world_setup(seed)
            self.assertIn(self.world.starting[0], ("Teeth (Necromancer)", "Raise Skeleton (Necromancer)"), f"seed {seed}")

    def test_tree_start_deals_damage_or_summons(self) -> None:
        self.check({"skill_mode": "tree"}, 3)
        self.check({"skill_tree_shuffle": True}, 6)

    def test_tree_barbarian_starts_with_bash(self) -> None:
        for seed in range(5):
            self.options = {**type(self).options, "skill_mode": "tree", "character_class": "barbarian"}
            self.world_setup(seed)
            self.assertEqual(self.world.starting, ["Bash (Barbarian)"], f"seed {seed}")
            self.assertEqual(self.world.fill_slot_data()["randomizer"]["start_skill"]["skill"], SKILL_IDS["Bash (Barbarian)"])

    def test_off_can_start_with_a_curse(self) -> None:
        starts = set()
        for seed in range(30):
            self.options = {**type(self).options, "character_class": "necromancer", "useful_starting_skills": False}
            self.world_setup(seed)
            starts.update(self.world.starting)
        self.assertIn("Amplify Damage (Necromancer)", starts)


class TestSkillBlocklist(D2RTestBase):
    options = {"character_class": "barbarian", "starting_skills": 3, "skill_classes": ["Barbarian", "Sorceress"],
               "skill_pool_size": 60, "skill_copies": 1,
               "skill_blocklist": ["Find Potion (Barbarian)", "Teleport (Sorceress)", "Bash (Barbarian)"]}

    def test_blocked_skills_stay_out(self) -> None:
        for seed in range(5):
            self.world_setup(seed)
            names = ({item.name for item in self.multiworld.itempool}
                     | {item.name for item in self.multiworld.precollected_items[self.player]})
            self.assertFalse(names & {"Find Potion (Barbarian)", "Teleport (Sorceress)", "Bash (Barbarian)"}, f"seed {seed}")
            self.assertIn("Telekinesis (Sorceress)", names, f"seed {seed}")
            # Bash was the Barbarian's only level-1 attack; another of his attacks starts instead.
            self.assertTrue(any(useful_skill(name) for name in self.world.starting), f"seed {seed}: {self.world.starting}")

    def test_tree_mode_leaves_blocked_skills_locked(self) -> None:
        self.options = {**type(self).options, "skill_mode": "tree"}
        self.world_setup(0)
        held = self.world.starting + self.world.skill_pool
        self.assertNotIn("Bash (Barbarian)", held)
        self.assertEqual(len(held), 28)

    def test_unknown_skill_is_refused(self) -> None:
        with self.assertRaises(OptionError) as refused:
            SkillBlocklist(["Bash (Barbarian)", "Fireball (Sorceress)"]).verify_keys()
        self.assertIn("Fireball", str(refused.exception))
