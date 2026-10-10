#include "randomizer.h"

#include <algorithm>
#include <map>
#include <set>
#include <unordered_map>

namespace ap::randomizer {

namespace {

const std::set<std::string> ClassCodes = { "ama", "sor", "nec", "pal", "bar", "dru", "ass", "war" };

// The Cow Level and the Pandemonium (uber) levels keep their monsters.
const std::set<int> FixedLevels = { 39, 133, 134, 135, 136 };

// Superuniques that roam the field. Quest bosses (Radament, the Summoner, the
// Council, the Countess, Shenk, Nihlathak, the Ancients), the Chaos Sanctuary
// seal bosses and Baal's minion waves are scripted, so they stay as they are.
const std::set<std::string> FieldSuperUniques = {
	"Bishibosh", "Bonebreak", "Coldcrow", "Rakanishu", "Treehead WoodFist", "Griswold", "Pitspawn Fouldog", "Flamespike the Crawler", "Boneash",
	"Corpsefire", "Bloodwitch the Wild", "Fangskin", "Beetleburst", "Leatherarm", "Coldworm the Burrower", "Fire Eye", "Dark Elder",
	"Ancient Kaa the Soulless", "Web Mage the Burning", "Witch Doctor Endugu", "Stormtree", "Sarina the Battlemaid", "Icehawk Riftwing",
	"Winged Death", "The Tormentor", "Taintbreeder", "Riftwraith the Cannibal", "Axe Dweller", "Bonesaw Breaker", "Dac Farren",
	"Megaflow Rectifier", "Eyeback Unleashed", "Threash Socket", "Pindleskin", "Snapchip Shatter", "Anodized Elite", "Vinvear Molech",
	"Sharp Tooth Sayer", "Magma Torquer", "Blaze Ripper", "Frozenstein",
};

// Superuniques are swapped within groups of this many, sorted by level.
constexpr size_t NearbyGroup = 6;
// Runeword runes are replaced by one at most this many tiers away.
constexpr int RuneSpread = 2;

auto ParseInt(const std::string& text, int fallback = 0) -> int {
	try {
		return text.empty() ? fallback : std::stoi(text);
	} catch (...) {
		return fallback;
	}
}

template <typename T>
void Permute(std::vector<T>& values, std::mt19937& random) {
	std::shuffle(values.begin(), values.end(), random);
}

} // namespace

auto Settings::Any() const -> bool {
	return monsters != Shuffle::Off || superUniques != Shuffle::Off || runewords || xpPercent != 100 || !itemLevelRequirements || !itemStatRequirements;
}

auto Settings::Stamp() const -> std::string {
	return "seed=" + std::to_string(seed) + " monsters=" + std::to_string(static_cast<int>(monsters)) + " superuniques="
	     + std::to_string(static_cast<int>(superUniques)) + " runewords=" + (runewords ? "1" : "0") + " xp=" + std::to_string(xpPercent)
	     + " levelreqs=" + (itemLevelRequirements ? "1" : "0") + " statreqs=" + (itemStatRequirements ? "1" : "0");
}

auto BlockClassSkills(Table& skills) -> int {
	const int charclass = skills.Column("charclass");
	const int reqlevel = skills.Column("reqlevel");
	if (charclass < 0 || reqlevel < 0) {
		return 0;
	}
	int blocked = 0;
	for (size_t row = 0; row < skills.Rows().size(); ++row) {
		if (ClassCodes.contains(skills.Cell(row, charclass))) {
			skills.Set(row, reqlevel, "100");
			++blocked;
		}
	}
	return blocked;
}

auto ShuffleMonsters(Table& levels, Shuffle mode, std::mt19937& random) -> int {
	if (mode == Shuffle::Off) {
		return 0;
	}
	std::vector<int> columns;
	for (const char* prefix : { "mon", "nmon", "umon" }) {
		for (int i = 1; i <= 25; ++i) {
			const int column = levels.Column(prefix + std::to_string(i));
			if (column >= 0) {
				columns.push_back(column);
			}
		}
	}
	const int id = levels.Column("Id");
	const int act = levels.Column("Act");
	const int first = levels.Column("mon1");
	if (columns.empty() || id < 0 || act < 0 || first < 0) {
		return 0;
	}
	// Group the areas that have monsters, by act unless anything goes.
	std::map<int, std::vector<size_t>> groups;
	for (size_t row = 0; row < levels.Rows().size(); ++row) {
		const std::string levelId = levels.Cell(row, id);
		if (levelId.empty() || levels.Cell(row, first).empty() || FixedLevels.contains(ParseInt(levelId))) {
			continue;
		}
		groups[mode == Shuffle::Nearby ? ParseInt(levels.Cell(row, act)) : 0].push_back(row);
	}
	int changed = 0;
	for (auto& [group, rows] : groups) {
		std::vector<std::vector<std::string>> lineups;
		for (const size_t row : rows) {
			std::vector<std::string> lineup;
			for (const int column : columns) {
				lineup.push_back(levels.Cell(row, column));
			}
			lineups.push_back(std::move(lineup));
		}
		auto shuffled = lineups;
		Permute(shuffled, random);
		for (size_t i = 0; i < rows.size(); ++i) {
			changed += shuffled[i] != lineups[i] ? 1 : 0;
			for (size_t c = 0; c < columns.size(); ++c) {
				levels.Set(rows[i], columns[c], shuffled[i][c]);
			}
		}
	}
	return changed;
}

auto ShuffleSuperUniques(Table& superUniques, const Table& monstats, Shuffle mode, std::mt19937& random) -> int {
	if (mode == Shuffle::Off) {
		return 0;
	}
	const int key = superUniques.Column("Superunique");
	const int cls = superUniques.Column("Class");
	const int statsId = monstats.Column("Id");
	const int statsLevel = monstats.Column("Level");
	if (key < 0 || cls < 0) {
		return 0;
	}
	std::unordered_map<std::string, int> monsterLevel;
	for (size_t row = 0; row < monstats.Rows().size(); ++row) {
		monsterLevel[monstats.Cell(row, statsId)] = ParseInt(monstats.Cell(row, statsLevel));
	}
	std::vector<size_t> rows;
	for (size_t row = 0; row < superUniques.Rows().size(); ++row) {
		if (FieldSuperUniques.contains(superUniques.Cell(row, key)) && !superUniques.Cell(row, cls).empty()) {
			rows.push_back(row);
		}
	}
	// Sort by the monster's level so Nearby groups hold similar threats.
	std::ranges::stable_sort(rows, {}, [&](size_t row) { return monsterLevel[superUniques.Cell(row, cls)]; });
	const size_t groupSize = mode == Shuffle::Nearby ? NearbyGroup : rows.size();
	int changed = 0;
	for (size_t start = 0; start < rows.size(); start += groupSize) {
		const size_t end = std::min(rows.size(), start + groupSize);
		std::vector<std::string> classes;
		for (size_t i = start; i < end; ++i) {
			classes.push_back(superUniques.Cell(rows[i], cls));
		}
		const auto original = classes;
		Permute(classes, random);
		for (size_t i = start; i < end; ++i) {
			changed += classes[i - start] != original[i - start] ? 1 : 0;
			superUniques.Set(rows[i], cls, classes[i - start]);
		}
	}
	return changed;
}

auto ShuffleRunewords(Table& runes, std::mt19937& random) -> std::vector<std::string> {
	static constexpr const char* RuneNames[] = { "El", "Eld", "Tir", "Nef", "Eth", "Ith", "Tal", "Ral", "Ort", "Thul", "Amn", "Sol", "Shael", "Dol", "Hel",
		"Io", "Lum", "Ko", "Fal", "Lem", "Pul", "Um", "Mal", "Ist", "Gul", "Vex", "Ohm", "Lo", "Sur", "Ber", "Jah", "Cham", "Zod" };
	const int name = runes.Column("*Rune Name");
	const int complete = runes.Column("complete");
	std::vector<int> slots;
	for (int i = 1; i <= 6; ++i) {
		slots.push_back(runes.Column("Rune" + std::to_string(i)));
	}
	std::vector<std::string> recipes;
	if (complete < 0 || slots[0] < 0) {
		return recipes;
	}
	for (size_t row = 0; row < runes.Rows().size(); ++row) {
		if (runes.Cell(row, complete) != "1" || runes.Cell(row, slots[0]).empty()) {
			continue;
		}
		std::string recipe = (name >= 0 ? runes.Cell(row, name) : "Runeword") + ":";
		for (const int slot : slots) {
			const std::string rune = runes.Cell(row, slot);
			if (slot < 0 || rune.size() != 3 || rune[0] != 'r') {
				continue;
			}
			const int tier = ParseInt(rune.substr(1));
			std::uniform_int_distribution<int> pick(std::max(1, tier - RuneSpread), std::min(33, tier + RuneSpread));
			const int replacement = pick(random);
			char code[4] {};
			code[0] = 'r';
			code[1] = static_cast<char>('0' + replacement / 10);
			code[2] = static_cast<char>('0' + replacement % 10);
			runes.Set(row, slot, code);
			recipe += std::string(" ") + RuneNames[replacement - 1];
		}
		recipes.push_back(recipe);
	}
	return recipes;
}

void ScaleExperience(Table& monlvl, int percent) {
	if (percent == 100) {
		return;
	}
	for (const char* name : { "XP", "XP(N)", "XP(H)", "L-XP", "L-XP(N)", "L-XP(H)" }) {
		const int column = monlvl.Column(name);
		for (size_t row = 0; column >= 0 && row < monlvl.Rows().size(); ++row) {
			const std::string value = monlvl.Cell(row, column);
			if (!value.empty()) {
				monlvl.Set(row, column, std::to_string(static_cast<long long>(ParseInt(value)) * percent / 100));
			}
		}
	}
}

void ClearRequirements(Table& items, bool level, bool stats) {
	std::vector<int> columns;
	if (level) {
		for (const char* name : { "levelreq", "lvl req" }) {
			columns.push_back(items.Column(name));
		}
	}
	if (stats) {
		columns.push_back(items.Column("reqstr"));
		columns.push_back(items.Column("reqdex"));
	}
	for (const int column : columns) {
		for (size_t row = 0; column >= 0 && row < items.Rows().size(); ++row) {
			if (!items.Cell(row, column).empty()) {
				items.Set(row, column, "0");
			}
		}
	}
}

} // namespace ap::randomizer
