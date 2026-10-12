#include "randomizer.h"

#include "game_data.h"

#include <algorithm>
#include <map>
#include <optional>
#include <set>
#include <unordered_map>

namespace ap::randomizer {

namespace {

const std::set<std::string> ClassCodes = { "ama", "sor", "nec", "pal", "bar", "dru", "ass", "war" };

// The Cow Level and the Pandemonium (uber) levels keep their monsters.
const std::set<int> FixedLevels = { 39, 133, 134, 135, 136 };

// Superuniques that roam the field (gen_data.py's list). Quest bosses, the
// Chaos Sanctuary seal bosses and Baal's minion waves are scripted, so they
// stay as they are.
auto IsFieldSuperUnique(const std::string& name) -> bool {
	return std::ranges::any_of(data::SuperUniques, [&](const auto& entry) { return name == entry.name; });
}

// Anywhere: an area takes a line-up only if every monster in it has a
// variant at most this many levels above the area's level.
constexpr int AnywhereTolerance = 3;
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
	return monsters != Shuffle::Off || superUniques != Shuffle::Off || runewords || xpPercent != 100 || !itemLevelRequirements || !itemStatRequirements
	    || monsterDensity != 100 || championPacks != 100 || betterDrops != 100 || superUniqueChecks;
}

auto Settings::Stamp() const -> std::string {
	return "seed=" + std::to_string(seed) + " monsters=" + std::to_string(static_cast<int>(monsters)) + " superuniques="
	     + std::to_string(static_cast<int>(superUniques)) + " runewords=" + (runewords ? "1" : "0") + " xp=" + std::to_string(xpPercent)
	     + " levelreqs=" + (itemLevelRequirements ? "1" : "0") + " statreqs=" + (itemStatRequirements ? "1" : "0") + " density="
	     + std::to_string(monsterDensity) + " packs=" + std::to_string(championPacks) + " drops=" + std::to_string(betterDrops)
	     + (superUniqueChecks ? " superunique_checks=1" : "");
}

auto BlockClassSkills(Table& skills) -> int {
	const int name = skills.Column("skill");
	const int charclass = skills.Column("charclass");
	const int reqlevel = skills.Column("reqlevel");
	const int reqskill[] = { skills.Column("reqskill1"), skills.Column("reqskill2"), skills.Column("reqskill3") };
	if (name < 0 || charclass < 0 || reqlevel < 0 || reqskill[0] < 0) {
		return 0;
	}
	int locked = 0;
	for (size_t row = 0; row < skills.Rows().size(); ++row) {
		if (!ClassCodes.contains(skills.Cell(row, charclass))) {
			continue;
		}
		skills.Set(row, reqlevel, "0");
		skills.Set(row, reqskill[0], skills.Cell(row, name));
		skills.Set(row, reqskill[1], "");
		skills.Set(row, reqskill[2], "");
		++locked;
	}
	return locked;
}

// Picks the variant of a monster's family whose Normal level is closest to
// the area's level (ties go to the weaker one).
class FamilyLevels {
public:
	explicit FamilyLevels(const Table& monstats) {
		const int id = monstats.Column("Id");
		const int base = monstats.Column("BaseId");
		const int level = monstats.Column("Level");
		for (size_t row = 0; row < monstats.Rows().size(); ++row) {
			const int value = ParseInt(monstats.Cell(row, level));
			const std::string family = monstats.Cell(row, base);
			if (value > 0 && !family.empty()) {
				familyOf_[monstats.Cell(row, id)] = family;
				members_[family].push_back({ value, monstats.Cell(row, id) });
			}
		}
	}

	auto Closest(const std::string& monster, int areaLevel) const -> std::string {
		const auto family = familyOf_.find(monster);
		if (family == familyOf_.end()) {
			return monster;
		}
		std::string best = monster;
		int bestDistance = INT32_MAX;
		for (const auto& [level, id] : members_.at(family->second)) {
			const int distance = std::abs(level - areaLevel) * 2 + (level > areaLevel ? 1 : 0);
			if (distance < bestDistance) {
				bestDistance = distance;
				best = id;
			}
		}
		return best;
	}

