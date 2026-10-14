#pragma once

// Kill-count and Secret Cow Level count checks from the saved counters.
//
// A count can be saved past its threshold without the check (the game stopped
// between the kill and the check reaching the save), so these look at every
// threshold a counter has reached, not just the one it equals, and the plugin
// runs them when a seed binds as well as after each kill.
//
// Cows add up per difficulty over the whole seed: every game and every
// connection counts toward the same count. Cows killed while no seed is bound
// are held and join the next seed the game binds to.
//
// The thresholds are the seed's (slot data: kill_counts, cow_kills and the
// find counts), each count with its own location id (ids.h); a count with no
// id of its own makes no check.

#include "game_data.h"
#include "ids.h"
#include "save_state.h"

#include <algorithm>
#include <cstdint>
#include <span>
#include <vector>

namespace ap::milestones {

inline constexpr int Difficulties = 3;
inline constexpr int KillActs     = 5;

// The level checks a character at `level` has reached: every `step` levels
// from step (but not level 1) up to the seed's level cap.
inline auto LevelChecks(int level, int step, int cap) -> std::vector<int64_t> {
	std::vector<int64_t> checks;
	if (step <= 0) {
		return checks;
	}
	for (int milestone = std::max(step, ids::FirstLevelCheck); milestone <= std::min(level, cap); milestone += step) {
		checks.push_back(ids::LevelLocation(static_cast<uint32_t>(milestone)));
	}
	return checks;
}

// The kill-count checks `count` kills in an act on a difficulty have reached,
// for the seed's thresholds `counts`.
inline auto KillChecks(int difficulty, int act, int count, std::span<const uint32_t> counts) -> std::vector<int64_t> {
	std::vector<int64_t> checks;
	if (difficulty < 0 || difficulty >= Difficulties || act < 0 || act >= KillActs) {
		return checks;
	}
	for (const uint32_t threshold : counts) {
		if (const int index = ids::CountIndex(ids::KillCounts, threshold); index >= 0 && count >= static_cast<int>(threshold)) {
			checks.push_back(ids::KillLocation(static_cast<uint32_t>(difficulty), static_cast<uint32_t>(act), static_cast<uint32_t>(index)));
		}
	}
	return checks;
}

// The find-count checks `uniques` unique items and `sets` set items found have
// reached, for the seed's thresholds.
inline auto FindChecks(size_t uniques, size_t sets, std::span<const uint32_t> uniqueCounts, std::span<const uint32_t> setCounts)
    -> std::vector<int64_t> {
	std::vector<int64_t> checks;
	for (const uint32_t threshold : uniqueCounts) {
		if (const int index = ids::CountIndex(ids::UniqueFinds, threshold); index >= 0 && uniques >= threshold) {
			checks.push_back(ids::UniqueFindLocation(static_cast<uint32_t>(index)));
		}
	}
	for (const uint32_t threshold : setCounts) {
		if (const int index = ids::CountIndex(ids::SetFinds, threshold); index >= 0 && sets >= threshold) {
			checks.push_back(ids::SetFindLocation(static_cast<uint32_t>(index)));
		}
	}
	return checks;
}

// The cow-count check `count` cows on a difficulty have reached, when the seed's
// check takes `cows`. Whatever counts as `count` (a seed's total or one game's)
// is up to the caller.
inline auto CowChecks(int difficulty, int count, uint32_t cows) -> std::vector<int64_t> {
	const int check = ids::CowKillCheck(cows);
	if (difficulty < 0 || difficulty >= Difficulties || check < 0 || count < static_cast<int>(cows)) {
		return {};
	}
	return { ids::CowLocation(static_cast<uint32_t>(difficulty), static_cast<uint32_t>(check)) };
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

// Counts a cow killed on a difficulty in an area into a bound seed's save,
// whose cow check takes `cows`.
inline auto CountCow(SaveState& save, int difficulty, int levelId, bool cowsShuffled, uint32_t cows) -> CowKill {
	CowKill kill;
	if (difficulty < 0 || difficulty >= Difficulties || !CowCounts(levelId, cowsShuffled)) {
		return kill;
	}
	kill.counted = true;
	for (const int64_t location : CowChecks(difficulty, ++save.cowKills[difficulty], cows)) {
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
// (which include the server's), for the seed's kill-count thresholds (none
// without kill-count checks) and the cows its cow check takes (0 without one).
inline auto Missing(const SaveState& save, std::span<const uint32_t> killCounts, uint32_t cows) -> std::vector<int64_t> {
	std::vector<int64_t> missing;
	auto add = [&](const std::vector<int64_t>& reached) {
		for (const int64_t location : reached) {
			if (!save.checks.contains(location)) {
				missing.push_back(location);
			}
		}
	};
	for (const auto& [key, count] : save.kills) {
		if (key >= 0) {
			add(KillChecks(key / 10, key % 10, count, killCounts));
		}
	}
	for (const auto& [difficulty, count] : save.cowKills) {
		add(CowChecks(difficulty, count, cows));
	}
	return missing;
}

// For the debug tools: the count one short of the lowest of the seed's
// kill-count thresholds an act on a difficulty hasn't got the check for in the
// save, so the next kill sends it; -1 if it has them all.
inline auto KillsBeforeNext(const SaveState& save, int difficulty, int act, std::span<const uint32_t> counts) -> int {
	if (difficulty < 0 || difficulty >= Difficulties || act < 0 || act >= KillActs) {
		return -1;
	}
	int next = -1;
	for (const uint32_t threshold : counts) {
		const int index = ids::CountIndex(ids::KillCounts, threshold);
		if (index >= 0 && !save.checks.contains(ids::KillLocation(static_cast<uint32_t>(difficulty), static_cast<uint32_t>(act), static_cast<uint32_t>(index)))
		    && (next < 0 || static_cast<int>(threshold) - 1 < next)) {
			next = static_cast<int>(threshold) - 1;
		}
	}
	return next;
}

// The same for the cow check on a difficulty, which takes `cows`.
inline auto CowsBeforeNext(const SaveState& save, int difficulty, uint32_t cows) -> int {
	const int check = ids::CowKillCheck(cows);
	if (difficulty < 0 || difficulty >= Difficulties || check < 0
	    || save.checks.contains(ids::CowLocation(static_cast<uint32_t>(difficulty), static_cast<uint32_t>(check)))) {
		return -1;
	}
	return static_cast<int>(cows) - 1;
}

} // namespace ap::milestones
