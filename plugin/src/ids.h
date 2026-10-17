#pragma once

// Must match apworld/d2r_archipelago/ids.py.

#include <cstddef>
#include <cstdint>

namespace ap::ids {

inline constexpr int64_t ItemBase     = 2'430'000;
inline constexpr int64_t LocationBase = 2'420'000;
// Skills are ItemBase + skill id (all below 900).
inline constexpr int64_t EmpowermentItem = ItemBase + 900;
// Tree mode: one unspent skill point.
inline constexpr int64_t SkillPointItem  = ItemBase + 901;
// Act keys: KeyBase + act index (1 = Normal Act II ... 14 = Hell Act V).
inline constexpr int64_t KeyBase         = ItemBase + 950;
// Locks: LockBase + Lock (Waypoints + act for each act's waypoints).
inline constexpr int64_t LockBase        = ItemBase + 980;
inline constexpr int64_t LootBase        = ItemBase + 1000;

enum class Lock : uint32_t { Cube, TownPortal, Potions, Stash, Merchants, Waypoints };

inline constexpr auto QuestLocation(uint32_t difficulty, uint32_t questFlag) noexcept -> int64_t {
	return LocationBase + difficulty * 100 + questFlag;
}

// Area entries per difficulty; level ids are all below 200.
inline constexpr auto AreaLocation(uint32_t difficulty, uint32_t levelId) noexcept -> int64_t {
	return LocationBase + 1000 + difficulty * 200 + levelId;
}

// Every level from FirstLevelCheck to 90 has one; the seed's level_step picks which are checks.
inline constexpr int FirstLevelCheck = 2;

inline constexpr auto LevelLocation(uint32_t level) noexcept -> int64_t {
	return LocationBase + 2000 + level;
}

// Field superunique kills: data::SuperUniques ids (all below 100) per difficulty.
inline constexpr auto SuperUniqueLocation(uint32_t difficulty, uint32_t superUnique) noexcept -> int64_t {
	return LocationBase + 3000 + difficulty * 100 + superUnique;
}

// Collections, once per seed.
inline constexpr auto RuneLocation(uint32_t rune) noexcept -> int64_t {
	return LocationBase + 5000 + rune;
}

inline constexpr auto GemLocation(uint32_t index) noexcept -> int64_t {
	return LocationBase + 5100 + index;
}

inline constexpr auto SetLocation(uint32_t index) noexcept -> int64_t {
	return LocationBase + 5200 + index;
}

inline constexpr auto SpecialLocation(uint32_t index) noexcept -> int64_t {
	return LocationBase + 5300 + index;
}

// Count thresholds. Each count is a location of its own, its id from its index
// here, so a location means the same count in every seed: the seed's options
// (slot data) only pick which of them it has. Append only: the index is the id.
// Cows per difficulty: CowLocation check 2 + index.
inline constexpr uint32_t CowKills[]    = { 100, 50, 250, 500 };
// Kills per act (KillLocation), uniques and set items found (UniqueFindLocation, SetFindLocation).
inline constexpr uint32_t KillCounts[]  = { 100, 250, 500, 50, 125, 200, 1000 };
inline constexpr uint32_t UniqueFinds[] = { 1, 5, 10, 25, 50, 2, 12, 20, 100 };
inline constexpr uint32_t SetFinds[]    = { 1, 5, 10, 25, 2, 12, 20, 50 };

// The index of a count among a threshold list (its location's index), or -1 if it has none.
template <size_t N>
inline constexpr auto CountIndex(const uint32_t (&counts)[N], uint32_t count) noexcept -> int {
	for (size_t index = 0; index < N; ++index) {
		if (counts[index] == count) {
			return static_cast<int>(index);
		}
	}
	return -1;
}

// Secret Cow Level, per difficulty: 0 enter, 1 The Cow King, 2.. CowKills cows.
inline constexpr auto CowLocation(uint32_t difficulty, uint32_t check) noexcept -> int64_t {
	return LocationBase + 5400 + difficulty * 10 + check;
}

// The cow check for killing `count` cows, or -1 if no check takes that many.
inline constexpr auto CowKillCheck(uint32_t count) noexcept -> int {
	const int index = CountIndex(CowKills, count);
	return index < 0 ? -1 : 2 + index;
}

inline constexpr uint32_t MaxShopChecks = 5;

// Parcels bought at each act's shop, once per seed.
inline constexpr auto ShopLocation(uint32_t act, uint32_t number) noexcept -> int64_t {
	return LocationBase + 5500 + act * 10 + number;
}

inline constexpr auto UberLocation(uint32_t index) noexcept -> int64_t {
	return LocationBase + 5600 + index;
}

inline constexpr auto UniqueFindLocation(uint32_t index) noexcept -> int64_t {
	return LocationBase + 5800 + index;
}

inline constexpr auto SetFindLocation(uint32_t index) noexcept -> int64_t {
	return LocationBase + 5810 + index;
}

// data::QuestItems per difficulty (the Horadric Cube only on Normal).
inline constexpr auto QuestItemLocation(uint32_t difficulty, uint32_t index) noexcept -> int64_t {
	return LocationBase + 5900 + difficulty * 20 + index;
}

// KillCounts[index] monsters killed in an act on a difficulty.
inline constexpr auto KillLocation(uint32_t difficulty, uint32_t act, uint32_t index) noexcept -> int64_t {
	return LocationBase + 6000 + difficulty * 100 + act * 10 + index;
}

inline constexpr auto IsLockItem(int64_t item) noexcept -> bool {
	return item >= LockBase && item < LockBase + 10;
}

inline constexpr auto IsSkillItem(int64_t item) noexcept -> bool {
	return item >= ItemBase && item < EmpowermentItem;
}

inline constexpr auto SkillOf(int64_t item) noexcept -> uint32_t {
	return static_cast<uint32_t>(item - ItemBase);
}

inline constexpr auto IsKeyItem(int64_t item) noexcept -> bool {
	return item > KeyBase && item < KeyBase + 15;
}

inline constexpr auto IsLootItem(int64_t item) noexcept -> bool {
	return item >= LootBase && item < LootBase + 1000;
}

} // namespace ap::ids
