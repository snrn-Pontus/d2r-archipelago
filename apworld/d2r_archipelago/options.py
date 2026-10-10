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


class CodexEmpowerments(Range):
    """How many Codex Empowerments are in the pool. Each one adds +1 to every skill in your Codex."""
    display_name = "Codex Empowerments"
    range_start = 0
    range_end = 10
    default = 3


class BossQuestsGiveUniques(DefaultOnToggle):
    """The act boss quests (Andariel, Duriel, Mephisto, Diablo, Baal) always reward you with a
    Random Unique, scaled to your level, instead of an item from the multiworld."""
    display_name = "Boss Quests Give Uniques"


class BasicLootWeight(Range):
    """Weight of potions, perfect gems, and low and mid runes among the filler items."""
    display_name = "Basic Loot Weight"
    range_start = 0
    range_end = 100
    default = 40


class UniqueWeight(Range):
    """Weight of Random Unique Weapon / Armor / Jewelry among the filler items. Uniques are rolled
    when you receive them, from those your character level can use."""
    display_name = "Unique Weight"
    range_start = 0
    range_end = 100
    default = 15


class SetItemWeight(Range):
    """Weight of Random Set Items among the filler items, scaled to your level like uniques."""
    display_name = "Set Item Weight"
    range_start = 0
    range_end = 100
    default = 10


class HighRuneWeight(Range):
    """Weight of High Runes (Pul through Zod; the higher, the rarer) among the filler items."""
    display_name = "High Rune Weight"
    range_start = 0
    range_end = 100
    default = 3


class SocketedBaseWeight(Range):
    """Weight of Socketed Bases among the filler items: a white weapon or armor with three or more
    sockets, ready for a runeword."""
    display_name = "Socketed Base Weight"
    range_start = 0
    range_end = 100
    default = 7


class JunkTrapPercentage(Range):
    """Percentage of filler items replaced by Junk Backpack traps, which stuff your free inventory
    space with worthless gear."""
    display_name = "Junk Trap Percentage"
    range_start = 0
    range_end = 50
    default = 0


@dataclass
class D2ROptions(PerGameCommonOptions):
    goal: Goal
    skill_pool_size: SkillPoolSize
    starting_skills: StartingSkills
    skill_classes: SkillClasses
    skills_per_act: SkillsPerAct
    area_checks: AreaChecks
    level_checks: LevelChecks
    codex_empowerments: CodexEmpowerments
    boss_quests_give_uniques: BossQuestsGiveUniques
    basic_loot_weight: BasicLootWeight
    unique_weight: UniqueWeight
    set_item_weight: SetItemWeight
    high_rune_weight: HighRuneWeight
    socketed_base_weight: SocketedBaseWeight
    junk_trap_percentage: JunkTrapPercentage
