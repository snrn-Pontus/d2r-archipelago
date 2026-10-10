#pragma once

#include <cstdint>
#include <filesystem>
#include <set>

namespace ap {

// What the plugin remembers about one seed and slot between sessions.
struct SaveState {
	// Every check this client has found, sent or not.
	std::set<int64_t> checks;
	// How many received items have had their filler handed out.
	size_t fillersDelivered = 0;
	bool goalReached = false;

	bool Load(const std::filesystem::path& path);
	bool Save(const std::filesystem::path& path) const;
};

} // namespace ap
