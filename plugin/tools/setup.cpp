// Writes the "Archipelago" D2R mod that stops skill points being spent on the
// class skill trees. It reads skills.txt from the player's own game files and
// raises every class skill's level requirement past the level cap, so skills
// only come from the Codex charm.
//
//   d2r-ap-setup [--game <D2R folder>] [--mod <name>] [--out <mods folder>]

#include <CascLib.h>

#include <windows.h>

#include <algorithm>
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <set>
#include <sstream>
#include <string>
#include <vector>

namespace {

// Above the level cap of 99, so the trees can never be allocated.
constexpr const char* BlockedLevel = "100";

const std::set<std::string> ClassCodes = { "ama", "sor", "nec", "pal", "bar", "dru", "ass", "war" };

// RotW tables live in excel/, the Classic and LoD tables in excel/base/.
const char* const SkillTables[] = {
	"data\\global\\excel\\skills.txt",
	"data\\global\\excel\\base\\skills.txt",
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

auto Split(const std::string& line) -> std::vector<std::string> {
	std::vector<std::string> fields;
	size_t start = 0;
	for (size_t tab = line.find('\t'); tab != std::string::npos; tab = line.find('\t', start)) {
		fields.push_back(line.substr(start, tab - start));
		start = tab + 1;
	}
	fields.push_back(line.substr(start));
	return fields;
}

auto Join(const std::vector<std::string>& fields) -> std::string {
	std::string line;
	for (size_t i = 0; i < fields.size(); ++i) {
		line += (i == 0 ? "" : "\t") + fields[i];
	}
	return line;
}

// Returns the patched table, or an empty string if the columns are missing.
auto BlockClassSkills(const std::string& table, int& blocked) -> std::string {
	std::istringstream lines(table);
	std::string line, output;
	int charclass = -1, reqlevel = -1;
	bool header = true;
	while (std::getline(lines, line)) {
		const bool carriageReturn = !line.empty() && line.back() == '\r';
		if (carriageReturn) {
			line.pop_back();
		}
		auto fields = Split(line);
		if (header) {
			for (int i = 0; i < static_cast<int>(fields.size()); ++i) {
				charclass = fields[i] == "charclass" ? i : charclass;
				reqlevel = fields[i] == "reqlevel" ? i : reqlevel;
			}
			if (charclass < 0 || reqlevel < 0) {
				return {};
			}
			header = false;
		} else if (static_cast<int>(fields.size()) > std::max(charclass, reqlevel) && ClassCodes.contains(fields[charclass])) {
			fields[reqlevel] = BlockedLevel;
			++blocked;
		}
		output += Join(fields) + (carriageReturn ? "\r\n" : "\n");
	}
	return output;
}

} // namespace

int main(int argc, char** argv) {
	std::string game = FindGameFolder();
	std::string mod = "Archipelago";
	std::string out;
	for (int i = 1; i + 1 < argc; i += 2) {
		const std::string arg = argv[i];
		if (arg == "--game") {
			game = argv[i + 1];
		} else if (arg == "--mod") {
			mod = argv[i + 1];
		} else if (arg == "--out") {
			out = argv[i + 1];
		}
	}
	std::printf("Diablo II: Resurrected folder: %s\n", game.c_str());

	HANDLE storage = nullptr;
	if (!CascOpenStorage(game.c_str(), 0, &storage)) {
		std::printf("Couldn't open the game files (error %lu). Pass the D2R folder with --game.\n", GetLastError());
		return 1;
	}
	const std::filesystem::path modRoot = (out.empty() ? std::filesystem::path(game) / "mods" : std::filesystem::path(out)) / mod;
	const std::filesystem::path mpq = modRoot / (mod + ".mpq");
	int written = 0;
	for (const char* table : SkillTables) {
		std::string contents;
		if (!ReadCascFile(storage, table, contents)) {
			std::printf("  %s: not in this build, skipped\n", table);
			continue;
		}
		int blocked = 0;
		const std::string patched = BlockClassSkills(contents, blocked);
		if (patched.empty()) {
			std::printf("  %s: charclass or reqlevel column missing, skipped\n", table);
			continue;
		}
		std::string relative = table;
		std::replace(relative.begin(), relative.end(), '\\', '/');
		const std::filesystem::path target = mpq / relative;
		std::filesystem::create_directories(target.parent_path());
		std::ofstream(target, std::ios::binary | std::ios::trunc) << patched;
		std::printf("  %s: blocked %d class skills\n", table, blocked);
		++written;
	}
	CascCloseStorage(storage);
	if (written == 0) {
		std::printf("No skill tables were written.\n");
		return 1;
	}

	const std::filesystem::path modinfo = mpq / "modinfo.json";
	if (!std::filesystem::exists(modinfo)) {
		std::ofstream(modinfo) << "{\n\t\"name\": \"" << mod << "\",\n\t\"savepath\": \"" << mod << "/\"\n}\n";
	}
	std::printf("Wrote the %s mod to %s\n", mod.c_str(), modRoot.string().c_str());
	return 0;
}