	// The lowest level the monster's family can appear at (0 if unknown).
	auto Floor(const std::string& monster) const -> int {
		const auto family = familyOf_.find(monster);
		if (family == familyOf_.end()) {
			return 0;
		}
		int lowest = INT32_MAX;
		for (const auto& [level, id] : members_.at(family->second)) {
			lowest = std::min(lowest, level);
		}
		return lowest;
	}

private:
	std::unordered_map<std::string, std::string> familyOf_;
	std::unordered_map<std::string, std::vector<std::pair<int, std::string>>> members_;
};

// Gives each area a random line-up whose monsters can all be scaled near its
// level: the lowest areas pick first, from line-ups whose floor (the highest
// of its monsters' lowest family levels) is within AnywhereTolerance.
auto AssignByLevel(const Table& levels, const std::vector<size_t>& rows, const std::vector<std::vector<std::string>>& lineups, int areaLevel,
	size_t normalCount, const FamilyLevels& families, std::mt19937& random) -> std::vector<std::vector<std::string>> {
	std::vector<int> floors;
	for (const auto& lineup : lineups) {
		int floor = 0;
		for (size_t i = 0; i < normalCount && i < lineup.size(); ++i) {
			if (!lineup[i].empty()) {
				floor = std::max(floor, families.Floor(lineup[i]));
			}
		}
		floors.push_back(floor);
	}
	std::vector<size_t> order(rows.size());
	for (size_t i = 0; i < order.size(); ++i) {
		order[i] = i;
	}
	Permute(order, random);
	std::ranges::stable_sort(order, {}, [&](size_t i) { return ParseInt(levels.Cell(rows[i], areaLevel)); });

	std::vector<bool> used(lineups.size(), false);
	std::vector<std::vector<std::string>> assigned(rows.size());
	for (const size_t area : order) {
		const int level = ParseInt(levels.Cell(rows[area], areaLevel));
		std::vector<size_t> fits;
		size_t lowest = SIZE_MAX;
		for (size_t i = 0; i < lineups.size(); ++i) {
			if (used[i]) {
				continue;
			}
			if (floors[i] <= level + AnywhereTolerance) {
				fits.push_back(i);
			}
			if (lowest == SIZE_MAX || floors[i] < floors[lowest]) {
				lowest = i;
			}
		}
		const size_t pick = fits.empty() ? lowest : fits[random() % fits.size()];
		used[pick] = true;
		assigned[area] = lineups[pick];
	}
	return assigned;
}

auto ShuffleMonsters(Table& levels, const Table& monstats, Shuffle mode, std::mt19937& random) -> int {
	if (mode == Shuffle::Off) {
		return 0;
	}
	// Columns in line-up order: mon1..25 (Normal), then nmon, then umon.
	std::vector<int> columns;
	size_t normalCount = 0;
	for (const char* prefix : { "mon", "nmon", "umon" }) {
		for (int i = 1; i <= 25; ++i) {
			const int column = levels.Column(prefix + std::to_string(i));
			if (column >= 0) {
				columns.push_back(column);
				normalCount += std::string(prefix) == "mon" ? 1 : 0;
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
	const FamilyLevels families(monstats);
	// Normal-difficulty area level: MonLvlEx in the expansion banks, MonLvl in Classic.
	const int areaLevel = levels.Column("MonLvlEx") >= 0 ? levels.Column("MonLvlEx") : levels.Column("MonLvl");
	std::vector<int> normalColumns;
	for (const char* prefix : { "mon", "umon" }) {
		for (int i = 1; i <= 25; ++i) {
			const int column = levels.Column(prefix + std::to_string(i));
			if (column >= 0) {
				normalColumns.push_back(column);
			}
		}
	}
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
		if (mode == Shuffle::Anywhere && areaLevel >= 0) {
			shuffled = AssignByLevel(levels, rows, lineups, areaLevel, normalCount, families, random);
		}
		for (size_t i = 0; i < rows.size(); ++i) {
			changed += shuffled[i] != lineups[i] ? 1 : 0;
			for (size_t c = 0; c < columns.size(); ++c) {
				levels.Set(rows[i], columns[c], shuffled[i][c]);
			}
			if (mode == Shuffle::Anywhere && areaLevel >= 0) {
				const int level = ParseInt(levels.Cell(rows[i], areaLevel));
				for (const int column : normalColumns) {
					const std::string monster = levels.Cell(rows[i], column);
					if (!monster.empty() && level > 0) {
						levels.Set(rows[i], column, families.Closest(monster, level));
					}
				}
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
		if (IsFieldSuperUnique(superUniques.Cell(row, key)) && !superUniques.Cell(row, cls).empty()) {
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

auto AddSuperUniqueMonsters(Table& monstats, Table& superUniques) -> int {
	const int id = monstats.Column("Id");
	const int index = monstats.Column("*hcIdx");
	const int minion = monstats.Column("minion1");
	const int key = superUniques.Column("Superunique");
	const int cls = superUniques.Column("Class");
	if (id < 0 || minion < 0 || key < 0 || cls < 0 || monstats.Rows().empty()) {
		return 0;
	}
	std::unordered_map<std::string, size_t> monsterRow;
	for (size_t row = 0; row < monstats.Rows().size(); ++row) {
		monsterRow.emplace(monstats.Cell(row, id), row);
	}
	std::unordered_map<std::string, size_t> superUniqueRow;
	for (size_t row = 0; row < superUniques.Rows().size(); ++row) {
		superUniqueRow.emplace(superUniques.Cell(row, key), row);
	}
	int linked = 0;
	for (const auto& entry : data::SuperUniques) {
		// Every entry gets a row, so the plugin finds each one at a fixed
		// distance from the end of the table. One without a superunique copies
		// the first monster, which then never spawns.
		const auto found = superUniqueRow.find(entry.name);
		const std::string original = found != superUniqueRow.end() ? superUniques.Cell(found->second, cls) : "";
		const auto source = monsterRow.find(original);
		std::vector<std::string> copy = monstats.Rows()[source != monsterRow.end() ? source->second : 0];
		const std::string name = std::string(SuperUniqueMonsterPrefix) + std::to_string(entry.id);
		// *hcIdx is only a comment; continue the numbering (divider rows have none).
		const int previous = index >= 0 ? ParseInt(monstats.Cell(monstats.Rows().size() - 1, index), -1) : -1;
		monstats.Rows().push_back(std::move(copy));
		const size_t row = monstats.Rows().size() - 1;
		monstats.Set(row, id, name);
		if (index >= 0) {
			monstats.Set(row, index, previous >= 0 ? std::to_string(previous + 1) : "");
		}
		if (source == monsterRow.end()) {
			continue;
		}
		// Minions come from minion1 if it's set and are copies of the boss
		// otherwise; they stay the original monster, so only the boss counts.
		if (monstats.Cell(row, minion).empty()) {
			monstats.Set(row, minion, original);
		}
		superUniques.Set(found->second, cls, name);
		++linked;
	}
	return linked;
}

void AddCodexUnique(Table& uniques, uint32_t record) {
	const int index = uniques.Column("index");
	if (index < 0) {
		return;
	}
	for (size_t row = 0; row < uniques.Rows().size(); ++row) {
		if (uniques.Cell(row, index) == "Archipelago Codex") {
			return;
		}
	}
	const std::pair<const char*, std::string> values[] = {
		{ "index", "Archipelago Codex" }, { "*ID", std::to_string(record) }, { "version", "100" }, { "spawnable", "0" }, { "rarity", "1" },
		{ "lvl", "1" }, { "lvl req", "1" }, { "code", "cm1" }, { "*ItemName", "charm" }, { "cost mult", "1" }, { "cost add", "0" },
		{ "dropsound", "item_gem" }, { "dropsfxframe", "12" }, { "usesound", "item_gem" }, { "prop1", "light" }, { "min1", "1" }, { "max1", "1" },
		{ "*eol", "0" },
	};
	uniques.Rows().emplace_back();
	const size_t row = uniques.Rows().size() - 1;
	for (const auto& [name, value] : values) {
		uniques.Set(row, uniques.Column(name), value);
	}
	// Pad the row to the header width so every column is present.
	const int last = uniques.Column("*eol");
	if (last >= 0 && uniques.Rows()[row].size() < static_cast<size_t>(last) + 1) {
		uniques.Rows()[row].resize(static_cast<size_t>(last) + 1);
	}
}

auto AddShopParcels(Table& misc) -> int {
	const int code = misc.Column("code");
	if (code < 0) {
		return 0;
	}
	std::optional<size_t> templateRow;
	for (size_t row = 0; row < misc.Rows().size(); ++row) {
		if (misc.Cell(row, code) == ParcelCodes[0]) {
			return 0;
		}
		if (misc.Cell(row, code) == "std") {
			templateRow = row;
		}
	}
	if (!templateRow) {
		return 0;
	}
	for (size_t act = 0; act < std::size(ParcelCodes); ++act) {
		misc.Rows().push_back(misc.Rows()[*templateRow]);
		const size_t row = misc.Rows().size() - 1;
		const std::string vendor = ParcelVendors[act];
		const std::pair<std::string, std::string> values[] = {
			{ "name", ParcelName }, { "code", ParcelCodes[act] }, { "namestr", ParcelName }, { "cost", std::to_string(ParcelCosts[act]) },
			{ "gamble cost", "0" }, { "level", "0" }, { "levelreq", "0" }, { "unique", "0" }, { "spawnable", "0" }, { "PermStoreItem", "1" },
			{ vendor + "Min", "1" }, { vendor + "Max", "1" }, { "multibuy", "0" },
		};
		for (const auto& [name, value] : values) {
			if (const int column = misc.Column(name); column >= 0) {
				misc.Set(row, column, value);
			}
		}
	}
	return static_cast<int>(std::size(ParcelCodes));
}

auto AddProphecyScrolls(Table& misc) -> int {
	const int code = misc.Column("code");
	if (code < 0) {
		return 0;
	}
	std::optional<size_t> templateRow;
	for (size_t row = 0; row < misc.Rows().size(); ++row) {
		if (misc.Cell(row, code) == ProphecyCodes[0]) {
			return 0;
		}
		if (misc.Cell(row, code) == "std") {
			templateRow = row;
		}
	}
	if (!templateRow) {
		return 0;
	}
	for (size_t tier = 0; tier < std::size(ProphecyCodes); ++tier) {
		misc.Rows().push_back(misc.Rows()[*templateRow]);
		const size_t row = misc.Rows().size() - 1;
		std::vector<std::pair<std::string, std::string>> values = {
			{ "name", ProphecyName }, { "code", ProphecyCodes[tier] }, { "namestr", ProphecyName }, { "cost", std::to_string(ProphecyCosts[tier]) },
			{ "gamble cost", "0" }, { "level", "0" }, { "levelreq", "0" }, { "unique", "0" }, { "spawnable", "0" }, { "PermStoreItem", "1" },
			{ "multibuy", "0" },
		};
		for (const char* vendor : ParcelVendors) {
			values.emplace_back(std::string(vendor) + "Min", "1");
			values.emplace_back(std::string(vendor) + "Max", "1");
		}
		for (const auto& [name, value] : values) {
			if (const int column = misc.Column(name); column >= 0) {
				misc.Set(row, column, value);
			}
		}
	}
	return static_cast<int>(std::size(ProphecyCodes));
}

void ScaleDensity(Table& levels, int densityPercent, int packsPercent) {
	auto scale = [&](const char* name, int percent, int low, int high) {
		const int column = levels.Column(name);
		for (size_t row = 0; column >= 0 && percent != 100 && row < levels.Rows().size(); ++row) {
			const std::string value = levels.Cell(row, column);
			if (!value.empty() && value != "0") {
				levels.Set(row, column, std::to_string(std::clamp(ParseInt(value) * percent / 100, low, high)));
			}
		}
	};
	for (const char* name : { "MonDen", "MonDen(N)", "MonDen(H)" }) {
		scale(name, densityPercent, 1, 9900);
	}
	for (const char* name : { "MonUMin", "MonUMax", "MonUMin(N)", "MonUMax(N)", "MonUMin(H)", "MonUMax(H)" }) {
		scale(name, packsPercent, 1, 255);
	}
}

void BoostDrops(Table& itemratio, int percent) {
	if (percent <= 100) {
		return;
	}
	for (const char* name : { "Unique", "UniqueMin", "Rare", "RareMin", "Set", "SetMin" }) {
		const int column = itemratio.Column(name);
		for (size_t row = 0; column >= 0 && row < itemratio.Rows().size(); ++row) {
			const std::string value = itemratio.Cell(row, column);
			if (!value.empty() && ParseInt(value) > 0) {
				itemratio.Set(row, column, std::to_string(std::max(1, ParseInt(value) * 100 / percent)));
			}
		}
	}
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
