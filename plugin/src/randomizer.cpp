#include "randomizer.h"

#include "game_data.h"

#include <algorithm>
#include <climits>
#include <cstdio>
#include <cstdlib>
#include <functional>
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
	    || monsterDensity != 100 || championPacks != 100 || betterDrops != 100 || superUniqueChecks || !skillTree.empty() || treeMode;
}

// A short fingerprint of a skill tree deal, for the mod's stamp.
auto TreeStamp(const SkillTree& tree) -> std::string {
	uint32_t hash = 2166136261U;
	for (const auto& [skill, slot] : tree) {
		hash = (hash ^ skill) * 16777619U;
		hash = (hash ^ slot) * 16777619U;
	}
	char text[16];
	std::snprintf(text, sizeof(text), "%08x", hash);
	return text;
}

auto Settings::Stamp() const -> std::string {
	return "seed=" + std::to_string(seed) + " monsters=" + std::to_string(static_cast<int>(monsters)) + " superuniques="
	     + std::to_string(static_cast<int>(superUniques)) + " runewords=" + (runewords ? "1" : "0") + " xp=" + std::to_string(xpPercent)
	     + " levelreqs=" + (itemLevelRequirements ? "1" : "0") + " statreqs=" + (itemStatRequirements ? "1" : "0") + " density="
	     + std::to_string(monsterDensity) + " packs=" + std::to_string(championPacks) + " drops=" + std::to_string(betterDrops)
	     // The count, so a mod written for an older superunique list doesn't
	     // match: the plugin finds each copy by its distance from the end.
	     + (superUniqueChecks ? " superunique_checks=" + std::to_string(std::size(data::SuperUniques)) : "")
	     + (skillTree.empty() ? "" : " skill_tree=" + TreeStamp(skillTree)) + (treeMode ? " tree_mode=1" : "");
}

auto BlockClassSkills(Table& skills, bool keepLevels) -> int {
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
		if (!keepLevels) {
			skills.Set(row, reqlevel, "0");
		}
		skills.Set(row, reqskill[0], skills.Cell(row, name));
		skills.Set(row, reqskill[1], "");
		skills.Set(row, reqskill[2], "");
		++locked;
	}
	return locked;
}

// A class skill's tree position, read from skills.txt and skilldesc.txt.
struct Place {
	std::string cls;
	int         page = 0, row = 0, column = 0;
	size_t      skillRow = 0, descRow = 0;
	// The skill's name string key ("skillname36", "Skillname226"), for tooltips.
	std::string nameKey;
};

auto ClassSkillPlaces(const Table& skills, const Table& skilldesc) -> std::map<std::string, Place> {
	const int name = skills.Column("skill"), charclass = skills.Column("charclass"), descOf = skills.Column("skilldesc");
	const int descName = skilldesc.Column("skilldesc"), page = skilldesc.Column("SkillPage"), row = skilldesc.Column("SkillRow");
	const int column = skilldesc.Column("SkillColumn"), nameKey = skilldesc.Column("str name");
	std::map<std::string, size_t> descRows;
	for (size_t d = 0; d < skilldesc.Rows().size(); ++d) {
		descRows.emplace(skilldesc.Cell(d, descName), d);
	}
	std::map<std::string, Place> places;
	for (size_t s = 0; s < skills.Rows().size(); ++s) {
		const auto desc = descRows.find(skills.Cell(s, descOf));
		if (ClassCodes.contains(skills.Cell(s, charclass)) && desc != descRows.end()) {
			const size_t d = desc->second;
			places[skills.Cell(s, name)] = { skills.Cell(s, charclass), ParseInt(skilldesc.Cell(d, page)), ParseInt(skilldesc.Cell(d, row)),
				ParseInt(skilldesc.Cell(d, column)), s, d, skilldesc.Cell(d, nameKey) };
		}
	}
	return places;
}

// Calls `replace` with each name in a formula's skill('Name'.x) references and
// puts back what it returns.
auto RenameSkillReferences(const std::string& formula, const std::function<std::string(const std::string&)>& replace) -> std::string {
	static const std::string Open = "skill('";
	std::string out;
	size_t at = 0;
	for (size_t found = formula.find(Open); found != std::string::npos; found = formula.find(Open, at)) {
		const size_t start = found + Open.size();
		const size_t end = formula.find('\'', start);
		if (end == std::string::npos) {
			break;
		}
		out += formula.substr(at, start - at) + replace(formula.substr(start, end - start));
		at = end;
	}
	return out + formula.substr(at);
}

