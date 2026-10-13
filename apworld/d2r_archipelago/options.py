from dataclasses import dataclass

from typing import Any

from Options import Choice, DeathLink, DefaultOnToggle, NamedRange, OptionGroup, OptionSet, PerGameCommonOptions, Range, Toggle

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
    """The class you'll play: your starting skills come from its level-1 skills. Codex skills of your own
    class work from level 1; other classes' skills need level 6 (Diablo II adds 6 to the requirement of
    another class's skill). "any" picks starting skills from the whole pool.
    Skills your class can't use stay out of the pool and the tree shuffle: claw skills need an Assassin,
    two-weapon skills a Barbarian (or an Assassin with claws), the Assassin's traps a Barbarian or
    Assassin, and wolf and bear attacks (Feral Rage, Maul...) a Druid who also gets that form. With "any",
    they all stay out."""
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


class SkillMode(Choice):
    """How skills reach you.
    codex: every skill you receive goes on your Archipelago Codex charms, from any class, at a level set
    by copies, Codex Empowerments and your character level.
    tree: the skills are the 30 in your class's skill tree (shuffled with Skill Tree Shuffle). Each one
    stays locked until its item arrives; then you spend skill points on it as usual. Extra copies and
    Skill Points give skill points. Skill Pool Size, Skill Focus, Skill Classes, Skill Level Scaling and
    Codex Empowerments don't apply. With "any" class, the seed picks one."""
    display_name = "Skill Mode"
    option_codex = 0
    option_tree = 1
    default = 0


class SkillPoints(Range):
    """Tree mode: Skill Point items in the pool, each worth one skill point."""
    display_name = "Skill Points"
    range_start = 0
    range_end = 30
    default = 6


class ActKeys(DefaultOnToggle):
    """Each act after the first needs its key ("Normal Act II Key" and so on), which can be anywhere in the
    multiworld. You can still walk into a locked act, but the checks you complete there are held and only
    sent once its key arrives, so you'll sometimes wait on other players, and they on you."""
    display_name = "Act Keys"


class SkillPoolSize(Range):
    """How many different skills are shuffled into the multiworld (each one Skill Copies times), for a
    four-act game. With Scale With Length, longer games get proportionally more. Capped by the number
    of locations."""
    display_name = "Skill Pool Size"
    range_start = 10
    range_end = 240
    default = 16


class SkillCopies(Range):
    """How many times each skill is in the pool. Every copy you find adds +1 to that skill."""
    display_name = "Skill Copies"
    range_start = 1
    range_end = 5
    default = 2


class SkillFocus(Choice):
    """Which skills go in the pool.
    any_skills: any skills from the allowed classes.
    class_weighted: half of them from your Character Class.
    few_but_deep: a third as many different skills, each with at least 3 copies."""
    display_name = "Skill Focus"
    option_any_skills = 0
    option_class_weighted = 1
    option_few_but_deep = 2
    default = 1


class SkillLevelScaling(Range):
    """Codex skills gain +1 level for every this many character levels (on top of copies and
    Codex Empowerments), so a skill found early keeps up. 0 turns scaling off."""
    display_name = "Skill Level Scaling"
    range_start = 0
    range_end = 10
    default = 5


class SkillScalingCap(Range):
    """The most levels Skill Level Scaling can add, so copies and Codex Empowerments still matter late
    in the game. 0 means no cap."""
    display_name = "Skill Scaling Cap"
    range_start = 0
    range_end = 30
    default = 10


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


class IncludeWarlock(Toggle):
    """Put Warlock skills in the pool. Only for characters created in the Reign of the Warlock expansion:
    Lord of Destruction characters can't use them (the plugin leaves them off their Codex)."""
    display_name = "Include Warlock"


class SkillsPerAct(Range):
    """Logic: how many skills you must have received for each act past the first.
    Act 3 of Normal with the default needs 4 skills, Act 1 of Nightmare needs 10."""
    display_name = "Skills Per Act"
    range_start = 0
    range_end = 6
    default = 1


class AreaChecks(DefaultOnToggle):
    """Entering each waypoint area (and a few landmark areas) for the first time is a check, on every
    difficulty in play."""
    display_name = "Area Checks"


class SideAreaChecks(Range):
    """How many side areas are checks on each difficulty in play: the bottom of an optional dungeon you'd
    usually skip, like the Mausoleum, the Ancient Tunnels, the Kurast temples or Abaddon. Each difficulty
    picks its own at random, so Normal, Nightmare and Hell send you to different places. The check is
    entering the area. 0 turns them off; 28 makes every side area a check on every difficulty."""
    display_name = "Side Area Checks"
    range_start = 0
    range_end = 28
    default = 5


