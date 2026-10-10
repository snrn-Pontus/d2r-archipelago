#pragma once

// Must match apworld/d2r_archipelago/ids.py.

#include <cstdint>

namespace ap::ids {

inline constexpr int64_t ItemBase     = 2'430'000;
inline constexpr int64_t LocationBase = 2'420'000;
// Skills are ItemBase + skill id (all below 900).
inline constexpr int64_t EmpowermentItem = ItemBase + 900;
// Act keys: KeyBase + act index (1 = Normal Act II ... 14 = Hell Act V).
inline constexpr int64_t KeyBase         = ItemBase + 950;
inline constexpr int64_t LootBase        = ItemBase + 1000;

inline constexpr auto QuestLocation(uint32_t difficulty, uint32_t questFlag) noexcept -> int64_t {
	return LocationBase + difficulty * 100 + questFlag;
}

inline constexpr auto AreaLocation(uint32_t levelId) noexcept -> int64_t {
	return LocationBase + 1000 + levelId;
}

inline constexpr auto LevelLocation(uint32_t level) noexcept -> int64_t {
	return LocationBase + 2000 + level;
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
