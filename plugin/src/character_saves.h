#pragma once

// The characters in the mod's save folder, from their .d2s headers (#156), for telling
// the player at the main menu which ones can play the seed. Only reads the files.

#include "saved_difficulty.h"

#include <algorithm>
#include <cstdint>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <optional>
#include <span>
#include <string>
#include <system_error>
#include <vector>

namespace ap::character_saves {

// D2R 3.x character files (version 0x69): the class at 0x18, the level at 0x1B and
// the name at 0x12B (48 bytes with its NUL).
inline constexpr size_t ClassAt    = 0x18;
inline constexpr size_t LevelAt    = 0x1B;
inline constexpr size_t NameAt     = 0x12B;
inline constexpr size_t NameBytes  = 48;
inline constexpr size_t HeaderSize = NameAt + NameBytes;

struct Character {
	std::string name;
	int         classId = -1;
	int         level   = 0;
};

inline auto FromHeader(std::span<const uint8_t> header) -> std::optional<Character> {
	if (header.size() < HeaderSize) {
		return std::nullopt;
	}
	uint32_t signature = 0, version = 0;
	std::memcpy(&signature, header.data(), 4);
	std::memcpy(&version, header.data() + 4, 4);
	if (signature != saved_difficulty::Signature || version != saved_difficulty::Version || header[ClassAt] > 7) {
		return std::nullopt;
	}
	const auto* name = reinterpret_cast<const char*>(header.data() + NameAt);
	Character character { .name = std::string(name, strnlen(name, NameBytes)), .classId = header[ClassAt], .level = header[LevelAt] };
	if (character.name.size() < 2 || character.name.size() >= NameBytes
	    || std::ranges::any_of(character.name, [](char c) { return static_cast<unsigned char>(c) < 0x20 || c == 0x7F; })) {
		return std::nullopt;
	}
	return character;
}

// The characters saved in directory, most recently played first.
inline auto List(const std::filesystem::path& directory) -> std::vector<Character> {
	std::vector<std::pair<std::filesystem::file_time_type, Character>> found;
	std::error_code error;
	for (const auto& entry : std::filesystem::directory_iterator(directory, error)) {
		std::error_code fileError;
		if (entry.path().extension() != L".d2s") {
			continue;
		}
		const auto saved = entry.last_write_time(fileError);
		uint8_t header[HeaderSize] {};
		std::ifstream file(entry.path(), std::ios::binary);
		if (fileError || !file.read(reinterpret_cast<char*>(header), sizeof(header))) {
			continue;
		}
		if (auto character = FromHeader(header)) {
			found.emplace_back(saved, std::move(*character));
		}
	}
	std::ranges::sort(found, [](const auto& a, const auto& b) { return a.first > b.first; });
	std::vector<Character> characters;
	for (auto& [saved, character] : found) {
		characters.push_back(std::move(character));
	}
	return characters;
}

// What the status line says at the main menu: which character to play the seed with.
// className names a class id; seedClass is -1 for a seed any class can claim, and seedName
// is empty until the seed has played a character the game client named (#155).
template <typename ClassName>
auto MenuLine(int seedClass, const std::string& seedName, const std::vector<Character>& characters, ClassName&& className) -> std::string {
	if (!seedName.empty()) {
		const auto mine = std::ranges::find(characters, seedName, &Character::name);
		if (mine == characters.end()) {
			return "Archipelago: play " + seedName + " (" + className(seedClass) + "), not in this mod's saves";
		}
		return "Archipelago: play " + seedName + " (" + className(mine->classId) + ", level " + std::to_string(mine->level) + ")";
	}
	std::vector<std::string> fitting;
	for (const auto& character : characters) {
		if (seedClass < 0 || character.classId == seedClass) {
			fitting.push_back(character.name);
		}
	}
	const std::string cls = seedClass < 0 ? std::string() : std::string(className(seedClass));
	const std::string who = seedClass < 0 ? std::string("any character") : (std::string("AEIOU").find(cls.empty() ? ' ' : cls[0]) != std::string::npos ? "an " : "a ") + cls;
	if (fitting.empty()) {
		return "Archipelago: create " + (seedClass < 0 ? std::string("a character") : who) + " for this seed";
	}
	constexpr size_t Shown = 4;
	std::string names;
	for (size_t i = 0; i < std::min(fitting.size(), Shown); ++i) {
		names += (i == 0 ? "" : ", ") + fitting[i];
	}
	if (fitting.size() > Shown) {
		names += " and " + std::to_string(fitting.size() - Shown) + " more";
	}
	return "Archipelago: play " + who + ": " + names;
}

} // namespace ap::character_saves
