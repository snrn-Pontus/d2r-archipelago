#pragma once

// Finds things in the game's code by their bytes instead of fixed addresses (#138).
// D2RLoader runs the game as a rebuilt image of its own, so addresses move with
// every game patch and every D2RLoader release, while a function's own bytes
// rarely change. A pattern counts only if it matches exactly once; otherwise it's
// "not found" and the caller falls back to what it did without it.

#include <cstddef>
#include <cstdint>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <vector>

namespace ap::native {

// Hex bytes and ?? wildcards ("48 8B 0D ?? ?? ?? ??"), a wildcard as -1.
// Empty if the text isn't a pattern.
auto ParsePattern(std::string_view text) -> std::vector<int16_t>;

// Offsets in haystack where the pattern matches, at most limit of them.
auto FindPattern(std::span<const uint8_t> haystack, std::span<const int16_t> pattern, size_t limit = 2) -> std::vector<size_t>;

struct Section {
	uint32_t rva        = 0;
	uint32_t size       = 0;
	bool     executable = false;
	bool     writable   = false;
};

// The sections of a PE image mapped at image (headers included). Empty if it isn't one.
auto Sections(const uint8_t* image) -> std::vector<Section>;

// Where a pattern matches once in the image's executable sections, or why it didn't.
struct Match {
	std::optional<uint32_t> rva;
	std::string             error;
};
auto FindUnique(const uint8_t* image, std::string_view pattern) -> Match;

// What a rip-relative operand points at: its disp32 is at dispRva and its instruction ends at endRva.
auto RipTarget(const uint8_t* image, uint32_t dispRva, uint32_t endRva) -> uint32_t;

} // namespace ap::native
