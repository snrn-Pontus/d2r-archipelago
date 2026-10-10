#include "save_state.h"

#include <fstream>
#include <string>
#include <system_error>

namespace ap {

// One "key value" pair per line: "check <id>", "fillers <count>", "goal 1", "level <n>".
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
		if (!file.flush()) {
			return false;
		}
	}
	std::filesystem::rename(temporary, path, error);
	return !error;
}

} // namespace ap