class LevelChecks(DefaultOnToggle):
    """Reaching every fifth character level is a check, up to a cap set by the goal
    (30 on Normal, 60 on Nightmare, 90 on Hell). Past level 70 they only hold filler."""
    display_name = "Level Checks"


class SuperUniqueChecks(DefaultOnToggle):
    """Killing each of the 31 superuniques that roam the field (Bishibosh, Rakanishu, Pindleskin...) is
    a check on every difficulty in play. Quest bosses and the Chaos Sanctuary seal bosses aren't included.
    Needs the seed's mod from d2r-ap-setup, which gives each of them a monster of its own."""
    display_name = "Superunique Checks"


class QuestItemChecks(DefaultOnToggle):
    """Picking up quest items is a check on every difficulty in play: the Scroll of Inifuss, Wirt's Leg,
    the Horadric Scroll, the Staff of Kings, the Book of Skill, Khalim's Eye, Heart, Brain and Flail, the
    Golden Bird, the Potion of Life, Mephisto's Soulstone, the Hellforge Hammer and the Scroll of
    Resistance. The Horadric Cube counts once."""
    display_name = "Quest Item Checks"


class ShopChecks(Range):
    """Each act's potion seller (Akara, Lysander, Ormus, Jamella, Malah) sells Archipelago Parcels; buying
    one is a check, up to this many per act. They cost more in later acts."""
    display_name = "Shop Checks"
    range_start = 0
    range_end = 5
    default = 3


class UberChecks(Toggle):
    """With a Hell goal through Baal: killing each of Pandemonium's six uber bosses (Lilith, Uber Duriel,
    Uber Izual, Uber Mephisto, Uber Diablo, Uber Baal) is a check. Only filler goes on these."""
    display_name = "Uber Checks"


class KillCountChecks(Toggle):
    """Killing 100, 250 and 500 monsters in each act on each difficulty is a check (counted across
    sessions). Only filler goes on these."""
    display_name = "Kill Count Checks"


class FindCountChecks(Toggle):
    """Finding 1, 5, 10, 25 and 50 unique items, and 1, 5, 10 and 25 set items, are checks. Items you
    receive from Archipelago don't count. Only filler goes on these."""
    display_name = "Find Count Checks"


class EnforceActKeys(DefaultOnToggle):
    """With Act Keys, walking into an act you don't have the key for sends you back to the town of an act
    you can be in, instead of just holding that act's checks."""
    display_name = "Enforce Act Keys"


class ActKeyWindow(NamedRange):
    """How far into a Diablo II world (another player's or yours) each of your act keys can land, counted
    from the act it unlocks. The key for your Act N may only be at their Act N - 1 + this or earlier, so
    with 1 your Normal Act II Key is somewhere in their Normal Act I or II, and a Nightmare key can sit
    in their Normal Act V. 0 keeps every key in an act before the one it opens. 4 (anywhere) turns the
    limit off. Keys going to other games can still be anywhere."""
    display_name = "Act Key Window"
    range_start = 0
    range_end = 4
    default = 1
    special_range_names = {"anywhere": 4}


class CubeLock(Toggle):
    """The Horadric Cube won't open until you find Horadric Cube Access. You need it for the Horadric
    Staff and Khalim's Will, so logic expects it before Duriel."""
    display_name = "Cube Lock"


class TownPortalLock(Toggle):
    """Town Portal scrolls and tomes can't be used until you find Town Portal Access."""
    display_name = "Town Portal Lock"


class PotionLock(Toggle):
    """Potions can't be drunk until you find Potion Access."""
    display_name = "Potion Lock"


class StashLock(Toggle):
    """Your stash won't open until you find Stash Access."""
    display_name = "Stash Lock"


class MerchantLock(Toggle):
    """Merchants won't trade (buy, sell, repair or gamble) until you find Merchant Access. Shop Checks
    then wait for it too, and so does the Secret Cow Level (its portal takes a Tome of Town Portal)."""
    display_name = "Merchant Lock"


class WaypointLock(Toggle):
    """The waypoints of Acts I to III only work once you find that act's Waypoints item (Act I Waypoints...).
    Acts IV and V keep theirs, since a waypoint is the only way back to an earlier act from there."""
    display_name = "Waypoint Lock"


class CainHints(DefaultOnToggle):
    """Deckard Cain gives you a hint each time you finish an act boss quest."""
    display_name = "Cain Hints"


