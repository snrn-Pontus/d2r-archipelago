// Writes the "Archipelago" D2R mod from the player's own game files: class
// skill trees locked (skills only come from the Codex charm), plus the seed's
// randomization (monsters, superuniques, runewords, experience, requirements).
//
//   d2r-ap-setup                                   use the server and slot saved by the plugin
//   d2r-ap-setup --connect <server> <slot> [--password <pw>]
//   d2r-ap-setup --offline                         skill lock only, no seed
//   d2r-ap-setup --extract <folder>                copy the raw tables for tools/gen_data.py
//   d2r-ap-setup --dump <game path> <file>         copy one game file, e.g. data\hd\items\items.json
//
// Options for all of them: --game <D2R folder>, --mod <name>, --out <mods folder>.

#include "ap_client.h"
#include "config.h"
#include "game_data.h"
#include "randomizer.h"
#include "table.h"

#include <CascLib.h>
#include <nlohmann/json.hpp>

#include <windows.h>

#include <chrono>
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <functional>
#include <set>
#include <sstream>
#include <string>
#include <thread>
#include <vector>

namespace {

namespace fs = std::filesystem;
using ap::Table;
namespace rnd = ap::randomizer;

// The tables tools/gen_data.py reads, from the RotW bank.
const char* const ExtractTables[] = { "skills.txt", "levels.txt", "uniqueitems.txt", "setitems.txt", "weapons.txt", "armor.txt", "misc.txt",
	"superuniques.txt", "runes.txt", "monlvl.txt", "monstats.txt", "magicprefix.txt", "magicsuffix.txt", "automagic.txt", "experience.txt" };

// RotW tables live in excel/, the Classic and LoD tables in excel/base/.
const char* const Banks[] = { "", "base\\" };

// Written next to the mod; the plugin compares it with the connected seed.
constexpr const char* StampFile = "archipelago.txt";

struct Options {
	std::string game;
	std::string mod = "Archipelago";
	std::string out;
	std::string extract;
	std::string dumpPath, dumpFile;
	std::string server, slot, password;
	bool offline = false;
};

auto FindGameFolder() -> std::string {
	char path[MAX_PATH] {};
	DWORD size = sizeof(path);
	if (RegGetValueA(HKEY_LOCAL_MACHINE, "SOFTWARE\\WOW6432Node\\Microsoft\\Windows\\CurrentVersion\\Uninstall\\Diablo II Resurrected", "InstallLocation",
		    RRF_RT_REG_SZ, nullptr, path, &size) == ERROR_SUCCESS) {
		return path;
	}
	return "C:\\Program Files (x86)\\Diablo II Resurrected";
}

auto ReadCascFile(HANDLE storage, const std::string& name, std::string& contents) -> bool {
	HANDLE file = nullptr;
	if (!CascOpenFile(storage, ("data:" + name).c_str(), 0, CASC_OPEN_BY_NAME, &file)) {
		return false;
	}
	contents.clear();
	char buffer[64 * 1024];
	DWORD read = 0;
	while (CascReadFile(file, buffer, sizeof(buffer), &read) && read > 0) {
		contents.append(buffer, read);
	}
	CascCloseFile(file);
	return !contents.empty();
}

auto ReadFile(const fs::path& path) -> std::string {
	std::ifstream file(path, std::ios::binary);
	std::stringstream text;
	text << file.rdbuf();
	return text.str();
}

auto ParseArgs(int argc, char** argv) -> Options {
	Options options;
	options.game = FindGameFolder();
	for (int i = 1; i < argc; ++i) {
		const std::string arg = argv[i];
		auto next = [&]() -> std::string { return i + 1 < argc ? argv[++i] : ""; };
		if (arg == "--game") {
			options.game = next();
		} else if (arg == "--mod") {
			options.mod = next();
		} else if (arg == "--out") {
			options.out = next();
		} else if (arg == "--extract") {
			options.extract = next();
		} else if (arg == "--dump") {
			options.dumpPath = next();
			options.dumpFile = next();
		} else if (arg == "--connect") {
			options.server = next();
			options.slot = next();
		} else if (arg == "--password") {
			options.password = next();
		} else if (arg == "--offline") {
			options.offline = true;
		}
	}
	return options;
}

int Extract(HANDLE storage, const Options& options) {
	fs::create_directories(options.extract);
	int copied = 0;
	for (const char* table : ExtractTables) {
		std::string contents;
		if (ReadCascFile(storage, std::string("data\\global\\excel\\") + table, contents)) {
			std::ofstream(fs::path(options.extract) / table, std::ios::binary | std::ios::trunc) << contents;
			++copied;
		} else {
			std::printf("  %s: not found\n", table);
		}
	}
	std::printf("Extracted %d tables to %s\n", copied, options.extract.c_str());
	return copied == static_cast<int>(std::size(ExtractTables)) ? 0 : 1;
}

struct Seed {
	std::string name, slot;
	rnd::Settings settings;
};

// Connects to the server just long enough to read the slot's settings.
auto FetchSeed(const Options& options, Seed& seed) -> bool {
	std::printf("Connecting to %s as %s...\n", options.server.c_str(), options.slot.c_str());
	ap::Client client([](const std::string&) {});
	client.Connect(options.server, options.slot, options.password);
	const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(20);
	while (client.State() != ap::ConnectionState::Connected && client.State() != ap::ConnectionState::Refused
		   && std::chrono::steady_clock::now() < deadline) {
		std::this_thread::sleep_for(std::chrono::milliseconds(100));
	}
	if (client.State() != ap::ConnectionState::Connected) {
		std::printf("%s\n", client.StatusText().c_str());
		return false;
	}
	seed = { client.Seed(), client.Slot(), client.Slots().randomizer };
	client.Disconnect();
	return true;
}

// Reads one table from a bank, edits it, and writes it to the mod if the edit
// changed anything.
auto PatchTable(HANDLE storage, const fs::path& excel, const std::string& bank, const std::string& name, const std::function<void(Table&)>& edit) -> bool {
	std::string contents;
	if (!ReadCascFile(storage, "data\\global\\excel\\" + bank + name, contents)) {
		return false;
	}
	Table table = Table::Parse(contents);
	edit(table);
	const std::string patched = table.Format();
	if (patched == contents) {
		return false;
	}
	const fs::path target = excel / (bank.empty() ? fs::path() : fs::path("base")) / name;
	fs::create_directories(target.parent_path());
	std::ofstream(target, std::ios::binary | std::ios::trunc) << patched;
	return true;
}

// Names the items this tool adds: D2R shows "An Evil Force" for a unique whose
// name has no string. Appends an entry per name, the same in every language,
// to the game's own item-names.json.
auto AddItemNames(HANDLE storage, const fs::path& mpq, const std::vector<std::string>& names) -> bool {
	constexpr const char* Path = "data\\local\\lng\\strings\\item-names.json";
	std::string text;
	if (!ReadCascFile(storage, Path, text)) {
		return false;
	}
	const bool bom = text.rfind("\xEF\xBB\xBF", 0) == 0;
	auto strings = nlohmann::json::parse(bom ? text.substr(3) : text, nullptr, false);
	if (!strings.is_array() || strings.empty()) {
		return false;
	}
	int maxId = 0;
	std::set<std::string> keys;
	for (const auto& entry : strings) {
		maxId = std::max(maxId, entry.value("id", 0));
		keys.insert(entry.value("Key", ""));
	}
	for (const std::string& name : names) {
		if (keys.contains(name)) {
			continue;
		}
		nlohmann::json added = { { "id", ++maxId }, { "Key", name } };
		for (const auto& [key, value] : strings.front().items()) {
			if (key != "id" && key != "Key" && value.is_string()) {
				added[key] = name;
			}
		}
		strings.push_back(added);
	}
	const fs::path target = mpq / "data" / "local" / "lng" / "strings" / "item-names.json";
	fs::create_directories(target.parent_path());
	std::ofstream(target, std::ios::binary | std::ios::trunc) << (bom ? "\xEF\xBB\xBF" : "") << strings.dump(2);
	return true;
}

// Gives the shop parcel an HD picture: items.json maps each item code to a
// sprite, and a code without one shows nothing. The parcel borrows the
// Standard of Heroes' flag, like its misc.txt row.
auto AddParcelGraphics(HANDLE storage, const fs::path& mpq) -> bool {
	constexpr const char* Path = "data\\hd\\items\\items.json";
	std::string text;
	if (!ReadCascFile(storage, Path, text)) {
		return false;
	}
	const bool bom = text.rfind("\xEF\xBB\xBF", 0) == 0;
	auto items = nlohmann::json::parse(bom ? text.substr(3) : text, nullptr, false);
	if (!items.is_array()) {
		return false;
	}
	// Parcels borrow the Standard of Heroes' flag, Scrolls of Prophecy the Scroll of Inifuss.
	nlohmann::json graphics, scroll;
	for (const auto& entry : items) {
		if (entry.contains(rnd::ParcelCodes[0])) {
			return true;
		}
		if (entry.contains("std")) {
			graphics = entry["std"];
		}
		if (entry.contains("bks")) {
			scroll = entry["bks"];
		}
	}
	if (graphics.is_null()) {
		return false;
	}
	for (const char* code : rnd::ParcelCodes) {
		items.push_back({ { code, graphics } });
	}
	for (const char* code : rnd::ProphecyCodes) {
		items.push_back({ { code, scroll.is_null() ? graphics : scroll } });
	}
	const fs::path target = mpq / "data" / "hd" / "items" / "items.json";
	fs::create_directories(target.parent_path());
	std::ofstream(target, std::ios::binary | std::ios::trunc) << (bom ? "\xEF\xBB\xBF" : "") << items.dump(2);
	return true;
}

// A generator per table, so each table's shuffle depends only on the seed and
// the table (FNV-1a of its name), never on which other tables were edited.
auto TableRandom(uint32_t seed, const std::string& table) -> std::mt19937 {
	uint32_t hash = 2166136261U;
	for (const char c : table) {
		hash = (hash ^ static_cast<uint8_t>(c)) * 16777619U;
	}
	return std::mt19937(seed ^ hash);
}

int WriteMod(HANDLE storage, const Options& options, const Seed* seed) {
	const fs::path modRoot = (options.out.empty() ? fs::path(options.game) / "mods" : fs::path(options.out)) / options.mod;
	const fs::path mpq = modRoot / (options.mod + ".mpq");
	const fs::path excel = mpq / "data" / "global" / "excel";
	// Clear the previous seed's tables, but only in a mod this tool made.
	if (fs::exists(excel) && !fs::exists(modRoot / StampFile)) {
		std::printf("%s exists but wasn't made by this tool; pick another name with --mod.\n", modRoot.string().c_str());
		return 1;
	}
	std::error_code error;
	fs::remove_all(excel, error);
	if (!AddItemNames(storage, mpq, { "Archipelago Codex", rnd::ParcelName, rnd::ProphecyName })) {
		std::printf("  couldn't add the Codex's name; it will show as \"An Evil Force\"\n");
	}
	if (!AddParcelGraphics(storage, mpq)) {
		std::printf("  couldn't give the shop parcel a picture\n");
	}

	const rnd::Settings settings = seed != nullptr ? seed->settings : rnd::Settings {};
	std::vector<std::string> recipes;
	for (const std::string bank : Banks) {
		const char* label = bank.empty() ? "RotW" : "Classic/LoD";
		int blocked = 0;
		PatchTable(storage, excel, bank, "skills.txt", [&](Table& t) { blocked = rnd::BlockClassSkills(t); });
		std::printf("  %s: locked %d class skills\n", label, blocked);

		if (settings.monsters != rnd::Shuffle::Off || settings.monsterDensity != 100 || settings.championPacks != 100) {
			std::string stats;
			ReadCascFile(storage, "data\\global\\excel\\" + bank + "monstats.txt", stats);
			const Table monstats = Table::Parse(stats);
			int changed = 0;
			auto random = TableRandom(settings.seed, "levels");
			PatchTable(storage, excel, bank, "levels.txt", [&](Table& t) {
				changed = rnd::ShuffleMonsters(t, monstats, settings.monsters, random);
				rnd::ScaleDensity(t, settings.monsterDensity, settings.championPacks);
			});
			std::printf("  %s: new monsters in %d areas, density %d%%, champion packs %d%%\n", label, changed, settings.monsterDensity, settings.championPacks);
		}
		if (settings.betterDrops > 100) {
			PatchTable(storage, excel, bank, "itemratio.txt", [&](Table& t) { rnd::BoostDrops(t, settings.betterDrops); });
			std::printf("  %s: uniques, sets and rares drop at %d%%\n", label, settings.betterDrops);
		}
		if (settings.superUniques != rnd::Shuffle::Off || settings.superUniqueChecks) {
			std::string stats;
			ReadCascFile(storage, "data\\global\\excel\\" + bank + "monstats.txt", stats);
			Table monstats = Table::Parse(stats);
			int changed = 0, linked = 0;
			auto random = TableRandom(settings.seed, "superuniques");
			PatchTable(storage, excel, bank, "superuniques.txt", [&](Table& t) {
				changed = rnd::ShuffleSuperUniques(t, monstats, settings.superUniques, random);
				if (settings.superUniqueChecks) {
					linked = rnd::AddSuperUniqueMonsters(monstats, t);
				}
			});
			if (settings.superUniqueChecks) {
				PatchTable(storage, excel, bank, "monstats.txt", [&](Table& t) { t = monstats; });
			}
			std::printf("  %s: %d superuniques changed shape, %d count as checks\n", label, changed, linked);
		}
		if (settings.runewords) {
			auto random = TableRandom(settings.seed, "runes");
			PatchTable(storage, excel, bank, "runes.txt", [&](Table& t) {
				auto list = rnd::ShuffleRunewords(t, random);
				if (bank.empty()) {
					recipes = std::move(list);
				}
			});
		}
		if (settings.xpPercent != 100) {
			PatchTable(storage, excel, bank, "monlvl.txt", [&](Table& t) { rnd::ScaleExperience(t, settings.xpPercent); });
			std::printf("  %s: monster experience at %d%%\n", label, settings.xpPercent);
		}
		const bool clearRequirements = !settings.itemLevelRequirements || !settings.itemStatRequirements;
		if (clearRequirements) {
			for (const char* table : { "weapons.txt", "armor.txt", "setitems.txt" }) {
				PatchTable(storage, excel, bank, table, [&](Table& t) { rnd::ClearRequirements(t, !settings.itemLevelRequirements, !settings.itemStatRequirements); });
			}
			std::printf("  %s: item requirements cleared\n", label);
		}
		// Misc gets the shop parcel, plus cleared requirements if asked.
		int parcels = 0;
		PatchTable(storage, excel, bank, "misc.txt", [&](Table& t) {
			if (clearRequirements) {
				rnd::ClearRequirements(t, !settings.itemLevelRequirements, !settings.itemStatRequirements);
			}
			parcels = rnd::AddShopParcels(t) + rnd::AddProphecyScrolls(t);
		});
		std::printf("  %s: %s\n", label, parcels > 0 ? "each act's potion seller sells an Archipelago Parcel" : "couldn't add the shop parcels");
		// UniqueItems gets the Codex charm, plus cleared requirements if asked.
		PatchTable(storage, excel, bank, "uniqueitems.txt", [&](Table& t) {
			if (clearRequirements) {
				rnd::ClearRequirements(t, !settings.itemLevelRequirements, !settings.itemStatRequirements);
			}
			rnd::AddCodexUnique(t, ap::data::CodexUnique);
		});
	}

	if (!recipes.empty()) {
		std::ofstream list(modRoot / "runewords.txt", std::ios::trunc);
		list << "Runeword recipes for this seed:\n\n";
		for (const auto& recipe : recipes) {
			list << recipe << "\n";
		}
		std::printf("  %zu runeword recipes changed; see %s\n", recipes.size(), (modRoot / "runewords.txt").string().c_str());
	} else {
		fs::remove(modRoot / "runewords.txt", error);
	}
	const fs::path modinfo = mpq / "modinfo.json";
	if (!fs::exists(modinfo)) {
		fs::create_directories(mpq);
		std::ofstream(modinfo) << "{\n\t\"name\": \"" << options.mod << "\",\n\t\"savepath\": \"" << options.mod << "/\"\n}\n";
	}
	std::ofstream(modRoot / StampFile, std::ios::trunc) << (seed != nullptr ? seed->name : "") << "\n"
	                                                  << (seed != nullptr ? seed->slot : "") << "\n"
	                                                  << settings.Stamp() << "\n";
	std::printf("Wrote the %s mod to %s\n", options.mod.c_str(), modRoot.string().c_str());
	return 0;
}

} // namespace

