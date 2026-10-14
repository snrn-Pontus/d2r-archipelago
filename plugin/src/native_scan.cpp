#include "native_scan.h"

#include <cstring>

namespace ap::native {

namespace {

constexpr uint32_t ExecutableSection = 0x20000000; // IMAGE_SCN_MEM_EXECUTE
constexpr uint32_t WritableSection   = 0x80000000; // IMAGE_SCN_MEM_WRITE

template <typename T>
auto ReadAt(const uint8_t* image, size_t offset) -> T {
	T value {};
	std::memcpy(&value, image + offset, sizeof(T));
	return value;
}

auto HexDigit(char c) -> int {
	if (c >= '0' && c <= '9') {
		return c - '0';
	}
	if (c >= 'a' && c <= 'f') {
		return c - 'a' + 10;
	}
	if (c >= 'A' && c <= 'F') {
		return c - 'A' + 10;
	}
	return -1;
}

} // namespace

auto ParsePattern(std::string_view text) -> std::vector<int16_t> {
	std::vector<int16_t> pattern;
	size_t i = 0;
	while (i < text.size()) {
		if (text[i] == ' ') {
			++i;
			continue;
		}
		if (i + 1 >= text.size() || (i + 2 < text.size() && text[i + 2] != ' ')) {
			return {};
		}
		if (text[i] == '?' && text[i + 1] == '?') {
			pattern.push_back(-1);
		} else {
			const int high = HexDigit(text[i]);
			const int low  = HexDigit(text[i + 1]);
			if (high < 0 || low < 0) {
				return {};
			}
			pattern.push_back(static_cast<int16_t>(high * 16 + low));
		}
		i += 2;
	}
	return pattern;
}

auto FindPattern(std::span<const uint8_t> haystack, std::span<const int16_t> pattern, size_t limit) -> std::vector<size_t> {
	std::vector<size_t> found;
	if (pattern.empty() || pattern.size() > haystack.size() || pattern[0] < 0) {
		return found;
	}
	const auto first = static_cast<uint8_t>(pattern[0]);
	const size_t last = haystack.size() - pattern.size();
	const uint8_t* data = haystack.data();
	for (size_t at = 0; at <= last && found.size() < limit;) {
		const auto* next = static_cast<const uint8_t*>(std::memchr(data + at, first, last - at + 1));
		if (next == nullptr) {
			break;
		}
		at = static_cast<size_t>(next - data);
		bool match = true;
		for (size_t k = 1; k < pattern.size() && match; ++k) {
			match = pattern[k] < 0 || data[at + k] == static_cast<uint8_t>(pattern[k]);
		}
		if (match) {
			found.push_back(at);
		}
		++at;
	}
	return found;
}

auto Sections(const uint8_t* image) -> std::vector<Section> {
	std::vector<Section> sections;
	if (image == nullptr || ReadAt<uint16_t>(image, 0) != 0x5A4D) { // "MZ"
		return sections;
	}
	const auto pe = ReadAt<uint32_t>(image, 0x3C);
	if (pe < 0x40 || pe > 0x1000 || ReadAt<uint32_t>(image, pe) != 0x00004550) { // "PE\0\0"
		return sections;
	}
	const auto count       = ReadAt<uint16_t>(image, pe + 6);
	const auto optionalSize = ReadAt<uint16_t>(image, pe + 20);
	const size_t table     = pe + 24 + optionalSize;
	if (count > 96 || table + count * 40U > 0x1000) {
		return sections;
	}
	for (uint16_t i = 0; i < count; ++i) {
		const size_t entry = table + i * 40U;
		const auto flags   = ReadAt<uint32_t>(image, entry + 36);
		sections.push_back({
			.rva        = ReadAt<uint32_t>(image, entry + 12),
			.size       = ReadAt<uint32_t>(image, entry + 8),
			.executable = (flags & ExecutableSection) != 0,
			.writable   = (flags & WritableSection) != 0,
		});
	}
	return sections;
}

auto FindUnique(const uint8_t* image, std::string_view text) -> Match {
	const auto pattern = ParsePattern(text);
	if (pattern.empty()) {
		return { .error = "bad pattern" };
	}
	std::vector<uint32_t> hits;
	for (const auto& section : Sections(image)) {
		if (!section.executable) {
			continue;
		}
		for (const size_t offset : FindPattern({ image + section.rva, section.size }, pattern, 2 - hits.size())) {
			hits.push_back(section.rva + static_cast<uint32_t>(offset));
		}
		if (hits.size() >= 2) {
			break;
		}
	}
	if (hits.size() == 1) {
		return { .rva = hits[0] };
	}
	return { .error = hits.empty() ? "not found" : "found more than once" };
}

auto RipTarget(const uint8_t* image, uint32_t dispRva, uint32_t endRva) -> uint32_t {
	return endRva + static_cast<uint32_t>(ReadAt<int32_t>(image, dispRva));
}

} // namespace ap::native
