#include "client_info.h"

#include "native_scan.h"

#include <windows.h>

namespace ap::client_info {

namespace {

// The game-flags handler: the difficulty's call is at +10 (rel32 at +11, ending at +15).
constexpr const char* FlagsPattern = "8B 79 02 48 8B D9 0F B6 49 01 E8 ?? ?? ?? ?? 0F B6 53 06 83 EA 01";
// One-line byte setters and getters: "mov [rip+disp32], cl; ret" and "movzx eax, byte [rip+disp32]; ret".
constexpr const char* SetterPattern = "88 0D ?? ?? ?? ?? C3";
constexpr const char* GetterPattern = "0F B6 05 ?? ?? ?? ?? C3";

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

} // namespace ap::client_info
