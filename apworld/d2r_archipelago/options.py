from dataclasses import dataclass

from typing import Any

from Options import (Choice, DeathLink, DefaultOnToggle, NamedRange, OptionCounter, OptionGroup, OptionSet, PerGameCommonOptions,
                     Range, Toggle)

from .data import CLASSES
from .ids import POOL_LOCKS, skill_names_by_class


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
    Skills your class can't use stay out of the pool and the tree shuffle: claw skills and kicks need an
    Assassin, the Assassin's traps an Assassin, Smite anyone but an Amazon or Barbarian, Dodge, Avoid and
    Evade anyone but a Paladin, Charge, Impale and Blade Fury anyone but a Necromancer (their animations),
    and wolf and bear attacks (Feral Rage, Maul...) a Druid who also gets that form. Skills the game itself
    ties to their class stay with it: Fend, Strafe, Jab; Leap, Leap Attack, Whirlwind, Double Swing, Double
    Throw, Frenzy; Inferno; Zeal; Arctic Blast, Fury, Shock Wave, Rabies, Hunger; Dragon Flight and the
    martial arts. With "any", they all stay out."""
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
    tree: the skills are the 30 in your class's skill tree (shuffled with Skill Tree Shuffle, which
    picks tree mode on its own). Each one
    stays locked until its item arrives; then you spend skill points on it as usual. Extra copies and
    Skill Points give skill points. Skill Pool Size, Skill Focus, Skill Classes, Skill Level Scaling, the
    Codex levels and Codex Empowerments don't apply. With "any" class, the seed picks one."""
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
    """How many times each skill is in the pool. Every copy you find adds Levels Per Copy (+1 by default) to
    that skill."""
    display_name = "Skill Copies"
    range_start = 1
    range_end = 5
    default = 2


class SkillFocus(Choice):
    """Which skills go in the pool.
    any_skills: any skills from the allowed classes.
    class_weighted: Class Weight Percent of them (half by default) from your Character Class.
    few_but_deep: a third as many different skills, each with at least 3 copies."""
    display_name = "Skill Focus"
    option_any_skills = 0
    option_class_weighted = 1
    option_few_but_deep = 2
    default = 1


class ClassWeightPercent(Range):
    """With Skill Focus class_weighted: the share of the pool's skills (in percent) from your Character Class.
    When either side runs out of skills, the other one makes up the difference."""
    display_name = "Class Weight Percent"
    range_start = 0
    range_end = 100
    default = 50


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


class LevelsPerCopy(Range):
    """How many levels each copy of a skill you find adds to it on the Codex."""
    display_name = "Levels Per Copy"
    range_start = 1
    range_end = 5
    default = 1


class LevelsPerEmpowerment(Range):
    """How many levels each Codex Empowerment adds to every skill on the Codex."""
    display_name = "Levels Per Empowerment"
    range_start = 1
    range_end = 5
    default = 1


class CodexLevelCap(Range):
    """The highest level a Codex skill can reach, from copies, Codex Empowerments and Skill Level Scaling
    together. Diablo II shows skill levels up to 99."""
    display_name = "Codex Level Cap"
    range_start = 10
    range_end = 99
    default = 60


class StartingSkills(Range):
    """How many skills from the pool you start with. In tree mode your starting weapon also gives +1 to one of them
    (an attack if you have one), like the Sorceress's Fire Bolt staff in the normal game."""
    display_name = "Starting Skills"
    range_start = 0
    range_end = 10
    default = 2


class UsefulStartingSkills(DefaultOnToggle):
    """At least one starting skill deals damage or summons, so you never start with only curses, auras, buffs,
    passives or skills like Find Potion. With a class, it's one of the class's level-1 attacks or summons; in tree
    mode one from the tree's top row if it has one (a weapon attack like Bash counts), else the lowest one."""
    display_name = "Useful Starting Skills"


class SkillBlocklist(OptionSet):
    """Skills that never go in the pool or your starting skills, by item name ("Find Potion (Barbarian)"). In tree
    mode they stay locked in your tree."""
    display_name = "Skill Blocklist"
    valid_keys = sorted(set().union(*skill_names_by_class.values()))
    default = frozenset()


class SkillClasses(OptionSet):
    """Which classes' skill trees the pool draws from. Empty means every class."""
    display_name = "Skill Classes"
    valid_keys = sorted(CLASSES.values())
    default = frozenset()


class IncludeWarlock(Toggle):
    """Put Warlock skills in the pool. Only for characters created in the Reign of the Warlock expansion:
    Lord of Destruction characters can't use them (the plugin leaves them off their Codex).
    With Skill Tree Shuffle, the Warlock's tree is dealt with the others. Character Class warlock turns
    this on."""
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
    """Reaching every fifth character level (Level Check Step) is a check, up to a cap set by the goal
    (30 on Normal, 60 on Nightmare, 90 on Hell). Past level 70 (Grind Level) they only hold filler."""
    display_name = "Level Checks"


class LevelCheckStep(Range):
    """With Level Checks, every this many character levels is a check: 5 is levels 5, 10, 15..., 1 is every
    level from 2. The level cap is rounded down to a multiple of it."""
    display_name = "Level Check Step"
    range_start = 1
    range_end = 10
    default = 5


