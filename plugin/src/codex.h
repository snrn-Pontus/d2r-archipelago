#pragma once

#include "ap_client.h"

#include <cstdint>
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
// `scaling` character levels (0 turns scaling off).
auto Entries(const std::vector<NetworkItem>& items, int characterLevel, int scaling) -> std::vector<Entry>;

// A 16-bit fingerprint of a run of entries, stored in the Codex charm's seed
// so the plugin can tell whether a charm is up to date.
auto Fingerprint(const std::vector<Entry>& entries, size_t first, size_t count, int playerClass) -> uint32_t;

} // namespace ap::codex
