#include "codex.h"

#include "ids.h"

#include <algorithm>
#include <unordered_map>

namespace ap::codex {

auto Entries(const std::vector<NetworkItem>& items, int characterLevel, int scaling) -> std::vector<Entry> {
	std::vector<Entry> entries;
	std::unordered_map<uint32_t, size_t> position;
	int empowerments = 0;
	for (const auto& item : items) {
		if (item.item == ids::EmpowermentItem) {
			++empowerments;
		} else if (ids::IsSkillItem(item.item)) {
			const uint32_t skill = ids::SkillOf(item.item);
			const auto [found, added] = position.try_emplace(skill, entries.size());
			if (added) {
				entries.push_back({ skill, 0 });
			}
			++entries[found->second].level;
		}
	}
	const int bonus = empowerments + (scaling > 0 ? std::max(characterLevel, 0) / scaling : 0);
	for (auto& entry : entries) {
		// D2 shows skill levels up to 99; stay well inside that.
		entry.level = std::min(entry.level + bonus, 60);
	}
	return entries;
}

auto Fingerprint(const std::vector<Entry>& entries, size_t first, size_t count, int playerClass) -> uint32_t {
	uint32_t hash = 2166136261U;
	auto mix = [&](uint32_t value) {
		for (int i = 0; i < 4; ++i) {
			hash = (hash ^ ((value >> (i * 8)) & 0xFF)) * 16777619U;
		}
	};
	mix(static_cast<uint32_t>(playerClass + 1));
	for (size_t i = first; i < first + count && i < entries.size(); ++i) {
		mix(entries[i].skill);
		mix(static_cast<uint32_t>(entries[i].level));
	}
	return (hash ^ (hash >> 16)) & 0xFFFF;
}

} // namespace ap::codex