class PageHintChance(Range):
    """Chance, in percent, that a superunique drops a torn page with a hint the first time you kill it."""
    display_name = "Page Hint Chance"
    range_start = 0
    range_end = 100
    default = 25


class ProphecyShop(DefaultOnToggle):
    """Every act's potion seller sells Scrolls of Prophecy, each a hint. Each one costs more than the last
    (1,000 gold up to 100,000); the vendor lists every price, and only the next one counts (a cheaper scroll
    is refunded)."""
    display_name = "Prophecy Shop"


class HintBalance(Range):
    """Percent of the hints you find that point at other players' important items in your world (they're told
    too). The rest tell you where your own important items are, act keys first."""
    display_name = "Hint Balance"
    range_start = 0
    range_end = 100
    default = 30


class ProphecyWeight(Range):
    """Weight of Prophecies among the filler items: a hint, as if you'd read a Scroll of Prophecy."""
    display_name = "Prophecy Weight"
    range_start = 0
    range_end = 100
    default = 4


class CowLevelChecks(DefaultOnToggle):
    """On each difficulty you finish (Baal beaten) before the goal: entering the Secret Cow Level, killing
    The Cow King, and killing 100 cows in one session are checks. Killing the Cow King closes the portal
    for that difficulty, so get the other two first."""
    display_name = "Cow Level Checks"


class RuneChecks(Range):
    """Finding each rune from El up to this one is a check (8 = up to Ral, 20 = up to Lem, 33 = all the
    way to Zod; 0 = none). Only filler goes on these, so high runes never hold up anyone else.
    Runes you receive from Archipelago don't count."""
    display_name = "Rune Checks"
    range_start = 0
    range_end = 33
    default = 0


class GemChecks(Toggle):
    """Finding each of the 35 gems (every type and grade, skulls included) is a check. Only filler
    goes on these. Gems you receive from Archipelago don't count."""
    display_name = "Gem Checks"


class SetChecks(Toggle):
    """Holding every piece of a Lord of Destruction set at once (worn or anywhere in your bags or stash)
    is a check, one per set (32). Only filler goes on these. Set items you receive from Archipelago
    don't count."""
    display_name = "Set Checks"


class SpecialItemChecks(Toggle):
    """With a Hell goal: finding each Pandemonium key, each uber organ, each essence, the Token of
    Absolution, the Hellfire Torch and Annihilus is a check (13). Only filler goes on these."""
    display_name = "Special Item Checks"


class ScaleWithLength(DefaultOnToggle):
    """Skill Pool Size and Codex Empowerments are for a four-act game (Normal to Diablo); longer games get
    more of both in proportion, so a full Hell run (15 acts) has almost four times as many. Turn off to use
    the numbers as they are."""
    display_name = "Scale With Length"


