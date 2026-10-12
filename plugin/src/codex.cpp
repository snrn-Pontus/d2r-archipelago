#include "codex.h"

#include "game_data.h"
#include "ids.h"

#include <algorithm>
#include <unordered_map>

namespace ap::codex {

auto Entries(const std::vector<NetworkItem>& items, int characterLevel, int scaling, int cap) -> std::vector<Entry> {
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
	int scaled = scaling > 0 ? std::max(characterLevel, 0) / scaling : 0;
	if (cap > 0) {
		scaled = std::min(scaled, cap);
	}
	const int bonus = empowerments + scaled;
	for (auto& entry : entries) {
		// D2 shows skill levels up to 99; stay well inside that.
		entry.level = std::min(entry.level + bonus, 60);
	}
	return entries;
}

auto PlanTreeDelivery(const std::vector<NetworkItem>& items, const std::set<uint32_t>& unlocked, int pointsGiven) -> TreeDelivery {
	TreeDelivery delivery;
	std::set<uint32_t> seen;
	int points = 0;
	for (const auto& item : items) {
		if (item.item == ids::SkillPointItem) {
			++points;
		} else if (ids::IsSkillItem(item.item)) {
			const uint32_t skill = ids::SkillOf(item.item);
			if (!seen.insert(skill).second) {
				++points;
			} else if (!unlocked.contains(skill)) {
				delivery.unlock.push_back(skill);
			}
		}
	}
	delivery.points = std::max(points - pointsGiven, 0);
	return delivery;
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

auto Chunks(const std::vector<Entry>& entries, std::string_view playerClass, size_t perChunk, const randomizer::SkillTree& tree)
	-> std::vector<std::vector<Entry>> {
	std::vector<std::vector<Entry>> chunks(ChunkCount);
	uint32_t own = 0, other = OtherClassChunk;
	for (const auto& entry : entries) {
		// The tree slot's original owner tells the class.
		const auto dealt = std::ranges::find_if(tree, [&](const auto& pair) { return pair.first == entry.skill; });
		const uint32_t owner = dealt != tree.end() ? dealt->second : entry.skill;
		const bool mine = !playerClass.empty() && std::ranges::any_of(data::Skills, [&](const data::Skill& skill) {
			return skill.id == owner && skill.cls == playerClass;
		});
		uint32_t& chunk = mine ? own : other;
		if (chunks[chunk].size() == perChunk) {
			++chunk;
		}
		if (chunk < (mine ? OtherClassChunk : ChunkCount)) {
			chunks[chunk].push_back(entry);
		}
	}
	return chunks;
}

} // namespace ap::codex