class GrindLevel(Range):
    """Level checks above this level take a long grind, so they only hold filler. 90 lets every level check hold
    anything; 0 keeps them all filler-only."""
    display_name = "Grind Level"
    range_start = 0
    range_end = 90
    default = 70


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
    """Each act's potion seller (Akara, Lysander, Ormus, Jamella, Malah), or blacksmith with Shop Vendor, sells
    Archipelago Parcels; buying one is a check, up to this many per act. They cost more in later acts."""
    display_name = "Shop Checks"
    range_start = 0
    range_end = 5
    default = 3


class ShopVendor(Choice):
    """Who sells the Archipelago Parcels in each act.
    potion_seller: Akara, Lysander, Ormus, Jamella and Malah.
    blacksmith: Charsi, Fara, Hratli, Halbu and Larzuk.
    Scrolls of Prophecy and the Writ of Absolution stay with the potion sellers."""
    display_name = "Shop Vendor"
    option_potion_seller = 0
    option_blacksmith = 1
    default = 0


class ParcelPriceMultiplier(Range):
    """Price of the Archipelago Parcels, in percent of the usual 2,500 (Act I) to 40,000 (Act V) gold. A parcel
    bought past the last check refunds this price."""
    display_name = "Parcel Price Multiplier"
    range_start = 10
    range_end = 500
    default = 100


class UberChecks(Toggle):
    """With a Hell goal through Baal: killing each of Pandemonium's six uber bosses (Lilith, Uber Duriel,
    Uber Izual, Uber Mephisto, Uber Diablo, Uber Baal) is a check. Only filler goes on these."""
    display_name = "Uber Checks"


class KillCountChecks(Toggle):
    """Killing 100, 250 and 500 monsters (Kill Count Multiplier) in each act on each difficulty is a check
    (counted across sessions). Only filler goes on these."""
    display_name = "Kill Count Checks"


class KillCountMultiplier(Choice):
    """With Kill Count Checks, how many monsters each act's checks take.
    half: 50, 125 and 250. normal: 100, 250 and 500. double: 200, 500 and 1,000."""
    display_name = "Kill Count Multiplier"
    option_half = 50
    option_normal = 100
    option_double = 200
    default = 100


class FindCountChecks(Toggle):
    """Finding 1, 5, 10, 25 and 50 unique items, and 1, 5, 10 and 25 set items (Find Count Multiplier), are
    checks. Items you receive from Archipelago don't count. Only filler goes on these."""
    display_name = "Find Count Checks"


class FindCountMultiplier(Choice):
    """With Find Count Checks, how many items the checks take.
    half: 1, 2, 5, 12 and 25 uniques, 1, 2, 5 and 12 set items.
    normal: 1, 5, 10, 25 and 50 uniques, 1, 5, 10 and 25 set items.
    double: 2, 10, 20, 50 and 100 uniques, 2, 10, 20 and 50 set items."""
    display_name = "Find Count Multiplier"
    option_half = 50
    option_normal = 100
    option_double = 200
    default = 100


class EnforceActKeys(DefaultOnToggle):
    """With Act Keys, walking into an act you don't have the key for sends you back to the town of an act
    you can be in, instead of just holding that act's checks."""
    display_name = "Enforce Act Keys"


class EnforceActKeysMode(Choice):
    """What Enforce Act Keys does when you walk into an act you don't have the key for.
    warp_to_town: sends you back to the town of an act you can be in.
    message_only: lets you stay, and says once each time you walk in that the act is locked and its checks
    wait for its key."""
    display_name = "Enforce Act Keys Mode"
    option_warp_to_town = 0
    option_message_only = 1
    default = 0


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
    """Town Portal scrolls and tomes can't be used until you find Town Portal Access. The ones you carry or
    pick up are set aside until then; Town Portal Access hands them back with a new Tome of Town Portal."""
    display_name = "Town Portal Lock"


class TownPortalAccessTomeCharges(Range):
    """Charges in the Tome of Town Portal that Town Portal Access brings. 0 brings no tome, only the scrolls
    and tomes set aside."""
    display_name = "Town Portal Access Tome Charges"
    range_start = 0
    range_end = 20
    default = 20


class PotionLock(Toggle):
    """Potions can't be drunk until you find Potion Access."""
    display_name = "Potion Lock"


class PotionLockMode(Choice):
    """What happens to potions you pick up while Potion Lock is on.
    set_aside: potions in your inventory are put away until Potion Access hands them back; belt potions stay
    but won't drink.
    drink: healing, mana and rejuvenation potions are drunk as soon as they reach your inventory or belt. Any
    healing potion fills your life and any mana potion your mana, whatever its size (a rejuvenation potion
    both); one drunk at full life or mana is wasted. Stamina, antidote and thawing potions are set aside.
    rejuvenation_allowed: like set_aside, but rejuvenation potions aren't locked: they stay and drink as usual."""
    display_name = "Potion Lock Mode"
    option_set_aside = 0
    option_drink = 1
    option_rejuvenation_allowed = 2
    default = 0


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


class EarlyLocks(OptionSet):
    """Locks whose item goes in the first checks anyone in the multiworld can reach, in any game. Only locks that
    are on count. Horadric Cube Access, Town Portal Access, Potion Access, Stash Access, Merchant Access, Act I
    Waypoints, Act II Waypoints, Act III Waypoints."""
    display_name = "Early Locks"
    valid_keys = sorted(POOL_LOCKS)
    default = frozenset()