class CodexEmpowerments(Range):
    """How many Codex Empowerments are in the pool, for a four-act game (more for longer games with Scale
    With Length). Each one adds +1 to every skill in your Codex."""
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
    when you receive them, from those your character level and class can use."""
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


class GoldWeight(Range):
    """Weight of gold among the filler items: a pile worth 1,000 gold per character level."""
    display_name = "Gold Weight"
    range_start = 0
    range_end = 100
    default = 8


class ExperienceWeight(Range):
    """Weight of experience among the filler items: 15% of what your current level needs to level up."""
    display_name = "Experience Weight"
    range_start = 0
    range_end = 100
    default = 6


class CharmWeight(Range):
    """Weight of Random Charms (small, large or grand) and Random Jewels (magic or rare) among the filler
    items, rolled at your character level."""
    display_name = "Charm and Jewel Weight"
    range_start = 0
    range_end = 100
    default = 8


class ShrineWeight(Range):
    """Weight of Random Shrines among the filler items: a helpful shrine (refill, defense, combat, a
    resistance, skill, mana recharge, experience or gem) appears at your feet."""
    display_name = "Shrine Weight"
    range_start = 0
    range_end = 100
    default = 6


class TrapPercentage(Range):
    """Percentage of filler items replaced by traps. The trap weights below pick which ones.
    Traps wait until you leave town, unless Traps Wait for Town is off."""
    display_name = "Trap Percentage"
    range_start = 0
    range_end = 50
    default = 10


class JunkBackpackWeight(Range):
    """Weight of Junk Backpack traps: your free inventory space fills with worthless gear."""
    display_name = "Junk Backpack Weight"
    range_start = 0
    range_end = 100
    default = 10


class AmbushWeight(Range):
    """Weight of Ambush traps: a pack of monsters appears around you (see Ambush Size and Ambush Monsters)."""
    display_name = "Ambush Weight"
    range_start = 0
    range_end = 100
    default = 10


class PoisonCloudWeight(Range):
    """Weight of Poison Cloud traps: clouds of poison gas surround you."""
    display_name = "Poison Cloud Weight"
    range_start = 0
    range_end = 100
    default = 8


class FirestormWeight(Range):
    """Weight of Firestorm traps: patches of fire burst up around you."""
    display_name = "Firestorm Weight"
    range_start = 0
    range_end = 100
    default = 8


class ManaDrainWeight(Range):
    """Weight of Mana Drain traps: your mana empties."""
    display_name = "Mana Drain Weight"
    range_start = 0
    range_end = 100
    default = 8


class TaxCollectorWeight(Range):
    """Weight of Tax Collector traps: you lose gold per character level (Tax Collector Gold, as much as you have)."""
    display_name = "Tax Collector Weight"
    range_start = 0
    range_end = 100
    default = 6


class MercenaryDownWeight(Range):
    """Weight of Mercenary Down traps: your mercenary dies. Off by default."""
    display_name = "Mercenary Down Weight"
    range_start = 0
    range_end = 100
    default = 0

class TrapsWaitForTown(DefaultOnToggle):
    """Traps that arrive while you're in town wait until you leave it. Off: they go off wherever you are,
    so an Ambush can land in town."""
    display_name = "Traps Wait for Town"


class AmbushSize(Range):
    """Monsters in each Ambush."""
    display_name = "Ambush Size"
    range_start = 1
    range_end = 30
    default = 6


class AmbushMonsters(Choice):
    """Which monsters an Ambush brings.
    Act: a common monster of the current act (Fallen, Sand Raiders, Fetishes...).
    Area: one of the monsters that live in the area you're in (after Monster Shuffle, on the current
    difficulty), so the ambush fits where you are. Falls back to the act's monsters where an area has none."""
    display_name = "Ambush Monsters"
    option_act = 0
    option_area = 1
    default = 0


class TaxCollectorGold(Range):
    """Gold a Tax Collector trap takes per character level (as much as you have)."""
    display_name = "Tax Collector Gold"
    range_start = 0
    range_end = 10000
    default = 1000


class JunkBackpackSize(Range):
    """The most pieces of junk a Junk Backpack puts in your inventory (fewer if it's fuller)."""
    display_name = "Junk Backpack Size"
    range_start = 1
    range_end = 40
    default = 40


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


class SkillTreeShuffle(Toggle):
    """Every class's skill tree is dealt skills from all classes, each in a row of its own level, with
    synergies pointing at skills in the same tree. The Druid's shapeshifts stay with the Druid, and the
    Warlock's tree stays as it is. With Skill Mode tree, your tree's skills are the ones you find."""
    display_name = "Skill Tree Shuffle"


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
    skill_mode: SkillMode
    skill_points: SkillPoints
    act_keys: ActKeys
    skill_pool_size: SkillPoolSize
    skill_copies: SkillCopies
    skill_focus: SkillFocus
    skill_level_scaling: SkillLevelScaling
    skill_scaling_cap: SkillScalingCap
    scale_with_length: ScaleWithLength
    starting_skills: StartingSkills
    skill_classes: SkillClasses
    include_warlock: IncludeWarlock
    skills_per_act: SkillsPerAct
    area_checks: AreaChecks
    side_area_checks: SideAreaChecks
    level_checks: LevelChecks
    superunique_checks: SuperUniqueChecks
    quest_item_checks: QuestItemChecks
    shop_checks: ShopChecks
    uber_checks: UberChecks
    kill_count_checks: KillCountChecks
    find_count_checks: FindCountChecks
    enforce_act_keys: EnforceActKeys
    act_key_window: ActKeyWindow
    cube_lock: CubeLock
    town_portal_lock: TownPortalLock
    potion_lock: PotionLock
    stash_lock: StashLock
    merchant_lock: MerchantLock
    waypoint_lock: WaypointLock
    cow_level_checks: CowLevelChecks
    cain_hints: CainHints
    page_hint_chance: PageHintChance
    prophecy_shop: ProphecyShop
    hint_balance: HintBalance
    rune_checks: RuneChecks
    gem_checks: GemChecks
    set_checks: SetChecks
    special_item_checks: SpecialItemChecks
    codex_empowerments: CodexEmpowerments
    boss_quests_give_uniques: BossQuestsGiveUniques
    basic_loot_weight: BasicLootWeight
    unique_weight: UniqueWeight
    set_item_weight: SetItemWeight
    high_rune_weight: HighRuneWeight
    socketed_base_weight: SocketedBaseWeight
    gold_weight: GoldWeight
    experience_weight: ExperienceWeight
    charm_weight: CharmWeight
    shrine_weight: ShrineWeight
    prophecy_weight: ProphecyWeight
    trap_percentage: TrapPercentage
    junk_backpack_weight: JunkBackpackWeight
    ambush_weight: AmbushWeight
    poison_cloud_weight: PoisonCloudWeight
    firestorm_weight: FirestormWeight
    mana_drain_weight: ManaDrainWeight
    tax_collector_weight: TaxCollectorWeight
    mercenary_down_weight: MercenaryDownWeight
    traps_wait_for_town: TrapsWaitForTown
    ambush_size: AmbushSize
    ambush_monsters: AmbushMonsters
    tax_collector_gold: TaxCollectorGold
    junk_backpack_size: JunkBackpackSize
    monster_shuffle: MonsterShuffle
    superunique_shuffle: SuperUniqueShuffle
    runeword_shuffle: RunewordShuffle
    skill_tree_shuffle: SkillTreeShuffle
    monster_density: MonsterDensity
    champion_packs: ChampionPacks
    better_drops: BetterDrops
    experience_multiplier: ExperienceMultiplier
    item_level_requirements: ItemLevelRequirements
    item_stat_requirements: ItemStatRequirements
    death_link: DeathLink


