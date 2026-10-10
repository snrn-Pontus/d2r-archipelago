from dataclasses import dataclass

from Options import Choice, DefaultOnToggle, OptionSet, PerGameCommonOptions, Range

from .data import CLASSES


class Goal(Choice):
    """Which difficulty's Baal kill (Eve of Destruction) wins the game.
    Quests on harder difficulties are left out of the seed."""
    display_name = "Goal"
    option_normal = 0
    option_nightmare = 1
    option_hell = 2
    default = 0


class SkillPoolSize(Range):
    """How many skills are shuffled into the multiworld. Capped at the number of locations."""
    display_name = "Skill Pool Size"
    range_start = 10
    range_end = 240
    default = 60


class StartingSkills(Range):
    """How many skills from the pool you start with."""
    display_name = "Starting Skills"
    range_start = 0
    range_end = 10
    default = 2


class SkillClasses(OptionSet):
    """Which classes' skill trees the pool draws from. Empty means every class."""
    display_name = "Skill Classes"
    valid_keys = sorted(CLASSES.values())
    default = frozenset()


class SkillsPerAct(Range):
    """Logic: how many skills you must have received for each act past the first.
    Act 3 of Normal with the default needs 4 skills, Act 1 of Nightmare needs 10."""
    display_name = "Skills Per Act"
    range_start = 0
    range_end = 6
    default = 2


class AreaChecks(DefaultOnToggle):
    """Entering each waypoint area (and a few landmark areas) for the first time is a check."""
    display_name = "Area Checks"


class LevelChecks(DefaultOnToggle):
    """Reaching every fifth character level is a check, up to a cap set by the goal
    (30 on Normal, 60 on Nightmare, 90 on Hell)."""
    display_name = "Level Checks"


@dataclass
class D2ROptions(PerGameCommonOptions):
    goal: Goal
    skill_pool_size: SkillPoolSize
    starting_skills: StartingSkills
    skill_classes: SkillClasses
    skills_per_act: SkillsPerAct
    area_checks: AreaChecks
    level_checks: LevelChecks