class LocalEarlyLocks(OptionSet):
    """Locks whose item goes in your own first checks (Act I with act keys on). Only locks that are on count; one
    in both lists goes here. Same names as Early Locks."""
    display_name = "Local Early Locks"
    valid_keys = sorted(POOL_LOCKS)
    default = frozenset()


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


class ProphecyPriceMultiplier(Range):
    """Price of the Scrolls of Prophecy, in percent of the usual 1,000 to 100,000 gold. A refunded scroll gives
    this price back."""
    display_name = "Prophecy Price Multiplier"
    range_start = 10
    range_end = 500
    default = 100


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
    The Cow King, and killing 100 cows (Cow Kill Count) are checks. The cows add up per difficulty over the whole
    seed, across games and reconnects (cows killed before connecting count too); only cows in the Cow Level count,
    unless Shuffle Cow Level moved them. Killing the Cow King closes the portal for that difficulty, so get the
    other two first."""
    display_name = "Cow Level Checks"


class CowKillCount(Choice):
    """With Cow Level Checks, how many cows the cow check on each difficulty takes."""
    display_name = "Cow Kill Count"
    option_50 = 50
    option_100 = 100
    option_250 = 250
    option_500 = 500
    default = 100


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
    """With a Hell goal through Baal: finding each Pandemonium key, each uber organ, each essence, the Token of
    Absolution, the Hellfire Torch and Annihilus is a check (13). Only filler goes on these."""
    display_name = "Special Item Checks"


class ScaleWithLength(DefaultOnToggle):
    """Skill Pool Size and Codex Empowerments are for a four-act game (Normal to Diablo); longer games get
    more of both in proportion, so a full Hell run (15 acts) has almost four times as many. Turn off to use
    the numbers as they are."""
    display_name = "Scale With Length"


class CodexEmpowerments(Range):
    """How many Codex Empowerments are in the pool, for a four-act game (more for longer games with Scale
    With Length). Each one adds Levels Per Empowerment (+1 by default) to every skill in your Codex."""
    display_name = "Codex Empowerments"
    range_start = 0
    range_end = 10
    default = 2


class BossQuestsGiveUniques(DefaultOnToggle):
    """The act boss quests (Andariel, Duriel, Mephisto, Diablo, Baal) always reward you with a
    Random Unique (or what Boss Reward Type picks), scaled to your level, instead of an item from the multiworld."""
    display_name = "Boss Quests Give Uniques"


class BossRewardType(Choice):
    """What the act boss quests give with Boss Quests Give Uniques on. Unique: a Random Unique of any kind."""
    display_name = "Boss Reward Type"
    option_unique = 0
    option_unique_weapon = 1
    option_unique_armor = 2
    option_unique_jewelry = 3
    option_set_item = 4
    option_high_rune = 5
    default = 0

    # The reward item for each choice.
    reward_items = ("Random Unique", "Random Unique Weapon", "Random Unique Armor", "Random Unique Jewelry", "Random Set Item", "High Rune")


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
    """Weight of Socketed Bases among the filler items: a white weapon or armor with two or more
    sockets, ready for a runeword."""
    display_name = "Socketed Base Weight"
    range_start = 0
    range_end = 100
    default = 7


class GoldWeight(Range):
    """Weight of gold among the filler items: a pile worth Gold per Level (1,000) per character level."""
    display_name = "Gold Weight"
    range_start = 0
    range_end = 100
    default = 8


class ExperienceWeight(Range):
    """Weight of experience among the filler items: Experience Reward Percent (15%) of what your current level
    needs to level up."""
    display_name = "Experience Weight"
    range_start = 0
    range_end = 100
    default = 6


class CharmWeight(Range):
    """Weight of Random Charms (small, large or grand) and Random Jewels (magic or rare) among the filler
    items, rolled at your character level. Jewel Share splits it between the two."""
    display_name = "Charm and Jewel Weight"
    range_start = 0
    range_end = 100
    default = 8


class JewelShare(Range):
    """Percentage of Charm and Jewel Weight that goes to Random Jewels; the rest goes to Random Charms.
    0: charms only. 100: jewels only."""
    display_name = "Jewel Share"
    range_start = 0
    range_end = 100
    default = 40


class ShrineWeight(Range):
    """Weight of Random Shrines among the filler items: a helpful shrine (from Shrine Pool) appears at your feet."""
    display_name = "Shrine Weight"
    range_start = 0
    range_end = 100
    default = 6


class GoldRewardPerLevel(Range):
    """Gold in a gold reward, per character level."""
    display_name = "Gold per Level"
    range_start = 0
    range_end = 10000
    default = 1000


class ExperienceRewardPercent(Range):
    """Experience in an experience reward, in percent of what your current level needs to level up
    (100: a whole level)."""
    display_name = "Experience Reward Percent"
    range_start = 0
    range_end = 100
    default = 15


class RewardLevelBelow(Range):
    """Random Uniques and Set Items (boss rewards too) are ones whose level requirement is at most this many
    levels below your character level..."""
    display_name = "Reward Levels Below"
    range_start = 0
    range_end = 30
    default = 12


class RewardLevelAbove(Range):
    """...and at most this many levels above it, so you may have to grow into them. When nothing falls in
    the window, the closest items to your level are used."""
    display_name = "Reward Levels Above"
    range_start = 0
    range_end = 10
    default = 2


class RareJewelChance(Range):
    """Percentage chance that a Random Jewel is rare instead of magic."""
    display_name = "Rare Jewel Chance"
    range_start = 0
    range_end = 100
    default = 30


class HighRuneCurve(Choice):
    """How much rarer each High Rune (Pul through Zod) is than the one before.
    Flat: all thirteen alike (Ber and Zod each 1 in 13).
    Gentle: Ber about 1 in 23, Zod 1 in 91.
    Steep: Ber about 1 in 51, Zod 1 in 819.
    Very steep: Ber about 1 in 129, Zod 1 in 8,281."""
    display_name = "High Rune Curve"
    option_flat = 0
    option_gentle = 1
    option_steep = 2
    option_very_steep = 3
    default = 2


class ShrinePool(OptionSet):
    """The shrines a Random Shrine can be. With none, Random Shrines aren't in the filler at all."""
    display_name = "Shrine Pool"
    # Shrine name -> its Shrines.txt row (loot::HelpfulShrines in the plugin).
    shrines = {"Refilling": 1, "Armor": 6, "Combat": 7, "Resist Fire": 8, "Resist Cold": 9, "Resist Lightning": 10,
               "Resist Poison": 11, "Skill": 12, "Mana Recharge": 13, "Experience": 15, "Gem": 18}
    valid_keys = sorted(shrines)
    default = frozenset(shrines)


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


