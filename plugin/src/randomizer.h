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
	// Percentages; 100 leaves the game as it is.
	int      monsterDensity = 100;
	int      championPacks = 100;
	int      betterDrops = 100;

	auto Any() const -> bool;
	// One line describing the settings, stored with the mod so the plugin can
	// tell whether the installed mod matches the connected seed.
	auto Stamp() const -> std::string;
};

// Locks the class skill trees: each class skill becomes its own prerequisite,
// so no skill can take its first point (item skills don't count as points).
// Level requirements drop to 0, because an item granting a skill inherits
// that skill's requirement (plus 6 for another class's skill), so the Codex
// works from level 6 whatever it holds.
// Returns how many skills it locked.
auto BlockClassSkills(Table& skills) -> int;

// Swaps whole monster line-ups (normal, nightmare/hell and unique lists move
// together) between areas: Nearby within each act, Anywhere across all of
// them. With Anywhere, each Normal-difficulty monster then becomes the
// variant of its family (zombie1..zombie5) closest to the area's level, so a
// Worldstone Keep line-up in the Blood Moor is beatable (idea from
// mapron/d2modgen). Nightmare and Hell already scale monsters to the area.
// Returns how many areas got a new line-up.
auto ShuffleMonsters(Table& levels, const Table& monstats, Shuffle mode, std::mt19937& random) -> int;

// Scales monster density and champion/unique pack counts, in percent.
void ScaleDensity(Table& levels, int densityPercent, int packsPercent);

// Makes uniques, set items and rares drop more often: divides their ratios
// in ItemRatio by percent / 100.
void BoostDrops(Table& itemratio, int percent);

// Gives field superuniques (Bishibosh, Pindleskin...) another superunique's
// monster type. Quest and scripted bosses keep theirs. Nearby only swaps
// between monsters of similar level. Returns how many changed.
auto ShuffleSuperUniques(Table& superUniques, const Table& monstats, Shuffle mode, std::mt19937& random) -> int;

// Rewrites each runeword's recipe with runes of a similar tier. Returns the new
// recipes, one "Name: Rune Rune ..." line each.
auto ShuffleRunewords(Table& runes, std::mt19937& random) -> std::vector<std::string>;

// Appends the "Archipelago Codex" unique small charm (level 1, never drops) as
// record `record`, unless the table already has it. The plugin adds the
// received skills to it when it creates the charm.
void AddCodexUnique(Table& uniques, uint32_t record);

// Multiplies monster experience on every difficulty.
void ScaleExperience(Table& monlvl, int percent);

// Clears level and/or strength and dexterity requirements in an item table
// (weapons, armor, misc, uniqueitems, setitems).
void ClearRequirements(Table& items, bool level, bool stats);

} // namespace ap::randomizer
