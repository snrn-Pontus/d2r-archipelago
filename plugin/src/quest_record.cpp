#include "quest_record.h"

#include "native_scan.h"

#include <windows.h>

#include <cstring>

namespace ap::quest_record {

namespace {

// The client's handler for the quest record message: "cmp <type>, 6; jne; mov rcx, [record];
// mov r8d, 0x60; xor r9d, r9d", then the copy. The record's disp32 is at +8, ending at +12.
constexpr const char* CopyPattern = "80 ?? 06 75 ?? 48 8B 0D ?? ?? ?? ?? 41 B8 60 00 00 00 45 33 C9";
// Leaving a game frees the two records ("mov rcx, [record]; call free" twice), then clears
// both pointers after "xor ebx, ebx". The first disp32 is at +3 (ending at +7), the first
// clear's at +36 (ending at +40).
constexpr const char* FreePattern = "48 8B 0D ?? ?? ?? ?? E8 ?? ?? ?? ?? 48 8B 0D ?? ?? ?? ?? E8 ?? ?? ?? ?? 48 8B 0D ?? ?? ?? ?? 33 DB 48 89 1D ?? ?? ?? ?? 48 89 1D";

// The record is a bit buffer: the bytes, then their size in bits.
struct Record {
	const uint8_t* bytes;
	uint64_t       bitCount;
};

// No C++ objects here: __try can't unwind them.
auto CopyGuarded(uintptr_t pointerAddress, uint8_t* out) -> ReadResult {
	__try {
		const auto* record = *reinterpret_cast<const Record* const*>(pointerAddress);
		if (record == nullptr) {
			return ReadResult::NoRecord;
		}
		if (record->bitCount != Bytes * 8 || record->bytes == nullptr) {
			return ReadResult::Invalid;
		}
		std::memcpy(out, record->bytes, Bytes);
		return ReadResult::Ok;
	} __except (GetExceptionCode() == EXCEPTION_ACCESS_VIOLATION ? EXCEPTION_EXECUTE_HANDLER : EXCEPTION_CONTINUE_SEARCH) {
		return ReadResult::Fault;
	}
}

} // namespace

auto Locate(const uint8_t* image) -> Location {
	const auto copy = native::FindUnique(image, CopyPattern);
	if (!copy.rva) {
		return { .error = "the quest record handler was " + copy.error };
	}
	const auto release = native::FindUnique(image, FreePattern);
	if (!release.rva) {
		return { .error = "the quest record cleanup was " + release.error };
	}
	const uint32_t fromCopy    = native::RipTarget(image, *copy.rva + 8, *copy.rva + 12);
	const uint32_t fromFree    = native::RipTarget(image, *release.rva + 3, *release.rva + 7);
	const uint32_t fromCleared = native::RipTarget(image, *release.rva + 36, *release.rva + 40);
	if (fromCopy != fromFree || fromCopy != fromCleared) {
		return { .error = "the quest record handler and cleanup disagree" };
	}
	for (const auto& section : native::Sections(image)) {
		if (section.writable && !section.executable && fromCopy >= section.rva && fromCopy + 8 <= section.rva + section.size) {
			return { .rva = fromCopy };
		}
	}
	return { .error = "the quest record pointer isn't in the game's data" };
}

auto Read(uintptr_t pointerAddress, Bits& bits) -> ReadResult {
	return CopyGuarded(pointerAddress, bits.data());
}

} // namespace ap::quest_record
