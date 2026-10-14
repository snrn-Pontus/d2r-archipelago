#pragma once

// The difficulty of a game that was left before anything in it said which one
// it was (#130). D2RLoader's area events don't carry the difficulty, so areas
// entered before the first kill wait for it; leaving the game would lose them.
// Save & Exit writes the character's .d2s, whose header marks the difficulty
// just played. (Creating the next game doesn't write it: a backup D2RLoader took
// 30 seconds into a game still had the time the last one was left.)
//
// D2RLoader doesn't say the character's name, so this looks at every character
// saved since the game began: one character plays a game, so all of them must agree.

#include <cstdint>
#include <filesystem>
#include <fstream>
#include <optional>
#include <set>
#include <span>
#include <system_error>

namespace ap::saved_difficulty {

// D2R 3.x character files (version 0x69): three bytes from 0x98, one per
// difficulty; the high bit marks the one last played, the low bits its act.
inline constexpr uint32_t Signature      = 0xAA55AA55;
inline constexpr uint32_t Version        = 0x69;
inline constexpr size_t   DifficultyBase = 0x98;
inline constexpr size_t   HeaderSize     = DifficultyBase + 3;

// The difficulty a character file's header marks as last played, if it's a
// version this knows and exactly one difficulty is marked.
inline auto FromHeader(std::span<const uint8_t> header) -> std::optional<int> {
	if (header.size() < HeaderSize) {
		return std::nullopt;
	}
	auto word = [&](size_t at) {
		return static_cast<uint32_t>(header[at]) | static_cast<uint32_t>(header[at + 1]) << 8 | static_cast<uint32_t>(header[at + 2]) << 16
		     | static_cast<uint32_t>(header[at + 3]) << 24;
	};
	if (word(0) != Signature || word(4) != Version) {
		return std::nullopt;
	}
	std::optional<int> marked;
	for (int difficulty = 0; difficulty < 3; ++difficulty) {
		if ((header[DifficultyBase + static_cast<size_t>(difficulty)] & 0x80) != 0) {
			if (marked) {
				return std::nullopt;
			}
			marked = difficulty;
		}
	}
	return marked;
}

// The difficulties of the characters in `directory` saved at or after `since`.
inline auto Find(const std::filesystem::path& directory, std::filesystem::file_time_type since) -> std::set<int> {
	std::set<int> found;
	std::error_code error;
	for (const auto& entry : std::filesystem::directory_iterator(directory, error)) {
		std::error_code fileError;
		const std::filesystem::path& path = entry.path();
		if (path.extension() != L".d2s" || entry.last_write_time(fileError) < since || fileError) {
			continue;
		}
		uint8_t header[HeaderSize] {};
		std::ifstream file(path, std::ios::binary);
		if (!file.read(reinterpret_cast<char*>(header), sizeof(header))) {
			continue;
		}
		if (const std::optional<int> difficulty = FromHeader(header)) {
			found.insert(*difficulty);
		}
	}
	return found;
}

} // namespace ap::saved_difficulty
