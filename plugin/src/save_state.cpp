#include "save_state.h"

#include <cstdlib>
#include <fstream>
#include <string>
#include <system_error>

namespace ap {

// One "key value" pair per line: "check <id>", "fillers <count>", "goal 1", "level <n>",
// "kills_<difficulty * 10 + act> <count>", "unique <seed>", "set <seed>".
bool SaveState::Load(const std::filesystem::path& path) {
	std::ifstream file(path);
	if (!file) {
		return false;
	}
	std::string key;
	long long value = 0;
	while (file >> key >> value) {
		if (key == "check") {
			checks.insert(value);
		} else if (key == "fillers") {
			fillersDelivered = static_cast<size_t>(value);
		} else if (key == "goal") {
			goalReached = value != 0;
		} else if (key == "level") {
			level = static_cast<int>(value);
		} else if (key == "class") {
			playerClass = static_cast<int>(value);
		} else if (key.rfind("kills_", 0) == 0) {
			kills[std::atoi(key.c_str() + 6)] = static_cast<int>(value);
		} else if (key == "unique") {
			foundUniques.insert(static_cast<uint32_t>(value));
		} else if (key == "set") {
			foundSets.insert(static_cast<uint32_t>(value));
		} else if (key == "hint") {
			ownHints.insert(static_cast<int>(value));
		} else if (key == "otherhint") {
			otherHints.insert(value);
		} else if (key == "prophecies") {
			prophecies = static_cast<int>(value);
		}
	}
	return true;
}

bool SaveState::Save(const std::filesystem::path& path) const {
	std::error_code error;
	std::filesystem::create_directories(path.parent_path(), error);
	// Write a temporary file and swap it in, so a crash can't leave half a save.
	auto temporary = path;
	temporary += L".tmp";
	{
		std::ofstream file(temporary, std::ios::trunc);
		if (!file) {
			return false;
		}
		file << "fillers " << fillersDelivered << "\ngoal " << (goalReached ? 1 : 0) << "\nlevel " << level << "\nclass " << playerClass << "\n";
		for (const int64_t check : checks) {
			file << "check " << check << "\n";
		}
		for (const auto& [key, count] : kills) {
			file << "kills_" << key << " " << count << "\n";
		}
		for (const uint32_t seed : foundUniques) {
			file << "unique " << seed << "\n";
		}
		for (const uint32_t seed : foundSets) {
			file << "set " << seed << "\n";
		}
		for (const int hint : ownHints) {
			file << "hint " << hint << "\n";
		}
		for (const int64_t location : otherHints) {
			file << "otherhint " << location << "\n";
		}
		file << "prophecies " << prophecies << "\n";
		if (!file.flush()) {
			return false;
		}
	}
	std::filesystem::rename(temporary, path, error);
	return !error;
}

} // namespace ap
