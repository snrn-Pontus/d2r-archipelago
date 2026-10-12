#pragma once

// Rolls the actual item for a received loot item, scaled to the character's
// level. Pure functions, so the rules can be tested without the game.

#include <D2RLPlugin/item.h>

#include <cstdint>
#include <random>
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

// Rolls one item. For JunkBackpack it returns one piece of junk; the caller
// repeats until the inventory is full.
auto Roll(Loot loot, int level, std::mt19937& random) -> Pick;

// Gold for a Gold reward: 1,000 per character level.
auto GoldAmount(int level) -> int;

// Experience for an Experience reward: 15% of what the level needs to level
// up (0 at level 99).
auto ExperienceAmount(int level) -> int64_t;

// A helpful Shrines.txt row for a Random Shrine.
auto ShrineId(std::mt19937& random) -> int;

// A MonStats id for an Ambush in this act (0-4): a common monster of the act.
auto AmbushMonster(int act, std::mt19937& random) -> const char*;

// The D2RCore console commands for a Random Shrine or a trap, and the line
// that announces it. Empty for other loot.
struct Commands {
	std::vector<std::string> commands;
	std::string              message;
	bool                     trap = false;
};
auto CommandsFor(Loot loot, int level, int act, std::mt19937& random) -> Commands;

// A level estimate from where the character is, for when no level-up has
// been seen yet: the start of the difficulty plus a few levels per act.
auto EstimateLevel(int difficulty, int act) -> int;

} // namespace ap::loot