class MonsterShuffleTolerance(Range):
    """With Monster Shuffle on anywhere, how many levels above an area the weakest version of its new
    monsters may be on Normal. Lower keeps early areas easier; higher allows wilder line-ups."""
    display_name = "Monster Shuffle Tolerance"
    range_start = 0
    range_end = 20
    default = 3


class ShuffleCowLevel(Toggle):
    """The Secret Cow Level joins Monster Shuffle: its cows go to another area (Act I with nearby, anywhere
    with anywhere) and it gets that area's monsters. The Cow King stays. With nearby, Normal's cows can land in
    an early Act I area at their full level. Cows count for the 100-cow check wherever they are."""
    display_name = "Shuffle Cow Level"


class SuperUniqueShuffle(Choice):
    """Gives field superuniques (Bishibosh, Rakanishu, Pindleskin...) another superunique's monster type.
    Quest bosses, act bosses, the Chaos Sanctuary seal bosses and Baal's waves are left alone.
    Nearby: only swaps between monsters of similar strength. Anywhere: any swap."""
    display_name = "Superunique Shuffle"
    option_off = 0
    option_nearby = 1
    option_anywhere = 2
    default = 1


class MainPathSuperUniqueCap(Range):
    """With Superunique Shuffle on, superuniques you can't avoid (on an act's route or at a quest objective, like
    Rakanishu, Griswold and Pindleskin) only become a monster at most this many levels above their own; the
    tougher ones go to the optional dungeons (Crypt, Cave, Stony Tomb, Tal Rasha's false tombs, Ruined Temple,
    Icy Cellar). 0 never makes them stronger; 50 turns the cap off."""
    display_name = "Main-Path Superunique Cap"
    range_start = 0
    range_end = 50
    default = 2


class MainPathSuperUniqueMinions(NamedRange):
    """With Superunique Shuffle on, the most minions a main-path superunique brings once it's a different
    monster (Rakanishu brings 8). Superuniques that kept their monster, and the optional ones, aren't capped.
    Applies after Superunique Minions."""
    display_name = "Main-Path Superunique Minions"
    range_start = 0
    range_end = 18
    default = 5
    special_range_names = {"off": -1}


class MainPathSuperUniqueStrength(Range):
    """HP and damage of the superuniques you can't avoid (on an act's route or at a quest objective; not the ones
    in optional dead-end dungeons), in percent, on every difficulty. Their minions are unaffected. Spells take
    their damage from the monster's level, so they don't scale."""
    display_name = "Main-Path Superunique Strength"
    range_start = 25
    range_end = 200
    default = 100


class MainPathSuperUniqueLevel(NamedRange):
    """On Normal, a main-path superunique's level is its usual level plus this, whatever monster Superunique
    Shuffle made it, so a reshaped Griswold is still level 5 at 0. Nightmare and Hell take every monster's level
    from the area, so they aren't affected. off: it takes its new monster's level."""
    display_name = "Main-Path Superunique Level"
    range_start = -10
    range_end = 10
    default = 0
    special_range_names = {"off": -11}


class SuperUniqueShuffleSpread(Range):
    """With Superunique Shuffle on nearby, superuniques are sorted by their monster's level and only swap
    within groups of this many. Smaller groups keep each one closer to its usual strength."""
    display_name = "Superunique Shuffle Spread"
    range_start = 2
    range_end = 31
    default = 6


class SuperUniqueMinions(Range):
    """How many minions each field superunique brings, in percent (rounded, at most 18). Quest and act
    bosses keep theirs."""
    display_name = "Superunique Minions"
    range_start = 0
    range_end = 200
    default = 100


