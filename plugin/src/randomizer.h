#pragma once

// Seeded edits to D2's excel tables, applied by d2r-ap-setup to tables read
// from the player's own install. Same seed and tables, same result.

#include "table.h"

#include <cstdint>
#include <random>
#include <string>
#include <vector>

namespace ap::randomizer {

// Must match the apworld's MonsterShuffle and SuperUniqueShuffle choices.
enum class Shuffle : int { Off = 0, Nearby = 1, Anywhere = 2 };

struct Settings {
	uint32_t seed = 0;
	Shuffle  monsters = Shuffle::Off;
	Shuffle  superUniques = Shuffle::Off;
	bool     runewords = false;
	int      xpPercent = 100;
	bool     itemLevelRequirements = true;
	bool     itemStatRequirements = true;

	auto Any() const -> bool;
	// One line describing the settings, stored with the mod so the plugin can
	// tell whether the installed mod matches the connected seed.
	auto Stamp() const -> std::string;
};

// Raises every class skill's level requirement past 99, so skill points can't
// go into the trees. Returns how many skills it blocked.
auto BlockClassSkills(Table& skills) -> int;

// Swaps whole monster line-ups (normal, nightmare/hell and unique lists move
// together) between areas: Nearby within each act, Anywhere across all of
// them. Returns how many areas got a new line-up.
auto ShuffleMonsters(Table& levels, Shuffle mode, std::mt19937& random) -> int;

// Gives field superuniques (Bishibosh, Pindleskin...) another superunique's
// monster type. Quest and scripted bosses keep theirs. Nearby only swaps
// between monsters of similar level. Returns how many changed.
auto ShuffleSuperUniques(Table& superUniques, const Table& monstats, Shuffle mode, std::mt19937& random) -> int;

// Rewrites each runeword's recipe with runes of a similar tier. Returns the new
// recipes, one "Name: Rune Rune ..." line each.
auto ShuffleRunewords(Table& runes, std::mt19937& random) -> std::vector<std::string>;

// Multiplies monster experience on every difficulty.
void ScaleExperience(Table& monlvl, int percent);

// Clears level and/or strength and dexterity requirements in an item table
// (weapons, armor, misc, uniqueitems, setitems).
void ClearRequirements(Table& items, bool level, bool stats);

} // namespace ap::randomizer
