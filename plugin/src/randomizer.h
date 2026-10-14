#pragma once

// Seeded edits to D2's excel tables, applied by d2r-ap-setup to tables read
// from the player's own install. Same seed and tables, same result.

#include "table.h"

#include <array>
#include <cstdint>
#include <optional>
#include <random>
#include <string>
#include <utility>
#include <vector>

namespace ap::randomizer {

// Must match the apworld's MonsterShuffle and SuperUniqueShuffle choices.
enum class Shuffle : int { Off = 0, Nearby = 1, Anywhere = 2 };

// One value for each difficulty: Normal, Nightmare, Hell.
using PerDifficulty = std::array<int, 3>;

// The skill tree shuffle: (skill id, id of the skill whose tree slot it takes).
using SkillTree = std::vector<std::pair<uint32_t, uint32_t>>;

struct Settings {
	uint32_t seed = 0;
	Shuffle  monsters = Shuffle::Off;
	Shuffle  superUniques = Shuffle::Off;
	// Levels a main-path superunique's new monster may be above its own; negative is no cap.
	int      mainPathSuperUniqueCap = -1;
	// Most minions a main-path superunique with a new monster brings; negative is no cap.
	int      mainPathSuperUniqueMinions = -1;
	// HP and damage of main-path superuniques, in percent, on every difficulty.
	int      mainPathSuperUniqueStrength = 100;
	// Normal: a main-path superunique's level is its own monster's usual level
	// plus this, whatever it became. Unset keeps the new monster's level.
	std::optional<int> mainPathSuperUniqueLevel;
	// Superuniques are swapped within groups of this many (Nearby), sorted by level.
	int      superUniqueSpread = 6;
	// Anywhere: an area takes a line-up only if every monster in it has a
	// variant at most this many levels above the area's level.
	int      shuffleTolerance = 3;
	// The Secret Cow Level trades line-ups too.
	bool     shuffleCowLevel = false;
	bool     runewords = false;
	// Runeword runes are replaced by one at most this many tiers away.
	int      runeSpread = 2;
	bool     itemLevelRequirements = true;
	bool     itemStatRequirements = true;
	// Percentages; 100 leaves the game as it is.
	PerDifficulty xpPercent = { 100, 100, 100 };
	PerDifficulty monsterDensity = { 100, 100, 100 };
	int      championPacks = 100;
	int      superUniqueMinions = 100;
	int      betterDrops = 100;
	int      betterMagic = 100;
	// Field superuniques get monster rows of their own, so their kills are checks.
	bool     superUniqueChecks = false;
	// The skill tree shuffle: (skill id, id of the skill whose tree slot it takes).
	SkillTree skillTree;
	// Tree mode: skills are unlocked in the tree, so the trees keep their level requirements.
	bool     treeMode = false;
	// Tree mode: this class's starting weapon gives +1 to startSkill (none when negative).
	// An empty class leaves charstats.txt alone.
	std::string startClass;
	int      startSkill = -1;

