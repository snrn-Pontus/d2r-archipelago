#pragma once

#include <cstdint>
#include <filesystem>
#include <map>
#include <set>

namespace ap {

// What the plugin remembers about one seed and slot between sessions.
struct SaveState {
	// Every check this client has found, sent or not.
	std::set<int64_t> checks;
	// How many received items have had their filler handed out.
	size_t fillersDelivered = 0;
	bool goalReached = false;
	// Highest character level seen, for scaling loot.
	int level = 0;
	// The player's class id as seen in game (0 Amazon ... 7 Warlock), or -1.
	int playerClass = -1;
	// Monster kills per difficulty * 10 + act, for the kill-count checks.
	std::map<int, int> kills;
	// Item seeds of the uniques and set items you found (not rewards), for the find-count checks.
	std::set<uint32_t> foundUniques;
	std::set<uint32_t> foundSets;
	// Hints found in game: indexes into the seed's hint list, and our own locations
	// hinted for other players. Scrolls of Prophecy bought (the next tier to buy).
	std::set<int> ownHints;
	std::set<int64_t> otherHints;
	int prophecies = 0;
	// Whether the character got its Archipelago Ledger (ap-ledger makes another).
	bool ledgerGiven = false;

	bool Load(const std::filesystem::path& path);
	bool Save(const std::filesystem::path& path) const;
};

} // namespace ap
