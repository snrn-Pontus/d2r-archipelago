#pragma once

// Seeded edits to D2's excel tables, applied by d2r-ap-setup to tables read
// from the player's own install. Same seed and tables, same result.

#include "table.h"

#include <cstdint>
#include <random>
#include <string>
#include <utility>
#include <vector>

namespace ap::randomizer {

// Must match the apworld's MonsterShuffle and SuperUniqueShuffle choices.
enum class Shuffle : int { Off = 0, Nearby = 1, Anywhere = 2 };

// The skill tree shuffle: (skill id, id of the skill whose tree slot it takes).
using SkillTree = std::vector<std::pair<uint32_t, uint32_t>>;

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
	// Field superuniques get monster rows of their own, so their kills are checks.
	bool     superUniqueChecks = false;
	// The skill tree shuffle: (skill id, id of the skill whose tree slot it takes).
	SkillTree skillTree;
	// Tree mode: skills are unlocked in the tree, so the trees keep their level requirements.
	bool     treeMode = false;

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
// keepLevels leaves the level requirements (tree mode, where you spend points).
// Returns how many skills it locked.
auto BlockClassSkills(Table& skills, bool keepLevels = false) -> int;

// A skill's icon after a tree shuffle: frame `fromCel` of its old class's icon
// sheet belongs at frame `toCel` (and the pressed frames after each) of the new one.
struct IconMove {
	std::string fromClass;
	int         fromCel = 0;
	std::string toClass;
	int         toCel = 0;
};

struct TreeShuffle {
	// Skills that changed class.
	int moved = 0;
	std::vector<IconMove> icons;
};

// Applies the seed's skill tree shuffle (dealt by the apworld): each skill takes
// the tree slot its pair names, with the slot's class and level requirement
// (skills.txt) and tab, place and icon (skilldesc.txt). Synergies then point at
// skills in each skill's new tree.
auto ApplySkillTrees(Table& skills, Table& skilldesc, const SkillTree& tree) -> TreeShuffle;

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

// Gives each field superunique (data::SuperUniques, in order) a monster row of
// its own, a copy of its current monster appended to MonStats, so a kill tells
// which superunique died: the copies are the last rows, in the same order.
// Run it after ShuffleSuperUniques. Returns how many superuniques it linked.
inline constexpr const char* SuperUniqueMonsterPrefix = "ap_superunique_";
auto AddSuperUniqueMonsters(Table& monstats, Table& superUniques) -> int;

// Rewrites each runeword's recipe with runes of a similar tier. Returns the new
// recipes, one "Name: Rune Rune ..." line each.
auto ShuffleRunewords(Table& runes, std::mt19937& random) -> std::vector<std::string>;

// Appends the "Archipelago Codex" unique small charm (level 1, never drops) as
// record `record`, unless the table already has it. The plugin adds the
// received skills to it when it creates the charm.
void AddCodexUnique(Table& uniques, uint32_t record);

// The shop checks' items, one per act: bought from that act's potion seller,
// then the plugin sends a check and removes it.
inline constexpr const char* ParcelCodes[] = { "ap1", "ap2", "ap3", "ap4", "ap5" };
inline constexpr const char* ParcelVendors[] = { "Akara", "Lysander", "Ormus", "Jamella", "Malah" };
inline constexpr int         ParcelCosts[] = { 2500, 7500, 15000, 25000, 40000 };
inline constexpr char        ParcelName[] = "Archipelago Parcel";

// Scrolls of Prophecy: a hint each, sold by every potion seller in rising price
// tiers; only the next tier counts.
inline constexpr const char* ProphecyCodes[] = { "aq1", "aq2", "aq3", "aq4", "aq5", "aq6", "aq7", "aq8" };
inline constexpr int         ProphecyCosts[] = { 1000, 2500, 5000, 10000, 20000, 35000, 60000, 100000 };
inline constexpr char        ProphecyName[] = "Scroll of Prophecy";

// The Archipelago Ledger: every character gets one, and its tooltip counts the
// checks done in each act. It's never sold and never drops.
inline constexpr char        LedgerCode[] = "apl";
inline constexpr char        LedgerName[] = "Archipelago Ledger";

// Appends the parcels to misc.txt as copies of the Standard of Heroes (an inert
// 1x1 item), each always in stock at its act's vendor, unless the table
// already has them. Returns how many it added.
auto AddShopParcels(Table& misc) -> int;

// Appends the Scrolls of Prophecy the same way, every tier at every potion seller.
auto AddProphecyScrolls(Table& misc) -> int;

// Appends the Ledger the same way, sold nowhere. Returns 1 if it added it.
auto AddLedger(Table& misc) -> int;

// Multiplies monster experience on every difficulty.
void ScaleExperience(Table& monlvl, int percent);

// Clears level and/or strength and dexterity requirements in an item table
// (weapons, armor, misc, uniqueitems, setitems).
void ClearRequirements(Table& items, bool level, bool stats);

} // namespace ap::randomizer
