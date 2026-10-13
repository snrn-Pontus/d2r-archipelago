#pragma once

// Kill-count and Secret Cow Level count checks from the saved counters.
//
// A count can be saved past its threshold without the check (the game stopped
// between the kill and the check reaching the save), so these look at every
// threshold a counter has reached, not just the one it equals, and the plugin
// runs them when a seed binds as well as after each kill.

#include "ids.h"
#include "save_state.h"

#include <cstdint>
#include <iterator>
#include <vector>

namespace ap::milestones {

inline constexpr int Difficulties = 3;
inline constexpr int KillActs     = 5;

// The kill-count checks `count` kills in an act on a difficulty have reached.
inline auto KillChecks(int difficulty, int act, int count) -> std::vector<int64_t> {
	std::vector<int64_t> checks;
	if (difficulty < 0 || difficulty >= Difficulties || act < 0 || act >= KillActs) {
		return checks;
	}
	for (uint32_t index = 0; index < std::size(ids::KillCounts); ++index) {
		if (count >= static_cast<int>(ids::KillCounts[index])) {
			checks.push_back(ids::KillLocation(static_cast<uint32_t>(difficulty), static_cast<uint32_t>(act), index));
		}
	}
	return checks;
}

// The cow-count check `count` cows on a difficulty have reached. Whatever
// counts as `count` (a seed's total or one game's) is up to the caller.
inline auto CowChecks(int difficulty, int count) -> std::vector<int64_t> {
	if (difficulty < 0 || difficulty >= Difficulties || count < static_cast<int>(ids::CowKills)) {
		return {};
	}
	return { ids::CowLocation(static_cast<uint32_t>(difficulty), 2) };
}

// The checks the save's counters have reached that aren't among its checks
// (which include the server's), for the categories the seed has.
inline auto Missing(const SaveState& save, bool killChecks, bool cowChecks) -> std::vector<int64_t> {
	std::vector<int64_t> missing;
	auto add = [&](const std::vector<int64_t>& reached) {
		for (const int64_t location : reached) {
			if (!save.checks.contains(location)) {
				missing.push_back(location);
			}
		}
	};
	if (killChecks) {
		for (const auto& [key, count] : save.kills) {
			if (key >= 0) {
				add(KillChecks(key / 10, key % 10, count));
			}
		}
	}
	if (cowChecks) {
		for (const auto& [difficulty, count] : save.cowKills) {
			add(CowChecks(difficulty, count));
		}
	}
	return missing;
}

} // namespace ap::milestones