int main(int argc, char** argv) {
	Options options = ParseArgs(argc, argv);
	std::printf("Diablo II: Resurrected folder: %s\n", options.game.c_str());

	HANDLE storage = nullptr;
	if (!CascOpenStorage(options.game.c_str(), 0, &storage)) {
		std::printf("Couldn't open the game files (error %lu). Pass the D2R folder with --game.\n", GetLastError());
		return 1;
	}
	if (!options.extract.empty()) {
		const int result = Extract(storage, options);
		CascCloseStorage(storage);
		return result;
	}
	if (!options.dumpPath.empty()) {
		std::string contents;
		const bool found = ReadCascFile(storage, options.dumpPath, contents);
		CascCloseStorage(storage);
		if (!found) {
			std::printf("%s: not found\n", options.dumpPath.c_str());
			return 1;
		}
		std::ofstream(options.dumpFile, std::ios::binary | std::ios::trunc) << contents;
		std::printf("Wrote %zu bytes to %s\n", contents.size(), options.dumpFile.c_str());
		return 0;
	}

	// Without --connect, use what the plugin saved after ap-connect.
	if (!options.offline && options.server.empty()) {
		const auto config = ap::ParseConfig(ReadFile(fs::path(options.game) / "d2rloader" / "config" / "archipelago.toml"));
		options.server = config.server;
		options.slot = config.slot;
		options.password = config.password;
	}
	Seed seed;
	bool haveSeed = false;
	if (!options.offline && !options.server.empty()) {
		haveSeed = FetchSeed(options, seed);
		if (!haveSeed) {
			CascCloseStorage(storage);
			std::printf("Couldn't read the seed. Check the server and slot, or use --offline for the skill lock alone.\n");
			return 1;
		}
		std::printf("Seed %s, slot %s: %s\n", seed.name.c_str(), seed.slot.c_str(), seed.settings.Stamp().c_str());
	} else {
		std::printf("No server given, so only the skill lock is written. Use --connect <server> <slot> for a seed's randomization.\n");
	}
	const int result = WriteMod(storage, options, haveSeed ? &seed : nullptr);
	CascCloseStorage(storage);
	return result;
}