# Sections for the Options Creator, the options web page and the template YAML.
OPTION_GROUPS = [
    OptionGroup("Goal and Length", [Goal, GoalAct, ActKeys, EnforceActKeys, ActKeyWindow, SkillsPerAct]),
    OptionGroup("Skills and Codex", [CharacterClass, SkillMode, SkillTreeShuffle, SkillPoints, SkillPoolSize, SkillCopies, SkillFocus, ScaleWithLength, StartingSkills,
                                     SkillClasses, IncludeWarlock, SkillLevelScaling, SkillScalingCap, CodexEmpowerments]),
    OptionGroup("Checks", [AreaChecks, SideAreaChecks, LevelChecks, SuperUniqueChecks, QuestItemChecks, ShopChecks, CowLevelChecks]),
    OptionGroup("Extra Checks (Filler Only)", [RuneChecks, GemChecks, SetChecks, SpecialItemChecks, UberChecks, KillCountChecks,
                                               FindCountChecks], start_collapsed=True),
    OptionGroup("Locks", [CubeLock, TownPortalLock, PotionLock, StashLock, MerchantLock, WaypointLock]),
    OptionGroup("Hints", [CainHints, PageHintChance, ProphecyShop, HintBalance]),
    OptionGroup("Rewards", [BossQuestsGiveUniques, BasicLootWeight, UniqueWeight, SetItemWeight, HighRuneWeight,
                            SocketedBaseWeight, GoldWeight, ExperienceWeight, CharmWeight, ShrineWeight, ProphecyWeight],
                start_collapsed=True),
    OptionGroup("Traps", [TrapPercentage, JunkBackpackWeight, AmbushWeight, PoisonCloudWeight, FirestormWeight, ManaDrainWeight,
                          TaxCollectorWeight, MercenaryDownWeight, TrapsWaitForTown, AmbushSize, AmbushMonsters, TaxCollectorGold,
                          JunkBackpackSize], start_collapsed=True),
    OptionGroup("Game Changes", [MonsterShuffle, SuperUniqueShuffle, RunewordShuffle, MonsterDensity, ChampionPacks, BetterDrops,
                                 ExperienceMultiplier, ItemLevelRequirements, ItemStatRequirements]),
    OptionGroup("Multiworld", [DeathLink]),
]

# Starting points on the options web page; anything not listed keeps its default.
OPTION_PRESETS: dict[str, dict[str, Any]] = {
    "Quick Normal": {
        "goal": "normal", "goal_act": 2, "shop_checks": 2, "trap_percentage": 5,
    },
    "Full Hell": {
        "goal": "hell", "goal_act": 5, "rune_checks": 20, "special_item_checks": True, "uber_checks": True,
        "kill_count_checks": True,
    },
    "Locked Down": {
        "cube_lock": True, "town_portal_lock": True, "potion_lock": True, "stash_lock": True, "merchant_lock": True,
        "waypoint_lock": True, "trap_percentage": 25,
    },
    "Treasure Hunter": {
        "goal": "nightmare", "goal_act": 5, "rune_checks": 33, "gem_checks": True, "set_checks": True,
        "find_count_checks": True, "better_drops": 250,
    },
}
