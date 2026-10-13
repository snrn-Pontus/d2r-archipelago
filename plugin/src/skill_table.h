#pragma once

#include <cstddef>
#include <cstdint>
#include <optional>
#include <span>

// Tree mode: d2r-ap-setup locks every class skill by making it its own first
// prerequisite (randomizer.cpp, BlockClassSkills). A received skill is unlocked
// by clearing that prerequisite in the game's compiled Skills rows, so no free
// level is given (a respec would refund it as a point) and the unlock lasts
// through a respec.

namespace ap::skill_table {

// The compiled value of an empty skill reference.
inline constexpr uint16_t NoSkill = 0xFFFF;

// The offset of the first prerequisite (reqskill1) in a compiled Skills row, found
// as the 16-bit field where most class skills name themselves with the other two
// prerequisites empty, and few other rows name themselves (which rules out the id).
// Rows are indexed by skill id. Nothing if no field fits.
auto FindSelfPrerequisite(const uint8_t* rows, size_t rowCount, size_t rowSize, std::span<const uint16_t> classSkills) -> std::optional<size_t>;

// Whether the skill still names itself at offset (locked); the caller unlocks it by
// writing NoSkill there.
auto IsSelfLocked(const uint8_t* rows, size_t rowCount, size_t rowSize, size_t offset, uint16_t skill) -> bool;

} // namespace ap::skill_table
