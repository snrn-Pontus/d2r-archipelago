#pragma once

// Kill-count and Secret Cow Level count checks from the saved counters.
//
// A count can be saved past its threshold without the check (the game stopped
// between the kill and the check reaching the save), so these look at every
// threshold a counter has reached, not just the one it equals, and the plugin
// runs them when a seed binds as well as after each kill.
//
// Cows add up per difficulty over the whole seed: every game and every
// connection counts toward the same 100. Cows killed while no seed is bound
// are held and join the next seed the game binds to.

#include "game_data.h"
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

// Whether a cow killed in `levelId` counts toward the 100: only in the Secret
// Cow Level, unless the seed shuffled the cow level's line-up into another
// area, whose cows count wherever they went. Ambush traps spawn the act's or
// the current area's monsters, so they only bring cows where cows live anyway.
inline auto CowCounts(int levelId, bool cowsShuffled) -> bool {
	return cowsShuffled || levelId == static_cast<int>(data::CowLevel);
}

struct CowKill {
	// Whether the cow counted (see CowCounts).
	bool counted = false;
	// The cow check it reached that isn't among the save's checks yet.
	std::vector<int64_t> reached;
};

// Counts a cow killed on a difficulty in an area into a bound seed's save.
inline auto CountCow(SaveState& save, int difficulty, int levelId, bool cowsShuffled) -> CowKill {
	CowKill kill;
	if (difficulty < 0 || difficulty >= Difficulties || !CowCounts(levelId, cowsShuffled)) {
		return kill;
	}
	kill.counted = true;
	for (const int64_t location : CowChecks(difficulty, ++save.cowKills[difficulty])) {
		if (!save.checks.contains(location)) {
			kill.reached.push_back(location);
		}
	}
	return kill;
}

// Cows killed while no seed is bound (before connecting, or after ap-connect
// to another slot), per difficulty. Whether one counts depends on the seed,
// so those outside the Cow Level are kept apart until a seed binds.
struct HeldCows {
	int inCowLevel[Difficulties] = {};
	int elsewhere[Difficulties]  = {};

	void Kill(int difficulty, int levelId) {
		if (difficulty >= 0 && difficulty < Difficulties) {
			++(levelId == static_cast<int>(data::CowLevel) ? inCowLevel : elsewhere)[difficulty];
		}
	}

	// Adds the held cows that count for a seed to its save, and forgets them all.
	// Missing() then finds a check they completed.
	void MoveTo(SaveState& save, bool cowsShuffled) {
		for (int difficulty = 0; difficulty < Difficulties; ++difficulty) {
			if (const int count = inCowLevel[difficulty] + (cowsShuffled ? elsewhere[difficulty] : 0); count > 0) {
				save.cowKills[difficulty] += count;
			}
		}
		*this = {};
	}
};

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

// For the debug tools: the count one short of the first kill-count check an act
// on a difficulty hasn't got in the save, so the next kill sends it; -1 if it
// has them all.
inline auto KillsBeforeNext(const SaveState& save, int difficulty, int act) -> int {
	if (difficulty < 0 || difficulty >= Difficulties || act < 0 || act >= KillActs) {
		return -1;
	}
	for (uint32_t index = 0; index < std::size(ids::KillCounts); ++index) {
		if (!save.checks.contains(ids::KillLocation(static_cast<uint32_t>(difficulty), static_cast<uint32_t>(act), index))) {
			return static_cast<int>(ids::KillCounts[index]) - 1;
		}
	}
	return -1;
}

// The same for the cow check on a difficulty.
inline auto CowsBeforeNext(const SaveState& save, int difficulty) -> int {
	if (difficulty < 0 || difficulty >= Difficulties || save.checks.contains(ids::CowLocation(static_cast<uint32_t>(difficulty), 2))) {
		return -1;
	}
	return static_cast<int>(ids::CowKills) - 1;
}

} // namespace ap::milestones
