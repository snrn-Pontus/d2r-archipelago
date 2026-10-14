#include "player_records.h"

#include "native_scan.h"

#include <windows.h>

#include <cstring>

namespace ap::player_records {

namespace {

// The client table: its disp32 at +6 (ending at +10), the next client's offset at +32.
constexpr const char* ClientsPattern = "0F B6 D9 48 8D 05 ?? ?? ?? ?? 4D 8B F8 4C 8B F2 48 8B 1C D8 48 85 DB 74 ?? 39 0B 74 ?? 48 8B 9B ?? ?? ?? ??";
// Removing a client: the name's offset is the byte at +7.
constexpr const char* NamePattern = "39 33 75 ?? 4C 8D 43 ?? BA 3D 00 00 00";
// CLIENTS_GetPlayerFromClient: the cached unit's offset at +16; then the lookup, with the
// unit id's offset at +3 and the cache's again at +33.
constexpr const char* CachePattern  = "F6 83 ?? ?? ?? ?? 01 75 ?? 85 FF 75 ?? 48 83 BB ?? ?? ?? ?? 00 75 ?? 33 C0";
constexpr const char* LookupPattern = "44 8B 83 ?? ?? ?? ?? 8B 93 ?? ?? ?? ?? 48 8B 8B ?? ?? ?? ?? E8 ?? ?? ?? ?? 48 85 C0 75 ?? 48 89 83";

constexpr uint32_t Buckets       = 256;
constexpr size_t   NameBytes     = 0x3D;
constexpr size_t   QuestsAt      = 0x40;
constexpr size_t   WaypointsAt   = 0x58;
constexpr uint64_t QuestBitCount = quest_record::Bytes * 8;

auto U32(const uint8_t* image, uint32_t rva) -> uint32_t {
	uint32_t value = 0;
	std::memcpy(&value, image + rva, sizeof(value));
	return value;
}

// What the guarded read copies out (no C++ objects under __try).
struct Raw {
	uint8_t quests[3][quest_record::Bytes];
	uint8_t waypoints[3][WaypointBytes];
};

auto NameAt(uintptr_t address, const char* name, size_t length) -> bool {
	const auto* text = reinterpret_cast<const volatile char*>(address);
	for (size_t i = 0; i < length; ++i) {
		if (text[i] != name[i]) {
			return false;
		}
	}
	return text[length] == '\0';
}

auto ReadGuarded(uintptr_t table, const Location& where, const char* name, size_t length, Raw* out) -> bool {
	__try {
		for (uint32_t bucket = 0; bucket < Buckets; ++bucket) {
			uintptr_t client = reinterpret_cast<const volatile uintptr_t*>(table)[bucket];
			for (int hops = 0; client != 0 && hops < 64; ++hops) {
				if (NameAt(client + where.name, name, length)) {
					const uintptr_t unit = *reinterpret_cast<const volatile uintptr_t*>(client + where.unit);
					if (unit == 0 || *reinterpret_cast<const volatile uint32_t*>(unit) != 0
					    || *reinterpret_cast<const volatile uint32_t*>(unit + 8) != *reinterpret_cast<const volatile uint32_t*>(client + where.unitId)) {
						return false;
					}
					const uintptr_t data = *reinterpret_cast<const volatile uintptr_t*>(unit + 0x10);
					if (data == 0 || !NameAt(data, name, length)) {
						return false;
					}
					for (size_t difficulty = 0; difficulty < 3; ++difficulty) {
						const uintptr_t record = *reinterpret_cast<const volatile uintptr_t*>(data + QuestsAt + difficulty * 8);
						const uintptr_t bytes  = *reinterpret_cast<const volatile uintptr_t*>(record);
						if (bytes == 0 || *reinterpret_cast<const volatile uint64_t*>(record + 8) != QuestBitCount) {
							return false;
						}
						const uintptr_t waypoints = *reinterpret_cast<const volatile uintptr_t*>(data + WaypointsAt + difficulty * 8);
						const auto*     header    = reinterpret_cast<const volatile uint8_t*>(waypoints);
						if (header[0] != 2 || header[1] != 1) {
							return false;
						}
						for (size_t i = 0; i < quest_record::Bytes; ++i) {
							out->quests[difficulty][i] = reinterpret_cast<const volatile uint8_t*>(bytes)[i];
						}
						for (size_t i = 0; i < WaypointBytes; ++i) {
							out->waypoints[difficulty][i] = header[2 + i];
						}
					}
					return true;
				}
				client = *reinterpret_cast<const volatile uintptr_t*>(client + where.next);
			}
		}
		return false;
	} __except (GetExceptionCode() == EXCEPTION_ACCESS_VIOLATION ? EXCEPTION_EXECUTE_HANDLER : EXCEPTION_CONTINUE_SEARCH) {
		return false;
	}
}

} // namespace

auto Locate(const uint8_t* image) -> Location {
	const auto clients = native::FindUnique(image, ClientsPattern);
	if (!clients.rva) {
		return { .error = "the client table was " + clients.error };
	}
	const auto name = native::FindUnique(image, NamePattern);
	if (!name.rva) {
		return { .error = "the client name was " + name.error };
	}
	const auto cache = native::FindUnique(image, CachePattern);
	const auto lookup = native::FindUnique(image, LookupPattern);
	if (!cache.rva || !lookup.rva) {
		return { .error = "the client's player was " + (cache.rva ? lookup.error : cache.error) };
	}
	Location where {
		.clients = native::RipTarget(image, *clients.rva + 6, *clients.rva + 10),
		.next    = U32(image, *clients.rva + 32),
		.name    = image[*name.rva + 7],
		.unit    = U32(image, *cache.rva + 16),
		.unitId  = U32(image, *lookup.rva + 3),
	};
	if (*lookup.rva < *cache.rva || *lookup.rva - *cache.rva > 0x100 || U32(image, *lookup.rva + 33) != where.unit) {
		return { .error = "the client's player lookup doesn't look right" };
	}
	if (where.next == 0 || where.next > 0x2000 || where.unit == 0 || where.unit > 0x2000 || where.unitId > 0x2000) {
		return { .error = "the client's offsets don't look right" };
	}
	bool inData = false;
	for (const auto& section : native::Sections(image)) {
		inData |= section.writable && !section.executable && where.clients >= section.rva && where.clients + Buckets * 8 <= section.rva + section.size;
	}
	if (!inData) {
		return { .error = "the client table isn't in the game's data" };
	}
	return where;
}

auto Read(uintptr_t base, const Location& where, std::string_view name) -> std::optional<Records> {
	if (base == 0 || where.clients == 0 || name.empty() || name.size() >= NameBytes) {
		return std::nullopt;
	}
	Raw raw {};
	if (!ReadGuarded(base + where.clients, where, name.data(), name.size(), &raw)) {
		return std::nullopt;
	}
	Records records;
	for (size_t difficulty = 0; difficulty < 3; ++difficulty) {
		std::memcpy(records.quests[difficulty].data(), raw.quests[difficulty], quest_record::Bytes);
		std::memcpy(records.waypoints[difficulty].data(), raw.waypoints[difficulty], WaypointBytes);
	}
	return records;
}

} // namespace ap::player_records