class MonsterDensity(Range):
    """How many monsters each area spawns, in percent. Sets Normal, and Nightmare and Hell unless they
    have their own value."""
    display_name = "Monster Density"
    range_start = 50
    range_end = 400
    default = 100


class MonsterDensityNightmare(NamedRange):
    """Monster Density on Nightmare, in percent. same: use Monster Density."""
    display_name = "Monster Density (Nightmare)"
    range_start = 50
    range_end = 400
    default = 0
    special_range_names = {"same": 0}


class MonsterDensityHell(NamedRange):
    """Monster Density on Hell, in percent. same: use Monster Density."""
    display_name = "Monster Density (Hell)"
    range_start = 50
    range_end = 400
    default = 0
    special_range_names = {"same": 0}


class ChampionPacks(Range):
    """How many champion and unique monster packs each area spawns, in percent."""
    display_name = "Champion Packs"
    range_start = 50
    range_end = 400
    default = 100


class BetterDrops(Range):
    """How often monsters drop uniques, set items and rares, in percent. Below 100 makes them rarer."""
    display_name = "Better Drops"
    range_start = 50
    range_end = 500
    default = 150


class BetterMagic(Range):
    """How often monsters drop magic items instead of plain ones, in percent."""
    display_name = "Better Magic Drops"
    range_start = 50
    range_end = 500
    default = 100


class SkillTreeShuffle(Toggle):
    """Every class's skill tree is dealt skills from all classes, each in a row of its own level, with
    synergies pointing at skills in the same tree. The Druid's shapeshifts stay with the Druid. The
    Warlock's tree joins with Include Warlock (a Reign of the Warlock character); otherwise it stays as it
    is. Turns on Skill Mode tree: your tree's skills are the ones you find."""
    display_name = "Skill Tree Shuffle"


class RunewordShuffle(Toggle):
    """Gives every runeword a new recipe of similar-tier runes. The setup tool writes the new
    recipes to runewords.txt in the mod folder."""
    display_name = "Runeword Shuffle"


class RunewordRuneSpread(Range):
    """With Runeword Shuffle on, how many tiers each rune in a recipe can move (Tal is 7, Ral 8, Ort 9...).
    0 keeps every recipe."""
    display_name = "Runeword Rune Spread"
    range_start = 0
    range_end = 10
    default = 2


class ExperienceMultiplier(Range):
    """Monster experience, in percent. Raise it for shorter games. Sets Normal, and Nightmare and Hell
    unless they have their own value."""
    display_name = "Experience Multiplier"
    range_start = 50
    range_end = 500
    default = 150


class ExperienceMultiplierNightmare(NamedRange):
    """Experience Multiplier on Nightmare, in percent. same: use Experience Multiplier."""
    display_name = "Experience Multiplier (Nightmare)"
    range_start = 50
    range_end = 500
    default = 0
    special_range_names = {"same": 0}


class ExperienceMultiplierHell(NamedRange):
    """Experience Multiplier on Hell, in percent. same: use Experience Multiplier."""
    display_name = "Experience Multiplier (Hell)"
    range_start = 50
    range_end = 500
    default = 0
    special_range_names = {"same": 0}


class DeathExperiencePenalty(Range):
    """Experience lost when you die on Nightmare and Hell, in percent of the game's own loss (5% and 10% of a
    level). 0 never loses experience. Applies to every death, DeathLink ones too; gold loss, your corpse and
    Hardcore stay as they are."""
    display_name = "Death Experience Penalty"
    range_start = 0
    range_end = 100
    default = 100


class ItemLevelRequirements(DefaultOnToggle):
    """Items keep their level requirements. Turn off to wear anything you find."""
    display_name = "Item Level Requirements"


class ItemStatRequirements(DefaultOnToggle):
    """Items keep their strength and dexterity requirements."""
    display_name = "Item Stat Requirements"


class FreeMercRevive(Toggle):
    """Resurrecting a mercenary at the town's hireling NPC costs nothing (usually up to 50,000 gold)."""
    display_name = "Free Mercenary Revive"


class TeleportCharm(Choice):
    """A Teleport Charm: a unique small charm with +1 to Teleport, for any class (from level 6 for classes other
    than the Sorceress). It works from your inventory, and you get a new one if you lose it.
    off: none. start: you begin with it. early: it's in one of your own first checks. anywhere: it's in the
    item pool like any useful item."""
    display_name = "Teleport Charm"
    option_off = 0
    option_start = 1
    option_early = 2
    option_anywhere = 3
    default = 0


class StartingGold(Range):
    """Gold your character gets once, the first time it plays the seed. At most 10,000, what a level 1
    character can carry."""
    display_name = "Starting Gold"
    range_start = 0
    range_end = 10000
    default = 0


RUNE_NAMES = ["El", "Eld", "Tir", "Nef", "Eth", "Ith", "Tal", "Ral", "Ort", "Thul", "Amn", "Sol", "Shael", "Dol", "Hel",
              "Io", "Lum", "Ko", "Fal", "Lem", "Pul", "Um", "Mal", "Ist", "Gul", "Vex", "Ohm", "Lo", "Sur", "Ber", "Jah",
              "Cham", "Zod"]

