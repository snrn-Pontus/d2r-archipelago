#include "personalize.h"

#include <cstdio>
#include <cstring>

namespace ap::personal {

namespace {

auto Read32(std::span<const uint8_t> bytes, size_t offset) -> uint32_t {
	uint32_t value = 0;
	std::memcpy(&value, bytes.data() + offset, sizeof(value));
	return value;
}

}

auto Name(std::string_view player) -> std::string {
	std::string name;
	for (const char c : player) {
		const bool letter = (c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z') || (c >= '0' && c <= '9');
		if (letter || ((c == ' ' || c == '-' || c == '_') && !name.empty() && name.back() != ' ')) {
			name += c;
		}
		if (name.size() == NameSize - 1) {
			break;
		}
	}
	while (!name.empty() && (name.back() == ' ' || name.back() == '-' || name.back() == '_')) {
		name.pop_back();
	}
	return name;
}

auto CheckLayout(std::span<const uint8_t> data, uint32_t quality, bool identified, bool ethereal) -> std::string {
	if (data.size() < DataName + NameSize) {
		return "item data too short";
	}
	if (Read32(data, DataQuality) != quality) {
		return "quality " + std::to_string(Read32(data, DataQuality)) + " isn't " + std::to_string(quality);
	}
	const uint32_t flags = Read32(data, DataFlags);
	if (((flags & FlagIdentified) != 0) != identified || ((flags & FlagEthereal) != 0) != ethereal) {
		char text[64] {};
		std::snprintf(text, sizeof(text), "flags %08X don't match the item", flags);
		return text;
	}
	const auto slot = data.subspan(DataName, NameSize);
	if ((flags & FlagPersonalized) == 0) {
		for (const uint8_t byte : slot) {
			if (byte != 0) {
				return "the name slot isn't empty";
			}
		}
		return {};
	}
	// Already personalized: the slot must hold a printable, terminated name.
	size_t length = 0;
	while (length < NameSize && slot[length] != 0) {
		if (slot[length] < 0x20 || slot[length] > 0x7E) {
			return "the name slot doesn't hold a name";
		}
		++length;
	}
	return length > 0 && length < NameSize ? std::string {} : "the name slot doesn't hold a name";
}

auto HexDump(std::span<const uint8_t> bytes) -> std::string {
	std::string text;
	for (size_t row = 0; row < bytes.size(); row += 16) {
		char line[8] {};
		std::snprintf(line, sizeof(line), "%04zX:", row);
		text += line;
		for (size_t i = row; i < row + 16 && i < bytes.size(); ++i) {
			std::snprintf(line, sizeof(line), " %02X", bytes[i]);
			text += line;
		}
		text += '\n';
	}
	return text;
}

}
