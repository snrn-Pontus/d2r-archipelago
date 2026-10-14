#pragma once

// The game's own quest log (#138), which says when a quest is really done.
//
// D2R's client keeps the local player's quest flags for the game's difficulty in
// a quest record: a bit buffer of 0x60 bytes, 16 bits per quest, indexed like the
// save file (1 = Den of Evil ... 40 = Eve of Destruction, the flags in
// data::Quests). The server sends the whole record again (packet 0x28, type 6)
// whenever a quest changes, and the client's handler copies it into the record
// that a global points at. The client allocates the record when a game starts and
// frees it (then clears the global) when it ends, on the UI thread, so it's read
// there. Found by the bytes of that handler and of the code that frees it, which
// must agree; only reads, no hooks.
//
// D2R 3.3.93847 with D2RLoader 1.3.1-beta: the global is at RVA 0x2A48778, the
// handler's copy at 0x10E710 and the free at 0x10C9C6.

#include "game_data.h"

#include <array>
#include <cstdint>
#include <string>

namespace ap::quest_record {

inline constexpr size_t Bytes = 0x60;
using Bits = std::array<uint8_t, Bytes>;

// Quest states (bits within a quest's 16): the reward was given, or the quest's
// goal is done and the reward waits (talking to the quest giver, or using it).
inline constexpr unsigned RewardGranted = 0;
inline constexpr unsigned RewardPending = 1;

inline constexpr auto State(const Bits& bits, unsigned quest, unsigned state) -> bool {
	const unsigned bit = quest * 16 + state;
	return bit / 8 < Bytes && (bits[bit / 8] >> (bit % 8) & 1) != 0;
}

// The data::Quests flags the record shows done (bit n for flag n).
inline constexpr auto DoneMask(const Bits& bits) -> uint64_t {
	uint64_t done = 0;
	for (const auto& quest : data::Quests) {
		if (State(bits, quest.flag, RewardGranted) || State(bits, quest.flag, RewardPending)) {
			done |= uint64_t { 1 } << quest.flag;
		}
	}
	return done;
}

// What a difficulty's quest log held as the game began, in the game where the seed first
// read it for its character (a quest done later in that game is progress, even one done
// before the plugin knew the difficulty).
// A character that already played the seed there (it has quest checks there) keeps
// everything; otherwise what it had done before is its own, not the seed's: an
// older character doesn't hand the seed all its quests at once.
inline constexpr auto Baseline(uint64_t done, bool seedHasQuestChecks) -> uint64_t {
	return seedHasQuestChecks ? 0 : done;
}

// Where the client's record pointer is, as an RVA in the game image, or why it wasn't found.
struct Location {
	uint32_t    rva = 0;
	std::string error;
};
auto Locate(const uint8_t* image) -> Location;

enum class ReadResult : uint8_t {
	Ok,
	NoRecord, // not in a game (the pointer is clear)
	Invalid,  // the record doesn't look like one
	Fault,    // reading it faulted
};
// Copies the record that the pointer at pointerAddress points at. UI thread.
auto Read(uintptr_t pointerAddress, Bits& bits) -> ReadResult;

} // namespace ap::quest_record
