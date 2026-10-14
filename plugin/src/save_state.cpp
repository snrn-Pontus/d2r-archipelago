#include "save_state.h"

#include <algorithm>
#include <cstdlib>
#include <fstream>
#include <string>
#include <system_error>

namespace ap {

// One "key value" pair per line: "check <id>", "fillers <count>", "goal 1", "level <n>",
// "kills_<difficulty * 10 + act> <count>", "cows_<difficulty> <count>", "questbase_<difficulty> <quest mask>",
// "unique <seed>", "set <seed>",
// "rewardcopies <code> <count>",
// "command <item> <text>" (the text runs to the end of the line).
bool SaveState::Load(const std::filesystem::path& path) {
	std::ifstream file(path);
	if (!file) {
		return false;
	}
	std::string line;
	bool hasCheckpoint = false;
	while (std::getline(file, line)) {
		if (!line.empty() && line.back() == '\r') {
			line.pop_back();
		}
		const size_t space = line.find(' ');
		if (space == std::string::npos) {
			continue;
		}
		const std::string key = line.substr(0, space);
		const std::string rest = line.substr(space + 1);
		const long long value = std::strtoll(rest.c_str(), nullptr, 10);
		if (key == "check") {
			checks.insert(value);
		} else if (key == "fillers") {
			fillersDelivered = static_cast<size_t>(value);
		} else if (key == "goal") {
			goalReached = value != 0;
		} else if (key == "level") {
			level = static_cast<int>(value);
		} else if (key == "seedclass") {
			// Not "class": older plugins wrote whichever character was loaded there, mules too.
			playerClass = static_cast<int>(value);
		} else if (key.rfind("kills_", 0) == 0) {
			kills[std::atoi(key.c_str() + 6)] = static_cast<int>(value);
		} else if (key.rfind("cows_", 0) == 0) {
			cowKills[std::atoi(key.c_str() + 5)] = static_cast<int>(value);
		} else if (key.rfind("questbase_", 0) == 0) {
			questBaseline[std::atoi(key.c_str() + 10)] = static_cast<uint64_t>(value);
		} else if (key == "unique") {
			foundUniques.insert(static_cast<uint32_t>(value));
		} else if (key == "set") {
			foundSets.insert(static_cast<uint32_t>(value));
		} else if (key == "rewardcopies") {
			if (const size_t gap = rest.find(' '); gap != std::string::npos) {
				rewardCopies[static_cast<uint32_t>(value)] = std::atoi(rest.c_str() + gap + 1);
			}
		} else if (key == "hint") {
			ownHints.insert(static_cast<int>(value));
		} else if (key == "otherhint") {
			otherHints.insert(value);
		} else if (key == "prophecies") {
			prophecies = static_cast<int>(value);
		} else if (key == "unlocked") {
			unlockedSkills.insert(static_cast<uint32_t>(value));
		} else if (key == "skillpoints") {
			skillPointsGiven = static_cast<int>(value);
		} else if (key == "notified") {
			notifiedItems = static_cast<size_t>(value);
		} else if (key == "portaltome") {
			portalTomeGiven = value != 0;
		} else if (key == "heldpotion") {
			heldPotions.push_back(static_cast<uint32_t>(value));
		} else if (key == "command" || key == "ran" || key == "lostcommand") {
			if (const size_t gap = rest.find(' '); gap != std::string::npos && gap + 1 < rest.size()) {
				(key == "command" ? commands : key == "ran" ? ran : lostCommands).push_back({ value, rest.substr(gap + 1) });
			}
		} else if (key == "redelivermark") {
			redeliverMark = static_cast<size_t>(value);
		} else if (key == "checkpoint") {
			checkpoint = static_cast<size_t>(value);
			hasCheckpoint = true;
		} else if (key == "lostfrom") {
			lostFrom = static_cast<size_t>(value);
		} else if (key == "lostto") {
			lostTo = static_cast<size_t>(value);
		} else if (key == "redeliver") {
			redeliver = value != 0;
		}
	}
	// A save from before checkpoints: what it delivered counts as kept, not lost.
	if (!hasCheckpoint) {
		checkpoint = fillersDelivered;
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
		file << "fillers " << fillersDelivered << "\ngoal " << (goalReached ? 1 : 0) << "\nlevel " << level << "\nseedclass " << playerClass << "\n";
		for (const int64_t check : checks) {
			file << "check " << check << "\n";
		}
		for (const auto& [key, count] : kills) {
			file << "kills_" << key << " " << count << "\n";
		}
		for (const auto& [difficulty, count] : cowKills) {
			file << "cows_" << difficulty << " " << count << "\n";
		}
		for (const auto& [difficulty, quests] : questBaseline) {
			file << "questbase_" << difficulty << " " << quests << "\n";
		}
		for (const uint32_t seed : foundUniques) {
			file << "unique " << seed << "\n";
		}
		for (const uint32_t seed : foundSets) {
			file << "set " << seed << "\n";
		}
		for (const auto& [code, count] : rewardCopies) {
			file << "rewardcopies " << code << " " << count << "\n";
		}
		for (const int hint : ownHints) {
			file << "hint " << hint << "\n";
		}
		for (const int64_t location : otherHints) {
			file << "otherhint " << location << "\n";
		}
		file << "prophecies " << prophecies << "\nskillpoints " << skillPointsGiven << "\nportaltome " << (portalTomeGiven ? 1 : 0) << "\nnotified " << notifiedItems << "\n";
		for (const uint32_t skill : unlockedSkills) {
			file << "unlocked " << skill << "\n";
		}
		for (const uint32_t code : heldPotions) {
			file << "heldpotion " << code << "\n";
		}
		auto writeCommands = [&](const char* key, const std::vector<PendingCommand>& list) {
			for (const auto& command : list) {
				// One line each: a line break would split it.
				if (!command.text.empty() && command.text.find_first_of("\r\n") == std::string::npos) {
					file << key << " " << command.item << " " << command.text << "\n";
				}
			}
		};
		writeCommands("command", commands);
		writeCommands("ran", ran);
		writeCommands("lostcommand", lostCommands);
		file << "checkpoint " << checkpoint << "\nlostfrom " << lostFrom << "\nlostto " << lostTo << "\nredeliver " << (redeliver ? 1 : 0)
		     << "\nredelivermark " << redeliverMark << "\n";
		if (!file.flush()) {
			return false;
		}
	}
	std::filesystem::rename(temporary, path, error);
	return !error;
}

bool SaveState::InLostRange(const PendingCommand& command) const {
	return command.item >= 0 && static_cast<size_t>(command.item) >= lostFrom && static_cast<size_t>(command.item) < lostTo;
}

void SaveState::RequeueLostCommands() {
	// Commands of the items handed out again are made again; the rest (tree skills, refunds,
	// earlier items' gold) run once more as they were.
	std::erase_if(commands, [&](const PendingCommand& command) { return InLostRange(command); });
	for (auto& command : lostCommands) {
		if (!InLostRange(command)) {
			commands.push_back(std::move(command));
		}
	}
	lostCommands.clear();
}

bool SaveState::DetectUncleanExit() {
	const bool delivered = checkpoint < fillersDelivered;
	const bool redelivered = redeliver && lostFrom > redeliverMark;
	if (!delivered && ran.empty() && !redelivered) {
		checkpoint = fillersDelivered;
		return false;
	}
	// A redelivery under way starts again from where the character was last saved.
	if (redeliver) {
		lostFrom = redeliverMark;
	}
	if (delivered) {
		const bool open = lostFrom < lostTo;
		if (!open) {
			lostFrom = checkpoint;
			lostTo = fillersDelivered;
		} else if (lostTo == checkpoint) {
			// Right after the earlier loss (no game was left in between): one range.
			lostTo = fillersDelivered;
		} else if (!redeliver) {
			// An older loss nobody asked for again gives way to this one.
			lostFrom = checkpoint;
			lostTo = fillersDelivered;
			lostCommands.clear();
		}
		// Otherwise a redelivery under way keeps its range; this loss can't be held next to it.
	}
	lostCommands.insert(lostCommands.end(), std::make_move_iterator(ran.begin()), std::make_move_iterator(ran.end()));
	ran.clear();
	if (redeliver) {
		RequeueLostCommands();
	}
	checkpoint = fillersDelivered;
	redeliverMark = lostFrom;
	return true;
}

void SaveState::Checkpoint() {
	checkpoint = fillersDelivered;
	ran.clear();
	// A finished redelivery is kept until now, so a crash before this rewinds it too.
	if (redeliver && lostFrom >= lostTo) {
		redeliver = false;
		lostFrom = lostTo = 0;
	}
	redeliverMark = lostFrom;
}

bool SaveState::StartRedelivery() {
	if (redeliver || (lostFrom >= lostTo && lostCommands.empty())) {
		return false;
	}
	RequeueLostCommands();
	redeliver = lostFrom < lostTo;
	redeliverMark = lostFrom;
	return true;
}

void SaveState::FinishCommands(const std::vector<PendingCommand>& done) {
	for (const auto& command : done) {
		if (const auto found = std::ranges::find(commands, command); found != commands.end()) {
			commands.erase(found);
			ran.push_back(command);
		}
	}
}

} // namespace ap