# The items Starting Items can give, by name, with their item codes. Must match Items in
# plugin/src/starting_items.h.
STARTING_ITEMS: dict[str, str] = {
    "Minor Healing Potion": "hp1", "Light Healing Potion": "hp2", "Healing Potion": "hp3",
    "Greater Healing Potion": "hp4", "Super Healing Potion": "hp5",
    "Minor Mana Potion": "mp1", "Light Mana Potion": "mp2", "Mana Potion": "mp3",
    "Greater Mana Potion": "mp4", "Super Mana Potion": "mp5",
    "Rejuvenation Potion": "rvs", "Full Rejuvenation Potion": "rvl",
    "Stamina Potion": "vps", "Antidote Potion": "yps", "Thawing Potion": "wms",
    "Scroll of Town Portal": "tsc", "Scroll of Identify": "isc",
    "Perfect Amethyst": "gpv", "Perfect Sapphire": "gpb", "Perfect Emerald": "gpg", "Perfect Ruby": "gpr",
    "Perfect Diamond": "gpw", "Perfect Topaz": "gpy", "Perfect Skull": "skz",
    **{f"{rune} Rune": f"r{number:02}" for number, rune in enumerate(RUNE_NAMES, 1)},
}


class StartingItems(OptionCounter):
    """Items your character gets once, the first time it plays the seed, as rewards (they send no checks), e.g.
    {"Full Rejuvenation Potion": 3, "Ber Rune": 1}. Up to 20 of each. They go in your inventory, then the cube
    and stash, and wait for room. The Potion and Town Portal locks treat them like any you pick up.
    Any potion (Minor to Super Healing and Mana, Rejuvenation, Full Rejuvenation, Stamina, Antidote, Thawing),
    Scroll of Town Portal, Scroll of Identify, perfect gem (Perfect Amethyst ... Perfect Skull) or rune (El Rune
    ... Zod Rune). For Archipelago items (skills, locks, keys) use start_inventory instead."""
    display_name = "Starting Items"
    valid_keys = sorted(STARTING_ITEMS)
    min = 0
    max = 20
    cull_zeroes = True
    default = {}


class LoaderToggle(Choice):
    """A D2RLoader setting d2r-ap-setup writes as the Archipelago mod's override. keep leaves your own D2RLoader
    setting alone."""
    loader_path = ""
    option_keep = 0
    option_on = 1
    option_off = 2
    default = 0

    def loader_value(self) -> bool | None:
        return None if self.value == self.option_keep else self.value == self.option_on


class LoaderRange(Range):
    """A numeric D2RLoader setting d2r-ap-setup writes as the Archipelago mod's override; the default (the game's
    own value) leaves your own D2RLoader setting alone."""
    loader_path = ""

    def loader_value(self) -> int | None:
        return None if self.value == self.default else self.value


class ShowGroundSockets(LoaderToggle):
    """D2RLoader: show socket counts on normal and superior items on the ground. keep leaves your own D2RLoader
    setting alone; on or off overrides it while you play this seed."""
    display_name = "Show Ground Sockets"
    loader_path = "d2rcore.items.show_ground_sockets"


class DisplayItemLevels(LoaderToggle):
    """D2RLoader: show item levels on weapons, armor, rings, amulets, charms and jewels. keep leaves your own
    D2RLoader setting alone; on or off overrides it while you play this seed."""
    display_name = "Display Item Levels"
    loader_path = "d2rcore.items.display_item_levels"


class ItemStatRanges(LoaderToggle):
    """D2RLoader: hold Ctrl (or the right trigger) to see the possible stat rolls of an item. keep leaves your own
    D2RLoader setting alone; on or off overrides it while you play this seed."""
    display_name = "Item Stat Ranges"
    loader_path = "d2rcore.items.item_stat_ranges"


class MaximumSockets(LoaderToggle):
    """D2RLoader: show the most sockets each item can have. keep leaves your own D2RLoader setting alone; on or
    off overrides it while you play this seed."""
    display_name = "Maximum Sockets"
    loader_path = "d2rcore.items.maximum_sockets"


class FloorItemLimit(LoaderRange):
    """D2RLoader: how many item labels on the floor show at once. 32 (the game's own) leaves your D2RLoader
    setting alone."""
    display_name = "Floor Item Labels"
    loader_path = "d2rcore.items.floor_item_display_limit"
    range_start = 32
    range_end = 128
    default = 32


class SharedStashTabs(LoaderRange):
    """D2RLoader: extra shared stash tabs after the game's own. 0 leaves your D2RLoader setting alone. With Stash
    Lock, they only matter once Stash Access arrives. D2RLoader warns the game may crash past about 500."""
    display_name = "Extra Shared Stash Tabs"
    loader_path = "d2rcore.stash.add_shared_tabs"
    range_start = 0
    range_end = 500
    default = 0


class MaterialsLimit(LoaderRange):
    """D2RLoader: how many of each material (runes, gems...) the advanced stash stacks. 99 (the game's own) leaves
    your D2RLoader setting alone."""
    display_name = "Materials Stack Limit"
    loader_path = "d2rcore.stash.set_materials_limit"
    range_start = 99
    range_end = 255
    default = 99


