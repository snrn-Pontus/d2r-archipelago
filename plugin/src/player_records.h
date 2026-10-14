#pragma once

// The player's own quest and waypoint records for all three difficulties (#157), as the
// game keeps them for the character's save: the in-process game server's copy of the
// player (single player runs the server inside the client). Found by byte pattern like
// the quest record (#138) and the client's player (#155); only reads, no hooks.
//
// The server keeps its clients in a table of 256 buckets by client id, chained:
//   movzx ebx, cl; lea rax, [table]; ...; mov rbx, [rax+rbx*8]; test; je; cmp [rbx], ecx; je; mov rbx, [rbx+<next>]
// A client has the character's name at +<name> (removing a client copies 0x3D bytes from there):
//   cmp [rbx], esi; jne; lea r8, [rbx+<name>]; mov edx, 0x3D
// and its player unit, once looked up, cached at +<unit> (CLIENTS_GetPlayerFromClient), next to
// the unit's type and id that look it up:
//   test byte [rbx+..], 1; jne; test edi, edi; jne; cmp qword [rbx+<unit>], 0; jne; xor eax, eax
//   mov r8d, [rbx+<id>]; mov edx, [rbx+<type>]; mov rcx, [rbx+<game>]; call <find>; test rax, rax; jne; mov [rbx+<unit>], rax
// The unit's player data (unit+0x10) starts with the name, then has the quest records for
// Normal, Nightmare and Hell at +0x40/+0x48/+0x50 (bit buffers like quest_record's) and the
// waypoint records at +0x58/+0x60/+0x68 ("02 01", then the waypoint bits, as in the .d2s).
// D2R 3.3.93847: the table at 0x2AAECA0, next at +0x630, the name at +0x2C, the unit at
// +0x278 and its id at +0x270.

#include "quest_record.h"

#include <array>
#include <cstdint>
#include <optional>
#include <string>
#include <string_view>

namespace ap::player_records {

struct Location {
	uint32_t    clients = 0; // RVA of the client table
	uint32_t    next    = 0; // offsets in a client
	uint32_t    name    = 0;
	uint32_t    unit    = 0;
	uint32_t    unitId  = 0;
	std::string error;
};
auto Locate(const uint8_t* image) -> Location;

// 39 waypoints, a bit each (index n is data::Waypoints[n]).
inline constexpr size_t WaypointBytes = 5;
using Waypoints = std::array<uint8_t, WaypointBytes>;

inline constexpr auto Has(const Waypoints& waypoints, size_t index) -> bool {
	return index / 8 < WaypointBytes && (waypoints[index / 8] >> (index % 8) & 1) != 0;
}

struct Records {
	std::array<quest_record::Bits, 3> quests {};
	std::array<Waypoints, 3>          waypoints {};
};

// The records of the player called name, from the image mapped at base: none if the server
// has no such client with a player, or anything doesn't look right. Game thread.
auto Read(uintptr_t base, const Location& where, std::string_view name) -> std::optional<Records>;

} // namespace ap::player_records