	auto Any() const -> bool;
	// The superuniques need monster rows of their own (AddSuperUniqueMonsters):
	// for their checks, or to change their level or strength.
	auto SuperUniqueCopies() const -> bool;
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

// Sets a class's StartSkill in charstats.txt, the +1 skill on its starting
// weapon (Fire Bolt on the Sorceress's staff), to the skills.txt skill with
// this id; a negative id clears it. Returns whether the class's row was found.
auto SetStartSkill(Table& charstats, const Table& skills, const std::string& className, int skill) -> bool;

// Swaps whole monster line-ups (normal, nightmare/hell and unique lists move
// together) between areas: Nearby within each act, Anywhere across all of
// them. With Anywhere, each Normal-difficulty monster then becomes the
// variant of its family (zombie1..zombie5) closest to the area's level, so a
// Worldstone Keep line-up in the Blood Moor is beatable (idea from
// mapron/d2modgen). Nightmare and Hell already scale monsters to the area.
// Returns how many areas got a new line-up.
// `tolerance` is how many levels above an area Anywhere lets its line-up's
// lowest variants be. The Secret Cow Level keeps its cows unless shuffleCows.
auto ShuffleMonsters(Table& levels, const Table& monstats, Shuffle mode, std::mt19937& random, int tolerance = 3, bool shuffleCows = false) -> int;

// Scales monster density (per difficulty) and champion/unique pack counts, in percent.
void ScaleDensity(Table& levels, const PerDifficulty& densityPercent, int packsPercent);

// Changes how often monsters drop uniques, set items and rares (percent) and
// magic items (magicPercent): divides their ratios in ItemRatio by percent / 100.
void BoostDrops(Table& itemratio, int percent, int magicPercent = 100);

// Gives field superuniques (Bishibosh, Pindleskin...) another superunique's
// monster type. Quest and scripted bosses keep theirs. Nearby only swaps
// between monsters of similar level. With mainPathCap of 0 or more, a
// superunique on the main path (an act's route or a quest objective) only gets
// a monster at most that many levels above its own; negative means no cap.
// Nearby swaps within groups of `group`. Returns how many changed.
auto ShuffleSuperUniques(Table& superUniques, const Table& monstats, Shuffle mode, std::mt19937& random, int mainPathCap = -1, int group = 6) -> int;

// Scales the field superuniques' minion counts (MinGrp/MaxGrp) by percent.
// A main-path superunique whose monster differs from `before` (the table as it
// was before the shuffle) then brings at most mainPathMax, unless that's negative.
// Returns how many superuniques changed.
auto ScaleSuperUniqueMinions(Table& superUniques, const Table& before, int percent, int mainPathMax) -> int;

// Gives each field superunique (data::SuperUniques, in order) a monster row of
// its own, a copy of its current monster appended to MonStats, so a kill tells
// which superunique died: the copies are the last rows, in the same order.
// Run it after ShuffleSuperUniques. Returns how many superuniques it linked;
// `copies` gets each linked copy's Id and the monster it copies, which the HD
// client needs to find the copy's model.
inline constexpr const char* SuperUniqueMonsterPrefix = "ap_superunique_";
auto AddSuperUniqueMonsters(Table& monstats, Table& superUniques, std::vector<std::pair<std::string, std::string>>* copies = nullptr) -> int;

// Tunes the main-path superuniques' own monster rows (made by
// AddSuperUniqueMonsters): HP and damage times strengthPercent / 100 on every
// difficulty, and with `level` set, Normal's level becomes the level of the
// monster the superunique had in `before` (superuniques.txt before the
// shuffle) plus `level`, at least 1. Minions keep their own rows. Returns how
// many superuniques changed.
auto TuneMainPathSuperUniques(Table& monstats, const Table& before, int strengthPercent, std::optional<int> level) -> int;

// Rewrites each runeword's recipe with runes of a similar tier. Returns the new
// recipes, one "Name: Rune Rune ..." line each.
// Each rune moves at most `spread` tiers.
auto ShuffleRunewords(Table& runes, std::mt19937& random, int spread = 2) -> std::vector<std::string>;

// One runeword's new recipe and the banks (indexes into the tables given to
// ShuffleRunewords) that make it this way.
struct RunewordRecipe {
	std::string         text; // "Name: Rune Rune ..."
	std::vector<size_t> banks;
};

// Shuffles several runes.txt banks (RotW's and Classic/LoD's) alike: a runeword
// with the same Name and the same runes in several banks gets one new recipe in
// all of them; the rest (only in one bank, or with other runes there) get their
// own. No two complete runewords in a bank end with one sequence. With one bank
// it rolls exactly like the single-table ShuffleRunewords.
auto ShuffleRunewords(const std::vector<Table*>& banks, std::mt19937& random, int spread = 2) -> std::vector<RunewordRecipe>;

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

// Vendors only stock item types with a StorePage, and the quest type has none,
// so the shop items get their own: the quest type on the Misc page.
inline constexpr char        ShopItemType[] = "apsh";

// The Archipelago Emblem: in tree mode (no Codex) the seed's character carries one,
// and its seed says the character's class. It's never sold and never drops.
inline constexpr char        EmblemCode[] = "ape";
inline constexpr char        EmblemName[] = "Archipelago Emblem";

// Appends the parcels to misc.txt as copies of the Standard of Heroes (an inert
// 1x1 item), each always in stock at its act's vendor, unless the table
// already has them. Returns how many it added.
auto AddShopParcels(Table& misc) -> int;

// Appends ShopItemType to itemtypes.txt, unless it's there. Returns 1 if it added it.
auto AddShopItemType(Table& itemTypes) -> int;

// D2R picks an item's HD picture from items.json by its place in the item
// tables, and both banks share one items.json. Appends a placeholder row for
// each of the other bank's codes this misc.txt lacks (RotW's last items, in
// Classic/LoD), so the items added after them line up in both banks.
auto PadMiscRows(Table& misc, const std::vector<std::string>& codes) -> int;

// Appends the Scrolls of Prophecy the same way, every tier at every potion seller.
auto AddProphecyScrolls(Table& misc) -> int;

// Appends the Emblem the same way, sold nowhere. Returns 1 if it added it.
auto AddEmblem(Table& misc) -> int;

// A cheap respec, always in stock at every potion seller: a copy of the Token
// of Absolution (same use, picture and size) under its own code, so finding a
// real Token of Absolution stays a check. Returns 1 if it added it.
inline constexpr char RespecCode[] = "apr";
inline constexpr char RespecName[] = "Writ of Absolution";
inline constexpr int  RespecCost   = 1000;
auto AddRespecWrit(Table& misc) -> int;

// Multiplies monster experience, in percent for each difficulty.
void ScaleExperience(Table& monlvl, const PerDifficulty& percent);

// Clears level and/or strength and dexterity requirements in an item table
// (weapons, armor, misc, uniqueitems, setitems).
void ClearRequirements(Table& items, bool level, bool stats);

} // namespace ap::randomizer
