#pragma once

// Rolls the actual item for a received loot item, scaled to the character's
// level. Pure functions, so the rules can be tested without the game.

#include <D2RLPlugin/item.h>

#include <cstdint>
#include <random>
#include <span>
#include <string>
#include <vector>

namespace ap::loot {

// Index of each loot item; its id is ids::LootBase + index. Must match LOOT
// in apworld/d2r_archipelago/ids.py.
enum class Loot : uint32_t {
	FullRejuvenation,
	PerfectGem,
	LowRune,
	MidRune,
	HighRune,
	RandomUnique,
	UniqueWeapon,
	UniqueArmor,
	UniqueJewelry,
	SetItem,
	SocketedBase,
	JunkBackpack,
	// Gold and Experience aren't items: the plugin adds them through D2RCore.
	Gold,
	Experience,
	RandomCharm,
	RandomJewel,
	// Also through D2RCore commands: a shrine at your feet, and traps.
	RandomShrine,
	AmbushTrap,
	PoisonCloudTrap,
	FirestormTrap,
	ManaDrainTrap,
	TaxCollectorTrap,
	MercenaryDownTrap,
	// A hint, like a Scroll of Prophecy.
	Prophecy,
	Count,
};

struct Pick {
	uint32_t             code = 0;
	D2RL::Items::Quality quality = D2RL::Items::Quality::Normal;
	// UniqueItems/SetItems *ID, or RandomQualityRecord.
	uint32_t             record = D2RL::Items::RandomQualityRecord;
	uint32_t             itemLevel = 1;
	uint32_t             sockets = 0;
	std::string          name;
};

auto CodeFromText(const char* text) -> uint32_t;

// The helpful Shrines.txt rows a Random Shrine can be: Refilling, Armor,
// Combat, the four resistances, Skill, Mana Recharge, Experience, Gem.
inline constexpr int HelpfulShrines[] = { 1, 6, 7, 8, 9, 10, 11, 12, 13, 15, 18 };

// The seed's reward options; the defaults are the rewards from before these
// were options.
struct RewardSettings {
	int goldPerLevel = 1000;
	// Percent of what the current level needs to level up.
	int experiencePercent = 15;
	// A unique or set item is level-appropriate when its requirement is at most
	// levelBelow under the character and at most levelAbove over it.
	int levelBelow = 12;
	int levelAbove = 2;
	int rareJewelPercent = 30;
	// High rune n (Pul 21 .. Zod 33) weighs (34 - n) to this power: 0 makes
	// them all alike, 2 makes Zod 169 times rarer than Pul.
	int highRuneCurve = 2;
	// The HelpfulShrines a Random Shrine picks from; empty (or none helpful)
	// means all of them.
	std::vector<int> shrines;
};

// Rolls one item. For JunkBackpack it returns one piece of junk; the caller
// repeats until the inventory is full. playerClass (ClassNames order, -1 for
// unknown) leaves out uniques, sets and bases only another class can use.
auto Roll(Loot loot, int level, std::mt19937& random, int playerClass = -1, const RewardSettings& rewards = {}) -> Pick;

// Gold for a Gold reward: goldPerLevel per character level.
auto GoldAmount(int level, const RewardSettings& rewards = {}) -> int;

// Experience for an Experience reward: experiencePercent of what the level
// needs to level up (0 at level 99).
auto ExperienceAmount(int level, const RewardSettings& rewards = {}) -> int64_t;

// A Shrines.txt row for a Random Shrine, from pool (see RewardSettings::shrines).
auto ShrineId(std::mt19937& random, std::span<const int> pool = {}) -> int;

// A MonStats id for an Ambush in this act (0-4): a common monster of the act.
auto AmbushMonster(int act, std::mt19937& random) -> const char*;

// The D2RCore console commands for a Random Shrine or a trap, and the line
// that announces it. Empty for other loot.
struct Commands {
	std::vector<std::string> commands;
	std::string              message;
	bool                     trap = false;
};
// The seed's trap options.
struct TrapSettings {
	int ambushSize = 6;
	int taxPerLevel = 1000;
};
// An Ambush picks from areaMonsters (MonStats ids) when there are any, and
// from the act's common monsters otherwise. A Random Shrine picks from
// rewards.shrines.
auto CommandsFor(Loot loot, int level, int act, std::mt19937& random, const TrapSettings& traps = {},
                 std::span<const std::string> areaMonsters = {}, const RewardSettings& rewards = {}) -> Commands;

// A level estimate from where the character is, for when no level-up has
// been seen yet: the start of the difficulty plus a few levels per act.
auto EstimateLevel(int difficulty, int act) -> int;

} // namespace ap::loot