class DeathLinkMode(Choice):
    """What another player's death does to you with DeathLink on.
    die: your character dies.
    one_hp: your life drops to 1.
    lose_gold: you lose gold like a Tax Collector trap (Tax Collector Gold per character level, as much as you have).
    random_trap: one of your seed's traps goes off (by the trap weights, Junk Backpack aside; it waits for town like
    the others). With every trap weight at 0, an Ambush, Poison Cloud, Firestorm, Mana Drain or Tax Collector."""
    display_name = "DeathLink Mode"
    option_die = 0
    option_one_hp = 1
    option_lose_gold = 2
    option_random_trap = 3
    default = 0


class DeathLinkAmnesty(Range):
    """With DeathLink on, how many of your deaths it takes to send one to the other players: 1 sends every death, 3
    every third. The count carries over between sessions. Deaths that DeathLink caused never count."""
    display_name = "DeathLink Amnesty"
    range_start = 1
    range_end = 20
    default = 1


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
    class_weight_percent: ClassWeightPercent
    skill_level_scaling: SkillLevelScaling
    skill_scaling_cap: SkillScalingCap
    levels_per_copy: LevelsPerCopy
    levels_per_empowerment: LevelsPerEmpowerment
    codex_level_cap: CodexLevelCap
    scale_with_length: ScaleWithLength
    starting_skills: StartingSkills
    useful_starting_skills: UsefulStartingSkills
    skill_blocklist: SkillBlocklist
    skill_classes: SkillClasses
    include_warlock: IncludeWarlock
    skills_per_act: SkillsPerAct
    area_checks: AreaChecks
    side_area_checks: SideAreaChecks
    level_checks: LevelChecks
    level_check_step: LevelCheckStep
    grind_level: GrindLevel
    superunique_checks: SuperUniqueChecks
    quest_item_checks: QuestItemChecks
    shop_checks: ShopChecks
    shop_vendor: ShopVendor
    parcel_price_multiplier: ParcelPriceMultiplier
    uber_checks: UberChecks
    kill_count_checks: KillCountChecks
    kill_count_multiplier: KillCountMultiplier
    find_count_checks: FindCountChecks
    find_count_multiplier: FindCountMultiplier
    enforce_act_keys: EnforceActKeys
    enforce_act_keys_mode: EnforceActKeysMode
    act_key_window: ActKeyWindow
    cube_lock: CubeLock
    town_portal_lock: TownPortalLock
    tp_access_tome_charges: TownPortalAccessTomeCharges
    potion_lock: PotionLock
    potion_lock_mode: PotionLockMode
    stash_lock: StashLock
    merchant_lock: MerchantLock
    waypoint_lock: WaypointLock
    early_locks: EarlyLocks
    local_early_locks: LocalEarlyLocks
    cow_level_checks: CowLevelChecks
    cow_kill_count: CowKillCount
    cain_hints: CainHints
    page_hint_chance: PageHintChance
    prophecy_shop: ProphecyShop
    prophecy_price_multiplier: ProphecyPriceMultiplier
    hint_balance: HintBalance
    rune_checks: RuneChecks
    gem_checks: GemChecks
    set_checks: SetChecks
    special_item_checks: SpecialItemChecks
    codex_empowerments: CodexEmpowerments
    boss_quests_give_uniques: BossQuestsGiveUniques
    boss_reward_type: BossRewardType
    teleport_charm: TeleportCharm
    starting_gold: StartingGold
    starting_items: StartingItems
    basic_loot_weight: BasicLootWeight
    unique_weight: UniqueWeight
    set_item_weight: SetItemWeight
    high_rune_weight: HighRuneWeight
    socketed_base_weight: SocketedBaseWeight
    gold_weight: GoldWeight
    experience_weight: ExperienceWeight
    charm_weight: CharmWeight
    jewel_share: JewelShare
    shrine_weight: ShrineWeight
    prophecy_weight: ProphecyWeight
    gold_reward_per_level: GoldRewardPerLevel
    experience_reward_percent: ExperienceRewardPercent
    reward_level_below: RewardLevelBelow
    reward_level_above: RewardLevelAbove
    rare_jewel_chance: RareJewelChance
    high_rune_curve: HighRuneCurve
    shrine_pool: ShrinePool
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
    monster_shuffle_tolerance: MonsterShuffleTolerance
    shuffle_cow_level: ShuffleCowLevel
    superunique_shuffle: SuperUniqueShuffle
    main_path_superunique_cap: MainPathSuperUniqueCap
    main_path_superunique_minions: MainPathSuperUniqueMinions
    main_path_superunique_strength: MainPathSuperUniqueStrength
    main_path_superunique_level: MainPathSuperUniqueLevel
    superunique_shuffle_spread: SuperUniqueShuffleSpread
    superunique_minions: SuperUniqueMinions
    runeword_shuffle: RunewordShuffle
    runeword_rune_spread: RunewordRuneSpread
    skill_tree_shuffle: SkillTreeShuffle
    monster_density: MonsterDensity
    monster_density_nightmare: MonsterDensityNightmare
    monster_density_hell: MonsterDensityHell
    champion_packs: ChampionPacks
    better_drops: BetterDrops
    better_magic: BetterMagic
    experience_multiplier: ExperienceMultiplier
    experience_multiplier_nightmare: ExperienceMultiplierNightmare
    experience_multiplier_hell: ExperienceMultiplierHell
    death_experience_penalty: DeathExperiencePenalty
    item_level_requirements: ItemLevelRequirements
    item_stat_requirements: ItemStatRequirements
    free_merc_revive: FreeMercRevive
    show_ground_sockets: ShowGroundSockets
    display_item_levels: DisplayItemLevels
    item_stat_ranges: ItemStatRanges
    maximum_sockets: MaximumSockets
    floor_item_limit: FloorItemLimit
    shared_stash_tabs: SharedStashTabs
    materials_limit: MaterialsLimit
    death_link: DeathLink
    death_link_mode: DeathLinkMode
    death_link_amnesty: DeathLinkAmnesty


