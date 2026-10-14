#include "skill_table.h"

#include <cstring>
#include <set>

namespace ap::skill_table {

namespace {

auto Read(const uint8_t* rows, size_t rowSize, size_t row, size_t offset) -> uint16_t {
	uint16_t value = 0;
	std::memcpy(&value, rows + row * rowSize + offset, sizeof(value));
	return value;
}

} // namespace

auto FindSelfPrerequisite(const uint8_t* rows, size_t rowCount, size_t rowSize, std::span<const uint16_t> classSkills) -> std::optional<size_t> {
	if (rows == nullptr || rowSize < 6) {
		return std::nullopt;
	}
	std::set<size_t> skills;
	for (const uint16_t skill : classSkills) {
		if (skill < rowCount) {
			skills.insert(skill);
		}
	}
	if (skills.size() < 10) {
		return std::nullopt;
	}
	for (size_t offset = 0; offset + 6 <= rowSize; offset += 2) {
		size_t self = 0, empty = 0;
		for (const size_t row : skills) {
			if (Read(rows, rowSize, row, offset) == row) {
				++self;
				empty += Read(rows, rowSize, row, offset + 2) == NoSkill && Read(rows, rowSize, row, offset + 4) == NoSkill ? 1 : 0;
			}
		}
		// Unlocked skills no longer name themselves, so half is enough.
		if (self * 2 < skills.size() || empty * 10 < self * 9) {
			continue;
		}
		size_t others = 0, otherSelf = 0;
		for (size_t row = 0; row < rowCount; ++row) {
			if (!skills.contains(row)) {
				++others;
				otherSelf += Read(rows, rowSize, row, offset) == row ? 1 : 0;
			}
		}
		if (otherSelf * 10 <= others) {
			return offset;
		}
	}
	return std::nullopt;
}

auto IsSelfLocked(const uint8_t* rows, size_t rowCount, size_t rowSize, size_t offset, uint16_t skill) -> bool {
	return rows != nullptr && skill < rowCount && offset + 2 <= rowSize && Read(rows, rowSize, skill, offset) == skill;
}

auto FindIconCel(const uint8_t* rows, size_t rowCount, size_t rowSize, std::span<const randomizer::LockedIcon> icons) -> std::optional<size_t> {
	if (rows == nullptr || icons.size() < 10) {
		return std::nullopt;
	}
	for (size_t offset = 0; offset < rowSize; ++offset) {
		size_t locked = 0, shown = 0;
		for (const auto& icon : icons) {
			if (icon.descRow >= rowCount) {
				continue;
			}
			const int cel = rows[icon.descRow * rowSize + offset];
			locked += cel == icon.lockedCel ? 1 : 0;
			shown += cel == icon.cel ? 1 : 0;
		}
		if (locked * 2 >= icons.size() && (locked + shown) * 10 >= icons.size() * 9) {
			return offset;
		}
	}
	return std::nullopt;
}

} // namespace ap::skill_table