// After a tree shuffle, points every synergy (and other skill('Name') reference
// in a class skill's formulas and tooltip) at a skill in the skill's new tree:
// a partner already in that class stays; any other becomes the skill nearest
// its old place on the skill's new tab (row first, then column), each partner
// a different skill. Tooltip lines name skills by their name string keys.
void RemapSynergies(Table& skills, Table& skilldesc, const std::map<std::string, Place>& before) {
	const auto after = ClassSkillPlaces(skills, skilldesc);
	std::map<std::string, std::string> skillOfKey;
	for (const auto& [skill, place] : after) {
		if (!place.nameKey.empty()) {
			skillOfKey[place.nameKey] = skill;
		}
	}
	for (const auto& [skill, place] : after) {
		std::map<std::string, std::string> partners;
		std::set<std::string> taken { skill };
		auto partner = [&](const std::string& other) -> std::string {
			const auto old = before.find(other);
			const auto now = after.find(other);
			if (old == before.end() || now == after.end() || now->second.cls == place.cls) {
				return other;
			}
			if (const auto known = partners.find(other); known != partners.end()) {
				return known->second;
			}
			std::string best = other;
			int bestCost = INT_MAX;
			for (const auto& [candidate, there] : after) {
				if (there.cls != place.cls || taken.contains(candidate)) {
					continue;
				}
				const int cost = (there.page != place.page ? 100 : 0) + std::abs(there.row - old->second.row) * 3 + std::abs(there.column - old->second.column);
				if (cost < bestCost) {
					best = candidate;
					bestCost = cost;
				}
			}
			taken.insert(best);
			partners[other] = best;
			return best;
		};
		for (std::string& cell : skills.Rows()[place.skillRow]) {
			if (cell.find("skill('") != std::string::npos) {
				cell = RenameSkillReferences(cell, partner);
			}
		}
		const auto& header = skilldesc.Rows();
		for (size_t c = 0; c < header[place.descRow].size(); ++c) {
			std::string& cell = skilldesc.Rows()[place.descRow][c];
			if (cell.find("skill('") != std::string::npos) {
				cell = RenameSkillReferences(cell, partner);
			} else if (const auto named = skillOfKey.find(cell); named != skillOfKey.end() && c != static_cast<size_t>(skilldesc.Column("str name"))) {
				const std::string renamed = partner(named->second);
				if (const auto target = after.find(renamed); target != after.end() && !target->second.nameKey.empty()) {
					cell = target->second.nameKey;
				}
			}
		}
	}
}