# Sections for the Options Creator, the options web page and the template YAML.
OPTION_GROUPS = [
    OptionGroup("Goal and Length", [Goal, GoalAct, ActKeys, EnforceActKeys, EnforceActKeysMode, ActKeyWindow, SkillsPerAct]),
    OptionGroup("Skills and Codex", [CharacterClass, SkillMode, SkillTreeShuffle, SkillPoints, SkillPoolSize, SkillCopies, SkillFocus, ClassWeightPercent,
                                     ScaleWithLength, StartingSkills, UsefulStartingSkills, SkillBlocklist, SkillClasses, IncludeWarlock,
                                     SkillLevelScaling, SkillScalingCap, CodexEmpowerments, LevelsPerCopy, LevelsPerEmpowerment,
                                     CodexLevelCap]),
    OptionGroup("Checks", [AreaChecks, SideAreaChecks, LevelChecks, LevelCheckStep, GrindLevel, SuperUniqueChecks, QuestItemChecks,
                           ShopChecks, ShopVendor, ParcelPriceMultiplier, CowLevelChecks, CowKillCount]),
    OptionGroup("Extra Checks (Filler Only)", [RuneChecks, GemChecks, SetChecks, SpecialItemChecks, UberChecks, KillCountChecks,
                                               KillCountMultiplier, FindCountChecks, FindCountMultiplier], start_collapsed=True),
    OptionGroup("Locks", [CubeLock, TownPortalLock, TownPortalAccessTomeCharges, PotionLock, PotionLockMode, StashLock, MerchantLock, WaypointLock,
                             EarlyLocks, LocalEarlyLocks]),
    OptionGroup("Hints", [CainHints, PageHintChance, ProphecyShop, ProphecyPriceMultiplier, HintBalance]),
    OptionGroup("Rewards", [BossQuestsGiveUniques, BossRewardType, BasicLootWeight, UniqueWeight, SetItemWeight, HighRuneWeight,
                            SocketedBaseWeight, GoldWeight, ExperienceWeight, CharmWeight, JewelShare, ShrineWeight, ProphecyWeight,
                            GoldRewardPerLevel, ExperienceRewardPercent, RewardLevelBelow, RewardLevelAbove, RareJewelChance,
                            HighRuneCurve, ShrinePool],
                start_collapsed=True),
    OptionGroup("Starting Gold and Items", [TeleportCharm, StartingGold, StartingItems], start_collapsed=True),
    OptionGroup("Traps", [TrapPercentage, JunkBackpackWeight, AmbushWeight, PoisonCloudWeight, FirestormWeight, ManaDrainWeight,
                          TaxCollectorWeight, MercenaryDownWeight, TrapsWaitForTown, AmbushSize, AmbushMonsters, TaxCollectorGold,
                          JunkBackpackSize], start_collapsed=True),
    OptionGroup("Game Changes", [MonsterShuffle, MonsterShuffleTolerance, ShuffleCowLevel, SuperUniqueShuffle, MainPathSuperUniqueCap,
                                 MainPathSuperUniqueMinions, MainPathSuperUniqueStrength, MainPathSuperUniqueLevel,
                                 SuperUniqueShuffleSpread, SuperUniqueMinions, RunewordShuffle,
                                 RunewordRuneSpread, MonsterDensity, MonsterDensityNightmare, MonsterDensityHell, ChampionPacks,
                                 BetterDrops, BetterMagic, ExperienceMultiplier, ExperienceMultiplierNightmare,
                                 ExperienceMultiplierHell, DeathExperiencePenalty, ItemLevelRequirements, ItemStatRequirements,
                                 FreeMercRevive]),
    OptionGroup("D2RLoader Settings", [ShowGroundSockets, DisplayItemLevels, ItemStatRanges, MaximumSockets, FloorItemLimit,
                                       SharedStashTabs, MaterialsLimit], start_collapsed=True),
    OptionGroup("Multiworld", [DeathLink, DeathLinkMode, DeathLinkAmnesty]),
]


def loader_settings(options: D2ROptions) -> dict[str, bool | int]:
    """The D2RLoader settings d2r-ap-setup writes as the mod's overrides, by their d2rloader.toml path; settings
    left at keep (or the game's own value) aren't included."""
    settings: dict[str, bool | int] = {}
    for name in D2ROptions.type_hints:
        option = getattr(options, name)
        if isinstance(option, (LoaderToggle, LoaderRange)) and (value := option.loader_value()) is not None:
            settings[option.loader_path] = value
    return settings

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
