#pragma once

#include "ap_client.h"

#include <cstdint>
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

// The Codex's skills, in the order they were first received: each gets one
// level per copy found, plus one per Codex Empowerment, plus one for every
// `scaling` character levels (0 turns scaling off), at most `cap` of them (0 = no cap).
auto Entries(const std::vector<NetworkItem>& items, int characterLevel, int scaling, int cap = 0) -> std::vector<Entry>;

// A 16-bit fingerprint of a run of entries, stored in the Codex charm's seed
// so the plugin can tell whether a charm is up to date.
auto Fingerprint(const std::vector<Entry>& entries, size_t first, size_t count, int playerClass) -> uint32_t;

// Codex charms hold chunks of skills: your class's on chunks 0.., other
// classes' on OtherClassChunk.., since D2 adds 6 to an oskill's level
// requirement when its class isn't yours, and one such skill would switch off
// a shared charm's every skill until level 6.
inline constexpr uint32_t OtherClassChunk = 8;
inline constexpr uint32_t ChunkCount      = 16;

// Splits the entries into ChunkCount chunks of at most perChunk skills each.
// An empty class name puts every skill with the other classes'.
auto Chunks(const std::vector<Entry>& entries, std::string_view playerClass, size_t perChunk) -> std::vector<std::vector<Entry>>;

} // namespace ap::codex
