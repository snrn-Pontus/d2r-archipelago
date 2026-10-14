#include "client_info.h"

#include "native_scan.h"

#include <windows.h>

#include <algorithm>
#include <cstring>

namespace ap::client_info {

namespace {

// The game-flags handler: the difficulty's call is at +10 (rel32 at +11, ending at +15).
constexpr const char* FlagsPattern = "8B 79 02 48 8B D9 0F B6 49 01 E8 ?? ?? ?? ?? 0F B6 53 06 83 EA 01";
// One-line byte setters and getters: "mov [rip+disp32], cl; ret" and "movzx eax, byte [rip+disp32]; ret".
constexpr const char* SetterPattern = "88 0D ?? ?? ?? ?? C3";
constexpr const char* GetterPattern = "0F B6 05 ?? ?? ?? ?? C3";

// The player lookup from its first byte: the ids' disp32 is at +0x41, ending at +0x45.
constexpr const char* LookupPattern = "48 89 5C 24 08 57 48 83 EC 20 83 F9 08 0F 83 ?? ?? ?? ?? 8B D9 48 89 5C 24 38 48 83 FB 08 72 ?? "
                                      "48 8D 44 24 38 48 8D 4C 24 40 48 89 44 24 40 E8 ?? ?? ?? ?? 84 C0 74 01 ?? 48 8B 44 24 38 48 8D 3D ?? ?? ?? ??";
// Further on in the lookup, the table: its disp32 at +3, ending at +7.
constexpr const char* TablePattern = "48 8D 0D ?? ?? ?? ?? 45 33 C9 44 8B 04 87 41 8B D0 83 E2 7F E8";
// The bucket walk after D2RLoader's hook: the next unit's offset is the disp32 at +14.
constexpr const char* WalkPattern = "44 39 40 08 75 05 44 39 08 74 ?? 48 8B 88 ?? ?? ?? ??";
// "call <slot>; mov ecx, eax; call <lookup>", and the slot getter "mov eax, [rip+disp32]; ret".
constexpr const char* CallPattern = "E8 ?? ?? ?? ?? 8B C8 E8";
constexpr const char* SlotPattern = "8B 05 ?? ?? ?? ?? C3";

constexpr uint32_t PlayerSlots = 8;
constexpr uint32_t Buckets     = 128;
constexpr size_t   NameBytes   = 48;

// What the guarded read copies out, before it's checked (no C++ objects under __try).
struct RawPlayer {
	int32_t classId;
	char    name[NameBytes];
};

auto InSection(const uint8_t* image, uint32_t rva, uint32_t size, bool executable) -> bool {
	for (const auto& section : native::Sections(image)) {
		if (section.executable == executable && (executable || section.writable) && rva >= section.rva && rva + size <= section.rva + section.size) {
			return true;
		}
	}
	return false;
}

auto Matches(const uint8_t* image, uint32_t rva, const char* text) -> bool {
	const auto pattern = native::ParsePattern(text);
	return InSection(image, rva, static_cast<uint32_t>(pattern.size()), true)
	    && !native::FindPattern({ image + rva, pattern.size() }, pattern, 1).empty();
}

// Whether one of the game's byte getters reads target.
auto HasGetter(const uint8_t* image, uint32_t target) -> bool {
	const auto pattern = native::ParsePattern(GetterPattern);
	for (const auto& section : native::Sections(image)) {
		if (!section.executable) {
			continue;
		}
		for (const size_t offset : native::FindPattern({ image + section.rva, section.size }, pattern, 4096)) {
			const auto rva = section.rva + static_cast<uint32_t>(offset);
			if (native::RipTarget(image, rva + 3, rva + 7) == target) {
				return true;
			}
		}
	}
	return false;
}

auto ReadByteGuarded(uintptr_t address, uint8_t* out) -> bool {
	__try {
		*out = *reinterpret_cast<const volatile uint8_t*>(address);
		return true;
	} __except (GetExceptionCode() == EXCEPTION_ACCESS_VIOLATION ? EXCEPTION_EXECUTE_HANDLER : EXCEPTION_CONTINUE_SEARCH) {
		return false;
	}
}

// The slot global that every "call <slot>; mov ecx, eax; call <lookup>" reads: 0 if there's
// no such call or they don't all agree.
auto SlotOf(const uint8_t* image, uint32_t lookup) -> uint32_t {
	const auto pattern = native::ParsePattern(CallPattern);
	uint32_t slot = 0;
	for (const auto& section : native::Sections(image)) {
		if (!section.executable) {
			continue;
		}
		for (const size_t offset : native::FindPattern({ image + section.rva, section.size }, pattern, size_t { 1 } << 22)) {
			const auto rva = section.rva + static_cast<uint32_t>(offset);
			if (rva + 12 > section.rva + section.size || native::RipTarget(image, rva + 8, rva + 12) != lookup) {
				continue;
			}
			const uint32_t getter = native::RipTarget(image, rva + 1, rva + 5);
			if (!Matches(image, getter, SlotPattern)) {
				return 0;
			}
			const uint32_t target = native::RipTarget(image, getter + 2, getter + 6);
			if (slot != 0 && target != slot) {
				return 0;
			}
			slot = target;
		}
	}
	return slot;
}

auto ReadPlayerGuarded(uintptr_t slotAddress, uintptr_t idsAddress, uintptr_t tableAddress, uint32_t next, RawPlayer* out) -> bool {
	__try {
		const uint32_t slot = *reinterpret_cast<const volatile uint32_t*>(slotAddress);
		if (slot >= PlayerSlots) {
			return false;
		}
		const uint32_t id = reinterpret_cast<const volatile uint32_t*>(idsAddress)[slot];
		if (id == 0xFFFFFFFF) {
			return false;
		}
		uintptr_t unit = reinterpret_cast<const volatile uintptr_t*>(tableAddress)[id % Buckets];
		for (int hops = 0; unit != 0 && hops < 256; ++hops) {
			if (*reinterpret_cast<const volatile uint32_t*>(unit) == 0 && *reinterpret_cast<const volatile uint32_t*>(unit + 8) == id) {
				const uintptr_t data = *reinterpret_cast<const volatile uintptr_t*>(unit + 0x10);
				if (data == 0) {
					return false;
				}
				out->classId = *reinterpret_cast<const volatile int32_t*>(unit + 4);
				for (size_t i = 0; i < NameBytes; ++i) {
					out->name[i] = reinterpret_cast<const volatile char*>(data)[i];
				}
				return true;
			}
			unit = *reinterpret_cast<const volatile uintptr_t*>(unit + next);
		}
		return false;
	} __except (GetExceptionCode() == EXCEPTION_ACCESS_VIOLATION ? EXCEPTION_EXECUTE_HANDLER : EXCEPTION_CONTINUE_SEARCH) {
		return false;
	}
}

} // namespace

auto LocateDifficulty(const uint8_t* image) -> Location {
	const auto flags = native::FindUnique(image, FlagsPattern);
	if (!flags.rva) {
		return { .error = "the game flags handler was " + flags.error };
	}
	const uint32_t setter = native::RipTarget(image, *flags.rva + 11, *flags.rva + 15);
	if (!Matches(image, setter, SetterPattern)) {
		return { .error = "the difficulty setter doesn't look right" };
	}
	const uint32_t target = native::RipTarget(image, setter + 2, setter + 6);
	if (!InSection(image, target, 1, false)) {
		return { .error = "the difficulty isn't in the game's data" };
	}
	if (!HasGetter(image, target)) {
		return { .error = "nothing reads the difficulty" };
	}
	return { .rva = target };
}

auto ReadDifficulty(uintptr_t address) -> std::optional<int> {
	uint8_t value = 0xFF;
	if (address == 0 || !ReadByteGuarded(address, &value) || value > 2) {
		return std::nullopt;
	}
	return value;
}

auto LocatePlayer(const uint8_t* image) -> PlayerLocation {
	const auto lookup = native::FindUnique(image, LookupPattern);
	if (!lookup.rva) {
		return { .error = "the player lookup was " + lookup.error };
	}
	const auto table = native::FindUnique(image, TablePattern);
	if (!table.rva) {
		return { .error = "the unit table was " + table.error };
	}
	if (*table.rva < *lookup.rva + 0x45 || *table.rva > *lookup.rva + 0x100) {
		return { .error = "the unit table isn't in the player lookup" };
	}
	const auto walk = native::FindUnique(image, WalkPattern);
	if (!walk.rva) {
		return { .error = "the bucket walk was " + walk.error };
	}
	PlayerLocation where;
	where.ids   = native::RipTarget(image, *lookup.rva + 0x41, *lookup.rva + 0x45);
	where.table = native::RipTarget(image, *table.rva + 3, *table.rva + 7);
	std::memcpy(&where.next, image + *walk.rva + 14, sizeof(where.next));
	where.slot  = SlotOf(image, *lookup.rva);
	if (where.slot == 0) {
		return { .error = "the player slot wasn't found" };
	}
	if (where.next < 0x18 || where.next > 0x1000) {
		return { .error = "the bucket walk doesn't look right" };
	}
	if (!InSection(image, where.slot, 4, false) || !InSection(image, where.ids, PlayerSlots * 4, false)
	    || !InSection(image, where.table, Buckets * 8, false)) {
		return { .error = "the player lookup's globals aren't in the game's data" };
	}
	return where;
}

auto ValidName(std::string_view text) -> bool {
	return text.size() >= 2 && text.size() < NameBytes
	    && std::ranges::none_of(text, [](char c) { return static_cast<unsigned char>(c) < 0x20 || c == 0x7F; });
}

auto ReadPlayer(uintptr_t base, const PlayerLocation& where) -> std::optional<Player> {
	if (base == 0 || where.slot == 0) {
		return std::nullopt;
	}
	RawPlayer raw {};
	if (!ReadPlayerGuarded(base + where.slot, base + where.ids, base + where.table, where.next, &raw) || raw.classId < 0 || raw.classId > 7) {
		return std::nullopt;
	}
	const std::string_view name(raw.name, strnlen(raw.name, NameBytes));
	if (!ValidName(name)) {
		return std::nullopt;
	}
	return Player { .classId = raw.classId, .name = std::string(name) };
}

} // namespace ap::client_info
