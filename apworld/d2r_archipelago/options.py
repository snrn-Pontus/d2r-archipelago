from dataclasses import dataclass

from Options import Choice, DefaultOnToggle, OptionSet, PerGameCommonOptions, Range, Toggle

from .data import CLASSES


class Goal(Choice):
    """The difficulty the game ends on. Together with Goal Act, this sets how long a game is:
    Normal with Goal Act 2 is a short game, Hell with Goal Act 5 is the full run.
    Everything past the goal is left out of the seed."""
    display_name = "Goal Difficulty"
    option_normal = 0
    option_nightmare = 1
    option_hell = 2
    default = 0


class GoalAct(Range):
    """The act whose boss ends the game on the goal difficulty: 1 Andariel, 2 Duriel, 3 Mephisto,
    4 Diablo, 5 Baal."""
    display_name = "Goal Act"
    range_start = 1
    range_end = 5
    default = 4


class CharacterClass(Choice):
    """The class you'll play. Your starting skills come from its level-1 skills, and the Codex grants
    that class's skills as class skills, usable from level 1. Skills of other classes need level 6
    (Diablo II adds 6 to the requirement of another class's skill). "any" picks starting skills from
    the whole pool. The plugin also detects your class in game, after your first kill."""
    display_name = "Character Class"
    option_any = 0
    option_amazon = 1
    option_sorceress = 2
    option_necromancer = 3
    option_paladin = 4
    option_barbarian = 5
    option_druid = 6
    option_assassin = 7
    option_warlock = 8
    default = 0


class ActKeys(DefaultOnToggle):
    """Each act after the first needs its key ("Normal Act II Key" and so on), which can be anywhere in the
    multiworld. You can still walk into a locked act, but the checks you complete there are held and only
    sent once its key arrives, so you'll sometimes wait on other players, and they on you."""
    display_name = "Act Keys"


class SkillPoolSize(Range):
    """How many different skills are shuffled into the multiworld (each one Skill Copies times).
    Capped by the number of locations."""
    display_name = "Skill Pool Size"
    range_start = 10
    range_end = 240
    default = 30


class SkillCopies(Range):
    """How many times each skill is in the pool. Every copy you find adds +1 to that skill."""
    display_name = "Skill Copies"
    range_start = 1
    range_end = 5
    default = 1


class SkillFocus(Choice):
    """Which skills go in the pool.
    any_skills: any skills from the allowed classes.
    class_weighted: half of them from your Character Class.
    few_but_deep: a third as many different skills, each with at least 3 copies."""
    display_name = "Skill Focus"
    option_any_skills = 0
    option_class_weighted = 1
    option_few_but_deep = 2
    default = 0


class SkillLevelScaling(Range):
    """Codex skills gain +1 level for every this many character levels (on top of copies and
    Codex Empowerments), so a skill found early keeps up. 0 turns scaling off."""
    display_name = "Skill Level Scaling"
    range_start = 0
    range_end = 10
    default = 3


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
    default = 1


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
    default = 2


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


class MonsterShuffle(Choice):
    """Shuffles which monsters live in which areas (all three difficulties).
    Nearby: areas trade monsters within the same act, so the challenge stays about the same.
    Anywhere: any area can get any act's monsters; on Normal each one is swapped for the version of its
    family nearest the area's level, so early areas stay beatable."""
    display_name = "Monster Shuffle"
    option_off = 0
    option_nearby = 1
    option_anywhere = 2
    default = 1


class SuperUniqueShuffle(Choice):
    """Gives field superuniques (Bishibosh, Rakanishu, Pindleskin...) another superunique's monster type.
    Quest bosses, act bosses, the Chaos Sanctuary seal bosses and Baal's waves are left alone.
    Nearby: only swaps between monsters of similar strength. Anywhere: any swap."""
    display_name = "Superunique Shuffle"
    option_off = 0
    option_nearby = 1
    option_anywhere = 2
    default = 1


class MonsterDensity(Range):
    """How many monsters each area spawns, in percent."""
    display_name = "Monster Density"
    range_start = 50
    range_end = 400
    default = 100


class ChampionPacks(Range):
    """How many champion and unique monster packs each area spawns, in percent."""
    display_name = "Champion Packs"
    range_start = 50
    range_end = 400
    default = 100


class BetterDrops(Range):
    """How often monsters drop uniques, set items and rares, in percent."""
    display_name = "Better Drops"
    range_start = 100
    range_end = 500
    default = 150


class RunewordShuffle(Toggle):
    """Gives every runeword a new recipe of similar-tier runes. The setup tool writes the new
    recipes to runewords.txt in the mod folder."""
    display_name = "Runeword Shuffle"


class ExperienceMultiplier(Range):
    """Monster experience, in percent. Raise it for shorter games."""
    display_name = "Experience Multiplier"
    range_start = 50
    range_end = 500
    default = 150


class ItemLevelRequirements(DefaultOnToggle):
    """Items keep their level requirements. Turn off to wear anything you find."""
    display_name = "Item Level Requirements"


class ItemStatRequirements(DefaultOnToggle):
    """Items keep their strength and dexterity requirements."""
    display_name = "Item Stat Requirements"


@dataclass
class D2ROptions(PerGameCommonOptions):
    goal: Goal
    goal_act: GoalAct
    character_class: CharacterClass
    act_keys: ActKeys
    skill_pool_size: SkillPoolSize
    skill_copies: SkillCopies
    skill_focus: SkillFocus
    skill_level_scaling: SkillLevelScaling
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
    monster_shuffle: MonsterShuffle
    superunique_shuffle: SuperUniqueShuffle
    runeword_shuffle: RunewordShuffle
    monster_density: MonsterDensity
    champion_packs: ChampionPacks
    better_drops: BetterDrops
    experience_multiplier: ExperienceMultiplier
    item_level_requirements: ItemLevelRequirements
    item_stat_requirements: ItemStatRequirements
