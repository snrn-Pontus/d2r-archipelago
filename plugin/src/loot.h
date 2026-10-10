#pragma once

// Rolls the actual item for a received loot item, scaled to the character's
// level. Pure functions, so the rules can be tested without the game.

#include <D2RLPlugin/item.h>

#include <cstdint>
#include <random>
#include <string>

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

// A level estimate from where the character is, for when no level-up has
// been seen yet: the start of the difficulty plus a few levels per act.
auto EstimateLevel(int difficulty, int act) -> int;

} // namespace ap::loot
