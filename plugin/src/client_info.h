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

#include <cstdint>
#include <optional>
#include <string>

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

} // namespace ap::client_info