auto ApplySkillTrees(Table& skills, Table& skilldesc, const SkillTree& tree) -> TreeShuffle {
	const int id = skills.Column("*Id");
	const int charclass = skills.Column("charclass");
	const int reqlevel = skills.Column("reqlevel");
	const int descOf = skills.Column("skilldesc");
	const int descName = skilldesc.Column("skilldesc");
	const int page = skilldesc.Column("SkillPage");
	const int rowColumn = skilldesc.Column("SkillRow");
	const int column = skilldesc.Column("SkillColumn");
	const int listRow = skilldesc.Column("ListRow");
	const int iconCel = skilldesc.Column("IconCel");
	if (id < 0 || charclass < 0 || descOf < 0 || descName < 0 || page < 0 || rowColumn < 0 || column < 0 || listRow < 0 || iconCel < 0
	    || tree.empty()) {
		return {};
	}
	std::map<std::string, size_t> descRows;
	for (size_t row = 0; row < skilldesc.Rows().size(); ++row) {
		descRows.emplace(skilldesc.Cell(row, descName), row);
	}
	// Every class skill's slot before the deal, by skill id: its class and level, and its place and icon.
	struct Slot {
		size_t      row = 0, desc = 0;
		std::string cls, reqlevel, page, treeRow, column, listRow, iconCel;
	};
	std::map<uint32_t, Slot> slots;
	for (size_t row = 0; row < skills.Rows().size(); ++row) {
		const auto desc = descRows.find(skills.Cell(row, descOf));
		if (ClassCodes.contains(skills.Cell(row, charclass)) && desc != descRows.end()) {
			const size_t d = desc->second;
			slots[static_cast<uint32_t>(ParseInt(skills.Cell(row, id)))] = { row, d, skills.Cell(row, charclass),
				reqlevel >= 0 ? skills.Cell(row, reqlevel) : "", skilldesc.Cell(d, page), skilldesc.Cell(d, rowColumn), skilldesc.Cell(d, column),
				skilldesc.Cell(d, listRow), skilldesc.Cell(d, iconCel) };
		}
	}
	const auto before = ClassSkillPlaces(skills, skilldesc);
	TreeShuffle result;
	for (const auto& [skill, owner] : tree) {
		const auto from = slots.find(skill);
		const auto to = slots.find(owner);
		if (from == slots.end() || to == slots.end()) {
			continue;
		}
		const Slot& mine = from->second;
		const Slot& slot = to->second;
		result.moved += mine.cls != slot.cls ? 1 : 0;
		result.icons.push_back({ mine.cls, ParseInt(mine.iconCel), slot.cls, ParseInt(slot.iconCel) });
		skills.Set(mine.row, charclass, slot.cls);
		if (reqlevel >= 0) {
			skills.Set(mine.row, reqlevel, slot.reqlevel);
		}
		skilldesc.Set(mine.desc, page, slot.page);
		skilldesc.Set(mine.desc, rowColumn, slot.treeRow);
		skilldesc.Set(mine.desc, column, slot.column);
		skilldesc.Set(mine.desc, listRow, slot.listRow);
		skilldesc.Set(mine.desc, iconCel, slot.iconCel);
	}
	RemapSynergies(skills, skilldesc, before);
	return result;
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

auto AddSuperUniqueMonsters(Table& monstats, Table& superUniques, std::vector<std::pair<std::string, std::string>>* copies) -> int {
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
		if (copies != nullptr) {
			copies->emplace_back(name, original);
		}
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
			{ "name", ParcelName }, { "code", ParcelCodes[act] }, { "type", ShopItemType }, { "namestr", ParcelName }, { "cost", std::to_string(ParcelCosts[act]) },
			{ "gamble cost", "0" }, { "level", "0" }, { "levelreq", "0" }, { "unique", "0" }, { "spawnable", "1" }, { "PermStoreItem", "1" },
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

auto AddShopItemType(Table& itemTypes) -> int {
	const int code = itemTypes.Column("Code");
	const int name = itemTypes.Column("ItemType");
	const int page = itemTypes.Column("StorePage");
	if (code < 0 || page < 0) {
		return 0;
	}
	std::optional<size_t> templateRow;
	for (size_t row = 0; row < itemTypes.Rows().size(); ++row) {
		if (itemTypes.Cell(row, code) == ShopItemType) {
			return 0;
		}
		if (itemTypes.Cell(row, code) == "ques") {
			templateRow = row;
		}
	}
	if (!templateRow) {
		return 0;
	}
	itemTypes.Rows().push_back(itemTypes.Rows()[*templateRow]);
	const size_t row = itemTypes.Rows().size() - 1;
	itemTypes.Set(row, code, ShopItemType);
	if (name >= 0) {
		itemTypes.Set(row, name, "Archipelago Shop Item");
	}
	itemTypes.Set(row, page, "misc");
	return 1;
}

auto PadMiscRows(Table& misc, const std::vector<std::string>& codes) -> int {
	const int code = misc.Column("code");
	const int spawnable = misc.Column("spawnable");
	if (code < 0) {
		return 0;
	}
	std::set<std::string> present;
	std::optional<size_t> templateRow;
	for (size_t row = 0; row < misc.Rows().size(); ++row) {
		present.insert(misc.Cell(row, code));
		if (misc.Cell(row, code) == "std") {
			templateRow = row;
		}
	}
	if (!templateRow) {
		return 0;
	}
	int added = 0;
	for (const std::string& missing : codes) {
		if (missing.empty() || present.contains(missing)) {
			continue;
		}
		// A copy of the Standard of Heroes sold nowhere, so it only holds the place.
		misc.Rows().push_back(misc.Rows()[*templateRow]);
		misc.Set(misc.Rows().size() - 1, code, missing);
		if (spawnable >= 0) {
			misc.Set(misc.Rows().size() - 1, spawnable, "0");
		}
		present.insert(missing);
		++added;
	}
	return added;
}

auto AddProphecyScrolls(Table& misc) -> int {
	const int code = misc.Column("code");
	if (code < 0) {
		return 0;
	}
	std::optional<size_t> templateRow, scrollRow;
	for (size_t row = 0; row < misc.Rows().size(); ++row) {
		if (misc.Cell(row, code) == ProphecyCodes[0]) {
			return 0;
		}
		if (misc.Cell(row, code) == "std") {
			templateRow = row;
		}
		if (misc.Cell(row, code) == "bks") {
			scrollRow = row;
		}
	}
	if (!templateRow) {
		return 0;
	}
	// The HD picture is the Scroll of Inifuss (setup.cpp), so take its size and
	// inventory picture too; a 1x1 item with a 2x2 picture spills over its cell.
	std::vector<std::pair<std::string, std::string>> looks;
	if (scrollRow) {
		for (const char* column : { "invwidth", "invheight", "invfile" }) {
			if (const int at = misc.Column(column); at >= 0) {
				looks.emplace_back(column, misc.Cell(*scrollRow, at));
			}
		}
	}
	for (size_t tier = 0; tier < std::size(ProphecyCodes); ++tier) {
		misc.Rows().push_back(misc.Rows()[*templateRow]);
		const size_t row = misc.Rows().size() - 1;
		std::vector<std::pair<std::string, std::string>> values = {
			{ "name", ProphecyName }, { "code", ProphecyCodes[tier] }, { "type", ShopItemType }, { "namestr", ProphecyName }, { "cost", std::to_string(ProphecyCosts[tier]) },
			{ "gamble cost", "0" }, { "level", "0" }, { "levelreq", "0" }, { "unique", "0" }, { "spawnable", "1" }, { "PermStoreItem", "1" },
			{ "multibuy", "0" },
		};
		for (const char* vendor : ParcelVendors) {
			values.emplace_back(std::string(vendor) + "Min", "1");
			values.emplace_back(std::string(vendor) + "Max", "1");
		}
		values.insert(values.end(), looks.begin(), looks.end());
		for (const auto& [name, value] : values) {
			if (const int column = misc.Column(name); column >= 0) {
				misc.Set(row, column, value);
			}
		}
	}
	return static_cast<int>(std::size(ProphecyCodes));
}

auto AddLedger(Table& misc) -> int {
	const int code = misc.Column("code");
	if (code < 0) {
		return 0;
	}
	std::optional<size_t> templateRow;
	for (size_t row = 0; row < misc.Rows().size(); ++row) {
		if (misc.Cell(row, code) == LedgerCode) {
			return 0;
		}
		if (misc.Cell(row, code) == "std") {
			templateRow = row;
		}
	}
	if (!templateRow) {
		return 0;
	}
	misc.Rows().push_back(misc.Rows()[*templateRow]);
	const size_t row = misc.Rows().size() - 1;
	const std::pair<std::string, std::string> values[] = {
		{ "name", LedgerName }, { "code", LedgerCode }, { "namestr", LedgerName }, { "cost", "0" }, { "gamble cost", "0" }, { "level", "0" },
		{ "levelreq", "0" }, { "unique", "0" }, { "spawnable", "0" }, { "PermStoreItem", "0" }, { "invfile", "invtoa" },
	};
	for (const auto& [name, value] : values) {
		if (const int column = misc.Column(name); column >= 0) {
			misc.Set(row, column, value);
		}
	}
	return 1;
}

auto AddRespecWrit(Table& misc) -> int {
	const int code = misc.Column("code");
	if (code < 0) {
		return 0;
	}
	std::optional<size_t> templateRow;
	for (size_t row = 0; row < misc.Rows().size(); ++row) {
		if (misc.Cell(row, code) == RespecCode) {
			return 0;
		}
		if (misc.Cell(row, code) == "toa") {
			templateRow = row;
		}
	}
	if (!templateRow) {
		return 0;
	}
	misc.Rows().push_back(misc.Rows()[*templateRow]);
	const size_t row = misc.Rows().size() - 1;
	std::vector<std::pair<std::string, std::string>> values = {
		{ "name", RespecName }, { "code", RespecCode }, { "type", ShopItemType }, { "namestr", RespecName }, { "cost", std::to_string(RespecCost) },
		{ "gamble cost", "0" }, { "level", "0" }, { "levelreq", "0" }, { "unique", "0" }, { "spawnable", "1" }, { "PermStoreItem", "1" },
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
	return 1;
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
