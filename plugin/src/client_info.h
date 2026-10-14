#pragma once

// What the game client knows about the game it's in, read from its own globals (#154),
// found by byte pattern like the quest record (#138). Only reads, no hooks; if a
// pattern isn't found the plugin learns these the old way (a kill, an area).
//
// The difficulty: the game's first packet (0x01, the game flags) says it, and the
// client's handler hands it to a one-line setter that stores it in a byte:
//   mov edi, [rcx+2]; mov rbx, rcx; movzx ecx, byte [rcx+1]; call <setter>; movzx edx, byte [rbx+6]; sub edx, 1
//   setter: mov [rip+disp32], cl; ret
// The game's getter (movzx eax, byte [rip+disp32]; ret) reads the same byte, which
// the locator checks. D2R 3.3.93847 with D2RLoader 1.3.1-beta: the handler's code at
// 0x1298CA, the setter at 0x8D9E0, the getter at 0x8AF40 and the byte at 0x2A233DF.
//
// The local player (#155): the client's "get my unit" is lookup(slot()), called
// that way all over the client ("call <slot>; mov ecx, eax; call <lookup>"):
//   slot:   mov eax, [rip+disp32]; ret                        the local player's slot, 0..7
//   lookup: if slot < 8 and ids[slot] != -1, find the player unit ids[slot] in the
//           client's unit table (128 buckets by id & 0x7F, chained), as
//           lea rdi, [ids]; ...; lea rcx, [table]; xor r9d, r9d; mov r8d, [rdi+rax*4]; ...; and edx, 0x7F; call <find>
//   find:   walks the bucket: cmp [rax+8], id; jne; cmp [rax], type; je; mov rcx, [rax+<next>]
// D2RLoader hooks find's first bytes, so the walk is done here. A unit has its type
// (0 = player) at +0, its class at +4, its id at +8 and its player data at +0x10,
// which starts with the character's name. D2R 3.3.93847: lookup at 0x9A480, the slot
// at 0x2A23704, the ids at 0x2A238F0, the table at 0x2A23910 and next at +0x158.

#include <cstdint>
#include <optional>
#include <string>
#include <string_view>

namespace ap::client_info {

// Where something is, as an RVA in the game image, or why it wasn't found.
struct Location {
	uint32_t    rva = 0;
	std::string error;
};

auto LocateDifficulty(const uint8_t* image) -> Location;

// The difficulty (0 Normal, 1 Nightmare, 2 Hell) in the byte at address, if it holds one.
// The byte keeps the last game's until the next game's first packet, which comes before
// D2RLoader says the game was joined. UI thread.
auto ReadDifficulty(uintptr_t address) -> std::optional<int>;

// Where the local player's unit is found from, as RVAs in the game image (0: not found).
struct PlayerLocation {
	uint32_t    slot  = 0;
	uint32_t    ids   = 0;
	uint32_t    table = 0;
	uint32_t    next  = 0; // the offset of a unit's next unit in its bucket
	std::string error;
};
auto LocatePlayer(const uint8_t* image) -> PlayerLocation;

struct Player {
	int         classId = -1; // 0 Amazon ... 7 Warlock
	std::string name;
};
// The local player as the client has it, read from the image mapped at base: none while
// not in a game, or if anything doesn't look right. UI thread.
auto ReadPlayer(uintptr_t base, const PlayerLocation& where) -> std::optional<Player>;

// Whether text looks like a character name: 2 to 47 bytes (the save's name field holds
// 48 with the NUL; UTF-8 letters take more than one), none of them control characters.
auto ValidName(std::string_view text) -> bool;

} // namespace ap::client_info
