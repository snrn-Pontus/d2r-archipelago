#pragma once

#include "ap_client.h"

#include <cstdint>
#include <set>
#include <string_view>
#include <utility>
#include <vector>

namespace ap::codex {

// One skill on the Codex and the level it grants.
struct Entry {
	uint32_t skill = 0;
	int      level = 0;

	auto operator==(const Entry&) const -> bool = default;
};

// How Codex skill levels add up; the seed's options, defaults as the apworld's.
struct Leveling {
	// +1 for every this many character levels (0 = off), at most scalingCap of them (0 = no cap).
	int scaling        = 0;
	int scalingCap     = 0;
	int perCopy        = 1;
	int perEmpowerment = 1;
	// No skill goes past this level (D2 shows skill levels up to 99).
	int levelCap       = 60;
};

// The Codex's skills, in the order they were first received: each gets
// perCopy levels per copy found, plus perEmpowerment per Codex Empowerment,
// plus the character level scaling, at most levelCap in all.
auto Entries(const std::vector<NetworkItem>& items, int characterLevel, const Leveling& leveling) -> std::vector<Entry>;

// A 16-bit fingerprint of a run of entries, stored in the Codex charm's seed
// so the plugin can tell whether a charm is up to date.
auto Fingerprint(const std::vector<Entry>& entries, size_t first, size_t count, int playerClass) -> uint32_t;

// Tree mode: what the received items still owe the character. Each skill's
// first copy unlocks it in the tree (once: skadd sets the skill's level); every
// later copy and every Skill Point item is an unspent skill point.
struct TreeDelivery {
	std::vector<uint32_t> unlock;
	int                   points = 0;
};
auto PlanTreeDelivery(const std::vector<NetworkItem>& items, const std::set<uint32_t>& unlocked, int pointsGiven) -> TreeDelivery;

// Codex charms hold chunks of skills: your class's on chunks 0.., other
// classes' on OtherClassChunk.., since D2 adds 6 to an oskill's level
// requirement when its class isn't yours, and one such skill would switch off
// a shared charm's every skill until level 6.
inline constexpr uint32_t OtherClassChunk = 8;
inline constexpr uint32_t ChunkCount      = 16;

// Splits the entries into ChunkCount chunks of at most perChunk skills each.
// A skill's class is the one whose tree it sits in after the seed's skill tree
// shuffle. An empty class name puts every skill with the other classes'.
auto Chunks(const std::vector<Entry>& entries, std::string_view playerClass, size_t perChunk, const randomizer::SkillTree& tree = {})
	-> std::vector<std::vector<Entry>>;

} // namespace ap::codex
